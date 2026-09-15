/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file logistics_hub.cpp Implementation of dedicated company logistics hub and warehouse buffering. */

#include "../stdafx.h"
#include "logistics_hub.h"
#include "company_stockpile.h"
#include "planet_manager.h"

#include <algorithm>
#include <map>
#include <mutex>

static std::map<uint32_t, LogisticsHub> _logistics_hubs;
static uint32_t _next_hub_id = 1;
static std::mutex _hub_mutex;

void LogisticsHubManager::Reset()
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	_logistics_hubs.clear();
	_next_hub_id = 1;
}

uint32_t LogisticsHubManager::RegisterHub(TileIndex tile, WorldID world, CompanyID company, StationID st, const std::string &name)
{
	if (tile == INVALID_TILE || world == INVALID_WORLD || company == CompanyID::Invalid()) return 0;

	std::lock_guard<std::mutex> lock(_hub_mutex);
	uint32_t id = _next_hub_id++;
	LogisticsHub hub{
		.hub_id = id,
		.tile = tile,
		.world_id = world,
		.company_id = company,
		.station_id = st,
		.name = name.empty() ? ("Logistics Hub #" + std::to_string(id)) : name,
		.reserve_floors = {},
	};
	_logistics_hubs[id] = hub;
	return id;
}

bool LogisticsHubManager::RemoveHub(uint32_t hub_id)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	return _logistics_hubs.erase(hub_id) > 0;
}

const LogisticsHub *LogisticsHubManager::GetHub(uint32_t hub_id)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	auto it = _logistics_hubs.find(hub_id);
	return (it != _logistics_hubs.end()) ? &it->second : nullptr;
}

const LogisticsHub *LogisticsHubManager::GetHubAtTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return nullptr;

	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (const auto &[id, hub] : _logistics_hubs) {
		if (hub.tile == tile) {
			return &hub;
		}
	}
	return nullptr;
}

const LogisticsHub *LogisticsHubManager::GetHubForStation(StationID st)
{
	if (st == StationID::Invalid()) return nullptr;

	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (const auto &[id, hub] : _logistics_hubs) {
		if (hub.station_id == st) {
			return &hub;
		}
	}
	return nullptr;
}

bool LogisticsHubManager::HasLogisticsHub(WorldID world, CompanyID company)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (const auto &[id, hub] : _logistics_hubs) {
		if (hub.world_id == world && hub.company_id == company) {
			return true;
		}
	}
	return false;
}

void LogisticsHubManager::SetReserveFloor(uint32_t hub_id, CargoType cargo, uint32_t min_amount)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	auto it = _logistics_hubs.find(hub_id);
	if (it != _logistics_hubs.end()) {
		it->second.reserve_floors[cargo] = min_amount;
	}
}

uint32_t LogisticsHubManager::GetReserveFloor(uint32_t hub_id, CargoType cargo)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	auto it = _logistics_hubs.find(hub_id);
	if (it == _logistics_hubs.end()) return 0;
	auto fit = it->second.reserve_floors.find(cargo);
	return (fit != it->second.reserve_floors.end()) ? fit->second : 0;
}

bool LogisticsHubManager::DepositToStockpile(TileIndex tile, CompanyID company, CargoType cargo, uint32_t amount)
{
	if (tile == INVALID_TILE || company == CompanyID::Invalid() || amount == 0) return false;

	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (auto &[id, hub] : _logistics_hubs) {
		if (hub.tile == tile && hub.company_id == company) {
			StockpileManager::AddCargo(hub.world_id, company, cargo, amount);
			hub.total_deposited += amount;
			return true;
		}
	}

	/* Fallback: if hub tile not exact, resolve planet region directly from tile */
	const PlanetRegion *region = PlanetManager::GetRegionByTile(tile);
	if (region != nullptr) {
		StockpileManager::AddCargo(region->id, company, cargo, amount);
		return true;
	}

	return false;
}

uint32_t LogisticsHubManager::WithdrawFromStockpile(TileIndex tile, CompanyID company, CargoType cargo, uint32_t max_amount)
{
	if (tile == INVALID_TILE || company == CompanyID::Invalid() || max_amount == 0) return 0;

	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (auto &[id, hub] : _logistics_hubs) {
		if (hub.tile == tile && hub.company_id == company) {
			uint32_t reserve_floor = 0;
			auto r_it = hub.reserve_floors.find(cargo);
			if (r_it != hub.reserve_floors.end()) {
				reserve_floor = r_it->second;
			}

			uint32_t current_stock = StockpileManager::GetStock(hub.world_id, company, cargo);
			if (current_stock <= reserve_floor) {
				return 0; // Reserve floor protected!
			}

			uint32_t available_surplus = current_stock - reserve_floor;
			uint32_t to_withdraw = std::min(max_amount, available_surplus);
			uint32_t actual = StockpileManager::WithdrawCargo(hub.world_id, company, cargo, to_withdraw);
			hub.total_dispatched += actual;
			return actual;
		}
	}

	return 0;
}

std::vector<LogisticsHub> LogisticsHubManager::GetAllHubs()
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	std::vector<LogisticsHub> result;
	result.reserve(_logistics_hubs.size());
	for (const auto &[id, hub] : _logistics_hubs) {
		result.push_back(hub);
	}
	return result;
}

void LogisticsHubManager::RestoreHub(const LogisticsHub &hub)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	_logistics_hubs[hub.hub_id] = hub;
	if (hub.hub_id >= _next_hub_id) {
		_next_hub_id = hub.hub_id + 1;
	}
}
