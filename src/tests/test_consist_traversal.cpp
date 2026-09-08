/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_consist_traversal.cpp Unit and integration tests for consist portal traversal. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../tunnelbridge.h"
#include "../rail_map.h"
#include "../portal/portal_registry.h"
#include "../portal/planet_manager.h"
#include "../train.h"
#include "../vehicle_base.h"
#include "../company_base.h"
#include "../settings_type.h"
#include "../table/sprites.h"
#include "mock_environment.h"

#include "../safeguards.h"

/* Functions from train_cmd.cpp */
bool TrainController(Train *v, Vehicle *nomove, bool reverse = true);

TEST_CASE("ConsistTraversal - Progress Tracking API")
{
	PortalRegistry::Reset();

	VehicleID vid1 = VehicleID{10};
	VehicleID vid2 = VehicleID{25};

	CHECK(PortalRegistry::GetPortalTransitProgress(vid1) == 0);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid2) == 0);

	CHECK(PortalRegistry::AdvancePortalTransit(vid1) == 1);
	CHECK(PortalRegistry::AdvancePortalTransit(vid1) == 2);
	CHECK(PortalRegistry::AdvancePortalTransit(vid1) == 3);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid1) == 3);

	CHECK(PortalRegistry::AdvancePortalTransit(vid2) == 1);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid2) == 1);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid1) == 3);

	PortalRegistry::ClearPortalTransit(vid1);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid1) == 0);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid2) == 1);

	PortalRegistry::Reset();
	CHECK(PortalRegistry::GetPortalTransitProgress(vid2) == 0);
}

TEST_CASE("ConsistTraversal - Single Locomotive Portal Emergence")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	PlanetManager::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	_settings_game.pf.path_backoff_interval = 1;

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	TileIndex portal_a = TileXY(10, 10);
	TileIndex portal_b = TileXY(50, 50);
	uint32_t virt_len = 2; // 32 units to traverse

	MakeRailTunnel(portal_a, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_b, Owner(0), DiagDirection::SW, RAILTYPE_BEGIN);

	/* Outgoing track from portal_b (moving NE from (50, 50) enters (49, 50)) */
	MakeRailNormal(TileXY(49, 50), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(TileXY(48, 50), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterPortalPair(
		portal_a, DiagDirection::NE, WorldID{0},
		portal_b, DiagDirection::SW, WorldID{1},
		virt_len, true
	);
	REQUIRE(pid != INVALID_PORTAL);

	REQUIRE(Vehicle::CanAllocateItem(1));
	Train *t = Vehicle::Create<Train>();
	t->SetFrontEngine();
	t->owner = Owner(0);
	t->direction = Direction::NE;
	t->tile = portal_a;
	t->track = Track::Wormhole;
	t->vehstatus.Set(VehState::Hidden);
	t->gcache.cached_veh_length = 8;
	t->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);
	t->compatible_railtypes = RailTypes{RAILTYPE_BEGIN};
	t->railtypes = RailTypes{RAILTYPE_BEGIN};

	uint32_t target_units = virt_len * TILE_SIZE; // 32 units

	/* Advance locomotive inside wormhole */
	for (uint32_t step = 1; step < target_units; step++) {
		TrainController(t, nullptr);
		CHECK(t->track == Track::Wormhole);
		CHECK(t->tile == portal_a);
		CHECK(t->vehstatus.Test(VehState::Hidden));
		CHECK(PortalRegistry::GetPortalTransitProgress(t->index) == step);
	}

	/* Step 32: should trigger emergence at portal_b! */
	TrainController(t, nullptr);

	CHECK(t->tile == portal_b);
	CHECK(t->track == Track::X);
	CHECK(t->direction == Direction::NE);
	CHECK(!t->vehstatus.Test(VehState::Hidden));
	CHECK(TileVirtXY(t->x_pos, t->y_pos) == portal_b);
	CHECK(PortalRegistry::GetPortalTransitProgress(t->index) == 0);

	/* Step further: locomotive rolls forward onto destination world track */
	TrainController(t, nullptr);

	CHECK(t->track == Track::X);
	CHECK(!t->vehstatus.Test(VehState::Hidden));

	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
	PortalRegistry::Reset();
	PlanetManager::Reset();
}

TEST_CASE("ConsistTraversal - Multi-Wagon Traversal Across Worlds")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	PlanetManager::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	_settings_game.pf.path_backoff_interval = 1;

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	/* World 0: (0, 0) to (30, 30) Core */
	PlanetRegion core_region;
	core_region.id = WorldID{0};
	core_region.name = "Core World";
	core_region.phase = WorldPhase::Phase1_Core;
	core_region.min_x = 0;
	core_region.min_y = 0;
	core_region.max_x = 30;
	core_region.max_y = 30;
	REQUIRE(PlanetManager::RegisterRegion(core_region));

	/* World 1: (32, 32) to (63, 63) Frontier */
	PlanetRegion frontier_region;
	frontier_region.id = WorldID{1};
	frontier_region.name = "Frontier World";
	frontier_region.phase = WorldPhase::Phase3_Frontier;
	frontier_region.min_x = 32;
	frontier_region.min_y = 32;
	frontier_region.max_x = 63;
	frontier_region.max_y = 63;
	REQUIRE(PlanetManager::RegisterRegion(frontier_region));

	TileIndex portal_a = TileXY(10, 10); // World 0
	TileIndex portal_b = TileXY(50, 50); // World 1
	uint32_t virt_len = 1; // 16 units virtual length

	/* Make tunnels and tracks */
	MakeRailTunnel(portal_a, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_b, Owner(0), DiagDirection::SW, RAILTYPE_BEGIN);

	/* Approach track into portal_a (moving NE from (11, 10)) */
	MakeRailNormal(TileXY(11, 10), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(TileXY(12, 10), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(TileXY(13, 10), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);

	/* Exit track out of portal_b (moving NE from (50, 50)) */
	MakeRailNormal(TileXY(49, 50), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(TileXY(48, 50), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(TileXY(47, 50), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(TileXY(46, 50), Owner(0), TrackBits{Track::X}, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterPortalPair(
		portal_a, DiagDirection::NE, WorldID{0},
		portal_b, DiagDirection::SW, WorldID{1},
		virt_len, true
	);
	REQUIRE(pid != INVALID_PORTAL);

	/* Create 3-car consist: Engine -> Wagon 1 -> Wagon 2 */
	REQUIRE(Vehicle::CanAllocateItem(3));
	Train *e  = Vehicle::Create<Train>();
	Train *w1 = Vehicle::Create<Train>();
	Train *w2 = Vehicle::Create<Train>();

	e->SetFrontEngine();
	w1->ClearFrontEngine();
	w2->ClearFrontEngine();

	e->owner  = Owner(0);
	w1->owner = Owner(0);
	w2->owner = Owner(0);

	e->direction  = Direction::NE;
	w1->direction = Direction::NE;
	w2->direction = Direction::NE;

	e->gcache.cached_veh_length  = 8;
	w1->gcache.cached_veh_length = 8;
	w2->gcache.cached_veh_length = 8;

	e->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);
	w1->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);
	w2->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);

	e->compatible_railtypes  = RailTypes{RAILTYPE_BEGIN};
	w1->compatible_railtypes = RailTypes{RAILTYPE_BEGIN};
	w2->compatible_railtypes = RailTypes{RAILTYPE_BEGIN};

	e->railtypes  = RailTypes{RAILTYPE_BEGIN};
	w1->railtypes = RailTypes{RAILTYPE_BEGIN};
	w2->railtypes = RailTypes{RAILTYPE_BEGIN};

	/* Position Engine inside portal_a wormhole */
	e->tile = portal_a;
	e->track = Track::Wormhole;
	e->vehstatus.Set(VehState::Hidden);
	e->x_pos = TileX(portal_a) * TILE_SIZE + 3; // Entrance visibility frame NE
	e->y_pos = TileY(portal_a) * TILE_SIZE + 8;
	e->z_pos = GetSlopePixelZ(e->x_pos, e->y_pos, true);

	/* Position Wagon 1 on approach track at TileXY(11, 10), exactly 8 units behind engine */
	w1->tile = TileXY(11, 10);
	w1->track = Track::X;
	w1->vehstatus = {};
	w1->x_pos = e->x_pos + 8; // Moving NE (decreasing X), so behind means higher X
	w1->y_pos = e->y_pos;
	w1->z_pos = GetSlopePixelZ(w1->x_pos, w1->y_pos, true);

	/* Position Wagon 2 on approach track, 8 units behind w1 */
	w2->tile = TileXY(11, 10);
	w2->track = Track::X;
	w2->vehstatus = {};
	w2->x_pos = w1->x_pos + 8;
	w2->y_pos = w1->y_pos;
	w2->z_pos = GetSlopePixelZ(w2->x_pos, w2->y_pos, true);

	/* Link consist */
	e->SetNext(w1);
	w1->SetNext(w2);

	/* Step through the portal until all 3 vehicles emerge at portal_b */
	bool e_emerged = false;
	bool w1_emerged = false;
	bool w2_emerged = false;

	for (int step = 0; step < 50; step++) {
		TrainController(e, nullptr);

		/* CheckTrainsLengths must never assert during transit */
		CHECK_NOTHROW(CheckTrainsLengths());

		if (!e_emerged && e->tile == portal_b && !e->vehstatus.Test(VehState::Hidden)) {
			e_emerged = true;
		}
		if (!w1_emerged && w1->tile == portal_b && !w1->vehstatus.Test(VehState::Hidden)) {
			w1_emerged = true;
			/* Wagon 1 must emerge only after Engine has emerged */
			CHECK(e_emerged);
		}
		if (!w2_emerged && w2->tile == portal_b && !w2->vehstatus.Test(VehState::Hidden)) {
			w2_emerged = true;
			/* Wagon 2 must emerge only after Wagon 1 has emerged */
			CHECK(w1_emerged);
		}

		if (e_emerged && w1_emerged && w2_emerged) break;
	}

	CHECK(e_emerged);
	CHECK(w1_emerged);
	CHECK(w2_emerged);

	/* Clean up */
	e->SetNext(nullptr);
	w1->SetNext(nullptr);
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();
	PortalRegistry::Reset();
	PlanetManager::Reset();
}

TEST_CASE("ConsistTraversal - Vehicle Destruction Clears Transit Tracker")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();

	REQUIRE(Vehicle::CanAllocateItem(1));
	Train *t = Vehicle::Create<Train>();
	VehicleID vid = t->index;

	PortalRegistry::AdvancePortalTransit(vid);
	CHECK(PortalRegistry::GetPortalTransitProgress(vid) == 1);

	/* CleanPool triggers vehicle destructors */
	_vehicle_pool.CleanPool();

	CHECK(PortalRegistry::GetPortalTransitProgress(vid) == 0);
	PortalRegistry::Reset();
}
