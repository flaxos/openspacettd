/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_placement_restrictions.cpp Unit tests for cross-world asset placement restrictions. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../table/strings.h"
#include "../rail_cmd.h"

#include "../safeguards.h"

TEST_CASE("Placement Restrictions - Industry Rules Matrix")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	/* Phase 1: Core World (0..399, 0..399) */
	PlanetRegion core_world{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 399,
		.max_y = 399,
		.development_score = 10000
	};

	/* Phase 2: Developed World (500..899, 0..399) */
	PlanetRegion dev_world{
		.id = WorldID{1},
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 500,
		.min_y = 0,
		.max_x = 899,
		.max_y = 399,
		.development_score = 5000
	};

	/* Phase 3: Frontier World (0..399, 500..899) */
	PlanetRegion frontier_world{
		.id = WorldID{2},
		.name = "Ceres Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 0,
		.min_y = 500,
		.max_x = 399,
		.max_y = 899,
		.development_score = 1000
	};

	REQUIRE(PlanetManager::RegisterRegion(core_world));
	REQUIRE(PlanetManager::RegisterRegion(dev_world));
	REQUIRE(PlanetManager::RegisterRegion(frontier_world));

	TileIndex core_tile = TileXY(200, 200);
	TileIndex dev_tile = TileXY(700, 200);
	TileIndex frontier_tile = TileXY(200, 700);
	TileIndex void_tile = TileXY(450, 200); // In buffer zone between Core and Developed

	/* 1. Core World Restrictions:
	 * Raw extractive / bio-farms are prohibited on Core Worlds.
	 * Processing factories are permitted. */
	CommandCost core_raw = PlanetManager::CheckIndustryPlacement(core_tile, true, false);
	CHECK(core_raw.Failed());
	CHECK(core_raw.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_CORE_WORLD);

	CommandCost core_proc = PlanetManager::CheckIndustryPlacement(core_tile, false, true);
	CHECK(core_proc.Succeeded());

	/* 2. Frontier World Restrictions:
	 * Raw extractive / bio-farms are permitted on Frontier Worlds.
	 * Heavy processing factories are prohibited on Frontier Worlds. */
	CommandCost front_raw = PlanetManager::CheckIndustryPlacement(frontier_tile, true, false);
	CHECK(front_raw.Succeeded());

	CommandCost front_proc = PlanetManager::CheckIndustryPlacement(frontier_tile, false, true);
	CHECK(front_proc.Failed());
	CHECK(front_proc.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);

	/* 3. Developed World:
	 * Both raw and processing industries are permitted. */
	CommandCost dev_raw = PlanetManager::CheckIndustryPlacement(dev_tile, true, false);
	CHECK(dev_raw.Succeeded());

	CommandCost dev_proc = PlanetManager::CheckIndustryPlacement(dev_tile, false, true);
	CHECK(dev_proc.Succeeded());

	/* 4. Void Buffer Space:
	 * All industry construction is strictly prohibited. */
	CommandCost void_res = PlanetManager::CheckIndustryPlacement(void_tile, false, true);
	CHECK(void_res.Failed());
	CHECK(void_res.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	PlanetManager::Reset();
}

TEST_CASE("Placement Restrictions - Train Depot Rules Matrix")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	PlanetRegion core_world{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 399,
		.max_y = 399,
		.development_score = 10000
	};

	PlanetRegion frontier_world{
		.id = WorldID{1},
		.name = "Ceres Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 0,
		.min_y = 500,
		.max_x = 399,
		.max_y = 899,
		.development_score = 1000
	};

	REQUIRE(PlanetManager::RegisterRegion(core_world));
	REQUIRE(PlanetManager::RegisterRegion(frontier_world));

	TileIndex core_tile = TileXY(200, 200);
	TileIndex frontier_tile = TileXY(200, 700);
	TileIndex void_tile = TileXY(200, 450); // Buffer zone between Core and Frontier

	/* 1. Core World:
	 * High-tech Vac-Train/Maglev depots and standard rail depots are allowed. */
	CHECK(PlanetManager::CheckDepotPlacement(core_tile, RAILTYPE_MAGLEV).Succeeded());
	CHECK(PlanetManager::CheckDepotPlacement(core_tile, RAILTYPE_RAIL).Succeeded());
	CHECK(PlanetManager::CheckDepotPlacement(core_tile, RAILTYPE_ELECTRIC).Succeeded());

	/* 2. Frontier World:
	 * Standard rail and electric rail depots are permitted.
	 * High-tech Vac-Train/Maglev depots are prohibited. */
	CHECK(PlanetManager::CheckDepotPlacement(frontier_tile, RAILTYPE_RAIL).Succeeded());
	CHECK(PlanetManager::CheckDepotPlacement(frontier_tile, RAILTYPE_ELECTRIC).Succeeded());

	CommandCost front_maglev = PlanetManager::CheckDepotPlacement(frontier_tile, RAILTYPE_MAGLEV);
	CHECK(front_maglev.Failed());
	CHECK(front_maglev.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);

	/* 3. Void Buffer Space:
	 * Any depot construction is rejected. */
	CommandCost void_depot = PlanetManager::CheckDepotPlacement(void_tile, RAILTYPE_RAIL);
	CHECK(void_depot.Failed());
	CHECK(void_depot.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	PlanetManager::Reset();
}

TEST_CASE("Placement Restrictions - CmdBuildTrainDepot Integration")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	PlanetRegion frontier_world{
		.id = WorldID{2},
		.name = "Frontier Mining",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 100,
		.min_y = 100,
		.max_x = 400,
		.max_y = 400,
		.development_score = 500
	};
	REQUIRE(PlanetManager::RegisterRegion(frontier_world));

	TileIndex frontier_tile = TileXY(250, 250);
	TileIndex void_tile = TileXY(50, 50);

	/* Attempting to build a Vac-Train/Maglev depot on a frontier world via CmdBuildTrainDepot */
	CommandCost ret_maglev = CmdBuildTrainDepot(DoCommandFlags{}, frontier_tile, RAILTYPE_MAGLEV, DiagDirection::NE);
	CHECK(ret_maglev.Failed());
	CHECK(ret_maglev.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);

	/* Attempting to build any depot in void buffer space */
	CommandCost ret_void = CmdBuildTrainDepot(DoCommandFlags{}, void_tile, RAILTYPE_RAIL, DiagDirection::NE);
	CHECK(ret_void.Failed());
	CHECK(ret_void.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	PlanetManager::Reset();
}
