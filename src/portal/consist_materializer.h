/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file consist_materializer.h Engine functions for clean consist despawn and materialization. */

#ifndef CONSIST_MATERIALIZER_H
#define CONSIST_MATERIALIZER_H

#include "consist_snapshot.h"
#include "../direction_type.h"
#include "../tile_type.h"

struct Train;

/** Result of a consist despawn operation. */
struct ConsistDespawnResult {
	bool success = false;
	ConsistSnapshotBytes snapshot_bytes{};
	ConsistSnapshot snapshot{};
	uint32_t total_cargo = 0;
	std::string error_message;
};

/** Result of a consist materialization operation. */
struct ConsistMaterializeResult {
	bool success = false;
	Train *consist = nullptr;
	uint32_t total_cargo = 0;
	std::string error_message;
};

/**
 * Engine subsystem responsible for cleanly despawning train consists for
 * inter-server federation transfer, and materializing incoming consists at
 * destination portal throats.
 */
class ConsistMaterializer {
public:
	/**
	 * Cleanly capture and despawn a train consist from the local simulation.
	 *
	 * - Captures snapshot via ConsistSnapshotCodec using current content manifest.
	 * - Encodes snapshot to canonical v2 wire format.
	 * - Clears portal transit tracking and frees all track reservations.
	 * - Deletes the train vehicle chain without triggering crashes or news alerts.
	 *
	 * @param consist Front engine or any unit in the consist to despawn.
	 * @param owner_token Authorization token of the owning company.
	 * @return ConsistDespawnResult with snapshot payload and status.
	 */
	static ConsistDespawnResult DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token);

	/**
	 * Check whether the receiving portal throat is clear for consist emergence.
	 *
	 * @param exit_tile Exit portal gate head tile.
	 * @param enter_dir Direction of travel into the portal head.
	 * @return True if throat is clear of reservations and vehicles.
	 */
	static bool CheckThroatClearance(TileIndex exit_tile, DiagDirection enter_dir);

	/**
	 * Materialize an incoming train consist from a validated snapshot at a portal gate throat.
	 *
	 * - Validates content manifest against local content.
	 * - Verifies exit throat clearance.
	 * - Allocates vehicles and cargo packets.
	 * - Reconstructs consist topology, engine types, cargo provenance, and dynamics.
	 * - Binds persistent GlobalConsistID to the newly allocated anchor.
	 * - Acquires track reservation at exit throat.
	 *
	 * @param snapshot Parsed consist snapshot.
	 * @param exit_tile Exit portal gate head tile.
	 * @param enter_dir Direction of travel into the portal head.
	 * @return ConsistMaterializeResult with pointer to front engine or failure reason.
	 */
	static ConsistMaterializeResult MaterializeFromTransfer(
		const ConsistSnapshot &snapshot,
		TileIndex exit_tile,
		DiagDirection enter_dir
	);
};

#endif /* CONSIST_MATERIALIZER_H */
