/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint27_feature_ui.cpp Unit and regression tests for Sprint 27 Feature UI completion. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"

#include "../window_gui.h"
#include "../widgets/trade_ledger_widget.h"
#include "../widgets/federation_auth_widget.h"
#include "../portal/universe_authority.h"
#include "../portal/federation_player.h"
#include "../portal/planet_manager.h"

#include <algorithm>
#include <vector>

#include "../safeguards.h"

extern std::vector<WindowDesc*> *_window_descs;

class Sprint27GuiFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();
};

TEST_CASE_METHOD(Sprint27GuiFixture, "Sprint 27 GUI - WindowDesc Registration & Widget Tree Compilation", "[sprint27_ui],[trade_ledger],[federation_auth]")
{
	REQUIRE(_window_descs != nullptr);

	const WindowDesc *trade_ledger_desc = nullptr;
	const WindowDesc *federation_auth_desc = nullptr;

	for (const WindowDesc *desc : *_window_descs) {
		if (desc->cls == WindowClass::TradeLedger) trade_ledger_desc = desc;
		if (desc->cls == WindowClass::FederationAuth) federation_auth_desc = desc;
	}

	SECTION("Trade Ledger WindowDesc is registered and compiles valid NWidget tree")
	{
		REQUIRE(trade_ledger_desc != nullptr);
		CHECK(trade_ledger_desc->ini_key == "view_trade_ledger");
		CHECK(trade_ledger_desc->GetDefaultWidth() == 620);
		CHECK(trade_ledger_desc->GetDefaultHeight() == 420);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(trade_ledger_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}

	SECTION("Federation Auth WindowDesc is registered and compiles valid NWidget tree")
	{
		REQUIRE(federation_auth_desc != nullptr);
		CHECK(federation_auth_desc->ini_key == "view_federation_auth");
		CHECK(federation_auth_desc->GetDefaultWidth() == 620);
		CHECK(federation_auth_desc->GetDefaultHeight() == 360);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(federation_auth_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}
}

#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"

static ConsistSnapshotBytes CreateTestCargoSnapshot(const std::map<uint8_t, uint32_t> &cargo_breakdown)
{
	ConsistSnapshot snapshot;
	snapshot.direction = to_underlying(Direction::NE);
	snapshot.speed = 120;
	snapshot.acceleration = 20;

	FederationNamespace ns{0xAAAABBBBCCCCDDDDULL, 0x1111222233334444ULL};
	snapshot.consist_id = GlobalConsistID{.name_space = ns, .sequence = 2002};
	snapshot.company_id = GlobalCompanyID{.name_space = ns, .sequence = 77};
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
	engine.subtype = 1;
	snapshot.units.push_back(engine);

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

TEST_CASE("Sprint 27 GUI - Empire Supply Chain Matrix & Feeder Presentation", "[sprint27_ui],[trade_ledger]")
{
	auto &service = UniverseAuthorityService::Instance();
	service.Reset();

	SECTION("Supply chain matrix accumulates infrastructure throughput")
	{
		service.RecordSpaceportThroughput(8500);
		service.RecordEdgeConduitThroughput(14200);

		auto matrix = service.GetEmpireSupplyChainMatrix();
		CHECK(matrix.spaceport_throughput_cargo == 8500);
		CHECK(matrix.edge_conduit_throughput_cargo == 14200);
	}
}

TEST_CASE("Sprint 27 GUI - Inter-World Trade Balances & Conservation Auditing", "[sprint27_ui],[trade_ledger]")
{
	auto &service = UniverseAuthorityService::Instance();
	service.Reset();

	RegisteredWorld w1{.world_id = WorldID{1}, .phase = WorldPhase::Phase1_Core, .name = "Core World Alpha", .address = "127.0.0.1:3979", .description = "Federation Capital"};
	RegisteredWorld w2{.world_id = WorldID{2}, .phase = WorldPhase::Phase3_Frontier, .name = "Mining Colony Beta", .address = "127.0.0.1:3981", .description = "Mining Colony"};
	REQUIRE(service.RegisterWorld(w1));
	REQUIRE(service.RegisterWorld(w2));

	auto snap = CreateTestCargoSnapshot({{8, 100}});

	SECTION("Initial transfers establish conservation balance")
	{
		std::string t1 = service.InitiateTransfer(WorldID{2}, WorldID{1}, 10, 20, snap, 100);
		service.DepartTransfer(t1, 50);

		auto audit = service.GetCommodityAudit();
		CHECK(audit.total_transfers_initiated == 1);
		CHECK(audit.total_transfers_in_transit == 1);
		CHECK(audit.total_transfers_completed == 0);
		CHECK(audit.IsConserved());

		auto detailed = service.GetDetailedCommodityAudit();
		CHECK(detailed.IsConserved());

		/* Claim and complete the transfer */
		auto claimed = service.ClaimTransfer(t1, WorldID{1});
		REQUIRE(claimed.has_value());
		REQUIRE(service.ConfirmTransferArrival(t1, WorldID{1}, true));

		audit = service.GetCommodityAudit();
		CHECK(audit.total_transfers_completed == 1);
		CHECK(audit.total_transfers_in_transit == 0);
		CHECK(audit.IsConserved());
	}

	SECTION("Bilateral trade balances accurately record exported and imported flows")
	{
		std::string t1 = service.InitiateTransfer(WorldID{2}, WorldID{1}, 10, 20, snap, 100);
		service.DepartTransfer(t1, 50);
		auto claimed = service.ClaimTransfer(t1, WorldID{1});
		REQUIRE(claimed.has_value());
		REQUIRE(service.ConfirmTransferArrival(t1, WorldID{1}, true));

		auto balances = service.GetAllTradeBalances();
		REQUIRE(balances.size() >= 2);

		auto b_src = service.GetWorldTradeBalance(WorldID{2});
		auto b_dst = service.GetWorldTradeBalance(WorldID{1});

		CHECK(b_src.world_id == WorldID{2});
		CHECK(b_dst.world_id == WorldID{1});
		CHECK(b_src.net_trade_balance_credits == 1000); // 100 * 10 Cr
		CHECK(b_dst.net_trade_balance_credits == -1000);
	}
}

TEST_CASE("Sprint 27 GUI - Federation Player Authentication & Active Session Management", "[sprint27_ui],[federation_auth]")
{
	FederationPlayerRegistry::Reset();

	SECTION("Session is unauthenticated by default")
	{
		auto session = FederationPlayerRegistry::GetActiveSession();
		CHECK_FALSE(session.has_value());
	}

	SECTION("Registration and session login establish active credentials")
	{
		auto pid = FederationPlayerRegistry::RegisterPlayer("commander_shepard");
		REQUIRE(pid.IsValid());

		const auto *acc = FederationPlayerRegistry::GetPlayer(pid);
		REQUIRE(acc != nullptr);
		CHECK(acc->username == "commander_shepard");

		REQUIRE(FederationPlayerRegistry::SetActiveSession(*acc));
		auto active = FederationPlayerRegistry::GetActiveSession();
		REQUIRE(active.has_value());
		CHECK(active->username == "commander_shepard");
		CHECK(active->player_id == pid);

		/* Clear session returns to guest state */
		FederationPlayerRegistry::ClearActiveSession();
		CHECK_FALSE(FederationPlayerRegistry::GetActiveSession().has_value());
	}

	SECTION("Authentication verifies username and auth tokens")
	{
		auto pid = FederationPlayerRegistry::RegisterPlayer("operator_jane", "token_12345");
		REQUIRE(pid.IsValid());

		auto ok_auth = FederationPlayerRegistry::Authenticate("operator_jane", "token_12345");
		REQUIRE(ok_auth.has_value());
		CHECK(ok_auth->player_id == pid);

		auto bad_auth = FederationPlayerRegistry::Authenticate("operator_jane", "wrong_token");
		CHECK_FALSE(bad_auth.has_value());

		auto missing_user = FederationPlayerRegistry::Authenticate("nonexistent_user", "token_12345");
		CHECK_FALSE(missing_user.has_value());
	}
}

TEST_CASE("Sprint 27 GUI - Corporate Charter Operations & Delegation Permissions", "[sprint27_ui],[federation_auth]")
{
	FederationPlayerRegistry::Reset();

	auto owner_id = FederationPlayerRegistry::RegisterPlayer("magnate_bob");
	auto delegate_id = FederationPlayerRegistry::RegisterPlayer("associate_alice");
	auto stranger_id = FederationPlayerRegistry::RegisterPlayer("intruder_eve");

	SECTION("Charter creation grants ownership and initializes treasury")
	{
		auto cid = FederationPlayerRegistry::CharterCompany(owner_id, "Commonwealth Stellar Freight");
		REQUIRE(cid.IsValid());

		const auto *charter = FederationPlayerRegistry::GetCompanyCharter(cid);
		REQUIRE(charter != nullptr);
		CHECK(charter->company_name == "Commonwealth Stellar Freight");
		CHECK(charter->owner_player_id == owner_id);
		CHECK(FederationPlayerRegistry::IsAuthorized(cid, owner_id));
		CHECK_FALSE(FederationPlayerRegistry::IsAuthorized(cid, delegate_id));
	}

	SECTION("Owner can authorize delegates; non-owner is rejected")
	{
		auto cid = FederationPlayerRegistry::CharterCompany(owner_id, "Apex Transit Syndicate");
		REQUIRE(cid.IsValid());

		/* Stranger cannot authorize delegates */
		CHECK_FALSE(FederationPlayerRegistry::AuthorizeDelegate(cid, delegate_id, stranger_id));
		CHECK_FALSE(FederationPlayerRegistry::IsAuthorized(cid, delegate_id));

		/* Legitimate owner authorizes delegate */
		REQUIRE(FederationPlayerRegistry::AuthorizeDelegate(cid, delegate_id, owner_id));
		CHECK(FederationPlayerRegistry::IsAuthorized(cid, delegate_id));

		/* Owner can revoke delegate */
		REQUIRE(FederationPlayerRegistry::RevokeDelegate(cid, delegate_id, owner_id));
		CHECK_FALSE(FederationPlayerRegistry::IsAuthorized(cid, delegate_id));
	}

	SECTION("World presences are tracked across corporate charter")
	{
		auto cid = FederationPlayerRegistry::CharterCompany(owner_id, "Interplanetary Haulage");
		REQUIRE(cid.IsValid());

		const auto *charter = FederationPlayerRegistry::GetCompanyCharter(cid);
		REQUIRE(charter != nullptr);
		/* Initial charter has 1 presence on DEFAULT_WORLD (WorldID{1}) */
		CHECK(charter->active_world_presences.size() == 1);
		CHECK(charter->active_world_presences[0] == DEFAULT_WORLD);

		/* Adding WorldID{2} expands presences */
		FederationPlayerRegistry::RegisterWorldPresence(cid, WorldID{2});

		charter = FederationPlayerRegistry::GetCompanyCharter(cid);
		REQUIRE(charter != nullptr);
		CHECK(charter->active_world_presences.size() == 2);
		CHECK(charter->active_world_presences[0] == DEFAULT_WORLD);
		CHECK(charter->active_world_presences[1] == WorldID{2});

		/* Duplicate world presence is ignored idempotently */
		FederationPlayerRegistry::RegisterWorldPresence(cid, WorldID{2});
		charter = FederationPlayerRegistry::GetCompanyCharter(cid);
		CHECK(charter->active_world_presences.size() == 2);
	}
}
