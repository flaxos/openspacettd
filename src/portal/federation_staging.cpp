/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file federation_staging.cpp Implementation of automatic holding loops and staging sidings. */

#include "../stdafx.h"
#include "federation_staging.h"
#include "federation_cmd.h"
#include "portal_registry.h"
#include "universe_authority.h"
#include "../train.h"
#include "../timer/timer_game_tick.h"
#include "../track_func.h"
#include "../window_func.h"
#include "../debug.h"
#include "../core/format.hpp"

#include "../safeguards.h"

static std::map<VehicleID, StagingHoldRecord> _held_trains;

void FederationStagingManager::Reset()
{
	_held_trains.clear();
}

bool FederationStagingManager::IsServerHoldingCondition(WorldID remote_world)
{
	if (remote_world == INVALID_WORLD) return false;

	const RegisteredWorld *world = UniverseAuthorityService::Instance().GetWorld(remote_world);
	if (world == nullptr) {
		/* In-memory mode requires explicit registration; under external authority, default to clear unless holding configured */
		return !FederationTransferManager::HasExternalAuthority();
	}

	if (world->status == WorldOnlineStatus::Maintenance || world->status == WorldOnlineStatus::Unreachable) {
		return true;
	}

	if (world->ping_ms > LATENCY_THRESHOLD_MS) {
		return true;
	}

	/* Also check freight corridor congestion */
	for (const auto &corridor : UniverseAuthorityService::Instance().GetFreightCorridors()) {
		if (corridor.dest_world == remote_world && corridor.congestion_level == CorridorCongestionLevel::Saturated) {
			return true;
		}
	}

	return false;
}

std::string FederationStagingManager::GetHoldingReason(WorldID remote_world)
{
	if (remote_world == INVALID_WORLD) return "Invalid Destination";

	const RegisteredWorld *world = UniverseAuthorityService::Instance().GetWorld(remote_world);
	if (world == nullptr) {
		return FederationTransferManager::HasExternalAuthority() ? "Mainline Clear" : "Destination Server Offline";
	}

	if (world->status == WorldOnlineStatus::Maintenance) {
		return "Server Maintenance / Scheduled Restart";
	}
	if (world->status == WorldOnlineStatus::Unreachable) {
		return "Server Unreachable / Connection Dropped";
	}
	if (world->ping_ms > LATENCY_THRESHOLD_MS) {
		return fmt::format("High Network Latency ({} ms > {} ms)", world->ping_ms, LATENCY_THRESHOLD_MS);
	}

	for (const auto &corridor : UniverseAuthorityService::Instance().GetFreightCorridors()) {
		if (corridor.dest_world == remote_world && corridor.congestion_level == CorridorCongestionLevel::Saturated) {
			return "Freight Corridor Saturated";
		}
	}

	return "Mainline Clear";
}

bool FederationStagingManager::CheckAndDivertToStaging(Train *consist, TileIndex portal_tile)
{
	if (consist == nullptr || portal_tile == INVALID_TILE) return false;

	const InterServerPortalLink *link = PortalRegistry::GetInterServerPortal(portal_tile);
	if (link == nullptr || !link->IsValid()) return false;

	Train *front = consist->First();
	if (front == nullptr) return false;

	/* If already holding in staging siding, continue holding */
	if (_held_trains.find(front->index) != _held_trains.end()) {
		return true;
	}

	/* Evaluate whether holding conditions apply to remote server */
	if (!IsServerHoldingCondition(link->remote_world)) {
		PortalRegistry::SetHoldingActive(portal_tile, false);
		return false;
	}

	/* Holding condition active: divert train into staging siding or hold before portal */
	PortalRegistry::SetHoldingActive(portal_tile, true);

	TileIndex siding = link->staging_siding_tile;
	std::string reason = GetHoldingReason(link->remote_world);

	/* Safely stop consist and free mainline reservations if valid */
	if (front->track != Track::Wormhole) {
		Trackdir td = front->GetVehicleTrackdir();
		if (IsValidTrackdir(td) && IsValidTrack(TrackdirToTrack(td))) {
			FreeTrainTrackReservation(front);
		}
	}
	front->cur_speed = 0;
	front->subspeed = 0;
	front->vehstatus.Set(VehState::Stopped);

	/* If designated staging siding tile exists, position the train safely on the siding */
	if (siding != INVALID_TILE && IsValidTile(siding)) {
		front->tile = siding;
		if (front->x_pos != 0 && front->y_pos != 0) {
			for (Train *u = front; u != nullptr; u = u->Next()) {
				u->UpdatePositionAndViewport();
			}
		}
	}

	StagingHoldRecord rec{
		.vehicle_id = front->index,
		.portal_tile = portal_tile,
		.siding_tile = siding,
		.target_world = link->remote_world,
		.hold_start_tick = TimerGameTick::counter,
		.hold_reason = reason,
	};
	_held_trains[front->index] = rec;

	SetWindowDirty(WindowClass::VehicleView, front->index);
	SetWindowDirty(WindowClass::UniverseDirectory, 0);

	Debug(net, 1, "[Federation Staging] Train {} diverted to staging siding {} (Target World {}, Reason: {})",
		front->index.base(), siding, link->remote_world.base(), reason);
	return true;
}

size_t FederationStagingManager::ReleaseHeldTrains(TileIndex portal_tile)
{
	if (portal_tile == INVALID_TILE) return 0;

	size_t released_count = 0;
	for (auto it = _held_trains.begin(); it != _held_trains.end();) {
		if (it->second.portal_tile == portal_tile) {
			Train *v = Train::GetIfValid(it->second.vehicle_id);
			if (v != nullptr) {
				Train *front = v->First();
				if (front != nullptr) {
					/* Clear stopped flag so train can resume motion into portal */
					front->vehstatus.Reset(VehState::Stopped);
					SetWindowDirty(WindowClass::VehicleView, front->index);
				}
			}
			it = _held_trains.erase(it);
			released_count++;
		} else {
			++it;
		}
	}

	PortalRegistry::SetHoldingActive(portal_tile, false);
	SetWindowDirty(WindowClass::UniverseDirectory, 0);
	if (released_count > 0) {
		Debug(net, 1, "[Federation Staging] Released {} held trains for portal {}", released_count, portal_tile.base());
	}
	return released_count;
}

size_t FederationStagingManager::ReleaseHeldTrains()
{
	size_t released = 0;
	for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
		if (link.is_holding_active && !IsServerHoldingCondition(link.remote_world)) {
			released += ReleaseHeldTrains(tile);
		}
	}
	return released;
}

void FederationStagingManager::OnGameTick([[maybe_unused]] uint64_t current_tick)
{
	/* Check holding status periodically */
	if ((current_tick % 10) != 0) return;

	ReleaseHeldTrains();
}

std::vector<StagingHoldRecord> FederationStagingManager::GetHeldTrainsForPortal(TileIndex portal_tile)
{
	std::vector<StagingHoldRecord> res;
	for (const auto &[vid, rec] : _held_trains) {
		if (rec.portal_tile == portal_tile) {
			res.push_back(rec);
		}
	}
	return res;
}

std::vector<StagingHoldRecord> FederationStagingManager::GetAllHeldTrains()
{
	std::vector<StagingHoldRecord> res;
	res.reserve(_held_trains.size());
	for (const auto &[vid, rec] : _held_trains) {
		res.push_back(rec);
	}
	return res;
}

size_t FederationStagingManager::GetTotalHeldTrainsCount()
{
	return _held_trains.size();
}

bool FederationStagingManager::IsTrainHeld(VehicleID vehicle_id)
{
	return _held_trains.find(vehicle_id) != _held_trains.end();
}
