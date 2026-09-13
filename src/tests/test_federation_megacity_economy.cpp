/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation_megacity_economy.cpp Unit tests for Phase F4 Megacity & Empire Economy. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/megacity_manager.h"
#include "../portal/universe_authority.h"
#include "../portal/consist_snapshot.h"
#include "../portal/content_manifest.h"
#include "../portal/portal_registry.h"
#include "../engine_base.h"

#include "../safeguards.h"

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

TEST_CASE("Federation Megacity - Registration, Quotas, and Monthly Evaluation Lifecycle")
{
	MegacityManager::Reset();
	REQUIRE(MegacityManager::GetAllMegacities().empty());

	TownID tid1{101};
	TownID tid2{102};

	/* Test invalid town ID */
	REQUIRE(!MegacityManager::RegisterMegacity(TownID::Invalid(), WorldID{1}, "InvalidCity"));

	/* Register Metropolis Prime with 10,000 population */
	REQUIRE(MegacityManager::RegisterMegacity(tid1, WorldID{1}, "Metropolis Prime", 10000));
	REQUIRE(MegacityManager::IsMegacity(tid1));
	REQUIRE(!MegacityManager::IsMegacity(tid2));

	const auto *profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile != nullptr);
	REQUIRE(profile->town_name == "Metropolis Prime");
	REQUIRE(profile->population == 10000);
	REQUIRE(profile->growth_state == MegacityGrowthState::Subsistence);

	/* Check formula quotas: population / 20, / 40, / 100 */
	REQUIRE(profile->monthly_quota[0] == 500); // Tier 1 Sustenance: 10000 / 20 = 500
	REQUIRE(profile->monthly_quota[1] == 250); // Tier 2 Expansion:  10000 / 40 = 250
	REQUIRE(profile->monthly_quota[2] == 100); // Tier 3 Prosperity: 10000 / 100 = 100

	/* Update population to 20,000 and verify dynamic quota scaling */
	MegacityManager::UpdatePopulation(tid1, 20000);
	profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile->population == 20000);
	REQUIRE(profile->monthly_quota[0] == 1000);
	REQUIRE(profile->monthly_quota[1] == 500);
	REQUIRE(profile->monthly_quota[2] == 200);

	/* Set custom quotas for precise testing: T1=100, T2=50, T3=20 */
	MegacityManager::SetCustomQuotas(tid1, 100, 50, 20);
	profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile->monthly_quota[0] == 100);
	REQUIRE(profile->monthly_quota[1] == 50);
	REQUIRE(profile->monthly_quota[2] == 20);

	/* -------------------------------------------------------------
	 * Cycle 1: Starvation (Tier 1 < 50% satisfaction)
	 * Deliver: T1=40 (40%), T2=50 (100%), T3=20 (100%)
	 * ------------------------------------------------------------- */
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 40);
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 50);
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier3_Prosperity, 20);

	MegacityManager::EvaluateMonthlySupply();

	profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile->delivered_last[0] == 40);
	REQUIRE(profile->delivered_last[1] == 50);
	REQUIRE(profile->delivered_last[2] == 20);
	REQUIRE(profile->delivered_current[0] == 0); // Reset for new month
	REQUIRE(profile->growth_state == MegacityGrowthState::Starvation);
	REQUIRE(profile->growth_multiplier == 0.0f);
	REQUIRE(profile->passenger_multiplier == 0.5f);

	/* -------------------------------------------------------------
	 * Cycle 2: Subsistence (Tier 1 >= 50%, Tier 2 < 100%)
	 * Deliver: Food (T1) = 80 (80%), Goods (T2) = 20 (40%), Diamonds (T3) = 20 (100%)
	 * Using RecordDeliveryByCargo
	 * ------------------------------------------------------------- */
	MegacityManager::RecordDeliveryByCargo(tid1, 11, 80); // Cargo 11 = Food -> Tier 1
	MegacityManager::RecordDeliveryByCargo(tid1, 5, 20);  // Cargo 5 = Goods -> Tier 2
	MegacityManager::RecordDeliveryByCargo(tid1, 10, 20); // Cargo 10 = Valuables/Diamonds -> Tier 3

	MegacityManager::EvaluateMonthlySupply();

	profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile->delivered_last[0] == 80);
	REQUIRE(profile->delivered_last[1] == 20);
	REQUIRE(profile->delivered_last[2] == 20);
	REQUIRE(profile->growth_state == MegacityGrowthState::Subsistence);
	REQUIRE(profile->growth_multiplier == 1.0f);
	REQUIRE(profile->passenger_multiplier == 1.0f);

	/* -------------------------------------------------------------
	 * Cycle 3: MetropolitanBoom (Tier 1 >= 100%, Tier 2 >= 100%, Tier 3 < 100%)
	 * Deliver: T1=120 (120%), T2=60 (120%), T3=10 (50%)
	 * ------------------------------------------------------------- */
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 120);
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 60);
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier3_Prosperity, 10);

	MegacityManager::EvaluateMonthlySupply();

	profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile->growth_state == MegacityGrowthState::MetropolitanBoom);
	REQUIRE(profile->growth_multiplier == 1.5f);
	REQUIRE(profile->passenger_multiplier == 1.25f);

	/* -------------------------------------------------------------
	 * Cycle 4: HyperGrowth (All 3 Tiers >= 100%)
	 * Deliver: T1=100, T2=50, T3=25 (125%)
	 * ------------------------------------------------------------- */
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 100);
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 50);
	MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier3_Prosperity, 25);

	MegacityManager::EvaluateMonthlySupply();

	profile = MegacityManager::GetProfile(tid1);
	REQUIRE(profile->growth_state == MegacityGrowthState::HyperGrowth);
	REQUIRE(profile->growth_multiplier == 2.0f);
	REQUIRE(profile->passenger_multiplier == 1.5f);

	/* Unregister */
	REQUIRE(MegacityManager::UnregisterMegacity(tid1));
	REQUIRE(!MegacityManager::IsMegacity(tid1));
	REQUIRE(MegacityManager::GetAllMegacities().empty());
}

TEST_CASE("Federation Freight Corridors - Congestion Escalation, Dynamic Delay & Priority QoS")
{
	auto &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	RegisteredWorld w1;
	w1.world_id = WorldID{1};
	w1.phase = WorldPhase::Phase3_Frontier;
	w1.name = "Frontier Mining";
	authority.RegisterWorld(w1);

	RegisteredWorld w2;
	w2.world_id = WorldID{2};
	w2.phase = WorldPhase::Phase1_Core;
	w2.name = "Core Megacity Alpha";
	authority.RegisterWorld(w2);

	/* Register corridor: max_active_in_transit = 4 trains, base transit = 100 ticks */
	InterServerRoute route;
	route.route_id = 100;
	route.source_world = WorldID{1};
	route.source_gate_id = 10;
	route.dest_world = WorldID{2};
	route.dest_gate_id = 20;
	route.transit_duration_ticks = 100;
	route.max_bandwidth_trains_per_min = 12;
	route.max_active_in_transit = 4;
	REQUIRE(authority.RegisterRoute(route));

	const auto *r_ptr = authority.GetRoute(100);
	REQUIRE(r_ptr != nullptr);
	REQUIRE(r_ptr->congestion_level == CorridorCongestionLevel::Clear);
	REQUIRE(r_ptr->current_in_transit_count == 0);

	/* Helper cargo snapshot */
	std::map<uint8_t, uint32_t> cargo{{0, 50}}; // 50 units
	auto snapshot_bytes = CreateTestCargoSnapshot(cargo);

	/* -------------------------------------------------------------
	 * Dispatch Train 1: Active = 1 / 4 (25% utilization) -> Clear (1.0x)
	 * Standard delay = 100 * 1.0 = 100
	 * ------------------------------------------------------------- */
	std::string tx1 = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, snapshot_bytes, 100, FreightPriority::Standard);
	REQUIRE(!tx1.empty());

	const auto *t1 = authority.GetTransfer(tx1);
	REQUIRE(t1 != nullptr);
	REQUIRE(t1->effective_transit_ticks == 100);
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 1);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Clear);

	/* -------------------------------------------------------------
	 * Dispatch Train 2: Active = 2 / 4 (50% utilization) -> Moderate (1.2x)
	 * Standard delay = 100 * 1.2 = 120
	 * ------------------------------------------------------------- */
	std::string tx2 = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, snapshot_bytes, 100, FreightPriority::Standard);
	REQUIRE(!tx2.empty());

	const auto *t2 = authority.GetTransfer(tx2);
	REQUIRE(t2 != nullptr);
	REQUIRE(t2->effective_transit_ticks == 120);
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 2);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Moderate);

	/* -------------------------------------------------------------
	 * Dispatch Train 3: Active = 3 / 4 (75% utilization) -> Moderate (1.2x)
	 * With Express Priority: penalty (0.2) halved to 0.1 -> 1.1x = 110 ticks
	 * ------------------------------------------------------------- */
	std::string tx3_express = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, snapshot_bytes, 100, FreightPriority::Express);
	REQUIRE(!tx3_express.empty());

	const auto *t3 = authority.GetTransfer(tx3_express);
	REQUIRE(t3 != nullptr);
	REQUIRE(t3->effective_transit_ticks == 110);
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 3);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Moderate);

	/* -------------------------------------------------------------
	 * Dispatch Train 4: Active = 4 / 4 (100% utilization) -> Congested (1.5x)
	 * Standard delay = 100 * 1.5 = 150
	 * ------------------------------------------------------------- */
	std::string tx4 = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, snapshot_bytes, 100, FreightPriority::Standard);
	REQUIRE(!tx4.empty());

	const auto *t4 = authority.GetTransfer(tx4);
	REQUIRE(t4 != nullptr);
	REQUIRE(t4->effective_transit_ticks == 150);
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 4);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Congested);

	/* -------------------------------------------------------------
	 * Dispatch Train 5: Active = 5 / 4 (125% utilization) -> Saturated (2.0x)
	 * Standard: 100 * 2.0 = 200
	 * PriorityUrgent: penalty (1.0) halved to 0.5 -> 1.5x = 150 ticks
	 * ------------------------------------------------------------- */
	std::string tx5_urgent = authority.InitiateTransfer(
		WorldID{1}, WorldID{2}, 10, 20, snapshot_bytes, 100, FreightPriority::PriorityUrgent);
	REQUIRE(!tx5_urgent.empty());

	const auto *t5 = authority.GetTransfer(tx5_urgent);
	REQUIRE(t5 != nullptr);
	REQUIRE(t5->effective_transit_ticks == 150);
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 5);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Saturated);

	/* -------------------------------------------------------------
	 * Stepwise Arrival Confirmations relieving corridor congestion
	 * ------------------------------------------------------------- */
	// Depart and claim tx1
	authority.DepartTransfer(tx1, 0);
	auto claim1 = authority.ClaimTransfer(tx1, WorldID{2});
	REQUIRE(claim1.has_value());
	REQUIRE(authority.ConfirmTransferArrival(tx1, WorldID{2}, true));

	// Active should drop from 5 to 4 -> Congested
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 4);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Congested);

	// Depart and claim tx2 & tx3
	authority.DepartTransfer(tx2, 0);
	authority.ClaimTransfer(tx2, WorldID{2});
	authority.ConfirmTransferArrival(tx2, WorldID{2}, true);

	// Active drops to 3 -> Moderate
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 3);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Moderate);

	authority.DepartTransfer(tx3_express, 0);
	authority.ClaimTransfer(tx3_express, WorldID{2});
	authority.ConfirmTransferArrival(tx3_express, WorldID{2}, true);

	// Active drops to 2 -> Moderate
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 2);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Moderate);

	authority.DepartTransfer(tx4, 0);
	authority.ClaimTransfer(tx4, WorldID{2});
	authority.ConfirmTransferArrival(tx4, WorldID{2}, true);

	// Active drops to 1 -> Clear
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 1);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Clear);

	authority.DepartTransfer(tx5_urgent, 0);
	authority.ClaimTransfer(tx5_urgent, WorldID{2});
	authority.ConfirmTransferArrival(tx5_urgent, WorldID{2}, true);

	// Active drops to 0 -> Clear
	REQUIRE(authority.GetRoute(100)->current_in_transit_count == 0);
	REQUIRE(authority.GetRoute(100)->congestion_level == CorridorCongestionLevel::Clear);

	/* Verify zero commodity loss during congestion transitions */
	auto audit = authority.GetCommodityAudit();
	REQUIRE(audit.IsConserved());
	REQUIRE(audit.total_cargo_initiated == 250);
	REQUIRE(audit.total_cargo_completed == 250);
	REQUIRE(audit.total_cargo_in_transit == 0);
}

TEST_CASE("Federation Empire Supply Chain Matrix & Conservation Accounting")
{
	auto &authority = UniverseAuthorityService::Instance();
	authority.Reset();

	/* Setup 3 Worlds: Phase 3 (Frontier), Phase 2 (Refinery Hub), Phase 1 (Core Megacity) */
	RegisteredWorld w_frontier;
	w_frontier.world_id = WorldID{1};
	w_frontier.phase = WorldPhase::Phase3_Frontier;
	w_frontier.name = "Frontier Extraction Outpost";
	authority.RegisterWorld(w_frontier);

	RegisteredWorld w_refinery;
	w_refinery.world_id = WorldID{2};
	w_refinery.phase = WorldPhase::Phase2_Developed;
	w_refinery.name = "Industrial Refinery Station";
	authority.RegisterWorld(w_refinery);

	RegisteredWorld w_core;
	w_core.world_id = WorldID{3};
	w_core.phase = WorldPhase::Phase1_Core;
	w_core.name = "Metropolis Core Prime";
	authority.RegisterWorld(w_core);

	/* Verify empty matrix initially */
	auto matrix = authority.GetEmpireSupplyChainMatrix();
	REQUIRE(matrix.frontier_to_refinery_cargo == 0);
	REQUIRE(matrix.refinery_to_core_cargo == 0);
	REQUIRE(matrix.frontier_to_core_cargo == 0);
	REQUIRE(matrix.core_export_cargo == 0);
	REQUIRE(matrix.total_interplanetary_cargo == 0);
	REQUIRE(matrix.total_tariffs_generated == 0);

	/* Flow 1: Frontier (P3) -> Refinery (P2): 100 units of Iron Ore / Oil */
	auto snap1 = CreateTestCargoSnapshot({{8, 100}});
	std::string tx1 = authority.InitiateTransfer(WorldID{1}, WorldID{2}, 1, 2, snap1, 50);
	REQUIRE(!tx1.empty());

	matrix = authority.GetEmpireSupplyChainMatrix();
	REQUIRE(matrix.frontier_to_refinery_cargo == 100);
	REQUIRE(matrix.total_interplanetary_cargo == 100);
	REQUIRE(matrix.total_tariffs_generated == 1000); // 100 * 10 Cr

	/* Flow 2: Refinery (P2) -> Core (P1): 60 units of Steel / Building Materials */
	auto snap2 = CreateTestCargoSnapshot({{9, 60}});
	std::string tx2 = authority.InitiateTransfer(WorldID{2}, WorldID{3}, 2, 3, snap2, 50);
	REQUIRE(!tx2.empty());

	matrix = authority.GetEmpireSupplyChainMatrix();
	REQUIRE(matrix.frontier_to_refinery_cargo == 100);
	REQUIRE(matrix.refinery_to_core_cargo == 60);
	REQUIRE(matrix.total_interplanetary_cargo == 160);
	REQUIRE(matrix.total_tariffs_generated == 1600);

	/* Flow 3: Frontier (P3) -> Core (P1): 80 units of direct Food sustenance */
	auto snap3 = CreateTestCargoSnapshot({{11, 80}});
	std::string tx3 = authority.InitiateTransfer(WorldID{1}, WorldID{3}, 1, 3, snap3, 50);
	REQUIRE(!tx3.empty());

	matrix = authority.GetEmpireSupplyChainMatrix();
	REQUIRE(matrix.frontier_to_refinery_cargo == 100);
	REQUIRE(matrix.refinery_to_core_cargo == 60);
	REQUIRE(matrix.frontier_to_core_cargo == 80);
	REQUIRE(matrix.total_interplanetary_cargo == 240);
	REQUIRE(matrix.total_tariffs_generated == 2400);

	/* Flow 4: Core (P1) -> Frontier (P3): 40 units of high-tech Machinery / Data Crystals export */
	auto snap4 = CreateTestCargoSnapshot({{10, 40}});
	std::string tx4 = authority.InitiateTransfer(WorldID{3}, WorldID{1}, 3, 1, snap4, 50);
	REQUIRE(!tx4.empty());

	matrix = authority.GetEmpireSupplyChainMatrix();
	REQUIRE(matrix.frontier_to_refinery_cargo == 100);
	REQUIRE(matrix.refinery_to_core_cargo == 60);
	REQUIRE(matrix.frontier_to_core_cargo == 80);
	REQUIRE(matrix.core_export_cargo == 40);
	REQUIRE(matrix.total_interplanetary_cargo == 280);
	REQUIRE(matrix.total_tariffs_generated == 2800);

	/* Mid-transit Conservation Check */
	auto audit_mid = authority.GetDetailedCommodityAudit();
	REQUIRE(audit_mid.IsConserved());
	REQUIRE(audit_mid.cargo_initiated[8] == 100);
	REQUIRE(audit_mid.cargo_in_transit[8] == 100);
	REQUIRE(audit_mid.cargo_initiated[9] == 60);
	REQUIRE(audit_mid.cargo_in_transit[9] == 60);
	REQUIRE(audit_mid.cargo_initiated[11] == 80);
	REQUIRE(audit_mid.cargo_in_transit[11] == 80);
	REQUIRE(audit_mid.cargo_initiated[10] == 40);
	REQUIRE(audit_mid.cargo_in_transit[10] == 40);

	/* Deliver all 4 transfers */
	authority.DepartTransfer(tx1, 0);
	authority.ClaimTransfer(tx1, WorldID{2});
	authority.ConfirmTransferArrival(tx1, WorldID{2}, true);

	authority.DepartTransfer(tx2, 0);
	authority.ClaimTransfer(tx2, WorldID{3});
	authority.ConfirmTransferArrival(tx2, WorldID{3}, true);

	authority.DepartTransfer(tx3, 0);
	authority.ClaimTransfer(tx3, WorldID{3});
	authority.ConfirmTransferArrival(tx3, WorldID{3}, true);

	authority.DepartTransfer(tx4, 0);
	authority.ClaimTransfer(tx4, WorldID{1});
	authority.ConfirmTransferArrival(tx4, WorldID{1}, true);

	/* Final Conservation Check */
	auto audit_final = authority.GetDetailedCommodityAudit();
	REQUIRE(audit_final.IsConserved());
	REQUIRE(audit_final.cargo_completed[8] == 100);
	REQUIRE(audit_final.cargo_completed[9] == 60);
	REQUIRE(audit_final.cargo_completed[11] == 80);
	REQUIRE(audit_final.cargo_completed[10] == 40);
	REQUIRE(audit_final.cargo_in_transit[8] == 0);
	REQUIRE(audit_final.cargo_in_transit[9] == 0);
	REQUIRE(audit_final.cargo_in_transit[11] == 0);
	REQUIRE(audit_final.cargo_in_transit[10] == 0);

	auto summary = authority.GetCommodityAudit();
	REQUIRE(summary.total_cargo_initiated == 280);
	REQUIRE(summary.total_cargo_completed == 280);
	REQUIRE(summary.total_cargo_in_transit == 0);
	REQUIRE(summary.IsConserved());
}
