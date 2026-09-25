/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation_order_restoration.cpp Unit tests for consist order restoration and round-trip routing. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../company_base.h"
#include "../company_func.h"
#include "../engine_base.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../order_base.h"
#include "../pathfinder/follow_track.hpp"
#include "../portal/consist_materializer.h"
#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/federation_cmd.h"
#include "../portal/federation_identity.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/universe_authority.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../vehicle_base.h"
#include "mock_environment.h"

#include "../safeguards.h"

static void InitTestEngines()
{
	_engine_pool.CleanPool();
	Engine::CreateAtIndex(EngineID{0}, VehicleType::Train, 0);
	Engine::CreateAtIndex(EngineID{1}, VehicleType::Train, 1);
}

TEST_CASE("Consist Snapshot - current_order_index serialization round-trip")
{
	ConsistSnapshot snapshot;
	FederationNamespace ns{0x123456789ABCDEF0ULL, 0x0FEDCBA987654321ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 100};
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 90;

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) snapshot.content_manifest = token_res.token;
	}

	ConsistSnapshotUnit unit;
	unit.engine_type = 0;
	unit.cargo_type = 0;
	unit.cargo_capacity = 60;
	unit.cargo_count = 0;
	snapshot.units.push_back(unit);

	GlobalStationID st1{.name_space = ns, .sequence = 1, .world_id = WorldID{3}};
	GlobalStationID st2{.name_space = ns, .sequence = 2, .world_id = WorldID{2}};
	GlobalStationID st3{.name_space = ns, .sequence = 3, .world_id = WorldID{1}};

	snapshot.orders.push_back(GlobalOrderDestinationID::ForStation(st1, false));
	snapshot.orders.push_back(GlobalOrderDestinationID::ForStation(st2, false));
	snapshot.orders.push_back(GlobalOrderDestinationID::ForStation(st3, false));
	snapshot.current_order_index = 1;

	/* Encode to V2 */
	ConsistSnapshotBytes encoded = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(encoded.Succeeded());

	/* Decode from V2 */
	ConsistSnapshotResult decoded = ConsistSnapshotCodec::Decode(encoded.bytes, snapshot.content_manifest);
	REQUIRE(decoded.Succeeded());
	CHECK(decoded.snapshot->current_order_index == 1);
	CHECK(decoded.snapshot->orders.size() == 3);
	CHECK(decoded.snapshot->orders[0].destination_sequence == 1);
	CHECK(decoded.snapshot->orders[1].destination_sequence == 2);
	CHECK(decoded.snapshot->orders[2].destination_sequence == 3);
	CHECK(*decoded.snapshot == snapshot);
}

TEST_CASE("Federation Identity - Order destination resolution across worlds")
{
	Map::Allocate(64, 64);
	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	_station_pool.CleanPool();
	_company_pool.CleanPool();

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg3{
		.id = WorldID{3},
		.name = "World 3",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63,
	};
	PlanetRegion reg2{
		.id = WorldID{2},
		.name = "World 2",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg3);
	PlanetManager::RegisterRegion(reg2);

	/* Create Station on World 3 */
	TileIndex st3_tile = TileXY(10, 10);
	MakeRailNormal(st3_tile, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *st3 = Station::Create(st3_tile);
	REQUIRE(st3 != nullptr);
	st3->owner = CompanyID{0};
	StationID st3_id = st3->index;
	auto global_st3 = FederationIdentityRegistry::GetOrCreateStation(st3_id);
	REQUIRE(global_st3.has_value());

	/* Create Station on World 2 */
	TileIndex st2_tile = TileXY(45, 10);
	MakeRailNormal(st2_tile, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *st2 = Station::Create(st2_tile);
	REQUIRE(st2 != nullptr);
	st2->owner = CompanyID{0};
	StationID st2_id = st2->index;
	auto global_st2 = FederationIdentityRegistry::GetOrCreateStation(st2_id);
	REQUIRE(global_st2.has_value());

	/* Register inter-server portal link W3 -> W2 */
	TileIndex gate_w3 = TileXY(25, 10);
	MakeRailTunnel(gate_w3, CompanyID{0}, DiagDirection::SW, RAILTYPE_BEGIN);
	PortalRegistry::RegisterInterServerPortal(gate_w3, DiagDirection::SW, WorldID{3}, WorldID{2}, 201, 15);

	GlobalOrderDestinationID ord_w3 = GlobalOrderDestinationID::ForStation(*global_st3, false);
	GlobalOrderDestinationID ord_w2 = GlobalOrderDestinationID::ForStation(*global_st2, false);

	/* On World 3:
	 * ord_w3 (targeting W3) resolves to st3 */
	auto res_local = FederationIdentityRegistry::ResolveOrderDestination(ord_w3, WorldID{3});
	REQUIRE(res_local.has_value());
	CHECK(res_local->ToStationID() == st3_id);

	/* Destination resolution preserves a real station identity; gate routing is a separate adapter. */
	auto res_gate = FederationIdentityRegistry::ResolveOrderDestination(ord_w2, WorldID{3});
	REQUIRE(res_gate.has_value());
	CHECK(res_gate->ToStationID() == st2_id);

	/* ord_w2 evaluated from W2 resolves to st2 */
	auto res_w2 = FederationIdentityRegistry::ResolveOrderDestination(ord_w2, WorldID{2});
	REQUIRE(res_w2.has_value());
	CHECK(res_w2->ToStationID() == st2_id);

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	_station_pool.CleanPool();
	_company_pool.CleanPool();
}

TEST_CASE("Consist Materializer - Restores Orders and Advances Order Index on Arrival")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	PlanetManager::Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
	InitTestEngines();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg2{
		.id = WorldID{2},
		.name = "World 2",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg2);

	/* Set up exit portal gate tile on World 2 */
	TileIndex exit_tile = TileXY(20, 20);
	MakeRailTunnel(exit_tile, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	SetTunnelBridgeReservation(exit_tile, false);
	PortalRegistry::RegisterInterServerPortal(exit_tile, DiagDirection::NE, WorldID{2}, WorldID{3}, 101, 20);

	/* Create destination station on World 2 */
	TileIndex st_tile = TileXY(25, 20);
	MakeRailNormal(st_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *dest_st = Station::Create(st_tile);
	REQUIRE(dest_st != nullptr);
	dest_st->owner = CompanyID{0};
	auto global_dest = FederationIdentityRegistry::GetOrCreateStation(dest_st->index);
	REQUIRE(global_dest.has_value());

	/* Build incoming snapshot with 2 orders:
	 * Order 0: Station on World 3 (Origin)
	 * Order 1: Station on World 2 (Destination)
	 * Current order index was 0 (the origin leg before entering portal) */
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::SW);
	snapshot.speed = 85;
	FederationNamespace ns = FederationIdentityRegistry::GetNamespace();
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 888};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 1};

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) snapshot.content_manifest = token_res.token;
	}

	ConsistSnapshotUnit lead;
	lead.engine_type = 0;
	lead.cargo_capacity = 0;
	lead.cargo_count = 0;
	snapshot.units.push_back(lead);

	ConsistSnapshotUnit wagon;
	wagon.engine_type = 1;
	wagon.subtype = 0;
	wagon.cargo_type = 0;
	wagon.cargo_capacity = 100;
	wagon.cargo_count = 75;
	wagon.cargo_provenance_unresolved = true;
	snapshot.units.push_back(wagon);

	GlobalStationID orig_st_id{.name_space = {ns.high + 1, ns.low}, .sequence = 50, .world_id = WorldID{3}};
	snapshot.orders.push_back(GlobalOrderDestinationID::ForStation(orig_st_id, false));
	snapshot.orders.push_back(GlobalOrderDestinationID::ForStation(*global_dest, false));
	snapshot.current_order_index = 0; // Transited from origin order 0
	const auto before = Vehicle::GetNumItems();
	CHECK_FALSE(ConsistMaterializer::MaterializeFromTransfer(snapshot, exit_tile, DiagDirection::NE).success);
	CHECK(Vehicle::GetNumItems() == before);
	REQUIRE(Station::CanAllocateItem());
	Station *origin_proxy = Station::Create(TileXY(30, 20));
	origin_proxy->owner = CompanyID{0};
	REQUIRE(FederationIdentityRegistry::RestoreStationMapping(origin_proxy->index, 50, orig_st_id.name_space, WorldID{3}));

	/* Materialize consist on World 2 */
	ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(snapshot, exit_tile, DiagDirection::NE);
	REQUIRE(mat_res.success);
	REQUIRE(mat_res.consist != nullptr);

	Train *front = mat_res.consist;
	/* Verify order list was restored */
	REQUIRE(front->orders != nullptr);
	CHECK(front->GetNumOrders() > 0);

	/* Active order index should have advanced to Order 1 (Station on World 2) */
	CHECK(front->cur_real_order_index == 1);
	CHECK(front->current_order.GetType() == OT_GOTO_STATION);
	CHECK(front->current_order.GetDestination().ToStationID() == dest_st->index);

	/* Clean up */
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	PlanetManager::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
}

TEST_CASE("Consist Materializer - End-to-end Autonomous Round-Trip Order Cycling")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	PlanetManager::Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
	InitTestEngines();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg3{
		.id = WorldID{3},
		.name = "World 3",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63,
	};
	PlanetRegion reg2{
		.id = WorldID{2},
		.name = "World 2",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg3);
	PlanetManager::RegisterRegion(reg2);

	/* Station A on World 3 */
	TileIndex st_a_tile = TileXY(10, 10);
	MakeRailNormal(st_a_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *st_a = Station::Create(st_a_tile);
	REQUIRE(st_a != nullptr);
	st_a->owner = CompanyID{0};

	/* Station B on World 2 */
	TileIndex st_b_tile = TileXY(45, 10);
	MakeRailNormal(st_b_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *st_b = Station::Create(st_b_tile);
	REQUIRE(st_b != nullptr);
	st_b->owner = CompanyID{0};

	/* Portals W3 <-> W2 */
	TileIndex gate_w3 = TileXY(25, 10);
	MakeRailTunnel(gate_w3, CompanyID{0}, DiagDirection::SW, RAILTYPE_BEGIN);
	PortalRegistry::RegisterInterServerPortal(gate_w3, DiagDirection::SW, WorldID{3}, WorldID{2}, 200, 10);

	TileIndex gate_w2 = TileXY(35, 10);
	MakeRailTunnel(gate_w2, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	PortalRegistry::RegisterInterServerPortal(gate_w2, DiagDirection::NE, WorldID{2}, WorldID{3}, 100, 10);

	/* Create train consist on World 3 */
	REQUIRE(Vehicle::CanAllocateItem());
	Train *engine = Vehicle::Create<Train>();
	engine->owner = CompanyID{0};
	engine->tile = st_a_tile;
	engine->track = Track::X;
	engine->engine_type = EngineID{0};
	engine->cargo_type = CargoType{0};
	engine->direction = Direction::NE;
	engine->cur_speed = 0;
	engine->SetFrontEngine();
	engine->SetEngine();

	/* Assign round-trip orders */
	REQUIRE(ConsistMaterializer::AssignRoundTripOrders(engine, st_a->index, WorldID{3}, st_b->index, WorldID{2}));
	REQUIRE(engine->orders != nullptr);
	CHECK(engine->GetNumOrders() == 2);
	CHECK(engine->cur_real_order_index == 0);

	/* Step 1: Consist finishes loading at st_a, advances to order 1 (st_b on W2) */
	engine->cur_real_order_index = 1;
	engine->current_order = *engine->GetOrder(1);

	/* Step 2: Consist enters Portal Gate W3 -> Despawn for transfer */
	auto comp_opt = FederationIdentityRegistry::FindCompany(engine->owner);
	GlobalOwnerToken owner_token = comp_opt ? comp_opt->ToOwnerToken() : GlobalOwnerToken{};
	ConsistDespawnResult despawn1 = ConsistMaterializer::DespawnForTransfer(engine, owner_token);
	REQUIRE(despawn1.success);
	CHECK(despawn1.snapshot.orders.size() == 2);
	CHECK(despawn1.snapshot.current_order_index == 1);

	/* Step 3: Materialize at World 2 Exit Gate */
	ConsistMaterializeResult mat1 = ConsistMaterializer::MaterializeFromTransfer(despawn1.snapshot, gate_w2, DiagDirection::NE);
	REQUIRE(mat1.success);
	Train *engine_w2 = mat1.consist;
	REQUIRE(engine_w2 != nullptr);
	REQUIRE(engine_w2->orders != nullptr);
	CHECK(engine_w2->GetNumOrders() == 2);
	/* On World 2, active order is Order 1 (Station B) */
	CHECK(engine_w2->cur_real_order_index == 1);
	CHECK(engine_w2->current_order.GetDestination().ToStationID() == st_b->index);

	/* Step 4: Consist unloads at Station B on World 2, order cycle advances to Order 0 (return to Station A on W3) */
	engine_w2->cur_real_order_index = 0;
	engine_w2->current_order = *engine_w2->GetOrder(0);

	/* Step 5: Consist enters Portal Gate W2 -> Despawn for return transfer */
	ConsistDespawnResult despawn2 = ConsistMaterializer::DespawnForTransfer(engine_w2, owner_token);
	REQUIRE(despawn2.success);
	CHECK(despawn2.snapshot.orders.size() == 2);
	CHECK(despawn2.snapshot.current_order_index == 0);

	/* Step 6: Materialize back at World 3 Exit Gate */
	ConsistMaterializeResult mat2 = ConsistMaterializer::MaterializeFromTransfer(despawn2.snapshot, gate_w3, DiagDirection::SW);
	REQUIRE(mat2.success);
	Train *engine_return = mat2.consist;
	REQUIRE(engine_return != nullptr);
	REQUIRE(engine_return->orders != nullptr);
	CHECK(engine_return->GetNumOrders() == 2);
	/* On World 3, active order is Order 0 (Station A) */
	CHECK(engine_return->cur_real_order_index == 0);
	CHECK(engine_return->current_order.GetDestination().ToStationID() == st_a->index);

	/* Verify GlobalConsistID remained identical throughout the entire round trip */
	auto cid_return = FederationIdentityRegistry::Find(engine_return);
	REQUIRE(cid_return.has_value());
	CHECK(cid_return->sequence == despawn1.snapshot.consist_id.sequence);

	/* Clean up */
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	PlanetManager::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
}
