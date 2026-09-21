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
#include "../fileio_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "mock_environment.h"

#include <filesystem>

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

	std::filesystem::remove(test_save_path);
}
