/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint46_live_federation.cpp Unit tests for Sprint 46 Seamless Multi-Server Live Federation Universe. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../company_base.h"
#include "../company_func.h"
#include "../engine_base.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../order_base.h"
#include "../language.h"
#include "../fileio_func.h"
#include "../strings_func.h"
#include "../pathfinder/follow_track.hpp"
#include "../portal/consist_materializer.h"
#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/federation_cmd.h"
#include "../portal/federation_identity.h"
#include "../portal/federation_staging.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/portal_cmd.h"
#include "../portal/universe_authority.h"
#include "../portal/universe_directory_gui.h"
#include "../widgets/universe_directory_widget.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../vehicle_base.h"
#include "../command_func.h"
#include "../window_func.h"
#include "../gfx_func.h"
#include "mock_environment.h"

#include "../safeguards.h"

extern std::vector<WindowDesc*> *_window_descs;
extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;

static void InitLiveFederationEngines()
{
	_engine_pool.CleanPool();
	Engine::CreateAtIndex(EngineID{0}, VehicleType::Train, 0);
	Engine::CreateAtIndex(EngineID{1}, VehicleType::Train, 1);
}

static ConsistSnapshot CreateFederationTrainSnapshot(uint32_t cargo_count = 60, uint64_t seq = 100)
{
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 85;
	snapshot.acceleration = 14;

	FederationNamespace ns{0xAA11BB22CC33DD44ULL, 0xEE55FF6600771188ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = seq};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 1};
	snapshot.owner = snapshot.company_id.ToOwnerToken();

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) snapshot.content_manifest = token_res.token;
	}

	ConsistSnapshotUnit engine;
	engine.engine_type = 0;
	engine.cargo_type = 0;
	engine.cargo_capacity = 0;
	engine.cargo_count = 0;
	engine.subtype = 1;
	snapshot.units.push_back(engine);

	ConsistSnapshotUnit wagon;
	wagon.engine_type = 1;
	wagon.cargo_type = 0;
	wagon.cargo_capacity = static_cast<uint16_t>(cargo_count + 40);
	wagon.cargo_count = cargo_count;
	wagon.subtype = 0;
	wagon.cargo_source.name_space = ns;
	wagon.cargo_source.origin_station.name_space = ns;
	wagon.cargo_source.origin_station.sequence = 1;
	wagon.cargo_source.origin_world = WorldID{1};
	wagon.cargo_source.origin_tile_x = 10;
	wagon.cargo_source.origin_tile_y = 10;
	snapshot.units.push_back(wagon);

	return snapshot;
}

class LiveFederationFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();
};

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - Seamless Trans-Server Train Traversal & Custody Lifecycle")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	FederationStagingManager::Reset();
	PlanetManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
	InitLiveFederationEngines();

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg1{
		.id = WorldID{1},
		.name = "Homeworld Alpha",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63,
	};
	PlanetRegion reg2{
		.id = WorldID{2},
		.name = "Frontier Beta",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg1);
	PlanetManager::RegisterRegion(reg2);

	/* Gateway on World 2 (arrival node) */
	TileIndex gate_w2 = TileXY(45, 20);
	MakeRailTunnel(gate_w2, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	SetTunnelBridgeReservation(gate_w2, false);
	PortalID pid_w2 = PortalRegistry::RegisterInterServerPortal(
		gate_w2, DiagDirection::NE, WorldID{2}, WorldID{1}, 101, 15);
	REQUIRE(pid_w2 != INVALID_PORTAL);

	auto &authority = UniverseAuthorityService::Instance();
	RegisteredWorld rw1{.world_id = WorldID{1}, .name = "Homeworld Alpha", .status = WorldOnlineStatus::Online};
	RegisteredWorld rw2{.world_id = WorldID{2}, .name = "Frontier Beta", .status = WorldOnlineStatus::Online};
	authority.RegisterWorld(rw1);
	authority.RegisterWorld(rw2);

	/* 1. Initiate transfer from Server A (World 1) to Server B (World 2) */
	ConsistSnapshot snapshot = CreateFederationTrainSnapshot(80, 501);
	auto encoded = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(encoded.Succeeded());

	std::string tx_id = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 101, pid_w2.base(), encoded, 10);
	REQUIRE_FALSE(tx_id.empty());
	CHECK(authority.GetTransfer(tx_id)->state == TransferState::Locked);

	/* 2. Depart transfer into Central Universe Authority custody */
	REQUIRE(authority.DepartTransfer(tx_id, 100));
	CHECK(authority.GetTransfer(tx_id)->state == TransferState::InTransit);

	/* 3. Before transit delay elapses (e.g. tick 105 < 110), arrival is not claimed */
	CHECK(FederationTransferManager::ProcessIncomingTransfers(WorldID{2}, 105) == 0);

	/* 4. When transit delay has elapsed (tick 110), transfer is claimed and materialized */
	CHECK(FederationTransferManager::ProcessIncomingTransfers(WorldID{2}, 110) == 1);
	CHECK(authority.GetTransfer(tx_id)->state == TransferState::Completed);

	/* 5. Consist verified on World 2 */
	Train *materialized = nullptr;
	size_t v_count = 0;
	uint32_t total_cargo = 0;
	for (Train *t : Train::Iterate()) {
		v_count++;
		total_cargo += t->cargo.StoredCount();
		if (t->IsFrontEngine()) materialized = t;
	}
	REQUIRE(materialized != nullptr);
	CHECK(v_count == 2);
	CHECK(total_cargo == 80);
	CHECK(HasTunnelBridgeReservation(gate_w2));

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
}

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - Cross-Server Round-Trip Orders & Continuous Scheduling")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	FederationStagingManager::Reset();
	PlanetManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
	InitLiveFederationEngines();

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg1{
		.id = WorldID{1},
		.name = "Origin World",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63,
	};
	PlanetRegion reg2{
		.id = WorldID{2},
		.name = "Remote World",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg1);
	PlanetManager::RegisterRegion(reg2);

	/* Station on World 1 */
	TileIndex st1_tile = TileXY(10, 10);
	MakeRailNormal(st1_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *st1 = Station::Create(st1_tile);
	REQUIRE(st1 != nullptr);
	st1->owner = CompanyID{0};
	auto gst1 = FederationIdentityRegistry::GetOrCreateStation(st1->index);
	REQUIRE(gst1.has_value());

	/* Station on World 2 */
	TileIndex st2_tile = TileXY(50, 10);
	MakeRailNormal(st2_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(Station::CanAllocateItem());
	Station *st2 = Station::Create(st2_tile);
	REQUIRE(st2 != nullptr);
	st2->owner = CompanyID{0};
	auto gst2 = FederationIdentityRegistry::GetOrCreateStation(st2->index);
	REQUIRE(gst2.has_value());

	/* Portals linking W1 <-> W2 */
	TileIndex gate_w1 = TileXY(25, 10);
	MakeRailTunnel(gate_w1, CompanyID{0}, DiagDirection::SW, RAILTYPE_BEGIN);
	PortalID pid_w1 = PortalRegistry::RegisterInterServerPortal(gate_w1, DiagDirection::SW, WorldID{1}, WorldID{2}, 200, 10);
	REQUIRE(pid_w1 != INVALID_PORTAL);

	TileIndex gate_w2 = TileXY(40, 10);
	MakeRailTunnel(gate_w2, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	PortalID pid_w2 = PortalRegistry::RegisterInterServerPortal(gate_w2, DiagDirection::NE, WorldID{2}, WorldID{1}, 100, 10);
	REQUIRE(pid_w2 != INVALID_PORTAL);

	/* Set up 2-way cross-server order schedule */
	GlobalOrderDestinationID ord1 = GlobalOrderDestinationID::ForStation(*gst1, false);
	GlobalOrderDestinationID ord2 = GlobalOrderDestinationID::ForStation(*gst2, false);

	ConsistSnapshot snapshot = CreateFederationTrainSnapshot(50, 601);
	snapshot.orders.push_back(ord1);
	snapshot.orders.push_back(ord2);
	snapshot.current_order_index = 0; // Currently at World 1 leg

	/* Materialize on World 2: ConsistMaterializer should advance active order to ord2 */
	SetTunnelBridgeReservation(gate_w2, false);
	ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(snapshot, gate_w2, DiagDirection::NE);
	REQUIRE(mat_res.success);
	REQUIRE(mat_res.consist != nullptr);
	CHECK(mat_res.consist->GetNumOrders() == 2);
	CHECK(mat_res.consist->cur_real_order_index == 1); // Advanced to World 2 station!

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	_orderlist_pool.CleanPool();
}

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - Staging Siding Configuration Command (CmdConfigurePortalStagingSiding)")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	PlanetManager::Reset();
	_company_pool.CleanPool();

	Company::CreateAtIndex(CompanyID{0});
	Company::CreateAtIndex(CompanyID{1});
	_current_company = CompanyID{0};

	PlanetRegion reg1{
		.id = WorldID{1},
		.name = "Core World",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63,
	};
	PlanetRegion reg2{
		.id = WorldID{2},
		.name = "Other World",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg1);
	PlanetManager::RegisterRegion(reg2);

	TileIndex portal_tile = TileXY(15, 15);
	MakeRailTunnel(portal_tile, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	PortalID pid = PortalRegistry::RegisterInterServerPortal(portal_tile, DiagDirection::NE, WorldID{1}, WorldID{2}, 201, 10);
	REQUIRE(pid != INVALID_PORTAL);

	TileIndex siding_tile = TileXY(14, 15);
	TileIndex foreign_world_siding = TileXY(40, 15);

	/* 1. Invalid portal tile fails */
	CommandCost res_bad = Command<Commands::ConfigurePortalStagingSiding>::Do(
		DoCommandFlags{}, TileXY(10, 10), siding_tile);
	CHECK(res_bad.Failed());

	/* 2. Siding tile in different world fails */
	CommandCost res_cross_world = Command<Commands::ConfigurePortalStagingSiding>::Do(
		DoCommandFlags{}, portal_tile, foreign_world_siding);
	CHECK(res_cross_world.Failed());

	/* 3. Non-owner company cannot configure siding */
	_current_company = CompanyID{1};
	CommandCost res_perm = Command<Commands::ConfigurePortalStagingSiding>::Do(
		DoCommandFlags{}, portal_tile, siding_tile);
	CHECK(res_perm.Failed());

	/* 4. Owner company configures valid siding */
	_current_company = CompanyID{0};
	CHECK(PortalRegistry::GetStagingSiding(portal_tile) == INVALID_TILE);
	CommandCost res_ok = Command<Commands::ConfigurePortalStagingSiding>::Do(
		DoCommandFlag::Execute, portal_tile, siding_tile);
	CHECK(res_ok.Succeeded());
	CHECK(PortalRegistry::GetStagingSiding(portal_tile) == siding_tile);

	/* 5. Clear siding with INVALID_TILE */
	CommandCost res_clear = Command<Commands::ConfigurePortalStagingSiding>::Do(
		DoCommandFlag::Execute, portal_tile, INVALID_TILE);
	CHECK(res_clear.Succeeded());
	CHECK(PortalRegistry::GetStagingSiding(portal_tile) == INVALID_TILE);

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	_company_pool.CleanPool();
}

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - Automatic Holding Loops on High Server Latency (> 250ms)")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	PlanetManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	InitLiveFederationEngines();

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg1{.id = WorldID{1}, .name = "Core", .phase = WorldPhase::Phase1_Core, .biome = WorldBiome::Temperate, .min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63};
	PlanetRegion reg2{.id = WorldID{2}, .name = "Remote", .phase = WorldPhase::Phase3_Frontier, .biome = WorldBiome::AridDesert, .min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63};
	PlanetManager::RegisterRegion(reg1);
	PlanetManager::RegisterRegion(reg2);

	TileIndex portal_tile = TileXY(10, 10);
	TileIndex siding_tile = TileXY(10, 12);
	TileIndex approach_tile = TileXY(10, 11);
	MakeRailTunnel(portal_tile, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailNormal(siding_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(approach_tile, CompanyID{0}, TrackBits{Track::Y}, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterInterServerPortal(portal_tile, DiagDirection::NE, WorldID{1}, WorldID{2}, 201, 10);
	REQUIRE(pid != INVALID_PORTAL);
	PortalRegistry::ConfigureStagingSiding(portal_tile, siding_tile);

	auto &authority = UniverseAuthorityService::Instance();
	authority.RegisterWorld(RegisteredWorld{.world_id = WorldID{2}, .name = "Remote", .status = WorldOnlineStatus::Online, .ping_ms = 40});

	/* Create train consist on World 1 */
	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	REQUIRE(train != nullptr);
	train->SetFrontEngine();
	train->owner = Owner(0);
	train->tile = portal_tile;
	train->track = Track::X;
	train->direction = Direction::NE;
	train->cur_speed = 60;

	/* When ping is normal (40ms < 250ms), holding is NOT required */
	CHECK_FALSE(FederationStagingManager::IsServerHoldingCondition(WorldID{2}));
	CHECK_FALSE(FederationStagingManager::CheckAndDivertToStaging(train, portal_tile));
	CHECK_FALSE(PortalRegistry::IsHoldingActive(portal_tile));
	CHECK_FALSE(train->vehstatus.Test(VehState::Stopped));

	/* Now remote server experiences high latency: 320ms (> 250ms) */
	authority.UpdateServerTelemetry(WorldID{2}, 320, 15.0f);
	CHECK(FederationStagingManager::IsServerHoldingCondition(WorldID{2}));
	CHECK(FederationStagingManager::GetHoldingReason(WorldID{2}).find("High Network Latency") != std::string::npos);

	/* Inbound train is automatically diverted to the designated staging siding */
	CHECK(FederationStagingManager::CheckAndDivertToStaging(train, portal_tile));
	CHECK(PortalRegistry::IsHoldingActive(portal_tile));
	CHECK(train->tile == siding_tile);
	CHECK(train->cur_speed == 0);
	CHECK(train->vehstatus.Test(VehState::Stopped));
	CHECK(FederationStagingManager::IsTrainHeld(train->index));
	CHECK(FederationStagingManager::GetTotalHeldTrainsCount() == 1);

	/* Remote server latency recovers to 25ms */
	authority.UpdateServerTelemetry(WorldID{2}, 25, 10.0f);
	CHECK_FALSE(FederationStagingManager::IsServerHoldingCondition(WorldID{2}));

	/* Auto-release restores train from holding siding */
	size_t released = FederationStagingManager::ReleaseHeldTrains();
	CHECK(released == 1);
	CHECK_FALSE(train->vehstatus.Test(VehState::Stopped));
	CHECK_FALSE(PortalRegistry::IsHoldingActive(portal_tile));
	CHECK(FederationStagingManager::GetTotalHeldTrainsCount() == 0);

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - Automatic Holding Loops on Server Maintenance & Saturated Corridors")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	PlanetManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
	InitLiveFederationEngines();

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg1{.id = WorldID{1}, .name = "Core", .phase = WorldPhase::Phase1_Core, .biome = WorldBiome::Temperate, .min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63};
	PlanetRegion reg2{.id = WorldID{2}, .name = "Remote", .phase = WorldPhase::Phase2_Developed, .biome = WorldBiome::AridDesert, .min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63};
	PlanetManager::RegisterRegion(reg1);
	PlanetManager::RegisterRegion(reg2);

	TileIndex portal_tile = TileXY(20, 20);
	TileIndex siding_tile = TileXY(20, 22);
	TileIndex approach_tile = TileXY(20, 21);
	MakeRailTunnel(portal_tile, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailNormal(siding_tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(approach_tile, CompanyID{0}, TrackBits{Track::Y}, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterInterServerPortal(portal_tile, DiagDirection::NE, WorldID{1}, WorldID{2}, 301, 10);
	REQUIRE(pid != INVALID_PORTAL);
	PortalRegistry::ConfigureStagingSiding(portal_tile, siding_tile);

	auto &authority = UniverseAuthorityService::Instance();
	RegisteredWorld rw{.world_id = WorldID{2}, .name = "Remote", .status = WorldOnlineStatus::Online, .ping_ms = 20};
	authority.RegisterWorld(rw);

	/* 1. Server Maintenance / Restart */
	RegisteredWorld *remote_ref = const_cast<RegisteredWorld*>(authority.GetWorld(WorldID{2}));
	remote_ref->status = WorldOnlineStatus::Maintenance;

	CHECK(FederationStagingManager::IsServerHoldingCondition(WorldID{2}));
	CHECK(FederationStagingManager::GetHoldingReason(WorldID{2}) == "Server Maintenance / Scheduled Restart");

	REQUIRE(Vehicle::CanAllocateItem());
	Train *train1 = Vehicle::Create<Train>();
	REQUIRE(train1 != nullptr);
	train1->SetFrontEngine();
	train1->owner = Owner(0);
	train1->tile = portal_tile;
	train1->track = Track::X;
	train1->direction = Direction::NE;
	train1->cur_speed = 70;

	CHECK(FederationStagingManager::CheckAndDivertToStaging(train1, portal_tile));
	CHECK(train1->tile == siding_tile);
	CHECK(train1->vehstatus.Test(VehState::Stopped));

	/* Server returns online: auto-release */
	remote_ref->status = WorldOnlineStatus::Online;
	CHECK_FALSE(FederationStagingManager::IsServerHoldingCondition(WorldID{2}));
	CHECK(FederationStagingManager::ReleaseHeldTrains() == 1);
	CHECK_FALSE(train1->vehstatus.Test(VehState::Stopped));

	/* 2. Corridor Saturation */
	InterServerRoute route{
		.route_id = 99,
		.source_world = WorldID{1},
		.source_gate_id = 1,
		.dest_world = WorldID{2},
		.dest_gate_id = 301,
		.max_active_in_transit = 2,
		.current_in_transit_count = 5,
	};
	authority.RegisterRoute(route);

	CHECK(FederationStagingManager::IsServerHoldingCondition(WorldID{2}));
	CHECK(FederationStagingManager::GetHoldingReason(WorldID{2}) == "Freight Corridor Saturated");

	CHECK(FederationStagingManager::CheckAndDivertToStaging(train1, portal_tile));
	CHECK(train1->vehstatus.Test(VehState::Stopped));

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
	_vehicle_pool.CleanPool();
}

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - Cross-Server Financial Clearing House Accounting")
{
	auto &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .name = "Industrial Forge"};
	RegisteredWorld w2{.world_id = WorldID{2}, .name = "Consumer Megacity"};
	authority.RegisterWorld(w1);
	authority.RegisterWorld(w2);

	/* Initially clearing balances are 0 */
	CHECK(authority.GetWorld(WorldID{1})->financial_clearing_balance == 0);
	CHECK(authority.GetWorld(WorldID{1})->total_cleared_revenue == 0);
	CHECK(authority.GetWorld(WorldID{2})->financial_clearing_balance == 0);

	/* Simulate consist transfer: World 1 exports 150 cargo units to World 2 */
	ConsistSnapshot snapshot = CreateFederationTrainSnapshot(150, 701);
	auto encoded = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(encoded.Succeeded());

	std::string tx_id = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 10, 20, encoded, 10);
	REQUIRE_FALSE(tx_id.empty());
	REQUIRE(authority.DepartTransfer(tx_id, 10));

	/* Claim transfer on arrival */
	auto claim_opt = authority.ClaimTransfer(tx_id, WorldID{2});
	REQUIRE(claim_opt.has_value());

	/* Confirm arrival: Valuation = 150 cargo units * 10 Cr = 1500 Cr */
	REQUIRE(authority.ConfirmTransferArrival(tx_id, WorldID{2}, true));

	const RegisteredWorld *w1_res = authority.GetWorld(WorldID{1});
	const RegisteredWorld *w2_res = authority.GetWorld(WorldID{2});
	REQUIRE(w1_res != nullptr);
	REQUIRE(w2_res != nullptr);

	/* World 1 credited +1500 Cr; World 2 debited -1500 Cr */
	CHECK(w1_res->financial_clearing_balance == 1500);
	CHECK(w1_res->total_cleared_revenue == 1500);
	CHECK(w2_res->financial_clearing_balance == -1500);

	/* Strict clearinghouse zero-sum balance: +1500 + (-1500) == 0 */
	CHECK((w1_res->financial_clearing_balance + w2_res->financial_clearing_balance) == 0);

	/* Second delivery of 100 units */
	ConsistSnapshot snap2 = CreateFederationTrainSnapshot(100, 702);
	std::string tx2 = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 10, 20, ConsistSnapshotCodec::Encode(snap2), 10);
	authority.DepartTransfer(tx2, 20);
	authority.ClaimTransfer(tx2, WorldID{2});
	authority.ConfirmTransferArrival(tx2, WorldID{2}, true);

	CHECK(authority.GetWorld(WorldID{1})->financial_clearing_balance == 2500);
	CHECK(authority.GetWorld(WorldID{1})->total_cleared_revenue == 2500);
	CHECK(authority.GetWorld(WorldID{2})->financial_clearing_balance == -2500);

	authority.Reset();
}

TEST_CASE_METHOD(LiveFederationFixture, "Sprint 46 - In-Game Galaxy Directory GUI Rendering & Telemetry Badges")
{
	if (_current_language == nullptr) {
		auto paths = _valid_searchpaths;
		auto binary = _searchpaths[Searchpath::BinaryDir];
		_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
		_valid_searchpaths = {Searchpath::BinaryDir};
		InitializeLanguagePacks();
		_valid_searchpaths = std::move(paths);
		_searchpaths[Searchpath::BinaryDir] = std::move(binary);
	}
	UnInitWindowSystem();
	_screen.width = _screen.pitch = 1024;
	_screen.height = 768;
	ScreenSizeChanged();
	InitWindowSystem();

	Map::Allocate(64, 64);
	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();

	Company::CreateAtIndex(CompanyID{0});
	_current_company = CompanyID{0};

	PlanetRegion reg1{.id = WorldID{1}, .name = "Core Prime", .phase = WorldPhase::Phase1_Core, .biome = WorldBiome::Temperate, .min_x = 0, .min_y = 0, .max_x = 31, .max_y = 63};
	PlanetRegion reg2{.id = WorldID{2}, .name = "Frontier Outpost", .phase = WorldPhase::Phase3_Frontier, .biome = WorldBiome::AridDesert, .min_x = 32, .min_y = 0, .max_x = 63, .max_y = 63};
	PlanetRegion reg3{.id = WorldID{3}, .name = "Deep Space Facility", .phase = WorldPhase::Phase4_Expansion, .biome = WorldBiome::Volcanic, .min_x = 0, .min_y = 0, .max_x = 31, .max_y = 31};
	PlanetManager::RegisterRegion(reg1);
	PlanetManager::RegisterRegion(reg2);
	PlanetManager::RegisterRegion(reg3);

	auto &authority = UniverseAuthorityService::Instance();
	RegisteredWorld rw1{.world_id = WorldID{1}, .name = "Core Prime", .status = WorldOnlineStatus::Online, .ping_ms = 18, .traffic_load_pct = 35.0f, .financial_clearing_balance = 5000, .total_cleared_revenue = 5000};
	RegisteredWorld rw2{.world_id = WorldID{2}, .name = "Frontier Outpost", .status = WorldOnlineStatus::Online, .ping_ms = 95, .traffic_load_pct = 78.0f, .financial_clearing_balance = -5000, .total_cleared_revenue = 0};
	RegisteredWorld rw3{.world_id = WorldID{3}, .name = "Deep Space Facility", .status = WorldOnlineStatus::Maintenance, .ping_ms = 350, .traffic_load_pct = 0.0f};

	authority.RegisterWorld(rw1);
	authority.RegisterWorld(rw2);
	authority.RegisterWorld(rw3);

	/* Setup a portal with staging holding active */
	TileIndex p_tile = TileXY(12, 12);
	MakeRailTunnel(p_tile, CompanyID{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	PortalRegistry::RegisterInterServerPortal(p_tile, DiagDirection::NE, WorldID{1}, WorldID{2}, 501, 10);
	PortalRegistry::ConfigureStagingSiding(p_tile, TileXY(12, 14));
	PortalRegistry::SetHoldingActive(p_tile, true);

	auto dir_gui = authority.GetWorldDirectoryForGUI();
	REQUIRE(dir_gui.size() >= 3);

	/* Check telemetry and clearing data populated */
	auto it1 = std::find_if(dir_gui.begin(), dir_gui.end(), [](const RegisteredWorld &w) { return w.world_id == WorldID{1}; });
	REQUIRE(it1 != dir_gui.end());
	CHECK(it1->ping_ms == 18);
	CHECK(it1->traffic_load_pct == 35.0f);
	CHECK(it1->financial_clearing_balance == 5000);

	auto it2 = std::find_if(dir_gui.begin(), dir_gui.end(), [](const RegisteredWorld &w) { return w.world_id == WorldID{2}; });
	REQUIRE(it2 != dir_gui.end());
	CHECK(it2->ping_ms == 95);
	CHECK(it2->traffic_load_pct == 78.0f);

	auto it3 = std::find_if(dir_gui.begin(), dir_gui.end(), [](const RegisteredWorld &w) { return w.world_id == WorldID{3}; });
	REQUIRE(it3 != dir_gui.end());
	CHECK(it3->status == WorldOnlineStatus::Maintenance);

	/* Test directory window lifecycle, widgets, and tree */
	const WindowDesc *directory_desc = nullptr;
	for (const WindowDesc *desc : *_window_descs) {
		if (desc->cls == WindowClass::UniverseDirectory) directory_desc = desc;
	}
	REQUIRE(directory_desc != nullptr);
	CHECK(directory_desc->ini_key == "view_universe_directory");
	CHECK(directory_desc->GetDefaultWidth() == 520);
	CHECK(directory_desc->GetDefaultHeight() == 370);

	NWidgetStacked *shade_select = nullptr;
	std::unique_ptr<NWidgetBase> root = nullptr;
	REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(directory_desc->nwid_parts, &shade_select));
	REQUIRE(root != nullptr);

	ShowUniverseDirectory();
	Window *w = FindWindowById(WindowClass::UniverseDirectory, 0);
	REQUIRE(w != nullptr);

	/* Trigger lifecycle events */
	w->OnClick({}, WID_UD_REFRESH, 1);

	CloseWindowById(WindowClass::UniverseDirectory, 0);
	UnInitWindowSystem();

	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationStagingManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	_company_pool.CleanPool();
}
