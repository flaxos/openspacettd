/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_cst_prefabs.cpp Unit and integration tests for the 8 CST Prefab Rail Blocks. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../command_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../blueprint/blueprint.h"
#include "../blueprint/blueprint_manager.h"
#include "../blueprint/blueprint_cmd.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../vehicle_base.h"
#include "../fileio_func.h"
#include "../economy_func.h"
#include "../table/strings.h"
#include "mock_environment.h"

static void SetupCSTPrefabTestEnv(uint32_t map_w = 256, uint32_t map_h = 256)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	PortalRegistry::Reset();
	PlanetManager::Reset();
	BlueprintManager::Reset();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);
	_current_company = c->index;
	c->money = 1'000'000'000;
	c->avail_railtypes.Set(RAILTYPE_BEGIN);
	c->avail_railtypes.Set(RAILTYPE_ELECTRIC);
	c->clear_limit = 1000 << 16;

	_price[Price::BuildRail] = 100;
	_price[Price::BuildSignals] = 50;
	_price[Price::BuildDepotTrain] = 500;
	_price[Price::BuildStationRail] = 200;

	/* World 0 across entire test area */
	PlanetRegion w0{
		.id = WorldID{0},
		.name = "Terra Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = map_w - 1,
		.max_y = map_h - 1,
	};
	PlanetManager::RegisterRegion(w0);
}

TEST_CASE("CST Prefabs - Complete 8-Layout Catalogue Registration", "[cst_prefab]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();

	REQUIRE(BlueprintManager::GetBuiltinCount() == 8);

	const std::vector<std::string> expected_prefabs = {
		"CST Mainline Double Straight",
		"CST Dual-Track Passing Siding",
		"CST Portal Gate Approach Corridor",
		"CST High-Speed 3-Way Wye Junction",
		"CST 4-Way Compact Roundabout Junction",
		"CST Ro-Ro 4-Platform Terminal Station Block",
		"CST Industrial Bulk Balloon Loop",
		"CST Depot Maintenance Staging Yard",
	};

	for (const auto &name : expected_prefabs) {
		const Blueprint *bp = BlueprintManager::FindBuiltin(name);
		REQUIRE(bp != nullptr);
		CHECK(bp->is_builtin == true);
		CHECK(bp->author == "Commonwealth Synergy Transport (CST)");
		CHECK(!bp->description.empty());
		CHECK(bp->IsValid());
		CHECK(!bp->tiles.empty());
	}
}

TEST_CASE("CST Prefabs - Structural & Functional Integrity", "[cst_prefab]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();

	/* 1. Mainline Double Straight (8x2) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Mainline Double Straight");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 8);
		CHECK(bp->height == 2);
		CHECK(bp->GetTrackPieceCount() == 16);
		CHECK(bp->GetSignalCount() == 2);
		CHECK(bp->GetStationCount() == 0);
		CHECK(bp->GetDepotCount() == 0);
	}

	/* 2. Passing Siding (14x4) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Dual-Track Passing Siding");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 14);
		CHECK(bp->height == 4);
		CHECK(bp->GetTrackPieceCount() >= 38);
		CHECK(bp->GetSignalCount() >= 3);
	}

	/* 3. Portal Gate Approach (10x4) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Portal Gate Approach Corridor");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 10);
		CHECK(bp->height == 4);
		CHECK(bp->GetTrackPieceCount() == 22);
		CHECK(bp->GetSignalCount() == 4);
	}

	/* 4. High-Speed 3-Way Wye (12x12) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST High-Speed 3-Way Wye Junction");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 12);
		CHECK(bp->height == 12);
		CHECK(bp->GetTrackPieceCount() >= 34);
		CHECK(bp->GetSignalCount() >= 8);
	}

	/* 5. 4-Way Compact Roundabout (10x10) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST 4-Way Compact Roundabout Junction");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 10);
		CHECK(bp->height == 10);
		CHECK(bp->GetTrackPieceCount() >= 36);
		CHECK(bp->GetSignalCount() >= 8);
	}

	/* 6. Ro-Ro 4-Platform Terminal Station (12x8) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Ro-Ro 4-Platform Terminal Station Block");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 12);
		CHECK(bp->height == 8);
		CHECK(bp->GetStationCount() == 24); // 4 platforms * 6 tiles
		CHECK(bp->GetSignalCount() >= 5);
	}

	/* 7. Industrial Bulk Balloon Loop (14x10) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Industrial Bulk Balloon Loop");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 14);
		CHECK(bp->height == 10);
		CHECK(bp->GetStationCount() == 10); // 2 platforms * 5 tiles
		CHECK(bp->GetSignalCount() >= 3);
	}

	/* 8. Depot Maintenance Staging Yard (10x6) */
	{
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Depot Maintenance Staging Yard");
		REQUIRE(bp != nullptr);
		CHECK(bp->width == 10);
		CHECK(bp->height == 6);
		CHECK(bp->GetDepotCount() == 2);
		CHECK(bp->GetSignalCount() >= 3);
	}
}

TEST_CASE("CST Prefabs - Geometric Transforms & RHD/LHD Invariance", "[cst_prefab]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();

	for (size_t i = 0; i < BlueprintManager::GetBuiltinCount(); ++i) {
		const Blueprint *orig = BlueprintManager::GetBlueprint(i);
		REQUIRE(orig != nullptr);

		/* 4 x 90° CW rotations = exact original identity */
		Blueprint r90 = orig->Rotate(1);
		CHECK(r90.width == orig->height);
		CHECK(r90.height == orig->width);
		CHECK(r90.tiles.size() == orig->tiles.size());

		Blueprint r180 = orig->Rotate(2);
		CHECK(r180.width == orig->width);
		CHECK(r180.height == orig->height);

		Blueprint r270 = orig->Rotate(3);
		CHECK(r270.width == orig->height);
		CHECK(r270.height == orig->width);

		Blueprint r360 = orig->Rotate(4);
		CHECK(r360.width == orig->width);
		CHECK(r360.height == orig->height);
		CHECK(r360.tiles.size() == orig->tiles.size());
		CHECK(r360.GetTrackPieceCount() == orig->GetTrackPieceCount());
		CHECK(r360.GetSignalCount() == orig->GetSignalCount());
		CHECK(r360.GetStationCount() == orig->GetStationCount());
		CHECK(r360.GetDepotCount() == orig->GetDepotCount());

		/* Double horizontal reflection (RHD <-> LHD) = identity */
		Blueprint flipped = orig->Mirror();
		CHECK(flipped.tiles.size() == orig->tiles.size());
		CHECK(flipped.GetTrackPieceCount() == orig->GetTrackPieceCount());
		CHECK(flipped.GetSignalCount() == orig->GetSignalCount());
		CHECK(flipped.GetStationCount() == orig->GetStationCount());
		CHECK(flipped.GetDepotCount() == orig->GetDepotCount());

		Blueprint double_flipped = flipped.Mirror();
		CHECK(double_flipped.width == orig->width);
		CHECK(double_flipped.height == orig->height);
		CHECK(double_flipped.tiles.size() == orig->tiles.size());
	}
}

TEST_CASE("CST Prefabs - Deterministic In-Game Map Placement", "[cst_prefab]")
{
	SetupCSTPrefabTestEnv();
	BlueprintManager::Reset();
	BlueprintManager::Initialize();

	/* Place each of the 8 CST prefabs on separate quadrants of the map */
	uint origin_x = 20;
	uint origin_y = 20;

	for (size_t i = 0; i < BlueprintManager::GetBuiltinCount(); ++i) {
		const Blueprint *bp = BlueprintManager::GetBlueprint(i);
		REQUIRE(bp != nullptr);

		TileIndex origin = TileXY(origin_x, origin_y);

		/* Dry run cost check */
		CommandCost dry_run = CmdPlaceBlueprint({}, origin, bp->ToJson(), RAILTYPE_BEGIN, false);
		REQUIRE(dry_run.Succeeded());
		CHECK(dry_run.GetCost() > 0);

		/* Execution placement check */
		CommandCost exec = CmdPlaceBlueprint(DoCommandFlag::Execute, origin, bp->ToJson(), RAILTYPE_BEGIN, false);
		REQUIRE(exec.Succeeded());
		CHECK(exec.GetCost() > 0);

		/* Verify at least one tile was placed with plain rail, depot, or station */
		TileIndex check_tile = TileAddWrap(origin, bp->tiles[0].dx, bp->tiles[0].dy);
		bool valid_infrastructure = IsPlainRailTile(check_tile) || IsRailStationTile(check_tile) || IsRailDepotTile(check_tile);
		CHECK(valid_infrastructure);

		origin_x += 25;
		if (origin_x > 200) {
			origin_x = 20;
			origin_y += 30;
		}
	}
}

TEST_CASE("CST Prefabs - Immutability & Builtin Deletion Protection", "[cst_prefab]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();

	for (size_t i = 0; i < BlueprintManager::GetBuiltinCount(); ++i) {
		/* Built-in prefabs cannot be deleted */
		CHECK(BlueprintManager::DeleteBlueprint(i) == false);

		/* Built-in prefabs cannot be renamed */
		CHECK(BlueprintManager::RenameBlueprint(i, "Hacked Prefab Name") == false);
	}

	CHECK(BlueprintManager::GetBuiltinCount() == 8);
}
