/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file planet_manager.cpp Implementation of spatial planet region manager and query index. */

#include "../stdafx.h"
#include "planet_manager.h"
#include "../map_func.h"
#include "../table/strings.h"
#include <cmath>
#include <cstdlib>

std::vector<PlanetRegion> PlanetManager::regions;
std::unordered_map<uint32_t, size_t> PlanetManager::id_to_region_index;
std::array<WorldID, PlanetManager::TOTAL_CELLS> PlanetManager::spatial_grid;

void PlanetManager::Reset()
{
	regions.clear();
	id_to_region_index.clear();
	spatial_grid.fill(INVALID_WORLD);
}

uint32_t PlanetManager::CoordToCellIndex(uint32_t x, uint32_t y)
{
	uint32_t cell_x = (x >> CELL_SHIFT);
	uint32_t cell_y = (y >> CELL_SHIFT);
	if (cell_x >= MAX_CELLS_PER_AXIS || cell_y >= MAX_CELLS_PER_AXIS) return (uint32_t)-1;
	return cell_y * MAX_CELLS_PER_AXIS + cell_x;
}

void PlanetManager::RebuildSpatialGrid()
{
	spatial_grid.fill(INVALID_WORLD);

	for (uint32_t cy = 0; cy < MAX_CELLS_PER_AXIS; ++cy) {
		uint32_t c_min_y = cy << CELL_SHIFT;
		uint32_t c_max_y = c_min_y + (1 << CELL_SHIFT) - 1;

		for (uint32_t cx = 0; cx < MAX_CELLS_PER_AXIS; ++cx) {
			uint32_t c_min_x = cx << CELL_SHIFT;
			uint32_t c_max_x = c_min_x + (1 << CELL_SHIFT) - 1;

			uint32_t idx = cy * MAX_CELLS_PER_AXIS + cx;

			for (const auto &r : regions) {
				/* Check if cell and region overlap */
				bool overlaps = (c_min_x <= r.max_x && c_max_x >= r.min_x &&
				                 c_min_y <= r.max_y && c_max_y >= r.min_y);
				if (!overlaps) continue;

				/* Check if cell is completely inside the region */
				bool fully_contained = (c_min_x >= r.min_x && c_max_x <= r.max_x &&
				                        c_min_y >= r.min_y && c_max_y <= r.max_y);

				if (fully_contained) {
					spatial_grid[idx] = r.id;
				} else {
					spatial_grid[idx] = MIXED_WORLD;
				}
				break;
			}
		}
	}
}

bool PlanetManager::RegisterRegion(const PlanetRegion &region)
{
	if (region.id == INVALID_WORLD || region.id == MIXED_WORLD) return false;
	if (region.min_x > region.max_x || region.min_y > region.max_y) return false;
	if (id_to_region_index.contains(region.id.base())) return false;

	/* Verify no overlap with existing registered regions */
	for (const auto &existing : regions) {
		bool overlaps = (region.min_x <= existing.max_x && region.max_x >= existing.min_x &&
		                 region.min_y <= existing.max_y && region.max_y >= existing.min_y);
		if (overlaps) return false;
	}

	size_t new_idx = regions.size();
	regions.push_back(region);
	id_to_region_index[region.id.base()] = new_idx;

	RebuildSpatialGrid();
	return true;
}

const PlanetRegion *PlanetManager::GetRegion(WorldID world)
{
	auto it = id_to_region_index.find(world.base());
	if (it == id_to_region_index.end()) return nullptr;
	return &regions[it->second];
}

const PlanetRegion *PlanetManager::GetRegionByCoord(uint32_t x, uint32_t y)
{
	uint32_t cell_idx = CoordToCellIndex(x, y);
	if (cell_idx != (uint32_t)-1) {
		WorldID wid = spatial_grid[cell_idx];
		if (wid == INVALID_WORLD) return nullptr;
		if (wid != MIXED_WORLD) {
			return GetRegion(wid);
		}
	}

	/* Cell is on a boundary or outside max axis; fall back to exact bounding box check */
	for (const auto &r : regions) {
		if (r.ContainsCoord(x, y)) return &r;
	}
	return nullptr;
}

const PlanetRegion *PlanetManager::GetRegionByTile(TileIndex tile)
{
	if (tile >= Map::Size()) return nullptr;
	return GetRegionByCoord(TileX(tile), TileY(tile));
}

WorldID PlanetManager::GetTileWorld(TileIndex tile)
{
	const PlanetRegion *r = GetRegionByTile(tile);
	return r != nullptr ? r->id : INVALID_WORLD;
}

WorldPhase PlanetManager::GetTilePhase(TileIndex tile)
{
	const PlanetRegion *r = GetRegionByTile(tile);
	return r != nullptr ? r->phase : WorldPhase::Phase3_Frontier;
}

size_t PlanetManager::Count()
{
	return regions.size();
}

const std::vector<PlanetRegion> &PlanetManager::GetAllRegions()
{
	return regions;
}

CommandCost PlanetManager::CheckIndustryPlacement(TileIndex tile, bool is_raw, bool is_processing)
{
	if (Count() == 0) return CommandCost();

	WorldID world = GetTileWorld(tile);
	if (world == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	WorldPhase phase = GetTilePhase(tile);
	if (phase == WorldPhase::Phase1_Core && is_raw) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_CORE_WORLD);
	}
	if (phase == WorldPhase::Phase3_Frontier && is_processing) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);
	}

	return CommandCost();
}

CommandCost PlanetManager::CheckDepotPlacement(TileIndex tile, RailType railtype)
{
	if (Count() == 0) return CommandCost();

	WorldID world = GetTileWorld(tile);
	if (world == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	WorldPhase phase = GetTilePhase(tile);
	/* Tier 3 Vac-Train/Maglev depots cannot be constructed on Phase 3 Frontier worlds */
	if (phase == WorldPhase::Phase3_Frontier && railtype == RAILTYPE_MAGLEV) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);
	}

	return CommandCost();
}

uint32_t PlanetManager::GetInterplanetaryBonusPercent(TileIndex src_tile, TileIndex dest_tile)
{
	if (Count() == 0) return 0;
	if (src_tile == INVALID_TILE || dest_tile == INVALID_TILE) return 0;

	WorldID src_world = GetTileWorld(src_tile);
	WorldID dest_world = GetTileWorld(dest_tile);

	if (src_world == INVALID_WORLD || dest_world == INVALID_WORLD) return 0;
	if (src_world == dest_world) return 0;

	/* Base interplanetary trade premium: +50% */
	uint32_t bonus = 50;

	/* Phase tier difference gradient bonus: +25% per phase tier step */
	WorldPhase src_phase = GetTilePhase(src_tile);
	WorldPhase dest_phase = GetTilePhase(dest_tile);
	int phase_diff = std::abs(static_cast<int>(src_phase) - static_cast<int>(dest_phase));
	bonus += static_cast<uint32_t>(phase_diff * 25);

	/* High-demand Core world market bonus: +25% if delivered into Phase 1 Core world */
	if (dest_phase == WorldPhase::Phase1_Core) {
		bonus += 25;
	}

	return bonus;
}

Money PlanetManager::GetInterplanetaryCargoProfit(Money base_profit, TileIndex src_tile, TileIndex dest_tile)
{
	if (base_profit <= 0) return base_profit;

	uint32_t bonus_pct = GetInterplanetaryBonusPercent(src_tile, dest_tile);
	if (bonus_pct == 0) return base_profit;

	return base_profit + (base_profit * bonus_pct) / 100;
}
