/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_identity.cpp Stable identities for future inter-server consist transfer. */

#include "../stdafx.h"
#include "federation_identity.h"
#include "federation_cargo.h"

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
std::map<uint32_t, FederationNamespace> _consist_namespaces;
std::map<uint8_t, uint64_t> _company_mappings;
std::map<uint32_t, uint64_t> _station_mappings;
std::map<uint32_t, uint64_t> _source_mappings;
std::map<uint32_t, uint64_t> _depot_mappings;
std::map<GlobalConsistID, std::vector<GlobalOrderDestinationID>> _consist_global_schedules;
std::map<uint8_t, FederationNamespace> _company_namespaces;
std::map<uint32_t, GlobalStationID> _station_aliases;

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
	VehicleID best_anchor = VehicleID::Invalid();
	for (const auto &[anchor_id, sequence] : _consist_mappings) {
		const Train *anchor = Train::GetIfValid(VehicleID{anchor_id});
		if (AnchorBelongsTo(anchor, front) && (!best_sequence.has_value() || sequence < *best_sequence)) {
			best_sequence = sequence;
			best_anchor = VehicleID{anchor_id};
		}
	}
	return best_sequence.has_value() ? std::optional<GlobalConsistID>{GlobalConsistID{GetAnchorNamespace(best_anchor), *best_sequence}} : std::nullopt;
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
					preferred_destination->name_space == GetAnchorNamespace(VehicleID{anchor_id})) {
				keep = anchor_id;
				break;
			}
			if (sequence < _consist_mappings.at(keep)) keep = anchor_id;
		}
		for (uint32_t anchor_id : anchors) {
			if (anchor_id != keep) {
				_consist_mappings.erase(anchor_id);
				_consist_namespaces.erase(anchor_id);
			}
		}
	}
}

void FederationIdentityRegistry::ReleaseVehicle(VehicleID vehicle)
{
	_consist_mappings.erase(vehicle.base());
	_consist_namespaces.erase(vehicle.base());
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

FederationNamespace FederationIdentityRegistry::GetAnchorNamespace(VehicleID anchor)
{
	auto it = _consist_namespaces.find(anchor.base());
	return it == _consist_namespaces.end() ? GetNamespace() : it->second;
}

bool FederationIdentityRegistry::RestoreMapping(VehicleID anchor, uint64_t sequence, FederationNamespace name_space)
{
	if (anchor == VehicleID::Invalid() || sequence == 0) return false;
	_consist_mappings[anchor.base()] = sequence;
	_consist_namespaces.erase(anchor.base());
	if (name_space.IsValid() && name_space != GetNamespace()) {
		_consist_namespaces[anchor.base()] = name_space;
		return true;
	}
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
			_consist_namespaces.erase(it->first);
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
			_consist_namespaces.erase(old_anchor);
			known->second = it->first;
			++it;
		} else {
			_consist_namespaces.erase(it->first);
			it = _consist_mappings.erase(it);
		}
	}
}

std::optional<GlobalCompanyID> FederationIdentityRegistry::FindCompany(CompanyID company)
{
	if (company == CompanyID::Invalid() || company >= MAX_COMPANIES) return std::nullopt;
	auto it = _company_mappings.find(company.base());
	if (it == _company_mappings.end()) return std::nullopt;
	auto ns = _company_namespaces.find(company.base());
	return GlobalCompanyID{ns == _company_namespaces.end() ? GetNamespace() : ns->second, it->second};
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
	_company_namespaces.erase(company.base());
}

const std::map<uint8_t, uint64_t> &FederationIdentityRegistry::GetCompanyMappings()
{
	return _company_mappings;
}

bool FederationIdentityRegistry::RestoreCompanyMapping(CompanyID company, uint64_t sequence, FederationNamespace name_space)
{
	if (company == CompanyID::Invalid() || sequence == 0) return false;
	_company_mappings[company.base()] = sequence;
	_company_namespaces.erase(company.base());
	if (name_space.IsValid() && name_space != GetNamespace()) {
		_company_namespaces[company.base()] = name_space;
		return true;
	}
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
			_company_namespaces.erase(it->first);
			it = _company_mappings.erase(it);
		} else {
			++it;
		}
	}
}

std::optional<GlobalStationID> FederationIdentityRegistry::FindStation(StationID station)
{
	if (station == StationID::Invalid()) return std::nullopt;
	if (auto alias = _station_aliases.find(station.base()); alias != _station_aliases.end()) return alias->second;
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
	_station_aliases.erase(station.base());
}

const std::map<uint32_t, uint64_t> &FederationIdentityRegistry::GetStationMappings()
{
	return _station_mappings;
}

bool FederationIdentityRegistry::RestoreStationMapping(StationID station, uint64_t sequence, FederationNamespace name_space, WorldID world)
{
	if (station == StationID::Invalid() || sequence == 0) return false;
	if (name_space.IsValid() && name_space != GetNamespace() && world == INVALID_WORLD) return false;
	_station_mappings[station.base()] = sequence;
	_station_aliases.erase(station.base());
	if (name_space.IsValid() && name_space != GetNamespace()) {
		_station_aliases[station.base()] = {name_space, sequence, world};
		return true;
	}
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
			_station_aliases.erase(it->first);
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
	for (const auto &[local, sequence] : _station_mappings) {
		const StationID id{static_cast<StationID::BaseType>(local)};
		if (!BaseStation::IsValidID(id)) continue;
		const auto identity = FindStation(id);
		if (identity && *identity == global_st) return id;
	}
	return std::nullopt;
}

std::optional<DestinationID> FederationIdentityRegistry::ResolveOrderDestination(const GlobalOrderDestinationID &order, WorldID current_world)
{
	if (!order.IsValid()) return std::nullopt;
	if (order.type == OrderDestinationType::Station || order.type == OrderDestinationType::Waypoint) {
		/* A foreign station requires an explicit local gate-station mapping.
		 * Never substitute a nearby station, a truncated tile, or a coincident pool ID. */
		if (auto station = ResolveStation(order.station_id)) return DestinationID(*station);
	} else if (order.type == OrderDestinationType::Depot && order.name_space == GetNamespace() && order.target_world == current_world) {
		for (const auto &[local, sequence] : _depot_mappings) {
			DepotID id{static_cast<DepotID::BaseType>(local)};
			if (sequence == order.destination_sequence && Depot::IsValidID(id)) return DestinationID(id);
		}
	}
	return std::nullopt;
}

void FederationIdentityRegistry::SetConsistSchedule(GlobalConsistID consist_id, std::vector<GlobalOrderDestinationID> schedule)
{
	if (!consist_id.IsValid()) return;
	_consist_global_schedules[consist_id] = std::move(schedule);
}

std::optional<std::vector<GlobalOrderDestinationID>> FederationIdentityRegistry::GetConsistSchedule(GlobalConsistID consist_id)
{
	auto it = _consist_global_schedules.find(consist_id);
	if (it == _consist_global_schedules.end()) return std::nullopt;
	return it->second;
}

void FederationIdentityRegistry::ClearConsistSchedule(GlobalConsistID consist_id)
{
	_consist_global_schedules.erase(consist_id);
}

const std::map<GlobalConsistID, std::vector<GlobalOrderDestinationID>> &FederationIdentityRegistry::GetConsistSchedules()
{
	return _consist_global_schedules;
}

void FederationIdentityRegistry::PruneStaleSchedules()
{
	std::erase_if(_consist_global_schedules, [](const auto &schedule) {
		for (const auto &[anchor, sequence] : _consist_mappings) {
			if (sequence == schedule.first.sequence && GetAnchorNamespace(VehicleID{anchor}) == schedule.first.name_space) return false;
		}
		return true;
	});
}

void FederationIdentityRegistry::RestoreCounters(uint64_t next_company, uint64_t next_station, uint64_t next_source)
{
	if (next_company != 0) _next_company_sequence = next_company;
	if (next_station != 0) _next_station_sequence = next_station;
	if (next_source != 0) _next_source_sequence = next_source;
}

void FederationIdentityRegistry::Reset()
{
	FederationCargoRegistry::Reset();
	_federation_namespace = {};
	_next_consist_sequence = 1;
	_next_company_sequence = 1;
	_next_station_sequence = 1;
	_next_source_sequence = 1;
	_next_depot_sequence = 1;
	_consist_mappings.clear();
	_consist_namespaces.clear();
	_company_mappings.clear();
	_company_namespaces.clear();
	_station_mappings.clear();
	_station_aliases.clear();
	_source_mappings.clear();
	_depot_mappings.clear();
	_consist_global_schedules.clear();
}
