/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_spaceports_and_conduits.cpp Unit tests for spaceports, virtual off-world trade, and edge extraction conduits. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/spaceport_manager.h"
#include "../portal/edge_conduit.h"
#include "../portal/portal_cmd.h"
#include "../portal/portal_registry.h"
#include "../station_base.h"
#include "../town.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../landscape_cmd.h"
#include "../vehicle_base.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../water_map.h"
#include "../tunnelbridge_map.h"
#include "../rail_map.h"
#include "../pathfinder/follow_track.hpp"
#include "../signal_func.h"
#include "../saveload/saveload_func.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../table/strings.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "mock_environment.h"

#include <filesystem>

#include "../safeguards.h"

static void SetupSprint9Environment(uint32_t map_w = 256, uint32_t map_h = 256)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	PlanetManager::Reset();
	PortalRegistry::Reset();
	SpaceportManager::Reset();
	EdgeConduitManager::Reset();

	_station_pool.CleanPool();
	_town_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_company_pool.CleanPool();

	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

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

	/* Create 3 test planetary worlds */
	/* World 0: Core World (10..90, 10..90) */
	PlanetRegion w0{
		.id = WorldID{0},
		.name = "Terra Core",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 10,
		.min_y = 10,
		.max_x = 90,
		.max_y = 90,
		.development_score = 10000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w0));

	/* World 1: Developed World (120..190, 10..90) */
	PlanetRegion w1{
		.id = WorldID{1},
		.name = "Vulcan Alpha",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 120,
		.min_y = 10,
		.max_x = 190,
		.max_y = 90,
		.development_score = 6000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w1));

	/* World 2: Frontier World (10..90, 120..190) */
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Haven Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 10,
		.min_y = 120,
		.max_x = 90,
		.max_y = 190,
		.development_score = 1000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w2));

	PlanetManager::RebuildSpatialGrid();

	/* Fill inner map tiles with clear grass by default */
	for (uint y = 1; y < map_h - 1; ++y) {
		for (uint x = 1; x < map_w - 1; ++x) {
			TileIndex t = TileXY(x, y);
			MakeClear(t, ClearGround::Grass, 0);
		}
	}
}

static void SetupMapEdgeWorld(WorldPhase phase = WorldPhase::Phase3_Frontier)
{
	SetupSprint9Environment(64, 64);
	for (uint i = 0; i <= Map::MaxX(); ++i) {
		MakeVoid(TileXY(i, 0));
		MakeVoid(TileXY(i, Map::MaxY()));
	}
	for (uint i = 0; i <= Map::MaxY(); ++i) {
		MakeVoid(TileXY(0, i));
		MakeVoid(TileXY(Map::MaxX(), i));
	}
	PlanetManager::Reset();
	PlanetRegion edge_world{
		.id = WorldID{0},
		.name = "Map Edge Test World",
		.phase = phase,
		.biome = WorldBiome::Temperate,
		.min_x = 1,
		.min_y = 1,
		.max_x = Map::MaxX() - 1,
		.max_y = Map::MaxY() - 1,
	};
	REQUIRE(PlanetManager::RegisterRegion(edge_world));
	PlanetManager::RebuildSpatialGrid();
}

TEST_CASE("Spaceport Manager - Lifecycle and Trade Calculation")
{
	SetupSprint9Environment();

	TileIndex st_tile = TileXY(30, 30);
	REQUIRE(Station::CanAllocateItem());
	Station *st = Station::Create(st_tile);
	REQUIRE(st != nullptr);
	StationID sid = st->index;

	/* Initially not a spaceport */
	CHECK(!SpaceportManager::IsSpaceport(sid));
	CHECK(SpaceportManager::Count() == 0);
	CHECK(SpaceportManager::GetSpaceport(sid) == nullptr);

	/* Register spaceport */
	CHECK(SpaceportManager::RegisterSpaceport(sid, WorldID{0}, 1));
	CHECK(SpaceportManager::IsSpaceport(sid));
	CHECK(SpaceportManager::Count() == 1);

	const SpaceportInfo *info = SpaceportManager::GetSpaceport(sid);
	REQUIRE(info != nullptr);
	CHECK(info->station_id == sid);
	CHECK(info->world_id == WorldID{0});
	CHECK(info->offworld_trade_tier == 1);
	CHECK(info->supplies_received == 0);
	CHECK(info->total_offworld_cargo_generated == 0);

	/* Trade calculation formula test:
	 * base = 25 * tier
	 * supply_bonus = min(150, supplies / 2)
	 * dev_bonus = dev_score / 200 (for World 0 dev_score=10000 -> 50)
	 * For tier 1, supplies 0: 25*1 + 0 + 50 = 75
	 */
	CHECK(SpaceportManager::CalculateTradeCargoProduction(*info) == 75);

	/* Deliver supplies */
	SpaceportManager::RecordSupplyDelivery(sid, CargoType{0}, 100);
	info = SpaceportManager::GetSpaceport(sid);
	REQUIRE(info != nullptr);
	CHECK(info->supplies_received == 100);

	/* Now trade production: base(25) + supply_bonus(50) + dev_bonus(50) = 125 */
	CHECK(SpaceportManager::CalculateTradeCargoProduction(*info) == 125);

	/* Unregister */
	CHECK(SpaceportManager::UnregisterSpaceport(sid));
	CHECK(!SpaceportManager::IsSpaceport(sid));
	CHECK(SpaceportManager::Count() == 0);
}

TEST_CASE("Spaceport Command - CmdDesignateSpaceport")
{
	SetupSprint9Environment();

	TileIndex airport_tile = TileXY(40, 40);
	REQUIRE(Station::CanAllocateItem());
	Station *st = Station::Create(airport_tile);
	REQUIRE(st != nullptr);
	StationID sid = st->index;
	st->owner = _current_company;

	/* Test failure without airport facility */
	CommandCost res = CmdDesignateSpaceport(DoCommandFlags{}, sid);
	CHECK(res.Failed());
	CHECK(res.GetErrorMessage() == STR_ERROR_CAN_T_BUILD_AIRPORT_HERE);

	/* Add airport facility to station */
	st->AddFacility(StationFacility::Airport, airport_tile);
	st->airport.tile = airport_tile;
	st->airport.w = 5;
	st->airport.h = 5;
	st->airport.type = AT_SMALL; // Starts at Tier 1 and can be upgraded through the station UI

	/* Test test-mode execution */
	res = CmdDesignateSpaceport(DoCommandFlags{}, sid);
	CHECK(res.Succeeded());
	CHECK(!SpaceportManager::IsSpaceport(sid));

	/* Test real execution */
	res = CmdDesignateSpaceport(DoCommandFlag::Execute, sid);
	CHECK(res.Succeeded());
	CHECK(SpaceportManager::IsSpaceport(sid));

	const SpaceportInfo *sp = SpaceportManager::GetSpaceport(sid);
	REQUIRE(sp != nullptr);
	CHECK(sp->world_id == WorldID{0});
	CHECK(sp->offworld_trade_tier == 1);

	/* Repeating the command upgrades without losing operating counters. */
	SpaceportManager::GetSpaceportMutable(sid)->supplies_received = 80;
	SpaceportManager::GetSpaceportMutable(sid)->total_offworld_cargo_generated = 150;
	REQUIRE(CmdDesignateSpaceport(DoCommandFlag::Execute, sid).Succeeded());
	sp = SpaceportManager::GetSpaceport(sid);
	REQUIRE(sp != nullptr);
	CHECK(sp->offworld_trade_tier == 2);
	CHECK(sp->supplies_received == 80);
	CHECK(sp->total_offworld_cargo_generated == 150);

	REQUIRE(CmdDesignateSpaceport(DoCommandFlag::Execute, sid).Succeeded());
	CHECK(SpaceportManager::GetSpaceport(sid)->offworld_trade_tier == 3);

	/* Tier 3 is the hard maximum. */
	CommandCost res_max = CmdDesignateSpaceport(DoCommandFlag::Execute, sid);
	CHECK(res_max.Failed());
	CHECK(res_max.GetErrorMessage() == STR_ERROR_SPACEPORT_MAX_TIER);

	/* Foreign company designation must fail */
	_current_company = CompanyID{1};
	CommandCost res_own = CmdDesignateSpaceport(DoCommandFlag::Execute, sid);
	CHECK(res_own.Failed());
	_current_company = CompanyID{0};
}

TEST_CASE("Edge Conduit - Boundary Void Adjacency Rules")
{
	SetupSprint9Environment();

	/* Mark an area around tile (50, 50) as VOID to test void border adjacency */
	TileIndex void_tile = TileXY(50, 50);
	MakeVoid(void_tile);

	TileIndex adjacent_tile = TileXY(51, 50); // Adjacent to void_tile
	TileIndex interior_tile = TileXY(30, 30); // Far interior of World 0

	CHECK(EdgeConduitManager::IsVoidAdjacent(adjacent_tile));
	CHECK(!EdgeConduitManager::IsVoidAdjacent(interior_tile));

	/* A coordinate near the map perimeter is not sufficient by itself; the
	 * neighbouring in-map tile must actually be TileType::Void. */
	TileIndex edge_tile = TileXY(1, 50);
	CHECK(!EdgeConduitManager::IsVoidAdjacent(edge_tile));
}

TEST_CASE("Edge Conduit - Construction and Demolition Commands")
{
	SetupSprint9Environment();

	/* Tile (50, 50) is void */
	TileIndex void_tile = TileXY(50, 50);
	MakeVoid(void_tile);

	TileIndex conduit_tile = TileXY(51, 50); // Border tile adjacent to void
	TileIndex interior_tile = TileXY(30, 30); // Interior tile

	/* Attempting to build conduit on interior tile must fail */
	CommandCost res_int = CmdBuildEdgeConduit(DoCommandFlag::Execute, interior_tile, DiagDirection::NE, CargoType{0}, RAILTYPE_BEGIN);
	CHECK(res_int.Failed());
	CHECK(res_int.GetErrorMessage() == STR_ERROR_EDGE_CONDUIT_REQUIRES_VOID);

	/* Building conduit on void-adjacent tile must succeed */
	CommandCost res_build = CmdBuildEdgeConduit(DoCommandFlag::Execute, conduit_tile, DiagDirection::NE, CargoType{0}, RAILTYPE_BEGIN);
	CHECK(res_build.Succeeded());
	UpdateSignalsInBuffer();
	CHECK(EdgeConduitManager::IsConduitTile(conduit_tile));

	const EdgeConduit *c = EdgeConduitManager::GetConduit(conduit_tile);
	REQUIRE(c != nullptr);
	CHECK(c->tile == conduit_tile);
	CHECK(c->world_id == WorldID{0});
	CHECK(c->owner == _current_company);
	CHECK(c->dir == DiagDirection::NE);

	/* Duplicate build must fail */
	CommandCost res_dup = CmdBuildEdgeConduit(DoCommandFlag::Execute, conduit_tile, DiagDirection::NE, CargoType{0}, RAILTYPE_BEGIN);
	CHECK(res_dup.Failed());
	CHECK(res_dup.GetErrorMessage() == STR_ERROR_ALREADY_BUILT);

	/* Demolish conduit */
	CommandCost res_dem = CmdDestroyEdgeConduit(DoCommandFlag::Execute, conduit_tile);
	CHECK(res_dem.Succeeded());
	UpdateSignalsInBuffer();
	CHECK(!EdgeConduitManager::IsConduitTile(conduit_tile));
	CHECK(EdgeConduitManager::GetConduit(conduit_tile) == nullptr);
	CHECK(IsTileType(conduit_tile, TileType::Clear));

	/* The GUI sends an invalid direction for automatic void-facing placement. */
	TileIndex auto_void = TileXY(60, 60);
	TileIndex auto_tile = TileXY(61, 60);
	MakeVoid(auto_void);
	CommandCost res_auto = CmdBuildEdgeConduit(DoCommandFlag::Execute, auto_tile, DiagDirection::Invalid, INVALID_CARGO, RAILTYPE_BEGIN);
	REQUIRE(res_auto.Succeeded());
	UpdateSignalsInBuffer();
	const EdgeConduit *auto_conduit = EdgeConduitManager::GetConduit(auto_tile);
	REQUIRE(auto_conduit != nullptr);
	CHECK(auto_conduit->dir == DiagDirection::NE);

	/* Ordinary OpenTTD dynamite must also remove the sidecar registry entry. */
	CommandCost res_clear = Command<Commands::LandscapeClear>::Do(DoCommandFlag::Execute, auto_tile);
	REQUIRE(res_clear.Succeeded());
	UpdateSignalsInBuffer();
	CHECK(!EdgeConduitManager::IsConduitTile(auto_tile));
	CHECK(IsTileType(auto_tile, TileType::Clear));
}

TEST_CASE("Edge Conduit - Frontier World Extraction Multiplier")
{
	SetupSprint9Environment();

	/* Conduit on World 0 (Core) */
	TileIndex void_w0 = TileXY(50, 50);
	MakeVoid(void_w0);
	TileIndex tile_core = TileXY(51, 50);
	REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, tile_core, DiagDirection::NE, CargoType{0}, RAILTYPE_BEGIN).Succeeded());

	/* Conduit on World 2 (Haven Frontier) */
	TileIndex void_w2 = TileXY(50, 150);
	MakeVoid(void_w2);
	TileIndex tile_frontier = TileXY(51, 150);
	REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, tile_frontier, DiagDirection::NE, CargoType{0}, RAILTYPE_BEGIN).Succeeded());
	UpdateSignalsInBuffer();

	const EdgeConduit *c_core = EdgeConduitManager::GetConduit(tile_core);
	const EdgeConduit *c_frontier = EdgeConduitManager::GetConduit(tile_frontier);
	REQUIRE(c_core != nullptr);
	REQUIRE(c_frontier != nullptr);

	uint32_t prod_core = EdgeConduitManager::CalculateProduction(*c_core);
	uint32_t prod_frontier = EdgeConduitManager::CalculateProduction(*c_frontier);

	/* Frontier world must receive 100% extraction bonus (200% production rate) */
	CHECK(prod_core == 25); // Core world receives 50 / 2 = 25
	CHECK(prod_frontier == 100); // Frontier world receives 50 * 2 = 100
	CHECK(prod_frontier == prod_core * 4);
}

TEST_CASE("Sprint 9 - Savegame Serialization Round-Trip (SPRT & COND)")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_sprint9.sav").string();
	std::filesystem::remove(test_save_file);

	SetupSprint9Environment();

	/* 1. Register a Spaceport */
	StationID sid = StationID{42};
	SpaceportInfo sp_orig{
		.station_id = sid,
		.world_id = WorldID{0},
		.supplies_received = 80,
		.offworld_trade_tier = 3,
		.total_offworld_cargo_generated = 150,
	};
	SpaceportManager::RestoreSpaceport(sp_orig);
	CHECK(SpaceportManager::Count() == 1);

	/* 2. Register an Edge Conduit */
	TileIndex cond_t = TileXY(61, 60);
	MakeVoid(TileXY(60, 60));
	EdgeConduit cond_orig{
		.id = 7,
		.tile = cond_t,
		.dir = DiagDirection::NE,
		.world_id = WorldID{0},
		.cargo_type = CargoType{0},
		.production_rate = 50,
		.owner = _current_company,
		.total_produced = 200,
	};
	EdgeConduitManager::RestoreConduit(cond_orig);
	CHECK(EdgeConduitManager::Count() == 1);

	/* 3. Save Game to disk */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));
	REQUIRE(std::filesystem::file_size(test_save_file) > 0);

	/* 4. Wipe memory registries */
	SpaceportManager::Reset();
	EdgeConduitManager::Reset();
	CHECK(SpaceportManager::Count() == 0);
	CHECK(EdgeConduitManager::Count() == 0);

	/* 5. Load Game back from disk */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	/* 6. Verify Spaceport restoration */
	CHECK(SpaceportManager::Count() == 1);
	CHECK(SpaceportManager::IsSpaceport(sid));
	const SpaceportInfo *restored_sp = SpaceportManager::GetSpaceport(sid);
	REQUIRE(restored_sp != nullptr);
	CHECK(restored_sp->station_id == sid);
	CHECK(restored_sp->world_id == WorldID{0});
	CHECK(restored_sp->offworld_trade_tier == 3);
	CHECK(restored_sp->supplies_received == 80);
	CHECK(restored_sp->total_offworld_cargo_generated == 150);

	/* 7. Verify Edge Conduit restoration */
	CHECK(EdgeConduitManager::Count() == 1);
	CHECK(EdgeConduitManager::IsConduitTile(cond_t));
	const EdgeConduit *restored_cond = EdgeConduitManager::GetConduit(cond_t);
	REQUIRE(restored_cond != nullptr);
	CHECK(restored_cond->id == 7);
	CHECK(restored_cond->tile == cond_t);
	CHECK(restored_cond->world_id == WorldID{0});
	CHECK(restored_cond->dir == DiagDirection::NE);
	CHECK(restored_cond->owner == _current_company);
	CHECK(restored_cond->production_rate == 50);
	CHECK(restored_cond->total_produced == 200);

	std::filesystem::remove(test_save_file);
}

TEST_CASE("Edge Conduit - One-ended head is safe for signal updates")
{
	SetupSprint9Environment();
	const TileIndex tile = TileXY(51, 50);
	const DiagDirection dir = DiagDirection::NE;
	const TileIndex approach = TileAddByDiagDir(tile, ReverseDiagDir(dir));
	MakeVoid(TileXY(50, 50));
	MakeRailNormal(approach, _current_company, TrackBits{DiagDirToDiagTrack(dir)}, RAILTYPE_BEGIN);
	REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, tile, dir, INVALID_CARGO, RAILTYPE_BEGIN).Succeeded());
	CHECK(GetOtherTunnelBridgeEnd(tile) == INVALID_TILE);
	UpdateSignalsInBuffer();
	CHECK(EdgeConduitManager::IsConduitTile(tile));

	CHECK(GetTileTrackStatus(tile, TransportType::Rail, RoadTramType::Invalid, ReverseDiagDir(dir)).trackdirs.None());
	CFollowTrackRail follower(_current_company, RailTypes{RAILTYPE_BEGIN});
	CHECK_FALSE(follower.Follow(approach, DiagDirToDiagTrackdir(dir)));
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);
	CHECK_FALSE(follower.Follow(tile, DiagDirToDiagTrackdir(dir)));
	CHECK(follower.new_tile == INVALID_TILE);
	CHECK(follower.err == CFollowTrackRail::ErrorCode::NoWay);
}

TEST_CASE("Edge Conduit - Physical map edges and corners are coordinate-safe")
{
	struct Case {
		uint x;
		uint y;
		DiagDirection dir;
	};
	const Case cases[] = {
		{1, 32, DiagDirection::NE},
		{62, 32, DiagDirection::SW},
		{32, 1, DiagDirection::NW},
		{32, 62, DiagDirection::SE},
		{1, 1, DiagDirection::NE},
		{62, 1, DiagDirection::SW},
		{1, 62, DiagDirection::NE},
		{62, 62, DiagDirection::SW},
	};

	for (const Case &test : cases) {
		SetupMapEdgeWorld();
		TileIndex tile = TileXY(test.x, test.y);
		INFO("map-edge tile (" << test.x << "," << test.y << ")");
		auto footprint = EdgeConduitManager::ResolvePlacement(tile, test.dir);
		REQUIRE(footprint.has_value());
		CHECK(footprint->void_tile < Map::Size());
		CHECK(IsTileType(footprint->void_tile, TileType::Void));
		CHECK(IsValidTile(footprint->approach_tile));

		CommandCost query = CmdBuildEdgeConduit({}, tile, test.dir, INVALID_CARGO, RAILTYPE_BEGIN);
		REQUIRE(query.Succeeded());
		CHECK(!EdgeConduitManager::IsConduitTile(tile));
		CHECK(!IsTunnelTile(tile));

		REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, tile, test.dir, INVALID_CARGO, RAILTYPE_BEGIN).Succeeded());
		UpdateSignalsInBuffer();
		CHECK(EdgeConduitManager::IsConduitTile(tile));
		CHECK(GetOtherTunnelBridgeEnd(tile) == INVALID_TILE);

		REQUIRE(CmdDestroyEdgeConduit(DoCommandFlag::Execute, tile).Succeeded());
		UpdateSignalsInBuffer();
		CHECK(!EdgeConduitManager::IsConduitTile(tile));
	}
}

TEST_CASE("Edge Conduit - Invalid boundaries, orientation and terrain fail atomically")
{
	SetupMapEdgeWorld();

	for (TileIndex invalid : {INVALID_TILE, TileIndex{Map::Size()}, TileIndex{Map::Size() + 123}}) {
		CHECK(CmdBuildEdgeConduit(DoCommandFlag::Execute, invalid, DiagDirection::NE, INVALID_CARGO, RAILTYPE_BEGIN).Failed());
	}
	UpdateSignalsInBuffer();
	CHECK(EdgeConduitManager::Count() == 0);

	TileIndex edge = TileXY(1, 32);
	CHECK(CmdBuildEdgeConduit(DoCommandFlag::Execute, edge, DiagDirection::SW, INVALID_CARGO, RAILTYPE_BEGIN).Failed());
	CHECK(IsTileType(edge, TileType::Clear));
	CHECK(EdgeConduitManager::Count() == 0);
	UpdateSignalsInBuffer();

	TileIndex water = TileXY(32, Map::MaxY() - 1);
	MakeSea(water);
	CommandCost water_result = CmdBuildEdgeConduit(DoCommandFlag::Execute, water, DiagDirection::SE, INVALID_CARGO, RAILTYPE_BEGIN);
	CHECK(water_result.Failed());
	CHECK(water_result.GetErrorMessage() == STR_ERROR_CAN_T_BUILD_ON_WATER);
	CHECK(IsTileType(water, TileType::Water));
	CHECK(!EdgeConduitManager::IsConduitTile(water));
	UpdateSignalsInBuffer();
}

TEST_CASE("Edge Conduit - Logical boundary, phases and adjacent rail remain supported")
{
	for (WorldPhase phase : {WorldPhase::Phase1_Core, WorldPhase::Phase2_Developed,
			WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion}) {
		SetupMapEdgeWorld(phase);
		TileIndex tile = TileXY(1, 24);
		TileIndex approach = TileXY(2, 24);
		MakeRailNormal(approach, _current_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
		REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, tile, DiagDirection::NE, INVALID_CARGO, RAILTYPE_BEGIN).Succeeded());
		UpdateSignalsInBuffer();
		CHECK(EdgeConduitManager::GetConduit(tile)->world_id == WorldID{0});
		CHECK(IsTileType(approach, TileType::Railway));
	}

	SetupSprint9Environment();
	TileIndex logical_void = TileXY(50, 50);
	TileIndex logical_edge = TileXY(51, 50);
	TileIndex approach = TileXY(52, 50);
	MakeVoid(logical_void);
	MakeRailNormal(approach, _current_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, logical_edge, DiagDirection::Invalid, INVALID_CARGO, RAILTYPE_BEGIN).Succeeded());
	UpdateSignalsInBuffer();
	CHECK(EdgeConduitManager::GetConduit(logical_edge)->dir == DiagDirection::NE);

	TileIndex ordinary_interior = TileXY(30, 30);
	CHECK(CmdBuildEdgeConduit(DoCommandFlag::Execute, ordinary_interior, DiagDirection::Invalid, INVALID_CARGO, RAILTYPE_BEGIN).Failed());
	CHECK(IsTileType(ordinary_interior, TileType::Clear));
	UpdateSignalsInBuffer();
}

TEST_CASE("Edge Conduit - Constructed tile survives save, reload and signal refresh")
{
	const std::string save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_edge_conduit_constructed.sav").string();
	std::filesystem::remove(save_file);
	SetupSprint9Environment();

	TileIndex void_tile = TileXY(50, 50);
	TileIndex conduit_tile = TileXY(51, 50);
	TileIndex approach_tile = TileXY(52, 50);
	MakeVoid(void_tile);
	MakeRailNormal(approach_tile, _current_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
	REQUIRE(CmdBuildEdgeConduit(DoCommandFlag::Execute, conduit_tile, DiagDirection::NE, INVALID_CARGO, RAILTYPE_BEGIN).Succeeded());
	UpdateSignalsInBuffer();

	REQUIRE(SaveOrLoad(save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(EdgeConduitManager::IsConduitTile(conduit_tile));
	CHECK(IsTunnelTile(conduit_tile));
	CHECK(GetOtherTunnelBridgeEnd(conduit_tile) == INVALID_TILE);

	AddSideToSignalBuffer(conduit_tile, DiagDirection::Invalid, _current_company);
	UpdateSignalsInBuffer();
	REQUIRE(CmdDestroyEdgeConduit(DoCommandFlag::Execute, conduit_tile).Succeeded());
	UpdateSignalsInBuffer();
	CHECK(!EdgeConduitManager::IsConduitTile(conduit_tile));

	REQUIRE(SaveOrLoad(save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	CHECK(!EdgeConduitManager::IsConduitTile(conduit_tile));
	CHECK(!IsTunnelTile(conduit_tile));
	std::filesystem::remove(save_file);
}
