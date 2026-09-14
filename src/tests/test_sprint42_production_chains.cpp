/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint42_production_chains.cpp Unit tests for Sprint 42 Factorio-Scale Multi-World Production Chains. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/production_chain.h"
#include "../portal/planet_manager.h"
#include "../portal/corporate_hq.h"
#include "../portal/company_stockpile.h"
#include "../portal/logistics_hub.h"
#include "../portal/tech_tree.h"
#include "../company_base.h"
#include "../company_func.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 42 Production Chains - Canonical Cargoes and Recipe Catalog")
{
	ProductionChainManager::Reset();

	/* Verify 13 canonical Commonwealth cargo mappings */
	CHECK(static_cast<uint8_t>(CommonwealthCargoID::Count) == 13);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StoneSlag) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::CopperOre) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::ConductiveWiring) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SilicaSand) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SiliconChips) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::RareEarthMinerals) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::Superalloys) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SyntheticComposites) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::BlankCrystals) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::EnrichedQuantumCrystals) != INVALID_CARGO);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::EncryptedConsumerCrystals) != INVALID_CARGO);

	/* Verify registered recipes */
	auto recipes = ProductionChainManager::GetAllRecipes();
	CHECK(recipes.size() >= 10);

	auto structural = ProductionChainManager::GetRecipesByPipeline(PipelineType::Structural);
	CHECK(structural.size() == 3);

	auto electronics = ProductionChainManager::GetRecipesByPipeline(PipelineType::Electronics);
	CHECK(electronics.size() == 3);

	auto propulsion = ProductionChainManager::GetRecipesByPipeline(PipelineType::Propulsion);
	CHECK(propulsion.size() == 2);

	auto datacrystals = ProductionChainManager::GetRecipesByPipeline(PipelineType::DataCrystals);
	CHECK(datacrystals.size() == 3);
}

TEST_CASE("Sprint 42 Production Chains - World Phase Placement Constraints")
{
	(void)MockEnvironment::Instance();
	PlanetManager::Reset();
	ProductionChainManager::Reset();

	WorldID w_core{1};
	PlanetRegion r_core{
		.id = w_core,
		.name = "Sol Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_core);

	WorldID w_refining{2};
	PlanetRegion r_refining{
		.id = w_refining,
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_refining);

	WorldID w_frontier{3};
	PlanetRegion r_frontier{
		.id = w_frontier,
		.name = "Sheldon Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_frontier);

	std::string err;

	/* Blast Furnace Structural Steel: Allowed on Phase 2 Refining only */
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_core, RECIPE_STEEL_SMELTING, err));
	CHECK(ProductionChainManager::CanConstructFacility(w_refining, RECIPE_STEEL_SMELTING, err));
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_frontier, RECIPE_STEEL_SMELTING, err));

	/* Quantum Telemetry Array: Allowed on Phase 3 Frontier / Phase 4 Expansion only */
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_core, RECIPE_QUANTUM_ENRICHMENT, err));
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_refining, RECIPE_QUANTUM_ENRICHMENT, err));
	CHECK(ProductionChainManager::CanConstructFacility(w_frontier, RECIPE_QUANTUM_ENRICHMENT, err));

	/* Metropolitan Consumer Crystal Formatting: Allowed on Phase 1 Core only */
	CHECK(ProductionChainManager::CanConstructFacility(w_core, RECIPE_CONSUMER_CRYSTAL_FORMAT, err));
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_refining, RECIPE_CONSUMER_CRYSTAL_FORMAT, err));
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_frontier, RECIPE_CONSUMER_CRYSTAL_FORMAT, err));

	/* Ballast Crushing: Allowed on both Phase 2 Refining and Phase 3 Frontier */
	CHECK_FALSE(ProductionChainManager::CanConstructFacility(w_core, RECIPE_BALLAST_CRUSHING, err));
	CHECK(ProductionChainManager::CanConstructFacility(w_refining, RECIPE_BALLAST_CRUSHING, err));
	CHECK(ProductionChainManager::CanConstructFacility(w_frontier, RECIPE_BALLAST_CRUSHING, err));
}

TEST_CASE("Sprint 42 Production Chains - Facility Lifecycle & Input-to-Output Conversion")
{
	(void)MockEnvironment::Instance();
	PlanetManager::Reset();
	ProductionChainManager::Reset();

	WorldID w_refining{2};
	PlanetRegion r_refining{
		.id = w_refining,
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_refining);

	CompanyID c0{0};
	TileIndex fac_tile{320};

	/* Register Blast Furnace on World 2 (monthly capacity = 50 batches) */
	FacilityID fid = ProductionChainManager::RegisterFacility(fac_tile, w_refining, RECIPE_STEEL_SMELTING, c0, 50);
	REQUIRE(fid != INVALID_FACILITY);

	ProcessingFacility *fac = ProductionChainManager::GetFacility(fid);
	REQUIRE(fac != nullptr);
	CHECK(fac->monthly_capacity == 50);

	CargoType c_iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	CargoType c_steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);

	/* Deliver 60 units of Iron Ore (recipe requires 2 Iron Ore per batch) */
	ProductionChainManager::DeliverCargo(fid, c_iron, 60);
	CHECK(fac->input_buffers[c_iron] == 60);

	/* Process monthly production: 60 iron / 2 = 30 batches -> produces 30 steel */
	ProductionChainManager::ProcessMonthlyProduction();

	CHECK(fac->input_buffers[c_iron] == 0);
	CHECK(fac->output_buffers[c_steel] == 30);
	CHECK(fac->last_month_production == 30);
	CHECK(fac->total_produced == 30);

	/* Withdraw 20 units of Steel */
	uint32_t withdrawn = ProductionChainManager::WithdrawOutput(fid, c_steel, 20);
	CHECK(withdrawn == 20);
	CHECK(fac->output_buffers[c_steel] == 10);
}

TEST_CASE("Sprint 42 Production Chains - Multi-Input Superalloy Foundry & Nanofab Perk")
{
	(void)MockEnvironment::Instance();
	PlanetManager::Reset();
	ProductionChainManager::Reset();
	TechTreeManager::Reset();

	WorldID w_refining{2};
	PlanetRegion r_refining{
		.id = w_refining,
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_refining);

	CompanyID c0{0};
	TileIndex fac_tile{330};

	/* Superalloy Foundry: 2 Steel + 1 Rare Earths -> 2 Superalloys */
	FacilityID fid = ProductionChainManager::RegisterFacility(fac_tile, w_refining, RECIPE_SUPERALLOY_FOUNDRY, c0, 100);
	REQUIRE(fid != INVALID_FACILITY);

	ProcessingFacility *fac = ProductionChainManager::GetFacility(fid);
	REQUIRE(fac != nullptr);

	CargoType c_steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	CargoType c_rare  = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::RareEarthMinerals);
	CargoType c_alloy = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::Superalloys);

	/* Unlock TECH_MATERIALS_3 (Automated Nanofabrication Lines: +15% production yield bonus) */
	TechTreeManager::RestoreCompanyTech(c0, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3});
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_3));

	/* Deliver 40 Steel and 20 Rare Earth Minerals (20 batches possible) */
	ProductionChainManager::DeliverCargo(fid, c_steel, 40);
	ProductionChainManager::DeliverCargo(fid, c_rare, 20);

	ProductionChainManager::ProcessMonthlyProduction();

	CHECK(fac->input_buffers[c_steel] == 0);
	CHECK(fac->input_buffers[c_rare] == 0);
	CHECK(fac->last_month_production == 20);

	/* Base output would be 20 * 2 = 40; with +15% perk: (20 * 2 * 115) / 100 = 46 */
	CHECK(fac->output_buffers[c_alloy] == 46);
}

TEST_CASE("Sprint 42 Production Chains - Quantum Observatory Telemetry & R&D Loop")
{
	(void)MockEnvironment::Instance();
	_company_pool.CleanPool();
	Company::CreateAtIndex(CompanyID{0});
	CompanyID c0{0};
	_current_company = c0;
	Company *comp = Company::Get(c0);
	REQUIRE(comp != nullptr);
	comp->money = 10000000;

	PlanetManager::Reset();
	CorporateHQManager::Reset();
	StockpileManager::Reset();
	TechTreeManager::Reset();
	ProductionChainManager::Reset();

	WorldID w_core{1};
	PlanetRegion r_core{
		.id = w_core,
		.name = "Sol Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_core);

	WorldID w_frontier{3};
	PlanetRegion r_frontier{
		.id = w_frontier,
		.name = "Sheldon Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_frontier);

	/* Establish Corporate HQ on World 1 (Core) */
	CorporateHQManager::RegisterHQ(c0, w_core, TileIndex{150}, "CST Global HQ");
	CHECK(CorporateHQManager::HasHQ(c0));

	/* Register Deep-Space Quantum Telemetry Array on World 3 (Frontier) */
	TileIndex obs_tile{520};
	FacilityID fid = ProductionChainManager::RegisterFacility(obs_tile, w_frontier, RECIPE_QUANTUM_ENRICHMENT, c0, 50);
	REQUIRE(fid != INVALID_FACILITY);

	CargoType c_blank = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::BlankCrystals);
	CargoType c_enrich = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::EnrichedQuantumCrystals);

	/* Deliver 10 Blank Data Crystals to Observatory */
	ProductionChainManager::DeliverCargo(fid, c_blank, 10);

	/* Process monthly conversion: 10 blank -> 10 enriched crystals */
	ProductionChainManager::ProcessMonthlyProduction();
	ProcessingFacility *fac = ProductionChainManager::GetFacility(fid);
	REQUIRE(fac != nullptr);
	CHECK(fac->output_buffers[c_enrich] == 10);

	/* Withdraw 10 Enriched Crystals and transport to Corporate HQ stockpile on World 1 */
	uint32_t transported = ProductionChainManager::WithdrawOutput(fid, c_enrich, 10);
	CHECK(transported == 10);
	StockpileManager::AddCargo(w_core, c0, c_enrich, transported);
	CHECK(StockpileManager::GetStock(w_core, c0, c_enrich) == 10);

	/* Set up Corporate R&D Project: Dual-Track Throat Arrays (300 RP) with $0 cash budget */
	TechTreeManager::RestoreCompanyTech(c0, TECH_NONE, 0, 0, {TECH_PORTAL_1});
	TechTreeManager::SetActiveProject(c0, TECH_PORTAL_2);
	TechTreeManager::SetMonthlyBudget(c0, 0);

	/* Month 1: Burns up to 5 Enriched Crystals from HQ stockpile -> +50 RP */
	TechTreeManager::ProcessMonthlyResearch();
	CHECK(TechTreeManager::GetAccumulatedRP(c0) == 50);
	CHECK(StockpileManager::GetStock(w_core, c0, c_enrich) == 5);

	/* Month 2: Burns remaining 5 Enriched Crystals -> +50 RP -> total 100 RP */
	TechTreeManager::ProcessMonthlyResearch();
	CHECK(TechTreeManager::GetAccumulatedRP(c0) == 100);
	CHECK(StockpileManager::GetStock(w_core, c0, c_enrich) == 0);
}

TEST_CASE("Sprint 42 Production Chains - Logistics Hub Stockpile Auto-Buffering")
{
	(void)MockEnvironment::Instance();
	PlanetManager::Reset();
	LogisticsHubManager::Reset();
	StockpileManager::Reset();
	ProductionChainManager::Reset();

	WorldID w_refining{2};
	PlanetRegion r_refining{
		.id = w_refining,
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r_refining);

	CompanyID c0{0};
	TileIndex hub_tile{310};
	StationID st{1};

	/* Build Logistics Hub on World 2 for Company 0 */
	LogisticsHubManager::RegisterHub(hub_tile, w_refining, c0, st, "Vulcan Logistics Hub");
	REQUIRE(LogisticsHubManager::HasLogisticsHub(w_refining, c0));

	/* Register Copper Smelter (2 Copper Ore -> 2 Conductive Wiring) */
	TileIndex smelter_tile{320};
	FacilityID fid = ProductionChainManager::RegisterFacility(smelter_tile, w_refining, RECIPE_COPPER_SMELTING, c0, 100, st);
	REQUIRE(fid != INVALID_FACILITY);

	CargoType c_copper = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::CopperOre);
	CargoType c_wire   = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::ConductiveWiring);

	/* Deliver 50 units of Copper Ore */
	ProductionChainManager::DeliverCargo(fid, c_copper, 50);

	/* Process monthly production: with Logistics Hub present, finished wire auto-buffers to stockpile! */
	ProductionChainManager::ProcessMonthlyProduction();

	ProcessingFacility *fac = ProductionChainManager::GetFacility(fid);
	REQUIRE(fac != nullptr);
	CHECK(fac->output_buffers[c_wire] == 0); // Output automatically deposited into stockpile
	CHECK(StockpileManager::GetStock(w_refining, c0, c_wire) == 50);
}

TEST_CASE("Sprint 42 Production Chains - Save/Load Serialization (PROD Chunk)")
{
	(void)MockEnvironment::Instance();
	PlanetManager::Reset();
	ProductionChainManager::Reset();

	WorldID w{2};
	PlanetRegion r{
		.id = w,
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
	};
	PlanetManager::RegisterRegion(r);

	CompanyID c0{0};
	TileIndex t1{320};
	StationID s1{5};

	FacilityID fid = ProductionChainManager::RegisterFacility(t1, w, RECIPE_STEEL_SMELTING, c0, 75, s1);
	REQUIRE(fid != INVALID_FACILITY);

	CargoType c_iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	CargoType c_steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);

	ProductionChainManager::DeliverCargo(fid, c_iron, 30);
	ProcessingFacility *fac = ProductionChainManager::GetFacility(fid);
	REQUIRE(fac != nullptr);
	fac->output_buffers[c_steel] = 15;
	fac->last_month_production = 15;
	fac->total_produced = 100;

	/* Save state */
	std::map<CargoType, uint32_t> saved_inputs = fac->input_buffers;
	std::map<CargoType, uint32_t> saved_outputs = fac->output_buffers;

	/* Reset and restore */
	ProductionChainManager::Reset();
	CHECK(ProductionChainManager::GetAllFacilities().empty());

	ProductionChainManager::RestoreFacility(fid, t1, w, RECIPE_STEEL_SMELTING, c0, s1, 75, 15, 100, saved_inputs, saved_outputs);

	ProcessingFacility *restored = ProductionChainManager::GetFacility(fid);
	REQUIRE(restored != nullptr);
	CHECK(restored->id == fid);
	CHECK(restored->tile == t1);
	CHECK(restored->world_id == w);
	CHECK(restored->recipe_id == RECIPE_STEEL_SMELTING);
	CHECK(restored->owner == c0);
	CHECK(restored->linked_station == s1);
	CHECK(restored->monthly_capacity == 75);
	CHECK(restored->last_month_production == 15);
	CHECK(restored->total_produced == 100);
	CHECK(restored->input_buffers[c_iron] == 30);
	CHECK(restored->output_buffers[c_steel] == 15);
}
