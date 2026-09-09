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
#include "../table/strings.h"

#include <algorithm>

#include "../safeguards.h"

std::unordered_map<TileIndex, EdgeConduit> EdgeConduitManager::conduits;
ConduitID EdgeConduitManager::next_conduit_id = 1;

bool EdgeConduitManager::IsVoidAdjacent(TileIndex tile)
{
	if (!IsValidTile(tile)) return false;

	uint x = TileX(tile);
	uint y = TileY(tile);

	/* Check outer map boundaries */
	if (x <= 1 || x >= Map::MaxX() - 1 || y <= 1 || y >= Map::MaxY() - 1) return true;

	/* Check orthogonal neighbors for void buffer space */
	for (DiagDirection d : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
		TileIndex nb = TileAddByDiagDir(tile, d);
		if (nb < Map::Size() && IsTileType(nb, TileType::Void)) return true;
	}

	return false;
}

ConduitID EdgeConduitManager::RegisterConduit(TileIndex tile, DiagDirection dir, WorldID world_id, CargoType cargo, Owner owner, uint32_t base_production)
{
	if (!IsValidTile(tile)) return INVALID_CONDUIT;

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
	conduits[conduit.tile] = conduit;
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
	for (auto &[tile, conduit] : conduits) {
		uint32_t amount = CalculateProduction(conduit);

		StationFinder finder(TileArea(conduit.tile, 1, 1));
		const StationList &stations = finder.GetStations();
		if (!stations.empty()) {
			MoveGoodsToStation(conduit.cargo_type, amount, {static_cast<SourceID>(conduit.id & 0xFFFF), SourceType::Industry}, stations);
			conduit.total_produced += amount;
		}
	}
}

void EdgeConduitManager::Reset()
{
	conduits.clear();
	next_conduit_id = 1;
}
