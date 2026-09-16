/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_registry.cpp Implementation of wormhole portal registry. */

#include "../stdafx.h"
#include "portal_registry.h"
#include "edge_conduit.h"
#include "federation_identity.h"
#include "transfer_journal.h"
#include "../tunnelbridge_map.h"
#include "../tile_map.h"
#include "../landscape.h"
#include "../direction_func.h"
#include "../vehicle_base.h"
#include "../economy_func.h"
#include "../map_func.h"

std::unordered_map<TileIndex, PortalID> PortalRegistry::tile_to_portal;
std::unordered_map<uint32_t, PortalLink> PortalRegistry::portal_links;
std::unordered_map<TileIndex, PortalEndpoint> PortalRegistry::unlinked_gates;
std::unordered_map<TileIndex, InterServerPortalLink> PortalRegistry::interserver_portals;
std::unordered_map<uint32_t, uint32_t> PortalRegistry::vehicle_portal_progress;
uint32_t PortalRegistry::next_portal_id = 1;

PortalID PortalRegistry::RegisterPortalPair(
	TileIndex tile_a, DiagDirection dir_a, WorldID world_a,
	TileIndex tile_b, DiagDirection dir_b, WorldID world_b,
	uint32_t virtual_length, bool bidirectional
)
{
	if (tile_a == INVALID_TILE || tile_b == INVALID_TILE || tile_a == tile_b) {
		return INVALID_PORTAL;
	}

	/* Avoid registering a tile that is already part of another portal link */
	if (tile_to_portal.find(tile_a) != tile_to_portal.end() ||
	    tile_to_portal.find(tile_b) != tile_to_portal.end()) {
		return INVALID_PORTAL;
	}

	/* Remove from unlinked gates if either was unlinked */
	unlinked_gates.erase(tile_a);
	unlinked_gates.erase(tile_b);

	PortalID id{next_portal_id++};

	PortalLink link;
	link.id = id;
	link.end_a = PortalEndpoint{tile_a, dir_a, world_a};
	link.end_b = PortalEndpoint{tile_b, dir_b, world_b};
	link.virtual_length = std::max(1u, virtual_length);
	link.bidirectional = bidirectional;

	portal_links[id.base()] = link;
	tile_to_portal[tile_a] = id;
	tile_to_portal[tile_b] = id;

	return id;
}

bool PortalRegistry::UnregisterPortal(PortalID id)
{
	auto it = portal_links.find(id.base());
	if (it == portal_links.end()) return false;

	tile_to_portal.erase(it->second.end_a.tile);
	tile_to_portal.erase(it->second.end_b.tile);
	portal_links.erase(it);
	return true;
}

bool PortalRegistry::UnregisterPortalByTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;

	auto it_is = interserver_portals.find(tile);
	if (it_is != interserver_portals.end()) {
		interserver_portals.erase(it_is);
		return true;
	}

	auto it_un = unlinked_gates.find(tile);
	if (it_un != unlinked_gates.end()) {
		unlinked_gates.erase(it_un);
		return true;
	}

	auto it = tile_to_portal.find(tile);
	if (it == tile_to_portal.end()) return false;

	return UnregisterPortal(it->second);
}

bool PortalRegistry::RegisterUnlinkedGate(TileIndex tile, DiagDirection dir, WorldID world_id)
{
	if (tile == INVALID_TILE) return false;
	if (tile_to_portal.find(tile) != tile_to_portal.end()) return false;
	if (unlinked_gates.find(tile) != unlinked_gates.end()) return false;

	unlinked_gates[tile] = PortalEndpoint{tile, dir, world_id};
	return true;
}

bool PortalRegistry::IsUnlinkedGate(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;
	return unlinked_gates.find(tile) != unlinked_gates.end();
}

const PortalEndpoint *PortalRegistry::GetUnlinkedGate(TileIndex tile)
{
	auto it = unlinked_gates.find(tile);
	if (it == unlinked_gates.end()) return nullptr;
	return &it->second;
}

const std::unordered_map<TileIndex, PortalEndpoint> &PortalRegistry::GetUnlinkedGates()
{
	return unlinked_gates;
}

PortalID PortalRegistry::LinkGates(TileIndex tile_a, TileIndex tile_b, uint32_t virtual_length, bool bidirectional)
{
	auto it_a = unlinked_gates.find(tile_a);
	auto it_b = unlinked_gates.find(tile_b);
	if (it_a == unlinked_gates.end() || it_b == unlinked_gates.end()) return INVALID_PORTAL;

	PortalEndpoint end_a = it_a->second;
	PortalEndpoint end_b = it_b->second;

	unlinked_gates.erase(it_a);
	unlinked_gates.erase(it_b);

	return RegisterPortalPair(
		end_a.tile, end_a.enter_dir, end_a.world_id,
		end_b.tile, end_b.enter_dir, end_b.world_id,
		virtual_length, bidirectional
	);
}

bool PortalRegistry::IsPortalInTransit(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;

	const PortalLink *link = GetPortalLink(tile);
	if (link == nullptr) {
		for (const auto &[veh_id, progress] : vehicle_portal_progress) {
			const Vehicle *v = Vehicle::GetIfValid(VehicleID{veh_id});
			if (v != nullptr && v->tile == tile) return true;
		}
		return false;
	}

	TileIndex tile_a = link->end_a.tile;
	TileIndex tile_b = link->end_b.tile;

	for (const auto &[veh_id, progress] : vehicle_portal_progress) {
		const Vehicle *v = Vehicle::GetIfValid(VehicleID{veh_id});
		if (v != nullptr && (v->tile == tile_a || v->tile == tile_b)) {
			return true;
		}
	}

	return false;
}

PortalID PortalRegistry::RegisterInterServerPortal(
	TileIndex local_tile,
	DiagDirection dir,
	WorldID local_world,
	WorldID remote_world,
	uint32_t remote_gate_id,
	uint32_t virtual_length,
	uint32_t local_gate_id)
{
	if (local_tile == INVALID_TILE || remote_world == INVALID_WORLD) {
		return INVALID_PORTAL;
	}

	if (tile_to_portal.find(local_tile) != tile_to_portal.end() ||
	    interserver_portals.find(local_tile) != interserver_portals.end()) {
		return INVALID_PORTAL;
	}

	unlinked_gates.erase(local_tile);

	PortalID id = local_gate_id != 0 ? PortalID{local_gate_id} : PortalID{next_portal_id++};
	if (id.base() >= next_portal_id) {
		next_portal_id = id.base() + 1;
	}
	InterServerPortalLink link;
	link.id = id;
	link.local_endpoint = PortalEndpoint{local_tile, dir, local_world};
	link.remote_world = remote_world;
	link.remote_gate_id = remote_gate_id;
	link.virtual_length = virtual_length;

	interserver_portals[local_tile] = link;
	return id;
}

bool PortalRegistry::RestoreInterServerPortal(const InterServerPortalLink &link)
{
	if (!link.IsValid()) return false;
	unlinked_gates.erase(link.local_endpoint.tile);
	interserver_portals[link.local_endpoint.tile] = link;
	if (link.id.base() >= next_portal_id) {
		next_portal_id = link.id.base() + 1;
	}
	return true;
}

bool PortalRegistry::IsInterServerPortal(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;
	return interserver_portals.find(tile) != interserver_portals.end();
}

const InterServerPortalLink *PortalRegistry::GetInterServerPortal(TileIndex tile)
{
	auto it = interserver_portals.find(tile);
	return it != interserver_portals.end() ? &it->second : nullptr;
}

bool PortalRegistry::UnregisterInterServerPortal(TileIndex tile)
{
	return interserver_portals.erase(tile) > 0;
}

const std::unordered_map<TileIndex, InterServerPortalLink> &PortalRegistry::GetAllInterServerPortals()
{
	return interserver_portals;
}

bool PortalRegistry::IsPortalTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return false;
	return tile_to_portal.find(tile) != tile_to_portal.end() ||
	       interserver_portals.find(tile) != interserver_portals.end();
}

/**
 * Check whether a registry entry still matches a real rail tunnel head.
 * @param tile Physical tile expected to hold the gate head.
 * @param dir Expected entry direction, or Invalid when any tunnel direction is acceptable.
 * @return True when the map still contains a rail tunnel head with the expected direction.
 */
static bool IsPhysicalRailGateHead(TileIndex tile, DiagDirection dir)
{
	if (tile == INVALID_TILE || !IsValidTile(tile) || !IsTunnelTile(tile)) return false;
	if (GetTunnelBridgeTransportType(tile) != TransportType::Rail) return false;
	return !IsValidDiagDirection(dir) || GetTunnelBridgeDirection(tile) == dir;
}

PortalGateClassification PortalRegistry::ClassifyGate(TileIndex tile)
{
	PortalGateClassification result;
	if (tile == INVALID_TILE) return result;

	if (EdgeConduitManager::IsConduitTile(tile)) {
		result.kind = PortalGateKind::EdgeConduit;
		const EdgeConduit *conduit = EdgeConduitManager::GetConduit(tile);
		if (conduit != nullptr) {
			result.local_endpoint = PortalEndpoint{conduit->tile, conduit->dir, conduit->world_id};
		}
		return result;
	}

	auto it_un = unlinked_gates.find(tile);
	if (it_un != unlinked_gates.end()) {
		result.kind = IsPhysicalRailGateHead(tile, it_un->second.enter_dir) ? PortalGateKind::LocalUnlinked : PortalGateKind::LocalStale;
		result.local_endpoint = it_un->second;
		return result;
	}

	auto it_is = interserver_portals.find(tile);
	if (it_is != interserver_portals.end()) {
		const InterServerPortalLink &link = it_is->second;
		result.id = link.id;
		result.local_endpoint = link.local_endpoint;
		result.remote_world = link.remote_world;
		result.remote_gate_id = link.remote_gate_id;
		result.virtual_length = link.virtual_length;

		if (!IsPhysicalRailGateHead(tile, link.local_endpoint.enter_dir)) {
			result.kind = PortalGateKind::ExternalStale;
		} else if (!link.IsValid() || link.remote_gate_id == 0) {
			result.kind = PortalGateKind::ExternalUnlinked;
		} else {
			result.kind = PortalGateKind::ExternalLinked;
		}
		return result;
	}

	const PortalLink *link = GetPortalLink(tile);
	if (link != nullptr) {
		result.id = link->id;
		result.virtual_length = link->virtual_length;
		if (link->end_a.tile == tile) {
			result.local_endpoint = link->end_a;
			result.opposite_endpoint = link->end_b;
		} else if (link->end_b.tile == tile) {
			result.local_endpoint = link->end_b;
			result.opposite_endpoint = link->end_a;
		}

		const bool local_ok = IsPhysicalRailGateHead(result.local_endpoint.tile, result.local_endpoint.enter_dir);
		const bool opposite_ok = IsPhysicalRailGateHead(result.opposite_endpoint.tile, result.opposite_endpoint.enter_dir);
		result.kind = link->IsValid() && local_ok && opposite_ok ? PortalGateKind::LocalLinked : PortalGateKind::LocalStale;
		return result;
	}

	return result;
}

bool PortalRegistry::CanEnterGate(TileIndex tile, DiagDirection vehicle_dir)
{
	PortalGateClassification gate = ClassifyGate(tile);
	if (gate.kind == PortalGateKind::None) return true;
	if (gate.kind != PortalGateKind::LocalLinked && gate.kind != PortalGateKind::ExternalLinked) return false;
	return gate.local_endpoint.enter_dir == vehicle_dir;
}

TileIndex PortalRegistry::GetOtherPortalEnd(TileIndex tile)
{
	const PortalLink *link = GetPortalLink(tile);
	if (link == nullptr) return INVALID_TILE;

	const PortalEndpoint *opp = link->GetOpposite(tile);
	return opp != nullptr ? opp->tile : INVALID_TILE;
}

PortalExitPosition PortalRegistry::GetPortalExitPosition(TileIndex entry_tile)
{
	PortalExitPosition pos;
	TileIndex exit_tile = GetOtherPortalEnd(entry_tile);
	if (exit_tile == INVALID_TILE) return pos;

	DiagDirection enter_dir = GetTunnelBridgeDirection(exit_tile);
	DiagDirection exit_vdir = ReverseDiagDir(enter_dir);

	/* OpenTTD tunnel visibility frames: NE=12, SE=8, SW=8, NW=12 */
	static constexpr DiagDirectionIndexArray<uint8_t> tunnel_vis_frame{12, 8, 8, 12};
	uint8_t frame = TILE_SIZE - tunnel_vis_frame[enter_dir];

	int offset_x = 8;
	int offset_y = 8;

	switch (exit_vdir) {
		case DiagDirection::NE:
			offset_x = TILE_SIZE - 1 - frame;
			break;
		case DiagDirection::SE:
			offset_y = frame;
			break;
		case DiagDirection::SW:
			offset_x = frame;
			break;
		case DiagDirection::NW:
			offset_y = TILE_SIZE - 1 - frame;
			break;
		default:
			break;
	}

	pos.tile = exit_tile;
	pos.x = TileX(exit_tile) * TILE_SIZE + offset_x;
	pos.y = TileY(exit_tile) * TILE_SIZE + offset_y;
	pos.z = GetSlopePixelZ(pos.x, pos.y, true);
	pos.dir = DiagDirToDir(exit_vdir);
	pos.track = DiagDirToDiagTrack(exit_vdir);
	return pos;
}

uint32_t PortalRegistry::GetPortalVirtualLength(TileIndex tile)
{
	const PortalLink *link = GetPortalLink(tile);
	if (link != nullptr) return link->virtual_length;
	const InterServerPortalLink *inter = GetInterServerPortal(tile);
	if (inter != nullptr) return inter->virtual_length;
	return 1;
}

const PortalLink *PortalRegistry::GetPortalLink(TileIndex tile)
{
	auto it = tile_to_portal.find(tile);
	if (it == tile_to_portal.end()) return nullptr;
	return GetPortalLinkByID(it->second);
}

const PortalLink *PortalRegistry::GetPortalLinkByID(PortalID id)
{
	auto it = portal_links.find(id.base());
	if (it == portal_links.end()) return nullptr;
	return &it->second;
}

uint32_t PortalRegistry::AdvancePortalTransit(VehicleID veh_id)
{
	return ++vehicle_portal_progress[veh_id.base()];
}

uint32_t PortalRegistry::GetPortalTransitProgress(VehicleID veh_id)
{
	auto it = vehicle_portal_progress.find(veh_id.base());
	return it != vehicle_portal_progress.end() ? it->second : 0;
}

void PortalRegistry::ClearPortalTransit(VehicleID veh_id)
{
	vehicle_portal_progress.erase(veh_id.base());
}

const std::unordered_map<uint32_t, PortalLink> &PortalRegistry::GetAllPortals()
{
	return portal_links;
}

const std::unordered_map<uint32_t, uint32_t> &PortalRegistry::GetAllVehicleTransit()
{
	return vehicle_portal_progress;
}

bool PortalRegistry::RestorePortalLink(const PortalLink &link)
{
	if (!link.IsValid()) return false;

	portal_links[link.id.base()] = link;
	tile_to_portal[link.end_a.tile] = link.id;
	tile_to_portal[link.end_b.tile] = link.id;

	if (link.id.base() >= next_portal_id) {
		next_portal_id = link.id.base() + 1;
	}
	return true;
}

void PortalRegistry::SetVehicleTransitProgress(VehicleID veh_id, uint32_t progress)
{
	vehicle_portal_progress[veh_id.base()] = progress;
}

size_t PortalRegistry::RepairLegacyGeneratedGateways()
{
	auto safe_adjacent_tile = [](TileIndex tile, DiagDirection dir) {
		int dx = 0;
		int dy = 0;
		switch (dir) {
			case DiagDirection::NE: dx = -1; break;
			case DiagDirection::SE: dy = 1; break;
			case DiagDirection::SW: dx = 1; break;
			case DiagDirection::NW: dy = -1; break;
			default: return INVALID_TILE;
		}
		return TileAddWrap(tile, dx, dy);
	};

	auto is_neutral_lead_track = [](TileIndex tile, Track expected_track) {
		return IsValidTile(tile) && IsPlainRailTile(tile) && GetTileOwner(tile) == OWNER_NONE &&
			GetTrackBits(tile).Test(expected_track);
	};

	size_t repaired = 0;
	for (auto &[id, link] : portal_links) {
		for (PortalEndpoint *endpoint : {&link.end_a, &link.end_b}) {
			TileIndex tile = endpoint->tile;
			if (!IsValidTile(tile) || !IsTunnelTile(tile) || GetTileOwner(tile) != OWNER_NONE) continue;

			DiagDirection old_dir = GetTunnelBridgeDirection(tile);
			DiagDirection new_dir = ReverseDiagDir(old_dir);
			Track expected_track = DiagDirToDiagTrack(new_dir);
			TileIndex old_forward_side = safe_adjacent_tile(tile, old_dir);
			TileIndex correct_entry_side = safe_adjacent_tile(tile, new_dir);

			/* The legacy generator put the lead track on old_forward_side. With
			 * old_dir this is the far side of the tunnel head, so trains cannot
			 * enter. Do not alter ambiguous portals that have track on both sides. */
			if (!is_neutral_lead_track(old_forward_side, expected_track) ||
					is_neutral_lead_track(correct_entry_side, expected_track)) {
				continue;
			}

			SB(Tile(tile).m5(), 0, 2, to_underlying(new_dir));
			endpoint->enter_dir = new_dir;
			repaired++;
		}
	}

	return repaired;
}

size_t PortalRegistry::Count()
{
	return portal_links.size();
}

Money PortalRegistry::GetCompanyPortalMaintenanceCost(Owner owner)
{
	if (owner >= MAX_COMPANIES) return 0;

	Money base_tunnel_cost = _price[Price::BuildTunnel] > 0 ? _price[Price::BuildTunnel] : Money(450);
	Money base_active_gate_cost = base_tunnel_cost / 4;
	Money base_unlinked_gate_cost = base_tunnel_cost / 8;
	Money total_cost = 0;

	for (const auto &[id, link] : portal_links) {
		bool owns_a = IsValidTile(link.end_a.tile) && GetTileOwner(link.end_a.tile) == owner;
		bool owns_b = IsValidTile(link.end_b.tile) && GetTileOwner(link.end_b.tile) == owner;

		if (owns_a) total_cost += base_active_gate_cost;
		if (owns_b) total_cost += base_active_gate_cost;

		if (owns_a && owns_b) {
			total_cost += static_cast<Money>(link.virtual_length * 50);
		} else if (owns_a || owns_b) {
			total_cost += static_cast<Money>(link.virtual_length * 25);
		}
	}

	for (const auto &[tile, link] : interserver_portals) {
		if (IsValidTile(tile) && GetTileOwner(tile) == owner) {
			total_cost += base_active_gate_cost + static_cast<Money>(link.virtual_length * 50);
		}
	}

	for (const auto &[tile, ep] : unlinked_gates) {
		if (IsValidTile(tile) && GetTileOwner(tile) == owner) {
			total_cost += base_unlinked_gate_cost;
		}
	}

	return total_cost;
}

static std::optional<DiagDirection> ResolveEndpointDirection(TileIndex tile)
{
	const PortalLink *link = PortalRegistry::GetPortalLink(tile);
	if (link != nullptr) {
		if (link->end_a.tile == tile) return link->end_a.enter_dir;
		if (link->end_b.tile == tile) return link->end_b.enter_dir;
	}

	const auto &unlinked = PortalRegistry::GetUnlinkedGates();
	auto it_un = unlinked.find(tile);
	if (it_un != unlinked.end()) return it_un->second.enter_dir;

	const auto &interserver = PortalRegistry::GetAllInterServerPortals();
	auto it_is = interserver.find(tile);
	if (it_is != interserver.end()) return it_is->second.local_endpoint.enter_dir;

	if (IsValidTile(tile) && IsTunnelTile(tile)) {
		return GetTunnelBridgeDirection(tile);
	}
	return std::nullopt;
}

bool PortalRegistry::IsTwinGateway(TileIndex tile_a, TileIndex tile_b)
{
	if (!IsValidTile(tile_a) || !IsValidTile(tile_b) || tile_a == tile_b) return false;
	if (!IsPortalTile(tile_a) && !IsUnlinkedGate(tile_a)) return false;
	if (!IsPortalTile(tile_b) && !IsUnlinkedGate(tile_b)) return false;

	if (DistanceManhattan(tile_a, tile_b) != 1) return false;

	auto dir_a = ResolveEndpointDirection(tile_a);
	auto dir_b = ResolveEndpointDirection(tile_b);
	if (!dir_a.has_value() || !dir_b.has_value()) return false;
	if (*dir_a != *dir_b) return false;

	DiagDirection gate_dir = *dir_a;
	bool perpendicular = false;
	for (DiagDirection d : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
		if (TileAddByDiagDir(tile_a, d) == tile_b) {
			if (d != gate_dir && d != ReverseDiagDir(gate_dir)) {
				perpendicular = true;
			}
			break;
		}
	}
	if (!perpendicular) return false;

	if (GetTileOwner(tile_a) != GetTileOwner(tile_b)) return false;

	return true;
}

TileIndex PortalRegistry::GetTwinGate(TileIndex tile)
{
	if (!IsValidTile(tile)) return INVALID_TILE;
	if (!IsPortalTile(tile) && !IsUnlinkedGate(tile)) return INVALID_TILE;

	for (DiagDirection d : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
		TileIndex adj = TileAddByDiagDir(tile, d);
		if (IsTwinGateway(tile, adj)) {
			return adj;
		}
	}
	return INVALID_TILE;
}

TileIndex PortalRegistry::ResolveGateTile(uint32_t gate_id, WorldID world_id)
{
	if (gate_id == 0) return INVALID_TILE;

	const PortalLink *pl = GetPortalLinkByID(PortalID(gate_id));
	if (pl != nullptr) {
		if (world_id != INVALID_WORLD) {
			if (pl->end_a.world_id == world_id) return pl->end_a.tile;
			if (pl->end_b.world_id == world_id) return pl->end_b.tile;
		}
		return pl->end_a.tile;
	}

	for (const auto &[tile, link] : interserver_portals) {
		if (link.id.base() == gate_id) {
			if (world_id == INVALID_WORLD || link.local_endpoint.world_id == world_id) {
				return tile;
			}
		}
	}

	TileIndex direct_tile = TileIndex(gate_id);
	if (IsValidTile(direct_tile) && (IsPortalTile(direct_tile) || IsUnlinkedGate(direct_tile))) {
		return direct_tile;
	}

	return INVALID_TILE;
}

void PortalRegistry::Reset()
{
	tile_to_portal.clear();
	portal_links.clear();
	unlinked_gates.clear();
	interserver_portals.clear();
	vehicle_portal_progress.clear();
	next_portal_id = 1;
	FederationIdentityRegistry::Reset();
	TransferJournal::Reset();
}
