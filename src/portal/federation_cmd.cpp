/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_cmd.cpp Federation inter-server transfer commands and coordinator implementation. */

#include "../stdafx.h"
#include "federation_cmd.h"
#include "consist_materializer.h"
#include "federation_identity.h"
#include "portal_registry.h"
#include "universe_authority.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../train.h"
#include "../strings_func.h"
#include "../timer/timer_game_tick.h"
#include "../table/strings.h"

#include "../safeguards.h"

void FederationTransferManager::Reset()
{
	UniverseAuthorityService::Instance().Reset();
}

bool FederationTransferManager::InitiateConsistDeparture(Train *consist, TileIndex portal_tile)
{
	if (consist == nullptr || portal_tile == INVALID_TILE) return false;

	const InterServerPortalLink *link = PortalRegistry::GetInterServerPortal(portal_tile);
	if (link == nullptr || !link->IsValid()) return false;

	Train *front = consist->First();
	if (front == nullptr) return false;

	/* Derive owner token */
	auto global_comp = FederationIdentityRegistry::FindCompany(front->owner);
	GlobalOwnerToken owner_token = global_comp.has_value() ? global_comp->ToOwnerToken() : GlobalOwnerToken{};

	/* 1. Despawn train from local map */
	ConsistDespawnResult despawn_res = ConsistMaterializer::DespawnForTransfer(front, owner_token);
	if (!despawn_res.success) {
		return false;
	}

	/* 2. Register transfer with Universe Authority */
	uint32_t transit_ticks = std::max<uint32_t>(10, link->virtual_length * 20);
	std::string tx_id = UniverseAuthorityService::Instance().InitiateTransfer(
		link->local_endpoint.world_id,
		link->remote_world,
		link->id.base(),
		link->remote_gate_id,
		despawn_res.snapshot_bytes,
		transit_ticks
	);

	if (tx_id.empty()) {
		return false;
	}

	/* 3. Mark consist departed */
	return UniverseAuthorityService::Instance().DepartTransfer(tx_id, TimerGameTick::counter);
}

size_t FederationTransferManager::ProcessIncomingTransfers(WorldID local_world, uint64_t current_tick)
{
	if (local_world == INVALID_WORLD) return 0;

	std::vector<std::string> pending = UniverseAuthorityService::Instance().QueryPendingTransfers(local_world, current_tick);
	size_t materialized_count = 0;

	for (const std::string &tx_id : pending) {
		auto claim_opt = UniverseAuthorityService::Instance().ClaimTransfer(tx_id, local_world);
		if (!claim_opt.has_value()) continue;

		const UniverseTransferRecord &rec = *claim_opt;

		/* Locate receiving portal gate tile on local_world matching dest_gate_id */
		TileIndex dest_tile = INVALID_TILE;
		DiagDirection enter_dir = DiagDirection::Begin;

		for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
			if (link.local_endpoint.world_id == local_world) {
				if (link.id.base() == rec.dest_gate_id || dest_tile == INVALID_TILE) {
					dest_tile = tile;
					enter_dir = link.local_endpoint.enter_dir;
					if (link.id.base() == rec.dest_gate_id) break;
				}
			}
		}

		/* If no dedicated inter-server gate was found, check unlinked gates */
		if (dest_tile == INVALID_TILE) {
			for (const auto &[tile, ep] : PortalRegistry::GetUnlinkedGates()) {
				if (ep.world_id == local_world) {
					dest_tile = tile;
					enter_dir = ep.enter_dir;
					break;
				}
			}
		}

		if (dest_tile == INVALID_TILE) {
			/* No eligible receiving gate currently registered; leave in ARRIVAL_PENDING */
			continue;
		}

		/* Check throat clearance */
		if (!ConsistMaterializer::CheckThroatClearance(dest_tile, enter_dir)) {
			/* Throat currently obstructed; remain in ARRIVAL_PENDING to retry next tick */
			continue;
		}

		/* Materialize train at destination throat */
		ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(
			rec.snapshot, dest_tile, enter_dir
		);

		if (mat_res.success) {
			UniverseAuthorityService::Instance().ConfirmTransferArrival(tx_id, local_world, true);
			materialized_count++;
		} else {
			UniverseAuthorityService::Instance().ConfirmTransferArrival(
				tx_id, local_world, false, mat_res.error_message
			);
		}
	}

	return materialized_count;
}

CommandCost CmdDispatchInterServerTransfer(DoCommandFlags flags, VehicleID vehicle_id, TileIndex portal_tile)
{
	Train *v = Train::GetIfValid(vehicle_id);
	if (v == nullptr || !v->IsFrontEngine()) {
		return CommandCost(STR_ERROR_TRAIN_IN_THE_WAY);
	}

	if (!PortalRegistry::IsInterServerPortal(portal_tile)) {
		return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		if (!FederationTransferManager::InitiateConsistDeparture(v, portal_tile)) {
			return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
		}
	}

	return CommandCost();
}
