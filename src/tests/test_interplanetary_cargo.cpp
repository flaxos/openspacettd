/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_interplanetary_cargo.cpp Unit tests for interplanetary cargo revenue, phase gradients, and supply chain. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/world_gen.h"
#include "../cargopacket.h"
#include "../economy_func.h"
#include "../table/strings.h"

#include "../safeguards.h"

TEST_CASE("Interplanetary Cargo - Trade Premium Matrix")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	/* Unregistered / empty manager must yield 0% bonus */
	TileIndex tile_a = TileXY(100, 100);
	TileIndex tile_b = TileXY(600, 100);
	CHECK(PlanetManager::GetInterplanetaryBonusPercent(tile_a, tile_b) == 0);
	CHECK(PlanetManager::GetInterplanetaryCargoProfit(1000, tile_a, tile_b) == 1000);

	/* Set up 4 distinct planetary worlds */
	PlanetRegion core_world{
		.id = WorldID{0},
		.name = "Earth Prime (Hub)",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 399,
		.max_y = 399,
		.development_score = 10000,
	};

	PlanetRegion dev_world{
		.id = WorldID{1},
		.name = "Vulcan Forge (Emerging)",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 500,
		.min_y = 0,
		.max_x = 899,
		.max_y = 399,
		.development_score = 5000,
	};

	PlanetRegion frontier_world{
		.id = WorldID{2},
		.name = "Ceres Bio-Outpost (Frontier)",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 0,
		.min_y = 500,
		.max_x = 399,
		.max_y = 899,
		.development_score = 1000,
	};

	PlanetRegion expansion_world{
		.id = WorldID{3},
		.name = "Haven Rim (Expansion)",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::AridDesert,
		.min_x = 500,
		.min_y = 500,
		.max_x = 899,
		.max_y = 899,
		.development_score = 100,
	};

	REQUIRE(PlanetManager::RegisterRegion(core_world));
	REQUIRE(PlanetManager::RegisterRegion(dev_world));
	REQUIRE(PlanetManager::RegisterRegion(frontier_world));
	REQUIRE(PlanetManager::RegisterRegion(expansion_world));

	TileIndex core_tile = TileXY(200, 200);
	TileIndex core_tile2 = TileXY(250, 250);
	TileIndex dev_tile = TileXY(700, 200);
	TileIndex frontier_tile = TileXY(200, 700);
	TileIndex expansion_tile = TileXY(700, 700);
	TileIndex void_tile = TileXY(450, 450);

	SECTION("Intra-World Shipments (No Interplanetary Bonus)")
	{
		CHECK(PlanetManager::GetInterplanetaryBonusPercent(core_tile, core_tile2) == 0);
		CHECK(PlanetManager::GetInterplanetaryBonusPercent(frontier_tile, frontier_tile) == 0);
		CHECK(PlanetManager::GetInterplanetaryCargoProfit(5000, core_tile, core_tile2) == 5000);
	}

	SECTION("Invalid and Void Tiles")
	{
		CHECK(PlanetManager::GetInterplanetaryBonusPercent(INVALID_TILE, core_tile) == 0);
		CHECK(PlanetManager::GetInterplanetaryBonusPercent(core_tile, INVALID_TILE) == 0);
		CHECK(PlanetManager::GetInterplanetaryBonusPercent(void_tile, core_tile) == 0);
		CHECK(PlanetManager::GetInterplanetaryBonusPercent(frontier_tile, void_tile) == 0);
	}

	SECTION("Phase 3 Frontier -> Phase 2 Emerging (+75% Premium)")
	{
		/* Base: 50% + Tier diff |3 - 2| * 25% = 75% */
		uint32_t bonus = PlanetManager::GetInterplanetaryBonusPercent(frontier_tile, dev_tile);
		CHECK(bonus == 75);

		Money base_profit = 10000;
		Money adjusted_profit = PlanetManager::GetInterplanetaryCargoProfit(base_profit, frontier_tile, dev_tile);
		CHECK(adjusted_profit == 17500);
	}

	SECTION("Phase 2 Emerging -> Phase 1 Core (+100% Premium)")
	{
		/* Base: 50% + Tier diff |2 - 1| * 25% + Core Demand Bonus (25%) = 100% */
		uint32_t bonus = PlanetManager::GetInterplanetaryBonusPercent(dev_tile, core_tile);
		CHECK(bonus == 100);

		Money base_profit = 10000;
		Money adjusted_profit = PlanetManager::GetInterplanetaryCargoProfit(base_profit, dev_tile, core_tile);
		CHECK(adjusted_profit == 20000);
	}

	SECTION("Phase 3 Frontier -> Phase 1 Core Hub (+125% Premium)")
	{
		/* Base: 50% + Tier diff |3 - 1| * 25% + Core Demand Bonus (25%) = 125% */
		uint32_t bonus = PlanetManager::GetInterplanetaryBonusPercent(frontier_tile, core_tile);
		CHECK(bonus == 125);

		Money base_profit = 10000;
		Money adjusted_profit = PlanetManager::GetInterplanetaryCargoProfit(base_profit, frontier_tile, core_tile);
		CHECK(adjusted_profit == 22500);
	}

	SECTION("Phase 1 Core -> Phase 3 Frontier (+100% Reverse Flow)")
	{
		/* Base: 50% + Tier diff |1 - 3| * 25% + Core Demand Bonus (0% - dest is P3) = 100% */
		uint32_t bonus = PlanetManager::GetInterplanetaryBonusPercent(core_tile, frontier_tile);
		CHECK(bonus == 100);

		Money base_profit = 10000;
		Money adjusted_profit = PlanetManager::GetInterplanetaryCargoProfit(base_profit, core_tile, frontier_tile);
		CHECK(adjusted_profit == 20000);
	}

	SECTION("Phase 4 Expansion -> Phase 1 Core Hub (+150% Frontier Expansion)")
	{
		/* Base: 50% + Tier diff |4 - 1| * 25% (75%) + Core Demand Bonus (25%) = 150% */
		uint32_t bonus = PlanetManager::GetInterplanetaryBonusPercent(expansion_tile, core_tile);
		CHECK(bonus == 150);

		Money base_profit = 10000;
		Money adjusted_profit = PlanetManager::GetInterplanetaryCargoProfit(base_profit, expansion_tile, core_tile);
		CHECK(adjusted_profit == 25000);
	}

	PlanetManager::Reset();
}

TEST_CASE("Interplanetary Cargo - Edge Case Handling and Profit Preservation")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	PlanetRegion core{
		.id = WorldID{0},
		.name = "Core Hub",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 399,
		.max_y = 399,
		.development_score = 10000,
	};
	PlanetRegion frontier{
		.id = WorldID{1},
		.name = "Frontier Post",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 500,
		.min_y = 0,
		.max_x = 899,
		.max_y = 399,
		.development_score = 1000,
	};

	REQUIRE(PlanetManager::RegisterRegion(core));
	REQUIRE(PlanetManager::RegisterRegion(frontier));

	TileIndex core_tile = TileXY(200, 200);
	TileIndex frontier_tile = TileXY(700, 200);

	/* Zero profit must stay zero */
	CHECK(PlanetManager::GetInterplanetaryCargoProfit(0, frontier_tile, core_tile) == 0);

	/* Negative profit (operating loss) must NOT be magnified */
	CHECK(PlanetManager::GetInterplanetaryCargoProfit(-1500, frontier_tile, core_tile) == -1500);

	/* Large 64-bit integer values must not overflow */
	Money massive_profit = 50'000'000'000LL;
	Money expected_profit = massive_profit + (massive_profit * 125) / 100;
	CHECK(PlanetManager::GetInterplanetaryCargoProfit(massive_profit, frontier_tile, core_tile) == expected_profit);

	PlanetManager::Reset();
}

TEST_CASE("Interplanetary Cargo - Industry Supply Chain Restrictions")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	PlanetRegion core_world{
		.id = WorldID{0},
		.name = "Core World",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 399,
		.max_y = 399,
		.development_score = 10000,
	};
	PlanetRegion dev_world{
		.id = WorldID{1},
		.name = "Developed World",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 500,
		.min_y = 0,
		.max_x = 899,
		.max_y = 399,
		.development_score = 5000,
	};
	PlanetRegion frontier_world{
		.id = WorldID{2},
		.name = "Frontier World",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 0,
		.min_y = 500,
		.max_x = 399,
		.max_y = 899,
		.development_score = 1000,
	};

	REQUIRE(PlanetManager::RegisterRegion(core_world));
	REQUIRE(PlanetManager::RegisterRegion(dev_world));
	REQUIRE(PlanetManager::RegisterRegion(frontier_world));

	TileIndex core_tile = TileXY(200, 200);
	TileIndex dev_tile = TileXY(700, 200);
	TileIndex frontier_tile = TileXY(200, 700);
	TileIndex void_tile = TileXY(450, 450);

	/* Bio-Farm (Raw Producer): is_raw = true, is_processing = false */
	CHECK(PlanetManager::CheckIndustryPlacement(frontier_tile, true, false).Succeeded());
	CHECK(PlanetManager::CheckIndustryPlacement(dev_tile, true, false).Succeeded());
	CommandCost core_farm = PlanetManager::CheckIndustryPlacement(core_tile, true, false);
	CHECK(core_farm.Failed());
	CHECK(core_farm.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_CORE_WORLD);

	/* Food Processor (Secondary Manufacturer): is_raw = false, is_processing = true */
	CHECK(PlanetManager::CheckIndustryPlacement(core_tile, false, true).Succeeded());
	CHECK(PlanetManager::CheckIndustryPlacement(dev_tile, false, true).Succeeded());
	CommandCost frontier_processor = PlanetManager::CheckIndustryPlacement(frontier_tile, false, true);
	CHECK(frontier_processor.Failed());
	CHECK(frontier_processor.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);

	/* Void Buffer Space: No industry permitted */
	CommandCost void_raw = PlanetManager::CheckIndustryPlacement(void_tile, true, false);
	CHECK(void_raw.Failed());
	CHECK(void_raw.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	PlanetManager::Reset();
}

TEST_CASE("Interplanetary Cargo - CargoPacket Source Tracking")
{
	Map::Allocate(1024, 1024);
	TileIndex origin_tile = TileXY(120, 340);

	if (CargoPacket::CanAllocateItem()) {
		CargoPacket *cp = CargoPacket::Create(StationID{1}, 50, Source{IndustryID{2}, SourceType::Industry});
		REQUIRE(cp != nullptr);

		/* Initially before first vehicle pickup, source_xy is INVALID_TILE */
		CHECK(cp->GetSourceXY() == INVALID_TILE);

		/* Upon pickup at the origin industry/station tile */
		cp->UpdateLoadingTile(origin_tile);
		CHECK(cp->GetSourceXY() == origin_tile);

		delete cp;
	}
}

TEST_CASE("Interplanetary Cargo - Procedural Layout Revenue Scaling")
{
	Map::Allocate(512, 512);
	PlanetManager::Reset();

	/* Generate standard 3-world procedural layout */
	REQUIRE(MultiWorldGen::GenerateMultiWorldLayout(512, 512));
	REQUIRE(PlanetManager::Count() == 3);

	const PlanetRegion *core = PlanetManager::GetRegion(WorldID{0});
	const PlanetRegion *dev = PlanetManager::GetRegion(WorldID{1});
	const PlanetRegion *frontier = PlanetManager::GetRegion(WorldID{2});

	REQUIRE(core != nullptr);
	REQUIRE(dev != nullptr);
	REQUIRE(frontier != nullptr);

	TileIndex core_tile = TileXY((core->min_x + core->max_x) / 2, (core->min_y + core->max_y) / 2);
	TileIndex dev_tile = TileXY((dev->min_x + dev->max_x) / 2, (dev->min_y + dev->max_y) / 2);
	TileIndex frontier_tile = TileXY((frontier->min_x + frontier->max_x) / 2, (frontier->min_y + frontier->max_y) / 2);

	/* Verify phase identities */
	CHECK(PlanetManager::GetTilePhase(core_tile) == WorldPhase::Phase1_Core);
	CHECK(PlanetManager::GetTilePhase(dev_tile) == WorldPhase::Phase2_Developed);
	CHECK(PlanetManager::GetTilePhase(frontier_tile) == WorldPhase::Phase3_Frontier);

	/* Simulate cargo income baseline */
	Money intra_income = 10000;

	/* Step 1: Frontier Bio-Farm -> Emerging Food Processor */
	Money step1_income = PlanetManager::GetInterplanetaryCargoProfit(intra_income, frontier_tile, dev_tile);
	CHECK(step1_income == 17500); // +75%

	/* Step 2: Emerging Food Processor -> Core Metropolitan Market */
	Money step2_income = PlanetManager::GetInterplanetaryCargoProfit(intra_income, dev_tile, core_tile);
	CHECK(step2_income == 20000); // +100%

	/* Direct: Frontier Bio-Farm -> Core Metropolitan Market */
	Money direct_income = PlanetManager::GetInterplanetaryCargoProfit(intra_income, frontier_tile, core_tile);
	CHECK(direct_income == 22500); // +125%

	PlanetManager::Reset();
}
