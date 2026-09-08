/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file planet_type.h Types and definitions for planetary world regions in OpenSpaceTTD. */

#ifndef PLANET_TYPE_H
#define PLANET_TYPE_H

#include "portal_type.h"
#include <string>
#include <cstdint>

/** Developmental phase / tier of a planetary world. */
enum class WorldPhase : uint8_t {
	Phase1_Core       = 1, ///< Highly developed capital hub; high consumption of advanced goods, no bio-farms/raw mining.
	Phase2_Developed  = 2, ///< Maturing industrialized world; secondary manufacturing, intermediate processing.
	Phase3_Frontier   = 3, ///< Emerging frontier world; raw resource extraction, bio-farms, mining, small colonies.
	Phase4_Expansion  = 4, ///< Uncharted wilderness / expansion worlds; untapped potential, no initial infrastructure.
};

/** Biome / climate classification for a planetary world. */
enum class WorldBiome : uint8_t {
	Temperate  = 0, ///< Earth-like, balanced agriculture and industry.
	SubArctic  = 1, ///< Tundra / permafrost, high heating and energy needs.
	SubTropic  = 2, ///< Arid / tropical jungle, specialized agricultural products.
	AridDesert = 3, ///< Water-scarce, mineral-rich desert planet.
	Volcanic   = 4, ///< Heavy thermal activity, rare minerals, harsh environment.
	Oceanic    = 5, ///< Archipelago / deep ocean world, aquafarming and off-shore rigs.
};

/** Definition of a distinct planetary world region within the global coordinate space. */
struct PlanetRegion {
	WorldID id = INVALID_WORLD;          ///< Unique identifier of the world.
	std::string name;                   ///< Display name of the planet/colony.
	WorldPhase phase = WorldPhase::Phase3_Frontier; ///< Development phase / tier.
	WorldBiome biome = WorldBiome::Temperate;       ///< Biome / ecological classification.

	/* Inclusive bounding coordinates in tile space */
	uint32_t min_x = 0;
	uint32_t min_y = 0;
	uint32_t max_x = 0;
	uint32_t max_y = 0;

	uint32_t development_score = 0;     ///< Score measuring infrastructure and economy progress.

	/** Check if coordinate (x, y) falls within this planet's boundaries. */
	constexpr bool ContainsCoord(uint32_t x, uint32_t y) const noexcept
	{
		return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
	}

	/** Total width of this world in tiles. */
	constexpr uint32_t Width() const noexcept
	{
		return max_x >= min_x ? (max_x - min_x + 1) : 0;
	}

	/** Total height of this world in tiles. */
	constexpr uint32_t Height() const noexcept
	{
		return max_y >= min_y ? (max_y - min_y + 1) : 0;
	}
};

#endif /* PLANET_TYPE_H */
