/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint48_prompt_to_savegame.cpp Unit and integration tests for Sprint 48: Prompt-to-Savegame Generator. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/prompt_scenario_generator.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/corporate_hq.h"
#include "../portal/corporate_alliance.h"
#include "../portal/production_chain.h"
#include "../portal/logistics_hub.h"
#include "../portal/company_stockpile.h"
#include "../portal/tech_tree.h"
#include "../portal/fabrication_manager.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../station_base.h"
#include "../train.h"
#include "../map_func.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "mock_environment.h"

#include <filesystem>

class Sprint48Fixture {
public:
	Sprint48Fixture()
	{
		(void)MockEnvironment::Instance();
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
		_engine_mngr.ResetToDefaultMapping();
		SetupEngines();
		StartupEngines();
		if (Company::GetIfValid(CompanyID{0}) == nullptr) {
			Company::CreateAtIndex(CompanyID{0});
		}
		if (Company::GetIfValid(CompanyID{1}) == nullptr) {
			Company::CreateAtIndex(CompanyID{1});
		}
		ProductionChainManager::InitDefaultRecipes();
	}

	~Sprint48Fixture()
	{
		PortalRegistry::Reset();
		PlanetManager::Reset();
		MegacityManager::Reset();
		CorporateHQManager::Reset();
		ProductionChainManager::Reset();
	}
};

TEST_CASE_METHOD(Sprint48Fixture, "Sprint 48 - Prompt Parsing & Semantic Entity Extraction", "[sprint48],[prompt_parser]")
{
	SECTION("Canonical user prompt: Arid mining strike vs greedy core superalloy demand")
	{
		const std::string prompt = "Generate a 3-world system where an arid mining colony is striking over water shortages while a greedy core world demands superalloys";
		PromptScenarioSpec spec = PromptScenarioGenerator::ParsePrompt(prompt);

		CHECK(spec.world_count == 3);
		CHECK(spec.rival_relation == CorporateRelation::Hostile);
		REQUIRE(spec.worlds.size() == 3);

		/* World 0: Core */
		CHECK(spec.worlds[0].phase == WorldPhase::Phase1_Core);
		CHECK(spec.worlds[0].biome == WorldBiome::Temperate);
		CHECK(spec.worlds[0].has_megacity);
		CHECK(spec.worlds[0].has_corporate_hq);
		CHECK(spec.worlds[0].population >= 20000);
		REQUIRE_FALSE(spec.worlds[0].demanded_cargos.empty());
		CHECK(spec.worlds[0].demanded_cargos[0] == CommonwealthCargoID::Superalloys);

		/* World 1: Industrial */
		CHECK(spec.worlds[1].phase == WorldPhase::Phase2_Developed);
		CHECK(spec.worlds[1].has_industrial_facility);
		CHECK(spec.worlds[1].industrial_recipe == RECIPE_SUPERALLOY_FOUNDRY);

		/* World 2: Frontier Mining Colony */
		CHECK(spec.worlds[2].phase == WorldPhase::Phase3_Frontier);
		CHECK(spec.worlds[2].biome == WorldBiome::AridDesert);
		CHECK(spec.worlds[2].is_striking);
		CHECK(spec.worlds[2].strike_cause.find("Water") != std::string::npos);
	}

	SECTION("Alternative prompt: 4 worlds with glacial quantum research and allied alliance")
	{
		const std::string prompt = "4-world system where an allied glacial research colony exports quantum crystals to a high-density capital";
		PromptScenarioSpec spec = PromptScenarioGenerator::ParsePrompt(prompt);

		CHECK(spec.world_count == 4);
		CHECK(spec.rival_relation == CorporateRelation::Allied);
		REQUIRE(spec.worlds.size() == 4);

		/* World 3 (frontier index): SubArctic Glacial */
		CHECK(spec.worlds[3].phase == WorldPhase::Phase3_Frontier);
		CHECK(spec.worlds[3].biome == WorldBiome::SubArctic);
		CHECK_FALSE(spec.worlds[3].is_striking);

		/* Core world demands quantum crystals */
		REQUIRE_FALSE(spec.worlds[0].demanded_cargos.empty());
		CHECK(spec.worlds[0].demanded_cargos[0] == CommonwealthCargoID::EnrichedQuantumCrystals);
	}
}

TEST_CASE_METHOD(Sprint48Fixture, "Sprint 48 - End-to-End Scenario Synthesis & .sav Persistence", "[sprint48],[synthesis]")
{
	const std::string prompt = "Generate a 3-world system where an arid mining colony is striking over water shortages while a greedy core world demands superalloys";
	const std::string test_save_path = "test_sprint48_generated.sav";

	/* Clean up any preexisting test file */
	if (std::filesystem::exists(test_save_path)) {
		std::filesystem::remove(test_save_path);
	}

	SECTION("Synthesize and verify complete scenario invariants")
	{
		ScenarioSynthesisResult result = PromptScenarioGenerator::GenerateFromPrompt(prompt, test_save_path);
		REQUIRE(result.success);
		CHECK(result.worlds_created == 3);
		CHECK(result.corridors_built >= 2);
		CHECK(result.trains_spawned >= 1);
		CHECK(result.facilities_placed >= 1);

		/* Verify world configurations in PlanetManager */
		CHECK(PlanetManager::Count() == 3);
		const PlanetRegion *r0 = PlanetManager::GetRegion(WorldID{0});
		const PlanetRegion *r1 = PlanetManager::GetRegion(WorldID{1});
		const PlanetRegion *r2 = PlanetManager::GetRegion(WorldID{2});

		REQUIRE(r0 != nullptr);
		REQUIRE(r1 != nullptr);
		REQUIRE(r2 != nullptr);

		CHECK(r0->phase == WorldPhase::Phase1_Core);
		CHECK(r1->phase == WorldPhase::Phase2_Developed);
		CHECK(r2->phase == WorldPhase::Phase3_Frontier);
		CHECK(r2->biome == WorldBiome::AridDesert);

		/* Verify Megacity custom quotas */
		auto megacities = MegacityManager::GetAllMegacities();
		REQUIRE_FALSE(megacities.empty());
		CHECK(megacities[0].monthly_quota[1] >= 200); // Tier 2 expansion quota boosted for Superalloys

		/* Verify Corporate HQ placed */
		CHECK(CorporateHQManager::HasHQ(CompanyID{0}));
		REQUIRE(CorporateHQManager::GetHQ(CompanyID{0}) != nullptr);
		CHECK(CorporateHQManager::GetHQ(CompanyID{0})->world_id == WorldID{0});

		/* Verify Corporate Alliance stance */
		CHECK(CorporateAllianceManager::GetRelation(CompanyID{0}, CompanyID{1}) == CorporateRelation::Hostile);

		/* Verify active trains and orders */
		Train *train = nullptr;
		for (Train *t : Train::Iterate()) {
			if (t->IsFrontEngine()) {
				train = t;
				break;
			}
		}
		REQUIRE(train != nullptr);
		CHECK_FALSE(train->vehstatus.Test(VehState::Stopped));
		CHECK(train->GetNumOrders() >= 2);

		/* Verify .sav file persistence and valid file size */
		REQUIRE(std::filesystem::exists(test_save_path));
		auto file_size = std::filesystem::file_size(test_save_path);
		CHECK(file_size > 5000); // Working full compressed OpenSpaceTTD savegame

		/* Verify .sav file can be successfully loaded back */
		SaveLoadResult load_res = SaveOrLoad(test_save_path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
		CHECK(load_res == SaveLoadResult::Ok);
		SetupEngines();
		StartupEngines();

		/* Clean up generated test save */
		std::filesystem::remove(test_save_path);
	}
}
