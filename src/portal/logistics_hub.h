/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file logistics_hub.h Domain model and manager for dedicated company logistics hubs and warehouses. */

#ifndef LOGISTICS_HUB_H
#define LOGISTICS_HUB_H

#include "../cargo_type.h"
#include "../company_type.h"
#include "../station_type.h"
#include "../tile_type.h"
#include "planet_type.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <optional>

/**
 * A dedicated company-owned logistics hub or warehouse on a planetary world.
 * Serves as an active bi-directional inventory buffer between passing freight consists
 * and the company's planetary stockpile.
 */
struct LogisticsHub {
	uint32_t hub_id = 0;
	TileIndex tile = INVALID_TILE;
	WorldID world_id = INVALID_WORLD;
	CompanyID company_id = CompanyID::Invalid();
	StationID station_id = StationID::Invalid();
	std::string name;

	/** Configurable reserve floor thresholds per cargo. Consists cannot withdraw below this minimum. */
	std::map<CargoType, uint32_t> reserve_floors;

	uint64_t total_deposited = 0;
	uint64_t total_dispatched = 0;
};

/**
 * Manager for registering, querying, and operating planetary company logistics hubs.
 */
class LogisticsHubManager {
public:
	/** Reset all registered logistics hubs (e.g. on new game or load). */
	static void Reset();

	/**
	 * Register a new logistics hub facility.
	 * @return Assigned hub_id, or 0 on failure.
	 */
	static uint32_t RegisterHub(TileIndex tile, WorldID world, CompanyID company, StationID st, const std::string &name);
	/** Resolve an owned rail station within four platform tiles of the hub. */
	static StationID ResolveStation(TileIndex tile, WorldID world, CompanyID company, StationID requested);
	/** True only while the saved attachment still meets the construction rules. */
	static bool ValidateForStation(const LogisticsHub &hub);
	static void RemoveForStation(StationID station);
	static void RefreshForStation(StationID station);
	static void ChangeCompanyOwner(CompanyID old_owner, CompanyID new_owner);
	static void ValidateAfterLoad();

	/**
	 * Remove a logistics hub (e.g. upon demolition).
	 */
	static bool RemoveHub(uint32_t hub_id);

	/**
	 * Find logistics hub by its unique ID.
	 */
	static const LogisticsHub *GetHub(uint32_t hub_id);

	/**
	 * Find logistics hub located at a specific tile.
	 */
	static const LogisticsHub *GetHubAtTile(TileIndex tile);

	/**
	 * Find logistics hub attached to a specific station ID.
	 */
	static const LogisticsHub *GetHubForStation(StationID st);

	/**
	 * Check if a company has an established logistics hub on a specific world.
	 */
	static bool HasLogisticsHub(WorldID world, CompanyID company);

	/**
	 * Set the minimum reserve floor threshold for a specific cargo at a hub.
	 */
	static void SetReserveFloor(uint32_t hub_id, CargoType cargo, uint32_t min_amount);

	/**
	 * Get the reserve floor threshold for a specific cargo at a hub.
	 */
	static uint32_t GetReserveFloor(uint32_t hub_id, CargoType cargo);

	/**
	 * Ingest cargo from a delivering train into the world's company stockpile.
	 * @return true if deposited successfully.
	 */
	static bool DepositToStockpile(TileIndex tile, CompanyID company, CargoType cargo, uint32_t amount);

	/**
	 * Withdraw surplus cargo from the world's stockpile to load onto an outgoing train.
	 * Honors the configured reserve floor for the hub.
	 * @return Amount of cargo successfully withdrawn and made available for loading.
	 */
	static uint32_t WithdrawFromStockpile(TileIndex tile, CompanyID company, CargoType cargo, uint32_t max_amount);

	/**
	 * Retrieve all registered hubs for serialization and UI display.
	 */
	static std::vector<LogisticsHub> GetAllHubs();

	/**
	 * Restore a hub during savegame deserialization.
	 */
	static void RestoreHub(const LogisticsHub &hub);
};

#endif /* LOGISTICS_HUB_H */
