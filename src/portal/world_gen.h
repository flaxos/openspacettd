/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file world_gen.h Multi-world procedural map generation, spatial partitioning, and gateway initialization. */

#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "planet_type.h"
#include "../tile_type.h"
#include <vector>
#include <string>

/**
 * Procedural generator for multi-world planetary layouts.
 * Partitions the contiguous coordinate map into distinct planetary regions
 * separated by void buffer bands, and initializes interplanetary gateways.
 */
class MultiWorldGen {
public:
	/**
	 * Configuration parameters for generating multi-world layouts.
	 */
	struct Config {
		uint32_t world_count = 3;       ///< Number of distinct planetary worlds to generate (default 3).
		uint32_t buffer_width = 0;      ///< Width of void buffer bands in tiles (0 = automatic based on map size).
		bool place_gateways = true;     ///< Whether to place and link starting interplanetary gateways.
		uint32_t border_padding = 2;    ///< Outer void border padding in tiles.
	};

	/** Check whether multi-world generation mode is active. */
	static bool IsEnabled();

	/** Enable or disable multi-world generation mode. */
	static void SetEnabled(bool enabled);

	/** Get the default world count for multi-world generation. */
	static uint32_t GetDefaultWorldCount();

	/** Set the default world count for multi-world generation (1-16). */
	static void SetDefaultWorldCount(uint32_t count);

	/**
	 * Generate a multi-world planetary layout on the currently allocated map.
	 * - Calculates non-overlapping bounding boxes for world_count planetary regions.
	 * - Sets all tiles outside the world regions (buffer bands and borders) to TileType::Void via MakeVoid().
	 * - Registers each PlanetRegion in PlanetManager with its phase, biome, and boundaries.
	 * - Pre-places and pairs starting gateway portal structures in PortalRegistry linking the worlds.
	 *
	 * @param size_x Map width in tiles.
	 * @param size_y Map height in tiles.
	 * @param config Optional configuration settings.
	 * @return true if generation succeeded; false if map is too small or invalid.
	 */
	static bool GenerateMultiWorldLayout(uint32_t size_x, uint32_t size_y);
	static bool GenerateMultiWorldLayout(uint32_t size_x, uint32_t size_y, const Config &config);

	static std::vector<PlanetRegion> CalculateLayout(uint32_t size_x, uint32_t size_y);
	static std::vector<PlanetRegion> CalculateLayout(uint32_t size_x, uint32_t size_y, const Config &config);

	/**
	 * Apply alien biome stylization to a world region:
	 * transforms ground, rocks, snow/permafrost, and foliage according to the region's WorldBiome.
	 * @param region The PlanetRegion to style.
	 */
	static void ApplyBiomeStyling(const PlanetRegion &region);

private:
	static bool enabled;
	static uint32_t default_world_count;
};

#endif /* WORLD_GEN_H */
