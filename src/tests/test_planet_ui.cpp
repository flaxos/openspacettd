/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_planet_ui.cpp Unit tests for planet UI labels, biomes, phases, and navigation helpers. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/world_gen.h"
#include "../core/format.hpp"

#include <string_view>

#include "../safeguards.h"

TEST_CASE("Planet UI - Phase & Biome Names")
{
	SECTION("World Phase Strings")
	{
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(WorldPhase::Phase1_Core)) == "Phase 1 (Core)");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(WorldPhase::Phase2_Developed)) == "Phase 2 (Developed)");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(WorldPhase::Phase3_Frontier)) == "Phase 3 (Frontier)");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(WorldPhase::Phase4_Expansion)) == "Phase 4 (Expansion)");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(static_cast<WorldPhase>(99))) == "Unknown Phase");
	}

	SECTION("World Biome Strings")
	{
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(WorldBiome::Temperate)) == "Temperate");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(WorldBiome::SubArctic)) == "Sub-Arctic");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(WorldBiome::SubTropic)) == "Sub-Tropic");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(WorldBiome::Volcanic)) == "Volcanic");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(WorldBiome::AridDesert)) == "Arid Desert");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(WorldBiome::Oceanic)) == "Oceanic");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(static_cast<WorldBiome>(99))) == "Standard");
	}
}

TEST_CASE("Planet UI - Viewport Status & Navigation")
{
	Map::Allocate(1024, 1024);
	PlanetManager::Reset();

	SECTION("Empty Manager Behavior")
	{
		CHECK(PlanetManager::Count() == 0);
		/* Without any registered planets, viewport status text is empty */
		CHECK(PlanetManager::GetViewportStatusText(nullptr).empty());
		/* Jump to non-existent planet fails cleanly */
		CHECK_FALSE(PlanetManager::JumpToPlanet(WorldID{0}));
		CHECK_FALSE(PlanetManager::JumpToPlanet(WorldID{99}));
	}

	SECTION("Registered Worlds Navigation & Viewport Resolution")
	{
		PlanetRegion core_world{
			.id = WorldID{0},
			.name = "Earth Prime",
			.phase = WorldPhase::Phase1_Core,
			.biome = WorldBiome::Temperate,
			.min_x = 0,
			.min_y = 0,
			.max_x = 399,
			.max_y = 399,
			.development_score = 10000,
		};

		PlanetRegion volcanic_world{
			.id = WorldID{1},
			.name = "Vulcan Forge",
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
			.name = "Ceres Outpost",
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
			.name = "Haven Rim",
			.phase = WorldPhase::Phase4_Expansion,
			.biome = WorldBiome::AridDesert,
			.min_x = 500,
			.min_y = 500,
			.max_x = 899,
			.max_y = 899,
			.development_score = 100,
		};

		REQUIRE(PlanetManager::RegisterRegion(core_world));
		REQUIRE(PlanetManager::RegisterRegion(volcanic_world));
		REQUIRE(PlanetManager::RegisterRegion(frontier_world));
		REQUIRE(PlanetManager::RegisterRegion(expansion_world));

		CHECK(PlanetManager::Count() == 4);

		/* Test JumpToPlanet for valid vs invalid world IDs */
		CHECK(PlanetManager::JumpToPlanet(WorldID{0}));
		CHECK(PlanetManager::JumpToPlanet(WorldID{1}));
		CHECK(PlanetManager::JumpToPlanet(WorldID{2}));
		CHECK(PlanetManager::JumpToPlanet(WorldID{3}));
		CHECK_FALSE(PlanetManager::JumpToPlanet(WorldID{4}));
		CHECK_FALSE(PlanetManager::JumpToPlanet(WorldID{99}));

		/* In headless test environment (no main window instantiated), GetViewportCurrentPlanet returns nullptr */
		CHECK(PlanetManager::GetViewportCurrentPlanet(nullptr) == nullptr);
		/* And GetViewportStatusText returns interplanetary void */
		CHECK(PlanetManager::GetViewportStatusText(nullptr) == "[Interplanetary Void]");

		/* Test planet detection by tile for UI queries */
		TileIndex tile_core = TileXY(200, 200);
		TileIndex tile_volcanic = TileXY(700, 200);
		TileIndex tile_frontier = TileXY(200, 700);
		TileIndex tile_expansion = TileXY(700, 700);
		TileIndex tile_void = TileXY(450, 450);

		const PlanetRegion *reg_core = PlanetManager::GetRegionByTile(tile_core);
		REQUIRE(reg_core != nullptr);
		CHECK(reg_core->name == "Earth Prime");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(reg_core->phase)) == "Phase 1 (Core)");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(reg_core->biome)) == "Temperate");

		const PlanetRegion *reg_volcanic = PlanetManager::GetRegionByTile(tile_volcanic);
		REQUIRE(reg_volcanic != nullptr);
		CHECK(reg_volcanic->name == "Vulcan Forge");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(reg_volcanic->phase)) == "Phase 2 (Developed)");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(reg_volcanic->biome)) == "Volcanic");

		const PlanetRegion *reg_frontier = PlanetManager::GetRegionByTile(tile_frontier);
		REQUIRE(reg_frontier != nullptr);
		CHECK(reg_frontier->name == "Ceres Outpost");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(reg_frontier->phase)) == "Phase 3 (Frontier)");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(reg_frontier->biome)) == "Sub-Arctic");

		const PlanetRegion *reg_expansion = PlanetManager::GetRegionByTile(tile_expansion);
		REQUIRE(reg_expansion != nullptr);
		CHECK(reg_expansion->name == "Haven Rim");
		CHECK(std::string_view(PlanetManager::GetWorldPhaseName(reg_expansion->phase)) == "Phase 4 (Expansion)");
		CHECK(std::string_view(PlanetManager::GetWorldBiomeName(reg_expansion->biome)) == "Arid Desert");

		const PlanetRegion *reg_void = PlanetManager::GetRegionByTile(tile_void);
		CHECK(reg_void == nullptr);

		/* Test Town / Station window title & info decoration formatting */
		std::string town_name = "New Oxford";
		std::string station_name = "Central Station";

		std::string decorated_town_caption = fmt::format("{} [{}]", town_name, reg_core->name);
		CHECK(decorated_town_caption == "New Oxford [Earth Prime]");

		std::string town_info_row = fmt::format("World: {} ({})", reg_core->name, PlanetManager::GetWorldPhaseName(reg_core->phase));
		CHECK(town_info_row == "World: Earth Prime (Phase 1 (Core))");

		std::string decorated_station_caption = fmt::format("{} [{}]", station_name, reg_volcanic->name);
		CHECK(decorated_station_caption == "Central Station [Vulcan Forge]");

		/* Format status bar text explicitly for a given region */
		std::string status_bar_text = fmt::format("[{}] {} | {}", reg_core->name, PlanetManager::GetWorldPhaseName(reg_core->phase), PlanetManager::GetWorldBiomeName(reg_core->biome));
		CHECK(status_bar_text == "[Earth Prime] Phase 1 (Core) | Temperate");
	}
}
