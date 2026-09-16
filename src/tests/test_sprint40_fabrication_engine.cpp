/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint40_fabrication_engine.cpp Unit tests for Sprint 40 In-Kind Fabrication Engine & Bill of Materials (BOM). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/company_stockpile.h"
#include "../portal/fabrication_manager.h"
#include "../portal/tech_tree.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../town.h"
#include "../depot_base.h"
#include "../station_base.h"
#include "../rail_map.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../economy_func.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../clear_map.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 40 Fabrication - BOM Recipes & Catalog")
{
	/* 1. Track BOMs */
	auto bom_rail = FabricationManager::GetTrackBOM(RAILTYPE_RAIL);
	CHECK(bom_rail.discount_percent == 80);
	CHECK(bom_rail.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 2);
	CHECK(bom_rail.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 1);

	auto bom_electric = FabricationManager::GetTrackBOM(RAILTYPE_ELECTRIC);
	CHECK(bom_electric.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 2);
	CHECK(bom_electric.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 1);
	CHECK(bom_electric.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 1);

	auto bom_mono = FabricationManager::GetTrackBOM(RAILTYPE_MONO);
	CHECK(bom_mono.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 4);
	CHECK(bom_mono.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 2);
	CHECK(bom_mono.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 1);

	auto bom_maglev = FabricationManager::GetTrackBOM(RAILTYPE_MAGLEV);
	CHECK(bom_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy)) == 2);
	CHECK(bom_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 2);
	CHECK(bom_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics)) == 1);

	/* 2. Signal BOM */
	auto bom_sig = FabricationManager::GetSignalBOM();
	CHECK(bom_sig.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 1);
	CHECK(bom_sig.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 1);

	/* 3. Depot BOMs */
	auto bom_depot_rail = FabricationManager::GetDepotBOM(RAILTYPE_RAIL);
	CHECK(bom_depot_rail.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 10);
	CHECK(bom_depot_rail.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 5);

	auto bom_depot_maglev = FabricationManager::GetDepotBOM(RAILTYPE_MAGLEV);
	CHECK(bom_depot_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy)) == 15);
	CHECK(bom_depot_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 8);
	CHECK(bom_depot_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 4);
	CHECK(bom_depot_maglev.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics)) == 2);

	/* 4. Vehicle BOMs */
	_engine_pool.CleanPool();
	Engine *e_steam = Engine::CreateAtIndex(EngineID{0}, VehicleType::Train, 0);
	REQUIRE(e_steam != nullptr);
	e_steam->VehInfo<RailVehicleInfo>().railveh_type = RailVehicleType::Singlehead;
	e_steam->VehInfo<RailVehicleInfo>().engclass = EngineClass::Steam;
	auto bom_steam = FabricationManager::GetVehicleBOM(e_steam);
	CHECK(bom_steam.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 30);
	CHECK(bom_steam.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 10);

	Engine *e_wagon = Engine::CreateAtIndex(EngineID{1}, VehicleType::Train, 1);
	REQUIRE(e_wagon != nullptr);
	e_wagon->VehInfo<RailVehicleInfo>().railveh_type = RailVehicleType::Wagon;
	e_wagon->VehInfo<RailVehicleInfo>().engclass = EngineClass::Steam;
	auto bom_wagon = FabricationManager::GetVehicleBOM(e_wagon);
	CHECK(bom_wagon.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 10);
	CHECK(bom_wagon.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Composites)) == 2);

	WorldID w0{0};
	CompanyID c0{0};
	StockpileManager::Reset();
	TechTreeManager::Reset();
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	CHECK_FALSE(FabricationManager::CanFabricateVehicle(w0, c0, e_steam));
	StockpileManager::AddCargo(w0, c0, StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal), 30);
	StockpileManager::AddCargo(w0, c0, StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast), 10);
	CHECK(FabricationManager::CanFabricateVehicle(w0, c0, e_steam));
	FabricationManager::ConsumeVehicleBOM(w0, c0, e_steam);
	CHECK(StockpileManager::GetStock(w0, c0, StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 0);
	CHECK(StockpileManager::GetStock(w0, c0, StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 0);
}

TEST_CASE("Sprint 40 Fabrication - Mode Toggling and Company Isolation")
{
	FabricationManager::Reset();
	_company_pool.CleanPool();

	CompanyID c0{0};
	CompanyID c1{1};

	/* Default is false (standard cash) */
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(c0));
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(c1));

	/* Enable for Company 0 only */
	FabricationManager::SetFabricateFromStockpile(c0, true);
	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(c0));
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(c1));

	/* Disable again */
	FabricationManager::SetFabricateFromStockpile(c0, false);
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(c0));

	/* Command integration */
	Company::CreateAtIndex(c0);
	_current_company = c0;

	auto res_cmd = Command<Commands::SetFabricationMode>::Do(DoCommandFlag::Execute, true);
	CHECK(res_cmd.Succeeded());
	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(c0));

	res_cmd = Command<Commands::SetFabricationMode>::Do(DoCommandFlag::Execute, false);
	CHECK(res_cmd.Succeeded());
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(c0));
}

TEST_CASE("Sprint 40 Fabrication - Track Building Interception")
{
	ResetRailTypes();
	PlanetManager::Reset();
	StockpileManager::Reset();
	TechTreeManager::Reset();
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	FabricationManager::Reset();
	_company_pool.CleanPool();
	_town_pool.CleanPool();
	_depot_pool.CleanPool();
	_station_pool.CleanPool();
	Map::Allocate(64, 64);

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};
	Company *c = Company::Get(CompanyID{0});
	REQUIRE(c != nullptr);
	c->money = 5000000;
	c->avail_railtypes.Set(RAILTYPE_BEGIN);
	c->avail_railtypes.Set(RAILTYPE_ELECTRIC);
	c->avail_railtypes.Set(RAILTYPE_MONO);
	c->avail_railtypes.Set(RAILTYPE_MAGLEV);
	c->clear_limit = 1000 << 16;

	_price[Price::BuildRail] = 100;
	_price[Price::BuildSignals] = 50;
	_price[Price::BuildDepotTrain] = 500;

	WorldID w0{0};
	PlanetRegion r_core{
		.id = w0,
		.name = "Terra Nova",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 63, .max_y = 63,
		.development_score = 10000
	};
	REQUIRE(PlanetManager::RegisterRegion(r_core));

	TileIndex t_cash = TileXY(10, 10);
	TileIndex t_fab1 = TileXY(11, 10);
	TileIndex t_fab2 = TileXY(12, 10);

	MakeClear(t_cash, ClearGround::Grass, 0);
	MakeClear(t_fab1, ClearGround::Grass, 0);
	MakeClear(t_fab2, ClearGround::Grass, 0);

	/* 1. Build track with fabrication mode OFF -> Standard cash cost, no stockpile touch */
	FabricationManager::SetFabricateFromStockpile(CompanyID{0}, false);
	auto res_cash = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, t_cash, RAILTYPE_RAIL, Track::X, false);
	CHECK(res_cash.Succeeded());
	Money cash_cost = res_cash.GetCost();
	CHECK(cash_cost > 0);
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 0);

	/* 2. Build track with fabrication mode ON but empty stockpile -> Fails with error! */
	FabricationManager::SetFabricateFromStockpile(CompanyID{0}, true);
	auto res_fail = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, t_fab1, RAILTYPE_RAIL, Track::X, false);
	CHECK(res_fail.Failed());
	CHECK(res_fail.GetErrorMessage() == STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS);

	/* 3. Add materials to stockpile and build track -> Succeeds with 80% discount and BOM deduction! */
	CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	CargoType steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);

	StockpileManager::AddCargo(w0, CompanyID{0}, ballast, 10);
	StockpileManager::AddCargo(w0, CompanyID{0}, steel, 5);

	auto res_fab = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, t_fab1, RAILTYPE_RAIL, Track::X, false);
	CHECK(res_fab.Succeeded());

	/* Track piece cost should be 20% of base rail build cost (80% discount!) */
	Money base_rail_cost = RailBuildCost(RAILTYPE_RAIL);
	CHECK(res_fab.GetCost() == base_rail_cost * 20 / 100);

	/* Verify exact BOM deduction (2 Ballast, 1 Steel) */
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, ballast) == 8);
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, steel) == 4);

	/* 4. Build second piece */
	auto res_fab2 = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, t_fab2, RAILTYPE_RAIL, Track::X, false);
	CHECK(res_fab2.Succeeded());
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, ballast) == 6);
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, steel) == 3);
}

TEST_CASE("Sprint 40 Fabrication - Signal and Depot Interception")
{
	ResetRailTypes();
	PlanetManager::Reset();
	StockpileManager::Reset();
	TechTreeManager::Reset();
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	FabricationManager::Reset();
	_company_pool.CleanPool();
	_town_pool.CleanPool();
	_depot_pool.CleanPool();
	_station_pool.CleanPool();
	Map::Allocate(64, 64);

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};
	Company *c = Company::Get(CompanyID{0});
	REQUIRE(c != nullptr);
	c->money = 5000000;
	c->avail_railtypes.Set(RAILTYPE_BEGIN);
	c->avail_railtypes.Set(RAILTYPE_ELECTRIC);
	c->avail_railtypes.Set(RAILTYPE_MONO);
	c->avail_railtypes.Set(RAILTYPE_MAGLEV);
	c->clear_limit = 1000 << 16;

	_price[Price::BuildRail] = 100;
	_price[Price::BuildSignals] = 50;
	_price[Price::BuildDepotTrain] = 500;

	WorldID w0{0};
	PlanetRegion r_core{
		.id = w0,
		.name = "Terra Nova",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 63, .max_y = 63,
		.development_score = 10000
	};
	REQUIRE(PlanetManager::RegisterRegion(r_core));

	CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	CargoType steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	CargoType wiring = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);

	/* Build track segment for signal testing */
	TileIndex t_sig = TileXY(20, 20);
	MakeClear(t_sig, ClearGround::Grass, 0);
	MakeRailNormal(t_sig, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_RAIL);

	FabricationManager::SetFabricateFromStockpile(CompanyID{0}, true);

	/* 1. Signal with empty stockpile -> Fails */
	auto res_sig_fail = Command<Commands::BuildSignal>::Do(DoCommandFlag::Execute, t_sig, Track::X, SignalType::Block, SignalVariant::Electric, false, false, false, SignalType::Block, SignalType::Block, 0, 0);
	CHECK(res_sig_fail.Failed());
	CHECK(res_sig_fail.GetErrorMessage() == STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS);

	/* Supply signal BOM: 1 Steel + 1 Wiring */
	StockpileManager::AddCargo(w0, CompanyID{0}, steel, 5);
	StockpileManager::AddCargo(w0, CompanyID{0}, wiring, 5);

	auto res_sig_ok = Command<Commands::BuildSignal>::Do(DoCommandFlag::Execute, t_sig, Track::X, SignalType::Block, SignalVariant::Electric, false, false, false, SignalType::Block, SignalType::Block, 0, 0);
	CHECK(res_sig_ok.Succeeded());
	CHECK(res_sig_ok.GetCost() == _price[Price::BuildSignals] * 20 / 100);
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, steel) == 4);
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, wiring) == 4);

	/* 2. Depot with empty stockpile -> Fails */
	TileIndex t_depot = TileXY(25, 20);
	MakeClear(t_depot, ClearGround::Grass, 0);
	auto res_depot_fail = Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, t_depot, RAILTYPE_RAIL, DiagDirection::NE);
	CHECK(res_depot_fail.Failed());
	CHECK(res_depot_fail.GetErrorMessage() == STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS);

	/* Supply depot BOM: 10 Steel + 5 Ballast */
	StockpileManager::AddCargo(w0, CompanyID{0}, steel, 10);
	StockpileManager::AddCargo(w0, CompanyID{0}, ballast, 5);

	auto res_depot_ok = Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, t_depot, RAILTYPE_RAIL, DiagDirection::NE);
	CHECK(res_depot_ok.Succeeded());

	/* Both depot building and track costs discounted by 80% */
	Money expected_depot = (_price[Price::BuildDepotTrain] * 20 / 100) + (RailBuildCost(RAILTYPE_RAIL) * 20 / 100);
	CHECK(res_depot_ok.GetCost() == expected_depot);

	/* Stockpile consumed */
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, ballast) == 0);
	CHECK(StockpileManager::GetStock(w0, CompanyID{0}, steel) == 4); // was 14, now 4
}

TEST_CASE("Sprint 40 Fabrication - Save/Load Serialization")
{
	FabricationManager::Reset();

	FabricationManager::SetFabricateFromStockpile(CompanyID{0}, true);
	FabricationManager::SetFabricateFromStockpile(CompanyID{2}, true);
	FabricationManager::SetFabricateFromStockpile(CompanyID{3}, false);

	auto modes = FabricationManager::GetAllCompanyModes();
	CHECK(modes.size() == 3);
	CHECK(modes[CompanyID{0}] == true);
	CHECK(modes[CompanyID{2}] == true);
	CHECK(modes[CompanyID{3}] == false);

	FabricationManager::Reset();
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{0}));
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{2}));

	for (const auto &[comp, enabled] : modes) {
		FabricationManager::RestoreCompanyMode(comp, enabled);
	}

	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{0}));
	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{2}));
	CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{3}));
}
