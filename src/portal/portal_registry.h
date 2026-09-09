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
	 * @param virtual_length Number of virtual tiles/ticks for transit duration and YAPF cost.
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
	 * Get the virtual traversal length (in tiles) for the portal containing the given tile.
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
	 * Clear all registered portals (for test isolation and new game setup).
	 */
	static void Reset();

private:
	static std::unordered_map<TileIndex, PortalID> tile_to_portal;
	static std::unordered_map<uint32_t, PortalLink> portal_links;
	static std::unordered_map<uint32_t, uint32_t> vehicle_portal_progress;
	static uint32_t next_portal_id;
};

#endif /* PORTAL_REGISTRY_H */
