/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file edge_conduit.cpp Implementation of perimeter Edge Extraction Conduits. */

#include "../stdafx.h"
#include "edge_conduit.h"
#include "planet_manager.h"
#include "../map_func.h"
#include "../cargotype.h"
#include "../economy_func.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../window_func.h"
#include "../table/strings.h"
#include "universe_authority.h"
#include "consist_snapshot.h"
#include "content_manifest.h"

#include <algorithm>

#include "../safeguards.h"

std::unordered_map<TileIndex, EdgeConduit> EdgeConduitManager::conduits;
ConduitID EdgeConduitManager::next_conduit_id = 1;

bool EdgeConduitManager::IsVoidAdjacent(TileIndex tile)
{
	return ResolvePlacement(tile, DiagDirection::Invalid).has_value();
}

static TileIndex GetAdjacentMapTile(TileIndex tile, DiagDirection dir)
{
	if (tile >= Map::Size() || !IsValidDiagDirection(dir)) return INVALID_TILE;

	int x = static_cast<int>(TileX(tile));
	int y = static_cast<int>(TileY(tile));
	switch (dir) {
		case DiagDirection::NE: --x; break;
		case DiagDirection::SE: ++y; break;
		case DiagDirection::SW: ++x; break;
		case DiagDirection::NW: --y; break;
		default: return INVALID_TILE;
	}

	if (x < 0 || y < 0 || x > static_cast<int>(Map::MaxX()) || y > static_cast<int>(Map::MaxY())) return INVALID_TILE;
	return TileXY(x, y);
}

std::optional<EdgeConduitPlacement> EdgeConduitManager::ResolvePlacement(TileIndex tile, DiagDirection requested_dir)
{
	if (tile >= Map::Size() || !IsValidTile(tile) || !IsInnerTile(tile)) return std::nullopt;

	auto resolve_direction = [tile](DiagDirection dir) -> std::optional<EdgeConduitPlacement> {
		TileIndex void_tile = GetAdjacentMapTile(tile, dir);
		TileIndex approach_tile = GetAdjacentMapTile(tile, ReverseDiagDir(dir));
		if (void_tile == INVALID_TILE || void_tile >= Map::Size() || !IsTileType(void_tile, TileType::Void)) return std::nullopt;
		if (approach_tile == INVALID_TILE || !IsValidTile(approach_tile)) return std::nullopt;
		return EdgeConduitPlacement{dir, void_tile, approach_tile};
	};

	if (IsValidDiagDirection(requested_dir)) return resolve_direction(requested_dir);
	for (DiagDirection candidate : DIAGDIRECTIONS_ALL) {
		if (auto placement = resolve_direction(candidate); placement.has_value()) return placement;
	}
	return std::nullopt;
}

ConduitID EdgeConduitManager::RegisterConduit(TileIndex tile, DiagDirection dir, WorldID world_id, CargoType cargo, Owner owner, uint32_t base_production)
{
	if (!ResolvePlacement(tile, dir).has_value()) return INVALID_CONDUIT;
	if (world_id == INVALID_WORLD || PlanetManager::GetTileWorld(tile) != world_id) return INVALID_CONDUIT;

	ConduitID id = next_conduit_id++;
	EdgeConduit conduit{
		.id = id,
		.tile = tile,
		.dir = dir,
		.world_id = world_id,
		.cargo_type = cargo,
		.production_rate = std::max(10u, base_production),
		.owner = owner,
		.total_produced = 0,
	};

	conduits[tile] = conduit;
	return id;
}

void EdgeConduitManager::RestoreConduit(const EdgeConduit &conduit)
{
	if (conduit.id == INVALID_CONDUIT || !ResolvePlacement(conduit.tile, conduit.dir).has_value()) return;
	if (conduit.world_id == INVALID_WORLD || PlanetManager::GetTileWorld(conduit.tile) != conduit.world_id) return;
	EdgeConduit restored = conduit;
	if (restored.last_delivery_status >= ConduitDeliveryStatus::End) {
		restored.last_delivery_status = ConduitDeliveryStatus::NeverRun;
	}
	conduits[conduit.tile] = restored;
	if (conduit.id >= next_conduit_id) {
		next_conduit_id = conduit.id + 1;
	}
}

bool EdgeConduitManager::UnregisterConduit(TileIndex tile)
{
	return conduits.erase(tile) > 0;
}

bool EdgeConduitManager::IsConduitTile(TileIndex tile)
{
	return conduits.find(tile) != conduits.end();
}

const EdgeConduit *EdgeConduitManager::GetConduit(TileIndex tile)
{
	auto it = conduits.find(tile);
	return it != conduits.end() ? &it->second : nullptr;
}

EdgeConduit *EdgeConduitManager::GetConduitMutable(TileIndex tile)
{
	auto it = conduits.find(tile);
	return it != conduits.end() ? &it->second : nullptr;
}

const std::unordered_map<TileIndex, EdgeConduit> &EdgeConduitManager::GetAllConduits()
{
	return conduits;
}

size_t EdgeConduitManager::Count()
{
	return conduits.size();
}

CargoType EdgeConduitManager::GetPreferredMineralCargo()
{
	static const CargoLabel candidates[] = {
		CT_IRON_ORE,
		CT_COAL,
		CT_COPPER_ORE,
		CT_OIL,
	};

	for (const auto &label : candidates) {
		CargoType ct = GetCargoTypeByLabel(label);
		if (IsValidCargoType(ct)) return ct;
	}

	return CargoType{0};
}

uint32_t EdgeConduitManager::CalculateProduction(const EdgeConduit &conduit)
{
	WorldPhase phase = PlanetManager::GetTilePhase(conduit.tile);
	uint32_t amount = conduit.production_rate;

	/* Frontier & expansion worlds receive +100% extraction bonus for deep crustal richness */
	if (phase == WorldPhase::Phase3_Frontier || phase == WorldPhase::Phase4_Expansion) {
		amount *= 2;
	} else if (phase == WorldPhase::Phase1_Core) {
		amount = std::max(10u, amount / 2);
	}

	return amount;
}

void EdgeConduitManager::ProduceAllConduits()
{
	bool state_changed = false;
	for (auto &[tile, conduit] : conduits) {
		uint32_t amount = CalculateProduction(conduit);
		if (amount == 0) continue;
		conduit.last_month_potential = amount;
		conduit.last_month_allocated = 0;
		state_changed = true;

		if (conduit.direct_feeder_enabled && conduit.target_dest_world != INVALID_WORLD) {
			/* Direct inter-world feeder pipeline: bypass local station handling and inject directly into federation corridor */
			ConsistSnapshot snapshot;
			snapshot.direction = to_underlying(Direction::NE);
			snapshot.speed = 80;
			snapshot.acceleration = 12;

			FederationNamespace ns{0x434F4E4455495400ULL /* "CONDUIT\0" */, static_cast<uint64_t>(conduit.id)};
			snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = conduit.total_piped_interplanetary + 1};
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
			wagon.cargo_type = static_cast<uint8_t>(conduit.cargo_type);
			wagon.cargo_capacity = static_cast<uint16_t>(amount);
			wagon.cargo_count = amount;
			wagon.subtype = 0;
			wagon.cargo_source.name_space = ns;
			wagon.cargo_source.source_sequence = static_cast<uint64_t>(conduit.id) + 1;
			wagon.cargo_source.origin_world = conduit.world_id;
			wagon.cargo_source.origin_tile_x = TileX(conduit.tile);
			wagon.cargo_source.origin_tile_y = TileY(conduit.tile);
			snapshot.units.push_back(wagon);

			ConsistSnapshotBytes snap_bytes = ConsistSnapshotCodec::Encode(snapshot);
			if (snap_bytes.Succeeded()) {
				auto &auth = UniverseAuthorityService::Instance();
				std::string tx_id = auth.InitiateTransfer(
					conduit.world_id,
					conduit.target_dest_world,
					0,
					0,
					snap_bytes,
					100,
					FreightPriority::Bulk
				);

				if (!tx_id.empty()) {
					auth.DepartTransfer(tx_id, 0);
					auth.RecordEdgeConduitThroughput(amount);
					conduit.total_piped_interplanetary += amount;
					conduit.total_produced += amount;
					conduit.last_delivery_status = ConduitDeliveryStatus::DirectFeederDispatched;
				} else {
					conduit.last_delivery_status = ConduitDeliveryStatus::DirectFeederUnavailable;
				}
			} else {
				conduit.last_delivery_status = ConduitDeliveryStatus::DirectFeederUnavailable;
			}
		} else {
			StationFinder finder(TileArea(conduit.tile, 1, 1));
			const StationList &stations = finder.GetStations();
			MoveGoodsToStationResult result = MoveGoodsToStationDetailed(
				conduit.cargo_type,
				amount,
				{static_cast<SourceID>(conduit.id & 0xFFFF), SourceType::Industry},
				stations);
			conduit.last_month_allocated = result.moved;
			conduit.total_allocated += result.moved;

			if (result.candidate_stations == 0) {
				conduit.last_delivery_status = ConduitDeliveryStatus::NoCatchment;
			} else if (result.eligible_stations == 0) {
				conduit.last_delivery_status = ConduitDeliveryStatus::NoEligibleStation;
			} else if (result.packet_allocation_failed) {
				conduit.last_delivery_status = ConduitDeliveryStatus::PacketAllocationFailed;
			} else if (result.moved == 0) {
				conduit.last_delivery_status = ConduitDeliveryStatus::NoWholeUnitsAllocated;
			} else {
				conduit.last_delivery_status = ConduitDeliveryStatus::Allocated;
			}

			/* Retain the legacy nominal counter for compatibility with existing saves
			 * and federation tests. Player-facing delivery totals use total_allocated. */
			if (!stations.empty()) {
				conduit.total_produced += amount;
			}
		}
	}
	if (state_changed) InvalidateWindowData(WindowClass::LandInfo, 0, 1);
}

bool EdgeConduitManager::ConfigureDirectFeeder(TileIndex tile, bool enabled, WorldID dest_world, uint32_t route_id)
{
	EdgeConduit *conduit = GetConduitMutable(tile);
	if (conduit == nullptr) return false;

	conduit->direct_feeder_enabled = enabled;
	conduit->target_dest_world = dest_world;
	conduit->target_route_id = route_id;
	InvalidateWindowData(WindowClass::LandInfo, 0, 1);
	return true;
}

void EdgeConduitManager::Reset()
{
	conduits.clear();
	next_conduit_id = 1;
}
