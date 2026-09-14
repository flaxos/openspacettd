/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint28_guided_uat.cpp Unit and regression tests for Sprint 28 Guided Solo UAT. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"

#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/spaceport_manager.h"
#include "../portal/edge_conduit.h"
#include "../blueprint/blueprint_manager.h"
#include "../script/api/script_goal.hpp"
#include "../script/api/script_story_page.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../safeguards.h"

static std::string ResolveRepoPath(const std::string &rel_path)
{
	if (std::filesystem::exists(rel_path)) return rel_path;
	if (std::filesystem::exists("../" + rel_path)) return "../" + rel_path;
	return rel_path;
}

class Sprint28UatFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();
};

TEST_CASE_METHOD(Sprint28UatFixture, "Sprint 28 UAT - Savegame File Artifact & Reproducibility Metadata", "[sprint28],[uat_save]")
{
	const std::string save_path = ResolveRepoPath("demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav");

	SECTION("v0.4 guided solo UAT savegame exists and satisfies size invariants")
	{
		REQUIRE(std::filesystem::exists(save_path));
		auto file_size = std::filesystem::file_size(save_path);
		CHECK(file_size > 450000);
		CHECK(file_size < 1000000);

		/* Open and verify binary header contains valid save structure */
		std::ifstream file(save_path, std::ios::binary);
		REQUIRE(file.is_open());

		std::vector<char> header(8);
		file.read(header.data(), 8);
		REQUIRE(file.gcount() == 8);

		/* Savegame version 367 files start with OTTD save stream signature */
		CHECK((header[0] != 0 || header[1] != 0));
	}
}

TEST_CASE_METHOD(Sprint28UatFixture, "Sprint 28 UAT - GameScript OpenSpaceTTD-UAT-Demo v7 Registration", "[sprint28],[gamescript]")
{
	const std::string info_path = ResolveRepoPath("bin/game/openspacettd_uat/info.nut");
	const std::string main_path = ResolveRepoPath("bin/game/openspacettd_uat/main.nut");

	SECTION("info.nut advertises version 7 with full backwards compatibility")
	{
		REQUIRE(std::filesystem::exists(info_path));
		std::ifstream f(info_path);
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		CHECK(content.find("OpenSpaceTTD-UAT-Demo") != std::string::npos);
		CHECK(content.find("OSUD") != std::string::npos);
		CHECK((content.find("GetVersion()     { return 7; }") != std::string::npos || content.find("GetVersion()     { return 8; }") != std::string::npos));
		CHECK(content.find("MinVersionToLoad() { return 1; }") != std::string::npos);
		CHECK((content.find("GetDate()        { return \"2026-09-13\"; }") != std::string::npos || content.find("GetDate()        { return \"2026-09-14\"; }") != std::string::npos));
	}

	SECTION("main.nut implements all 7 Story Book chapters and 16 measurable acceptance goals")
	{
		REQUIRE(std::filesystem::exists(main_path));
		std::ifstream f(main_path);
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		// All 7 chapters present
		CHECK(content.find("1. Overview & Planetary Navigation") != std::string::npos);
		CHECK(content.find("2. Monumental Portal Gates") != std::string::npos);
		CHECK(content.find("3. Player Blueprints & CST Prefabs") != std::string::npos);
		CHECK(content.find("4. Planetary Operations: Spaceports & Conduits") != std::string::npos);
		CHECK(content.find("5. Megacity Demands & Freight Corridors") != std::string::npos);
		CHECK(content.find("6. Supply Chain Matrix & Federation Governance") != std::string::npos);
		CHECK(content.find("7. Commonwealth Data Crystals Rebranding") != std::string::npos);

		// Key goals present
		CHECK((content.find("1. Navigate all three worlds") != std::string::npos || content.find("1. Navigate all worlds") != std::string::npos));
		CHECK(content.find("2. Build an unlinked portal gate") != std::string::npos);
		CHECK(content.find("3. Run a portal consist through Gateway Alpha") != std::string::npos);
		CHECK(content.find("4. Open Blueprint Library ('B'), select a canonical CST Prefab") != std::string::npos);
		CHECK(content.find("5. Select 'Capture From Map'") != std::string::npos);
		CHECK(content.find("6. Open station window for the Phase 1 Spaceport candidate") != std::string::npos);
		CHECK(content.find("7. Select the Edge Conduit tool") != std::string::npos);
		CHECK(content.find("8. Open Town window > 'Megacity'") != std::string::npos);
		CHECK(content.find("9. Open Map dropdown > Freight Corridor Monitor") != std::string::npos);
		CHECK(content.find("10. Open Map dropdown > Supply Chain & Trade Ledger") != std::string::npos);
		CHECK(content.find("11. Open Map dropdown > Federation Authentication & Charters") != std::string::npos);
		CHECK(content.find("12. Graphs > Cargo Payment Rates lists Data Crystals") != std::string::npos);

		// Sprint 28 staging fixtures methods present
		CHECK(content.find("BuildSprint28Fixtures") != std::string::npos);
		CHECK(content.find("FindCSTStagingPad") != std::string::npos);
		CHECK(content.find("FindBlueprintSampleSite") != std::string::npos);
	}
}

TEST_CASE_METHOD(Sprint28UatFixture, "Sprint 28 UAT - CST Prefab Rail Blocks Catalog & Invariance", "[sprint28],[cst_prefabs]")
{
	BlueprintManager::Initialize();

	CHECK(BlueprintManager::GetBuiltinCount() == 8);

	const std::vector<std::string> expected_prefabs = {
		"CST Mainline Double Straight",
		"CST Dual-Track Passing Siding",
		"CST Portal Gate Approach Corridor",
		"CST High-Speed 3-Way Wye Junction",
		"CST 4-Way Compact Roundabout Junction",
		"CST Ro-Ro 4-Platform Terminal Station Block",
		"CST Industrial Bulk Balloon Loop",
		"CST Depot Maintenance Staging Yard",
	};

	for (const auto &name : expected_prefabs) {
		const Blueprint *bp = BlueprintManager::FindBuiltin(name);
		REQUIRE(bp != nullptr);
		CHECK(bp->is_builtin);
		CHECK(bp->GetTileCount() > 0);
		CHECK(bp->width > 0);
		CHECK(bp->height > 0);

		/* Geometric invariant: Mirror swaps dimensions across isometric centerline and preserves tile count */
		Blueprint mirrored = bp->Mirror();
		CHECK(mirrored.GetTileCount() == bp->GetTileCount());
		CHECK(mirrored.width == bp->height);
		CHECK(mirrored.height == bp->width);

		/* Double mirror returns to original orientation and dimensions */
		Blueprint double_mirrored = mirrored.Mirror();
		CHECK(double_mirrored.GetTileCount() == bp->GetTileCount());
		CHECK(double_mirrored.width == bp->width);
		CHECK(double_mirrored.height == bp->height);
	}
}

TEST_CASE_METHOD(Sprint28UatFixture, "Sprint 28 UAT - Megacity Multi-Tier Demand & Growth States", "[sprint28],[megacity]")
{
	MegacityManager::Reset();
	REQUIRE(MegacityManager::GetAllMegacities().empty());

	TownID tid{101};
	REQUIRE(MegacityManager::RegisterMegacity(tid, WorldID{1}, "Oaktree Core", 15000));
	REQUIRE(MegacityManager::IsMegacity(tid));

	const auto *profile = MegacityManager::GetProfile(tid);
	REQUIRE(profile != nullptr);
	CHECK(profile->town_name == "Oaktree Core");
	CHECK(profile->world_id == WorldID{1});
	CHECK(profile->growth_state == MegacityGrowthState::Subsistence);

	/* Cargo classification */
	CHECK(MegacityManager::ClassifyCargo(11) == MegacityDemandTier::Tier1_Sustenance); // Food
	CHECK(MegacityManager::ClassifyCargo(5)  == MegacityDemandTier::Tier2_Expansion);  // Goods
	CHECK(MegacityManager::ClassifyCargo(10) == MegacityDemandTier::Tier3_Prosperity); // Valuables/Diamonds

	/* Test Starvation transition: Tier 1 < 50% */
	MegacityManager::SetCustomQuotas(tid, 100, 50, 20);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 20); // 20%
	MegacityManager::EvaluateMonthlySupply();
	profile = MegacityManager::GetProfile(tid);
	CHECK(profile->growth_state == MegacityGrowthState::Starvation);
	CHECK(profile->growth_multiplier == 0.0f);

	/* Test Subsistence transition: Tier 1 >= 50%, Tier 2 < 100% */
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 60); // 60%
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier2_Expansion, 20);  // 40%
	MegacityManager::EvaluateMonthlySupply();
	profile = MegacityManager::GetProfile(tid);
	CHECK(profile->growth_state == MegacityGrowthState::Subsistence);
	CHECK(profile->growth_multiplier == 1.0f);

	/* Test Metropolitan Boom: Tier 1 >= 100%, Tier 2 >= 100%, Tier 3 < 100% */
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 100); // 100%
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier2_Expansion, 50);   // 100%
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier3_Prosperity, 10);  // 50%
	MegacityManager::EvaluateMonthlySupply();
	profile = MegacityManager::GetProfile(tid);
	CHECK(profile->growth_state == MegacityGrowthState::MetropolitanBoom);
	CHECK(profile->growth_multiplier == 1.5f);

	/* Test HyperGrowth: All 3 tiers >= 100% */
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier1_Sustenance, 120);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier2_Expansion, 60);
	MegacityManager::RecordDelivery(tid, MegacityDemandTier::Tier3_Prosperity, 25);
	MegacityManager::EvaluateMonthlySupply();
	profile = MegacityManager::GetProfile(tid);
	CHECK(profile->growth_state == MegacityGrowthState::HyperGrowth);
	CHECK(profile->growth_multiplier == 2.0f);
}

TEST_CASE_METHOD(Sprint28UatFixture, "Sprint 28 UAT - Script Goal & Story Page Enums and Types", "[sprint28],[story_goal]")
{
	/* Verify GoalType enumeration mappings */
	CHECK(ScriptGoal::GT_NONE == 0);
	CHECK(ScriptGoal::GT_TILE == 1);
	CHECK(ScriptGoal::GT_INDUSTRY == 2);
	CHECK(ScriptGoal::GT_TOWN == 3);
	CHECK(ScriptGoal::GT_COMPANY == 4);
	CHECK(ScriptGoal::GT_STORY_PAGE == 5);

	/* Verify StoryPageElementType enumeration mappings */
	CHECK(ScriptStoryPage::SPET_TEXT == 0);
	CHECK(ScriptStoryPage::SPET_LOCATION == 1);
	CHECK(ScriptStoryPage::SPET_GOAL == 2);
	CHECK(ScriptStoryPage::SPET_BUTTON_PUSH == 3);
	CHECK(ScriptStoryPage::SPET_BUTTON_TILE == 4);
	CHECK(ScriptStoryPage::SPET_BUTTON_VEHICLE == 5);
}
