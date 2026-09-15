/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint30_biomes_colonization.cpp Unit tests for Sprint 30 exotic alien biomes and Phase 4 planetary colonization engine. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/world_gen.h"
#include "../portal/portal_cmd.h"
#include "../portal/universe_authority.h"
#include "../clear_map.h"
#include "../tree_map.h"
#include "../tile_map.h"
#include "../void_map.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../town.h"
#include "../rail_map.h"
#include "../road.h"
#include "../newgrf_house.h"
#include "../station_base.h"
#include "../news_func.h"
#include "../saveload/saveload_func.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../strings_func.h"
#include "../language.h"
#include "../timer/timer_game_calendar.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include <filesystem>

#include "../safeguards.h"

TEST_CASE("Sprint 30 Biomes - All 6 Biomes Environmental Styling & Spatial Resolution")
{
	PlanetManager::Reset();
	Map::Allocate(256, 256);

	PlanetRegion r_temperate{
		.id = WorldID{0},
		.name = "Oaktree Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 4, .min_y = 4, .max_x = 39, .max_y = 39,
		.development_score = 10000
	};
	PlanetRegion r_arid{
		.id = WorldID{1},
		.name = "Merredin Industrial",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 44, .min_y = 4, .max_x = 79, .max_y = 39,
		.development_score = 5000
	};
	PlanetRegion r_arctic{
		.id = WorldID{2},
		.name = "Calyx Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 84, .min_y = 4, .max_x = 119, .max_y = 39,
		.development_score = 2000
	};
	PlanetRegion r_volcanic{
		.id = WorldID{3},
		.name = "Ignis Caldera",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 124, .min_y = 4, .max_x = 159, .max_y = 39,
		.development_score = 100
	};
	PlanetRegion r_subtropic{
		.id = WorldID{4},
		.name = "Viridis Rainforest",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::SubTropic,
		.min_x = 164, .min_y = 4, .max_x = 199, .max_y = 39,
		.development_score = 100
	};
	PlanetRegion r_oceanic{
		.id = WorldID{5},
		.name = "Pelagios Archipelago",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Oceanic,
		.min_x = 204, .min_y = 4, .max_x = 239, .max_y = 39,
		.development_score = 100
	};

	REQUIRE(PlanetManager::RegisterRegion(r_temperate));
	REQUIRE(PlanetManager::RegisterRegion(r_arid));
	REQUIRE(PlanetManager::RegisterRegion(r_arctic));
	REQUIRE(PlanetManager::RegisterRegion(r_volcanic));
	REQUIRE(PlanetManager::RegisterRegion(r_subtropic));
	REQUIRE(PlanetManager::RegisterRegion(r_oceanic));
	CHECK(PlanetManager::Count() == 6);

	/* Verify O(1) spatial resolution across all 6 biomes */
	CHECK(PlanetManager::GetTileBiome(TileXY(20, 20)) == WorldBiome::Temperate);
	CHECK(PlanetManager::GetTilePhase(TileXY(20, 20)) == WorldPhase::Phase1_Core);

	CHECK(PlanetManager::GetTileBiome(TileXY(60, 20)) == WorldBiome::AridDesert);
	CHECK(PlanetManager::GetTilePhase(TileXY(60, 20)) == WorldPhase::Phase2_Developed);

	CHECK(PlanetManager::GetTileBiome(TileXY(100, 20)) == WorldBiome::SubArctic);
	CHECK(PlanetManager::GetTilePhase(TileXY(100, 20)) == WorldPhase::Phase3_Frontier);

	CHECK(PlanetManager::GetTileBiome(TileXY(140, 20)) == WorldBiome::Volcanic);
	CHECK(PlanetManager::GetTilePhase(TileXY(140, 20)) == WorldPhase::Phase4_Expansion);

	CHECK(PlanetManager::GetTileBiome(TileXY(180, 20)) == WorldBiome::SubTropic);
	CHECK(PlanetManager::GetTilePhase(TileXY(180, 20)) == WorldPhase::Phase4_Expansion);

	CHECK(PlanetManager::GetTileBiome(TileXY(220, 20)) == WorldBiome::Oceanic);
	CHECK(PlanetManager::GetTilePhase(TileXY(220, 20)) == WorldPhase::Phase4_Expansion);

	/* Void buffer tile returns Temperate default */
	CHECK(PlanetManager::GetTileBiome(TileXY(41, 20)) == WorldBiome::Temperate);
	CHECK(PlanetManager::GetTileWorld(TileXY(41, 20)) == INVALID_WORLD);

	/* SetWorldBiome dynamic update */
	CHECK(PlanetManager::SetWorldBiome(WorldID{3}, WorldBiome::SubTropic));
	CHECK(PlanetManager::GetTileBiome(TileXY(140, 20)) == WorldBiome::SubTropic);
}

TEST_CASE("Sprint 30 Biomes - MultiWorldGen Biome Partitioning for 4+ Worlds")
{
	Map::Allocate(256, 256);

	MultiWorldGen::Config cfg;
	cfg.world_count = 6;
	cfg.place_gateways = false;

	std::vector<PlanetRegion> layout = MultiWorldGen::CalculateLayout(256, 256, cfg);
	REQUIRE(layout.size() == 6);

	/* Canonical 3 showcase worlds */
	CHECK(layout[0].phase == WorldPhase::Phase1_Core);
	CHECK(layout[0].biome == WorldBiome::Temperate);

	CHECK(layout[1].phase == WorldPhase::Phase2_Developed);
	CHECK(layout[1].biome == WorldBiome::AridDesert);

	CHECK(layout[2].phase == WorldPhase::Phase3_Frontier);
	CHECK(layout[2].biome == WorldBiome::SubArctic);

	/* Expansion wilderness worlds cycling through Volcanic, SubTropic, Oceanic */
	CHECK(layout[3].phase == WorldPhase::Phase4_Expansion);
	CHECK(layout[3].biome == WorldBiome::Volcanic);

	CHECK(layout[4].phase == WorldPhase::Phase4_Expansion);
	CHECK(layout[4].biome == WorldBiome::SubTropic);

	CHECK(layout[5].phase == WorldPhase::Phase4_Expansion);
	CHECK(layout[5].biome == WorldBiome::Oceanic);
}

TEST_CASE("Sprint 30 Biomes - Procedural Environmental Styling and Foliage")
{
	Map::Allocate(128, 128);

	PlanetRegion r_volcanic{
		.id = WorldID{3},
		.name = "Volcanic World",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 2, .min_y = 2, .max_x = 40, .max_y = 40
	};
	PlanetRegion r_subtropic{
		.id = WorldID{4},
		.name = "SubTropic World",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::SubTropic,
		.min_x = 45, .min_y = 2, .max_x = 80, .max_y = 40
	};
	PlanetRegion r_oceanic{
		.id = WorldID{5},
		.name = "Oceanic World",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Oceanic,
		.min_x = 85, .min_y = 2, .max_x = 120, .max_y = 40
	};

	TileIndex t_vol_clear = TileXY(10, 10);
	TileIndex t_vol_tree = TileXY(11, 10);
	MakeClear(t_vol_clear, ClearGround::Grass, 2);
	MakeTree(t_vol_tree, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);

	TileIndex t_sub_clear = TileXY(55, 10);
	TileIndex t_sub_tree = TileXY(56, 10);
	MakeClear(t_sub_clear, ClearGround::Grass, 1);
	MakeTree(t_sub_tree, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);

	TileIndex t_oce_clear = TileXY(95, 10);
	TileIndex t_oce_tree = TileXY(96, 10);
	MakeClear(t_oce_clear, ClearGround::Grass, 2);
	MakeTree(t_oce_tree, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);

	MultiWorldGen::ApplyBiomeStyling(r_volcanic);
	MultiWorldGen::ApplyBiomeStyling(r_subtropic);
	MultiWorldGen::ApplyBiomeStyling(r_oceanic);

	/* Volcanic: clear land is rough or rocky crust, trees have rough ground */
	ClearGround g_vol = GetClearGround(t_vol_clear);
	CHECK((g_vol == ClearGround::Rough || g_vol == ClearGround::Rocks));
	CHECK(GetTreeGround(t_vol_tree) == TreeGround::Rough);

	/* Sub-Tropic: tropic zone is Rainforest, trees are TREE_RAINFOREST */
	CHECK(GetTropicZone(t_sub_clear) == TropicZone::Rainforest);
	CHECK(GetTreeType(t_sub_tree) == TREE_RAINFOREST);
	CHECK(GetTreeGround(t_sub_tree) == TreeGround::Grass);

	/* Oceanic: clear land is grass or coastal rocks, trees are lush TREE_RAINFOREST */
	ClearGround g_oce = GetClearGround(t_oce_clear);
	CHECK((g_oce == ClearGround::Grass || g_oce == ClearGround::Rocks));
	CHECK(GetTreeType(t_oce_tree) == TREE_RAINFOREST);
	CHECK(GetTreeGround(t_oce_tree) == TreeGround::Grass);
}

TEST_CASE("Sprint 30 Colonization - Phase 4 Expansion World Pre-Colonization Restrictions")
{
	PlanetManager::Reset();
	Map::Allocate(128, 128);

	PlanetRegion r_expansion{
		.id = WorldID{0},
		.name = "Unsettled Rim",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 4, .min_y = 4, .max_x = 60, .max_y = 60,
		.development_score = 50
	};
	REQUIRE(PlanetManager::RegisterRegion(r_expansion));

	TileIndex t = TileXY(20, 20);

	/* Construction placement inside world boundary succeeds base check */
	CHECK(PlanetManager::CheckConstructionPlacement(t).Succeeded());

	/* Void tile fails placement */
	CHECK(PlanetManager::CheckConstructionPlacement(TileXY(2, 2)).Failed());

	/* Heavy industry placement is strictly restricted on uncolonized Phase 4 worlds */
	CommandCost raw_cost = PlanetManager::CheckIndustryPlacement(t, true, false);
	CHECK(raw_cost.Failed());
	CHECK(raw_cost.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);

	CommandCost proc_cost = PlanetManager::CheckIndustryPlacement(t, false, true);
	CHECK(proc_cost.Failed());
	CHECK(proc_cost.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);

	/* Rail depot construction is restricted until a colonial outpost is established */
	CommandCost depot_cost = PlanetManager::CheckDepotPlacement(t, RAILTYPE_BEGIN);
	CHECK(depot_cost.Failed());
	CHECK(depot_cost.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
}

TEST_CASE("Sprint 30 Colonization - PlanetManager ColonizeWorld and Promotion Hierarchy")
{
	PlanetManager::Reset();
	Map::Allocate(128, 128);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Core Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 4, .min_y = 4, .max_x = 30, .max_y = 30,
		.development_score = 10000
	};
	PlanetRegion r_exp{
		.id = WorldID{1},
		.name = "Wild Frontier Rim",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 35, .min_y = 4, .max_x = 70, .max_y = 30,
		.development_score = 25
	};
	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_exp));

	/* ColonizeWorld fails on Phase 1 Core world */
	CHECK_FALSE(PlanetManager::ColonizeWorld(WorldID{0}, "Attempt"));

	/* ColonizeWorld succeeds on Phase 4 Expansion world */
	CHECK(PlanetManager::ColonizeWorld(WorldID{1}, "New Caldera"));
	const PlanetRegion *updated = PlanetManager::GetRegion(WorldID{1});
	REQUIRE(updated != nullptr);
	CHECK(updated->phase == WorldPhase::Phase3_Frontier);
	CHECK(updated->name == "New Caldera");
	CHECK(updated->development_score == 125);

	/* Cannot colonize again once promoted to Phase 3 */
	CHECK_FALSE(PlanetManager::ColonizeWorld(WorldID{1}, "Duplicate"));

	/* Phase promotion hierarchy: Phase 3 -> Phase 2 -> Phase 1 */
	CHECK(PlanetManager::PromoteWorldPhase(WorldID{1}));
	CHECK(PlanetManager::GetRegion(WorldID{1})->phase == WorldPhase::Phase2_Developed);
	CHECK(PlanetManager::GetRegion(WorldID{1})->development_score == 375);

	CHECK(PlanetManager::PromoteWorldPhase(WorldID{1}));
	CHECK(PlanetManager::GetRegion(WorldID{1})->phase == WorldPhase::Phase1_Core);
	CHECK(PlanetManager::GetRegion(WorldID{1})->development_score == 875);

	/* Phase 1 Core is top tier and cannot be promoted further */
	CHECK_FALSE(PlanetManager::PromoteWorldPhase(WorldID{1}));
}

TEST_CASE("Sprint 30 Colonization - CmdColonizeOutpost Command and Rule Unlock")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
	extern std::string _config_language_file;
	auto saved_paths = _valid_searchpaths;
	auto saved_binary = _searchpaths[Searchpath::BinaryDir];
	_searchpaths[Searchpath::BinaryDir] = (std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "build").string() + "/";
	_valid_searchpaths = {Searchpath::BinaryDir};
	_languages.clear();
	_config_language_file = "english.lng";
	InitializeLanguagePacks();
	_valid_searchpaths = std::move(saved_paths);
	_searchpaths[Searchpath::BinaryDir] = std::move(saved_binary);
	_game_mode = GameMode::Normal;
	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	ResetHouses();
	InitializeBuildingCounts();
	ResetRoadTypes();

	Map::Allocate(128, 128);
	_town_pool.CleanPool();
	RebuildTownKdtree();
	_company_pool.CleanPool();
	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);
	_current_company = c->index;
	c->money = 1000000;
	_price[Price::BuildTown] = 10000;

	PlanetManager::Reset();
	PlanetRegion r_exp{
		.id = WorldID{0},
		.name = "Rim Sector 7",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 4, .min_y = 4, .max_x = 60, .max_y = 60,
		.development_score = 0
	};
	REQUIRE(PlanetManager::RegisterRegion(r_exp));

	TileIndex t_void = TileXY(2, 2);
	TileIndex t_world = TileXY(25, 25);
	for (uint y = 4; y <= 60; ++y) {
		for (uint x = 4; x <= 60; ++x) MakeClear(TileXY(x, y), ClearGround::Grass, 3);
	}

	/* Execution on void tile fails */
	CommandCost res_void = CmdColonizeOutpost({}, t_void, "Outpost Fail");
	CHECK(res_void.Failed());
	const auto phase_before_site_failure = PlanetManager::GetRegion(WorldID{0})->phase;
	const uint towns_before_site_failure = Town::GetNumItems();
	MakeRailNormal(t_world, c->index, TrackBits{Track::X}, RAILTYPE_RAIL);
	CHECK(CmdColonizeOutpost(DoCommandFlag::Execute, t_world, "Unsuitable Outpost").Failed());
	CHECK(PlanetManager::GetRegion(WorldID{0})->phase == phase_before_site_failure);
	CHECK(Town::GetNumItems() == towns_before_site_failure);
	MakeClear(t_world, ClearGround::Grass, 3);

	/* Test-mode execution succeeds and verifies non-zero cost */
	CommandCost res_test = CmdColonizeOutpost(DoCommandFlag::Auto, t_world, "Outpost Alpha");
	CHECK(res_test.Succeeded());
	CHECK(res_test.GetCost() > 0);
	/* Phase unchanged in test mode */
	CHECK(PlanetManager::GetRegion(WorldID{0})->phase == WorldPhase::Phase4_Expansion);

	/* Denied callers and insufficient funds cannot create a town or change phase. */
	_current_company = CompanyID::Invalid();
	CHECK(CmdColonizeOutpost(DoCommandFlag::Execute, t_world, "Denied outpost").Failed());
	CHECK(Town::GetNumItems() == 0);
	CHECK(PlanetManager::GetRegion(WorldID{0})->phase == WorldPhase::Phase4_Expansion);
	_current_company = c->index;
	Money saved_money = c->money;
	c->money = 0;
	CHECK(CmdColonizeOutpost(DoCommandFlag::Execute, t_world, "Unaffordable outpost").Failed());
	CHECK(Town::GetNumItems() == 0);
	CHECK(PlanetManager::GetRegion(WorldID{0})->phase == WorldPhase::Phase4_Expansion);
	c->money = saved_money;

	/* Exhaustion is injected at the pool admission check without allocating
	 * thousands of fake towns; command failure must leave the world untouched. */
	{
		AutoRestoreBackup pool_items(_town_pool.items, TownPool::MAX_SIZE);
		REQUIRE_FALSE(Town::CanAllocateItem());
		CHECK(CmdColonizeOutpost(DoCommandFlag::Auto, t_world, "No town slots").Failed());
		CHECK(CmdColonizeOutpost(DoCommandFlag::Execute, t_world, "No town slots").Failed());
		CHECK(PlanetManager::GetRegion(WorldID{0})->phase == WorldPhase::Phase4_Expansion);
		CHECK(PlanetManager::GetRegion(WorldID{0})->name == "Rim Sector 7");
		CHECK(PlanetManager::GetRegion(WorldID{0})->development_score == 0);
		CHECK(PlanetManager::GetWorldPrimaryTown(WorldID{0}) == nullptr);
	}
	CHECK(Town::GetNumItems() == 0);

	/* Legacy metadata must stay intact until a separate town repair is chosen. */
	REQUIRE(Town::CanAllocateItem());
	Town *legacy_town = Town::Create(t_world);
	REQUIRE(legacy_town != nullptr);
	legacy_town->name = "Legacy Shell";
	REQUIRE(PlanetManager::GetWorldPrimaryTown(WorldID{0}) == legacy_town);
	REQUIRE(legacy_town->cache.num_houses == 0);
	REQUIRE(legacy_town->cache.population == 0);
	CommandCost legacy_query = CmdColonizeOutpost(DoCommandFlag::Auto, t_world, "Outpost Alpha");
	CommandCost legacy_execute = CmdColonizeOutpost(DoCommandFlag::Execute, t_world, "Outpost Alpha");
	CHECK(legacy_query.GetErrorMessage() == STR_ERROR_CANNOT_COLONIZE_INCOMPLETE_OUTPOST);
	CHECK(legacy_execute.GetErrorMessage() == STR_ERROR_CANNOT_COLONIZE_INCOMPLETE_OUTPOST);
	CHECK(PlanetManager::GetRegion(WorldID{0})->phase == WorldPhase::Phase4_Expansion);
	CHECK(PlanetManager::GetRegion(WorldID{0})->name == "Rim Sector 7");
	CHECK(PlanetManager::GetRegion(WorldID{0})->development_score == 0);
	CHECK(PlanetManager::GetWorldPrimaryTown(WorldID{0}) == legacy_town);
	CHECK(legacy_town->name == "Legacy Shell");
	delete legacy_town;
	REQUIRE(Town::GetNumItems() == 0);

	/* Live execution initializes a native settlement before elevating the world. */
	CommandCost res_exec = CmdColonizeOutpost(DoCommandFlags{DoCommandFlag::Execute, DoCommandFlag::Auto}, t_world, "Outpost Alpha");
	REQUIRE(res_exec.Succeeded());

	const PlanetRegion *promoted = PlanetManager::GetRegion(WorldID{0});
	REQUIRE(promoted != nullptr);
	CHECK(promoted->phase == WorldPhase::Phase3_Frontier);
	CHECK(promoted->name == "Outpost Alpha");
	CHECK(promoted->development_score == 100);
	Town *settlement = PlanetManager::GetWorldPrimaryTown(WorldID{0});
	REQUIRE(settlement != nullptr);
	CHECK(settlement->xy == t_world);
	CHECK(settlement->name == "Outpost Alpha");
	CHECK(settlement->cache.num_houses > 0);
	CHECK(settlement->cache.population > 0);
	CHECK(PlanetManager::GetWorldPopulation(WorldID{0}) == settlement->cache.population);
	CHECK(CalcClosestTownFromTile(t_world) == settlement);
	CHECK(Town::GetNumItems() == 1);
	const auto directory = UniverseAuthorityService::Instance().GetWorldDirectoryForGUI();
	const auto directory_world = std::find_if(directory.begin(), directory.end(), [](const RegisteredWorld &entry) { return entry.world_id == WorldID{0}; });
	REQUIRE(directory_world != directory.end());
	CHECK(directory_world->name == promoted->name);
	CHECK(directory_world->phase == promoted->phase);

	/* Post-colonization build rules: raw extraction and depots now unlocked! */
	CHECK(PlanetManager::CheckIndustryPlacement(t_world, true, false).Succeeded());
	CHECK(PlanetManager::CheckDepotPlacement(t_world, RAILTYPE_BEGIN).Succeeded());

	/* Processing facilities still restricted on Phase 3 Frontier */
	CHECK(PlanetManager::CheckIndustryPlacement(t_world, false, true).Failed());
}

TEST_CASE("Sprint 30 Colonization - Save/Load Persistence of Promoted World Phase")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_sprint30_colonization.sav").string();
	std::filesystem::remove(test_save_file);

	Map::Allocate(128, 128);
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
	extern std::string _config_language_file;
	auto saved_paths = _valid_searchpaths;
	auto saved_binary = _searchpaths[Searchpath::BinaryDir];
	_searchpaths[Searchpath::BinaryDir] = (std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "build").string() + "/";
	_valid_searchpaths = {Searchpath::BinaryDir};
	_languages.clear();
	_config_language_file = "english.lng";
	InitializeLanguagePacks();
	_valid_searchpaths = std::move(saved_paths);
	_searchpaths[Searchpath::BinaryDir] = std::move(saved_binary);
	_game_mode = GameMode::Normal;
	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	ResetHouses();
	InitializeBuildingCounts();
	ResetRoadTypes();
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	_company_pool.CleanPool();
	_town_pool.CleanPool();
	RebuildTownKdtree();
	_station_pool.CleanPool();
	InitNewsItemStructs();
	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);
	_current_company = c->index;
	c->money = 1000000;
	_price[Price::BuildTown] = 10000;

	PlanetManager::Reset();
	PlanetRegion r_exp{
		.id = WorldID{0},
		.name = "Dormant Caldera",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 4, .min_y = 4, .max_x = 100, .max_y = 100,
		.development_score = 10
	};
	REQUIRE(PlanetManager::RegisterRegion(r_exp));

	for (uint y = 4; y <= 100; ++y) {
		for (uint x = 4; x <= 100; ++x) MakeClear(TileXY(x, y), ClearGround::Grass, 3);
	}
	TileIndex outpost_tile = TileXY(50, 50);
	REQUIRE(CmdColonizeOutpost(DoCommandFlags{DoCommandFlag::Execute, DoCommandFlag::Auto}, outpost_tile, "Fortuna Caldera").Succeeded());
	CHECK(PlanetManager::GetRegion(WorldID{0})->phase == WorldPhase::Phase3_Frontier);
	CHECK(PlanetManager::GetRegion(WorldID{0})->development_score == 110);
	Town *before_save = PlanetManager::GetWorldPrimaryTown(WorldID{0});
	REQUIRE(before_save != nullptr);
	REQUIRE(before_save->cache.num_houses > 0);
	REQUIRE(before_save->cache.population > 0);
	uint32_t population_before_save = before_save->cache.population;

	/* Save game state */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));

	/* Reset in-memory state */
	PlanetManager::Reset();
	CHECK(PlanetManager::Count() == 0);

	/* Reload game state */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	/* Verify colonization promotion persisted */
	REQUIRE(PlanetManager::Count() == 1);
	const PlanetRegion *loaded = PlanetManager::GetRegion(WorldID{0});
	REQUIRE(loaded != nullptr);
	CHECK(loaded->phase == WorldPhase::Phase3_Frontier);
	CHECK(loaded->biome == WorldBiome::Volcanic);
	CHECK(loaded->name == "Fortuna Caldera");
	CHECK(loaded->development_score == 110);
	CHECK(loaded->outpost_tile == outpost_tile);
	Town *reloaded_town = PlanetManager::GetWorldPrimaryTown(WorldID{0});
	REQUIRE(reloaded_town != nullptr);
	CHECK(reloaded_town->xy == outpost_tile);
	CHECK(reloaded_town->cache.num_houses > 0);
	CHECK(reloaded_town->cache.population == population_before_save);
	CHECK(CalcClosestTownFromTile(outpost_tile) == reloaded_town);

	std::filesystem::remove(test_save_file);
}
