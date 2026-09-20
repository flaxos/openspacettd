/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

#ifndef BALANCING_CRITIC_H
#define BALANCING_CRITIC_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

/**
 * @file balancing_critic.h
 * Sprint 48 Part 2: Autonomous Headless Balancing Critic & Tuner.
 * Fast-forward multi-decade headless simulation harness, macroeconomic and logistics telemetry,
 * and automated balancing critic generating actionable tuning recommendations for BOM formulas,
 * Megacity quotas, and freight tariffs.
 */

struct BalancingCompanyStats {
	uint8_t company_id;
	int64_t money;
	int64_t current_loan;
	int64_t annual_revenue;
	int64_t annual_expenses;
	double profit_margin;
};

struct BalancingStockpileStats {
	uint32_t world_id;
	uint8_t company_id;
	std::map<uint8_t, uint32_t> inventory; ///< CargoType -> amount
	uint32_t total_units;
	uint32_t zero_stock_cargos;
};

struct BalancingHubStats {
	uint32_t hub_id;
	uint32_t world_id;
	uint8_t company_id;
	uint32_t reserve_floor_deficits;
};

struct BalancingPortalStats {
	uint32_t total_portals;
	uint32_t total_vehicles_in_transit;
	uint32_t peak_wait_ticks;
	uint32_t stuck_train_count;
	uint32_t trains_waiting_signal;
};

struct BalancingFacilityStats {
	uint32_t total_facilities;
	uint32_t monthly_produced_batches;
	uint32_t input_starved_facilities;
	uint32_t output_blocked_facilities;
};

struct BalancingMegacityStats {
	uint32_t town_id;
	uint32_t world_id;
	uint32_t population;
	uint32_t quota_demanded;
	uint32_t quota_delivered;
	double satisfaction_pct;
};

struct BalancingSnapshot {
	uint32_t calendar_year;
	uint32_t calendar_month;
	uint32_t calendar_day;
	uint64_t tick_counter;

	/* Macroeconomics & Inflation */
	uint64_t inflation_prices;
	uint64_t inflation_payment;
	double inflation_drift_ratio;

	std::vector<BalancingCompanyStats> companies;
	BalancingPortalStats portals;
	std::vector<BalancingStockpileStats> stockpiles;
	std::vector<BalancingHubStats> hubs;
	BalancingFacilityStats facilities;
	std::vector<BalancingMegacityStats> megacities;
	uint64_t total_interplanetary_tariffs;
};

struct TuningRecommendation {
	std::string category;       ///< "BOM", "Tariff", "Megacity", "Portal"
	std::string rule_triggered; ///< Diagnostic rule triggering recommendation
	std::string severity;       ///< "INFO", "WARNING", "CRITICAL"
	std::string message;        ///< Human-readable explanation
	std::string parameter_name; ///< Specific game parameter to tune
	std::string current_value;  ///< Current setting/status
	std::string recommended_value; ///< Recommended adjustment
};

struct BalancingCriticReport {
	uint32_t simulation_years = 0;
	uint64_t total_ticks_simulated = 0;
	std::vector<BalancingSnapshot> timeseries;

	/* Summary Diagnostics */
	double avg_profit_margin = 0.0;
	double compound_annual_inflation = 0.0;
	double price_payment_divergence = 0.0;
	double portal_choke_point_index = 0.0;
	double stockpile_starvation_rate = 0.0;
	double megacity_avg_satisfaction = 0.0;

	std::vector<std::string> diagnoses;
	std::vector<TuningRecommendation> recommendations;

	bool is_sustainable = true;
};

class BalancingCritic {
public:
	/** Take a single instantaneous telemetry snapshot of the current simulation state. */
	static BalancingSnapshot TakeSnapshot();

	/**
	 * Run a fast-forward headless simulation for the specified number of game years.
	 * @param years Number of game years to simulate.
	 * @param snapshots_per_year Number of snapshots to sample per year (default: 1 annual snapshot).
	 * @return Completed report with timeseries and critic analysis.
	 */
	static BalancingCriticReport SimulateYears(uint32_t years, uint32_t snapshots_per_year = 1);

	/**
	 * Perform diagnostic analysis on a recorded timeseries and generate tuning recommendations.
	 * @param timeseries Series of snapshots over time.
	 * @return Analysis report with diagnoses and tuning recommendations.
	 */
	static BalancingCriticReport AnalyzeAndCritique(const std::vector<BalancingSnapshot> &timeseries);

	/** Export the critic report to a JSON file. */
	static bool ExportReportJson(const BalancingCriticReport &report, const std::string &filepath);

	/**
	 * Run headless critic execution: simulates years, outputs diagnostics to console, and saves JSON.
	 * @param years Number of game years to simulate.
	 * @param output_json_path Target file for exported JSON report.
	 * @return True if the scenario was determined to be economically sustainable.
	 */
	static bool RunHeadlessCritic(uint32_t years, const std::string &output_json_path);
};

#endif /* BALANCING_CRITIC_H */
