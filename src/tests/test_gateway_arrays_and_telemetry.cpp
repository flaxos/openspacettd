/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_gateway_arrays_and_telemetry.cpp Unit tests for Twin Gateway Arrays, In-Transit Telemetry, Upkeep Economics, and Viewport Navigation. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../tunnelbridge_map.h"
#include "../rail_map.h"
#include "../company_base.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/universe_authority.h"
#include "../portal/content_manifest.h"
#include "../portal/consist_snapshot.h"
#include "mock_environment.h"

#include "../safeguards.h"

static ConsistSnapshotBytes CreateTestConsistBytes(uint32_t cargo_units = 120, FreightPriority priority = FreightPriority::Standard)
{
	(void)priority;
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 100;
	snapshot.acceleration = 15;

	FederationNamespace ns{0xABCD1234EF567890ULL, 0x098765FEDCBA4321ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 5001};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 1};
	snapshot.owner = snapshot.company_id.ToOwnerToken();

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) {
			snapshot.content_manifest = token_res.token;
		}
	}

	/* Locomotive */
	ConsistSnapshotUnit engine;
	engine.engine_type = 0;
	engine.cargo_type = 0;
	engine.cargo_capacity = 0;
	engine.cargo_count = 0;
	engine.subtype = 1;
	snapshot.units.push_back(engine);

	/* Cargo Wagon */
	ConsistSnapshotUnit wagon;
	wagon.engine_type = 1;
	wagon.cargo_type = 1;
	wagon.cargo_capacity = static_cast<uint16_t>(cargo_units + 50);
	wagon.cargo_count = cargo_units;
	wagon.subtype = 0;
	wagon.cargo_source.name_space = ns;
	wagon.cargo_source.origin_station.name_space = ns;
	wagon.cargo_source.origin_station.sequence = 1;
	wagon.cargo_source.origin_world = WorldID{1};
	wagon.cargo_source.origin_tile_x = 10;
	wagon.cargo_source.origin_tile_y = 20;
	snapshot.units.push_back(wagon);

	return ConsistSnapshotCodec::Encode(snapshot);
}

TEST_CASE("Twin Gateway Arrays - Spatial Recognition and Pairing")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	_company_pool.CleanPool();

	REQUIRE(Company::CanAllocateItem());
	Company *c1 = Company::Create();

	REQUIRE(Company::CanAllocateItem());
	Company *c2 = Company::Create();

	const DiagDirection dir = DiagDirection::NE;
	const TileIndex gate_a = TileXY(20, 20);
	MakeRailTunnel(gate_a, c1->index, dir, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_a, dir, WorldID{0}));

	/* Twin gate B placed perpendicular to gate A with 1-tile separation */
	const TileIndex gate_b = TileAddByDiagDir(gate_a, DiagDirection::NW);
	MakeRailTunnel(gate_b, c1->index, dir, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_b, dir, WorldID{0}));

	/* Recognition */
	CHECK(PortalRegistry::IsTwinGateway(gate_a, gate_b));
	CHECK(PortalRegistry::IsTwinGateway(gate_b, gate_a));
	CHECK(PortalRegistry::GetTwinGate(gate_a) == gate_b);
	CHECK(PortalRegistry::GetTwinGate(gate_b) == gate_a);

	/* Gate C placed in-line along entrance axis (facing approach, not perpendicular) */
	const TileIndex gate_c = TileAddByDiagDir(gate_a, DiagDirection::NE);
	MakeRailTunnel(gate_c, c1->index, dir, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_c, dir, WorldID{0}));
	CHECK_FALSE(PortalRegistry::IsTwinGateway(gate_a, gate_c));
	CHECK_FALSE(PortalRegistry::IsTwinGateway(gate_c, gate_a));

	/* Gate D placed with distance > 1 */
	const TileIndex gate_d = TileXY(22, 20);
	MakeRailTunnel(gate_d, c1->index, dir, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_d, dir, WorldID{0}));
	CHECK_FALSE(PortalRegistry::IsTwinGateway(gate_a, gate_d));

	/* Gate E placed with orthogonal/different direction */
	const TileIndex gate_e = TileAddByDiagDir(gate_a, DiagDirection::SE);
	MakeRailTunnel(gate_e, c1->index, DiagDirection::NW, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_e, DiagDirection::NW, WorldID{0}));
	CHECK_FALSE(PortalRegistry::IsTwinGateway(gate_a, gate_e));

	/* Gate F adjacent & perpendicular but owned by a different company */
	const TileIndex gate_f = TileXY(25, 20);
	const TileIndex gate_f_twin = TileAddByDiagDir(gate_f, DiagDirection::NW);
	MakeRailTunnel(gate_f, c1->index, dir, RAILTYPE_BEGIN);
	MakeRailTunnel(gate_f_twin, c2->index, dir, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_f, dir, WorldID{0}));
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_f_twin, dir, WorldID{0}));
	CHECK_FALSE(PortalRegistry::IsTwinGateway(gate_f, gate_f_twin));

	PortalRegistry::Reset();
}

TEST_CASE("Twin Gateway Arrays - Corridor Capacity and Congestion Mitigation")
{
	UniverseAuthorityService &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1;
	w1.world_id = WorldID{1};
	w1.phase = WorldPhase::Phase3_Frontier;
	w1.name = "Mining-Beta";

	RegisteredWorld w2;
	w2.world_id = WorldID{2};
	w2.phase = WorldPhase::Phase1_Core;
	w2.name = "Foundry-Prime";

	REQUIRE(auth.RegisterWorld(w1));
	REQUIRE(auth.RegisterWorld(w2));

	/* Route 1: Standard single-throat corridor (max_active = 2) */
	InterServerRoute r1;
	r1.route_id = 101;
	r1.source_world = WorldID{1};
	r1.source_gate_id = 11;
	r1.dest_world = WorldID{2};
	r1.dest_gate_id = 22;
	r1.transit_duration_ticks = 100;
	r1.max_active_in_transit = 2;
	r1.is_twin_array = false;
	REQUIRE(auth.RegisterRoute(r1));

	/* Route 2: Twin gateway array corridor (max_active = 2 base, effective 4) */
	InterServerRoute r2;
	r2.route_id = 102;
	r2.source_world = WorldID{1};
	r2.source_gate_id = 33;
	r2.dest_world = WorldID{2};
	r2.dest_gate_id = 44;
	r2.transit_duration_ticks = 100;
	r2.max_active_in_transit = 2;
	r2.is_twin_array = true;
	REQUIRE(auth.RegisterRoute(r2));

	/* Dispatch 1 train on route 1 (util = 1/2 = 50% -> Moderate, 1.2x delay) */
	auto snap1 = CreateTestConsistBytes(100);
	std::string tx1 = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 11, 22, snap1, 100, FreightPriority::Standard);
	REQUIRE(!tx1.empty());
	const auto *rec1 = auth.GetTransfer(tx1);
	REQUIRE(rec1 != nullptr);
	CHECK(rec1->effective_transit_ticks == 120); // 100 * 1.2x

	/* Dispatch 1 train on route 2 (util = 1/4 = 25% -> Clear, 1.0x delay due to 2x capacity) */
	auto snap2 = CreateTestConsistBytes(100);
	std::string tx2 = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 33, 44, snap2, 100, FreightPriority::Standard);
	REQUIRE(!tx2.empty());
	const auto *rec2 = auth.GetTransfer(tx2);
	REQUIRE(rec2 != nullptr);
	CHECK(rec2->effective_transit_ticks == 100); // 100 * 1.0x (Clear!)

	/* Dispatch 2 more trains on route 2 (active = 3/4 = 75% -> Moderate) */
	auth.InitiateTransfer(WorldID{1}, WorldID{2}, 33, 44, snap2, 100, FreightPriority::Standard);
	std::string tx3 = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 33, 44, snap2, 100, FreightPriority::Standard);
	const auto *rec3 = auth.GetTransfer(tx3);
	REQUIRE(rec3 != nullptr);
	/* Route 2 has twin array: Moderate penalty (0.2) is halved to 0.1 -> 1.1x */
	CHECK(rec3->effective_transit_ticks == 110);

	auth.Reset();
}

TEST_CASE("In-Transit Consist Telemetry - Querying and Lifecycle Updates")
{
	UniverseAuthorityService &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1;
	w1.world_id = WorldID{1};
	w1.phase = WorldPhase::Phase3_Frontier;
	w1.name = "Outpost-1";

	RegisteredWorld w2;
	w2.world_id = WorldID{2};
	w2.phase = WorldPhase::Phase2_Developed;
	w2.name = "Refinery-2";

	REQUIRE(auth.RegisterWorld(w1));
	REQUIRE(auth.RegisterWorld(w2));

	InterServerRoute r1;
	r1.route_id = 201;
	r1.source_world = WorldID{1};
	r1.source_gate_id = 10;
	r1.dest_world = WorldID{2};
	r1.dest_gate_id = 20;
	r1.transit_duration_ticks = 150;
	r1.max_active_in_transit = 10;
	REQUIRE(auth.RegisterRoute(r1));

	InterServerRoute r2;
	r2.route_id = 202;
	r2.source_world = WorldID{1};
	r2.source_gate_id = 30;
	r2.dest_world = WorldID{2};
	r2.dest_gate_id = 40;
	r2.transit_duration_ticks = 150;
	r2.max_active_in_transit = 10;
	REQUIRE(auth.RegisterRoute(r2));

	/* Telemetry is empty initially */
	CHECK(auth.GetInTransitTransfersForRoute(201).empty());
	CHECK(auth.GetInTransitTransfersForRoute(202).empty());

	/* Dispatch consist on route 1 */
	auto snap_r1 = CreateTestConsistBytes(250, FreightPriority::Express);
	std::string tx_r1 = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 10, 20, snap_r1, 150, FreightPriority::Express);
	REQUIRE(!tx_r1.empty());

	/* Dispatch consist on route 2 */
	auto snap_r2 = CreateTestConsistBytes(400, FreightPriority::Standard);
	std::string tx_r2 = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 30, 40, snap_r2, 150, FreightPriority::Standard);
	REQUIRE(!tx_r2.empty());

	/* Verify route 1 telemetry isolation */
	auto telem1 = auth.GetInTransitTransfersForRoute(201);
	REQUIRE(telem1.size() == 1);
	CHECK(telem1[0].transfer_id == tx_r1);
	CHECK(telem1[0].total_cargo_units == 250);
	CHECK(telem1[0].priority == FreightPriority::Express);
	CHECK(telem1[0].route_id == 201);

	/* Verify route 2 telemetry isolation */
	auto telem2 = auth.GetInTransitTransfersForRoute(202);
	REQUIRE(telem2.size() == 1);
	CHECK(telem2[0].transfer_id == tx_r2);
	CHECK(telem2[0].total_cargo_units == 400);
	CHECK(telem2[0].priority == FreightPriority::Standard);
	CHECK(telem2[0].route_id == 202);

	/* Advance lifecycle: arrive and confirm consist on route 1 */
	REQUIRE(auth.DepartTransfer(tx_r1, 1));
	REQUIRE(auth.ClaimTransfer(tx_r1, WorldID{2}).has_value());
	REQUIRE(auth.ConfirmTransferArrival(tx_r1, WorldID{2}, true));

	/* Route 1 telemetry should now be clear, route 2 remains active */
	CHECK(auth.GetInTransitTransfersForRoute(201).empty());
	CHECK(auth.GetInTransitTransfersForRoute(202).size() == 1);

	auth.Reset();
}

TEST_CASE("Portal Maintenance Economics - Upkeep Fees and Calculation")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	_company_pool.CleanPool();

	REQUIRE(Company::CanAllocateItem());
	Company *c1 = Company::Create();

	REQUIRE(Company::CanAllocateItem());
	Company *c2 = Company::Create();

	/* Company 1 has no portals -> 0 cost */
	CHECK(PortalRegistry::GetCompanyPortalMaintenanceCost(c1->index) == 0);

	/* Register unlinked gate owned by Company 1 */
	const TileIndex unlinked_tile = TileXY(15, 15);
	MakeRailTunnel(unlinked_tile, c1->index, DiagDirection::NE, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(unlinked_tile, DiagDirection::NE, WorldID{0}));

	Money unlinked_cost = PortalRegistry::GetCompanyPortalMaintenanceCost(c1->index);
	CHECK(unlinked_cost > 0); // 450 / 8 = 56

	/* Register linked portal pair owned by Company 1 */
	const TileIndex link_a = TileXY(25, 25);
	const TileIndex link_b = TileXY(35, 35);
	MakeRailTunnel(link_a, c1->index, DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(link_b, c1->index, DiagDirection::SW, RAILTYPE_BEGIN);

	uint32_t virt_len = 10;
	PortalID pid = PortalRegistry::RegisterPortalPair(
		link_a, DiagDirection::NE, WorldID{0},
		link_b, DiagDirection::SW, WorldID{1},
		virt_len, true
	);
	REQUIRE(pid != INVALID_PORTAL);

	Money total_c0 = PortalRegistry::GetCompanyPortalMaintenanceCost(c1->index);
	/* 2 active heads (2 * (450 / 4)) + distance (10 * 50) + unlinked head (450 / 8) */
	CHECK(total_c0 == Money(2 * (450 / 4) + (virt_len * 50) + (450 / 8)));

	/* Company 2 owns nothing -> 0 cost */
	CHECK(PortalRegistry::GetCompanyPortalMaintenanceCost(c2->index) == 0);

	PortalRegistry::Reset();
}

TEST_CASE("Viewport World-Hopping and Gate Resolution")
{
	Map::Allocate(128, 128);
	PortalRegistry::Reset();
	PlanetManager::Reset();

	/* Register world region */
	PlanetRegion reg;
	reg.id = WorldID{1};
	reg.min_x = 40;
	reg.min_y = 40;
	reg.max_x = 80;
	reg.max_y = 80;
	REQUIRE(PlanetManager::RegisterRegion(reg));
	PlanetManager::RebuildSpatialGrid();

	/* Jump to registered world succeeds */
	CHECK(PlanetManager::JumpToPlanet(WorldID{1}));
	/* Jump to invalid world returns false */
	CHECK_FALSE(PlanetManager::JumpToPlanet(WorldID{99}));

	/* Gate tile resolution */
	const TileIndex gate_tile = TileXY(50, 50);
	MakeRailTunnel(gate_tile, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate_tile, DiagDirection::NE, WorldID{1}));

	/* Resolve by direct tile */
	CHECK(PortalRegistry::ResolveGateTile(gate_tile.base()) == gate_tile);
	/* Resolve 0 returns INVALID_TILE */
	CHECK(PortalRegistry::ResolveGateTile(0) == INVALID_TILE);

	/* Link portal and resolve by PortalID */
	const TileIndex gate_remote = TileXY(70, 70);
	MakeRailTunnel(gate_remote, Owner(0), DiagDirection::SW, RAILTYPE_BEGIN);
	PortalID pid = PortalRegistry::RegisterPortalPair(
		gate_tile, DiagDirection::NE, WorldID{1},
		gate_remote, DiagDirection::SW, WorldID{1},
		15, true
	);
	REQUIRE(pid != INVALID_PORTAL);
	CHECK(PortalRegistry::ResolveGateTile(pid.base(), WorldID{1}) == gate_tile);

	PortalRegistry::Reset();
	PlanetManager::Reset();
}
