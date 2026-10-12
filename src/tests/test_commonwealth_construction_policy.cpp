/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_commonwealth_construction_policy.cpp Fixed native fixtures, never scenario execution. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_manager_face.h"
#include "../core/random_func.hpp"
#include "../economy_base.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../road.h"
#include "../road_map.h"
#include "../road_cmd.h"
#include "../landscape_cmd.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../newgrf_station.h"
#include "../town.h"
#include "../town_map.h"
#include "../house.h"
#include "../newgrf_house.h"
#include "../train.h"
#include "../vehicle_func.h"
#include "../signal_func.h"
#include "../bridge.h"
#include "../bridge_map.h"
#include "../tunnel_map.h"
#include "../water_map.h"
#include "../blueprint/blueprint.h"
#include "../blueprint/blueprint_cmd.h"
#include "../cargotype.h"
#include "../portal/tech_tree.h"
#include "../portal/planet_manager.h"
#include "../newgrf_roadstop.h"
#include "../newgrf_airport.h"
#include "../newgrf_config.h"
#include "../portal/commonwealth_pack.h"
#include "../gfxinit.h"
#include "../airport.h"
#include "../station_base.h"
#include "../tunnelbridge_cmd.h"
#include "../network/network_internal.h"
#include "../portal/integrated_economy.h"
#include "../portal/connected_economy.h"
#include "../portal/company_stockpile.h"
#include "../portal/fabrication_manager.h"
#include "../window_func.h"
#include "../base_media_graphics.h"
#include "../fileio_func.h"
#include "../saveload/saveload.h"
#include "../driver.h"
#include "../video/video_driver.hpp"
#include "../table/strings.h"
#include <fstream>
#include <cstdlib>
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
extern RoadTypeInfo _roadtypes[ROADTYPE_END];
extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
extern void DetermineBasePaths(std::string_view exe);

namespace {
using Json = nlohmann::json;
void SetupPolicy(int mode)
{
	IntegratedEconomy::Reset();
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	_random.SetSeed(11);
	/* A real saved face label avoids native missing-style migration on cold load. */
	for (Company *company : Company::Iterate()) SetCompanyManagerFaceStyle(company->face,0);
	ResetRailTypes(); ResetRoadTypes(); ResetBridges(); StationClass::Reset();
	Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_RAIL);
	Company::Get(CompanyID{0})->avail_roadtypes.Set(ROADTYPE_ROAD);
	Company::Get(CompanyID{0})->avail_roadtypes.Set(ROADTYPE_TRAM);
	Company::Get(CompanyID{0})->clear_limit = 1000 << 16;
	_settings_game.station.station_spread = 12;
	_settings_game.station.distant_join_stations = true;
	_settings_game.construction.build_on_slopes = true;
	_settings_game.economy.dist_local_authority = 64;
	_settings_game.construction.crossing_with_competitor = false;
	_settings_game.construction.extra_dynamite = false;
	_settings_game.construction.max_bridge_length = 16;
	_settings_game.construction.max_bridge_height = 12;
	_settings_game.difficulty.town_council_tolerance = TOWN_COUNCIL_LENIENT;
	_settings_game.difficulty.infinite_money = false;
	_price[Price::BuildRail] = Money{100}; _price[Price::ClearRoad] = Money{100};
	_price[Price::BuildBridge] = Money{275}; _price[Price::ClearBridge] = Money{100};
	_price[Price::BuildRoad] = Money{100}; _price[Price::ClearTunnel] = Money{100}; _price[Price::ClearHouse] = Money{100};
	_price[Price::BuildStationBus] = Money{100}; _price[Price::BuildStationDock] = Money{100};
	_price[Price::BuildStationRail] = Money{100}; _price[Price::BuildStationRailLength] = Money{100};
	for (uint y = 1; y < 63; ++y) for (uint x = 1; x < 63; ++x) SetTileHeight(TileXY(x, y), 1);
	if (mode != 0) {
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::SetRulesVersion(mode == 1 ? EconomyRulesVersion::V1 : EconomyRulesVersion::V2);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
	}
	Town *town = Town::Get(TownID{0});
	InitializeBuildingCounts(town);
	town->have_ratings.Set(CompanyID{0}); town->ratings[CompanyID{0}] = -200;
}
struct PolicyFixture {
	GameSettings settings = _settings_game;
	Randomizer rng = _random;
	decltype(_price) prices = _price;
	std::array<BridgeSpec, MAX_BRIDGES> bridges;
	explicit PolicyFixture(int mode) { std::copy_n(_bridge, MAX_BRIDGES, bridges.begin()); SetupPolicy(mode); }
	~PolicyFixture() {
		UpdateSignalsInBuffer(); _vehicle_pool.CleanPool(); ResetVehicleHash();
		UnInitWindowSystem(); IntegratedEconomy::Reset(); SetupCargoForClimate(LandscapeType::Temperate); _settings_game = settings; _price = prices;
		_random = rng;
		std::copy(bridges.begin(), bridges.end(), _bridge);
	}
};
Json RoadState(TileIndex tile)
{
	Json result{{"tile", tile.base()}, {"type", to_underlying(GetTileType(tile))}, {"owner", GetTileOwner(tile).base()}};
	if (IsTileType(tile, TileType::Road)) {
		result["road_owner"] = GetRoadOwner(tile, RoadTramType::Road).base();
		result["tram_owner"] = GetRoadOwner(tile, RoadTramType::Tram).base();
		result["crossing"] = IsLevelCrossingTile(tile);
		result["road_bits"] = IsLevelCrossingTile(tile) ? GetCrossingRoadBits(tile).base() : GetAllRoadBits(tile).base();
	}
	return result;
}
Json State()
{
	/* Every stored native tile field, including bridge/road/tram/rail metadata.
	 * Never hash object padding or depend on the lighter production terrain projection. */
	uint64_t map = 14695981039346656037ULL;
	for (TileIndex index : Map::Iterate()) {
		Tile tile(index);
		for (uint16_t value : {uint16_t(tile.type()), uint16_t(tile.height()), uint16_t(tile.m1()), tile.m2(),
			uint16_t(tile.m3()), uint16_t(tile.m4()), uint16_t(tile.m5()), uint16_t(tile.m6()), uint16_t(tile.m7()), tile.m8()}) {
			for (uint8_t byte : {uint8_t(value), uint8_t(value >> 8)}) { map ^= byte; map *= 1099511628211ULL; }
		}
	}
	Json materials = Json::array();
	for (const auto &stock : StockpileManager::GetAllStockpiles()) materials.push_back(
		{{"world", stock.world_id.base()}, {"company", stock.company_id.base()}, {"inventory", stock.inventory}});
	return {{"mode", UsesCommonwealthConstructionRules() ? (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V1 ? 1 : 2) : 0},
		{"rating", Town::Get(TownID{0})->ratings[CompanyID{0}]}, {"money", int64_t(Company::Get(CompanyID{0})->money)},
		{"materials", materials}, {"economy", IntegratedEconomy::Save()}, {"map", map},
		{"rng", {_random.state[0], _random.state[1]}}, {"crossing", RoadState(TileXY(25, 30))},
		{"foreign_tram", RoadState(TileXY(27, 30))}, {"demolition", RoadState(TileXY(35, 30))},
		{"competitor_crossing", _settings_game.construction.crossing_with_competitor},
		{"extra_dynamite", _settings_game.construction.extra_dynamite}};
}
void TownRoad(TileIndex tile, RoadBits bits = ROAD_Y, Owner tram = OWNER_NONE)
{
	MakeRoadNormal(tile, bits, ROADTYPE_ROAD, tram == OWNER_NONE ? INVALID_ROADTYPE : ROADTYPE_TRAM,
		TownID{0}, OWNER_TOWN, tram);
}
auto Rail(TileIndex tile, Track track, bool execute = false)
{
	return Command<Commands::BuildRail>::Do(execute ? DoCommandFlags{DoCommandFlag::Execute} : DoCommandFlags{},
		tile, RAILTYPE_RAIL, track, false);
}
auto Clear(TileIndex tile, bool execute = false)
{
	return Command<Commands::LandscapeClear>::Do(execute ? DoCommandFlags{DoCommandFlag::Execute} : DoCommandFlags{}, tile);
}

void PolicyNativeGraphics(const char *binary)
{
	DetermineBasePaths(binary);
	_valid_searchpaths.clear(); for (Searchpath sp : EnumRange(Searchpath::End)) if (!_searchpaths[sp].empty()) _valid_searchpaths.push_back(sp);
	TarScanner::DoScan(TarScanner::Mode::Baseset); BaseGraphics::FindSets(); REQUIRE(BaseGraphics::SetSet(nullptr));
	DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
	ScanNewGRFFiles(nullptr);
}
}

void SetupConstructionPolicyNetworkFixture(int mode)
{
	SetupPolicy(mode);
	TownRoad(TileXY(25, 30)); TownRoad(TileXY(27, 30), ROAD_Y, CompanyID{1});
	for (uint x = 34; x <= 36; ++x) TownRoad(TileXY(x, 30), ROAD_X);
}
std::string ConstructionPolicyState() { return State().dump(); }
CommandPacket MakeConstructionPolicyCommand(char op)
{
	CommandPacket cp; cp.company = CompanyID{0}; cp.err_msg = StringID{0}; cp.callback = nullptr;
	if (op == 'd') {
		cp.cmd = Commands::LandscapeClear;
		cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(TileXY(35, 30)));
	} else {
		cp.cmd = Commands::BuildRail;
		cp.data = EndianBufferWriter<CommandDataBuffer>::FromValue(std::make_tuple(
			op == 'f' ? TileXY(27, 30) : TileXY(25, 30), RAILTYPE_RAIL, Track::X, false));
	}
	return cp;
}

TEST_CASE("Construction policy predicate is pure across Classic v1 v2", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) {
		PolicyFixture fixture(mode); auto before = State();
		for (int i = 0; i < 10; ++i) CHECK(UsesCommonwealthConstructionRules() == (mode != 0));
		CHECK(State() == before);
	}
}

TEST_CASE("Construction policy compatible town road conversion keeps company ownership rules", "[construction-policy]")
{
	for (int mode : {0,1,2}) for (bool foreign : {false,true}) {
		PolicyFixture fixture(mode); const RoadType converted = AllocateRoadType(RoadTypeLabel{"POLY"},RoadTramType::Road);
		REQUIRE(converted != INVALID_ROADTYPE);
		_roadtypes[converted].powered_roadtypes.Set(ROADTYPE_ROAD);
		_roadtypes[ROADTYPE_ROAD].powered_roadtypes.Set(converted);
		Company::Get(CompanyID{0})->avail_roadtypes.Set(converted);
		MakeRoadNormal(TileXY(25,30),ROAD_Y,ROADTYPE_ROAD,INVALID_ROADTYPE,TownID{0},foreign ? CompanyID{1} : OWNER_TOWN,OWNER_NONE);
		const auto before = State(); auto quote = Command<Commands::ConvertRoad>::Do({},TileXY(25,30),TileXY(25,30),converted,false);
		CHECK(quote.Succeeded() == (mode != 0 && !foreign)); CHECK(State() == before);
		if (quote.Succeeded()) {
			const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Command<Commands::ConvertRoad>::Do(DoCommandFlag::Execute,TileXY(25,30),TileXY(25,30),converted,false).Succeeded());
			CHECK(GetRoadTypeRoad(TileXY(25,30)) == converted); CHECK(GetRoadOwner(TileXY(25,30),RoadTramType::Road) == OWNER_TOWN);
			CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
		}
	}
}

TEST_CASE("Construction policy preserves uncolonized world construction refusal", "[construction-policy]")
{
	for (int mode : {1,2}) {
		PolicyFixture fixture(mode); REQUIRE(PlanetManager::SetWorldPhase(WorldID{0},WorldPhase::Phase4_Expansion));
		TownRoad(TileXY(25,30)); const auto before = State();
		Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_MONO);
		const auto denied = Command<Commands::BuildRail>::Do({},TileXY(25,30),RAILTYPE_MONO,Track::X,false);
		CHECK(denied.Failed()); CHECK(denied.GetErrorMessage() == STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD); CHECK(State() == before);
	}
}

TEST_CASE("Construction policy permits integrated public roads while retaining foreign tram and cash refusal", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) {
		PolicyFixture fixture(mode);
		SetupConstructionPolicyNetworkFixture(mode);
		const Json before = State(); auto foreign = Rail(TileXY(27, 30), Track::X);
		CHECK(foreign.Failed()); CHECK(foreign.GetErrorMessage() == STR_ERROR_OWNED_BY);
		if (mode != 0) CHECK(foreign.GetErrorOwner() == CompanyID{1});
		CHECK(State() == before);
		auto crossing = Rail(TileXY(25, 30), Track::X);
		CHECK(crossing.Succeeded() == (mode != 0)); CHECK(State() == before);
		if (mode != 0) {
			Company::Get(CompanyID{0})->money = Money{0}; const auto poor = State();
			CHECK(Rail(TileXY(25, 30), Track::X, true).Failed()); CHECK(State() == poor);
			Company::Get(CompanyID{0})->money = Money{10000000};
			const auto funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Rail(TileXY(25, 30), Track::X, true).Succeeded());
			CHECK(IsLevelCrossingTile(TileXY(25, 30))); CHECK(GetRoadOwner(TileXY(25, 30), RoadTramType::Road) == OWNER_TOWN);
			CHECK(Town::Get(TownID{0})->ratings[CompanyID{0}] == -200);
			CHECK(funds - Company::Get(CompanyID{0})->money == crossing.GetCost());
		}
	}
}

TEST_CASE("Construction policy five native pieces preserve Classic partial refusal and complete v1 v2", "[construction-policy]")
{
	struct Cell { uint8_t x, y, bits; };
	const std::array<Cell, 15> roads{{{23,28,11},{24,28,14},{25,28,11},{26,28,14},{27,28,10},{28,28,12},
		{24,29,5},{28,29,5},{23,30,14},{24,30,11},{25,30,15},{26,30,10},{27,30,10},{25,31,5},{28,31,5}}};
	for (int mode : {0, 1, 2}) {
		PolicyFixture fixture(mode); _settings_game.construction.extra_dynamite = true; _settings_game.construction.crossing_with_competitor = true;
		/* TileXY needs the allocated map's dimensions, including when this case runs alone. */
		const std::array<std::pair<TileIndex, Track>, 5> pieces{{{TileXY(27,30),Track::X},{TileXY(26,30),Track::X},
			{TileXY(25,30),Track::X},{TileXY(24,30),Track::Left},{TileXY(24,29),Track::Y}}};
		Town *town = Town::Get(TownID{0}); town->ratings[CompanyID{0}] = 114;
		for (const auto &cell : roads) TownRoad(TileXY(cell.x,cell.y), RoadBits{cell.bits});
		int64_t spent = 0; const Money funds = Company::Get(CompanyID{0})->money;
		const std::array<int16_t, 5> ratings{{96,78,28,-22,-40}};
		for (size_t i = 0; i < pieces.size(); ++i) {
			const auto before = State(); auto quote = Rail(pieces[i].first, pieces[i].second);
			INFO("mode=" << mode << " piece=" << i << " native error=" << quote.GetErrorMessage().base());
			CHECK(State() == before);
			if (mode == 0 && i == 4) {
				CHECK(quote.Failed()); CHECK(quote.GetErrorMessage() == STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
				CHECK(town->ratings[CompanyID{0}] == -22); break;
			}
			REQUIRE(quote.Succeeded()); REQUIRE(quote.GetCost() > 0);
			auto execute = Rail(pieces[i].first,pieces[i].second,true); REQUIRE(execute.Succeeded());
			CHECK(execute.GetCost() == quote.GetCost()); spent += int64_t(execute.GetCost());
			CHECK(int64_t(funds) - int64_t(Company::Get(CompanyID{0})->money) == spent);
			CHECK(town->ratings[CompanyID{0}] == ratings[i]); CHECK(IsPlainRailTile(pieces[i].first));
		}
	}
}

TEST_CASE("Construction policy explicit house demolition retains human and protected AI semantics", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) {
		PolicyFixture fixture(mode); const TileIndex tile = TileXY(13,13);
		MakeHouseTile(tile, TownID{0}, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true);
		const auto before = State(); auto quote = Clear(tile);
		CHECK(State() == before); CHECK(quote.Succeeded() == (mode != 0));
		Company::Get(CompanyID{0})->is_ai = true; const auto ai = State();
		CHECK(Clear(tile).Failed()); CHECK(State() == ai); Company::Get(CompanyID{0})->is_ai = false;
		if (mode != 0) {
			Town *town = Town::Get(TownID{0}); town->cache.num_houses = 1; IncreaseBuildingCount(town, HouseID{0});
			town->cache.population = HouseSpec::Get(0)->population;
			REQUIRE(Clear(tile,true).Succeeded()); CHECK(IsTileType(tile,TileType::Clear));
			CHECK(town->ratings[CompanyID{0}] <= -200);
		}
	}
}

TEST_CASE("Construction policy connected road demolition preserves rating bookkeeping", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) for (int rating : {-1000, -200, 15, 114}) for (int tolerance : {TOWN_COUNCIL_LENIENT, TOWN_COUNCIL_HOSTILE}) for (bool dynamite : {false, true}) {
		PolicyFixture fixture(mode); Town *town = Town::Get(TownID{0}); town->ratings[CompanyID{0}] = rating;
		_settings_game.difficulty.town_council_tolerance = tolerance; _settings_game.construction.extra_dynamite = dynamite;
		for (uint x = 34; x <= 36; ++x) TownRoad(TileXY(x, 30), ROAD_X);
		const Json before = State(); auto quote = Clear(TileXY(35, 30)); CHECK(State() == before);
		if (mode != 0) {
			REQUIRE(quote.Succeeded()); const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Clear(TileXY(35, 30), true).Succeeded()); CHECK(IsTileType(TileXY(35, 30), TileType::Clear));
			CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
			CHECK(town->ratings[CompanyID{0}] == (rating > RATING_ROAD_MINIMUM ? std::max(rating - 50, RATING_ROAD_MINIMUM) : rating));
		} else if (!dynamite || rating < 16) CHECK(quote.Failed());
	}
}

TEST_CASE("Construction policy station threshold changes only integrated municipality permission", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) for (int rating : {-1000, -200, -199, 15, 114}) {
		PolicyFixture fixture(mode); Town::Get(TownID{0})->ratings[CompanyID{0}] = rating;
		const auto before = State();
		auto quote = Command<Commands::BuildRailStation>::Do({}, TileXY(12, 12), RAILTYPE_RAIL, Axis::X,
			uint8_t{1}, uint8_t{2}, STAT_CLASS_DFLT, uint16_t{0}, NEW_STATION, true);
		CHECK(State() == before); CHECK(quote.Succeeded() == (mode != 0 || rating > -200));
		if (quote.Succeeded()) {
			const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, TileXY(12, 12), RAILTYPE_RAIL,
				Axis::X, uint8_t{1}, uint8_t{2}, STAT_CLASS_DFLT, uint16_t{0}, NEW_STATION, true).Succeeded());
			CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost()); CHECK(IsRailStation(TileXY(12, 12)));
		}
	}
}

TEST_CASE("Construction policy cannot clear occupied foreign illegal or indestructible infrastructure", "[construction-policy]")
{
	for (int mode : {1, 2}) {
		PolicyFixture fixture(mode);
		MakeRailNormal(TileXY(25, 30), CompanyID{1}, TrackBits{Track::X}, RAILTYPE_RAIL);
		const auto before = State(); CHECK(Clear(TileXY(25, 30)).Failed()); CHECK(State() == before);
		CHECK(Rail(INVALID_TILE, Track::X).Failed()); CHECK(State() == before);
		REQUIRE(Train::CanAllocateItem());
		Train *train = Train::Create(); train->owner = CompanyID{0}; train->tile = TileXY(26, 30); train->track = TrackBits{Track::X}; train->UpdatePosition();
		MakeRailNormal(train->tile,CompanyID{0},TrackBits{Track::X},RAILTYPE_RAIL);
		const auto occupied = State(); CHECK(Clear(train->tile).Failed()); CHECK(State() == occupied);
		MakeHouseTile(TileXY(13, 13), TownID{0}, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, false);
		HouseSpec *hs = HouseSpec::Get(0); const auto rating = hs->remove_rating_decrease; hs->remove_rating_decrease = RATING_MAXIMUM + 1;
		const auto house = State(); CHECK(Clear(TileXY(13, 13)).Failed()); CHECK(State() == house);
		hs->remove_rating_decrease = rating;
		CHECK(Command<Commands::BuildRail>::Do(DoCommandFlag::Auto,TileXY(13,13),RAILTYPE_RAIL,Track::X,false).Failed());
	}
}

TEST_CASE("Construction policy slower public bridge gets no improvement reward", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) {
		PolicyFixture fixture(mode); const TileIndex a = TileXY(30, 20), b = TileXY(33, 20);
		auto initial = Command<Commands::BuildBridge>::Do(DoCommandFlag::Execute, b, a, TransportType::Road, BridgeType{1}, RAILTYPE_RAIL, ROADTYPE_ROAD);
		INFO("initial bridge error=" << initial.GetErrorMessage().base()); REQUIRE(initial.Succeeded());
		for (TileIndex tile : {a, b}) { SetTileOwner(tile, OWNER_TOWN); SetRoadOwner(tile, RoadTramType::Road, OWNER_TOWN); }
		Town *town = Town::Get(TownID{0}); town->ratings[CompanyID{0}] = 114;
		REQUIRE(GetBridgeSpec(0)->speed < GetBridgeSpec(1)->speed);
		const auto before = State(); auto quote = Command<Commands::BuildBridge>::Do({}, b, a, TransportType::Road, BridgeType{0}, RAILTYPE_RAIL, ROADTYPE_ROAD);
		CHECK(State() == before); CHECK(quote.Succeeded() == (mode != 0));
		if (mode != 0) {
			REQUIRE(quote.GetCost() > 0); const Money funds = Company::Get(CompanyID{0})->money;
			auto paid = Command<Commands::BuildBridge>::Do(DoCommandFlag::Execute, b, a, TransportType::Road, BridgeType{0}, RAILTYPE_RAIL, ROADTYPE_ROAD);
			REQUIRE(paid.Succeeded()); CHECK(paid.GetCost() == quote.GetCost()); CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
			CHECK(GetBridgeType(a) == 0); CHECK(town->ratings[CompanyID{0}] == 114);
			const auto no_dynamite = State(); const auto removal = Clear(a); REQUIRE(removal.Succeeded()); REQUIRE(removal.GetCost() > 0); CHECK(State() == no_dynamite);
			const Money before_remove = Company::Get(CompanyID{0})->money;
			REQUIRE(Clear(a, true).Succeeded()); CHECK(before_remove - Company::Get(CompanyID{0})->money == removal.GetCost());
			CHECK(IsTileType(a, TileType::Clear)); CHECK(IsTileType(b, TileType::Clear));
		}
	}
}

TEST_CASE("Construction policy public tunnel demolition keeps foreign tram protection", "[construction-policy]")
{
	for (int mode : {0, 1, 2}) for (bool foreign : {false, true}) {
		PolicyFixture fixture(mode); const TileIndex a = TileXY(30,20), b = TileXY(34,20);
		/* Fixed native tunnel endpoints; no generation or simulated economy. */
		MakeRoadTunnel(a, OWNER_TOWN, DiagDirection::SW, ROADTYPE_ROAD, foreign ? ROADTYPE_TRAM : INVALID_ROADTYPE);
		MakeRoadTunnel(b, OWNER_TOWN, DiagDirection::NE, ROADTYPE_ROAD, foreign ? ROADTYPE_TRAM : INVALID_ROADTYPE);
		if (foreign) for (TileIndex tile : {a,b}) SetRoadOwner(tile,RoadTramType::Tram,CompanyID{1});
		REQUIRE(GetOtherTunnelEnd(a) == b); const auto before = State(); auto quote = Clear(a);
		CHECK(quote.Succeeded() == (mode != 0 && !foreign)); CHECK(State() == before);
		if (quote.Succeeded()) {
			REQUIRE(quote.GetCost() > 0); const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Clear(a,true).Succeeded()); CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
			CHECK(IsTileType(a,TileType::Clear)); CHECK(IsTileType(b,TileType::Clear));
		}
	}
}

TEST_CASE("Construction policy preserves Blueprint road footprint exclusion", "[construction-policy]")
{
	for (int mode : {0,1,2}) {
		PolicyFixture fixture(mode); TownRoad(TileXY(25,30));
		Blueprint bp; bp.name = "Policy crossing"; bp.width = bp.height = 1;
		BlueprintTile tile; tile.trackbits = TrackBits{Track::X}; bp.tiles.push_back(tile);
		const auto before = State(); auto quote = Command<Commands::PlaceBlueprint>::Do({},TileXY(25,30),bp.ToJson(),RAILTYPE_RAIL,false);
		CHECK(State() == before); CHECK(quote.Failed()); CHECK(quote.GetErrorMessage() == STR_ERROR_IMPOSSIBLE_TRACK_COMBINATION);
	}
}

TEST_CASE("Construction policy Blueprint station uses shared municipal permission", "[construction-policy]")
{
	for (int mode : {0,1,2}) {
		PolicyFixture fixture(mode); Blueprint bp; bp.name = "Policy station"; bp.width = bp.height = 1;
		BlueprintTile tile; tile.type = BlueprintTileType::Station; bp.tiles.push_back(tile);
		const auto before = State(); auto quote = Command<Commands::PlaceBlueprint>::Do({},TileXY(12,12),bp.ToJson(),RAILTYPE_RAIL,false);
		CHECK(State() == before); CHECK(quote.Succeeded() == (mode != 0));
		if (quote.Succeeded()) {
			const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Command<Commands::PlaceBlueprint>::Do(DoCommandFlag::Execute,TileXY(12,12),bp.ToJson(),RAILTYPE_RAIL,false).Succeeded());
			CHECK(IsRailStation(TileXY(12,12))); CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
		}
	}
}

TEST_CASE("Construction policy preserves material research and company world custody", "[construction-policy]")
{
	for (int mode : {1,2}) {
		PolicyFixture fixture(mode);
		/* Cargo identities are unit-fixture prerequisites, never mission grants. */
		const CargoType steel{2}, ballast{19};
		CargoSpec::Get(steel)->label = CargoLabel{"STEL"}; CargoSpec::Get(steel)->bitnum = steel;
		CargoSpec::Get(ballast)->label = CargoLabel{"BALL"}; CargoSpec::Get(ballast)->bitnum = ballast; BuildCargoLabelMap();
		FabricationManager::SetFabricateFromStockpile(CompanyID{0},true);
		TownRoad(TileXY(25,30)); StockpileManager::AddCargo(WorldID{0},CompanyID{1},steel,10); StockpileManager::AddCargo(WorldID{0},CompanyID{1},ballast,20);
		const auto locked = State(); auto query = Rail(TileXY(25,30),Track::X);
		INFO("locked error=" << query.GetErrorMessage().base());
		CHECK(query.Failed()); CHECK(query.GetErrorMessage() == STR_ERROR_COMMONWEALTH_RESEARCH); CHECK(State() == locked);
		TechTreeManager::RestoreCompanyTech(CompanyID{0},0,0,0,{TECH_MATERIALS_1});
		const auto missing = State(); query = Rail(TileXY(25,30),Track::X);
		CHECK(query.Failed()); CHECK(query.GetErrorMessage() == STR_ERROR_ECONOMY_MATERIALS); CHECK(State() == missing);
		StockpileManager::AddCargo(WorldID{0},CompanyID{0},steel,1); StockpileManager::AddCargo(WorldID{0},CompanyID{0},ballast,2);
		const auto ready = State(); query = Rail(TileXY(25,30),Track::X); REQUIRE(query.Succeeded()); CHECK(State() == ready);
		Company::Get(CompanyID{0})->money = Money{0}; const auto poor = State();
		CHECK(Rail(TileXY(25,30),Track::X,true).Failed()); CHECK(State() == poor);
		Company::Get(CompanyID{0})->money = Money{10000000}; const Money funds = Company::Get(CompanyID{0})->money;
		REQUIRE(Rail(TileXY(25,30),Track::X,true).Succeeded()); CHECK(funds - Company::Get(CompanyID{0})->money == query.GetCost());
		CHECK(StockpileManager::GetStock(WorldID{0},CompanyID{0},steel) == 0); CHECK(StockpileManager::GetStock(WorldID{0},CompanyID{0},ballast) == 0);
		CHECK(StockpileManager::GetStock(WorldID{0},CompanyID{1},steel) == 10); CHECK(StockpileManager::GetStock(WorldID{0},CompanyID{1},ballast) == 20);
		SetupCargoForClimate(LandscapeType::Temperate);
	}
}

TEST_CASE("Construction policy poor rating station extension and dock retain normal constraints", "[construction-policy]")
{
	for (int mode : {0,1,2}) {
		PolicyFixture fixture(mode); Town *town = Town::Get(TownID{0}); town->ratings[CompanyID{0}] = 500;
		REQUIRE(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute,TileXY(12,12),RAILTYPE_RAIL,Axis::X,
			uint8_t{1},uint8_t{1},STAT_CLASS_DFLT,uint16_t{0},NEW_STATION,true).Succeeded());
		const StationID station = GetStationIndex(TileXY(12,12)); town->ratings[CompanyID{0}] = -200;
		const auto before = State(); auto quote = Command<Commands::BuildRailStation>::Do({},TileXY(13,12),RAILTYPE_RAIL,Axis::X,
			uint8_t{1},uint8_t{1},STAT_CLASS_DFLT,uint16_t{0},station,true);
		INFO("extension mode=" << mode << " error=" << quote.GetErrorMessage().base());
		CHECK(State() == before); CHECK(quote.Succeeded() == (mode != 0));
		if (quote.Succeeded()) {
			const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute,TileXY(13,12),RAILTYPE_RAIL,Axis::X,
				uint8_t{1},uint8_t{1},STAT_CLASS_DFLT,uint16_t{0},station,true).Succeeded());
			CHECK(GetStationIndex(TileXY(13,12)) == station); CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
			Station::Get(station)->owner = CompanyID{1};
			CHECK(Command<Commands::BuildRailStation>::Do({},TileXY(14,12),RAILTYPE_RAIL,Axis::X,uint8_t{1},uint8_t{1},STAT_CLASS_DFLT,uint16_t{0},station,true).Failed());
			Station::Get(station)->owner = CompanyID{0};
			_settings_game.station.station_spread = 1;
			CHECK(Command<Commands::BuildRailStation>::Do({},TileXY(14,12),RAILTYPE_RAIL,Axis::X,uint8_t{1},uint8_t{1},STAT_CLASS_DFLT,uint16_t{0},station,true).Failed());
		}
	}
	for (int mode : {0,1,2}) {
		PolicyFixture fixture(mode);
		for (uint y = 13; y <= 15; ++y) for (uint x = 12; x <= 13; ++x) SetTileHeight(TileXY(x,y),0);
		MakeSea(TileXY(12,13)); MakeSea(TileXY(12,14)); REQUIRE(GetTileSlope(TileXY(12,12)) == SLOPE_NW);
		const auto before = State(); auto quote = Command<Commands::BuildDock>::Do({},TileXY(12,12),NEW_STATION,true);
		INFO("dock mode=" << mode << " error=" << quote.GetErrorMessage().base());
		CHECK(State() == before); CHECK(quote.Succeeded() == (mode != 0));
		if (quote.Succeeded()) {
			const Money funds = Company::Get(CompanyID{0})->money;
			REQUIRE(Command<Commands::BuildDock>::Do(DoCommandFlag::Execute,TileXY(12,12),NEW_STATION,true).Succeeded());
			CHECK(IsDockTile(TileXY(12,12))); CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
		}
	}
}

TEST_CASE("Construction policy road stops and airports retain physical noise and count limits", "[construction-policy]")
{
	for (int mode : {0,1,2}) {
		PolicyFixture fixture(mode); RoadStopClass::Reset(); AirportSpec::ResetAirports();
		auto before = State();
		auto stop = Command<Commands::BuildRoadStop>::Do({},TileXY(12,12),uint8_t{1},uint8_t{1},RoadStopType::Bus,false,DiagDirection::NE,
			ROADTYPE_ROAD,ROADSTOP_CLASS_DFLT,uint16_t{0},NEW_STATION,true);
		CHECK(stop.Succeeded() == (mode != 0)); CHECK(State() == before);
		_settings_game.economy.station_noise_level = true; Town::Get(TownID{0})->noise_reached = 65535;
		auto airport = Command<Commands::BuildAirport>::Do({},TileXY(12,12),uint8_t{AT_SMALL},uint8_t{0},NEW_STATION,true);
		CHECK(airport.Failed()); if (mode != 0) CHECK(airport.GetErrorMessage() == STR_ERROR_LOCAL_AUTHORITY_REFUSES_NOISE);
		_settings_game.economy.station_noise_level = false;
		for (int i = 0; i < 2; ++i) { REQUIRE(Station::CanAllocateItem()); auto station = Station::Create(TileXY(40+i*8,40)); station->town = Town::Get(TownID{0}); station->facilities.Set(StationFacility::Airport); station->airport.type = AT_SMALL; }
		airport = Command<Commands::BuildAirport>::Do({},TileXY(12,12),uint8_t{AT_SMALL},uint8_t{0},NEW_STATION,true);
		CHECK(airport.Failed()); if (mode != 0) CHECK(airport.GetErrorMessage() == STR_ERROR_LOCAL_AUTHORITY_REFUSES_AIRPORT);
	}
}

/* Separate writer and reader test processes, no game loop and no authored mission. */
TEST_CASE("Construction policy cold fixture writer", "[.][construction-policy-persistence]")
{
	const char *save = std::getenv("OSTTD_POLICY_SAVE"); const char *state = std::getenv("OSTTD_POLICY_STATE"); const char *mode = std::getenv("OSTTD_POLICY_MODE");
	REQUIRE(save != nullptr); REQUIRE(state != nullptr); REQUIRE(mode != nullptr);
	REQUIRE((std::string_view(mode) == "0" || std::string_view(mode) == "1" || std::string_view(mode) == "2"));
	const int rules = mode[0] - '0'; SetupConstructionPolicyNetworkFixture(rules);
	if (rules != 0) {
		const char *binary = std::getenv("OSTTD_TEST_BINARY"); REQUIRE(binary != nullptr);
		UnInitWindowSystem(); PolicyNativeGraphics(binary);
		_grfconfig.clear();
		for (const char *file : {"openspacettd_integrated_v1.grf", "openspacettd_rail_v3.grf", "openspacettd_equipment_v1.grf", "openspacettd_gates_v1.grf"}) {
			auto config = std::make_unique<GRFConfig>((std::filesystem::path(binary).parent_path() / "newgrf" / file).string());
			REQUIRE(FillGRFDetails(*config,false)); _grfconfig.push_back(std::move(config));
		}
		GfxLoadSprites();
		Json content = Json::array();
		for (const auto &config : _grfconfig) content.push_back({{"file",config->filename},{"version",config->version},{"status",to_underlying(config->status)}});
		INFO(content.dump());
		INFO(CommonwealthPackManager::GetContentStatus().reason);
		REQUIRE(IntegratedEconomy::ContentReady());
	}
	TownRoad(TileXY(29,30));
	/* Built-in cargo remains native across GRF startup and cold remapping. */
	StockpileManager::AddCargo(WorldID{0},CompanyID{0},CargoType{1},99);
	StockpileManager::AddCargo(WorldID{0},CompanyID{1},CargoType{1},17);
	const auto before = State();
	CHECK(Rail(TileXY(25,30),Track::X,true).Succeeded() == (rules != 0));
	CHECK(Clear(TileXY(35,30),true).Succeeded() == (rules != 0));
	if (rules == 0) CHECK(State() == before);
	else { CHECK(IsLevelCrossingTile(TileXY(25,30))); CHECK(Company::Get(CompanyID{0})->money < Money{10000000}); }
	UnInitWindowSystem();
	REQUIRE(SaveOrLoad(save, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	std::ofstream out(state); REQUIRE(out.good()); out << State().dump();
}
TEST_CASE("Construction policy cold fixture reader", "[.][construction-policy-persistence]")
{
	const char *save = std::getenv("OSTTD_POLICY_SAVE"); const char *state = std::getenv("OSTTD_POLICY_STATE"); const char *binary = std::getenv("OSTTD_TEST_BINARY");
	REQUIRE(save != nullptr); REQUIRE(state != nullptr); REQUIRE(binary != nullptr);
	SetupPolicy(0); UnInitWindowSystem();
	PolicyNativeGraphics(binary);
	REQUIRE(SaveOrLoad(save, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	_current_company = _local_company = CompanyID{0};
	std::ifstream in(state); REQUIRE(in.good()); Json expected; in >> expected;
	CHECK(State() == expected);
	const auto before = State(); auto quote = Rail(TileXY(29, 30), Track::X);
	CHECK(quote.Succeeded() == (expected.at("mode") != 0)); CHECK(State() == before);
	if (quote.Succeeded()) {
		const Money funds = Company::Get(CompanyID{0})->money;
		REQUIRE(Rail(TileXY(29,30),Track::X,true).Succeeded()); CHECK(IsLevelCrossingTile(TileXY(29,30)));
		CHECK(funds - Company::Get(CompanyID{0})->money == quote.GetCost());
		CHECK(State().at("materials") == expected.at("materials"));
	}
}
