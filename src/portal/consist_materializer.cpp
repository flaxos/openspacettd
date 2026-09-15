/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file consist_materializer.cpp Engine functions for consist despawn and materialization. */

#include "../stdafx.h"
#include "consist_materializer.h"
#include "consist_snapshot.h"
#include "content_manifest.h"
#include "federation_identity.h"
#include "portal_registry.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../direction_func.h"
#include "../engine_base.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnelbridge_map.h"
#include "../timer/timer_game_calendar.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "planet_manager.h"

#include "../safeguards.h"

ConsistDespawnResult ConsistMaterializer::DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token,
	const std::function<bool(const ConsistSnapshotBytes &)> &admit)
{
	return DespawnForTransfer(consist, owner_token,
		[admit](const ConsistSnapshot &, const ConsistSnapshotBytes &bytes) {
			return !admit || admit(bytes);
		});
}

ConsistDespawnResult ConsistMaterializer::DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token,
	const std::function<bool(const ConsistSnapshot &, const ConsistSnapshotBytes &)> &admit)
{
	ConsistDespawnResult result;
	if (consist == nullptr) {
		result.error_message = "Null consist pointer";
		return result;
	}

	Train *front = consist->First();
	if (front == nullptr) {
		result.error_message = "No front engine in consist";
		return result;
	}

	/* 1. Capture snapshot for active content */
	ConsistSnapshotResult capture_res = ConsistSnapshotCodec::CaptureForCurrentContent(front, owner_token);
	if (!capture_res.Succeeded()) {
		result.error_message = "Failed to capture consist snapshot";
		return result;
	}
	result.snapshot = *capture_res.snapshot;

	/* 2. Encode to canonical v2 wire format */
	result.snapshot_bytes = ConsistSnapshotCodec::Encode(result.snapshot);
	if (!result.snapshot_bytes.Succeeded()) {
		result.error_message = "Failed to encode consist snapshot";
		return result;
	}

	/* 3. Compute cargo units */
	for (const auto &unit : result.snapshot.units) {
		result.total_cargo += unit.cargo_count;
	}

	/* Admission must succeed before relinquishing the physical consist. */
	if (admit && !admit(result.snapshot, result.snapshot_bytes)) {
		result.error_message = "Consist transfer admission rejected";
		return result;
	}

	/* 4. Release vehicle transit progress in portal registry */
	PortalRegistry::ClearPortalTransit(front->index);

	/* 5. Free track reservations */
	FreeTrainTrackReservation(front);

	/* Unreserve tile track if still reserved */
	TileIndex tile = front->tile;
	if (IsTunnelTile(tile)) {
		SetTunnelBridgeReservation(tile, false);
	} else if (IsPlainRailTile(tile)) {
		Track track = TrackBitsToTrack(front->track);
		if (IsValidTrack(track) && HasTrack(tile, track) && GetRailReservationTrackBits(tile).Test(track)) {
			UnreserveTrack(tile, track);
		}
	}

	/* 6. Release local vehicle anchor mapping */
	FederationIdentityRegistry::ReleaseVehicle(front->index);

	/* 7. Cleanly delete vehicle chain */
	delete front;

	result.success = true;
	return result;
}

bool ConsistMaterializer::CheckThroatClearance(TileIndex exit_tile, DiagDirection enter_dir)
{
	(void)enter_dir;
	if (!IsValidTile(exit_tile) || !IsTunnelTile(exit_tile)) {
		return false;
	}

	/* Check for conflicting reservations */
	if (HasTunnelBridgeReservation(exit_tile)) {
		return false;
	}

	/* Check for vehicles currently on the exit tile */
	for (const Vehicle *v : VehiclesOnTile(exit_tile)) {
		if (v->type == VehicleType::Train) {
			return false;
		}
	}

	return true;
}

ConsistMaterializeResult ConsistMaterializer::MaterializeFromTransfer(
	const ConsistSnapshot &snapshot,
	TileIndex exit_tile,
	DiagDirection enter_dir)
{
	ConsistMaterializeResult result;
	if (snapshot.units.empty()) {
		result.error_message = "Snapshot contains no units";
		return result;
	}

	/* 1. Validate content manifest compatibility */
	ContentManifestResult current_manifest = ContentManifestCodec::CaptureCurrent();
	if (current_manifest.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*current_manifest.manifest);
		if (token_res.Succeeded() && token_res.token != snapshot.content_manifest) {
			result.error_message = "Universe content manifest mismatch between servers";
			return result;
		}
	}

	/* 2. Check exit throat clearance */
	if (!CheckThroatClearance(exit_tile, enter_dir)) {
		result.error_message = "Portal throat is obstructed";
		return result;
	}

	/* 3. Check pool allocations and engine types */
	if (!Vehicle::CanAllocateItem(snapshot.units.size())) {
		result.error_message = "Vehicle pool exhausted";
		return result;
	}

	for (const auto &u_snap : snapshot.units) {
		if (!Engine::IsValidID(EngineID{u_snap.engine_type})) {
			result.error_message = "Invalid engine type";
			return result;
		}
	}

	/* 4. Resolve local company */
	CompanyID local_company = CompanyID::Invalid();
	for (const auto &[cid, seq] : FederationIdentityRegistry::GetCompanyMappings()) {
		if (snapshot.company_id.sequence == seq) {
			local_company = CompanyID{cid};
			break;
		}
	}
	if (local_company == CompanyID::Invalid() || !Company::IsValidID(local_company)) {
		local_company = (_current_company != COMPANY_SPECTATOR && Company::IsValidID(_current_company))
			? _current_company : CompanyID{0};
	}

	/* 5. Calculate vehicle emergence position */
	DiagDirection exit_vdir = ReverseDiagDir(enter_dir);
	static constexpr DiagDirectionIndexArray<uint8_t> tunnel_vis_frame{12, 8, 8, 12};
	uint8_t frame = TILE_SIZE - tunnel_vis_frame[enter_dir];

	int offset_x = 8;
	int offset_y = 8;
	switch (exit_vdir) {
		case DiagDirection::NE: offset_x = TILE_SIZE - 1 - frame; break;
		case DiagDirection::SE: offset_y = frame; break;
		case DiagDirection::SW: offset_x = frame; break;
		case DiagDirection::NW: offset_y = TILE_SIZE - 1 - frame; break;
		default: break;
	}

	int x = TileX(exit_tile) * TILE_SIZE + offset_x;
	int y = TileY(exit_tile) * TILE_SIZE + offset_y;
	int z = GetSlopePixelZ(x, y, true);
	Direction dir = DiagDirToDir(exit_vdir);
	Track track = DiagDirToDiagTrack(exit_vdir);

	/* 6. Construct train consist chain */
	Train *front = nullptr;
	Train *prev = nullptr;
	uint32_t total_cargo = 0;

	for (size_t i = 0; i < snapshot.units.size(); ++i) {
		const auto &u_snap = snapshot.units[i];
		Train *t = Vehicle::Create<Train>();
		t->subtype = u_snap.subtype;
		if (i == 0) {
			front = t;
			t->SetFrontEngine();
			t->SetEngine();
		} else {
			t->ClearFrontEngine();
			if (u_snap.subtype & 1) {
				t->SetEngine();
			} else {
				t->SetWagon();
			}
		}

		t->owner = local_company;
		t->tile = exit_tile;
		t->x_pos = x;
		t->y_pos = y;
		t->z_pos = z;
		t->direction = dir;
		t->track = track;
		t->vehstatus.Reset(VehState::Hidden);

		t->engine_type = EngineID{u_snap.engine_type};
		if (u_snap.cargo_type < to_underlying(NUM_CARGO)) {
			t->cargo_type = CargoType{u_snap.cargo_type};
		} else {
			t->cargo_type = CargoType{0};
		}
		t->cargo_subtype = u_snap.cargo_subtype;
		t->cargo_cap = u_snap.cargo_capacity;
		t->refit_cap = u_snap.refit_capacity;
		t->build_year = TimerGameCalendar::Year{u_snap.build_year};
		t->age = TimerGameCalendar::Date{u_snap.age};
		t->max_age = TimerGameCalendar::Date{u_snap.max_age};
		t->value = u_snap.value;
		t->reliability = u_snap.reliability;
		t->reliability_spd_dec = u_snap.reliability_speed_decrease;
		t->breakdown_ctr = u_snap.breakdown_counter;
		t->breakdown_delay = u_snap.breakdown_delay;
		t->breakdowns_since_last_service = u_snap.breakdowns_since_service;
		t->breakdown_chance = u_snap.breakdown_chance;
		t->random_bits = u_snap.random_bits;

		if (u_snap.cargo_count > 0 && CargoPacket::CanAllocateItem()) {
			StationID st_id = StationID::Invalid();
			if (u_snap.cargo_source.IsValid() && u_snap.cargo_source.origin_station.IsValid()) {
				st_id = StationID(static_cast<uint16_t>(u_snap.cargo_source.origin_station.sequence));
			}
			TileIndex source_xy = u_snap.cargo_source.IsValid()
				? TileXY(u_snap.cargo_source.origin_tile_x, u_snap.cargo_source.origin_tile_y)
				: INVALID_TILE;
			CargoPacket *cp = CargoPacket::Create(u_snap.cargo_count, 0, st_id, source_xy, 0);
			t->cargo.Append(cp);
			total_cargo += u_snap.cargo_count;
		}

		if (prev != nullptr) {
			prev->SetNext(t);
		}
		prev = t;
	}

	if (front == nullptr) {
		result.error_message = "Failed to create front engine";
		return result;
	}

	/* 7. Dynamics & consist properties */
	front->cur_speed = snapshot.speed;
	front->subspeed = snapshot.subspeed;
	front->acceleration = snapshot.acceleration;
	if (snapshot.stopped) front->vehstatus.Set(VehState::Stopped);
	if (snapshot.driving_backwards) front->vehicle_flags.Set(VehicleFlag::DrivingBackwards);

	front->ConsistChanged(CCF_ARRANGE);
	for (Train *u = front; u != nullptr; u = u->Next()) {
		u->UpdatePositionAndViewport();
	}

	/* 8. Acquire exit track reservation */
	if (IsTunnelTile(exit_tile)) {
		SetTunnelBridgeReservation(exit_tile, true);
	} else if (IsPlainRailTile(exit_tile) && HasTrack(exit_tile, track)) {
		TryReserveTrack(exit_tile, track);
	}

	/* 9. Restore persistent GlobalConsistID */
	if (snapshot.consist_id.IsValid()) {
		FederationIdentityRegistry::RestoreMapping(front->index, snapshot.consist_id.sequence);
	}

	/* 10. Restore and resolve order schedule */
	if (!snapshot.orders.empty() && OrderList::CanAllocateItem()) {
		WorldID exit_world = PlanetManager::GetTileWorld(exit_tile);
		if (exit_world == INVALID_WORLD) {
			const InterServerPortalLink *link = PortalRegistry::GetInterServerPortal(exit_tile);
			if (link != nullptr) exit_world = link->local_endpoint.world_id;
		}

		/* Cache master schedule for this consist in FederationIdentityRegistry */
		if (snapshot.consist_id.IsValid()) {
			FederationIdentityRegistry::SetConsistSchedule(snapshot.consist_id.sequence, snapshot.orders);
		}

		std::vector<Order> restored_orders;
		restored_orders.reserve(snapshot.orders.size());

		for (const GlobalOrderDestinationID &g_ord : snapshot.orders) {
			auto dest_opt = FederationIdentityRegistry::ResolveOrderDestination(g_ord, exit_world);
			if (dest_opt.has_value()) {
				Order ord;
				if (g_ord.type == OrderDestinationType::Waypoint) {
					ord.MakeGoToWaypoint(dest_opt->ToStationID());
				} else if (g_ord.type == OrderDestinationType::Depot) {
					ord.MakeGoToDepot(*dest_opt, OrderDepotTypeFlags{});
				} else {
					ord.MakeGoToStation(dest_opt->ToStationID());
				}
				restored_orders.push_back(std::move(ord));
			}
		}

		if (!restored_orders.empty()) {
			front->orders = OrderList::Create(std::move(restored_orders), front);

			/* Advance order index if the previous order was targeting the origin world */
			uint16_t active_idx = 0;
			if (snapshot.current_order_index < snapshot.orders.size()) {
				const auto &orig_order = snapshot.orders[snapshot.current_order_index];
				if (orig_order.target_world == exit_world) {
					active_idx = snapshot.current_order_index;
				} else {
					active_idx = static_cast<uint16_t>((snapshot.current_order_index + 1) % snapshot.orders.size());
				}
			}

			if (active_idx >= front->GetNumOrders()) active_idx = 0;
			front->cur_real_order_index = active_idx;
			front->cur_implicit_order_index = active_idx;
			front->current_order = *front->GetOrder(active_idx);
		}
	}

	result.success = true;
	result.consist = front;
	result.total_cargo = total_cargo;
	return result;
}

bool ConsistMaterializer::AssignRoundTripOrders(Train *consist, StationID origin_st, WorldID origin_world, StationID dest_st, WorldID dest_world)
{
	if (consist == nullptr || !BaseStation::IsValidID(origin_st) || !BaseStation::IsValidID(dest_st)) return false;

	Train *front = consist->First();
	if (front == nullptr) return false;

	/* Register stations with FederationIdentityRegistry */
	auto orig_global = FederationIdentityRegistry::GetOrCreateStation(origin_st);
	auto dest_global = FederationIdentityRegistry::GetOrCreateStation(dest_st);
	if (!orig_global.has_value() || !dest_global.has_value()) return false;

	orig_global->world_id = origin_world;
	dest_global->world_id = dest_world;

	/* Construct master schedule */
	std::vector<GlobalOrderDestinationID> master_schedule;
	master_schedule.push_back(GlobalOrderDestinationID::ForStation(*orig_global, false));
	master_schedule.push_back(GlobalOrderDestinationID::ForStation(*dest_global, false));

	std::optional<GlobalConsistID> cid = FederationIdentityRegistry::GetOrCreate(front);
	if (cid.has_value()) {
		FederationIdentityRegistry::SetConsistSchedule(cid->sequence, master_schedule);
	}

	/* Build local projection */
	std::vector<Order> local_orders;
	Order o1;
	o1.MakeGoToStation(origin_st);
	local_orders.push_back(std::move(o1));

	Order o2;
	auto resolved_dest = FederationIdentityRegistry::ResolveOrderDestination(master_schedule[1], origin_world);
	if (resolved_dest.has_value()) {
		o2.MakeGoToStation(resolved_dest->ToStationID());
	} else {
		o2.MakeGoToStation(dest_st);
	}
	local_orders.push_back(std::move(o2));

	if (front->orders != nullptr) {
		front->orders->FreeChain();
		front->orders = nullptr;
	}

	if (!OrderList::CanAllocateItem()) return false;
	front->orders = OrderList::Create(std::move(local_orders), front);
	front->cur_real_order_index = 0;
	front->cur_implicit_order_index = 0;
	front->current_order = *front->GetOrder(0);
	return true;
}
