/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint24_alien_biomes.cpp Unit tests for Sprint 24 alien biomes, procedural styling, and CST portal visuals. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/world_gen.h"
#include "../portal/portal_registry.h"
#include "../clear_map.h"
#include "../tree_map.h"
#include "../tile_map.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../void_map.h"
#include "../table/sprites.h"

#include "mock_environment.h"
#include "../safeguards.h"

TEST_CASE("Alien Biomes - PlanetManager GetTileBiome O(1) query")
{
	PlanetManager::Reset();
	Map::Allocate(128, 128);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Oaktree Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 4,
		.min_y = 4,
		.max_x = 123,
		.max_y = 40,
		.development_score = 1000
	};

	PlanetRegion r_industrial{
		.id = WorldID{1},
		.name = "Merredin Industrial",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 4,
		.min_y = 45,
		.max_x = 123,
		.max_y = 80,
		.development_score = 500
	};

	PlanetRegion r_frontier{
		.id = WorldID{2},
		.name = "Calyx Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 4,
		.min_y = 85,
		.max_x = 123,
		.max_y = 123,
		.development_score = 100
	};

	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_industrial));
	REQUIRE(PlanetManager::RegisterRegion(r_frontier));

	/* Core world: Temperate */
	CHECK(PlanetManager::GetTileBiome(TileXY(20, 20)) == WorldBiome::Temperate);
	CHECK(PlanetManager::GetTilePhase(TileXY(20, 20)) == WorldPhase::Phase1_Core);

	/* Industrial world: Arid Desert */
	CHECK(PlanetManager::GetTileBiome(TileXY(20, 60)) == WorldBiome::AridDesert);
	CHECK(PlanetManager::GetTilePhase(TileXY(20, 60)) == WorldPhase::Phase2_Developed);

	/* Frontier world: Sub-Arctic */
	CHECK(PlanetManager::GetTileBiome(TileXY(20, 100)) == WorldBiome::SubArctic);
	CHECK(PlanetManager::GetTilePhase(TileXY(20, 100)) == WorldPhase::Phase3_Frontier);

	/* Void buffer tile: defaults to Temperate */
	CHECK(PlanetManager::GetTileBiome(TileXY(20, 42)) == WorldBiome::Temperate);
}

TEST_CASE("Alien Biomes - Procedural MultiWorldGen Biome Environmental Styling")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	Map::Allocate(128, 128);

	/* Initialize playable inner tiles as base grass. OpenTTD requires the
	 * outer map edge to remain TileType::Void. */
	for (uint32_t y = 0; y < 128; ++y) {
		for (uint32_t x = 0; x < 128; ++x) {
			TileIndex t = TileXY(x, y);
			if (!IsInnerTile(t)) continue;

			MakeClear(t, ClearGround::Grass, 3);
			SetTropicZone(t, TropicZone::Normal);
		}
	}

	/* Run multi-world generator */
	MultiWorldGen::Config cfg;
	cfg.world_count = 3;
	cfg.place_gateways = true;

	bool ok = MultiWorldGen::GenerateMultiWorldLayout(128, 128, cfg);
	REQUIRE(ok);
	REQUIRE(PlanetManager::Count() == 3);

	const auto *core = PlanetManager::GetRegion(WorldID{0});
	const auto *industrial = PlanetManager::GetRegion(WorldID{1});
	const auto *frontier = PlanetManager::GetRegion(WorldID{2});

	REQUIRE(core != nullptr);
	REQUIRE(industrial != nullptr);
	REQUIRE(frontier != nullptr);

	/* 1. Core world (Temperate): grass with normal tropic zone */
	TileIndex core_tile = TileXY((core->min_x + core->max_x) / 2, (core->min_y + core->max_y) / 2);
	CHECK(PlanetManager::GetTileBiome(core_tile) == WorldBiome::Temperate);
	CHECK(GetTropicZone(core_tile) == TropicZone::Normal);
	CHECK(IsTileType(core_tile, TileType::Clear));
	CHECK(GetClearGround(core_tile) == ClearGround::Grass);
	CHECK_FALSE(IsSnowTile(core_tile));

	/* 2. Industrial world (Arid Desert): desert or rocks with desert tropic zone */
	TileIndex ind_tile = TileXY((industrial->min_x + industrial->max_x) / 2, (industrial->min_y + industrial->max_y) / 2);
	CHECK(PlanetManager::GetTileBiome(ind_tile) == WorldBiome::AridDesert);
	CHECK(GetTropicZone(ind_tile) == TropicZone::Desert);
	CHECK(IsTileType(ind_tile, TileType::Clear));
	ClearGround ind_ground = GetClearGround(ind_tile);
	CHECK((ind_ground == ClearGround::Desert || ind_ground == ClearGround::Rocks));
	CHECK_FALSE(IsSnowTile(ind_tile));

	/* 3. Frontier world (Sub-Arctic): snowy permafrost */
	TileIndex front_tile = TileXY((frontier->min_x + frontier->max_x) / 2, (frontier->min_y + frontier->max_y) / 2);
	CHECK(PlanetManager::GetTileBiome(front_tile) == WorldBiome::SubArctic);
	CHECK(IsTileType(front_tile, TileType::Clear));
	CHECK(IsSnowTile(front_tile));
}

TEST_CASE("Alien Biomes - Tree Foliage Transformation per Biome")
{
	Map::Allocate(64, 64);

	PlanetRegion r_des{
		.id = WorldID{0},
		.name = "Desert Colony",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 2,
		.min_y = 2,
		.max_x = 30,
		.max_y = 30
	};

	PlanetRegion r_arc{
		.id = WorldID{1},
		.name = "Arctic Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 34,
		.min_y = 2,
		.max_x = 62,
		.max_y = 30
	};

	TileIndex t_des = TileXY(10, 10);
	MakeTree(t_des, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);

	TileIndex t_arc = TileXY(40, 10);
	MakeTree(t_arc, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);

	MultiWorldGen::ApplyBiomeStyling(r_des);
	MultiWorldGen::ApplyBiomeStyling(r_arc);

	/* Desert world trees converted to Cacti */
	CHECK(IsTileType(t_des, TileType::Trees));
	CHECK(GetTreeType(t_des) == TREE_CACTUS);
	CHECK(GetTreeGround(t_des) == TreeGround::SnowOrDesert);

	/* Arctic world trees converted to Sub-Arctic conifers */
	CHECK(IsTileType(t_arc, TileType::Trees));
	CHECK(GetTreeType(t_arc) == TREE_SUB_ARCTIC);
	CHECK(GetTreeGround(t_arc) == TreeGround::SnowOrDesert);
}

TEST_CASE("Alien Biomes - CST Portal Gate Classification and Visual Styling")
{
	PortalRegistry::Reset();
	Map::Allocate(64, 64);

	TileIndex gate_a = TileXY(10, 10);
	TileIndex gate_b = TileXY(50, 10);

	/* Setup plain rail tunnels */
	MakeClear(gate_a, ClearGround::Grass, 3);
	MakeRailTunnel(gate_a, OWNER_NONE, DiagDirection::SW, RAILTYPE_BEGIN);

	MakeClear(gate_b, ClearGround::Grass, 3);
	MakeRailTunnel(gate_b, OWNER_NONE, DiagDirection::NE, RAILTYPE_BEGIN);

	/* Unlinked gate registration */
	PortalRegistry::RegisterUnlinkedGate(gate_a, DiagDirection::SW, WorldID{0});
	CHECK(PortalRegistry::IsUnlinkedGate(gate_a));
	CHECK_FALSE(PortalRegistry::IsPortalTile(gate_a));

	/* Register paired active link */
	PortalRegistry::RegisterPortalPair(gate_a, DiagDirection::SW, WorldID{0}, gate_b, DiagDirection::NE, WorldID{1}, 10, true);
	CHECK(PortalRegistry::IsPortalTile(gate_a));
	CHECK(PortalRegistry::IsPortalTile(gate_b));
	CHECK_FALSE(PortalRegistry::IsUnlinkedGate(gate_a));

	/* Verify palettes are canonical CST infrastructure colours */
	CHECK(PALETTE_TO_STRUCT_BLUE == 795);   ///< Active CST conduit excitation
	CHECK(PALETTE_TO_STRUCT_YELLOW == 801); ///< Unlinked standby amber excitation
}
