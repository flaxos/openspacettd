/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint_cmd.cpp Deterministic, preflighted blueprint placement. */

#include "../stdafx.h"
#include "blueprint_cmd.h"
#include "blueprint.h"
#include "../rail_cmd.h"
#include "../station_cmd.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../depot_base.h"
#include "../station_base.h"
#include "../rail.h"
#include "../town.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/fabrication_manager.h"
#include "../portal/company_stockpile.h"
#include "../table/strings.h"

#include <map>
#include <vector>

namespace {

enum class PlacementKind : uint8_t { Station, Track, Depot, Signal };

struct PlacementOp {
	PlacementKind kind;
	TileIndex tile;
	RailType railtype = RAILTYPE_BEGIN;
	Track track = Track::X;
	DiagDirection dir = DiagDirection::NE;
	Axis axis = Axis::X;
	uint8_t numtracks = 1;
	uint8_t plat_len = 1;
	StationClassID spec_class = STAT_CLASS_DFLT;
	uint16_t spec_index = 0;
	BlueprintSignal signal{};
};

struct TownRatingQueryScope {
	TownRatingQueryScope() { SetTownRatingTestMode(true); }
	~TownRatingQueryScope() { SetTownRatingTestMode(false); }
};

using Coord = std::pair<int16_t, int16_t>;
using RequiredMaterials = std::map<WorldID, std::map<CargoType, uint64_t>>;

RailType ResolveRailType(const BlueprintTile &tile, RailType override_type)
{
	return override_type == INVALID_RAILTYPE ? tile.railtype : override_type;
}

void AddBOM(RequiredMaterials &required, WorldID world, const BillOfMaterials &bom)
{
	for (const auto &[cargo, amount] : bom.materials) required[world][cargo] += amount;
}

CommandCost CheckAggregateMaterials(const RequiredMaterials &required)
{
	for (const auto &[world, cargos] : required) {
		for (const auto &[cargo, amount] : cargos) {
			if (StockpileManager::GetStock(world, _current_company, cargo) < amount) {
				return CommandCost(STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS);
			}
		}
	}
	return CommandCost();
}

CommandCost RunPlacementOp(const PlacementOp &op, DoCommandFlags flags)
{
	switch (op.kind) {
		case PlacementKind::Station:
			return CmdBuildRailStation(flags, op.tile, op.railtype, op.axis, op.numtracks, op.plat_len,
					op.spec_class, op.spec_index, NEW_STATION, true);
		case PlacementKind::Track:
			return CmdBuildSingleRail(flags, op.tile, op.railtype, op.track, false);
		case PlacementKind::Depot:
			return CmdBuildTrainDepot(flags, op.tile, op.railtype, op.dir);
		case PlacementKind::Signal:
			return CmdBuildSingleSignal(flags, op.tile, op.signal.track, op.signal.sigtype, op.signal.sigvar,
					false, false, false, SignalType::Block, SignalType::Block, 0, op.signal.signals_copy);
	}
	return CMD_ERROR;
}

bool SameStationCell(const BlueprintTile &a, const BlueprintTile &b, RailType override_type)
{
	return a.type == BlueprintTileType::Station && b.type == BlueprintTileType::Station &&
			ResolveRailType(a, override_type) == ResolveRailType(b, override_type) &&
			a.axis == b.axis && a.spec_class == b.spec_class && a.spec_index == b.spec_index;
}

} // namespace

CommandCost CmdPlaceBlueprint(DoCommandFlags flags, TileIndex origin_tile, const std::string &blueprint_json, RailType railtype_override, [[maybe_unused]] bool clear_first)
{
	if (!IsValidTile(origin_tile)) return CMD_ERROR;
	if (railtype_override != INVALID_RAILTYPE && !ValParamRailType(railtype_override)) return CMD_ERROR;

	auto bp_opt = Blueprint::FromJson(blueprint_json);
	if (!bp_opt.has_value() || !bp_opt->IsValid()) return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
	const Blueprint &bp = *bp_opt;

	std::map<Coord, size_t> cells;
	std::vector<TileIndex> targets(bp.tiles.size());
	for (size_t i = 0; i < bp.tiles.size(); ++i) {
		const BlueprintTile &cell = bp.tiles[i];
		if (!cells.emplace(Coord{cell.dx, cell.dy}, i).second) return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
		TileIndex target = TileAddWrap(origin_tile, cell.dx, cell.dy);
		if (target == INVALID_TILE || !IsValidTile(target)) return CommandCost(STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP);
		targets[i] = target;
		if (!ValParamRailType(ResolveRailType(cell, railtype_override))) return CMD_ERROR;
		if (PlanetManager::Count() > 0) {
			CommandCost check = PlanetManager::CheckConstructionPlacement(target);
			if (check.Failed()) return check;
		}
		if (HasTileWaterGround(target)) return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);
		if (PortalRegistry::IsPortalTile(target) || PortalRegistry::IsUnlinkedGate(target)) return CommandCost(STR_ERROR_ALREADY_BUILT);
	}

	DoCommandFlags child_flags = DoCommandFlags{flags}.Set(DoCommandFlag::Auto).Set(DoCommandFlag::NoWater);
	DoCommandFlags test_flags = DoCommandFlags{child_flags}.Reset(DoCommandFlag::Execute);
	CommandCost quote(ExpensesType::Construction);
	std::vector<PlacementOp> station_ops, build_ops, signal_ops;
	RequiredMaterials required;
	size_t new_depots = 0, station_groups = 0;

	/* Repeat the identical preflight in both phases. Tree-clearing town ratings
	 * accumulate only in the query ledger until all children are validated. */
	{
		TownRatingQueryScope rating_scope;
		std::vector<bool> station_seen(bp.tiles.size(), false);
		for (size_t i = 0; i < bp.tiles.size(); ++i) {
			const BlueprintTile &first = bp.tiles[i];
			if (first.type != BlueprintTileType::Station || station_seen[i]) continue;
			if (first.spec_class != STAT_CLASS_DFLT || first.spec_index != 0) return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
			std::vector<size_t> component, pending{i};
			station_seen[i] = true;
			int min_x = first.dx, max_x = first.dx, min_y = first.dy, max_y = first.dy;
			while (!pending.empty()) {
				size_t index = pending.back();
				pending.pop_back();
				component.push_back(index);
				const BlueprintTile &cell = bp.tiles[index];
				min_x = std::min(min_x, static_cast<int>(cell.dx));
				max_x = std::max(max_x, static_cast<int>(cell.dx));
				min_y = std::min(min_y, static_cast<int>(cell.dy));
				max_y = std::max(max_y, static_cast<int>(cell.dy));
				for (Coord neighbour : {Coord{static_cast<int16_t>(cell.dx - 1), cell.dy},
						Coord{static_cast<int16_t>(cell.dx + 1), cell.dy},
						Coord{cell.dx, static_cast<int16_t>(cell.dy - 1)},
						Coord{cell.dx, static_cast<int16_t>(cell.dy + 1)}}) {
					auto it = cells.find(neighbour);
					if (it == cells.end() || station_seen[it->second] || !SameStationCell(first, bp.tiles[it->second], railtype_override)) continue;
					station_seen[it->second] = true;
					pending.push_back(it->second);
				}
			}
			int width = max_x - min_x + 1, height = max_y - min_y + 1;
			if (component.size() != static_cast<size_t>(width * height) || width > UINT8_MAX || height > UINT8_MAX) {
				return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
			}
			bool any_existing = false, all_existing = true;
			StationID existing_id = StationID::Invalid();
			for (int y = min_y; y <= max_y; ++y) {
				for (int x = min_x; x <= max_x; ++x) {
					auto it = cells.find({static_cast<int16_t>(x), static_cast<int16_t>(y)});
					if (it == cells.end() || !SameStationCell(first, bp.tiles[it->second], railtype_override)) return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
					TileIndex target = targets[it->second];
					/* Authority checks read real ratings. Build all station groups
					 * before ordinary tree-clearing rail and depot operations. */
					if (IsRailStationTile(target)) {
						any_existing = true;
						CommandCost own = CheckTileOwnership(target);
						if (own.Failed()) return own;
						if (GetRailStationAxis(target) != first.axis || GetRailType(target) != ResolveRailType(first, railtype_override) ||
								GetCustomStationSpecIndex(target) != 0) {
							return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
						}
						if (existing_id == StationID::Invalid()) existing_id = GetStationIndex(target);
						if (existing_id != GetStationIndex(target)) return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
					} else if (!IsTileType(target, TileType::Clear)) {
						return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
					} else {
						all_existing = false;
					}
				}
			}
			if (any_existing && !all_existing) return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
			if (all_existing) continue;
			PlacementOp op{PlacementKind::Station, TileAddWrap(origin_tile, min_x, min_y)};
			op.railtype = ResolveRailType(first, railtype_override);
			op.axis = first.axis;
			op.plat_len = first.axis == Axis::X ? width : height;
			op.numtracks = first.axis == Axis::X ? height : width;
			op.spec_class = first.spec_class;
			op.spec_index = first.spec_index;
			CommandCost child = RunPlacementOp(op, test_flags);
			if (child.Failed()) return child;
			quote.AddCost(child.GetCost());
			station_ops.push_back(op);
			station_groups++;
		}
		if (station_groups > 1) return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
		if (!Station::CanAllocateItem(station_groups)) return CommandCost(STR_ERROR_TOO_MANY_STATIONS_LOADING);

		/* Track bits advance on each tile. The first piece on bare land takes
		 * the complete child quote, including terrain and foundation cost. */
		for (size_t i = 0; i < bp.tiles.size(); ++i) {
			const BlueprintTile &cell = bp.tiles[i];
			if (cell.type == BlueprintTileType::Station) continue;
			TileIndex target = targets[i];
			RailType rt = ResolveRailType(cell, railtype_override);
			WorldID world = PlanetManager::GetTileWorld(target);
			bool fabrication = world != INVALID_WORLD && FabricationManager::IsFabricateFromStockpileEnabled(_current_company);
			if (cell.type == BlueprintTileType::Track) {
				if (cell.trackbits.None() || TrackBits{cell.trackbits}.Reset(TRACK_BIT_ALL).Any()) return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
				bool original_rail = IsPlainRailTile(target);
				if (original_rail) {
					CommandCost own = CheckTileOwnership(target);
					if (own.Failed()) return own;
					if (GetRailType(target) != rt) return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
				} else if (!IsTileType(target, TileType::Clear) && !IsTileType(target, TileType::Trees)) {
					return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
				}
				TrackBits prospective = original_rail ? GetTrackBits(target) : TrackBits{};
				bool first_new = true;
				for (Track track : cell.trackbits) {
					if (prospective.Test(track)) continue;
					PlacementOp op{PlacementKind::Track, target};
					op.railtype = rt;
					op.track = track;
					CommandCost slope = CheckRailSlope(GetTileSlope(target), track, prospective, target);
					if (slope.Failed()) return slope;
					if (original_rail && HasSignals(target) && TracksOverlap(prospective | track)) {
						return CommandCost(STR_ERROR_MUST_REMOVE_SIGNALS_FIRST);
					}
					if (first_new || original_rail) {
						CommandCost child = RunPlacementOp(op, test_flags);
						if (child.Failed()) return child;
						if (first_new && !original_rail) quote.AddCost(child.GetCost());
					}
					if (original_rail || !first_new) {
						quote.AddCost(slope.GetCost());
						quote.AddCost(GetNewRailTrackCost(rt, world, _current_company));
					}
					if (fabrication) AddBOM(required, world, FabricationManager::GetTrackBOM(rt));
					prospective.Set(track);
					build_ops.push_back(op);
					first_new = false;
				}
				if (TracksOverlap(prospective) && !cell.signals.empty()) return CommandCost(STR_ERROR_NO_SUITABLE_RAILROAD_TRACK);
			} else if (cell.type == BlueprintTileType::Depot) {
				if (IsRailDepotTile(target)) {
					CommandCost own = CheckTileOwnership(target);
					if (own.Failed()) return own;
					if (GetRailDepotDirection(target) != cell.dir || GetRailType(target) != rt) {
						return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
					}
					continue;
				}
				if (!IsTileType(target, TileType::Clear) && !IsTileType(target, TileType::Trees)) {
					return CommandCost(STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
				}
				PlacementOp op{PlacementKind::Depot, target};
				op.railtype = rt;
				op.dir = cell.dir;
				CommandCost child = RunPlacementOp(op, test_flags);
				if (child.Failed()) return child;
				quote.AddCost(child.GetCost());
				build_ops.push_back(op);
				new_depots++;
				if (fabrication) AddBOM(required, world, FabricationManager::GetDepotBOM(rt));
			} else {
				return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
			}
		}
		if (!Depot::CanAllocateItem(new_depots)) return CMD_ERROR;

		for (size_t i = 0; i < bp.tiles.size(); ++i) {
			const BlueprintTile &cell = bp.tiles[i];
			if (cell.type != BlueprintTileType::Track || cell.signals.empty()) continue;
			TileIndex target = targets[i];
			WorldID world = PlanetManager::GetTileWorld(target);
			bool fabrication = world != INVALID_WORLD && FabricationManager::IsFabricateFromStockpileEnabled(_current_company);
			TrackBits future = IsPlainRailTile(target) ? GetTrackBits(target) : TrackBits{};
			future.Set(cell.trackbits);
			if (TracksOverlap(future)) return CommandCost(STR_ERROR_NO_SUITABLE_RAILROAD_TRACK);
			TrackBits signalled{};
			for (const BlueprintSignal &signal : cell.signals) {
				if (!IsValidTrack(signal.track) || !future.Test(signal.track) || signalled.Test(signal.track) ||
						signal.sigtype >= SignalType::End || signal.sigvar >= SignalVariant::End ||
						signal.signals_copy == 0 || (signal.signals_copy & SignalOnTrack(signal.track)) == 0 ||
						(signal.signals_copy & ~SignalOnTrack(signal.track)) != 0) {
					return CommandCost(STR_ERROR_BLUEPRINT_INVALID);
				}
				signalled.Set(signal.track);
				bool existing = IsPlainRailTile(target) && HasSignalOnTrack(target, signal.track);
				if (existing && (GetPresentSignals(target) & SignalOnTrack(signal.track)) == signal.signals_copy &&
						GetSignalType(target, signal.track) == signal.sigtype && GetSignalVariant(target, signal.track) == signal.sigvar) continue;
				PlacementOp op{PlacementKind::Signal, target};
				op.signal = signal;
				if (IsPlainRailTile(target) && HasTrack(target, signal.track)) {
					CommandCost child = RunPlacementOp(op, test_flags);
					if (child.Failed()) return child;
					quote.AddCost(child.GetCost());
				} else {
					if (fabrication && !FabricationManager::CanFabricateSignal(world, _current_company)) {
						return CommandCost(STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS);
					}
					quote.AddCost(GetNewRailSignalCost(world, _current_company));
				}
				if (!existing && fabrication) AddBOM(required, world, FabricationManager::GetSignalBOM());
				signal_ops.push_back(op);
			}
		}
		CommandCost materials = CheckAggregateMaterials(required);
		if (materials.Failed()) return materials;
	}

	if (!flags.Test(DoCommandFlag::Execute)) return quote;
	CommandCost available = quote;
	if (!CheckCompanyHasMoney(available)) return available;

	CommandCost actual(ExpensesType::Construction);
	for (const auto *ops : {&station_ops, &build_ops, &signal_ops}) {
		for (const PlacementOp &op : *ops) {
			CommandCost child = RunPlacementOp(op, child_flags);
			if (child.Failed()) return child; // all deterministic child failures must be excluded by preflight
			actual.AddCost(child.GetCost());
		}
	}
	return actual;
}
