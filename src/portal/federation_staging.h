/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file federation_staging.h Automatic holding loops and local staging sidings during federation latency/restarts. */

#ifndef FEDERATION_STAGING_H
#define FEDERATION_STAGING_H

#include "../tile_type.h"
#include "../vehicle_type.h"
#include "portal_type.h"
#include <string>
#include <vector>
#include <map>

struct Train;

/** Record of a train consist currently held in a local staging siding. */
struct StagingHoldRecord {
	VehicleID vehicle_id = VehicleID::Invalid();
	TileIndex portal_tile = INVALID_TILE;
	TileIndex siding_tile = INVALID_TILE;
	WorldID target_world = INVALID_WORLD;
	uint64_t hold_start_tick = 0;
	std::string hold_reason;
};

/**
 * Manager responsible for automatic holding loops and staging sidings when
 * remote world servers experience network latency, maintenance, or restarts.
 */
class FederationStagingManager {
public:
	/** Ping latency threshold (ms) beyond which inbound traffic is held in staging. */
	static constexpr uint32_t LATENCY_THRESHOLD_MS = 250;

	static void Reset();

	/**
	 * Check if a remote world is currently experiencing conditions that require holding:
	 * Unreachable, Maintenance/Restarting, Ping > 250ms, or Saturated corridor.
	 */
	static bool IsServerHoldingCondition(WorldID remote_world);

	/**
	 * Check whether a portal gate approach or throat is currently congested:
	 * vehicle on portal tile, tunnel reservation active, or downstream signal obstruction.
	 */
	static bool IsPortalApproachCongested(TileIndex portal_tile);

	/**
	 * Find the most suitable staging siding tile for a portal, either the explicitly
	 * configured siding or the closest designated StationFacility::HoldingSiding facility.
	 */
	static TileIndex FindStagingSidingForPortal(TileIndex portal_tile);

	/**
	 * Get a human-readable diagnostic description of why holding is active.
	 */
	static std::string GetHoldingReason(WorldID remote_world);

	/**
	 * Check if an inbound consist approaching or entering an inter-server portal
	 * needs to be diverted into the designated staging siding.
	 *
	 * If holding conditions are met, safely halts the train in the siding,
	 * frees mainline track reservations, and tracks the consist.
	 *
	 * @param consist Front engine of the inbound train.
	 * @param portal_tile Inter-server portal gate tile.
	 * @return True if consist was diverted or is holding; false if clear to traverse.
	 */
	static bool CheckAndDivertToStaging(Train *consist, TileIndex portal_tile);

	/**
	 * Release all trains held in staging for a given portal gate once the remote
	 * server health and latency have recovered.
	 *
	 * @param portal_tile Portal gate whose held trains should be released.
	 * @return Number of released consists.
	 */
	static size_t ReleaseHeldTrains(TileIndex portal_tile);

	/**
	 * Check all portal gates and release held trains whose remote servers have recovered.
	 * @return Number of released consists.
	 */
	static size_t ReleaseHeldTrains();

	/**
	 * Periodic simulation tick handler checking for remote server recovery and
	 * auto-releasing held trains.
	 */
	static void OnGameTick(uint64_t current_tick);

	/**
	 * Get records of trains currently held for a specific portal.
	 */
	static std::vector<StagingHoldRecord> GetHeldTrainsForPortal(TileIndex portal_tile);

	/**
	 * Get all currently held trains across all portal gates.
	 */
	static std::vector<StagingHoldRecord> GetAllHeldTrains();

	/**
	 * Total number of consists currently holding in staging sidings.
	 */
	static size_t GetTotalHeldTrainsCount();

	/**
	 * Test whether a specific train is currently held in staging.
	 */
	static bool IsTrainHeld(VehicleID vehicle_id);
};

#endif /* FEDERATION_STAGING_H */
