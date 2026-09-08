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
#include "../void_map.h"
#include "../tunnel_map.h"
#include "../rail_map.h"
#include "../clear_map.h"
#include "../tile_map.h"
#include "../map_func.h"
#include <algorithm>
#include <vector>

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
				MakeVoid(TileXY(x, y));
			}
		}
	}

	/* 2. Register all world regions in PlanetManager */
	PlanetManager::Reset();
	for (const auto &reg : regions) {
		PlanetManager::RegisterRegion(reg);
	}

	/* 3. Place and register starting gateway pairs between adjacent worlds */
	if (config.place_gateways && regions.size() >= 2) {
		PortalRegistry::Reset();

		bool split_y = (size_y >= size_x);
		uint32_t cross_center = (split_y ? size_x : size_y) / 2;

		for (size_t i = 0; i < regions.size() - 1; i++) {
			const auto &reg_a = regions[i];
			const auto &reg_b = regions[i + 1];

			TileIndex t_a, t_a_track;
			TileIndex t_b, t_b_track;
			DiagDirection dir_a, dir_b;
			TrackBits track_bits;

			if (split_y) {
				t_a = TileXY(cross_center, reg_a.max_y - 1);
				t_a_track = TileXY(cross_center, reg_a.max_y - 2);
				dir_a = DiagDirection::NW;

				t_b = TileXY(cross_center, reg_b.min_y + 1);
				t_b_track = TileXY(cross_center, reg_b.min_y + 2);
				dir_b = DiagDirection::SE;

				track_bits = TrackBits{Track::Y};
			} else {
				t_a = TileXY(reg_a.max_x - 1, cross_center);
				t_a_track = TileXY(reg_a.max_x - 2, cross_center);
				dir_a = DiagDirection::NE;

				t_b = TileXY(reg_b.min_x + 1, cross_center);
				t_b_track = TileXY(reg_b.min_x + 2, cross_center);
				dir_b = DiagDirection::SW;

				track_bits = TrackBits{Track::X};
			}

			/* Construct gateway portal and lead track for world A */
			MakeClear(t_a, ClearGround::Grass, 3);
			SetTileHeight(t_a, 1);
			MakeRailTunnel(t_a, OWNER_NONE, dir_a, RAILTYPE_BEGIN);

			MakeClear(t_a_track, ClearGround::Grass, 3);
			SetTileHeight(t_a_track, 1);
			MakeRailNormal(t_a_track, OWNER_NONE, track_bits, RAILTYPE_BEGIN);

			/* Construct gateway portal and lead track for world B */
			MakeClear(t_b, ClearGround::Grass, 3);
			SetTileHeight(t_b, 1);
			MakeRailTunnel(t_b, OWNER_NONE, dir_b, RAILTYPE_BEGIN);

			MakeClear(t_b_track, ClearGround::Grass, 3);
			SetTileHeight(t_b_track, 1);
			MakeRailNormal(t_b_track, OWNER_NONE, track_bits, RAILTYPE_BEGIN);

			/* Register the bidirectional wormhole portal link */
			uint32_t virt_dist = split_y ? (reg_b.min_y - reg_a.max_y - 1) : (reg_b.min_x - reg_a.max_x - 1);
			virt_dist = std::max(2u, virt_dist);

			PortalRegistry::RegisterPortalPair(
				t_a, dir_a, reg_a.id,
				t_b, dir_b, reg_b.id,
				virt_dist, true
			);
		}
	}

	return true;
}
