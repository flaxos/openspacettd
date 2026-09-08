/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_planet_manager.cpp Unit tests for spatial planet region manager. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"

#include "../safeguards.h"

TEST_CASE("PlanetManager - Registration and Validation")
{
	PlanetManager::Reset();
	REQUIRE(PlanetManager::Count() == 0);

	PlanetRegion r1{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 511,
		.max_y = 511,
		.development_score = 1000
	};

	CHECK(PlanetManager::RegisterRegion(r1));
	CHECK(PlanetManager::Count() == 1);

	/* Duplicate ID registration must fail */
	PlanetRegion r_dup = r1;
	r_dup.min_x = 600;
	r_dup.max_x = 900;
	CHECK_FALSE(PlanetManager::RegisterRegion(r_dup));

	/* Overlapping bounding box registration must fail */
	PlanetRegion r_overlap{
		.id = WorldID{1},
		.name = "Colony Beta",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::SubArctic,
		.min_x = 400, // Overlaps with r1 (0..511)
		.min_y = 400,
		.max_x = 800,
		.max_y = 800,
		.development_score = 500
	};
	CHECK_FALSE(PlanetManager::RegisterRegion(r_overlap));

	/* Inverted bounds registration must fail */
	PlanetRegion r_inv{
		.id = WorldID{2},
		.name = "Broken World",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::Volcanic,
		.min_x = 700,
		.min_y = 700,
		.max_x = 600, // max < min
		.max_y = 600,
		.development_score = 0
	};
	CHECK_FALSE(PlanetManager::RegisterRegion(r_inv));

	/* Invalid WorldID registration must fail */
	PlanetRegion r_inval = r1;
	r_inval.id = INVALID_WORLD;
	r_inval.min_x = 1000;
	r_inval.max_x = 1500;
	CHECK_FALSE(PlanetManager::RegisterRegion(r_inval));

	PlanetManager::Reset();
}

TEST_CASE("PlanetManager - Multi-World Spatial Partitioning and Buffer Zones")
{
	Map::Allocate(2048, 2048);
	PlanetManager::Reset();

	/* 4 planetary worlds separated by 128-tile void buffers */
	PlanetRegion world_core{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 895,
		.max_y = 895,
		.development_score = 10000
	};

	PlanetRegion world_developed{
		.id = WorldID{1},
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 1024,
		.min_y = 0,
		.max_x = 1919,
		.max_y = 895,
		.development_score = 5000
	};

	PlanetRegion world_frontier{
		.id = WorldID{2},
		.name = "Ceres Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 0,
		.min_y = 1024,
		.max_x = 895,
		.max_y = 1919,
		.development_score = 1200
	};

	PlanetRegion world_expansion{
		.id = WorldID{3},
		.name = "Eden Wilderness",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Oceanic,
		.min_x = 1024,
		.min_y = 1024,
		.max_x = 1919,
		.max_y = 1919,
		.development_score = 0
	};

	REQUIRE(PlanetManager::RegisterRegion(world_core));
	REQUIRE(PlanetManager::RegisterRegion(world_developed));
	REQUIRE(PlanetManager::RegisterRegion(world_frontier));
	REQUIRE(PlanetManager::RegisterRegion(world_expansion));
	REQUIRE(PlanetManager::Count() == 4);

	/* Check points inside World 0 (Phase1_Core) */
	TileIndex tile_core_interior = TileXY(400, 400);
	CHECK(PlanetManager::GetTileWorld(tile_core_interior) == WorldID{0});
	CHECK(PlanetManager::GetTilePhase(tile_core_interior) == WorldPhase::Phase1_Core);
	const PlanetRegion *rc = PlanetManager::GetRegionByTile(tile_core_interior);
	REQUIRE(rc != nullptr);
	CHECK(rc->name == "Earth Prime");
	CHECK(rc->biome == WorldBiome::Temperate);

	/* Boundary corners of World 0 */
	CHECK(PlanetManager::GetTileWorld(TileXY(0, 0)) == WorldID{0});
	CHECK(PlanetManager::GetTileWorld(TileXY(895, 895)) == WorldID{0});
	CHECK(PlanetManager::GetTileWorld(TileXY(0, 895)) == WorldID{0});
	CHECK(PlanetManager::GetTileWorld(TileXY(895, 0)) == WorldID{0});

	/* Check points inside World 1 (Phase2_Developed) */
	TileIndex tile_dev_interior = TileXY(1500, 500);
	CHECK(PlanetManager::GetTileWorld(tile_dev_interior) == WorldID{1});
	CHECK(PlanetManager::GetTilePhase(tile_dev_interior) == WorldPhase::Phase2_Developed);
	const PlanetRegion *rd = PlanetManager::GetRegionByTile(tile_dev_interior);
	REQUIRE(rd != nullptr);
	CHECK(rd->name == "Vulcan Forge");

	/* Check points inside World 2 (Phase3_Frontier) */
	TileIndex tile_front_interior = TileXY(300, 1500);
	CHECK(PlanetManager::GetTileWorld(tile_front_interior) == WorldID{2});
	CHECK(PlanetManager::GetTilePhase(tile_front_interior) == WorldPhase::Phase3_Frontier);

	/* Check points inside World 3 (Phase4_Expansion) */
	TileIndex tile_exp_interior = TileXY(1600, 1600);
	CHECK(PlanetManager::GetTileWorld(tile_exp_interior) == WorldID{3});
	CHECK(PlanetManager::GetTilePhase(tile_exp_interior) == WorldPhase::Phase4_Expansion);

	/* Check Buffer Zone tiles between worlds (must resolve to INVALID_WORLD) */
	TileIndex buffer_x = TileXY(960, 400); // Between World 0 and World 1
	CHECK(PlanetManager::GetTileWorld(buffer_x) == INVALID_WORLD);
	CHECK(PlanetManager::GetRegionByTile(buffer_x) == nullptr);

	TileIndex buffer_y = TileXY(400, 960); // Between World 0 and World 2
	CHECK(PlanetManager::GetTileWorld(buffer_y) == INVALID_WORLD);
	CHECK(PlanetManager::GetRegionByTile(buffer_y) == nullptr);

	TileIndex buffer_intersection = TileXY(960, 960); // Intersection of void buffer bands
	CHECK(PlanetManager::GetTileWorld(buffer_intersection) == INVALID_WORLD);
	CHECK(PlanetManager::GetRegionByTile(buffer_intersection) == nullptr);

	/* Check boundary edge exactness */
	CHECK(PlanetManager::GetTileWorld(TileXY(895, 400)) == WorldID{0});
	CHECK(PlanetManager::GetTileWorld(TileXY(896, 400)) == INVALID_WORLD);
	CHECK(PlanetManager::GetTileWorld(TileXY(1023, 400)) == INVALID_WORLD);
	CHECK(PlanetManager::GetTileWorld(TileXY(1024, 400)) == WorldID{1});

	PlanetManager::Reset();
}

TEST_CASE("PlanetManager - Arbitrary Non-Cell-Aligned Boundary Precision")
{
	Map::Allocate(128, 128);
	PlanetManager::Reset();

	/* World region spanning non-multiple-of-64 tile coordinates: 15..77 */
	PlanetRegion r{
		.id = WorldID{5},
		.name = "Odd Colony",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 15,
		.min_y = 20,
		.max_x = 77,
		.max_y = 85,
		.development_score = 42
	};
	REQUIRE(PlanetManager::RegisterRegion(r));

	/* Just outside boundary */
	CHECK(PlanetManager::GetTileWorld(TileXY(14, 50)) == INVALID_WORLD);
	CHECK(PlanetManager::GetTileWorld(TileXY(78, 50)) == INVALID_WORLD);
	CHECK(PlanetManager::GetTileWorld(TileXY(50, 19)) == INVALID_WORLD);
	CHECK(PlanetManager::GetTileWorld(TileXY(50, 86)) == INVALID_WORLD);

	/* Exactly on boundary */
	CHECK(PlanetManager::GetTileWorld(TileXY(15, 50)) == WorldID{5});
	CHECK(PlanetManager::GetTileWorld(TileXY(77, 50)) == WorldID{5});
	CHECK(PlanetManager::GetTileWorld(TileXY(50, 20)) == WorldID{5});
	CHECK(PlanetManager::GetTileWorld(TileXY(50, 85)) == WorldID{5});

	PlanetManager::Reset();
}
