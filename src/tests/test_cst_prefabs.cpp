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
#include "../rail_cmd.h"
#include "../tunnel_map.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../depot_base.h"
#include "../station_base.h"
#include "../station_cmd.h"
#include "../town.h"
#include "../portal/fabrication_manager.h"
#include "../portal/tech_tree.h"
#include "../station_map.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../direction_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../vehicle_base.h"
#include "../train.h"
#include "../vehicle_cmd.h"
#include "../vehicle_func.h"
#include "../engine_func.h"
#include "../engine_base.h"
#include "../order_cmd.h"
#include "../waypoint_cmd.h"
#include "../economy_base.h"
#include "../settings_internal.h"
#include "../timer/timer_game_calendar.h"
#include "../pathfinder/yapf/yapf.h"
#include "../pbs.h"
#include "../fileio_func.h"
#include "../economy_func.h"
#include "../strings_func.h"
#include "../language.h"
#include "../table/strings.h"
#include "mock_environment.h"

#include <filesystem>
#include <deque>
#include <set>
#include <tuple>

/* Follow native track entry/exit rules through the model, including station tracks. */
static bool HasBlueprintPortRoute(const Blueprint &bp, int start_x, int start_y, DiagDirection entry_dir,
	int end_x, int end_y, DiagDirection exit_dir, std::optional<std::pair<int, int>> platform_tile = std::nullopt)
{
	using State = std::tuple<int, int, DiagDirection, bool>;
	std::map<std::pair<int, int>, const BlueprintTile *> pieces;
	for (const auto &piece : bp.tiles) pieces[{piece.dx, piece.dy}] = &piece;
	std::deque<State> pending{{start_x, start_y, entry_dir, !platform_tile.has_value()}};
	std::set<State> seen;
	while (!pending.empty()) {
		auto [x, y, entry, visited_platform] = pending.front();
		pending.pop_front();
		if (!seen.emplace(x, y, entry, visited_platform).second) continue;
		auto it = pieces.find({x, y});
		if (it == pieces.end()) continue;
		const BlueprintTile &piece = *it->second;
		if (piece.type == BlueprintTileType::Depot) {
			if (x == end_x && y == end_y && entry == piece.dir && visited_platform) return true;
			if (x == start_x && y == start_y && entry == piece.dir) {
				pending.emplace_back(x + (piece.dir == DiagDirection::NE ? -1 : piece.dir == DiagDirection::SW ? 1 : 0),
					y + (piece.dir == DiagDirection::NW ? -1 : piece.dir == DiagDirection::SE ? 1 : 0), ReverseDiagDir(piece.dir), visited_platform);
			}
			continue;
		}
		visited_platform |= platform_tile.has_value() && std::pair{x, y} == *platform_tile;
		TrackBits bits = piece.type == BlueprintTileType::Station ? TrackBits{piece.axis == Axis::X ? Track::X : Track::Y} : piece.trackbits;
		for (Track track : bits) {
			if (TrackExitdirToTrackdir(track, entry) == Trackdir::Invalid) continue;
			for (DiagDirection exit : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
				if (exit == entry || TrackExitdirToTrackdir(track, exit) == Trackdir::Invalid) continue;
				const Trackdir td = TrackExitdirToTrackdir(track, exit);
				bool forbidden = false;
				for (const BlueprintSignal &signal : piece.signals) {
					if (signal.track == track && signal.sigtype != SignalType::Path &&
							(signal.signals_copy & SignalAlongTrackdir(td)) == 0) forbidden = true;
				}
				if (forbidden) continue;
				if (x == end_x && y == end_y && exit == exit_dir && visited_platform) return true;
				int next_x = x + (exit == DiagDirection::NE ? -1 : exit == DiagDirection::SW ? 1 : 0);
				int next_y = y + (exit == DiagDirection::NW ? -1 : exit == DiagDirection::SE ? 1 : 0);
				if (next_x >= 0 && next_y >= 0 && next_x < bp.width && next_y < bp.height) {
				pending.emplace_back(next_x, next_y, ReverseDiagDir(exit), visited_platform);
				}
			}
		}
	}
	return false;
}

struct BlueprintPort { int x; int y; DiagDirection direction; };

static BlueprintPort RotatePort(BlueprintPort p, int old_height)
{
	DiagDirection d = p.direction == DiagDirection::NE ? DiagDirection::NW :
		p.direction == DiagDirection::NW ? DiagDirection::SW :
		p.direction == DiagDirection::SW ? DiagDirection::SE : DiagDirection::NE;
	return {old_height - 1 - p.y, p.x, d};
}

static BlueprintPort MirrorPort(BlueprintPort p)
{
	DiagDirection d = p.direction == DiagDirection::NE ? DiagDirection::NW :
		p.direction == DiagDirection::NW ? DiagDirection::NE :
		p.direction == DiagDirection::SE ? DiagDirection::SW : DiagDirection::SE;
	return {p.y, p.x, d};
}

struct Route { BlueprintPort start; BlueprintPort end; std::optional<std::pair<int, int>> station; };

/** Movement contract: follow lane signals, including every platform and depot. */
static auto CSTAdvertisedRoutes()
{
	return std::vector<std::pair<std::string, std::vector<Route>>>{
		{"CST Mainline Double Straight", {
			{{0, 0, DiagDirection::NE}, {7, 0, DiagDirection::SW}, std::nullopt},
			{{7, 1, DiagDirection::SW}, {0, 1, DiagDirection::NE}, std::nullopt},
		}},
		{"CST Dual-Track Passing Siding", {
			{{0, 1, DiagDirection::NE}, {13, 1, DiagDirection::SW}, std::nullopt},
			{{0, 1, DiagDirection::NE}, {13, 1, DiagDirection::SW}, std::pair{5, 0}},
			{{13, 2, DiagDirection::SW}, {0, 2, DiagDirection::NE}, std::nullopt},
		}},
		{"CST Portal Gate Approach Corridor", {
			{{0, 1, DiagDirection::NE}, {9, 1, DiagDirection::SW}, std::nullopt},
			{{9, 2, DiagDirection::SW}, {0, 2, DiagDirection::NE}, std::nullopt},
			{{0, 1, DiagDirection::NE}, {0, 2, DiagDirection::NE}, std::nullopt},
			{{9, 2, DiagDirection::SW}, {9, 1, DiagDirection::SW}, std::nullopt},
		}},
		{"CST High-Speed 3-Way Wye Junction", {
			{{0, 5, DiagDirection::NE}, {11, 5, DiagDirection::SW}, std::nullopt},
			{{11, 6, DiagDirection::SW}, {0, 6, DiagDirection::NE}, std::nullopt},
			{{0, 5, DiagDirection::NE}, {6, 0, DiagDirection::NW}, std::nullopt},
			{{5, 0, DiagDirection::NW}, {11, 5, DiagDirection::SW}, std::nullopt},
			{{5, 0, DiagDirection::NW}, {0, 6, DiagDirection::NE}, std::nullopt},
			{{11, 6, DiagDirection::SW}, {6, 0, DiagDirection::NW}, std::nullopt},
		}},
		{"CST Ro-Ro 4-Platform Terminal Station Block", {
			{{0, 2, DiagDirection::NE}, {11, 2, DiagDirection::SW}, std::pair{3, 2}},
			{{0, 2, DiagDirection::NE}, {11, 2, DiagDirection::SW}, std::pair{3, 3}},
			{{0, 2, DiagDirection::NE}, {11, 2, DiagDirection::SW}, std::pair{3, 4}},
			{{0, 2, DiagDirection::NE}, {11, 2, DiagDirection::SW}, std::pair{3, 5}},
		}},
		{"CST 4-Way Compact Roundabout Junction", {
			{{0, 4, DiagDirection::NE}, {9, 4, DiagDirection::SW}, std::nullopt},
			{{0, 4, DiagDirection::NE}, {4, 0, DiagDirection::NW}, std::nullopt},
			{{0, 4, DiagDirection::NE}, {5, 9, DiagDirection::SE}, std::nullopt},
			{{5, 0, DiagDirection::NW}, {5, 9, DiagDirection::SE}, std::nullopt},
			{{5, 0, DiagDirection::NW}, {0, 5, DiagDirection::NE}, std::nullopt},
			{{5, 0, DiagDirection::NW}, {9, 4, DiagDirection::SW}, std::nullopt},
			{{9, 5, DiagDirection::SW}, {0, 5, DiagDirection::NE}, std::nullopt},
			{{9, 5, DiagDirection::SW}, {4, 0, DiagDirection::NW}, std::nullopt},
			{{9, 5, DiagDirection::SW}, {5, 9, DiagDirection::SE}, std::nullopt},
			{{4, 9, DiagDirection::SE}, {4, 0, DiagDirection::NW}, std::nullopt},
			{{4, 9, DiagDirection::SE}, {0, 5, DiagDirection::NE}, std::nullopt},
			{{4, 9, DiagDirection::SE}, {9, 4, DiagDirection::SW}, std::nullopt},
		}},
		{"CST Industrial Bulk Balloon Loop", {
			{{0, 3, DiagDirection::NE}, {0, 6, DiagDirection::NE}, std::pair{5, 3}},
			{{0, 3, DiagDirection::NE}, {0, 6, DiagDirection::NE}, std::pair{5, 4}},
		}},
		{"CST Depot Maintenance Staging Yard", {
			{{0, 1, DiagDirection::NE}, {9, 1, DiagDirection::SW}, std::nullopt},
			{{9, 2, DiagDirection::SW}, {0, 2, DiagDirection::NE}, std::nullopt},
			{{9, 2, DiagDirection::SW}, {4, 4, DiagDirection::SW}, std::nullopt},
			{{9, 2, DiagDirection::SW}, {4, 5, DiagDirection::SW}, std::nullopt},
			{{4, 4, DiagDirection::NE}, {0, 2, DiagDirection::NE}, std::nullopt},
			{{4, 5, DiagDirection::NE}, {0, 2, DiagDirection::NE}, std::nullopt},
		}},
	};
}

static void SetupCSTPrefabTestEnv(uint32_t map_w = 256, uint32_t map_h = 256)
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
	ResetVehicleHash();
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
	_settings_game.station.station_spread = 64;
	_settings_game.difficulty.infinite_money = false;
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(15, 15));
	town->name = "CST test town";
	town->townnametype = SPECSTR_TOWNNAME_START;
	RebuildTownKdtree();
	RebuildStationKdtree();

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
		CHECK(bp->layout_revision == 2);
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
		CHECK(bp->GetTrackPieceCount() == 32); // Mainlines plus two turnback loops without 90-degree curves.
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

TEST_CASE("CST Wye and RoRo advertise traversable ports", "[cst_prefab][blueprint-route]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	const Blueprint *wye = BlueprintManager::FindBuiltin("CST High-Speed 3-Way Wye Junction");
	REQUIRE(wye != nullptr);
	CHECK(HasBlueprintPortRoute(*wye, 5, 0, DiagDirection::NW, 0, 6, DiagDirection::NE));
	CHECK(HasBlueprintPortRoute(*wye, 11, 6, DiagDirection::SW, 6, 0, DiagDirection::NW));
	const Blueprint *roro = BlueprintManager::FindBuiltin("CST Ro-Ro 4-Platform Terminal Station Block");
	REQUIRE(roro != nullptr);
	for (int row = 2; row <= 5; ++row) {
		CAPTURE(row);
		CHECK(HasBlueprintPortRoute(*roro, 0, 2, DiagDirection::NE, 11, 2, DiagDirection::SW, std::pair{3, row}));
	}
}

TEST_CASE("CST compact roundabout connects every cardinal corridor", "[cst_prefab][blueprint-route]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	const Blueprint *junction = BlueprintManager::FindBuiltin("CST 4-Way Compact Roundabout Junction");
	REQUIRE(junction != nullptr);
	struct Movement { BlueprintPort start; BlueprintPort end; };
	for (const Movement &route : std::vector<Movement>{
		{{0, 4, DiagDirection::NE}, {9, 4, DiagDirection::SW}},
		{{0, 4, DiagDirection::NE}, {4, 0, DiagDirection::NW}},
		{{0, 4, DiagDirection::NE}, {5, 9, DiagDirection::SE}},
		{{5, 0, DiagDirection::NW}, {5, 9, DiagDirection::SE}},
		{{5, 0, DiagDirection::NW}, {0, 5, DiagDirection::NE}},
		{{5, 0, DiagDirection::NW}, {9, 4, DiagDirection::SW}},
		{{9, 5, DiagDirection::SW}, {0, 5, DiagDirection::NE}},
		{{9, 5, DiagDirection::SW}, {4, 0, DiagDirection::NW}},
		{{9, 5, DiagDirection::SW}, {5, 9, DiagDirection::SE}},
		{{4, 9, DiagDirection::SE}, {4, 0, DiagDirection::NW}},
		{{4, 9, DiagDirection::SE}, {0, 5, DiagDirection::NE}},
		{{4, 9, DiagDirection::SE}, {9, 4, DiagDirection::SW}},
	}) {
		CAPTURE(route.start.x, route.start.y, route.end.x, route.end.y);
		CHECK(HasBlueprintPortRoute(*junction, route.start.x, route.start.y, route.start.direction,
			route.end.x, route.end.y, route.end.direction));
	}
}

TEST_CASE("CST bulk balloon turns both platforms into the return lead", "[cst_prefab][blueprint-route]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	const Blueprint *loop = BlueprintManager::FindBuiltin("CST Industrial Bulk Balloon Loop");
	REQUIRE(loop != nullptr);
	for (int row : {3, 4}) {
		CAPTURE(row);
		CHECK(HasBlueprintPortRoute(*loop, 0, 3, DiagDirection::NE,
			0, 6, DiagDirection::NE, std::pair{5, row}));
	}
}

TEST_CASE("CST staging yard reaches both depots and rejoins the bypass", "[cst_prefab][blueprint-route]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	const Blueprint *yard = BlueprintManager::FindBuiltin("CST Depot Maintenance Staging Yard");
	REQUIRE(yard != nullptr);
	for (int row : {4, 5}) {
		CAPTURE(row);
		CHECK(HasBlueprintPortRoute(*yard, 9, 2, DiagDirection::SW,
			4, row, DiagDirection::SW));
		CHECK(HasBlueprintPortRoute(*yard, 4, row, DiagDirection::NE,
			0, 2, DiagDirection::NE));
	}
}

TEST_CASE("CST straight siding and corridor retain their advertised routes", "[cst_prefab][blueprint-route]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	const Blueprint *straight = BlueprintManager::FindBuiltin("CST Mainline Double Straight");
	REQUIRE(straight != nullptr);
	for (int row : {0, 1}) {
		CHECK(HasBlueprintPortRoute(*straight, row == 0 ? 0 : 7, row, row == 0 ? DiagDirection::NE : DiagDirection::SW,
			row == 0 ? 7 : 0, row, row == 0 ? DiagDirection::SW : DiagDirection::NE));
	}
	const Blueprint *siding = BlueprintManager::FindBuiltin("CST Dual-Track Passing Siding");
	REQUIRE(siding != nullptr);
	CHECK(HasBlueprintPortRoute(*siding, 0, 1, DiagDirection::NE,
		13, 1, DiagDirection::SW, std::pair{5, 0}));
	CHECK(HasBlueprintPortRoute(*siding, 13, 2, DiagDirection::SW,
		0, 2, DiagDirection::NE));
	const Blueprint *corridor = BlueprintManager::FindBuiltin("CST Portal Gate Approach Corridor");
	REQUIRE(corridor != nullptr);
	for (int row : {1, 2}) {
		CHECK(HasBlueprintPortRoute(*corridor, row == 1 ? 0 : 9, row, row == 1 ? DiagDirection::NE : DiagDirection::SW,
			row == 1 ? 9 : 0, row, row == 1 ? DiagDirection::SW : DiagDirection::NE));
	}
	CHECK(HasBlueprintPortRoute(*corridor, 0, 1, DiagDirection::NE,
		0, 2, DiagDirection::NE));
	CHECK(HasBlueprintPortRoute(*corridor, 9, 2, DiagDirection::SW,
		9, 1, DiagDirection::SW));
}

TEST_CASE("CST prefabs run every advertised route with native trains and intact signals", "[cst_prefab][blueprint-yapf]")
{
	const int layout = GENERATE(range(0, 8));
	const int rotation = GENERATE(0, 1, 2, 3);
	const bool mirror = GENERATE(false, true);
	const auto catalogue = CSTAdvertisedRoutes();
	const auto &[name, routes] = catalogue[layout];
	for (const Route &original_route : routes) {
		SetupCSTPrefabTestEnv();
		for (const SettingVariant &setting : GetSaveLoadSettingTable()) {
			const SettingDesc *desc = GetSettingDesc(setting);
			if (desc->GetName().starts_with("pf.")) desc->ResetToDefault(&_settings_game);
		}
		BlueprintManager::Initialize();
		Blueprint bp = *BlueprintManager::FindBuiltin(name);
		Route route = original_route;
		for (int i = 0; i < rotation; ++i) {
			route.start = RotatePort(route.start, bp.height);
			route.end = RotatePort(route.end, bp.height);
			if (route.station) route.station = std::pair{bp.height - 1 - route.station->second, route.station->first};
			bp = bp.Rotate();
		}
		if (mirror) {
			route.start = MirrorPort(route.start);
			route.end = MirrorPort(route.end);
			if (route.station) route.station = std::pair{route.station->second, route.station->first};
			bp = bp.Mirror();
		}
		CAPTURE(name, rotation, mirror, original_route.start.x, original_route.start.y,
			original_route.end.x, original_route.end.y, original_route.station);
		if (original_route.station) INFO("via=" << original_route.station->first << ',' << original_route.station->second);
		const TileIndex origin = TileXY(80, 80);
		REQUIRE(Command<Commands::PlaceBlueprint>::Post(origin, bp.ToJson(), RAILTYPE_BEGIN, false));
		const TileIndex start = TileAddWrap(origin, route.start.x, route.start.y);
		TileIndex end = TileAddWrap(origin, route.end.x, route.end.y);
		Order destination;
		if (IsRailDepotTile(end)) {
			destination.MakeGoToDepot(GetDepotIndex(end), {});
		} else {
			/* Give the complete prefab an outgoing lead and a real destination. */
			for (uint i = 0; i < 3; ++i) {
				end = TileAddByDiagDir(end, route.end.direction);
				if (i < 2) REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute,
					end, RAILTYPE_BEGIN, DiagDirToDiagTrack(route.end.direction), false).Succeeded());
			}
			REQUIRE(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute,
				end, RAILTYPE_BEGIN, DiagDirToAxis(route.end.direction), 1, 1,
				STAT_CLASS_DFLT, 0, NEW_STATION, true).Succeeded());
			destination.MakeGoToStation(GetStationIndex(end));
		}
		TileIndex depot = start;
		if (!IsRailDepotTile(depot)) {
			for (uint i = 0; i < 5; ++i) depot = TileAddByDiagDir(depot, route.start.direction);
			REQUIRE(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute,
				depot, RAILTYPE_BEGIN, ReverseDiagDir(route.start.direction)).Succeeded());
			for (TileIndex tile = TileAddByDiagDir(depot, ReverseDiagDir(route.start.direction)); tile != start;
					tile = TileAddByDiagDir(tile, ReverseDiagDir(route.start.direction))) {
				REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute,
					tile, RAILTYPE_BEGIN, DiagDirToDiagTrack(route.start.direction), false).Succeeded());
			}
		}
		_settings_game.game_creation.landscape = LandscapeType::Temperate;
		_game_mode = GameMode::Normal;
		TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
		SetupCargoForClimate(LandscapeType::Temperate);
		_engine_mngr.ResetToDefaultMapping();
		SetupEngines();
		for (Engine *engine : Engine::Iterate()) {
			if (const CargoLabel *label = std::get_if<CargoLabel>(&engine->info.cargo_label)) {
				const CargoType cargo = GetCargoTypeByLabel(*label);
				if (IsValidCargoType(cargo)) engine->info.cargo_type = cargo;
			}
		}
		StartupEngines();
		_settings_game.vehicle.max_trains = 10;
		_settings_game.vehicle.max_train_length = 10;
		_settings_game.pf.path_backoff_interval = 1;
		_settings_game.pf.wait_for_pbs_path = 16;
		_settings_game.pf.reverse_at_signals = false;
		_settings_game.pf.forbid_90_deg = true;
		auto [cost, id, capacity, mail, capacities] = Command<Commands::BuildVehicle>::Do(
			DoCommandFlag::Execute, depot, EngineID{0}, false, INVALID_CARGO, ClientID::Invalid);
		REQUIRE(cost.Succeeded());
		Train *train = Train::Get(id);
		TileIndex via = INVALID_TILE;
		if (route.station) {
			via = TileAddWrap(origin, route.station->first, route.station->second);
			Order intermediate;
			if (IsRailStationTile(via)) {
				/* Occupied alternate platforms force each advertised platform to be
				 * exercised. Track geometry and every signal remain as stamped. */
				const Axis axis = GetRailStationAxis(via);
				for (const BlueprintTile &piece : bp.tiles) {
					if (piece.type != BlueprintTileType::Station) continue;
					const bool other_platform = axis == Axis::X ? piece.dy != route.station->second : piece.dx != route.station->first;
					if (other_platform) SetRailStationReservation(TileAddWrap(origin, piece.dx, piece.dy), true);
				}
				intermediate.MakeGoToStation(GetStationIndex(via));
			} else {
				/* A native waypoint orders the train through the passing loop. */
				REQUIRE(Command<Commands::BuildRailWaypoint>::Do(DoCommandFlag::Execute,
					via, DiagDirToAxis(ReverseDiagDir(route.start.direction)), 1, 1,
					STAT_CLASS_WAYP, 0, NEW_STATION, true).Succeeded());
				intermediate.MakeGoToWaypoint(GetStationIndex(via));
			}
			REQUIRE(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute,
				train->index, VehicleOrderID{0}, intermediate).Succeeded());
		}
		REQUIRE(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute,
			train->index, static_cast<VehicleOrderID>(train->GetNumOrders()), destination).Succeeded());
		train->current_order = *train->GetOrder(0);
		train->dest_tile = route.station ? via : end;
		UpdateSignalsInBuffer();
		bool path_found = false;
		(void)YapfTrainChooseTrack(train, depot, ReverseDiagDir(route.start.direction),
			TrackBits{DiagDirToDiagTrack(route.start.direction)}, path_found, false, nullptr, nullptr);
		CHECK(path_found);
		REQUIRE(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, id, false).Succeeded());
		bool visited_via = !route.station.has_value();
		bool alive = true;
		auto arrived = [&]() {
			return train->tile == end && (IsRailDepotTile(end) ? train->track == Track::Depot :
				train->current_order.IsType(OT_LOADING) && train->last_station_visited == GetStationIndex(end));
		};
		uint ticks = 0;
		for (; ticks < 4096 && !arrived(); ++ticks) {
			if (!(alive = train->Tick())) break;
			visited_via |= train->tile == via;
			for (Station *station : Station::Iterate()) LoadUnloadStation(station);
			UpdateSignalsInBuffer();
		}
		CHECK(alive);
		INFO("ticks=" << ticks << " tile=" << TileX(train->tile) << ',' << TileY(train->tile));
		CHECK(visited_via);
		CHECK(arrived());
		CHECK_FALSE(train->vehstatus.Test(VehState::Crashed));
	}
}

TEST_CASE("Portal YAPF routes native trains through short off-axis wormholes", "[portal][yapf-regression]")
{
	const bool reverse = GENERATE(false, true);
	SetupCSTPrefabTestEnv();
	for (const SettingVariant &setting : GetSaveLoadSettingTable()) {
		const SettingDesc *desc = GetSettingDesc(setting);
		if (desc->GetName().starts_with("pf.")) desc->ResetToDefault(&_settings_game);
	}
	_settings_game.pf.forbid_90_deg = true;
	const TileIndex a = TileXY(50, 50);
	const TileIndex b = TileXY(190, 190);
	MakeRailTunnel(a, _current_company, DiagDirection::SW, RAILTYPE_RAIL);
	MakeRailTunnel(b, _current_company, DiagDirection::NW, RAILTYPE_RAIL);
	REQUIRE(PortalRegistry::RegisterPortalPair(a, DiagDirection::SW, WorldID{0},
		b, DiagDirection::NW, WorldID{1}, 1, true) != INVALID_PORTAL);
	const TileIndex entry = reverse ? b : a;
	const TileIndex exit = reverse ? a : b;
	const DiagDirection entering = reverse ? DiagDirection::NW : DiagDirection::SW;
	const DiagDirection leaving = reverse ? DiagDirection::NE : DiagDirection::SE;
	TileIndex depot = entry;
	for (uint i = 0; i < 8; ++i) depot = TileAddByDiagDir(depot, ReverseDiagDir(entering));
	REQUIRE(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot,
		RAILTYPE_RAIL, entering).Succeeded());
	for (TileIndex tile = TileAddByDiagDir(depot, entering); tile != entry; tile = TileAddByDiagDir(tile, entering)) {
		REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile,
			RAILTYPE_RAIL, DiagDirToDiagTrack(entering), false).Succeeded());
		if (TileAddByDiagDir(tile, entering) == entry) {
			for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
				if (TrackExitdirToTrackdir(track, ReverseDiagDir(entering)) == Trackdir::Invalid) continue;
				REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute,
					tile, RAILTYPE_RAIL, track, false).Succeeded());
				break;
			}
		}
	}
	TileIndex destination = exit;
	for (uint i = 1; i <= 8; ++i) {
		destination = TileAddByDiagDir(destination, leaving);
		if (i == 8) break;
		REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, destination,
			RAILTYPE_RAIL, DiagDirToDiagTrack(leaving), false).Succeeded());
		if (i == 2) {
			/* End the post-portal segment at a genuine track choice. The old
			 * geographic estimate decreases across this cheap, distant jump. */
			for (Track track : {Track::Upper, Track::Lower, Track::Left, Track::Right}) {
				if (TrackExitdirToTrackdir(track, ReverseDiagDir(leaving)) == Trackdir::Invalid) continue;
				REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute,
					destination, RAILTYPE_RAIL, track, false).Succeeded());
				break;
			}
		}
	}
	REQUIRE(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, destination,
		RAILTYPE_RAIL, DiagDirToAxis(leaving), 1, 1, STAT_CLASS_DFLT, 0, NEW_STATION, true).Succeeded());
	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	_game_mode = GameMode::Normal;
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	SetupCargoForClimate(LandscapeType::Temperate);
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
	for (Engine *engine : Engine::Iterate()) {
		if (const CargoLabel *label = std::get_if<CargoLabel>(&engine->info.cargo_label)) {
			const CargoType cargo = GetCargoTypeByLabel(*label);
			if (IsValidCargoType(cargo)) engine->info.cargo_type = cargo;
		}
	}
	StartupEngines();
	_settings_game.vehicle.max_trains = 10;
	_settings_game.vehicle.max_train_length = 10;
	auto [cost, id, capacity, mail, capacities] = Command<Commands::BuildVehicle>::Do(
		DoCommandFlag::Execute, depot, EngineID{0}, false, INVALID_CARGO, ClientID::Invalid);
	REQUIRE(cost.Succeeded());
	Train *train = Train::Get(id);
	Order order;
	order.MakeGoToStation(GetStationIndex(destination));
	REQUIRE(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, id, VehicleOrderID{0}, order).Succeeded());
	train->current_order = order;
	train->dest_tile = destination;
	bool path_found = false;
	(void)YapfTrainChooseTrack(train, depot, entering, TrackBits{DiagDirToDiagTrack(entering)},
		path_found, false, nullptr, nullptr);
	REQUIRE(path_found);
	REQUIRE(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, id, false).Succeeded());
	bool crossed = false;
	for (uint ticks = 0; ticks < 4096 && !train->current_order.IsType(OT_LOADING); ++ticks) {
		REQUIRE(train->Tick());
		crossed |= train->tile == exit;
		for (Station *station : Station::Iterate()) LoadUnloadStation(station);
		UpdateSignalsInBuffer();
	}
	CHECK(crossed);
	CHECK(train->tile == destination);
	CHECK(train->last_station_visited == GetStationIndex(destination));
	CHECK(train->current_order.IsType(OT_LOADING));
	CHECK_FALSE(train->vehstatus.Test(VehState::Crashed));
}

TEST_CASE("CST prefab routes survive sequential rotations and mirrors", "[cst_prefab][blueprint-route]")
{
	BlueprintManager::Reset();
	BlueprintManager::Initialize();
	for (const auto &[name, routes] : CSTAdvertisedRoutes()) {
		const Blueprint *original = BlueprintManager::FindBuiltin(name);
		REQUIRE(original != nullptr);
		Blueprint rotated = *original;
		std::vector<Route> rotated_routes = routes;
		for (int step = 0; step < 4; ++step) {
			for (bool mirror : {false, true}) {
				Blueprint variant = mirror ? rotated.Mirror() : rotated;
				CAPTURE(name, step, mirror);
				REQUIRE(variant.IsValid());
				CHECK(variant.layout_revision == original->layout_revision);
				for (const auto &route : rotated_routes) {
					BlueprintPort start = mirror ? MirrorPort(route.start) : route.start;
					BlueprintPort end = mirror ? MirrorPort(route.end) : route.end;
					std::optional<std::pair<int, int>> station = route.station;
					if (mirror && station.has_value()) station = std::pair{station->second, station->first};
					CHECK(HasBlueprintPortRoute(variant, start.x, start.y, start.direction,
						end.x, end.y, end.direction, station));
				}
			}
			const int old_height = rotated.height;
			rotated = rotated.Rotate(1);
			for (auto &route : rotated_routes) {
				route.start = RotatePort(route.start, old_height);
				route.end = RotatePort(route.end, old_height);
				if (route.station.has_value()) {
					BlueprintPort site = RotatePort({route.station->first, route.station->second, DiagDirection::NE}, old_height);
					route.station = std::pair{site.x, site.y};
				}
			}
		}
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

		Blueprint r360 = *orig;
		for (int step = 0; step < 4; ++step) r360 = r360.Rotate(1);
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

TEST_CASE("CST Prefabs - Deterministic In-Game Map Placement", "[cst_prefab][blueprint-regression]")
{
	const size_t index = GENERATE(size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{4}, size_t{5}, size_t{6}, size_t{7});
	const int rotation = GENERATE(0, 1, 2, 3);
	const bool mirror = GENERATE(false, true);
	const bool fabricate = GENERATE(false, true);
	SetupCSTPrefabTestEnv();
	BlueprintManager::Initialize();
	const Blueprint *original = BlueprintManager::GetBlueprint(index);
	REQUIRE(original != nullptr);
	Blueprint bp = *original;
	for (int step = 0; step < rotation; ++step) bp = bp.Rotate(1);
	if (mirror) bp = bp.Mirror();
	CAPTURE(bp.name, rotation, mirror, fabricate);
	const TileIndex origin = TileXY(40, 40);
	std::map<CargoType, uint32_t> required;
	auto add_bom = [&](const BillOfMaterials &bom, size_t count) {
		for (const auto &[cargo, amount] : bom.materials) {
			const uint64_t total = static_cast<uint64_t>(amount) * count;
			REQUIRE(total <= UINT32_MAX - required[cargo]);
			required[cargo] += static_cast<uint32_t>(total);
		}
	};
	add_bom(FabricationManager::GetTrackBOM(RAILTYPE_BEGIN), bp.GetTrackPieceCount());
	add_bom(FabricationManager::GetSignalBOM(), bp.GetSignalCount());
	add_bom(FabricationManager::GetDepotBOM(RAILTYPE_BEGIN), bp.GetDepotCount());
	for (const auto &[cargo, amount] : required) StockpileManager::AddCargo(WorldID{0}, _current_company, cargo, amount);
	FabricationManager::SetFabricateFromStockpile(_current_company, fabricate);

	const CommandCost query = CmdPlaceBlueprint({}, origin, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(query.Succeeded());
	CHECK(query.GetCost() > 0);
	for (const auto &piece : bp.tiles) {
		CHECK(IsTileType(TileAddWrap(origin, piece.dx, piece.dy), TileType::Clear));
	}
	for (const auto &[cargo, amount] : required) CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, cargo) == amount);

	const Money before = Company::Get(_current_company)->money;
	REQUIRE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp.ToJson(), RAILTYPE_BEGIN, false));
	CHECK(Company::Get(_current_company)->money == before - query.GetCost());
	StationID station_id = StationID::Invalid();
	for (const auto &piece : bp.tiles) {
		const TileIndex tile = TileAddWrap(origin, piece.dx, piece.dy);
		CAPTURE(piece.dx, piece.dy);
		CHECK(GetTileOwner(tile) == _current_company);
		CHECK(GetRailType(tile) == RAILTYPE_BEGIN);
		switch (piece.type) {
			case BlueprintTileType::Track:
				REQUIRE(IsPlainRailTile(tile));
				CHECK(GetTrackBits(tile) == piece.trackbits);
				for (const auto &signal : piece.signals) {
					REQUIRE(HasSignalOnTrack(tile, signal.track));
					CHECK(GetSignalType(tile, signal.track) == signal.sigtype);
					CHECK(GetSignalVariant(tile, signal.track) == signal.sigvar);
					CHECK((GetPresentSignals(tile) & SignalOnTrack(signal.track)) == signal.signals_copy);
				}
				break;
			case BlueprintTileType::Depot:
				REQUIRE(IsRailDepotTile(tile));
				CHECK(GetRailDepotDirection(tile) == piece.dir);
				break;
			case BlueprintTileType::Station:
				REQUIRE(IsRailStationTile(tile));
				CHECK(GetRailStationAxis(tile) == piece.axis);
				if (station_id == StationID::Invalid()) station_id = GetStationIndex(tile);
				CHECK(GetStationIndex(tile) == station_id);
				break;
		}
	}
	for (const auto &[cargo, amount] : required) CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, cargo) == (fabricate ? 0 : amount));
	CHECK(Depot::GetNumItems() == bp.GetDepotCount());
	CHECK(Station::GetNumItems() == (bp.GetStationCount() == 0 ? 0 : 1));

	/* Identical stamping is free and needs no further materials, including signals. */
	const auto overlap = CmdPlaceBlueprint({}, origin, bp.ToJson(), RAILTYPE_BEGIN, false);
	REQUIRE(overlap.Succeeded());
	CHECK(overlap.GetCost() == 0);
	REQUIRE(Command<Commands::PlaceBlueprint>::Post(StringID{0}, origin, bp.ToJson(), RAILTYPE_BEGIN, false));
	CHECK(Company::Get(_current_company)->money == before - query.GetCost());
	for (const auto &[cargo, amount] : required) CHECK(StockpileManager::GetStock(WorldID{0}, _current_company, cargo) == (fabricate ? 0 : amount));
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
