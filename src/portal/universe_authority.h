/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_authority.h Centralized Universe Authority coordinator and transaction ledger. */

#ifndef UNIVERSE_AUTHORITY_H
#define UNIVERSE_AUTHORITY_H

#include "consist_snapshot.h"
#include "planet_type.h"
#include "portal_type.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/** Lifecycle states of an inter-server consist transfer. */
enum class TransferState : uint8_t {
	Idle             = 0, ///< No transfer active.
	Preparing        = 1, ///< Transfer initiated; consist locked against modification.
	Locked           = 2, ///< Snapshot captured and serialized.
	Departed         = 3, ///< Train despawned from source server and reservations freed.
	InTransit        = 4, ///< Authority holding consist in transit; transit timer running.
	ArrivalPending   = 5, ///< Claimed by destination server; awaiting throat clearance.
	Completed        = 6, ///< Train materialized successfully on destination server.
	Failed           = 7, ///< Materialization failed or cancelled.
	RecoveryRequired = 8, ///< Timeout or network drop; placed into recovery bay.
};

/** Live connectivity status of a world server. */
enum class WorldOnlineStatus : uint8_t {
	Online = 0,
	Unreachable = 1,
	Maintenance = 2,
};

/** Metadata and live directory entry for a world server registered with the Universe Authority. */
struct RegisteredWorld {
	WorldID world_id = INVALID_WORLD;
	WorldPhase phase = WorldPhase::Phase3_Frontier;
	std::string name;
	ContentManifestToken content_manifest{};
	uint64_t last_heartbeat_tick = 0;
	std::string address;
	std::string description;
	uint32_t active_clients = 0;
	uint32_t max_clients = 32;
	uint32_t active_trains = 0;
	WorldOnlineStatus status = WorldOnlineStatus::Online;
};

/** Freight corridor congestion level determined by active transit utilization. */
enum class CorridorCongestionLevel : uint8_t {
	Clear     = 0, ///< < 50% capacity: Normal 1.0x transit duration.
	Moderate  = 1, ///< 50-80% capacity: 1.2x transit duration.
	Congested = 2, ///< 80-100% capacity: 1.5x transit duration.
	Saturated = 3, ///< > 100% capacity: 2.0x transit duration (backpressure delay).
};

/** Quality of Service priority tier for inter-world freight shipments. */
enum class FreightPriority : uint8_t {
	Bulk           = 0, ///< Low priority bulk raw freight.
	Standard       = 1, ///< Default standard freight.
	Express        = 2, ///< High-speed goods/perishables (50% congestion penalty reduction).
	PriorityUrgent = 3, ///< Highest priority emergency/VIP (50% congestion penalty reduction).
};

/** Pre-configured or dynamic inter-server portal route between two world servers. */
struct InterServerRoute {
	uint32_t route_id = 0;
	WorldID source_world = INVALID_WORLD;
	uint32_t source_gate_id = 0;
	WorldID dest_world = INVALID_WORLD;
	uint32_t dest_gate_id = 0;
	uint32_t transit_duration_ticks = 100;
	uint32_t max_bandwidth_trains_per_min = 10;
	uint32_t max_active_in_transit = 8;
	FreightPriority priority = FreightPriority::Standard;
	CorridorCongestionLevel congestion_level = CorridorCongestionLevel::Clear;
	uint32_t current_in_transit_count = 0;
	uint64_t total_trains_dispatched = 0;
};

/** Transactional record for a train consist transfer across server boundaries. */
struct UniverseTransferRecord {
	std::string transfer_id;
	WorldID source_world = INVALID_WORLD;
	WorldID dest_world = INVALID_WORLD;
	uint32_t source_gate_id = 0;
	uint32_t dest_gate_id = 0;
	ConsistSnapshotBytes snapshot_bytes{};
	ConsistSnapshot snapshot{};
	TransferState state = TransferState::Idle;
	uint64_t departure_tick = 0;
	uint64_t arrival_tick = 0;
	uint32_t total_cargo_units = 0;
	std::map<uint8_t, uint32_t> cargo_by_type{};
	std::string status_message;
	FreightPriority priority = FreightPriority::Standard;
	uint32_t effective_transit_ticks = 100;
	uint32_t route_id = 0;
};

/** Empire-wide supply chain aggregation across developmental world phases. */
struct EmpireSupplyChainMatrix {
	uint64_t frontier_to_refinery_cargo = 0; ///< Raw inputs feeding manufacturing (Phase 3 -> 2).
	uint64_t refinery_to_core_cargo     = 0; ///< Refined goods/materials feeding megacity (Phase 2 -> 1).
	uint64_t frontier_to_core_cargo     = 0; ///< Direct raw shipments to megacity (Phase 3 -> 1).
	uint64_t core_export_cargo          = 0; ///< High-tech and consumer exports from core (Phase 1 -> Any).
	uint64_t total_interplanetary_cargo = 0;
	uint64_t total_tariffs_generated    = 0; ///< Interplanetary trade premiums credited (Cr).
};

/** Commodity conservation metrics for ledger verification. */
struct CommodityAuditResult {
	uint64_t total_transfers_initiated = 0;
	uint64_t total_transfers_completed = 0;
	uint64_t total_transfers_in_transit = 0;
	uint64_t total_cargo_initiated = 0;
	uint64_t total_cargo_completed = 0;
	uint64_t total_cargo_in_transit = 0;

	/** Verifies strict commodity conservation: initial == completed + in_transit. */
	constexpr bool IsConserved() const noexcept
	{
		return total_cargo_initiated == (total_cargo_completed + total_cargo_in_transit);
	}
};

/** Inter-world trade balance accounting for one registered world. */
struct TradeBalanceSummary {
	WorldID world_id = INVALID_WORLD;
	std::map<uint8_t, uint64_t> exported_cargo{};
	std::map<uint8_t, uint64_t> imported_cargo{};
	int64_t net_trade_balance_credits = 0;
};

/** Per-cargo-type commodity conservation audit. */
struct DetailedCommodityAudit {
	std::map<uint8_t, uint64_t> cargo_initiated{};
	std::map<uint8_t, uint64_t> cargo_completed{};
	std::map<uint8_t, uint64_t> cargo_in_transit{};

	bool IsConserved() const noexcept
	{
		for (const auto &[cargo, init] : cargo_initiated) {
			uint64_t comp = cargo_completed.contains(cargo) ? cargo_completed.at(cargo) : 0;
			uint64_t in_trans = cargo_in_transit.contains(cargo) ? cargo_in_transit.at(cargo) : 0;
			if (init != comp + in_trans) return false;
		}
		return true;
	}
};

/**
 * Authoritative Universe Authority service managing world registration,
 * world discovery directory, inter-server routes, transfer state machines,
 * per-cargo commodity ledgers, and trade balance accounting.
 */
class UniverseAuthorityService {
public:
	static UniverseAuthorityService &Instance();

	/* World Registration & Directory */
	bool RegisterWorld(const RegisteredWorld &world);
	bool UnregisterWorld(WorldID world_id);
	const RegisteredWorld *GetWorld(WorldID world_id) const;
	std::vector<RegisteredWorld> GetWorlds() const;
	bool UpdateWorldHeartbeat(WorldID world_id, uint32_t active_clients, uint32_t active_trains, uint64_t current_tick);
	size_t PruneStaleWorlds(uint64_t current_tick, uint64_t timeout_ticks = 300);
	std::vector<RegisteredWorld> FindWorldsByPhase(WorldPhase phase) const;
	std::vector<RegisteredWorld> GetWorldDirectory() const;

	/* Inter-Server Route & Freight Corridor Management */
	bool RegisterRoute(const InterServerRoute &route);
	const InterServerRoute *FindRoute(WorldID source_world, uint32_t source_gate_id) const;
	const InterServerRoute *GetRoute(uint32_t route_id) const;
	std::vector<InterServerRoute> GetFreightCorridors() const;
	bool UpdateCorridorLimits(uint32_t route_id, uint32_t max_bandwidth, uint32_t max_in_transit);
	CorridorCongestionLevel EvaluateCorridorCongestion(uint32_t route_id);

	/* Transfer Handoff State Machine */
	std::string InitiateTransfer(
		WorldID source_world,
		WorldID dest_world,
		uint32_t source_gate_id,
		uint32_t dest_gate_id,
		const ConsistSnapshotBytes &snapshot_bytes,
		uint32_t transit_duration_ticks = 100,
		FreightPriority priority = FreightPriority::Standard
	);

	bool DepartTransfer(const std::string &transfer_id, uint64_t current_tick);

	std::vector<std::string> QueryPendingTransfers(WorldID dest_world, uint64_t current_tick) const;

	std::optional<UniverseTransferRecord> ClaimTransfer(const std::string &transfer_id, WorldID dest_world);

	bool ConfirmTransferArrival(
		const std::string &transfer_id,
		WorldID dest_world,
		bool success,
		const std::string &reason = ""
	);

	const UniverseTransferRecord *GetTransfer(const std::string &transfer_id) const;
	std::vector<UniverseTransferRecord> GetAllTransfers() const;

	/* Commodity Conservation & Trade Balance Ledger */
	CommodityAuditResult GetCommodityAudit() const;
	DetailedCommodityAudit GetDetailedCommodityAudit() const;
	TradeBalanceSummary GetWorldTradeBalance(WorldID world_id) const;
	std::map<WorldID, TradeBalanceSummary> GetAllTradeBalances() const;
	EmpireSupplyChainMatrix GetEmpireSupplyChainMatrix() const;

	void Reset();

private:
	uint64_t _next_transfer_seq = 1;
	std::map<WorldID, RegisteredWorld> _worlds;
	std::map<uint32_t, InterServerRoute> _routes;
	std::map<std::string, UniverseTransferRecord> _transfers;
	std::map<WorldID, TradeBalanceSummary> _trade_balances;
	std::map<uint8_t, uint64_t> _detailed_initiated;
	std::map<uint8_t, uint64_t> _detailed_completed;
	std::map<uint8_t, uint64_t> _detailed_in_transit;
	EmpireSupplyChainMatrix _supply_chain_matrix{};
};

#endif /* UNIVERSE_AUTHORITY_H */
