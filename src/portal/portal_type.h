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
#include "../track_type.h"
#include "../core/strong_typedef_type.hpp"
#include <cstdint>

/** Logical world identifier. World 0 is typically the primary/home world. */
using WorldID = StrongType::Typedef<uint32_t, struct WorldIDTag, StrongType::Compare, StrongType::Integer>;

/** Unique identifier for a registered portal link. */
using PortalID = StrongType::Typedef<uint32_t, struct PortalIDTag, StrongType::Compare, StrongType::Integer>;

static constexpr PortalID INVALID_PORTAL = PortalID{ (uint32_t)-1 };
static constexpr WorldID INVALID_WORLD   = WorldID{ (uint32_t)-1 };
static constexpr WorldID DEFAULT_WORLD   = WorldID{ 0 };

/**
 * Physical distance a vehicle spends hidden while crossing a portal.
 * Portal route cost remains independently represented by PortalLink::virtual_length.
 */
static constexpr uint32_t PORTAL_TRANSIT_DISTANCE = TILE_SIZE;

/** Vehicle emergence position upon exiting a portal wormhole gate. */
struct PortalExitPosition {
	TileIndex tile = INVALID_TILE;
	int x = 0;
	int y = 0;
	int z = 0;
	Direction dir = Direction::N;
	Track track = Track::Begin;
};

/** Distinct operational classes for one-tile OpenSpace portal-like heads. */
enum class PortalGateKind : uint8_t {
	None = 0,              ///< Not managed by OpenSpace portal sidecar state.
	LocalLinked,           ///< A local wormhole pair with both physical endpoints.
	LocalUnlinked,         ///< A constructed local gate head that has not been linked.
	LocalStale,            ///< A local pair whose registry or physical opposite is missing.
	ExternalLinked,        ///< A valid federation departure gate with a remote world and gate id.
	ExternalUnlinked,      ///< An external gate record without a usable remote binding.
	ExternalStale,         ///< An external gate whose local physical endpoint is gone or invalid.
	EdgeConduit,           ///< A one-ended extraction conduit, not a train portal.
};

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

/** Classification result for a portal, federation gate, conduit or stale endpoint. */
struct PortalGateClassification {
	PortalGateKind kind = PortalGateKind::None;       ///< Operational class resolved for the queried tile.
	PortalID id = INVALID_PORTAL;                     ///< Local portal or federation gate identifier, if present.
	PortalEndpoint local_endpoint{};                  ///< Local physical head represented by this classification.
	PortalEndpoint opposite_endpoint{};               ///< Opposite local endpoint for local wormhole pairs.
	WorldID remote_world = INVALID_WORLD;             ///< Remote federation world for external gate records.
	uint32_t remote_gate_id = 0;                       ///< Remote federation gate identifier, or 0 when unbound.
	uint32_t virtual_length = 1;                       ///< Virtual routing length associated with the gate.

	/**
	 * Check whether this classification represents a traversable local wormhole pair.
	 * @return True for a valid local linked portal.
	 */
	constexpr bool IsLocalWormhole() const
	{
		return kind == PortalGateKind::LocalLinked;
	}

	/**
	 * Check whether this classification represents a valid external departure gate.
	 * @return True for a valid linked inter-server gate.
	 */
	constexpr bool IsExternalDeparture() const
	{
		return kind == PortalGateKind::ExternalLinked;
	}

	/**
	 * Check whether native rail entry must stop at this head.
	 * @return True for unlinked, stale or conduit heads that are not safe train portals.
	 */
	constexpr bool IsClosedHead() const
	{
		return kind == PortalGateKind::LocalUnlinked || kind == PortalGateKind::LocalStale ||
				kind == PortalGateKind::ExternalUnlinked || kind == PortalGateKind::ExternalStale ||
				kind == PortalGateKind::EdgeConduit;
	}
};

/** A bidirectional or directed portal link connecting two portal endpoints. */
struct PortalLink {
	PortalID id = INVALID_PORTAL;
	PortalEndpoint end_a;              ///< Primary endpoint (e.g. World A)
	PortalEndpoint end_b;              ///< Secondary endpoint (e.g. World B)
	uint32_t virtual_length = 1;       ///< Virtual route length in tiles for YAPF; physical transit time is fixed.
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

/** A gateway portal endpoint linked to a remote world server across the federation. */
struct InterServerPortalLink {
	PortalID id = INVALID_PORTAL;
	PortalEndpoint local_endpoint;     ///< Local portal head on this server.
	WorldID remote_world = INVALID_WORLD; ///< Destination world ID on remote server.
	uint32_t remote_gate_id = 0;       ///< Remote gateway identifier.
	uint32_t virtual_length = 1;       ///< Virtual route length for YAPF.

	constexpr bool IsValid() const
	{
		return id != INVALID_PORTAL && local_endpoint.IsValid() && remote_world != INVALID_WORLD;
	}
};

#endif /* PORTAL_TYPE_H */
