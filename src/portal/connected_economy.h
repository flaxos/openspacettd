/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file connected_economy.h Lifecycle of isolated connected-economy proof authorization. */

#ifndef CONNECTED_ECONOMY_H
#define CONNECTED_ECONOMY_H

#include <cstdint>
#include <string>
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

#endif /* CONNECTED_ECONOMY_H */
