/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file commonwealth_slice.h Offline economic acceptance fixture and optional read-only cargo audit. */
#ifndef COMMONWEALTH_SLICE_H
#define COMMONWEALTH_SLICE_H

#include "../cargo_type.h"
#include <array>
#include <span>
#include <string_view>

/** Optional observer counters used by the offline WP11 economic slice harness. */
struct CommonwealthSliceAudit {
	std::array<uint64_t, NUM_CARGO> produced{}; ///< Native production released by cargo.
	std::array<uint64_t, NUM_CARGO> unallocated{}; ///< Produced cargo not allocated to a station.
	std::array<uint64_t, NUM_CARGO> discarded{}; ///< Station cargo removed by truncation or rating loss.
	int64_t cash_debits = 0; ///< Actual company cash debits observed during the run.
};
/** Active WP11 audit observer, or nullptr outside the harness. */
extern CommonwealthSliceAudit *_commonwealth_slice_audit;
/**
 * Execute the offline WP11 economic slice console command.
 * @param argv Console command arguments.
 * @return Always true because command errors are reported through console output.
 */
bool ConCommonwealthSlice(std::span<std::string_view> argv);

/** Prepare a disposable empty-map federation test before any clients join. */
bool ConFederationTestFixture(std::span<std::string_view> argv);

#endif
