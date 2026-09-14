/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_cmd.cpp Implementation of gateway portal construction, linking, and demolition commands. */

#include "../stdafx.h"
#include "portal_cmd.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "planet_manager.h"
#include "spaceport_manager.h"
#include "edge_conduit.h"
#include "universe_authority.h"
#include "megacity_manager.h"
#include "company_stockpile.h"
#include "logistics_hub.h"
#include "corporate_hq.h"
#include "fabrication_manager.h"
#include "tech_tree.h"
#include "../town.h"
#include "../station_base.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_gui.h"
#include "../landscape.h"
#include "../landscape_cmd.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../tunnelbridge.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../vehicle_func.h"
#include "../economy_func.h"
#include "../strings_func.h"
#include "../language.h"
#include "../map_func.h"
#include "../pbs.h"
#include "../train.h"

#include "../table/strings.h"
#include "../news_func.h"
#include "../3rdparty/fmt/format.h"
#include "../safeguards.h"

static TileIndex GetPortalAdjacentTile(TileIndex tile, DiagDirection dir)
{
	if (tile >= Map::Size() || !IsValidDiagDirection(dir)) return INVALID_TILE;

	int x = static_cast<int>(TileX(tile));
	int y = static_cast<int>(TileY(tile));
	switch (dir) {
		case DiagDirection::NE: --x; break;
		case DiagDirection::SE: ++y; break;
		case DiagDirection::SW: ++x; break;
		case DiagDirection::NW: --y; break;
		default: return INVALID_TILE;
	}
	if (x < 0 || y < 0 || x > static_cast<int>(Map::MaxX()) || y > static_cast<int>(Map::MaxY())) return INVALID_TILE;
	return TileXY(x, y);
}

static CommandCost ValidatePortalGateFootprint(TileIndex tile, DiagDirection dir, WorldID world_id)
{
	TileIndex portal_side = GetPortalAdjacentTile(tile, dir);
	TileIndex approach = GetPortalAdjacentTile(tile, ReverseDiagDir(dir));
	if (portal_side == INVALID_TILE) return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	if (!IsValidTile(approach) || PlanetManager::GetTileWorld(approach) != world_id) {
		return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}
	return CommandCost();
}

/**
 * Validate and price a complete Portal Gate terminal without modifying the map.
 *
 * PortalTerminal::Build deliberately contains no fallible operations. Keeping
 * every landscape check here makes direct command execution just as atomic as
 * the normal query-then-execute command route.
 */
static CommandCost ValidatePortalTerminal(DoCommandFlags flags, const PortalTerminalLayout &layout, RailType railtype)
{
	CommandCost cost(ExpensesType::Construction);
	if (GetTileSlope(layout.gate_tile) != SLOPE_FLAT) return CommandCost(STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);
	uint gate_height = TileHeight(layout.gate_tile);
	DoCommandFlags test_flags = DoCommandFlags{flags}.Set(DoCommandFlag::Auto).Reset(DoCommandFlag::Execute);

	for (const PortalTerminalTile &part : layout.tiles) {
		if (!IsValidTile(part.tile) || !IsInnerTile(part.tile) ||
				PlanetManager::GetTileWorld(part.tile) != layout.world_id ||
				GetTileSlope(part.tile) != SLOPE_FLAT || TileHeight(part.tile) != gate_height ||
				PortalRegistry::IsPortalTile(part.tile) || PortalRegistry::IsUnlinkedGate(part.tile) ||
				EdgeConduitManager::IsConduitTile(part.tile)) {
			return CommandCost(STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);
		}

		CommandCost clear = Command<Commands::LandscapeClear>::Do(test_flags, part.tile);
		if (clear.Failed()) return CommandCost(STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);
		cost.AddCost(clear.GetCost());
	}

	cost.AddCost(RailBuildCost(railtype) * layout.GetTrackPieceCount());
	cost.AddCost(_price[Price::BuildSignals] * static_cast<uint>(layout.signals.size()));
	return cost;
}

/** Clear a terminal whose entire footprint has already passed preflight. */
static void ClearPortalTerminal(DoCommandFlags flags, const PortalTerminalLayout &layout)
{
	DoCommandFlags execute_flags = DoCommandFlags{flags}.Set({DoCommandFlag::Auto, DoCommandFlag::Execute});
	for (const PortalTerminalTile &part : layout.tiles) {
		CommandCost clear = Command<Commands::LandscapeClear>::Do(execute_flags, part.tile);
		assert(clear.Succeeded());
	}
}

CommandCost CmdBuildPortalGate(DoCommandFlags flags, TileIndex tile, DiagDirection dir, RailType railtype)
{
	CommandCost placement = PlanetManager::CheckConstructionPlacement(tile);
	if (placement.Failed()) return placement;
	if (!ValParamRailType(railtype)) return CMD_ERROR;
	if (!IsValidDiagDirection(dir)) {
		/* If direction is invalid, attempt to infer from slope */
		auto [tileh, z] = GetTileSlopeZ(tile);
		dir = GetInclinedSlopeDirection(tileh);
		if (!IsValidDiagDirection(dir)) return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	CompanyID company = _current_company;
	if (!Company::IsValidID(company) && company != OWNER_DEITY) return CMD_ERROR;

	/* Verify tile is within a valid logical planetary world */
	WorldID world_id = PlanetManager::GetTileWorld(tile);
	assert(world_id != INVALID_WORLD);
	placement = ValidatePortalGateFootprint(tile, dir, world_id);
	if (placement.Failed()) return placement;
	std::optional<PortalTerminalLayout> terminal = PortalTerminal::Plan(tile, dir, world_id);
	if (!terminal.has_value()) return CommandCost(STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);

	/* Cannot build on an already existing portal gate */
	if (PortalRegistry::IsPortalTile(tile) || PortalRegistry::IsUnlinkedGate(tile)) {
		return CommandCost(STR_ERROR_ALREADY_BUILT);
	}

	if (HasTileWaterGround(tile)) return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);

	/* Preflight the head and every approach tile before modifying any state. */
	DoCommandFlags test_flags = DoCommandFlags{flags}.Set(DoCommandFlag::Auto).Reset(DoCommandFlag::Execute);
	CommandCost ret = Command<Commands::LandscapeClear>::Do(test_flags, tile);
	if (ret.Failed()) return ret;
	CommandCost terminal_cost = ValidatePortalTerminal(flags, *terminal, railtype);
	if (terminal_cost.Failed()) return terminal_cost;

	CommandCost cost(ret);
	cost.AddCost(terminal_cost.GetCost());
	/* Base gateway construction cost (capital-intensive infrastructure) */
	cost.AddCost(_price[Price::BuildTunnel] * 5);
	cost.AddCost(RailBuildCost(railtype));

	if (flags.Test(DoCommandFlag::Execute)) {
		CommandCost clear_head = Command<Commands::LandscapeClear>::Do(DoCommandFlags{flags}.Set({DoCommandFlag::Auto, DoCommandFlag::Execute}), tile);
		assert(clear_head.Succeeded());
		ClearPortalTerminal(flags, *terminal);
		PortalTerminal::Build(*terminal, railtype, company);

		Company *c = Company::GetIfValid(company);
		if (c != nullptr) c->infrastructure.rail[railtype] += TUNNELBRIDGE_TRACKBIT_FACTOR;

		MakeRailTunnel(tile, company, dir, railtype);
		AddSideToSignalBuffer(tile, DiagDirection::Invalid, company);
		YapfNotifyTrackLayoutChange(tile, DiagDirToDiagTrack(dir));
		DirtyCompanyInfrastructureWindows(company);

		PortalRegistry::RegisterUnlinkedGate(tile, dir, world_id);
	}

	return cost;
}

CommandCost CmdLinkPortalGates(DoCommandFlags flags, TileIndex tile_a, TileIndex tile_b)
{
	if (!IsValidTile(tile_a) || !IsValidTile(tile_b) || tile_a == tile_b) return CMD_ERROR;

	/* Both must be registered unlinked gates */
	if (!PortalRegistry::IsUnlinkedGate(tile_a) || !PortalRegistry::IsUnlinkedGate(tile_b)) {
		return CommandCost(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE);
	}

	/* Verify ownership */
	CommandCost ret_own_a = CheckOwnership(GetTileOwner(tile_a));
	if (ret_own_a.Failed()) return ret_own_a;
	CommandCost ret_own_b = CheckOwnership(GetTileOwner(tile_b));
	if (ret_own_b.Failed()) return ret_own_b;

	/* Rail types must match */
	RailType rt_a = GetRailType(tile_a);
	RailType rt_b = GetRailType(tile_b);
	if (rt_a != rt_b) return CommandCost(STR_ERROR_INCOMPATIBLE_RAIL_TYPES);

	/* Gates must be on distinct, valid worlds */
	WorldID world_a = PlanetManager::GetTileWorld(tile_a);
	WorldID world_b = PlanetManager::GetTileWorld(tile_b);
	if (world_a == INVALID_WORLD || world_b == INVALID_WORLD || world_a == world_b) {
		return CommandCost(STR_ERROR_PORTAL_GATES_DIFFERENT_WORLDS);
	}

	/* Virtual length is proportional to coordinate distance across worlds */
	uint32_t dist = DistanceManhattan(tile_a, tile_b);
	uint32_t virtual_length = std::max(2u, dist / 4);

	/* Wormhole excitation and link stabilization cost */
	CommandCost cost(ExpensesType::Construction);
	cost.AddCost(_price[Price::BuildTunnel] * 10);

	if (flags.Test(DoCommandFlag::Execute)) {
		PortalID pid = PortalRegistry::LinkGates(tile_a, tile_b, virtual_length, true);
		if (pid == INVALID_PORTAL) return CMD_ERROR;

		YapfNotifyTrackLayoutChange(tile_a, DiagDirToDiagTrack(GetTunnelBridgeDirection(tile_a)));
		YapfNotifyTrackLayoutChange(tile_b, DiagDirToDiagTrack(GetTunnelBridgeDirection(tile_b)));
	}

	return cost;
}

CommandCost CmdBuildPortalPair(DoCommandFlags flags, TileIndex tile_a, DiagDirection dir_a, TileIndex tile_b, DiagDirection dir_b, RailType railtype)
{
	if (tile_a == tile_b) return CMD_ERROR;
	CommandCost placement_a = PlanetManager::CheckConstructionPlacement(tile_a);
	if (placement_a.Failed()) return placement_a;
	CommandCost placement_b = PlanetManager::CheckConstructionPlacement(tile_b);
	if (placement_b.Failed()) return placement_b;
	if (!ValParamRailType(railtype)) return CMD_ERROR;

	if (!IsValidDiagDirection(dir_a)) {
		auto [tileh, z] = GetTileSlopeZ(tile_a);
		dir_a = GetInclinedSlopeDirection(tileh);
		if (!IsValidDiagDirection(dir_a)) return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}
	if (!IsValidDiagDirection(dir_b)) {
		auto [tileh, z] = GetTileSlopeZ(tile_b);
		dir_b = GetInclinedSlopeDirection(tileh);
		if (!IsValidDiagDirection(dir_b)) return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	CompanyID company = _current_company;
	if (!Company::IsValidID(company) && company != OWNER_DEITY) return CMD_ERROR;

	WorldID world_a = PlanetManager::GetTileWorld(tile_a);
	WorldID world_b = PlanetManager::GetTileWorld(tile_b);
	if (world_a == INVALID_WORLD || world_b == INVALID_WORLD || world_a == world_b) {
		return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}
	placement_a = ValidatePortalGateFootprint(tile_a, dir_a, world_a);
	if (placement_a.Failed()) return placement_a;
	placement_b = ValidatePortalGateFootprint(tile_b, dir_b, world_b);
	if (placement_b.Failed()) return placement_b;
	std::optional<PortalTerminalLayout> terminal_a = PortalTerminal::Plan(tile_a, dir_a, world_a);
	std::optional<PortalTerminalLayout> terminal_b = PortalTerminal::Plan(tile_b, dir_b, world_b);
	if (!terminal_a.has_value() || !terminal_b.has_value()) return CommandCost(STR_ERROR_PORTAL_TERMINAL_FOOTPRINT);

	if (PortalRegistry::IsPortalTile(tile_a) || PortalRegistry::IsUnlinkedGate(tile_a) ||
	    PortalRegistry::IsPortalTile(tile_b) || PortalRegistry::IsUnlinkedGate(tile_b)) {
		return CommandCost(STR_ERROR_ALREADY_BUILT);
	}

	if (HasTileWaterGround(tile_a) || HasTileWaterGround(tile_b)) return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);

	DoCommandFlags test_flags = DoCommandFlags{flags}.Set(DoCommandFlag::Auto).Reset(DoCommandFlag::Execute);
	CommandCost ret_a = Command<Commands::LandscapeClear>::Do(test_flags, tile_a);
	if (ret_a.Failed()) return ret_a;
	CommandCost ret_b = Command<Commands::LandscapeClear>::Do(test_flags, tile_b);
	if (ret_b.Failed()) return ret_b;
	CommandCost terminal_cost_a = ValidatePortalTerminal(flags, *terminal_a, railtype);
	if (terminal_cost_a.Failed()) return terminal_cost_a;
	CommandCost terminal_cost_b = ValidatePortalTerminal(flags, *terminal_b, railtype);
	if (terminal_cost_b.Failed()) return terminal_cost_b;

	CommandCost cost(ret_a);
	cost.AddCost(ret_b.GetCost());
	cost.AddCost(terminal_cost_a.GetCost());
	cost.AddCost(terminal_cost_b.GetCost());
	cost.AddCost(_price[Price::BuildTunnel] * 20); // 2 heads + linking
	cost.AddCost(RailBuildCost(railtype) * 2);

	uint32_t dist = DistanceManhattan(tile_a, tile_b);
	uint32_t virtual_length = std::max(2u, dist / 4);

	if (flags.Test(DoCommandFlag::Execute)) {
		DoCommandFlags execute_flags = DoCommandFlags{flags}.Set({DoCommandFlag::Auto, DoCommandFlag::Execute});
		CommandCost clear_a = Command<Commands::LandscapeClear>::Do(execute_flags, tile_a);
		CommandCost clear_b = Command<Commands::LandscapeClear>::Do(execute_flags, tile_b);
		assert(clear_a.Succeeded() && clear_b.Succeeded());
		ClearPortalTerminal(flags, *terminal_a);
		ClearPortalTerminal(flags, *terminal_b);
		PortalTerminal::Build(*terminal_a, railtype, company);
		PortalTerminal::Build(*terminal_b, railtype, company);

		Company *c = Company::GetIfValid(company);
		if (c != nullptr) c->infrastructure.rail[railtype] += 2 * TUNNELBRIDGE_TRACKBIT_FACTOR;

		MakeRailTunnel(tile_a, company, dir_a, railtype);
		MakeRailTunnel(tile_b, company, dir_b, railtype);

		AddSideToSignalBuffer(tile_a, DiagDirection::Invalid, company);
		AddSideToSignalBuffer(tile_b, DiagDirection::Invalid, company);

		PortalRegistry::RegisterPortalPair(
			tile_a, dir_a, world_a,
			tile_b, dir_b, world_b,
			virtual_length, true
		);

		YapfNotifyTrackLayoutChange(tile_a, DiagDirToDiagTrack(dir_a));
		YapfNotifyTrackLayoutChange(tile_b, DiagDirToDiagTrack(dir_b));

		DirtyCompanyInfrastructureWindows(company);
	}

	return cost;
}

CommandCost CmdDestroyPortalGate(DoCommandFlags flags, TileIndex tile, bool demolish_both)
{
	if (!IsValidTile(tile)) return CMD_ERROR;

	bool is_portal = PortalRegistry::IsPortalTile(tile);
	bool is_unlinked = PortalRegistry::IsUnlinkedGate(tile);
	if (!is_portal && !is_unlinked) return CommandCost(STR_ERROR_CAN_T_BUILD_TUNNEL_HERE);

	CommandCost ret = CheckOwnership(GetTileOwner(tile));
	if (ret.Failed()) return ret;

	TileIndex endtile = is_portal ? PortalRegistry::GetOtherPortalEnd(tile) : INVALID_TILE;

	/* Reject demolition if any consist is traversing the wormhole corridor */
	if (PortalRegistry::IsPortalInTransit(tile)) {
		return CommandCost(STR_ERROR_TRAIN_IN_THE_WAY);
	}
	if (endtile != INVALID_TILE && PortalRegistry::IsPortalInTransit(endtile)) {
		return CommandCost(STR_ERROR_TRAIN_IN_THE_WAY);
	}

	ret = TunnelBridgeIsFree(tile, endtile);
	if (ret.Failed()) return ret;

	if (demolish_both && endtile != INVALID_TILE) {
		CommandCost ret_end = CheckOwnership(GetTileOwner(endtile));
		if (ret_end.Failed()) return ret_end;
	}

	Money base_cost = _price[Price::ClearTunnel] + RailClearCost(GetRailType(tile));
	CommandCost cost(ExpensesType::Construction);
	cost.AddCost(base_cost * ((demolish_both && endtile != INVALID_TILE) ? 2 : 1));

	if (flags.Test(DoCommandFlag::Execute)) {
		DiagDirection dir = GetTunnelBridgeDirection(tile);
		Track track = DiagDirToDiagTrack(dir);
		Owner owner = GetTileOwner(tile);

		Train *v = nullptr;
		if (HasTunnelBridgeReservation(tile)) {
			v = GetTrainForReservation(tile, track);
			if (v != nullptr) FreeTrainTrackReservation(v);
		}

		if (Company::IsValidID(owner)) {
			Company::Get(owner)->infrastructure.rail[GetRailType(tile)] -= TUNNELBRIDGE_TRACKBIT_FACTOR;
		}

		if (is_unlinked) {
			PortalRegistry::UnregisterPortalByTile(tile);
			DoClearSquare(tile);
			AddSideToSignalBuffer(tile, ReverseDiagDir(dir), owner);
			YapfNotifyTrackLayoutChange(tile, track);
		} else if (is_portal) {
			if (demolish_both && endtile != INVALID_TILE) {
				DiagDirection end_dir = GetTunnelBridgeDirection(endtile);
				Track end_track = DiagDirToDiagTrack(end_dir);
				Owner end_owner = GetTileOwner(endtile);

				if (Company::IsValidID(end_owner)) {
					Company::Get(end_owner)->infrastructure.rail[GetRailType(endtile)] -= TUNNELBRIDGE_TRACKBIT_FACTOR;
				}

				PortalRegistry::UnregisterPortalByTile(tile);
				DoClearSquare(tile);
				DoClearSquare(endtile);

				AddSideToSignalBuffer(tile, ReverseDiagDir(dir), owner);
				AddSideToSignalBuffer(endtile, ReverseDiagDir(end_dir), end_owner);

				YapfNotifyTrackLayoutChange(tile, track);
				YapfNotifyTrackLayoutChange(endtile, end_track);
			} else {
				/* Demolish only this gate; convert opposite end back to unlinked gate */
				const PortalLink *link = PortalRegistry::GetPortalLink(tile);
				PortalEndpoint opp = *link->GetOpposite(tile);

				PortalRegistry::UnregisterPortalByTile(tile);
				PortalRegistry::RegisterUnlinkedGate(opp.tile, opp.enter_dir, opp.world_id);

				DoClearSquare(tile);
				AddSideToSignalBuffer(tile, ReverseDiagDir(dir), owner);
				YapfNotifyTrackLayoutChange(tile, track);
				YapfNotifyTrackLayoutChange(opp.tile, DiagDirToDiagTrack(opp.enter_dir));
			}
		}

		DirtyCompanyInfrastructureWindows(owner);
		if (v != nullptr) TryPathReserve(v);
	}

	return cost;
}

CommandCost CmdDesignateSpaceport(DoCommandFlags flags, StationID station)
{
	if (!Station::IsValidID(station)) return CMD_ERROR;

	Station *st = Station::GetIfValid(station);
	if (st == nullptr) return CMD_ERROR;

	if (!st->facilities.Test(StationFacility::Airport) || st->airport.IsEmpty()) {
		return CommandCost(STR_ERROR_CAN_T_BUILD_AIRPORT_HERE);
	}

	CommandCost ret_own = CheckOwnership(st->owner);
	if (ret_own.Failed()) return ret_own;

	const SpaceportInfo *existing = SpaceportManager::GetSpaceport(station);
	if (existing != nullptr && existing->offworld_trade_tier >= 3) return CommandCost(STR_ERROR_SPACEPORT_MAX_TIER);

	WorldID world_id = PlanetManager::GetTileWorld(st->xy);
	if (world_id == INVALID_WORLD) {
		world_id = PlanetManager::GetTileWorld(st->airport.tile);
	}

	CommandCost cost(ExpensesType::Construction, _price[Price::BuildStationAirport] * (existing == nullptr ? 2 : existing->offworld_trade_tier + 1));

	if (flags.Test(DoCommandFlag::Execute)) {
		if (existing == nullptr) {
			uint8_t tier = 1;
			if (st->airport.type == AT_INTERCON) tier = 3;
			else if (st->airport.type == AT_INTERNATIONAL || st->airport.type == AT_METROPOLITAN) tier = 2;

			SpaceportManager::RegisterSpaceport(station, world_id, tier);
		} else {
			SpaceportInfo *spaceport = SpaceportManager::GetSpaceportMutable(station);
			spaceport->offworld_trade_tier++;
		}
		/* A normal in-game station already owns a viewport sign. Bare command
		 * tests create pool objects without one and need no visual refresh. */
		if (st->sign.kdtree_valid) st->UpdateVirtCoord();
		SetWindowDirty(WindowClass::StationView, station);
	}

	return cost;
}

CommandCost CmdBuildEdgeConduit(DoCommandFlags flags, TileIndex tile, DiagDirection dir, CargoType cargo, RailType railtype)
{
	CommandCost placement_result = PlanetManager::CheckConstructionPlacement(tile);
	if (placement_result.Failed()) return placement_result;
	if (!ValParamRailType(railtype)) return CMD_ERROR;

	CompanyID company = _current_company;
	if (!Company::IsValidID(company) && company != OWNER_DEITY) return CMD_ERROR;

	WorldID world_id = PlanetManager::GetTileWorld(tile);
	assert(world_id != INVALID_WORLD);

	std::optional<EdgeConduitPlacement> footprint = EdgeConduitManager::ResolvePlacement(tile, dir);
	if (!footprint.has_value()) return CommandCost(STR_ERROR_EDGE_CONDUIT_REQUIRES_VOID);
	if (PlanetManager::GetTileWorld(footprint->approach_tile) != world_id) {
		return CommandCost(STR_ERROR_EDGE_CONDUIT_REQUIRES_VOID);
	}
	dir = footprint->dir;

	if (EdgeConduitManager::IsConduitTile(tile) || PortalRegistry::IsPortalTile(tile) || PortalRegistry::IsUnlinkedGate(tile)) {
		return CommandCost(STR_ERROR_ALREADY_BUILT);
	}

	if (HasTileWaterGround(tile)) return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);

	if (!IsValidCargoType(cargo)) {
		cargo = EdgeConduitManager::GetPreferredMineralCargo();
	}

	CommandCost ret = Command<Commands::LandscapeClear>::Do(flags | DoCommandFlag::Auto, tile);
	if (ret.Failed()) return ret;

	CommandCost cost(ret);
	cost.AddCost(_price[Price::BuildTunnel] * 4);
	cost.AddCost(RailBuildCost(railtype));

	if (flags.Test(DoCommandFlag::Execute)) {
		Company *c = Company::GetIfValid(company);
		if (c != nullptr) c->infrastructure.rail[railtype] += TUNNELBRIDGE_TRACKBIT_FACTOR;

		MakeRailTunnel(tile, company, dir, railtype);
		AddSideToSignalBuffer(tile, DiagDirection::Invalid, company);
		YapfNotifyTrackLayoutChange(tile, DiagDirToDiagTrack(dir));
		DirtyCompanyInfrastructureWindows(company);

		EdgeConduitManager::RegisterConduit(tile, dir, world_id, cargo, company);
	}

	return cost;
}

CommandCost CmdDestroyEdgeConduit(DoCommandFlags flags, TileIndex tile)
{
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!EdgeConduitManager::IsConduitTile(tile)) return CommandCost(STR_ERROR_MUST_DEMOLISH_TUNNEL_FIRST);

	CommandCost ret_own = CheckOwnership(GetTileOwner(tile));
	if (ret_own.Failed()) return ret_own;

	CommandCost cost(ExpensesType::Construction, _price[Price::ClearTunnel]);

	if (flags.Test(DoCommandFlag::Execute)) {
		DiagDirection dir = GetTunnelBridgeDirection(tile);
		Track track = DiagDirToDiagTrack(dir);
		Owner owner = GetTileOwner(tile);

		Company *c = Company::GetIfValid(owner);
		if (c != nullptr) {
			c->infrastructure.rail[GetRailType(tile)] -= TUNNELBRIDGE_TRACKBIT_FACTOR;
		}

		EdgeConduitManager::UnregisterConduit(tile);
		DoClearSquare(tile);
		AddSideToSignalBuffer(tile, ReverseDiagDir(dir), owner);
		YapfNotifyTrackLayoutChange(tile, track);
		DirtyCompanyInfrastructureWindows(owner);
	}

	return cost;
}

CommandCost CmdConfigureSpaceportBridge(DoCommandFlags flags, StationID station, WorldID dest_world, uint32_t route_id, bool auto_dispatch)
{
	if (!Station::IsValidID(station)) return CMD_ERROR;

	Station *st = Station::GetIfValid(station);
	if (st == nullptr) return CMD_ERROR;

	if (!SpaceportManager::IsSpaceport(station)) {
		return CMD_ERROR;
	}

	CommandCost ret_own = CheckOwnership(st->owner);
	if (ret_own.Failed()) return ret_own;

	if (flags.Test(DoCommandFlag::Execute)) {
		SpaceportManager::ConfigureSpaceportBridge(station, dest_world, route_id, auto_dispatch);
		SetWindowDirty(WindowClass::StationView, station);
	}

	return CommandCost();
}

CommandCost CmdConfigureEdgeConduitFeeder(DoCommandFlags flags, TileIndex tile, bool enabled, WorldID dest_world, uint32_t route_id)
{
	if (!IsValidTile(tile)) return CMD_ERROR;
	if (!EdgeConduitManager::IsConduitTile(tile)) return CMD_ERROR;

	const EdgeConduit *conduit = EdgeConduitManager::GetConduit(tile);
	if (conduit == nullptr) return CMD_ERROR;

	CommandCost ret_own = CheckOwnership(conduit->owner);
	if (ret_own.Failed()) return ret_own;

	if (flags.Test(DoCommandFlag::Execute)) {
		EdgeConduitManager::ConfigureDirectFeeder(tile, enabled, dest_world, route_id);
	}

	return CommandCost();
}

CommandCost CmdColonizeOutpost(DoCommandFlags flags, TileIndex tile, const std::string &outpost_name)
{
	CommandCost placement = PlanetManager::CheckConstructionPlacement(tile);
	if (placement.Failed()) return placement;

	WorldID world = PlanetManager::GetTileWorld(tile);
	if (world == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}

	const PlanetRegion *region = PlanetManager::GetRegion(world);
	if (region == nullptr) return CMD_ERROR;

	if (region->phase != WorldPhase::Phase4_Expansion) {
		return CommandCost(STR_ERROR_CANNOT_COLONIZE_NON_EXPANSION);
	}

	/* Capital colonization and outpost expedition fee */
	CommandCost cost(ExpensesType::Construction, _price[Price::BuildTown] * 5);

	if (flags.Test(DoCommandFlag::Execute)) {
		std::string name = outpost_name;
		if (name.empty()) {
			name = fmt::format("Outpost {}", region->name);
		}

		PlanetManager::ColonizeWorld(world, name, tile);

		/* Spawn initial frontier settlement if none exists on this world */
		Town *existing_town = PlanetManager::GetWorldPrimaryTown(world);
		if (existing_town == nullptr && Town::CanAllocateItem() && IsValidTile(tile)) {
			Town *t = Town::Create(tile);
			if (t != nullptr) {
				t->name = name;
				if (_current_language != nullptr) {
					t->UpdateVirtCoord();
				}
			}
		}

		/* Broadcast colony founding news */
		AddTileNewsItem(GetEncodedString(STR_NEWS_WORLD_COLONIZED, name), NewsType::CompanyInfo, tile);
	}

	return cost;
}

CommandCost CmdPromoteWorld(DoCommandFlags flags, WorldID world)
{
	const PlanetRegion *region = PlanetManager::GetRegion(world);
	if (region == nullptr) return CMD_ERROR;

	if (region->phase == WorldPhase::Phase1_Core) {
		return CommandCost(STR_ERROR_ALREADY_MAX_PHASE);
	}

	if (!PlanetManager::CanPromoteWorld(world)) {
		return CommandCost(STR_ERROR_NOT_ENOUGH_DEVELOPMENT);
	}

	/* Civic promotion and infrastructure elevation fee */
	CommandCost cost(ExpensesType::Construction, _price[Price::BuildTown] * 8);

	if (flags.Test(DoCommandFlag::Execute)) {
		WorldPhase prev_phase = region->phase;
		PlanetManager::PromoteWorldPhase(world);

		TileIndex news_tile = region->outpost_tile != INVALID_TILE ?
			region->outpost_tile :
			TileXY((region->min_x + region->max_x) / 2, (region->min_y + region->max_y) / 2);

		if (prev_phase == WorldPhase::Phase3_Frontier) {
			AddTileNewsItem(GetEncodedString(STR_NEWS_WORLD_DEVELOPED, region->name), NewsType::CompanyInfo, news_tile);
		} else if (prev_phase == WorldPhase::Phase2_Developed) {
			AddTileNewsItem(GetEncodedString(STR_NEWS_WORLD_CORE_METROPOLIS, region->name), NewsType::CompanyInfo, news_tile);
			/* Elevate primary settlement to Imperial Megacity */
			Town *primary = PlanetManager::GetWorldPrimaryTown(world);
			if (primary != nullptr && !MegacityManager::IsMegacity(primary->index)) {
				MegacityManager::RegisterMegacity(primary->index, world, primary->name, std::max(1000u, primary->cache.population));
			}
		}

		UniverseAuthorityService::Instance().PromoteWorld(world);
	}

	return cost;
}

CommandCost CmdPlaceCorporateHQ(DoCommandFlags flags, TileIndex tile, const std::string &hq_name)
{
	if (tile == INVALID_TILE) return CMD_ERROR;
	CompanyID company = _current_company;
	if (company == CompanyID::Invalid()) return CMD_ERROR;

	std::string err_msg;
	if (!CorporateHQManager::CanPlaceHQ(company, tile, err_msg)) {
		if (err_msg.find("Phase 1 Core") != std::string::npos) {
			return CommandCost(STR_ERROR_CANNOT_BUILD_HQ_NOT_CORE_WORLD);
		}
		if (err_msg.find("active Corporate Headquarters") != std::string::npos) {
			return CommandCost(STR_ERROR_ALREADY_HAS_CORPORATE_HQ);
		}
		if (err_msg.find("5,000,000") != std::string::npos) {
			return CommandCost(STR_ERROR_CANNOT_BUILD_HQ_INSUFFICIENT_FUNDS);
		}
		if (err_msg.find("3 distinct world phases") != std::string::npos) {
			return CommandCost(STR_ERROR_CANNOT_BUILD_HQ_INSUFFICIENT_PRESENCE);
		}
		return CommandCost(STR_ERROR_SITE_UNSUITABLE);
	}

	/* Construction cost: 2,500,000 Cr */
	CommandCost cost(ExpensesType::Construction, 2500000);

	if (flags.Test(DoCommandFlag::Execute)) {
		const PlanetRegion *region = PlanetManager::GetRegionByTile(tile);
		WorldID wid = (region != nullptr) ? region->id : PlanetManager::GetTileWorld(tile);
		std::string name = hq_name.empty() ? "Corporate HQ Campus" : hq_name;
		CorporateHQManager::RegisterHQ(company, wid, tile, name);

		AddTileNewsItem(GetEncodedString(STR_NEWS_CORPORATE_HQ_ESTABLISHED, name), NewsType::CompanyInfo, tile);
	}

	return cost;
}

CommandCost CmdBuildLogisticsHub(DoCommandFlags flags, TileIndex tile, StationID st, const std::string &hub_name)
{
	if (tile == INVALID_TILE) return CMD_ERROR;
	CompanyID company = _current_company;
	if (company == CompanyID::Invalid()) return CMD_ERROR;

	WorldID world_id = PlanetManager::GetTileWorld(tile);
	if (world_id == INVALID_WORLD) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);
	}
	const PlanetRegion *region = PlanetManager::GetRegion(world_id);
	if (region == nullptr) return CMD_ERROR;

	if (region->phase == WorldPhase::Phase4_Expansion && region->development_score == 0 && region->outpost_tile == INVALID_TILE) {
		return CommandCost(STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD);
	}

	/* Construction cost: 75,000 Cr */
	CommandCost cost(ExpensesType::Construction, 75000);

	if (flags.Test(DoCommandFlag::Execute)) {
		if (st == StationID::Invalid()) {
			if (IsTileType(tile, TileType::Station)) {
				const Station *station = Station::GetByTile(tile);
				if (station != nullptr) st = station->index;
			}
			if (st == StationID::Invalid()) {
				for (const Station *s : Station::Iterate()) {
					if (s->owner == company && DistanceManhattan(tile, s->xy) <= 4) {
						st = s->index;
						break;
					}
				}
			}
		}

		std::string name = hub_name.empty() ? ("Logistics Hub " + region->name) : hub_name;
		LogisticsHubManager::RegisterHub(tile, region->id, company, st, name);
	}

	return cost;
}

CommandCost CmdSetFabricationMode(DoCommandFlags flags, bool enabled)
{
	CompanyID company = _current_company;
	if (company == CompanyID::Invalid()) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		FabricationManager::SetFabricateFromStockpile(company, enabled);
		SetWindowDirty(WindowClass::CorporateHQ, company.base());
	}

	return CommandCost();
}

CommandCost CmdSelectResearchProject(DoCommandFlags flags, TechID project_id)
{
	CompanyID company = _current_company;
	if (company == CompanyID::Invalid()) return CMD_ERROR;

	if (project_id != TECH_NONE) {
		std::string err_msg;
		if (!TechTreeManager::CanResearch(company, project_id, err_msg)) {
			return CMD_ERROR;
		}
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		TechTreeManager::SetActiveProject(company, project_id);
		SetWindowDirty(WindowClass::CorporateHQ, company.base());
	}

	return CommandCost();
}

CommandCost CmdSetResearchBudget(DoCommandFlags flags, uint32_t budget)
{
	CompanyID company = _current_company;
	if (company == CompanyID::Invalid()) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		TechTreeManager::SetMonthlyBudget(company, budget);
		SetWindowDirty(WindowClass::CorporateHQ, company.base());
	}

	return CommandCost();
}


