/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file federation_orders.cpp Resolve explicit remote station identities to local gate heads. */

#include "../stdafx.h"
#include "universe_network.h"
#include "stellar_network.h"
#include "../map_func.h"
#include "federation_orders.h"
#include "federation_identity.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "../order_base.h"
#include "../vehicle_base.h"
#include "../train.h"
#include "../safeguards.h"

std::optional<TileIndex> GetFederationOrderGate(const Vehicle *vehicle, const Order *order)
{
	if (vehicle == nullptr || order == nullptr || vehicle->type != VehicleType::Train || !order->IsType(OT_GOTO_STATION)) return std::nullopt;
	if (auto pin = UniverseNetwork::GateOrder(order->GetDestination().ToStationID())) {
  WorldID world = PlanetManager::GetTileWorld(vehicle->tile);
  if (world != pin->target_world) return UniverseNetwork::Route(world, pin->target_world, vehicle->owner, Train::From(vehicle)->compatible_railtypes);
  for (const auto &[key,g] : UniverseNetwork::Directory()) {
   if (g.value("kind", "") == "gate" && UniverseNetwork::ParseNamespace(g.at("namespace")) == pin->name_space &&
     g.at("id").get<uint64_t>() == pin->destination_sequence && g.value("online",false)) {
    TileIndex tile{g.at("tile").get<uint32_t>()};
    if (tile < Map::Size() && UniverseNetwork::IsPinnedGate(*pin,tile) && StellarNetwork::CanUseGate(tile,vehicle->owner)) return tile;
   }
  }
  return INVALID_TILE;
 }
	const auto station = FederationIdentityRegistry::FindStation(order->GetDestination().ToStationID());
	if (!station) return std::nullopt;
	/* Ordinary local inter-region orders retain the existing portal pathfinder. */
	if (station->name_space == FederationIdentityRegistry::GetNamespace()) return std::nullopt;
	const auto world = PlanetManager::GetTileWorld(vehicle->tile);
	if (station->world_id == world) return std::nullopt;
	if (UniverseNetwork::Directory().contains(UniverseNetwork::Namespace(FederationIdentityRegistry::GetNamespace()))) return UniverseNetwork::Route(world, station->world_id, vehicle->owner, Train::From(vehicle)->compatible_railtypes);
	TileIndex gate = INVALID_TILE;
	for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
		if (link.local_endpoint.world_id == world && link.remote_world == station->world_id &&
				(gate == INVALID_TILE || tile < gate)) gate = tile;
	}
	return gate;
}
