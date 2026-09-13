/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation_persistent_universe.cpp Unit tests for Phase F3 Persistent Universe & Corporate Ledger. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/federation_player.h"
#include "../portal/universe_authority.h"
#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/portal_registry.h"
#include "../engine_base.h"

#include "../safeguards.h"

static ConsistSnapshotBytes CreateMultiCargoSnapshotBytes(const std::map<uint8_t, uint32_t> &cargo_breakdown)
{
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 100;
	snapshot.acceleration = 15;

	FederationNamespace ns{0x1234567890ABCDEFULL, 0xFEDCBA0987654321ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 1001};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 42};
	snapshot.owner = snapshot.company_id.ToOwnerToken();

	ContentManifestResult manifest_res = ContentManifestCodec::CaptureCurrent();
	if (manifest_res.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*manifest_res.manifest);
		if (token_res.Succeeded()) {
			snapshot.content_manifest = token_res.token;
		}
	}

	/* Engine lead */
	ConsistSnapshotUnit engine;
	engine.engine_type = 0;
	engine.cargo_type = 0;
	engine.cargo_capacity = 0;
	engine.cargo_count = 0;
	engine.subtype = 1;
	snapshot.units.push_back(engine);

	/* Wagons per cargo item in breakdown */
	for (const auto &[cargo_type, count] : cargo_breakdown) {
		ConsistSnapshotUnit wagon;
		wagon.engine_type = 1;
		wagon.cargo_type = cargo_type;
		wagon.cargo_capacity = static_cast<uint16_t>(count + 50);
		wagon.cargo_count = count;
		wagon.subtype = 0;
		wagon.cargo_source.name_space = ns;
		wagon.cargo_source.origin_station.name_space = ns;
		wagon.cargo_source.origin_station.sequence = 1;
		wagon.cargo_source.origin_world = WorldID{1};
		wagon.cargo_source.origin_tile_x = 10;
		wagon.cargo_source.origin_tile_y = 20;
		snapshot.units.push_back(wagon);
	}

	return ConsistSnapshotCodec::Encode(snapshot);
}

TEST_CASE("Federation Persistent Universe - Player Registration and Authentication")
{
	FederationPlayerRegistry::Reset();

	REQUIRE(FederationPlayerRegistry::GetAllPlayers().empty());

	/* Register player 1 with default generated token */
	auto p1_id = FederationPlayerRegistry::RegisterPlayer("alice");
	REQUIRE(p1_id.IsValid());

	const auto *p1 = FederationPlayerRegistry::GetPlayer(p1_id);
	REQUIRE(p1 != nullptr);
	REQUIRE(p1->username == "alice");
	REQUIRE(!p1->auth_token.empty());

	/* Cannot register duplicate username */
	auto p1_dup = FederationPlayerRegistry::RegisterPlayer("alice");
	REQUIRE(!p1_dup.IsValid());

	/* Register player 2 with explicit token */
	auto p2_id = FederationPlayerRegistry::RegisterPlayer("bob", "secret-token-xyz");
	REQUIRE(p2_id.IsValid());

	const auto *p2 = FederationPlayerRegistry::GetPlayer(p2_id);
	REQUIRE(p2 != nullptr);
	REQUIRE(p2->auth_token == "secret-token-xyz");

	/* Authentication checks */
	auto auth_ok = FederationPlayerRegistry::Authenticate("bob", "secret-token-xyz");
	REQUIRE(auth_ok.has_value());
	REQUIRE(auth_ok->player_id == p2_id);

	auto auth_bad_pwd = FederationPlayerRegistry::Authenticate("bob", "wrong-token");
	REQUIRE(!auth_bad_pwd.has_value());

	auto auth_no_user = FederationPlayerRegistry::Authenticate("charlie", "any");
	REQUIRE(!auth_no_user.has_value());

	/* Token validation */
	auto val_ok = FederationPlayerRegistry::ValidateToken("secret-token-xyz");
	REQUIRE(val_ok.has_value());
	REQUIRE(val_ok->player_id == p2_id);

	auto val_bad = FederationPlayerRegistry::ValidateToken("invalid-token");
	REQUIRE(!val_bad.has_value());

	/* Total registered players */
	REQUIRE(FederationPlayerRegistry::GetAllPlayers().size() == 2);
}

TEST_CASE("Federation Persistent Universe - Corporate Charters and Multi-World Ownership")
{
	FederationPlayerRegistry::Reset();

	auto alice_id = FederationPlayerRegistry::RegisterPlayer("alice");
	auto bob_id = FederationPlayerRegistry::RegisterPlayer("bob");
	auto charlie_id = FederationPlayerRegistry::RegisterPlayer("charlie");
	GlobalPlayerID invalid_id{};

	/* Cannot charter with invalid owner */
	auto fail_corp = FederationPlayerRegistry::CharterCompany(invalid_id, "Ghost Rail");
	REQUIRE(!fail_corp.IsValid());

	/* Alice charters Trans-Stellar Transport */
	auto corp_id = FederationPlayerRegistry::CharterCompany(alice_id, "Trans-Stellar Transport");
	REQUIRE(corp_id.IsValid());

	const auto *charter = FederationPlayerRegistry::GetCompanyCharter(corp_id);
	REQUIRE(charter != nullptr);
	REQUIRE(charter->company_name == "Trans-Stellar Transport");
	REQUIRE(charter->owner_player_id == alice_id);
	REQUIRE(charter->global_treasury_credits == 1000000);
	REQUIRE(charter->active_world_presences.size() == 1);
	REQUIRE(charter->active_world_presences[0] == DEFAULT_WORLD);

	/* Ownership and authorization checks */
	REQUIRE(FederationPlayerRegistry::IsAuthorized(corp_id, alice_id));
	REQUIRE(!FederationPlayerRegistry::IsAuthorized(corp_id, bob_id));

	/* Bob cannot authorize Charlie as delegate (not owner) */
	REQUIRE(!FederationPlayerRegistry::AuthorizeDelegate(corp_id, charlie_id, bob_id));
	REQUIRE(!FederationPlayerRegistry::IsAuthorized(corp_id, charlie_id));

	/* Alice authorizes Bob as delegate */
	REQUIRE(FederationPlayerRegistry::AuthorizeDelegate(corp_id, bob_id, alice_id));
	REQUIRE(FederationPlayerRegistry::IsAuthorized(corp_id, bob_id));

	/* Bob cannot revoke Alice */
	REQUIRE(!FederationPlayerRegistry::RevokeDelegate(corp_id, alice_id, bob_id));

	/* Alice revokes Bob */
	REQUIRE(FederationPlayerRegistry::RevokeDelegate(corp_id, bob_id, alice_id));
	REQUIRE(!FederationPlayerRegistry::IsAuthorized(corp_id, bob_id));

	/* World presence expansion */
	FederationPlayerRegistry::RegisterWorldPresence(corp_id, WorldID{1});
	FederationPlayerRegistry::RegisterWorldPresence(corp_id, WorldID{2});
	/* Redundant registration ignored */
	FederationPlayerRegistry::RegisterWorldPresence(corp_id, WorldID{1});

	const auto *updated = FederationPlayerRegistry::GetCompanyCharter(corp_id);
	REQUIRE(updated->active_world_presences.size() == 3);

	/* Query player companies */
	auto alice_comps = FederationPlayerRegistry::GetPlayerCompanies(alice_id);
	REQUIRE(alice_comps.size() == 1);
	REQUIRE(alice_comps[0].company_id == corp_id);

	auto bob_comps = FederationPlayerRegistry::GetPlayerCompanies(bob_id);
	REQUIRE(bob_comps.empty());
}

TEST_CASE("Federation Persistent Universe - Dynamic World Directory & Heartbeats")
{
	auto &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	REQUIRE(authority.GetWorldDirectory().empty());

	/* Register 3 worlds with different phases and statuses */
	RegisteredWorld w1{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Terra Nova",
		.content_manifest = {},
		.last_heartbeat_tick = 100,
		.address = "127.0.0.1:3979",
		.description = "Industrial Heartland",
		.active_clients = 4,
		.max_clients = 16,
		.active_trains = 20,
		.status = WorldOnlineStatus::Online
	};
	RegisteredWorld w2{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase2_Developed,
		.name = "Aurelia Mining Outpost",
		.content_manifest = {},
		.last_heartbeat_tick = 100,
		.address = "127.0.0.1:3980",
		.description = "Deep Core Extraction",
		.active_clients = 2,
		.max_clients = 8,
		.active_trains = 15,
		.status = WorldOnlineStatus::Online
	};
	RegisteredWorld w3{
		.world_id = WorldID{3},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Sigma Orbital Gate",
		.content_manifest = {},
		.last_heartbeat_tick = 50, // Older heartbeat
		.address = "127.0.0.1:3981",
		.description = "Interstellar Hub",
		.active_clients = 8,
		.max_clients = 32,
		.active_trains = 50,
		.status = WorldOnlineStatus::Online
	};

	REQUIRE(authority.RegisterWorld(w1));
	REQUIRE(authority.RegisterWorld(w2));
	REQUIRE(authority.RegisterWorld(w3));

	REQUIRE(authority.GetWorldDirectory().size() == 3);

	/* Filter worlds by phase */
	auto phase2_worlds = authority.FindWorldsByPhase(WorldPhase::Phase2_Developed);
	REQUIRE(phase2_worlds.size() == 1);
	REQUIRE(phase2_worlds[0].name == "Aurelia Mining Outpost");

	auto phase3_worlds = authority.FindWorldsByPhase(WorldPhase::Phase3_Frontier);
	REQUIRE(phase3_worlds.size() == 1);
	REQUIRE(phase3_worlds[0].name == "Sigma Orbital Gate");

	/* Update heartbeat of world 1 */
	REQUIRE(authority.UpdateWorldHeartbeat(WorldID{1}, 6, 25, 200));
	const auto *w1_updated = authority.GetWorld(WorldID{1});
	REQUIRE(w1_updated != nullptr);
	REQUIRE(w1_updated->active_clients == 6);
	REQUIRE(w1_updated->active_trains == 25);
	REQUIRE(w1_updated->last_heartbeat_tick == 200);

	/* Stale world pruning at tick 250 with timeout 120 ticks:
	 * w1 heartbeat: 200 (age 50 -> alive)
	 * w2 heartbeat: 100 (age 150 -> stale -> marked Unreachable)
	 * w3 heartbeat: 50 (age 200 -> stale -> marked Unreachable)
	 */
	size_t pruned = authority.PruneStaleWorlds(250, 120);
	REQUIRE(pruned == 2);
	REQUIRE(authority.GetWorld(WorldID{1})->status == WorldOnlineStatus::Online);
	REQUIRE(authority.GetWorld(WorldID{2})->status == WorldOnlineStatus::Unreachable);
	REQUIRE(authority.GetWorld(WorldID{3})->status == WorldOnlineStatus::Unreachable);
}

TEST_CASE("Federation Persistent Universe - Detailed Per-Cargo Commodity Conservation & Trade Balances")
{
	auto &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	RegisteredWorld source_w{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase2_Developed,
		.name = "Origin Earth",
		.content_manifest = {},
		.last_heartbeat_tick = 10,
		.address = "127.0.0.1:3979",
		.description = "Primary World",
		.active_clients = 1,
		.max_clients = 16,
		.active_trains = 5,
		.status = WorldOnlineStatus::Online
	};
	RegisteredWorld dest_w{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase2_Developed,
		.name = "Mars Colony",
		.content_manifest = {},
		.last_heartbeat_tick = 10,
		.address = "127.0.0.1:3980",
		.description = "Colony World",
		.active_clients = 1,
		.max_clients = 16,
		.active_trains = 3,
		.status = WorldOnlineStatus::Online
	};
	authority.RegisterWorld(source_w);
	authority.RegisterWorld(dest_w);

	InterServerRoute route{
		.route_id = 1,
		.source_world = WorldID{1},
		.source_gate_id = 10,
		.dest_world = WorldID{2},
		.dest_gate_id = 20,
		.transit_duration_ticks = 40
	};
	authority.RegisterRoute(route);

	/* Consist carrying multi-cargo types:
	 * Cargo 0 (Coal): 60 units
	 * Cargo 1 (Iron Ore): 40 units
	 * Cargo 2 (Steel): 25 units
	 * Total = 125 units
	 */
	std::map<uint8_t, uint32_t> cargo_manifest{
		{0, 60},
		{1, 40},
		{2, 25}
	};

	auto snapshot_bytes = CreateMultiCargoSnapshotBytes(cargo_manifest);
	REQUIRE(snapshot_bytes.bytes.size() > 0);

	/* Check initial empty audit */
	auto audit_init = authority.GetDetailedCommodityAudit();
	REQUIRE(audit_init.IsConserved());
	REQUIRE(audit_init.cargo_initiated.empty());

	/* Step 1: Initiate transfer */
	std::string tx_id = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, snapshot_bytes, 40
	);
	REQUIRE(!tx_id.empty());

	/* Check ledger after initiation */
	auto audit_step1 = authority.GetDetailedCommodityAudit();
	REQUIRE(audit_step1.IsConserved());
	REQUIRE(audit_step1.cargo_initiated[0] == 60);
	REQUIRE(audit_step1.cargo_initiated[1] == 40);
	REQUIRE(audit_step1.cargo_initiated[2] == 25);
	REQUIRE(audit_step1.cargo_in_transit[0] == 60);
	REQUIRE(audit_step1.cargo_in_transit[1] == 40);
	REQUIRE(audit_step1.cargo_in_transit[2] == 25);
	REQUIRE(audit_step1.cargo_completed[0] == 0);

	/* Step 2: Depart */
	REQUIRE(authority.DepartTransfer(tx_id, 100));

	/* Step 3: Pending & Claim */
	auto pending = authority.QueryPendingTransfers(WorldID{2}, 145);
	REQUIRE(pending.size() == 1);
	REQUIRE(pending[0] == tx_id);

	auto claimed = authority.ClaimTransfer(tx_id, WorldID{2});
	REQUIRE(claimed.has_value());

	/* Still in transit until confirmed */
	auto audit_step2 = authority.GetDetailedCommodityAudit();
	REQUIRE(audit_step2.IsConserved());
	REQUIRE(audit_step2.cargo_completed[0] == 0);

	/* Step 4: Confirm Arrival */
	REQUIRE(authority.ConfirmTransferArrival(tx_id, WorldID{2}, true));

	/* Check detailed conservation after arrival */
	auto audit_final = authority.GetDetailedCommodityAudit();
	REQUIRE(audit_final.IsConserved());
	REQUIRE(audit_final.cargo_initiated[0] == 60);
	REQUIRE(audit_final.cargo_in_transit[0] == 0);
	REQUIRE(audit_final.cargo_completed[0] == 60);

	REQUIRE(audit_final.cargo_initiated[1] == 40);
	REQUIRE(audit_final.cargo_in_transit[1] == 0);
	REQUIRE(audit_final.cargo_completed[1] == 40);

	REQUIRE(audit_final.cargo_initiated[2] == 25);
	REQUIRE(audit_final.cargo_in_transit[2] == 0);
	REQUIRE(audit_final.cargo_completed[2] == 25);

	/* Verify total commodity audit summary */
	auto summary = authority.GetCommodityAudit();
	REQUIRE(summary.total_cargo_initiated == 125);
	REQUIRE(summary.total_cargo_completed == 125);
	REQUIRE(summary.total_cargo_in_transit == 0);
	REQUIRE(summary.IsConserved());

	/* Verify Inter-World Trade Balances */
	auto bal1 = authority.GetWorldTradeBalance(WorldID{1});
	REQUIRE(bal1.exported_cargo.at(0) == 60);
	REQUIRE(bal1.exported_cargo.at(1) == 40);
	REQUIRE(bal1.exported_cargo.at(2) == 25);
	REQUIRE(bal1.imported_cargo.empty());
	REQUIRE(bal1.net_trade_balance_credits > 0);

	auto bal2 = authority.GetWorldTradeBalance(WorldID{2});
	REQUIRE(bal2.imported_cargo.at(0) == 60);
	REQUIRE(bal2.imported_cargo.at(1) == 40);
	REQUIRE(bal2.imported_cargo.at(2) == 25);
	REQUIRE(bal2.exported_cargo.empty());
	REQUIRE(bal2.net_trade_balance_credits == -bal1.net_trade_balance_credits);
}
