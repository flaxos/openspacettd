/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_cmd.h Federation inter-server transfer commands and coordinator. */

#ifndef FEDERATION_CMD_H
#define FEDERATION_CMD_H

#include "../command_type.h"
#include "../tile_type.h"
#include "../vehicle_type.h"
#include "portal_type.h"

struct Train;

/**
 * Command to manually or automatically dispatch a train consist across server
 * boundaries via an inter-server portal gate.
 *
 * @param flags Command execution flags.
 * @param vehicle_id The lead vehicle of the consist.
 * @param portal_tile The inter-server portal gate tile.
 * @return Command cost or failure.
 */
CommandCost CmdDispatchInterServerTransfer(DoCommandFlags flags, VehicleID vehicle_id, TileIndex portal_tile);

/**
 * Federation transfer coordinator managing local departure handoffs, arrival
 * polling, and clearance checks.
 */
class FederationTransferManager {
public:
	/**
	 * Initiate the departure handoff of a consist entering an inter-server portal gate.
	 *
	 * - Captures snapshot and despawns the physical train from local map.
	 * - Registers transfer with Universe Authority.
	 * - Transitions state to IN_TRANSIT.
	 *
	 * @param consist Front engine of the consist.
	 * @param portal_tile The portal gate tile the train entered.
	 * @return True if transfer was initiated and train despawned successfully.
	 */
	static bool InitiateConsistDeparture(Train *consist, TileIndex portal_tile);

	/**
	 * Process incoming in-transit transfers for a specific world server.
	 *
	 * - Queries Universe Authority for pending arrivals whose transit delay elapsed.
	 * - Checks exit throat clearance at destination gate.
	 * - Materializes train on clear throat and confirms delivery.
	 *
	 * @param local_world The world ID of this server.
	 * @param current_tick The current simulation tick.
	 * @return Number of successfully materialized consists.
	 */
	static size_t ProcessIncomingTransfers(WorldID local_world, uint64_t current_tick);

	/**
	 * Reset coordinator state (for tests and session initialization).
	 */
	static void Reset();

	/**
	 * Configure external Universe Authority base URL. If empty, uses in-memory service.
	 */
	static void SetAuthorityUrl(std::string url);
	static const std::string &GetAuthorityUrl();
	static bool HasExternalAuthority();

	/**
	 * Periodic simulation tick handler for background federation polling.
	 */
	static void OnGameTick(uint64_t current_tick);
};

#endif /* FEDERATION_CMD_H */
