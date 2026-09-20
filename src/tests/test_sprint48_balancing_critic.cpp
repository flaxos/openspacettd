/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint48_balancing_critic.cpp Unit and integration tests for Sprint 48 Part 2: Autonomous Headless Balancing Critic. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/balancing_critic.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/corporate_hq.h"
#include "../portal/corporate_alliance.h"
#include "../portal/production_chain.h"
#include "../portal/logistics_hub.h"
#include "../portal/company_stockpile.h"
#include "../portal/tech_tree.h"
#include "../portal/fabrication_manager.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../station_base.h"
#include "../train.h"
#include "../map_func.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "mock_environment.h"

#include <filesystem>
#include <fstream>
#include "../genworld.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../town.h"
#include "../economy_base.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_economy.h"
#include "../timer/timer_game_tick.h"
#include "../openttd.h"

#include "../landscape.h"

#include "../core/pool_type.hpp"

class Sprint48CriticFixture {
public:
	Sprint48CriticFixture()
	{
		(void)MockEnvironment::Instance();
		SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
		if (_current_language == nullptr) {
			extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
			auto saved_paths = _valid_searchpaths;
			auto saved_binary = _searchpaths[Searchpath::BinaryDir];
			_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
			_valid_searchpaths = {Searchpath::BinaryDir};
			InitializeLanguagePacks();
			_valid_searchpaths = std::move(saved_paths);
			_searchpaths[Searchpath::BinaryDir] = std::move(saved_binary);
		}
		if (_valid_searchpaths.empty()) {
			_valid_searchpaths.push_back(Searchpath::WorkingDir);
		}
		_engine_mngr.ResetToDefaultMapping();
		SetupEngines();
		StartupEngines();

		PoolBase::Clean(PoolType::Normal);
		Map::Allocate(64, 64);
		InitializeLandscape();
		extern TileIndex _cur_tileloop_tile;
		_cur_tileloop_tile = TileIndex{1};

		SetupCargoForClimate(LandscapeType::Temperate);
		_settings_game.game_creation.landscape = LandscapeType::Temperate;
		_settings_game.economy.minutes_per_calendar_year = CalendarTime::DEF_MINUTES_PER_YEAR;
		TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{2030}, 0, 1), 0);
		TimerGameEconomy::SetDate(TimerGameEconomy::ConvertYMDToDate(TimerGameEconomy::Year{2030}, 0, 1), 0);

		if (Company::GetIfValid(CompanyID{0}) == nullptr) {
			Company::CreateAtIndex(CompanyID{0});
		}
		if (Company::GetIfValid(CompanyID{1}) == nullptr) {
			Company::CreateAtIndex(CompanyID{1});
		}
		_current_company = _local_company = CompanyID{0};
		_game_mode = GameMode::Normal;
		_pause_mode = {};
		_generating_world = false;
		_screen.width = _screen.pitch = 1024;
		_screen.height = 768;
		ScreenSizeChanged();

		ProductionChainManager::InitDefaultRecipes();
	}

	~Sprint48CriticFixture()
	{
		PortalRegistry::Reset();
		PlanetManager::Reset();
		MegacityManager::Reset();
		CorporateHQManager::Reset();
		ProductionChainManager::Reset();
		PoolBase::Clean(PoolType::Normal);
	}
};

TEST_CASE_METHOD(Sprint48CriticFixture, "Sprint 48 Part 2 - Telemetry Snapshot & Aggregations", "[sprint48],[critic],[snapshot]")
{
	BalancingSnapshot snap = BalancingCritic::TakeSnapshot();

	/* Verify calendar fields are valid */
	CHECK(snap.calendar_year == 2030);
	CHECK(snap.calendar_month >= 1);
	CHECK(snap.calendar_month <= 12);
	CHECK(snap.calendar_day >= 1);
	CHECK(snap.calendar_day <= 31);

	/* Check companies stats collection */
	CHECK_FALSE(snap.companies.empty());
	bool found_company_0 = false;
	for (const auto &comp : snap.companies) {
		if (comp.company_id == 0) {
			found_company_0 = true;
			break;
		}
	}
	CHECK(found_company_0);
}

TEST_CASE_METHOD(Sprint48CriticFixture, "Sprint 48 Part 2 - Diagnostic Critic Rules & Tuning Recommendations", "[sprint48],[critic],[rules]")
{
	SECTION("Rule: Operating at negative treasury / profit margin triggers tariff recommendation")
	{
		std::vector<BalancingSnapshot> timeseries;

		BalancingSnapshot s1;
		s1.calendar_year = 2030;
		s1.calendar_month = 1;
		s1.calendar_day = 1;
		s1.inflation_prices = 65536;
		s1.inflation_payment = 65536;

		BalancingCompanyStats c1;
		c1.company_id = 0;
		c1.money = 50000;
		c1.annual_revenue = 10000;
		c1.annual_expenses = 30000;
		c1.profit_margin = -2.0;
		s1.companies.push_back(c1);

		BalancingSnapshot s2 = s1;
		s2.calendar_year = 2031;
		s2.companies[0].money = -25000; // Negative treasury insolvency!
		s2.companies[0].profit_margin = -1.5;

		timeseries.push_back(s1);
		timeseries.push_back(s2);

		BalancingCriticReport report = BalancingCritic::AnalyzeAndCritique(timeseries);
		REQUIRE_FALSE(report.recommendations.empty());
		CHECK_FALSE(report.is_sustainable);

		bool found_tariff_rec = false;
		for (const auto &rec : report.recommendations) {
			if (rec.category == "Tariff") {
				found_tariff_rec = true;
				CHECK(rec.severity == "CRITICAL");
				CHECK(rec.parameter_name == "InterplanetaryTradePremium");
				break;
			}
		}
		CHECK(found_tariff_rec);
	}

	SECTION("Rule: Inter-world portal congestion triggers gate capacity recommendation")
	{
		std::vector<BalancingSnapshot> timeseries;

		BalancingSnapshot s1;
		s1.calendar_year = 2030;
		s1.calendar_month = 1;
		s1.calendar_day = 1;
		s1.inflation_prices = 65536;
		s1.inflation_payment = 65536;
		s1.portals.total_portals = 2;
		s1.portals.total_vehicles_in_transit = 10;
		s1.portals.peak_wait_ticks = 250; // Congested (>150 ticks)
		s1.portals.stuck_train_count = 1;

		timeseries.push_back(s1);

		BalancingCriticReport report = BalancingCritic::AnalyzeAndCritique(timeseries);
		bool found_portal_rec = false;
		for (const auto &rec : report.recommendations) {
			if (rec.category == "Portal") {
				found_portal_rec = true;
				CHECK(rec.severity == "CRITICAL");
				CHECK(rec.rule_triggered == "PORTAL_SIGNAL_CONGESTION");
				break;
			}
		}
		CHECK(found_portal_rec);
		CHECK_FALSE(report.is_sustainable);
	}

	SECTION("Rule: Stockpile starvation triggers BOM batch tuning recommendation")
	{
		std::vector<BalancingSnapshot> timeseries;

		BalancingSnapshot s1;
		s1.calendar_year = 2030;
		s1.calendar_month = 1;
		s1.calendar_day = 1;
		s1.inflation_prices = 65536;
		s1.inflation_payment = 65536;

		BalancingStockpileStats st;
		st.company_id = 0;
		st.world_id = 2;
		st.inventory[1] = 0;
		st.inventory[2] = 0;
		st.total_units = 0;
		st.zero_stock_cargos = 2; // Both depleted
		s1.stockpiles.push_back(st);

		timeseries.push_back(s1);

		BalancingCriticReport report = BalancingCritic::AnalyzeAndCritique(timeseries);
		bool found_bom_rec = false;
		for (const auto &rec : report.recommendations) {
			if (rec.category == "BOM") {
				found_bom_rec = true;
				CHECK(rec.rule_triggered == "STOCKPILE_CHRONIC_STARVATION");
				CHECK(rec.parameter_name.find("BOM") != std::string::npos);
				break;
			}
		}
		CHECK(found_bom_rec);
	}

	SECTION("Rule: Megacity demand deficit triggers Megacity consumption rate reduction")
	{
		std::vector<BalancingSnapshot> timeseries;

		BalancingSnapshot s1;
		s1.calendar_year = 2030;
		s1.calendar_month = 1;
		s1.calendar_day = 1;
		s1.inflation_prices = 65536;
		s1.inflation_payment = 65536;

		BalancingMegacityStats mega;
		mega.town_id = 0;
		mega.world_id = 0;
		mega.population = 45000;
		mega.quota_demanded = 300;
		mega.quota_delivered = 35;
		mega.satisfaction_pct = 11.6; // Deficit (< 40%)
		s1.megacities.push_back(mega);

		timeseries.push_back(s1);

		BalancingCriticReport report = BalancingCritic::AnalyzeAndCritique(timeseries);
		bool found_megacity_rec = false;
		for (const auto &rec : report.recommendations) {
			if (rec.category == "Megacity") {
				found_megacity_rec = true;
				CHECK(rec.rule_triggered == "MEGACITY_DEFICIT_GROWTH_PENALTY");
				break;
			}
		}
		CHECK(found_megacity_rec);
	}
}

TEST_CASE_METHOD(Sprint48CriticFixture, "Sprint 48 Part 2 - Multi-Year Simulation & JSON Export", "[sprint48],[critic],[json]")
{
	const std::string test_report = "test_critic_report.json";

	if (std::filesystem::exists(test_report)) {
		std::filesystem::remove(test_report);
	}

	SECTION("Run short 1-year headless stepping and export JSON report")
	{
		BalancingCriticReport report = BalancingCritic::SimulateYears(1, 2); // 1 year, 2 snapshots
		CHECK(report.timeseries.size() >= 2);
		CHECK(report.simulation_years == 1);

		bool exported = BalancingCritic::ExportReportJson(report, test_report);
		REQUIRE(exported);
		REQUIRE(std::filesystem::exists(test_report));
		CHECK(std::filesystem::file_size(test_report) > 100);

		/* Verify JSON structure contains core keys */
		{
			std::ifstream ifs(test_report);
			std::string content((std::istreambuf_iterator<char>(ifs)),
			                    std::istreambuf_iterator<char>());
			CHECK(content.find("\"simulation_years\": 1") != std::string::npos);
			CHECK(content.find("\"timeseries\":") != std::string::npos);
			CHECK(content.find("\"recommendations\":") != std::string::npos);
			CHECK(content.find("\"is_sustainable\":") != std::string::npos);
		}

		/* Clean up */
		std::filesystem::remove(test_report);
	}
}
