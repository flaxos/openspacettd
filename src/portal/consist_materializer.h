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
#include <functional>

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
	 * Capture a train consist snapshot without changing the physical train.
	 *
	 * The returned snapshot is suitable for authority admission and journal
	 * prepare/bind work. Call ReleaseCapturedConsist() only after custody has
	 * moved to the authority.
	 */
	static ConsistDespawnResult CaptureForTransfer(Train *consist, const GlobalOwnerToken &owner_token);

	/**
	 * Release a previously captured consist from the local simulation.
	 *
	 * This clears portal transit tracking, frees reservations, releases the
	 * local identity anchor, and deletes the vehicle chain.
	 */
	static bool ReleaseCapturedConsist(Train *consist);

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
	 * @param admit Optional synchronous admission callback, invoked after encoding
	 *              but before any vehicle or reservation is removed. Must not mutate
	 *              the consist. Rejection leaves the physical train intact.
	 * @return ConsistDespawnResult with snapshot payload and status.
	 */
	static ConsistDespawnResult DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token,
		const std::function<bool(const ConsistSnapshot &, const ConsistSnapshotBytes &)> &admit);
	static ConsistDespawnResult DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token,
		const std::function<bool(const ConsistSnapshotBytes &)> &admit = {});

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

	/**
	 * Configure a 2-point cross-world round-trip schedule on a consist.
	 *
	 * @param consist Front engine of the consist.
	 * @param origin_st Station on origin world.
	 * @param origin_world Logical world containing origin station.
	 * @param dest_st Station on destination world.
	 * @param dest_world Logical world containing destination station.
	 * @return True if schedule successfully assigned.
	 */
	static bool AssignRoundTripOrders(Train *consist, StationID origin_st, WorldID origin_world, StationID dest_st, WorldID dest_world);
};

#endif /* CONSIST_MATERIALIZER_H */
