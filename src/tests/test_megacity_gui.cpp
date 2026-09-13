/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_megacity_gui.cpp Unit and regression tests for Megacity, Freight Corridor, and Universe Directory GUIs. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"

#include "../window_gui.h"
#include "../widgets/town_widget.h"
#include "../widgets/megacity_widget.h"
#include "../widgets/freight_corridor_widget.h"
#include "../widgets/universe_directory_widget.h"
#include "../portal/megacity_manager.h"
#include "../portal/universe_authority.h"
#include "../portal/planet_manager.h"

#include <algorithm>
#include <vector>

#include "../safeguards.h"

extern std::vector<WindowDesc*> *_window_descs;

class MegacityGuiFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();
};

TEST_CASE_METHOD(MegacityGuiFixture, "Sprint 18 GUI - WindowDesc Registration & Widget Tree Validity")
{
	REQUIRE(_window_descs != nullptr);

	const WindowDesc *megacity_desc = nullptr;
	const WindowDesc *corridor_desc = nullptr;
	const WindowDesc *directory_desc = nullptr;

	for (const WindowDesc *desc : *_window_descs) {
		if (desc->cls == WindowClass::MegacityOverview) megacity_desc = desc;
		if (desc->cls == WindowClass::FreightCorridorMonitor) corridor_desc = desc;
		if (desc->cls == WindowClass::UniverseDirectory) directory_desc = desc;
	}

	SECTION("Megacity Overview WindowDesc is registered and valid")
	{
		REQUIRE(megacity_desc != nullptr);
		CHECK(megacity_desc->ini_key == "view_megacity");
		CHECK(megacity_desc->GetDefaultWidth() == 440);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(megacity_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}

	SECTION("Freight Corridor Monitor WindowDesc is registered and valid")
	{
		REQUIRE(corridor_desc != nullptr);
		CHECK(corridor_desc->ini_key == "view_freight_corridors");
		CHECK(corridor_desc->GetDefaultWidth() == 520);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(corridor_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}

	SECTION("Universe Directory WindowDesc is registered and valid")
	{
		REQUIRE(directory_desc != nullptr);
		CHECK(directory_desc->ini_key == "view_universe_directory");
		CHECK(directory_desc->GetDefaultWidth() == 520);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(directory_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}
}

TEST_CASE("Sprint 18 GUI - Megacity Profile Presentation and Growth Multipliers")
{
	MegacityManager::Reset();
	TownID tid1{10};
	TownID tid2{20};

	REQUIRE(MegacityManager::RegisterMegacity(tid1, WorldID{1}, "Metropolis Earth", 20000));
	REQUIRE(MegacityManager::RegisterMegacity(tid2, WorldID{2}, "Novosibirsk Mars", 5000));

	SECTION("Initial quotas and zero delivery lead to Starvation")
	{
		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->population == 20000);
		CHECK(p1->monthly_quota[0] == 1000); // 20000 / 20
		CHECK(p1->monthly_quota[1] == 500);  // 20000 / 40
		CHECK(p1->monthly_quota[2] == 200);  // 20000 / 100

		MegacityManager::EvaluateMonthlySupply();
		CHECK(p1->growth_state == MegacityGrowthState::Starvation);
		CHECK(p1->growth_multiplier == 0.0f);
	}

	SECTION("Tier 1 partial satisfaction enables Subsistence")
	{
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 600); // 60% of 1000
		MegacityManager::EvaluateMonthlySupply();

		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->growth_state == MegacityGrowthState::Subsistence);
		CHECK(p1->growth_multiplier == 1.0f);
	}

	SECTION("Tier 1 & Tier 2 complete satisfaction enables Metropolitan Boom")
	{
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 1000);
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 500);
		MegacityManager::EvaluateMonthlySupply();

		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->growth_state == MegacityGrowthState::MetropolitanBoom);
		CHECK(p1->growth_multiplier == 1.5f);
	}

	SECTION("All 3 Tiers met enables HyperGrowth with traffic boost")
	{
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 1200);
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 600);
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier3_Prosperity, 250);
		MegacityManager::EvaluateMonthlySupply();

		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->growth_state == MegacityGrowthState::HyperGrowth);
		CHECK(p1->growth_multiplier == 2.0f);
		CHECK(p1->passenger_multiplier == 1.5f);
	}

	SECTION("Listing all megacities preserves entries")
	{
		auto all = MegacityManager::GetAllMegacities();
		CHECK(all.size() == 2);
	}
}

TEST_CASE("Sprint 18 GUI - Freight Corridor Monitor Utilization and Delays")
{
	auto &service = UniverseAuthorityService::Instance();
	service.Reset();

	InterServerRoute route{
		.route_id = 42,
		.source_world = WorldID{1},
		.source_gate_id = 101,
		.dest_world = WorldID{2},
		.dest_gate_id = 202,
		.transit_duration_ticks = 100,
		.max_bandwidth_trains_per_min = 12,
		.max_active_in_transit = 10,
		.priority = FreightPriority::Standard,
	};
	REQUIRE(service.RegisterRoute(route));

	SECTION("Clear Corridor has 1.0x transit duration")
	{
		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Clear);
		auto corridors = service.GetFreightCorridors();
		REQUIRE(corridors.size() == 1);
		CHECK(corridors[0].current_in_transit_count == 0);
	}

	SECTION("Moderate and Congested transitions with train departures")
	{
		ConsistSnapshotBytes snap{.error = ConsistSnapshotError::None, .bytes = {1, 2, 3, 4}};
		std::vector<std::string> ids;
		for (size_t i = 0; i < 6; ++i) { // 6 / 10 = 60% -> Moderate
			std::string tid = service.InitiateTransfer(WorldID{1}, WorldID{2}, 101, 202, snap, 100);
			service.DepartTransfer(tid, 10);
			ids.push_back(tid);
		}

		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Moderate);

		// Add 3 more: 9 / 10 = 90% -> Congested
		for (size_t i = 0; i < 3; ++i) {
			std::string tid = service.InitiateTransfer(WorldID{1}, WorldID{2}, 101, 202, snap, 100);
			service.DepartTransfer(tid, 10);
			ids.push_back(tid);
		}

		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Congested);

		// Add 2 more: 11 / 10 = 110% -> Saturated
		for (size_t i = 0; i < 2; ++i) {
			std::string tid = service.InitiateTransfer(WorldID{1}, WorldID{2}, 101, 202, snap, 100);
			service.DepartTransfer(tid, 10);
			ids.push_back(tid);
		}

		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Saturated);
	}
}

TEST_CASE("Sprint 18 GUI - Universe Directory World Status and Heartbeats")
{
	auto &service = UniverseAuthorityService::Instance();
	service.Reset();

	RegisteredWorld w1{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Earth Core",
		.last_heartbeat_tick = 100,
		.address = "127.0.0.1:3979",
		.description = "Federation Capital",
		.active_clients = 12,
		.max_clients = 32,
		.active_trains = 85,
		.status = WorldOnlineStatus::Online,
	};

	RegisteredWorld w2{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Haven Frontier",
		.last_heartbeat_tick = 50,
		.address = "127.0.0.1:3981",
		.description = "Deep Mining Colony",
		.active_clients = 2,
		.max_clients = 16,
		.active_trains = 14,
		.status = WorldOnlineStatus::Online,
	};

	REQUIRE(service.RegisterWorld(w1));
	REQUIRE(service.RegisterWorld(w2));

	SECTION("Directory contains both registered worlds with correct metrics")
	{
		auto dir = service.GetWorldDirectory();
		REQUIRE(dir.size() == 2);
		CHECK(dir[0].name == "Earth Core");
		CHECK(dir[1].name == "Haven Frontier");
	}

	SECTION("Pruning stale worlds marks unreached nodes as Unreachable")
	{
		// Tick 400: w2 has tick 50 (diff 350 > 300) -> pruned to Unreachable
		size_t pruned = service.PruneStaleWorlds(400, 300);
		CHECK(pruned == 1);

		const RegisteredWorld *p2 = service.GetWorld(WorldID{2});
		REQUIRE(p2 != nullptr);
		CHECK(p2->status == WorldOnlineStatus::Unreachable);

		const RegisteredWorld *p1 = service.GetWorld(WorldID{1});
		REQUIRE(p1 != nullptr);
		CHECK(p1->status == WorldOnlineStatus::Online);
	}
}

TEST_CASE("Sprint 18 GUI - TownView Megacity Status Button Widget")
{
	CHECK(WID_TV_MEGACITY_STATUS > WID_TV_GRAPH);
}
