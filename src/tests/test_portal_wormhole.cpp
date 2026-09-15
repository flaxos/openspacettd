/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_portal_wormhole.cpp Unit tests for wormhole portal registry and pathfinding integration. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../tunnelbridge.h"
#include "../rail_map.h"
#include "../pathfinder/follow_track.hpp"
#include "../pathfinder/yapf/yapf_rail_portal_heuristic.hpp"
#include "../pathfinder/yapf/yapf_common.hpp"
#include "../portal/portal_registry.h"
#include "../train.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

TEST_CASE("Portal rail estimates retain normal distances and use chained shortcuts", "[portal][yapf-regression]")
{
	Map::Allocate(256, 256);
	PortalRegistry::Reset();
	const TileIndex a = TileXY(20, 20);
	const TileIndex b = TileXY(100, 100);
	const TileIndex c = TileXY(100, 103);
	const TileIndex d = TileXY(220, 220);
	const TileIndex destination = TileXY(220, 230);
	YapfRailPortalHeuristic estimate;
	estimate.SetDestination(destination);
	CHECK(estimate.Empty());
	CHECK(estimate.Estimate(a, Trackdir::X_SW) == OctileDistanceCost(a, Trackdir::X_SW, destination));
	for (bool reverse_registration : {false, true}) {
		PortalRegistry::Reset();
		auto first = [&]() { REQUIRE(PortalRegistry::RegisterPortalPair(a, DiagDirection::SW, WorldID{0},
			b, DiagDirection::NW, WorldID{1}, 1, false) != INVALID_PORTAL); };
		auto second = [&]() { REQUIRE(PortalRegistry::RegisterPortalPair(c, DiagDirection::SE, WorldID{1},
			d, DiagDirection::NW, WorldID{2}, 1, false) != INVALID_PORTAL); };
		if (reverse_registration) { second(); first(); } else { first(); second(); }
		estimate.SetDestination(destination);
		CHECK_FALSE(estimate.Empty());
		/* Two 200-cost jumps, 300 between their heads, then 900 to the target. */
		CHECK(estimate.Estimate(a, Trackdir::X_SW) == 1600);
		estimate.SetDestination(a);
		CHECK(estimate.Estimate(d, Trackdir::Y_NW) == OctileDistanceCost(d, Trackdir::Y_NW, a));
	}
	PortalRegistry::Reset();
	estimate.SetDestination(destination);
	CHECK(estimate.Empty());
	CHECK(estimate.Estimate(a, Trackdir::X_SW) == OctileDistanceCost(a, Trackdir::X_SW, destination));
}

TEST_CASE("Portal rail estimates remain consistent over rail steps and every head orientation", "[portal][yapf-regression]")
{
	Map::Allocate(128, 128);
	const uint32_t length = GENERATE(0, 1, 25);
	const DiagDirection entering = GENERATE(DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW);
	const DiagDirection other_entering = GENERATE(DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW);
	PortalRegistry::Reset();
	const TileIndex a = TileXY(20, 20);
	const TileIndex b = TileXY(100, 100);
	REQUIRE(PortalRegistry::RegisterPortalPair(a, entering, WorldID{0}, b, other_entering, WorldID{1}, length, true) != INVALID_PORTAL);
	const uint32_t canonical_length = PortalRegistry::GetPortalVirtualLength(a);
	CHECK(canonical_length == std::max(uint32_t{1}, length));
	YapfRailPortalHeuristic estimate;
	estimate.SetDestination(TileXY(105, 105));
	CHECK(estimate.Estimate(a, DiagDirToDiagTrackdir(entering)) <=
		static_cast<int>((canonical_length + 1) * YAPF_TILE_LENGTH) + estimate.Estimate(b, DiagDirToDiagTrackdir(ReverseDiagDir(other_entering))));
	CHECK(estimate.Estimate(b, DiagDirToDiagTrackdir(other_entering)) <=
		static_cast<int>((canonical_length + 1) * YAPF_TILE_LENGTH) + estimate.Estimate(a, DiagDirToDiagTrackdir(ReverseDiagDir(entering))));
	for (TileIndex tile : {TileXY(19, 20), a, TileXY(21, 20), b, TileXY(104, 105)}) {
		for (Trackdir td : TrackdirBits{TRACKDIR_BIT_MASK}) {
			const DiagDirection exit = TrackdirToExitdir(td);
			const TileIndex next = TileAddByDiagDir(tile, exit);
			for (Track track : {Track::X, Track::Y, Track::Upper, Track::Lower, Track::Left, Track::Right}) {
				Trackdir next_td = TrackExitdirToTrackdir(track, ReverseDiagDir(exit));
				if (next_td == Trackdir::Invalid) continue;
				next_td = ReverseTrackdir(next_td);
				const int cost = IsDiagonalTrack(track) ? YAPF_TILE_LENGTH : YAPF_TILE_CORNER_LENGTH;
				CHECK(estimate.Estimate(tile, td) <= cost + estimate.Estimate(next, next_td));
			}
		}
	}
	PortalRegistry::Reset();
}

TEST_CASE("PortalRegistry - Basic Pairing and Lookups")
{
	PortalRegistry::Reset();
	REQUIRE(PortalRegistry::Count() == 0);

	TileIndex tile_a = TileIndex{100};
	TileIndex tile_b = TileIndex{50000};
	uint32_t virt_len = 25;

	PortalID pid = PortalRegistry::RegisterPortalPair(
		tile_a, DiagDirection::NE, WorldID{0},
		tile_b, DiagDirection::SW, WorldID{1},
		virt_len, true
	);

	REQUIRE(pid != INVALID_PORTAL);
	REQUIRE(PortalRegistry::Count() == 1);

	/* Verify membership */
	CHECK(PortalRegistry::IsPortalTile(tile_a));
	CHECK(PortalRegistry::IsPortalTile(tile_b));
	CHECK(!PortalRegistry::IsPortalTile(TileIndex{999}));
	CHECK(!PortalRegistry::IsPortalTile(INVALID_TILE));

	/* Verify bidirectional resolution */
	CHECK(PortalRegistry::GetOtherPortalEnd(tile_a) == tile_b);
	CHECK(PortalRegistry::GetOtherPortalEnd(tile_b) == tile_a);
	CHECK(PortalRegistry::GetOtherPortalEnd(TileIndex{999}) == INVALID_TILE);

	/* Verify virtual length */
	CHECK(PortalRegistry::GetPortalVirtualLength(tile_a) == virt_len);
	CHECK(PortalRegistry::GetPortalVirtualLength(tile_b) == virt_len);

	/* Verify link data */
	const PortalLink *link = PortalRegistry::GetPortalLink(tile_a);
	REQUIRE(link != nullptr);
	CHECK(link->id == pid);
	CHECK(link->end_a.world_id == WorldID{0});
	CHECK(link->end_b.world_id == WorldID{1});
	CHECK(link->virtual_length == virt_len);

	/* Verify unregistration */
	CHECK(PortalRegistry::UnregisterPortal(pid));
	CHECK(PortalRegistry::Count() == 0);
	CHECK(!PortalRegistry::IsPortalTile(tile_a));
	CHECK(!PortalRegistry::IsPortalTile(tile_b));
	CHECK(PortalRegistry::GetOtherPortalEnd(tile_a) == INVALID_TILE);

	PortalRegistry::Reset();
}

TEST_CASE("PortalRegistry - Multi-World Topologies and Validation")
{
	PortalRegistry::Reset();

	/* Cannot register invalid or duplicate tiles */
	CHECK(PortalRegistry::RegisterPortalPair(
		INVALID_TILE, DiagDirection::NE, WorldID{0},
		TileIndex{200}, DiagDirection::SW, WorldID{1}
	) == INVALID_PORTAL);

	CHECK(PortalRegistry::RegisterPortalPair(
		TileIndex{100}, DiagDirection::NE, WorldID{0},
		TileIndex{100}, DiagDirection::SW, WorldID{1}
	) == INVALID_PORTAL);

	/* Pair 1: World 0 (Home) <-> World 1 (Mining World) */
	PortalID p1 = PortalRegistry::RegisterPortalPair(
		TileIndex{100}, DiagDirection::NE, WorldID{0},
		TileIndex{200}, DiagDirection::SW, WorldID{1},
		10
	);
	REQUIRE(p1 != INVALID_PORTAL);

	/* Cannot reuse an already registered tile */
	CHECK(PortalRegistry::RegisterPortalPair(
		TileIndex{100}, DiagDirection::NW, WorldID{0},
		TileIndex{300}, DiagDirection::SE, WorldID{2}
	) == INVALID_PORTAL);

	/* Pair 2: World 1 <-> World 2 (High-Tech Colony) */
	PortalID p2 = PortalRegistry::RegisterPortalPair(
		TileIndex{250}, DiagDirection::NW, WorldID{1},
		TileIndex{350}, DiagDirection::SE, WorldID{2},
		50
	);
	REQUIRE(p2 != INVALID_PORTAL);
	CHECK(PortalRegistry::Count() == 2);

	/* Verify isolation between different portal links */
	CHECK(PortalRegistry::GetOtherPortalEnd(TileIndex{100}) == TileIndex{200});
	CHECK(PortalRegistry::GetOtherPortalEnd(TileIndex{200}) == TileIndex{100});
	CHECK(PortalRegistry::GetOtherPortalEnd(TileIndex{250}) == TileIndex{350});
	CHECK(PortalRegistry::GetOtherPortalEnd(TileIndex{350}) == TileIndex{250});

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Engine Hook Integration (GetOtherTunnelBridgeEnd & Length)")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	/* Set up two non-colinear, non-adjacent tiles */
	TileIndex tile_a = TileXY(10, 10);
	TileIndex tile_b = TileXY(50, 45); // Completely diagonal and distant
	uint32_t virt_len = 42;

	/* Configure them as rail tunnel tiles */
	MakeRailTunnel(tile_a, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(tile_b, Owner(0), DiagDirection::SW, RAILTYPE_BEGIN);

	/* Register portal pair connecting them */
	PortalID pid = PortalRegistry::RegisterPortalPair(
		tile_a, DiagDirection::NE, WorldID{0},
		tile_b, DiagDirection::SW, WorldID{1},
		virt_len
	);
	REQUIRE(pid != INVALID_PORTAL);

	/* Verify GetOtherTunnelBridgeEnd resolves portal end across diagonal space */
	CHECK(GetOtherTunnelBridgeEnd(tile_a) == tile_b);
	CHECK(GetOtherTunnelBridgeEnd(tile_b) == tile_a);

	/* Verify GetOtherTunnelEnd resolves portal end */
	CHECK(GetOtherTunnelEnd(tile_a) == tile_b);
	CHECK(GetOtherTunnelEnd(tile_b) == tile_a);

	/* Verify GetTunnelBridgeLength returns virtual traversal length */
	CHECK(GetTunnelBridgeLength(tile_a, tile_b) == virt_len);
	CHECK(GetTunnelBridgeLength(tile_b, tile_a) == virt_len);

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - YAPF Track Follower Traversal")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	TileIndex portal_a = TileXY(15, 20);
	TileIndex portal_b = TileXY(45, 55); // Different quadrant / logical world
	uint32_t virt_len = 18;

	/* Make portal entrance and exit tiles as rail tunnels */
	MakeRailTunnel(portal_a, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_b, Owner(0), DiagDirection::SW, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterPortalPair(
		portal_a, DiagDirection::NE, WorldID{0},
		portal_b, DiagDirection::SW, WorldID{1},
		virt_len
	);
	REQUIRE(pid != INVALID_PORTAL);

	struct TestRailFollower : public CFollowTrackRail {
		using CFollowTrackRail::CFollowTrackRail;
		using CFollowTrackRail::FollowTileExit;
	};

	/* Create a rail track follower simulating YAPF advancing into portal_a */
	TestRailFollower ft(Owner(0), RailTypes{RAILTYPE_BEGIN});
	ft.old_tile = portal_a;
	ft.exitdir = DiagDirection::NE; // Entering portal_a facing NE

	/* Execute follower exit resolution */
	ft.FollowTileExit();

	/* Verify pathfinder recognized tunnel wormhole */
	CHECK(ft.is_tunnel);
	CHECK(!ft.is_bridge);

	/* Verify pathfinder jumped directly to distant portal_b */
	CHECK(ft.new_tile == portal_b);

	/* Verify pathfinder assigned configured virtual length for routing cost */
	CHECK(ft.tiles_skipped == static_cast<int>(virt_len));

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Unlinked gate is a closed rail terminal")
{
	struct TestRailFollower : public CFollowTrackRail {
		using CFollowTrackRail::CFollowTrackRail;
		using CFollowTrackRail::QueryNewTileTrackStatus;
	};

	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	const DiagDirection dir = DiagDirection::NE;
	const TileIndex gate = TileXY(20, 20);
	const TileIndex approach = TileAddByDiagDir(gate, ReverseDiagDir(dir));
	MakeRailTunnel(gate, Owner(0), dir, RAILTYPE_BEGIN);
	MakeRailNormal(approach, Owner(0), TrackBits{DiagDirToDiagTrack(dir)}, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate, dir, WorldID{0}));

	/* A moving train and route follower must stop before entering the head. */
	CHECK(GetTileTrackStatus(gate, TransportType::Rail, RoadTramType::Invalid, ReverseDiagDir(dir)).trackdirs.None());
	TestRailFollower follower(Owner(0), RailTypes{RAILTYPE_BEGIN});
	CHECK_FALSE(follower.Follow(approach, DiagDirToDiagTrackdir(dir)));
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);

	/* Following directly from the head resolves no remote tile without dereferencing it. */
	CHECK_FALSE(follower.Follow(gate, DiagDirToDiagTrackdir(dir)));
	CHECK(follower.new_tile == INVALID_TILE);
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);

	/* Corrupt or stale endpoints beyond the map boundary are rejected too. */
	follower.new_tile = TileIndex{Map::Size()};
	CHECK_FALSE(follower.QueryNewTileTrackStatus());
	CHECK(follower.new_td_bits.None());
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Stale non-tunnel endpoint is rejected")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	const DiagDirection dir = DiagDirection::NE;
	const TileIndex entry = TileXY(20, 20);
	const TileIndex approach = TileAddByDiagDir(entry, ReverseDiagDir(dir));
	const TileIndex stale_remote = TileXY(40, 40);
	MakeRailTunnel(entry, Owner(0), dir, RAILTYPE_BEGIN);
	MakeRailNormal(approach, Owner(0), TrackBits{DiagDirToDiagTrack(dir)}, RAILTYPE_BEGIN);
	REQUIRE_FALSE(IsTunnelTile(stale_remote));
	REQUIRE(PortalRegistry::RegisterPortalPair(
		entry, dir, WorldID{0},
		stale_remote, DiagDirection::SW, WorldID{1},
		18
	) != INVALID_PORTAL);

	CHECK(GetTileTrackStatus(entry, TransportType::Rail, RoadTramType::Invalid, ReverseDiagDir(dir)).trackdirs.None());
	CFollowTrackRail follower(Owner(0), RailTypes{RAILTYPE_BEGIN});
	CHECK_FALSE(follower.Follow(approach, DiagDirToDiagTrackdir(dir)));
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);
	CHECK_FALSE(follower.Follow(entry, DiagDirToDiagTrackdir(dir)));
	CHECK(follower.new_tile == INVALID_TILE);
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Crashing a stale train on an unlinked head clears its reservation")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();

	const TileIndex gate = TileXY(20, 20);
	MakeRailTunnel(gate, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(gate, DiagDirection::NE, WorldID{0}));
	SetTunnelBridgeReservation(gate, true);

	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	train->SetFrontEngine();
	train->owner = Owner(0);
	train->tile = gate;
	train->track = Track::X;
	train->direction = Direction::SW;
	train->flags.Set(VehicleRailFlag::Stuck);

	CHECK(train->Crash(false) == 2);
	CHECK(train->vehstatus.Test(VehState::Crashed));
	CHECK_FALSE(HasTunnelBridgeReservation(gate));

	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();
}

TEST_CASE("Portal Wormhole - Routing follows every exit orientation with rail turns disabled")
{
	const auto dir_a = GENERATE(DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW);
	const auto dir_b = GENERATE(DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW);
	CAPTURE(dir_a, dir_b);
	Map::Allocate(64, 64);
	PortalRegistry::Reset();
	const TileIndex a = TileXY(16, 16);
	const TileIndex b = TileXY(48, 48);
	MakeRailTunnel(a, OWNER_NONE, dir_a, RAILTYPE_BEGIN);
	MakeRailTunnel(b, OWNER_NONE, dir_b, RAILTYPE_BEGIN);
	for (auto [tile, dir] : {std::pair{a, dir_a}, std::pair{b, dir_b}}) {
		MakeRailNormal(TileAddByDiagDir(tile, ReverseDiagDir(dir)), OWNER_NONE, TrackBits{DiagDirToDiagTrack(dir)}, RAILTYPE_BEGIN);
	}
	REQUIRE(PortalRegistry::RegisterPortalPair(a, dir_a, WorldID{0}, b, dir_b, WorldID{1}, 32) != INVALID_PORTAL);
	for (auto [entry, exit, dir] : {std::tuple{a, b, dir_a}, std::tuple{b, a, dir_b}}) {
		CFollowTrackRailNo90 follower(Owner{0}, RailTypes{RAILTYPE_BEGIN});
		REQUIRE(follower.Follow(entry, DiagDirToDiagTrackdir(dir)));
		CHECK(follower.new_tile == exit);
		CHECK(follower.tiles_skipped == 32);
		const auto exit_dir = ReverseDiagDir(GetTunnelBridgeDirection(exit));
		CHECK(follower.new_td_bits == TrackdirBits{DiagDirToDiagTrackdir(exit_dir)});
		REQUIRE(follower.Follow(exit, FindFirstTrackdir(follower.new_td_bits)));
		CHECK(follower.new_tile == TileAddByDiagDir(exit, exit_dir));
	}
	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Neutral Generated Gateway Is Shared Rail")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	constexpr Owner company{0};
	TileIndex owned_a = TileXY(18, 20);
	TileIndex approach_a = TileXY(19, 20);
	TileIndex portal_a = TileXY(20, 20);
	TileIndex portal_b = TileXY(40, 20);
	TileIndex approach_b = TileXY(41, 20);
	TileIndex owned_b = TileXY(42, 20);

	MakeRailNormal(owned_a, company, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(approach_a, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_a, OWNER_NONE, DiagDirection::SW, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_b, OWNER_NONE, DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailNormal(approach_b, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(owned_b, company, TrackBits{Track::X}, RAILTYPE_BEGIN);

	REQUIRE(PortalRegistry::RegisterPortalPair(
		portal_a, DiagDirection::SW, WorldID{0},
		portal_b, DiagDirection::NE, WorldID{1},
		18
	) != INVALID_PORTAL);

	CFollowTrackRail follower(company, RailTypes{RAILTYPE_BEGIN});
	Trackdir southwest = DiagDirToDiagTrackdir(DiagDirection::SW);
	REQUIRE(follower.Follow(owned_a, southwest));
	CHECK(follower.new_tile == approach_a);
	REQUIRE(follower.Follow(approach_a, southwest));
	CHECK(follower.new_tile == portal_a);
	REQUIRE(follower.Follow(portal_a, southwest));
	CHECK(follower.new_tile == portal_b);
	REQUIRE(follower.Follow(portal_b, southwest));
	CHECK(follower.new_tile == approach_b);
	REQUIRE(follower.Follow(approach_b, southwest));
	CHECK(follower.new_tile == owned_b);

	Trackdir northeast = DiagDirToDiagTrackdir(DiagDirection::NE);
	REQUIRE(follower.Follow(owned_b, northeast));
	CHECK(follower.new_tile == approach_b);
	REQUIRE(follower.Follow(approach_b, northeast));
	CHECK(follower.new_tile == portal_b);
	REQUIRE(follower.Follow(portal_b, northeast));
	CHECK(follower.new_tile == portal_a);
	REQUIRE(follower.Follow(portal_a, northeast));
	CHECK(follower.new_tile == approach_a);
	REQUIRE(follower.Follow(approach_a, northeast));
	CHECK(follower.new_tile == owned_a);

	TileIndex owned_control = TileXY(18, 22);
	TileIndex competitor_track = TileXY(19, 22);
	MakeRailNormal(owned_control, company, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(competitor_track, Owner{1}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	CHECK_FALSE(follower.Follow(owned_control, southwest));

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Legacy Generated Gateway Direction Repair")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	TileIndex portal_a = TileXY(20, 20);
	TileIndex lead_a = TileXY(19, 20);
	TileIndex portal_b = TileXY(40, 20);
	TileIndex lead_b = TileXY(41, 20);

	/* Early generated saves stored directions opposite to the world-side lead
	 * tracks: NE points left at A and SW points right at B. */
	MakeRailNormal(lead_a, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_a, OWNER_NONE, DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_b, OWNER_NONE, DiagDirection::SW, RAILTYPE_BEGIN);
	MakeRailNormal(lead_b, OWNER_NONE, TrackBits{Track::X}, RAILTYPE_BEGIN);

	REQUIRE(PortalRegistry::RegisterPortalPair(
		portal_a, DiagDirection::NE, WorldID{0},
		portal_b, DiagDirection::SW, WorldID{1},
		18
	) != INVALID_PORTAL);

	CHECK(PortalRegistry::RepairLegacyGeneratedGateways() == 2);
	CHECK(GetTunnelBridgeDirection(portal_a) == DiagDirection::SW);
	CHECK(GetTunnelBridgeDirection(portal_b) == DiagDirection::NE);

	const PortalLink *link = PortalRegistry::GetPortalLink(portal_a);
	REQUIRE(link != nullptr);
	CHECK(link->end_a.enter_dir == DiagDirection::SW);
	CHECK(link->end_b.enter_dir == DiagDirection::NE);

	/* A second pass must leave already-corrected data unchanged. */
	CHECK(PortalRegistry::RepairLegacyGeneratedGateways() == 0);

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Exit Emergence Coordinates Calculation")
{
	Map::Allocate(64, 64);
	PortalRegistry::Reset();

	TileIndex portal_a = TileXY(10, 10);
	TileIndex portal_b = TileXY(50, 40);

	MakeRailTunnel(portal_a, Owner(0), DiagDirection::NE, RAILTYPE_BEGIN);
	MakeRailTunnel(portal_b, Owner(0), DiagDirection::SW, RAILTYPE_BEGIN);

	PortalID pid = PortalRegistry::RegisterPortalPair(
		portal_a, DiagDirection::NE, WorldID{0},
		portal_b, DiagDirection::SW, WorldID{1},
		25
	);
	REQUIRE(pid != INVALID_PORTAL);

	PortalExitPosition exit_pos = PortalRegistry::GetPortalExitPosition(portal_a);
	CHECK(exit_pos.tile == portal_b);
	CHECK(TileVirtXY(exit_pos.x, exit_pos.y) == portal_b);
	CHECK(exit_pos.dir == Direction::NE);
	CHECK(exit_pos.track == Track::X);
	CHECK(exit_pos.z == GetSlopePixelZ(exit_pos.x, exit_pos.y, true));

	PortalRegistry::Reset();
}

TEST_CASE("Portal Wormhole - Consist Distance Decoupling Across Wormhole")
{
	Map::Allocate(64, 64);
	_vehicle_pool.CleanPool();

	/* Verify CheckTrainsLengths with vehicles decoupled by Track::Wormhole */
	REQUIRE(Vehicle::CanAllocateItem(2));
	Train *v1 = Vehicle::Create<Train>();
	Train *v2 = Vehicle::Create<Train>();

	v1->vehstatus = {};
	v1->x_pos = 100;
	v1->y_pos = 100;
	v1->track = Track::X;

	v2->vehstatus = {};
	v2->x_pos = 2000; // Far away across disjoint coordinates
	v2->y_pos = 2000;
	v2->track = Track::Wormhole; // Wagon 2 is in wormhole transit

	/* Link v1 and v2 into moving consist */
	v1->SetNext(v2);

	/* CheckTrainsLengths iterates over Train::Iterate(). When wagons are marked Track::Wormhole,
	 * the Euclidean distance gap across the wormhole must not trigger an assertion. */
	CHECK_NOTHROW(CheckTrainsLengths());

	/* Clean up consist pointers and vehicle pool */
	v1->SetNext(nullptr);
	_vehicle_pool.CleanPool();
}
