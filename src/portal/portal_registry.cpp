/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_registry.cpp Implementation of wormhole portal registry. */

#include "../stdafx.h"
#include "portal_registry.h"
#include "../tunnelbridge_map.h"
#include "../tile_map.h"
#include "../landscape.h"
#include "../direction_func.h"

std::unordered_map<TileIndex, PortalID> PortalRegistry::tile_to_portal;
std::unordered_map<uint32_t, PortalLink> PortalRegistry::portal_links;
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

size_t PortalRegistry::Count()
{
	return portal_links.size();
}

void PortalRegistry::Reset()
{
	tile_to_portal.clear();
	portal_links.clear();
	vehicle_portal_progress.clear();
	next_portal_id = 1;
}
