/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint.h Player rail blueprint data model, serialization, and transformations. */

#ifndef BLUEPRINT_H
#define BLUEPRINT_H

#include "../rail_type.h"
#include "../track_type.h"
#include "../signal_type.h"
#include "../direction_type.h"
#include "../station_type.h"
#include "../newgrf_station.h"

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

/** Category of infrastructure component in a blueprint tile. */
enum class BlueprintTileType : uint8_t {
	Track = 0,   ///< Plain railroad track bits and signals.
	Depot = 1,   ///< Train depot with facing direction.
	Station = 2, ///< Rail station platform segment.
};

/** Saved signal configuration for a specific track on a blueprint tile. */
struct BlueprintSignal {
	Track track = Track::X;                        ///< Track possessing the signal.
	SignalType sigtype = SignalType::Block;        ///< Signal type (block, pre-signal, path, one-way path).
	SignalVariant sigvar = SignalVariant::Electric;///< Signal visual variant (electric or semaphore).
	uint8_t signals_copy = 0;                      ///< Signal presence and orientation bitmask.
};

/** A single tile entry in a blueprint template. */
struct BlueprintTile {
	int16_t dx = 0;                                ///< X offset relative to blueprint origin (0 .. width - 1).
	int16_t dy = 0;                                ///< Y offset relative to blueprint origin (0 .. height - 1).
	BlueprintTileType type = BlueprintTileType::Track;
	RailType railtype = RAILTYPE_BEGIN;            ///< Rail type of this infrastructure element.

	/* Track attributes */
	TrackBits trackbits{};                         ///< Track layout bits on this tile.
	std::vector<BlueprintSignal> signals{};        ///< Signals attached to tracks on this tile.

	/* Depot attributes */
	DiagDirection dir = DiagDirection::NE;         ///< Facing direction of depot entrance.

	/* Station attributes */
	Axis axis = Axis::X;                           ///< Alignment axis of station platform.
	StationClassID spec_class = STAT_CLASS_DFLT;   ///< Station class ID.
	uint16_t spec_index = 0;                       ///< Station specification index.
};

/** A self-contained rail blueprint template. */
struct Blueprint {
	std::string name = "New Blueprint";            ///< Player-assigned or prefab name.
	std::string description = "";                  ///< Optional description or operating notes.
	std::string author = "";                       ///< Creator name or organization.
	uint32_t version = 1;                          ///< Blueprint format version.
	uint16_t width = 0;                            ///< Footprint size along X axis.
	uint16_t height = 0;                           ///< Footprint size along Y axis.
	bool is_builtin = false;                       ///< True if this is a factory prefab (read-only).
	int64_t created_time = 0;                      ///< Creation timestamp.
	std::vector<BlueprintTile> tiles{};            ///< Sparse list of non-empty infrastructure tiles.

	/** Validate consistency of blueprint dimensions and tile data. */
	bool IsValid() const;

	/** Count distinct tiles occupied by the blueprint. */
	size_t GetTileCount() const { return this->tiles.size(); }

	/** Count total individual track pieces across all tiles. */
	size_t GetTrackPieceCount() const;

	/** Count total signals across all tracks. */
	size_t GetSignalCount() const;

	/** Count total rail station platform tiles. */
	size_t GetStationCount() const;

	/** Count total rail depots. */
	size_t GetDepotCount() const;

	/** Serialize blueprint to a JSON string representation. */
	std::string ToJson() const;

	/** Deserialize a blueprint from a JSON string representation. */
	static std::optional<Blueprint> FromJson(const std::string &json_str);

	/** Rotate the blueprint clockwise by 90-degree steps (1 = 90 deg, 2 = 180 deg, 3 = 270 deg). */
	Blueprint Rotate(int steps_90_cw = 1) const;

	/** Mirror the blueprint horizontally across the isometric screen centerline. */
	Blueprint Mirror() const;
};

/** Helper to rotate a Track enum 90 degrees clockwise. */
Track RotateTrack90CW(Track track);

/** Helper to rotate a Trackdir enum 90 degrees clockwise. */
Trackdir RotateTrackdir90CW(Trackdir td);

/** Helper to mirror a Track enum across screen vertical axis. */
Track MirrorTrack(Track track);

/** Helper to mirror a Trackdir enum across screen vertical axis. */
Trackdir MirrorTrackdir(Trackdir td);

#endif /* BLUEPRINT_H */
