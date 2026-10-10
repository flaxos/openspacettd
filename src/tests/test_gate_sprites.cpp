/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_gate_sprites.cpp Unit tests for Commonwealth wormhole gate sprite runtime resolver. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/gate_sprites.h"
#include "../portal/visual_overhaul.h"
#include "../portal/portal_registry.h"
#include "../portal/planet_manager.h"
#include "../direction_type.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"

TEST_CASE("PortalGateSprites initial state and reset lifecycle", "[gate-sprites]")
{
	PortalGateSprites::Reset();

	CHECK_FALSE(PortalGateSprites::IsActive());
	CHECK(PortalGateSprites::GetBase() == 0);

	for (DiagDirection dir : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
		CHECK(PortalGateSprites::GetSprite(dir, WormholeVisualState::ActiveCyanPulse, false) == 0);
		CHECK(PortalGateSprites::GetSprite(dir, WormholeVisualState::ActiveCyanPulse, true) == 0);
		CHECK(PortalGateSprites::GetSprite(dir, WormholeVisualState::StandbyAmber, false) == 0);
		CHECK(PortalGateSprites::GetSprite(dir, WormholeVisualState::StandbyAmber, true) == 0);
		CHECK(PortalGateSprites::GetSprite(dir, WormholeVisualState::TransitRipple, false) == 0);
		CHECK(PortalGateSprites::GetSprite(dir, WormholeVisualState::QuarantineRed, false) == 0);
	}
}

TEST_CASE("PortalGateSprites directional and state index resolution", "[gate-sprites]")
{
	PortalGateSprites::Reset();
	constexpr SpriteID TEST_BASE = 50000;
	PortalGateSprites::SetBaseForTesting(TEST_BASE);

	REQUIRE(PortalGateSprites::IsActive());
	REQUIRE(PortalGateSprites::GetBase() == TEST_BASE);

	/* ActiveCyanPulse (offset 0..7) */
	/* NE: rear=50000, front=50001 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::ActiveCyanPulse, false) == TEST_BASE + 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::ActiveCyanPulse, true) == TEST_BASE + 1);

	/* SE: rear=50002, front=50003 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SE, WormholeVisualState::ActiveCyanPulse, false) == TEST_BASE + 2);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SE, WormholeVisualState::ActiveCyanPulse, true) == TEST_BASE + 3);

	/* SW: rear=50004, front=50005 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, WormholeVisualState::ActiveCyanPulse, false) == TEST_BASE + 4);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, WormholeVisualState::ActiveCyanPulse, true) == TEST_BASE + 5);

	/* NW: rear=50006, front=50007 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NW, WormholeVisualState::ActiveCyanPulse, false) == TEST_BASE + 6);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NW, WormholeVisualState::ActiveCyanPulse, true) == TEST_BASE + 7);

	/* StandbyAmber (offset 8..15) */
	/* NE: rear=50008, front=50009 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::StandbyAmber, false) == TEST_BASE + 8);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::StandbyAmber, true) == TEST_BASE + 9);

	/* SE: rear=50010, front=50011 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SE, WormholeVisualState::StandbyAmber, false) == TEST_BASE + 10);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SE, WormholeVisualState::StandbyAmber, true) == TEST_BASE + 11);

	/* SW: rear=50012, front=50013 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, WormholeVisualState::StandbyAmber, false) == TEST_BASE + 12);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, WormholeVisualState::StandbyAmber, true) == TEST_BASE + 13);

	/* NW: rear=50014, front=50015 */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NW, WormholeVisualState::StandbyAmber, false) == TEST_BASE + 14);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NW, WormholeVisualState::StandbyAmber, true) == TEST_BASE + 15);

	/* Unhandled visual states fall back cleanly to native tunnels (returns 0) */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::TransitRipple, false) == 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::TransitRipple, true) == 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::QuarantineRed, false) == 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, WormholeVisualState::QuarantineRed, true) == 0);

	/* Invalid direction */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::End, WormholeVisualState::ActiveCyanPulse, false) == 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::End, WormholeVisualState::ActiveCyanPulse, true) == 0);

	/* Cleanup */
	PortalGateSprites::Reset();
	CHECK_FALSE(PortalGateSprites::IsActive());
}

TEST_CASE("PortalGateSprites integration with event horizon visual states", "[gate-sprites]")
{
	Map::Allocate(64, 64);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}

	PortalRegistry::Reset();
	VisualOverhaulManager::Reset();
	PortalGateSprites::Reset();

	constexpr SpriteID TEST_BASE = 60000;
	PortalGateSprites::SetBaseForTesting(TEST_BASE);
	REQUIRE(PortalGateSprites::IsActive());

	TileIndex gate_a = TileXY(10, 10);
	TileIndex gate_b = TileXY(20, 20);

	/* 1. Unlinked standby gate: Amber excitation across all directions */
	PortalRegistry::RegisterUnlinkedGate(gate_a, DiagDirection::SW, WorldID{0});
	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_a) == WormholeVisualState::StandbyAmber);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, VisualOverhaulManager::GetEventHorizonVisualState(gate_a), false) == TEST_BASE + 12);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, VisualOverhaulManager::GetEventHorizonVisualState(gate_a), true) == TEST_BASE + 13);

	/* 2. Linked active portal: Cyan excitation */
	PortalRegistry::RegisterPortalPair(gate_a, DiagDirection::SW, WorldID{0}, gate_b, DiagDirection::NE, WorldID{1}, 10, true);
	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_a) == WormholeVisualState::ActiveCyanPulse);
	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_b) == WormholeVisualState::ActiveCyanPulse);

	/* Gate A (SW) */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, VisualOverhaulManager::GetEventHorizonVisualState(gate_a), false) == TEST_BASE + 4);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, VisualOverhaulManager::GetEventHorizonVisualState(gate_a), true) == TEST_BASE + 5);

	/* Gate B (NE) */
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, VisualOverhaulManager::GetEventHorizonVisualState(gate_b), false) == TEST_BASE + 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::NE, VisualOverhaulManager::GetEventHorizonVisualState(gate_b), true) == TEST_BASE + 1);

	/* 3. Train transit shimmer: TransitRipple falls back to native tunnel sprites (returns 0) */
	VisualOverhaulManager::OnTrainTraversePortal(gate_a, VehicleID{1});
	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_a) == WormholeVisualState::TransitRipple);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, VisualOverhaulManager::GetEventHorizonVisualState(gate_a), false) == 0);
	CHECK(PortalGateSprites::GetSprite(DiagDirection::SW, VisualOverhaulManager::GetEventHorizonVisualState(gate_a), true) == 0);

	/* 4. Reset & cleanup */
	PortalGateSprites::Reset();
	PortalRegistry::Reset();
	VisualOverhaulManager::Reset();
	CHECK_FALSE(PortalGateSprites::IsActive());
}
