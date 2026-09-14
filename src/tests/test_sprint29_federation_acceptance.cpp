/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint29_federation_acceptance.cpp Sprint 29 Federation Acceptance Test Suite. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../company_base.h"
#include "../engine_base.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../order_base.h"
#include "../portal/consist_materializer.h"
#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/federation_cmd.h"
#include "../portal/federation_identity.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/universe_authority.h"
#include "../station_base.h"
#include "../train.h"
#include "../vehicle_base.h"
#include "mock_environment.h"

#include "../safeguards.h"

static ConsistSnapshotBytes BuildTestSnapshot(
	uint8_t cargo_type,
	uint32_t count,
	WorldID origin_world,
	const std::vector<GlobalOrderDestinationID> &orders = {},
	uint16_t current_order_index = 0)
{
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 100;
	snapshot.acceleration = 12;

	FederationNamespace ns{0xFEEDFACECAFEBEEFULL, 0x1122334455667788ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 7701};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 42};
	snapshot.owner = snapshot.company_id.ToOwnerToken();
	snapshot.orders = orders;
	snapshot.current_order_index = current_order_index;

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

	/* Wagon */
	if (count > 0) {
		ConsistSnapshotUnit wagon;
		wagon.engine_type = 1;
		wagon.cargo_type = cargo_type;
		wagon.cargo_capacity = static_cast<uint16_t>(count + 20);
		wagon.cargo_count = count;
		wagon.subtype = 0;
		wagon.cargo_source.name_space = ns;
		wagon.cargo_source.origin_station.name_space = ns;
		wagon.cargo_source.origin_station.sequence = 1;
		wagon.cargo_source.origin_world = origin_world;
		wagon.cargo_source.origin_tile_x = 16;
		wagon.cargo_source.origin_tile_y = 32;
		snapshot.units.push_back(wagon);
	}

	return ConsistSnapshotCodec::Encode(snapshot);
}

TEST_CASE("Sprint 29 Federation - Dynamic World Directory Discovery & Liveliness")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Earth Core",
		.last_heartbeat_tick = 100,
		.address = "127.0.0.1:3979",
		.description = "Phase 1 Core Megacity Hub",
		.active_clients = 6,
		.max_clients = 32,
		.active_trains = 20,
		.status = WorldOnlineStatus::Online
	};
	RegisteredWorld w2{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase2_Developed,
		.name = "Vulcan Forge",
		.last_heartbeat_tick = 95,
		.address = "127.0.0.1:3980",
		.description = "Phase 2 Developed Manufacturing Hub",
		.active_clients = 3,
		.max_clients = 16,
		.active_trains = 14,
		.status = WorldOnlineStatus::Online
	};
	RegisteredWorld w3{
		.world_id = WorldID{3},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Haven Rim",
		.last_heartbeat_tick = 50, // older heartbeat
		.address = "127.0.0.1:3981",
		.description = "Phase 3 Frontier Resource Extraction",
		.active_clients = 1,
		.max_clients = 8,
		.active_trains = 8,
		.status = WorldOnlineStatus::Online
	};

	REQUIRE(auth.RegisterWorld(w1));
	REQUIRE(auth.RegisterWorld(w2));
	REQUIRE(auth.RegisterWorld(w3));

	/* Directory size */
	CHECK(auth.GetWorlds().size() == 3);
	CHECK(auth.GetWorldDirectory().size() == 3);

	/* Multi-phase queries */
	CHECK(auth.FindWorldsByPhase(WorldPhase::Phase1_Core).size() == 1);
	CHECK(auth.FindWorldsByPhase(WorldPhase::Phase2_Developed).size() == 1);
	CHECK(auth.FindWorldsByPhase(WorldPhase::Phase3_Frontier).size() == 1);

	/* Heartbeat liveliness update */
	CHECK(auth.UpdateWorldHeartbeat(WorldID{3}, 2, 10, 105));
	const auto *updated_w3 = auth.GetWorld(WorldID{3});
	REQUIRE(updated_w3 != nullptr);
	CHECK(updated_w3->last_heartbeat_tick == 105);
	CHECK(updated_w3->active_clients == 2);
	CHECK(updated_w3->active_trains == 10);

	/* Prune stale nodes */
	CHECK(auth.PruneStaleWorlds(200, 500) == 0); // None stale within 500 ticks
	auth.UpdateWorldHeartbeat(WorldID{1}, 6, 20, 1000);
	auth.UpdateWorldHeartbeat(WorldID{2}, 3, 14, 1000);
	CHECK(auth.PruneStaleWorlds(1000, 500) == 1); // Haven Rim (last heartbeat 105) marked unreachable
	CHECK(auth.GetWorld(WorldID{3})->status == WorldOnlineStatus::Unreachable);
	CHECK(auth.GetWorld(WorldID{1})->status == WorldOnlineStatus::Online);
	CHECK(auth.GetWorld(WorldID{2})->status == WorldOnlineStatus::Online);
}

TEST_CASE("Sprint 29 Federation - Strict Content Admission Rejection & Invariant Guard")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Forge"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);

	InterServerRoute route{.route_id = 1, .source_world = WorldID{1}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1};
	auth.RegisterRoute(route);

	/* Create a valid consist snapshot */
	ConsistSnapshotBytes valid_bytes = BuildTestSnapshot(1 /* Coal */, 50, WorldID{1});
	REQUIRE(valid_bytes.Succeeded());

	/* 1. Incompatible Manifest Token Rejection */
	ContentManifestToken wrong_token{};
	wrong_token.fill(0xEE);
	ConsistSnapshotResult wrong_decode = ConsistSnapshotCodec::Decode(valid_bytes.bytes, wrong_token);
	CHECK(wrong_decode.error == ConsistSnapshotError::ManifestMismatch);

	/* 2. Corrupt Payload Rejection */
	std::vector<uint8_t> corrupt_bytes = valid_bytes.bytes;
	corrupt_bytes[20] ^= 0xFF; // Flip byte
	ConsistSnapshotResult corrupt_decode = ConsistSnapshotCodec::Decode(corrupt_bytes, wrong_token);
	CHECK(corrupt_decode.error == ConsistSnapshotError::ChecksumMismatch);

	/* 3. Truncated Stream Rejection */
	std::span<const uint8_t> truncated(valid_bytes.bytes.data(), 8);
	ConsistSnapshotResult truncated_decode = ConsistSnapshotCodec::Decode(truncated, wrong_token);
	CHECK(truncated_decode.error == ConsistSnapshotError::Truncated);

	/* 4. Strict Commodity Conservation Guard: Rejection must NOT leak cargo */
	auto audit_before = auth.GetCommodityAudit();
	CHECK(audit_before.total_cargo_initiated == 0);
	CHECK(audit_before.total_cargo_in_transit == 0);
	CHECK(audit_before.IsConserved());

	/* An invalid transfer with empty snapshot is rejected */
	ConsistSnapshotBytes empty_snap{};
	std::string tx_empty = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 1, empty_snap);
	CHECK(tx_empty.empty());

	/* Ledger remains strictly conserved with zero cargo leaked */
	auto audit_after = auth.GetCommodityAudit();
	CHECK(audit_after.total_cargo_initiated == 0);
	CHECK(audit_after.total_cargo_in_transit == 0);
	CHECK(audit_after.IsConserved());
}

TEST_CASE("Sprint 29 Federation - Freight Corridor Congestion & Dynamic Priority Relief")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{3}, .phase = WorldPhase::Phase3_Frontier, .name = "Haven Rim", .address = "127.0.0.1:3981", .description = "Rim"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Forge"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);

	/* Route with capacity of 4 in-transit trains */
	InterServerRoute route{
		.route_id = 1,
		.source_world = WorldID{3},
		.source_gate_id = 1,
		.dest_world = WorldID{2},
		.dest_gate_id = 1,
		.transit_duration_ticks = 100,
		.max_bandwidth_trains_per_min = 12,
		.max_active_in_transit = 4
	};
	auth.RegisterRoute(route);

	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Clear);

	/* Dispatch 1 train: 1/4 = 25% (Clear) */
	ConsistSnapshotBytes s1 = BuildTestSnapshot(2 /* Ore */, 40, WorldID{3});
	std::string tx1 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, s1, 100, FreightPriority::Standard);
	auth.DepartTransfer(tx1, 10);
	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Clear);

	/* Dispatch train 2: 2/4 = 50% (Moderate starts at 50%) */
	ConsistSnapshotBytes s2 = BuildTestSnapshot(2, 40, WorldID{3});
	std::string tx2 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, s2, 100, FreightPriority::Standard);
	auth.DepartTransfer(tx2, 12);
	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Moderate);

	/* Dispatch train 3: 3/4 = 75% (Moderate) */
	ConsistSnapshotBytes s3 = BuildTestSnapshot(2, 40, WorldID{3});
	std::string tx3 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, s3, 100, FreightPriority::Standard);
	auth.DepartTransfer(tx3, 14);
	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Moderate);

	/* Dispatch train 4: 4/4 = 100% (Congested) */
	ConsistSnapshotBytes s4 = BuildTestSnapshot(2, 40, WorldID{3});
	std::string tx4 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, s4, 100, FreightPriority::Standard);
	auth.DepartTransfer(tx4, 16);
	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Congested);

	/* Dispatch train 5 (Over capacity): 5/4 = 125% (Saturated with backpressure multiplier) */
	ConsistSnapshotBytes s5 = BuildTestSnapshot(2, 40, WorldID{3});
	std::string tx5 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, s5, 100, FreightPriority::PriorityUrgent);
	auth.DepartTransfer(tx5, 18);
	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Saturated);

	/* Verify PriorityUrgent received 50% congestion penalty relief */
	const auto *rec5 = auth.GetTransfer(tx5);
	REQUIRE(rec5 != nullptr);
	CHECK(rec5->priority == FreightPriority::PriorityUrgent);
	/* Base duration 100 ticks, Saturated multiplier 2.0x => 200 ticks.
	 * PriorityUrgent 50% relief: multiplier 1.5x => 150 ticks */
	CHECK(rec5->effective_transit_ticks == 150);

	/* Relieve corridor congestion by confirming arrivals */
	auth.ClaimTransfer(tx1, WorldID{2});
	auth.ConfirmTransferArrival(tx1, WorldID{2}, true);
	auth.ClaimTransfer(tx2, WorldID{2});
	auth.ConfirmTransferArrival(tx2, WorldID{2}, true);
	auth.ClaimTransfer(tx3, WorldID{2});
	auth.ConfirmTransferArrival(tx3, WorldID{2}, true);
	auth.ClaimTransfer(tx4, WorldID{2});
	auth.ConfirmTransferArrival(tx4, WorldID{2}, true);
	auth.ClaimTransfer(tx5, WorldID{2});
	auth.ConfirmTransferArrival(tx5, WorldID{2}, true);

	CHECK(auth.EvaluateCorridorCongestion(1) == CorridorCongestionLevel::Clear);
	const auto *final_route = auth.GetRoute(1);
	REQUIRE(final_route != nullptr);
	CHECK(final_route->current_in_transit_count == 0);
}

TEST_CASE("Sprint 29 Federation - Cross-Server Multi-Hop Round Trip & Order Progression")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Forge"};
	RegisteredWorld w3{.world_id = WorldID{3}, .phase = WorldPhase::Phase3_Frontier, .name = "Haven Rim", .address = "127.0.0.1:3981", .description = "Rim"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);
	auth.RegisterWorld(w3);

	InterServerRoute r1{.route_id = 1, .source_world = WorldID{3}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1};
	InterServerRoute r2{.route_id = 2, .source_world = WorldID{2}, .source_gate_id = 1, .dest_world = WorldID{1}, .dest_gate_id = 1};
	InterServerRoute r3{.route_id = 3, .source_world = WorldID{1}, .source_gate_id = 1, .dest_world = WorldID{3}, .dest_gate_id = 1};
	auth.RegisterRoute(r1);
	auth.RegisterRoute(r2);
	auth.RegisterRoute(r3);

	/* Set up 3-station global order itinerary */
	FederationNamespace ns{0xABCDABCDABCDABCDULL, 0x1234123412341234ULL};
	GlobalStationID st_w2{.name_space = ns, .sequence = 101, .world_id = WorldID{2}};
	GlobalStationID st_w1{.name_space = ns, .sequence = 102, .world_id = WorldID{1}};
	GlobalStationID st_w3{.name_space = ns, .sequence = 103, .world_id = WorldID{3}};

	std::vector<GlobalOrderDestinationID> orders = {
		GlobalOrderDestinationID::ForStation(st_w2, false),
		GlobalOrderDestinationID::ForStation(st_w1, false),
		GlobalOrderDestinationID::ForStation(st_w3, false)
	};

	/* --- HOP 1: Haven Rim (W3) -> Vulcan Forge (W2) --- */
	ConsistSnapshotBytes snap_hop1 = BuildTestSnapshot(2 /* Ore */, 80, WorldID{3}, orders, 0);
	std::string tx1 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, snap_hop1);
	REQUIRE(!tx1.empty());
	auth.DepartTransfer(tx1, 10);
	auth.ClaimTransfer(tx1, WorldID{2});
	REQUIRE(auth.ConfirmTransferArrival(tx1, WorldID{2}, true));

	/* Verify order index advanced: 0 -> 1 */
	const auto *rec1 = auth.GetTransfer(tx1);
	REQUIRE(rec1 != nullptr);
	CHECK(rec1->state == TransferState::Completed);
	CHECK(rec1->snapshot.current_order_index == 1);

	/* --- HOP 2: Vulcan Forge (W2) -> Earth Core (W1) --- */
	ConsistSnapshotBytes snap_hop2 = BuildTestSnapshot(3 /* Steel */, 50, WorldID{2}, orders, 1);
	std::string tx2 = auth.InitiateTransfer(WorldID{2}, WorldID{1}, 1, 1, snap_hop2);
	REQUIRE(!tx2.empty());
	auth.DepartTransfer(tx2, 20);
	auth.ClaimTransfer(tx2, WorldID{1});
	REQUIRE(auth.ConfirmTransferArrival(tx2, WorldID{1}, true));

	/* Verify order index advanced: 1 -> 2 */
	const auto *rec2 = auth.GetTransfer(tx2);
	REQUIRE(rec2 != nullptr);
	CHECK(rec2->state == TransferState::Completed);
	CHECK(rec2->snapshot.current_order_index == 2);

	/* --- HOP 3: Earth Core (W1) -> Haven Rim (W3) Return Loop --- */
	ConsistSnapshotBytes snap_hop3 = BuildTestSnapshot(5 /* Goods */, 40, WorldID{1}, orders, 2);
	std::string tx3 = auth.InitiateTransfer(WorldID{1}, WorldID{3}, 1, 1, snap_hop3);
	REQUIRE(!tx3.empty());
	auth.DepartTransfer(tx3, 30);
	auth.ClaimTransfer(tx3, WorldID{3});
	REQUIRE(auth.ConfirmTransferArrival(tx3, WorldID{3}, true));

	/* Verify order index wrapped around: 2 -> 0 */
	const auto *rec3 = auth.GetTransfer(tx3);
	REQUIRE(rec3 != nullptr);
	CHECK(rec3->state == TransferState::Completed);
	CHECK(rec3->snapshot.current_order_index == 0);

	/* Verify continuous commodity conservation across full 3-hop journey */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 170); // 80 + 50 + 40
	CHECK(audit.total_cargo_completed == 170);
	CHECK(audit.total_cargo_in_transit == 0);
	CHECK(audit.IsConserved());
}

TEST_CASE("Sprint 29 Federation - Destination Node Crash, Quarantine Bay & Auto-Recovery")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Forge"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);

	InterServerRoute route{.route_id = 1, .source_world = WorldID{1}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1};
	auth.RegisterRoute(route);

	ConsistSnapshotBytes snap = BuildTestSnapshot(1 /* Coal */, 90, WorldID{1});
	std::string tx = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 1, snap);
	REQUIRE(!tx.empty());
	auth.DepartTransfer(tx, 10);
	CHECK(auth.GetTransfer(tx)->state == TransferState::InTransit);

	/* Simulate Node Crash */
	size_t quarantined = auth.QuarantineTransfersForWorld(WorldID{2}, "Vulcan Forge process dropped (SIGTERM)");
	CHECK(quarantined == 1);
	CHECK(auth.GetTransfer(tx)->state == TransferState::RecoveryRequired);
	CHECK(auth.GetQuarantinedTransfers(WorldID{2}).size() == 1);

	/* Commodity conservation MUST hold during quarantine */
	auto audit_q = auth.GetCommodityAudit();
	CHECK(audit_q.total_cargo_initiated == 90);
	CHECK(audit_q.total_cargo_in_transit == 90);
	CHECK(audit_q.IsConserved());

	/* Simulate Node Recovery */
	size_t recovered = auth.RecoverTransfersForWorld(WorldID{2});
	CHECK(recovered == 1);
	CHECK(auth.GetTransfer(tx)->state == TransferState::InTransit);
	CHECK(auth.GetQuarantinedTransfers(WorldID{2}).empty());

	/* Destination claims and confirms */
	auth.ClaimTransfer(tx, WorldID{2});
	REQUIRE(auth.ConfirmTransferArrival(tx, WorldID{2}, true));
	CHECK(auth.GetTransfer(tx)->state == TransferState::Completed);

	/* Final conservation check */
	auto audit_done = auth.GetCommodityAudit();
	CHECK(audit_done.total_cargo_completed == 90);
	CHECK(audit_done.total_cargo_in_transit == 0);
	CHECK(audit_done.IsConserved());
}

TEST_CASE("Sprint 29 Federation - Empire-Wide Multi-Commodity Conservation & Bilateral Trade Balances")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Forge"};
	RegisteredWorld w3{.world_id = WorldID{3}, .phase = WorldPhase::Phase3_Frontier, .name = "Haven Rim", .address = "127.0.0.1:3981", .description = "Rim"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);
	auth.RegisterWorld(w3);

	InterServerRoute r1{.route_id = 1, .source_world = WorldID{3}, .dest_world = WorldID{2}};
	InterServerRoute r2{.route_id = 2, .source_world = WorldID{2}, .dest_world = WorldID{1}};
	auth.RegisterRoute(r1);
	auth.RegisterRoute(r2);

	/* Multi-commodity shipments */
	std::string tx_ore = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, BuildTestSnapshot(2 /* Ore */, 120, WorldID{3}));
	auth.DepartTransfer(tx_ore, 10);
	auth.ClaimTransfer(tx_ore, WorldID{2});
	auth.ConfirmTransferArrival(tx_ore, WorldID{2}, true);

	std::string tx_steel = auth.InitiateTransfer(WorldID{2}, WorldID{1}, 1, 1, BuildTestSnapshot(3 /* Steel */, 70, WorldID{2}));
	auth.DepartTransfer(tx_steel, 20);
	auth.ClaimTransfer(tx_steel, WorldID{1});
	auth.ConfirmTransferArrival(tx_steel, WorldID{1}, true);

	/* Ship goods still in-transit */
	std::string tx_goods = auth.InitiateTransfer(WorldID{2}, WorldID{1}, 1, 1, BuildTestSnapshot(5 /* Goods */, 30, WorldID{2}));
	auth.DepartTransfer(tx_goods, 30);

	/* Per-cargo type conservation audit */
	auto detailed = auth.GetDetailedCommodityAudit();
	CHECK(detailed.IsConserved());
	CHECK(detailed.cargo_completed[2] == 120); // Ore
	CHECK(detailed.cargo_completed[3] == 70);  // Steel
	CHECK(detailed.cargo_in_transit[5] == 30); // Goods in transit

	/* Macro conservation audit */
	auto macro = auth.GetCommodityAudit();
	CHECK(macro.total_cargo_initiated == 220);
	CHECK(macro.total_cargo_completed == 190);
	CHECK(macro.total_cargo_in_transit == 30);
	CHECK(macro.IsConserved());

	/* Trade balances */
	auto tb_w3 = auth.GetWorldTradeBalance(WorldID{3});
	auto tb_w2 = auth.GetWorldTradeBalance(WorldID{2});
	auto tb_w1 = auth.GetWorldTradeBalance(WorldID{1});

	CHECK(tb_w3.exported_cargo[2] == 120);
	CHECK(tb_w3.net_trade_balance_credits == 1200); // 120 * 10

	CHECK(tb_w2.imported_cargo[2] == 120);
	CHECK(tb_w2.exported_cargo[3] == 70);
	// W2 imported 120 (-1200 Cr) and exported 70 (+700 Cr) => net -500 Cr
	CHECK(tb_w2.net_trade_balance_credits == -500);

	CHECK(tb_w1.imported_cargo[3] == 70);
	CHECK(tb_w1.net_trade_balance_credits == -700); // -700 Cr

	/* Net sum of all balances across universe must be exactly ZERO */
	int64_t universe_sum = tb_w3.net_trade_balance_credits + tb_w2.net_trade_balance_credits + tb_w1.net_trade_balance_credits;
	CHECK(universe_sum == 0);
}
