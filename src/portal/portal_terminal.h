/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file portal_terminal.h Deterministic high-capacity rail throats for Portal Gates. */

#ifndef PORTAL_TERMINAL_H
#define PORTAL_TERMINAL_H

#include "planet_type.h"
#include "../company_type.h"
#include "../direction_type.h"
#include "../rail_type.h"
#include "../tile_type.h"
#include "../track_type.h"

#include <optional>
#include <vector>

/**
 * Clear train length provided by each portal holding lane.
 *
 * Fourteen tiles is twice the traditional seven-tile maximum station platform,
 * leaving room for very long locomotives and wagon consists without fouling the
 * shared portal head.
 */
static constexpr uint PORTAL_TERMINAL_HOLDING_LENGTH = 14;

/** Longitudinal distance from the gate head to the player connection tile. */
static constexpr uint PORTAL_TERMINAL_APPROACH_LENGTH = PORTAL_TERMINAL_HOLDING_LENGTH + 4;

/** Tracks to construct on one tile of a portal terminal. */
struct PortalTerminalTile {
	TileIndex tile{INVALID_TILE};
	TrackBits tracks{};
};

/** One directional path signal controlling a portal terminal lane. */
struct PortalTerminalSignal {
	TileIndex tile{INVALID_TILE};
	Track track{Track::Invalid};
	DiagDirection travel_dir{DiagDirection::Invalid};
};

/** Complete, orientation-independent two-lane terminal plan. */
struct PortalTerminalLayout {
	TileIndex gate_tile{INVALID_TILE};
	TileIndex connection_tile{INVALID_TILE};
	WorldID world_id{INVALID_WORLD};
	DiagDirection gate_dir{DiagDirection::Invalid};
	DiagDirection outward_dir{DiagDirection::Invalid};
	std::vector<PortalTerminalTile> tiles;
	std::vector<PortalTerminalSignal> signals;

	/** Return the requested track bits for a footprint tile. */
	TrackBits GetTracks(TileIndex tile) const;

	/** Number of individual rail pieces in the plan, including switch pieces. */
	uint GetTrackPieceCount() const;
};

class PortalTerminal {
public:
	/**
	 * Plan a two-lane portal throat behind a gate head.
	 *
	 * The main lane carries exiting trains away from the gate. A parallel lane
	 * holds entering trains behind a one-way path signal. Both lanes have at
	 * least PORTAL_TERMINAL_HOLDING_LENGTH clear tiles and merge through native
	 * OpenTTD rail switches.
	 */
	static std::optional<PortalTerminalLayout> Plan(TileIndex gate_tile, DiagDirection gate_dir, WorldID world_id);

	/**
	 * Materialise a validated layout using ordinary OpenTTD rail and PBS state.
	 * All footprint tiles must already be clear and level.
	 */
	static void Build(const PortalTerminalLayout &layout, RailType railtype, Owner owner);

	/** UAT fixture repair only: validate or adopt a complete neutral terminal without rebuilding it.
	 * Refuses foreign ownership or missing rails. Caller must refresh company infrastructure totals. */
	static bool AdoptForUAT(const PortalTerminalLayout &layout, CompanyID company, bool execute);
};

#endif /* PORTAL_TERMINAL_H */
