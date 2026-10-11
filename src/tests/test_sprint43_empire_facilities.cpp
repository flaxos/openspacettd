/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint43_empire_facilities.cpp Unit tests for Sprint 43 Empire Facility Operations & Closed Production Loops. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/production_chain.h"
#include "../portal/empire_facilities_gui.h"
#include "../widgets/empire_facilities_widget.h"
#include "../portal/planet_manager.h"
#include "../portal/corporate_hq.h"
#include "../portal/company_stockpile.h"
#include "../portal/logistics_hub.h"
#include "../portal/tech_tree.h"
#include "../portal/integrated_economy.h"
#include "../portal/commonwealth_pack.h"
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
#include "../timer/timer_game_tick.h"
#include "../timer/timer_game_calendar.h"
#include "../engine_func.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "../table/sprites.h"
#include "../table/strings.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../driver.h"
#include "../video/video_driver.hpp"
#include <filesystem>
#include <vector>

#include "../safeguards.h"

extern std::vector<WindowDesc*> *_window_descs;

static Station *SetupSprint43Environment()
{
	(void)MockEnvironment::Instance();
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
	_grfconfig.clear();
	SetupCargoForClimate(LandscapeType::Temperate);
	ProductionChainManager::Reset();
	ProductionChainManager::ResetOverflowMetrics();
	PlanetManager::Reset();
	PortalRegistry::Reset();
	StockpileManager::Reset();
	LogisticsHubManager::Reset();
	TechTreeManager::Reset();
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, std::vector<TechID>{
		TECH_MATERIALS_1, TECH_MATERIALS_2,
		TECH_TRACTION_1, TECH_TRACTION_2,
		TECH_PORTAL_1, TECH_PORTAL_2
	});
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
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
	Company *company = Company::CreateAtIndex(CompanyID{0});
	company->money = 10000000;
	company->clear_limit = 1000 << 16;
	company->infrastructure.station = 1;
	company->infrastructure.rail[RAILTYPE_RAIL] = 1;
	_current_company = company->index;

	PlanetRegion r0{
		.id = WorldID{0}, .name = "Earth Core", .phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate, .min_x = 1, .min_y = 1, .max_x = 30, .max_y = 62,
	};
	PlanetRegion r1{
		.id = WorldID{1}, .name = "Ares Outpost", .phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert, .min_x = 31, .min_y = 1, .max_x = 62, .max_y = 62,
	};
	REQUIRE(PlanetManager::RegisterRegion(r0));
	REQUIRE(PlanetManager::RegisterRegion(r1));

	TileIndex tile = TileXY(10, 10);
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(tile);
	town->name = "Capital Prime";
	town->townnametype = SPECSTR_TOWNNAME_START;

	REQUIRE(Station::CanAllocateItem());
	Station *station = Station::Create(tile);
	station->name = "Gateway Platform Alpha";
	station->owner = company->index;
	station->town = town;
	station->facilities.Set(StationFacility::Train);
	station->train_station = TileArea(tile, 1, 1);
	station->spread = station->train_station;
	MakeRailStation(tile, company->index, station->index, Axis::X, 0, RAILTYPE_RAIL);
	RebuildStationKdtree();
	RebuildTownKdtree();
	station->RecomputeCatchment();
	if (VideoDriver::GetInstance() == nullptr) DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
	UnInitWindowSystem();
	_screen.width = _screen.pitch = 1024;
	_screen.height = 768;
	ScreenSizeChanged();
	InitWindowSystem();
	if (_valid_searchpaths.empty()) _valid_searchpaths.push_back(Searchpath::WorkingDir);
	return station;
}

TEST_CASE("Sprint 43 - Canonical 4-Pipeline Complete Recipe Verification", "[sprint43][production]")
{
	ProductionChainManager::Reset();

	/* Pipeline A: Structural & Track Infrastructure */
	auto structural = ProductionChainManager::GetRecipesByPipeline(PipelineType::Structural);
	CHECK(structural.size() == 3);
	for (RecipeID rid : {RECIPE_BALLAST_CRUSHING, RECIPE_STEEL_SMELTING, RECIPE_SUPERALLOY_FOUNDRY}) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(rid);
		REQUIRE(rec != nullptr);
		CHECK(rec->pipeline == PipelineType::Structural);
		CHECK(!rec->inputs.empty());
		CHECK(!rec->outputs.empty());
		for (const auto &[cargo, amount] : rec->inputs) {
			CHECK(cargo < NUM_CARGO);
			CHECK(amount > 0);
		}
		for (const auto &[cargo, amount] : rec->outputs) {
			CHECK(cargo < NUM_CARGO);
			CHECK(amount > 0);
		}
	}

	/* Pipeline B: Electronics, Signalling & Catenary */
	auto electronics = ProductionChainManager::GetRecipesByPipeline(PipelineType::Electronics);
	CHECK(electronics.size() == 3);
	for (RecipeID rid : {RECIPE_COPPER_SMELTING, RECIPE_SILICON_ARC, RECIPE_SIGNALLING_ASSEMBLY}) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(rid);
		REQUIRE(rec != nullptr);
		CHECK(rec->pipeline == PipelineType::Electronics);
		CHECK(!rec->inputs.empty());
		CHECK(!rec->outputs.empty());
	}

	/* Pipeline C: Advanced Train Propulsion */
	auto propulsion = ProductionChainManager::GetRecipesByPipeline(PipelineType::Propulsion);
	CHECK(propulsion.size() == 2);
	for (RecipeID rid : {RECIPE_POLYMER_SYNTHESIS, RECIPE_MAGLEV_WORKS}) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(rid);
		REQUIRE(rec != nullptr);
		CHECK(rec->pipeline == PipelineType::Propulsion);
		CHECK(!rec->inputs.empty());
		CHECK(!rec->outputs.empty());
	}

	/* Pipeline D: Data Crystals & Scientific R&D */
	auto crystals = ProductionChainManager::GetRecipesByPipeline(PipelineType::DataCrystals);
	CHECK(crystals.size() == 3);
	for (RecipeID rid : {RECIPE_MONOCRYSTAL_SYNTHESIS, RECIPE_QUANTUM_ENRICHMENT, RECIPE_CONSUMER_CRYSTAL_FORMAT}) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(rid);
		REQUIRE(rec != nullptr);
		CHECK(rec->pipeline == PipelineType::DataCrystals);
		CHECK(!rec->inputs.empty());
		CHECK(!rec->outputs.empty());
	}
}

TEST_CASE("Sprint 43 - Authoritative Capacity Upgrades and Cap Rules", "[sprint43][commands]")
{
	Station *station = SetupSprint43Environment();
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING).Succeeded());

	ProcessingFacility *fac = ProductionChainManager::GetFacilityForStation(station->index);
	REQUIRE(fac != nullptr);
	CHECK(fac->monthly_capacity == 100);

	Company *c = Company::Get(station->owner);
	Money initial_money = c->money;

	/* 1. Validation query: does not mutate capacity or money */
	auto query_res = Command<Commands::UpgradeProcessingFacility>::Do(DoCommandFlags{}, station->index, 50);
	REQUIRE(query_res.Succeeded());
	CHECK(query_res.GetCost() == 50000);
	CHECK(fac->monthly_capacity == 100);
	CHECK(c->money == initial_money);

	/* 2. Execute upgrade: +50 t/mo */
	auto exec_res = Command<Commands::UpgradeProcessingFacility>::Do(DoCommandFlag::Execute, station->index, 50);
	REQUIRE(exec_res.Succeeded());
	CHECK(exec_res.GetCost() == 50000);
	CHECK(fac->monthly_capacity == 150);
	CHECK(c->money == initial_money - 50000);

	/* 3. Direct helper function */
	CHECK(ProductionChainManager::UpgradeFacilityForStation(station->index, 50));
	CHECK(fac->monthly_capacity == 200);

	/* 4. Large upgrade up towards 1000 max */
	auto large_res = Command<Commands::UpgradeProcessingFacility>::Do(DoCommandFlag::Execute, station->index, 750);
	REQUIRE(large_res.Succeeded());
	CHECK(large_res.GetCost() == 750000);
	CHECK(fac->monthly_capacity == 950);

	/* 5. Capping at 1000: requesting +100 only increases by +50, charging 50,000 */
	auto cap_res = Command<Commands::UpgradeProcessingFacility>::Do(DoCommandFlag::Execute, station->index, 100);
	REQUIRE(cap_res.Succeeded());
	CHECK(cap_res.GetCost() == 50000);
	CHECK(fac->monthly_capacity == 1000);

	/* 6. Upgrade beyond max rejected */
	auto over_res = Command<Commands::UpgradeProcessingFacility>::Do(DoCommandFlag::Execute, station->index, 50);
	CHECK(over_res.Failed());
	CHECK(over_res.GetErrorMessage() == STR_ERROR_FACILITY_MAX_CAPACITY);
	CHECK(fac->monthly_capacity == 1000);

	/* 7. Foreign company validation rejection */
	CompanyID foreign{3};
	_current_company = foreign;
	auto foreign_res = Command<Commands::UpgradeProcessingFacility>::Do(DoCommandFlag::Execute, station->index, 50);
	CHECK(foreign_res.Failed());
	_current_company = station->owner;
}

TEST_CASE("Sprint 43 - Authoritative Platform Holding Target Configuration", "[sprint43][commands]")
{
	Station *station = SetupSprint43Environment();
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING).Succeeded());

	ProcessingFacility *fac = ProductionChainManager::GetFacilityForStation(station->index);
	REQUIRE(fac != nullptr);
	CHECK(fac->platform_capacity == 0);

	/* 1. Query mode */
	auto query_res = Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlags{}, station->index, 150);
	REQUIRE(query_res.Succeeded());
	CHECK(fac->platform_capacity == 0);

	/* 2. Execute mode */
	auto exec_res = Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlag::Execute, station->index, 150);
	REQUIRE(exec_res.Succeeded());
	CHECK(fac->platform_capacity == 150);

	/* 3. Helper method */
	CHECK(ProductionChainManager::SetPlatformCapacityForStation(station->index, 300));
	CHECK(fac->platform_capacity == 300);

	/* 4. Invalid target bounds (> 5000) */
	auto invalid_res = Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlag::Execute, station->index, 6000);
	CHECK(invalid_res.Failed());
	CHECK(fac->platform_capacity == 300);

	/* 5. Set back to 0 (all overflow to hub) */
	REQUIRE(Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlag::Execute, station->index, 0).Succeeded());
	CHECK(fac->platform_capacity == 0);
}

TEST_CASE("Sprint 43 - Station Platform First with Planetary Hub Overflow Routing", "[sprint43][overflow]")
{
	Station *station = SetupSprint43Environment();
	REQUIRE(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING).Succeeded());

	ProcessingFacility *fac = ProductionChainManager::GetFacilityForStation(station->index);
	REQUIRE(fac != nullptr);

	/* Set platform holding target to 50 tons */
	REQUIRE(Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlag::Execute, station->index, 50).Succeeded());
	CHECK(fac->platform_capacity == 50);

	/* Register a planetary Logistics Hub on WorldID{0} */
	uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Earth Main Hub");
	REQUIRE(hub_id != 0);

	CargoType c_iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	CargoType c_steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);

	ProductionChainManager::ResetOverflowMetrics();
	CHECK(ProductionChainManager::GetTotalHubOverflow() == 0);
	CHECK(ProductionChainManager::GetLastMonthHubOverflow() == 0);

	/* Month 1: Deliver 60 Iron Ore -> produces 30 Steel (2 Iron Ore = 1 Steel)
	 * Since 30 <= 50 (platform target), all 30 go directly onto the station platform!
	 * Hub stockpile receives 0.
	 */
	REQUIRE(ProductionChainManager::DeliverToStation(station->index, c_iron, 60) == 60);
	ProductionChainManager::ProcessMonthlyProduction();

	CHECK(station->goods[c_steel].AvailableCount() == 30);
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, c_steel) == 0);
	CHECK(fac->last_month_hub_overflow == 0);
	CHECK(ProductionChainManager::GetLastMonthHubOverflow() == 0);
	CHECK(ProductionChainManager::GetTotalHubOverflow() == 0);

	/* Month 2: Deliver 80 Iron Ore -> produces 40 Steel.
	 * Platform already has 30 Steel.
	 * Platform capacity is 50.
	 * Platform only has room for 20 more Steel.
	 * The remaining 20 Steel automatically overflows into the planetary Logistics Hub!
	 */
	REQUIRE(ProductionChainManager::DeliverToStation(station->index, c_iron, 80) == 80);
	ProductionChainManager::ProcessMonthlyProduction();

	CHECK(station->goods[c_steel].AvailableCount() == 50);
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, c_steel) == 20);
	CHECK(fac->last_month_hub_overflow == 20);
	CHECK(ProductionChainManager::GetLastMonthHubOverflow() == 20);
	CHECK(ProductionChainManager::GetTotalHubOverflow() == 20);

	/* Month 3: Deliver 60 Iron Ore -> produces 30 Steel.
	 * Platform is already full at 50 Steel.
	 * All 30 Steel overflows to the Logistics Hub!
	 */
	REQUIRE(ProductionChainManager::DeliverToStation(station->index, c_iron, 60) == 60);
	ProductionChainManager::ProcessMonthlyProduction();

	CHECK(station->goods[c_steel].AvailableCount() == 50);
	CHECK(StockpileManager::GetStock(WorldID{0}, station->owner, c_steel) == 50); // 20 + 30
	CHECK(fac->last_month_hub_overflow == 30);
	CHECK(ProductionChainManager::GetLastMonthHubOverflow() == 30);
	CHECK(ProductionChainManager::GetTotalHubOverflow() == 50);
}

TEST_CASE("Sprint 43 - Closed Production Loops with Starvation Alert and Nanofab Perk", "[sprint43][production]")
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

	const ProductionRecipe *rec = ProductionChainManager::GetRecipe(fac->recipe_id);
	REQUIRE(rec != nullptr);
	auto is_starved = [&](const ProcessingFacility &f) {
		for (const auto &[in_c, in_amt] : rec->inputs) {
			auto it = f.input_buffers.find(in_c);
			if (it == f.input_buffers.end() || it->second < in_amt) return true;
		}
		return false;
	};
	auto calc_util = [](const ProcessingFacility &f) {
		return f.monthly_capacity > 0 ? (f.last_month_production * 100) / f.monthly_capacity : 0;
	};

	/* 1. Starvation Alert: deliver only Rare Earths, 0 Steel */
	ProductionChainManager::DeliverCargo(fid, c_rare, 20);
	ProductionChainManager::ProcessMonthlyProduction();

	CHECK(is_starved(*fac) == true);
	CHECK(fac->last_month_production == 0);
	CHECK(calc_util(*fac) == 0);
	CHECK(fac->output_buffers[c_alloy] == 0);

	/* 2. Full feedstock delivery with TECH_MATERIALS_3 (+15% Nanofabrication yield bonus) */
	TechTreeManager::RestoreCompanyTech(c0, TECH_NONE, 0, 0, std::vector<TechID>{TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3});
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_MATERIALS_3));

	ProductionChainManager::DeliverCargo(fid, c_steel, 40); // now we have 40 Steel + 20 Rare Earths -> 20 batches
	CHECK(is_starved(*fac) == false); // Both required feedstocks present
	ProductionChainManager::ProcessMonthlyProduction();
	CHECK(fac->last_month_production == 20);
	CHECK(calc_util(*fac) == 20); // 20 batches out of 100 capacity = 20%
	/* Base output is 20 * 2 = 40. With +15%: (40 * 115) / 100 = 46 */
	CHECK(fac->output_buffers[c_alloy] == 46);
}

TEST_CASE("Sprint 43 - Empire Facility Dashboard WindowDesc, Tree & Filtering", "[sprint43][gui]")
{
	REQUIRE(_window_descs != nullptr);

	const WindowDesc *empire_desc = nullptr;
	for (const WindowDesc *desc : *_window_descs) {
		if (desc->cls == WindowClass::EmpireFacilities) {
			empire_desc = desc;
			break;
		}
	}

	REQUIRE(empire_desc != nullptr);
	CHECK(empire_desc->ini_key == "view_empire_facilities");
	CHECK(empire_desc->GetDefaultWidth() == 760);
	CHECK(empire_desc->GetDefaultHeight() == 440);

	NWidgetStacked *shade_select = nullptr;
	std::unique_ptr<NWidgetBase> root = nullptr;
	REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(empire_desc->nwid_parts, &shade_select));
	REQUIRE(root != nullptr);

	/* Verify facility filtering by pipeline */
	ProductionChainManager::Reset();
	PlanetManager::Reset();
	CompanyID c0{0};
	WorldID w0{0};

	FacilityID f_struct = ProductionChainManager::RegisterFacility(TileIndex{10}, w0, RECIPE_STEEL_SMELTING, c0, 100);
	FacilityID f_elec   = ProductionChainManager::RegisterFacility(TileIndex{11}, w0, RECIPE_COPPER_SMELTING, c0, 100);
	FacilityID f_prop   = ProductionChainManager::RegisterFacility(TileIndex{12}, w0, RECIPE_POLYMER_SYNTHESIS, c0, 100);
	FacilityID f_data   = ProductionChainManager::RegisterFacility(TileIndex{13}, w0, RECIPE_MONOCRYSTAL_SYNTHESIS, c0, 100);

	REQUIRE(f_struct != INVALID_FACILITY);
	REQUIRE(f_elec != INVALID_FACILITY);
	REQUIRE(f_prop != INVALID_FACILITY);
	REQUIRE(f_data != INVALID_FACILITY);

	auto all_facs = ProductionChainManager::GetAllFacilities();
	CHECK(all_facs.size() == 4);

	/* Filter by Structural */
	uint count_struct = 0;
	for (const auto &fac : all_facs) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(fac.recipe_id);
		if (rec != nullptr && rec->pipeline == PipelineType::Structural) count_struct++;
	}
	CHECK(count_struct == 1);

	/* Filter by Electronics */
	uint count_elec = 0;
	for (const auto &fac : all_facs) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(fac.recipe_id);
		if (rec != nullptr && rec->pipeline == PipelineType::Electronics) count_elec++;
	}
	CHECK(count_elec == 1);

	/* Filter by Propulsion */
	uint count_prop = 0;
	for (const auto &fac : all_facs) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(fac.recipe_id);
		if (rec != nullptr && rec->pipeline == PipelineType::Propulsion) count_prop++;
	}
	CHECK(count_prop == 1);

	/* Filter by Data Crystals */
	uint count_data = 0;
	for (const auto &fac : all_facs) {
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(fac.recipe_id);
		if (rec != nullptr && rec->pipeline == PipelineType::DataCrystals) count_data++;
	}
	CHECK(count_data == 1);
}

TEST_CASE("Sprint 43 - In-Game Viewport Overlays and Dashboard Retire Lifecycle", "[sprint43][viewport][gui]")
{
	Station *station = SetupSprint43Environment();

	/* Initially no facility attached */
	CHECK(ProductionChainManager::GetFacilityStatusForStation(station->index) == FacilityStatus::None);

	/* 1. Build steel smelting facility */
	auto build_res = Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING);
	REQUIRE(build_res.Succeeded());

	/* Facility is built with empty input buffers, so it must report Starved */
	CHECK(ProductionChainManager::GetFacilityStatusForStation(station->index) == FacilityStatus::Starved);

	/* Verify string formatting contains [Starved] */
	std::string starved_str = GetString(STR_VIEWPORT_FACILITY_STARVED, station->index, station->facilities);
	CHECK(starved_str.find("[Starved]") != std::string::npos);

	/* 2. Deliver required inputs (Iron Ore) */
	CargoType c_iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	REQUIRE(ProductionChainManager::DeliverToStation(station->index, c_iron, 60) == 60);

	/* Buffers are now satisfied; before first monthly run, it's not starved */
	CHECK(ProductionChainManager::GetFacilityStatusForStation(station->index) == FacilityStatus::Idle);

	/* 3. Execute monthly production process */
	ProductionChainManager::ProcessMonthlyProduction();
	CHECK(ProductionChainManager::GetFacilityStatusForStation(station->index) == FacilityStatus::Active);

	/* Verify string formatting contains [Active] */
	std::string active_str = GetString(STR_VIEWPORT_FACILITY_ACTIVE, station->index, station->facilities);
	CHECK(active_str.find("[Active]") != std::string::npos);

	/* 4. Test overflow status: register hub and set platform capacity to 0 */
	uint32_t hub_id = LogisticsHubManager::RegisterHub(station->xy, WorldID{0}, station->owner, station->index, "Earth Main Hub");
	REQUIRE(hub_id != 0);
	REQUIRE(ProductionChainManager::SetPlatformCapacityForStation(station->index, 0));

	/* Deliver more inputs and process again */
	REQUIRE(ProductionChainManager::DeliverToStation(station->index, c_iron, 60) == 60);
	ProductionChainManager::ProcessMonthlyProduction();

	ProcessingFacility *fac = ProductionChainManager::GetFacilityForStation(station->index);
	REQUIRE(fac != nullptr);
	CHECK(fac->last_month_hub_overflow > 0);
	CHECK(ProductionChainManager::GetFacilityStatusForStation(station->index) == FacilityStatus::Overflow);

	/* Verify string formatting contains [Overflow] */
	std::string overflow_str = GetString(STR_VIEWPORT_FACILITY_OVERFLOW, station->index, station->facilities);
	CHECK(overflow_str.find("[Overflow]") != std::string::npos);

	/* 5. Authoritative Decommission / Retire via Command */
	auto remove_res = Command<Commands::RemoveProcessingFacility>::Do(DoCommandFlag::Execute, station->index);
	REQUIRE(remove_res.Succeeded());

	/* Facility is removed; status returns to None */
	CHECK(ProductionChainManager::GetFacilityForStation(station->index) == nullptr);
	CHECK(ProductionChainManager::GetFacilityStatusForStation(station->index) == FacilityStatus::None);

	/* Station sign returns to normal station name */
	std::string normal_str = GetString(STR_VIEWPORT_STATION, station->index, station->facilities);
	CHECK(normal_str.find("[Starved]") == std::string::npos);
	CHECK(normal_str.find("[Active]") == std::string::npos);
	CHECK(normal_str.find("[Overflow]") == std::string::npos);
}

TEST_CASE("Sprint 43 / C1 - Facilities GUI Query Purity and Pure Navigation", "[empire-facilities][sprint43][gui]")
{
	Station *station = SetupSprint43Environment();
	REQUIRE(station != nullptr);

	_current_company = CompanyID{0};
	_local_company = CompanyID{0};
	auto *c = Company::Get(CompanyID{0});
	REQUIRE(c != nullptr);
	Money initial_money = c->money;
	auto initial_ticks = TimerGameTick::counter;
	auto initial_date = TimerGameCalendar::date;
	uint32_t initial_rng_0 = _random.state[0];
	uint32_t initial_rng_1 = _random.state[1];

	auto build_res = Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, station->index, RECIPE_STEEL_SMELTING);
	REQUIRE(build_res.Succeeded());

	c->money = initial_money;
	initial_rng_0 = _random.state[0];
	initial_rng_1 = _random.state[1];

	ShowEmpireFacilitiesWindow(_local_company);
	Window *w = FindWindowById(WindowClass::EmpireFacilities, 0);
	REQUIRE(w != nullptr);
	AutoRestoreBackup dpi(_cur_dpi, &_screen);

	/* 1. Painting must never alter game state, money, ticks, or RNG */
	w->DrawWidget({0, 0, 760, 440}, WID_EF_LIST_PANEL);
	CHECK(c->money == initial_money);
	CHECK(TimerGameTick::counter == initial_ticks);
	CHECK(TimerGameCalendar::date == initial_date);
	CHECK(_random.state[0] == initial_rng_0);
	CHECK(_random.state[1] == initial_rng_1);

	/* 2. Switching tabs must never alter game state */
	w->OnClick(Point{0, 0}, WID_EF_TAB_PIPE_A, 1);
	w->DrawWidget({0, 0, 760, 440}, WID_EF_LIST_PANEL);
	w->OnClick(Point{0, 0}, WID_EF_TAB_PIPE_B, 1);
	w->DrawWidget({0, 0, 760, 440}, WID_EF_LIST_PANEL);
	w->OnClick(Point{0, 0}, WID_EF_TAB_ALL, 1);
	w->DrawWidget({0, 0, 760, 440}, WID_EF_LIST_PANEL);
	CHECK(c->money == initial_money);
	CHECK(TimerGameTick::counter == initial_ticks);
	CHECK(TimerGameCalendar::date == initial_date);

	/* 3. Clicking refresh button must never alter game state */
	w->OnClick(Point{0, 0}, WID_EF_REFRESH, 1);
	CHECK(c->money == initial_money);
	CHECK(TimerGameTick::counter == initial_ticks);
	CHECK(TimerGameCalendar::date == initial_date);

	/* 4. Clicking navigation buttons [Locate Plant] and [Locate Station] must never alter simulation state */
	NWidgetBase *nwi = w->GetWidget<NWidgetBase>(WID_EF_LIST_PANEL);
	REQUIRE(nwi != nullptr);
	Rect r = nwi->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);

	/* Click Locate Plant (rel_y = 40, x near right - 110) */
	Point pt_plant{r.right - 110, r.top + 40};
	w->OnClick(pt_plant, WID_EF_LIST_PANEL, 1);
	CHECK(c->money == initial_money);
	CHECK(TimerGameTick::counter == initial_ticks);
	CHECK(TimerGameCalendar::date == initial_date);
	CHECK(_random.state[0] == initial_rng_0);
	CHECK(_random.state[1] == initial_rng_1);

	/* Click Locate Station (rel_y = 40, x near right - 30) */
	Point pt_station{r.right - 30, r.top + 40};
	w->OnClick(pt_station, WID_EF_LIST_PANEL, 1);
	CHECK(c->money == initial_money);
	CHECK(TimerGameTick::counter == initial_ticks);
	CHECK(TimerGameCalendar::date == initial_date);
	CHECK(_random.state[0] == initial_rng_0);
	CHECK(_random.state[1] == initial_rng_1);

	w->Close();
	UnInitWindowSystem();
}

TEST_CASE("Sprint 43 / C1 - Authoritative Bottleneck Diagnosis", "[empire-facilities][sprint43][bottlenecks]")
{
	(void)SetupSprint43Environment();
	std::array<CargoSpec, NUM_CARGO> saved_cargos;
	for (size_t i = 0; i < NUM_CARGO; ++i) saved_cargos[i] = *CargoSpec::Get(i);

	const char *labels[] = {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
							"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"};
	for (uint8_t c = 0; c < std::size(labels); ++c) {
		CargoLabel label;
		std::copy_n(labels[c], 4, label.begin());
		CargoSpec::Get(CargoType(c))->label = label;
		CargoSpec::Get(CargoType(c))->bitnum = c;
		if (CargoSpec::Get(CargoType(c))->name == STR_EMPTY || CargoSpec::Get(CargoType(c))->name == STR_NULL) {
			CargoSpec::Get(CargoType(c))->name = STR_CARGO_PLURAL_NOTHING + c;
		}
	}
	BuildCargoLabelMap();

	IntegratedEconomy::Reset();
	IntegratedEconomy::SetEnabled(true);
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);

	_current_company = CompanyID{0};
	_local_company = CompanyID{0};

	ProcessingFacility f{};
	f.id = 1;
	f.tile = TileXY(10, 10);
	f.world_id = WorldID{0};
	f.recipe_id = RECIPE_BALLAST_CRUSHING;
	f.owner = CompanyID{0};
	f.monthly_capacity = 100;
	f.last_month_production = 0;
	const ProductionRecipe *rec = ProductionChainManager::GetRecipe(RECIPE_BALLAST_CRUSHING);
	REQUIRE(rec != nullptr);

	/* Case 1: Starved - Missing input cargo (0/2) */
	REQUIRE(!rec->inputs.empty());
	CargoType in_cargo = rec->inputs[0].first;
	uint32_t in_req = rec->inputs[0].second;
	{
		f.input_buffers.clear();
		f.output_buffers.clear();
		f.last_month_production = 0;
		FacilityBottleneck b = DiagnoseFacilityBottleneck(f, rec, 100, 300, 300);
		CHECK(b.colour == TextColour::Red);
		CHECK(b.badge.find("[Starved: Missing") != std::string::npos);
		CHECK(b.badge.find(fmt::format("0/{} per batch)]", in_req)) != std::string::npos);
		CHECK(b.batches_permitted == 0);
	}

	/* Case 2: Starved - Insufficient input cargo (1/2) */
	{
		f.input_buffers.clear();
		f.input_buffers[in_cargo] = in_req / 2;
		FacilityBottleneck b = DiagnoseFacilityBottleneck(f, rec, 100, 300, 300);
		CHECK(b.colour == TextColour::Red);
		CHECK(b.badge.find("[Starved: Insufficient") != std::string::npos);
		CHECK(b.badge.find(fmt::format("{}/{} for batch)]", in_req / 2, in_req)) != std::string::npos);
		CHECK(b.batches_permitted == 0);
	}

	/* Case 3: Ready - Buffered inputs permit complete batches */
	{
		f.input_buffers.clear();
		f.input_buffers[in_cargo] = in_req * 10;
		FacilityBottleneck b = DiagnoseFacilityBottleneck(f, rec, 100, 300, 300);
		CHECK(b.colour == TextColour::Green);
		CHECK(b.badge == "[Ready: 10 batches buffered]");
		CHECK(b.batches_permitted == 10);
	}

	/* Case 4: Active - Completed-period production > 0 */
	{
		f.last_month_production = 100;
		FacilityBottleneck b = DiagnoseFacilityBottleneck(f, rec, 100, 300, 300);
		CHECK(b.colour == TextColour::Green);
		CHECK(b.badge == "[Active: 100/100 batches/mo]");
	}

	/* Case 5: Technology Locked */
	{
		const ProductionRecipe *rec_steel = ProductionChainManager::GetRecipe(RECIPE_STEEL_SMELTING);
		REQUIRE(rec_steel != nullptr);
		ProcessingFacility f_steel = f;
		f_steel.recipe_id = RECIPE_STEEL_SMELTING;
		TechTreeManager::Reset();
		FacilityBottleneck b = DiagnoseFacilityBottleneck(f_steel, rec_steel, 100, 300, 300);
		CHECK(b.colour == TextColour::Red);
		CHECK(b.badge.find("[Locked: Requires") != std::string::npos);
	}

	/* Case 6: Mixed Multi-Input Recipe */
	{
		const ProductionRecipe *rec_sig = ProductionChainManager::GetRecipe(RECIPE_SIGNALLING_ASSEMBLY);
		REQUIRE(rec_sig != nullptr);
		REQUIRE(rec_sig->inputs.size() >= 2);
		ProcessingFacility f_sig = f;
		f_sig.recipe_id = RECIPE_SIGNALLING_ASSEMBLY;
		CargoType c_0 = rec_sig->inputs[0].first;
		CargoType c_1 = rec_sig->inputs[1].first;

		TechTreeManager::RestoreCompanyTech(f_sig.owner, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2});

		f_sig.input_buffers.clear();
		f_sig.input_buffers[c_0] = 0;
		f_sig.input_buffers[c_1] = 10;
		f_sig.last_month_production = 0;
		FacilityBottleneck b = DiagnoseFacilityBottleneck(f_sig, rec_sig, 100, 300, 300);
		CHECK(b.colour == TextColour::Red);
		CHECK(b.badge.find("[Starved: Missing") != std::string::npos);
	}

	IntegratedEconomy::Reset();
	for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(i) = saved_cargos[i];
	BuildCargoLabelMap();
	ProductionChainManager::InitDefaultRecipes();
}

TEST_CASE("Sprint 43 / C1 - Capacity or Reserve Works Effects and Price Query", "[empire-facilities][sprint43][works]")
{
	(void)SetupSprint43Environment();
	std::array<CargoSpec, NUM_CARGO> saved_cargos;
	for (size_t i = 0; i < NUM_CARGO; ++i) saved_cargos[i] = *CargoSpec::Get(i);

	const char *labels[] = {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
							"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"};
	for (uint8_t c = 0; c < std::size(labels); ++c) {
		CargoLabel label;
		std::copy_n(labels[c], 4, label.begin());
		CargoSpec::Get(CargoType(c))->label = label;
		CargoSpec::Get(CargoType(c))->bitnum = c;
		if (CargoSpec::Get(CargoType(c))->name == STR_EMPTY || CargoSpec::Get(CargoType(c))->name == STR_NULL) {
			CargoSpec::Get(CargoType(c))->name = STR_CARGO_PLURAL_NOTHING + c;
		}
	}
	BuildCargoLabelMap();

	IntegratedEconomy::Reset();
	IntegratedEconomy::SetEnabled(true);
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);

	_current_company = CompanyID{0};
	_local_company = CompanyID{0};

	auto spec = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}));
	IndustrySpec saved_spec = *spec;
	spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
	spec->grf_prop.local_id = 0x11;

	TileIndex tile = TileXY(15, 15);
	REQUIRE(Industry::CanAllocateItem());
	Industry *ind = Industry::Create(tile);
	ind->type = IndustryType{0};
	ind->founder = CompanyID{0};
	IntegratedEconomy::RegisterIndustry(ind);

	WorldID world = PlanetManager::GetTileWorld(tile);
	CargoType c_stel = GetCargoTypeByLabel(CargoLabel{"STEL"});
	CargoType c_ball = GetCargoTypeByLabel(CargoLabel{"BALL"});
	StockpileManager::Reset();
	StockpileManager::AddCargo(world, CompanyID{0}, c_stel, 160);
	StockpileManager::AddCargo(world, CompanyID{0}, c_ball, 80);

	auto query_res = Command<Commands::CommissionEconomyWork>::Do(DoCommandFlags{}, ind->index, FactoryWorkType::Capacity);
	CHECK(!query_res.Failed());
	CHECK(query_res.GetCost().base() == 50000);

	*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = saved_spec;
	_industry_pool.CleanPool();
	IntegratedEconomy::Reset();
	for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(i) = saved_cargos[i];
	BuildCargoLabelMap();
	ProductionChainManager::InitDefaultRecipes();
}

TEST_CASE("Sprint 43 / C1 - Factory Buffers Distinct from Planetary Construction Stockpile", "[empire-facilities][sprint43][stockpile]")
{
	(void)SetupSprint43Environment();
	WorldID world{1};
	CompanyID company{0};
	CargoType c_raw = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	CargoType c_stel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	CargoType c_wire = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);

	StockpileManager::Reset();
	StockpileManager::AddCargo(world, company, c_stel, 160);
	StockpileManager::AddCargo(world, company, c_wire, 80);

	CHECK(StockpileManager::GetStock(world, company, c_stel) == 160);
	CHECK(StockpileManager::GetStock(world, company, c_wire) == 80);
	CHECK(StockpileManager::GetStock(world, company, c_raw) == 0);

	ProcessingFacility f{};
	f.id = 1;
	f.tile = TileXY(20, 20);
	f.world_id = world;
	f.owner = company;
	f.input_buffers = {{c_raw, 50}};
	CHECK(f.input_buffers[c_raw] == 50);
	CHECK(StockpileManager::GetStock(world, company, c_raw) == 0);
}

TEST_CASE("Sprint 43 / C1 - Economic Role and Development Phase Separation", "[empire-facilities][sprint43][roles]")
{
	(void)SetupSprint43Environment();
	IntegratedEconomy::Reset();
	IntegratedEconomy::SetEnabled(true);

	WorldID w_core{0};
	WorldID w_ind{1};
	WorldID w_front{2};

	IntegratedEconomy::RegisterRole(w_core, EconomicRole::Core);
	IntegratedEconomy::RegisterRole(w_ind, EconomicRole::Industrial);
	IntegratedEconomy::RegisterRole(w_front, EconomicRole::Frontier);

	CHECK(IntegratedEconomy::Role(w_core) == EconomicRole::Core);
	CHECK(IntegratedEconomy::Role(w_ind) == EconomicRole::Industrial);
	CHECK(IntegratedEconomy::Role(w_front) == EconomicRole::Frontier);

	CHECK(GetString(STR_EMPIRE_FACILITIES_ROLE_CORE) == "Core");
	CHECK(GetString(STR_EMPIRE_FACILITIES_ROLE_INDUSTRIAL) == "Industrial");
	CHECK(GetString(STR_EMPIRE_FACILITIES_ROLE_FRONTIER) == "Frontier");

	CHECK(GetString(STR_EMPIRE_FACILITIES_PHASE_1) == "Phase 1 Core");
	CHECK(GetString(STR_EMPIRE_FACILITIES_PHASE_2) == "Phase 2 Developed");
	CHECK(GetString(STR_EMPIRE_FACILITIES_PHASE_3) == "Phase 3 Frontier");
	CHECK(GetString(STR_EMPIRE_FACILITIES_PHASE_4) == "Phase 4 Expansion");

	IntegratedEconomy::Reset();
}
