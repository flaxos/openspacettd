/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file balancing_critic.cpp Autonomous Headless Balancing Critic and Multi-Decade Simulation Harness. */

#include "../stdafx.h"
#include "balancing_critic.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "megacity_manager.h"
#include "corporate_hq.h"
#include "company_stockpile.h"
#include "logistics_hub.h"
#include "production_chain.h"
#include "universe_authority.h"
#include "fabrication_manager.h"

#include "../economy_type.h"
#include "../economy_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../train.h"
#include "../vehicle_base.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_economy.h"
#include "../timer/timer_game_tick.h"
#include "../openttd.h"
#include "../core/backup_type.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include "../3rdparty/fmt/format.h"

#include <cmath>
#include <fstream>
#include <filesystem>
#include <algorithm>

BalancingSnapshot BalancingCritic::TakeSnapshot()
{
	BalancingSnapshot snap;
	auto ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	snap.calendar_year  = static_cast<uint32_t>(ymd.year.base());
	snap.calendar_month = static_cast<uint32_t>(ymd.month + 1);
	snap.calendar_day   = static_cast<uint32_t>(ymd.day);
	snap.tick_counter   = TimerGameTick::counter;

	/* 1. Macroeconomics & Inflation */
	snap.inflation_prices  = _economy.inflation_prices;
	snap.inflation_payment = _economy.inflation_payment;
	snap.inflation_drift_ratio = (snap.inflation_payment > 0)
		? (static_cast<double>(snap.inflation_prices) / static_cast<double>(snap.inflation_payment))
		: 1.0;

	/* 2. Companies & Solvency */
	for (const Company *c : Company::Iterate()) {
		BalancingCompanyStats cs;
		cs.company_id      = c->index.base();
		cs.money           = static_cast<int64_t>(c->money);
		cs.current_loan    = static_cast<int64_t>(c->current_loan);
		cs.annual_revenue  = static_cast<int64_t>(c->cur_economy.income);
		cs.annual_expenses = static_cast<int64_t>(c->cur_economy.expenses);
		cs.profit_margin   = (cs.annual_revenue > 0)
			? (static_cast<double>(cs.annual_revenue - cs.annual_expenses) / static_cast<double>(cs.annual_revenue))
			: 0.0;
		snap.companies.push_back(cs);
	}

	/* 3. Inter-World Portals & Congestion */
	snap.portals.total_portals = static_cast<uint32_t>(PortalRegistry::GetAllPortals().size());
	snap.portals.total_vehicles_in_transit = static_cast<uint32_t>(PortalRegistry::GetAllVehicleTransit().size());

	uint32_t max_wait = 0;
	uint32_t stuck_count = 0;
	uint32_t waiting_signal = 0;
	for (const Train *t : Train::Iterate()) {
		if (!t->IsFrontEngine()) continue;
		if (t->wait_counter > max_wait) max_wait = t->wait_counter;
		if (t->flags.Test(VehicleRailFlag::Stuck)) stuck_count++;
		if (t->wait_counter > 50) waiting_signal++;
	}
	snap.portals.peak_wait_ticks = max_wait;
	snap.portals.stuck_train_count = stuck_count;
	snap.portals.trains_waiting_signal = waiting_signal;

	/* 4. Planetary Stockpiles */
	for (const auto &stock : StockpileManager::GetAllStockpiles()) {
		BalancingStockpileStats ss;
		ss.world_id = stock.world_id.base();
		ss.company_id = stock.company_id.base();
		ss.total_units = 0;
		ss.zero_stock_cargos = 0;
		for (const auto &[cargo, amount] : stock.inventory) {
			ss.inventory[cargo] = amount;
			ss.total_units += amount;
			if (amount == 0) ss.zero_stock_cargos++;
		}
		snap.stockpiles.push_back(ss);
	}

	/* 5. Logistics Hubs & Reserve Floors */
	for (const auto &hub : LogisticsHubManager::GetAllHubs()) {
		BalancingHubStats hs;
		hs.hub_id = hub.hub_id;
		hs.world_id = hub.world_id.base();
		hs.company_id = hub.company_id.base();
		hs.reserve_floor_deficits = 0;
		for (const auto &[cargo, floor] : hub.reserve_floors) {
			uint32_t current = StockpileManager::GetStock(hub.world_id, hub.company_id, cargo);
			if (current < floor) hs.reserve_floor_deficits += (floor - current);
		}
		snap.hubs.push_back(hs);
	}

	/* 6. Processing Facilities Throughput */
	auto facilities = ProductionChainManager::GetAllFacilities();
	snap.facilities.total_facilities = static_cast<uint32_t>(facilities.size());
	snap.facilities.monthly_produced_batches = 0;
	snap.facilities.input_starved_facilities = 0;
	snap.facilities.output_blocked_facilities = 0;
	for (const auto &f : facilities) {
		snap.facilities.monthly_produced_batches += f.last_month_production;
		bool starved = false;
		for (const auto &[cargo, amt] : f.input_buffers) {
			if (amt == 0) { starved = true; break; }
		}
		if (starved && !f.input_buffers.empty()) snap.facilities.input_starved_facilities++;
		bool blocked = false;
		for (const auto &[cargo, amt] : f.output_buffers) {
			if (amt >= 200) { blocked = true; break; }
		}
		if (blocked) snap.facilities.output_blocked_facilities++;
	}

	/* 7. Megacity Satisfaction */
	for (const auto &m : MegacityManager::GetAllMegacities()) {
		BalancingMegacityStats ms;
		ms.town_id = m.town_id.base();
		ms.world_id = m.world_id.base();
		ms.population = m.population;
		ms.quota_demanded = 0;
		ms.quota_delivered = 0;
		for (size_t tier = 0; tier < 3; ++tier) {
			ms.quota_demanded += m.monthly_quota[tier];
			ms.quota_delivered += m.delivered_current[tier];
		}
		ms.satisfaction_pct = (ms.quota_demanded > 0)
			? (static_cast<double>(ms.quota_delivered) * 100.0 / static_cast<double>(ms.quota_demanded))
			: 100.0;
		snap.megacities.push_back(ms);
	}

	/* 8. Interplanetary Tariffs */
	snap.total_interplanetary_tariffs = UniverseAuthorityService::Instance().GetEmpireSupplyChainMatrix().total_tariffs_generated;

	return snap;
}

BalancingCriticReport BalancingCritic::SimulateYears(uint32_t years, uint32_t snapshots_per_year)
{
	BalancingCriticReport report;
	report.simulation_years = years;

	fmt::print("[SimulateYears] Taking initial baseline snapshot...\n");
	/* Record baseline initial snapshot */
	report.timeseries.push_back(TakeSnapshot());
	fmt::print("[SimulateYears] Baseline snapshot taken successfully!\n");

	if (years == 0) {
		return AnalyzeAndCritique(report.timeseries);
	}

	AutoRestoreBackup pause(_pause_mode);
	_pause_mode.Reset();

	extern TileIndex _cur_tileloop_tile;
	if (_cur_tileloop_tile == 0) _cur_tileloop_tile = TileIndex{1};
	if (!IsLocalCompany()) _current_company = _local_company;
	if (_settings_game.linkgraph.recalc_interval < 4) _settings_game.linkgraph.recalc_interval = 16;
	if (_settings_game.linkgraph.recalc_time < 1) _settings_game.linkgraph.recalc_time = 32;
	_settings_game.construction.freeform_edges = true;

	uint32_t start_year = static_cast<uint32_t>(TimerGameCalendar::year.base());
	uint32_t target_year = start_year + years;
	uint32_t prev_year = start_year;
	uint32_t prev_month = TimerGameCalendar::month;

	fmt::print("[SimulateYears] Stepping simulation from year {} to {}...\n", start_year, target_year);

	while (static_cast<uint32_t>(TimerGameCalendar::year.base()) < target_year) {
		/* Step 1 month of simulation ticks */
		for (uint32_t t = 0; t < Ticks::DAY_TICKS * 30; ++t) {
			StateGameLoop();
			report.total_ticks_simulated++;
		}

		uint32_t cur_year = static_cast<uint32_t>(TimerGameCalendar::year.base());
		uint32_t cur_month = TimerGameCalendar::month;

		bool should_sample = false;
		if (snapshots_per_year <= 1) {
			if (cur_year != prev_year) should_sample = true;
		} else if (snapshots_per_year == 4) {
			if (cur_month / 3 != prev_month / 3 || cur_year != prev_year) should_sample = true;
		} else {
			if (cur_month != prev_month || cur_year != prev_year) should_sample = true;
		}

		if (should_sample) {
			report.timeseries.push_back(TakeSnapshot());
			prev_year = cur_year;
			prev_month = cur_month;
		}
	}

	/* Record final snapshot */
	report.timeseries.push_back(TakeSnapshot());

	BalancingCriticReport analyzed = AnalyzeAndCritique(report.timeseries);
	analyzed.simulation_years = years;
	analyzed.total_ticks_simulated = report.total_ticks_simulated;
	return analyzed;
}

BalancingCriticReport BalancingCritic::AnalyzeAndCritique(const std::vector<BalancingSnapshot> &timeseries)
{
	BalancingCriticReport report;
	report.timeseries = timeseries;
	if (timeseries.empty()) return report;

	const auto &initial = timeseries.front();
	const auto &final   = timeseries.back();

	/* 1. Inflation & Price/Payment Gap */
	double initial_prices = static_cast<double>(initial.inflation_prices) / 65536.0;
	double final_prices   = static_cast<double>(final.inflation_prices) / 65536.0;
	double final_pay      = static_cast<double>(final.inflation_payment) / 65536.0;

	uint32_t years = std::max(1u, final.calendar_year - initial.calendar_year);
	report.compound_annual_inflation = (initial_prices > 0.0) ? (std::pow(final_prices / initial_prices, 1.0 / years) - 1.0) : 0.0;
	report.price_payment_divergence = final_prices - final_pay;

	if (report.price_payment_divergence > 0.25) {
		report.diagnoses.push_back(fmt::format(
			"Inflation Divergence Alert: Operating prices have grown to {:.2f}x while cargo payment rates grew to only {:.2f}x (gap: +{:.1f}%).",
			final_prices, final_pay, report.price_payment_divergence * 100.0));

		TuningRecommendation rec;
		rec.category = "Tariff";
		rec.rule_triggered = "INFLATION_DIVERGENCE_GAP";
		rec.severity = "WARNING";
		rec.message = "Increase freight payment rate multiplier to prevent long-term operating margin erosion.";
		rec.parameter_name = "FreightTariffMultiplier";
		rec.current_value = "1.00x";
		rec.recommended_value = fmt::format("{:.2f}x", 1.0 + (report.price_payment_divergence * 0.5));
		report.recommendations.push_back(rec);
	}

	/* 2. Corporate Solvency & Profit Margins */
	double sum_margin = 0.0;
	uint32_t margin_samples = 0;
	int64_t min_treasury = INT64_MAX;
	for (const auto &snap : timeseries) {
		for (const auto &c : snap.companies) {
			if (c.company_id == 0) {
				sum_margin += c.profit_margin;
				margin_samples++;
				if (c.money < min_treasury) min_treasury = c.money;
			}
		}
	}
	report.avg_profit_margin = (margin_samples > 0) ? (sum_margin / margin_samples) : 0.0;

	if (min_treasury < 0) {
		report.diagnoses.push_back(fmt::format("Insolvency Alert: Corporate treasury dipped into debt ({:d} Cr).", min_treasury));
		report.is_sustainable = false;

		TuningRecommendation rec;
		rec.category = "Tariff";
		rec.rule_triggered = "NEGATIVE_TREASURY_INSOLVENCY";
		rec.severity = "CRITICAL";
		rec.message = "Operating revenue insufficient to cover infrastructure maintenance; boost baseline tariffs.";
		rec.parameter_name = "InterplanetaryTradePremium";
		rec.current_value = "10 Cr / unit";
		rec.recommended_value = "15 Cr / unit";
		report.recommendations.push_back(rec);
	}

	/* 3. Inter-World Portal Choke Points */
	uint32_t congested_snaps = 0;
	uint32_t max_wait_seen = 0;
	uint32_t stuck_seen = 0;
	for (const auto &snap : timeseries) {
		if (snap.portals.peak_wait_ticks > 150 || snap.portals.stuck_train_count > 0) {
			congested_snaps++;
		}
		if (snap.portals.peak_wait_ticks > max_wait_seen) max_wait_seen = snap.portals.peak_wait_ticks;
		if (snap.portals.stuck_train_count > stuck_seen) stuck_seen = snap.portals.stuck_train_count;
	}
	report.portal_choke_point_index = !timeseries.empty() ? (static_cast<double>(congested_snaps) / timeseries.size()) : 0.0;

	if (report.portal_choke_point_index > 0.20 || stuck_seen > 0) {
		report.diagnoses.push_back(fmt::format(
			"Portal Choke Point: Gateway terminals congested in {:.1f}% of intervals (peak wait: {} ticks, stuck trains: {}).",
			report.portal_choke_point_index * 100.0, max_wait_seen, stuck_seen));

		if (stuck_seen > 0) report.is_sustainable = false;

		TuningRecommendation rec;
		rec.category = "Portal";
		rec.rule_triggered = "PORTAL_SIGNAL_CONGESTION";
		rec.severity = stuck_seen > 0 ? "CRITICAL" : "WARNING";
		rec.message = "Gateway throat throughput saturated; deploy dual portal staging tracks or increase block distance.";
		rec.parameter_name = "GatewayTerminalThroatCapacity";
		rec.current_value = "4-platform single-throat terminal";
		rec.recommended_value = "Staging sidings + flying junction approach";
		report.recommendations.push_back(rec);
	}

	/* 4. Planetary Stockpile Starvation */
	uint32_t total_stock_checks = 0;
	uint32_t zero_stock_checks = 0;
	for (const auto &snap : timeseries) {
		for (const auto &s : snap.stockpiles) {
			total_stock_checks += s.inventory.size();
			zero_stock_checks  += s.zero_stock_cargos;
		}
	}
	report.stockpile_starvation_rate = (total_stock_checks > 0) ? (static_cast<double>(zero_stock_checks) / total_stock_checks) : 0.0;

	if (report.stockpile_starvation_rate > 0.25) {
		report.diagnoses.push_back(fmt::format(
			"Feedstock Starvation Alert: Planetary stockpiles experienced zero-inventory conditions in {:.1f}% of evaluated roles.",
			report.stockpile_starvation_rate * 100.0));

		TuningRecommendation rec;
		rec.category = "BOM";
		rec.rule_triggered = "STOCKPILE_CHRONIC_STARVATION";
		rec.severity = "WARNING";
		rec.message = "Heavy fabrication requirements are depleting local stockpiles faster than logistics replenishment.";
		rec.parameter_name = "TrackBOM.StructuralMetal";
		rec.current_value = "1 unit / tile";
		rec.recommended_value = "0.75 units / tile (or boost Smelter capacity by +50%)";
		report.recommendations.push_back(rec);
	}

	/* 5. Megacity Quota Satisfaction */
	double sum_sat = 0.0;
	uint32_t sat_count = 0;
	for (const auto &snap : timeseries) {
		for (const auto &m : snap.megacities) {
			sum_sat += m.satisfaction_pct;
			sat_count++;
		}
	}
	report.megacity_avg_satisfaction = (sat_count > 0) ? (sum_sat / sat_count) : 100.0;

	if (report.megacity_avg_satisfaction < 40.0) {
		report.diagnoses.push_back(fmt::format(
			"Megacity Supply Deficit: Average commodity demand satisfaction is only {:.1f}%.",
			report.megacity_avg_satisfaction));

		TuningRecommendation rec;
		rec.category = "Megacity";
		rec.rule_triggered = "MEGACITY_DEFICIT_GROWTH_PENALTY";
		rec.severity = "INFO";
		rec.message = "Megacity quota targets exceed current freight corridor throughput; recalibrate baseline quota.";
		rec.parameter_name = "MegacityTier2Quota";
		rec.current_value = "300 units/mo";
		rec.recommended_value = "200 units/mo";
		report.recommendations.push_back(rec);
	}

	if (report.diagnoses.empty()) {
		report.diagnoses.push_back("System Equilibrium: Macroeconomic inflation, logistics flow, and stockpile reserves are stable.");
	}

	return report;
}

bool BalancingCritic::ExportReportJson(const BalancingCriticReport &report, const std::string &filepath)
{
	if (filepath.empty()) return false;

	try {
		std::error_code ec;
		std::filesystem::path p(filepath);
		if (p.has_parent_path()) {
			std::filesystem::create_directories(p.parent_path(), ec);
		}

		nlohmann::json root;
		root["simulation_years"] = report.simulation_years;
		root["total_ticks_simulated"] = report.total_ticks_simulated;

		root["summary"] = {
			{"avg_profit_margin", report.avg_profit_margin},
			{"compound_annual_inflation", report.compound_annual_inflation},
			{"price_payment_divergence", report.price_payment_divergence},
			{"portal_choke_point_index", report.portal_choke_point_index},
			{"stockpile_starvation_rate", report.stockpile_starvation_rate},
			{"megacity_avg_satisfaction", report.megacity_avg_satisfaction},
			{"is_sustainable", report.is_sustainable},
		};

		root["diagnoses"] = report.diagnoses;

		root["recommendations"] = nlohmann::json::array();
		for (const auto &r : report.recommendations) {
			root["recommendations"].push_back({
				{"category", r.category},
				{"rule_triggered", r.rule_triggered},
				{"severity", r.severity},
				{"message", r.message},
				{"parameter_name", r.parameter_name},
				{"current_value", r.current_value},
				{"recommended_value", r.recommended_value},
			});
		}

		root["timeseries"] = nlohmann::json::array();
		for (const auto &snap : report.timeseries) {
			nlohmann::json s_json;
			s_json["year"] = snap.calendar_year;
			s_json["month"] = snap.calendar_month;
			s_json["tick"] = snap.tick_counter;
			s_json["inflation_prices"] = snap.inflation_prices;
			s_json["inflation_payment"] = snap.inflation_payment;
			s_json["inflation_drift_ratio"] = snap.inflation_drift_ratio;

			s_json["companies"] = nlohmann::json::array();
			for (const auto &c : snap.companies) {
				s_json["companies"].push_back({
					{"company_id", c.company_id},
					{"money", c.money},
					{"loan", c.current_loan},
					{"revenue", c.annual_revenue},
					{"expenses", c.annual_expenses},
					{"profit_margin", c.profit_margin},
				});
			}

			s_json["portals"] = {
				{"total_portals", snap.portals.total_portals},
				{"in_transit", snap.portals.total_vehicles_in_transit},
				{"peak_wait_ticks", snap.portals.peak_wait_ticks},
				{"stuck_trains", snap.portals.stuck_train_count},
				{"waiting_signal", snap.portals.trains_waiting_signal},
			};

			s_json["stockpiles"] = nlohmann::json::array();
			for (const auto &st : snap.stockpiles) {
				s_json["stockpiles"].push_back({
					{"world_id", st.world_id},
					{"company_id", st.company_id},
					{"total_units", st.total_units},
					{"zero_stock_cargos", st.zero_stock_cargos},
					{"inventory", st.inventory},
				});
			}

			s_json["facilities"] = {
				{"total_facilities", snap.facilities.total_facilities},
				{"produced_batches", snap.facilities.monthly_produced_batches},
				{"input_starved", snap.facilities.input_starved_facilities},
				{"output_blocked", snap.facilities.output_blocked_facilities},
			};

			s_json["megacities"] = nlohmann::json::array();
			for (const auto &m : snap.megacities) {
				s_json["megacities"].push_back({
					{"town_id", m.town_id},
					{"world_id", m.world_id},
					{"population", m.population},
					{"demanded", m.quota_demanded},
					{"delivered", m.quota_delivered},
					{"satisfaction_pct", m.satisfaction_pct},
				});
			}

			s_json["total_tariffs"] = snap.total_interplanetary_tariffs;
			root["timeseries"].push_back(s_json);
		}

		std::ofstream ofs(filepath);
		if (!ofs.is_open()) return false;
		ofs << root.dump(2);
		return true;
	} catch (...) {
		return false;
	}
}

bool BalancingCritic::RunHeadlessCritic(uint32_t years, const std::string &output_json_path)
{
	fmt::print("Autonomous Balancing Critic: initiating fast-forward simulation for {} years...\n", years);
	auto report = SimulateYears(years);

	fmt::print("Simulation Completed! Ticks simulated: {}, Snapshots recorded: {}\n",
		report.total_ticks_simulated, report.timeseries.size());
	fmt::print("--- Macroeconomic & Logistics Diagnostics ({}) ---\n", report.diagnoses.size());
	for (const auto &d : report.diagnoses) {
		fmt::print(" * {}\n", d);
	}

	fmt::print("--- Balancing Tuning Recommendations ({}) ---\n", report.recommendations.size());
	for (const auto &r : report.recommendations) {
		fmt::print(" [{}] {}: {} -> {} ({})\n",
			r.severity, r.parameter_name, r.current_value, r.recommended_value, r.message);
	}

	if (!output_json_path.empty()) {
		if (ExportReportJson(report, output_json_path)) {
			fmt::print("Exported full timeseries critic report to {}\n", output_json_path);
		} else {
			fmt::print(stderr, "Failed to write report to {}\n", output_json_path);
		}
	}

	fmt::print("Sustainability Verdict: {}\n", report.is_sustainable ? "SUSTAINABLE (PASS)" : "UNSUSTAINABLE (WARNING)");
	return report.is_sustainable;
}
