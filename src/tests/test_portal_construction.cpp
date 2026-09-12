/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_portal_construction.cpp Unit tests for player portal gateway construction, linking, demolition, and safeguards. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../command_func.h"
#include "../landscape_cmd.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/portal_terminal.h"
#include "../portal/portal_cmd.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../tunnelbridge.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../clear_map.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../vehicle_func.h"
#include "../train.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../table/strings.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "mock_environment.h"

#include <filesystem>

#include "../safeguards.h"

static void SetupTestWorlds(uint32_t map_w = 256, uint32_t map_h = 256)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	PortalRegistry::Reset();
	PlanetManager::Reset();
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

	/* World 1: (150..240, 10..100) */
	PlanetRegion w1{
		.id = WorldID{1},
		.name = "Vulcan Alpha",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 150,
		.min_y = 10,
		.max_x = 240,
		.max_y = 100,
		.development_score = 5000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w1));

	/* World 2: (10..100, 150..240) */
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Ceres Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 10,
		.min_y = 150,
		.max_x = 100,
		.max_y = 240,
		.development_score = 2000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w2));

	PlanetManager::RebuildSpatialGrid();
}

TEST_CASE("Portal Construction - Unlinked Gate Lifecycle and Restrictions")
{
	SetupTestWorlds();

	TileIndex tile_w0 = TileXY(50, 50); // Inside World 0
	TileIndex tile_void = TileXY(120, 50); // Inside void buffer (101..149)

	/* 1. Attempt building in void buffer space: should be rejected */
	CommandCost res_void = CmdBuildPortalGate({}, tile_void, DiagDirection::NE, RAILTYPE_BEGIN);
	CHECK(res_void.Failed());
	CHECK(res_void.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	/* 2. Successfully construct unlinked portal gate on World 0 */
	CommandCost res_build = CmdBuildPortalGate(DoCommandFlag::Execute, tile_w0, DiagDirection::NE, RAILTYPE_BEGIN);
	CHECK(res_build.Succeeded());
	UpdateSignalsInBuffer();
	CHECK(IsTunnelTile(tile_w0));
	CHECK(PortalRegistry::IsUnlinkedGate(tile_w0));
	CHECK(!PortalRegistry::IsPortalTile(tile_w0)); // Not linked yet!

	/* Unlinked gate properties */
	const PortalEndpoint *ep = PortalRegistry::GetUnlinkedGate(tile_w0);
	REQUIRE(ep != nullptr);
	CHECK(ep->tile == tile_w0);
	CHECK(ep->enter_dir == DiagDirection::NE);
	CHECK(ep->world_id == WorldID{0});

	/* Engine hooks for unlinked gates */
	CHECK(GetOtherTunnelEnd(tile_w0) == INVALID_TILE);
	CHECK(GetOtherTunnelBridgeEnd(tile_w0) == INVALID_TILE);
	CHECK(GetTunnelBridgeLength(tile_w0, INVALID_TILE) == 0);

	/* 3. Attempt building duplicate gate on already-built tile: should be rejected */
	CommandCost res_dup = CmdBuildPortalGate({}, tile_w0, DiagDirection::NE, RAILTYPE_BEGIN);
	CHECK(res_dup.Failed());
	CHECK(res_dup.GetErrorMessage() == STR_ERROR_ALREADY_BUILT);
}

TEST_CASE("Portal Construction - High-capacity terminal topology and PBS directions")
{
	SetupTestWorlds();
	TileIndex gate = TileXY(50, 50);

	for (uint8_t value = to_underlying(DiagDirection::Begin); value < to_underlying(DiagDirection::End); ++value) {
		DiagDirection dir = static_cast<DiagDirection>(value);
		INFO("gate direction " << static_cast<uint>(value));
		std::optional<PortalTerminalLayout> planned = PortalTerminal::Plan(gate, dir, WorldID{0});
		REQUIRE(planned.has_value());
		CHECK(planned->tiles.size() == 34);
		CHECK(planned->GetTrackPieceCount() == 36);
		CHECK(planned->signals.size() == 2);
		CHECK(PlanetManager::GetTileWorld(planned->connection_tile) == WorldID{0});
	}

	std::optional<PortalTerminalLayout> terminal = PortalTerminal::Plan(gate, DiagDirection::NE, WorldID{0});
	REQUIRE(terminal.has_value());
	REQUIRE(CmdBuildPortalGate(DoCommandFlag::Execute, gate, DiagDirection::NE, RAILTYPE_BEGIN).Succeeded());
	UpdateSignalsInBuffer();

	for (const PortalTerminalTile &part : terminal->tiles) {
		CAPTURE(part.tile);
		CHECK(IsPlainRailTile(part.tile));
		CHECK(GetTrackBits(part.tile) == part.tracks);
	}
	for (const PortalTerminalSignal &signal : terminal->signals) {
		CAPTURE(signal.tile);
		CHECK(HasSignalOnTrack(signal.tile, signal.track));
		CHECK(GetSignalType(signal.tile, signal.track) == SignalType::PathOneWay);
		CHECK(GetPresentSignals(signal.tile) == SignalAlongTrackdir(DiagDirToDiagTrackdir(signal.travel_dir)));
	}
}

TEST_CASE("Portal Construction - Terminal footprint rejection is atomic")
{
	SetupTestWorlds();

	/* The gate head and immediate approach are legal, but the 18-tile terminal
	 * would cross the logical world boundary. */
	TileIndex boundary_gate = TileXY(20, 30);
	CommandCost boundary = CmdBuildPortalGate(DoCommandFlag::Execute, boundary_gate, DiagDirection::SW, RAILTYPE_BEGIN);
	CHECK(boundary.Failed());
	CHECK(boundary.GetErrorMessage() == STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);
	CHECK(IsTileType(boundary_gate, TileType::Clear));
	CHECK(!PortalRegistry::IsUnlinkedGate(boundary_gate));

	/* Existing infrastructure anywhere in the bay rejects the whole build and
	 * remains untouched. */
	TileIndex gate = TileXY(50, 50);
	auto terminal = PortalTerminal::Plan(gate, DiagDirection::NE, WorldID{0});
	REQUIRE(terminal.has_value());
	TileIndex obstruction = terminal->tiles.at(8).tile;
	MakeRailNormal(obstruction, _current_company, TrackBits{Track::Y}, RAILTYPE_BEGIN);
	CommandCost blocked = CmdBuildPortalGate(DoCommandFlag::Execute, gate, DiagDirection::NE, RAILTYPE_BEGIN);
	CHECK(blocked.Failed());
	CHECK(blocked.GetErrorMessage() == STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);
	CHECK(IsTileType(gate, TileType::Clear));
	CHECK(IsPlainRailTile(obstruction));
	CHECK(GetTrackBits(obstruction) == TrackBits{Track::Y});
	CHECK(!PortalRegistry::IsUnlinkedGate(gate));

	/* A blocked second terminal must not partially construct the first end. */
	TileIndex pair_a = TileXY(50, 70);
	TileIndex pair_b = TileXY(180, 70);
	auto terminal_b = PortalTerminal::Plan(pair_b, DiagDirection::SW, WorldID{1});
	REQUIRE(terminal_b.has_value());
	TileIndex pair_obstruction = terminal_b->tiles.at(10).tile;
	MakeRailNormal(pair_obstruction, _current_company, TrackBits{Track::Y}, RAILTYPE_BEGIN);
	CHECK(CmdBuildPortalPair(DoCommandFlag::Execute, pair_a, DiagDirection::NE,
			pair_b, DiagDirection::SW, RAILTYPE_BEGIN).Failed());
	CHECK(IsTileType(pair_a, TileType::Clear));
	CHECK(!PortalRegistry::IsPortalTile(pair_a));
	CHECK(!PortalRegistry::IsPortalTile(pair_b));
	CHECK(IsPlainRailTile(pair_obstruction));
}

TEST_CASE("Portal Construction - Authoritative world and Phase placement rules")
{
	SetupTestWorlds();
	static_assert(to_underlying(Commands::BuildPortalGate) == 146);
	static_assert(to_underlying(Commands::BuildEdgeConduit) == 151);

	/* All currently modelled Phases permit a gate. The documented restriction
	 * is registered-world membership, not a Phase 1/2/3 technology gate. */
	for (TileIndex tile : {TileXY(30, 30), TileXY(170, 30), TileXY(30, 170)}) {
		CommandCost query = CmdBuildPortalGate({}, tile, DiagDirection::NE, RAILTYPE_BEGIN);
		REQUIRE(query.Succeeded());
		CHECK(!PortalRegistry::IsUnlinkedGate(tile));
		REQUIRE(CmdBuildPortalGate(DoCommandFlag::Execute, tile, DiagDirection::NE, RAILTYPE_BEGIN).Succeeded());
		UpdateSignalsInBuffer();
		CHECK(PortalRegistry::IsUnlinkedGate(tile));
	}

	TileIndex void_buffer = TileXY(120, 50);
	CHECK(CmdBuildPortalGate(DoCommandFlag::Execute, void_buffer, DiagDirection::NE, RAILTYPE_BEGIN).Failed());
	CHECK(!PortalRegistry::IsUnlinkedGate(void_buffer));
	CHECK(CmdBuildPortalGate(DoCommandFlag::Execute, INVALID_TILE, DiagDirection::NE, RAILTYPE_BEGIN).Failed());

	/* The gate can face any direction, but its rail approach must remain in the
	 * same logical world. At the minimum-X boundary SW would put the approach
	 * into the unregistered buffer, so the authoritative command rejects it. */
	TileIndex boundary = TileXY(10, 50);
	CHECK(CmdBuildPortalGate(DoCommandFlag::Execute, boundary, DiagDirection::SW, RAILTYPE_BEGIN).Failed());
	CHECK(!PortalRegistry::IsUnlinkedGate(boundary));

	TileIndex valid_pair_end = TileXY(40, 40);
	TileIndex invalid_pair_end = TileXY(120, 60);
	CHECK(CmdBuildPortalPair(DoCommandFlag::Execute, valid_pair_end, DiagDirection::NE,
		invalid_pair_end, DiagDirection::SW, RAILTYPE_BEGIN).Failed());
	CHECK(!PortalRegistry::IsUnlinkedGate(valid_pair_end));
	CHECK(!IsTunnelTile(valid_pair_end));
	UpdateSignalsInBuffer();
}

TEST_CASE("Portal Construction - Cross-World Gate Linking")
{
	SetupTestWorlds();

	TileIndex tile_a = TileXY(50, 50);  // World 0
	TileIndex tile_b = TileXY(180, 50); // World 1
	TileIndex tile_c = TileXY(80, 80);  // World 0, clear of gate A's terminal

	/* Construct 3 unlinked gates */
	REQUIRE(CmdBuildPortalGate(DoCommandFlag::Execute, tile_a, DiagDirection::NE, RAILTYPE_BEGIN).Succeeded());
	REQUIRE(CmdBuildPortalGate(DoCommandFlag::Execute, tile_b, DiagDirection::SW, RAILTYPE_BEGIN).Succeeded());
	REQUIRE(CmdBuildPortalGate(DoCommandFlag::Execute, tile_c, DiagDirection::SE, RAILTYPE_BEGIN).Succeeded());
	UpdateSignalsInBuffer();

	/* 1. Attempt same-world linking (tile_a on World 0 and tile_c on World 0): must fail */
	CommandCost res_same_world = CmdLinkPortalGates({}, tile_a, tile_c);
	CHECK(res_same_world.Failed());
	CHECK(res_same_world.GetErrorMessage() == STR_ERROR_PORTAL_GATES_DIFFERENT_WORLDS);

	/* 2. Successfully link gate A (World 0) and gate B (World 1) */
	CommandCost res_link = CmdLinkPortalGates({}, tile_a, tile_b);
	CHECK(res_link.Succeeded());
	CHECK(res_link.GetExpensesType() == ExpensesType::Construction);
	CHECK(IsNetworkRegisteredCallback(&CcPortalLink));
	REQUIRE(Command<Commands::LinkPortalGates>::Post(STR_ERROR_CAN_T_LINK_PORTAL_GATES, CcPortalLink, tile_a, tile_b));

	/* Verify they transitioned from unlinked to active linked portal pair */
	CHECK(!PortalRegistry::IsUnlinkedGate(tile_a));
	CHECK(!PortalRegistry::IsUnlinkedGate(tile_b));
	CHECK(PortalRegistry::IsPortalTile(tile_a));
	CHECK(PortalRegistry::IsPortalTile(tile_b));

	/* Verify bidirectional wormhole resolution */
	CHECK(GetOtherTunnelEnd(tile_a) == tile_b);
	CHECK(GetOtherTunnelEnd(tile_b) == tile_a);
	CHECK(GetOtherTunnelBridgeEnd(tile_a) == tile_b);
	CHECK(GetOtherTunnelBridgeEnd(tile_b) == tile_a);

	/* Verify virtual traversal length */
	uint32_t expected_virt_len = std::max(2u, DistanceManhattan(tile_a, tile_b) / 4);
	CHECK(PortalRegistry::GetPortalVirtualLength(tile_a) == expected_virt_len);
	CHECK(PortalRegistry::GetPortalVirtualLength(tile_b) == expected_virt_len);

	/* Gate C remains unlinked */
	CHECK(PortalRegistry::IsUnlinkedGate(tile_c));
	CHECK(GetOtherTunnelEnd(tile_c) == INVALID_TILE);
}

TEST_CASE("Portal Construction - Atomic Pair Builder")
{
	SetupTestWorlds();

	TileIndex tile_w1 = TileXY(190, 60);  // World 1
	TileIndex tile_w2 = TileXY(50, 190);  // World 2

	CommandCost res = CmdBuildPortalPair(
		DoCommandFlag::Execute,
		tile_w1, DiagDirection::SW,
		tile_w2, DiagDirection::NE,
		RAILTYPE_BEGIN
	);
	CHECK(res.Succeeded());
	UpdateSignalsInBuffer();

	CHECK(PortalRegistry::IsPortalTile(tile_w1));
	CHECK(PortalRegistry::IsPortalTile(tile_w2));
	CHECK(GetOtherTunnelEnd(tile_w1) == tile_w2);
	CHECK(GetOtherTunnelEnd(tile_w2) == tile_w1);
}

TEST_CASE("Portal Construction - Demolition Safeguards While Consist In Transit")
{
	SetupTestWorlds();

	TileIndex tile_a = TileXY(50, 50);
	TileIndex tile_b = TileXY(180, 50);

	REQUIRE(CmdBuildPortalPair(
		DoCommandFlag::Execute,
		tile_a, DiagDirection::NE,
		tile_b, DiagDirection::SW,
		RAILTYPE_BEGIN
	).Succeeded());
	UpdateSignalsInBuffer();

	/* 1. When no train is in transit: free and can be demolished */
	CHECK(!PortalRegistry::IsPortalInTransit(tile_a));
	CHECK(!PortalRegistry::IsPortalInTransit(tile_b));
	CHECK(TunnelBridgeIsFree(tile_a, tile_b).Succeeded());

	/* 2. Simulate train consist entering wormhole from tile_a */
	REQUIRE(Vehicle::CanAllocateItem(1));
	Train *t = Vehicle::Create<Train>();
	t->SetFrontEngine();
	t->owner = Owner(0);
	t->direction = Direction::NE;
	t->tile = tile_a;
	t->track = Track::Wormhole;
	t->vehstatus.Set(VehState::Hidden);
	t->compatible_railtypes = RailTypes{RAILTYPE_BEGIN};
	t->railtypes = RailTypes{RAILTYPE_BEGIN};

	PortalRegistry::AdvancePortalTransit(t->index);

	/* 3. In-transit guards trigger */
	CHECK(PortalRegistry::IsPortalInTransit(tile_a));
	CHECK(PortalRegistry::IsPortalInTransit(tile_b));

	/* TunnelBridgeIsFree must reject demolition */
	CommandCost free_check = TunnelBridgeIsFree(tile_a, tile_b);
	CHECK(free_check.Failed());
	CHECK(free_check.GetErrorMessage() == STR_ERROR_TRAIN_IN_THE_WAY);

	/* CmdDestroyPortalGate must reject demolition */
	CommandCost dem_a = CmdDestroyPortalGate({}, tile_a, false);
	CHECK(dem_a.Failed());
	CHECK(dem_a.GetErrorMessage() == STR_ERROR_TRAIN_IN_THE_WAY);

	CommandCost dem_b = CmdDestroyPortalGate({}, tile_b, false);
	CHECK(dem_b.Failed());
	CHECK(dem_b.GetErrorMessage() == STR_ERROR_TRAIN_IN_THE_WAY);

	/* 4. Train completes transit and emerges */
	PortalRegistry::ClearPortalTransit(t->index);
	t->tile = tile_b;
	t->track = Track::X;
	t->vehstatus.Reset(VehState::Hidden);

	/* Demolition safeguards cleared once train leaves wormhole entrance */
	CHECK(!PortalRegistry::IsPortalInTransit(tile_a));
}

TEST_CASE("Portal Construction - Partial Demolition Unlinking")
{
	SetupTestWorlds();

	TileIndex tile_a = TileXY(50, 50);
	TileIndex tile_b = TileXY(180, 50);

	REQUIRE(CmdBuildPortalPair(
		DoCommandFlag::Execute,
		tile_a, DiagDirection::NE,
		tile_b, DiagDirection::SW,
		RAILTYPE_BEGIN
	).Succeeded());
	UpdateSignalsInBuffer();

	/* Demolish only gate A (demolish_both = false) */
	CommandCost res_dem = CmdDestroyPortalGate(DoCommandFlag::Execute, tile_a, false);
	CHECK(res_dem.Succeeded());
	CHECK(res_dem.GetExpensesType() == ExpensesType::Construction);
	UpdateSignalsInBuffer();

	/* Tile A is cleared */
	CHECK(!IsTunnelTile(tile_a));
	CHECK(!PortalRegistry::IsPortalTile(tile_a));
	CHECK(!PortalRegistry::IsUnlinkedGate(tile_a));

	/* Gate B survives and reverts into an unlinked gate! */
	CHECK(IsTunnelTile(tile_b));
	CHECK(!PortalRegistry::IsPortalTile(tile_b)); // No longer part of active link
	CHECK(PortalRegistry::IsUnlinkedGate(tile_b)); // Now an unlinked dormant gate
	CHECK(GetOtherTunnelEnd(tile_b) == INVALID_TILE);

	/* Demolish the remaining unlinked gate B */
	CommandCost res_dem_b = CmdDestroyPortalGate(DoCommandFlag::Execute, tile_b, false);
	CHECK(res_dem_b.Succeeded());
	UpdateSignalsInBuffer();

	CHECK(!IsTunnelTile(tile_b));
	CHECK(!PortalRegistry::IsUnlinkedGate(tile_b));
}

TEST_CASE("Portal Construction - Area demolition safely removes a reserved linked pair")
{
	SetupTestWorlds();

	TileIndex tile_a = TileXY(50, 50);
	TileIndex tile_b = TileXY(180, 50);
	REQUIRE(CmdBuildPortalPair(
		DoCommandFlag::Execute,
		tile_a, DiagDirection::NE,
		tile_b, DiagDirection::SW,
		RAILTYPE_BEGIN
	).Succeeded());
	UpdateSignalsInBuffer();

	Company *company = Company::Get(_current_company);
	REQUIRE(company != nullptr);
	uint terminal_piece_count = 2 * 36;
	CHECK(company->infrastructure.rail[RAILTYPE_BEGIN] == 2 * TUNNELBRIDGE_TRACKBIT_FACTOR + terminal_piece_count);

	/* Reproduce the area-bulldozer path from the crash. Both heads can carry
	 * the same tunnel reservation while the reserving train is elsewhere. */
	SetTunnelBridgeReservation(tile_a, true);
	SetTunnelBridgeReservation(tile_b, true);
	CHECK(HasTunnelBridgeReservation(tile_a));
	CHECK(HasTunnelBridgeReservation(tile_b));

	auto [clear_result, money] = Command<Commands::ClearArea>::Do(DoCommandFlag::Execute, tile_a, tile_a, false);
	REQUIRE(clear_result.Succeeded());
	UpdateSignalsInBuffer();

	CHECK(!PortalRegistry::IsPortalTile(tile_a));
	CHECK(!PortalRegistry::IsPortalTile(tile_b));
	CHECK(IsTileType(tile_a, TileType::Clear));
	CHECK(IsTileType(tile_b, TileType::Clear));
	/* Demolishing a head intentionally preserves its ordinary rail terminal so
	 * players can reuse or alter the approaches without losing infrastructure. */
	CHECK(company->infrastructure.rail[RAILTYPE_BEGIN] == terminal_piece_count);
}

TEST_CASE("Portal Construction - Savegame Persistence of Unlinked and Linked Portals")
{
	SetupTestWorlds();

	std::string test_save_file = (std::filesystem::temp_directory_path() / "test_portal_constr_save.sav").string();
	if (std::filesystem::exists(test_save_file)) std::filesystem::remove(test_save_file);

	/* Build 1 active linked portal pair (World 0 <-> World 1) */
	TileIndex pair_a = TileXY(40, 40);
	TileIndex pair_b = TileXY(170, 40);
	REQUIRE(CmdBuildPortalPair(
		DoCommandFlag::Execute,
		pair_a, DiagDirection::NE,
		pair_b, DiagDirection::SW,
		RAILTYPE_BEGIN
	).Succeeded());

	/* Build 1 unlinked gate on World 2 */
	TileIndex unlinked_tile = TileXY(50, 180);
	REQUIRE(CmdBuildPortalGate(
		DoCommandFlag::Execute,
		unlinked_tile, DiagDirection::SE,
		RAILTYPE_BEGIN
	).Succeeded());
	UpdateSignalsInBuffer();

	CHECK(PortalRegistry::Count() == 1);
	CHECK(PortalRegistry::GetUnlinkedGates().size() == 1);

	/* Save the game */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));

	/* Clear memory */
	PortalRegistry::Reset();
	PlanetManager::Reset();
	CHECK(PortalRegistry::Count() == 0);
	CHECK(PortalRegistry::GetUnlinkedGates().empty());

	/* Load the game */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	/* Verify active pair restored */
	CHECK(PortalRegistry::Count() == 1);
	CHECK(PortalRegistry::IsPortalTile(pair_a));
	CHECK(PortalRegistry::IsPortalTile(pair_b));
	CHECK(GetOtherTunnelEnd(pair_a) == pair_b);
	CHECK(GetOtherTunnelEnd(pair_b) == pair_a);

	/* Verify unlinked gate restored */
	CHECK(PortalRegistry::IsUnlinkedGate(unlinked_tile));
	CHECK(!PortalRegistry::IsPortalTile(unlinked_tile));
	const PortalEndpoint *restored_un = PortalRegistry::GetUnlinkedGate(unlinked_tile);
	REQUIRE(restored_un != nullptr);
	CHECK(restored_un->tile == unlinked_tile);
	CHECK(restored_un->enter_dir == DiagDirection::SE);
	CHECK(restored_un->world_id == WorldID{2});

	/* Cleanup */
	if (std::filesystem::exists(test_save_file)) std::filesystem::remove(test_save_file);
}
