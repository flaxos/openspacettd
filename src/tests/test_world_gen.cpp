/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_world_gen.cpp Unit tests for multi-world procedural layout generation, void buffers, and gateway initialization. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/world_gen.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../void_map.h"
#include "../tunnel_map.h"
#include "../rail_map.h"
#include "../water_map.h"

#include "../safeguards.h"

TEST_CASE("MultiWorldGen - Layout Calculation Geometry")
{
	/* Test 256x256 map layout calculation */
	auto regions = MultiWorldGen::CalculateLayout(256, 256);
	REQUIRE(regions.size() == 3);

	const auto &r0 = regions[0];
	const auto &r1 = regions[1];
	const auto &r2 = regions[2];

	/* Check phase classification */
	CHECK(r0.phase == WorldPhase::Phase1_Core);
	CHECK(r1.phase == WorldPhase::Phase2_Developed);
	CHECK(r2.phase == WorldPhase::Phase3_Frontier);

	/* Check bounds are non-inverted and strictly positive */
	CHECK(r0.min_x <= r0.max_x);
	CHECK(r0.min_y <= r0.max_y);
	CHECK(r1.min_x <= r1.max_x);
	CHECK(r1.min_y <= r1.max_y);
	CHECK(r2.min_x <= r2.max_x);
	CHECK(r2.min_y <= r2.max_y);

	/* Check vertical sequence along Y: r0 is North, r1 is Middle, r2 is South */
	CHECK(r0.max_y < r1.min_y);
	CHECK(r1.max_y < r2.min_y);

	/* Buffer gaps must exist and be at least 4 tiles */
	uint32_t buffer_0_1 = r1.min_y - r0.max_y - 1;
	uint32_t buffer_1_2 = r2.min_y - r1.max_y - 1;
	CHECK(buffer_0_1 >= 4);
	CHECK(buffer_1_2 >= 4);

	/* Outer border padding must exist */
	CHECK(r0.min_x >= 2);
	CHECK(r0.min_y >= 2);
	CHECK(r2.max_x <= 253);
	CHECK(r2.max_y <= 253);
}

TEST_CASE("MultiWorldGen - Map Partitioning and Void Isolation")
{
	Map::Allocate(64, 64);

	bool ok = MultiWorldGen::GenerateMultiWorldLayout(64, 64);
	REQUIRE(ok);

	/* Verify PlanetManager registrations */
	REQUIRE(PlanetManager::Count() == 3);

	const PlanetRegion *p0 = PlanetManager::GetRegion(WorldID{0});
	const PlanetRegion *p1 = PlanetManager::GetRegion(WorldID{1});
	const PlanetRegion *p2 = PlanetManager::GetRegion(WorldID{2});

	REQUIRE(p0 != nullptr);
	REQUIRE(p1 != nullptr);
	REQUIRE(p2 != nullptr);

	CHECK(p0->phase == WorldPhase::Phase1_Core);
	CHECK(p1->phase == WorldPhase::Phase2_Developed);
	CHECK(p2->phase == WorldPhase::Phase3_Frontier);

	/* Check center of World 0 is non-void and mapped to World 0 */
	uint32_t c0_x = (p0->min_x + p0->max_x) / 2;
	uint32_t c0_y = (p0->min_y + p0->max_y) / 2;
	TileIndex tile_w0 = TileXY(c0_x, c0_y);

	CHECK(PlanetManager::GetTileWorld(tile_w0) == WorldID{0});
	CHECK(PlanetManager::GetTilePhase(tile_w0) == WorldPhase::Phase1_Core);
	CHECK_FALSE(IsTileType(tile_w0, TileType::Void));

	/* Check buffer between World 0 and World 1 is void and INVALID_WORLD */
	uint32_t buf_y = (p0->max_y + p1->min_y) / 2;
	TileIndex tile_buf = TileXY(c0_x, buf_y);

	CHECK(PlanetManager::GetTileWorld(tile_buf) == INVALID_WORLD);
	CHECK(IsTileType(tile_buf, TileType::Void));

	/* Check center of World 1 */
	uint32_t c1_x = (p1->min_x + p1->max_x) / 2;
	uint32_t c1_y = (p1->min_y + p1->max_y) / 2;
	TileIndex tile_w1 = TileXY(c1_x, c1_y);

	CHECK(PlanetManager::GetTileWorld(tile_w1) == WorldID{1});
	CHECK(PlanetManager::GetTilePhase(tile_w1) == WorldPhase::Phase2_Developed);
	CHECK_FALSE(IsTileType(tile_w1, TileType::Void));

	/* Check outer map borders (x=0, y=0, etc.) are void */
	CHECK(IsTileType(TileXY(0, 0), TileType::Void));
	CHECK(IsTileType(TileXY(0, 32), TileType::Void));
	CHECK(IsTileType(TileXY(32, 0), TileType::Void));
	CHECK(IsTileType(TileXY(63, 63), TileType::Void));
}

TEST_CASE("MultiWorldGen - Gateway Initialization and Pairing")
{
	Map::Allocate(64, 64);

	bool ok = MultiWorldGen::GenerateMultiWorldLayout(64, 64);
	REQUIRE(ok);

	/* 3 worlds should produce 2 gateway pairs (0-1 and 1-2) */
	REQUIRE(PortalRegistry::Count() == 2);

	const PlanetRegion *p0 = PlanetManager::GetRegion(WorldID{0});
	const PlanetRegion *p1 = PlanetManager::GetRegion(WorldID{1});
	const PlanetRegion *p2 = PlanetManager::GetRegion(WorldID{2});

	REQUIRE(p0 != nullptr);
	REQUIRE(p1 != nullptr);
	REQUIRE(p2 != nullptr);

	/* Gateway between World 0 and World 1 */
	TileIndex gw_w0 = TileXY(32, p0->max_y - 1);
	TileIndex gw_w1 = TileXY(32, p1->min_y + 1);

	CHECK(PortalRegistry::IsPortalTile(gw_w0));
	CHECK(PortalRegistry::IsPortalTile(gw_w1));
	CHECK(IsTunnel(gw_w0));
	CHECK(IsTunnel(gw_w1));

	/* Cross-world resolution */
	CHECK(PortalRegistry::GetOtherPortalEnd(gw_w0) == gw_w1);
	CHECK(PortalRegistry::GetOtherPortalEnd(gw_w1) == gw_w0);

	/* Gateway between World 1 and World 2 */
	TileIndex gw_w1_s = TileXY(32, p1->max_y - 1);
	TileIndex gw_w2_n = TileXY(32, p2->min_y + 1);

	CHECK(PortalRegistry::IsPortalTile(gw_w1_s));
	CHECK(PortalRegistry::IsPortalTile(gw_w2_n));
	CHECK(IsTunnel(gw_w1_s));
	CHECK(IsTunnel(gw_w2_n));

	CHECK(PortalRegistry::GetOtherPortalEnd(gw_w1_s) == gw_w2_n);
	CHECK(PortalRegistry::GetOtherPortalEnd(gw_w2_n) == gw_w1_s);

	/* Check track approach tiles exist and have track */
	TileIndex track_w0 = TileXY(32, p0->max_y - 2);
	TileIndex track_w1 = TileXY(32, p1->min_y + 2);
	CHECK(IsPlainRailTile(track_w0));
	CHECK(IsPlainRailTile(track_w1));
	CHECK(GetTrackBits(track_w0) == TrackBits{Track::Y});
	CHECK(GetTrackBits(track_w1) == TrackBits{Track::Y});
}

TEST_CASE("MultiWorldGen - Asymmetric Map Partitioning along X")
{
	/* When size_x > size_y, partitioning must split along X */
	Map::Allocate(128, 64);

	bool ok = MultiWorldGen::GenerateMultiWorldLayout(128, 64);
	REQUIRE(ok);

	REQUIRE(PlanetManager::Count() == 3);
	REQUIRE(PortalRegistry::Count() == 2);

	const PlanetRegion *p0 = PlanetManager::GetRegion(WorldID{0});
	const PlanetRegion *p1 = PlanetManager::GetRegion(WorldID{1});
	const PlanetRegion *p2 = PlanetManager::GetRegion(WorldID{2});

	/* Bounding boxes must advance along X */
	CHECK(p0->max_x < p1->min_x);
	CHECK(p1->max_x < p2->min_x);

	/* Check void buffer along X between World 0 and World 1 */
	uint32_t buf_x = (p0->max_x + p1->min_x) / 2;
	TileIndex tile_buf_x = TileXY(buf_x, 32);

	CHECK(PlanetManager::GetTileWorld(tile_buf_x) == INVALID_WORLD);
	CHECK(IsTileType(tile_buf_x, TileType::Void));

	/* Check gateways along X have X track */
	TileIndex gw_w0 = TileXY(p0->max_x - 1, 32);
	TileIndex gw_w1 = TileXY(p1->min_x + 1, 32);

	CHECK(PortalRegistry::IsPortalTile(gw_w0));
	CHECK(PortalRegistry::IsPortalTile(gw_w1));
	CHECK(PortalRegistry::GetOtherPortalEnd(gw_w0) == gw_w1);

	TileIndex track_w0 = TileXY(p0->max_x - 2, 32);
	CHECK(IsPlainRailTile(track_w0));
	CHECK(GetTrackBits(track_w0) == TrackBits{Track::X});
}

TEST_CASE("MultiWorldGen - Partitioning Preserves Terrain Heights and Shores")
{
	Map::Allocate(64, 64);

	auto regions = MultiWorldGen::CalculateLayout(64, 64);
	REQUIRE(regions.size() == 3);

	/* A valid coast at the lower edge of the first world uses height points
	 * that lie in the future void buffer. */
	TileIndex shore = TileXY(10, regions[0].max_y);
	SetTileHeight(TileXY(10, regions[0].max_y + 1), 1);
	SetTileHeight(TileXY(11, regions[0].max_y + 1), 1);
	MakeShore(shore);
	REQUIRE(GetTileSlope(shore) == SLOPE_SE);

	std::vector<uint8_t> heights;
	heights.reserve(Map::Size());
	for (TileIndex tile : Map::Iterate()) heights.push_back(TileHeight(tile));

	REQUIRE(MultiWorldGen::GenerateMultiWorldLayout(64, 64));

	uint changed_heights = 0;
	for (TileIndex tile : Map::Iterate()) {
		if (TileHeight(tile) != heights[tile.base()]) changed_heights++;
	}

	CHECK(changed_heights == 0);
	CHECK(IsCoastTile(shore));
	CHECK(GetTileSlope(shore) == SLOPE_SE);
}
