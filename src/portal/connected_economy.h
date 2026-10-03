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
	static constexpr uint32_t LIMIT = 30000;
	uint32_t states = 0; ///< All predecessor allocations in this invocation.
	uint32_t candidates = 0; ///< Station candidates examined in this invocation.
	bool exhausted = false; ///< A search attempted to exceed the invocation limit.
	bool TryState()
	{
		if (this->states >= LIMIT) { this->exhausted = true; return false; }
		++this->states;
		return true;
	}
};

/** Native environment observations used by the separately guarded loaded adapter. */
struct CoreBasketAdmissionContext {
	bool normal = false;
	bool offline = false;
	bool paused = false;
	bool profile = false;
	bool loaded = false;
	uint32_t companies = 0;
};

/** Read-only admission decision; an empty string permits the exact checkpoint contract. */
std::string GetCoreBasketAdmissionError(const nlohmann::json &state, const CoreBasketAdmissionContext &context, bool continuation);

/** Native quote for one owned reusable or new track piece; neutral infrastructure is never adopted. */
CommandCost QueryCoreBasketRail(TileIndex tile, Track track, TrackBits prospective = {});

/** Separate non-mutating all-cargo, native growth, private signal and reservation evidence. */
nlohmann::json GetCoreBasketObservation();

/** Revoke transient fresh-generation proof authorization before any game start or load. */
void ResetConnectedEconomyProof();

/** @return Read-only native map type/height/ownership fingerprint, including ownerless tiles. */
uint64_t GetConnectedEconomyMapFingerprint();

#endif /* CONNECTED_ECONOMY_H */
