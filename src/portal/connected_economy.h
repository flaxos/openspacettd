/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file connected_economy.h Lifecycle of isolated connected-economy proof authorization. */

#ifndef CONNECTED_ECONOMY_H
#define CONNECTED_ECONOMY_H

#include <cstdint>

/** Revoke transient fresh-generation proof authorization before any game start or load. */
void ResetConnectedEconomyProof();

/** @return Read-only native map type/height/ownership fingerprint, including ownerless tiles. */
uint64_t GetConnectedEconomyMapFingerprint();

#endif /* CONNECTED_ECONOMY_H */
