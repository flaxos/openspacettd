/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint31_colonization_gui.cpp Unit tests for Sprint 31 Planetary Colonization GUI, Universe Authority Federation Expansion, and Settlement Lifecycle. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/universe_authority.h"
#include "../portal/universe_directory_gui.h"
#include "../widgets/universe_directory_widget.h"
#include "../clear_map.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../table/strings.h"
#include "../window_gui.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 31 GUI - Universe Directory Widget Hierarchy and Button States")
{
	PlanetManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	Map::Allocate(256, 256);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Oaktree Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 4, .min_y = 4, .max_x = 40, .max_y = 40,
		.development_score = 10000
	};
	PlanetRegion r_frontier{
		.id = WorldID{1},
		.name = "Calyx Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 45, .min_y = 4, .max_x = 80, .max_y = 40,
		.development_score = 2000
	};
	PlanetRegion r_wilderness{
		.id = WorldID{2},
		.name = "Ignis Caldera",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 85, .min_y = 4, .max_x = 120, .max_y = 40,
		.development_score = 100
	};

	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_frontier));
	REQUIRE(PlanetManager::RegisterRegion(r_wilderness));

	/* Verify widget IDs are properly enumerated */
	CHECK(WID_UD_CAPTION == 0);
	CHECK(WID_UD_HEADER_PANEL == 1);
	CHECK(WID_UD_WORLD_LIST == 2);
	CHECK(WID_UD_SCROLLBAR == 3);
	CHECK(WID_UD_DETAILS_PANEL == 4);
	CHECK(WID_UD_REFRESH == 5);
	CHECK(WID_UD_JUMP_BTN == 6);
	CHECK(WID_UD_COLONIZE_BTN == 7);

	/* Check phase evaluation for colonize button */
	const PlanetRegion *reg_core = PlanetManager::GetRegion(WorldID{0});
	const PlanetRegion *reg_frontier = PlanetManager::GetRegion(WorldID{1});
	const PlanetRegion *reg_wilderness = PlanetManager::GetRegion(WorldID{2});

	REQUIRE(reg_core != nullptr);
	REQUIRE(reg_frontier != nullptr);
	REQUIRE(reg_wilderness != nullptr);

	CHECK(reg_core->phase != WorldPhase::Phase4_Expansion);
	CHECK(reg_frontier->phase != WorldPhase::Phase4_Expansion);
	CHECK(reg_wilderness->phase == WorldPhase::Phase4_Expansion);
}

TEST_CASE("Sprint 31 Origin - Outpost Origin Coordinate Assignment and Viewport Navigation")
{
	PlanetManager::Reset();
	Map::Allocate(256, 256);

	PlanetRegion r_exp{
		.id = WorldID{3},
		.name = "Pelagios Sea",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Oceanic,
		.min_x = 100, .min_y = 100, .max_x = 200, .max_y = 200,
		.development_score = 0,
		.outpost_tile = INVALID_TILE
	};
	REQUIRE(PlanetManager::RegisterRegion(r_exp));

	/* Prior to colonization, outpost tile is invalid */
	const PlanetRegion *r_init = PlanetManager::GetRegion(WorldID{3});
	REQUIRE(r_init != nullptr);
	CHECK(r_init->outpost_tile == INVALID_TILE);

	/* Found colonial outpost at specific tile (142, 158) */
	TileIndex outpost_loc = TileXY(142, 158);
	CHECK(PlanetManager::ColonizeWorld(WorldID{3}, "Port Pelagios", outpost_loc));

	/* Verify outpost origin coordinate recorded */
	const PlanetRegion *r_colonized = PlanetManager::GetRegion(WorldID{3});
	REQUIRE(r_colonized != nullptr);
	CHECK(r_colonized->phase == WorldPhase::Phase3_Frontier);
	CHECK(r_colonized->name == "Port Pelagios");
	CHECK(r_colonized->outpost_tile == outpost_loc);

	/* Viewport jump targeting */
	CHECK(PlanetManager::JumpToPlanet(WorldID{3}));
}

TEST_CASE("Sprint 31 Authority - RegisteredWorld Biome Tracking and Directory Sync")
{
	UniverseAuthorityService &service = UniverseAuthorityService::Instance();
	service.Reset();

	RegisteredWorld w_temp{
		.world_id = WorldID{0},
		.phase = WorldPhase::Phase1_Core,
		.name = "Earth Core",
		.biome = WorldBiome::Temperate
	};
	RegisteredWorld w_arid{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase2_Developed,
		.name = "Merredin",
		.biome = WorldBiome::AridDesert
	};
	RegisteredWorld w_arctic{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Calyx",
		.biome = WorldBiome::SubArctic
	};
	RegisteredWorld w_volc{
		.world_id = WorldID{3},
		.phase = WorldPhase::Phase4_Expansion,
		.name = "Vulcan",
		.biome = WorldBiome::Volcanic
	};
	RegisteredWorld w_subtropic{
		.world_id = WorldID{4},
		.phase = WorldPhase::Phase4_Expansion,
		.name = "Viridis",
		.biome = WorldBiome::SubTropic
	};
	RegisteredWorld w_oceanic{
		.world_id = WorldID{5},
		.phase = WorldPhase::Phase4_Expansion,
		.name = "Pelagios",
		.biome = WorldBiome::Oceanic
	};

	REQUIRE(service.RegisterWorld(w_temp));
	REQUIRE(service.RegisterWorld(w_arid));
	REQUIRE(service.RegisterWorld(w_arctic));
	REQUIRE(service.RegisterWorld(w_volc));
	REQUIRE(service.RegisterWorld(w_subtropic));
	REQUIRE(service.RegisterWorld(w_oceanic));

	CHECK(service.GetWorld(WorldID{0})->biome == WorldBiome::Temperate);
	CHECK(service.GetWorld(WorldID{1})->biome == WorldBiome::AridDesert);
	CHECK(service.GetWorld(WorldID{2})->biome == WorldBiome::SubArctic);
	CHECK(service.GetWorld(WorldID{3})->biome == WorldBiome::Volcanic);
	CHECK(service.GetWorld(WorldID{4})->biome == WorldBiome::SubTropic);
	CHECK(service.GetWorld(WorldID{5})->biome == WorldBiome::Oceanic);
}

TEST_CASE("Sprint 31 Authority - Remote ColonizeWorld Lifecycle")
{
	UniverseAuthorityService &service = UniverseAuthorityService::Instance();
	service.Reset();

	RegisteredWorld w_core{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Capital Prime",
		.biome = WorldBiome::Temperate
	};
	RegisteredWorld w_exp{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase4_Expansion,
		.name = "Wild Rim Alpha",
		.biome = WorldBiome::Volcanic
	};

	REQUIRE(service.RegisterWorld(w_core));
	REQUIRE(service.RegisterWorld(w_exp));

	/* Colonizing a Phase 1 Core world fails */
	CHECK_FALSE(service.ColonizeWorld(WorldID{1}, "Illegal Core Colony"));

	/* Colonizing a non-existent world fails */
	CHECK_FALSE(service.ColonizeWorld(WorldID{99}, "Ghost"));

	/* Colonizing Phase 4 Expansion world succeeds */
	CHECK(service.ColonizeWorld(WorldID{2}, "Fortuna Station"));

	const RegisteredWorld *colonized = service.GetWorld(WorldID{2});
	REQUIRE(colonized != nullptr);
	CHECK(colonized->phase == WorldPhase::Phase3_Frontier);
	CHECK(colonized->name == "Fortuna Station");
	CHECK(colonized->biome == WorldBiome::Volcanic);

	/* Cannot colonize already colonized world */
	CHECK_FALSE(service.ColonizeWorld(WorldID{2}, "Duplicate"));
}
