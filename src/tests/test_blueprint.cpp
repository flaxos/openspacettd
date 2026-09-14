/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_blueprint.cpp Comprehensive unit tests for player rail blueprints, serialization, transforms, and deterministic placement. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../command_func.h"
#include "../landscape_cmd.h"
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

#include <filesystem>

static void SetupBlueprintTestEnv(uint32_t map_w = 256, uint32_t map_h = 256)
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

	/* World 0: (10..100, 10..100) */
	PlanetRegion w0{
		.id = WorldID{0},
		.name = "Terra Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10,
		.min_y = 10,
		.max_x = 100,
		.max_y = 100,
		.development_score = 10000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w0));
}

TEST_CASE("Blueprint Data Model and JSON Round-Trip", "[blueprint]")
{
	Blueprint bp;
	bp.name = "Test Terminal Junction";
	bp.description = "Test layout for serialization verification.";
	bp.author = "Test Engineer";
	bp.version = 1;
	bp.width = 6;
	bp.height = 4;
	bp.is_builtin = false;
	bp.created_time = 123456789;

	/* Track tile with cross tracks and path signal */
	BlueprintTile t1;
	t1.dx = 1;
	t1.dy = 1;
	t1.type = BlueprintTileType::Track;
	t1.railtype = RAILTYPE_BEGIN;
	t1.trackbits = TrackBits{Track::X, Track::Y};
	BlueprintSignal s1;
	s1.track = Track::X;
	s1.sigtype = SignalType::PathOneWay;
	s1.sigvar = SignalVariant::Electric;
	s1.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
	t1.signals.push_back(s1);
	bp.tiles.push_back(t1);

	/* Depot tile */
	BlueprintTile t2;
	t2.dx = 3;
	t2.dy = 1;
	t2.type = BlueprintTileType::Depot;
	t2.railtype = RAILTYPE_BEGIN;
	t2.dir = DiagDirection::SE;
	bp.tiles.push_back(t2);

	/* Station tile */
	BlueprintTile t3;
	t3.dx = 4;
	t3.dy = 2;
	t3.type = BlueprintTileType::Station;
	t3.railtype = RAILTYPE_BEGIN;
	t3.axis = Axis::Y;
	t3.spec_class = STAT_CLASS_DFLT;
	t3.spec_index = 0;
	bp.tiles.push_back(t3);

	REQUIRE(bp.IsValid());
	CHECK(bp.GetTileCount() == 3);
	CHECK(bp.GetTrackPieceCount() == 2);
	CHECK(bp.GetSignalCount() == 1);
	CHECK(bp.GetDepotCount() == 1);
	CHECK(bp.GetStationCount() == 1);

	/* Serialize to JSON */
	std::string json_str = bp.ToJson();
	REQUIRE(!json_str.empty());

	/* Deserialize */
	auto loaded_opt = Blueprint::FromJson(json_str);
	REQUIRE(loaded_opt.has_value());
	const Blueprint &loaded = *loaded_opt;

	CHECK(loaded.name == bp.name);
	CHECK(loaded.description == bp.description);
	CHECK(loaded.author == bp.author);
	CHECK(loaded.version == bp.version);
	CHECK(loaded.width == bp.width);
	CHECK(loaded.height == bp.height);
	CHECK(loaded.tiles.size() == bp.tiles.size());

	/* Verify tile 0 */
	CHECK(loaded.tiles[0].dx == 1);
	CHECK(loaded.tiles[0].dy == 1);
	CHECK(loaded.tiles[0].type == BlueprintTileType::Track);
	CHECK(loaded.tiles[0].trackbits == TrackBits{Track::X, Track::Y});
	REQUIRE(loaded.tiles[0].signals.size() == 1);
	CHECK(loaded.tiles[0].signals[0].track == Track::X);
	CHECK(loaded.tiles[0].signals[0].sigtype == SignalType::PathOneWay);
	CHECK(loaded.tiles[0].signals[0].signals_copy == s1.signals_copy);

	/* Verify tile 1 (Depot) */
	CHECK(loaded.tiles[1].dx == 3);
	CHECK(loaded.tiles[1].dy == 1);
	CHECK(loaded.tiles[1].type == BlueprintTileType::Depot);
	CHECK(loaded.tiles[1].dir == DiagDirection::SE);

	/* Verify tile 2 (Station) */
	CHECK(loaded.tiles[2].dx == 4);
	CHECK(loaded.tiles[2].dy == 2);
	CHECK(loaded.tiles[2].type == BlueprintTileType::Station);
	CHECK(loaded.tiles[2].axis == Axis::Y);
}

TEST_CASE("Blueprint Rotation 90, 180, 270 and Invariance", "[blueprint]")
{
	Blueprint bp;
	bp.name = "Asymmetric Module";
	bp.width = 4;
	bp.height = 2;

	/* (0, 0): Track X with signal facing SW */
	BlueprintTile t1;
	t1.dx = 0;
	t1.dy = 0;
	t1.type = BlueprintTileType::Track;
	t1.trackbits = TrackBits{Track::X};
	BlueprintSignal s1;
	s1.track = Track::X;
	s1.sigtype = SignalType::PathOneWay;
	s1.sigvar = SignalVariant::Electric;
	s1.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
	t1.signals.push_back(s1);
	bp.tiles.push_back(t1);

	/* (1, 0): Depot facing NE */
	BlueprintTile t2;
	t2.dx = 1;
	t2.dy = 0;
	t2.type = BlueprintTileType::Depot;
	t2.dir = DiagDirection::NE;
	bp.tiles.push_back(t2);

	/* (2, 0): Station on Axis X */
	BlueprintTile t3;
	t3.dx = 2;
	t3.dy = 0;
	t3.type = BlueprintTileType::Station;
	t3.axis = Axis::X;
	bp.tiles.push_back(t3);

	/* Rotate 90 deg clockwise */
	Blueprint rot90 = bp.Rotate(1);
	CHECK(rot90.width == 2);
	CHECK(rot90.height == 4);
	REQUIRE(rot90.tiles.size() == 3);

	/* Old (0, 0) -> New ((2 - 1) - 0, 0) = (1, 0) */
	CHECK(rot90.tiles[0].dx == 1);
	CHECK(rot90.tiles[0].dy == 0);
	CHECK(rot90.tiles[0].type == BlueprintTileType::Track);
	CHECK(rot90.tiles[0].trackbits == TrackBits{Track::Y});
	REQUIRE(rot90.tiles[0].signals.size() == 1);
	CHECK(rot90.tiles[0].signals[0].track == Track::Y);
	/* Trackdir X_SW rotated 90 CW faces NW on Y-axis (Trackdir::Y_NW) */
	CHECK(rot90.tiles[0].signals[0].signals_copy == SignalAlongTrackdir(Trackdir::Y_NW));

	/* Old (1, 0) -> New ((2 - 1) - 0, 1) = (1, 1) */
	CHECK(rot90.tiles[1].dx == 1);
	CHECK(rot90.tiles[1].dy == 1);
	CHECK(rot90.tiles[1].type == BlueprintTileType::Depot);
	CHECK(rot90.tiles[1].dir == DiagDirection::SE); // NE + 90 CW = SE

	/* Old (2, 0) -> New ((2 - 1) - 0, 2) = (1, 2) */
	CHECK(rot90.tiles[2].dx == 1);
	CHECK(rot90.tiles[2].dy == 2);
	CHECK(rot90.tiles[2].type == BlueprintTileType::Station);
	CHECK(rot90.tiles[2].axis == Axis::Y); // Axis X + 90 CW = Axis Y

	/* Rotate 360 degrees (4 steps): should equal original */
	Blueprint rot360 = bp.Rotate(4);
	CHECK(rot360.width == bp.width);
	CHECK(rot360.height == bp.height);
	REQUIRE(rot360.tiles.size() == bp.tiles.size());
	for (size_t i = 0; i < bp.tiles.size(); ++i) {
		CHECK(rot360.tiles[i].dx == bp.tiles[i].dx);
		CHECK(rot360.tiles[i].dy == bp.tiles[i].dy);
		CHECK(rot360.tiles[i].type == bp.tiles[i].type);
		if (bp.tiles[i].type == BlueprintTileType::Track) {
			CHECK(rot360.tiles[i].trackbits == bp.tiles[i].trackbits);
			if (!bp.tiles[i].signals.empty()) {
				CHECK(rot360.tiles[i].signals[0].signals_copy == bp.tiles[i].signals[0].signals_copy);
			}
		} else if (bp.tiles[i].type == BlueprintTileType::Depot) {
			CHECK(rot360.tiles[i].dir == bp.tiles[i].dir);
		} else if (bp.tiles[i].type == BlueprintTileType::Station) {
			CHECK(rot360.tiles[i].axis == bp.tiles[i].axis);
		}
	}
}

TEST_CASE("Blueprint Horizontal Flip/Mirror", "[blueprint]")
{
	Blueprint bp;
	bp.width = 3;
	bp.height = 2;

	BlueprintTile t1;
	t1.dx = 1;
	t1.dy = 0;
	t1.type = BlueprintTileType::Track;
	t1.trackbits = TrackBits{Track::Left};
	bp.tiles.push_back(t1);

	BlueprintTile t2;
	t2.dx = 2;
	t2.dy = 1;
	t2.type = BlueprintTileType::Depot;
	t2.dir = DiagDirection::NE;
	bp.tiles.push_back(t2);

	Blueprint mirrored = bp.Mirror();
	CHECK(mirrored.width == 2);
	CHECK(mirrored.height == 3);

	/* dx and dy swapped */
	CHECK(mirrored.tiles[0].dx == 0);
	CHECK(mirrored.tiles[0].dy == 1);
	CHECK(mirrored.tiles[0].trackbits == TrackBits{Track::Right}); // Left <-> Right

	CHECK(mirrored.tiles[1].dx == 1);
	CHECK(mirrored.tiles[1].dy == 2);
	CHECK(mirrored.tiles[1].dir == DiagDirection::NW); // NE <-> NW

	/* Mirror twice returns to original */
	Blueprint double_mirrored = mirrored.Mirror();
	CHECK(double_mirrored.width == bp.width);
	CHECK(double_mirrored.height == bp.height);
	CHECK(double_mirrored.tiles[0].dx == bp.tiles[0].dx);
	CHECK(double_mirrored.tiles[0].dy == bp.tiles[0].dy);
	CHECK(double_mirrored.tiles[0].trackbits == bp.tiles[0].trackbits);
	CHECK(double_mirrored.tiles[1].dir == bp.tiles[1].dir);
}

TEST_CASE("Blueprint Capture from Game Map", "[blueprint]")
{
	SetupBlueprintTestEnv();

	TileIndex t1 = TileXY(20, 20);
	TileIndex t2 = TileXY(21, 20);

	/* Build track on map */
	MakeRailNormal(t1, _current_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
	MakeRailNormal(t2, _current_company, TrackBits{Track::X}, RAILTYPE_BEGIN);

	/* Add signal on t1 */
	SetHasSignals(t1, true);
	SetPresentSignals(t1, SignalAlongTrackdir(Trackdir::X_SW));
	SetSignalStates(t1, 0xF & ~SignalAlongTrackdir(Trackdir::X_SW));
	SetSignalType(t1, Track::X, SignalType::PathOneWay);
	SetSignalVariant(t1, Track::X, SignalVariant::Electric);

	/* Capture the 2x1 area */
	auto bp_opt = BlueprintManager::CaptureArea(t1, t2, "Test Capture");
	REQUIRE(bp_opt.has_value());
	const Blueprint &bp = *bp_opt;

	CHECK(bp.name == "Test Capture");
	CHECK(bp.width == 2);
	CHECK(bp.height == 1);
	CHECK(bp.tiles.size() == 2);
	CHECK(bp.GetTrackPieceCount() == 2);
	CHECK(bp.GetSignalCount() == 1);

	/* Verify t1 had signal */
	CHECK(bp.tiles[0].signals.size() == 1);
	CHECK(bp.tiles[0].signals[0].sigtype == SignalType::PathOneWay);
}

TEST_CASE("Blueprint Manager Storage and Builtin Protection", "[blueprint]")
{
	SetupBlueprintTestEnv();
	BlueprintManager::Initialize();

	const auto &bps = BlueprintManager::GetBlueprints();
	REQUIRE(!bps.empty());

	/* CST prefab built-in is present */
	CHECK(bps[0].is_builtin == true);
	CHECK(bps[0].name.find("CST") != std::string::npos);

	/* Built-in cannot be deleted */
	CHECK(BlueprintManager::DeleteBlueprint(0) == false);

	/* Built-in cannot be renamed */
	CHECK(BlueprintManager::RenameBlueprint(0, "Illegal Rename") == false);

	/* Save a player blueprint */
	Blueprint player_bp;
	player_bp.name = "My Custom Loop";
	player_bp.width = 4;
	player_bp.height = 4;
	BlueprintTile pt;
	pt.dx = 0; pt.dy = 0;
	pt.type = BlueprintTileType::Track;
	pt.trackbits = TrackBits{Track::X};
	player_bp.tiles.push_back(pt);

	REQUIRE(BlueprintManager::SaveBlueprint(player_bp));

	/* Verify player blueprint is now in library */
	size_t new_idx = BlueprintManager::GetBlueprints().size() - 1;
	const Blueprint *saved = BlueprintManager::GetBlueprint(new_idx);
	REQUIRE(saved != nullptr);
	CHECK(saved->name == "My Custom Loop");
	CHECK(saved->is_builtin == false);

	/* Export to string */
	std::string exported = BlueprintManager::ExportToString(new_idx);
	REQUIRE(!exported.empty());

	/* Delete player blueprint */
	CHECK(BlueprintManager::DeleteBlueprint(new_idx) == true);
}

TEST_CASE("Deterministic Blueprint Placement Command", "[blueprint]")
{
	SetupBlueprintTestEnv();

	Blueprint bp;
	bp.name = "Straight Track Stamp";
	bp.width = 3;
	bp.height = 1;

	for (int16_t x = 0; x < 3; ++x) {
		BlueprintTile bt;
		bt.dx = x;
		bt.dy = 0;
		bt.type = BlueprintTileType::Track;
		bt.railtype = RAILTYPE_BEGIN;
		bt.trackbits = TrackBits{Track::X};
		if (x == 1) {
			BlueprintSignal sig;
			sig.track = Track::X;
			sig.sigtype = SignalType::PathOneWay;
			sig.sigvar = SignalVariant::Electric;
			sig.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
			bt.signals.push_back(sig);
		}
		bp.tiles.push_back(bt);
	}

	std::string json_bp = bp.ToJson();
	TileIndex origin = TileXY(30, 30);

	/* Dry-run cost query without Execute flag */
	CommandCost query = CmdPlaceBlueprint({}, origin, json_bp, RAILTYPE_BEGIN, false);
	REQUIRE(query.Succeeded());
	CHECK(query.GetCost() > 0);
	/* Verify tile was not modified by dry-run */
	CHECK(!IsPlainRailTile(origin));

	/* Invalid JSON failure check */
	CommandCost bad_json = CmdPlaceBlueprint({}, origin, "invalid json {]", RAILTYPE_BEGIN, false);
	CHECK(bad_json.Failed());

	/* Edge boundary rejection */
	CommandCost edge_fail = CmdPlaceBlueprint({}, TileXY(255, 255), json_bp, RAILTYPE_BEGIN, false);
	CHECK(edge_fail.Failed());

	/* Void buffer rejection */
	TileIndex void_tile = TileXY(5, 5); // (5, 5) is void buffer outside world 0 (10..100)
	CommandCost void_fail = CmdPlaceBlueprint({}, void_tile, json_bp, RAILTYPE_BEGIN, false);
	CHECK(void_fail.Failed());

	/* Execute placement */
	CommandCost exec = CmdPlaceBlueprint(DoCommandFlag::Execute, origin, json_bp, RAILTYPE_BEGIN, false);
	REQUIRE(exec.Succeeded());
	CHECK(exec.GetCost() > 0);

	/* Verify placed infrastructure on map */
	TileIndex t0 = TileXY(30, 30);
	TileIndex t1 = TileXY(31, 30);
	TileIndex t2 = TileXY(32, 30);

	CHECK(IsPlainRailTile(t0));
	CHECK(HasTrack(t0, Track::X));
	CHECK(IsPlainRailTile(t1));
	CHECK(HasTrack(t1, Track::X));
	CHECK(HasSignals(t1));
	CHECK(GetSignalType(t1, Track::X) == SignalType::PathOneWay);
	CHECK(IsPlainRailTile(t2));
	CHECK(HasTrack(t2, Track::X));

	/* Idempotent overlapping placement: stamp exact same blueprint again */
	CommandCost overlap = CmdPlaceBlueprint(DoCommandFlag::Execute, origin, json_bp, RAILTYPE_BEGIN, false);
	REQUIRE(overlap.Succeeded());
	/* Zero additional cost for already built identical pieces */
	CHECK(overlap.GetCost() == 0);
}
