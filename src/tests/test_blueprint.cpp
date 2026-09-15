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
#include "../rail_cmd.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../depot_base.h"
#include "../portal/fabrication_manager.h"
#include "../portal/tech_tree.h"
#include "../tree_map.h"
#include "../station_base.h"
#include "../town.h"
#include "../station_map.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../vehicle_base.h"
#include "../fileio_func.h"
#include "../economy_func.h"
#include "../strings_func.h"
#include "../language.h"
#include "../saveload/saveload.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../engine_func.h"
#include "../timer/timer_game_calendar.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include <filesystem>
#include <chrono>

static void SetupBlueprintTestEnv(uint32_t map_w = 256, uint32_t map_h = 256)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}
	PortalRegistry::Reset();
	PlanetManager::Reset();
	BlueprintManager::Reset();
	FabricationManager::Reset();
	StockpileManager::Reset();
	TechTreeManager::Reset();
	ResetRailTypes();
	StationClass::Reset();
	_vehicle_pool.CleanPool();
	_depot_pool.CleanPool();
	_station_pool.CleanPool();
	_town_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;

	/* Real station sign formatting needs the build's language pack. */
	if (_current_language == nullptr) {
		extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
		auto saved_paths = _valid_searchpaths;
		auto saved_binary = _searchpaths[Searchpath::BinaryDir];
		_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
		_valid_searchpaths = {Searchpath::BinaryDir};
		InitializeLanguagePacks();
		_valid_searchpaths = std::move(saved_paths);
		_searchpaths[Searchpath::BinaryDir] = std::move(saved_binary);
	}

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);
	_current_company = c->index;
	_local_company = c->index;
	c->money = 1'000'000'000;
	c->avail_railtypes.Set(RAILTYPE_BEGIN);
	c->avail_railtypes.Set(RAILTYPE_ELECTRIC);
	c->clear_limit = 1000 << 16;

	_price[Price::BuildRail] = 100;
	_price[Price::BuildSignals] = 50;
	_price[Price::BuildDepotTrain] = 500;
	_price[Price::BuildStationRail] = 200;
	_price[Price::BuildStationRailLength] = 30;
	_price[Price::BuildFoundation] = 75;
	_price[Price::ClearTrees] = 7;
	_price[Price::ClearSignals] = 15;
	_settings_game.station.station_spread = 64;
	_settings_game.construction.build_on_slopes = true;
	_settings_game.economy.dist_local_authority = 64;
	_settings_game.difficulty.infinite_money = false;
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(15, 15));
	town->name = "Blueprint test town";
	town->townnametype = SPECSTR_TOWNNAME_START;
	RebuildTownKdtree();
	RebuildStationKdtree();

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

	/* Track tile with a native-compatible path signal */
	BlueprintTile t1;
	t1.dx = 1;
	t1.dy = 1;
	t1.type = BlueprintTileType::Track;
	t1.railtype = RAILTYPE_BEGIN;
	t1.trackbits = TrackBits{Track::X};
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
	CHECK(bp.GetTrackPieceCount() == 1);
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
	CHECK(loaded.tiles[0].trackbits == TrackBits{Track::X});
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
	/* X_SW's exit direction transforms to Y_SE with the tile coordinates. */
	CHECK(rot90.tiles[0].signals[0].signals_copy == SignalAlongTrackdir(Trackdir::Y_SE));

	/* Old (1, 0) -> New ((2 - 1) - 0, 1) = (1, 1) */
	CHECK(rot90.tiles[1].dx == 1);
	CHECK(rot90.tiles[1].dy == 1);
	CHECK(rot90.tiles[1].type == BlueprintTileType::Depot);
	CHECK(rot90.tiles[1].dir == DiagDirection::NW); // NE offset rotates to NW

	/* Old (2, 0) -> New ((2 - 1) - 0, 2) = (1, 2) */
	CHECK(rot90.tiles[2].dx == 1);
	CHECK(rot90.tiles[2].dy == 2);
	CHECK(rot90.tiles[2].type == BlueprintTileType::Station);
	CHECK(rot90.tiles[2].axis == Axis::Y); // Axis X + 90 CW = Axis Y

	/* Rotate 360 degrees (4 steps): should equal original */
	Blueprint rot360 = bp;
	for (int step = 0; step < 4; ++step) rot360 = rot360.Rotate(1);
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
	CHECK_FALSE(BlueprintManager::CaptureArea(TileXY(30, 30), TileXY(31, 31)).has_value());
	MakeRailNormal(t2, CompanyID{1}, TrackBits{Track::X}, RAILTYPE_BEGIN);
	CHECK_FALSE(BlueprintManager::CaptureArea(t1, t2, "Foreign capture").has_value());
}

TEST_CASE("Blueprint Manager Storage and Builtin Protection", "[blueprint]")
{
	SetupBlueprintTestEnv();
	extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
	const auto root = std::filesystem::temp_directory_path() / ("ost-bp-legacy-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	REQUIRE(std::filesystem::create_directory(root));
	AutoRestoreBackup personal(_searchpaths[Searchpath::PersonalDir], root.string() + "/");
	AutoRestoreBackup valid_paths(_valid_searchpaths, std::vector<Searchpath>{Searchpath::PersonalDir});
	struct Cleanup {
		std::filesystem::path root;
		~Cleanup() { BlueprintManager::Reset(); std::error_code ec; std::filesystem::remove_all(root, ec); }
	} cleanup{root};
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

TEST_CASE("Blueprint depot quote matches execution", "[blueprint][blueprint-regression]")
{
	SetupBlueprintTestEnv();
	Blueprint bp;
	bp.name = "Depot cost regression";
	bp.width = bp.height = 1;
	BlueprintTile depot;
	depot.type = BlueprintTileType::Depot;
	depot.railtype = RAILTYPE_BEGIN;
	depot.dir = DiagDirection::NE;
	bp.tiles.push_back(depot);
	const TileIndex tile = TileXY(30, 30);
	const auto query = CmdPlaceBlueprint({}, tile, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(query.Succeeded());
	CHECK(!IsRailDepotTile(tile));
	const auto canonical = CmdBuildTrainDepot({}, tile, RAILTYPE_BEGIN, depot.dir);
	REQUIRE(canonical.Succeeded());
	REQUIRE(RailBuildCost(RAILTYPE_BEGIN) == 100);
	CHECK(query.GetCost() == canonical.GetCost());
	const auto execute = CmdPlaceBlueprint(DoCommandFlag::Execute, tile, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(execute.Succeeded());
	CHECK(query.GetCost() == execute.GetCost());
	CHECK(IsRailDepotTile(tile));
}

/* Inspect the complete map, including metadata, when a rejected command must be atomic. */
static std::vector<std::array<uint32_t, 10>> BlueprintMapSnapshot()
{
	std::vector<std::array<uint32_t, 10>> result;
	result.reserve(Map::Size());
	for (TileIndex index{0}; index < Map::Size(); ++index) {
		Tile tile(index);
		result.push_back({tile.type(), tile.height(), tile.m1(), tile.m2(), tile.m3(), tile.m4(), tile.m5(), tile.m6(), tile.m7(), tile.m8()});
	}
	return result;
}

static Blueprint BlueprintStraight(uint16_t length)
{
	Blueprint bp;
	bp.name = "Placement regression";
	bp.width = length;
	bp.height = 1;
	for (uint16_t x = 0; x < length; ++x) {
		BlueprintTile tile;
		tile.dx = x;
		tile.trackbits = TrackBits{Track::X};
		bp.tiles.push_back(tile);
	}
	return bp;
}

TEST_CASE("Blueprint rejected placement preserves map money and materials", "[blueprint][blueprint-regression]")
{
	SetupBlueprintTestEnv();
	Blueprint bp = BlueprintStraight(3);
	const TileIndex origin = TileXY(30, 30);
	Company *company = Company::Get(_current_company);
	bool insufficient_cash = false;
	const CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	const CargoType steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	StockpileManager::AddCargo(WorldID{0}, _current_company, ballast, 5);
	StockpileManager::AddCargo(WorldID{0}, _current_company, steel, 3);

	SECTION("late foreign track") {
		REQUIRE(Company::CanAllocateItem());
		Company *foreign = Company::Create();
		MakeRailNormal(TileXY(32, 30), foreign->index, TrackBits{Track::X}, RAILTYPE_BEGIN);
	}
	SECTION("late incompatible rail") {
		MakeRailNormal(TileXY(32, 30), _current_company, TrackBits{Track::X}, RAILTYPE_ELECTRIC);
	}
	SECTION("late slope with building on slopes disabled") {
		_settings_game.construction.build_on_slopes = false;
		SetTileHeight(TileXY(33, 31), 1);
	}
	SECTION("late invalid signal") {
		bp.tiles.front().type = BlueprintTileType::Depot;
		bp.tiles[1].type = BlueprintTileType::Station;
		BlueprintSignal signal;
		signal.track = Track::Y;
		bp.tiles.back().signals.push_back(signal);
	}
	SECTION("aggregate materials short by one") {
		FabricationManager::SetFabricateFromStockpile(_current_company, true);
	}
	SECTION("aggregate cash short by one") {
		company->money = 299;
		insufficient_cash = true;
	}
	SECTION("frontier forbids maglev") {
		company->avail_railtypes.Set(RAILTYPE_MAGLEV);
		REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase3_Frontier));
		for (auto &tile : bp.tiles) tile.railtype = RAILTYPE_MAGLEV;
	}

	const auto before = BlueprintMapSnapshot();
	const Money money = company->money;
	const size_t depots = Depot::GetNumItems();
	const size_t stations = Station::GetNumItems();
	const auto query = CmdPlaceBlueprint({}, origin, bp.ToJson(), INVALID_RAILTYPE, false);
	CHECK(query.Failed() == !insufficient_cash);
	CHECK((BlueprintMapSnapshot() == before));
	CHECK_FALSE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp.ToJson(), INVALID_RAILTYPE, false));
	const auto execute = CmdPlaceBlueprint(DoCommandFlag::Execute, origin, bp.ToJson(), INVALID_RAILTYPE, false);
	CHECK(execute.Failed());
	CHECK((BlueprintMapSnapshot() == before));
	CHECK(company->money == money);
	CHECK(Depot::GetNumItems() == depots);
	CHECK(Station::GetNumItems() == stations);
	CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, ballast) == 5);
	CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, steel) == 3);
}

TEST_CASE("Blueprint Mainline placement clears trees with exact fabrication materials", "[blueprint][blueprint-regression]")
{
	const bool fabricate = GENERATE(false, true);
	SetupBlueprintTestEnv();
	BlueprintManager::Initialize();
	const Blueprint *bp = BlueprintManager::FindBuiltin("CST Mainline Double Straight");
	REQUIRE(bp != nullptr);
	const TileIndex origin = TileXY(30, 30);
	MakeTree(origin, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);
	const auto ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	const auto steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	const auto wiring = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);
	StockpileManager::AddCargo(WorldID{0}, _current_company, ballast, 32);
	StockpileManager::AddCargo(WorldID{0}, _current_company, steel, 18);
	StockpileManager::AddCargo(WorldID{0}, _current_company, wiring, 2);
	FabricationManager::SetFabricateFromStockpile(_current_company, fabricate);
	const auto before = BlueprintMapSnapshot();
	const Money money = Company::Get(_current_company)->money;
	const auto rating = Town::Get(TownID{0})->ratings[_current_company];
	const auto query = CmdPlaceBlueprint({}, origin, bp->ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(query.Succeeded());
	CHECK(query.GetCost() == (fabricate ? 361 : 1721));
	CHECK((BlueprintMapSnapshot() == before));
	CHECK(Town::Get(TownID{0})->ratings[_current_company] == rating);
	REQUIRE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp->ToJson(), RAILTYPE_BEGIN, false));
	CHECK(Company::Get(_current_company)->money == money - query.GetCost());
	CHECK(Town::Get(TownID{0})->ratings[_current_company] == rating + RATING_TREE_DOWN_STEP);
	for (const auto &piece : bp->tiles) {
		const auto tile = TileAddWrap(origin, piece.dx, piece.dy);
		REQUIRE(IsPlainRailTile(tile));
		CHECK(GetTrackBits(tile) == piece.trackbits);
	}
	CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, ballast) == (fabricate ? 0 : 32));
	CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, steel) == (fabricate ? 0 : 18));
	CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, wiring) == (fabricate ? 0 : 2));
}

TEST_CASE("Blueprint dispatcher charges quote exactly once", "[blueprint][blueprint-regression]")
{
	SetupBlueprintTestEnv();
	Blueprint bp = BlueprintStraight(1);
	bp.tiles.front().type = BlueprintTileType::Depot;
	const TileIndex origin = TileXY(30, 30);
	const Money before = Company::Get(_current_company)->money;
	const auto query = CmdPlaceBlueprint({}, origin, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(query.Succeeded());
	REQUIRE(query.GetCost() == 600);
	REQUIRE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp.ToJson(), RAILTYPE_BEGIN, false));
	CHECK(IsRailDepotTile(origin));
	CHECK(Company::Get(_current_company)->money == before - query.GetCost());
	REQUIRE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp.ToJson(), RAILTYPE_BEGIN, false));
	CHECK(Company::Get(_current_company)->money == before - query.GetCost());
}

TEST_CASE("Blueprint unsupported station layouts reject without changes", "[blueprint][blueprint-regression]")
{
	const int layout = GENERATE(0, 1, 2, 3, 4);
	CAPTURE(layout);
	SetupBlueprintTestEnv();
	Blueprint bp = BlueprintStraight(3);
	for (auto &tile : bp.tiles) tile.type = BlueprintTileType::Station;
	const TileIndex origin = TileXY(30, 30);
	switch (layout) {
		case 0: // Irregular L-shaped group.
			bp.width = bp.height = 2;
			bp.tiles.back().dx = 0;
			bp.tiles.back().dy = 1;
			break;
		case 1: // Two separate new station groups.
			bp.width = 4;
			bp.tiles.back().dx = 3;
			break;
		case 2: // Custom station specification.
			bp.tiles.front().spec_index = 1;
			break;
		case 3: // Trees under a new station.
			MakeTree(origin, TREE_TEMPERATE, 0, TreeGrowthStage::Grown, TreeGround::Grass, 3);
			break;
		case 4: { // Part of the rectangle already belongs to a station.
			Blueprint existing = bp;
			existing.tiles.resize(1);
			existing.width = 1;
			REQUIRE(CmdPlaceBlueprint(DoCommandFlag::Execute, origin, existing.ToJson(), RAILTYPE_BEGIN, false).Succeeded());
			break;
		}
	}
	const auto before = BlueprintMapSnapshot();
	const auto stations = Station::GetNumItems();
	const auto money = Company::Get(_current_company)->money;
	CHECK(CmdPlaceBlueprint({}, origin, bp.ToJson(), RAILTYPE_BEGIN, false).Failed());
	CHECK(CmdPlaceBlueprint(DoCommandFlag::Execute, origin, bp.ToJson(), RAILTYPE_BEGIN, false).Failed());
	CHECK((BlueprintMapSnapshot() == before));
	CHECK(Station::GetNumItems() == stations);
	CHECK(Company::Get(_current_company)->money == money);
}

TEST_CASE("Blueprint clearing foundation and signal replacement use canonical costs", "[blueprint][blueprint-regression]")
{
	SetupBlueprintTestEnv();
	Blueprint bp = BlueprintStraight(1);
	const TileIndex origin = TileXY(30, 30);
	CommandCost canonical;
	SECTION("three trees") {
		MakeTree(origin, TREE_TEMPERATE, 2, TreeGrowthStage::Grown, TreeGround::Grass, 3);
		canonical = CmdBuildSingleRail(DoCommandFlag::Auto, origin, RAILTYPE_BEGIN, Track::X, false);
		REQUIRE(canonical.GetCost() == 121);
	}
	SECTION("depot foundation") {
		bp.tiles.front().type = BlueprintTileType::Depot;
		bp.tiles.front().dir = DiagDirection::NE;
		SetTileHeight(origin, 1);
		canonical = CmdBuildTrainDepot(DoCommandFlag::Auto, origin, RAILTYPE_BEGIN, DiagDirection::NE);
		REQUIRE(canonical.GetCost() == 675);
	}
	SECTION("signal variant conversion") {
		MakeRailNormal(origin, _current_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
		REQUIRE(CmdBuildSingleSignal(DoCommandFlag::Execute, origin, Track::X, SignalType::Block, SignalVariant::Semaphore,
			false, false, false, SignalType::Block, SignalType::Block, 0, SignalOnTrack(Track::X)).Succeeded());
		BlueprintSignal signal;
		signal.sigtype = SignalType::PathOneWay;
		signal.sigvar = SignalVariant::Electric;
		signal.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
		bp.tiles.front().signals.push_back(signal);
		canonical = CmdBuildSingleSignal({}, origin, Track::X, signal.sigtype, signal.sigvar,
			false, false, false, SignalType::Block, SignalType::Block, 0, signal.signals_copy);
		REQUIRE(canonical.GetCost() == 65);
	}
	REQUIRE(canonical.Succeeded());
	const auto before = BlueprintMapSnapshot();
	const auto query = CmdPlaceBlueprint({}, origin, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(query.Succeeded());
	CHECK((BlueprintMapSnapshot() == before));
	CHECK(query.GetCost() == canonical.GetCost());
	const auto execute = CmdPlaceBlueprint(DoCommandFlag::Execute, origin, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(execute.Succeeded());
	CHECK(execute.GetCost() == canonical.GetCost());
	if (!bp.tiles.front().signals.empty()) {
		CHECK(GetSignalVariant(origin, Track::X) == SignalVariant::Electric);
		CHECK(GetSignalType(origin, Track::X) == SignalType::PathOneWay);
		CHECK((GetPresentSignals(origin) & SignalOnTrack(Track::X)) == SignalAlongTrackdir(Trackdir::X_NE));
	}
}

TEST_CASE("Blueprint infrastructure money and materials survive save reload", "[blueprint][blueprint-regression]")
{
	SetupBlueprintTestEnv();
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
	Blueprint bp = BlueprintStraight(3);
	bp.height = 2;
	bp.tiles.back().type = BlueprintTileType::Depot;
	BlueprintSignal signal;
	signal.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
	bp.tiles.front().signals.push_back(signal);
	for (int16_t x = 0; x < 2; ++x) {
		BlueprintTile station;
		station.type = BlueprintTileType::Station;
		station.dx = x;
		station.dy = 1;
		bp.tiles.push_back(station);
	}
	const CompanyID company = _current_company;
	/* Availability is rebuilt from engines on load; provide a real conventional
	 * engine as well as the manually enabled construction types in the fixture. */
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	StartupEngines();
	REQUIRE(HasRailTypeAvail(company, RAILTYPE_BEGIN));
	const TileIndex origin = TileXY(30, 30);
	FabricationManager::SetFabricateFromStockpile(company, true);
	for (auto role : {FabricationRole::Ballast, FabricationRole::StructuralMetal, FabricationRole::Wiring}) {
		StockpileManager::AddCargo(WorldID{0}, company, StockpileManager::RoleToDefaultCargo(role), 100);
	}
	REQUIRE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp.ToJson(), RAILTYPE_BEGIN, false));
	const auto money = Company::Get(company)->money;
	const auto ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	const auto steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	const auto wiring = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);
	CHECK(StockpileManager::GetStock(WorldID{0}, company, ballast) == 91);
	CHECK(StockpileManager::GetStock(WorldID{0}, company, steel) == 87);
	CHECK(StockpileManager::GetStock(WorldID{0}, company, wiring) == 99);
	const auto dir = std::filesystem::temp_directory_path() /
		("openspacettd-blueprint-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	REQUIRE(std::filesystem::create_directory(dir));
	const auto path = dir / "roundtrip.sav";
	REQUIRE(SaveOrLoad(path.string(), SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(path.string(), SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	/* Without a video driver, AfterLoadGame skips the graphics/NewGRF path that
	 * creates engines. Restore the vanilla catalogue through its normal rules;
	 * this test verifies infrastructure/ledger persistence, not engine loading. */
	SetupEngines();
	StartupEngines();
	_current_company = _local_company = company;
	REQUIRE(HasRailTypeAvail(company, RAILTYPE_BEGIN));
	CHECK(Company::Get(company)->money == money);
	CHECK(FabricationManager::IsFabricateFromStockpileEnabled(company));
	CHECK(StockpileManager::GetStock(WorldID{0}, company, ballast) == 91);
	CHECK(StockpileManager::GetStock(WorldID{0}, company, steel) == 87);
	CHECK(StockpileManager::GetStock(WorldID{0}, company, wiring) == 99);
	REQUIRE(IsPlainRailTile(origin));
	CHECK(HasSignalOnTrack(origin, Track::X));
	CHECK((GetPresentSignals(origin) & SignalOnTrack(Track::X)) == signal.signals_copy);
	REQUIRE(IsRailDepotTile(TileXY(32, 30)));
	CHECK(GetRailDepotDirection(TileXY(32, 30)) == DiagDirection::NE);
	REQUIRE(IsRailStationTile(TileXY(30, 31)));
	REQUIRE(IsRailStationTile(TileXY(31, 31)));
	CHECK(GetStationIndex(TileXY(30, 31)) == GetStationIndex(TileXY(31, 31)));
	const auto overlap = CmdPlaceBlueprint({}, origin, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(overlap.Succeeded());
	CHECK(overlap.GetCost() == 0);
	std::filesystem::remove(path);
	std::filesystem::remove(dir);
}
