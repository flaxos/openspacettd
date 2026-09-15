/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file portal_terminal.cpp Deterministic high-capacity rail throats for Portal Gates. */

#include "../stdafx.h"
#include "portal_terminal.h"

#include "planet_manager.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../direction_func.h"
#include "../map_func.h"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../tile_map.h"
#include "../tunnelbridge_map.h"
#include "../track_func.h"
#include "../viewport_func.h"
#include "../window_func.h"

#include "../safeguards.h"

bool PortalTerminal::AdoptForUAT(const PortalTerminalLayout &layout, CompanyID company, bool execute)
{
	if (!Company::IsValidID(company)) return false;
	auto admissible = [company](TileIndex tile) {
		return tile < Map::Size() && (GetTileOwner(tile) == OWNER_NONE || GetTileOwner(tile) == company);
	};
	if (!admissible(layout.gate_tile) || !IsTunnelTile(layout.gate_tile) || GetTunnelBridgeTransportType(layout.gate_tile) != TransportType::Rail) return false;
	for (const auto &part : layout.tiles) {
		if (!admissible(part.tile) || !IsPlainRailTile(part.tile) || (GetTrackBits(part.tile) & part.tracks) != part.tracks) return false;
	}
	if (execute) {
		SetTileOwner(layout.gate_tile, company);
		MarkTileDirtyByTile(layout.gate_tile);
		for (const auto &part : layout.tiles) {
			SetTileOwner(part.tile, company);
			MarkTileDirtyByTile(part.tile);
		}
	}
	return true;
}

namespace {

static TileIndex OffsetTile(TileIndex origin, DiagDirection longitudinal_dir, int longitudinal, DiagDirection lateral_dir, int lateral)
{
	if (origin >= Map::Size() || !IsValidDiagDirection(longitudinal_dir) || !IsValidDiagDirection(lateral_dir)) return INVALID_TILE;

	int x = static_cast<int>(TileX(origin));
	int y = static_cast<int>(TileY(origin));
	auto apply = [&x, &y](DiagDirection dir, int distance) {
		switch (dir) {
			case DiagDirection::NE: x -= distance; break;
			case DiagDirection::SE: y += distance; break;
			case DiagDirection::SW: x += distance; break;
			case DiagDirection::NW: y -= distance; break;
			default: break;
		}
	};
	apply(longitudinal_dir, longitudinal);
	apply(lateral_dir, lateral);

	if (x < 0 || y < 0 || x > static_cast<int>(Map::MaxX()) || y > static_cast<int>(Map::MaxY())) return INVALID_TILE;
	return TileXY(x, y);
}

static Track TrackBetween(DiagDirection first, DiagDirection second)
{
	for (uint8_t value = to_underlying(Track::Begin); value < to_underlying(Track::End); ++value) {
		Track track = static_cast<Track>(value);
		if (TrackExitdirToTrackdir(track, first) != Trackdir::Invalid && TrackExitdirToTrackdir(track, second) != Trackdir::Invalid) return track;
	}
	return Track::Invalid;
}

static bool AddTrack(PortalTerminalLayout &layout, TileIndex tile, Track track)
{
	if (tile == INVALID_TILE || !IsValidTrack(track)) return false;
	for (PortalTerminalTile &part : layout.tiles) {
		if (part.tile != tile) continue;
		part.tracks.Set(track);
		return true;
	}
	layout.tiles.push_back({tile, TrackBits{track}});
	return true;
}

} // namespace

TrackBits PortalTerminalLayout::GetTracks(TileIndex tile) const
{
	for (const PortalTerminalTile &part : this->tiles) {
		if (part.tile == tile) return part.tracks;
	}
	return {};
}

uint PortalTerminalLayout::GetTrackPieceCount() const
{
	uint count = 0;
	for (const PortalTerminalTile &part : this->tiles) count += part.tracks.Count();
	return count;
}

std::optional<PortalTerminalLayout> PortalTerminal::Plan(TileIndex gate_tile, DiagDirection gate_dir, WorldID world_id)
{
	if (gate_tile >= Map::Size() || !IsValidDiagDirection(gate_dir) || world_id == INVALID_WORLD) return std::nullopt;

	PortalTerminalLayout layout;
	layout.gate_tile = gate_tile;
	layout.gate_dir = gate_dir;
	layout.world_id = world_id;
	layout.outward_dir = ReverseDiagDir(gate_dir);
	DiagDirection lateral_dir = ChangeDiagDir(layout.outward_dir, DiagDirDiff::Right90);
	DiagDirection inward_dir = gate_dir;
	DiagDirection reverse_lateral = ReverseDiagDir(lateral_dir);
	Track straight = DiagDirToDiagTrack(layout.outward_dir);

	/* Main exit lane and common player connection. */
	for (uint distance = 1; distance <= PORTAL_TERMINAL_APPROACH_LENGTH; distance++) {
		TileIndex tile = OffsetTile(gate_tile, layout.outward_dir, distance, lateral_dir, 0);
		if (!AddTrack(layout, tile, straight)) return std::nullopt;
		if (distance == PORTAL_TERMINAL_APPROACH_LENGTH) layout.connection_tile = tile;
	}

	/* Near switch: divert the entering lane one tile to the right. */
	TileIndex near_junction = OffsetTile(gate_tile, layout.outward_dir, 2, lateral_dir, 0);
	TileIndex near_curve = OffsetTile(gate_tile, layout.outward_dir, 2, lateral_dir, 1);
	if (!AddTrack(layout, near_junction, TrackBetween(inward_dir, lateral_dir))) return std::nullopt;
	if (!AddTrack(layout, near_curve, TrackBetween(reverse_lateral, layout.outward_dir))) return std::nullopt;

	/* Fourteen clear tiles form the inbound holding bay. */
	for (uint distance = 3; distance <= PORTAL_TERMINAL_HOLDING_LENGTH + 2; distance++) {
		TileIndex tile = OffsetTile(gate_tile, layout.outward_dir, distance, lateral_dir, 1);
		if (!AddTrack(layout, tile, straight)) return std::nullopt;
	}

	/* Far switch: merge both lanes into the single external connection. */
	uint far_distance = PORTAL_TERMINAL_HOLDING_LENGTH + 3;
	TileIndex far_curve = OffsetTile(gate_tile, layout.outward_dir, far_distance, lateral_dir, 1);
	TileIndex far_junction = OffsetTile(gate_tile, layout.outward_dir, far_distance, lateral_dir, 0);
	if (!AddTrack(layout, far_curve, TrackBetween(inward_dir, reverse_lateral))) return std::nullopt;
	if (!AddTrack(layout, far_junction, TrackBetween(lateral_dir, layout.outward_dir))) return std::nullopt;

	/* Reject physical or logical boundary crossings before any tile is read by a
	 * construction subsystem. */
	for (const PortalTerminalTile &part : layout.tiles) {
		if (!IsValidTile(part.tile) || !IsInnerTile(part.tile)) return std::nullopt;
		if (PlanetManager::GetTileWorld(part.tile) != world_id) return std::nullopt;
	}

	/* An exiting train reaches its signal only after its full 14-tile consist has
	 * cleared the portal head. The inbound signal sits at the near end of the
	 * parallel bay, so an entering consist waits entirely off the exit lane. */
	TileIndex exit_signal_tile = OffsetTile(gate_tile, layout.outward_dir, PORTAL_TERMINAL_HOLDING_LENGTH + 2, lateral_dir, 0);
	TileIndex entry_signal_tile = OffsetTile(gate_tile, layout.outward_dir, 3, lateral_dir, 1);
	layout.signals.push_back({exit_signal_tile, straight, layout.outward_dir});
	layout.signals.push_back({entry_signal_tile, straight, inward_dir});
	return layout;
}

void PortalTerminal::Build(const PortalTerminalLayout &layout, RailType railtype, Owner owner)
{
	Company *company = Company::GetIfValid(owner);

	for (const PortalTerminalTile &part : layout.tiles) {
		MakeRailNormal(part.tile, owner, part.tracks, railtype);
		if (company != nullptr) company->infrastructure.rail[railtype] += part.tracks.Count();
		MarkTileDirtyByTile(part.tile);
		for (Track track : part.tracks) {
			YapfNotifyTrackLayoutChange(part.tile, track);
		}
	}

	for (const PortalTerminalSignal &signal : layout.signals) {
		Trackdir trackdir = DiagDirToDiagTrackdir(signal.travel_dir);
		uint8_t present = SignalAlongTrackdir(trackdir);
		SetHasSignals(signal.tile, true);
		SetPresentSignals(signal.tile, present);
		SetSignalStates(signal.tile, 0xF & ~present);
		SetSignalType(signal.tile, signal.track, SignalType::PathOneWay);
		SetSignalVariant(signal.tile, signal.track, SignalVariant::Electric);
		if (company != nullptr) company->infrastructure.signal += 1;
		AddTrackToSignalBuffer(signal.tile, signal.track, owner);
		YapfNotifyTrackLayoutChange(signal.tile, signal.track);
		MarkTileDirtyByTile(signal.tile);
	}

	if (company != nullptr) DirtyCompanyInfrastructureWindows(owner);
}
