/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation_cluster.cpp Unit tests for Dedicated Server Cluster Orchestration & Quarantine Failover. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/portal_registry.h"
#include "../portal/universe_authority.h"
#include "../engine_base.h"

#include "../safeguards.h"

static ConsistSnapshotBytes CreateTestClusterSnapshot(uint8_t cargo_type, uint32_t count, WorldID origin_world)
{
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 110;
	snapshot.acceleration = 16;

	FederationNamespace ns{0xCAFEBABEDEADBEEFULL, 0x1234567887654321ULL};
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

	/* Wagon */
	ConsistSnapshotUnit wagon;
	wagon.engine_type = 1;
	wagon.cargo_type = cargo_type;
	wagon.cargo_capacity = static_cast<uint16_t>(count + 50);
	wagon.cargo_count = count;
	wagon.subtype = 0;
	wagon.cargo_source.name_space = ns;
	wagon.cargo_source.origin_station.name_space = ns;
	wagon.cargo_source.origin_station.sequence = 1;
	wagon.cargo_source.origin_world = origin_world;
	wagon.cargo_source.origin_tile_x = 32;
	wagon.cargo_source.origin_tile_y = 64;
	snapshot.units.push_back(wagon);

	return ConsistSnapshotCodec::Encode(snapshot);
}

TEST_CASE("Federation Cluster - Topology Registration and Phase Hierarchy")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	/* 1. Register 3-tier developmental world cluster */
	RegisteredWorld w1{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Earth Core",
		.address = "127.0.0.1:3979",
		.description = "Phase 1 Core Megacity World",
		.active_clients = 5,
		.max_clients = 32,
		.active_trains = 24
	};
	RegisteredWorld w2{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase2_Developed,
		.name = "Vulcan Forge",
		.address = "127.0.0.1:3980",
		.description = "Phase 2 Manufacturing & Smelting Hub",
		.active_clients = 2,
		.max_clients = 16,
		.active_trains = 18
	};
	RegisteredWorld w3{
		.world_id = WorldID{3},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Haven Rim",
		.address = "127.0.0.1:3981",
		.description = "Phase 3 Frontier Mining & Conduit Hub",
		.active_clients = 1,
		.max_clients = 8,
		.active_trains = 12
	};

	REQUIRE(auth.RegisterWorld(w1));
	REQUIRE(auth.RegisterWorld(w2));
	REQUIRE(auth.RegisterWorld(w3));

	CHECK(auth.GetWorlds().size() == 3);
	CHECK(auth.FindWorldsByPhase(WorldPhase::Phase1_Core).size() == 1);
	CHECK(auth.FindWorldsByPhase(WorldPhase::Phase2_Developed).size() == 1);
	CHECK(auth.FindWorldsByPhase(WorldPhase::Phase3_Frontier).size() == 1);

	/* 2. Register Inter-World Freight Corridors */
	InterServerRoute r1{
		.route_id = 1,
		.source_world = WorldID{3},
		.source_gate_id = 1,
		.dest_world = WorldID{2},
		.dest_gate_id = 1,
		.transit_duration_ticks = 50,
		.max_bandwidth_trains_per_min = 12,
		.max_active_in_transit = 4
	};
	InterServerRoute r2{
		.route_id = 2,
		.source_world = WorldID{2},
		.source_gate_id = 1,
		.dest_world = WorldID{1},
		.dest_gate_id = 1,
		.transit_duration_ticks = 60,
		.max_bandwidth_trains_per_min = 16,
		.max_active_in_transit = 6
	};

	REQUIRE(auth.RegisterRoute(r1));
	REQUIRE(auth.RegisterRoute(r2));

	CHECK(auth.GetFreightCorridors().size() == 2);
	const auto *retrieved_r1 = auth.GetRoute(1);
	REQUIRE(retrieved_r1 != nullptr);
	CHECK(retrieved_r1->source_world == WorldID{3});
	CHECK(retrieved_r1->dest_world == WorldID{2});
}

TEST_CASE("Federation Cluster - Multi-Phase Freight Transit & Supply Chain Accounting")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Developed"};
	RegisteredWorld w3{.world_id = WorldID{3}, .phase = WorldPhase::Phase3_Frontier, .name = "Haven Rim", .address = "127.0.0.1:3981", .description = "Frontier"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);
	auth.RegisterWorld(w3);

	InterServerRoute r1{.route_id = 1, .source_world = WorldID{3}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1, .transit_duration_ticks = 40};
	InterServerRoute r2{.route_id = 2, .source_world = WorldID{2}, .source_gate_id = 1, .dest_world = WorldID{1}, .dest_gate_id = 1, .transit_duration_ticks = 40};
	auth.RegisterRoute(r1);
	auth.RegisterRoute(r2);

	/* Consist 1: Raw Ore from Haven Rim (Phase 3) -> Vulcan Forge (Phase 2) */
	ConsistSnapshotBytes snap1 = CreateTestClusterSnapshot(2 /* Ore */, 80, WorldID{3});
	std::string tx1 = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, snap1);
	REQUIRE(!tx1.empty());
	REQUIRE(auth.DepartTransfer(tx1, 100));

	/* Consist 2: Refined Alloys from Vulcan Forge (Phase 2) -> Earth Core (Phase 1) */
	ConsistSnapshotBytes snap2 = CreateTestClusterSnapshot(5 /* Goods */, 50, WorldID{2});
	std::string tx2 = auth.InitiateTransfer(WorldID{2}, WorldID{1}, 1, 1, snap2);
	REQUIRE(!tx2.empty());
	REQUIRE(auth.DepartTransfer(tx2, 100));

	/* Verify supply chain flow tracking */
	auto matrix = auth.GetEmpireSupplyChainMatrix();
	CHECK(matrix.frontier_to_refinery_cargo == 80);
	CHECK(matrix.refinery_to_core_cargo == 50);
	CHECK(matrix.total_interplanetary_cargo == 130);
	CHECK(matrix.total_tariffs_generated == 1300);

	/* Verify conservation in-transit */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 130);
	CHECK(audit.total_cargo_in_transit == 130);
	CHECK(audit.total_cargo_completed == 0);
	CHECK(audit.IsConserved());
}

TEST_CASE("Federation Cluster - Node Drop, Quarantine Bay & Invariant Preservation")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Developed"};
	RegisteredWorld w3{.world_id = WorldID{3}, .phase = WorldPhase::Phase3_Frontier, .name = "Haven Rim", .address = "127.0.0.1:3981", .description = "Frontier"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);
	auth.RegisterWorld(w3);

	InterServerRoute r1{.route_id = 1, .source_world = WorldID{3}, .source_gate_id = 1, .dest_world = WorldID{2}, .dest_gate_id = 1, .transit_duration_ticks = 30};
	auth.RegisterRoute(r1);

	/* Dispatch 2 consists destined for World 2 (Vulcan Forge) */
	ConsistSnapshotBytes snapA = CreateTestClusterSnapshot(1 /* Coal */, 60, WorldID{3});
	ConsistSnapshotBytes snapB = CreateTestClusterSnapshot(2 /* Ore */, 90, WorldID{3});

	std::string txA = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, snapA);
	std::string txB = auth.InitiateTransfer(WorldID{3}, WorldID{2}, 1, 1, snapB);
	REQUIRE(!txA.empty());
	REQUIRE(!txB.empty());

	REQUIRE(auth.DepartTransfer(txA, 10));
	REQUIRE(auth.DepartTransfer(txB, 12));

	/* Dispatch 1 consist destined for World 1 (Earth Core) to verify unaffected node */
	ConsistSnapshotBytes snapC = CreateTestClusterSnapshot(0 /* Passengers */, 40, WorldID{3});
	std::string txC = auth.InitiateTransfer(WorldID{3}, WorldID{1}, 1, 1, snapC);
	REQUIRE(auth.DepartTransfer(txC, 15));

	/* Verify state before crash */
	CHECK(auth.GetTransfer(txA)->state == TransferState::InTransit);
	CHECK(auth.GetTransfer(txB)->state == TransferState::InTransit);
	CHECK(auth.GetTransfer(txC)->state == TransferState::InTransit);

	/* SIMULATE NODE CRASH: World 2 (Vulcan Forge) drops offline */
	size_t quarantined_count = auth.QuarantineTransfersForWorld(WorldID{2}, "Vulcan Forge server process terminated unexpectedly");
	CHECK(quarantined_count == 2);

	/* Verify transfers destined for World 2 are quarantined */
	const auto *recA = auth.GetTransfer(txA);
	const auto *recB = auth.GetTransfer(txB);
	const auto *recC = auth.GetTransfer(txC);

	REQUIRE(recA != nullptr);
	REQUIRE(recB != nullptr);
	REQUIRE(recC != nullptr);

	CHECK(recA->state == TransferState::RecoveryRequired);
	CHECK(recA->status_message == "Vulcan Forge server process terminated unexpectedly");
	CHECK(recB->state == TransferState::RecoveryRequired);

	/* Consist destined for World 1 remains unaffected in transit */
	CHECK(recC->state == TransferState::InTransit);

	/* Query quarantine bay */
	auto q_list = auth.GetQuarantinedTransfers(WorldID{2});
	CHECK(q_list.size() == 2);
	CHECK(auth.GetQuarantinedTransfers(WorldID{1}).empty());

	/* CRITICAL INVARIANT: Strict commodity conservation MUST hold during quarantine! */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 190);
	CHECK(audit.total_cargo_in_transit == 190); /* Quarantined goods are accounted as in-transit */
	CHECK(audit.total_cargo_completed == 0);
	CHECK(audit.IsConserved());

	auto detailed_audit = auth.GetDetailedCommodityAudit();
	CHECK(detailed_audit.IsConserved());
}

TEST_CASE("Federation Cluster - Server Recovery and Consist Re-Activation")
{
	auto &auth = UniverseAuthorityService::Instance();
	auth.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Earth Core", .address = "127.0.0.1:3979", .description = "Core"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase2_Developed, .name = "Vulcan Forge", .address = "127.0.0.1:3980", .description = "Developed"};
	auth.RegisterWorld(w1);
	auth.RegisterWorld(w2);

	/* Consist to World 2 */
	ConsistSnapshotBytes snap = CreateTestClusterSnapshot(3 /* Steel */, 75, WorldID{1});
	std::string tx = auth.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 1, snap);
	auth.DepartTransfer(tx, 5);

	/* Crash World 2 and quarantine */
	auth.QuarantineTransfersForWorld(WorldID{2}, "Node failure");
	CHECK(auth.GetTransfer(tx)->state == TransferState::RecoveryRequired);

	/* SIMULATE SUPERVISOR RESTART & RECOVERY: World 2 comes back online */
	size_t recovered_count = auth.RecoverTransfersForWorld(WorldID{2});
	CHECK(recovered_count == 1);
	CHECK(auth.GetQuarantinedTransfers(WorldID{2}).empty());

	/* Verify transfer is back in transit and immediately ready for emergence */
	const auto *rec = auth.GetTransfer(tx);
	REQUIRE(rec != nullptr);
	CHECK(rec->state == TransferState::InTransit);
	CHECK(rec->arrival_tick == 0);

	/* Destination world queries pending transfers */
	uint64_t current_tick = 50;
	auto pending = auth.QueryPendingTransfers(WorldID{2}, current_tick);
	REQUIRE(pending.size() == 1);
	CHECK(pending[0] == tx);

	/* Destination world claims transfer */
	auto claimed = auth.ClaimTransfer(tx, WorldID{2});
	REQUIRE(claimed.has_value());
	CHECK(claimed->state == TransferState::ArrivalPending);

	/* Confirm successful arrival and emergence */
	REQUIRE(auth.ConfirmTransferArrival(tx, WorldID{2}, true));
	CHECK(auth.GetTransfer(tx)->state == TransferState::Completed);

	/* Ledger verification: cargo moved to completed, strictly conserved */
	auto audit = auth.GetCommodityAudit();
	CHECK(audit.total_cargo_initiated == 75);
	CHECK(audit.total_cargo_completed == 75);
	CHECK(audit.total_cargo_in_transit == 0);
	CHECK(audit.IsConserved());

	/* Trade balance accounting */
	auto tb_w1 = auth.GetWorldTradeBalance(WorldID{1});
	auto tb_w2 = auth.GetWorldTradeBalance(WorldID{2});
	CHECK(tb_w1.exported_cargo[3] == 75);
	CHECK(tb_w2.imported_cargo[3] == 75);
	CHECK(tb_w1.net_trade_balance_credits == 750);  /* 75 * 10 credits */
	CHECK(tb_w2.net_trade_balance_credits == -750);
}
