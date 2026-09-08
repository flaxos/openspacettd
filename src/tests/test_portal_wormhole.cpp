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
#include "../portal/portal_registry.h"

#include "../safeguards.h"

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
