/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file prebuilt_trade.h Prebuilt simulated trade gateways and economy proxy engine for Sprint 49 (WP-49.3 & WP-49.4). */

#ifndef PREBUILT_TRADE_H
#define PREBUILT_TRADE_H

#include "consist_snapshot.h"
#include "planet_type.h"
#include "portal_type.h"
#include "production_chain.h"
#include "universe_graph.h"
#include "../company_type.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

/** Status of an off-world simulated trade transaction. */
enum class TradeTransactionStatus : uint8_t {
	Queued,     ///< Outbound consist entered gate; tariff credited; awaiting return timer.
	InTransit,  ///< Return consist in transit through wormhole proxy.
	Arrived,    ///< Return consist reached gate head and materialized / completed.
	Failed,     ///< Trade rejected or unresolvable.
};

/** Record of a scheduled return consist from an off-world Commonwealth trade partner. */
struct ScheduledTradeReturn {
	std::string trade_id{};
	TileIndex portal_tile = INVALID_TILE;
	WorldID local_world = INVALID_WORLD;
	std::string target_world_id{};
	std::string target_world_name{};
	uint64_t dispatch_tick = 0;
	uint64_t arrival_tick = 0;
	int64_t tariff_credited = 0;
	CommonwealthCargoID exported_cargo_id = CommonwealthCargoID::StructuralSteel;
	uint32_t exported_cargo_units = 0;
	CommonwealthCargoID return_cargo_id = CommonwealthCargoID::EnrichedQuantumCrystals;
	uint32_t return_cargo_units = 0;
	ConsistSnapshot return_snapshot{};
	TradeTransactionStatus status = TradeTransactionStatus::Queued;
};

/** Configuration and metrics for a local portal gate linked to a prebuilt Commonwealth trade partner. */
struct PrebuiltTradeGateway {
	TileIndex portal_tile = INVALID_TILE;
	uint32_t gate_id = 0;
	WorldID local_world = INVALID_WORLD;
	std::string target_world_id{};
	std::string target_world_name{};
	uint32_t virtual_length_tiles = 32;
	float tariff_multiplier = 1.0f;
	uint64_t total_trains_exported = 0;
	uint64_t total_trains_imported = 0;
	uint64_t total_cargo_exported = 0;
	uint64_t total_cargo_imported = 0;
	int64_t total_tariffs_earned = 0;
	TileIndex parallel_throat_tile = INVALID_TILE; ///< Auxiliary parallel throat tile for multi-track gateway returns.
	bool active = true;
};

/**
 * Manager handling simulated off-world Commonwealth trade partners, proxy cargo consumption,
 * freight tariffs, and scheduled return consists without requiring external dedicated servers.
 */
class PrebuiltTradeManager {
public:
	/**
	 * Get singleton instance of PrebuiltTradeManager.
	 * @return Reference to the PrebuiltTradeManager singleton.
	 */
	static PrebuiltTradeManager &Instance();

	/**
	 * Register a local portal gate as a simulated trade gateway to an off-world Commonwealth node.
	 *
	 * @param portal_tile The local portal entrance tile.
	 * @param target_world_id Commonwealth world ID (e.g. "world_augusta", "world_merredin").
	 * @param local_world Local world region containing the portal tile.
	 * @param virtual_length Route length in tiles for transit delay and tariff calculation.
	 * @return True if successfully registered.
	 */
	bool RegisterTradeGateway(
		TileIndex portal_tile,
		const std::string &target_world_id,
		WorldID local_world,
		uint32_t virtual_length = 32
	);

	/**
	 * Configure an auxiliary parallel throat tile for multi-track trade gateway operations.
	 */
	bool ConfigureGatewayParallelThroat(TileIndex portal_tile, TileIndex secondary_throat_tile);

	/**
	 * Get the auxiliary parallel throat tile for a trade gateway, or INVALID_TILE if none configured.
	 */
	TileIndex GetGatewayParallelThroat(TileIndex portal_tile) const;

	/**
	 * Unregister a trade gateway by tile.
	 * @param portal_tile Portal entrance tile to unbind.
	 * @return True if unregister succeeded, false if not found.
	 */
	bool UnregisterTradeGateway(TileIndex portal_tile);

	/**
	 * Check if a portal tile is a registered prebuilt trade gateway.
	 * @param portal_tile Portal tile to test.
	 * @return True if tile is registered as an active trade gateway.
	 */
	bool IsTradeGateway(TileIndex portal_tile) const;

	/**
	 * Get trade gateway metadata for a tile.
	 * @param portal_tile Portal tile to query.
	 * @return Pointer to PrebuiltTradeGateway, or nullptr if not registered.
	 */
	const PrebuiltTradeGateway *GetTradeGateway(TileIndex portal_tile) const;

	/**
	 * Get all registered trade gateways.
	 * @return Vector of all registered trade gateway records.
	 */
	std::vector<PrebuiltTradeGateway> GetAllTradeGateways() const;

	/**
	 * Process an outbound consist entering a prebuilt trade gateway.
	 * Despawns the train into authority custody, calculates tariff revenue,
	 * credits the company treasury, and schedules an inbound return consist.
	 *
	 * @param portal_tile Gateway entrance tile.
	 * @param consist Consist snapshot captured at entry.
	 * @param current_tick Current simulation tick.
	 * @param company Owning company of the consist.
	 * @return Trade transaction ID, or empty string on failure.
	 */
	std::string DispatchOutboundConsist(
		TileIndex portal_tile,
		const ConsistSnapshot &consist,
		uint64_t current_tick,
		CompanyID company
	);

	/**
	 * Advance simulation and process any return consists whose transit delay has elapsed.
	 *
	 * @param current_tick Current simulation tick.
	 * @return Number of return consists that arrived and completed.
	 */
	size_t ProcessScheduledReturns(uint64_t current_tick);

	/**
	 * Get a scheduled trade return by trade ID.
	 * @param trade_id Unique trade transaction ID.
	 * @return Pointer to ScheduledTradeReturn, or nullptr if not found.
	 */
	const ScheduledTradeReturn *GetTradeReturn(const std::string &trade_id) const;

	/**
	 * Get all pending or in-transit trade returns.
	 * @return Vector of pending trade returns.
	 */
	std::vector<ScheduledTradeReturn> GetPendingReturns() const;

	/**
	 * Calculate freight tariff revenue based on cargo volume, target world multiplier, and distance.
	 *
	 * Tariff = cargo_units * BASE_TARIFF_PER_UNIT * tariff_multiplier * (1 + virtual_length / 100)
	 * @param cargo_units Number of cargo units carried.
	 * @param tariff_multiplier Target world economic tariff multiplier.
	 * @param virtual_length_tiles Virtual route distance in tiles.
	 * @return Calculated tariff revenue in credits.
	 */
	static int64_t CalculateFreightTariff(
		uint32_t cargo_units,
		float tariff_multiplier,
		uint32_t virtual_length_tiles
	) noexcept;

	/**
	 * Resolve a canonical export string (e.g. "SUPERALLOYS", "QUANTUM_CRYSTALS") to CommonwealthCargoID.
	 * @param export_name Export cargo name identifier.
	 * @return Corresponding CommonwealthCargoID enum.
	 */
	static CommonwealthCargoID ResolveExportCargoID(const std::string &export_name) noexcept;

	/**
	 * Restore a trade gateway record from savegame serialization.
	 * @param gw Trade gateway record to restore.
	 */
	void RestoreGateway(const PrebuiltTradeGateway &gw);

	/** Clear all registered gateways and scheduled returns (for test isolation). */
	void Reset();

private:
	PrebuiltTradeManager() = default;

	static constexpr int64_t BASE_TARIFF_PER_UNIT = 100; ///< Base tariff credits per cargo unit.

	uint64_t _next_trade_seq = 1;
	std::map<TileIndex, PrebuiltTradeGateway> _gateways{};
	std::map<std::string, ScheduledTradeReturn> _scheduled_returns{};
};

#endif /* PREBUILT_TRADE_H */
