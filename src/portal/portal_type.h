/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_type.h Data types for OpenSpaceTTD wormhole portal gates. */

#ifndef PORTAL_TYPE_H
#define PORTAL_TYPE_H

#include "../tile_type.h"
#include "../direction_type.h"
#include "../core/strong_typedef_type.hpp"
#include <cstdint>

/** Logical world identifier. World 0 is typically the primary/home world. */
using WorldID = StrongType::Typedef<uint32_t, struct WorldIDTag, StrongType::Compare, StrongType::Integer>;

/** Unique identifier for a registered portal link. */
using PortalID = StrongType::Typedef<uint32_t, struct PortalIDTag, StrongType::Compare, StrongType::Integer>;

static constexpr PortalID INVALID_PORTAL = PortalID{ (uint32_t)-1 };
static constexpr WorldID INVALID_WORLD   = WorldID{ (uint32_t)-1 };
static constexpr WorldID DEFAULT_WORLD   = WorldID{ 0 };

/** Endpoint of a portal wormhole gate. */
struct PortalEndpoint {
	TileIndex tile = INVALID_TILE;                    ///< Physical tile location of the portal head.
	DiagDirection enter_dir = DiagDirection::Begin;   ///< Direction of vehicles entering this portal head.
	WorldID world_id = DEFAULT_WORLD;                 ///< Logical world region this endpoint belongs to.

	constexpr bool IsValid() const
	{
		return tile != INVALID_TILE;
	}
};

/** A bidirectional or directed portal link connecting two portal endpoints. */
struct PortalLink {
	PortalID id = INVALID_PORTAL;
	PortalEndpoint end_a;              ///< Primary endpoint (e.g. World A)
	PortalEndpoint end_b;              ///< Secondary endpoint (e.g. World B)
	uint32_t virtual_length = 1;       ///< Virtual length in tiles for traversal time & YAPF routing penalty.
	bool bidirectional = true;         ///< Whether vehicles can traverse in both directions.

	constexpr bool IsValid() const
	{
		return id != INVALID_PORTAL && end_a.IsValid() && end_b.IsValid();
	}

	/** Returns true if the given tile matches either endpoint. */
	bool HasTile(TileIndex tile) const
	{
		return end_a.tile == tile || end_b.tile == tile;
	}

	/** Returns the opposite endpoint given one end tile. */
	const PortalEndpoint *GetOpposite(TileIndex tile) const
	{
		if (end_a.tile == tile) return &end_b;
		if (end_b.tile == tile) return &end_a;
		return nullptr;
	}
};

#endif /* PORTAL_TYPE_H */
