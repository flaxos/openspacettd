/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file spaceport_manager.h Registry and logic for interplanetary spaceports and virtual off-world trade. */

#ifndef PORTAL_SPACEPORT_MANAGER_H
#define PORTAL_SPACEPORT_MANAGER_H

#include "../station_type.h"
#include "planet_type.h"
#include "../cargo_type.h"
#include "../tile_type.h"

#include <unordered_map>
#include <vector>

/** Information about a designated interplanetary spaceport. */
struct SpaceportInfo {
	StationID station_id{StationID::Invalid()};
	WorldID world_id{INVALID_WORLD};
	uint32_t supplies_received{0};              ///< Life support / consumer supplies delivered in current cycle.
	uint8_t offworld_trade_tier{1};             ///< Trade hub tier (1 = Hub, 2 = Major Gateway, 3 = Prime Terminal).
	uint32_t total_offworld_cargo_generated{0}; ///< Cumulative off-world cargo produced.
};

class SpaceportManager {
public:
	/**
	 * Register a station as an interplanetary spaceport.
	 * @param station Station ID to register.
	 * @param world_id World where the spaceport is located.
	 * @param tier Initial spaceport trade tier.
	 * @return True if successfully registered.
	 */
	static bool RegisterSpaceport(StationID station, WorldID world_id, uint8_t tier = 1);

	/**
	 * Restore a spaceport from savegame.
	 * @param info The spaceport info to restore.
	 */
	static void RestoreSpaceport(const SpaceportInfo &info);

	/**
	 * Unregister a spaceport station.
	 * @param station Station ID to unregister.
	 * @return True if station was registered and removed.
	 */
	static bool UnregisterSpaceport(StationID station);

	/** Check if a station is a registered spaceport. */
	static bool IsSpaceport(StationID station);

	/** Check if a tile belongs to a spaceport station. */
	static bool IsSpaceportTile(TileIndex tile);

	/** Get the SpaceportInfo for a station. */
	static const SpaceportInfo *GetSpaceport(StationID station);
	static SpaceportInfo *GetSpaceportMutable(StationID station);

	/** Get all registered spaceports. */
	static const std::unordered_map<StationID, SpaceportInfo> &GetAllSpaceports();

	/** Number of registered spaceports. */
	static size_t Count();

	/**
	 * Record delivery of consumer / life-support goods to a spaceport.
	 * Fueling the virtual off-world trade satisfaction.
	 */
	static void RecordSupplyDelivery(StationID station, CargoType cargo, uint32_t amount);

	/**
	 * Periodic / monthly off-world trade loop.
	 * Generates exotic off-world cargo packets directly into the spaceport station's waiting bay.
	 */
	static void ProcessOffWorldTrade();

	/**
	 * Calculate off-world cargo production for a given spaceport.
	 */
	static uint32_t CalculateTradeCargoProduction(const SpaceportInfo &info);

	/**
	 * Resolve the preferred exotic off-world cargo type for the current game.
	 */
	static CargoType GetPreferredOffWorldCargo();

	/** Reset the manager (for new game / load). */
	static void Reset();

private:
	static std::unordered_map<StationID, SpaceportInfo> spaceports;
};

#endif /* PORTAL_SPACEPORT_MANAGER_H */
