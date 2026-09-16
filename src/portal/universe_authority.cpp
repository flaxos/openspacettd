/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_authority.cpp Centralized Universe Authority coordinator and transaction ledger. */

#include "../stdafx.h"
#include "universe_authority.h"
#include "consist_snapshot.h"
#include "planet_manager.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "../safeguards.h"

UniverseAuthorityService &UniverseAuthorityService::Instance()
{
	static UniverseAuthorityService instance;
	return instance;
}

void UniverseAuthorityService::Reset()
{
	this->_next_transfer_seq = 1;
	this->_worlds.clear();
	this->_routes.clear();
	this->_transfers.clear();
	this->_request_index.clear();
	this->_trade_balances.clear();
	this->_detailed_initiated.clear();
	this->_detailed_completed.clear();
	this->_detailed_in_transit.clear();
	this->_supply_chain_matrix = EmpireSupplyChainMatrix{};
}

bool UniverseAuthorityService::RegisterWorld(const RegisteredWorld &world)
{
	if (world.world_id == INVALID_WORLD) return false;
	this->_worlds[world.world_id] = world;
	return true;
}

bool UniverseAuthorityService::UnregisterWorld(WorldID world_id)
{
	return this->_worlds.erase(world_id) > 0;
}

const RegisteredWorld *UniverseAuthorityService::GetWorld(WorldID world_id) const
{
	auto it = this->_worlds.find(world_id);
	return it != this->_worlds.end() ? &it->second : nullptr;
}

std::vector<RegisteredWorld> UniverseAuthorityService::GetWorlds() const
{
	return this->GetWorldDirectory();
}

bool UniverseAuthorityService::UpdateWorldHeartbeat(
	WorldID world_id,
	uint32_t active_clients,
	uint32_t active_trains,
	uint64_t current_tick)
{
	auto it = this->_worlds.find(world_id);
	if (it == this->_worlds.end()) return false;

	it->second.active_clients = active_clients;
	it->second.active_trains = active_trains;
	it->second.last_heartbeat_tick = current_tick;
	it->second.status = WorldOnlineStatus::Online;
	return true;
}

size_t UniverseAuthorityService::PruneStaleWorlds(uint64_t current_tick, uint64_t timeout_ticks)
{
	size_t stale_count = 0;
	for (auto &[id, world] : this->_worlds) {
		if (world.status == WorldOnlineStatus::Online &&
		    (current_tick > world.last_heartbeat_tick + timeout_ticks)) {
			world.status = WorldOnlineStatus::Unreachable;
			stale_count++;
		}
	}
	return stale_count;
}

std::vector<RegisteredWorld> UniverseAuthorityService::FindWorldsByPhase(WorldPhase phase) const
{
	std::vector<RegisteredWorld> result;
	for (const auto &[id, world] : this->_worlds) {
		if (world.phase == phase) {
			result.push_back(world);
		}
	}
	return result;
}

std::vector<RegisteredWorld> UniverseAuthorityService::GetWorldDirectory() const
{
	std::vector<RegisteredWorld> result;
	result.reserve(this->_worlds.size());
	for (const auto &[id, world] : this->_worlds) {
		result.push_back(world);
	}
	return result;
}

std::vector<RegisteredWorld> UniverseAuthorityService::GetWorldDirectoryForGUI() const
{
	auto result = this->GetWorldDirectory();
	for (const PlanetRegion &region : PlanetManager::GetAllRegions()) {
		auto it = std::find_if(result.begin(), result.end(), [&](const RegisteredWorld &world) { return world.world_id == region.id; });
		if (it == result.end()) {
			RegisteredWorld world{};
			world.world_id = region.id;
			world.address = "Local Node";
			world.status = WorldOnlineStatus::Online;
			result.push_back(world);
			it = std::prev(result.end());
		}
		it->phase = region.phase;
		it->name = region.name;
		it->biome = region.biome;
	}
	std::sort(result.begin(), result.end(), [](const RegisteredWorld &a, const RegisteredWorld &b) { return a.world_id < b.world_id; });
	return result;
}

void UniverseAuthorityService::SyncLocalWorld(WorldID world_id)
{
	const PlanetRegion *region = PlanetManager::GetRegion(world_id);
	if (region == nullptr) return;
	auto it = this->_worlds.find(world_id);
	if (it == this->_worlds.end()) {
		RegisteredWorld world{};
		world.world_id = world_id;
		world.address = "Local Node";
		world.status = WorldOnlineStatus::Online;
		it = this->_worlds.emplace(world_id, std::move(world)).first;
	}
	it->second.phase = region->phase;
	it->second.name = region->name;
	it->second.biome = region->biome;
	if (region->phase == WorldPhase::Phase1_Core) {
		it->second.is_megacity = true;
		if (it->second.megacity_growth_state.empty()) it->second.megacity_growth_state = "Subsistence";
	}
}

bool UniverseAuthorityService::ColonizeWorld(WorldID world_id, const std::string &outpost_name)
{
	auto it = this->_worlds.find(world_id);
	if (it == this->_worlds.end()) return false;
	if (it->second.phase != WorldPhase::Phase4_Expansion) return false;

	it->second.phase = WorldPhase::Phase3_Frontier;
	if (!outpost_name.empty()) {
		it->second.name = outpost_name;
	}
	return true;
}

bool UniverseAuthorityService::PromoteWorld(WorldID world_id)
{
	auto it = this->_worlds.find(world_id);
	if (it == this->_worlds.end()) return false;
	switch (it->second.phase) {
		case WorldPhase::Phase4_Expansion:
			it->second.phase = WorldPhase::Phase3_Frontier;
			return true;
		case WorldPhase::Phase3_Frontier:
			it->second.phase = WorldPhase::Phase2_Developed;
			return true;
		case WorldPhase::Phase2_Developed:
			it->second.phase = WorldPhase::Phase1_Core;
			it->second.is_megacity = true;
			it->second.megacity_growth_state = "Subsistence";
			return true;
		case WorldPhase::Phase1_Core:
		default:
			return false;
	}
}

bool UniverseAuthorityService::UpdateMegacityStatus(
	WorldID world_id,
	bool is_megacity,
	const std::string &growth_state,
	float satisfaction_pct,
	uint32_t population)
{
	auto it = this->_worlds.find(world_id);
	if (it == this->_worlds.end()) return false;

	it->second.is_megacity = is_megacity;
	it->second.megacity_growth_state = growth_state;
	it->second.satisfaction_pct = satisfaction_pct;
	if (population > 0) {
		it->second.population = population;
	}
	return true;
}

bool UniverseAuthorityService::RegisterRoute(const InterServerRoute &route)
{
	if (route.route_id == 0 || route.source_world == INVALID_WORLD || route.dest_world == INVALID_WORLD) {
		return false;
	}
	this->_routes[route.route_id] = route;
	this->EvaluateCorridorCongestion(route.route_id);
	return true;
}

const InterServerRoute *UniverseAuthorityService::FindRoute(WorldID source_world, uint32_t source_gate_id) const
{
	for (const auto &[id, route] : this->_routes) {
		if (route.source_world == source_world && route.source_gate_id == source_gate_id) {
			return &route;
		}
	}
	return nullptr;
}

const InterServerRoute *UniverseAuthorityService::GetRoute(uint32_t route_id) const
{
	auto it = this->_routes.find(route_id);
	return it != this->_routes.end() ? &it->second : nullptr;
}

std::vector<InterServerRoute> UniverseAuthorityService::GetFreightCorridors() const
{
	std::vector<InterServerRoute> result;
	result.reserve(this->_routes.size());
	for (const auto &[id, route] : this->_routes) {
		result.push_back(route);
	}
	return result;
}

bool UniverseAuthorityService::UpdateCorridorLimits(uint32_t route_id, uint32_t max_bandwidth, uint32_t max_in_transit)
{
	auto it = this->_routes.find(route_id);
	if (it == this->_routes.end()) return false;
	it->second.max_bandwidth_trains_per_min = max_bandwidth;
	it->second.max_active_in_transit = max_in_transit;
	this->EvaluateCorridorCongestion(route_id);
	return true;
}

CorridorCongestionLevel UniverseAuthorityService::EvaluateCorridorCongestion(uint32_t route_id)
{
	auto it = this->_routes.find(route_id);
	if (it == this->_routes.end()) return CorridorCongestionLevel::Clear;

	uint32_t active = it->second.current_in_transit_count;
	uint32_t cap = it->second.max_active_in_transit;
	if (it->second.is_twin_array) cap *= 2;
	if (cap == 0) cap = 1;

	float util = static_cast<float>(active) / static_cast<float>(cap);
	CorridorCongestionLevel level;
	if (util < 0.5f) {
		level = CorridorCongestionLevel::Clear;
	} else if (util < 0.8f) {
		level = CorridorCongestionLevel::Moderate;
	} else if (util <= 1.0f) {
		level = CorridorCongestionLevel::Congested;
	} else {
		level = CorridorCongestionLevel::Saturated;
	}
	it->second.congestion_level = level;
	return level;
}

std::string UniverseAuthorityService::InitiateTransfer(
	WorldID source_world,
	WorldID dest_world,
	uint32_t source_gate_id,
	uint32_t dest_gate_id,
	const ConsistSnapshotBytes &snapshot_bytes,
	uint32_t transit_duration_ticks,
	FreightPriority priority,
	const std::string &request_id)
{
	if (source_world == INVALID_WORLD || dest_world == INVALID_WORLD || !snapshot_bytes.Succeeded() || snapshot_bytes.bytes.empty()) {
		return "";
	}

	if (!request_id.empty()) {
		auto req_it = this->_request_index.find({source_world, request_id});
		if (req_it != this->_request_index.end()) {
			auto tx_it = this->_transfers.find(req_it->second);
			if (tx_it == this->_transfers.end()) {
				this->_request_index.erase(req_it);
			} else {
				const UniverseTransferRecord &existing = tx_it->second;
				const bool same_request =
					existing.source_world == source_world &&
					existing.dest_world == dest_world &&
					existing.source_gate_id == source_gate_id &&
					existing.dest_gate_id == dest_gate_id &&
					existing.priority == priority &&
					existing.snapshot_bytes.bytes == snapshot_bytes.bytes;
				return same_request ? existing.transfer_id : "";
			}
		}
	}

	std::ostringstream ss;
	ss << "TRANSFER-X" << std::setw(9) << std::setfill('0') << this->_next_transfer_seq++;
	std::string tx_id = ss.str();

	UniverseTransferRecord rec;
	rec.transfer_id = tx_id;
	rec.request_id = request_id;
	rec.source_world = source_world;
	rec.dest_world = dest_world;
	rec.source_gate_id = source_gate_id;
	rec.dest_gate_id = dest_gate_id;
	rec.snapshot_bytes = snapshot_bytes;
	rec.state = TransferState::Locked;
	rec.priority = priority;

	/* Parse snapshot to count total cargo units for commodity ledger */
	ContentManifestToken token{};
	if (snapshot_bytes.bytes.size() >= 12 + token.size()) {
		std::copy_n(snapshot_bytes.bytes.begin() + 12, token.size(), token.begin());
	}
	auto decode_res = ConsistSnapshotCodec::Decode(snapshot_bytes.bytes, token);
	if (decode_res.Succeeded()) {
		rec.snapshot = *decode_res.snapshot;
		for (const auto &unit : rec.snapshot.units) {
			rec.total_cargo_units += unit.cargo_count;
			if (unit.cargo_count > 0) {
				rec.cargo_by_type[unit.cargo_type] += unit.cargo_count;
				this->_detailed_initiated[unit.cargo_type] += unit.cargo_count;
				this->_detailed_in_transit[unit.cargo_type] += unit.cargo_count;

				this->_trade_balances[source_world].world_id = source_world;
				this->_trade_balances[source_world].exported_cargo[unit.cargo_type] += unit.cargo_count;
			}
		}
	}

	/* Look up route duration and apply corridor congestion scaling */
	InterServerRoute *route = nullptr;
	for (auto &[id, r] : this->_routes) {
		if (r.source_world == source_world && r.source_gate_id == source_gate_id) {
			route = &r;
			break;
		}
	}

	uint32_t base_duration = (route != nullptr) ? route->transit_duration_ticks : transit_duration_ticks;
	float multiplier = 1.0f;
	if (route != nullptr) {
		rec.route_id = route->route_id;
		route->current_in_transit_count++;
		route->total_trains_dispatched++;
		this->EvaluateCorridorCongestion(route->route_id);

		switch (route->congestion_level) {
			case CorridorCongestionLevel::Clear:
				multiplier = 1.0f;
				break;
			case CorridorCongestionLevel::Moderate:
				multiplier = 1.2f;
				break;
			case CorridorCongestionLevel::Congested:
				multiplier = 1.5f;
				break;
			case CorridorCongestionLevel::Saturated:
				multiplier = 2.0f;
				break;
		}

		/* High-priority QoS mitigation: reduce congestion penalty by 50% */
		if (priority == FreightPriority::Express || priority == FreightPriority::PriorityUrgent) {
			float penalty = multiplier - 1.0f;
			multiplier = 1.0f + (penalty * 0.5f);
		}

		/* Twin gateway array mitigation: reduce congestion penalty by 50% */
		if (route->is_twin_array && multiplier > 1.0f) {
			float penalty = multiplier - 1.0f;
			multiplier = 1.0f + (penalty * 0.5f);
		}
	}

	rec.effective_transit_ticks = static_cast<uint32_t>(base_duration * multiplier);
	rec.arrival_tick = rec.effective_transit_ticks;

	/* Track supply chain matrix flows between phases */
	const auto *src_w = this->GetWorld(source_world);
	const auto *dst_w = this->GetWorld(dest_world);
	if (src_w != nullptr && dst_w != nullptr && rec.total_cargo_units > 0) {
		this->_supply_chain_matrix.total_interplanetary_cargo += rec.total_cargo_units;
		this->_supply_chain_matrix.total_tariffs_generated += (rec.total_cargo_units * 10);

		if (src_w->phase == WorldPhase::Phase3_Frontier && dst_w->phase == WorldPhase::Phase2_Developed) {
			this->_supply_chain_matrix.frontier_to_refinery_cargo += rec.total_cargo_units;
		} else if (src_w->phase == WorldPhase::Phase2_Developed && dst_w->phase == WorldPhase::Phase1_Core) {
			this->_supply_chain_matrix.refinery_to_core_cargo += rec.total_cargo_units;
		} else if (src_w->phase == WorldPhase::Phase3_Frontier && dst_w->phase == WorldPhase::Phase1_Core) {
			this->_supply_chain_matrix.frontier_to_core_cargo += rec.total_cargo_units;
		} else if (src_w->phase == WorldPhase::Phase1_Core) {
			this->_supply_chain_matrix.core_export_cargo += rec.total_cargo_units;
		}
	}

	this->_transfers[tx_id] = std::move(rec);
	if (!request_id.empty()) {
		this->_request_index[{source_world, request_id}] = tx_id;
	}
	return tx_id;
}

bool UniverseAuthorityService::DepartTransfer(const std::string &transfer_id, uint64_t current_tick)
{
	auto it = this->_transfers.find(transfer_id);
	if (it == this->_transfers.end()) return false;

	if (it->second.state == TransferState::InTransit) return true;

	if (it->second.state != TransferState::Locked && it->second.state != TransferState::Preparing) {
		return false;
	}

	uint32_t duration = it->second.effective_transit_ticks;
	it->second.departure_tick = current_tick;
	it->second.arrival_tick = current_tick + duration;
	it->second.state = TransferState::InTransit;
	return true;
}

std::vector<std::string> UniverseAuthorityService::QueryPendingTransfers(WorldID dest_world, uint64_t current_tick) const
{
	std::vector<std::string> pending;
	for (const auto &[id, rec] : this->_transfers) {
		if (rec.dest_world == dest_world &&
				(rec.state == TransferState::InTransit || rec.state == TransferState::ArrivalPending) &&
				current_tick >= rec.arrival_tick) {
			pending.push_back(id);
		}
	}
	return pending;
}

std::optional<UniverseTransferRecord> UniverseAuthorityService::ClaimTransfer(const std::string &transfer_id, WorldID dest_world)
{
	auto it = this->_transfers.find(transfer_id);
	if (it == this->_transfers.end()) return std::nullopt;

	if (it->second.dest_world != dest_world) {
		return std::nullopt;
	}

	if (it->second.state == TransferState::ArrivalPending) return it->second;
	if (it->second.state != TransferState::InTransit) return std::nullopt;

	it->second.state = TransferState::ArrivalPending;
	return it->second;
}

bool UniverseAuthorityService::ConfirmTransferArrival(
	const std::string &transfer_id,
	WorldID dest_world,
	bool success,
	const std::string &reason)
{
	auto it = this->_transfers.find(transfer_id);
	if (it == this->_transfers.end()) return false;

	if (it->second.dest_world != dest_world) {
		return false;
	}
	if (it->second.state == TransferState::Completed && success) return true;
	if (it->second.state != TransferState::ArrivalPending) return false;

	if (success) {
		it->second.state = TransferState::Completed;
		it->second.status_message = "Delivered successfully";

		/* Advance order progression if consist has an itinerary */
		if (!it->second.snapshot.orders.empty()) {
			it->second.snapshot.current_order_index = static_cast<uint16_t>((it->second.snapshot.current_order_index + 1) % it->second.snapshot.orders.size());
		}

		/* Update detailed commodity ledger and trade balances */
		for (const auto &[cargo_type, count] : it->second.cargo_by_type) {
			this->_detailed_completed[cargo_type] += count;
			if (this->_detailed_in_transit[cargo_type] >= count) {
				this->_detailed_in_transit[cargo_type] -= count;
			} else {
				this->_detailed_in_transit[cargo_type] = 0;
			}

			this->_trade_balances[dest_world].world_id = dest_world;
			this->_trade_balances[dest_world].imported_cargo[cargo_type] += count;

			/* Settle transport valuation: 10 credits per cargo unit */
			int64_t trade_val = static_cast<int64_t>(count) * 10;
			this->_trade_balances[it->second.source_world].net_trade_balance_credits += trade_val;
			this->_trade_balances[dest_world].net_trade_balance_credits -= trade_val;
		}

		/* Relieve corridor congestion */
		if (it->second.route_id != 0) {
			auto r_it = this->_routes.find(it->second.route_id);
			if (r_it != this->_routes.end()) {
				if (r_it->second.current_in_transit_count > 0) {
					r_it->second.current_in_transit_count--;
				}
				this->EvaluateCorridorCongestion(r_it->first);
			}
		}
	} else {
		it->second.state = TransferState::RecoveryRequired;
		it->second.status_message = reason.empty() ? "Materialization failed" : reason;
		/* Failed goods remain in transit/recovery ledger */
	}
	return true;
}

size_t UniverseAuthorityService::QuarantineTransfersForWorld(WorldID dest_world, const std::string &reason)
{
	size_t count = 0;
	for (auto &[id, rec] : this->_transfers) {
		if (rec.dest_world == dest_world && (rec.state == TransferState::InTransit || rec.state == TransferState::ArrivalPending || rec.state == TransferState::Locked)) {
			rec.state = TransferState::RecoveryRequired;
			rec.status_message = reason.empty() ? "Quarantined due to destination node drop" : reason;
			count++;
		}
	}
	return count;
}

size_t UniverseAuthorityService::RecoverTransfersForWorld(WorldID dest_world)
{
	size_t count = 0;
	for (auto &[id, rec] : this->_transfers) {
		if (rec.dest_world == dest_world && rec.state == TransferState::RecoveryRequired) {
			rec.state = TransferState::InTransit;
			rec.arrival_tick = 0;
			rec.status_message = "Recovered from quarantine bay; ready for arrival";
			count++;
		}
	}
	return count;
}

std::vector<std::string> UniverseAuthorityService::GetQuarantinedTransfers(WorldID dest_world) const
{
	std::vector<std::string> results;
	for (const auto &[id, rec] : this->_transfers) {
		if (rec.state == TransferState::RecoveryRequired) {
			if (dest_world != INVALID_WORLD && rec.dest_world != dest_world) continue;
			results.push_back(id);
		}
	}
	return results;
}

const UniverseTransferRecord *UniverseAuthorityService::GetTransfer(const std::string &transfer_id) const
{
	auto it = this->_transfers.find(transfer_id);
	return it != this->_transfers.end() ? &it->second : nullptr;
}

std::vector<UniverseTransferRecord> UniverseAuthorityService::GetAllTransfers() const
{
	std::vector<UniverseTransferRecord> list;
	list.reserve(this->_transfers.size());
	for (const auto &[id, rec] : this->_transfers) {
		list.push_back(rec);
	}
	return list;
}

std::vector<UniverseTransferRecord> UniverseAuthorityService::GetInTransitTransfersForRoute(uint32_t route_id) const
{
	std::vector<UniverseTransferRecord> result;
	for (const auto &[id, rec] : this->_transfers) {
		if ((route_id == 0 || rec.route_id == route_id) &&
		    (rec.state == TransferState::InTransit ||
		     rec.state == TransferState::ArrivalPending ||
		     rec.state == TransferState::Departed ||
		     rec.state == TransferState::Locked)) {
			result.push_back(rec);
		}
	}
	return result;
}

CommodityAuditResult UniverseAuthorityService::GetCommodityAudit() const
{
	CommodityAuditResult audit;
	for (const auto &[id, rec] : this->_transfers) {
		audit.total_transfers_initiated++;
		audit.total_cargo_initiated += rec.total_cargo_units;

		switch (rec.state) {
			case TransferState::Completed:
				audit.total_transfers_completed++;
				audit.total_cargo_completed += rec.total_cargo_units;
				break;
			case TransferState::InTransit:
			case TransferState::ArrivalPending:
			case TransferState::Locked:
			case TransferState::Preparing:
			case TransferState::Departed:
			case TransferState::Failed:
			case TransferState::RecoveryRequired:
			case TransferState::Idle:
			default:
				audit.total_transfers_in_transit++;
				audit.total_cargo_in_transit += rec.total_cargo_units;
				break;
		}
	}
	return audit;
}

DetailedCommodityAudit UniverseAuthorityService::GetDetailedCommodityAudit() const
{
	DetailedCommodityAudit audit;
	audit.cargo_initiated = this->_detailed_initiated;
	audit.cargo_completed = this->_detailed_completed;
	audit.cargo_in_transit = this->_detailed_in_transit;
	return audit;
}

TradeBalanceSummary UniverseAuthorityService::GetWorldTradeBalance(WorldID world_id) const
{
	auto it = this->_trade_balances.find(world_id);
	if (it != this->_trade_balances.end()) {
		return it->second;
	}
	TradeBalanceSummary summary;
	summary.world_id = world_id;
	return summary;
}

std::map<WorldID, TradeBalanceSummary> UniverseAuthorityService::GetAllTradeBalances() const
{
	return this->_trade_balances;
}

EmpireSupplyChainMatrix UniverseAuthorityService::GetEmpireSupplyChainMatrix() const
{
	return this->_supply_chain_matrix;
}

void UniverseAuthorityService::RecordSpaceportThroughput(uint64_t cargo_units)
{
	this->_supply_chain_matrix.spaceport_throughput_cargo += cargo_units;
}

void UniverseAuthorityService::RecordEdgeConduitThroughput(uint64_t cargo_units)
{
	this->_supply_chain_matrix.edge_conduit_throughput_cargo += cargo_units;
}
