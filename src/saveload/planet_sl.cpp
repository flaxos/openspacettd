/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file planet_sl.cpp Code handling saving and loading of planetary worlds, wormholes, and consist transit. */

#include "../stdafx.h"

#include "saveload.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"

#include "../safeguards.h"

/** Temporary storage for PlanetRegion serialization. */
struct SlPlanetRegion {
	uint32_t id;
	std::string name;
	uint8_t phase;
	uint8_t biome;
	uint32_t min_x;
	uint32_t min_y;
	uint32_t max_x;
	uint32_t max_y;
	uint32_t development_score;
};

static const SaveLoad _planet_region_desc[] = {
	    SLE_VAR(SlPlanetRegion, id,                VarTypes::U32),
	   SLE_SSTR(SlPlanetRegion, name,              VarTypes::STR),
	    SLE_VAR(SlPlanetRegion, phase,             VarTypes::U8),
	    SLE_VAR(SlPlanetRegion, biome,             VarTypes::U8),
	    SLE_VAR(SlPlanetRegion, min_x,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, min_y,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, max_x,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, max_y,             VarTypes::U32),
	    SLE_VAR(SlPlanetRegion, development_score, VarTypes::U32),
};

/** Chunk handler for planetary worlds (PLNT). */
struct PLNTChunkHandler : ChunkHandler {
	PLNTChunkHandler() : ChunkHandler("PLNT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_planet_region_desc);

		int i = 0;
		for (const auto &r : PlanetManager::GetAllRegions()) {
			SlPlanetRegion sl_reg{
				.id = r.id.base(),
				.name = r.name,
				.phase = to_underlying(r.phase),
				.biome = to_underlying(r.biome),
				.min_x = r.min_x,
				.min_y = r.min_y,
				.max_x = r.max_x,
				.max_y = r.max_y,
				.development_score = r.development_score,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_reg, _planet_region_desc);
		}
	}

	void Load() const override
	{
		PlanetManager::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_planet_region_desc);

		SlPlanetRegion sl_reg{};
		while (SlIterateArray() != -1) {
			sl_reg = {};
			SlObject(&sl_reg, slt);
			PlanetRegion r{
				.id = WorldID{sl_reg.id},
				.name = sl_reg.name,
				.phase = static_cast<WorldPhase>(sl_reg.phase),
				.biome = static_cast<WorldBiome>(sl_reg.biome),
				.min_x = sl_reg.min_x,
				.min_y = sl_reg.min_y,
				.max_x = sl_reg.max_x,
				.max_y = sl_reg.max_y,
				.development_score = sl_reg.development_score,
			};
			PlanetManager::RegisterRegion(r);
		}
		PlanetManager::RebuildSpatialGrid();
	}
};

/** Temporary storage for PortalLink serialization. */
struct SlPortalLink {
	uint32_t id;
	uint32_t tile_a;
	uint8_t dir_a;
	uint32_t world_a;
	uint32_t tile_b;
	uint8_t dir_b;
	uint32_t world_b;
	uint32_t virtual_length;
	uint8_t bidirectional;
};

static const SaveLoad _portal_link_desc[] = {
	    SLE_VAR(SlPortalLink, id,             VarTypes::U32),
	    SLE_VAR(SlPortalLink, tile_a,         VarTypes::U32),
	    SLE_VAR(SlPortalLink, dir_a,          VarTypes::U8),
	    SLE_VAR(SlPortalLink, world_a,        VarTypes::U32),
	    SLE_VAR(SlPortalLink, tile_b,         VarTypes::U32),
	    SLE_VAR(SlPortalLink, dir_b,          VarTypes::U8),
	    SLE_VAR(SlPortalLink, world_b,        VarTypes::U32),
	    SLE_VAR(SlPortalLink, virtual_length, VarTypes::U32),
	    SLE_VAR(SlPortalLink, bidirectional,  VarTypes::U8),
};

/** Chunk handler for portal wormhole links (PORT). */
struct PORTChunkHandler : ChunkHandler {
	PORTChunkHandler() : ChunkHandler("PORT", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_portal_link_desc);

		int i = 0;
		for (const auto &[id, link] : PortalRegistry::GetAllPortals()) {
			SlPortalLink sl_link{
				.id = link.id.base(),
				.tile_a = link.end_a.tile.base(),
				.dir_a = to_underlying(link.end_a.enter_dir),
				.world_a = link.end_a.world_id.base(),
				.tile_b = link.end_b.tile.base(),
				.dir_b = to_underlying(link.end_b.enter_dir),
				.world_b = link.end_b.world_id.base(),
				.virtual_length = link.virtual_length,
				.bidirectional = static_cast<uint8_t>(link.bidirectional ? 1 : 0),
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_link, _portal_link_desc);
		}

		for (const auto &[tile, endpoint] : PortalRegistry::GetUnlinkedGates()) {
			SlPortalLink sl_link{
				.id = 0,
				.tile_a = endpoint.tile.base(),
				.dir_a = to_underlying(endpoint.enter_dir),
				.world_a = endpoint.world_id.base(),
				.tile_b = INVALID_TILE.base(),
				.dir_b = 0,
				.world_b = INVALID_WORLD.base(),
				.virtual_length = 0,
				.bidirectional = 0,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_link, _portal_link_desc);
		}
	}

	void Load() const override
	{
		PortalRegistry::Reset();
		const std::vector<SaveLoad> slt = SlTableHeader(_portal_link_desc);

		SlPortalLink sl_link{};
		while (SlIterateArray() != -1) {
			sl_link = {};
			SlObject(&sl_link, slt);

			if (TileIndex{sl_link.tile_b} == INVALID_TILE) {
				PortalRegistry::RegisterUnlinkedGate(
					TileIndex{sl_link.tile_a},
					static_cast<DiagDirection>(sl_link.dir_a),
					WorldID{sl_link.world_a}
				);
			} else {
				PortalLink link;
				link.id = PortalID{sl_link.id};
				link.end_a = PortalEndpoint{
					TileIndex{sl_link.tile_a},
					static_cast<DiagDirection>(sl_link.dir_a),
					WorldID{sl_link.world_a},
				};
				link.end_b = PortalEndpoint{
					TileIndex{sl_link.tile_b},
					static_cast<DiagDirection>(sl_link.dir_b),
					WorldID{sl_link.world_b},
				};
				link.virtual_length = sl_link.virtual_length;
				link.bidirectional = (sl_link.bidirectional != 0);

				PortalRegistry::RestorePortalLink(link);
			}
		}
	}
};

/** Temporary storage for vehicle in-flight transit progress. */
struct SlPortalTransit {
	uint32_t veh_id;
	uint32_t progress;
};

static const SaveLoad _portal_transit_desc[] = {
	    SLE_VAR(SlPortalTransit, veh_id,   VarTypes::U32),
	    SLE_VAR(SlPortalTransit, progress, VarTypes::U32),
};

/** Chunk handler for in-flight consist transit progress (PRTX). */
struct PRTXChunkHandler : ChunkHandler {
	PRTXChunkHandler() : ChunkHandler("PRTX", ChunkType::Table) {}

	void Save() const override
	{
		SlTableHeader(_portal_transit_desc);

		int i = 0;
		for (const auto &[veh_id, progress] : PortalRegistry::GetAllVehicleTransit()) {
			SlPortalTransit sl_transit{
				.veh_id = veh_id,
				.progress = progress,
			};
			SlSetArrayIndex(i++);
			SlObject(&sl_transit, _portal_transit_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_portal_transit_desc);

		SlPortalTransit sl_transit{};
		while (SlIterateArray() != -1) {
			sl_transit = {};
			SlObject(&sl_transit, slt);
			PortalRegistry::SetVehicleTransitProgress(VehicleID{sl_transit.veh_id}, sl_transit.progress);
		}
	}
};

static const PLNTChunkHandler PLNT;
static const PORTChunkHandler PORT;
static const PRTXChunkHandler PRTX;

static const ChunkHandlerRef planet_chunk_handlers[] = {
	PLNT,
	PORT,
	PRTX,
};

extern const ChunkHandlerTable _planet_chunk_handlers(planet_chunk_handlers);
