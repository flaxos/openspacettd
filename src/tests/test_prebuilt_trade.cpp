/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_prebuilt_trade.cpp Unit tests for Sprint 49 Prebuilt Trade Gateways and Economy Calibration (WP-49.3 & WP-49.4). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/prebuilt_trade.h"
#include "../portal/universe_graph.h"
#include "../portal/universe_authority.h"
#include <algorithm>

TEST_CASE("Sprint 49: PrebuiltTradeManager Gateway Registration and Tariff Formulas", "[sprint49][prebuilt_trade]")
{
	auto &graph = UniverseGraphManager::Instance();
	REQUIRE(graph.EnsureLoaded());

	auto &trade = PrebuiltTradeManager::Instance();
	trade.Reset();

	TileIndex tile_a(100);
	WorldID local_w(1);

	SECTION("Gateway Registration and Lifecycle")
	{
		CHECK_FALSE(trade.IsTradeGateway(tile_a));
		CHECK(trade.GetTradeGateway(tile_a) == nullptr);

		REQUIRE(trade.RegisterTradeGateway(tile_a, "world_augusta", local_w, 50));
		CHECK(trade.IsTradeGateway(tile_a));

		const PrebuiltTradeGateway *gw = trade.GetTradeGateway(tile_a);
		REQUIRE(gw != nullptr);
		CHECK(gw->portal_tile == tile_a);
		CHECK(gw->target_world_id == "world_augusta");
		CHECK(gw->target_world_name == "Augusta");
		CHECK(gw->virtual_length_tiles == 50);
		CHECK(gw->tariff_multiplier == Approx(1.25f));
		CHECK(gw->active);

		auto all = trade.GetAllTradeGateways();
		CHECK(all.size() == 1);

		CHECK(trade.UnregisterTradeGateway(tile_a));
		CHECK_FALSE(trade.IsTradeGateway(tile_a));
		CHECK(trade.GetAllTradeGateways().empty());
	}

	SECTION("WP-49.4: Freight Tariff Formula across Economy Profiles")
	{
		// Tariff = cargo_units * BASE_TARIFF (100) * tariff_multiplier * (1 + virtual_length / 100)

		// Zero cargo returns 0
		CHECK(PrebuiltTradeManager::CalculateFreightTariff(0, 1.25f, 50) == 0);

		// Augusta (Big15 Core, multiplier 1.25): 100 units, virtual length 50
		// 100 * 100 * 1.25 * (1 + 0.50) = 12500 * 1.5 = 18750
		int64_t tariff_augusta = PrebuiltTradeManager::CalculateFreightTariff(100, 1.25f, 50);
		CHECK(tariff_augusta == 18750);

		// Merredin (Phase 2 Industrial, multiplier 1.0): 200 units, virtual length 100
		// 200 * 100 * 1.0 * (1 + 1.0) = 20000 * 2.0 = 40000
		int64_t tariff_merredin = PrebuiltTradeManager::CalculateFreightTariff(200, 1.0f, 100);
		CHECK(tariff_merredin == 40000);

		// Frontier World (Phase 3, multiplier 0.85): 100 units, virtual length 0
		// 100 * 100 * 0.85 * 1.0 = 8500
		int64_t tariff_frontier = PrebuiltTradeManager::CalculateFreightTariff(100, 0.85f, 0);
		CHECK(tariff_frontier == 8500);

		// Wilderness World (Phase 4, multiplier 0.70): 50 units, virtual length 20
		// 50 * 100 * 0.70 * (1 + 0.20) = 3500 * 1.20 = 4200
		int64_t tariff_wild = PrebuiltTradeManager::CalculateFreightTariff(50, 0.70f, 20);
		CHECK(tariff_wild == 4200);
	}

	SECTION("Cargo ID Resolution from Canonical Lore Exports")
	{
		CHECK(PrebuiltTradeManager::ResolveExportCargoID("STRUCTURAL_STEEL") == CommonwealthCargoID::StructuralSteel);
		CHECK(PrebuiltTradeManager::ResolveExportCargoID("SUPERALLOYS") == CommonwealthCargoID::Superalloys);
		CHECK(PrebuiltTradeManager::ResolveExportCargoID("QUANTUM_CRYSTALS") == CommonwealthCargoID::EnrichedQuantumCrystals);
		CHECK(PrebuiltTradeManager::ResolveExportCargoID("CONSUMER_CRYSTALS") == CommonwealthCargoID::EncryptedConsumerCrystals);
		CHECK(PrebuiltTradeManager::ResolveExportCargoID("SILICON_CHIPS") == CommonwealthCargoID::SiliconChips);
		CHECK(PrebuiltTradeManager::ResolveExportCargoID("IRON_ORE") == CommonwealthCargoID::IronOre);
	}
}

TEST_CASE("Sprint 49: Outbound Consist Despawn and Scheduled Return Pipeline (WP-49.3)", "[sprint49][prebuilt_trade]")
{
	auto &graph = UniverseGraphManager::Instance();
	REQUIRE(graph.EnsureLoaded());

	auto &trade = PrebuiltTradeManager::Instance();
	trade.Reset();

	TileIndex gate_tile(500);
	WorldID local_w(2);
	uint32_t virtual_dist = 40;

	// Register trade gateway to Augusta (Big15 core world, primary export SUPERALLOYS, multiplier 1.25)
	REQUIRE(trade.RegisterTradeGateway(gate_tile, "world_augusta", local_w, virtual_dist));

	// Build a mock ConsistSnapshot
	ConsistSnapshot snap;
	snap.consist_id.sequence = 42;
	snap.consist_id.name_space.low = 1;
	snap.speed = 160;

	// Engine
	ConsistSnapshotUnit eng;
	eng.engine_type = 1;
	eng.cargo_type = 0;
	eng.cargo_capacity = 0;
	eng.cargo_count = 0;
	snap.units.push_back(eng);

	// Wagon carrying Structural Steel
	ConsistSnapshotUnit wagon1;
	wagon1.engine_type = 2;
	wagon1.cargo_type = static_cast<uint8_t>(CommonwealthCargoID::StructuralSteel);
	wagon1.cargo_capacity = 50;
	wagon1.cargo_count = 50;
	snap.units.push_back(wagon1);

	// Wagon carrying Structural Steel
	ConsistSnapshotUnit wagon2;
	wagon2.engine_type = 2;
	wagon2.cargo_type = static_cast<uint8_t>(CommonwealthCargoID::StructuralSteel);
	wagon2.cargo_capacity = 50;
	wagon2.cargo_count = 50;
	snap.units.push_back(wagon2);

	uint64_t start_tick = 5000;
	CompanyID comp(0);

	SECTION("Dispatch Outbound Consist")
	{
		std::string tx_id = trade.DispatchOutboundConsist(gate_tile, snap, start_tick, comp);
		REQUIRE_FALSE(tx_id.empty());
		CHECK(tx_id.rfind("TRADE-X", 0) == 0);

		const PrebuiltTradeGateway *gw = trade.GetTradeGateway(gate_tile);
		REQUIRE(gw != nullptr);
		CHECK(gw->total_trains_exported == 1);
		CHECK(gw->total_cargo_exported == 100);
		// Tariff: 100 units * 100 * 1.25 * (1 + 0.40) = 12500 * 1.40 = 17500
		CHECK(gw->total_tariffs_earned == 17500);

		const ScheduledTradeReturn *ret = trade.GetTradeReturn(tx_id);
		REQUIRE(ret != nullptr);
		CHECK(ret->trade_id == tx_id);
		CHECK(ret->portal_tile == gate_tile);
		CHECK(ret->local_world == local_w);
		CHECK(ret->target_world_id == "world_augusta");
		CHECK(ret->dispatch_tick == start_tick);
		CHECK(ret->arrival_tick == start_tick + virtual_dist);
		CHECK(ret->tariff_credited == 17500);
		CHECK(ret->exported_cargo_id == CommonwealthCargoID::StructuralSteel);
		CHECK(ret->exported_cargo_units == 100);
		CHECK(ret->return_cargo_id == CommonwealthCargoID::EnrichedQuantumCrystals);
		CHECK(ret->return_cargo_units == 100);
		CHECK(ret->status == TradeTransactionStatus::Queued);

		auto pending = trade.GetPendingReturns();
		CHECK(pending.size() == 1);
	}

	SECTION("Simulate Transit Delay and Scheduled Return Arrival")
	{
		std::string tx_id = trade.DispatchOutboundConsist(gate_tile, snap, start_tick, comp);
		REQUIRE_FALSE(tx_id.empty());

		// Tick before arrival
		size_t arrived_early = trade.ProcessScheduledReturns(start_tick + virtual_dist - 1);
		CHECK(arrived_early == 0);

		const ScheduledTradeReturn *ret = trade.GetTradeReturn(tx_id);
		REQUIRE(ret != nullptr);
		CHECK(ret->status == TradeTransactionStatus::Queued);

		// Arrival tick
		size_t arrived = trade.ProcessScheduledReturns(start_tick + virtual_dist);
		CHECK(arrived == 1);
		CHECK(ret->status == TradeTransactionStatus::Arrived);

		const PrebuiltTradeGateway *gw = trade.GetTradeGateway(gate_tile);
		REQUIRE(gw != nullptr);
		CHECK(gw->total_trains_imported == 1);
		CHECK(gw->total_cargo_imported == 100);

		// Subsequent ticks do not re-process
		size_t arrived_after = trade.ProcessScheduledReturns(start_tick + virtual_dist + 10);
		CHECK(arrived_after == 0);

		CHECK(trade.GetPendingReturns().empty());
	}
}
