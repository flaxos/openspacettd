/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_first_line_approach.cpp Fixed native command fixtures, never generated scenarios. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/first_line_approach.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/portal_terminal.h"
#include "../portal/resource_sites.h"
#include "../portal/connected_economy.h"
#include "../portal/integrated_economy.h"
#include "../command_func.h"
#include "../bridge.h"
#include "../company_base.h"
#include "../core/backup_type.hpp"
#include "../economy_base.h"
#include "../core/random_func.hpp"
#include "../map_func.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../road_map.h"
#include "../water_map.h"
#include "../void_map.h"
#include "../town_map.h"
#include "../town.h"
#include "../table/strings.h"
#include "../industry_map.h"
#include "../tunnelbridge_map.h"
#include "../window_func.h"
#include "../safeguards.h"
#include <algorithm>

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace {
using Json = nlohmann::json;
struct ApproachFixture {
	GameSettings old_settings = _settings_game;
	Money old_rail_price = _price[Price::BuildRail];
	Money old_bridge_price = _price[Price::BuildBridge];
	std::array<BridgeSpec, MAX_BRIDGES> old_bridges{};
	ApproachFixture()
	{
		IntegratedEconomy::Reset(); // This historical authority witness explicitly exercises Classic.
		for (size_t i = 0; i < MAX_BRIDGES; ++i) old_bridges[i] = _bridge[i];
		SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 2000);
		ResourceSiteManager::Reset();
		ResetBridges();
		_settings_game.construction.max_bridge_length = 16;
		_settings_game.construction.max_bridge_height = 12;
		_settings_game.construction.crossing_with_competitor = true; // Retained opening PATS: ordinary company-on-town crossings are enabled.
		_price[Price::BuildRail] = Money{100};
		_price[Price::BuildBridge] = Money{275};
		for (uint y = 1; y < 63; ++y) for (uint x = 1; x < 63; ++x) SetTileHeight(TileXY(x, y), 1);
		ResetRailTypes();
		Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_RAIL);
		Company::Get(CompanyID{0})->clear_limit = 1000 << 16;
	}
	~ApproachFixture()
	{
		UpdateSignalsInBuffer(); // Finish native rail notifications against this fixture's still-live map.
		UnInitWindowSystem(); PortalRegistry::Reset();
		_settings_game = old_settings;
		_price[Price::BuildRail] = old_rail_price;
		_price[Price::BuildBridge] = old_bridge_price;
		for (size_t i = 0; i < MAX_BRIDGES; ++i) _bridge[i] = old_bridges[i];
	}
	PortalEndpoint Gate() const { return {TileXY(54, 30), DiagDirection::SW, WorldID{0}}; }
	TileIndex Platform() const { return TileXY(10, 30); }
	std::set<TileIndex> Blocked() const
	{
		return {TileXY(10, 30), TileXY(11, 30), TileXY(12, 30), TileXY(13, 30)};
	}
	bool Quote(FirstLineApproachPlan &plan, Json &report, FirstLineApproachBudget &budget)
	{
		std::map<TileIndex, TrackBits> rails;
		FirstLineBridgeReservations bridges;
		return FirstLineQuoteApproach(Platform(), Axis::X, 4, Gate(), Blocked(), rails, bridges, budget, plan, report);
	}
};

struct TownRoadCell { uint8_t x, y, bits; };
static constexpr std::array<TownRoadCell, 15> RETAINED_MITO_ROADS{{
	{23, 28, 11}, {24, 28, 14}, {25, 28, 11}, {26, 28, 14}, {27, 28, 10}, {28, 28, 12},
	{24, 29, 5}, {28, 29, 5},
	{23, 30, 14}, {24, 30, 11}, {25, 30, 15}, {26, 30, 10}, {27, 30, 10},
	{25, 31, 5}, {28, 31, 5},
}};

void SetRetainedLowRatingRoads()
{
	_settings_game.difficulty.town_council_tolerance = TOWN_COUNCIL_LENIENT;
	_settings_game.construction.extra_dynamite = true;
	Town *town = Town::Get(TownID{0});
	town->have_ratings.Set(CompanyID{0});
	town->ratings[CompanyID{0}] = 114; // Saved company-0 rating after authoring, never a production grant.
	for (const auto &cell : RETAINED_MITO_ROADS) {
		MakeRoadNormal(TileXY(cell.x, cell.y), static_cast<RoadBits>(cell.bits), ROADTYPE_ROAD,
			INVALID_ROADTYPE, town->index, OWNER_TOWN, OWNER_NONE);
	}
}

RoadBits PreservedRoadBits(TileIndex tile)
{
	return IsLevelCrossingTile(tile) ? GetCrossingRoadBits(tile) : GetAllRoadBits(tile);
}
}

TEST_CASE("First Line flat approach uses only quoted ordinary native rails", "[first-line-approach]")
{
	ApproachFixture fixture;
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	const auto rng_before = _random;
	const size_t sites_before = ResourceSiteManager::Sites().size();
	REQUIRE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK(report.at("status") == "quoted");
	CHECK(report.at("bridge_pieces") == 0);
	CHECK(plan.cost > 0);
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	CHECK(_random.state[0] == rng_before.state[0]);
	CHECK(_random.state[1] == rng_before.state[1]);
	CHECK(ResourceSiteManager::Sites().size() == sites_before);
	CHECK(IsTileType(plan.start, TileType::Clear));
	Json execution;
	REQUIRE(FirstLineExecuteApproach(plan, execution));
	INFO(execution.dump());
	CHECK(execution.at("actual_cost") == plan.cost);
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - plan.cost);
	CHECK(IsPlainRailTile(plan.start));
}

TEST_CASE("First Line execution receipt preserves paid rail after a later native refusal", "[first-line-approach]")
{
	ApproachFixture fixture;
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json quote_report;
	REQUIRE(fixture.Quote(plan, quote_report, budget));
	INFO(quote_report.dump());
	REQUIRE(plan.pieces.size() >= 3);
	const auto &first = plan.pieces[0], &later = plan.pieces[1], &trailing = plan.pieces[2];
	REQUIRE(first.kind == FirstLineApproachPiece::Kind::Rail);
	REQUIRE(later.kind == FirstLineApproachPiece::Kind::Rail);
	REQUIRE(trailing.kind == FirstLineApproachPiece::Kind::Rail);
	REQUIRE(first.tile != later.tile);
	REQUIRE(trailing.tile != first.tile);
	REQUIRE(trailing.tile != later.tile);
	const auto first_quote = QueryCoreBasketRail(first.tile, first.track);
	REQUIRE(first_quote.Succeeded());
	const int64_t first_cost = int64_t(first_quote.GetCost());
	REQUIRE(first_cost > 0);
	const auto later_quote = QueryCoreBasketRail(later.tile, later.track);
	REQUIRE(later_quote.Succeeded());
	REQUIRE(int64_t(later_quote.GetCost()) > 0);

	SECTION("later native quote refuses foreign-owned rail") {
		MakeRailNormal(later.tile, CompanyID{1}, TrackBits{later.track}, RAILTYPE_RAIL);
		const Money funds = Company::Get(CompanyID{0})->money;
		Json receipt;
		CHECK_FALSE(FirstLineExecuteApproach(plan, receipt));
		INFO(receipt.dump());
		CHECK(receipt.at("status") == "quote_failed");
		CHECK(receipt.at("failed_piece") == 1);
		CHECK(receipt.at("actual_cost") == first_cost);
		CHECK(int64_t(funds) - int64_t(Company::Get(CompanyID{0})->money) == first_cost);
		CHECK(IsPlainRailTile(first.tile));
		CHECK(GetTileOwner(later.tile) == CompanyID{1});
		CHECK(IsTileType(trailing.tile, TileType::Clear));
	}
	SECTION("later native execution refuses insufficient cash") {
		Company::Get(CompanyID{0})->money = Money(first_cost);
		Json receipt;
		CHECK_FALSE(FirstLineExecuteApproach(plan, receipt));
		INFO(receipt.dump());
		CHECK(receipt.at("status") == "execute_failed");
		CHECK(receipt.at("failed_piece") == 1);
		CHECK(receipt.at("actual_cost") == first_cost);
		CHECK(int64_t(Company::Get(CompanyID{0})->money) == 0);
		CHECK(IsPlainRailTile(first.tile));
		CHECK_FALSE(IsPlainRailTile(later.tile));
		CHECK(IsTileType(trailing.tile, TileType::Clear));
	}
}

TEST_CASE("First Line spans a complete one-water-tile barrier with a paid native bridge", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeSea(TileXY(25, y));
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	const auto rng_before = _random;
	const size_t sites_before = ResourceSiteManager::Sites().size();
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const bool quoted = fixture.Quote(plan, report, budget);
	INFO(report.dump());
	REQUIRE(quoted);
	CHECK(report.at("bridge_pieces").get<uint32_t>() > 0);
	CHECK(IsTileType(TileXY(25, 30), TileType::Water));
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	CHECK(_random.state[0] == rng_before.state[0]);
	CHECK(_random.state[1] == rng_before.state[1]);
	CHECK(ResourceSiteManager::Sites().size() == sites_before);
	Json execution;
	REQUIRE(FirstLineExecuteApproach(plan, execution));
	INFO(execution.dump());
	CHECK(execution.at("actual_cost") == plan.cost);
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - plan.cost);
	CHECK(IsTileType(TileXY(25, 30), TileType::Water));
	CHECK(std::ranges::any_of(plan.pieces, [](const FirstLineApproachPiece &piece) {
		return piece.kind == FirstLineApproachPiece::Kind::Bridge && IsBridgeTile(piece.tile) && IsBridgeTile(piece.end);
	}));
}

TEST_CASE("First Line disconnected void barrier reports a finite refusal without mutation", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeVoid(TileXY(25, y));
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	CHECK_FALSE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK((report.at("status") == "no_route" || report.at("status") == "leg_state_cap" || report.at("status") == "budget_exhausted"));
	CHECK(report.at("states").get<uint32_t>() <= 30000);
	CHECK(budget.states <= FirstLineApproachBudget::MAX_STATES);
	CHECK(budget.queries <= FirstLineApproachBudget::MAX_QUERIES);
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	CHECK(IsTileType(TileXY(24, 30), TileType::Clear));
}

TEST_CASE("First Line cannot adopt a foreign rail wall", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeRailNormal(TileXY(25, y), CompanyID{1}, TrackBits{Track::X}, RAILTYPE_RAIL);
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const Money funds = Company::Get(CompanyID{0})->money;
	CHECK_FALSE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetTileOwner(TileXY(25, 30)) == CompanyID{1});
}

TEST_CASE("First Line checks native road crossing instead of clearing the road", "[first-line-approach]")
{
	ApproachFixture fixture;
	TileIndex road = TileXY(25, 30);
	for (uint y = 1; y < 63; ++y) MakeRoadNormal(TileXY(25, y), ROAD_Y, ROADTYPE_ROAD, INVALID_ROADTYPE, TownID{0}, CompanyID{0}, OWNER_NONE);
	CHECK(QueryCoreBasketRail(road, Track::X).Succeeded());
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan, shared;
	Json report, shared_report;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	const auto blocked = fixture.Blocked();
	const bool quoted = FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked,
		rails, bridges, budget, plan, report);
	INFO(report.dump());
	REQUIRE(quoted);
	REQUIRE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked,
		rails, bridges, budget, shared, shared_report));
	CHECK(shared.cost == 0);
	CHECK(IsTileType(road, TileType::Road));
	REQUIRE(std::ranges::any_of(plan.pieces, [](const FirstLineApproachPiece &piece) {
		return piece.kind == FirstLineApproachPiece::Kind::Rail && TileX(piece.tile) == 25;
	}));
	Json execution;
	const Money funds = Company::Get(CompanyID{0})->money;
	REQUIRE(FirstLineExecuteApproach(plan, execution));
	CHECK(execution.at("actual_cost") == plan.cost);
	CHECK(std::ranges::any_of(plan.pieces, [](const FirstLineApproachPiece &piece) {
		return piece.kind == FirstLineApproachPiece::Kind::Rail && TileX(piece.tile) == 25 && IsLevelCrossingTile(piece.tile);
	}));
	REQUIRE(FirstLineExecuteApproach(shared, execution));
	CHECK(execution.at("actual_cost") == 0);
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - plan.cost);
}

TEST_CASE("First Line old road-demolition sequence exhausts saved town authority", "[first-line-approach][first-line-authority]")
{
	ApproachFixture fixture;
	SetRetainedLowRatingRoads();
	Town *town = Town::Get(TownID{0});
	const std::array<FirstLineApproachPiece, 5> pieces{{
		{FirstLineApproachPiece::Kind::Rail, TileXY(27, 30), Track::X, INVALID_TILE},
		{FirstLineApproachPiece::Kind::Rail, TileXY(26, 30), Track::X, INVALID_TILE},
		{FirstLineApproachPiece::Kind::Rail, TileXY(25, 30), Track::X, INVALID_TILE},
		{FirstLineApproachPiece::Kind::Rail, TileXY(24, 30), Track::Left, INVALID_TILE},
		{FirstLineApproachPiece::Kind::Rail, TileXY(24, 29), Track::Y, INVALID_TILE},
	}};
	FirstLineApproachPlan old_route;
	old_route.pieces.assign(pieces.begin(), pieces.end());
	for (const auto &piece : pieces) {
		const auto native = QueryCoreBasketRail(piece.tile, piece.track);
		INFO("tile=" << piece.tile.base() << " track=" << static_cast<uint>(piece.track) << " native_error=" << native.GetErrorMessage().base());
		REQUIRE(native.Succeeded()); // Each separate normal quote sees the saved 114 rating.
		old_route.cost += int64_t(native.GetCost());
	}
	CHECK(town->ratings[CompanyID{0}] == 114);
	const Money funds = Company::Get(CompanyID{0})->money;
	SECTION("ordinary native commands expose each retained authority debit") {
		const std::array<int16_t, 4> ratings{{96, 78, 28, -22}};
		int64_t spent = 0;
		for (size_t i = 0; i < ratings.size(); ++i) {
			const auto quote = QueryCoreBasketRail(pieces[i].tile, pieces[i].track);
			REQUIRE(quote.Succeeded());
			const auto done = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute,
				pieces[i].tile, RAILTYPE_RAIL, pieces[i].track, false);
			REQUIRE(done.Succeeded());
			CHECK(done.GetCost() == quote.GetCost());
			spent += int64_t(done.GetCost());
			CHECK(int64_t(funds) - int64_t(Company::Get(CompanyID{0})->money) == spent);
			CHECK(town->ratings[CompanyID{0}] == ratings[i]);
		}
		const auto refused = QueryCoreBasketRail(pieces[4].tile, pieces[4].track);
		CHECK(refused.Failed());
		CHECK(refused.GetErrorMessage() == STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
		CHECK(int64_t(funds) - int64_t(Company::Get(CompanyID{0})->money) == spent);
	}
	SECTION("legacy quoted route has an honest paid partial receipt") {
		Json receipt;
		CHECK_FALSE(FirstLineExecuteApproach(old_route, receipt));
		INFO(receipt.dump());
		CHECK(receipt.at("status") == "quote_failed");
		CHECK(receipt.at("failed_piece") == 4);
		CHECK(town->ratings[CompanyID{0}] == -22); // 114 - 18 - 18 - 50 - 50.
		CHECK(receipt.at("actual_cost").get<int64_t>() == int64_t(funds) - int64_t(Company::Get(CompanyID{0})->money));
		for (size_t i = 0; i < 4; ++i) CHECK(IsPlainRailTile(pieces[i].tile));
		CHECK_FALSE(IsPlainRailTile(pieces[4].tile));
	}
}

TEST_CASE("First Line chooses the clean platform end and preserves saved town roads", "[first-line-approach][first-line-authority]")
{
	ApproachFixture fixture;
	SetRetainedLowRatingRoads();
	MakeHouseTile(TileXY(33, 30), TownID{0}, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true);
	MakeHouseTile(TileXY(32, 31), TownID{0}, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true);
	const TileIndex platform = TileXY(28, 30);
	const PortalEndpoint west_gate{TileXY(25, 30), DiagDirection::SW, WorldID{0}};
	const std::set<TileIndex> shell{TileXY(28, 30), TileXY(29, 30), TileXY(30, 30), TileXY(31, 30)};
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	const auto rng_before = _random;
	const bool quoted = FirstLineQuoteApproach(platform, Axis::X, 4, west_gate, shell, rails, bridges, budget, plan, report);
	INFO(report.dump());
	REQUIRE(quoted);
	CHECK(plan.start == TileXY(32, 30));
	CHECK(report.at("start") == plan.start.base());
	CHECK(budget.states <= FirstLineApproachBudget::MAX_STATES);
	CHECK(budget.queries <= FirstLineApproachBudget::MAX_QUERIES);
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	CHECK(_random.state[0] == rng_before.state[0]);
	CHECK(_random.state[1] == rng_before.state[1]);
	CHECK(Town::Get(TownID{0})->ratings[CompanyID{0}] == 114);
	Json execution;
	REQUIRE(FirstLineExecuteApproach(plan, execution));
	INFO(execution.dump());
	CHECK(execution.at("actual_cost") == plan.cost);
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - plan.cost);
	CHECK(Town::Get(TownID{0})->ratings[CompanyID{0}] == 114);
	for (const auto &cell : RETAINED_MITO_ROADS) {
		TileIndex tile = TileXY(cell.x, cell.y);
		CHECK(IsTileType(tile, TileType::Road));
		CHECK(GetRoadOwner(tile, RoadTramType::Road) == OWNER_TOWN);
		CHECK(PreservedRoadBits(tile) == static_cast<RoadBits>(cell.bits));
	}
}

TEST_CASE("First Line crosses a town-owned straight road without a rating penalty", "[first-line-approach][first-line-authority]")
{
	ApproachFixture fixture;
	_settings_game.difficulty.town_council_tolerance = TOWN_COUNCIL_LENIENT;
	Town *town = Town::Get(TownID{0});
	town->have_ratings.Set(CompanyID{0}); town->ratings[CompanyID{0}] = 114;
	for (uint y = 1; y < 63; ++y) MakeRoadNormal(TileXY(20, y), ROAD_Y, ROADTYPE_ROAD,
		INVALID_ROADTYPE, town->index, OWNER_TOWN, OWNER_NONE);
	const TileIndex platform = TileXY(28, 30);
	const PortalEndpoint west_gate{TileXY(25, 30), DiagDirection::SW, WorldID{0}};
	const std::set<TileIndex> shell{TileXY(28, 30), TileXY(29, 30), TileXY(30, 30), TileXY(31, 30)};
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const bool quoted = FirstLineQuoteApproach(platform, Axis::X, 4, west_gate, shell, rails, bridges, budget, plan, report);
	INFO(report.dump());
	REQUIRE(quoted);
	REQUIRE(std::ranges::any_of(plan.pieces, [](const FirstLineApproachPiece &piece) {
		return piece.kind == FirstLineApproachPiece::Kind::Rail && TileX(piece.tile) == 20 && piece.track == Track::X;
	}));
	Json execution;
	REQUIRE(FirstLineExecuteApproach(plan, execution));
	CHECK(IsLevelCrossingTile(TileXY(20, 30)));
	CHECK(GetRoadOwner(TileXY(20, 30), RoadTramType::Road) == OWNER_TOWN);
	CHECK(GetCrossingRoadBits(TileXY(20, 30)) == ROAD_Y);
	CHECK(town->ratings[CompanyID{0}] == 114);
	FirstLineApproachBudget reuse_budget;
	FirstLineApproachPlan reuse;
	Json reuse_report;
	std::map<TileIndex, TrackBits> fresh_rails;
	FirstLineBridgeReservations fresh_bridges;
	const bool requoted = FirstLineQuoteApproach(platform, Axis::X, 4, west_gate, shell,
		fresh_rails, fresh_bridges, reuse_budget, reuse, reuse_report);
	INFO(reuse_report.dump());
	REQUIRE(requoted); // The owned native crossing is now on the map, not in prospective rails.
	CHECK(reuse.cost == 0);
	Json reuse_execution;
	REQUIRE(FirstLineExecuteApproach(reuse, reuse_execution));
	CHECK(reuse_execution.at("actual_cost") == 0);
	CHECK(town->ratings[CompanyID{0}] == 114);
}

TEST_CASE("First Line bridges a road then water from clear heads without town demolition", "[first-line-approach][first-line-authority]")
{
	ApproachFixture fixture;
	_settings_game.difficulty.town_council_tolerance = TOWN_COUNCIL_LENIENT;
	Town *town = Town::Get(TownID{0});
	town->have_ratings.Set(CompanyID{0}); town->ratings[CompanyID{0}] = 114;
	for (uint y = 1; y < 63; ++y) {
		MakeRoadNormal(TileXY(24, y), ROAD_Y, ROADTYPE_ROAD, INVALID_ROADTYPE,
			town->index, OWNER_TOWN, OWNER_NONE);
		MakeSea(TileXY(25, y));
	}
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	REQUIRE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	auto bridge = std::ranges::find_if(plan.pieces, [](const FirstLineApproachPiece &piece) {
		return piece.kind == FirstLineApproachPiece::Kind::Bridge && TileX(piece.tile) < 24 && TileX(piece.end) > 25;
	});
	REQUIRE(bridge != plan.pieces.end());
	CHECK(IsTileType(bridge->tile, TileType::Clear));
	CHECK(IsTileType(bridge->end, TileType::Clear));
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	CHECK(town->ratings[CompanyID{0}] == 114);
	Json execution;
	REQUIRE(FirstLineExecuteApproach(plan, execution));
	INFO(execution.dump());
	CHECK(execution.at("actual_cost") == plan.cost);
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - plan.cost);
	CHECK((IsBridgeTile(bridge->tile) && IsBridgeTile(bridge->end)));
	CHECK(IsTileType(TileXY(24, 30), TileType::Road));
	CHECK(GetRoadOwner(TileXY(24, 30), RoadTramType::Road) == OWNER_TOWN);
	CHECK(GetAllRoadBits(TileXY(24, 30)) == ROAD_Y);
	CHECK(town->ratings[CompanyID{0}] == 114);
}

TEST_CASE("First Line refuses two blocked platform exits within its shared budget", "[first-line-approach][first-line-authority]")
{
	ApproachFixture fixture;
	MakeVoid(TileXY(9, 30));
	MakeVoid(TileXY(14, 30));
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	CHECK_FALSE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK(report.at("status") != "quoted");
	CHECK(budget.states <= FirstLineApproachBudget::MAX_STATES);
	CHECK(budget.queries <= FirstLineApproachBudget::MAX_QUERIES);
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
}

TEST_CASE("First Line second leg reuses the exact paid bridge and rails", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeSea(TileXY(25, y));
	FirstLineApproachBudget budget;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	FirstLineApproachPlan first, second;
	Json a, b;
	const auto blocked = fixture.Blocked();
	const bool first_quoted = FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked, rails, bridges, budget, first, a);
	INFO(a.dump());
	REQUIRE(first_quoted);
	REQUIRE(a.at("bridge_pieces").get<uint32_t>() > 0);
	const Money funds = Company::Get(CompanyID{0})->money;
	const auto map_before = GetConnectedEconomyMapFingerprint();
	REQUIRE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked, rails, bridges, budget, second, b));
	INFO(b.dump());
	CHECK(b.at("reused_bridge_pieces").get<uint32_t>() > 0);
	CHECK(second.cost == 0);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	Json execution;
	REQUIRE(FirstLineExecuteApproach(first, execution));
	auto reused = std::ranges::find_if(second.pieces, [](const FirstLineApproachPiece &piece) { return piece.kind == FirstLineApproachPiece::Kind::ReuseBridge; });
	REQUIRE(reused != second.pieces.end());
	SetTileOwner(reused->tile, CompanyID{1});
	CHECK_FALSE(FirstLineExecuteApproach(second, execution));
	CHECK(execution.at("status") == "reused_bridge_missing");
	SetTileOwner(reused->tile, CompanyID{0});
	REQUIRE(FirstLineExecuteApproach(second, execution));
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - first.cost);
}

TEST_CASE("First Line invalid requests and exhausted shared budget are read-only", "[first-line-approach]")
{
	ApproachFixture fixture;
	const Money funds = Company::Get(CompanyID{0})->money;
	const auto map_before = GetConnectedEconomyMapFingerprint();
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	const auto blocked = fixture.Blocked();
	CHECK_FALSE(FirstLineQuoteApproach(fixture.Platform(), Axis::Invalid, 4, fixture.Gate(), blocked, rails, bridges, budget, plan, report));
	CHECK(report.at("status") == "invalid_request");
	{
		AutoRestoreBackup actor(_current_company, CompanyID{1});
		CHECK_FALSE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked, rails, bridges, budget, plan, report));
		CHECK(report.at("status") == "invalid_request");
	}
	CHECK_FALSE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 0, fixture.Gate(), blocked, rails, bridges, budget, plan, report));
	CHECK(report.at("status") == "invalid_request");
	PortalEndpoint wrong = fixture.Gate(); wrong.world_id = WorldID{99};
	CHECK_FALSE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, wrong, blocked, rails, bridges, budget, plan, report));
	CHECK(report.at("status") == "invalid_request");
	budget.queries = FirstLineApproachBudget::MAX_QUERIES;
	CHECK_FALSE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked, rails, bridges, budget, plan, report));
	CHECK(report.at("budget_exhausted") == true);
	CHECK(rails.empty());
	CHECK(bridges.empty());
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
}

TEST_CASE("First Line reports the consumed first root when the second exceeds the shared state cap", "[first-line-approach]")
{
	ApproachFixture fixture;
	FirstLineApproachBudget budget;
	budget.states = FirstLineApproachBudget::MAX_STATES - 1;
	FirstLineApproachPlan plan;
	Json report;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	const Money funds = Company::Get(CompanyID{0})->money;
	const uint64_t map_before = GetConnectedEconomyMapFingerprint();
	const auto rng_before = _random;
	CHECK_FALSE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), fixture.Blocked(),
		rails, bridges, budget, plan, report));
	INFO(report.dump());
	CHECK(report.at("status") == "budget_exhausted");
	CHECK(report.at("budget_exhausted") == true);
	CHECK(report.at("shared_states") == FirstLineApproachBudget::MAX_STATES);
	CHECK(budget.states == FirstLineApproachBudget::MAX_STATES);
	CHECK(budget.queries == 0);
	CHECK(budget.exhausted);
	CHECK(rails.empty());
	CHECK(bridges.empty());
	CHECK(Company::Get(CompanyID{0})->money == funds);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
	CHECK(_random.state[0] == rng_before.state[0]);
	CHECK(_random.state[1] == rng_before.state[1]);
}

TEST_CASE("First Line can reuse a planned ordinary bridge in reverse", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeSea(TileXY(25, y));
	FirstLineApproachBudget budget;
	FirstLineApproachPlan east, west;
	Json first_report, second_report;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	const bool east_quoted = FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), fixture.Blocked(),
		rails, bridges, budget, east, first_report);
	INFO(first_report.dump());
	REQUIRE(east_quoted);
	std::set<TileIndex> other_platform{TileXY(40, 30), TileXY(41, 30), TileXY(42, 30), TileXY(43, 30)};
	PortalEndpoint west_gate{TileXY(25, 30), DiagDirection::SW, WorldID{0}};
	REQUIRE(FirstLineQuoteApproach(TileXY(40, 30), Axis::X, 4, west_gate, other_platform,
		rails, bridges, budget, west, second_report));
	INFO(second_report.dump());
	CHECK(second_report.at("reused_bridge_pieces").get<uint32_t>() > 0);
	CHECK(second_report.at("bridge_pieces").get<uint32_t>() == 0);
	const Money funds = Company::Get(CompanyID{0})->money;
	Json execution;
	REQUIRE(FirstLineExecuteApproach(east, execution));
	REQUIRE(FirstLineExecuteApproach(west, execution));
	CHECK(int64_t(Company::Get(CompanyID{0})->money) == int64_t(funds) - east.cost - west.cost);
}

TEST_CASE("First Line records native house and industry obstructions without reading nonexistent owners", "[first-line-approach]")
{
	ApproachFixture fixture;
	TileIndex start = TileXY(14, 30);
	MakeVoid(TileXY(9, 30)); // Isolate the obstructed east exit after both platform ends become searchable.
	SECTION("House tile") { MakeHouseTile(start, TownID{0}, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true); }
	SECTION("Industry tile") { MakeIndustry(start, IndustryID{0}, 0, 0, WaterClass::Invalid); }
	const auto map_before = GetConnectedEconomyMapFingerprint();
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	CHECK_FALSE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK(report.at("rejections").value("obstruction", 0u) > 0);
	CHECK_FALSE(report.at("samples").empty());
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
}

TEST_CASE("First Line accepts only a direction-matched reserved bridge head at route start", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeSea(TileXY(25, y));
	FirstLineApproachBudget budget;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	FirstLineApproachPlan first, from_head;
	Json first_report, report;
	REQUIRE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), fixture.Blocked(),
		rails, bridges, budget, first, first_report));
	auto bridge = std::ranges::find_if(first.pieces, [](const FirstLineApproachPiece &piece) { return piece.kind == FirstLineApproachPiece::Kind::Bridge; });
	REQUIRE(bridge != first.pieces.end());
	const uint x = TileX(bridge->tile), y = TileY(bridge->tile);
	REQUIRE(x > 4);
	TileIndex platform = TileXY(x - 4, y);
	std::set<TileIndex> head_platform{TileXY(x - 4, y), TileXY(x - 3, y), TileXY(x - 2, y), TileXY(x - 1, y)};
	REQUIRE(FirstLineQuoteApproach(platform, Axis::X, 4, fixture.Gate(), head_platform,
		rails, bridges, budget, from_head, report));
	INFO(report.dump());
	CHECK(from_head.start == bridge->tile);
	CHECK(report.at("reused_bridge_pieces").get<uint32_t>() > 0);
	/* These two are state-level refusals; their prospective platforms overlap the
	 * reserved span, so they are not claims of legal station construction. */
	PortalEndpoint west_gate{TileXY(25, y), DiagDirection::SW, WorldID{0}};
	std::set<TileIndex> wrong_platform{TileXY(x + 1, y), TileXY(x + 2, y), TileXY(x + 3, y), TileXY(x + 4, y)};
	CHECK_FALSE(FirstLineQuoteApproach(TileXY(x + 1, y), Axis::X, 4, west_gate, wrong_platform,
		rails, bridges, budget, from_head, report));
	CHECK(report.at("status") != "quoted");
	std::set<TileIndex> interior_platform{TileXY(x - 3, y), TileXY(x - 2, y), TileXY(x - 1, y), TileXY(x, y)};
	interior_platform.insert(TileXY(x - 4, y)); // Block the newly searchable clear opposite exit; isolate the bridge-span exit.
	CHECK_FALSE(FirstLineQuoteApproach(TileXY(x - 3, y), Axis::X, 4, fixture.Gate(), interior_platform,
		rails, bridges, budget, from_head, report));
	CHECK(report.at("status") != "quoted");
}

TEST_CASE("First Line uses the other end of one platform when its first exit is blocked", "[first-line-approach]")
{
	ApproachFixture fixture;
	MakeVoid(TileXY(14, 30));
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	REQUIRE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), fixture.Blocked(), rails, bridges, budget, plan, report));
	INFO(report.dump());
	CHECK(plan.start == TileXY(9, 30));
	CHECK(report.at("status") == "quoted");
	CHECK(budget.states <= FirstLineApproachBudget::MAX_STATES);
}

TEST_CASE("First Line quotes two opposed public approaches from one platform", "[first-line-approach]")
{
	ApproachFixture fixture;
	FirstLineApproachBudget budget;
	FirstLineApproachPlan east, west;
	Json a, b;
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	const auto blocked = fixture.Blocked();
	REQUIRE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, fixture.Gate(), blocked, rails, bridges, budget, east, a));
	PortalEndpoint opposite{TileXY(25, 30), DiagDirection::SW, WorldID{0}};
	REQUIRE(FirstLineQuoteApproach(fixture.Platform(), Axis::X, 4, opposite, blocked, rails, bridges, budget, west, b));
	INFO(b.dump());
	CHECK(east.start != west.start);
	CHECK(east.target != west.target);
	CHECK(budget.states <= FirstLineApproachBudget::MAX_STATES);
}

TEST_CASE("First Line does not adopt a neutral rail barrier", "[first-line-approach]")
{
	ApproachFixture fixture;
	for (uint y = 1; y < 63; ++y) MakeRailNormal(TileXY(25, y), OWNER_NONE, TrackBits{Track::X}, RAILTYPE_RAIL);
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	const auto map_before = GetConnectedEconomyMapFingerprint();
	CHECK_FALSE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK(report.at("rejections").value("ownership", 0u) > 0);
	CHECK(GetTileOwner(TileXY(25, 30)) == OWNER_NONE);
	CHECK(GetConnectedEconomyMapFingerprint() == map_before);
}

TEST_CASE("First Line reports native slope refusal on a valid one-height corner", "[first-line-approach]")
{
	ApproachFixture fixture;
	MakeVoid(TileXY(9, 30)); // Isolate the sloped east exit after both platform ends become searchable.
	AutoRestoreBackup slopes(_settings_game.construction.build_on_slopes, false);
	TileIndex start = TileXY(14, 30);
	SetTileHeight(start, 2); // Every adjacent corner remains height 1: no invalid terrain edge.
	const auto native = QueryCoreBasketRail(start, Track::X);
	REQUIRE(native.Failed());
	FirstLineApproachBudget budget;
	FirstLineApproachPlan plan;
	Json report;
	CHECK_FALSE(fixture.Quote(plan, report, budget));
	INFO(report.dump());
	CHECK_FALSE(report.at("errors").empty());
	CHECK(report.at("rejections").value("slope", 0u) > 0);
}
