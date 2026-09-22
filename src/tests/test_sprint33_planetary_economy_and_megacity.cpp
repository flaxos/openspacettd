/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint33_planetary_economy_and_megacity.cpp Unit tests for Sprint 33 Planetary Town Growth, Megacity Supply Loops, and Biome-Specific Industry Lifecycle. */

#include "../stdafx.h"
#include "../cargotype.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/megacity_manager.h"
#include "../portal/universe_authority.h"
#include "../town.h"
#include "../industry.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 33 - Planetary Town Placement Protection by World Phase")
{
	PlanetManager::Reset();
	Map::Allocate(512, 512);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
		.development_score = 10000
	};
	PlanetRegion r_developed{
		.id = WorldID{1},
		.name = "Vulcan Foundry",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
		.development_score = 5000
	};
	PlanetRegion r_frontier{
		.id = WorldID{2},
		.name = "Boreas Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
		.development_score = 1500
	};
	PlanetRegion r_expansion{
		.id = WorldID{3},
		.name = "Viridis Wilderness",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::SubTropic,
		.min_x = 310, .min_y = 10, .max_x = 390, .max_y = 90,
		.development_score = 0
	};

	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_developed));
	REQUIRE(PlanetManager::RegisterRegion(r_frontier));
	REQUIRE(PlanetManager::RegisterRegion(r_expansion));

	TileIndex t_void = TileXY(5, 5);
	TileIndex t_core = TileXY(50, 50);
	TileIndex t_dev = TileXY(150, 50);
	TileIndex t_front = TileXY(250, 50);
	TileIndex t_exp = TileXY(350, 50);

	/* 1. Void space: strictly prohibits town founding */
	CHECK(PlanetManager::CheckTownPlacement(t_void).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	/* 2. Expansion wilderness (Phase 4): prohibits town founding until colonized */
	CHECK(PlanetManager::CheckTownPlacement(t_exp).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);

	/* 3. Frontier, Developed, and Core worlds: permit town founding */
	CHECK(PlanetManager::CheckTownPlacement(t_front).Succeeded());
	CHECK(PlanetManager::CheckTownPlacement(t_dev).Succeeded());
	CHECK(PlanetManager::CheckTownPlacement(t_core).Succeeded());
}

TEST_CASE("Sprint 33 - Biome-Specific Industry Placement Protection")
{
	PlanetManager::Reset();
	Map::Allocate(512, 512);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
		.development_score = 10000
	};
	PlanetRegion r_volcanic{
		.id = WorldID{1},
		.name = "Vulcan Foundry",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
		.development_score = 5000
	};
	PlanetRegion r_expansion{
		.id = WorldID{2},
		.name = "Aridis Wilderness",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::AridDesert,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
		.development_score = 0
	};

	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_volcanic));
	REQUIRE(PlanetManager::RegisterRegion(r_expansion));

	TileIndex t_void = TileXY(5, 5);
	TileIndex t_core = TileXY(50, 50);
	TileIndex t_volc = TileXY(150, 50);
	TileIndex t_exp = TileXY(250, 50);

	/* 1. Void space rejects all industry placement */
	CHECK(PlanetManager::CheckIndustryPlacement(t_void, true, false, false).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	/* 2. Expansion world rejects all industry placement */
	CHECK(PlanetManager::CheckIndustryPlacement(t_exp, true, false, false).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);

	/* 3. Volcanic world rejects bio-farms/plantations */
	CHECK(PlanetManager::CheckIndustryPlacement(t_volc, false, false, true).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_FARM_ON_VOLCANIC_WORLD);

	/* 4. Volcanic world allows raw mineral extraction and processing facilities */
	CHECK(PlanetManager::CheckIndustryPlacement(t_volc, true, false, false).Succeeded());
	CHECK(PlanetManager::CheckIndustryPlacement(t_volc, false, true, false).Succeeded());

	/* 5. Temperate Core world allows bio-farms */
	CHECK(PlanetManager::CheckIndustryPlacement(t_core, false, false, true).Succeeded());
}

TEST_CASE("Sprint 33 - Megacity Supply Delivery Tracking & Monthly Evaluation Integration")
{
	SetupCargoForClimate(LandscapeType::Temperate);
	MegacityManager::Reset();
	REQUIRE(MegacityManager::GetAllMegacities().empty());

	TownID tid1{42};
	WorldID wid0{0};

	REQUIRE(MegacityManager::RegisterMegacity(tid1, wid0, "Novosibirsk Metropolis", 20000));
	REQUIRE(MegacityManager::IsMegacity(tid1));

	const auto *prof = MegacityManager::GetProfile(tid1);
	REQUIRE(prof != nullptr);
	CHECK(prof->population == 20000);
	CHECK(prof->monthly_quota[0] == 1000); // 20000 / 20
	CHECK(prof->monthly_quota[1] == 500);  // 20000 / 40
	CHECK(prof->monthly_quota[2] == 200);  // 20000 / 100
	CHECK(prof->growth_state == MegacityGrowthState::Subsistence);

	/* Simulate cargo deliveries via cargo type mapping */
	/* Cargo 6 (Grain) -> Tier 1 */
	MegacityManager::RecordDeliveryByCargo(tid1, 6, 1200);
	/* Cargo 5 (Goods) -> Tier 2 */
	MegacityManager::RecordDeliveryByCargo(tid1, 5, 600);
	/* Cargo 10 (Valuables/Diamonds) -> Tier 3 */
	MegacityManager::RecordDeliveryByCargo(tid1, 10, 250);

	/* Trigger monthly supply evaluation */
	MegacityManager::EvaluateMonthlySupply();

	prof = MegacityManager::GetProfile(tid1);
	REQUIRE(prof != nullptr);
	CHECK(prof->satisfaction_pct[0] >= 1.0f);
	CHECK(prof->satisfaction_pct[1] >= 1.0f);
	CHECK(prof->satisfaction_pct[2] >= 1.0f);
	CHECK(prof->growth_state == MegacityGrowthState::HyperGrowth);
	CHECK(prof->growth_multiplier == 2.0f);
	CHECK(prof->passenger_multiplier == 1.5f);

	/* Next month: starve tier 1 (< 50% delivered) */
	MegacityManager::RecordDeliveryByCargo(tid1, 6, 300); // 300 / 1000 = 30% < 50%
	MegacityManager::RecordDeliveryByCargo(tid1, 5, 500);
	MegacityManager::RecordDeliveryByCargo(tid1, 10, 200);

	MegacityManager::EvaluateMonthlySupply();

	prof = MegacityManager::GetProfile(tid1);
	REQUIRE(prof != nullptr);
	CHECK(prof->satisfaction_pct[0] < 0.5f);
	CHECK(prof->growth_state == MegacityGrowthState::Starvation);
	CHECK(prof->growth_multiplier == 0.0f);
	CHECK(prof->passenger_multiplier == 0.5f);
}

TEST_CASE("Sprint 33 - Megacity Multipliers and Town Growth Controls")
{
	MegacityManager::Reset();
	TownID tid{10};

	REQUIRE(MegacityManager::RegisterMegacity(tid, WorldID{0}, "Olympus City", 10000));

	/* Check initial Subsistence state */
	CHECK(MegacityManager::GetGrowthMultiplier(tid) == 1.0f);
	CHECK(MegacityManager::GetPassengerMultiplier(tid) == 1.0f);

	/* Starvation state */
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 100); // 100 / 500 = 20%
	MegacityManager::EvaluateMonthlySupply();
	CHECK(MegacityManager::GetGrowthMultiplier(tid) == 0.0f);
	CHECK(MegacityManager::GetPassengerMultiplier(tid) == 0.5f);

	/* Metropolitan Boom (Tier 1 & 2 met, Tier 3 unmet) */
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 600);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier2_Expansion, 300);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier3_Prosperity, 10);
	MegacityManager::EvaluateMonthlySupply();
	CHECK(MegacityManager::GetGrowthMultiplier(tid) == 1.5f);
	CHECK(MegacityManager::GetPassengerMultiplier(tid) == 1.25f);

	/* Hyper Growth (All 3 tiers fully met) */
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 600);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier2_Expansion, 300);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier3_Prosperity, 150);
	MegacityManager::EvaluateMonthlySupply();
	CHECK(MegacityManager::GetGrowthMultiplier(tid) == 2.0f);
	CHECK(MegacityManager::GetPassengerMultiplier(tid) == 1.5f);
}

TEST_CASE("Sprint 33 - Universe Authority Megacity Federation Tracking & State")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w_core{};
	w_core.world_id = WorldID{1};
	w_core.phase = WorldPhase::Phase1_Core;
	w_core.name = "Earth Metropolitan Hub";
	w_core.address = "127.0.0.1:3980";
	w_core.status = WorldOnlineStatus::Online;
	w_core.biome = WorldBiome::Temperate;
	w_core.population = 45000;
	w_core.is_megacity = true;
	w_core.megacity_growth_state = "HyperGrowth";
	w_core.satisfaction_pct = 125.5f;
	REQUIRE(auth.RegisterWorld(w_core));

	const auto *retrieved = auth.GetWorld(WorldID{1});
	REQUIRE(retrieved != nullptr);
	CHECK(retrieved->is_megacity == true);
	CHECK(retrieved->megacity_growth_state == "HyperGrowth");
	CHECK(retrieved->population == 45000);
	CHECK(retrieved->satisfaction_pct == Approx(125.5f));

	/* Update megacity status */
	REQUIRE(auth.UpdateMegacityStatus(WorldID{1}, true, "MetropolitanBoom", 108.0f, 48000));
	retrieved = auth.GetWorld(WorldID{1});
	REQUIRE(retrieved != nullptr);
	CHECK(retrieved->megacity_growth_state == "MetropolitanBoom");
	CHECK(retrieved->population == 48000);
	CHECK(retrieved->satisfaction_pct == Approx(108.0f));

	/* Test Phase 2 promotion auto-elevation to megacity */
	RegisteredWorld w_dev{};
	w_dev.world_id = WorldID{2};
	w_dev.phase = WorldPhase::Phase2_Developed;
	w_dev.name = "Ares Forge";
	w_dev.address = "127.0.0.1:3981";
	w_dev.status = WorldOnlineStatus::Online;
	w_dev.biome = WorldBiome::AridDesert;
	w_dev.is_megacity = false;
	REQUIRE(auth.RegisterWorld(w_dev));
	REQUIRE(auth.PromoteWorld(WorldID{2})); // Phase 2 -> Phase 1 Core

	const auto *promoted = auth.GetWorld(WorldID{2});
	REQUIRE(promoted != nullptr);
	CHECK(promoted->phase == WorldPhase::Phase1_Core);
	CHECK(promoted->is_megacity == true);
	CHECK(promoted->megacity_growth_state == "Subsistence");
}
