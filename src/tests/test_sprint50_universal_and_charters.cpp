/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint50_universal_and_charters.cpp Unit tests for Sprint 50 Universal Rail Freight Gateways & Corporate Charters (WP-50.3 & WP-50.4). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/universe_graph.h"
#include "../portal/prebuilt_trade.h"
#include "../portal/corporate_charter.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "mock_environment.h"

#include "../safeguards.h"

static void InitCharterTestMap()
{
	(void)MockEnvironment::Instance();
	_company_pool.CleanPool();
	CorporateCharterManager::Instance().Reset();
	PrebuiltTradeManager::Instance().Reset();

	Map::Allocate(64, 64);
	for (uint y = 0; y < 64; ++y) {
		for (uint x = 0; x < 64; ++x) {
			TileIndex tile = TileXY(x, y);
			if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
			else MakeVoid(tile);
		}
	}
}

TEST_CASE("Sprint 50: WP-50.3 Universal Rail Freight Gateways (Far Away & Vinmar)", "[sprint50][universal_rail]")
{
	auto &graph = UniverseGraphManager::Instance();
	REQUIRE(graph.EnsureLoaded());

	SECTION("Far Away has permanent through-running rail connectivity")
	{
		const UniverseNode *far_away = graph.FindNode("world_far_away");
		REQUIRE(far_away != nullptr);
		CHECK(far_away->canonical_name == "Far Away");
		CHECK(far_away->connection_mode == ConnectionMode::Rail);

		const UniverseEdge *edge_fa = graph.FindEdge("world_far_away", "world_half_way");
		REQUIRE(edge_fa != nullptr);
		CHECK(edge_fa->connection_type == ConnectionMode::Rail);
		CHECK(edge_fa->availability == "PERMANENT");
		CHECK(edge_fa->permits_through_running_train == true);
		CHECK(edge_fa->enabled_in_game == true);

		/* Verify export/import cargo profiles */
		REQUIRE_FALSE(far_away->economic_profile.primary_exports.empty());
		CHECK(far_away->economic_profile.primary_exports[0] == "IRON_ORE");
		CHECK(far_away->economic_profile.tariff_multiplier == Approx(0.85f));
	}

	SECTION("Vinmar has permanent through-running rail connectivity")
	{
		const UniverseNode *vinmar = graph.FindNode("world_vinmar");
		REQUIRE(vinmar != nullptr);
		CHECK(vinmar->canonical_name == "Vinmar");
		CHECK(vinmar->connection_mode == ConnectionMode::Rail);

		const UniverseEdge *edge_vm = graph.FindEdge("world_vinmar", "world_augusta");
		REQUIRE(edge_vm != nullptr);
		CHECK(edge_vm->connection_type == ConnectionMode::Rail);
		CHECK(edge_vm->availability == "PERMANENT");
		CHECK(edge_vm->permits_through_running_train == true);
		CHECK(edge_vm->enabled_in_game == true);

		/* Verify export/import cargo profiles */
		REQUIRE_FALSE(vinmar->economic_profile.primary_exports.empty());
		CHECK(vinmar->economic_profile.primary_exports[0] == "IRON_ORE");
		CHECK(vinmar->economic_profile.tariff_multiplier == Approx(0.85f));
	}

	SECTION("Prebuilt Trade Gateways to Far Away & Vinmar support standard consist dispatch")
	{
		auto &trade = PrebuiltTradeManager::Instance();
		trade.Reset();

		TileIndex gate_fa = TileXY(12, 12);
		TileIndex gate_vm = TileXY(14, 14);

		REQUIRE(trade.RegisterTradeGateway(gate_fa, "world_far_away", WorldID(1), 50));
		REQUIRE(trade.RegisterTradeGateway(gate_vm, "world_vinmar", WorldID(1), 60));

		const PrebuiltTradeGateway *gw_fa = trade.GetTradeGateway(gate_fa);
		REQUIRE(gw_fa != nullptr);
		CHECK(gw_fa->target_world_id == "world_far_away");
		CHECK(gw_fa->target_world_name == "Far Away");
		CHECK(gw_fa->tariff_multiplier == Approx(0.85f));

		const PrebuiltTradeGateway *gw_vm = trade.GetTradeGateway(gate_vm);
		REQUIRE(gw_vm != nullptr);
		CHECK(gw_vm->target_world_id == "world_vinmar");
		CHECK(gw_vm->target_world_name == "Vinmar");
		CHECK(gw_vm->tariff_multiplier == Approx(0.85f));

		/* Build test consist */
		ConsistSnapshot snap;
		snap.consist_id.sequence = 77;
		snap.consist_id.name_space.low = 1;
		snap.speed = 120;

		ConsistSnapshotUnit eng;
		eng.engine_type = 1;
		snap.units.push_back(eng);

		ConsistSnapshotUnit wagon;
		wagon.engine_type = 2;
		wagon.cargo_type = static_cast<uint8_t>(CommonwealthCargoID::IronOre);
		wagon.cargo_capacity = 60;
		wagon.cargo_count = 60;
		snap.units.push_back(wagon);

		/* Dispatch consist to Far Away */
		std::string tx_id = trade.DispatchOutboundConsist(gate_fa, snap, 1000, CompanyID(0));
		REQUIRE_FALSE(tx_id.empty());
		CHECK(gw_fa->total_trains_exported == 1);
		CHECK(gw_fa->total_cargo_exported == 60);

		// Tariff: 60 * 100 * 0.85 * (1 + 0.50) = 5100 * 1.5 = 7650
		CHECK(gw_fa->total_tariffs_earned == 7650);
	}
}

TEST_CASE("Sprint 50: WP-50.4 Corporate Charters & Gate Access Policies", "[sprint50][corporate_charters]")
{
	InitCharterTestMap();

	Company *comp = Company::CreateAtIndex(CompanyID{0});
	comp->money = 2000000;
	_current_company = CompanyID{0};

	auto &charter_mgr = CorporateCharterManager::Instance();
	TileIndex portal_tile = TileXY(18, 18);

	SECTION("Private worlds classification")
	{
		CHECK(CorporateCharterManager::IsPrivateWorld("world_cressat"));
		CHECK(CorporateCharterManager::IsPrivateWorld("world_solidade"));
		CHECK(CorporateCharterManager::IsPrivateWorld("world_hardrock"));
		CHECK(CorporateCharterManager::IsPrivateWorld("world_ozzies_asteroid"));

		CHECK_FALSE(CorporateCharterManager::IsPrivateWorld("world_earth"));
		CHECK_FALSE(CorporateCharterManager::IsPrivateWorld("world_augusta"));
		CHECK_FALSE(CorporateCharterManager::IsPrivateWorld("world_far_away"));
		CHECK_FALSE(CorporateCharterManager::IsPrivateWorld("world_vinmar"));
	}

	SECTION("GateAccessPolicy::Public allows all traffic")
	{
		charter_mgr.SetGatePolicy(portal_tile, GateAccessPolicy::Public);
		GateAccessResult res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_augusta");
		CHECK(res.allowed);
		CHECK(res.toll_charged == 0);
	}

	SECTION("GateAccessPolicy::ReputationRestricted checks company rating or charter override")
	{
		charter_mgr.SetGatePolicy(portal_tile, GateAccessPolicy::ReputationRestricted);

		/* Low reputation without charter -> Rejected */
		comp->old_economy[0].performance_history = 500; // 50%
		GateAccessResult res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_solidade");
		CHECK_FALSE(res.allowed);
		CHECK(res.reason.find("reputation") != std::string::npos);

		/* High reputation (>= 80%) -> Allowed */
		comp->old_economy[0].performance_history = 850; // 85%
		res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_solidade");
		CHECK(res.allowed);

		/* Low reputation WITH diplomatic charter -> Allowed */
		comp->old_economy[0].performance_history = 300; // 30%
		charter_mgr.GrantCharter(CompanyID{0}, "world_solidade");
		res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_solidade");
		CHECK(res.allowed);
	}

	SECTION("GateAccessPolicy::CharterRequired enforces charter ownership")
	{
		charter_mgr.SetGatePolicy(portal_tile, GateAccessPolicy::CharterRequired);

		/* Company without charter -> Rejected */
		GateAccessResult res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_cressat");
		CHECK_FALSE(res.allowed);
		CHECK(res.reason.find("diplomatic charter") != std::string::npos);

		/* Grant charter -> Allowed */
		CHECK(charter_mgr.GrantCharter(CompanyID{0}, "world_cressat"));
		CHECK(charter_mgr.HasCharter(CompanyID{0}, "world_cressat"));
		res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_cressat");
		CHECK(res.allowed);

		/* Revoke charter -> Rejected again */
		CHECK(charter_mgr.RevokeCharter(CompanyID{0}, "world_cressat"));
		CHECK_FALSE(charter_mgr.HasCharter(CompanyID{0}, "world_cressat"));
		res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_cressat");
		CHECK_FALSE(res.allowed);
	}

	SECTION("GateAccessPolicy::TollRequired automatically deducts per-train toll")
	{
		Money initial_cash = comp->money;
		Money toll = 15000;
		charter_mgr.SetGatePolicy(portal_tile, GateAccessPolicy::TollRequired, toll);

		GateAccessResult res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_augusta");
		CHECK(res.allowed);
		CHECK(res.toll_charged == toll);
		CHECK(comp->money == initial_cash - toll);

		/* If company has insufficient cash for toll -> Rejected */
		comp->money = 5000;
		res = charter_mgr.CheckAndProcessAccess(CompanyID{0}, portal_tile, "world_augusta");
		CHECK_FALSE(res.allowed);
		CHECK(comp->money == 5000); // untouched
	}

	SECTION("CmdPurchaseDiplomaticCharter deducts cost and grants charter")
	{
		comp->money = 1000000;
		CHECK_FALSE(charter_mgr.HasCharter(CompanyID{0}, "world_hardrock"));

		CommandCost cost_res = CmdPurchaseDiplomaticCharter(DoCommandFlag::Execute, CompanyID{0}, "world_hardrock");
		CHECK(cost_res.Succeeded());
		CHECK(cost_res.GetCost() == CorporateCharterManager::DEFAULT_CHARTER_COST);
		CHECK(charter_mgr.HasCharter(CompanyID{0}, "world_hardrock"));

		/* Insufficient cash fails command */
		comp->money = 100;
		cost_res = CmdPurchaseDiplomaticCharter(DoCommandFlag::Execute, CompanyID{0}, "world_ozzies_asteroid");
		CHECK(cost_res.Failed());
		CHECK_FALSE(charter_mgr.HasCharter(CompanyID{0}, "world_ozzies_asteroid"));
	}

	SECTION("CmdSetGateAccessPolicy command execution")
	{
		CommandCost res = CmdSetGateAccessPolicy(DoCommandFlag::Execute, portal_tile, GateAccessPolicy::TollRequired, 25000);
		CHECK(res.Succeeded());
		CHECK(charter_mgr.GetGatePolicy(portal_tile) == GateAccessPolicy::TollRequired);
		CHECK(charter_mgr.GetGateToll(portal_tile) == 25000);
	}
}
