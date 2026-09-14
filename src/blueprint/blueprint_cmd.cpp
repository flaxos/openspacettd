/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint_cmd.cpp Implementation of deterministic blueprint placement command. */

#include "../stdafx.h"
#include "blueprint_cmd.h"
#include "blueprint.h"
#include "../rail_cmd.h"
#include "../station_cmd.h"
#include "../landscape_cmd.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../table/strings.h"
#include "../viewport_func.h"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../company_func.h"
#include "../command_func.h"

CommandCost CmdPlaceBlueprint(DoCommandFlags flags, TileIndex origin_tile, const std::string &blueprint_json, RailType railtype_override, [[maybe_unused]] bool clear_first)
{
	if (!IsValidTile(origin_tile)) return CMD_ERROR;

	auto bp_opt = Blueprint::FromJson(blueprint_json);
	if (!bp_opt.has_value() || !bp_opt->IsValid()) {
		return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
	}
	const Blueprint &bp = *bp_opt;

	/* Validate footprint bounding box and boundaries */
	for (const auto &tile : bp.tiles) {
		TileIndex target_tile = TileAddWrap(origin_tile, tile.dx, tile.dy);
		if (target_tile == INVALID_TILE || !IsValidTile(target_tile)) {
			return CommandCost(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
		}
		if (PlanetManager::Count() > 0) {
			CommandCost check = PlanetManager::CheckConstructionPlacement(target_tile);
			if (check.Failed()) return check;
		}
		if (HasTileWaterGround(target_tile)) {
			return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);
		}
		if (PortalRegistry::IsPortalTile(target_tile) || PortalRegistry::IsUnlinkedGate(target_tile)) {
			return CommandCost(STR_ERROR_ALREADY_BUILT);
		}
	}

	CommandCost total_cost(ExpensesType::Construction);
	DoCommandFlags test_flags = DoCommandFlags{flags}.Set(DoCommandFlag::Auto).Reset(DoCommandFlag::Execute);

	/* Dry-run validation and cost query phase */
	if (!flags.Test(DoCommandFlag::Execute)) {
		for (const auto &tile : bp.tiles) {
			TileIndex target_tile = TileAddWrap(origin_tile, tile.dx, tile.dy);
			RailType rt = (railtype_override != INVALID_RAILTYPE && ValParamRailType(railtype_override)) ? railtype_override : tile.railtype;
			if (!ValParamRailType(rt)) rt = RAILTYPE_BEGIN;

			if (IsTileType(target_tile, TileType::Railway)) {
				CommandCost own = CheckTileOwnership(target_tile);
				if (own.Failed()) return own;

				if (tile.type == BlueprintTileType::Track) {
					for (Track track : tile.trackbits) {
						if (!HasTrack(target_tile, track)) {
							total_cost.AddCost(RailBuildCost(rt));
						}
					}
					for (const auto &sig : tile.signals) {
						if (!HasSignalOnTrack(target_tile, sig.track)) {
							total_cost.AddCost(_price[Price::BuildSignals]);
						}
					}
				} else if (tile.type == BlueprintTileType::Depot) {
					if (!IsRailDepot(target_tile) || GetRailDepotDirection(target_tile) != tile.dir) {
						total_cost.AddCost(_price[Price::BuildDepotTrain]);
					}
				}
			} else if (IsRailStationTile(target_tile)) {
				CommandCost own = CheckTileOwnership(target_tile);
				if (own.Failed()) return own;

				if (tile.type == BlueprintTileType::Station) {
					if (GetRailStationAxis(target_tile) != tile.axis) {
						total_cost.AddCost(_price[Price::BuildStationRail] + RailBuildCost(rt));
					}
				}
			} else {
				CommandCost clear = CmdLandscapeClear(test_flags, target_tile);
				if (clear.Failed()) return clear;
				total_cost.AddCost(clear.GetCost());

				if (tile.type == BlueprintTileType::Track) {
					total_cost.AddCost(RailBuildCost(rt) * tile.trackbits.Count());
					total_cost.AddCost(_price[Price::BuildSignals] * static_cast<uint>(tile.signals.size()));
				} else if (tile.type == BlueprintTileType::Depot) {
					total_cost.AddCost(_price[Price::BuildDepotTrain]);
				} else if (tile.type == BlueprintTileType::Station) {
					total_cost.AddCost(_price[Price::BuildStationRail] + RailBuildCost(rt));
				}
			}
		}
		return total_cost;
	}

	/* Execution phase - Pass 1: Build tracks, depots, and station platforms */
	for (const auto &tile : bp.tiles) {
		TileIndex target_tile = TileAddWrap(origin_tile, tile.dx, tile.dy);
		RailType rt = (railtype_override != INVALID_RAILTYPE && ValParamRailType(railtype_override)) ? railtype_override : tile.railtype;
		if (!ValParamRailType(rt)) rt = RAILTYPE_BEGIN;

		if (tile.type == BlueprintTileType::Track) {
			for (Track track : tile.trackbits) {
				if (!IsPlainRailTile(target_tile) || !HasTrack(target_tile, track)) {
					CommandCost res = CmdBuildSingleRail(flags, target_tile, rt, track, false);
					if (res.Succeeded()) total_cost.AddCost(res.GetCost());
				}
			}
		} else if (tile.type == BlueprintTileType::Depot) {
			if (!IsRailDepotTile(target_tile) || GetRailDepotDirection(target_tile) != tile.dir) {
				CommandCost res = CmdBuildTrainDepot(flags, target_tile, rt, tile.dir);
				if (res.Succeeded()) total_cost.AddCost(res.GetCost());
			}
		} else if (tile.type == BlueprintTileType::Station) {
			if (!IsRailStationTile(target_tile) || GetRailStationAxis(target_tile) != tile.axis) {
				CommandCost res = CmdBuildRailStation(flags, target_tile, rt, tile.axis, 1, 1, tile.spec_class, tile.spec_index, StationID::Invalid(), true);
				if (res.Succeeded()) total_cost.AddCost(res.GetCost());
			}
		}
	}

	/* Execution phase - Pass 2: Attach signals to existing tracks */
	for (const auto &tile : bp.tiles) {
		if (tile.type != BlueprintTileType::Track || tile.signals.empty()) continue;
		TileIndex target_tile = TileAddWrap(origin_tile, tile.dx, tile.dy);
		if (!IsPlainRailTile(target_tile)) continue;

		for (const auto &sig : tile.signals) {
			uint8_t cur_sig = HasSignalOnTrack(target_tile, sig.track) ? static_cast<uint8_t>(GetPresentSignals(target_tile) & SignalOnTrack(sig.track)) : 0;
			if (cur_sig != sig.signals_copy || GetSignalType(target_tile, sig.track) != sig.sigtype || GetSignalVariant(target_tile, sig.track) != sig.sigvar) {
				CommandCost res = CmdBuildSingleSignal(flags, target_tile, sig.track, sig.sigtype, sig.sigvar, false, false, false, SignalType::Block, SignalType::Block, 0, sig.signals_copy);
				if (res.Succeeded()) total_cost.AddCost(res.GetCost());
			}
		}
	}

	/* Execution phase - Pass 3: Dirty tiles and inform pathfinder */
	for (const auto &tile : bp.tiles) {
		TileIndex target_tile = TileAddWrap(origin_tile, tile.dx, tile.dy);
		MarkTileDirtyByTile(target_tile);
		if (tile.type == BlueprintTileType::Track) {
			for (Track track : tile.trackbits) {
				YapfNotifyTrackLayoutChange(target_tile, track);
			}
		}
	}

	return total_cost;
}
