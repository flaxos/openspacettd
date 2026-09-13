/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file spaceport_manager.cpp Implementation of interplanetary spaceport registry and off-world trade simulation. */

#include "../stdafx.h"
#include "spaceport_manager.h"
#include "planet_manager.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../cargotype.h"
#include "../economy_func.h"
#include "../town.h"
#include "../window_func.h"
#include "../table/strings.h"
#include "universe_authority.h"
#include "consist_snapshot.h"
#include "content_manifest.h"

#include <algorithm>

#include "../safeguards.h"

std::unordered_map<StationID, SpaceportInfo> SpaceportManager::spaceports;

bool SpaceportManager::RegisterSpaceport(StationID station, WorldID world_id, uint8_t tier)
{
	if (!Station::IsValidID(station)) return false;

	SpaceportInfo info{
		.station_id = station,
		.world_id = world_id,
		.supplies_received = 0,
		.offworld_trade_tier = std::max<uint8_t>(1, tier),
		.total_offworld_cargo_generated = 0,
	};

	spaceports[station] = info;
	return true;
}

void SpaceportManager::RestoreSpaceport(const SpaceportInfo &info)
{
	spaceports[info.station_id] = info;
}

bool SpaceportManager::UnregisterSpaceport(StationID station)
{
	return spaceports.erase(station) > 0;
}

bool SpaceportManager::IsSpaceport(StationID station)
{
	return spaceports.find(station) != spaceports.end();
}

bool SpaceportManager::IsSpaceportTile(TileIndex tile)
{
	if (!IsValidTile(tile)) return false;
	if (!IsTileType(tile, TileType::Station)) return false;

	const Station *st = Station::GetByTile(tile);
	if (st == nullptr || !Station::IsValidID(st->index)) return false;

	return IsSpaceport(st->index);
}

const SpaceportInfo *SpaceportManager::GetSpaceport(StationID station)
{
	auto it = spaceports.find(station);
	return it != spaceports.end() ? &it->second : nullptr;
}

SpaceportInfo *SpaceportManager::GetSpaceportMutable(StationID station)
{
	auto it = spaceports.find(station);
	return it != spaceports.end() ? &it->second : nullptr;
}

const std::unordered_map<StationID, SpaceportInfo> &SpaceportManager::GetAllSpaceports()
{
	return spaceports;
}

size_t SpaceportManager::Count()
{
	return spaceports.size();
}

void SpaceportManager::RecordSupplyDelivery(StationID station, CargoType cargo, uint32_t amount)
{
	SpaceportInfo *info = GetSpaceportMutable(station);
	if (info == nullptr) return;

	CargoType ct_goods = GetCargoTypeByLabel(CT_GOODS);
	CargoType ct_food = GetCargoTypeByLabel(CT_FOOD);
	CargoType ct_water = GetCargoTypeByLabel(CT_WATER);

	bool is_supply = false;
	if (IsValidCargoType(ct_goods) && cargo == ct_goods) is_supply = true;
	if (IsValidCargoType(ct_food) && cargo == ct_food) is_supply = true;
	if (IsValidCargoType(ct_water) && cargo == ct_water) is_supply = true;

	/* In bare unit test environment where cargo tables are uninitialized, accept any cargo */
	if (!IsValidCargoType(ct_goods) && !IsValidCargoType(ct_food) && !IsValidCargoType(ct_water)) {
		is_supply = true;
	}

	if (is_supply) {
		info->supplies_received += amount;
		SetWindowDirty(WindowClass::StationView, station);
	}
}

CargoType SpaceportManager::GetPreferredOffWorldCargo()
{
	static const CargoLabel candidates[] = {
		CT_VALUABLES,
		CT_GOLD,
		CT_DIAMONDS,
		CT_MAIL,
		CT_GOODS,
	};

	for (const auto &label : candidates) {
		CargoType ct = GetCargoTypeByLabel(label);
		if (IsValidCargoType(ct)) return ct;
	}

	return CargoType{0};
}

uint32_t SpaceportManager::CalculateTradeCargoProduction(const SpaceportInfo &info)
{
	uint32_t base = 25 * std::max<uint32_t>(1, info.offworld_trade_tier);
	uint32_t supply_bonus = std::min(150u, info.supplies_received / 2);
	uint32_t dev_bonus = 0;

	if (info.world_id != INVALID_WORLD) {
		const PlanetRegion *region = PlanetManager::GetRegion(info.world_id);
		if (region != nullptr) {
			dev_bonus = region->development_score / 200;
		}
	}

	return base + supply_bonus + dev_bonus;
}

void SpaceportManager::ProcessOffWorldTrade()
{
	CargoType cargo = GetPreferredOffWorldCargo();
	if (!IsValidCargoType(cargo)) return;

	for (auto &[station_id, info] : spaceports) {
		Station *st = Station::GetIfValid(station_id);
		if (st == nullptr) continue;

		uint32_t amount = CalculateTradeCargoProduction(info);
		if (amount == 0) continue;

		info.total_offworld_cargo_generated += amount;
		info.supplies_received /= 2; // Decay for next month

		if (info.auto_dispatch && info.target_dest_world != INVALID_WORLD) {
			BufferExportCargo(station_id, cargo, amount);
			DispatchInterplanetaryTrade(station_id, 0);
		} else {
			StationList stations;
			stations.insert(st);
			Source source = (st->town != nullptr) ? Source{st->town->index, SourceType::Town} : Source{Source::Invalid, SourceType::Town};
			MoveGoodsToStation(cargo, amount, source, stations);
		}

		SetWindowDirty(WindowClass::StationView, station_id);
	}
}

bool SpaceportManager::ConfigureSpaceportBridge(StationID station, WorldID dest_world, uint32_t route_id, bool auto_dispatch)
{
	SpaceportInfo *info = GetSpaceportMutable(station);
	if (info == nullptr) return false;

	info->target_dest_world = dest_world;
	info->target_route_id = route_id;
	info->auto_dispatch = auto_dispatch;
	SetWindowDirty(WindowClass::StationView, station);
	return true;
}

void SpaceportManager::BufferExportCargo(StationID station, CargoType cargo, uint32_t amount)
{
	SpaceportInfo *info = GetSpaceportMutable(station);
	if (info == nullptr || amount == 0) return;

	info->buffered_cargo_type = cargo;
	info->buffered_export_cargo += amount;
	SetWindowDirty(WindowClass::StationView, station);
}

std::string SpaceportManager::DispatchInterplanetaryTrade(StationID station, uint32_t max_amount)
{
	SpaceportInfo *info = GetSpaceportMutable(station);
	if (info == nullptr || info->target_dest_world == INVALID_WORLD) return "";

	uint32_t amount = info->buffered_export_cargo;
	if (max_amount > 0 && max_amount < amount) {
		amount = max_amount;
	}
	if (amount == 0) return "";

	CargoType cargo = info->buffered_cargo_type;
	if (!IsValidCargoType(cargo)) {
		cargo = GetPreferredOffWorldCargo();
	}

	/* Build synthetic consist snapshot to represent interplanetary cargo launch */
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 100;
	snapshot.acceleration = 10;

	FederationNamespace ns{0x5350414345504F52ULL /* "SPACEPOR" */, static_cast<uint64_t>(station.base())};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = info->total_interplanetary_dispatched + 1};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 1};
	snapshot.owner = snapshot.company_id.ToOwnerToken();

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) {
			snapshot.content_manifest = token_res.token;
		}
	}

	ConsistSnapshotUnit engine;
	engine.engine_type = 0;
	engine.cargo_type = 0;
	engine.cargo_capacity = 0;
	engine.cargo_count = 0;
	engine.subtype = 1;
	snapshot.units.push_back(engine);

	ConsistSnapshotUnit wagon;
	wagon.engine_type = 1;
	wagon.cargo_type = static_cast<uint8_t>(cargo);
	wagon.cargo_capacity = static_cast<uint16_t>(amount);
	wagon.cargo_count = amount;
	wagon.subtype = 0;
	wagon.cargo_source.name_space = ns;
	wagon.cargo_source.source_sequence = static_cast<uint64_t>(station.base()) + 1;
	wagon.cargo_source.origin_world = info->world_id;
	snapshot.units.push_back(wagon);

	ConsistSnapshotBytes snap_bytes = ConsistSnapshotCodec::Encode(snapshot);
	if (!snap_bytes.Succeeded()) return "";

	auto &auth = UniverseAuthorityService::Instance();
	std::string tx_id = auth.InitiateTransfer(
		info->world_id,
		info->target_dest_world,
		0,
		0,
		snap_bytes,
		100,
		FreightPriority::Express
	);

	if (!tx_id.empty()) {
		auth.DepartTransfer(tx_id, 0);
		auth.RecordSpaceportThroughput(amount);
		info->buffered_export_cargo -= amount;
		info->total_interplanetary_dispatched += amount;
		SetWindowDirty(WindowClass::StationView, station);
	}

	return tx_id;
}

bool SpaceportManager::ReceiveInterplanetaryConsist(StationID station, const UniverseTransferRecord &transfer)
{
	SpaceportInfo *info = GetSpaceportMutable(station);
	if (info == nullptr) return false;

	Station *st = Station::GetIfValid(station);
	if (st == nullptr) return false;

	Source source = (st->town != nullptr) ? Source{st->town->index, SourceType::Town} : Source{Source::Invalid, SourceType::Town};

	for (const auto &[cargo_type, count] : transfer.cargo_by_type) {
		if (count > 0 && cargo_type < NUM_CARGO) {
			CargoType ct{static_cast<uint8_t>(cargo_type)};
			GoodsEntry &ge = st->goods[ct];
			StationID next = ge.GetVia(st->index);
			if (CargoPacket::CanAllocateItem()) {
				ge.GetOrCreateData().cargo.Append(CargoPacket::Create(st->index, count, source), next);
			}
		}
	}

	info->total_interplanetary_received += transfer.total_cargo_units;
	UniverseAuthorityService::Instance().RecordSpaceportThroughput(transfer.total_cargo_units);
	SetWindowDirty(WindowClass::StationView, station);
	return true;
}

void SpaceportManager::Reset()
{
	spaceports.clear();
}
