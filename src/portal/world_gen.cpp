/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file world_gen.cpp Multi-world procedural map generation, spatial partitioning, and gateway initialization. */

#include "../stdafx.h"
#include "world_gen.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "../void_map.h"
#include "../tunnel_map.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../clear_map.h"
#include "../tree_map.h"
#include "../direction_func.h"
#include "../tile_map.h"
#include "../map_func.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

/**
 * Find the nearest clear, level site for a generated gateway head and its full
 * two-lane terminal. This keeps the local rail transition physically valid
 * without constraining the remote endpoint to a matching coordinate or axis.
 */
static TileIndex FindGeneratedGatewaySite(const PlanetRegion &region, TileIndex nominal, DiagDirection &enter_dir)
{
	int nominal_x = TileX(nominal);
	int nominal_y = TileY(nominal);
	int max_radius = std::max(region.max_x - region.min_x, region.max_y - region.min_y);
	std::array<DiagDirection, 4> directions{
		enter_dir,
		ChangeDiagDir(enter_dir, DiagDirDiff::Right90),
		ReverseDiagDir(enter_dir),
		ChangeDiagDir(enter_dir, DiagDirDiff::Left90),
	};

	for (DiagDirection candidate_dir : directions) {
		for (int radius = 0; radius <= max_radius; radius++) {
			for (int dy = -radius; dy <= radius; dy++) {
				for (int dx = -radius; dx <= radius; dx++) {
					if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
					int x = nominal_x + dx;
					int y = nominal_y + dy;
					if (x < static_cast<int>(region.min_x) || x > static_cast<int>(region.max_x) ||
							y < static_cast<int>(region.min_y) || y > static_cast<int>(region.max_y)) continue;

					TileIndex tile = TileXY(x, y);
					if (!IsTileType(tile, TileType::Clear) || GetTileSlope(tile) != SLOPE_FLAT) continue;
					std::optional<PortalTerminalLayout> terminal = PortalTerminal::Plan(tile, candidate_dir, region.id);
					if (!terminal.has_value()) continue;

					bool suitable = true;
					for (const PortalTerminalTile &part : terminal->tiles) {
						if (!IsTileType(part.tile, TileType::Clear) || GetTileSlope(part.tile) != SLOPE_FLAT ||
								TileHeight(part.tile) != TileHeight(tile)) {
							suitable = false;
							break;
						}
					}
					if (!suitable) continue;

					enter_dir = candidate_dir;
					return tile;
				}
			}
		}
	}

	return INVALID_TILE;
}

} // namespace

bool MultiWorldGen::enabled = true;

bool MultiWorldGen::IsEnabled()
{
	return enabled;
}

void MultiWorldGen::SetEnabled(bool val)
{
	enabled = val;
}

std::vector<PlanetRegion> MultiWorldGen::CalculateLayout(uint32_t size_x, uint32_t size_y)
{
	return CalculateLayout(size_x, size_y, Config{});
}

std::vector<PlanetRegion> MultiWorldGen::CalculateLayout(uint32_t size_x, uint32_t size_y, const Config &config)
{
	if (config.world_count == 0 || size_x < 32 || size_y < 32) return {};

	bool split_y = (size_y >= size_x);
	uint32_t total_length = split_y ? size_y : size_x;
	uint32_t cross_length = split_y ? size_x : size_y;

	uint32_t pad = config.border_padding;
	if (total_length <= 2 * pad + config.world_count * 4) return {};

	uint32_t start_coord = pad;
	uint32_t end_coord = total_length - 1 - pad;
	uint32_t available_span = end_coord - start_coord + 1;
	uint32_t num_buffers = config.world_count - 1;

	uint32_t buffer_w = config.buffer_width;
	if (buffer_w == 0) {
		buffer_w = std::max(4u, std::min(size_x, size_y) / 16u);
	}

	while (num_buffers > 0 && (num_buffers * buffer_w + config.world_count * 8 > available_span) && buffer_w > 2) {
		buffer_w /= 2;
	}

	if (num_buffers * buffer_w >= available_span) return {};

	uint32_t net_world_span = available_span - (num_buffers * buffer_w);
	uint32_t base_world_span = net_world_span / config.world_count;
	uint32_t remainder = net_world_span % config.world_count;

	std::vector<PlanetRegion> result;
	result.reserve(config.world_count);

	uint32_t cur_coord = start_coord;
	for (uint32_t i = 0; i < config.world_count; i++) {
		uint32_t span = base_world_span + (i < remainder ? 1 : 0);
		PlanetRegion region;
		region.id = WorldID{i};
		region.development_score = 0;

		switch (i) {
			case 0:
				region.phase = WorldPhase::Phase1_Core;
				region.biome = WorldBiome::Temperate;
				region.name = "Core Hub (Phase 1)";
				break;
			case 1:
				region.phase = WorldPhase::Phase2_Developed;
				region.biome = WorldBiome::AridDesert;
				region.name = "Emerging Colony (Phase 2)";
				break;
			case 2:
				region.phase = WorldPhase::Phase3_Frontier;
				region.biome = WorldBiome::SubArctic;
				region.name = "Frontier Outskirts (Phase 3)";
				break;
			default:
				region.phase = WorldPhase::Phase4_Expansion;
				region.biome = WorldBiome::Volcanic;
				region.name = "Wilderness (Phase 4)";
				break;
		}

		if (split_y) {
			region.min_x = pad;
			region.max_x = cross_length - 1 - pad;
			region.min_y = cur_coord;
			region.max_y = cur_coord + span - 1;
		} else {
			region.min_y = pad;
			region.max_y = cross_length - 1 - pad;
			region.min_x = cur_coord;
			region.max_x = cur_coord + span - 1;
		}

		result.push_back(region);
		cur_coord += span + buffer_w;
	}

	return result;
}

bool MultiWorldGen::GenerateMultiWorldLayout(uint32_t size_x, uint32_t size_y)
{
	return GenerateMultiWorldLayout(size_x, size_y, Config{});
}

bool MultiWorldGen::GenerateMultiWorldLayout(uint32_t size_x, uint32_t size_y, const Config &config)
{
	std::vector<PlanetRegion> regions = CalculateLayout(size_x, size_y, config);
	if (regions.empty()) return false;

	/* 1. Set all non-world tiles (buffers and outer borders) to TileType::Void */
	for (uint32_t y = 0; y < size_y; y++) {
		for (uint32_t x = 0; x < size_x; x++) {
			bool in_world = false;
			for (const auto &reg : regions) {
				if (reg.ContainsCoord(x, y)) {
					in_world = true;
					break;
				}
			}
			if (!in_world) {
				uint height = TileHeight(TileXY(x, y));
				MakeVoid(TileXY(x, y));
				SetTileHeight(TileXY(x, y), height);
			}
		}
	}

	/* 2. Register all world regions in PlanetManager */
	PlanetManager::Reset();
	for (const auto &reg : regions) {
		PlanetManager::RegisterRegion(reg);
	}

	/* 2b. Apply alien biome environmental stylization across worlds */
	for (const auto &reg : regions) {
		ApplyBiomeStyling(reg);
	}

	/* 3. Place and register starting gateway pairs between adjacent worlds */
	if (config.place_gateways && regions.size() >= 2) {
		PortalRegistry::Reset();

		bool split_y = (size_y >= size_x);

		for (size_t i = 0; i < regions.size() - 1; i++) {
			const auto &reg_a = regions[i];
			const auto &reg_b = regions[i + 1];

			TileIndex t_a;
			TileIndex t_b;
			DiagDirection dir_a, dir_b;

			/* Place the two heads well inside their worlds, at different X/Y
			 * coordinates and on perpendicular axes. Generated gateways must visibly
			 * demonstrate arbitrary wormhole endpoints rather than resemble a long
			 * straight tunnel across the void buffer. */
			uint32_t span_a_x = reg_a.max_x - reg_a.min_x;
			uint32_t span_a_y = reg_a.max_y - reg_a.min_y;
			uint32_t span_b_x = reg_b.max_x - reg_b.min_x;
			uint32_t span_b_y = reg_b.max_y - reg_b.min_y;

			if ((i & 1) == 0) {
				t_a = TileXY(reg_a.min_x + 3 * span_a_x / 4, reg_a.min_y + span_a_y / 3);
				dir_a = DiagDirection::SW;

				t_b = TileXY(reg_b.min_x + span_b_x / 4, reg_b.min_y + 2 * span_b_y / 3);
				dir_b = DiagDirection::NW;
			} else {
				t_a = TileXY(reg_a.min_x + 3 * span_a_x / 4, reg_a.min_y + 2 * span_a_y / 3);
				dir_a = DiagDirection::SE;

				t_b = TileXY(reg_b.min_x + span_b_x / 4, reg_b.min_y + span_b_y / 3);
				dir_b = DiagDirection::NE;
			}

			t_a = FindGeneratedGatewaySite(reg_a, t_a, dir_a);
			t_b = FindGeneratedGatewaySite(reg_b, t_b, dir_b);
			if (t_a == INVALID_TILE || t_b == INVALID_TILE) return false;

			std::optional<PortalTerminalLayout> terminal_a = PortalTerminal::Plan(t_a, dir_a, reg_a.id);
			std::optional<PortalTerminalLayout> terminal_b = PortalTerminal::Plan(t_b, dir_b, reg_b.id);
			if (!terminal_a.has_value() || !terminal_b.has_value()) return false;

			/* Construct gateway portal and high-capacity terminal for world A. */
			MakeClear(t_a, ClearGround::Grass, 3);
			MakeRailTunnel(t_a, OWNER_NONE, dir_a, RAILTYPE_BEGIN);
			for (const PortalTerminalTile &part : terminal_a->tiles) MakeClear(part.tile, ClearGround::Grass, 3);
			PortalTerminal::Build(*terminal_a, RAILTYPE_BEGIN, OWNER_NONE);

			/* Construct gateway portal and high-capacity terminal for world B. */
			MakeClear(t_b, ClearGround::Grass, 3);
			MakeRailTunnel(t_b, OWNER_NONE, dir_b, RAILTYPE_BEGIN);
			for (const PortalTerminalTile &part : terminal_b->tiles) MakeClear(part.tile, ClearGround::Grass, 3);
			PortalTerminal::Build(*terminal_b, RAILTYPE_BEGIN, OWNER_NONE);

			/* Register the bidirectional wormhole portal link */
			uint32_t virt_dist = split_y ? (reg_b.min_y - reg_a.max_y - 1) : (reg_b.min_x - reg_a.max_x - 1);
			virt_dist = std::max(2u, virt_dist);

			PortalRegistry::RegisterPortalPair(
				t_a, dir_a, reg_a.id,
				t_b, dir_b, reg_b.id,
				virt_dist, true
			);
		}
		UpdateSignalsInBuffer();
	}

	return true;
}

void MultiWorldGen::ApplyBiomeStyling(const PlanetRegion &region)
{
	for (uint32_t y = region.min_y; y <= region.max_y; ++y) {
		for (uint32_t x = region.min_x; x <= region.max_x; ++x) {
			TileIndex t = TileXY(x, y);
			if (!IsValidTile(t)) continue;

			switch (region.biome) {
				case WorldBiome::AridDesert: {
					SetTropicZone(t, TropicZone::Desert);
					if (IsTileType(t, TileType::Clear)) {
						uint density = GetClearDensity(t);
						if ((TileHash(x, y) & 0x7) == 0) {
							SetClearGroundDensity(t, ClearGround::Rocks, density);
						} else {
							SetClearGroundDensity(t, ClearGround::Desert, density);
						}
					} else if (IsTileType(t, TileType::Trees)) {
						uint tree_count = GetTreeCount(t);
						TreeGrowthStage growth = GetTreeGrowth(t);
						MakeTree(t, TREE_CACTUS, tree_count - 1, growth, TreeGround::SnowOrDesert, 3);
					}
					break;
				}

				case WorldBiome::SubArctic: {
					if (IsTileType(t, TileType::Clear)) {
						uint density = GetClearDensity(t);
						if (!IsSnowTile(t)) {
							MakeSnow(t, density);
						}
						if ((TileHash(x, y) & 0x7) == 0) {
							SetClearGroundDensity(t, ClearGround::Rocks, density);
						}
					} else if (IsTileType(t, TileType::Trees)) {
						uint tree_count = GetTreeCount(t);
						TreeGrowthStage growth = GetTreeGrowth(t);
						MakeTree(t, TREE_SUB_ARCTIC, tree_count - 1, growth, TreeGround::SnowOrDesert, 3);
					}
					break;
				}

				case WorldBiome::Temperate: {
					SetTropicZone(t, TropicZone::Normal);
					if (IsTileType(t, TileType::Clear)) {
						if (IsSnowTile(t)) ClearSnow(t);
						uint density = GetClearDensity(t);
						SetClearGroundDensity(t, ClearGround::Grass, density);
					} else if (IsTileType(t, TileType::Trees)) {
						uint tree_count = GetTreeCount(t);
						TreeGrowthStage growth = GetTreeGrowth(t);
						MakeTree(t, TREE_TEMPERATE, tree_count - 1, growth, TreeGround::Grass, 3);
					}
					break;
				}

				case WorldBiome::Volcanic: {
					if (IsTileType(t, TileType::Clear)) {
						if (IsSnowTile(t)) ClearSnow(t);
						uint density = GetClearDensity(t);
						if ((TileHash(x, y) & 0x3) == 0) {
							SetClearGroundDensity(t, ClearGround::Rocks, density);
						} else {
							SetClearGroundDensity(t, ClearGround::Rough, density);
						}
					}
					break;
				}

				default:
					break;
			}
		}
	}
}

