/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file connected_economy.h Lifecycle of isolated connected-economy proof authorization. */

#ifndef CONNECTED_ECONOMY_H
#define CONNECTED_ECONOMY_H

#include <cstdint>
#include <string>
#include <string_view>
#include "../3rdparty/nlohmann/json.hpp"
#include "../command_type.h"
#include "../tile_type.h"
#include "../track_type.h"

/** Invocation budget shared by every station candidate, including failed searches. */
struct CoreBasketSearchBudget {
	static constexpr uint32_t LIMIT = 30000; ///< Maximum predecessor allocations per invocation.
	uint32_t states = 0; ///< All predecessor allocations in this invocation.
	uint32_t candidates = 0; ///< Station candidates examined in this invocation.
	bool exhausted = false; ///< A search attempted to exceed the invocation limit.
	/** @return Whether another predecessor may be allocated within the shared bound. */
	bool TryState()
	{
		if (this->states >= LIMIT) { this->exhausted = true; return false; }
		++this->states;
		return true;
	}
};

/** Native environment observations used by the separately guarded loaded adapter. */
struct CoreBasketAdmissionContext {
	bool normal = false; ///< Ordinary gameplay mode is active.
	bool offline = false; ///< No network session can accept commands.
	bool paused = false; ///< The saved simulation is paused.
	bool profile = false; ///< Exact canonical settings and content are loaded.
	bool loaded = false; ///< Authorization comes from a loaded save, not generation.
	uint32_t companies = 0; ///< Number of native companies in the save.
};

/** Native environment for the one offline Core town station repair. */
struct CoreTownStationAdmissionContext {
	bool normal = false; ///< Ordinary gameplay mode is active.
	bool offline = false; ///< No network session can accept commands.
	bool paused = false; ///< The saved simulation is paused.
	bool profile = false; ///< Exact canonical settings and content are loaded.
	bool loaded = false; ///< Authorization comes from a loaded save, not generation.
	bool competing_basket = false; ///< Competing basket operations are absent.
	bool shared_orders = false; ///< Shared order list constraints are respected.
	uint32_t companies = 0; ///< Number of native companies in the save.
};

struct Train;

/**
 * Read-only admission for the inherited FOOD train and its ordinary orders.
 * @param state Paused native checkpoint projection.
 * @param context Verified native environment conditions.
 * @return Empty on admission, otherwise denial reason.
 */
std::string GetCoreTownStationAdmissionError(const nlohmann::json &state, const CoreTownStationAdmissionContext &context);

/**
 * Reject native order details that cannot survive the bounded insert/delete stage.
 * @param train Native train pointer to validate.
 * @return Empty if orders are valid, otherwise denial reason.
 */
std::string GetCoreTownStationNativeTrainError(const Train *train);

/**
 * Reserve the sole process-local token; game load does not clear its history.
 * @param token Token identifier to reserve.
 * @return True if token was successfully reserved.
 */
bool TryReserveCoreTownStationToken(std::string_view token);

/**
 * Compare the paid link's live native quote with its frozen preconstruction cost.
 * @param quote Live native command quote.
 * @param frozen_cost Frozen preconstruction cost.
 * @param paid Cumulative paid cost so far.
 * @param maximum Budget ceiling.
 * @return Empty if within bounds, otherwise error string.
 */
std::string GetCoreTownStationLinkQuoteError(const CommandCost &quote, int64_t frozen_cost, int64_t paid, int64_t maximum);

/**
 * Keep a repeated read-only quote identical to the first accepted components.
 * @param frozen Output frozen quote JSON.
 * @param live Live quote JSON to compare or freeze.
 * @return True if quotes match or was newly frozen.
 */
bool TryFreezeCoreTownStationQuote(nlohmann::json &frozen, const nlohmann::json &live);

/**
 * The single connector may reuse owned compatible rail or clear ordinary ground.
 * @param tile Tile to inspect.
 * @param track Track direction to check.
 * @return True if tile can host the connector rail.
 */
bool IsCoreTownStationConnectorTileAllowed(TileIndex tile, Track track);

/**
 * Native, read-only quote for a two-tile independent Axis::X Core platform.
 * @param tile Tile where platform originates.
 * @return Native command quote or failure cost.
 */
CommandCost QueryCoreTownStation(TileIndex tile);

/**
 * Read-only admission decision for the exact checkpoint contract.
 * @param state Complete paused native checkpoint projection.
 * @param context Verified native environment conditions.
 * @param continuation Whether this is the planned cold continuation.
 * @return Empty on admission, otherwise the first precise denial reason.
 */
std::string GetCoreBasketAdmissionError(const nlohmann::json &state, const CoreBasketAdmissionContext &context, bool continuation);

/**
 * Quote one owned reusable or new track piece without adopting neutral rail.
 * @param tile Tile receiving or retaining the rail piece.
 * @param track Requested track direction.
 * @param prospective Earlier frozen pieces planned on this tile.
 * @return Native command quote or a failed legality result.
 */
CommandCost QueryCoreBasketRail(TileIndex tile, Track track, TrackBits prospective = {});

/** @return Separate non-mutating cargo, native growth, rail and reservation evidence. */
nlohmann::json GetCoreBasketObservation();

/** Revoke transient fresh-generation proof authorization before any game start or load. */
void ResetConnectedEconomyProof();

/** @return Read-only native map type/height/ownership fingerprint, including ownerless tiles. */
uint64_t GetConnectedEconomyMapFingerprint();

/** Prepare isolated Commonwealth works prototype scenario. */
bool PrepareWorks();

/** Commission alternative raw-resource connection on World 3 with independent portal link. */
bool ConnectWorldSource(nlohmann::json &report);

/** Commission Pipeline B research freight chain and surveyed Rare Earth opportunity. */
bool CommissionResearchChain(nlohmann::json &report);

/** Test atomic escrow reservation, insufficient kit rejection, and refund negative paths. */
bool TestResearchNegativePaths(nlohmann::json &report);

/** Control closure of primary World 2 silica corridor. */
bool SetPrimaryCorridorClosed(bool closed);

#endif /* CONNECTED_ECONOMY_H */
