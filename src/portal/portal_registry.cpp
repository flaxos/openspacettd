/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_registry.cpp Implementation of wormhole portal registry. */

#include "../stdafx.h"
#include "portal_registry.h"
#include "federation_identity.h"
#include "../tunnelbridge_map.h"
#include "../tile_map.h"
#include "../landscape.h"
#include "../direction_func.h"
#include "../vehicle_base.h"

std::unordered_map<TileIndex, PortalID> PortalRegistry::tile_to_portal;
std::unordered_map<uint32_t, PortalLink> PortalRegistry::portal_links;
std::unordered_map<TileIndex, PortalEndpoint> PortalRegistry::unlinked_gates;
std::unordered_map<uint32_t, uint32_t> PortalRegistry::vehicle_portal_progress;
uint32_t PortalRegistry::next_portal_id = 1;

PortalID PortalRegistry::RegisterPortalPair(
	TileIndex tile_a, DiagDirection dir_a, WorldID world_a,
	TileIndex tile_b, DiagDirection dir_b, WorldID world_b,
	uint32_t virtual_length, bool bidirectional
)
{
	if (tile_a == INVALID_TILE || tile_b == INVALID_TILE || tile_a == tile_b) {
		return INVALID_PORTAL;
	}

	/* Avoid registering a tile that is already part of another portal link */
	if (tile_to_portal.find(tile_a) != tile_to_portal.end() ||
	    tile_to_portal.find(tile_b) != tile_to_portal.end()) {
		return INVALID_PORTAL;
	}

	/* Remove from unlinked gates if either was unlinked */
	unlinked_gates.erase(tile_a);
	unlinked_gates.erase(tile_b);

	PortalID id{next_portal_id++};

	PortalLink link;
	link.id = id;
	link.end_a = PortalEndpoint{tile_a, dir_a, world_a};
	link.end_b = PortalEndpoint{tile_b, dir_b, world_b};
	link.virtual_length = std::max(1u, virtual_length);
	link.bidirectional = bidirectional;

	portal_links[id.base()] = link;
	tile_to_portal[tile_a] = id;
	tile_to_portal[tile_b] = id;

	return id;
}

bool PortalRegistry::UnregisterPortal(PortalID id)
{
	auto it = portal_links.find(id.base());
	if (it == portal_links.end()) return false;

	tile_to_portal.erase(it->second.end_a.tile);
	tile_to_portal.erase(it->second.end_b.tile);
	portal_links.erase(it);
	return true;
}

bool PortalRegistry::UnregisterPortalByTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;

	auto it_un = unlinked_gates.find(tile);
	if (it_un != unlinked_gates.end()) {
		unlinked_gates.erase(it_un);
		return true;
	}

	auto it = tile_to_portal.find(tile);
	if (it == tile_to_portal.end()) return false;

	return UnregisterPortal(it->second);
}

bool PortalRegistry::RegisterUnlinkedGate(TileIndex tile, DiagDirection dir, WorldID world_id)
{
	if (tile == INVALID_TILE) return false;
	if (tile_to_portal.find(tile) != tile_to_portal.end()) return false;
	if (unlinked_gates.find(tile) != unlinked_gates.end()) return false;

	unlinked_gates[tile] = PortalEndpoint{tile, dir, world_id};
	return true;
}

bool PortalRegistry::IsUnlinkedGate(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;
	return unlinked_gates.find(tile) != unlinked_gates.end();
}

const PortalEndpoint *PortalRegistry::GetUnlinkedGate(TileIndex tile)
{
	auto it = unlinked_gates.find(tile);
	if (it == unlinked_gates.end()) return nullptr;
	return &it->second;
}

const std::unordered_map<TileIndex, PortalEndpoint> &PortalRegistry::GetUnlinkedGates()
{
	return unlinked_gates;
}

PortalID PortalRegistry::LinkGates(TileIndex tile_a, TileIndex tile_b, uint32_t virtual_length, bool bidirectional)
{
	auto it_a = unlinked_gates.find(tile_a);
	auto it_b = unlinked_gates.find(tile_b);
	if (it_a == unlinked_gates.end() || it_b == unlinked_gates.end()) return INVALID_PORTAL;

	PortalEndpoint end_a = it_a->second;
	PortalEndpoint end_b = it_b->second;

	unlinked_gates.erase(it_a);
	unlinked_gates.erase(it_b);

	return RegisterPortalPair(
		end_a.tile, end_a.enter_dir, end_a.world_id,
		end_b.tile, end_b.enter_dir, end_b.world_id,
		virtual_length, bidirectional
	);
}

bool PortalRegistry::IsPortalInTransit(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;

	const PortalLink *link = GetPortalLink(tile);
	if (link == nullptr) {
		for (const auto &[veh_id, progress] : vehicle_portal_progress) {
			const Vehicle *v = Vehicle::GetIfValid(VehicleID{veh_id});
			if (v != nullptr && v->tile == tile) return true;
		}
		return false;
	}

	TileIndex tile_a = link->end_a.tile;
	TileIndex tile_b = link->end_b.tile;

	for (const auto &[veh_id, progress] : vehicle_portal_progress) {
		const Vehicle *v = Vehicle::GetIfValid(VehicleID{veh_id});
		if (v != nullptr && (v->tile == tile_a || v->tile == tile_b)) {
			return true;
		}
	}

	return false;
}

bool PortalRegistry::IsPortalTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;
	return tile_to_portal.find(tile) != tile_to_portal.end();
}

TileIndex PortalRegistry::GetOtherPortalEnd(TileIndex tile)
{
	const PortalLink *link = GetPortalLink(tile);
	if (link == nullptr) return INVALID_TILE;

	const PortalEndpoint *opp = link->GetOpposite(tile);
	return opp != nullptr ? opp->tile : INVALID_TILE;
}

PortalExitPosition PortalRegistry::GetPortalExitPosition(TileIndex entry_tile)
{
	PortalExitPosition pos;
	TileIndex exit_tile = GetOtherPortalEnd(entry_tile);
	if (exit_tile == INVALID_TILE) return pos;

	DiagDirection enter_dir = GetTunnelBridgeDirection(exit_tile);
	DiagDirection exit_vdir = ReverseDiagDir(enter_dir);

	/* OpenTTD tunnel visibility frames: NE=12, SE=8, SW=8, NW=12 */
	static constexpr DiagDirectionIndexArray<uint8_t> tunnel_vis_frame{12, 8, 8, 12};
	uint8_t frame = TILE_SIZE - tunnel_vis_frame[enter_dir];

	int offset_x = 8;
	int offset_y = 8;

	switch (exit_vdir) {
		case DiagDirection::NE:
			offset_x = TILE_SIZE - 1 - frame;
			break;
		case DiagDirection::SE:
			offset_y = frame;
			break;
		case DiagDirection::SW:
			offset_x = frame;
			break;
		case DiagDirection::NW:
			offset_y = TILE_SIZE - 1 - frame;
			break;
		default:
			break;
	}

	pos.tile = exit_tile;
	pos.x = TileX(exit_tile) * TILE_SIZE + offset_x;
	pos.y = TileY(exit_tile) * TILE_SIZE + offset_y;
	pos.z = GetSlopePixelZ(pos.x, pos.y, true);
	pos.dir = DiagDirToDir(exit_vdir);
	pos.track = DiagDirToDiagTrack(exit_vdir);
	return pos;
}

uint32_t PortalRegistry::GetPortalVirtualLength(TileIndex tile)
{
	const PortalLink *link = GetPortalLink(tile);
	return link != nullptr ? link->virtual_length : 1;
}

const PortalLink *PortalRegistry::GetPortalLink(TileIndex tile)
{
	auto it = tile_to_portal.find(tile);
	if (it == tile_to_portal.end()) return nullptr;
	return GetPortalLinkByID(it->second);
}

const PortalLink *PortalRegistry::GetPortalLinkByID(PortalID id)
{
	auto it = portal_links.find(id.base());
	if (it == portal_links.end()) return nullptr;
	return &it->second;
}

uint32_t PortalRegistry::AdvancePortalTransit(VehicleID veh_id)
{
	return ++vehicle_portal_progress[veh_id.base()];
}

uint32_t PortalRegistry::GetPortalTransitProgress(VehicleID veh_id)
{
	auto it = vehicle_portal_progress.find(veh_id.base());
	return it != vehicle_portal_progress.end() ? it->second : 0;
}

void PortalRegistry::ClearPortalTransit(VehicleID veh_id)
{
	vehicle_portal_progress.erase(veh_id.base());
}

const std::unordered_map<uint32_t, PortalLink> &PortalRegistry::GetAllPortals()
{
	return portal_links;
}

const std::unordered_map<uint32_t, uint32_t> &PortalRegistry::GetAllVehicleTransit()
{
	return vehicle_portal_progress;
}

bool PortalRegistry::RestorePortalLink(const PortalLink &link)
{
	if (!link.IsValid()) return false;

	portal_links[link.id.base()] = link;
	tile_to_portal[link.end_a.tile] = link.id;
	tile_to_portal[link.end_b.tile] = link.id;

	if (link.id.base() >= next_portal_id) {
		next_portal_id = link.id.base() + 1;
	}
	return true;
}

void PortalRegistry::SetVehicleTransitProgress(VehicleID veh_id, uint32_t progress)
{
	vehicle_portal_progress[veh_id.base()] = progress;
}

size_t PortalRegistry::RepairLegacyGeneratedGateways()
{
	auto safe_adjacent_tile = [](TileIndex tile, DiagDirection dir) {
		int dx = 0;
		int dy = 0;
		switch (dir) {
			case DiagDirection::NE: dx = -1; break;
			case DiagDirection::SE: dy = 1; break;
			case DiagDirection::SW: dx = 1; break;
			case DiagDirection::NW: dy = -1; break;
			default: return INVALID_TILE;
		}
		return TileAddWrap(tile, dx, dy);
	};

	auto is_neutral_lead_track = [](TileIndex tile, Track expected_track) {
		return IsValidTile(tile) && IsPlainRailTile(tile) && GetTileOwner(tile) == OWNER_NONE &&
			GetTrackBits(tile).Test(expected_track);
	};

	size_t repaired = 0;
	for (auto &[id, link] : portal_links) {
		for (PortalEndpoint *endpoint : {&link.end_a, &link.end_b}) {
			TileIndex tile = endpoint->tile;
			if (!IsValidTile(tile) || !IsTunnelTile(tile) || GetTileOwner(tile) != OWNER_NONE) continue;

			DiagDirection old_dir = GetTunnelBridgeDirection(tile);
			DiagDirection new_dir = ReverseDiagDir(old_dir);
			Track expected_track = DiagDirToDiagTrack(new_dir);
			TileIndex old_forward_side = safe_adjacent_tile(tile, old_dir);
			TileIndex correct_entry_side = safe_adjacent_tile(tile, new_dir);

			/* The legacy generator put the lead track on old_forward_side. With
			 * old_dir this is the far side of the tunnel head, so trains cannot
			 * enter. Do not alter ambiguous portals that have track on both sides. */
			if (!is_neutral_lead_track(old_forward_side, expected_track) ||
					is_neutral_lead_track(correct_entry_side, expected_track)) {
				continue;
			}

			SB(Tile(tile).m5(), 0, 2, to_underlying(new_dir));
			endpoint->enter_dir = new_dir;
			repaired++;
		}
	}

	return repaired;
}

size_t PortalRegistry::Count()
{
	return portal_links.size();
}

void PortalRegistry::Reset()
{
	tile_to_portal.clear();
	portal_links.clear();
	unlinked_gates.clear();
	vehicle_portal_progress.clear();
	next_portal_id = 1;
	FederationIdentityRegistry::Reset();
}
