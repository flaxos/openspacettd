/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint50_throat_and_staging.cpp Unit tests for Sprint 50 Multi-Track Portal Throats & Automated Staging Loops (WP-50.1 & WP-50.2). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/portal_registry.h"
#include "../portal/prebuilt_trade.h"
#include "../portal/federation_staging.h"
#include "../portal/portal_cmd.h"
#include "../portal/consist_materializer.h"
#include "../station_base.h"
#include "../station_type.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "../train.h"
#include "mock_environment.h"

#include "../safeguards.h"

static void InitTestMap()
{
	(void)MockEnvironment::Instance();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	_station_pool.CleanPool();
	_company_pool.CleanPool();

	Map::Allocate(64, 64);
	for (uint y = 0; y < 64; ++y) {
		for (uint x = 0; x < 64; ++x) {
			TileIndex tile = TileXY(x, y);
			if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
			else MakeVoid(tile);
		}
	}
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	PrebuiltTradeManager::Instance().Reset();
}

TEST_CASE("Sprint 50: WP-50.1 Multi-Track Portal Throats & Clearance Fallback", "[sprint50][portal_throat]")
{
	InitTestMap();

	TileIndex tile_a = TileXY(10, 10);
	TileIndex tile_b = TileXY(20, 20);
	TileIndex parallel_a = TileXY(10, 11);
	TileIndex parallel_b = TileXY(20, 21);

	SECTION("PortalRegistry parallel throat configuration on local wormholes")
	{
		PortalID pid = PortalRegistry::RegisterPortalPair(
			tile_a, DiagDirection::NE, WorldID(1),
			tile_b, DiagDirection::SW, WorldID(2),
			10, true
		);
		REQUIRE(pid != INVALID_PORTAL);

		CHECK_FALSE(PortalRegistry::HasParallelThroat(tile_a));
		CHECK(PortalRegistry::GetParallelThroat(tile_a) == INVALID_TILE);
		CHECK(PortalRegistry::GetThroatTiles(tile_a) == std::vector<TileIndex>{tile_a});

		/* Configure parallel throat for tile_a */
		CHECK(PortalRegistry::ConfigureParallelThroat(tile_a, parallel_a));
		CHECK(PortalRegistry::HasParallelThroat(tile_a));
		CHECK(PortalRegistry::GetParallelThroat(tile_a) == parallel_a);

		auto throats_a = PortalRegistry::GetThroatTiles(tile_a);
		REQUIRE(throats_a.size() == 2);
		CHECK(throats_a[0] == tile_a);
		CHECK(throats_a[1] == parallel_a);

		/* End B is still single-throat until configured */
		CHECK_FALSE(PortalRegistry::HasParallelThroat(tile_b));
		CHECK(PortalRegistry::ConfigureParallelThroat(tile_b, parallel_b));
		CHECK(PortalRegistry::HasParallelThroat(tile_b));
		CHECK(PortalRegistry::GetParallelThroat(tile_b) == parallel_b);
	}

	SECTION("Prebuilt Trade Gateway parallel throat configuration")
	{
		TileIndex trade_tile = TileXY(30, 30);
		TileIndex trade_parallel = TileXY(30, 31);
		REQUIRE(PrebuiltTradeManager::Instance().RegisterTradeGateway(trade_tile, "world_far_away", WorldID(1), 80));

		CHECK(PrebuiltTradeManager::Instance().GetGatewayParallelThroat(trade_tile) == INVALID_TILE);
		CHECK(PrebuiltTradeManager::Instance().ConfigureGatewayParallelThroat(trade_tile, trade_parallel));
		CHECK(PrebuiltTradeManager::Instance().GetGatewayParallelThroat(trade_tile) == trade_parallel);

		/* Configuring on unregistered tile returns false */
		CHECK_FALSE(PrebuiltTradeManager::Instance().ConfigureGatewayParallelThroat(TileXY(5, 5), parallel_a));
	}

	SECTION("ConsistMaterializer::ResolveClearThroat routing and fallback")
	{
		PortalID pid = PortalRegistry::RegisterPortalPair(
			tile_a, DiagDirection::NE, WorldID(1),
			tile_b, DiagDirection::SW, WorldID(2),
			10, true
		);
		REQUIRE(pid != INVALID_PORTAL);
		PortalRegistry::ConfigureParallelThroat(tile_a, parallel_a);

		/* 1. When both primary and parallel are empty, resolves primary */
		TileIndex resolved = ConsistMaterializer::ResolveClearThroat(tile_a, DiagDirection::NE);
		CHECK(resolved == tile_a);

		/* 2. When primary is occupied by a train, falls back to parallel throat */
		REQUIRE(Train::CanAllocateItem());
		Train *train = Train::Create();
		train->tile = tile_a;
		train->UpdatePosition();

		resolved = ConsistMaterializer::ResolveClearThroat(tile_a, DiagDirection::NE);
		CHECK(resolved == parallel_a);

		/* 3. When both primary and parallel throats are occupied, returns INVALID_TILE to prevent collision */
		REQUIRE(Train::CanAllocateItem());
		Train *train_parallel = Train::Create();
		train_parallel->tile = parallel_a;
		train_parallel->UpdatePosition();

		resolved = ConsistMaterializer::ResolveClearThroat(tile_a, DiagDirection::NE);
		CHECK(resolved == INVALID_TILE);

		_vehicle_pool.CleanPool();
		ResetVehicleHash();
	}
}

TEST_CASE("Sprint 50: WP-50.2 Automated Staging Loops & Siding Management", "[sprint50][staging_loops]")
{
	InitTestMap();

	Company *c1 = Company::CreateAtIndex(CompanyID{0});
	c1->money = 1000000;
	Company *c2 = Company::CreateAtIndex(CompanyID{1});
	c2->money = 1000000;
	_current_company = CompanyID{0};

	TileIndex st1_tile = TileXY(15, 15);
	REQUIRE(Station::CanAllocateItem());
	Station *st1 = Station::Create(st1_tile);
	st1->owner = CompanyID{0};
	StationID st1_id = st1->index;

	TileIndex st2_tile = TileXY(25, 25);
	REQUIRE(Station::CanAllocateItem());
	Station *st2 = Station::Create(st2_tile);
	st2->owner = CompanyID{0};
	StationID st2_id = st2->index;
	(void)st2_id;

	SECTION("CmdDesignateHoldingSiding toggles StationFacility::HoldingSiding")
	{
		CHECK_FALSE(st1->facilities.Test(StationFacility::HoldingSiding));

		/* Enable holding siding */
		CommandCost res = CmdDesignateHoldingSiding(DoCommandFlag::Execute, st1_id, true);
		CHECK(res.Succeeded());
		CHECK(st1->facilities.Test(StationFacility::HoldingSiding));

		/* Disable holding siding */
		res = CmdDesignateHoldingSiding(DoCommandFlag::Execute, st1_id, false);
		CHECK(res.Succeeded());
		CHECK_FALSE(st1->facilities.Test(StationFacility::HoldingSiding));

		/* Permission check: company 1 cannot designate company 0's station */
		_current_company = CompanyID{1};
		res = CmdDesignateHoldingSiding(DoCommandFlag::Execute, st1_id, true);
		CHECK(res.Failed());

		/* Invalid station fails */
		res = CmdDesignateHoldingSiding(DoCommandFlag::Execute, StationID{999}, true);
		CHECK(res.Failed());

		_current_company = CompanyID{0};
	}

	SECTION("FederationStagingManager::IsPortalApproachCongested detects train occupancy")
	{
		TileIndex portal_tile = TileXY(20, 20);
		CHECK_FALSE(FederationStagingManager::IsPortalApproachCongested(portal_tile));

		REQUIRE(Train::CanAllocateItem());
		Train *t = Train::Create();
		t->tile = portal_tile;
		t->UpdatePosition();

		CHECK(FederationStagingManager::IsPortalApproachCongested(portal_tile));

		_vehicle_pool.CleanPool();
		ResetVehicleHash();
		CHECK_FALSE(FederationStagingManager::IsPortalApproachCongested(portal_tile));
	}

	SECTION("FederationStagingManager::FindStagingSidingForPortal selects closest designated siding")
	{
		TileIndex portal_tile = TileXY(14, 14);

		/* Neither station is designated yet */
		CHECK(FederationStagingManager::FindStagingSidingForPortal(portal_tile) == INVALID_TILE);

		/* Designate both st1 (dist 2) and st2 (dist 22) */
		st1->facilities.Set(StationFacility::HoldingSiding);
		st2->facilities.Set(StationFacility::HoldingSiding);

		TileIndex best = FederationStagingManager::FindStagingSidingForPortal(portal_tile);
		CHECK(best == st1_tile);

		/* If st1 loses holding siding designation, fallback to st2 */
		st1->facilities.Reset(StationFacility::HoldingSiding);
		best = FederationStagingManager::FindStagingSidingForPortal(portal_tile);
		CHECK(best == st2_tile);
	}

	SECTION("Holding siding reservation and release")
	{
		TileIndex portal_tile = TileXY(14, 14);
		st1->facilities.Set(StationFacility::HoldingSiding);

		PortalRegistry::RegisterInterServerPortal(
			portal_tile, DiagDirection::NE,
			WorldID(1), WorldID(2),
			101, 10
		);
		PortalRegistry::ConfigureStagingSiding(portal_tile, st1_tile);

		REQUIRE(Train::CanAllocateItem());
		Train *train = Train::Create();
		train->owner = CompanyID{0};
		train->tile = portal_tile;
		train->UpdatePosition();

		/* Approach congested because train is on portal */
		CHECK(FederationStagingManager::IsPortalApproachCongested(portal_tile));

		/* Inbound train approaching congested portal is diverted to siding */
		REQUIRE(Train::CanAllocateItem());
		Train *train_inbound = Train::Create();
		train_inbound->owner = CompanyID{0};
		train_inbound->tile = TileXY(13, 13);
		train_inbound->UpdatePosition();

		bool diverted = FederationStagingManager::CheckAndDivertToStaging(train_inbound, portal_tile);
		CHECK(diverted);
		CHECK(FederationStagingManager::IsTrainHeld(train_inbound->index));
		CHECK(FederationStagingManager::GetTotalHeldTrainsCount() == 1);
		CHECK(train_inbound->tile == st1_tile);

		/* Release held trains for portal */
		size_t released = FederationStagingManager::ReleaseHeldTrains(portal_tile);
		CHECK(released == 1);
		CHECK(FederationStagingManager::GetTotalHeldTrainsCount() == 0);
		CHECK_FALSE(FederationStagingManager::IsTrainHeld(train_inbound->index));
	}

	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	_station_pool.CleanPool();
	_company_pool.CleanPool();
}
