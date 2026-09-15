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
#include "../portal/portal_cmd.h"
#include "../portal/portal_registry.h"
#include "../command_func.h"
#include "../station_cmd.h"
#include "../station_func.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../station_kdtree.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../train.h"
#include "../clear_map.h"
#include "../map_func.h"
#include "../cargotype.h"
#include "../economy_base.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "../table/sprites.h"
#include <filesystem>

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

static Station *SetupProductionGameplay()
{
	(void)MockEnvironment::Instance();
	/* Real station removal/save-load formats viewport labels, unlike the pure
	 * domain tests. Supply a real language pack as well as mock graphics. */
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
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
	ProductionChainManager::Reset();
	PlanetManager::Reset();
	PortalRegistry::Reset();
	StockpileManager::Reset();
	LogisticsHubManager::Reset();
	TechTreeManager::Reset();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_town_pool.CleanPool();
	_company_pool.CleanPool();
	_cargopacket_pool.CleanPool();
	Map::Allocate(64, 64);
	for (uint y = 0; y < 64; ++y) {
		for (uint x = 0; x < 64; ++x) MakeClear(TileXY(x, y), ClearGround::Grass, 3);
	}
	SetupCargoForClimate(LandscapeType::Temperate);
	Company *company = Company::CreateAtIndex(CompanyID{0});
	company->money = 10000000;
	company->clear_limit = 1000 << 16;
	company->infrastructure.station = 1;
	company->infrastructure.rail[RAILTYPE_RAIL] = 1;
	_current_company = company->index;
	PlanetRegion region{
		.id = WorldID{0}, .name = "Production test world", .phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate, .min_x = 1, .min_y = 1, .max_x = 62, .max_y = 62,
	};
	REQUIRE(PlanetManager::RegisterRegion(region));
	TileIndex tile = TileXY(20, 20);
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(10, 10));
	town->name = "Production Test Town";
	town->townnametype = SPECSTR_TOWNNAME_START;
	REQUIRE(Station::CanAllocateItem());
	Station *station = Station::Create(tile);
	station->name = "Production Test Station";
	station->owner = company->index;
	station->town = town;
	station->facilities.Set(StationFacility::Train);
	station->train_station = TileArea(tile, 1, 1);
	station->spread = station->train_station;
	MakeRailStation(tile, company->index, station->index, Axis::X, 0, RAILTYPE_RAIL);
	RebuildStationKdtree();
	RebuildTownKdtree();
	station->RecomputeCatchment();
	if (_valid_searchpaths.empty()) _valid_searchpaths.push_back(Searchpath::WorkingDir);
	return station;
}

TEST_CASE("Production gameplay - command validation and station lifecycle", "[production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	StationID sid = station->index;
	const CargoType iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	CHECK(Command<Commands::BuildProcessingFacility>::Do({}, sid, RECIPE_STEEL_SMELTING).GetCost() == 100000);
	CHECK(ProductionChainManager::GetFacilityForStation(sid) == nullptr);
	CHECK(Command<Commands::BuildProcessingFacility>::Do({}, sid, RECIPE_QUANTUM_ENRICHMENT).Failed());
	CHECK(Command<Commands::BuildProcessingFacility>::Do({}, sid, RECIPE_NONE).Failed());
	CHECK(Command<Commands::BuildProcessingFacility>::Do({}, StationID::Invalid(), RECIPE_STEEL_SMELTING).Failed());
	REQUIRE(Company::CanAllocateItem());
	Company *other = Company::Create();
	_current_company = other->index;
	CHECK(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, sid, RECIPE_STEEL_SMELTING).Failed());
	_current_company = station->owner;
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, sid, RECIPE_STEEL_SMELTING).Succeeded());
	CHECK(station->goods[iron].status.Test(GoodsEntry::State::Acceptance));
	CHECK_FALSE(station->always_accepted.Test(iron));
	CHECK(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, sid, RECIPE_COPPER_SMELTING).Failed());
	CHECK(ProductionChainManager::GetAllFacilities().size() == 1);
	CHECK(ProductionChainManager::DeliverToStation(sid, iron, 3) == 3);
	CHECK(ProductionChainManager::DeliverToStation(sid, GetCargoTypeByLabel(CT_PASSENGERS), 9) == 0);

	SECTION("Large output buffers are split into valid cargo packets") {
		CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
		ProcessingFacility *facility = ProductionChainManager::GetFacilityForStation(sid);
		facility->output_buffers[steel] = 70000;
		ProductionChainManager::PublishStationOutput(*facility);
		CHECK(facility->output_buffers[steel] == 0);
		CHECK(station->goods[steel].AvailableCount() == 70000);
		CHECK(station->goods[steel].HasRating());
	}
	SECTION("Retirement salvages inputs and dry run preserves attachment") {
		_current_company = other->index;
		CHECK(Command<Commands::RemoveProcessingFacility>::Do(DoCommandFlag::Execute, sid).Failed());
		_current_company = station->owner;
		CHECK(Command<Commands::RemoveProcessingFacility>::Do({}, sid).Succeeded());
		CHECK(ProductionChainManager::GetFacilityForStation(sid) != nullptr);
		CHECK(Command<Commands::RemoveProcessingFacility>::Do(DoCommandFlag::Execute, sid).Succeeded());
		CHECK(ProductionChainManager::GetFacilityForStation(sid) == nullptr);
		CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 3);
		CHECK_FALSE(station->goods[iron].status.Test(GoodsEntry::State::Acceptance));
	}
	SECTION("Removing the last platform retires production immediately") {
		TileIndex tile = station->train_station.tile;
		REQUIRE(Command<Commands::RemoveFromRailStation>::Do({}, tile, tile, false).Succeeded());
		CHECK(ProductionChainManager::GetFacilityForStation(sid) != nullptr);
		REQUIRE(Command<Commands::RemoveFromRailStation>::Do(DoCommandFlag::Execute, tile, tile, false).Succeeded());
		CHECK(ProductionChainManager::GetFacilityForStation(sid) == nullptr);
		CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, iron) == 3);
		CHECK_FALSE(station->goods[iron].status.Test(GoodsEntry::State::Acceptance));
	}
	SECTION("Partial platform removal preserves production at the remaining tile") {
		const TileIndex first = station->train_station.tile;
		const TileIndex remaining = TileXY(TileX(first) + 1, TileY(first));
		MakeRailStation(remaining, station->owner, sid, Axis::X, 0, RAILTYPE_RAIL);
		station->train_station = TileArea(first, 2, 1);
		station->spread = station->train_station;
		REQUIRE(Command<Commands::RemoveFromRailStation>::Do(DoCommandFlag::Execute, first, first, false).Succeeded());
		const auto *facility = ProductionChainManager::GetFacilityForStation(sid);
		REQUIRE(facility != nullptr);
		CHECK(facility->tile == remaining);
		CHECK(facility->input_buffers.at(iron) == 3);
		CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 0);
	}
	SECTION("Acquisition transfers facility ownership and bankruptcy removes it") {
		ProductionChainManager::ChangeCompanyOwner(station->owner, other->index);
		CHECK(ProductionChainManager::GetFacilityForStation(sid)->owner == other->index);
		ProductionChainManager::ChangeCompanyOwner(other->index, CompanyID::Invalid());
		CHECK(ProductionChainManager::GetFacilityForStation(sid) == nullptr);
	}
}

TEST_CASE("Production gameplay - real cargo unloading conversion and onward loading", "[production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING).Succeeded());
	const CargoType iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	const CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	REQUIRE(Train::CanAllocateItem());
	Train *train = Train::Create();
	train->owner = station->owner;
	train->last_station_visited = station->index;
	train->tile = station->xy;
	train->cargo_type = iron;
	train->cargo_cap = 60;
	REQUIRE(CargoPayment::CanAllocateItem());
	CargoPayment *payment = CargoPayment::Create(train);
	train->cargo_payment = payment;
	REQUIRE(CargoPacket::CanAllocateItem());
	CargoPacket *input = CargoPacket::Create(60, 1, StationID::Invalid(), TileXY(5, 5), 0);
	input->UpdateLoadingTile(TileXY(5, 5));
	train->cargo.Append(input, VehicleCargoList::MoveToAction::Keep);
	train->cargo.Stage(true, station->index, {}, OrderUnloadType::Unload, &station->goods[iron], iron, payment, station->xy);

	SECTION("Output becomes ordinary station cargo") {
		CHECK(train->cargo.Unload(60, &station->goods[iron].GetOrCreateData().cargo, iron, payment, station->xy) == 60);
		CHECK(train->cargo.StoredCount() == 0);
		CHECK(ProductionChainManager::GetFacilityForStation(station->index)->input_buffers[iron] == 60);
		CHECK(Company::Get(station->owner)->cur_economy.delivered_cargo[iron] == 60);
		CHECK(station->town->received[CargoSpec::Get(iron)->town_acceptance_effect].new_act == 0);
		ProductionChainManager::ProcessMonthlyProduction();
		CHECK(station->goods[steel].AvailableCount() == 30);
		CHECK(ProductionChainManager::GetFacilityForStation(station->index)->input_buffers[iron] == 0);
		train->cargo_type = steel;
		CHECK(station->goods[steel].GetOrCreateData().cargo.Load(30, &train->cargo, {}, station->xy) == 30);
		CHECK(train->cargo.StoredCount() == 30);
		CHECK(station->goods[steel].AvailableCount() == 0);
		ProductionChainManager::ProcessMonthlyProduction();
		CHECK(station->goods[steel].AvailableCount() == 0);
	}
	SECTION("A hub cannot duplicate delivered recipe inputs") {
		LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Test hub");
		CHECK(train->cargo.Unload(60, &station->goods[iron].GetOrCreateData().cargo, iron, payment, station->xy) == 60);
		CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 0);
		ProductionChainManager::ProcessMonthlyProduction();
		CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, steel) == 30);
		CHECK(station->goods[steel].AvailableCount() == 0);
	}
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE("Production gameplay - station facility survives actual save and reload", "[production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	StationID sid = station->index;
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, sid, RECIPE_STEEL_SMELTING).Succeeded());
	const CargoType iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	const CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	REQUIRE(ProductionChainManager::DeliverToStation(sid, iron, 41) == 41);
	ProductionChainManager::ProcessMonthlyProduction();
	const auto path = (std::filesystem::temp_directory_path() / "openspacettd-production-gameplay.sav").string();
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	const ProcessingFacility *facility = ProductionChainManager::GetFacilityForStation(sid);
	REQUIRE(facility != nullptr);
	CHECK(facility->recipe_id == RECIPE_STEEL_SMELTING);
	CHECK(facility->input_buffers.at(iron) == 1);
	CHECK(facility->last_month_production == 20);
	station = Station::Get(sid);
	CHECK(station->goods[steel].AvailableCount() == 20);
	CHECK(station->goods[iron].status.Test(GoodsEntry::State::Acceptance));
	ProductionChainManager::DeliverToStation(sid, iron, 1);
	ProductionChainManager::ProcessMonthlyProduction();
	CHECK(station->goods[steel].AvailableCount() == 21);
	std::filesystem::remove(path);
}
