/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file planet_manager.h Central manager and spatial index for planetary world regions. */

#ifndef PLANET_MANAGER_H
#define PLANET_MANAGER_H

#include "planet_type.h"
#include "../tile_type.h"
#include "../command_type.h"
#include "../rail_type.h"
#include "../economy_type.h"
#include <vector>
#include <unordered_map>
#include <array>
#include <string>

struct Window;

/**
 * Spatial manager and query engine for planetary worlds.
 * Maintains registered planet regions, bounding boxes, and an accelerated spatial lookup table.
 */
class PlanetManager {
public:
	static constexpr uint32_t CELL_SHIFT = 6;                          ///< 64x64 tiles per spatial cell (2^6).
	static constexpr uint32_t MAX_CELLS_PER_AXIS = 4096 >> CELL_SHIFT; ///< 64 cells along X and Y for a 4096-wide map.
	static constexpr uint32_t TOTAL_CELLS = MAX_CELLS_PER_AXIS * MAX_CELLS_PER_AXIS;

	static constexpr WorldID MIXED_WORLD = WorldID{ (uint32_t)-2 };    ///< Spatial cell spans across multiple regions/boundaries.

	/** Reset and clear all registered planet regions and spatial acceleration structures. */
	static void Reset();

	/**
	 * Register a new planetary world region.
	 * @param region The planet configuration and bounding box.
	 * @return true if registered successfully; false if overlapping with an existing region or invalid.
	 */
	static bool RegisterRegion(const PlanetRegion &region);

	/**
	 * Retrieve a planet region by its unique WorldID.
	 * @param world The world identifier.
	 * @return Pointer to the PlanetRegion, or nullptr if not found.
	 */
	static const PlanetRegion *GetRegion(WorldID world);

	/**
	 * Retrieve the planet region containing the given tile.
	 * @param tile Tile to query.
	 * @return Pointer to the PlanetRegion containing the tile, or nullptr if in void/buffer space.
	 */
	static const PlanetRegion *GetRegionByTile(TileIndex tile);

	/**
	 * Retrieve the planet region containing coordinate (x, y).
	 * @param x Tile X coordinate.
	 * @param y Tile Y coordinate.
	 * @return Pointer to the PlanetRegion containing (x, y), or nullptr if in void/buffer space.
	 */
	static const PlanetRegion *GetRegionByCoord(uint32_t x, uint32_t y);

	/**
	 * Get the WorldID for a given tile in O(1) time.
	 * @param tile Tile to query.
	 * @return The WorldID of the tile, or INVALID_WORLD if in void/buffer space.
	 */
	static WorldID GetTileWorld(TileIndex tile);

	/**
	 * Get the WorldPhase for a given tile.
	 * @param tile Tile to query.
	 * @return The WorldPhase of the tile (defaults to Phase3_Frontier if invalid).
	 */
	static WorldPhase GetTilePhase(TileIndex tile);

	/**
	 * Validate the common base-tile requirements for world-aware construction.
	 * Commands remain responsible for asset-specific footprint and Phase rules.
	 * @param tile Proposed construction tile.
	 * @return Success inside a registered logical world, otherwise a useful error.
	 */
	static CommandCost CheckConstructionPlacement(TileIndex tile);

	/**
	 * Check if an industry is permitted on the world at the given tile.
	 * @param tile Tile location for the proposed industry.
	 * @param is_raw Whether the industry is an extractive or organic raw producer (e.g. Bio-Farm, Mine).
	 * @param is_processing Whether the industry is a processing facility (e.g. Factory, Refinery).
	 * @return Succeeded CommandCost if permitted; error CommandCost with explanation if restricted.
	 */
	static CommandCost CheckIndustryPlacement(TileIndex tile, bool is_raw, bool is_processing);

	/**
	 * Check if a rail depot of the specified railtype is permitted on the world at the given tile.
	 * @param tile Tile location for the proposed depot.
	 * @param railtype Rail type of the depot.
	 * @return Succeeded CommandCost if permitted; error CommandCost with explanation if restricted.
	 */
	static CommandCost CheckDepotPlacement(TileIndex tile, RailType railtype);

	/**
	 * Calculate the percentage bonus applied to an interplanetary cargo shipment.
	 * Returns 0 if intra-world or either tile is invalid/in void space.
	 * @param src_tile Tile where cargo originated.
	 * @param dest_tile Tile where cargo was delivered.
	 * @return Bonus percentage (e.g. 75 for +75%, 125 for +125%).
	 */
	static uint32_t GetInterplanetaryBonusPercent(TileIndex src_tile, TileIndex dest_tile);

	/**
	 * Calculate the interplanetary profit adjustment for cargo delivered between two tiles.
	 * If the cargo traveled between different planetary worlds, applies an interplanetary
	 * trade premium based on phase gradients and high-demand Core world markets.
	 *
	 * @param base_profit The base profit calculated from distance and transit time.
	 * @param src_tile Tile where the cargo was loaded.
	 * @param dest_tile Tile where the cargo was delivered.
	 * @return Adjusted profit incorporating interplanetary trade premiums.
	 */
	static Money GetInterplanetaryCargoProfit(Money base_profit, TileIndex src_tile, TileIndex dest_tile);

	/**
	 * Get a user-friendly display name for a WorldPhase.
	 * @param phase The WorldPhase.
	 * @return Human-readable phase name.
	 */
	static const char *GetWorldPhaseName(WorldPhase phase);

	/**
	 * Get a user-friendly display name for a WorldBiome.
	 * @param biome The WorldBiome.
	 * @return Human-readable biome name.
	 */
	static const char *GetWorldBiomeName(WorldBiome biome);

	/**
	 * Resolve the planet region currently centered in the main game viewport.
	 * @param main_window Optional pointer to the main window (defaults to FindWindowById(WindowClass::MainWindow, 0)).
	 * @return Pointer to current PlanetRegion, or nullptr if in void space or no planets registered.
	 */
	static const PlanetRegion *GetViewportCurrentPlanet(const Window *main_window = nullptr);

	/**
	 * Format a status badge string describing the planet currently centered in the viewport.
	 * @param main_window Optional pointer to the main window.
	 * @return Formatted status text, e.g. "[Core Hub (Phase 1)] Phase 1 (Core) | Temperate", or empty if no planets.
	 */
	static std::string GetViewportStatusText(const Window *main_window = nullptr);

	/**
	 * Jump the main viewport center to the geometric center of the specified planetary world.
	 * @param world_id The WorldID to jump to.
	 * @return True if jump succeeded, false if world not found.
	 */
	static bool JumpToPlanet(WorldID world_id);

	/** Get the total number of registered planet regions. */
	static size_t Count();

	/** Get all registered regions. */
	static const std::vector<PlanetRegion> &GetAllRegions();

	/** Rebuild the spatial grid acceleration structure from registered regions. */
	static void RebuildSpatialGrid();

private:
	static std::vector<PlanetRegion> regions;
	static std::unordered_map<uint32_t, size_t> id_to_region_index;
	static std::array<WorldID, TOTAL_CELLS> spatial_grid;

	static uint32_t CoordToCellIndex(uint32_t x, uint32_t y);
};

#endif /* PLANET_MANAGER_H */
