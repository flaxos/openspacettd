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
#include "../vehicle_func.h"
#include "../clear_map.h"
#include "../map_func.h"
#include "../cargotype.h"
#include "../economy_base.h"
#include "../economy_func.h"
#include "../industry.h"
#include "../void_map.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "../table/sprites.h"
#include <filesystem>
#include <chrono>

#include "../table/strings.h"
#include "../safeguards.h"

static Station *SetupProductionGameplay();

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
	Station *station = SetupProductionGameplay();
	WorldID w_refining{0};
	CompanyID c0{0};
	TileIndex hub_tile = station->xy;
	StationID st = station->index;

	/* The auto-buffer requires a live, owned rail attachment. */
	REQUIRE(LogisticsHubManager::RegisterHub(hub_tile, w_refining, c0, st, "Vulcan Logistics Hub") != 0);
	REQUIRE(LogisticsHubManager::HasLogisticsHub(w_refining, c0));

	/* Register Copper Smelter (2 Copper Ore -> 2 Conductive Wiring) */
	TileIndex smelter_tile = station->xy;
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
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	_station_pool.CleanPool();
	_industry_pool.CleanPool();
	_town_pool.CleanPool();
	_company_pool.CleanPool();
	_cargopacket_pool.CleanPool();
	Map::Allocate(64, 64);
	for (uint y = 0; y < 64; ++y) {
		for (uint x = 0; x < 64; ++x) {
			TileIndex tile = TileXY(x, y);
			if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
			else MakeVoid(tile);
		}
	}
	SetupCargoForClimate(LandscapeType::Temperate);
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
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

TEST_CASE("Hub authority - invalid attachments reject query and execute without mutation", "[hub-authority]")
{
	Station *station = SetupProductionGameplay();
	TileIndex tile = station->xy;
	StationID sid = station->index;
	SECTION("Missing explicit station") { sid = StationID{100}; }
	SECTION("Invalid tile") { tile = INVALID_TILE; }
	SECTION("Outside map") { tile = TileIndex{Map::Size()}; }
	SECTION("Nonexistent company") { _current_company = CompanyID{1}; }
	SECTION("Foreign explicit station") {
		REQUIRE(Company::CanAllocateItem());
		_current_company = Company::Create()->index;
	}
	SECTION("Foreign automatic station") {
		REQUIRE(Company::CanAllocateItem());
		_current_company = Company::Create()->index;
		sid = StationID::Invalid();
	}
	SECTION("Rail facility removed") { station->facilities.Reset(StationFacility::Train); }
	SECTION("Rail area empty") { station->train_station.Clear(); }
	SECTION("Platform removed but metadata remains") { MakeClear(tile, ClearGround::Grass, 3); }
	SECTION("Waypoint is not a freight platform") { MakeRailWaypoint(tile, station->owner, sid, Axis::X, 0, RAILTYPE_RAIL); }
	SECTION("Platform owner differs from station owner") { SetTileOwner(tile, OWNER_NONE); }
	SECTION("Distant explicit station") { tile = TileXY(25, 20); }
	SECTION("No nearby automatic station") { tile = TileXY(25, 20); sid = StationID::Invalid(); }
	SECTION("Wrong world despite nearby platform") {
		PlanetManager::Reset();
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "West", .phase = WorldPhase::Phase2_Developed,
			.biome = WorldBiome::Temperate, .min_x = 1, .min_y = 1, .max_x = 20, .max_y = 62}));
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{1}, .name = "East", .phase = WorldPhase::Phase2_Developed,
			.biome = WorldBiome::Temperate, .min_x = 21, .min_y = 1, .max_x = 62, .max_y = 62}));
		tile = TileXY(21, 20);
	}
	SECTION("Already bound station") {
		REQUIRE(LogisticsHubManager::RegisterHub(tile, WorldID{0}, station->owner, sid, "Existing") != 0);
		tile = TileXY(21, 20);
	}
	SECTION("Already occupied hub tile") {
		REQUIRE(LogisticsHubManager::RegisterHub(tile, WorldID{0}, station->owner, sid, "Existing") != 0);
	}
	const size_t hubs_before = LogisticsHubManager::GetAllHubs().size();
	const auto money_before = Company::Get(CompanyID{0})->money;
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, CargoType{2}, 123);
	CHECK(Command<Commands::BuildLogisticsHub>::Do({}, tile, sid, "Rejected").Failed());
	CHECK(LogisticsHubManager::GetAllHubs().size() == hubs_before);
	CHECK(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, tile, sid, "Rejected").Failed());
	CHECK(LogisticsHubManager::GetAllHubs().size() == hubs_before);
	CHECK(Company::Get(CompanyID{0})->money == money_before);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, CargoType{2}) == 123);
}

static Station *AddHubTestStation(Station *first, TileIndex tile)
{
	REQUIRE(Station::CanAllocateItem());
	Station *station = Station::Create(tile);
	station->name = "Other hub station";
	station->owner = first->owner;
	station->town = first->town;
	station->facilities.Set(StationFacility::Train);
	station->train_station = TileArea(tile, 1, 1);
	station->spread = station->train_station;
	MakeRailStation(tile, station->owner, station->index, Axis::X, 0, RAILTYPE_RAIL);
	Company::Get(station->owner)->infrastructure.station++;
	Company::Get(station->owner)->infrastructure.rail[RAILTYPE_RAIL]++;
	RebuildStationKdtree();
	station->RecomputeCatchment();
	return station;
}

TEST_CASE("Hub authority - explicit and automatic placement share cost and eligibility", "[hub-authority]")
{
	Station *station = SetupProductionGameplay();
	const bool automatic = GENERATE(false, true);
	const TileIndex tile = TileXY(24, 20); // Inclusive four-tile platform boundary.
	const auto money = Company::Get(station->owner)->money;
	const StationID request = automatic ? StationID::Invalid() : station->index;
	auto query = Command<Commands::BuildLogisticsHub>::Do({}, tile, request, "Boundary hub");
	REQUIRE(query.Succeeded());
	CHECK(query.GetCost() == 75000);
	CHECK(LogisticsHubManager::GetAllHubs().empty());
	CHECK(Company::Get(station->owner)->money == money);
	auto execute = Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, tile, request, "Boundary hub");
	REQUIRE(execute.Succeeded());
	CHECK(execute.GetCost() == query.GetCost());
	CHECK(Company::Get(station->owner)->money == money - 75000);
	const auto *hub = LogisticsHubManager::GetHubAtTile(tile);
	REQUIRE(hub != nullptr);
	CHECK(hub->hub_id == 1); // Quotes never allocate an ID.
	CHECK(hub->station_id == station->index);
	CHECK(hub->company_id == station->owner);
	CHECK(hub->world_id == WorldID{0});
	CHECK(LogisticsHubManager::RegisterHub(tile, WorldID{0}, station->owner, station->index, "Duplicate") == 0);
	CHECK(LogisticsHubManager::GetAllHubs().size() == 1);
}

TEST_CASE("Hub authority - automatic placement chooses nearest eligible platform deterministically", "[hub-authority]")
{
	Station *first = SetupProductionGameplay();
	Station *second = AddHubTestStation(first, TileXY(24, 20));
	TileIndex tile = TileXY(22, 20);
	StationID expected = first->index;
	SECTION("Equal distance chooses lower station ID") { }
	SECTION("Closer higher station ID wins") { tile = TileXY(23, 20); expected = second->index; }
	SECTION("An already bound station is skipped") {
		REQUIRE(LogisticsHubManager::RegisterHub(first->xy, WorldID{0}, first->owner, first->index, "Existing") != 0);
		expected = second->index;
	}
	SECTION("Nonrail station is skipped") {
		first->facilities.Reset(StationFacility::Train);
		expected = second->index;
	}
	SECTION("Foreign station is skipped") {
		REQUIRE(Company::CanAllocateItem());
		first->owner = Company::Create()->index;
		SetTileOwner(first->xy, first->owner);
		expected = second->index;
	}
	const auto count = LogisticsHubManager::GetAllHubs().size();
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do({}, tile, StationID::Invalid(), "Auto").Succeeded());
	CHECK(LogisticsHubManager::GetAllHubs().size() == count);
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, tile, StationID::Invalid(), "Auto").Succeeded());
	REQUIRE(LogisticsHubManager::GetHubAtTile(tile) != nullptr);
	CHECK(LogisticsHubManager::GetHubAtTile(tile)->station_id == expected);
}

TEST_CASE("Hub authority - platform removal and station deletion retire only invalid bindings", "[hub-authority]")
{
	Station *station = SetupProductionGameplay();
	const StationID sid = station->index;
	const TileIndex first = station->xy;
	const TileIndex remaining = TileXY(21, 20);
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	TileIndex anchor = first;
	bool keep = false;
	bool destroy = false;
	SECTION("Last rail tile removes hub immediately") { }
	SECTION("Partial removal keeps nearby remaining platform") { keep = true; }
	SECTION("Partial removal retires hub when remaining platform is too far") { anchor = TileXY(16, 20); }
	SECTION("Station destructor cannot leave binding for a reused ID") { destroy = true; }
	if (keep || anchor != first) {
		MakeRailStation(remaining, station->owner, sid, Axis::X, 0, RAILTYPE_RAIL);
		station->train_station = TileArea(first, 2, 1);
		station->spread = station->train_station;
		Company::Get(station->owner)->infrastructure.station++;
		Company::Get(station->owner)->infrastructure.rail[RAILTYPE_RAIL]++;
	}
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, anchor, sid, "Lifecycle").Succeeded());
	const uint32_t id = LogisticsHubManager::GetHubAtTile(anchor)->hub_id;
	LogisticsHubManager::SetReserveFloor(id, iron, 40);
	REQUIRE(LogisticsHubManager::DepositToStockpile(anchor, station->owner, iron, 100));
	if (destroy) {
		delete station;
		station = Station::CreateAtIndex(sid, first);
		station->town = Town::Get(TownID{0});
		station->owner = CompanyID{0};
		station->train_station = TileArea(first, 1, 1);
		station->facilities.Set(StationFacility::Train);
	} else {
		REQUIRE(Command<Commands::RemoveFromRailStation>::Do({}, first, first, false).Succeeded());
		CHECK(LogisticsHubManager::GetHub(id) != nullptr);
		REQUIRE(Command<Commands::RemoveFromRailStation>::Do(DoCommandFlag::Execute, first, first, false).Succeeded());
	}
	CHECK((LogisticsHubManager::GetHub(id) != nullptr) == keep);
	CHECK((LogisticsHubManager::GetHubForStation(sid) != nullptr) == keep);
	CHECK(LogisticsHubManager::HasLogisticsHub(WorldID{0}, CompanyID{0}) == keep);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, iron) == 100);
	if (keep) {
		CHECK(LogisticsHubManager::GetHub(id)->tile == anchor);
		CHECK(LogisticsHubManager::GetReserveFloor(id, iron) == 40);
	} else {
		CHECK_FALSE(LogisticsHubManager::DepositToStockpile(anchor, CompanyID{0}, iron, 1));
		CHECK(LogisticsHubManager::WithdrawFromStockpile(anchor, CompanyID{0}, iron, 1) == 0);
	}
}

/** Exercise the actual LHUB save chunk and full engine after-load lifecycle. */
static Station *ReloadHubAuthority(StationID sid)
{
	const auto dir = std::filesystem::temp_directory_path() / fmt::format("openspacettd-hub-wp04-{}", std::chrono::steady_clock::now().time_since_epoch().count());
	REQUIRE(std::filesystem::create_directory(dir));
	const auto path = (dir / "authority.sav").string();
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	std::filesystem::remove(path);
	std::filesystem::remove(dir);
	return Station::Get(sid);
}

TEST_CASE("Hub authority - native company acquisition transfers attachment and bankruptcy removes it", "[hub-authority]")
{
	Station *station = SetupProductionGameplay();
	const StationID sid = station->index;
	const TileIndex tile = station->xy;
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	REQUIRE(Company::CanAllocateItem());
	const CompanyID buyer = Company::Create()->index;
	Company::Get(buyer)->money = 10000000;
	AutoRestoreBackup local_company(_local_company, COMPANY_SPECTATOR);
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, tile, sid, "Acquired hub").Succeeded());
	const uint32_t id = LogisticsHubManager::GetHubForStation(sid)->hub_id;
	LogisticsHubManager::SetReserveFloor(id, iron, 40);
	ChangeOwnershipOfCompanyItems(CompanyID{0}, buyer);
	REQUIRE(LogisticsHubManager::GetHubForStation(sid) != nullptr);
	CHECK(LogisticsHubManager::GetHub(id)->company_id == buyer);
	CHECK(station->owner == buyer);
	CHECK(GetTileOwner(tile) == buyer);
	CHECK_FALSE(LogisticsHubManager::HasLogisticsHub(WorldID{0}, CompanyID{0}));
	CHECK(LogisticsHubManager::HasLogisticsHub(WorldID{0}, buyer));
	CHECK_FALSE(LogisticsHubManager::DepositToStockpile(tile, CompanyID{0}, iron, 100));
	CHECK(LogisticsHubManager::WithdrawFromStockpile(tile, CompanyID{0}, iron, 100) == 0);
	CHECK(LogisticsHubManager::DepositToStockpile(tile, buyer, iron, 100));
	CHECK(LogisticsHubManager::WithdrawFromStockpile(tile, buyer, iron, 100) == 60);
	station = ReloadHubAuthority(sid);
	REQUIRE(LogisticsHubManager::GetHubForStation(sid) != nullptr);
	CHECK(LogisticsHubManager::GetHub(id)->company_id == buyer);
	CHECK(LogisticsHubManager::GetReserveFloor(id, iron) == 40);
	CHECK(LogisticsHubManager::GetHub(id)->total_deposited == 100);
	CHECK(LogisticsHubManager::GetHub(id)->total_dispatched == 60);
	CHECK(StockpileManager::GetStock(WorldID{0}, buyer, iron) == 40);
	ChangeOwnershipOfCompanyItems(buyer, INVALID_OWNER);
	CHECK(LogisticsHubManager::GetHub(id) == nullptr);
	CHECK_FALSE(LogisticsHubManager::HasLogisticsHub(WorldID{0}, buyer));
	CHECK(StockpileManager::GetStock(WorldID{0}, buyer, iron) == 40);
	ReloadHubAuthority(sid);
	CHECK(LogisticsHubManager::GetAllHubs().empty());
	CHECK(StockpileManager::GetStock(WorldID{0}, buyer, iron) == 40);
}

TEST_CASE("Hub authority - save reload retains valid bindings and retires legacy invalid duplicates", "[hub-authority]")
{
	Station *station = SetupProductionGameplay();
	const StationID sid = station->index;
	const TileIndex tile = station->xy;
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, tile, sid, "Valid survivor").Succeeded());
	const uint32_t id = LogisticsHubManager::GetHubForStation(sid)->hub_id;
	LogisticsHubManager::SetReserveFloor(id, iron, 40);
	REQUIRE(LogisticsHubManager::DepositToStockpile(tile, CompanyID{0}, iron, 100));
	CHECK(LogisticsHubManager::WithdrawFromStockpile(tile, CompanyID{0}, iron, 20) == 20);
	const LogisticsHub valid = *LogisticsHubManager::GetHub(id);
	LogisticsHub legacy = valid;
	legacy.hub_id = 100;
	SECTION("Duplicate station at nearby anchor") { legacy.tile = TileXY(21, 20); }
	SECTION("Duplicate tile with another eligible station") { legacy.station_id = AddHubTestStation(station, TileXY(21, 20))->index; }
	SECTION("Missing station") { legacy.station_id = StationID{100}; legacy.tile = TileXY(30, 20); }
	SECTION("Foreign station") { legacy.company_id = CompanyID{1}; }
	SECTION("Missing world") { legacy.world_id = INVALID_WORLD; }
	SECTION("Out of map tile") { legacy.tile = TileIndex{Map::Size()}; }
	SECTION("Distant station") { legacy.tile = TileXY(30, 20); }
	SECTION("Duplicate maximum hub ID") { legacy.hub_id = UINT32_MAX; }
	SECTION("Reserved zero hub ID") { legacy.hub_id = 0; }
	LogisticsHubManager::RestoreHub(legacy);
	station = ReloadHubAuthority(sid);
	REQUIRE(LogisticsHubManager::GetAllHubs().size() == 1);
	const auto *restored = LogisticsHubManager::GetHub(id);
	REQUIRE(restored != nullptr);
	CHECK(restored->name == valid.name);
	CHECK(restored->station_id == sid);
	CHECK(restored->company_id == CompanyID{0});
	CHECK(restored->tile == tile);
	CHECK(restored->world_id == WorldID{0});
	CHECK(restored->reserve_floors == valid.reserve_floors);
	CHECK(restored->total_deposited == 100);
	CHECK(restored->total_dispatched == 20);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, iron) == 80);
	// Round-trip the repaired save, and prove a malicious ID cannot disable future construction.
	station = ReloadHubAuthority(sid);
	Station *second = AddHubTestStation(station, TileXY(30, 20));
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, second->xy, second->index, "After reload").Succeeded());
	REQUIRE(LogisticsHubManager::GetHubForStation(second->index) != nullptr);
	CHECK(LogisticsHubManager::GetHubForStation(second->index)->hub_id != 0);
	CHECK(LogisticsHubManager::GetHubForStation(second->index)->hub_id != id);
}

TEST_CASE("Hub authority - maximum saved hub ID preserves binding and future construction", "[hub-authority]")
{
	Station *station = SetupProductionGameplay();
	const StationID sid = station->index;
	LogisticsHubManager::RestoreHub({.hub_id = UINT32_MAX, .tile = station->xy, .world_id = WorldID{0},
		.company_id = station->owner, .station_id = sid, .name = "Maximum ID", .reserve_floors = {}});
	station = ReloadHubAuthority(sid);
	REQUIRE(LogisticsHubManager::GetHub(UINT32_MAX) != nullptr);
	CHECK(LogisticsHubManager::GetHub(UINT32_MAX)->station_id == sid);
	Station *other = AddHubTestStation(station, TileXY(30, 20));
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do({}, other->xy, other->index, "New ID").Succeeded());
	REQUIRE(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, other->xy, other->index, "New ID").Succeeded());
	REQUIRE(LogisticsHubManager::GetHubForStation(other->index) != nullptr);
	CHECK(LogisticsHubManager::GetHubForStation(other->index)->hub_id != 0);
	CHECK(LogisticsHubManager::GetHubForStation(other->index)->hub_id != UINT32_MAX);
	CHECK(LogisticsHubManager::GetAllHubs().size() == 2);
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

/** A real cargo packet entering the normal station unloading preparation path. */
static Train *PrepareHubDelivery(Station *station, CargoType cargo, OrderUnloadType unload = OrderUnloadType::Unload)
{
	if (Engine::GetIfValid(EngineID{0}) == nullptr) {
		_engine_mngr.ResetToDefaultMapping();
		SetupEngines();
	}
	REQUIRE(Train::CanAllocateItem());
	Train *train = Train::Create();
	train->SetFrontEngine();
	train->engine_type = EngineID{0};
	train->owner = station->owner;
	train->last_station_visited = station->index;
	train->tile = station->xy;
	train->cargo_type = cargo;
	train->cargo_cap = 60;
	train->current_order.MakeLoading(false);
	train->current_order.SetUnloadType(unload);
	train->current_order.SetLoadType(OrderLoadType::NoLoad);
	REQUIRE(CargoPacket::CanAllocateItem());
	CargoPacket *input = CargoPacket::Create(60, 1, StationID::Invalid(), TileXY(5, 5), 0);
	input->UpdateLoadingTile(TileXY(5, 5));
	train->cargo.Append(input, VehicleCargoList::MoveToAction::Keep);
	PrepareUnload(train);
	return train;
}

TEST_CASE("Hub unloading - industry cargo has exactly one destination", "[hub-unloading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	CargoSpec::Get(iron)->current_payment = 1000;
	REQUIRE(Industry::CanAllocateItem());
	Industry *industry = Industry::Create(TileXY(21, 20));
	industry->town = station->town;
	industry->accepted.push_back({.cargo = iron});
	station->industries_near.insert({0, industry});
	station->goods[iron].status.Set(GoodsEntry::State::Acceptance);
	const bool has_hub = GENERATE(false, true);
	CAPTURE(has_hub);
	if (has_hub) REQUIRE(LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Conservation hub") != 0);
	Train *train = PrepareHubDelivery(station, iron);
	REQUIRE(train->cargo.ActionCount(VehicleCargoList::MoveToAction::Deliver) == 60);
	CargoPayment *payment = train->cargo_payment;
	const uint32_t development = PlanetManager::GetRegion(WorldID{0})->development_score;
	const auto town_effect = CargoSpec::Get(iron)->town_acceptance_effect;
	for (uint moved : {17u, 43u}) {
		REQUIRE(train->cargo.Unload(moved, &station->goods[iron].GetOrCreateData().cargo, iron, payment, station->xy) == moved);
		uint delivered = 60 - train->cargo.StoredCount();
		uint stock = StockpileManager::GetStock(WorldID{0}, station->owner, iron);
		CHECK(industry->accepted[0].waiting + stock + train->cargo.StoredCount() + station->goods[iron].AvailableCount() == 60);
		CHECK(industry->accepted[0].waiting == (has_hub ? 0 : delivered));
		CHECK(stock == (has_hub ? delivered : 0));
		CHECK(Company::Get(station->owner)->cur_economy.delivered_cargo[iron] == (has_hub ? 0 : delivered));
		CHECK(station->town->received[town_effect].new_act == (has_hub ? 0 : delivered));
		if (has_hub) {
			CHECK(payment->route_profit == 0);
			CHECK(PlanetManager::GetRegion(WorldID{0})->development_score == development);
			CHECK(LogisticsHubManager::GetHubForStation(station->index)->total_deposited == delivered);
		} else {
			CHECK(payment->route_profit > 0);
			CHECK(PlanetManager::GetRegion(WorldID{0})->development_score > development);
		}
	}
	/* Finish the station cycle so native production flushes its deferred list. */
	LoadUnloadStation(station);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE("Hub unloading - isolated freight hub accepts real unloading", "[hub-unloading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	REQUIRE(LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Isolated hub") != 0);
	REQUIRE_FALSE(station->goods[iron].status.Test(GoodsEntry::State::Acceptance));
	Train *train = PrepareHubDelivery(station, iron);
	CHECK(train->cargo.ActionCount(VehicleCargoList::MoveToAction::Deliver) == 60);
	AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, false);
	/* Exercise both PrepareUnload and the acceptance recheck in the station tick. */
	LoadUnloadStation(station);
	CHECK(train->cargo.StoredCount() == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 60);
	CHECK(station->goods[iron].AvailableCount() == 0);
	CHECK(train->cargo_payment->route_profit == 0);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE("Hub unloading - town and nonfreight delivery controls", "[hub-unloading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CargoLabel label = GENERATE(CT_GOODS, CT_PASSENGERS, CT_MAIL);
	const CargoType cargo = GetCargoTypeByLabel(label);
	const int hub_owner = GENERATE(-1, 0, 1);
	CAPTURE(label, hub_owner);
	CargoSpec::Get(cargo)->current_payment = 1000;
	station->always_accepted.Set(cargo);
	station->goods[cargo].status.Set(GoodsEntry::State::Acceptance);
	if (hub_owner >= 0) {
		if (hub_owner == 1) {
			REQUIRE(Company::CanAllocateItem());
			REQUIRE(Company::Create()->index == CompanyID{1});
			/* Old saves can contain a foreign binding; it must remain inactive. */
			LogisticsHubManager::RestoreHub({.hub_id = 1, .tile = station->xy, .world_id = WorldID{0},
				.company_id = CompanyID{1}, .station_id = station->index, .name = "Legacy foreign hub", .reserve_floors = {}});
		} else {
			REQUIRE(LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Town hub") != 0);
		}
	}
	Train *train = PrepareHubDelivery(station, cargo);
	REQUIRE(train->cargo.Unload(60, &station->goods[cargo].GetOrCreateData().cargo, cargo, train->cargo_payment, station->xy) == 60);
	const uint stored = hub_owner == 0 && CargoSpec::Get(cargo)->is_freight ? 60 : 0;
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, cargo) == stored);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{1}, cargo) == 0);
	CHECK(station->town->GetOrCreateCargoAccepted(cargo).history[THIS_MONTH].accepted == 60 - stored);
	CHECK(Company::Get(station->owner)->cur_economy.delivered_cargo[cargo] == 60 - stored);
	CHECK((train->cargo_payment->route_profit == 0) == (stored == 60));
	CHECK(station->goods[cargo].AvailableCount() + train->cargo.StoredCount() == 0);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE("Hub unloading - explicit transfer and no unload keep their meaning", "[hub-unloading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	REQUIRE(LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Order hub") != 0);
	const OrderUnloadType order = GENERATE(OrderUnloadType::Transfer, OrderUnloadType::NoUnload);
	Train *train = PrepareHubDelivery(station, iron, order);
	CHECK(train->cargo.ActionCount(VehicleCargoList::MoveToAction::Deliver) == 0);
	if (order == OrderUnloadType::Transfer) {
		CHECK(train->cargo.Unload(60, &station->goods[iron].GetOrCreateData().cargo, iron, train->cargo_payment, station->xy) == 60);
		CHECK(station->goods[iron].AvailableCount() == 60);
	} else {
		CHECK(train->cargo.StoredCount() == 60);
		CHECK(train->cargo.UnloadCount() == 0);
	}
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 0);
	CHECK(train->cargo_payment->route_profit == 0);
	CHECK(Company::Get(station->owner)->cur_economy.delivered_cargo[iron] == 0);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE("Hub unloading - deposited cargo survives actual save and reload", "[hub-unloading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const StationID sid = station->index;
	const CompanyID owner = station->owner;
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	const uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, owner, sid, "Persistent hub");
	REQUIRE(hub_id != 0);
	/* Existing balances remain intact; unloading adds only the physical delivery. */
	StockpileManager::AddCargo(WorldID{0}, owner, iron, 11);
	Train *train = PrepareHubDelivery(station, iron);
	REQUIRE(train->cargo.Unload(60, &station->goods[iron].GetOrCreateData().cargo, iron, train->cargo_payment, station->xy) == 60);
	const Money money = Company::Get(owner)->money;
	delete train->cargo_payment;
	CHECK(Company::Get(owner)->money == money);
	station->loading_vehicles.clear();
	_vehicle_pool.CleanPool();
	const auto dir = std::filesystem::temp_directory_path() / fmt::format("openspacettd-hub-wp02-{}", std::chrono::steady_clock::now().time_since_epoch().count());
	REQUIRE(std::filesystem::create_directory(dir));
	const auto path = (dir / "hub.sav").string();
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == 71);
	REQUIRE(LogisticsHubManager::GetHub(hub_id) != nullptr);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_deposited == 60);
	CHECK(LogisticsHubManager::GetHub(hub_id)->station_id == sid);
	CHECK(Company::Get(owner)->money == money);
	CHECK(Company::Get(owner)->cur_economy.delivered_cargo[iron] == 0);
	CHECK(PlanetManager::GetRegion(WorldID{0})->development_score == 0);
	CHECK(Station::Get(sid)->goods[iron].AvailableCount() == 0);
	std::filesystem::remove(path);
	std::filesystem::remove(dir);
}

TEST_CASE("Hub unloading - invalid stored destination cannot consume cargo", "[hub-unloading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	const bool invalid_tile = GENERATE(false, true);
	LogisticsHubManager::RestoreHub({.hub_id = 1, .tile = invalid_tile ? INVALID_TILE : station->xy,
		.world_id = invalid_tile ? WorldID{0} : INVALID_WORLD, .company_id = station->owner, .station_id = station->index,
		.name = "Invalid destination", .reserve_floors = {}});
	Train *train = PrepareHubDelivery(station, iron);
	CHECK(train->cargo.ActionCount(VehicleCargoList::MoveToAction::Deliver) == 0);
	REQUIRE(train->cargo.Unload(60, &station->goods[iron].GetOrCreateData().cargo, iron, train->cargo_payment, station->xy) == 60);
	CHECK(station->goods[iron].AvailableCount() == 60);
	CHECK(LogisticsHubManager::GetHub(1)->total_deposited == 0);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

/** Reuse the real train fixture for pickup without an incoming delivery. */
static Train *PrepareHubPickup(Station *station, CargoType cargo)
{
	Train *train = PrepareHubDelivery(station, cargo, OrderUnloadType::NoUnload);
	train->cargo.Truncate();
	/* Loading refreshes the consist's capacity and length. Give the controlled
	 * 60-unit test vehicle matching engine data and initialize the native cache. */
	Engine *engine = Engine::Get(train->engine_type);
	engine->info.cargo_type = cargo;
	engine->VehInfo<RailVehicleInfo>().capacity = 60;
	train->ConsistChanged(CCF_ARRANGE);
	REQUIRE(train->cargo_cap == 60);
	train->current_order.SetLoadType(OrderLoadType::LoadIfPossible);
	return train;
}

/** Exercise native allocation checks with a bounded number of free slots. The
 * occupancy bias is removed on scope exit, preserving real allocations/deletions. */
class ScopedCargoPacketCapacity {
	size_t bias;
public:
	explicit ScopedCargoPacketCapacity(size_t free_slots)
	{
		REQUIRE(_cargopacket_pool.items + free_slots <= CargoPacketPool::MAX_SIZE);
		this->bias = CargoPacketPool::MAX_SIZE - free_slots - _cargopacket_pool.items;
		_cargopacket_pool.items += this->bias;
	}
	~ScopedCargoPacketCapacity() { _cargopacket_pool.items -= this->bias; }
};

/** Persist the physical stockpile/station cargo after a refused pickup. */
static Station *ReloadHubPickup(Station *station, Train *train)
{
	REQUIRE(train->cargo.StoredCount() == 0);
	const StationID sid = station->index;
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	train->cargo.Truncate();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	const auto dir = std::filesystem::temp_directory_path() / fmt::format("openspacettd-hub-wp03-{}", std::chrono::steady_clock::now().time_since_epoch().count());
	REQUIRE(std::filesystem::create_directory(dir));
	const auto path = (dir / "pickup.sav").string();
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	std::filesystem::remove(path);
	std::filesystem::remove(dir);
	return Station::Get(sid);
}

TEST_CASE("Hub loading - full packet pool preserves inventory and recovers", "[hub-loading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	const uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Pickup hub");
	REQUIRE(hub_id != 0);
	StockpileManager::AddCargo(WorldID{0}, station->owner, iron, 100);
	LogisticsHubManager::SetReserveFloor(hub_id, iron, 40);
	Train *train = PrepareHubPickup(station, iron);
	const uint16_t waiting = GENERATE(0, 20);
	if (waiting > 0) {
		REQUIRE(CargoPacket::CanAllocateItem());
		station->goods[iron].GetOrCreateData().cargo.Append(CargoPacket::Create(station->index, waiting, Source{}), StationID::Invalid());
	}
	AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, false);
	const size_t packets = CargoPacket::GetNumItems();
	{
		/* Deterministically reject allocation through the native pool check without
		 * constructing sixteen million packets. Existing packets can still move. */
		ScopedCargoPacketCapacity full_pool(0);
		REQUIRE_FALSE(CargoPacket::CanAllocateItem());
		LoadUnloadStation(station);
		CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 100);
		CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 0);
		CHECK(station->goods[iron].AvailableCount() == 0);
		CHECK(train->cargo.StoredCount() == waiting);
		CHECK(_cargopacket_pool.items == CargoPacketPool::MAX_SIZE);
	}
	CHECK(CargoPacket::GetNumItems() == packets);
	REQUIRE(CargoPacket::CanAllocateItem());
	/* Start another pickup attempt after LoadIfPossible completed its earlier slice. */
	train->vehicle_flags.Reset({VehicleFlag::StopLoading, VehicleFlag::LoadingFinished});
	train->load_unload_ticks = 1;
	LoadUnloadStation(station);
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) == 40u + waiting);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 60u - waiting);
	CHECK(train->cargo.StoredCount() == 60);
	CHECK(station->goods[iron].AvailableCount() == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, iron) + train->cargo.StoredCount() + station->goods[iron].AvailableCount() == 100u + waiting);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	train->cargo.Truncate();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	CHECK(CargoPacket::GetNumItems() == 0);
}

TEST_CASE("Hub loading - capacity reserve and rights boundaries conserve cargo", "[hub-loading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CompanyID owner = station->owner;
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	const uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, owner, station->index, "Boundary hub");
	REQUIRE(hub_id != 0);
	Train *train = PrepareHubPickup(station, iron);
	uint32_t stock = 100, expected = 60;
	uint16_t waiting = 0, onboard = 0;
	SECTION("Empty inventory releases the unused allocated packet") { stock = 0; expected = 0; }
	SECTION("Below reserve releases the unused allocated packet") { stock = 39; expected = 0; }
	SECTION("At reserve releases the unused allocated packet") { stock = 40; expected = 0; }
	SECTION("One unit above reserve reduces the allocated packet") { stock = 41; expected = 1; }
	SECTION("Available stock fills capacity") { }
	SECTION("Zero capacity") { train->cargo_cap = 0; expected = 0; }
	SECTION("Full vehicle") { onboard = 60; expected = 0; }
	SECTION("No loading order") { train->current_order.SetLoadType(OrderLoadType::NoLoad); expected = 0; }
	SECTION("Waiting cargo already fills the vehicle") { waiting = 60; expected = 0; }
	SECTION("Only the gap after waiting cargo is withdrawn") { waiting = 20; expected = 40; }
	SECTION("Exclusive rights deny pickup without withdrawing") {
		station->owner = OWNER_NONE;
		station->town->exclusive_counter = 12;
		station->town->exclusivity = CompanyID{1};
		expected = 0;
	}
	SECTION("Exclusive rights allow waiting cargo but cannot revive an unowned hub") {
		station->owner = OWNER_NONE;
		station->town->exclusive_counter = 12;
		station->town->exclusivity = owner;
		waiting = 20;
		expected = 0;
	}
	SECTION("Owned hub is unaffected by another company's town exclusivity") {
		station->town->exclusive_counter = 12;
		station->town->exclusivity = CompanyID{1};
	}
	StockpileManager::AddCargo(WorldID{0}, owner, iron, stock);
	LogisticsHubManager::SetReserveFloor(hub_id, iron, 40);
	if (waiting > 0) {
		REQUIRE(CargoPacket::CanAllocateItem());
		station->goods[iron].GetOrCreateData().cargo.Append(CargoPacket::Create(station->index, waiting, Source{}), StationID::Invalid());
	}
	if (onboard > 0) {
		REQUIRE(CargoPacket::CanAllocateItem());
		CargoPacket *cp = CargoPacket::Create(onboard, 1, StationID::Invalid(), TileXY(5, 5), 0);
		cp->UpdateLoadingTile(TileXY(5, 5));
		train->cargo.Append(cp, VehicleCargoList::MoveToAction::Keep);
	}
	AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, false);
	LoadUnloadStation(station);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == stock - expected);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == expected);
	CHECK(train->cargo.StoredCount() == onboard + waiting + expected);
	CHECK(station->goods[iron].AvailableCount() == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) + train->cargo.StoredCount() + station->goods[iron].AvailableCount() == stock + waiting + onboard);
	if (train->cargo.StoredCount() == 0) CHECK(CargoPacket::GetNumItems() == 0);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	train->cargo.Truncate();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	CHECK(CargoPacket::GetNumItems() == 0);
}

TEST_CASE("Hub loading - failed pickup survives reload and retries", "[hub-loading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CompanyID owner = station->owner;
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	const uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, owner, station->index, "Reload hub");
	REQUIRE(hub_id != 0);
	StockpileManager::AddCargo(WorldID{0}, owner, iron, 100);
	LogisticsHubManager::SetReserveFloor(hub_id, iron, 40);
	Train *train = PrepareHubPickup(station, iron);
	{
		ScopedCargoPacketCapacity full_pool(0);
		LoadUnloadStation(station);
	}
	station = ReloadHubPickup(station, train);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == 100);
	REQUIRE(LogisticsHubManager::GetHub(hub_id) != nullptr);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 0);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, iron) == 40);
	CHECK(station->goods[iron].AvailableCount() == 0);
	train = PrepareHubPickup(station, iron);
	AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, false);
	LoadUnloadStation(station);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == 40);
	CHECK(train->cargo.StoredCount() == 60);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 60);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	train->cargo.Truncate();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	CHECK(CargoPacket::GetNumItems() == 0);
}

TEST_CASE("Hub loading - failed packet split keeps cargo available", "[hub-loading][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	const CompanyID owner = station->owner;
	const CargoType iron = GetCargoTypeByLabel(CT_IRON_ORE);
	const uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, owner, station->index, "Split hub");
	REQUIRE(hub_id != 0);
	StockpileManager::AddCargo(WorldID{0}, owner, iron, 100);
	LogisticsHubManager::SetReserveFloor(hub_id, iron, 40);
	Train *train = PrepareHubPickup(station, iron);
	{
		AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, true);
		AutoRestoreBackup load_amount(Engine::Get(train->engine_type)->info.load_amount, uint8_t{10});
		/* The withdrawal packet fits; the subsequent gradual-loading split does not. */
		ScopedCargoPacketCapacity last_slot(1);
		LoadUnloadStation(station);
		CHECK(_cargopacket_pool.items == CargoPacketPool::MAX_SIZE);
	}
	CHECK(CargoPacket::GetNumItems() == 1);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == 40);
	CHECK(station->goods[iron].AvailableCount() == 60);
	CHECK(train->cargo.StoredCount() == 0);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 60);
	station = ReloadHubPickup(station, train);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == 40);
	CHECK(station->goods[iron].AvailableCount() == 60);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 60);
	train = PrepareHubPickup(station, iron);
	AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, false);
	LoadUnloadStation(station);
	CHECK(train->cargo.StoredCount() == 60);
	CHECK(station->goods[iron].AvailableCount() == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, owner, iron) == 40);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 60);
	station->loading_vehicles.clear();
	_cargo_payment_pool.CleanPool();
	train->cargo.Truncate();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	CHECK(CargoPacket::GetNumItems() == 0);
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

TEST_CASE("WP11 furnace research denial is atomic for query and execution", "[wp11][production-gameplay]")
{
	Station *station = SetupProductionGameplay();
	TechTreeManager::Reset();
	Money cash = Company::Get(station->owner)->money;
	for (DoCommandFlags flags : {DoCommandFlags{}, DoCommandFlags{DoCommandFlag::Execute}}) {
		CHECK(Command<Commands::BuildProcessingFacility>::Do(flags, station->index, RECIPE_STEEL_SMELTING).GetErrorMessage() == STR_ERROR_COMMONWEALTH_RESEARCH);
		CHECK(ProductionChainManager::GetFacilityForStation(station->index) == nullptr);
		CHECK(Company::Get(station->owner)->money == cash);
	}
	TechTreeManager::RestoreCompanyTech(station->owner, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do({}, station->index, RECIPE_STEEL_SMELTING).Succeeded());
	CHECK(ProductionChainManager::GetFacilityForStation(station->index) == nullptr);
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING).Succeeded());
	CHECK(Company::Get(station->owner)->money == cash - 100000);
	CHECK(ProductionChainManager::GetFacilityForStation(station->index) != nullptr);
}
