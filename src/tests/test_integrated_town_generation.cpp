/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */
/** @file test_integrated_town_generation.cpp Native bounded Core-town generation regressions. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../clear_map.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../core/random_func.hpp"
#include "../genworld.h"
#include "../house.h"
#include "../openttd.h"
#include "../portal/corporate_hq.h"
#include "../portal/integrated_economy.h"
#include "../portal/megacity_manager.h"
#include "../portal/planet_manager.h"
#include "../portal/stellar_network.h"
#include "../settings_type.h"
#include "../town_cmd.h"
#include "../town_kdtree.h"
#include "../town_map.h"
#include "../void_map.h"
#include "../water_map.h"

#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

/** Reset real native pools/content, with stable ordinary flat terrain rather than authored towns. */
struct IntegratedTownFixture {
	GameSettings game_settings = _settings_game;
	GameSettings newgame_settings = _settings_newgame;
	GameMode game_mode = _game_mode;
	bool generating_world = _generating_world;
	CompanyID current_company = _current_company;

	~IntegratedTownFixture()
	{
		IntegratedEconomy::Reset();
		ResetHouses();
		_generating_world = this->generating_world;
		_game_mode = this->game_mode;
		_current_company = this->current_company;
		_settings_game = this->game_settings;
		_settings_newgame = this->newgame_settings;
	}

	void Reset(uint32_t seed, uint size = 128)
	{
		SetupCommandAuthorityWorld(WorldPhase::Phase4_Expansion, 0);
		PlanetManager::Reset();
		CorporateHQManager::Reset();
		StellarNetwork::Reset();
		IntegratedEconomy::Reset();
		Map::Allocate(size, size);
		for (TileIndex tile : Map::Iterate()) {
			SetTileHeight(tile, 1);
			if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
			else MakeVoid(tile);
		}
		_settings_game.difficulty.number_towns = static_cast<uint>(CUSTOM_TOWN_NUMBER_DIFFICULTY);
		_settings_newgame.game_creation.custom_town_number = 1;
		_settings_game.game_creation.town_name = 0;
		_settings_game.economy.larger_towns = 3;
		_settings_game.economy.initial_city_size = 2;
		_settings_game.economy.town_min_distance = 16;
		_settings_game.construction.build_on_slopes = true;
		_current_company = OWNER_TOWN;
		_generating_world = true;
		IntegratedEconomy::SetEnabled(true);
		_random.SetSeed(seed);
		Map::CountLandTiles();
		RebuildTownKdtree();
	}

	void Region(WorldID id, uint min_x, uint min_y, uint max_x, uint max_y, EconomicRole role, WorldPhase phase = WorldPhase::Phase1_Core)
	{
		REQUIRE(PlanetManager::RegisterRegion({.id = id, .name = fmt::format("Native generation {}", id.base()), .phase = phase,
			.biome = WorldBiome::Temperate, .min_x = min_x, .min_y = min_y, .max_x = max_x, .max_y = max_y}));
		IntegratedEconomy::RegisterRole(id, role);
	}

	void Split(uint size = 128)
	{
		/* Register out of ID order. Economic Core is deliberately not WorldID 0. */
		this->Region(WorldID{7}, 1, 1, size - 2, size / 2 - 1, EconomicRole::Core);
		this->Region(WorldID{2}, 1, size / 2, size - 2, size - 2, EconomicRole::Industrial, WorldPhase::Phase2_Developed);
	}
};

/** Projection includes native town/house/road state and consumed synchronized RNG. */
static std::string IntegratedTownProjection()
{
	std::string result = fmt::format("rng:{},{};", _random.state[0], _random.state[1]);
	for (const Town *town : Town::Iterate()) {
		result += fmt::format("town:{},{},{},{},{},{},{};", town->index.base(), town->xy.base(), town->cache.population,
			town->cache.num_houses, town->townnameparts, town->larger_town, town->GetCachedName());
	}
	for (TileIndex tile : Map::Iterate()) {
		if (IsTileType(tile, TileType::House) || IsTileType(tile, TileType::Road)) {
			result += fmt::format("tile:{},{},{};", tile.base(), to_underlying(GetTileType(tile)), GetTownIndex(tile).base());
		}
	}
	const auto &stats = GetIntegratedCoreTownGenerationStats();
	result += fmt::format("search:{},{},{},{},{},{},{},{},{};", stats.target, stats.city_offset, stats.probes, stats.coastal_probes,
		stats.creation_attempts, stats.deleted_candidates, stats.zero_population_candidates, stats.no_core_house_candidates, stats.probe_hash);
	return result;
}

static void CheckNoTownInfrastructure()
{
	CHECK(Town::GetNumItems() == 0);
	CHECK_FALSE(HasValidIntegratedCoreTown());
	for (TileIndex tile : Map::Iterate()) {
		REQUIRE_FALSE(IsTileType(tile, TileType::House));
		REQUIRE_FALSE(IsTileType(tile, TileType::Road));
	}
}

TEST_CASE("Integrated town generation reserves one native Core slot reproducibly", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	const TownLayout layout = GENERATE(TownLayout::Original, TownLayout::BetterRoads, TownLayout::Grid2x2, TownLayout::Grid3x3, TownLayout::Random);
	std::string first;
	for (uint repeat = 0; repeat < 2; ++repeat) {
		fixture.Reset(11);
		fixture.Split();
		REQUIRE(GenerateTowns(layout));
		REQUIRE(Town::GetNumItems() == 1);
		REQUIRE(HasValidIntegratedCoreTown());
		const auto &stats = GetIntegratedCoreTownGenerationStats();
		CHECK(stats.active);
		CHECK(stats.target == 1);
		CHECK(stats.failure == IntegratedCoreTownFailure::None);
		CHECK(stats.probes <= 10000);
		CHECK(stats.creation_attempts <= 20);
		const Town *town = Town::Get(stats.core_town);
		CHECK(PlanetManager::GetTileWorld(town->xy) == WorldID{7});
		CHECK(town->cache.population > 0);
		CHECK(town->larger_town == (stats.city_offset == 0));
		CHECK_FALSE(town->GetCachedName().empty());
		CHECK(MegacityManager::GetAllMegacities().empty());
		if (layout == TownLayout::Grid2x2) {
			CHECK(TileX(town->xy) % 3 == 0);
			CHECK(TileY(town->xy) % 3 == 0);
		} else if (layout == TownLayout::Grid3x3) {
			CHECK(TileX(town->xy) % 4 == 0);
			CHECK(TileY(town->xy) % 4 == 0);
		}
		if (repeat == 0) first = IntegratedTownProjection();
		else CHECK(IntegratedTownProjection() == first);
	}
}

TEST_CASE("Integrated town generation retains remaining slots names spacing and city offset", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(101, 256);
	fixture.Split(256);
	_settings_newgame.game_creation.custom_town_number = 6;
	REQUIRE(GenerateTowns(TownLayout::Grid3x3));
	REQUIRE(HasValidIntegratedCoreTown());
	REQUIRE(Town::GetNumItems() == 6);
	const auto &stats = GetIntegratedCoreTownGenerationStats();
	CHECK(stats.target == 6);
	std::set<std::string> names;
	uint number = 0;
	for (const Town *town : Town::Iterate()) {
		CHECK(names.insert(town->GetCachedName()).second);
		CHECK(town->larger_town == ((stats.city_offset + number) % _settings_game.economy.larger_towns == 0));
		for (const Town *other : Town::Iterate()) {
			if (other->index == town->index) continue;
			CHECK(DistanceManhattan(town->xy, other->xy) >= _settings_game.economy.town_min_distance);
		}
		++number;
	}
}

TEST_CASE("Integrated town generation preserves native density target and pool clamp", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(2026);
	fixture.Region(WorldID{9}, 1, 1, 126, 126, EconomicRole::Industrial);
	uint expected = 0;
	SECTION("Density and land scaling") {
		_settings_game.difficulty.number_towns = 2;
		Randomizer reference = _random;
		expected = Clamp<uint>(Map::ScaleByLandProportion(GetDefaultTownsForMapSize() + (reference.Next() & 7)), 1, TownPool::MAX_SIZE);
	}
	SECTION("Custom zero remains clamped to one") {
		_settings_newgame.game_creation.custom_town_number = 0;
		expected = 1;
	}
	SECTION("Explicit oversized native target is pool clamped") {
		expected = TownPool::MAX_SIZE;
		CHECK_FALSE(GenerateTowns(TownLayout::Original, TownPool::MAX_SIZE + 1));
		CHECK(GetIntegratedCoreTownGenerationStats().target == expected);
		CHECK(GetIntegratedCoreTownGenerationStats().failure == IntegratedCoreTownFailure::NoCoreRegion);
		CheckNoTownInfrastructure();
		return;
	}
	CHECK_FALSE(GenerateTowns(TownLayout::Original));
	CHECK(GetIntegratedCoreTownGenerationStats().target == expected);
	CHECK(GetIntegratedCoreTownGenerationStats().failure == IntegratedCoreTownFailure::NoCoreRegion);
	CHECK(GetIntegratedCoreTownGenerationStats().probes == 0);
	CheckNoTownInfrastructure();
}

TEST_CASE("Integrated town generation orders Core regions by ID and keeps native phase restrictions", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(11);
	fixture.Region(WorldID{9}, 1, 64, 126, 126, EconomicRole::Core);
	fixture.Region(WorldID{3}, 1, 1, 126, 63, EconomicRole::Core, WorldPhase::Phase4_Expansion);
	REQUIRE(GenerateTowns(TownLayout::Original));
	CHECK(PlanetManager::GetTileWorld(Town::Get(GetIntegratedCoreTownGenerationStats().core_town)->xy) == WorldID{9});
	CHECK(GetIntegratedCoreTownGenerationStats().probes > 126 * 63);
	CHECK(HasValidIntegratedCoreTown());
}

TEST_CASE("Integrated town generation charges all rejected aligned centers without non-Core fallback", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(101);
	fixture.Region(WorldID{5}, 1, 1, 126, 100, EconomicRole::Core);
	fixture.Region(WorldID{2}, 1, 101, 126, 126, EconomicRole::Industrial);
	for (uint y = 1; y <= 100; ++y) {
		for (uint x = 1; x <= 126; ++x) MakeClear(TileXY(x, y), ClearGround::Rough, 3);
	}
	CHECK_FALSE(GenerateTowns(TownLayout::Original));
	const auto &stats = GetIntegratedCoreTownGenerationStats();
	CHECK(stats.failure == IntegratedCoreTownFailure::ProbeBudget);
	CHECK(stats.probes == 10000);
	CHECK(stats.creation_attempts == 0);
	/* Freeze seed 101's approved row/stride traversal and native city/name/
	 * traversal RNG consumption, including all rejected rough centers. */
	CHECK(stats.city_offset == 0);
	CHECK(stats.probe_hash == 15554271762323410103ULL);
	CHECK(_random.state[0] == 1846885903U);
	CHECK(_random.state[1] == 3678815853U);
	CheckNoTownInfrastructure();
}

TEST_CASE("Integrated town generation uses shared coastal probes and rejects non-Core landings", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(2026, 64);
	fixture.Region(WorldID{7}, 20, 20, 23, 23, EconomicRole::Core);
	fixture.Region(WorldID{2}, 1, 24, 62, 62, EconomicRole::Industrial);
	for (uint y = 1; y <= 23; ++y) {
		for (uint x = 1; x <= 62; ++x) MakeSea(TileXY(x, y));
	}
	CHECK_FALSE(GenerateTowns(TownLayout::Grid3x3));
	const auto &stats = GetIntegratedCoreTownGenerationStats();
	CHECK(stats.failure == IntegratedCoreTownFailure::SitesExhausted);
	CHECK(stats.coastal_probes > 0);
	CHECK(stats.probes == stats.coastal_probes + 1);
	CHECK(stats.probes <= 10000);
	CHECK(stats.creation_attempts == 0);
	CheckNoTownInfrastructure();
}

TEST_CASE("Integrated town generation deletes all zero-population attempts and retains consumed RNG", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	std::string first;
	for (uint repeat = 0; repeat < 2; ++repeat) {
		fixture.Reset(101);
		fixture.Region(WorldID{7}, 1, 1, 126, 126, EconomicRole::Core);
		for (HouseSpec &house : HouseSpec::Specs()) house.enabled = false;
		const Randomizer before = _random;
		CHECK_FALSE(GenerateTowns(TownLayout::Original));
		const auto &stats = GetIntegratedCoreTownGenerationStats();
		CHECK(stats.failure == IntegratedCoreTownFailure::CreationBudget);
		CHECK(stats.creation_attempts == 20);
		CHECK(stats.deleted_candidates == 20);
		CHECK(stats.zero_population_candidates == 20);
		CHECK(stats.no_core_house_candidates == 0);
		CHECK_FALSE((_random.state[0] == before.state[0] && _random.state[1] == before.state[1]));
		CheckNoTownInfrastructure();
		if (repeat == 0) first = IntegratedTownProjection();
		else CHECK(IntegratedTownProjection() == first);
	}
}

TEST_CASE("Integrated town generation deletes populated candidates without an own house in the same Core world", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(11);
	/* Grid roads pass through the center column; every permissible house lies
	 * outside this one-column Core region, in ordinary legal Frontier terrain. */
	fixture.Region(WorldID{2}, 1, 1, 47, 126, EconomicRole::Frontier, WorldPhase::Phase3_Frontier);
	fixture.Region(WorldID{7}, 48, 1, 48, 126, EconomicRole::Core);
	fixture.Region(WorldID{9}, 49, 1, 126, 126, EconomicRole::Frontier, WorldPhase::Phase3_Frontier);
	CHECK_FALSE(GenerateTowns(TownLayout::Grid2x2));
	const auto &stats = GetIntegratedCoreTownGenerationStats();
	CHECK(stats.failure == IntegratedCoreTownFailure::CreationBudget);
	CHECK(stats.creation_attempts == 20);
	CHECK(stats.deleted_candidates == 20);
	CHECK(stats.no_core_house_candidates > 0);
	CHECK(stats.no_core_house_candidates + stats.zero_population_candidates == 20);
	CheckNoTownInfrastructure();
}

TEST_CASE("Integrated town generation predicate requires positive population and own same-world house", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(11);
	fixture.Split();
	REQUIRE(GenerateTowns(TownLayout::Original));
	Town *town = Town::Get(GetIntegratedCoreTownGenerationStats().core_town);
	REQUIRE(HasValidIntegratedCoreTown());
	SECTION("Zero population") { town->cache.population = 0; }
	SECTION("Economic role rather than development phase") {
		/* Roles are immutable after registration. Reinitialize this negative
		 * fixture instead of expecting a second registration to overwrite one. */
		IntegratedEconomy::Reset();
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::RegisterRole(WorldID{7}, EconomicRole::Industrial);
		REQUIRE(IntegratedEconomy::Role(WorldID{7}) == EconomicRole::Industrial);
		REQUIRE(PlanetManager::GetTilePhase(town->xy) == WorldPhase::Phase1_Core);
	}
	SECTION("Only foreign-town Core houses") {
		REQUIRE(Town::CanAllocateItem());
		Town *other = Town::Create(TileXY(30, 90));
		for (TileIndex tile : Map::Iterate()) {
			if (IsTileType(tile, TileType::House) && GetTownIndex(tile) == town->index) SetTownIndex(tile, other->index);
		}
	}
	SECTION("Own houses only outside center Core world") {
		IntegratedEconomy::Reset();
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::RegisterRole(WorldID{7}, EconomicRole::Core);
		IntegratedEconomy::RegisterRole(WorldID{2}, EconomicRole::Core);
		REQUIRE(IntegratedEconomy::Role(WorldID{2}) == EconomicRole::Core);
		for (TileIndex tile : Map::Iterate()) {
			if (IsTileType(tile, TileType::House) && GetTownIndex(tile) == town->index) MakeClear(tile, ClearGround::Grass, 3);
		}
		MakeHouseTile(TileXY(30, 90), town->index, 0, TOWN_HOUSE_COMPLETED, 0, 0, false);
	}
	CHECK_FALSE(HasValidIntegratedCoreTown());
}

TEST_CASE("Integrated Core town requirement leaves Classic and editor generation ordinary", "[integrated-town-generation]")
{
	IntegratedTownFixture fixture;
	fixture.Reset(2026);
	fixture.Region(WorldID{9}, 1, 1, 126, 126, EconomicRole::Industrial, WorldPhase::Phase2_Developed);
	SECTION("Classic ordinary new game") { IntegratedEconomy::SetEnabled(false); }
	SECTION("Scenario editor with integrated state") { _game_mode = GameMode::Editor; }
	REQUIRE(GenerateTowns(TownLayout::Original));
	CHECK(Town::GetNumItems() == 1);
	CHECK_FALSE(GetIntegratedCoreTownGenerationStats().active);
	CHECK(GetIntegratedCoreTownGenerationStats().creation_attempts == 0);
	CHECK_FALSE(HasValidIntegratedCoreTown());
}
