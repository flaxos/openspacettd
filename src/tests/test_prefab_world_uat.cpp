/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_prefab_world_uat.cpp Unit tests for Commonwealth Prefab World Saves and UAT verification automation. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/prompt_scenario_generator.h"
#include "../portal/planet_manager.h"
#include "../portal/megacity_manager.h"
#include "../portal/corporate_hq.h"
#include "../portal/production_chain.h"
#include "../portal/prebuilt_trade.h"
#include "../portal/portal_registry.h"
#include "../linkgraph/linkgraphschedule.h"
#include "../core/pool_type.hpp"
#include "../saveload/saveload.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../train.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../fileio_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "mock_environment.h"
#include "../industry.h"
#include "../blueprint/blueprint.h"
#include "../blueprint/blueprint_manager.h"
#include "../blueprint/blueprint_cmd.h"
#include "../portal/fabrication_manager.h"
#include "../portal/tech_tree.h"
#include "../portal/federation_staging.h"
#include "../portal/corporate_charter.h"
#include "../console_func.h"
#include "../portal/portal_cmd.h"
#include "../signal_func.h"
#include "../clear_map.h"
#include "../core/backup_type.hpp"
#include <filesystem>

static constexpr IndustryType IT_STEEL_MILL = 8;
static constexpr IndustryType IT_IRON_MINE = 18;

#include "../safeguards.h"

TEST_CASE("Prefab World Saves - Automation & UAT Verification (Sprint 50 Tooling)", "[prefab_world][uat]")
{
	const std::string test_save_path = (std::filesystem::temp_directory_path() / "test_commonwealth_uat_matrix.sav").string();
	std::filesystem::remove(test_save_path);

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
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

	IConsoleInit();

	SECTION("Generate Commonwealth Prefab World and Verify In-Memory Invariants")
	{
		ScenarioSynthesisResult result = PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(test_save_path, 4);
		REQUIRE(result.success);
		CHECK(result.worlds_created == 4);
		CHECK(result.corridors_built >= 3);
		CHECK(result.trains_spawned >= 1);
		CHECK(result.facilities_placed >= 2);

		/* Verify file exists and has healthy size */
		REQUIRE(std::filesystem::exists(test_save_path));
		CHECK(std::filesystem::file_size(test_save_path) > 5000);

		/* Run built-in UAT verification */
		std::string uat_err;
		bool verified = PromptScenarioGenerator::VerifyCommonwealthUAT(&uat_err);
		INFO("UAT Error: " << uat_err);
		CHECK(verified);
		CHECK(uat_err.empty());

		/* Specific checks on prebuilt entities */
		CHECK(PlanetManager::Count() == 4);
		const PlanetRegion *sol = PlanetManager::GetRegion(WorldID{0});
		REQUIRE(sol != nullptr);
		CHECK(sol->name == "Sol Earth Core");
		CHECK(sol->phase == WorldPhase::Phase1_Core);

		const PlanetRegion *aug = PlanetManager::GetRegion(WorldID{1});
		REQUIRE(aug != nullptr);
		CHECK(aug->name == "Augusta CST Hub");
		CHECK(aug->phase == WorldPhase::Phase2_Developed);

		const PlanetRegion *mer = PlanetManager::GetRegion(WorldID{2});
		REQUIRE(mer != nullptr);
		CHECK(mer->name == "Merredin Mining Colony");
		CHECK(mer->phase == WorldPhase::Phase3_Frontier);
		CHECK(mer->biome == WorldBiome::AridDesert);

		const PlanetRegion *pro = PlanetManager::GetRegion(WorldID{3});
		REQUIRE(pro != nullptr);
		CHECK(pro->name == "Prometheus Caldera Outpost");
		CHECK(pro->phase == WorldPhase::Phase4_Expansion);
		CHECK(pro->biome == WorldBiome::Volcanic);

		/* Check trade gateway bound on Sol Earth */
		CHECK(PrebuiltTradeManager::Instance().GetAllTradeGateways().size() >= 1);
	}

	SECTION("Save/Load Round-Trip Persistence and TRAD Chunk Restoration")
	{
		ScenarioSynthesisResult gen_res = PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(test_save_path, 4);
		REQUIRE(gen_res.success);

		size_t initial_gateways = PrebuiltTradeManager::Instance().GetAllTradeGateways().size();
		REQUIRE(initial_gateways >= 1);

		/* Clear in-memory state before loading */
		PrebuiltTradeManager::Instance().Reset();
		CHECK(PrebuiltTradeManager::Instance().GetAllTradeGateways().empty());

		LinkGraphSchedule::Clear();

		/* Load the saved prefab world back from disk */
		SaveLoadResult load_res = SaveOrLoad(test_save_path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
		REQUIRE(load_res == SaveLoadResult::Ok);

		SetupEngines();
		StartupEngines();

		/* Verify TRAD chunk restored the prebuilt trade gateway */
		CHECK(PrebuiltTradeManager::Instance().GetAllTradeGateways().size() == initial_gateways);

		/* Run comprehensive UAT verification on reloaded world */
		std::string reload_uat_err;
		bool reload_verified = PromptScenarioGenerator::VerifyCommonwealthUAT(&reload_uat_err);
		INFO("Reload UAT Error: " << reload_uat_err);
		CHECK(reload_verified);
		CHECK(reload_uat_err.empty());
	}

	SECTION("Autonomous UAT Task 1: CST Prefab Stamping and Stockpile BOM Consumption")
	{
		ScenarioSynthesisResult gen_res = PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(test_save_path, 4);
		REQUIRE(gen_res.success);

		AutoRestoreBackup cur_company(_current_company, CompanyID{0});
		BlueprintManager::Initialize();

		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Mainline Double Straight");
		REQUIRE(bp != nullptr);

		/* Find a clear area on Augusta (World 1) */
		const PlanetRegion *aug = PlanetManager::GetRegion(WorldID{1});
		REQUIRE(aug != nullptr);
		TileIndex stamp_tile = TileXY(aug->min_x + 10, aug->min_y + 10);

		for (int dy = -1; dy <= bp->height + 1; ++dy) {
			for (int dx = -1; dx <= bp->width + 1; ++dx) {
				TileIndex t = TileAddWrap(stamp_tile, dx, dy);
				if (IsValidTile(t)) MakeClear(t, ClearGround::Grass, 0);
			}
		}

		CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
		CargoType metal   = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
		CargoType wiring  = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);

		uint32_t ballast_before = StockpileManager::GetStock(WorldID{1}, CompanyID{0}, ballast);
		uint32_t metal_before   = StockpileManager::GetStock(WorldID{1}, CompanyID{0}, metal);
		uint32_t wiring_before  = StockpileManager::GetStock(WorldID{1}, CompanyID{0}, wiring);

		CommandCost place_res = CmdPlaceBlueprint(DoCommandFlag::Execute, stamp_tile, bp->ToJson(), RAILTYPE_BEGIN, false);
		if (place_res.Failed()) {
			StringID err_id = place_res.GetErrorMessage();
			std::string err_str = (err_id != INVALID_STRING_ID) ? GetString(err_id) : "INVALID_STRING_ID";
			UNSCOPED_INFO("place_res error: " << err_str << " cost: " << place_res.GetCost());
		}
		REQUIRE(place_res.Succeeded());

		uint32_t ballast_after = StockpileManager::GetStock(WorldID{1}, CompanyID{0}, ballast);
		uint32_t metal_after   = StockpileManager::GetStock(WorldID{1}, CompanyID{0}, metal);
		uint32_t wiring_after  = StockpileManager::GetStock(WorldID{1}, CompanyID{0}, wiring);

		/* Track: 16 pieces (8x2), Signal: 2 signals. Total Ballast = 16, Metal = 16 + 2 = 18, Wiring = 2 */
		CHECK(ballast_after < ballast_before);
		CHECK(metal_after < metal_before);
		CHECK(wiring_after < wiring_before);

		/* Also test CLI command place_prefab */
		TileIndex stamp_tile2 = TileXY(aug->min_x + 10, aug->min_y + 20);
		for (int dy = -1; dy <= bp->height + 1; ++dy) {
			for (int dx = -1; dx <= bp->width + 1; ++dx) {
				TileIndex t = TileAddWrap(stamp_tile2, dx, dy);
				if (IsValidTile(t)) MakeClear(t, ClearGround::Grass, 0);
			}
		}
		IConsoleCmdExec(fmt::format("place_prefab {} \"CST Mainline Double Straight\"", stamp_tile2.base()));
		CHECK(IsPlainRailTile(stamp_tile2));
		UpdateSignalsInBuffer();
	}

	SECTION("Autonomous UAT Task 2: Resource Extraction and Freight Consignment Loop")
	{
		ScenarioSynthesisResult gen_res = PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(test_save_path, 4);
		REQUIRE(gen_res.success);

		/* Find Merredin station (World 2) and Augusta station (World 1) */
		Station *st_merredin = nullptr;
		Station *st_augusta = nullptr;
		for (Station *st : Station::Iterate()) {
			if (st->name.find("Merredin") != std::string::npos) st_merredin = st;
			if (st->name.find("Augusta CST Hub Central") != std::string::npos) st_augusta = st;
		}
		REQUIRE(st_merredin != nullptr);
		REQUIRE(st_augusta != nullptr);

		CargoType ore = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
		REQUIRE(ore < NUM_CARGO);

		/* Verify 100 tons pre-seeded Iron Ore waiting at Merredin */
		CHECK(st_merredin->goods[ore].AvailableCount() >= 100);

		/* Verify Train consist spawned and configured for Iron Ore */
		const Train *front_train = nullptr;
		for (const Train *t : Train::Iterate()) {
			if (t->IsFrontEngine()) {
				front_train = t;
				break;
			}
		}
		REQUIRE(front_train != nullptr);
		CHECK(front_train->owner == CompanyID{0});
		CHECK_FALSE(front_train->vehstatus.Test(VehState::Stopped));

		/* Verify wagons carry Iron Ore */
		size_t ore_wagons = 0;
		for (const Train *u = front_train; u != nullptr; u = u->Next()) {
			if (u->cargo_type == ore && u->cargo_cap > 0) ore_wagons++;
		}
		CHECK(ore_wagons >= 2);

		/* Verify industry catchment: Merredin station services Iron Mine, Augusta services Steel Mill */
		bool mine_serviced = false;
		for (const Industry *ind : Industry::Iterate()) {
			if (ind->type == IT_IRON_MINE) {
				if (ind->stations_near.find(st_merredin) != ind->stations_near.end()) mine_serviced = true;
			}
		}
		CHECK(mine_serviced);

		bool mill_accepts = false;
		for (const auto &entry : st_augusta->industries_near) {
			if (entry.industry->type == IT_STEEL_MILL) mill_accepts = true;
		}
		CHECK(mill_accepts);
	}

	SECTION("Autonomous UAT Task 3: Sprint 50 Gateway Staging and Holding Siding Queue")
	{
		ScenarioSynthesisResult gen_res = PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(test_save_path, 4);
		REQUIRE(gen_res.success);

		Station *st_holding = nullptr;
		for (Station *st : Station::Iterate()) {
			if (st->facilities.Test(StationFacility::HoldingSiding)) {
				st_holding = st;
				break;
			}
		}
		REQUIRE(st_holding != nullptr);
		CHECK(st_holding->name == "Augusta Gateway Holding Siding");
		CHECK(st_holding->facilities.Test(StationFacility::HoldingSiding));

		/* Find the Augusta portal tile */
		TileIndex gate_tile = INVALID_TILE;
		for (const auto &[pid, link] : PortalRegistry::GetAllPortals()) {
			if (link.end_a.world_id == WorldID{1}) {
				gate_tile = link.end_a.tile;
				break;
			} else if (link.end_b.world_id == WorldID{1}) {
				gate_tile = link.end_b.tile;
				break;
			}
		}
		REQUIRE(gate_tile != INVALID_TILE);

		/* Find closest designated staging siding for Augusta portal */
		TileIndex closest_siding = FederationStagingManager::FindStagingSidingForPortal(gate_tile);
		CHECK(closest_siding == st_holding->xy);

		/* Configure parallel throat for the gate */
		TileIndex parallel_throat = TileXY(TileX(gate_tile), TileY(gate_tile) + 1);
		CHECK(PortalRegistry::ConfigureParallelThroat(gate_tile, parallel_throat));
		CHECK(PortalRegistry::HasParallelThroat(gate_tile));
		CHECK(PortalRegistry::GetParallelThroat(gate_tile) == parallel_throat);

		auto throats = PortalRegistry::GetThroatTiles(gate_tile);
		REQUIRE(throats.size() == 2);
		CHECK(throats[0] == gate_tile);
		CHECK(throats[1] == parallel_throat);

		/* Test holding siding command toggle */
		AutoRestoreBackup cur_company(_current_company, CompanyID{0});
		CommandCost toggle_off = CmdDesignateHoldingSiding(DoCommandFlag::Execute, st_holding->index, false);
		CHECK(toggle_off.Succeeded());
		CHECK_FALSE(st_holding->facilities.Test(StationFacility::HoldingSiding));

		CommandCost toggle_on = CmdDesignateHoldingSiding(DoCommandFlag::Execute, st_holding->index, true);
		CHECK(toggle_on.Succeeded());
		CHECK(st_holding->facilities.Test(StationFacility::HoldingSiding));

		/* Verify staging siding is discovered again */
		CHECK(FederationStagingManager::FindStagingSidingForPortal(gate_tile) == st_holding->xy);
	}

	SECTION("Autonomous UAT Task 4: Sprint 50 Corporate Charters and Toll Collection")
	{
		ScenarioSynthesisResult gen_res = PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(test_save_path, 4);
		REQUIRE(gen_res.success);

		auto &charter_mgr = CorporateCharterManager::Instance();
		Company *comp0 = Company::GetIfValid(CompanyID{0});
		REQUIRE(comp0 != nullptr);

		/* Locate gate on Sol Earth Core */
		TileIndex gate_tile = INVALID_TILE;
		for (const auto &[pid, link] : PortalRegistry::GetAllPortals()) {
			if (link.end_a.world_id == WorldID{0}) {
				gate_tile = link.end_a.tile;
				break;
			}
		}
		REQUIRE(gate_tile != INVALID_TILE);

		/* Set Toll Required policy of 25,000 Cr */
		Money toll = 25000;
		charter_mgr.SetGatePolicy(gate_tile, GateAccessPolicy::TollRequired, toll);
		CHECK(charter_mgr.GetGatePolicy(gate_tile) == GateAccessPolicy::TollRequired);
		CHECK(charter_mgr.GetGateToll(gate_tile) == toll);

		Money cash_before = comp0->money;
		GateAccessResult access = charter_mgr.CheckAndProcessAccess(CompanyID{0}, gate_tile, "world_augusta");
		CHECK(access.allowed);
		CHECK(access.toll_charged == toll);
		CHECK(comp0->money == cash_before - toll);

		/* Grant diplomatic charter for private world */
		CHECK(CorporateCharterManager::IsPrivateWorld("world_cressat"));
		charter_mgr.SetGatePolicy(gate_tile, GateAccessPolicy::CharterRequired);

		/* Without charter -> Rejected */
		GateAccessResult no_charter = charter_mgr.CheckAndProcessAccess(CompanyID{0}, gate_tile, "world_cressat");
		CHECK_FALSE(no_charter.allowed);

		/* Grant charter -> Allowed */
		CHECK(charter_mgr.GrantCharter(CompanyID{0}, "world_cressat"));
		CHECK(charter_mgr.HasCharter(CompanyID{0}, "world_cressat"));
		GateAccessResult with_charter = charter_mgr.CheckAndProcessAccess(CompanyID{0}, gate_tile, "world_cressat");
		CHECK(with_charter.allowed);
	}

	std::filesystem::remove(test_save_path);
}
