/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_registry.cpp Implementation of wormhole portal registry. */

#include "../stdafx.h"
#include "portal_registry.h"

std::unordered_map<TileIndex, PortalID> PortalRegistry::tile_to_portal;
std::unordered_map<uint32_t, PortalLink> PortalRegistry::portal_links;
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

size_t PortalRegistry::Count()
{
	return portal_links.size();
}

void PortalRegistry::Reset()
{
	tile_to_portal.clear();
	portal_links.clear();
	next_portal_id = 1;
}
