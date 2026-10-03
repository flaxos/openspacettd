/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_core_construction_basket.cpp Loaded-checkpoint bounds and native basket evidence. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cargotype.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../core/random_func.hpp"
#include "../economy_func.h"
#include "../map_func.h"
#include "../portal/commonwealth_slice.h"
#include "../portal/connected_economy.h"
#include "../portal/integrated_economy.h"
#include "../portal/planet_manager.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../town.h"
#include "../train.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "../vehicle_func.h"
#include "../window_func.h"

#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace {
using Json = nlohmann::json;

/** Independent projection of the archived final_state, not an authored game or save.
 * The console adapter additionally compares the complete frozen snapshot. */
Json FrozenAdmissionProjection()
{
	return Json::parse(R"({
		"company":"ASSISTED FUNCTIONAL Core Supply","loan":100000,"max_loan":300000,
		"seed":11,"tick":167168,"money":3705864,"research":[[0,0,0,[301]]],"tech_company_ids":[0],
		"worlds":[
			{"id":0,"role":"Core","phase":1},{"id":1,"role":"Industrial","phase":2},
			{"id":2,"role":"Frontier","phase":3},{"id":3,"role":"Frontier","phase":3},
			{"id":4,"role":"Frontier","phase":4},{"id":5,"role":"Industrial","phase":4},
			{"id":6,"role":"Frontier","phase":4}
		],
		"towns":[{"id":0,"world":0,"megacity":true,"population":1191,
			"profile":{"growth":0.0,"passengers":0.5}}],
		"stations":[{"id":3,"tile":64278,"owner":0,"town":0,"warehouse":false,"consumer":true,
			"catchment_houses":[{"tile":68378,"town":0,"world":0}]}],
		"hubs":[],"stocks":[],
		"trains":[
			{"id":0,"owner":0,"engine":32,"crashed":false,"lost":false,
			 "orders":[[1,0,2,4,1,0],[1,1,4,0,1,0]]},
			{"id":1,"owner":0,"engine":50,"crashed":false,"lost":false},
			{"id":2,"owner":0,"engine":50,"crashed":false,"lost":false},
			{"id":3,"owner":0,"engine":50,"crashed":false,"lost":false},
			{"id":4,"owner":0,"engine":32,"crashed":false,"lost":false,
			 "orders":[[1,2,2,4,1,0],[1,3,4,0,1,0]]},
			{"id":5,"owner":0,"engine":52,"crashed":false,"lost":false},
			{"id":6,"owner":0,"engine":52,"crashed":false,"lost":false},
			{"id":7,"owner":0,"engine":52,"crashed":false,"lost":false}
		]
	})");
}

CoreBasketAdmissionContext LoadedContext()
{
	return {.normal = true, .offline = true, .paused = true, .profile = true, .loaded = true, .companies = 1};
}

struct BasketFixture {
	BasketFixture()
	{
		IntegratedEconomy::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
		ResetRailTypes();
		Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_RAIL);
		Company::Get(CompanyID{0})->clear_limit = 1000 << 16;
	}
	~BasketFixture()
	{
		UpdateSignalsInBuffer();
		_vehicle_pool.CleanPool();
		ResetVehicleHash();
		IntegratedEconomy::Reset();
		SetupCargoForClimate(LandscapeType::Temperate);
		ProductionChainManager::InitDefaultRecipes();
		UnInitWindowSystem();
	}
	Town *Core()
	{
		/* Reuse the integrated-economy fixture's complete, distinct label map. */
		const char *labels[] = {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
			"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"};
		for (uint8_t c = 0; c < std::size(labels); ++c) {
			CargoLabel label;
			std::copy_n(labels[c], 4, label.begin());
			CargoSpec::Get(CargoType{c})->label = label;
			CargoSpec::Get(CargoType{c})->bitnum = c;
		}
		BuildCargoLabelMap();
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
		auto town = *Town::Iterate().begin();
		town->cache.population = 1191;
		return town;
	}
};

CargoType C(const char (&label)[5])
{
	return GetCargoTypeByLabel(CargoLabel{label});
}

Json NativeEvidence()
{
	const auto town = *Town::Iterate().begin();
	return {{"economy", IntegratedEconomy::Save()}, {"money", int64_t(Company::Get(CompanyID{0})->money)},
		{"rng", {_random.state[0], _random.state[1]}}, {"tick", TimerGameTick::counter},
		{"map", GetConnectedEconomyMapFingerprint()}, {"population", town->cache.population},
		{"houses", town->cache.num_houses}, {"growth_rate", town->growth_rate},
		{"grow_counter", town->grow_counter}, {"flags", town->flags.base()}};
}

std::map<CargoType, uint32_t> CargoMap(const Json &value)
{
	return value.get<std::map<CargoType, uint32_t>>();
}
} // namespace

TEST_CASE("Core basket failed station searches share one predecessor budget", "[core-construction-basket][basket-search]")
{
	CoreBasketSearchBudget invocation;
	/* Two unsuccessful searches must leave their predecessor allocations charged.
	 * The third candidate cannot receive a new 30,000-state allowance. */
	for (uint32_t states : {14999u, 15000u}) {
		++invocation.candidates;
		for (uint32_t n = 0; n < states; ++n) {
			if (!invocation.TryState()) FAIL("Search exhausted before the combined predecessor limit");
		}
	}
	CHECK(invocation.states == 29999);
	++invocation.candidates;
	REQUIRE(invocation.TryState());
	CHECK(invocation.states == 30000);
	CHECK_FALSE(invocation.exhausted);
	CHECK_FALSE(invocation.TryState());
	CHECK(invocation.exhausted);
	CHECK(invocation.states == 30000);
	CHECK_FALSE(invocation.TryState());
	CHECK(invocation.states == 30000);
	CHECK(invocation.candidates == 3);
	CoreBasketSearchBudget separate_invocation;
	CHECK(separate_invocation.states == 0);
	CHECK_FALSE(separate_invocation.exhausted);
	CHECK(separate_invocation.TryState());
	CHECK(invocation.states == 30000);
}

TEST_CASE("Core basket admission preserves the archived projection and denies unsafe environments", "[core-construction-basket][basket-admission]")
{
	const Json state = FrozenAdmissionProjection();
	const auto before = state.dump();
	const auto context = LoadedContext();
	CHECK(GetCoreBasketAdmissionError(state, context, false).empty());
	CHECK(state.dump() == before);
	struct Denial { const char *reason; CoreBasketAdmissionContext context; };
	const std::array<Denial, 7> denials = {{
		{"game-mode", {.normal = false, .offline = true, .paused = true, .profile = true, .loaded = true, .companies = 1}},
		{"network-offline", {.normal = true, .offline = false, .paused = true, .profile = true, .loaded = true, .companies = 1}},
		{"fresh-generation", {.normal = true, .offline = true, .paused = true, .profile = true, .loaded = false, .companies = 1}},
		{"pause", {.normal = true, .offline = true, .paused = false, .profile = true, .loaded = true, .companies = 1}},
		{"profile-content", {.normal = true, .offline = true, .paused = true, .profile = false, .loaded = true, .companies = 1}},
		{"company-count", {.normal = true, .offline = true, .paused = true, .profile = true, .loaded = true, .companies = 0}},
		{"company-count", {.normal = true, .offline = true, .paused = true, .profile = true, .loaded = true, .companies = 2}},
	}};
	for (const auto &denial : denials) {
		INFO(denial.reason);
		CHECK(GetCoreBasketAdmissionError(state, denial.context, false) == denial.reason);
		CHECK(state.dump() == before);
	}
}

TEST_CASE("Core basket admission rejects checkpoint debt research custody and inherited fleet changes", "[core-construction-basket][basket-admission]")
{
	struct Denial { const char *path; Json value; const char *reason; };
	const std::vector<Denial> denials = {
		{"/company", "Other company", "company-debt"}, {"/company", 0, "company-debt"},
		{"/loan", 110000, "company-debt"},
		{"/max_loan", 400000, "company-debt"}, {"/money", 3705865, "cash"},
		{"/tick", 167169, "tick-seed"}, {"/seed", 12, "tick-seed"},
		{"/research/0/0", 302, "research"}, {"/research/0/1", 1, "research"},
		{"/research/0/2", 1000, "research"}, {"/research/0/3", Json::array({301, 302}), "research"},
		{"/tech_company_ids", Json::array({0, 1}), "research"},
		{"/worlds/1/role", "Frontier", "roles"}, {"/worlds/3/phase", 4, "roles"},
		{"/towns/0/world", 1, "core-profile"}, {"/towns/0/population", 0, "core-profile"},
		{"/towns/0/megacity", false, "core-profile"}, {"/towns/0/profile", nullptr, "core-profile"},
		{"/stations/0/owner", 1, "receiver"}, {"/stations/0/town", 1, "receiver"},
		{"/stations/0/warehouse", true, "receiver"}, {"/stations/0/consumer", false, "receiver"},
		{"/stations/0/catchment_houses", Json::array({{{"tile", 68378}, {"town", 1}, {"world", 0}}}), "receiver-house"},
		{"/hubs", Json::array({1}), "cargo-custody"}, {"/stocks", Json::array({1}), "cargo-custody"},
		{"/trains/0/engine", 33, "inherited-fleet"}, {"/trains/7/owner", 1, "inherited-fleet"},
		{"/trains/3/crashed", true, "inherited-fleet"}, {"/trains/4/lost", true, "inherited-fleet"},
		{"/trains/4/orders/1/1", 2, "inherited-orders"},
	};
	for (const auto &denial : denials) {
		INFO(denial.path);
		auto state = FrozenAdmissionProjection();
		state[Json::json_pointer{denial.path}] = denial.value;
		const auto before = state.dump();
		CHECK(GetCoreBasketAdmissionError(state, LoadedContext(), false) == denial.reason);
		CHECK(state.dump() == before);
	}
	for (const char *key : {"company", "worlds", "towns", "stations", "trains"}) {
		auto state = FrozenAdmissionProjection(); state.erase(key);
		const auto before = state.dump();
		CHECK(GetCoreBasketAdmissionError(state, LoadedContext(), false) == "checkpoint-json");
		CHECK(state.dump() == before);
	}
	auto malformed = FrozenAdmissionProjection(); malformed["towns"][0]["population"] = "1191";
	const auto before = malformed.dump();
	CHECK(GetCoreBasketAdmissionError(malformed, LoadedContext(), false) == "checkpoint-json");
	CHECK(malformed.dump() == before);
}

TEST_CASE("Core basket cold admission stays within the initial phase tick and fleet bounds", "[core-construction-basket][basket-admission]")
{
	auto state = FrozenAdmissionProjection();
	/* The fixed four services add twelve native units: four engines and eight wagons.
	 * Their exact consists and orders have a separate console continuation guard. */
	for (uint id = 8; id < 20; ++id) state["trains"].push_back({{"id", id}});
	state["money"] = 3400000;
	for (uint64_t tick : {167168 + 2048u, 167168 + 240 * 2048u}) {
		state["tick"] = tick;
		const auto before = state.dump();
		CHECK(GetCoreBasketAdmissionError(state, LoadedContext(), true).empty());
		CHECK(state.dump() == before);
	}
	for (uint64_t tick : {167168u, 167168 + 240 * 2048u + 1}) {
		state["tick"] = tick;
		CHECK(GetCoreBasketAdmissionError(state, LoadedContext(), true) == "tick-seed");
	}
	state["tick"] = 167168 + 2048;
	state["trains"].erase(state["trains"].size() - 1);
	CHECK(GetCoreBasketAdmissionError(state, LoadedContext(), true) == "fleet");
	state = FrozenAdmissionProjection(); state["trains"].erase(7);
	CHECK(GetCoreBasketAdmissionError(state, LoadedContext(), false) == "fleet");
}

TEST_CASE("Core basket reuses owned curved rail without adopting neutral or incompatible track", "[core-construction-basket][basket-rail]")
{
	BasketFixture fixture;
	const TileIndex tile = TileXY(35, 35);
	MakeRailNormal(tile, CompanyID{0}, TrackBits{Track::Lower}, RAILTYPE_RAIL);
	const auto before = NativeEvidence();
	CHECK(Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, Track::Lower, false).Failed());
	const auto quote = QueryCoreBasketRail(tile, Track::Lower);
	REQUIRE(quote.Succeeded());
	CHECK(quote.GetCost() == 0);
	CHECK(GetTrackBits(tile) == TrackBits{Track::Lower});
	CHECK(NativeEvidence() == before);
	for (Owner owner : {Owner{OWNER_NONE}, Owner{CompanyID{1}}}) {
		MakeRailNormal(tile, owner, TrackBits{Track::Lower}, RAILTYPE_RAIL);
		const auto unchanged = NativeEvidence();
		CHECK(QueryCoreBasketRail(tile, Track::Lower, TrackBits{Track::Lower}).Failed());
		CHECK(GetTileOwner(tile) == owner);
		CHECK(GetTrackBits(tile) == TrackBits{Track::Lower});
		CHECK(NativeEvidence() == unchanged);
	}
	MakeRailNormal(tile, CompanyID{0}, TrackBits{Track::Lower}, RAILTYPE_ELECTRIC);
	CHECK(QueryCoreBasketRail(tile, Track::Lower).Failed());
	CHECK(GetRailType(tile) == RAILTYPE_ELECTRIC);
	CHECK(QueryCoreBasketRail(INVALID_TILE, Track::X).Failed());
	CHECK(QueryCoreBasketRail(TileXY(0, 0), Track::X).Failed());
	CHECK(QueryCoreBasketRail(tile, Track::Invalid).Failed());
	AutoRestoreBackup owner(_current_company, CompanyID{1});
	CHECK(QueryCoreBasketRail(TileXY(36, 35), Track::X).Failed());
}

TEST_CASE("Core basket future shared junction quotes match sequential native paid commands", "[core-construction-basket][basket-rail]")
{
	BasketFixture fixture;
	AutoRestoreBackup rail_price(_price[Price::BuildRail], Money{100});
	AutoRestoreBackup grass_price(_price[Price::ClearGrass], Money{20});
	const TileIndex tile = TileXY(35, 35);
	const auto before = NativeEvidence();
	const auto first = QueryCoreBasketRail(tile, Track::X);
	const auto second = QueryCoreBasketRail(tile, Track::Y, TrackBits{Track::X});
	const auto duplicate = QueryCoreBasketRail(tile, Track::X, TrackBits{Track::X});
	REQUIRE(first.Succeeded()); REQUIRE(second.Succeeded()); REQUIRE(duplicate.Succeeded());
	CHECK(first.GetCost() > 0);
	CHECK(second.GetCost() > 0);
	CHECK(duplicate.GetCost() == 0);
	CHECK(NativeEvidence() == before);
	const auto native_first = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Track::X, false);
	REQUIRE(native_first.Succeeded());
	CHECK(native_first.GetCost() == first.GetCost());
	const auto native_second_query = Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, Track::Y, false);
	REQUIRE(native_second_query.Succeeded());
	CHECK(native_second_query.GetCost() == second.GetCost());
	const auto native_second = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Track::Y, false);
	REQUIRE(native_second.Succeeded());
	CHECK(native_second.GetCost() == second.GetCost());
	CHECK(first.GetCost() + second.GetCost() == native_first.GetCost() + native_second.GetCost());
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == before["money"].get<int64_t>() - int64_t(first.GetCost()) - int64_t(second.GetCost()));
	CHECK(GetTrackBits(tile) == TRACK_BIT_CROSS);
	CHECK(QueryCoreBasketRail(tile, Track::X).GetCost() == 0);
}

TEST_CASE("Core basket prospective junctions retain native occupancy and world placement denials", "[core-construction-basket][basket-rail]")
{
	BasketFixture fixture;
	const TileIndex tile = TileXY(35, 35);
	SECTION("An inherited train blocks a new overlapping piece but permits exact reuse") {
		MakeRailNormal(tile, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_RAIL);
		REQUIRE(Train::CanAllocateItem());
		Train *train = Train::Create();
		train->owner = CompanyID{0};
		train->tile = tile;
		train->track = TrackBits{Track::X};
		train->UpdatePosition();
		const auto before = NativeEvidence();
		const auto native = Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, Track::Y, false);
		REQUIRE(native.Failed());
		const auto quote = QueryCoreBasketRail(tile, Track::Y, TrackBits{Track::X});
		CHECK(quote.Failed());
		CHECK(quote.GetErrorMessage() == native.GetErrorMessage());
		const auto reuse = QueryCoreBasketRail(tile, Track::X);
		REQUIRE(reuse.Succeeded());
		CHECK(reuse.GetCost() == 0);
		CHECK(GetTrackBits(tile) == TrackBits{Track::X});
		CHECK(train->tile == tile); CHECK(train->track == TrackBits{Track::X});
		CHECK(NativeEvidence() == before);
	}
	SECTION("A prospective piece cannot turn terrain outside every world into private rail") {
		PlanetManager::Reset();
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "Small Core", .phase = WorldPhase::Phase1_Core,
			.min_x = 1, .min_y = 1, .max_x = 10, .max_y = 10}));
		REQUIRE(PlanetManager::GetTileWorld(tile) == INVALID_WORLD);
		const auto before = NativeEvidence();
		const auto native = Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, Track::Y, false);
		REQUIRE(native.Failed());
		const auto quote = QueryCoreBasketRail(tile, Track::Y, TrackBits{Track::X});
		CHECK(quote.Failed());
		CHECK(quote.GetErrorMessage() == native.GetErrorMessage());
		CHECK(NativeEvidence() == before);
	}
}

TEST_CASE("Core basket monthly audit distinguishes missing BALL from complete atomic consumption", "[core-construction-basket][basket-monthly]")
{
	BasketFixture fixture;
	Town *town = fixture.Core();
	REQUIRE(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 120, true) == 120);
	REQUIRE(IntegratedEconomy::AcceptCity(town->index, C("STEL"), 20, true) == 20);
	REQUIRE(IntegratedEconomy::AcceptCity(town->index, C("BALL"), 19, true) == 19);
	std::vector<Json> months;
	CommonwealthSliceAudit audit;
	AutoRestoreBackup observer(_integrated_city_month_audit, &months);
	AutoRestoreBackup cargo_observer(_commonwealth_slice_audit, &audit);
	AutoRestoreBackup tick(TimerGameTick::counter, uint64_t{1000});
	float growth = 99, passengers = 99;
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	REQUIRE(months.size() == 1);
	CHECK(months[0]["food_sufficient"] == true);
	CHECK(months[0]["construction_complete"] == false);
	CHECK(growth == 0); CHECK(passengers == 1);
	CHECK(CargoMap(months[0]["consumed"]) == std::map<CargoType, uint32_t>{{C("FOOD"), 60}});
	CHECK(CargoMap(months[0]["after"]).at(C("STEL")) == 20);
	CHECK(CargoMap(months[0]["after"]).at(C("BALL")) == 19);
	CHECK(audit.consumed[C("STEL")] == 0); CHECK(audit.consumed[C("BALL")] == 0);
	CHECK(months[0]["native_growth_observed"] == false);
	REQUIRE(IntegratedEconomy::AcceptCity(town->index, C("BALL"), 1, true) == 1);
	++TimerGameTick::counter;
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	REQUIRE(months.size() == 2);
	CHECK(months[1]["construction_complete"] == true);
	CHECK(months[1]["growth"] == 1); CHECK(months[1]["passengers"] == 1);
	CHECK(growth == 1); CHECK(passengers == 1);
	CHECK(CargoMap(months[1]["consumed"]) == std::map<CargoType, uint32_t>{{C("FOOD"), 60}, {C("STEL"), 20}, {C("BALL"), 20}});
	CHECK(audit.consumed[C("FOOD")] == 120);
	CHECK(audit.consumed[C("STEL")] == 20); CHECK(audit.consumed[C("BALL")] == 20);
	CHECK(months[0]["construction_complete"] == false);
	const auto saved = IntegratedEconomy::Save();
	IntegratedEconomy::Reset();
	REQUIRE(IntegratedEconomy::Load(saved));
	CHECK(IntegratedEconomy::Save() == saved);
	CHECK(IntegratedEconomy::City(town->index)->consumed.at(C("STEL")) == 20);
	CHECK(IntegratedEconomy::City(town->index)->consumed.at(C("BALL")) == 20);
}

TEST_CASE("Core basket observers preserve economy native growth flags cash and RNG", "[core-construction-basket][basket-monthly]")
{
	BasketFixture fixture;
	Town *town = fixture.Core();
	for (const char *label : {"FOOD", "STEL", "BALL"}) {
		CargoLabel cargo; std::copy_n(label, 4, cargo.begin());
		const auto type = GetCargoTypeByLabel(cargo);
		REQUIRE(IntegratedEconomy::AcceptCity(town->index, type, IntegratedEconomy::CityDemand(town->index).at(type), true) > 0);
	}
	const auto initial = IntegratedEconomy::Save();
	float plain_growth, plain_passengers;
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, plain_growth, plain_passengers));
	const auto plain = NativeEvidence();
	REQUIRE(IntegratedEconomy::Load(initial));
	std::vector<Json> months;
	AutoRestoreBackup observer(_integrated_city_month_audit, &months);
	float growth, passengers;
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(NativeEvidence() == plain);
	CHECK(growth == plain_growth); CHECK(passengers == plain_passengers);
	REQUIRE(months.size() == 1);
	const auto mode = GENERATE(0, 1, 2);
	/* Set fixture-native flags to distinguish permission from active growth.
	 * This case checks observation without advancing simulation; existing native
	 * save/load tests separately cover MEGA growth-state retention. */
	town->growth_rate = mode == 0 ? TOWN_GROWTH_RATE_NONE : 87;
	town->grow_counter = 13;
	town->flags.Set(TownFlag::IsGrowing, mode == 2);
	town->flags.Set(TownFlag::CustomGrowth, mode == 2);
	const auto before = NativeEvidence();
	IntegratedEconomy::ObserveMonthlyGrowth(town->index);
	CHECK(NativeEvidence() == before);
	CHECK(months[0]["native_growth_observed"] == true);
	CHECK(months[0]["native_growth_rate"] == town->growth_rate);
	CHECK(months[0]["native_grow_counter"] == 13);
	CHECK(months[0]["native_flags"] == town->flags.base());
	CHECK(months[0]["native_growth_hook_enabled"] == (mode == 1));
	CHECK(months[0]["native_growth_enabled"] == (mode == 2));
	const auto event = months[0];
	IntegratedEconomy::ObserveMonthlyGrowth(TownID::Invalid());
	CHECK(months[0] == event);
	AutoRestoreBackup tick(TimerGameTick::counter, TimerGameTick::counter + 1);
	IntegratedEconomy::ObserveMonthlyGrowth(town->index);
	CHECK(months[0] == event);
}
