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
#include "planet_manager.h"
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
#include "../map_func.h"
#include "../pbs.h"
#include "../train.h"

#include "../table/strings.h"
#include "../safeguards.h"

CommandCost CmdBuildPortalGate(DoCommandFlags flags, TileIndex tile, DiagDirection dir, RailType railtype)
{
	if (!IsValidTile(tile)) return CMD_ERROR;
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
	if (world_id == INVALID_WORLD) return CommandCost(STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE);

	/* Cannot build on an already existing portal gate */
	if (PortalRegistry::IsPortalTile(tile) || PortalRegistry::IsUnlinkedGate(tile)) {
		return CommandCost(STR_ERROR_ALREADY_BUILT);
	}

	if (HasTileWaterGround(tile)) return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);

	/* Clear existing tile contents */
	CommandCost ret = Command<Commands::LandscapeClear>::Do(flags, tile);
	if (ret.Failed()) return ret;

	CommandCost cost(ret);
	/* Base gateway construction cost (capital-intensive infrastructure) */
	cost.AddCost(_price[Price::BuildTunnel] * 5);
	cost.AddCost(RailBuildCost(railtype));

	if (flags.Test(DoCommandFlag::Execute)) {
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
		return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	/* Virtual length is proportional to coordinate distance across worlds */
	uint32_t dist = DistanceManhattan(tile_a, tile_b);
	uint32_t virtual_length = std::max(2u, dist / 4);

	/* Wormhole excitation and link stabilization cost */
	CommandCost cost;
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
	if (!IsValidTile(tile_a) || !IsValidTile(tile_b) || tile_a == tile_b) return CMD_ERROR;
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

	if (PortalRegistry::IsPortalTile(tile_a) || PortalRegistry::IsUnlinkedGate(tile_a) ||
	    PortalRegistry::IsPortalTile(tile_b) || PortalRegistry::IsUnlinkedGate(tile_b)) {
		return CommandCost(STR_ERROR_ALREADY_BUILT);
	}

	if (HasTileWaterGround(tile_a) || HasTileWaterGround(tile_b)) return CommandCost(STR_ERROR_CAN_T_BUILD_ON_WATER);

	CommandCost ret_a = Command<Commands::LandscapeClear>::Do(flags, tile_a);
	if (ret_a.Failed()) return ret_a;
	CommandCost ret_b = Command<Commands::LandscapeClear>::Do(flags, tile_b);
	if (ret_b.Failed()) return ret_b;

	CommandCost cost(ret_a);
	cost.AddCost(ret_b.GetCost());
	cost.AddCost(_price[Price::BuildTunnel] * 20); // 2 heads + linking
	cost.AddCost(RailBuildCost(railtype) * 2);

	uint32_t dist = DistanceManhattan(tile_a, tile_b);
	uint32_t virtual_length = std::max(2u, dist / 4);

	if (flags.Test(DoCommandFlag::Execute)) {
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
	CommandCost cost;
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
