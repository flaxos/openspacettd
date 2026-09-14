/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_identity.cpp Stable identities for future inter-server consist transfer. */

#include "../stdafx.h"
#include "federation_identity.h"

#include "../company_base.h"
#include "../depot_base.h"
#include "../industry.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../settings_type.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../town.h"
#include "../train.h"
#include "../vehicle_base.h"
#include "planet_manager.h"
#include "portal_registry.h"

#include "../safeguards.h"

namespace {

FederationNamespace _federation_namespace{};
uint64_t _next_consist_sequence = 1;
uint64_t _next_company_sequence = 1;
uint64_t _next_station_sequence = 1;
uint64_t _next_source_sequence = 1;
uint64_t _next_depot_sequence = 1;
std::map<uint32_t, uint64_t> _consist_mappings;
std::map<uint8_t, uint64_t> _company_mappings;
std::map<uint32_t, uint64_t> _station_mappings;
std::map<uint32_t, uint64_t> _source_mappings;
std::map<uint32_t, uint64_t> _depot_mappings;
std::map<uint64_t, std::vector<GlobalOrderDestinationID>> _consist_global_schedules;

static uint64_t Mix64(uint64_t value)
{
	value += 0x9E3779B97F4A7C15ULL;
	value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
	value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
	return value ^ (value >> 31);
}

static const Train *CanonicalFront(const Train *train)
{
	if (train == nullptr) return nullptr;
	const Train *front = train->First();
	return front != nullptr && front->IsFrontEngine() ? front : nullptr;
}

static bool AnchorBelongsTo(const Train *anchor, const Train *front)
{
	return anchor != nullptr && front != nullptr && anchor->First() == front;
}

} // namespace

FederationNamespace FederationIdentityRegistry::DeriveNamespace(uint32_t generation_seed, uint32_t map_x, uint32_t map_y, int32_t starting_year)
{
	uint64_t geometry = (static_cast<uint64_t>(map_x) << 32) | map_y;
	uint64_t calendar = static_cast<uint32_t>(starting_year);
	FederationNamespace result{
		Mix64((static_cast<uint64_t>(generation_seed) << 32) ^ geometry ^ 0x4F53545444464944ULL),
		Mix64(geometry ^ (calendar << 32) ^ generation_seed ^ 0x534156454E414D45ULL),
	};
	if (!result.IsValid()) result.low = 1;
	return result;
}

void FederationIdentityRegistry::EnsureNamespace()
{
	if (_federation_namespace.IsValid()) return;
	if (!_game_session_stats.savegame_id.empty()) {
		uint64_t high = 0xCBF29CE484222325ULL;
		uint64_t low = 0x84222325CBF29CE4ULL;
		for (uint8_t byte : _game_session_stats.savegame_id) {
			high = (high ^ byte) * 0x100000001B3ULL;
			low = (low ^ (byte + 0x9D)) * 0x100000001B3ULL;
		}
		_federation_namespace = {Mix64(high), Mix64(low)};
		if (_federation_namespace.IsValid()) return;
	}
	_federation_namespace = DeriveNamespace(_settings_game.game_creation.generation_seed, Map::SizeX(), Map::SizeY(),
			_settings_game.game_creation.starting_year.base());
}

FederationNamespace FederationIdentityRegistry::GetNamespace()
{
	EnsureNamespace();
	return _federation_namespace;
}

uint64_t FederationIdentityRegistry::GetNextSequence()
{
	return _next_consist_sequence;
}

uint64_t FederationIdentityRegistry::GetNextCompanySequence()
{
	return _next_company_sequence;
}

uint64_t FederationIdentityRegistry::GetNextStationSequence()
{
	return _next_station_sequence;
}

uint64_t FederationIdentityRegistry::GetNextSourceSequence()
{
	return _next_source_sequence;
}

GlobalOwnerToken GlobalCompanyID::ToOwnerToken() const
{
	GlobalOwnerToken token{};
	if (!this->IsValid()) return token;
	uint64_t high = this->name_space.high ^ Mix64(this->sequence);
	uint64_t low = this->name_space.low ^ Mix64(this->sequence + 0x9E3779B97F4A7C15ULL);
	for (int i = 0; i < 8; ++i) {
		token[i] = static_cast<uint8_t>(high >> (i * 8));
		token[8 + i] = static_cast<uint8_t>(low >> (i * 8));
	}
	return token;
}

std::optional<GlobalConsistID> FederationIdentityRegistry::Find(const Train *train)
{
	const Train *front = CanonicalFront(train);
	if (front == nullptr) return std::nullopt;

	std::optional<uint64_t> best_sequence;
	for (const auto &[anchor_id, sequence] : _consist_mappings) {
		const Train *anchor = Train::GetIfValid(VehicleID{anchor_id});
		if (AnchorBelongsTo(anchor, front) && (!best_sequence.has_value() || sequence < *best_sequence)) best_sequence = sequence;
	}
	return best_sequence.has_value() ? std::optional<GlobalConsistID>{GlobalConsistID{GetNamespace(), *best_sequence}} : std::nullopt;
}

std::optional<GlobalConsistID> FederationIdentityRegistry::GetOrCreate(const Train *train)
{
	const Train *front = CanonicalFront(train);
	if (front == nullptr) return std::nullopt;
	if (auto existing = Find(front); existing.has_value()) return existing;

	EnsureNamespace();
	if (_next_consist_sequence == 0) return std::nullopt;
	uint64_t sequence = _next_consist_sequence++;
	_consist_mappings[front->index.base()] = sequence;
	return GlobalConsistID{_federation_namespace, sequence};
}

void FederationIdentityRegistry::ReconcileConsistChange(const Train *source, const Train *destination,
		std::optional<GlobalConsistID> preferred_destination)
{
	const Train *source_front = CanonicalFront(source);
	const Train *destination_front = CanonicalFront(destination);

	/* A split needs no rewrite: the stable anchor follows its resulting chain.
	 * A merge can bring two mappings into one chain; retain the explicitly
	 * preferred destination identity, or the oldest sequence as a fallback. */
	for (const Train *front : {source_front, destination_front}) {
		if (front == nullptr) continue;
		std::vector<uint32_t> anchors;
		for (const auto &[anchor_id, sequence] : _consist_mappings) {
			const Train *anchor = Train::GetIfValid(VehicleID{anchor_id});
			if (AnchorBelongsTo(anchor, front)) anchors.push_back(anchor_id);
		}
		if (anchors.size() < 2) continue;

		uint32_t keep = anchors.front();
		for (uint32_t anchor_id : anchors) {
			uint64_t sequence = _consist_mappings.at(anchor_id);
			if (preferred_destination.has_value() && sequence == preferred_destination->sequence &&
					preferred_destination->name_space == GetNamespace()) {
				keep = anchor_id;
				break;
			}
			if (sequence < _consist_mappings.at(keep)) keep = anchor_id;
		}
		for (uint32_t anchor_id : anchors) {
			if (anchor_id != keep) _consist_mappings.erase(anchor_id);
		}
	}
}

void FederationIdentityRegistry::ReleaseVehicle(VehicleID vehicle)
{
	_consist_mappings.erase(vehicle.base());
}

const std::map<uint32_t, uint64_t> &FederationIdentityRegistry::GetMappings()
{
	return _consist_mappings;
}

void FederationIdentityRegistry::RestoreState(FederationNamespace name_space, uint64_t next_sequence)
{
	_federation_namespace = name_space;
	_next_consist_sequence = next_sequence;
}

bool FederationIdentityRegistry::RestoreMapping(VehicleID anchor, uint64_t sequence)
{
	if (anchor == VehicleID::Invalid() || sequence == 0) return false;
	_consist_mappings[anchor.base()] = sequence;
	if (sequence == UINT64_MAX) {
		_next_consist_sequence = 0;
	} else if (_next_consist_sequence != 0) {
		_next_consist_sequence = std::max(_next_consist_sequence, sequence + 1);
	}
	return true;
}

void FederationIdentityRegistry::PruneStaleMappings()
{
	for (auto it = _consist_mappings.begin(); it != _consist_mappings.end();) {
		const Train *anchor = Train::GetIfValid(VehicleID{it->first});
		if (CanonicalFront(anchor) == nullptr) {
			it = _consist_mappings.erase(it);
		} else {
			++it;
		}
	}

	/* Broken or legacy state may contain two anchors that now belong to one
	 * consist. Keep the oldest identity as the deterministic fallback. */
	std::map<uint32_t, uint32_t> front_to_anchor;
	for (auto it = _consist_mappings.begin(); it != _consist_mappings.end();) {
		const Train *anchor = Train::GetIfValid(VehicleID{it->first});
		uint32_t front_id = anchor->First()->index.base();
		auto [known, inserted] = front_to_anchor.emplace(front_id, it->first);
		if (inserted) {
			++it;
			continue;
		}

		uint32_t old_anchor = known->second;
		if (it->second < _consist_mappings.at(old_anchor)) {
			_consist_mappings.erase(old_anchor);
			known->second = it->first;
			++it;
		} else {
			it = _consist_mappings.erase(it);
		}
	}
}

std::optional<GlobalCompanyID> FederationIdentityRegistry::FindCompany(CompanyID company)
{
	if (company == CompanyID::Invalid() || company >= MAX_COMPANIES) return std::nullopt;
	auto it = _company_mappings.find(company.base());
	if (it == _company_mappings.end()) return std::nullopt;
	return GlobalCompanyID{GetNamespace(), it->second};
}

std::optional<GlobalCompanyID> FederationIdentityRegistry::GetOrCreateCompany(CompanyID company)
{
	if (company == CompanyID::Invalid() || company >= MAX_COMPANIES) return std::nullopt;
	if (auto existing = FindCompany(company); existing.has_value()) return existing;

	EnsureNamespace();
	if (_next_company_sequence == 0) return std::nullopt;
	uint64_t sequence = _next_company_sequence++;
	_company_mappings[company.base()] = sequence;
	return GlobalCompanyID{_federation_namespace, sequence};
}

void FederationIdentityRegistry::ReleaseCompany(CompanyID company)
{
	_company_mappings.erase(company.base());
}

const std::map<uint8_t, uint64_t> &FederationIdentityRegistry::GetCompanyMappings()
{
	return _company_mappings;
}

bool FederationIdentityRegistry::RestoreCompanyMapping(CompanyID company, uint64_t sequence)
{
	if (company == CompanyID::Invalid() || sequence == 0) return false;
	_company_mappings[company.base()] = sequence;
	if (sequence == UINT64_MAX) {
		_next_company_sequence = 0;
	} else if (_next_company_sequence != 0) {
		_next_company_sequence = std::max(_next_company_sequence, sequence + 1);
	}
	return true;
}

void FederationIdentityRegistry::PruneStaleCompanyMappings()
{
	for (auto it = _company_mappings.begin(); it != _company_mappings.end();) {
		if (!Company::IsValidID(CompanyID{it->first})) {
			it = _company_mappings.erase(it);
		} else {
			++it;
		}
	}
}

std::optional<GlobalStationID> FederationIdentityRegistry::FindStation(StationID station)
{
	if (station == StationID::Invalid()) return std::nullopt;
	auto it = _station_mappings.find(station.base());
	if (it == _station_mappings.end()) return std::nullopt;

	WorldID world = WorldID{0};
	const Station *st = Station::GetIfValid(station);
	if (st != nullptr && st->xy != INVALID_TILE) {
		const PlanetRegion *region = PlanetManager::GetRegionByTile(st->xy);
		if (region != nullptr) world = region->id;
	}

	return GlobalStationID{GetNamespace(), it->second, world};
}

std::optional<GlobalStationID> FederationIdentityRegistry::GetOrCreateStation(StationID station)
{
	if (station == StationID::Invalid()) return std::nullopt;
	if (auto existing = FindStation(station); existing.has_value()) return existing;

	EnsureNamespace();
	if (_next_station_sequence == 0) return std::nullopt;
	uint64_t sequence = _next_station_sequence++;
	_station_mappings[station.base()] = sequence;

	WorldID world = WorldID{0};
	const Station *st = Station::GetIfValid(station);
	if (st != nullptr && st->xy != INVALID_TILE) {
		const PlanetRegion *region = PlanetManager::GetRegionByTile(st->xy);
		if (region != nullptr) world = region->id;
	}

	return GlobalStationID{_federation_namespace, sequence, world};
}

void FederationIdentityRegistry::ReleaseStation(StationID station)
{
	_station_mappings.erase(station.base());
}

const std::map<uint32_t, uint64_t> &FederationIdentityRegistry::GetStationMappings()
{
	return _station_mappings;
}

bool FederationIdentityRegistry::RestoreStationMapping(StationID station, uint64_t sequence)
{
	if (station == StationID::Invalid() || sequence == 0) return false;
	_station_mappings[station.base()] = sequence;
	if (sequence == UINT64_MAX) {
		_next_station_sequence = 0;
	} else if (_next_station_sequence != 0) {
		_next_station_sequence = std::max(_next_station_sequence, sequence + 1);
	}
	return true;
}

void FederationIdentityRegistry::PruneStaleStationMappings()
{
	for (auto it = _station_mappings.begin(); it != _station_mappings.end();) {
		if (Station::GetIfValid(StationID{static_cast<uint16_t>(it->first)}) == nullptr) {
			it = _station_mappings.erase(it);
		} else {
			++it;
		}
	}
}

GlobalCargoSourceID FederationIdentityRegistry::CreateCargoSource(StationID first_station, Source source, TileIndex source_xy)
{
	EnsureNamespace();
	GlobalCargoSourceID result;
	result.name_space = _federation_namespace;

	if (first_station != StationID::Invalid()) {
		if (auto st = GetOrCreateStation(first_station); st.has_value()) {
			result.origin_station = *st;
			result.origin_world = st->world_id;
		}
	}

	if (source.IsValid()) {
		result.source_type = source.type;
		uint32_t key = (static_cast<uint32_t>(source.type) << 16) | (source.id & 0xFFFF);
		auto it = _source_mappings.find(key);
		if (it != _source_mappings.end()) {
			result.source_sequence = it->second;
		} else {
			uint64_t seq = _next_source_sequence++;
			_source_mappings[key] = seq;
			result.source_sequence = seq;
		}
	}

	if (source_xy != INVALID_TILE) {
		result.origin_tile_x = TileX(source_xy);
		result.origin_tile_y = TileY(source_xy);
		const PlanetRegion *region = PlanetManager::GetRegionByTile(source_xy);
		if (region != nullptr) {
			result.origin_world = region->id;
		}
	} else if (result.origin_station.IsValid()) {
		const Station *st = Station::GetIfValid(first_station);
		if (st != nullptr && st->xy != INVALID_TILE) {
			result.origin_tile_x = TileX(st->xy);
			result.origin_tile_y = TileY(st->xy);
		}
	}

	return result;
}

const std::map<uint32_t, uint64_t> &FederationIdentityRegistry::GetSourceMappings()
{
	return _source_mappings;
}

bool FederationIdentityRegistry::RestoreSourceMapping(uint32_t source_key, uint64_t sequence)
{
	if (sequence == 0) return false;
	_source_mappings[source_key] = sequence;
	if (sequence == UINT64_MAX) {
		_next_source_sequence = 0;
	} else if (_next_source_sequence != 0) {
		_next_source_sequence = std::max(_next_source_sequence, sequence + 1);
	}
	return true;
}

void FederationIdentityRegistry::PruneStaleSourceMappings()
{
	for (auto it = _source_mappings.begin(); it != _source_mappings.end();) {
		SourceType type = static_cast<SourceType>(it->first >> 16);
		uint16_t id = static_cast<uint16_t>(it->first & 0xFFFF);
		bool valid = false;
		switch (type) {
			case SourceType::Industry:
				valid = Industry::IsValidID(IndustryID{id});
				break;
			case SourceType::Town:
				valid = Town::IsValidID(TownID{id});
				break;
			case SourceType::Headquarters:
				valid = Company::IsValidID(CompanyID{static_cast<uint8_t>(id)});
				break;
			default:
				break;
		}
		if (!valid) {
			it = _source_mappings.erase(it);
		} else {
			++it;
		}
	}
}

std::optional<GlobalOrderDestinationID> FederationIdentityRegistry::FindOrderDestination(DestinationID dest, OrderType order_type)
{
	switch (order_type) {
		case OT_GOTO_STATION:
		case OT_IMPLICIT: {
			auto st = FindStation(dest.ToStationID());
			if (!st.has_value()) return std::nullopt;
			return GlobalOrderDestinationID::ForStation(*st, false);
		}
		case OT_GOTO_WAYPOINT: {
			auto st = FindStation(dest.ToStationID());
			if (!st.has_value()) return std::nullopt;
			return GlobalOrderDestinationID::ForStation(*st, true);
		}
		case OT_GOTO_DEPOT: {
			DepotID depot = dest.ToDepotID();
			auto it = _depot_mappings.find(depot.base());
			if (it == _depot_mappings.end()) return std::nullopt;
			return GlobalOrderDestinationID::ForDepot(GetNamespace(), it->second, WorldID{0});
		}
		default:
			return std::nullopt;
	}
}

std::optional<GlobalOrderDestinationID> FederationIdentityRegistry::GetOrCreateOrderDestination(DestinationID dest, OrderType order_type)
{
	switch (order_type) {
		case OT_GOTO_STATION:
		case OT_IMPLICIT: {
			auto st = GetOrCreateStation(dest.ToStationID());
			if (!st.has_value()) return std::nullopt;
			return GlobalOrderDestinationID::ForStation(*st, false);
		}
		case OT_GOTO_WAYPOINT: {
			auto st = GetOrCreateStation(dest.ToStationID());
			if (!st.has_value()) return std::nullopt;
			return GlobalOrderDestinationID::ForStation(*st, true);
		}
		case OT_GOTO_DEPOT: {
			EnsureNamespace();
			DepotID depot = dest.ToDepotID();
			uint64_t seq = 0;
			auto it = _depot_mappings.find(depot.base());
			if (it != _depot_mappings.end()) {
				seq = it->second;
			} else {
				seq = _next_depot_sequence++;
				_depot_mappings[depot.base()] = seq;
			}
			return GlobalOrderDestinationID::ForDepot(_federation_namespace, seq, WorldID{0});
		}
		default:
			return std::nullopt;
	}
}

std::optional<StationID> FederationIdentityRegistry::FindStationBySequence(uint64_t sequence)
{
	for (const auto &[st_base, seq] : _station_mappings) {
		if (seq == sequence) {
			StationID st_id{static_cast<StationID::BaseType>(st_base)};
			if (BaseStation::IsValidID(st_id)) return st_id;
		}
	}
	return std::nullopt;
}

std::optional<StationID> FederationIdentityRegistry::ResolveStation(const GlobalStationID &global_st)
{
	if (!global_st.IsValid()) return std::nullopt;
	if (global_st.name_space == GetNamespace()) {
		auto found = FindStationBySequence(global_st.sequence);
		if (found.has_value()) return found;
	}
	StationID direct_id{static_cast<StationID::BaseType>(global_st.sequence)};
	if (BaseStation::IsValidID(direct_id)) return direct_id;

	return std::nullopt;
}

static StationID FindStationNearTile(TileIndex tile, uint radius = 15)
{
	if (!IsValidTile(tile)) return StationID::Invalid();
	int tx = TileX(tile);
	int ty = TileY(tile);
	int min_x = std::max<int>(0, tx - radius);
	int max_x = std::min<int>(Map::MaxX(), tx + radius);
	int min_y = std::max<int>(0, ty - radius);
	int max_y = std::min<int>(Map::MaxY(), ty + radius);

	for (int y = min_y; y <= max_y; ++y) {
		for (int x = min_x; x <= max_x; ++x) {
			TileIndex t = TileXY(x, y);
			if (HasStationTileRail(t) || IsRailWaypointTile(t)) {
				StationID st_id = GetStationIndex(t);
				if (BaseStation::IsValidID(st_id)) return st_id;
			}
		}
	}
	return StationID::Invalid();
}

std::optional<DestinationID> FederationIdentityRegistry::ResolveOrderDestination(const GlobalOrderDestinationID &order, WorldID current_world)
{
	if (!order.IsValid()) return std::nullopt;

	/* 1. If the destination targets the current local world */
	if (order.target_world == current_world || order.target_world == INVALID_WORLD || order.target_world == DEFAULT_WORLD) {
		switch (order.type) {
			case OrderDestinationType::Station:
			case OrderDestinationType::Waypoint: {
				if (order.station_id.IsValid()) {
					if (auto st = ResolveStation(order.station_id); st.has_value()) {
						return DestinationID(*st);
					}
				}
				if (auto st = FindStationBySequence(order.destination_sequence); st.has_value()) {
					return DestinationID(*st);
				}
				StationID direct_id{static_cast<StationID::BaseType>(order.destination_sequence)};
				if (BaseStation::IsValidID(direct_id)) return DestinationID(direct_id);
				break;
			}
			case OrderDestinationType::Depot: {
				for (const auto &[depot_base, seq] : _depot_mappings) {
					if (seq == order.destination_sequence) {
						DepotID d{static_cast<DepotID::BaseType>(depot_base)};
						if (Depot::IsValidID(d)) return DestinationID(d);
					}
				}
				DepotID direct_id{static_cast<DepotID::BaseType>(order.destination_sequence)};
				if (Depot::IsValidID(direct_id)) return DestinationID(direct_id);
				break;
			}
			case OrderDestinationType::PortalGate: {
				for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
					if (link.local_endpoint.world_id == current_world && link.id.base() == order.destination_sequence) {
						StationID nearby_st = FindStationNearTile(tile);
						if (nearby_st != StationID::Invalid()) return DestinationID(nearby_st);
						return DestinationID(StationID{static_cast<uint16_t>(tile.base())});
					}
				}
				break;
			}
			default:
				break;
		}
	} else {
		/* 2. Destination targets a REMOTE world!
		 * On current_world, route towards the inter-server portal gate connecting to order.target_world. */
		for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
			if (link.local_endpoint.world_id == current_world && link.remote_world == order.target_world) {
				StationID nearby_st = FindStationNearTile(tile);
				if (nearby_st != StationID::Invalid()) return DestinationID(nearby_st);
				return DestinationID(StationID{static_cast<uint16_t>(tile.base())});
			}
		}
	}

	return std::nullopt;
}

void FederationIdentityRegistry::SetConsistSchedule(uint64_t consist_seq, std::vector<GlobalOrderDestinationID> schedule)
{
	if (consist_seq == 0) return;
	_consist_global_schedules[consist_seq] = std::move(schedule);
}

std::optional<std::vector<GlobalOrderDestinationID>> FederationIdentityRegistry::GetConsistSchedule(uint64_t consist_seq)
{
	auto it = _consist_global_schedules.find(consist_seq);
	if (it == _consist_global_schedules.end()) return std::nullopt;
	return it->second;
}

void FederationIdentityRegistry::ClearConsistSchedule(uint64_t consist_seq)
{
	_consist_global_schedules.erase(consist_seq);
}

void FederationIdentityRegistry::RestoreCounters(uint64_t next_company, uint64_t next_station, uint64_t next_source)
{
	if (next_company != 0) _next_company_sequence = next_company;
	if (next_station != 0) _next_station_sequence = next_station;
	if (next_source != 0) _next_source_sequence = next_source;
}

void FederationIdentityRegistry::Reset()
{
	_federation_namespace = {};
	_next_consist_sequence = 1;
	_next_company_sequence = 1;
	_next_station_sequence = 1;
	_next_source_sequence = 1;
	_next_depot_sequence = 1;
	_consist_mappings.clear();
	_company_mappings.clear();
	_station_mappings.clear();
	_source_mappings.clear();
	_depot_mappings.clear();
	_consist_global_schedules.clear();
}
