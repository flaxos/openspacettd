/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint32_lifecycle_and_tech.cpp Unit tests for Sprint 32 Planetary Settlement Lifecycle, Economy-Driven Phase Promotion, and Technology Progression Engine. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/universe_authority.h"
#include "../clear_map.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 32 Tech - Track Infrastructure Placement Restrictions by World Phase")
{
	PlanetManager::Reset();
	Map::Allocate(512, 512);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Earth Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
		.development_score = 10000
	};
	PlanetRegion r_developed{
		.id = WorldID{1},
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
		.development_score = 5000
	};
	PlanetRegion r_frontier{
		.id = WorldID{2},
		.name = "Calyx Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
		.development_score = 1500
	};
	PlanetRegion r_expansion{
		.id = WorldID{3},
		.name = "Viridis Wilds",
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

	/* 1. Void space: strictly prohibits any track placement */
	CHECK(PlanetManager::CheckTrackPlacement(t_void, RAILTYPE_RAIL).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	CHECK(PlanetManager::CheckTrackPlacement(t_void, RAILTYPE_ELECTRIC).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	CHECK(PlanetManager::CheckTrackPlacement(t_void, RAILTYPE_MAGLEV).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	/* 2. Expansion world (Phase 4): only pioneer standard rail allowed */
	CHECK(PlanetManager::CheckTrackPlacement(t_exp, RAILTYPE_RAIL).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_exp, RAILTYPE_ELECTRIC).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
	CHECK(PlanetManager::CheckTrackPlacement(t_exp, RAILTYPE_MAGLEV).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
	CHECK(PlanetManager::CheckTrackPlacement(t_exp, RAILTYPE_MONO).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);

	/* 3. Frontier world (Phase 3): standard rail and catenary electric allowed; Monorail and Maglev restricted */
	CHECK(PlanetManager::CheckTrackPlacement(t_front, RAILTYPE_RAIL).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_front, RAILTYPE_ELECTRIC).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_front, RAILTYPE_MONO).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);
	CHECK(PlanetManager::CheckTrackPlacement(t_front, RAILTYPE_MAGLEV).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);

	/* 4. Developed world (Phase 2): standard, electric, and monorail allowed; Maglev restricted to Core */
	CHECK(PlanetManager::CheckTrackPlacement(t_dev, RAILTYPE_RAIL).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_dev, RAILTYPE_ELECTRIC).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_dev, RAILTYPE_MONO).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_dev, RAILTYPE_MAGLEV).GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_DEVELOPED_WORLD);

	/* 5. Core world (Phase 1): all rail types including CST Vacuum-Tube Maglev unlocked */
	CHECK(PlanetManager::CheckTrackPlacement(t_core, RAILTYPE_RAIL).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_core, RAILTYPE_ELECTRIC).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_core, RAILTYPE_MONO).Succeeded());
	CHECK(PlanetManager::CheckTrackPlacement(t_core, RAILTYPE_MAGLEV).Succeeded());
}

TEST_CASE("Sprint 32 Economy - Cargo Delivery and Development Points Accumulation")
{
	PlanetManager::Reset();
	Map::Allocate(512, 512);

	PlanetRegion r_origin{
		.id = WorldID{1},
		.name = "Mining Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
		.development_score = 500
	};
	PlanetRegion r_dest{
		.id = WorldID{2},
		.name = "Refinery Station",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
		.development_score = 100
	};

	REQUIRE(PlanetManager::RegisterRegion(r_origin));
	REQUIRE(PlanetManager::RegisterRegion(r_dest));

	TileIndex t_origin = TileXY(50, 50);
	TileIndex t_dest = TileXY(150, 50);

	/* 1. Local delivery within World 2 */
	TileIndex t_dest_local_src = TileXY(160, 50);
	PlanetManager::RecordCargoDelivery(t_dest, CargoType{0}, 100, t_dest_local_src);

	const PlanetRegion *dest_after_local = PlanetManager::GetRegion(WorldID{2});
	REQUIRE(dest_after_local != nullptr);
	/* 100 units / 10 = 10 points -> score becomes 110 */
	CHECK(dest_after_local->development_score == 110);

	/* 2. Interplanetary import from World 1 to World 2 */
	PlanetManager::RecordCargoDelivery(t_dest, CargoType{1}, 100, t_origin);

	const PlanetRegion *dest_after_import = PlanetManager::GetRegion(WorldID{2});
	REQUIRE(dest_after_import != nullptr);
	/* 100 units / 2 = 50 points -> score becomes 110 + 50 = 160 */
	CHECK(dest_after_import->development_score == 160);
}

TEST_CASE("Sprint 32 Lifecycle - Multi-Tier Phase Promotion and Threshold Gating")
{
	(void)MockEnvironment::Instance();
	_company_pool.CleanPool();
	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};
	PlanetManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	Map::Allocate(512, 512);

	PlanetRegion r_source{
		.id = WorldID{1},
		.name = "Earth Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 450, .min_y = 450, .max_x = 510, .max_y = 510,
		.development_score = 10000
	};
	PlanetRegion r_colony{
		.id = WorldID{10},
		.name = "Far Away",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 20, .min_y = 20, .max_x = 80, .max_y = 80,
		.development_score = 100,
		.outpost_tile = TileXY(50, 50)
	};
	REQUIRE(PlanetManager::RegisterRegion(r_source));
	REQUIRE(PlanetManager::RegisterRegion(r_colony));

	RegisteredWorld rw{
		.world_id = WorldID{10},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Far Away",
		.address = "node-10",
		.description = "Frontier mining colony",
		.status = WorldOnlineStatus::Online,
		.biome = WorldBiome::AridDesert,
	};
	REQUIRE(UniverseAuthorityService::Instance().RegisterWorld(rw));

	/* Threshold check: Phase 3 requires 2000 points */
	CHECK(PlanetManager::GetPromotionThreshold(WorldPhase::Phase3_Frontier) == DEVELOPMENT_THRESHOLD_DEVELOPED);
	CHECK(PlanetManager::GetPromotionThreshold(WorldPhase::Phase2_Developed) == DEVELOPMENT_THRESHOLD_CORE);
	CHECK(PlanetManager::GetPromotionThreshold(WorldPhase::Phase1_Core) == UINT32_MAX);

	/* 1. Score (100) < 2000 -> Cannot promote */
	CHECK_FALSE(PlanetManager::CanPromoteWorld(WorldID{10}));
	CommandCost early_promo = CmdPromoteWorld(DoCommandFlags{}, WorldID{10});
	CHECK(early_promo.Failed());
	CHECK(early_promo.GetErrorMessage() == STR_ERROR_NOT_ENOUGH_DEVELOPMENT);

	/* 2. Stimulate development to 2100 points */
	PlanetManager::RecordCargoDelivery(TileXY(50, 50), CargoType{0}, 4000, TileXY(500, 500));
	const PlanetRegion *reg_ready = PlanetManager::GetRegion(WorldID{10});
	REQUIRE(reg_ready != nullptr);
	CHECK(reg_ready->development_score >= DEVELOPMENT_THRESHOLD_DEVELOPED);
	CHECK(PlanetManager::CanPromoteWorld(WorldID{10}));

	/* 3. Execute promotion to Phase 2 Developed */
	CommandCost promo1 = CmdPromoteWorld(DoCommandFlags{DoCommandFlag::Execute}, WorldID{10});
	CHECK(promo1.Succeeded());

	const PlanetRegion *reg_p2 = PlanetManager::GetRegion(WorldID{10});
	REQUIRE(reg_p2 != nullptr);
	CHECK(reg_p2->phase == WorldPhase::Phase2_Developed);
	CHECK(reg_p2->development_score >= 2100 + 250);

	/* Synchronized with UniverseAuthorityService */
	const RegisteredWorld *auth_w10 = UniverseAuthorityService::Instance().GetWorld(WorldID{10});
	REQUIRE(auth_w10 != nullptr);
	CHECK(auth_w10->phase == WorldPhase::Phase2_Developed);

	/* 4. Score (2350) < 5000 -> Cannot promote to Core yet */
	CHECK_FALSE(PlanetManager::CanPromoteWorld(WorldID{10}));

	/* Stimulate further growth to 5200 points */
	PlanetManager::RecordCargoDelivery(TileXY(50, 50), CargoType{0}, 6000, TileXY(500, 500));
	CHECK(PlanetManager::CanPromoteWorld(WorldID{10}));

	/* 5. Execute promotion to Phase 1 Core Metropolis */
	CommandCost promo2 = CmdPromoteWorld(DoCommandFlags{DoCommandFlag::Execute}, WorldID{10});
	CHECK(promo2.Succeeded());

	const PlanetRegion *reg_p1 = PlanetManager::GetRegion(WorldID{10});
	REQUIRE(reg_p1 != nullptr);
	CHECK(reg_p1->phase == WorldPhase::Phase1_Core);

	/* Cannot promote beyond Core */
	CHECK_FALSE(PlanetManager::CanPromoteWorld(WorldID{10}));
	CommandCost max_promo = CmdPromoteWorld(DoCommandFlags{}, WorldID{10});
	CHECK(max_promo.Failed());
	CHECK(max_promo.GetErrorMessage() == STR_ERROR_ALREADY_MAX_PHASE);
}

TEST_CASE("Sprint 32 Authority - Universe Authority Remote World Promotion")
{
	UniverseAuthorityService &service = UniverseAuthorityService::Instance();
	service.Reset();

	RegisteredWorld w_frontier{
		.world_id = WorldID{5},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Frontier Station 5",
		.address = "node-5",
		.description = "Active colony",
		.status = WorldOnlineStatus::Online,
		.biome = WorldBiome::Volcanic,
	};
	REQUIRE(service.RegisterWorld(w_frontier));

	/* Promote Frontier -> Developed */
	CHECK(service.PromoteWorld(WorldID{5}));
	CHECK(service.GetWorld(WorldID{5})->phase == WorldPhase::Phase2_Developed);

	/* Promote Developed -> Core */
	CHECK(service.PromoteWorld(WorldID{5}));
	CHECK(service.GetWorld(WorldID{5})->phase == WorldPhase::Phase1_Core);

	/* Promote Core -> Fails (already max) */
	CHECK_FALSE(service.PromoteWorld(WorldID{5}));
	CHECK(service.GetWorld(WorldID{5})->phase == WorldPhase::Phase1_Core);

	/* Non-existent world -> Fails */
	CHECK_FALSE(service.PromoteWorld(WorldID{999}));
}
