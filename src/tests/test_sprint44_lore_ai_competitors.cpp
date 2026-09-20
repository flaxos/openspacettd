/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint44_lore_ai_competitors.cpp Unit tests for Sprint 44 Autonomous Lore-Driven AI Competitors. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/lore_competitor.h"
#include "../portal/corporate_alliance.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/production_chain.h"
#include "../portal/tech_tree.h"
#include "../portal/federation_staging.h"
#include "../portal/universe_authority.h"
#include "../blueprint/blueprint_manager.h"
#include "../blueprint/blueprint_cmd.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../station_base.h"
#include "../rail_map.h"
#include "../rail.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../signal_func.h"
#include "../depot_base.h"
#include "../vehicle_base.h"
#include "../town.h"
#include "../map_func.h"
#include "../saveload/saveload.h"
#include "../saveload/saveload_func.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../economy_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "mock_environment.h"

#include <filesystem>
#include <vector>
#include <algorithm>

#include "../safeguards.h"

static void SetupTestEnvironment(uint32_t map_w = 256, uint32_t map_h = 256)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}

	ResetRailTypes();
	StationClass::Reset();
	_vehicle_pool.CleanPool();
	_depot_pool.CleanPool();
	_station_pool.CleanPool();
	_town_pool.CleanPool();
	_company_pool.CleanPool();

	if (_current_language == nullptr) {
		extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
		auto saved_paths = _valid_searchpaths;
		auto saved_binary = _searchpaths[Searchpath::BinaryDir];
		_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
		_valid_searchpaths = {Searchpath::BinaryDir};
		InitializeLanguagePacks();
		_valid_searchpaths = std::move(saved_paths);
		_searchpaths[Searchpath::BinaryDir] = std::move(saved_binary);
	}

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	_price[Price::BuildRail] = 100;
	_price[Price::BuildSignals] = 50;
	_price[Price::BuildDepotTrain] = 500;
	_price[Price::BuildStationRail] = 200;
	_price[Price::BuildStationRailLength] = 30;
	_settings_game.station.station_spread = 64;
	_settings_game.difficulty.infinite_money = false;
	_settings_game.difficulty.town_council_tolerance = TOWN_COUNCIL_PERMISSIVE;

	PlanetManager::Reset();
	PlanetRegion w0{
		.id = WorldID{0},
		.name = "Terra Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 79,
		.max_y = 255,
	};
	PlanetRegion w1{
		.id = WorldID{1},
		.name = "Merredin Developed",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 80,
		.min_y = 0,
		.max_x = 159,
		.max_y = 255,
	};
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Calyx Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::Temperate,
		.min_x = 160,
		.min_y = 0,
		.max_x = 255,
		.max_y = 255,
	};
	PlanetManager::RegisterRegion(w0);
	PlanetManager::RegisterRegion(w1);
	PlanetManager::RegisterRegion(w2);

	if (Town::CanAllocateItem()) {
		Town *town0 = Town::Create(TileXY(15, 15));
		if (town0 != nullptr) {
			town0->name = "CST Anchor City";
			town0->townnametype = SPECSTR_TOWNNAME_START;
		}
	}
	if (Town::CanAllocateItem()) {
		Town *town1 = Town::Create(TileXY(95, 25));
		if (town1 != nullptr) {
			town1->name = "Merredin Core";
			town1->townnametype = SPECSTR_TOWNNAME_START;
		}
	}
	RebuildTownKdtree();
	RebuildStationKdtree();
}

TEST_CASE("Sprint 44 - Lore Competitor Initialization & Canonical Profiles")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	SetupTestEnvironment();

	/* Human player is company 0 */
	Company::CreateAtIndex(CompanyID{0});
	Company *c0 = Company::GetIfValid(CompanyID{0});
	REQUIRE(c0 != nullptr);
	c0->name = "Commonwealth Interplanetary Transport";

	LoreCompetitorManager::Reset();
	CorporateAllianceManager::Reset();

	REQUIRE(LoreCompetitorManager::SpawnCompetitors());

	/* 1. CST Profile Verification */
	const LoreCompetitorProfile *cst = LoreCompetitorManager::GetProfile(CompetitorType::CST);
	REQUIRE(cst != nullptr);
	CHECK(cst->name == "Commonwealth Synergy Transport");
	CHECK(cst->president_name == "Nigel Sheldon");
	CHECK(cst->colour == Colours::DarkGreen);
	CHECK(cst->home_world == WorldID{1});
	CHECK(cst->active);

	Company *comp_cst = Company::GetIfValid(cst->company_id);
	REQUIRE(comp_cst != nullptr);
	CHECK(comp_cst->is_ai);
	CHECK(comp_cst->money >= 50000000);

	/* 2. Grand Central Profile Verification */
	const LoreCompetitorProfile *gc = LoreCompetitorManager::GetProfile(CompetitorType::GrandCentral);
	REQUIRE(gc != nullptr);
	CHECK(gc->name == "Grand Central Trans-Portal");
	CHECK(gc->president_name == "Mellonie Gardner");
	CHECK(gc->colour == Colours::DarkBlue);
	CHECK(gc->home_world == WorldID{0});
	CHECK(gc->active);

	Company *comp_gc = Company::GetIfValid(gc->company_id);
	REQUIRE(comp_gc != nullptr);
	CHECK(comp_gc->is_ai);

	/* 3. InterWorld Logistics Profile Verification */
	const LoreCompetitorProfile *iw = LoreCompetitorManager::GetProfile(CompetitorType::InterWorld);
	REQUIRE(iw != nullptr);
	CHECK(iw->name == "InterWorld Logistics");
	CHECK(iw->president_name == "Bradley Johansson");
	CHECK(iw->colour == Colours::Yellow);
	CHECK(iw->home_world == WorldID{2});
	CHECK(iw->active);

	Company *comp_iw = Company::GetIfValid(iw->company_id);
	REQUIRE(comp_iw != nullptr);
	CHECK(comp_iw->is_ai);

	/* 4. Diplomatic Treaties & Track Rights with Player */
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, cst->company_id) == CorporateRelation::Neutral);
	CHECK_FALSE(CorporateAllianceManager::CanTraverseTrack(CompanyID{0}, static_cast<Owner>(cst->company_id.base())));

	/* Change treaty to Allied */
	CorporateAllianceManager::SetRelation(CompanyID{0}, cst->company_id, CorporateRelation::Allied);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, cst->company_id) == CorporateRelation::Allied);
	CHECK(CorporateAllianceManager::CanTraverseTrack(CompanyID{0}, static_cast<Owner>(cst->company_id.base())));
}

TEST_CASE("Sprint 44 - Deterministic Prefab Stamping via Commands::PlaceBlueprint")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	SetupTestEnvironment();

	Company::CreateAtIndex(CompanyID{0});
	LoreCompetitorManager::Reset();
	REQUIRE(LoreCompetitorManager::SpawnCompetitors());

	const LoreCompetitorProfile *cst = LoreCompetitorManager::GetProfile(CompetitorType::CST);
	REQUIRE(cst != nullptr);

	/* Stamp CST Mainline Double Straight at (20, 20) */
	TileIndex origin_cst = TileXY(20, 20);
	REQUIRE(LoreCompetitorManager::TriggerMilestoneExpansion(CompetitorType::CST, origin_cst, "CST Mainline Double Straight"));

	/* Inspect stamped tiles: should be owned by CST's company */
	for (int x = 20; x < 28; ++x) {
		TileIndex t1 = TileXY(x, 20);
		TileIndex t2 = TileXY(x, 21);
		CHECK(IsPlainRailTile(t1));
		CHECK(GetTileOwner(t1) == static_cast<Owner>(cst->company_id.base()));
		CHECK(IsPlainRailTile(t2));
		CHECK(GetTileOwner(t2) == static_cast<Owner>(cst->company_id.base()));
	}

	const LoreCompetitorProfile *updated_cst = LoreCompetitorManager::GetProfile(CompetitorType::CST);
	CHECK(updated_cst->prefabs_placed == 1);
	CHECK(updated_cst->placed_prefabs_history.size() == 1);
	CHECK(updated_cst->placed_prefabs_history.front() == "CST Mainline Double Straight");

	/* Stamp Grand Central Ro-Ro 4-Platform Terminal Station at (40, 20) */
	const LoreCompetitorProfile *gc = LoreCompetitorManager::GetProfile(CompetitorType::GrandCentral);
	REQUIRE(gc != nullptr);
	TileIndex origin_gc = TileXY(40, 20);
	REQUIRE(LoreCompetitorManager::TriggerMilestoneExpansion(CompetitorType::GrandCentral, origin_gc, "CST Ro-Ro 4-Platform Terminal Station Block"));

	const LoreCompetitorProfile *updated_gc = LoreCompetitorManager::GetProfile(CompetitorType::GrandCentral);
	CHECK(updated_gc->prefabs_placed == 1);

	/* Stamp InterWorld Portal Gate Approach Corridor at (60, 20) */
	const LoreCompetitorProfile *iw = LoreCompetitorManager::GetProfile(CompetitorType::InterWorld);
	REQUIRE(iw != nullptr);
	TileIndex origin_iw = TileXY(60, 20);
	REQUIRE(LoreCompetitorManager::TriggerMilestoneExpansion(CompetitorType::InterWorld, origin_iw, "CST Portal Gate Approach Corridor"));

	const LoreCompetitorProfile *updated_iw = LoreCompetitorManager::GetProfile(CompetitorType::InterWorld);
	CHECK(updated_iw->prefabs_placed == 1);
}

TEST_CASE("Sprint 44 - Dynamic Milestone Triggers & Autonomous Expansion Progression")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	SetupTestEnvironment();
	Company::CreateAtIndex(CompanyID{0});

	LoreCompetitorManager::Reset();
	REQUIRE(LoreCompetitorManager::SpawnCompetitors());

	/* Autonomous expansion tier progression on World 1 */
	TileIndex anchor1 = TileXY(90, 20);
	CHECK(LoreCompetitorManager::AutonomousExpand(CompetitorType::CST, WorldID{1}, anchor1));
	CHECK(LoreCompetitorManager::GetProfile(CompetitorType::CST)->milestone_tier == 1);

	TileIndex anchor2 = TileXY(110, 20);
	CHECK(LoreCompetitorManager::AutonomousExpand(CompetitorType::CST, WorldID{1}, anchor2));
	CHECK(LoreCompetitorManager::GetProfile(CompetitorType::CST)->milestone_tier == 2);

	/* Milestone trigger on population thresholds */
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(60, 60));
	REQUIRE(town != nullptr);
	town->cache.population = 6000; // Passes 2500 and 5000 thresholds

	LoreCompetitorManager::CheckMilestoneTriggers();

	const LoreCompetitorProfile *gc = LoreCompetitorManager::GetProfile(CompetitorType::GrandCentral);
	CHECK(gc->milestone_tier >= 2);
}

TEST_CASE("Sprint 44 - Gateway Transit Scheduling & Staging Siding Holding during Congestion")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	SetupTestEnvironment();
	Company::CreateAtIndex(CompanyID{0});

	LoreCompetitorManager::Reset();
	MegacityManager::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();

	REQUIRE(LoreCompetitorManager::SpawnCompetitors());

	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(50, 50));
	REQUIRE(town != nullptr);
	REQUIRE(MegacityManager::RegisterMegacity(town->index, WorldID{0}, "Augusta Metropole", 50000));

	/* 1. Clear transit to Augusta Megacity */
	RegisteredWorld world0;
	world0.world_id = WorldID{0};
	world0.status = WorldOnlineStatus::Online;
	world0.ping_ms = 20;
	UniverseAuthorityService::Instance().RegisterWorld(world0);

	CHECK_FALSE(FederationStagingManager::IsServerHoldingCondition(WorldID{0}));

	CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	REQUIRE(LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType::CST, WorldID{1}, WorldID{0}, steel, 100, FreightPriority::Bulk));

	const LoreCompetitorProfile *cst = LoreCompetitorManager::GetProfile(CompetitorType::CST);
	CHECK(cst->total_cargo_delivered == 100);
	CHECK(cst->total_revenue_earned > 0);

	const MegacityProfile *mp = MegacityManager::GetProfile(town->index);
	REQUIRE(mp != nullptr);
	CHECK(mp->delivered_current[to_underlying(MegacityDemandTier::Tier2_Expansion)] >= 100);

	/* 2. Corridor Saturated condition -> holding in staging */
	RegisteredWorld world_maint = world0;
	world_maint.status = WorldOnlineStatus::Maintenance;
	UniverseAuthorityService::Instance().RegisterWorld(world_maint);
	CHECK(FederationStagingManager::IsServerHoldingCondition(WorldID{0}));

	REQUIRE(LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType::GrandCentral, WorldID{1}, WorldID{0}, CargoType{0}, 80, FreightPriority::Express));

	auto transits_held = LoreCompetitorManager::GetActiveTransits();
	REQUIRE(!transits_held.empty());
	CHECK(transits_held.back().is_holding);
	uint32_t gc_transit_id = transits_held.back().transit_id;

	/* 3. Server recovers -> OnMonthlyTick releases from holding */
	world_maint.status = WorldOnlineStatus::Online;
	UniverseAuthorityService::Instance().RegisterWorld(world_maint);
	CHECK_FALSE(FederationStagingManager::IsServerHoldingCondition(WorldID{0}));

	LoreCompetitorManager::OnMonthlyTick();

	auto transits_after = LoreCompetitorManager::GetActiveTransits();
	auto it = std::find_if(transits_after.begin(), transits_after.end(), [gc_transit_id](const auto &t) {
		return t.transit_id == gc_transit_id;
	});
	REQUIRE(it != transits_after.end());
	CHECK_FALSE(it->is_holding);

	const LoreCompetitorProfile *gc = LoreCompetitorManager::GetProfile(CompetitorType::GrandCentral);
	CHECK(gc->total_cargo_delivered >= 80);
	CHECK(gc->total_revenue_earned > 0);
}

TEST_CASE("Sprint 44 - Megacity Multi-Commodity Supply Competition")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	SetupTestEnvironment();
	Company::CreateAtIndex(CompanyID{0});

	LoreCompetitorManager::Reset();
	MegacityManager::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();

	REQUIRE(LoreCompetitorManager::SpawnCompetitors());

	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(70, 70));
	REQUIRE(town != nullptr);
	REQUIRE(MegacityManager::RegisterMegacity(town->index, WorldID{0}, "Augusta Metropole", 10000));
	MegacityManager::SetCustomQuotas(town->index, 100, 100, 50);

	RegisteredWorld w0;
	w0.world_id = WorldID{0};
	w0.status = WorldOnlineStatus::Online;
	UniverseAuthorityService::Instance().RegisterWorld(w0);

	/* Rivals fulfill multi-tier quotas */
	CargoType minerals = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	CargoType crystals = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::EncryptedConsumerCrystals);

	/* InterWorld supplies Tier 1 Sustenance */
	LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType::InterWorld, WorldID{2}, WorldID{0}, minerals, 100);
	/* CST supplies Tier 2 Expansion */
	LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType::CST, WorldID{1}, WorldID{0}, steel, 100);
	/* Grand Central supplies Tier 3 Prosperity */
	LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType::GrandCentral, WorldID{1}, WorldID{0}, crystals, 50);

	MegacityManager::EvaluateMonthlySupply();

	const MegacityProfile *profile = MegacityManager::GetProfile(town->index);
	REQUIRE(profile != nullptr);
	CHECK(profile->satisfaction_pct[0] >= 1.0f);
	CHECK(profile->satisfaction_pct[1] >= 1.0f);
	CHECK(profile->satisfaction_pct[2] >= 1.0f);
	CHECK(profile->growth_state == MegacityGrowthState::HyperGrowth);
}

TEST_CASE("Sprint 44 - Save/Load Serialization via LORE Chunk")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_lore_competitor.sav").string();
	std::filesystem::remove(test_save_file);

	SetupTestEnvironment();
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	Company::CreateAtIndex(CompanyID{0});

	LoreCompetitorManager::Reset();
	REQUIRE(LoreCompetitorManager::SpawnCompetitors());

	/* Autonomous expand CST on World 1 */
	CHECK(LoreCompetitorManager::AutonomousExpand(CompetitorType::CST, WorldID{1}, TileXY(90, 30)));

	/* Dispatch transit */
	CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	CHECK(LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType::CST, WorldID{1}, WorldID{0}, steel, 150));

	const LoreCompetitorProfile *cst_before = LoreCompetitorManager::GetProfile(CompetitorType::CST);
	REQUIRE(cst_before != nullptr);
	uint32_t prefabs_before = cst_before->prefabs_placed;
	uint64_t cargo_before = cst_before->total_cargo_delivered;
	uint64_t revenue_before = cst_before->total_revenue_earned;
	uint32_t tier_before = cst_before->milestone_tier;

	/* Save game */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));

	/* Reset in-memory competitor state */
	LoreCompetitorManager::Reset();
	CHECK(LoreCompetitorManager::GetAllCompetitors().empty());

	/* Load game */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	const LoreCompetitorProfile *cst_after = LoreCompetitorManager::GetProfile(CompetitorType::CST);
	REQUIRE(cst_after != nullptr);
	CHECK(cst_after->name == "Commonwealth Synergy Transport");
	CHECK(cst_after->president_name == "Nigel Sheldon");
	CHECK(cst_after->prefabs_placed == prefabs_before);
	CHECK(cst_after->total_cargo_delivered == cargo_before);
	CHECK(cst_after->total_revenue_earned == revenue_before);
	CHECK(cst_after->milestone_tier == tier_before);

	std::filesystem::remove(test_save_file);
}
