/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_spaceport_conduit_bridge.cpp Unit tests for Sprint 20 Planetary Infrastructure Integration. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/spaceport_manager.h"
#include "../portal/edge_conduit.h"
#include "../portal/portal_cmd.h"
#include "../portal/portal_registry.h"
#include "../portal/universe_authority.h"
#include "../station_base.h"
#include "../town.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../landscape_cmd.h"
#include "../vehicle_base.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../water_map.h"
#include "../tunnelbridge_map.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "mock_environment.h"

#include "../safeguards.h"

static void SetupSprint20Environment(uint32_t map_w = 128, uint32_t map_h = 128)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	PlanetManager::Reset();
	PortalRegistry::Reset();
	SpaceportManager::Reset();
	EdgeConduitManager::Reset();
	UniverseAuthorityService::Instance().Reset();

	_station_pool.CleanPool();
	_town_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);
	_current_company = c->index;
	c->money = 1'000'000'000;
	c->avail_railtypes.Set(RAILTYPE_BEGIN);

	/* World 1: Core World (10..40, 10..40) */
	PlanetRegion w1{
		.id = WorldID{1},
		.name = "Earth Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10,
		.min_y = 10,
		.max_x = 40,
		.max_y = 40,
		.development_score = 10000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w1));

	/* World 2: Developed Refinery World (50..80, 10..40) */
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 50,
		.min_y = 10,
		.max_x = 80,
		.max_y = 40,
		.development_score = 5000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w2));

	/* World 3: Frontier Mining World (90..120, 10..40) */
	PlanetRegion w3{
		.id = WorldID{3},
		.name = "Haven Rim",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 90,
		.min_y = 10,
		.max_x = 120,
		.max_y = 40,
		.development_score = 2000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w3));

	PlanetManager::RebuildSpatialGrid();

	/* Register worlds in Universe Authority */
	auto &auth = UniverseAuthorityService::Instance();
	RegisteredWorld rw1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld rw2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Developed"};
	RegisteredWorld rw3{.world_id = WorldID{3}, .phase = WorldPhase::Phase3_Frontier, .name = "Haven Rim", .address = "127.0.0.1:3981", .description = "Frontier"};
	auth.RegisterWorld(rw1);
	auth.RegisterWorld(rw2);
	auth.RegisterWorld(rw3);

	/* Corridors */
	InterServerRoute r1{.route_id = 1, .source_world = WorldID{3}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1, .transit_duration_ticks = 40};
	InterServerRoute r2{.route_id = 2, .source_world = WorldID{2}, .source_gate_id = 1, .dest_world = WorldID{1}, .dest_gate_id = 1, .transit_duration_ticks = 40};
	InterServerRoute r3{.route_id = 3, .source_world = WorldID{1}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1, .transit_duration_ticks = 40};
	auth.RegisterRoute(r1);
	auth.RegisterRoute(r2);
	auth.RegisterRoute(r3);
}

static StationID CreateTestAirportStation(TileIndex tile, Owner owner, [[maybe_unused]] WorldID world_id)
{
	REQUIRE(Station::CanAllocateItem());
	Station *st = Station::Create(tile);
	st->owner = owner;
	st->AddFacility(StationFacility::Airport, tile);
	st->airport.tile = tile;
	st->airport.w = 3;
	st->airport.h = 3;
	st->airport.type = AT_COMMUTER;
	return st->index;
}

TEST_CASE("Spaceport Bridge - Configuration and Command Execution")
{
	SetupSprint20Environment();

	TileIndex st_tile = TileXY(20, 20); // World 1 (Earth Core)
	StationID sid = CreateTestAirportStation(st_tile, _current_company, WorldID{1});

	/* Certify as spaceport */
	REQUIRE(CmdDesignateSpaceport(DoCommandFlag::Execute, sid).Succeeded());
	REQUIRE(SpaceportManager::IsSpaceport(sid));

	const SpaceportInfo *info = SpaceportManager::GetSpaceport(sid);
	REQUIRE(info != nullptr);
	CHECK(info->target_dest_world == INVALID_WORLD);
	CHECK(!info->auto_dispatch);

	/* Configure spaceport bridge via command */
	CommandCost cmd_res = CmdConfigureSpaceportBridge(DoCommandFlag::Execute, sid, WorldID{2}, 3, true);
	REQUIRE(cmd_res.Succeeded());

	info = SpaceportManager::GetSpaceport(sid);
	CHECK(info->target_dest_world == WorldID{2});
	CHECK(info->target_route_id == 3);
	CHECK(info->auto_dispatch);

	/* Test failure on non-spaceport station */
	StationID normal_sid = CreateTestAirportStation(TileXY(25, 25), _current_company, WorldID{1});
	CommandCost bad_res = CmdConfigureSpaceportBridge(DoCommandFlag::Execute, normal_sid, WorldID{2}, 0, true);
	CHECK(bad_res.Failed());

	/* Test failure on invalid station */
	CHECK(CmdConfigureSpaceportBridge(DoCommandFlag::Execute, StationID::Invalid(), WorldID{2}, 0, true).Failed());
}

TEST_CASE("Spaceport Bridge - Interplanetary Trade Dispatch and Reception")
{
	SetupSprint20Environment();
	auto &auth = UniverseAuthorityService::Instance();

	/* 1. Origin Spaceport on World 1 (Earth Core) */
	TileIndex origin_tile = TileXY(20, 20);
	StationID origin_sid = CreateTestAirportStation(origin_tile, _current_company, WorldID{1});
	REQUIRE(CmdDesignateSpaceport(DoCommandFlag::Execute, origin_sid).Succeeded());
	REQUIRE(CmdConfigureSpaceportBridge(DoCommandFlag::Execute, origin_sid, WorldID{2}, 3, true).Succeeded());

	/* 2. Destination Spaceport on World 2 (Vulcan Forge) */
	TileIndex dest_tile = TileXY(60, 20);
	StationID dest_sid = CreateTestAirportStation(dest_tile, _current_company, WorldID{2});
	REQUIRE(CmdDesignateSpaceport(DoCommandFlag::Execute, dest_sid).Succeeded());

	/* Buffer high-tech goods (CargoType 5) for export */
	SpaceportManager::BufferExportCargo(origin_sid, CargoType{5}, 80);
	const auto *sp_origin = SpaceportManager::GetSpaceport(origin_sid);
	REQUIRE(sp_origin != nullptr);
	CHECK(sp_origin->buffered_export_cargo == 80);

	/* Dispatch trade into interplanetary corridor */
	std::string tx_id = SpaceportManager::DispatchInterplanetaryTrade(origin_sid, 0);
	REQUIRE(!tx_id.empty());

	/* Origin spaceport state */
	sp_origin = SpaceportManager::GetSpaceport(origin_sid);
	CHECK(sp_origin->buffered_export_cargo == 0);
	CHECK(sp_origin->total_interplanetary_dispatched == 80);

	/* Authority state */
	const auto *tx = auth.GetTransfer(tx_id);
	REQUIRE(tx != nullptr);
	CHECK(tx->source_world == WorldID{1});
	CHECK(tx->dest_world == WorldID{2});
	CHECK(tx->total_cargo_units == 80);
	CHECK(tx->state == TransferState::InTransit);

	/* Verify supply chain matrix spaceport throughput */
	auto matrix = auth.GetEmpireSupplyChainMatrix();
	CHECK(matrix.spaceport_throughput_cargo == 80);
	CHECK(matrix.total_interplanetary_cargo == 80);

	/* Verify commodity conservation while in transit */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 80);
	CHECK(audit.total_cargo_in_transit == 80);
	CHECK(audit.total_cargo_completed == 0);
	CHECK(audit.IsConserved());

	/* Simulate consist arrival at World 2 Spaceport */
	REQUIRE(SpaceportManager::ReceiveInterplanetaryConsist(dest_sid, *tx));
	const auto *sp_dest = SpaceportManager::GetSpaceport(dest_sid);
	REQUIRE(sp_dest != nullptr);
	CHECK(sp_dest->total_interplanetary_received == 80);

	/* Goods delivered into destination station waiting pool */
	Station *dest_st = Station::Get(dest_sid);
	CHECK(dest_st->goods[5].TotalCount() == 80);

	/* Confirm transfer completion with Authority */
	auth.ClaimTransfer(tx_id, WorldID{2});
	REQUIRE(auth.ConfirmTransferArrival(tx_id, WorldID{2}, true));
	CHECK(auth.GetTransfer(tx_id)->state == TransferState::Completed);

	/* Final conservation audit */
	audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_completed == 80);
	CHECK(audit.total_cargo_in_transit == 0);
	CHECK(audit.IsConserved());
}

TEST_CASE("Edge Conduit Feeder - Configuration and Direct Inter-World Piping")
{
	SetupSprint20Environment();
	auto &auth = UniverseAuthorityService::Instance();

	/* Place a conduit in World 3 (Haven Rim: 90..120, 10..40) */
	/* Make tile 89, 20 a void tile so tile 90, 20 is on the perimeter facing void WEST (DiagDirection::NW) */
	TileIndex void_tile = TileXY(89, 20);
	MakeVoid(void_tile);

	TileIndex conduit_tile = TileXY(90, 20);
	MakeClear(conduit_tile, ClearGround::Grass, 0);

	ConduitID cid = EdgeConduitManager::RegisterConduit(
		conduit_tile,
		DiagDirection::NE,
		WorldID{3},
		CargoType{2} /* Iron Ore */,
		_current_company,
		60
	);
	REQUIRE(cid != INVALID_CONDUIT);
	REQUIRE(EdgeConduitManager::IsConduitTile(conduit_tile));

	const auto *conduit = EdgeConduitManager::GetConduit(conduit_tile);
	REQUIRE(conduit != nullptr);
	CHECK(!conduit->direct_feeder_enabled);
	CHECK(conduit->target_dest_world == INVALID_WORLD);

	/* Configure direct feeder mode to World 2 (Vulcan Forge) via command */
	CommandCost cmd_res = CmdConfigureEdgeConduitFeeder(DoCommandFlag::Execute, conduit_tile, true, WorldID{2}, 1);
	REQUIRE(cmd_res.Succeeded());

	conduit = EdgeConduitManager::GetConduit(conduit_tile);
	CHECK(conduit->direct_feeder_enabled);
	CHECK(conduit->target_dest_world == WorldID{2});
	CHECK(conduit->target_route_id == 1);

	/* Monthly extraction loop: Haven Rim is Phase 3 Frontier, receives +100% extraction bonus (60 -> 120 units) */
	EdgeConduitManager::ProduceAllConduits();

	conduit = EdgeConduitManager::GetConduit(conduit_tile);
	CHECK(conduit->total_produced == 120);
	CHECK(conduit->total_piped_interplanetary == 120);

	/* Check Universe Authority transfer queue */
	auto transfers = auth.GetAllTransfers();
	REQUIRE(transfers.size() == 1);
	const auto &tx = transfers[0];
	CHECK(tx.source_world == WorldID{3});
	CHECK(tx.dest_world == WorldID{2});
	CHECK(tx.total_cargo_units == 120);
	CHECK(tx.state == TransferState::InTransit);

	/* Check Empire Supply Chain Matrix flows */
	auto matrix = auth.GetEmpireSupplyChainMatrix();
	CHECK(matrix.edge_conduit_throughput_cargo == 120);
	CHECK(matrix.frontier_to_refinery_cargo == 120);
	CHECK(matrix.total_interplanetary_cargo == 120);
	CHECK(matrix.total_tariffs_generated == 1200);

	/* Strict commodity conservation holds */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 120);
	CHECK(audit.total_cargo_in_transit == 120);
	CHECK(audit.IsConserved());

	/* Destination claims and confirms arrival */
	auth.ClaimTransfer(tx.transfer_id, WorldID{2});
	REQUIRE(auth.ConfirmTransferArrival(tx.transfer_id, WorldID{2}, true));

	audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_completed == 120);
	CHECK(audit.total_cargo_in_transit == 0);
	CHECK(audit.IsConserved());

	/* Trade balance: World 3 credited 1200, World 2 debited 1200 */
	auto tb_w3 = auth.GetWorldTradeBalance(WorldID{3});
	auto tb_w2 = auth.GetWorldTradeBalance(WorldID{2});
	CHECK(tb_w3.net_trade_balance_credits == 1200);
	CHECK(tb_w2.net_trade_balance_credits == -1200);
}

TEST_CASE("Planetary Infrastructure - Concurrent Spaceport and Conduit Federation")
{
	SetupSprint20Environment();
	auto &auth = UniverseAuthorityService::Instance();

	/* 1. Edge Conduit on World 3 (Frontier) -> piping 100 units ore to World 2 */
	TileIndex void_tile = TileXY(89, 25);
	MakeVoid(void_tile);
	TileIndex conduit_tile = TileXY(90, 25);
	MakeClear(conduit_tile, ClearGround::Grass, 0);

	EdgeConduitManager::RegisterConduit(conduit_tile, DiagDirection::NE, WorldID{3}, CargoType{2}, _current_company, 50);
	EdgeConduitManager::ConfigureDirectFeeder(conduit_tile, true, WorldID{2}, 1);

	/* 2. Spaceport on World 2 (Developed) -> exporting 60 units goods to World 1 (Core) */
	TileIndex sp2_tile = TileXY(60, 25);
	StationID sp2_sid = CreateTestAirportStation(sp2_tile, _current_company, WorldID{2});
	SpaceportManager::RegisterSpaceport(sp2_sid, WorldID{2}, 2);
	SpaceportManager::ConfigureSpaceportBridge(sp2_sid, WorldID{1}, 2, false);
	SpaceportManager::BufferExportCargo(sp2_sid, CargoType{5}, 60);

	/* Execute conduit extraction (50 * 2 = 100 units) */
	EdgeConduitManager::ProduceAllConduits();

	/* Execute spaceport manual dispatch (60 units) */
	std::string sp_tx = SpaceportManager::DispatchInterplanetaryTrade(sp2_sid, 60);
	REQUIRE(!sp_tx.empty());

	/* Supply Chain Matrix reflects both throughputs */
	auto matrix = auth.GetEmpireSupplyChainMatrix();
	CHECK(matrix.edge_conduit_throughput_cargo == 100);
	CHECK(matrix.spaceport_throughput_cargo == 60);
	CHECK(matrix.frontier_to_refinery_cargo == 100);
	CHECK(matrix.refinery_to_core_cargo == 60);
	CHECK(matrix.total_interplanetary_cargo == 160);

	/* Commodity Audit strictly conserved */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 160);
	CHECK(audit.total_cargo_in_transit == 160);
	CHECK(audit.IsConserved());

	auto detailed = auth.GetDetailedCommodityAudit();
	CHECK(detailed.IsConserved());
}
