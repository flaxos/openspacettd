/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation_transfer.cpp Unit tests for inter-server transfer lifecycle and commodity conservation. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../company_base.h"
#include "../engine_base.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../pathfinder/follow_track.hpp"
#include "../portal/consist_materializer.h"
#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/federation_cmd.h"
#include "../portal/federation_identity.h"
#include "../portal/portal_registry.h"
#include "../portal/universe_authority.h"
#include "../rail_map.h"
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

static ConsistSnapshot CreateSampleSnapshot(uint32_t cargo_count = 50)
{
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 80;
	snapshot.acceleration = 12;

	FederationNamespace ns{0x1111222233334444ULL, 0x5555666677778888ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 777};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 10};
	snapshot.owner = snapshot.company_id.ToOwnerToken();

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) {
			snapshot.content_manifest = token_res.token;
		}
	}

	ConsistSnapshotUnit engine;
	engine.engine_type = 0;
	engine.cargo_type = 0;
	engine.cargo_capacity = 0;
	engine.cargo_count = 0;
	engine.subtype = 1; // engine
	snapshot.units.push_back(engine);

	ConsistSnapshotUnit wagon;
	wagon.engine_type = 1;
	wagon.cargo_type = 0;
	wagon.cargo_capacity = static_cast<uint16_t>(cargo_count + 50);
	wagon.cargo_count = cargo_count;
	wagon.subtype = 0; // wagon
	wagon.cargo_source.name_space = ns;
	wagon.cargo_source.origin_station.name_space = ns;
	wagon.cargo_source.origin_station.sequence = 5;
	wagon.cargo_source.origin_world = WorldID{1};
	wagon.cargo_source.origin_tile_x = 12;
	wagon.cargo_source.origin_tile_y = 14;
	snapshot.units.push_back(wagon);

	return snapshot;
}

TEST_CASE("Federation Transfer - Inter-Server Portal Registration and Query")
{
	PortalRegistry::Reset();
	REQUIRE(PortalRegistry::GetAllInterServerPortals().empty());

	TileIndex gate_tile = TileIndex{555};
	DiagDirection gate_dir = DiagDirection::NE;
	WorldID local_world = WorldID{1};
	WorldID remote_world = WorldID{2};
	uint32_t remote_gate_id = 99;
	uint32_t virt_len = 150;

	PortalID pid = PortalRegistry::RegisterInterServerPortal(
		gate_tile, gate_dir, local_world,
		remote_world, remote_gate_id, virt_len
	);

	REQUIRE(pid != INVALID_PORTAL);
	REQUIRE(PortalRegistry::GetAllInterServerPortals().size() == 1);
	CHECK(PortalRegistry::IsPortalTile(gate_tile));
	CHECK(PortalRegistry::IsInterServerPortal(gate_tile));
	CHECK(PortalRegistry::GetPortalVirtualLength(gate_tile) == virt_len);

	const InterServerPortalLink *link = PortalRegistry::GetInterServerPortal(gate_tile);
	REQUIRE(link != nullptr);
	CHECK(link->local_endpoint.tile == gate_tile);
	CHECK(link->local_endpoint.enter_dir == gate_dir);
	CHECK(link->local_endpoint.world_id == local_world);
	CHECK(link->remote_world == remote_world);
	CHECK(link->remote_gate_id == remote_gate_id);

	/* Check unregistration */
	CHECK(PortalRegistry::UnregisterInterServerPortal(gate_tile));
	CHECK(PortalRegistry::GetAllInterServerPortals().empty());
	CHECK(!PortalRegistry::IsPortalTile(gate_tile));
	CHECK(!PortalRegistry::IsInterServerPortal(gate_tile));

	PortalRegistry::Reset();
}

TEST_CASE("Federation Transfer - Universe Authority Lifecycle and State Machine")
{
	UniverseAuthorityService &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	/* 1. Register participating worlds */
	RegisteredWorld w1{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Sol-Prime",
	};
	RegisteredWorld w2{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Mars-Colony",
	};

	REQUIRE(authority.RegisterWorld(w1));
	REQUIRE(authority.RegisterWorld(w2));
	CHECK(authority.GetWorlds().size() == 2);

	/* 2. Register route */
	InterServerRoute route{
		.route_id = 1,
		.source_world = WorldID{1},
		.source_gate_id = 10,
		.dest_world = WorldID{2},
		.dest_gate_id = 20,
		.transit_duration_ticks = 200,
	};
	REQUIRE(authority.RegisterRoute(route));
	REQUIRE(authority.FindRoute(WorldID{1}, 10) != nullptr);

	/* 3. Encode sample consist snapshot */
	ConsistSnapshot snapshot = CreateSampleSnapshot(75);
	ConsistSnapshotBytes enc = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(enc.Succeeded());

	/* 4. Initiate transfer */
	std::string tx_id = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, enc, 200
	);
	REQUIRE(!tx_id.empty());

	const UniverseTransferRecord *rec = authority.GetTransfer(tx_id);
	REQUIRE(rec != nullptr);
	CHECK(rec->state == TransferState::Locked);
	CHECK(rec->total_cargo_units == 75);

	/* 5. Attempting to claim before departure must fail */
	auto premature_claim = authority.ClaimTransfer(tx_id, WorldID{2});
	CHECK(!premature_claim.has_value());

	/* 6. Consist departs at tick 1000 */
	REQUIRE(authority.DepartTransfer(tx_id, 1000));
	rec = authority.GetTransfer(tx_id);
	REQUIRE(rec != nullptr);
	CHECK(rec->state == TransferState::InTransit);
	CHECK(rec->departure_tick == 1000);
	CHECK(rec->arrival_tick == 1200);

	/* 7. Query pending before arrival tick */
	std::vector<std::string> pending_early = authority.QueryPendingTransfers(WorldID{2}, 1150);
	CHECK(pending_early.empty());

	/* 8. Query pending at or after arrival tick */
	std::vector<std::string> pending_ready = authority.QueryPendingTransfers(WorldID{2}, 1200);
	REQUIRE(pending_ready.size() == 1);
	CHECK(pending_ready[0] == tx_id);

	/* 9. Claim from wrong destination world */
	auto wrong_claim = authority.ClaimTransfer(tx_id, WorldID{1});
	CHECK(!wrong_claim.has_value());

	/* 10. Claim by receiving destination world */
	auto valid_claim = authority.ClaimTransfer(tx_id, WorldID{2});
	REQUIRE(valid_claim.has_value());
	CHECK(valid_claim->transfer_id == tx_id);
	rec = authority.GetTransfer(tx_id);
	CHECK(rec->state == TransferState::ArrivalPending);

	/* 11. Duplicate claim prevention */
	auto duplicate_claim = authority.ClaimTransfer(tx_id, WorldID{2});
	CHECK(!duplicate_claim.has_value());

	/* 12. Confirm arrival */
	REQUIRE(authority.ConfirmTransferArrival(tx_id, WorldID{2}, true));
	rec = authority.GetTransfer(tx_id);
	CHECK(rec->state == TransferState::Completed);

	/* 13. Completed transfer no longer appears in pending query */
	std::vector<std::string> pending_after = authority.QueryPendingTransfers(WorldID{2}, 1200);
	CHECK(pending_after.empty());

	authority.Reset();
}

TEST_CASE("Federation Transfer - Commodity Conservation Invariant Audit")
{
	UniverseAuthorityService &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Alpha"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase3_Frontier, .name = "Beta"};
	authority.RegisterWorld(w1);
	authority.RegisterWorld(w2);

	/* Prepare 3 transfers with distinct cargo amounts: 100, 200, 300 */
	ConsistSnapshot snap1 = CreateSampleSnapshot(100);
	ConsistSnapshot snap2 = CreateSampleSnapshot(200);
	ConsistSnapshot snap3 = CreateSampleSnapshot(300);

	ConsistSnapshotBytes enc1 = ConsistSnapshotCodec::Encode(snap1);
	ConsistSnapshotBytes enc2 = ConsistSnapshotCodec::Encode(snap2);
	ConsistSnapshotBytes enc3 = ConsistSnapshotCodec::Encode(snap3);

	std::string tx1 = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 2, enc1, 100);
	std::string tx2 = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 2, enc2, 100);
	std::string tx3 = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 2, enc3, 100);

	REQUIRE(!tx1.empty());
	REQUIRE(!tx2.empty());
	REQUIRE(!tx3.empty());

	/* Audit while all transfers are locked/pending */
	CommodityAuditResult audit_init = authority.GetCommodityAudit();
	CHECK(audit_init.total_transfers_initiated == 3);
	CHECK(audit_init.total_cargo_initiated == 600);
	CHECK(audit_init.total_cargo_in_transit == 600);
	CHECK(audit_init.total_cargo_completed == 0);
	CHECK(audit_init.IsConserved());

	/* Depart all 3 */
	authority.DepartTransfer(tx1, 500);
	authority.DepartTransfer(tx2, 500);
	authority.DepartTransfer(tx3, 500);

	/* Claim and complete tx1 */
	authority.ClaimTransfer(tx1, WorldID{2});
	authority.ConfirmTransferArrival(tx1, WorldID{2}, true);

	/* Claim and fail tx2 (requires recovery, counts against in-transit) */
	authority.ClaimTransfer(tx2, WorldID{2});
	authority.ConfirmTransferArrival(tx2, WorldID{2}, false, "Throat blocked");

	/* tx3 remains in transit */
	CommodityAuditResult audit_mid = authority.GetCommodityAudit();
	CHECK(audit_mid.total_transfers_initiated == 3);
	CHECK(audit_mid.total_transfers_completed == 1);
	CHECK(audit_mid.total_cargo_completed == 100);
	CHECK(audit_mid.total_cargo_in_transit == 500); // 200 failed + 300 active in transit
	CHECK(audit_mid.IsConserved());

	authority.Reset();
}

TEST_CASE("Federation Transfer - Consist Despawn for Transfer")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	InitTestEngines();

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	TileIndex gate_tile = TileXY(15, 15);
	MakeRailTunnel(gate_tile, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterInterServerPortal(
		gate_tile, DiagDirection::NE, WorldID{1}, WorldID{2}, 42, 100
	);
	REQUIRE(pid != INVALID_PORTAL);
	SetTunnelBridgeReservation(gate_tile, true);

	/* Create train consist */
	REQUIRE(Vehicle::CanAllocateItem(2));
	Train *engine = Vehicle::Create<Train>();
	engine->SetFrontEngine();
	engine->SetEngine();
	engine->owner = Owner(0);
	engine->engine_type = EngineID{0};
	engine->cargo_type = CargoType{0};
	engine->tile = gate_tile;
	engine->track = Track::Wormhole;
	engine->direction = Direction::NE;
	engine->cur_speed = 70;

	Train *wagon = Vehicle::Create<Train>();
	wagon->ClearFrontEngine();
	wagon->SetWagon();
	wagon->owner = Owner(0);
	wagon->engine_type = EngineID{1};
	wagon->cargo_type = CargoType{0};
	wagon->cargo_cap = 40;
	wagon->tile = gate_tile;
	wagon->track = Track::Wormhole;
	wagon->direction = Direction::NE;
	engine->SetNext(wagon);

	GlobalCompanyID comp_id{
		.name_space = {1, 2},
		.sequence = 1,
	};
	GlobalOwnerToken owner_token = comp_id.ToOwnerToken();

	VehicleID engine_id = engine->index;
	VehicleID wagon_id = wagon->index;

	/* Despawn consist */
	ConsistDespawnResult despawn_res = ConsistMaterializer::DespawnForTransfer(engine, owner_token);
	REQUIRE(despawn_res.success);
	REQUIRE(!despawn_res.snapshot_bytes.bytes.empty());

	/* Verify vehicles were deleted from vehicle pool */
	CHECK(Train::GetIfValid(engine_id) == nullptr);
	CHECK(Train::GetIfValid(wagon_id) == nullptr);

	/* Verify tunnel reservation was released */
	CHECK_FALSE(HasTunnelBridgeReservation(gate_tile));

	/* Verify snapshot can be decoded */
	ConsistSnapshotResult dec_res = ConsistSnapshotCodec::DecodeForCurrentContent(despawn_res.snapshot_bytes.bytes);
	REQUIRE(dec_res.Succeeded());
	CHECK(dec_res.snapshot->units.size() == 2);
	CHECK(dec_res.snapshot->speed == 70);

	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
}

TEST_CASE("Federation Transfer - Consist Materialization on Destination Gate")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	InitTestEngines();

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	TileIndex exit_gate = TileXY(25, 25);
	MakeRailTunnel(exit_gate, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterInterServerPortal(
		exit_gate, DiagDirection::NE, WorldID{2}, WorldID{1}, 10, 100
	);
	REQUIRE(pid != INVALID_PORTAL);

	ConsistSnapshot snapshot = CreateSampleSnapshot(40);

	/* Materialize consist emerging from portal */
	ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(
		snapshot, exit_gate, DiagDirection::NE
	);

	REQUIRE(mat_res.success);
	REQUIRE(mat_res.consist != nullptr);
	CHECK(mat_res.total_cargo == 40);

	Train *emerged_front = mat_res.consist;
	CHECK(emerged_front->IsFrontEngine());
	CHECK(emerged_front->cur_speed == snapshot.speed);
	CHECK(emerged_front->tile == exit_gate);

	/* Consist must have 2 units */
	Train *emerged_wagon = emerged_front->Next();
	REQUIRE(emerged_wagon != nullptr);
	CHECK(emerged_wagon->cargo.StoredCount() == 40);

	/* Exit portal tile reservation must be acquired */
	CHECK(HasTunnelBridgeReservation(exit_gate));

	/* Clean up */
	delete emerged_front;
	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
}

TEST_CASE("Federation Transfer - Obstruction and Manifest Rejection")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	InitTestEngines();

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	TileIndex exit_gate = TileXY(30, 30);
	MakeRailTunnel(exit_gate, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);

	ConsistSnapshot snapshot = CreateSampleSnapshot(20);

	/* 1. Simulate throat obstruction via existing tunnel reservation */
	SetTunnelBridgeReservation(exit_gate, true);
	ConsistMaterializeResult blocked_res = ConsistMaterializer::MaterializeFromTransfer(
		snapshot, exit_gate, DiagDirection::NE
	);
	CHECK(!blocked_res.success);
	CHECK(blocked_res.error_message == "Portal throat is obstructed");

	SetTunnelBridgeReservation(exit_gate, false);

	/* 2. Simulate manifest mismatch */
	ConsistSnapshot bad_manifest_snap = snapshot;
	bad_manifest_snap.content_manifest.fill(0xFF);
	ConsistMaterializeResult manifest_res = ConsistMaterializer::MaterializeFromTransfer(
		bad_manifest_snap, exit_gate, DiagDirection::NE
	);
	CHECK(!manifest_res.success);
	CHECK(manifest_res.error_message == "Universe content manifest mismatch between servers");

	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
}
