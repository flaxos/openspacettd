/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file commonwealth_slice.h Offline economic acceptance fixture and optional read-only cargo audit. */
#ifndef COMMONWEALTH_SLICE_H
#define COMMONWEALTH_SLICE_H

#include "../cargo_type.h"
#include <array>
#include <map>
#include <span>
#include <vector>
#include <string_view>

/** Optional observer counters used by the offline WP11 economic slice harness. */
struct CommonwealthSliceAudit {
	std::array<uint64_t, NUM_CARGO> processing_bonus{}; ///< Extra output from research, observed after per-month rounding.
	std::array<uint64_t, NUM_CARGO> produced{}; ///< Native production released by cargo.
	std::array<uint64_t, NUM_CARGO> raw_produced{}; ///< Cargo actually added to native primary-industry waiting buffers.
	std::array<uint64_t, NUM_CARGO> raw_removed{}; ///< Primary waiting-buffer withdrawals before distribution/recession losses.
	std::array<uint64_t, NUM_CARGO> unallocated{}; ///< Produced cargo not allocated to a station.
	std::array<uint64_t, NUM_CARGO> discarded{}; ///< Station cargo removed by truncation or rating loss.
	std::array<uint64_t, NUM_CARGO> consumed{}; ///< Cargo accepted by native industries or town consumers.
	std::map<uint32_t, std::array<uint64_t, NUM_CARGO>> vehicle_deliveries; ///< Accepted cargo by vehicle.
	std::array<uint64_t, NUM_CARGO> research_consumed{}; ///< Actual research feedstock withdrawals.
	std::map<uint32_t, std::array<uint64_t, NUM_CARGO>> deliveries; ///< Accepted cargo by destination station.
	std::map<uint32_t, std::pair<uint64_t, int64_t>> gate_tolls; ///< Per-gate admissions and actual tolls, observer only.
	std::vector<std::array<int64_t, 6>> payments; ///< Tick, front, station, cargo, accepted units and native packet revenue.
	std::vector<std::array<int64_t, 4>> cash_payments; ///< Tick, front, destination and actual completed native payment.
	std::vector<std::array<int64_t, 3>> arrivals; ///< Native tick, front and station at BeginLoading, distinguishing trips from fragments.
	std::vector<std::array<int64_t, 4>> city_months; ///< Calendar date, town, FOOD basket consumed and remaining FOOD reserve.
	std::vector<std::array<int64_t, 4>> processor_cargo; ///< Calendar date, industry, cargo and signed physical recipe-buffer delta.
	std::map<uint32_t, int64_t> service_income; ///< Actual completed CargoPayment cash credits by front.
	std::map<uint32_t, int64_t> service_running; ///< Actual integer cash charges from native fractional train running debits.
	std::map<uint32_t, int64_t> vehicle_tolls; ///< Actual toll cash debits by front.
	std::map<uint8_t, int64_t> expenses; ///< Native company debits by expense type across year rollovers.
	std::vector<std::array<int64_t, 3>> cash_transactions; ///< Tick, native expense type and signed debit; negative values are receipts.
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
/**
 * Build and inspect the isolated connected-economy demo.
 * @param argv Console command arguments.
 * @return Always true; errors are reported through console output.
 */
bool ConConnectedEconomy(std::span<std::string_view> argv);

/**
 * Repair legacy connected-demo terrain before rendering or simulation after load.
 * Other saves are unchanged. Refuses to alter any corner shared by infrastructure.
 * @return True on success or when the save is not the connected fixture.
 */
bool RepairConnectedEconomyTerrain();

/** Smooth exposed terrain before building an isolated UAT fixture. */
bool SmoothExposedUATTerrain();

/**
 * Native-command obstruction controls restricted to the disposable federation fixture.
 * @param argv Console command and requested obstruction action.
 * @return True if the native command was posted; false for invalid fixture or action.
 */
bool ConFederationFixtureBlock(std::span<std::string_view> argv);

/** Prepare a disposable empty-map federation test before any clients join. */
bool ConFederationTestFixture(std::span<std::string_view> argv);

#endif
