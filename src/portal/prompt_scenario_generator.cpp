/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file prompt_scenario_generator.cpp Procedural scenario synthesis from natural language narrative prompts. */

#include "../stdafx.h"
#include "prompt_scenario_generator.h"
#include "planet_manager.h"
#include "world_gen.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "megacity_manager.h"
#include "corporate_hq.h"
#include "company_stockpile.h"
#include "logistics_hub.h"
#include "consist_materializer.h"
#include "tech_tree.h"
#include "fabrication_manager.h"
#include "spaceport_manager.h"
#include "edge_conduit.h"
#include "prebuilt_trade.h"

#include "../core/pool_type.hpp"
#include "../linkgraph/linkgraphschedule.h"
#include "../viewport_kdtree.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../station_base.h"
#include "../station_kdtree.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../vehicle_base.h"
#include "../train.h"
#include "../order_base.h"
#include "../waypoint_base.h"
#include "../saveload/saveload.h"
#include "../animated_tile_func.h"
#include "../signal_func.h"
#include "../direction_func.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../timer/timer_game_calendar.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../blueprint/blueprint_manager.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../industrytype.h"
#include "../core/backup_type.hpp"
#include "../depot_base.h"
#include "../rail_cmd.h"
#include "../cargotype.h"
#include "../settings_type.h"
#include <filesystem>

static constexpr IndustryType IT_COAL_MINE     = 0;
static constexpr IndustryType IT_POWER_STATION = 1;
static constexpr IndustryType IT_FACTORY       = 6;
static constexpr IndustryType IT_STEEL_MILL    = 8;
static constexpr IndustryType IT_IRON_MINE     = 18;

#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>

static std::string ToLower(std::string_view s)
{
	std::string res;
	res.reserve(s.size());
	for (char c : s) res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	return res;
}

PromptScenarioSpec PromptScenarioGenerator::ParsePrompt(const std::string &prompt_text)
{
	PromptScenarioSpec spec;
	spec.original_prompt = prompt_text;
	spec.title = "Commonwealth Narrative Scenario";
	spec.narrative_summary = prompt_text;

	std::string lower = ToLower(prompt_text);

	/* 1. Extract world count */
	uint32_t count = 3;
	std::regex world_count_re(R"((\d+)[ -]?(?:world|planet))");
	std::smatch match;
	if (std::regex_search(lower, match, world_count_re)) {
		try {
			uint32_t val = static_cast<uint32_t>(std::stoul(match[1].str()));
			if (val >= 2 && val <= 6) count = val;
		} catch (...) {}
	} else if (lower.find("two world") != std::string::npos || lower.find("two planet") != std::string::npos) {
		count = 2;
	} else if (lower.find("four world") != std::string::npos || lower.find("four planet") != std::string::npos) {
		count = 4;
	} else if (lower.find("five world") != std::string::npos) {
		count = 5;
	} else if (lower.find("six world") != std::string::npos) {
		count = 6;
	}
	spec.world_count = count;

	/* 2. Determine diplomatic stance */
	if (lower.find("hostile") != std::string::npos || lower.find("greedy") != std::string::npos || lower.find("conflict") != std::string::npos || lower.find("war") != std::string::npos) {
		spec.rival_relation = CorporateRelation::Hostile;
	} else if (lower.find("allied") != std::string::npos || lower.find("alliance") != std::string::npos || lower.find("partner") != std::string::npos) {
		spec.rival_relation = CorporateRelation::Allied;
	} else {
		spec.rival_relation = CorporateRelation::Neutral;
	}

	if (lower.find("grand central") != std::string::npos) {
		spec.rival_corporation = "Grand Central Trans-Portal";
	} else {
		spec.rival_corporation = "CST";
	}

	/* 3. Build world specifications */
	spec.worlds.resize(count);

	/* World 0: Primary Core / Capital World */
	spec.worlds[0].id = WorldID{0};
	spec.worlds[0].phase = WorldPhase::Phase1_Core;
	spec.worlds[0].biome = WorldBiome::Temperate;
	spec.worlds[0].name = "Augusta Prime Core";
	spec.worlds[0].role_description = "High-density consumer metropolis and planetary corporate headquarters.";
	spec.worlds[0].has_megacity = true;
	spec.worlds[0].population = 25000;
	spec.worlds[0].has_corporate_hq = true;

	/* Check for demanded commodities in core world */
	if (lower.find("superalloy") != std::string::npos) {
		spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::Superalloys);
	}
	if (lower.find("electronic") != std::string::npos || lower.find("chip") != std::string::npos) {
		spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::SiliconChips);
	}
	if (lower.find("crystal") != std::string::npos) {
		spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::EnrichedQuantumCrystals);
	}
	if (lower.find("steel") != std::string::npos) {
		spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::StructuralSteel);
	}
	if (spec.worlds[0].demanded_cargos.empty()) {
		spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::Superalloys);
	}

	/* World 1: Industrial Processing or Secondary Colony */
	if (count >= 3) {
		spec.worlds[1].id = WorldID{1};
		spec.worlds[1].phase = WorldPhase::Phase2_Developed;
		spec.worlds[1].biome = (lower.find("volcanic") != std::string::npos) ? WorldBiome::Volcanic : WorldBiome::Temperate;
		spec.worlds[1].name = "Hephaestus Smelting Forge";
		spec.worlds[1].role_description = "Heavy industrial processing and foundry operations.";
		spec.worlds[1].has_industrial_facility = true;
		spec.worlds[1].industrial_recipe = RECIPE_SUPERALLOY_FOUNDRY;
		spec.worlds[1].supplied_cargos.push_back(CommonwealthCargoID::Superalloys);
		spec.worlds[1].demanded_cargos.push_back(CommonwealthCargoID::IronOre);
		spec.worlds[1].demanded_cargos.push_back(CommonwealthCargoID::RareEarthMinerals);
		spec.worlds[1].population = 8000;
	}

	/* Last World: Resource Extraction Frontier / Mining Colony */
	size_t frontier_idx = count - 1;
	spec.worlds[frontier_idx].id = WorldID{static_cast<uint32_t>(frontier_idx)};
	spec.worlds[frontier_idx].phase = WorldPhase::Phase3_Frontier;

	/* Biome detection for frontier colony */
	if (lower.find("arid") != std::string::npos || lower.find("desert") != std::string::npos || lower.find("sand") != std::string::npos) {
		spec.worlds[frontier_idx].biome = WorldBiome::AridDesert;
		spec.worlds[frontier_idx].name = "Merredin Desert Outpost";
	} else if (lower.find("ice") != std::string::npos || lower.find("glacial") != std::string::npos || lower.find("frozen") != std::string::npos || lower.find("arctic") != std::string::npos) {
		spec.worlds[frontier_idx].biome = WorldBiome::SubArctic;
		spec.worlds[frontier_idx].name = "Calyx Glacial Basin";
	} else if (lower.find("volcanic") != std::string::npos || lower.find("magma") != std::string::npos) {
		spec.worlds[frontier_idx].biome = WorldBiome::Volcanic;
		spec.worlds[frontier_idx].name = "Prometheus Caldera Outpost";
	} else if (lower.find("ocean") != std::string::npos || lower.find("sea") != std::string::npos) {
		spec.worlds[frontier_idx].biome = WorldBiome::Oceanic;
		spec.worlds[frontier_idx].name = "Pelican Archipelago";
	} else {
		spec.worlds[frontier_idx].biome = WorldBiome::AridDesert;
		spec.worlds[frontier_idx].name = "Merredin Mining Colony";
	}

	spec.worlds[frontier_idx].role_description = "High-yield raw resource extraction and deep-vein mining.";
	spec.worlds[frontier_idx].supplied_cargos.push_back(CommonwealthCargoID::IronOre);
	spec.worlds[frontier_idx].supplied_cargos.push_back(CommonwealthCargoID::RareEarthMinerals);
	spec.worlds[frontier_idx].population = 3500;

	/* Strike / Shortage detection */
	if (lower.find("strik") != std::string::npos || lower.find("shortage") != std::string::npos || lower.find("drought") != std::string::npos || lower.find("famine") != std::string::npos) {
		spec.worlds[frontier_idx].is_striking = true;
		spec.worlds[frontier_idx].strike_cause = "Critical Life-Support & Water Scarcity";
		spec.worlds[frontier_idx].demanded_cargos.push_back(CommonwealthCargoID::StoneSlag); // sustenance analog
	}

	/* Populate any intermediate worlds if count > 3 */
	for (size_t i = 2; i < frontier_idx; ++i) {
		spec.worlds[i].id = WorldID{static_cast<uint32_t>(i)};
		spec.worlds[i].phase = WorldPhase::Phase2_Developed;
		spec.worlds[i].biome = WorldBiome::SubTropic;
		spec.worlds[i].name = fmt::format("Outer Territory Alpha-{}", i);
		spec.worlds[i].role_description = "Regional agrarian and assembly sector.";
		spec.worlds[i].population = 5000;
	}

	return spec;
}

bool PromptScenarioGenerator::BuildCorridorsAndStations(const PromptScenarioSpec &spec, ScenarioSynthesisResult &result)
{
	CompanyID human_company{0};
	AutoRestoreBackup cur_company(_current_company, human_company);
	AutoRestoreBackup old_game_mode(_game_mode, GameMode::Editor);

	for (size_t i = 0; i < spec.worlds.size(); ++i) {
		const PlanetRegion *region = PlanetManager::GetRegion(WorldID{static_cast<uint32_t>(i)});
		if (region == nullptr) continue;

		uint32_t center_x = (region->min_x + region->max_x) / 2;
		uint32_t center_y = (region->min_y + region->max_y) / 2;

		/* 1. Build a local station platform */
		TileIndex station_tile = TileXY(center_x, center_y);
		MakeClear(station_tile, ClearGround::Grass, 0);

		Town *town = ClosestTownFromTile(station_tile, UINT_MAX);
		if (town == nullptr && Town::CanAllocateItem()) {
			town = Town::Create(station_tile);
			if (town != nullptr) {
				town->name = spec.worlds[i].name;
				town->townnametype = SPECSTR_TOWNNAME_START;
				RebuildTownKdtree();
			}
		}

		if (Station::CanAllocateItem() && town != nullptr) {
			Station *st = Station::Create(station_tile);
			if (st != nullptr) {
				st->name = fmt::format("{} Central Terminal", spec.worlds[i].name);
				st->owner = human_company;
				st->town = town;
				st->facilities.Set(StationFacility::Train);
				st->train_station = TileArea(station_tile, 1, 1);
				st->spread = st->train_station;
				MakeRailStation(station_tile, human_company, st->index, Axis::X, 0, RAILTYPE_BEGIN);
				st->RecomputeCatchment();
				result.stations_placed++;

				/* Attach company logistics hub to station */
				TileIndex hub_tile = TileXY(center_x - 4, center_y + 4);
				LogisticsHubManager::RegisterHub(hub_tile, region->id, human_company, st->index, fmt::format("{} Logistics Hub", spec.worlds[i].name));
			}
		}

		/* 2. Find the nearest portal terminal in this world */
		TileIndex gate_tile = INVALID_TILE;
		for (const auto &[pid, link] : PortalRegistry::GetAllPortals()) {
			if (link.end_a.world_id == region->id) {
				gate_tile = link.end_a.tile;
				break;
			} else if (link.end_b.world_id == region->id) {
				gate_tile = link.end_b.tile;
				break;
			}
		}

		if (gate_tile != INVALID_TILE) {
			/* Connect station to portal terminal with straight track segments */
			int sx = static_cast<int>(TileX(station_tile));
			int sy = static_cast<int>(TileY(station_tile));
			int gx = static_cast<int>(TileX(gate_tile));
			int gy = static_cast<int>(TileY(gate_tile));
			(void)gy;

			/* Place connecting rail line toward the gate */
			int step_x = (gx > sx) ? 1 : (gx < sx) ? -1 : 0;
			int cur_x = sx + step_x;
			int cur_y = sy;

			int pieces = 0;
			while (cur_x != gx && pieces < 20) {
				TileIndex t = TileXY(cur_x, cur_y);
				if (IsValidTile(t) && !IsTileType(t, TileType::Void) && !IsTileType(t, TileType::Station) && !IsTileType(t, TileType::TunnelBridge)) {
					MakeClear(t, ClearGround::Grass, 0);
					MakeRailNormal(t, human_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
					pieces++;
				}
				cur_x += step_x;
			}

			/* Place track behind the station for consist tail buffer / siding */
			for (int b = 1; b <= 3; ++b) {
				TileIndex bt = TileXY(sx - step_x * b, sy);
				if (IsValidTile(bt) && !IsTileType(bt, TileType::Void) && !IsTileType(bt, TileType::Station) && !IsTileType(bt, TileType::TunnelBridge)) {
					MakeClear(bt, ClearGround::Grass, 0);
					MakeRailNormal(bt, human_company, TrackBits{Track::X}, RAILTYPE_BEGIN);
				}
			}

			/* Place train depot at end of siding */
			TileIndex depot_tile = TileXY(sx - step_x * 4, sy);
			if (IsValidTile(depot_tile) && !IsTileType(depot_tile, TileType::Void) && Depot::CanAllocateItem()) {
				MakeClear(depot_tile, ClearGround::Grass, 0);
				DiagDirection depot_dir = (step_x > 0) ? DiagDirection::SW : DiagDirection::NE;
				CommandCost depot_res = CmdBuildTrainDepot(DoCommandFlag::Execute, depot_tile, RAILTYPE_BEGIN, depot_dir);
				if (depot_res.Succeeded()) {
					result.depots_placed++;
				} else {
					Depot *d = Depot::Create(depot_tile);
					if (d != nullptr) {
						MakeRailDepot(depot_tile, human_company, d->index, depot_dir, RAILTYPE_BEGIN);
						MakeDefaultName(d);
						Company *c = Company::GetIfValid(human_company);
						if (c != nullptr) c->infrastructure.rail[RAILTYPE_BEGIN]++;
						result.depots_placed++;
					}
				}
			}

			/* Place a waypoint along the approach */
			TileIndex wp_tile = TileXY(sx + step_x * 4, sy);
			if (IsValidTile(wp_tile) && Waypoint::CanAllocateItem()) {
				Waypoint *wp = Waypoint::Create(wp_tile);
				if (wp != nullptr) {
					wp->owner = human_company;
					wp->facilities.Set(StationFacility::Train);
					wp->name = fmt::format("{} Portal Approach", spec.worlds[i].name);
					MakeRailWaypoint(wp_tile, human_company, wp->index, Axis::X, 0, RAILTYPE_BEGIN);
				}
			}

			/* On Augusta Hub (World 1), create a designated Holding Siding station */
			if (region->id == WorldID{1} && town != nullptr && Station::CanAllocateItem()) {
				TileIndex staging_tile = TileXY(sx + step_x * 2, sy + 2);
				MakeClear(staging_tile, ClearGround::Grass, 0);
				Station *st_staging = Station::Create(staging_tile);
				if (st_staging != nullptr) {
					st_staging->name = "Augusta Gateway Holding Siding";
					st_staging->owner = human_company;
					st_staging->town = town;
					st_staging->facilities.Set(StationFacility::Train);
					st_staging->facilities.Set(StationFacility::HoldingSiding);
					st_staging->train_station = TileArea(staging_tile, 1, 1);
					st_staging->spread = st_staging->train_station;
					MakeRailStation(staging_tile, human_company, st_staging->index, Axis::X, 0, RAILTYPE_BEGIN);
				}
			}

			result.corridors_built++;
		}
	}

	RebuildStationKdtree();
	RebuildTownKdtree();
	UpdateSignalsInBuffer();
	return true;
}

bool PromptScenarioGenerator::PlaceCanonicalIndustries(const PromptScenarioSpec &spec, ScenarioSynthesisResult &result)
{
	AutoRestoreBackup cur_company(_current_company, OWNER_DEITY);
	AutoRestoreBackup old_game_mode(_game_mode, GameMode::Editor);

	for (size_t i = 0; i < spec.worlds.size(); ++i) {
		const auto &w_spec = spec.worlds[i];
		const PlanetRegion *region = PlanetManager::GetRegion(WorldID{static_cast<uint32_t>(i)});
		if (region == nullptr) continue;

		uint32_t center_x = (region->min_x + region->max_x) / 2;
		uint32_t center_y = (region->min_y + region->max_y) / 2;

		IndustryType primary_type = IT_INVALID;
		IndustryType secondary_type = IT_INVALID;

		if (w_spec.phase == WorldPhase::Phase3_Frontier || w_spec.name.find("Mining") != std::string::npos) {
			/* Mining Frontier: Iron Ore Mine + Coal Mine */
			primary_type = IT_IRON_MINE;
			secondary_type = IT_COAL_MINE;
		} else if (w_spec.phase == WorldPhase::Phase2_Developed || w_spec.name.find("Augusta") != std::string::npos) {
			/* Industrial Hub: Steel Mill */
			primary_type = IT_STEEL_MILL;
		} else if (w_spec.phase == WorldPhase::Phase1_Core || w_spec.name.find("Earth") != std::string::npos) {
			/* Metropolitan Core: Factory */
			primary_type = IT_FACTORY;
		} else if (w_spec.phase == WorldPhase::Phase4_Expansion || w_spec.name.find("Prometheus") != std::string::npos) {
			/* Energy / Outpost: Power Station */
			primary_type = IT_POWER_STATION;
		}

		auto try_build_industry = [&](IndustryType it, int offset_x, int offset_y) -> bool {
			if (it == IT_INVALID) return false;
			const IndustrySpec *indspec = GetIndustrySpec(it);
			if (!indspec->enabled || indspec->layouts.empty()) return false;

			for (int try_dx : {offset_x, offset_x + 2, offset_x - 2, offset_x + 3, offset_x - 3}) {
				for (int try_dy : {offset_y, -offset_y, offset_y + 1, -offset_y - 1}) {
					TileIndex ind_tile = TileXY(center_x + try_dx, center_y + try_dy);
					if (!IsValidTile(ind_tile) || IsTileType(ind_tile, TileType::Void)) continue;

					/* Clear 6x6 footprint around candidate ind_tile */
					for (int cy = -1; cy <= 4; ++cy) {
						for (int cx = -1; cx <= 4; ++cx) {
							TileIndex t = TileXY(center_x + try_dx + cx, center_y + try_dy + cy);
							if (IsValidTile(t) && !IsTileType(t, TileType::Void) && !IsTileType(t, TileType::Station) && !IsTileType(t, TileType::TunnelBridge)) {
								MakeClear(t, ClearGround::Grass, 0);
							}
						}
					}

					CommandCost res = CmdBuildIndustry(DoCommandFlag::Execute, ind_tile, it, 0, true, 42);
					if (res.Succeeded()) {
						result.industries_placed++;
						return true;
					}
				}
			}
			return false;
		};

		if (primary_type != IT_INVALID) {
			try_build_industry(primary_type, 0, 3);
		}
		if (secondary_type != IT_INVALID) {
			try_build_industry(secondary_type, 3, -3);
		}
	}

	Station::RecomputeCatchmentForAll();
	return true;
}

bool PromptScenarioGenerator::SpawnActiveFleets(const PromptScenarioSpec &spec, ScenarioSynthesisResult &result)
{
	CompanyID human_company{0};
	if (spec.worlds.size() < 2) return true;

	/* Find stations in World 0 and the mining world */
	Station *st_core = nullptr;
	Station *st_mining = nullptr;

	WorldID core_id = WorldID{0};
	WorldID mining_id = WorldID{static_cast<uint32_t>(spec.worlds.size() - 1)};
	for (const auto &w_spec : spec.worlds) {
		bool provides_ore = false;
		for (auto cargo : w_spec.supplied_cargos) {
			if (cargo == CommonwealthCargoID::IronOre) provides_ore = true;
		}
		if (provides_ore) {
			mining_id = w_spec.id;
			break;
		}
	}

	for (Station *st : Station::Iterate()) {
		if (st->owner != human_company) continue;
		WorldID wid = PlanetManager::GetTileWorld(st->xy);
		if (wid == core_id && st_core == nullptr) {
			st_core = st;
		} else if (wid == mining_id && st_mining == nullptr) {
			st_mining = st;
		}
	}

	if (st_core == nullptr || st_mining == nullptr) return false;

	/* Find or initialize train engines */
	if (Engine::GetNumItems() == 0) {
		_engine_mngr.ResetToDefaultMapping();
		SetupEngines();
		StartupEngines();
	}

	EngineID locomotive = EngineID::Invalid();
	EngineID wagon = EngineID::Invalid();
	for (const Engine *e : Engine::Iterate()) {
		if (e->type != VehicleType::Train) continue;
		const auto &info = e->VehInfo<RailVehicleInfo>();
		if (info.railveh_type != RailVehicleType::Wagon && locomotive == EngineID::Invalid()) locomotive = e->index;
		if (info.railveh_type == RailVehicleType::Wagon && wagon == EngineID::Invalid()) wagon = e->index;
	}
	if (locomotive == EngineID::Invalid() && Engine::GetNumItems() > 0) locomotive = EngineID{0};
	if (wagon == EngineID::Invalid()) wagon = locomotive;

	/* 1. Spawn heavy mining freight train in the mining world */
	TileIndex gate_tile = INVALID_TILE;
	for (const auto &[pid, link] : PortalRegistry::GetAllPortals()) {
		if (link.end_a.world_id == mining_id) {
			gate_tile = link.end_a.tile;
			break;
		} else if (link.end_b.world_id == mining_id) {
			gate_tile = link.end_b.tile;
			break;
		}
	}

	int step_x = (gate_tile != INVALID_TILE && TileX(gate_tile) >= TileX(st_mining->xy)) ? 1 : -1;
	DiagDirection exit_dir = (step_x > 0) ? DiagDirection::SW : DiagDirection::NE;
	Direction dir = DiagDirToDir(exit_dir);
	Track track = Track::X;

	TileIndex spawn_tile = st_mining->xy;

	int x_fract = (step_x > 0) ? 4 : 10;
	int y_fract = 8;
	int eng_x = TileX(spawn_tile) * TILE_SIZE + x_fract;
	int eng_y = TileY(spawn_tile) * TILE_SIZE + y_fract;
	int eng_z = GetSlopePixelZ(eng_x, eng_y, true);
	int trail_dx = (step_x > 0) ? -8 : 8;

	if (Vehicle::CanAllocateItem(3)) {
		Train *engine = Vehicle::Create<Train>();
		Train *w1 = Vehicle::Create<Train>();
		Train *w2 = Vehicle::Create<Train>();

		if (engine != nullptr && w1 != nullptr && w2 != nullptr) {
			engine->SetFrontEngine();
			engine->SetEngine();
			w1->ClearFrontEngine();
			w1->SetWagon();
			w2->ClearFrontEngine();
			w2->SetWagon();

			engine->owner = human_company;
			w1->owner = human_company;
			w2->owner = human_company;

			engine->engine_type = locomotive;
			w1->engine_type = wagon;
			w2->engine_type = wagon;

			engine->build_year = TimerGameCalendar::Year{1950};
			w1->build_year = TimerGameCalendar::Year{1950};
			w2->build_year = TimerGameCalendar::Year{1950};

			engine->cur_speed = 0;
			w1->cur_speed = 0;
			w2->cur_speed = 0;

			engine->direction = dir;
			engine->track = track;
			engine->x_pos = eng_x;
			engine->y_pos = eng_y;
			engine->z_pos = eng_z;
			engine->tile = spawn_tile;

			w1->direction = dir;
			w1->track = track;
			w1->x_pos = eng_x + trail_dx;
			w1->y_pos = eng_y;
			w1->z_pos = GetSlopePixelZ(w1->x_pos, w1->y_pos, true);
			w1->tile = TileVirtXY(w1->x_pos, w1->y_pos);

			w2->direction = dir;
			w2->track = track;
			w2->x_pos = eng_x + 2 * trail_dx;
			w2->y_pos = eng_y;
			w2->z_pos = GetSlopePixelZ(w2->x_pos, w2->y_pos, true);
			w2->tile = TileVirtXY(w2->x_pos, w2->y_pos);

			for (Train *u : {engine, w1, w2}) {
				u->gcache.cached_veh_length = 8;
				u->sprite_cache.sprite_seq.Set(SPR_IMG_QUERY);
				u->compatible_railtypes = RailTypes{RAILTYPE_BEGIN};
				u->railtypes = RailTypes{RAILTYPE_BEGIN};
			}

			engine->SetNext(w1);
			w1->SetNext(w2);
			engine->ConsistChanged(CCF_ARRANGE);

			for (Train *u = engine; u != nullptr; u = u->Next()) {
				u->UpdatePositionAndViewport();
				if (IsRailStationTile(u->tile)) {
					SetRailStationReservation(u->tile, true);
				} else if (IsPlainRailTile(u->tile) && HasTrack(u->tile, track)) {
					TryReserveTrack(u->tile, track);
				}
			}

			CargoType ore = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
			w1->cargo_type = ore;
			w1->cargo_cap = 50;
			w2->cargo_type = ore;
			w2->cargo_cap = 50;

			/* Pre-seed 100 tons of Iron Ore waiting at st_mining so trains can immediately load */
			if (ore < NUM_CARGO) {
				GoodsEntry &ge = st_mining->goods[ore];
				StationID next = ge.GetVia(st_mining->index);
				Source source = (st_mining->town != nullptr) ? Source{st_mining->town->index, SourceType::Town} : Source{Source::Invalid, SourceType::Town};
				if (CargoPacket::CanAllocateItem()) {
					ge.GetOrCreateData().cargo.Append(CargoPacket::Create(st_mining->index, 100, source), next);
				}
			}

			/* Assign round trip orders between Mining and Core */
			ConsistMaterializer::AssignRoundTripOrders(engine, st_mining->index, mining_id, st_core->index, core_id);

			/* Activate vehicle state */
			engine->vehstatus.Reset(VehState::Stopped);
			engine->vehstatus.Reset(VehState::Hidden);

			result.trains_spawned++;
		}
	}

	return true;
}

ScenarioSynthesisResult PromptScenarioGenerator::SynthesizeAndSave(const PromptScenarioSpec &spec, const std::string &output_path)
{
	ScenarioSynthesisResult result;
	result.output_file = output_path;

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}
	if (_cursor.sprites.empty()) {
		SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
	}

	/* 1. Allocate fresh clean map and reset simulation pools */
	uint32_t map_size = 256;
	Map::Allocate(map_size, map_size);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}
	InitializeAnimatedTiles();
	extern TileIndex _cur_tileloop_tile;
	_cur_tileloop_tile = TileIndex{1};
	LinkGraphSchedule::Clear();
	PoolBase::Clean(PoolType::Normal);

	UpdateSignalsInBuffer();
	ProductionChainManager::Reset();
	StockpileManager::Reset();
	LogisticsHubManager::Reset();
	CorporateHQManager::Reset();
	TechTreeManager::Reset();
	FabricationManager::Reset();
	PlanetManager::Reset();
	PortalRegistry::Reset();
	SpaceportManager::Reset();
	EdgeConduitManager::Reset();

	RebuildStationKdtree();
	RebuildTownKdtree();
	RebuildViewportKdtree();

	if (Engine::GetNumItems() == 0 || _engine_mngr.mappings[VehicleType::Train].empty()) {
		_engine_mngr.ResetToDefaultMapping();
	}
	SetupEngines();
	StartupEngines();

	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	_settings_game.economy.multiple_industry_per_town = true;
	SetupCargoForClimate(LandscapeType::Temperate);
	ResetIndustries();
	BlueprintManager::Initialize();

	ProductionChainManager::InitDefaultRecipes();

	/* 2. Generate multi-world spatial partitioning */
	MultiWorldGen::Config gen_cfg;
	gen_cfg.world_count = spec.world_count;
	gen_cfg.place_gateways = true;

	if (!MultiWorldGen::GenerateMultiWorldLayout(Map::SizeX(), Map::SizeY(), gen_cfg)) {
		result.error_message = "MultiWorldGen layout partitioning failed";
		return result;
	}

	/* 3. Configure world metadata and styling */
	for (size_t i = 0; i < spec.worlds.size(); ++i) {
		const auto &w_spec = spec.worlds[i];
		WorldID wid{static_cast<uint32_t>(i)};

		PlanetManager::SetWorldPhase(wid, w_spec.phase);
		PlanetManager::SetWorldBiome(wid, w_spec.biome);

		const PlanetRegion *reg = PlanetManager::GetRegion(wid);
		if (reg != nullptr) {
			PlanetRegion *mut_reg = const_cast<PlanetRegion *>(reg);
			mut_reg->name = w_spec.name;
			MultiWorldGen::ApplyBiomeStyling(*mut_reg);
			result.worlds_created++;
		}
	}

	/* 4. Setup human Company 0 and rival Company 1 */
	Company *c0 = Company::GetIfValid(CompanyID{0});
	if (c0 == nullptr) {
		c0 = Company::CreateAtIndex(CompanyID{0});
	}
	if (c0 != nullptr) {
		c0->money = 100000000; // 100M Cr starting capital
		c0->name = "Commonwealth Interplanetary Transport";
		c0->avail_railtypes.Set(RAILTYPE_BEGIN);
		c0->avail_railtypes.Set(RAILTYPE_ELECTRIC);
		c0->avail_railtypes.Set(RAILTYPE_MONO);
		c0->avail_railtypes.Set(RAILTYPE_MAGLEV);
		TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {
			TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3,
			TECH_TRACTION_1, TECH_TRACTION_2, TECH_TRACTION_3,
			TECH_PORTAL_1, TECH_PORTAL_2
		});
		FabricationManager::SetFabricateFromStockpile(CompanyID{0}, true);
	}

	Company *c1 = Company::GetIfValid(CompanyID{1});
	if (c1 == nullptr) {
		c1 = Company::CreateAtIndex(CompanyID{1});
	}
	if (c1 != nullptr) {
		c1->name = "Consortium Heavy Industries";
		c1->avail_railtypes.Set(RAILTYPE_BEGIN);
		c1->avail_railtypes.Set(RAILTYPE_ELECTRIC);
		c1->avail_railtypes.Set(RAILTYPE_MONO);
		c1->avail_railtypes.Set(RAILTYPE_MAGLEV);
	}

	/* 5. Diplomatic Stance */
	CorporateAllianceManager::SetRelation(CompanyID{0}, CompanyID{1}, spec.rival_relation);

	/* 6. Populate settlements, megacity, and processing facilities */
	for (size_t i = 0; i < spec.worlds.size(); ++i) {
		const auto &w_spec = spec.worlds[i];
		const PlanetRegion *reg = PlanetManager::GetRegion(WorldID{static_cast<uint32_t>(i)});
		if (reg == nullptr) continue;

		uint32_t center_x = (reg->min_x + reg->max_x) / 2;
		uint32_t center_y = (reg->min_y + reg->max_y) / 2;
		TileIndex center_tile = TileXY(center_x, center_y);

		Town *town = ClosestTownFromTile(center_tile, UINT_MAX);
		if (town == nullptr && Town::CanAllocateItem()) {
			town = Town::Create(center_tile);
			if (town != nullptr) {
				town->townnametype = SPECSTR_TOWNNAME_START;
				RebuildTownKdtree();
			}
		}
		if (town != nullptr) {
			town->name = w_spec.name;
		}

		/* Core Megacity & Corporate HQ */
		if (w_spec.has_megacity && town != nullptr) {
			MegacityManager::RegisterMegacity(town->index, reg->id, w_spec.name, w_spec.population);
			uint32_t alloy_quota = 300;
			MegacityManager::SetCustomQuotas(town->index, 100, alloy_quota, 50);
		}

		if (w_spec.has_corporate_hq) {
			TileIndex hq_tile = TileXY(center_x + 6, center_y + 6);
			MakeClear(hq_tile, ClearGround::Grass, 0);
			CorporateHQManager::RegisterHQ(CompanyID{0}, reg->id, hq_tile, "Commonwealth Central HQ");
			CorporateHQManager::UpgradeHQTier(CompanyID{0});
		}

		/* Industrial Processing Facilities */
		if (w_spec.has_industrial_facility) {
			TileIndex fac_tile = TileXY(center_x - 6, center_y - 6);
			MakeClear(fac_tile, ClearGround::Grass, 0);
			ProductionChainManager::RegisterFacility(fac_tile, reg->id, w_spec.industrial_recipe, CompanyID{0}, 200);
			result.facilities_placed++;
		}

		/* Planetary Stockpiles & Strike Mechanics: seed all 6 fabrication roles in abundance */
		CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
		CargoType steel   = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
		CargoType wiring  = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);
		CargoType chips   = StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics);
		CargoType alloy   = StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy);
		CargoType comp    = StockpileManager::RoleToDefaultCargo(FabricationRole::Composites);

		StockpileManager::AddCargo(reg->id, CompanyID{0}, ballast, 10000);
		StockpileManager::AddCargo(reg->id, CompanyID{0}, steel,   10000);
		StockpileManager::AddCargo(reg->id, CompanyID{0}, wiring,  10000);
		StockpileManager::AddCargo(reg->id, CompanyID{0}, chips,   5000);
		StockpileManager::AddCargo(reg->id, CompanyID{0}, alloy,   5000);
		StockpileManager::AddCargo(reg->id, CompanyID{0}, comp,    5000);

		if (w_spec.is_striking) {
			/* Empty sustenance stockpile and configure urgent relief floor */
			PlanetManager::AddDevelopmentScore(reg->id, 50);
			TileIndex hub_tile = TileXY(center_x - 4, center_y + 4);
			uint32_t hub_id = LogisticsHubManager::RegisterHub(hub_tile, reg->id, CompanyID{0}, StationID::Invalid(), "Colony Emergency Relief Stockpile");
			if (hub_id != 0) {
				LogisticsHubManager::SetReserveFloor(hub_id, ballast, 500); // Demands urgent inbound deliveries
			}
		} else {
			PlanetManager::AddDevelopmentScore(reg->id, w_spec.phase == WorldPhase::Phase1_Core ? 25000 : 5000);
		}
	}

	/* 7. Build corridors, stations, and waypoints */
	if (spec.create_prebuilt_corridors) {
		BuildCorridorsAndStations(spec, result);
	}

	/* 8. Place canonical resource industries within station catchments */
	PlaceCanonicalIndustries(spec, result);

	/* 9. Spawn active fleets */
	if (spec.create_active_fleets) {
		SpawnActiveFleets(spec, result);
	}

	/* 9. Save scenario file */
	if (!output_path.empty()) {
		std::error_code ec;
		std::filesystem::path p(output_path);
		if (p.has_parent_path()) {
			std::filesystem::create_directories(p.parent_path(), ec);
		}
		LinkGraphSchedule::Clear();
		SaveLoadResult save_res = SaveOrLoad(output_path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
		if (save_res != SaveLoadResult::Ok) {
			result.error_message = "SaveOrLoad failed to write .sav file";
			return result;
		}
	}

	result.success = true;
	return result;
}

ScenarioSynthesisResult PromptScenarioGenerator::GenerateFromPrompt(const std::string &prompt_text, const std::string &output_path)
{
	PromptScenarioSpec spec = ParsePrompt(prompt_text);
	return SynthesizeAndSave(spec, output_path);
}

ScenarioSynthesisResult PromptScenarioGenerator::GenerateCommonwealthPrefabWorld(const std::string &output_path, uint32_t world_count)
{
	PromptScenarioSpec spec;
	spec.title = "Commonwealth Interplanetary UAT Matrix";
	spec.narrative_summary = "Verified multi-world logistics network connecting Sol Earth Core to Augusta Hub, Merredin Outpost, and Prometheus Caldera.";
	spec.world_count = std::clamp<uint32_t>(world_count, 3, 6);
	spec.rival_relation = CorporateRelation::Neutral;
	spec.rival_corporation = "Consortium Heavy Industries";
	spec.create_active_fleets = true;
	spec.create_prebuilt_corridors = true;

	spec.worlds.resize(spec.world_count);

	/* World 0: Core Earth */
	spec.worlds[0].id = WorldID{0};
	spec.worlds[0].name = "Sol Earth Core";
	spec.worlds[0].phase = WorldPhase::Phase1_Core;
	spec.worlds[0].biome = WorldBiome::Temperate;
	spec.worlds[0].role_description = "High-density metropolitan administrative nexus.";
	spec.worlds[0].population = 25000;
	spec.worlds[0].has_megacity = true;
	spec.worlds[0].has_corporate_hq = true;
	spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::Superalloys);
	spec.worlds[0].demanded_cargos.push_back(CommonwealthCargoID::SiliconChips);
	spec.worlds[0].supplied_cargos.push_back(CommonwealthCargoID::StructuralSteel);

	/* World 1: Developed Augusta */
	spec.worlds[1].id = WorldID{1};
	spec.worlds[1].name = "Augusta CST Hub";
	spec.worlds[1].phase = WorldPhase::Phase2_Developed;
	spec.worlds[1].biome = WorldBiome::SubTropic;
	spec.worlds[1].role_description = "Primary industrial fabrication and assembly nexus.";
	spec.worlds[1].population = 10000;
	spec.worlds[1].has_industrial_facility = true;
	spec.worlds[1].industrial_recipe = RECIPE_STEEL_SMELTING;
	spec.worlds[1].demanded_cargos.push_back(CommonwealthCargoID::IronOre);
	spec.worlds[1].demanded_cargos.push_back(CommonwealthCargoID::RareEarthMinerals);
	spec.worlds[1].supplied_cargos.push_back(CommonwealthCargoID::Superalloys);

	/* World 2: Frontier Merredin */
	spec.worlds[2].id = WorldID{2};
	spec.worlds[2].name = "Merredin Mining Colony";
	spec.worlds[2].phase = WorldPhase::Phase3_Frontier;
	spec.worlds[2].biome = WorldBiome::AridDesert;
	spec.worlds[2].role_description = "Deep-vein mineral extraction and rare earths mining.";
	spec.worlds[2].population = 3500;
	spec.worlds[2].is_striking = true;
	spec.worlds[2].strike_cause = "Life-Support and Water Scarcity";
	spec.worlds[2].supplied_cargos.push_back(CommonwealthCargoID::IronOre);
	spec.worlds[2].supplied_cargos.push_back(CommonwealthCargoID::RareEarthMinerals);

	if (spec.world_count >= 4) {
		/* World 3: Expansion Prometheus */
		spec.worlds[3].id = WorldID{3};
		spec.worlds[3].name = "Prometheus Caldera Outpost";
		spec.worlds[3].phase = WorldPhase::Phase4_Expansion;
		spec.worlds[3].biome = WorldBiome::Volcanic;
		spec.worlds[3].role_description = "High-energy plasma cracking and quantum crystal synthesis.";
		spec.worlds[3].population = 1200;
		spec.worlds[3].has_industrial_facility = true;
		spec.worlds[3].industrial_recipe = RECIPE_QUANTUM_ENRICHMENT;
		spec.worlds[3].supplied_cargos.push_back(CommonwealthCargoID::EnrichedQuantumCrystals);
	}

	for (size_t i = 4; i < spec.world_count; ++i) {
		spec.worlds[i].id = WorldID{static_cast<uint32_t>(i)};
		spec.worlds[i].name = fmt::format("Outer Territory Alpha-{}", i);
		spec.worlds[i].phase = WorldPhase::Phase3_Frontier;
		spec.worlds[i].biome = WorldBiome::SubArctic;
		spec.worlds[i].population = 2000;
	}

	auto result = SynthesizeAndSave(spec, output_path);
	if (!result.success) return result;

	/* Bind trade gateway on Sol Earth Core to off-world Augusta node */
	for (const auto &[pid, link] : PortalRegistry::GetAllPortals()) {
		if (link.end_a.world_id == WorldID{0}) {
			PrebuiltTradeManager::Instance().RegisterTradeGateway(link.end_a.tile, "world_augusta", WorldID{0}, 32);
			break;
		}
	}

	/* Re-save to persist newly registered trade gateways into TRAD chunk */
	if (!output_path.empty()) {
		LinkGraphSchedule::Clear();
		SaveOrLoad(output_path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	}

	return result;
}

bool PromptScenarioGenerator::VerifyCommonwealthUAT(std::string *error_msg)
{
	auto fail = [error_msg](std::string msg) {
		if (error_msg != nullptr) *error_msg = std::move(msg);
		return false;
	};

	if (PlanetManager::Count() < 3) {
		return fail(fmt::format("Expected >= 3 worlds, found {}", PlanetManager::Count()));
	}

	if (PortalRegistry::Count() < 2) {
		return fail(fmt::format("Expected >= 2 portal links, found {}", PortalRegistry::Count()));
	}

	const Company *c0 = Company::GetIfValid(CompanyID{0});
	if (c0 == nullptr) {
		return fail("Company 0 does not exist");
	}

	if (c0->money < 10000000) {
		return fail("Company 0 has insufficient capital for Commonwealth operations");
	}

	const auto *hq = CorporateHQManager::GetHQ(CompanyID{0});
	if (hq == nullptr) {
		return fail("Corporate HQ not registered for Company 0");
	}

	if (MegacityManager::GetAllMegacities().empty()) {
		return fail("No Megacity registered");
	}

	if (PrebuiltTradeManager::Instance().GetAllTradeGateways().empty()) {
		return fail("No Prebuilt Trade Gateways registered");
	}

	/* Verify canonical resource industries exist */
	bool has_iron_mine = false;
	bool has_steel_mill = false;
	for (const Industry *ind : Industry::Iterate()) {
		if (ind->type == IT_IRON_MINE) has_iron_mine = true;
		if (ind->type == IT_STEEL_MILL) has_steel_mill = true;
	}
	if (!has_iron_mine) {
		return fail("Missing canonical Iron Ore Mine on extraction world");
	}
	if (!has_steel_mill) {
		return fail("Missing canonical Steel Mill on industrial world");
	}

	/* Verify station catchment contains at least one nearby industry */
	bool any_station_has_industry = false;
	for (const Station *st : Station::Iterate()) {
		if (st->owner == CompanyID{0} && !st->industries_near.empty()) {
			any_station_has_industry = true;
			break;
		}
	}
	if (!any_station_has_industry) {
		for (const Industry *ind : Industry::Iterate()) {
			if (!ind->stations_near.empty()) {
				any_station_has_industry = true;
				break;
			}
		}
	}
	if (!any_station_has_industry) {
		return fail("No station has resource industries within its catchment area");
	}

	/* Verify train depots exist for fleet maintenance and purchase */
	if (Depot::GetNumItems() == 0) {
		return fail("No train depots found on any colonized world");
	}

	/* Verify company planetary stockpiles have sufficient fabrication materials */
	CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	CargoType steel   = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	CargoType wiring  = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);
	CargoType chips   = StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics);
	CargoType alloy   = StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy);

	for (const PlanetRegion &reg : PlanetManager::GetAllRegions()) {
		WorldID wid = reg.id;
		if (StockpileManager::GetStock(wid, CompanyID{0}, ballast) < 500 ||
				StockpileManager::GetStock(wid, CompanyID{0}, steel) < 500 ||
				StockpileManager::GetStock(wid, CompanyID{0}, wiring) < 500 ||
				StockpileManager::GetStock(wid, CompanyID{0}, chips) < 250 ||
				StockpileManager::GetStock(wid, CompanyID{0}, alloy) < 250) {
			return fail(fmt::format("World {} has insufficient stockpiles for prefab fabrication", wid.base()));
		}
	}

	/* Verify essential Commonwealth research is unlocked */
	if (!TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_1) ||
			!TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_2) ||
			!TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_PORTAL_1)) {
		return fail("Company 0 lacks foundational Commonwealth research (materials/portals)");
	}

	size_t train_count = 0;
	for (const Train *t : Train::Iterate()) {
		if (t->IsFrontEngine()) {
			train_count++;
			if (t->vehstatus.Test(VehState::Crashed)) {
				return fail(fmt::format("Train {} is crashed", t->index.base()));
			}
		}
	}

	if (train_count == 0) {
		return fail("No active train consists found");
	}

	return true;
}
