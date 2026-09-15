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
#include "../company_base.h"
#include "../map_func.h"
#include "../station_base.h"
#include "../station_map.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <limits>
#include <set>

static std::map<uint32_t, LogisticsHub> _logistics_hubs;
static uint32_t _next_hub_id = 1;
static std::mutex _hub_mutex;

/* Call with _hub_mutex held. The requested-ID path is also used by runtime validation. */
static StationID ResolveStationUnlocked(TileIndex tile, WorldID world, CompanyID company, StationID requested)
{
	if (tile >= Map::Size() || world == INVALID_WORLD || !Company::IsValidID(company) || PlanetManager::GetTileWorld(tile) != world) return StationID::Invalid();
	const uint x = TileX(tile), y = TileY(tile);
	StationID best = StationID::Invalid();
	uint best_distance = 5;
	const uint min_x = x > 4 ? x - 4 : 0, min_y = y > 4 ? y - 4 : 0;
	const uint max_x = std::min<uint>(Map::MaxX(), x + 4), max_y = std::min<uint>(Map::MaxY(), y + 4);
	/* Only real owned rail platforms count; a moved sign or sparse station rectangle does not. */
	for (uint py = min_y; py <= max_y; ++py) {
		for (uint px = min_x; px <= max_x; ++px) {
			uint distance = (px > x ? px - x : x - px) + (py > y ? py - y : y - py);
			if (distance > 4) continue;
			TileIndex platform = TileXY(px, py);
			if (!IsRailStationTile(platform) || GetTileOwner(platform) != company || PlanetManager::GetTileWorld(platform) != world) continue;
			StationID id = GetStationIndex(platform);
			if (requested != StationID::Invalid() && id != requested) continue;
			if (requested == StationID::Invalid()) {
				bool occupied = false;
				for (const auto &[hub_id, hub] : _logistics_hubs) {
					if (hub.station_id == id) { occupied = true; break; }
				}
				if (occupied) continue;
			}
			const Station *station = Station::GetIfValid(id);
			if (station == nullptr || station->owner != company || !station->facilities.Test(StationFacility::Train) || station->train_station.IsEmpty() || station->xy >= Map::Size() || PlanetManager::GetTileWorld(station->xy) != world) continue;
			if (distance < best_distance || (distance == best_distance && (best == StationID::Invalid() || id < best))) {
				best = id;
				best_distance = distance;
			}
		}
	}
	return best;
}

StationID LogisticsHubManager::ResolveStation(TileIndex tile, WorldID world, CompanyID company, StationID requested)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	return ResolveStationUnlocked(tile, world, company, requested);
}

static bool ValidateForStationUnlocked(const LogisticsHub &hub)
{
	return hub.hub_id != 0 && hub.station_id != StationID::Invalid() && ResolveStationUnlocked(hub.tile, hub.world_id, hub.company_id, hub.station_id) == hub.station_id;
}

bool LogisticsHubManager::ValidateForStation(const LogisticsHub &hub)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	return ValidateForStationUnlocked(hub);
}

void LogisticsHubManager::Reset()
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	_logistics_hubs.clear();
	_next_hub_id = 1;
}

uint32_t LogisticsHubManager::RegisterHub(TileIndex tile, WorldID world, CompanyID company, StationID st, const std::string &name)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	if (st == StationID::Invalid() || ResolveStationUnlocked(tile, world, company, st) != st) return 0;
	for (const auto &[id, existing] : _logistics_hubs) {
		if (existing.tile == tile || existing.station_id == st) return 0;
	}
	/* Saved IDs may reach UINT32_MAX. Cycle to one and skip occupied IDs. */
	uint32_t id = _next_hub_id == 0 ? 1 : _next_hub_id;
	const uint32_t first = id;
	while (_logistics_hubs.contains(id)) {
		id = id == std::numeric_limits<uint32_t>::max() ? 1 : id + 1;
		if (id == first) return 0;
	}
	_next_hub_id = id == std::numeric_limits<uint32_t>::max() ? 1 : id + 1;
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
		if (hub.station_id == st && ValidateForStationUnlocked(hub)) {
			return &hub;
		}
	}
	return nullptr;
}

bool LogisticsHubManager::HasLogisticsHub(WorldID world, CompanyID company)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (const auto &[id, hub] : _logistics_hubs) {
		if (hub.world_id == world && hub.company_id == company && ValidateForStationUnlocked(hub)) {
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
		if (hub.tile == tile && hub.company_id == company && ValidateForStationUnlocked(hub)) {
			StockpileManager::AddCargo(hub.world_id, company, cargo, amount);
			hub.total_deposited += amount;
			return true;
		}
	}

	return false;
}

uint32_t LogisticsHubManager::WithdrawFromStockpile(TileIndex tile, CompanyID company, CargoType cargo, uint32_t max_amount)
{
	if (tile == INVALID_TILE || company == CompanyID::Invalid() || max_amount == 0) return 0;

	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (auto &[id, hub] : _logistics_hubs) {
		if (hub.tile == tile && hub.company_id == company && ValidateForStationUnlocked(hub)) {
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
	if (hub.hub_id == std::numeric_limits<uint32_t>::max()) {
		_next_hub_id = 1;
	} else if (hub.hub_id >= _next_hub_id) {
		_next_hub_id = hub.hub_id + 1;
	}
}

void LogisticsHubManager::RemoveForStation(StationID station)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (auto it = _logistics_hubs.begin(); it != _logistics_hubs.end();) {
		it = it->second.station_id == station ? _logistics_hubs.erase(it) : std::next(it);
	}
}

void LogisticsHubManager::RefreshForStation(StationID station)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (auto it = _logistics_hubs.begin(); it != _logistics_hubs.end();) {
		it = it->second.station_id == station && !ValidateForStationUnlocked(it->second) ? _logistics_hubs.erase(it) : std::next(it);
	}
}

void LogisticsHubManager::ChangeCompanyOwner(CompanyID old_owner, CompanyID new_owner)
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	for (auto it = _logistics_hubs.begin(); it != _logistics_hubs.end();) {
		if (it->second.company_id != old_owner) { ++it; continue; }
		if (!Company::IsValidID(new_owner)) { it = _logistics_hubs.erase(it); continue; }
		it->second.company_id = new_owner;
		it = ValidateForStationUnlocked(it->second) ? std::next(it) : _logistics_hubs.erase(it);
	}
}

void LogisticsHubManager::ValidateAfterLoad()
{
	std::lock_guard<std::mutex> lock(_hub_mutex);
	std::set<StationID> stations;
	std::set<TileIndex> tiles;
	for (auto it = _logistics_hubs.begin(); it != _logistics_hubs.end();) {
		const LogisticsHub &hub = it->second;
		/* Ascending map order retains the lowest valid ID for each station/tile. */
		if (!ValidateForStationUnlocked(hub) || stations.contains(hub.station_id) || tiles.contains(hub.tile)) {
			it = _logistics_hubs.erase(it);
		} else {
			stations.insert(hub.station_id);
			tiles.insert(hub.tile);
			++it;
		}
	}
	_next_hub_id = 1;
	if (!_logistics_hubs.empty()) {
		uint32_t last = _logistics_hubs.rbegin()->first;
		_next_hub_id = last == std::numeric_limits<uint32_t>::max() ? 1 : last + 1;
	}
}
