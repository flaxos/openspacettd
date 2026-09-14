/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint39_corporate_hq_and_stockpile.cpp Unit tests for Sprint 39 Corporate Headquarters and Logistics Hub Stockpiles. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/company_stockpile.h"
#include "../portal/logistics_hub.h"
#include "../portal/corporate_hq.h"
#include "../portal/federation_identity.h"
#include "../portal/federation_player.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../station_base.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 39 Stockpile - Multi-World & Company Ledger Isolation")
{
	StockpileManager::Reset();

	WorldID w0{0};
	WorldID w1{1};
	CompanyID c0{0};
	CompanyID c1{1};
	CargoType cargo_steel{2};
	CargoType cargo_copper{3};

	/* 1. Deposits to isolated buckets */
	StockpileManager::AddCargo(w0, c0, cargo_steel, 500);
	StockpileManager::AddCargo(w0, c1, cargo_steel, 200);
	StockpileManager::AddCargo(w1, c0, cargo_steel, 1000);
	StockpileManager::AddCargo(w0, c0, cargo_copper, 150);

	/* 2. Balance verifications */
	CHECK(StockpileManager::GetStock(w0, c0, cargo_steel) == 500);
	CHECK(StockpileManager::GetStock(w0, c1, cargo_steel) == 200);
	CHECK(StockpileManager::GetStock(w1, c0, cargo_steel) == 1000);
	CHECK(StockpileManager::GetStock(w1, c1, cargo_steel) == 0);
	CHECK(StockpileManager::GetStock(w0, c0, cargo_copper) == 150);

	/* 3. Partial withdrawal */
	uint32_t w_drawn = StockpileManager::WithdrawCargo(w0, c0, cargo_steel, 300);
	CHECK(w_drawn == 300);
	CHECK(StockpileManager::GetStock(w0, c0, cargo_steel) == 200);

	/* Other company / world balances must be untouched */
	CHECK(StockpileManager::GetStock(w0, c1, cargo_steel) == 200);
	CHECK(StockpileManager::GetStock(w1, c0, cargo_steel) == 1000);

	/* 4. Capped withdrawal (requesting more than available) */
	uint32_t capped = StockpileManager::WithdrawCargo(w0, c0, cargo_steel, 500);
	CHECK(capped == 200);
	CHECK(StockpileManager::GetStock(w0, c0, cargo_steel) == 0);

	/* 5. Withdrawing from empty stock returns 0 */
	uint32_t empty_wd = StockpileManager::WithdrawCargo(w0, c0, cargo_steel, 100);
	CHECK(empty_wd == 0);
}

TEST_CASE("Sprint 39 Stockpile - Bill of Materials (BOM) Validation and Consumption")
{
	StockpileManager::Reset();

	WorldID w{2};
	CompanyID c{0};
	CargoType steel{2};
	CargoType wiring{3};
	CargoType chips{4};

	std::map<CargoType, uint32_t> bom = {
		{steel, 100},
		{wiring, 50},
		{chips, 25}
	};

	/* Initially empty: check fails */
	CHECK_FALSE(StockpileManager::HasSufficient(w, c, bom));
	CHECK_FALSE(StockpileManager::ConsumeBOM(w, c, bom));

	/* Partial inventory: check fails */
	StockpileManager::AddCargo(w, c, steel, 150);
	StockpileManager::AddCargo(w, c, wiring, 49); // 1 short
	StockpileManager::AddCargo(w, c, chips, 30);
	CHECK_FALSE(StockpileManager::HasSufficient(w, c, bom));
	CHECK_FALSE(StockpileManager::ConsumeBOM(w, c, bom));

	/* Add missing item to satisfy BOM */
	StockpileManager::AddCargo(w, c, wiring, 1);
	CHECK(StockpileManager::HasSufficient(w, c, bom));

	/* Consume BOM */
	REQUIRE(StockpileManager::ConsumeBOM(w, c, bom));

	/* Verify exact remaining quantities */
	CHECK(StockpileManager::GetStock(w, c, steel) == 50);
	CHECK(StockpileManager::GetStock(w, c, wiring) == 0);
	CHECK(StockpileManager::GetStock(w, c, chips) == 5);

	/* Subsequent consumption must fail due to zero wiring */
	CHECK_FALSE(StockpileManager::HasSufficient(w, c, bom));
	CHECK_FALSE(StockpileManager::ConsumeBOM(w, c, bom));
}

TEST_CASE("Sprint 39 Stockpile - Role Mappings and Deserialization")
{
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast) == CargoType{1});
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal) == CargoType{9});
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring) == CargoType{5});
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics) == CargoType{10});
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::EnrichedCrystals) == CargoType{10});

	StockpileManager::Reset();
	WorldID w{5};
	CompanyID c{2};
	std::map<CargoType, uint32_t> saved_inv = {
		{CargoType{1}, 750},
		{CargoType{3}, 1200}
	};

	StockpileManager::RestoreStockpile(w, c, saved_inv);
	CHECK(StockpileManager::GetStock(w, c, CargoType{1}) == 750);
	CHECK(StockpileManager::GetStock(w, c, CargoType{3}) == 1200);

	auto all_stocks = StockpileManager::GetAllStockpiles();
	REQUIRE(all_stocks.size() == 1);
	CHECK(all_stocks[0].world_id == w);
	CHECK(all_stocks[0].company_id == c);
}

TEST_CASE("Sprint 39 Logistics Hub - Bi-Directional Buffering and Reserve Floors")
{
	StockpileManager::Reset();
	LogisticsHubManager::Reset();

	WorldID world{1};
	CompanyID company{0};
	StationID st{10};
	TileIndex tile = TileXY(40, 40);
	CargoType cargo{2};

	uint32_t hub_id = LogisticsHubManager::RegisterHub(tile, world, company, st, "Foundry Rail Warehouse");
	REQUIRE(hub_id > 0);

	const LogisticsHub *hub = LogisticsHubManager::GetHub(hub_id);
	REQUIRE(hub != nullptr);
	CHECK(hub->name == "Foundry Rail Warehouse");
	CHECK(hub->station_id == st);
	CHECK(LogisticsHubManager::GetHubAtTile(tile) == hub);
	CHECK(LogisticsHubManager::GetHubForStation(st) == hub);

	/* 1. Deposit into warehouse feeds planetary stockpile */
	CHECK(LogisticsHubManager::DepositToStockpile(tile, company, cargo, 300));
	CHECK(StockpileManager::GetStock(world, company, cargo) == 300);
	CHECK(hub->total_deposited == 300);

	/* 2. Set reserve floor to 100 units */
	LogisticsHubManager::SetReserveFloor(hub_id, cargo, 100);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, cargo) == 100);

	/* 3. Withdraw above reserve floor (current: 300, floor: 100 -> available: 200) */
	uint32_t w1 = LogisticsHubManager::WithdrawFromStockpile(tile, company, cargo, 50);
	CHECK(w1 == 50);
	CHECK(StockpileManager::GetStock(world, company, cargo) == 250);
	CHECK(hub->total_dispatched == 50);

	/* 4. Requesting more than available surplus (stock 250, floor 100 -> surplus 150) */
	uint32_t w2 = LogisticsHubManager::WithdrawFromStockpile(tile, company, cargo, 200);
	CHECK(w2 == 150); // Capped by reserve floor!
	CHECK(StockpileManager::GetStock(world, company, cargo) == 100);
	CHECK(hub->total_dispatched == 200);

	/* 5. Requesting withdrawal when stockpile is exactly at reserve floor -> 0 granted */
	uint32_t w3 = LogisticsHubManager::WithdrawFromStockpile(tile, company, cargo, 50);
	CHECK(w3 == 0);
	CHECK(StockpileManager::GetStock(world, company, cargo) == 100);
	CHECK(hub->total_dispatched == 200);

	/* 6. Removal */
	CHECK(LogisticsHubManager::RemoveHub(hub_id));
	CHECK(LogisticsHubManager::GetHub(hub_id) == nullptr);
	CHECK(LogisticsHubManager::GetHubAtTile(tile) == nullptr);
}

TEST_CASE("Sprint 39 Corporate HQ - Placement Rules & Validation")
{
	PlanetManager::Reset();
	CorporateHQManager::Reset();
	_company_pool.CleanPool();
	Map::Allocate(512, 512);

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};
	Company *c = Company::Get(CompanyID{0});
	REQUIRE(c != nullptr);

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
		.development_score = 10000
	};
	PlanetRegion r_developed{
		.id = WorldID{1},
		.name = "Vulcan Foundry",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
		.development_score = 5000
	};
	PlanetRegion r_frontier{
		.id = WorldID{2},
		.name = "Boreas Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
		.development_score = 1500
	};
	PlanetRegion r_expansion{
		.id = WorldID{3},
		.name = "Viridis Wilderness",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::SubTropic,
		.min_x = 310, .min_y = 10, .max_x = 390, .max_y = 90,
		.development_score = 0
	};

	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_developed));
	REQUIRE(PlanetManager::RegisterRegion(r_frontier));
	REQUIRE(PlanetManager::RegisterRegion(r_expansion));

	TileIndex t_void = TileXY(5, 5);
	TileIndex t_core = TileXY(50, 50);
	TileIndex t_dev = TileXY(150, 50);
	TileIndex t_front = TileXY(250, 50);
	TileIndex t_exp = TileXY(350, 50);

	std::string err;

	/* 1. Invalid company */
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID::Invalid(), t_core, err));

	/* 2. Void space placement */
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_void, err));
	CHECK(err.find("within a registered world boundary") != std::string::npos);

	/* 3. Non-Phase 1 world tiles */
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_dev, err));
	CHECK(err.find("Phase 1 Core World") != std::string::npos);
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_front, err));
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_exp, err));

	/* 4. Insufficient capital funds (< 5,000,000 Cr) */
	c->money = 2000000;
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_core, err));
	CHECK(err.find("5,000,000 Cr") != std::string::npos);

	/* 5. Presence check: regions r_core, r_developed, and r_frontier have dev > 0 */
	c->money = 6000000;
	CHECK(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_core, err));

	/* 6. Establishment & single HQ per company rule */
	REQUIRE(CorporateHQManager::RegisterHQ(CompanyID{0}, WorldID{0}, t_core, "Federation Directorate"));
	CHECK(CorporateHQManager::HasHQ(CompanyID{0}));
	CHECK(CorporateHQManager::GetHQ(CompanyID{0}) != nullptr);
	CHECK(CorporateHQManager::GetHQAtTile(t_core) != nullptr);

	/* Duplicate attempt */
	CHECK_FALSE(CorporateHQManager::CanPlaceHQ(CompanyID{0}, t_core, err));
	CHECK(err.find("already has an active Corporate Headquarters") != std::string::npos);
}

TEST_CASE("Sprint 39 Corporate HQ - Tier Advancement & Serialization")
{
	CorporateHQManager::Reset();
	CompanyID comp{0};
	WorldID world{0};
	TileIndex tile = TileXY(25, 25);

	REQUIRE(CorporateHQManager::RegisterHQ(comp, world, tile, "Commonwealth Central"));
	const CorporateHQProfile *hq = CorporateHQManager::GetHQ(comp);
	REQUIRE(hq != nullptr);
	CHECK(hq->tier == CorporateHQTier::RegionalBranch);

	/* Tier 1 -> Tier 2: PlanetaryHQ */
	CHECK(CorporateHQManager::UpgradeHQTier(comp));
	CHECK(hq->tier == CorporateHQTier::PlanetaryHQ);

	/* Tier 2 -> Tier 3: Interstellar */
	CHECK(CorporateHQManager::UpgradeHQTier(comp));
	CHECK(hq->tier == CorporateHQTier::Interstellar);

	/* Tier 3 -> Tier 4: CST_Arcology */
	CHECK(CorporateHQManager::UpgradeHQTier(comp));
	CHECK(hq->tier == CorporateHQTier::CST_Arcology);

	/* Cannot upgrade past CST_Arcology */
	CHECK_FALSE(CorporateHQManager::UpgradeHQTier(comp));
	CHECK(hq->tier == CorporateHQTier::CST_Arcology);

	/* Serialization check */
	auto list = CorporateHQManager::GetAllHQ();
	REQUIRE(list.size() == 1);
	CHECK(list[0].campus_name == "Commonwealth Central");
	CHECK(list[0].tier == CorporateHQTier::CST_Arcology);

	CorporateHQManager::Reset();
	CHECK_FALSE(CorporateHQManager::HasHQ(comp));
	CorporateHQManager::RestoreHQ(list[0]);
	CHECK(CorporateHQManager::HasHQ(comp));
	CHECK(CorporateHQManager::GetHQ(comp)->tier == CorporateHQTier::CST_Arcology);
}

TEST_CASE("Sprint 39 Commands - PlaceCorporateHQ and BuildLogisticsHub")
{
	PlanetManager::Reset();
	CorporateHQManager::Reset();
	LogisticsHubManager::Reset();
	_company_pool.CleanPool();
	Map::Allocate(512, 512);

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};
	Company *c = Company::Get(CompanyID{0});
	REQUIRE(c != nullptr);
	c->money = 10000000;

	PlanetRegion r_core{
		.id = WorldID{0},
		.name = "Earth Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10, .min_y = 10, .max_x = 90, .max_y = 90,
		.development_score = 10000
	};
	PlanetRegion r_dev{
		.id = WorldID{1},
		.name = "Hephaestus",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 110, .min_y = 10, .max_x = 190, .max_y = 90,
		.development_score = 5000
	};
	PlanetRegion r_front{
		.id = WorldID{2},
		.name = "Avalon",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 210, .min_y = 10, .max_x = 290, .max_y = 90,
		.development_score = 1500
	};

	REQUIRE(PlanetManager::RegisterRegion(r_core));
	REQUIRE(PlanetManager::RegisterRegion(r_dev));
	REQUIRE(PlanetManager::RegisterRegion(r_front));

	TileIndex t_void = TileXY(5, 5);
	TileIndex t_core_hq = TileXY(45, 45);
	TileIndex t_dev_hub = TileXY(145, 45);

	/* 1. BuildLogisticsHub command on void tile -> Fails */
	auto res_hub_void = Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, t_void, StationID::Invalid(), "Void Hub");
	CHECK(res_hub_void.Failed());
	CHECK(res_hub_void.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	/* 2. BuildLogisticsHub on valid world tile -> Succeeds */
	auto res_hub = Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, t_dev_hub, StationID::Invalid(), "Hephaestus Hub");
	CHECK(res_hub.Succeeded());
	CHECK(LogisticsHubManager::GetHubAtTile(t_dev_hub) != nullptr);

	/* 3. PlaceCorporateHQ command on non-core world tile -> Fails */
	auto res_hq_dev = Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, t_dev_hub, "Dev HQ");
	CHECK(res_hq_dev.Failed());
	CHECK(res_hq_dev.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_HQ_NOT_CORE_WORLD);

	/* 4. PlaceCorporateHQ on core world tile -> Succeeds */
	auto res_hq = Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, t_core_hq, "Arcology Tower One");
	CHECK(res_hq.Succeeded());
	CHECK(CorporateHQManager::HasHQ(CompanyID{0}));
	CHECK(CorporateHQManager::GetHQ(CompanyID{0})->campus_name == "Arcology Tower One");

	/* 5. Duplicate PlaceCorporateHQ command -> Fails */
	TileIndex t_core_hq2 = TileXY(46, 45);
	auto res_hq_dup = Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, t_core_hq2, "Arcology Tower Two");
	CHECK(res_hq_dup.Failed());
	CHECK(res_hq_dup.GetErrorMessage() == STR_ERROR_ALREADY_HAS_CORPORATE_HQ);
}
