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
#include "../window_func.h"
#include "../window_gui.h"
#include "../viewport_func.h"
#include "../viewport_type.h"
#include "../landscape.h"
#include "../town.h"
#include "../industry.h"
#include "../3rdparty/fmt/format.h"
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
	if (region.phase != WorldPhase::Phase1_Core && region.phase != WorldPhase::Phase2_Developed &&
			region.phase != WorldPhase::Phase3_Frontier && region.phase != WorldPhase::Phase4_Expansion) return false;
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

bool PlanetManager::SetWorldPhase(WorldID world, WorldPhase phase)
{
	auto it = id_to_region_index.find(world.base());
	if (it == id_to_region_index.end()) return false;
	regions[it->second].phase = phase;
	return true;
}

bool PlanetManager::SetWorldBiome(WorldID world, WorldBiome biome)
{
	auto it = id_to_region_index.find(world.base());
	if (it == id_to_region_index.end()) return false;
	regions[it->second].biome = biome;
	return true;
}

bool PlanetManager::PromoteWorldPhase(WorldID world)
{
	auto it = id_to_region_index.find(world.base());
	if (it == id_to_region_index.end()) return false;
	auto &r = regions[it->second];
	switch (r.phase) {
		case WorldPhase::Phase4_Expansion:
			r.phase = WorldPhase::Phase3_Frontier;
			r.development_score += 100;
			return true;
		case WorldPhase::Phase3_Frontier:
			r.phase = WorldPhase::Phase2_Developed;
			r.development_score += 250;
			return true;
		case WorldPhase::Phase2_Developed:
			r.phase = WorldPhase::Phase1_Core;
			r.development_score += 500;
			return true;
		case WorldPhase::Phase1_Core:
		default:
			return false;
	}
}

bool PlanetManager::CanPromoteWorld(WorldID world)
{
	const PlanetRegion *r = GetRegion(world);
	if (r == nullptr) return false;
	switch (r->phase) {
		case WorldPhase::Phase4_Expansion:
			return true;
		case WorldPhase::Phase3_Frontier:
			return r->development_score >= DEVELOPMENT_THRESHOLD_DEVELOPED;
		case WorldPhase::Phase2_Developed:
			return r->development_score >= DEVELOPMENT_THRESHOLD_CORE;
		case WorldPhase::Phase1_Core:
		default:
			return false;
	}
}

uint32_t PlanetManager::GetPromotionThreshold(WorldPhase phase)
{
	switch (phase) {
		case WorldPhase::Phase4_Expansion:
			return DEVELOPMENT_THRESHOLD_FRONTIER;
		case WorldPhase::Phase3_Frontier:
			return DEVELOPMENT_THRESHOLD_DEVELOPED;
		case WorldPhase::Phase2_Developed:
			return DEVELOPMENT_THRESHOLD_CORE;
		case WorldPhase::Phase1_Core:
		default:
			return UINT32_MAX;
	}
}

bool PlanetManager::ColonizeWorld(WorldID world, const std::string &outpost_name, TileIndex outpost_tile)
{
	auto it = id_to_region_index.find(world.base());
	if (it == id_to_region_index.end()) return false;
	auto &r = regions[it->second];
	if (r.phase != WorldPhase::Phase4_Expansion) return false;

	r.phase = WorldPhase::Phase3_Frontier;
	r.development_score += 100;
	if (!outpost_name.empty()) {
		r.name = outpost_name;
	}
	if (outpost_tile != INVALID_TILE) {
		r.outpost_tile = outpost_tile;
	}
	return true;
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

WorldBiome PlanetManager::GetTileBiome(TileIndex tile)
{
	const PlanetRegion *r = GetRegionByTile(tile);
	return r != nullptr ? r->biome : WorldBiome::Temperate;
}

size_t PlanetManager::Count()
{
	return regions.size();
}

const std::vector<PlanetRegion> &PlanetManager::GetAllRegions()
{
	return regions;
}

CommandCost PlanetManager::CheckConstructionPlacement(TileIndex tile)
{
	/* IsValidTile distinguishes usable terrain from TileType::Void, while
	 * IsInnerTile excludes physical map-border cells that OpenTTD reserves. */
	if (tile >= Map::Size() || !IsValidTile(tile) || !IsInnerTile(tile)) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	if (GetTileWorld(tile) == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	return CommandCost();
}

CommandCost PlanetManager::CheckTownPlacement(TileIndex tile)
{
	CommandCost cost = CheckConstructionPlacement(tile);
	if (cost.Failed()) return cost;

	WorldPhase phase = GetTilePhase(tile);
	if (phase == WorldPhase::Phase4_Expansion) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
	}

	return CommandCost();
}

CommandCost PlanetManager::CheckIndustryPlacement(TileIndex tile, bool is_raw, bool is_processing, bool is_farm)
{
	if (Count() == 0) return CommandCost();

	WorldID world = GetTileWorld(tile);
	if (world == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	WorldPhase phase = GetTilePhase(tile);
	if (phase == WorldPhase::Phase4_Expansion) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
	}
	if (phase == WorldPhase::Phase1_Core && is_raw) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_CORE_WORLD);
	}
	if (phase == WorldPhase::Phase3_Frontier && is_processing) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);
	}
	if (is_farm && GetTileBiome(tile) == WorldBiome::Volcanic) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_FARM_ON_VOLCANIC_WORLD);
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
	if (phase == WorldPhase::Phase4_Expansion) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
	}
	/* Tier 3 Vac-Train/Maglev depots cannot be constructed on Phase 3 Frontier worlds */
	if (phase == WorldPhase::Phase3_Frontier && railtype == RAILTYPE_MAGLEV) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);
	}

	return CommandCost();
}

CommandCost PlanetManager::CheckTrackPlacement(TileIndex tile, RailType railtype)
{
	if (Count() == 0) return CommandCost();

	WorldID world = GetTileWorld(tile);
	if (world == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	WorldPhase phase = GetTilePhase(tile);
	if (phase == WorldPhase::Phase4_Expansion) {
		/* On uncolonized expansion wilderness, only basic pioneer track is permitted */
		if (railtype != RAILTYPE_RAIL) {
			return CommandCost(STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
		}
	} else if (phase == WorldPhase::Phase3_Frontier) {
		/* On frontier worlds, pioneer rail and electric catenary are permitted; Maglev and Monorail require Developed/Core */
		if (railtype == RAILTYPE_MAGLEV || railtype == RAILTYPE_MONO) {
			return CommandCost(STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD);
		}
	} else if (phase == WorldPhase::Phase2_Developed) {
		/* On developed worlds, Maglev is restricted to Phase 1 Core Worlds */
		if (railtype == RAILTYPE_MAGLEV) {
			return CommandCost(STR_ERROR_CANNOT_BUILD_ON_DEVELOPED_WORLD);
		}
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

void PlanetManager::RecordCargoDelivery(TileIndex dest_tile, [[maybe_unused]] CargoType cargo_type, uint num_pieces, TileIndex src_tile)
{
	if (Count() == 0 || num_pieces == 0 || dest_tile == INVALID_TILE) return;

	WorldID dest_world = GetTileWorld(dest_tile);
	if (dest_world == INVALID_WORLD) return;

	auto it = id_to_region_index.find(dest_world.base());
	if (it == id_to_region_index.end()) return;

	PlanetRegion &reg = regions[it->second];

	/* Determine if this was an interplanetary import */
	bool is_import = false;
	if (src_tile != INVALID_TILE) {
		WorldID src_world = GetTileWorld(src_tile);
		if (src_world != dest_world) {
			is_import = true;
		}
	}

	/* Development points formula:
	 * Local deliveries: 1 point per 10 cargo units (minimum 1).
	 * Interplanetary imports: 5 points per 10 cargo units (minimum 3). */
	uint32_t pts = 0;
	if (is_import) {
		pts = std::max(3u, num_pieces / 2);
	} else {
		pts = std::max(1u, num_pieces / 10);
	}

	reg.development_score += pts;
}

const char *PlanetManager::GetWorldPhaseName(WorldPhase phase)
{
	switch (phase) {
		case WorldPhase::Phase1_Core:      return "Phase 1 (Core)";
		case WorldPhase::Phase2_Developed: return "Phase 2 (Developed)";
		case WorldPhase::Phase3_Frontier:  return "Phase 3 (Frontier)";
		case WorldPhase::Phase4_Expansion: return "Phase 4 (Expansion)";
		default:                           return "Unknown Phase";
	}
}

const char *PlanetManager::GetWorldBiomeName(WorldBiome biome)
{
	switch (biome) {
		case WorldBiome::Temperate:  return "Temperate";
		case WorldBiome::SubArctic:  return "Sub-Arctic";
		case WorldBiome::SubTropic:  return "Sub-Tropic";
		case WorldBiome::AridDesert: return "Arid Desert";
		case WorldBiome::Volcanic:   return "Volcanic";
		case WorldBiome::Oceanic:    return "Oceanic";
		default:                     return "Standard";
	}
}

const PlanetRegion *PlanetManager::GetViewportCurrentPlanet(const Window *main_window)
{
	if (Count() == 0) return nullptr;

	if (main_window == nullptr) {
		main_window = FindWindowById(WindowClass::MainWindow, 0);
	}
	if (main_window == nullptr || main_window->viewport == nullptr) return nullptr;

	const Viewport &vp = *main_window->viewport;
	int center_x = vp.virtual_left + vp.virtual_width / 2;
	int center_y = vp.virtual_top + vp.virtual_height / 2;
	Point pt = InverseRemapCoords2(center_x, center_y, true);
	TileIndex tile = TileVirtXYClampedToMap(pt.x, pt.y);

	return GetRegionByTile(tile);
}

std::string PlanetManager::GetViewportStatusText(const Window *main_window)
{
	if (Count() == 0) return "";

	const PlanetRegion *region = GetViewportCurrentPlanet(main_window);
	if (region != nullptr) {
		return fmt::format("[{}] {} | {}", region->name, GetWorldPhaseName(region->phase), GetWorldBiomeName(region->biome));
	}

	return "[Interplanetary Void]";
}

bool PlanetManager::JumpToPlanet(WorldID world_id)
{
	const PlanetRegion *region = GetRegion(world_id);
	if (region == nullptr) return false;

	TileIndex target;
	if (region->outpost_tile != INVALID_TILE && IsValidTile(region->outpost_tile)) {
		target = region->outpost_tile;
	} else {
		uint center_x = (region->min_x + region->max_x) / 2;
		uint center_y = (region->min_y + region->max_y) / 2;
		target = TileXY(center_x, center_y);
	}

	Window *main_window = FindWindowById(WindowClass::MainWindow, 0);
	if (main_window != nullptr && main_window->viewport != nullptr) {
		return ScrollMainWindowToTile(target);
	}
	return true;
}

uint32_t PlanetManager::GetWorldPopulation(WorldID world)
{
	if (world == INVALID_WORLD || Count() == 0) return 0;
	uint32_t pop = 0;
	for (const Town *t : Town::Iterate()) {
		if (GetTileWorld(t->xy) == world) {
			pop += t->cache.population;
		}
	}
	return pop;
}

Town *PlanetManager::GetWorldPrimaryTown(WorldID world)
{
	if (world == INVALID_WORLD || Count() == 0) return nullptr;
	Town *best_town = nullptr;
	uint32_t max_pop = 0;
	const PlanetRegion *reg = GetRegion(world);

	for (Town *t : Town::Iterate()) {
		if (GetTileWorld(t->xy) == world) {
			/* If an outpost tile matches exactly, prefer it */
			if (reg != nullptr && reg->outpost_tile != INVALID_TILE && t->xy == reg->outpost_tile) {
				return t;
			}
			if (best_town == nullptr || t->cache.population >= max_pop) {
				max_pop = t->cache.population;
				best_town = t;
			}
		}
	}
	return best_town;
}

size_t PlanetManager::GetWorldIndustryCount(WorldID world)
{
	if (world == INVALID_WORLD || Count() == 0) return 0;
	size_t count = 0;
	for (const Industry *i : Industry::Iterate()) {
		if (GetTileWorld(i->location.tile) == world) {
			count++;
		}
	}
	return count;
}

