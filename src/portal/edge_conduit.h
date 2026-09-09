/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file edge_conduit.h Registry and logic for perimeter Edge Extraction Conduits. */

#ifndef PORTAL_EDGE_CONDUIT_H
#define PORTAL_EDGE_CONDUIT_H

#include "../map_type.h"
#include "../direction_type.h"
#include "planet_type.h"
#include "../cargo_type.h"
#include "../company_type.h"

#include <unordered_map>
#include <vector>

using ConduitID = uint32_t;
static constexpr ConduitID INVALID_CONDUIT{UINT32_MAX};

/** Representation of an edge mineral extraction conduit. */
struct EdgeConduit {
	ConduitID id{INVALID_CONDUIT};
	TileIndex tile{INVALID_TILE};
	DiagDirection dir{DiagDirection::Invalid};
	WorldID world_id{INVALID_WORLD};
	CargoType cargo_type{0};
	uint32_t production_rate{50}; ///< Base monthly production rate in units.
	Owner owner{INVALID_OWNER};
	uint32_t total_produced{0};
};

class EdgeConduitManager {
public:
	/**
	 * Check if a tile is directly adjacent to a void buffer tile or map boundary.
	 * Edge conduits must be built on the boundary looking out into the void abyss.
	 */
	static bool IsVoidAdjacent(TileIndex tile);

	/**
	 * Register a new edge extraction conduit.
	 * @param tile Tile where the conduit drillhead is located.
	 * @param dir Orientation of the conduit tunnel head.
	 * @param world_id Logical world of origin.
	 * @param cargo Type of mineral cargo to extract.
	 * @param owner Owning company.
	 * @param base_production Base monthly production rate.
	 * @return Assigned ConduitID, or INVALID_CONDUIT on failure.
	 */
	static ConduitID RegisterConduit(TileIndex tile, DiagDirection dir, WorldID world_id, CargoType cargo, Owner owner, uint32_t base_production = 50);

	/** Restore a conduit from savegame. */
	static void RestoreConduit(const EdgeConduit &conduit);

	/** Unregister a conduit by tile. */
	static bool UnregisterConduit(TileIndex tile);

	/** Check if a tile contains an edge conduit. */
	static bool IsConduitTile(TileIndex tile);

	/** Get conduit by tile. */
	static const EdgeConduit *GetConduit(TileIndex tile);
	static EdgeConduit *GetConduitMutable(TileIndex tile);

	/** Get all registered conduits. */
	static const std::unordered_map<TileIndex, EdgeConduit> &GetAllConduits();

	/** Number of registered conduits. */
	static size_t Count();

	/**
	 * Periodic production loop for edge conduits.
	 * Queries nearby rail stations and feeds raw minerals directly into catchment bays.
	 * Applies world phase multipliers (e.g. Frontier worlds receive +100% extraction bonus).
	 */
	static void ProduceAllConduits();

	/**
	 * Calculate effective monthly production rate for an edge conduit.
	 */
	static uint32_t CalculateProduction(const EdgeConduit &conduit);

	/**
	 * Resolve the preferred raw mineral cargo for edge extraction in the current climate.
	 */
	static CargoType GetPreferredMineralCargo();

	/** Reset the manager (for new game / load). */
	static void Reset();

private:
	static std::unordered_map<TileIndex, EdgeConduit> conduits;
	static ConduitID next_conduit_id;
};

#endif /* PORTAL_EDGE_CONDUIT_H */
