/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_orders.cpp Resolve explicit remote station identities to local gate heads. */

#include "../stdafx.h"
#include "federation_orders.h"
#include "federation_identity.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "../order_base.h"
#include "../vehicle_base.h"
#include "../safeguards.h"

std::optional<TileIndex> GetFederationOrderGate(const Vehicle *vehicle, const Order *order)
{
	if (vehicle == nullptr || order == nullptr || vehicle->type != VehicleType::Train || !order->IsType(OT_GOTO_STATION)) return std::nullopt;
	const auto station = FederationIdentityRegistry::FindStation(order->GetDestination().ToStationID());
	if (!station) return std::nullopt;
	/* Ordinary local inter-region orders retain the existing portal pathfinder. */
	if (station->name_space == FederationIdentityRegistry::GetNamespace()) return std::nullopt;
	const auto world = PlanetManager::GetTileWorld(vehicle->tile);
	if (station->world_id == world) return std::nullopt;
	TileIndex gate = INVALID_TILE;
	for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
		if (link.local_endpoint.world_id == world && link.remote_world == station->world_id &&
				(gate == INVALID_TILE || tile < gate)) gate = tile;
	}
	return gate;
}
