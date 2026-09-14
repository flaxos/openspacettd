/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_registry.h Registry and resolution for wormhole portal gates. */

#ifndef PORTAL_REGISTRY_H
#define PORTAL_REGISTRY_H

#include "portal_type.h"
#include "../vehicle_type.h"
#include "../company_type.h"
#include "../economy_type.h"
#include <vector>
#include <unordered_map>
#include <optional>

/**
 * Registry managing paired wormhole portals connecting logical world regions.
 */
class PortalRegistry {
public:
	/**
	 * Register a bidirectional or directed portal link between two tiles.
	 *
	 * @param tile_a Entrance tile A.
	 * @param dir_a Vehicle entry direction into portal head A.
	 * @param world_a Logical world ID containing tile A.
	 * @param tile_b Entrance tile B.
	 * @param dir_b Vehicle entry direction into portal head B.
	 * @param world_b Logical world ID containing tile B.
	 * @param virtual_length Virtual route length in tiles for YAPF cost.
	 * @param bidirectional Whether vehicles can travel in both directions.
	 * @return The assigned PortalID, or INVALID_PORTAL on failure (e.g. duplicate tile).
	 */
	static PortalID RegisterPortalPair(
		TileIndex tile_a, DiagDirection dir_a, WorldID world_a,
		TileIndex tile_b, DiagDirection dir_b, WorldID world_b,
		uint32_t virtual_length = 1, bool bidirectional = true
	);

	/**
	 * Unregister a portal link by ID.
	 * @param id The portal ID to remove.
	 * @return True if found and removed.
	 */
	static bool UnregisterPortal(PortalID id);

	/**
	 * Unregister a portal gate by its tile (handles both linked portal links and unlinked gates).
	 * If part of a linked portal, removes the link for both ends.
	 * @param tile The portal tile to unregister.
	 * @return True if found and removed.
	 */
	static bool UnregisterPortalByTile(TileIndex tile);

	/**
	 * Register an unlinked portal gate head on a tile.
	 *
	 * @param tile Entrance tile.
	 * @param dir Vehicle entry direction into portal head.
	 * @param world_id Logical world ID containing tile.
	 * @return True if registered successfully, false if duplicate or invalid.
	 */
	static bool RegisterUnlinkedGate(TileIndex tile, DiagDirection dir, WorldID world_id);

	/**
	 * Check whether a given tile is a registered unlinked portal gate head.
	 * @param tile The tile to query.
	 * @return True if the tile is an unlinked portal gate.
	 */
	static bool IsUnlinkedGate(TileIndex tile);

	/**
	 * Get the unlinked portal endpoint for a tile.
	 * @param tile The tile to query.
	 * @return Pointer to PortalEndpoint or nullptr if not an unlinked gate.
	 */
	static const PortalEndpoint *GetUnlinkedGate(TileIndex tile);

	/**
	 * Get all currently unlinked portal gates.
	 */
	static const std::unordered_map<TileIndex, PortalEndpoint> &GetUnlinkedGates();

	/**
	 * Link two unlinked portal gates into an active bidirectional or directed wormhole link.
	 *
	 * @param tile_a Entrance tile A.
	 * @param tile_b Entrance tile B.
	 * @param virtual_length Virtual route length in tiles for YAPF cost.
	 * @param bidirectional Whether vehicles can travel in both directions.
	 * @return The assigned PortalID, or INVALID_PORTAL on failure.
	 */
	static PortalID LinkGates(TileIndex tile_a, TileIndex tile_b, uint32_t virtual_length = 1, bool bidirectional = true);

	/**
	 * Check whether any vehicle is currently in transit through the portal wormhole connecting to this tile.
	 * @param tile Portal endpoint tile to check.
	 * @return True if at least one vehicle is in transit through the portal link.
	 */
	static bool IsPortalInTransit(TileIndex tile);

	/**
	 * Check whether a given tile is a registered portal gate.
	 * @param tile The tile to query.
	 * @return True if the tile is registered as a portal endpoint.
	 */
	static bool IsPortalTile(TileIndex tile);

	/**
	 * Resolve the opposite portal endpoint tile.
	 * @param tile The source portal tile.
	 * @return The destination portal tile, or INVALID_TILE if not a registered portal.
	 */
	static TileIndex GetOtherPortalEnd(TileIndex tile);

	/**
	 * Calculate the vehicle emergence position and direction at the exit portal.
	 * @param entry_tile The source portal tile the vehicle entered.
	 * @return PortalExitPosition containing tile, x, y, z, direction, and track.
	 */
	static PortalExitPosition GetPortalExitPosition(TileIndex entry_tile);

	/**
	 * Get the virtual route length (in tiles) for the portal containing the given tile.
	 * @param tile The portal tile.
	 * @return Virtual length (defaults to 1 if not a portal).
	 */
	static uint32_t GetPortalVirtualLength(TileIndex tile);

	/**
	 * Get the full PortalLink definition for a portal tile.
	 * @param tile The tile to query.
	 * @return Pointer to PortalLink or nullptr.
	 */
	static const PortalLink *GetPortalLink(TileIndex tile);

	/**
	 * Get the full PortalLink definition by PortalID.
	 * @param id The portal ID.
	 * @return Pointer to PortalLink or nullptr.
	 */
	static const PortalLink *GetPortalLinkByID(PortalID id);

	/**
	 * Total number of active portal links.
	 */
	static size_t Count();

	/**
	 * Advance the transit progress counter for a vehicle traveling through a portal wormhole.
	 * @param veh_id The vehicle ID.
	 * @return The updated progress distance in movement units.
	 */
	static uint32_t AdvancePortalTransit(VehicleID veh_id);

	/**
	 * Get the current transit progress counter for a vehicle.
	 * @param veh_id The vehicle ID.
	 * @return Progress distance in movement units (or 0 if not tracked).
	 */
	static uint32_t GetPortalTransitProgress(VehicleID veh_id);

	/**
	 * Clear the transit progress counter for a vehicle upon emergence or deletion.
	 * @param veh_id The vehicle ID.
	 */
	static void ClearPortalTransit(VehicleID veh_id);

	/**
	 * Get all registered portal links.
	 */
	static const std::unordered_map<uint32_t, PortalLink> &GetAllPortals();

	/**
	 * Get all vehicles currently in portal transit.
	 */
	static const std::unordered_map<uint32_t, uint32_t> &GetAllVehicleTransit();

	/**
	 * Directly restore a portal link (for savegame loading).
	 * @param link The portal link to restore.
	 * @return True if restored successfully.
	 */
	static bool RestorePortalLink(const PortalLink &link);

	/**
	 * Set the transit progress for a vehicle (for savegame loading).
	 * @param veh_id The vehicle ID.
	 * @param progress The transit progress distance.
	 */
	static void SetVehicleTransitProgress(VehicleID veh_id, uint32_t progress);

	/**
	 * Register an inter-server portal link connecting a local portal head to a remote world server.
	 */
	static PortalID RegisterInterServerPortal(
		TileIndex local_tile,
		DiagDirection dir,
		WorldID local_world,
		WorldID remote_world,
		uint32_t remote_gate_id,
		uint32_t virtual_length = 1,
		uint32_t local_gate_id = 0
	);

	/**
	 * Restore an existing inter-server portal link (e.g. during savegame load).
	 */
	static bool RestoreInterServerPortal(const InterServerPortalLink &link);

	/**
	 * Check whether a given tile is a registered inter-server portal gate.
	 */
	static bool IsInterServerPortal(TileIndex tile);

	/**
	 * Get the InterServerPortalLink for a local portal tile.
	 */
	static const InterServerPortalLink *GetInterServerPortal(TileIndex tile);

	/**
	 * Unregister an inter-server portal gate by local tile.
	 */
	static bool UnregisterInterServerPortal(TileIndex tile);

	/**
	 * Get all registered inter-server portals.
	 */
	static const std::unordered_map<TileIndex, InterServerPortalLink> &GetAllInterServerPortals();

	/**
	 * Repair generated neutral gateway heads from early multi-world saves where
	 * the stored entry direction pointed away from the world-side lead track.
	 * This operation is deterministic and idempotent.
	 * @return Number of endpoint directions repaired.
	 */
	static size_t RepairLegacyGeneratedGateways();

	/**
	 * Compute monthly maintenance and excitation power upkeep for all active portal gates owned by a company.
	 * @param owner Company to evaluate.
	 * @return Upkeep cost in currency.
	 */
	static Money GetCompanyPortalMaintenanceCost(Owner owner);

	/**
	 * Test if two portal gate heads form a parallel twin gateway array (1-tile separation, parallel orientation).
	 * @param tile_a First gate tile.
	 * @param tile_b Second gate tile.
	 * @return True if gates form a coordinated twin array.
	 */
	static bool IsTwinGateway(TileIndex tile_a, TileIndex tile_b);

	/**
	 * Resolve the parallel twin gate head adjacent to a given portal gate, if one exists.
	 * @param tile Base gate tile.
	 * @return Tile of the twin gate, or INVALID_TILE if none.
	 */
	static TileIndex GetTwinGate(TileIndex tile);

	/**
	 * Resolve the world tile corresponding to a gateway identifier or tile index.
	 * @param gate_id Gateway identifier or direct tile.
	 * @param world_id Optional world filter.
	 * @return Tile of the gate head, or INVALID_TILE if unresolvable.
	 */
	static TileIndex ResolveGateTile(uint32_t gate_id, WorldID world_id = INVALID_WORLD);

	/**
	 * Clear all registered portals (for test isolation and new game setup).
	 */
	static void Reset();

private:
	static std::unordered_map<TileIndex, PortalID> tile_to_portal;
	static std::unordered_map<uint32_t, PortalLink> portal_links;
	static std::unordered_map<TileIndex, PortalEndpoint> unlinked_gates;
	static std::unordered_map<TileIndex, InterServerPortalLink> interserver_portals;
	static std::unordered_map<uint32_t, uint32_t> vehicle_portal_progress;
	static uint32_t next_portal_id;
};

#endif /* PORTAL_REGISTRY_H */
