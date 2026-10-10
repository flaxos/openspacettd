/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_electronics_loop.cpp Unit and integration tests for Player-Funded Electronics Loop (Pipeline B). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/tech_tree.h"
#include "../portal/corporate_hq.h"
#include "../portal/company_stockpile.h"
#include "../portal/production_chain.h"
#include "../portal/planet_manager.h"
#include "../portal/resource_sites.h"
#include "../portal/megacity_manager.h"
#include "../portal/portal_cmd.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Player-Funded Electronics Loop - TECH_MATERIALS_2 Unlock Lifecycle", "[electronics-loop]")
{
	(void)MockEnvironment::Instance();
	TechTreeManager::Reset();
	CorporateHQManager::Reset();

	CompanyID c0{0};
	std::string err;

	/* Pre-condition: cannot research without Corporate HQ */
	CHECK_FALSE(CorporateHQManager::HasHQ(c0));
	CHECK_FALSE(TechTreeManager::CanResearch(c0, TECH_MATERIALS_2, err));

	/* Register Corporate HQ */
	CorporateHQManager::RegisterHQ(c0, WorldID{1}, TileIndex{1000}, "CST Industrial HQ");
	CHECK(CorporateHQManager::HasHQ(c0));

	/* Cannot jump straight to Tier 2 without prerequisite TECH_MATERIALS_1 */
	CHECK_FALSE(TechTreeManager::CanResearch(c0, TECH_MATERIALS_2, err));
	CHECK(err.find("Prerequisite") != std::string::npos);

	/* Unlock Tier 1 (TECH_MATERIALS_1, 100 RP) */
	CHECK(TechTreeManager::CanResearch(c0, TECH_MATERIALS_1, err));
	TechTreeManager::SetActiveProject(c0, TECH_MATERIALS_1);
	TechTreeManager::AddResearchPoints(c0, 100);
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_1));
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_NONE);

	/* Now TECH_MATERIALS_2 is eligible (250 RP) */
	CHECK(TechTreeManager::CanResearch(c0, TECH_MATERIALS_2, err));
	TechTreeManager::SetActiveProject(c0, TECH_MATERIALS_2);
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_MATERIALS_2);

	/* Incremental funding / research progress */
	TechTreeManager::AddResearchPoints(c0, 150);
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_2));
	CHECK(TechTreeManager::GetAccumulatedRP(c0) == 150);

	/* Final funding pushes past 250 RP milestone */
	TechTreeManager::AddResearchPoints(c0, 100);
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_2));
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_NONE);

	/* Cannot re-research unlocked tech */
	CHECK_FALSE(TechTreeManager::CanResearch(c0, TECH_MATERIALS_2, err));
	CHECK(err.find("already researched") != std::string::npos);
}

TEST_CASE("Player-Funded Electronics Loop - Resource Site Discovery and Mapping", "[electronics-loop]")
{
	(void)MockEnvironment::Instance();
	ResourceSiteManager::Reset();
	PlanetManager::Reset();

	/* Verify canonical Commonwealth cargo mappings for copper and silica sand */
	CargoType c_copper = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::CopperOre);
	CargoType c_sand = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SilicaSand);
	CHECK(c_copper != INVALID_CARGO);
	CHECK(c_sand != INVALID_CARGO);

	/* Setup World 2 as Developed/Refining world where extractive operations occur */
	WorldID w2{2};
	PlanetRegion r_refining{
		.id = w2,
		.name = "Astraea Refining",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 100, .min_y = 100, .max_x = 200, .max_y = 200,
	};
	PlanetManager::RegisterRegion(r_refining);

	/* Verify site manager state */
	ResourceSiteManager::SetEnabled(true);
	CHECK(ResourceSiteManager::Enabled());
}

TEST_CASE("Player-Funded Electronics Loop - Pipeline B Batch Conversion Conservation", "[electronics-loop]")
{
	(void)MockEnvironment::Instance();
	PlanetManager::Reset();
	ProductionChainManager::Reset();
	TechTreeManager::Reset();

	WorldID w_refining{2};
	PlanetRegion r_refining{
		.id = w_refining,
		.name = "Astraea Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_refining);

	CompanyID c0{0};

	/* Check recipe catalog registration */
	const auto *r_copper = ProductionChainManager::GetRecipe(RECIPE_COPPER_SMELTING);
	REQUIRE(r_copper != nullptr);
	CHECK(r_copper->pipeline == PipelineType::Electronics);

	const auto *r_silicon = ProductionChainManager::GetRecipe(RECIPE_SILICON_ARC);
	REQUIRE(r_silicon != nullptr);
	CHECK(r_silicon->pipeline == PipelineType::Electronics);

	const auto *r_signal = ProductionChainManager::GetRecipe(RECIPE_SIGNALLING_ASSEMBLY);
	REQUIRE(r_signal != nullptr);
	CHECK(r_signal->pipeline == PipelineType::Electronics);

	CargoType c_copper = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::CopperOre);
	CargoType c_wire   = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::ConductiveWiring);
	CargoType c_sand   = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SilicaSand);
	CargoType c_chips  = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SiliconChips);

	/* Register Facility 1: Copper Smelter (2 Copper -> 2 Wire, capacity 50 batches) */
	FacilityID fid_copper = ProductionChainManager::RegisterFacility(TileIndex{500}, w_refining, RECIPE_COPPER_SMELTING, c0, 50);
	REQUIRE(fid_copper != INVALID_FACILITY);

	/* Register Facility 2: Silicon Arc (2 Sand -> 1 Chips, capacity 50 batches) */
	FacilityID fid_silicon = ProductionChainManager::RegisterFacility(TileIndex{510}, w_refining, RECIPE_SILICON_ARC, c0, 50);
	REQUIRE(fid_silicon != INVALID_FACILITY);

	/* Deliver 60 units Copper Ore to smelter */
	ProductionChainManager::DeliverCargo(fid_copper, c_copper, 60);
	ProcessingFacility *fac_cu = ProductionChainManager::GetFacility(fid_copper);
	REQUIRE(fac_cu != nullptr);
	CHECK(fac_cu->input_buffers[c_copper] == 60);

	/* Deliver 60 units Silica Sand to silicon arc */
	ProductionChainManager::DeliverCargo(fid_silicon, c_sand, 60);
	ProcessingFacility *fac_si = ProductionChainManager::GetFacility(fid_silicon);
	REQUIRE(fac_si != nullptr);
	CHECK(fac_si->input_buffers[c_sand] == 60);

	/* Execute monthly simulation tick */
	ProductionChainManager::ProcessMonthlyProduction();

	/* Verify exact batch conversion and conservation:
	 * 60 Copper / 2 = 30 batches -> produces 30 * 2 = 60 Conductive Wire.
	 * 60 Sand / 2 = 30 batches -> produces 30 * 1 = 30 Silicon Chips.
	 */
	CHECK(fac_cu->input_buffers[c_copper] == 0);
	CHECK(fac_cu->output_buffers[c_wire] == 60);
	CHECK(fac_cu->last_month_production == 30);
	CHECK(fac_cu->total_produced == 30);

	CHECK(fac_si->input_buffers[c_sand] == 0);
	CHECK(fac_si->output_buffers[c_chips] == 30);
	CHECK(fac_si->last_month_production == 30);
	CHECK(fac_si->total_produced == 30);

	/* Withdraw 40 units Wire and 20 units Chips */
	uint32_t withdrawn_wire = ProductionChainManager::WithdrawOutput(fid_copper, c_wire, 40);
	uint32_t withdrawn_chips = ProductionChainManager::WithdrawOutput(fid_silicon, c_chips, 20);
	CHECK(withdrawn_wire == 40);
	CHECK(withdrawn_chips == 20);
	CHECK(fac_cu->output_buffers[c_wire] == 20);
	CHECK(fac_si->output_buffers[c_chips] == 10);

	/* Register Facility 3: Signalling Works (1 Chips + 1 Wire -> 2 Logic, capacity 50) */
	FacilityID fid_signal = ProductionChainManager::RegisterFacility(TileIndex{520}, w_refining, RECIPE_SIGNALLING_ASSEMBLY, c0, 50);
	REQUIRE(fid_signal != INVALID_FACILITY);
	ProcessingFacility *fac_sig = ProductionChainManager::GetFacility(fid_signal);
	REQUIRE(fac_sig != nullptr);

	/* Deliver 20 Wire and 20 Chips to Signalling Works */
	ProductionChainManager::DeliverCargo(fid_signal, c_wire, 20);
	ProductionChainManager::DeliverCargo(fid_signal, c_chips, 20);
	CHECK(fac_sig->input_buffers[c_wire] == 20);
	CHECK(fac_sig->input_buffers[c_chips] == 20);

	/* Process monthly production: min(20/1, 20/1) = 20 batches -> 20 * 2 = 40 output */
	ProductionChainManager::ProcessMonthlyProduction();
	CHECK(fac_sig->input_buffers[c_wire] == 0);
	CHECK(fac_sig->input_buffers[c_chips] == 0);
	CHECK(fac_sig->output_buffers[c_chips] == 40);
	CHECK(fac_sig->last_month_production == 20);
}

TEST_CASE("Player-Funded Electronics Loop - Core Megacity Tier 2 and Tier 3 Acceptance", "[electronics-loop]")
{
	(void)MockEnvironment::Instance();
	SetupCargoForClimate(LandscapeType::Temperate);
	MegacityManager::Reset();

	TownID t1{1};
	WorldID w1{1};
	REQUIRE(MegacityManager::RegisterMegacity(t1, w1, "Astraea Prime", 3000));

	const MegacityProfile *profile = MegacityManager::GetProfile(t1);
	REQUIRE(profile != nullptr);

	/* Set quotas: Tier 1 = 60, Tier 2 = 40, Tier 3 = 20 */
	MegacityManager::SetCustomQuotas(t1, 60, 40, 20);
	CHECK(profile->monthly_quota[0] == 60);
	CHECK(profile->monthly_quota[1] == 40);
	CHECK(profile->monthly_quota[2] == 20);

	/* Verify classification of Pipeline B outputs */
	CargoType c_wire = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::ConductiveWiring);
	CargoType c_chips = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SiliconChips);

	MegacityDemandTier tier_wire = MegacityManager::ClassifyCargo(to_underlying(c_wire));
	MegacityDemandTier tier_chips = MegacityManager::ClassifyCargo(to_underlying(c_chips));

	/* WIRE satisfies Tier 2 Expansion */
	CHECK(tier_wire == MegacityDemandTier::Tier2_Expansion);

	/* CHIP satisfies Tier 3 Prosperity */
	CHECK(tier_chips == MegacityDemandTier::Tier3_Prosperity);

	/* Record deliveries of Pipeline B goods */
	MegacityManager::RecordDeliveryByCargo(t1, to_underlying(c_wire), 40);
	MegacityManager::RecordDeliveryByCargo(t1, to_underlying(c_chips), 20);

	CHECK(profile->delivered_current[1] == 40);
	CHECK(profile->delivered_current[2] == 20);

	/* Deliver Tier 1 sustenance to achieve full prosperity conditions */
	MegacityManager::RecordDelivery(t1, MegacityDemandTier::Tier1_Sustenance, 60);
	CHECK(profile->delivered_current[0] == 60);

	/* Evaluate monthly supply: full Tier 1 + Tier 2 + Tier 3 triggers HyperGrowth */
	MegacityManager::EvaluateMonthlySupply();
	CHECK(profile->growth_state == MegacityGrowthState::HyperGrowth);
	CHECK(profile->growth_multiplier == 2.0f);
	CHECK(profile->passenger_multiplier == 1.5f);
	CHECK(profile->overall_supply_index == 1.0f);
}

TEST_CASE("Player-Funded Electronics Loop - Full End-to-End Progression Lifecycle", "[electronics-loop][e2e]")
{
	(void)MockEnvironment::Instance();
	SetupCargoForClimate(LandscapeType::Temperate);
	TechTreeManager::Reset();
	CorporateHQManager::Reset();
	PlanetManager::Reset();
	ResourceSiteManager::Reset();
	ProductionChainManager::Reset();
	MegacityManager::Reset();

	CompanyID c0{0};
	WorldID w_core{0}, w_developed{1};
	TownID t_core{0};

	/* 1. Register Core & Developed Worlds */
	PlanetManager::RegisterRegion({
		.id = w_core,
		.name = "Astraea Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 100, .max_y = 100,
	});
	PlanetManager::RegisterRegion({
		.id = w_developed,
		.name = "Merredin Mining & Refining",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 101, .min_y = 0, .max_x = 200, .max_y = 100,
	});

	/* 2. Core Megacity Registration */
	REQUIRE(MegacityManager::RegisterMegacity(t_core, w_core, "Mito Megacity", 2000));
	MegacityManager::SetCustomQuotas(t_core, 60, 40, 20);
	const auto *megacity = MegacityManager::GetProfile(t_core);
	REQUIRE(megacity != nullptr);

	/* 3. Corporate HQ and Materials I Prerequisite */
	CorporateHQManager::RegisterHQ(c0, w_core, TileIndex{100}, "Core Supply HQ");
	TechTreeManager::SetActiveProject(c0, TECH_MATERIALS_1);
	TechTreeManager::AddResearchPoints(c0, 100);
	REQUIRE(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_1));

	/* 4. Player Funds TECH_MATERIALS_2 (250 RP) */
	std::string err;
	REQUIRE(TechTreeManager::CanResearch(c0, TECH_MATERIALS_2, err));
	TechTreeManager::SetActiveProject(c0, TECH_MATERIALS_2);
	TechTreeManager::AddResearchPoints(c0, 250);
	REQUIRE(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_2));
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_NONE);

	/* 5. Resource Site Surveying Enabled */
	ResourceSiteManager::SetEnabled(true);
	CHECK(ResourceSiteManager::Enabled());

	/* 6. Cargo definitions */
	CargoType c_copper = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::CopperOre);
	CargoType c_sand   = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SilicaSand);
	CargoType c_wire   = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::ConductiveWiring);
	CargoType c_chips  = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SiliconChips);
	REQUIRE(c_copper != INVALID_CARGO);
	REQUIRE(c_sand != INVALID_CARGO);
	REQUIRE(c_wire != INVALID_CARGO);
	REQUIRE(c_chips != INVALID_CARGO);

	/* 7. Pipeline B Facilities in Developed World */
	FacilityID fid_cu = ProductionChainManager::RegisterFacility(TileIndex{200}, w_developed, RECIPE_COPPER_SMELTING, c0, 100);
	FacilityID fid_si = ProductionChainManager::RegisterFacility(TileIndex{210}, w_developed, RECIPE_SILICON_ARC, c0, 100);
	REQUIRE(fid_cu != INVALID_FACILITY);
	REQUIRE(fid_si != INVALID_FACILITY);

	/* 8. Deliver inputs: 80 Copper Ore, 40 Silica Sand */
	ProductionChainManager::DeliverCargo(fid_cu, c_copper, 80);
	ProductionChainManager::DeliverCargo(fid_si, c_sand, 40);

	/* 9. Monthly batch conversion:
	 * 80 Copper / 2 = 40 batches -> 80 Wire
	 * 40 Sand / 2 = 20 batches -> 20 Chips
	 */
	ProductionChainManager::ProcessMonthlyProduction();
	uint32_t produced_wire = ProductionChainManager::WithdrawOutput(fid_cu, c_wire, 80);
	uint32_t produced_chips = ProductionChainManager::WithdrawOutput(fid_si, c_chips, 20);
	CHECK(produced_wire == 80);
	CHECK(produced_chips == 20);

	/* 10. Core Megacity Delivery & Multi-Tier Satisfaction */
	// Deliver Sustenance (Tier 1)
	MegacityManager::RecordDelivery(t_core, MegacityDemandTier::Tier1_Sustenance, 60);
	// Deliver 40 Wire (Tier 2 Expansion)
	MegacityManager::RecordDeliveryByCargo(t_core, to_underlying(c_wire), 40);
	// Deliver 20 Chips (Tier 3 Prosperity)
	MegacityManager::RecordDeliveryByCargo(t_core, to_underlying(c_chips), 20);

	/* 11. Monthly Evaluation -> HyperGrowth */
	MegacityManager::EvaluateMonthlySupply();
	CHECK(megacity->growth_state == MegacityGrowthState::HyperGrowth);
	CHECK(megacity->growth_multiplier == 2.0f);
	CHECK(megacity->passenger_multiplier == 1.5f);
	CHECK(megacity->overall_supply_index == 1.0f);
}
