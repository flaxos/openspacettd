/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_fleet_shuttle_growth.cpp Catch2 integration tests for Horizon A Fleet Shuttle Scheduling & Sustained Megacity Growth. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../cargotype.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../core/random_func.hpp"
#include "../economy_func.h"
#include "../engine_func.h"
#include "../map_func.h"
#include "../portal/commonwealth_slice.h"
#include "../portal/connected_economy.h"
#include "../portal/integrated_economy.h"
#include "../portal/megacity_manager.h"
#include "../portal/planet_manager.h"
#include "../portal/production_chain.h"
#include "../portal/tech_tree.h"
#include "../portal/corporate_hq.h"
#include "../rail.h"
#include "../road_map.h"
#include "../station_base.h"
#include "../town.h"
#include "../timer/timer_game_calendar.h"
#include "../signal_func.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "../window_func.h"
#include "mock_environment.h"

#include "../safeguards.h"

extern void OnTick_Town();
extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace {

CargoType GetCargo(const char (&label)[5])
{
	CargoLabel cl;
	std::copy_n(label, 4, cl.begin());
	return GetCargoTypeByLabel(cl);
}

struct FleetGrowthFixture {
	FleetGrowthFixture()
	{
		IntegratedEconomy::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);

		const char *labels[] = {
			"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
			"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"
		};
		for (uint8_t c = 0; c < std::size(labels); ++c) {
			CargoLabel label;
			std::copy_n(labels[c], 4, label.begin());
			CargoSpec::Get(CargoType{c})->label = label;
			CargoSpec::Get(CargoType{c})->bitnum = c;
		}
		BuildCargoLabelMap();

		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);

		_settings_game.economy.allow_town_roads = true;
	}

	~FleetGrowthFixture()
	{
		UpdateSignalsInBuffer();
		_vehicle_pool.CleanPool();
		ResetVehicleHash();
		IntegratedEconomy::Reset();
		MegacityManager::Reset();
		SetupCargoForClimate(LandscapeType::Temperate);
		ProductionChainManager::InitDefaultRecipes();
		UnInitWindowSystem();
	}

	Town *SetupCoreTown(uint32_t population = 1191)
	{
		auto town = *Town::Iterate().begin();
		town->cache.population = population;
		town->cache.num_houses = 29;

		TileIndex center = town->xy;
		MakeRoadNormal(center, ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, CompanyID{0}, OWNER_TOWN);
		MakeRoadNormal(center + TileDiffXY(1, 0), ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, CompanyID{0}, OWNER_TOWN);
		MakeRoadNormal(center + TileDiffXY(0, 1), ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, CompanyID{0}, OWNER_TOWN);
		MakeRoadNormal(center + TileDiffXY(1, 1), ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, CompanyID{0}, OWNER_TOWN);

		MegacityManager::RegisterMegacity(town->index, WorldID{0}, "Mito Megacity", population);
		return town;
	}
};

} // namespace

TEST_CASE("Fleet Shuttle Logistics - 17-Month Transit Loop vs Storage Cap Analysis", "[fleet-shuttle-growth]")
{
	FleetGrowthFixture fixture;
	Town *town = fixture.SetupCoreTown(1096);
	REQUIRE(town != nullptr);

	const uint32_t monthly_demand = 55;
	const uint32_t storage_cap = monthly_demand * 3; // 165
	const uint32_t train_capacity = 105; // 3 wagons x 35
	const uint32_t round_trip_ticks = 35000; // ~17.1 months
	const uint32_t ticks_per_month = 2048;

	/* 1. Prove single consist starvation trap */
	uint32_t current_stock = 0;
	// Delivery arrives at month 0
	uint32_t accepted = std::min(train_capacity, storage_cap - current_stock);
	CHECK(accepted == 105);
	current_stock += accepted;

	uint32_t fed_months = 0;
	uint32_t starved_months = 0;
	uint32_t total_months = round_trip_ticks / ticks_per_month; // 17 months

	for (uint32_t m = 0; m < total_months; ++m) {
		if (current_stock >= monthly_demand) {
			current_stock -= monthly_demand;
			++fed_months;
		} else {
			++starved_months;
		}
	}

	CHECK(fed_months == 1); // 105 - 55 = 50 left (less than 55, so month 2 starves!)
	CHECK(starved_months == 16); // 16 out of 17 months in complete starvation
	CHECK(starved_months > total_months / 2);

	/* 2. Prove multi-consist shuttle fleet eliminates starvation */
	current_stock = 0;
	uint32_t fleet_starved_months = 0;
	uint32_t fleet_fed_months = 0;
	double next_arrival = 0.0;
	const double headway_months = 1.7; // 10 consists on 17.1-month loop

	for (uint32_t m = 0; m < 36; ++m) { // 3 years (36 months)
		// Consists arrive during the month before end-of-month consumption
		while (next_arrival < m + 1.0) {
			uint32_t delivery = std::min(train_capacity, storage_cap - current_stock);
			current_stock += delivery;
			next_arrival += headway_months;
		}

		if (current_stock >= monthly_demand) {
			current_stock -= monthly_demand;
			++fleet_fed_months;
		} else {
			++fleet_starved_months;
		}
	}

	CHECK(fleet_starved_months == 0);
	CHECK(fleet_fed_months == 36);
	CHECK(current_stock >= 40);
}

TEST_CASE("Fleet Shuttle Logistics - Multi-Consist Scheduled Staggering Maintains Town Reserves", "[fleet-shuttle-growth]")
{
	FleetGrowthFixture fixture;
	Town *town = fixture.SetupCoreTown(1191);
	REQUIRE(town != nullptr);

	CargoType c_food = GetCargo("FOOD");
	REQUIRE(c_food != INVALID_CARGO);

	const auto *profile = MegacityManager::GetProfile(town->index);
	REQUIRE(profile != nullptr);

	/* Simulate 24 months of scheduled multi-consist shuttle deliveries */
	const uint32_t demand = IntegratedEconomy::CityDemand(town->index).at(c_food);
	CHECK(demand == 60);

	for (uint32_t month = 1; month <= 24; ++month) {
		// Staggered shuttle arrival delivering 70 food before monthly consumption
		uint32_t accepted = IntegratedEconomy::AcceptCity(town->index, c_food, 70, true);
		CHECK(accepted > 0);

		// Monthly evaluation via MegacityManager (which invokes IntegratedEconomy::EvaluateCity)
		MegacityManager::EvaluateMonthlySupply();

		CHECK(profile->growth_state != MegacityGrowthState::Starvation);
		CHECK(profile->passenger_multiplier >= 1.0f);

		// Town food reserve is never 0
		const auto *city = IntegratedEconomy::City(town->index);
		REQUIRE(city != nullptr);
		CHECK(city->reserves.at(c_food) > 0);
	}
}

TEST_CASE("Fleet Shuttle Logistics - Continuous Countdown and Multi-Year House Additions", "[fleet-shuttle-growth]")
{
	FleetGrowthFixture fixture;
	Town *town = fixture.SetupCoreTown(1191);
	REQUIRE(town != nullptr);

	CargoType c_food = GetCargo("FOOD");
	CargoType c_stel = GetCargo("STEL");
	CargoType c_ball = GetCargo("BALL");

	/* Set up active town growth parameters */
	town->growth_rate = 5;
	town->grow_counter = 2;
	town->flags.Set(TownFlag::IsGrowing);

	/* Under sustained food and construction supply, town stays in MetropolitanBoom */
	IntegratedEconomy::AcceptCity(town->index, c_food, 120, true);
	IntegratedEconomy::AcceptCity(town->index, c_stel, 20, true);
	IntegratedEconomy::AcceptCity(town->index, c_ball, 20, true);

	MegacityManager::EvaluateMonthlySupply();
	CHECK(MegacityManager::GetProfile(town->index)->growth_state == MegacityGrowthState::MetropolitanBoom);
	CHECK(town->flags.Test(TownFlag::IsGrowing));

	/* Verify continuous decrementing of grow_counter */
	OnTick_Town();
	CHECK(town->grow_counter == 1);
	OnTick_Town();
	CHECK(town->grow_counter == 0);

	/* Decrementing through 0 triggers GrowTown and resets counter to growth_rate */
	OnTick_Town();
	CHECK(town->grow_counter == town->growth_rate);

	/* Subsequent ticks continue smooth continuous decrementing */
	OnTick_Town();
	CHECK(town->grow_counter == town->growth_rate - 1);
	OnTick_Town();
	CHECK(town->grow_counter == town->growth_rate - 2);
}

TEST_CASE("Fleet Shuttle Logistics - Multi-Tier Prosperity Delivery Triggers HyperGrowth Boom", "[fleet-shuttle-growth]")
{
	FleetGrowthFixture fixture;
	Town *town = fixture.SetupCoreTown(1191);
	REQUIRE(town != nullptr);

	CargoType c_food = GetCargo("FOOD");
	CargoType c_stel = GetCargo("STEL");
	CargoType c_ball = GetCargo("BALL");
	CargoType c_chip = GetCargo("CHIP");
	CargoType c_ccry = GetCargo("CCRY");

	const auto *profile = MegacityManager::GetProfile(town->index);
	REQUIRE(profile != nullptr);

	/* Stage 1: Starvation when food is missing */
	MegacityManager::EvaluateMonthlySupply();
	CHECK(profile->growth_state == MegacityGrowthState::Starvation);
	CHECK(profile->growth_multiplier == 0.0f);
	CHECK(profile->passenger_multiplier == 0.5f);

	/* Stage 2: Subsistence with sustained Food only */
	IntegratedEconomy::AcceptCity(town->index, c_food, 60, true);
	MegacityManager::EvaluateMonthlySupply();
	CHECK(profile->growth_state == MegacityGrowthState::Subsistence);
	CHECK(profile->growth_multiplier == 0.0f);
	CHECK(profile->passenger_multiplier == 1.0f);

	/* Stage 3: Metropolitan Boom with Food + Steel + Ballast */
	IntegratedEconomy::AcceptCity(town->index, c_food, 60, true);
	IntegratedEconomy::AcceptCity(town->index, c_stel, 20, true);
	IntegratedEconomy::AcceptCity(town->index, c_ball, 20, true);
	MegacityManager::EvaluateMonthlySupply();
	CHECK(profile->growth_state == MegacityGrowthState::MetropolitanBoom);
	CHECK(profile->growth_multiplier == 1.0f);
	CHECK(profile->passenger_multiplier == 1.0f);

	/* Stage 4: HyperGrowth Boom with Food + Steel + Ballast + Chips + Crystals */
	IntegratedEconomy::AcceptCity(town->index, c_food, 60, true);
	IntegratedEconomy::AcceptCity(town->index, c_stel, 20, true);
	IntegratedEconomy::AcceptCity(town->index, c_ball, 20, true);
	IntegratedEconomy::AcceptCity(town->index, c_chip, 20, true);
	IntegratedEconomy::AcceptCity(town->index, c_ccry, 10, true);
	MegacityManager::EvaluateMonthlySupply();
	CHECK(profile->growth_state == MegacityGrowthState::HyperGrowth);
	CHECK(profile->growth_multiplier == 2.0f);
	CHECK(profile->passenger_multiplier == 1.5f);
	CHECK(profile->overall_supply_index == 1.0f);
}

TEST_CASE("Fleet Shuttle Logistics - Conservation, Cash, and State Deserialization", "[fleet-shuttle-growth]")
{
	FleetGrowthFixture fixture;
	Town *town = fixture.SetupCoreTown(1191);
	REQUIRE(town != nullptr);

	CargoType c_food = GetCargo("FOOD");
	CargoType c_stel = GetCargo("STEL");
	CargoType c_ball = GetCargo("BALL");

	IntegratedEconomy::AcceptCity(town->index, c_food, 120, true);
	IntegratedEconomy::AcceptCity(town->index, c_stel, 20, true);
	IntegratedEconomy::AcceptCity(town->index, c_ball, 20, true);

	MegacityManager::EvaluateMonthlySupply();

	/* Save state */
	const auto saved_economy = IntegratedEconomy::Save();
	const auto megacities_before = MegacityManager::GetAllMegacities();
	REQUIRE(megacities_before.size() == 1);

	/* Reset and reload */
	IntegratedEconomy::Reset();
	MegacityManager::Reset();
	REQUIRE(MegacityManager::GetAllMegacities().empty());

	REQUIRE(IntegratedEconomy::Load(saved_economy));
	MegacityManager::RestoreMegacity(megacities_before[0]);

	const auto *profile = MegacityManager::GetProfile(town->index);
	REQUIRE(profile != nullptr);
	CHECK(profile->growth_state == MegacityGrowthState::MetropolitanBoom);
	CHECK(profile->growth_multiplier == 1.0f);

	const auto *city = IntegratedEconomy::City(town->index);
	REQUIRE(city != nullptr);
	CHECK(city->consumed.at(c_food) == 60);
	CHECK(city->consumed.at(c_stel) == 20);
	CHECK(city->consumed.at(c_ball) == 20);
	CHECK(city->reserves.at(c_food) == 60);
	CHECK(city->reserves.at(c_stel) == 0);
	CHECK(city->reserves.at(c_ball) == 0);
}
