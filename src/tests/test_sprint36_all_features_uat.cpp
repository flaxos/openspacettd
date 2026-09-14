/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint36_all_features_uat.cpp Unit and regression tests for Sprint 36 / All-Feature Guided Solo UAT (Sprints 1-40). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/spaceport_manager.h"
#include "../portal/edge_conduit.h"
#include "../portal/corporate_hq.h"
#include "../portal/company_stockpile.h"
#include "../portal/logistics_hub.h"
#include "../portal/fabrication_manager.h"
#include "../portal/world_gen.h"
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

class Sprint36UatFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();
};

TEST_CASE_METHOD(Sprint36UatFixture, "Sprint 36 UAT - Savegame File Artifact & Invariants", "[sprint36],[uat_save]")
{
	const std::string save_path_v1 = ResolveRepoPath("demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav");
	const std::string save_path_v04 = ResolveRepoPath("demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav");

	SECTION("Canonical all-features UAT savegame v1.0 exists and satisfies size invariants")
	{
		REQUIRE(std::filesystem::exists(save_path_v1));
		auto file_size = std::filesystem::file_size(save_path_v1);
		CHECK(file_size > 450000);
		CHECK(file_size < 1000000);

		/* Open and verify binary header contains valid save structure */
		std::ifstream file(save_path_v1, std::ios::binary);
		REQUIRE(file.is_open());

		std::vector<char> header(8);
		file.read(header.data(), 8);
		REQUIRE(file.gcount() == 8);

		/* Savegame files start with valid non-zero save stream signature */
		CHECK((header[0] != 0 || header[1] != 0));
	}

	SECTION("Existing backwards-compatible v0.4 savegame remains intact and untouched")
	{
		REQUIRE(std::filesystem::exists(save_path_v04));
		auto file_size = std::filesystem::file_size(save_path_v04);
		CHECK(file_size > 450000);
		CHECK(file_size < 1000000);
	}
}

TEST_CASE_METHOD(Sprint36UatFixture, "Sprint 36 UAT - GameScript OpenSpaceTTD-UAT-Demo v8 Metadata & Backwards Compatibility", "[sprint36],[gamescript]")
{
	const std::string info_path = ResolveRepoPath("bin/game/openspacettd_uat/info.nut");
	const std::string main_path = ResolveRepoPath("bin/game/openspacettd_uat/main.nut");

	SECTION("info.nut advertises version 8 with full backwards compatibility to version 1")
	{
		REQUIRE(std::filesystem::exists(info_path));
		std::ifstream f(info_path);
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		CHECK(content.find("OpenSpaceTTD-UAT-Demo") != std::string::npos);
		CHECK(content.find("OSUD") != std::string::npos);
		CHECK(content.find("GetVersion()     { return 8; }") != std::string::npos);
		CHECK(content.find("MinVersionToLoad() { return 1; }") != std::string::npos);
		CHECK(content.find("GetDate()        { return \"2026-09-14\"; }") != std::string::npos);
	}

	SECTION("main.nut implements all 12 Story Book chapters and 25 measurable acceptance goals")
	{
		REQUIRE(std::filesystem::exists(main_path));
		std::ifstream f(main_path);
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		// All 12 chapters present
		CHECK(content.find("1. Overview & Planetary Navigation") != std::string::npos);
		CHECK(content.find("2. Monumental Portal Gates") != std::string::npos);
		CHECK(content.find("3. Player Blueprints & CST Prefabs") != std::string::npos);
		CHECK(content.find("4. Planetary Operations: Spaceports & Conduits") != std::string::npos);
		CHECK(content.find("5. Megacity Demands & Freight Corridors") != std::string::npos);
		CHECK(content.find("6. Supply Chain Matrix & Federation Governance") != std::string::npos);
		CHECK(content.find("7. Commonwealth Data Crystals Rebranding") != std::string::npos);
		CHECK(content.find("8. Phase 4 Colonisation & Frontier Outposts") != std::string::npos);
		CHECK(content.find("9. Planetary Development Scoring & Phase Promotion") != std::string::npos);
		CHECK(content.find("10. Corporate Headquarters Campus") != std::string::npos);
		CHECK(content.find("11. Planetary Stockpiles & Logistics Hubs") != std::string::npos);
		CHECK(content.find("12. In-Kind Fabrication & BOM Construction") != std::string::npos);

		// Key goals present across all phases (1 through 25)
		CHECK(content.find("1. Navigate all worlds using Ctrl+Alt+1..6") != std::string::npos);
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
		CHECK(content.find("13. Game Settings search shows Distribution mode for data crystals") != std::string::npos);
		CHECK(content.find("14. Town station acceptance and waiting lists display Data Crystals") != std::string::npos);
		CHECK(content.find("15. Train depot purchase list contains the Data Van wagon") != std::string::npos);
		CHECK(content.find("16. Road depot purchase list contains the MPS Data Courier") != std::string::npos);
		CHECK(content.find("17. Inspect uncolonised Expansion Worlds (Worlds 4, 5, 6)") != std::string::npos);
		CHECK(content.find("18. Found a colonial outpost on an Expansion World to elevate it to Phase 3 Frontier status") != std::string::npos);
		CHECK(content.find("19. Deliver inter-world cargo across gateway pairs to accumulate planetary development score points.") != std::string::npos);
		CHECK(content.find("20. Promote a Frontier or Developed world to its next development tier") != std::string::npos);
		CHECK(content.find("21. Open Map menu > 'Corporate Headquarters & Stockpiles'") != std::string::npos);
		CHECK(content.find("22. Advance headquarters tier through Planetary HQ") != std::string::npos);
		CHECK(content.find("23. Open Corporate Headquarters > 'Planetary Stockpiles' tab") != std::string::npos);
		CHECK(content.find("24. Inspect the Merredin Planetary Logistics Hub on World 2") != std::string::npos);
		CHECK(content.find("25. Toggle In-Kind Fabrication mode in the Corporate HQ window") != std::string::npos);

		// Staging fixtures methods present
		CHECK(content.find("BuildSprint10Fixtures") != std::string::npos);
		CHECK(content.find("BuildSprint28Fixtures") != std::string::npos);
		CHECK(content.find("BuildSprint36Fixtures") != std::string::npos);
	}
}

TEST_CASE_METHOD(Sprint36UatFixture, "Sprint 36 UAT - Six-World Biome Partition & Layout Geometry", "[sprint36],[world_gen]")
{
	MultiWorldGen::Config cfg;
	cfg.world_count = 6;
	cfg.place_gateways = true;

	auto regions = MultiWorldGen::CalculateLayout(1024, 512, cfg);
	REQUIRE(regions.size() == 6);

	// Verify all 6 biomes in sequential progression
	CHECK(regions[0].biome == WorldBiome::Temperate);
	CHECK(regions[0].phase == WorldPhase::Phase1_Core);
	CHECK(regions[0].name.find("Phase 1") != std::string::npos);

	CHECK(regions[1].biome == WorldBiome::AridDesert);
	CHECK(regions[1].phase == WorldPhase::Phase2_Developed);
	CHECK(regions[1].name.find("Phase 2") != std::string::npos);

	CHECK(regions[2].biome == WorldBiome::SubArctic);
	CHECK(regions[2].phase == WorldPhase::Phase3_Frontier);
	CHECK(regions[2].name.find("Phase 3") != std::string::npos);

	CHECK(regions[3].biome == WorldBiome::Volcanic);
	CHECK(regions[3].phase == WorldPhase::Phase4_Expansion);
	CHECK(regions[3].name.find("World 4") != std::string::npos);

	CHECK(regions[4].biome == WorldBiome::SubTropic);
	CHECK(regions[4].phase == WorldPhase::Phase4_Expansion);
	CHECK(regions[4].name.find("World 5") != std::string::npos);

	CHECK(regions[5].biome == WorldBiome::Oceanic);
	CHECK(regions[5].phase == WorldPhase::Phase4_Expansion);
	CHECK(regions[5].name.find("World 6") != std::string::npos);

	// Non-overlapping X bounds separated by void buffer bands
	for (size_t i = 0; i < regions.size(); i++) {
		CHECK(regions[i].min_x < regions[i].max_x);
		CHECK(regions[i].min_y < regions[i].max_y);
		if (i > 0) {
			CHECK(regions[i].min_x > regions[i - 1].max_x); // Void buffer separation
		}
	}
}

TEST_CASE_METHOD(Sprint36UatFixture, "Sprint 36 UAT - Mid/Late Game Fixtures & Engine System Validation", "[sprint36],[fixtures]")
{
	CorporateHQManager::Reset();
	StockpileManager::Reset();
	LogisticsHubManager::Reset();
	FabricationManager::Reset();
	PlanetManager::Reset();
	MegacityManager::Reset();

	WorldID w0{0};
	WorldID w1{1};
	CompanyID c0{0};

	/* 1. Corporate HQ Registration and Tiers */
	TileIndex hq_tile = TileXY(83, 265);
	REQUIRE(CorporateHQManager::RegisterHQ(c0, w0, hq_tile, "Central HQ"));
	const auto *hq = CorporateHQManager::GetHQ(c0);
	REQUIRE(hq != nullptr);
	CHECK(hq->tier == CorporateHQTier::RegionalBranch);
	CHECK(hq->world_id == w0);
	CHECK(hq->tile == hq_tile);

	CorporateHQManager::UpgradeHQTier(c0);
	hq = CorporateHQManager::GetHQ(c0);
	REQUIRE(hq != nullptr);
	CHECK(hq->tier == CorporateHQTier::PlanetaryHQ);

	/* 2. Stockpile Seeding and Isolation across 6 fabrication roles */
	CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	CargoType steel   = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	CargoType wire    = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);
	CargoType chips   = StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics);
	CargoType alloy   = StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy);
	CargoType comp    = StockpileManager::RoleToDefaultCargo(FabricationRole::Composites);

	StockpileManager::AddCargo(w0, c0, ballast, 1000);
	StockpileManager::AddCargo(w0, c0, steel,   500);
	StockpileManager::AddCargo(w0, c0, wire,    300);
	StockpileManager::AddCargo(w0, c0, chips,   150);
	StockpileManager::AddCargo(w0, c0, alloy,   200);
	StockpileManager::AddCargo(w0, c0, comp,    100);

	CHECK(StockpileManager::GetStock(w0, c0, ballast) == 1000);
	CHECK(StockpileManager::GetStock(w0, c0, chips)   == 150);
	// In vanilla cargo mappings, StructuralMetal and Superalloy share steel cargo bucket, while Wiring and Composites share goods bucket
	CHECK(StockpileManager::GetStock(w0, c0, steel)   == (steel == alloy ? 700 : 500));
	CHECK(StockpileManager::GetStock(w0, c0, wire)    == (wire == comp ? 400 : 300));

	/* 3. Logistics Hub & Reserve Floor */
	TileIndex hub_tile = TileXY(240, 240);
	StationID st_id{5};
	uint32_t hub_id = LogisticsHubManager::RegisterHub(hub_tile, w1, c0, st_id, "Merredin Planetary Logistics Hub");
	REQUIRE(hub_id != 0);

	LogisticsHubManager::SetReserveFloor(hub_id, ballast, 200);
	LogisticsHubManager::SetReserveFloor(hub_id, steel, 100);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, ballast) == 200);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, steel) == 100);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, wire) == 0);

	/* 4. In-Kind Fabrication Toggle and Discount Rate */
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(c0));
	FabricationManager::SetFabricateFromStockpile(c0, true);
	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(c0));

	auto bom_rail = FabricationManager::GetTrackBOM(RAILTYPE_RAIL);
	CHECK(bom_rail.discount_percent == 80);

	/* 5. Planetary Development Scoring */
	PlanetRegion pr0{.id = w0, .name = "World 0", .phase = WorldPhase::Phase1_Core, .biome = WorldBiome::Temperate, .min_x = 0, .min_y = 0, .max_x = 100, .max_y = 100, .development_score = 0};
	PlanetRegion pr1{.id = w1, .name = "World 1", .phase = WorldPhase::Phase2_Developed, .biome = WorldBiome::AridDesert, .min_x = 110, .min_y = 0, .max_x = 210, .max_y = 100, .development_score = 0};
	PlanetManager::RegisterRegion(pr0);
	PlanetManager::RegisterRegion(pr1);

	PlanetManager::AddDevelopmentScore(w0, 25000);
	PlanetManager::AddDevelopmentScore(w1, 8000);
	CHECK(PlanetManager::GetRegion(w0)->development_score == 25000);
	CHECK(PlanetManager::GetRegion(w1)->development_score == 8000);
}
