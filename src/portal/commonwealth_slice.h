/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file commonwealth_slice.h Offline economic acceptance fixture and optional read-only cargo audit. */
#ifndef COMMONWEALTH_SLICE_H
#define COMMONWEALTH_SLICE_H

#include "../cargo_type.h"
#include <array>
#include <span>
#include <string_view>

struct CommonwealthSliceAudit {
	std::array<uint64_t, NUM_CARGO> produced{};
	std::array<uint64_t, NUM_CARGO> unallocated{};
	std::array<uint64_t, NUM_CARGO> discarded{};
	int64_t cash_debits = 0;
};
extern CommonwealthSliceAudit *_commonwealth_slice_audit;
bool ConCommonwealthSlice(std::span<std::string_view> argv);

#endif
