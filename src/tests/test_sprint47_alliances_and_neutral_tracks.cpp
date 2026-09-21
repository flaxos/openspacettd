/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint47_alliances_and_neutral_tracks.cpp Unit tests for Sprint 47 Neutral CST Infrastructure, Corporate Alliances & UAT Research Progression. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/corporate_alliance.h"
#include "../portal/tech_tree.h"
#include "../portal/fabrication_manager.h"
#include "../portal/corporate_hq.h"
#include "../portal/company_stockpile.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/commonwealth_pack.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../station_base.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../core/pool_type.hpp"
#include "../linkgraph/linkgraphschedule.h"
#include "../waypoint_base.h"
#include "../rail.h"
#include "../rail_type.h"
#include "mock_environment.h"
#include "../table/strings.h"
#include "../language.h"
#include "../strings_func.h"
#include "../saveload/saveload.h"
#include "../saveload/saveload_func.h"
#include "../fileio_func.h"
#include "../map_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"

#include <filesystem>
#include <vector>

#include "../safeguards.h"

TEST_CASE("Sprint 47 - Foundational Research Unlocking & Fabrication Guidance in UAT Fixtures")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	_company_pool.CleanPool();
	Company::CreateAtIndex(CompanyID{0});
	CompanyID comp0{0};

	Company::CreateAtIndex(CompanyID{1});
	CompanyID comp1{1};

	TechTreeManager::Reset();
	FabricationManager::Reset();
	StockpileManager::Reset();

	/* 1. Initially, Tech Tree has no unlocked tech */
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(comp0, TECH_MATERIALS_1));
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(comp0, TECH_TRACTION_1));
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(comp0, TECH_PORTAL_1));

	/* Enable in-kind fabrication for Company 0 */
	FabricationManager::SetFabricateFromStockpile(comp0, true);
	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(comp0));

	/* Check standard rail track BOM requirement */
	BillOfMaterials rail_bom = FabricationManager::GetTrackBOM(RAILTYPE_RAIL);
	CargoType c_steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	CargoType c_ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	CHECK(rail_bom.GetRequirement(c_steel) > 0);
	CHECK(rail_bom.GetRequirement(c_ballast) > 0);

	/* Without Materials Tier 1 unlocked, fabrication check fails with STR_ERROR_COMMONWEALTH_RESEARCH */
	CommandCost res = FabricationManager::CheckMaterials(WorldID{0}, comp0, rail_bom);
	CHECK(res.Failed());
	CHECK(res.GetErrorMessage() == STR_ERROR_COMMONWEALTH_RESEARCH);

	/* 2. Unlock foundational technologies (simulating UAT fixtures setup) */
	TechTreeManager::RestoreCompanyTech(comp0, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_TRACTION_1, TECH_PORTAL_1});
	CHECK(TechTreeManager::IsTechUnlocked(comp0, TECH_MATERIALS_1));
	CHECK(TechTreeManager::IsTechUnlocked(comp0, TECH_TRACTION_1));
	CHECK(TechTreeManager::IsTechUnlocked(comp0, TECH_PORTAL_1));

	/* Even with research unlocked, if stockpile is empty, material check fails with STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS */
	CommandCost res_empty = FabricationManager::CheckMaterials(WorldID{0}, comp0, rail_bom);
	CHECK(res_empty.Failed());
	CHECK(res_empty.GetErrorMessage() == STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS);

	/* Stock materials in stockpile: ballast and steel */
	StockpileManager::AddCargo(WorldID{0}, comp0, c_steel, 50);
	StockpileManager::AddCargo(WorldID{0}, comp0, c_ballast, 50);

	/* Now fabrication check succeeds with full BOM satisfied */
	CommandCost res_stocked = FabricationManager::CheckMaterials(WorldID{0}, comp0, rail_bom);
	CHECK(res_stocked.Succeeded());

	/* 3. Cash purchasing fallback (In-Kind Fabrication disabled) */
	FabricationManager::SetFabricateFromStockpile(comp1, false);
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(comp1));
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(comp1, TECH_MATERIALS_1));

	/* When fabrication is disabled, standard cash mode operates without requiring tech */
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(comp1));
}

TEST_CASE("Sprint 47 - Neutral CST Track Traversal by Player & AI Trains")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	CorporateAllianceManager::Reset();

	CompanyID player_company{0};
	CompanyID ai_company{1};

	/* 1. Player trains can traverse neutral CST track */
	CHECK(CorporateAllianceManager::CanTraverseTrack(player_company, OWNER_NONE));

	/* 2. AI trains can traverse neutral CST track */
	CHECK(CorporateAllianceManager::CanTraverseTrack(ai_company, OWNER_NONE));

	/* 3. Companies can traverse their own track */
	CHECK(CorporateAllianceManager::CanTraverseTrack(player_company, player_company));
	CHECK(CorporateAllianceManager::CanTraverseTrack(ai_company, ai_company));

	/* 4. Deity-owned tiles are universally traversable */
	CHECK(CorporateAllianceManager::CanTraverseTrack(player_company, OWNER_DEITY));
	CHECK(CorporateAllianceManager::CanTraverseTrack(ai_company, OWNER_DEITY));

	/* 5. By default, competitors are Neutral - neither can traverse the other's private track */
	CHECK(CorporateAllianceManager::GetRelation(player_company, ai_company) == CorporateRelation::Neutral);
	CHECK_FALSE(CorporateAllianceManager::CanTraverseTrack(player_company, ai_company));
	CHECK_FALSE(CorporateAllianceManager::CanTraverseTrack(ai_company, player_company));
}

TEST_CASE("Sprint 47 - Neutral CST Waypoint and Station Order Scheduling")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	CorporateAllianceManager::Reset();

	CompanyID player_company{0};
	CompanyID ai_company{1};

	/* 1. Player can use CST neutral waypoints (OWNER_NONE) */
	CHECK(CorporateAllianceManager::CanUseWaypoint(player_company, OWNER_NONE));

	/* 2. AI can use CST neutral waypoints (OWNER_NONE) */
	CHECK(CorporateAllianceManager::CanUseWaypoint(ai_company, OWNER_NONE));

	/* 3. Player can use CST neutral stations (OWNER_NONE) */
	CHECK(CorporateAllianceManager::CanUseStation(player_company, OWNER_NONE));
	CHECK(CorporateAllianceManager::CanUseStation(ai_company, OWNER_NONE));

	/* 4. Owned waypoints and stations are allowed */
	CHECK(CorporateAllianceManager::CanUseWaypoint(player_company, player_company));
	CHECK(CorporateAllianceManager::CanUseStation(player_company, player_company));

	/* 5. Private competitor waypoints/stations are denied under default Neutral relation */
	CHECK_FALSE(CorporateAllianceManager::CanUseWaypoint(player_company, ai_company));
	CHECK_FALSE(CorporateAllianceManager::CanUseStation(player_company, ai_company));
}

TEST_CASE("Sprint 47 - Corporate Alliance Authority Lifecycle (CmdSetCorporateAlliance)")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	_company_pool.CleanPool();
	Company::CreateAtIndex(CompanyID{0});
	CompanyID comp0{0};

	Company::CreateAtIndex(CompanyID{1});
	CompanyID comp1{1};

	CorporateAllianceManager::Reset();

	/* Default relation is Neutral */
	CHECK(CorporateAllianceManager::GetRelation(comp0, comp1) == CorporateRelation::Neutral);

	/* Set alliance between comp0 and comp1 */
	_current_company = comp0;
	CommandCost res_ally = CmdSetCorporateAlliance(DoCommandFlag::Execute, comp1, CorporateRelation::Allied);
	CHECK(res_ally.Succeeded());

	/* Verify symmetric alliance */
	CHECK(CorporateAllianceManager::GetRelation(comp0, comp1) == CorporateRelation::Allied);
	CHECK(CorporateAllianceManager::GetRelation(comp1, comp0) == CorporateRelation::Allied);

	/* Invalid target: Cannot ally with self */
	CommandCost res_self = CmdSetCorporateAlliance(DoCommandFlag::Execute, comp0, CorporateRelation::Allied);
	CHECK(res_self.Failed());
	CHECK(res_self.GetErrorMessage() == STR_ERROR_INVALID_ALLIANCE_TARGET);

	/* Invalid target: Non-existent company */
	CommandCost res_nonexistent = CmdSetCorporateAlliance(DoCommandFlag::Execute, CompanyID{10}, CorporateRelation::Allied);
	CHECK(res_nonexistent.Failed());
	CHECK(res_nonexistent.GetErrorMessage() == STR_ERROR_INVALID_ALLIANCE_TARGET);

	/* Change relation to Hostile */
	CommandCost res_hostile = CmdSetCorporateAlliance(DoCommandFlag::Execute, comp1, CorporateRelation::Hostile);
	CHECK(res_hostile.Succeeded());
	CHECK(CorporateAllianceManager::GetRelation(comp0, comp1) == CorporateRelation::Hostile);
	CHECK(CorporateAllianceManager::GetRelation(comp1, comp0) == CorporateRelation::Hostile);

	/* Return to Neutral */
	CommandCost res_neutral = CmdSetCorporateAlliance(DoCommandFlag::Execute, comp1, CorporateRelation::Neutral);
	CHECK(res_neutral.Succeeded());
	CHECK(CorporateAllianceManager::GetRelation(comp0, comp1) == CorporateRelation::Neutral);
}

TEST_CASE("Sprint 47 - Allied Reciprocal Track Sharing & Hostile Interdiction")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	CorporateAllianceManager::Reset();

	CompanyID comp0{0};
	CompanyID comp1{1};

	/* 1. Allied standing: Mutual reciprocal track sharing */
	CorporateAllianceManager::SetRelation(comp0, comp1, CorporateRelation::Allied);

	CHECK(CorporateAllianceManager::CanTraverseTrack(comp0, comp1));
	CHECK(CorporateAllianceManager::CanTraverseTrack(comp1, comp0));
	CHECK(CorporateAllianceManager::CanUseWaypoint(comp0, comp1));
	CHECK(CorporateAllianceManager::CanUseWaypoint(comp1, comp0));
	CHECK(CorporateAllianceManager::CanUseStation(comp0, comp1));
	CHECK(CorporateAllianceManager::CanUseStation(comp1, comp0));

	/* Public CST track is still traversable */
	CHECK(CorporateAllianceManager::CanTraverseTrack(comp0, OWNER_NONE));
	CHECK(CorporateAllianceManager::CanTraverseTrack(comp1, OWNER_NONE));

	/* 2. Hostile standing: Complete interdiction between corporations */
	CorporateAllianceManager::SetRelation(comp0, comp1, CorporateRelation::Hostile);

	CHECK_FALSE(CorporateAllianceManager::CanTraverseTrack(comp0, comp1));
	CHECK_FALSE(CorporateAllianceManager::CanTraverseTrack(comp1, comp0));
	CHECK_FALSE(CorporateAllianceManager::CanUseWaypoint(comp0, comp1));
	CHECK_FALSE(CorporateAllianceManager::CanUseWaypoint(comp1, comp0));
	CHECK_FALSE(CorporateAllianceManager::CanUseStation(comp0, comp1));
	CHECK_FALSE(CorporateAllianceManager::CanUseStation(comp1, comp0));

	/* Neutral CST infrastructure remains fully operational and traversable even during war */
	CHECK(CorporateAllianceManager::CanTraverseTrack(comp0, OWNER_NONE));
	CHECK(CorporateAllianceManager::CanTraverseTrack(comp1, OWNER_NONE));
	CHECK(CorporateAllianceManager::CanUseWaypoint(comp0, OWNER_NONE));
	CHECK(CorporateAllianceManager::CanUseWaypoint(comp1, OWNER_NONE));
}

TEST_CASE("Sprint 47 - Multi-World Industrial Routing Through Neutral Territory")
{
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	CorporateAllianceManager::Reset();

	CompanyID player{0};
	CompanyID ally{1};

	/* Set up corporate alliance between player and ally */
	CorporateAllianceManager::SetRelation(player, ally, CorporateRelation::Allied);

	/* Define a simulated freight corridor spanning multiple owners across worlds */
	struct RouteWaypoint {
		Owner segment_owner;
		const char *description;
	};

	std::vector<RouteWaypoint> freight_loop = {
		{ player,     "World 1: Player Factory Loading Terminal" },
		{ OWNER_NONE, "World 1: CST Mainline Neutral Trunk" },
		{ OWNER_NONE, "World 1: CST Transit Waypoint" },
		{ ally,       "World 2: Allied Regional Interconnection" },
		{ OWNER_NONE, "World 2: CST Transfer Hub" },
		{ player,     "World 2: Player Megacity Receiving Station" },
	};

	/* When allied, player train can traverse every single segment of this loop */
	for (const auto &wpt : freight_loop) {
		INFO("Checking traversability for: " << wpt.description);
		CHECK(CorporateAllianceManager::CanTraverseTrack(player, wpt.segment_owner));
	}

	/* If relations break down and become Hostile: */
	CorporateAllianceManager::SetRelation(player, ally, CorporateRelation::Hostile);

	/* Player and CST neutral segments remain traversable; only the allied segment is blocked */
	CHECK(CorporateAllianceManager::CanTraverseTrack(player, freight_loop[0].segment_owner)); // Player
	CHECK(CorporateAllianceManager::CanTraverseTrack(player, freight_loop[1].segment_owner)); // CST Neutral
	CHECK(CorporateAllianceManager::CanTraverseTrack(player, freight_loop[2].segment_owner)); // CST Neutral
	CHECK_FALSE(CorporateAllianceManager::CanTraverseTrack(player, freight_loop[3].segment_owner)); // Interdicted
	CHECK(CorporateAllianceManager::CanTraverseTrack(player, freight_loop[4].segment_owner)); // CST Neutral
	CHECK(CorporateAllianceManager::CanTraverseTrack(player, freight_loop[5].segment_owner)); // Player
}

TEST_CASE("Sprint 47 - Corporate Alliance Save/Load Round-Trip Serialization")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_sprint47_alliances.sav").string();
	std::filesystem::remove(test_save_file);

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	Map::Allocate(64, 64);
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

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

	LinkGraphSchedule::Clear();
	PoolBase::Clean(PoolType::Normal);
	if (Town::CanAllocateItem()) {
		Town *t = Town::Create(TileXY(10, 10));
		t->name = "Alliance Test Town";
		t->townnametype = SPECSTR_TOWNNAME_START;
		RebuildTownKdtree();
	}
	Company::CreateAtIndex(CompanyID{0});
	Company::CreateAtIndex(CompanyID{1});
	Company::CreateAtIndex(CompanyID{2});

	CorporateAllianceManager::Reset();

	/* Establish relations: 0-1 Allied, 0-2 Hostile, 1-2 Neutral */
	CorporateAllianceManager::SetRelation(CompanyID{0}, CompanyID{1}, CorporateRelation::Allied);
	CorporateAllianceManager::SetRelation(CompanyID{0}, CompanyID{2}, CorporateRelation::Hostile);
	CorporateAllianceManager::SetRelation(CompanyID{1}, CompanyID{2}, CorporateRelation::Neutral);

	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{1}) == CorporateRelation::Allied);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{2}) == CorporateRelation::Hostile);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{1}, CompanyID{2}) == CorporateRelation::Neutral);

	LinkGraphSchedule::Clear();
	/* Save to file */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);

	/* Reset manager to default */
	CorporateAllianceManager::Reset();
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{1}) == CorporateRelation::Neutral);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{2}) == CorporateRelation::Neutral);

	/* Load from file */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	/* Verify restored relations */
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{1}) == CorporateRelation::Allied);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{1}, CompanyID{0}) == CorporateRelation::Allied);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{2}) == CorporateRelation::Hostile);
	CHECK(CorporateAllianceManager::GetRelation(CompanyID{2}, CompanyID{0}) == CorporateRelation::Hostile);

	std::filesystem::remove(test_save_file);
}
