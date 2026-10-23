/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_alien_worlds.cpp Alien Frontier terrain toolkit, script world facts and volcanic ground colour. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cargotype.h"
#include "../clear_func.h"
#include "../company_base.h"
#include "../map_func.h"
#include "../portal/alien_worlds.h"
#include "../portal/company_stockpile.h"
#include "../portal/planet_manager.h"
#include "../script/api/script_planet.hpp"
#include "../table/sprites.h"
#include "../safeguards.h"

using namespace AlienTerrain;

namespace {
/** @brief Read one quantized corner. */
int Level(const std::vector<uint8_t> &levels, uint32_t width, uint32_t x, uint32_t y) { return levels[size_t(y) * width + x]; }

/** @brief Register a sub-arctic frontier and a volcanic industrial world either side of a void band. */
void TwoWorlds()
{
	Map::Allocate(64, 64);
	PlanetManager::Reset();
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "Hollowfrost", .phase = WorldPhase::Phase3_Frontier, .biome = WorldBiome::SubArctic,
		.min_x = 1, .min_y = 1, .max_x = 29, .max_y = 62}));
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{1}, .name = "Kestrel Forge", .phase = WorldPhase::Phase2_Developed, .biome = WorldBiome::Volcanic,
		.min_x = 34, .min_y = 1, .max_x = 62, .max_y = 62}));
}
} // namespace

TEST_CASE("Alien terrain shapes always quantize to valid slopes", "[alien-worlds]")
{
	constexpr uint32_t W = 128, H = 96;
	Field field(W, H);
	for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < W; ++x) field.At(x, y) = 4.0f;
	field.AddRidge({5, 90, 60, 20, 9});
	field.AddCrater({40, 40, 20, 10, 4, 8});
	/* A younger impact cuts the older rim; the repair pass settles the seam. */
	field.AddCrater({56, 50, 9, 8, 3, 0});
	field.AddMesa({100, 60, 10, 9});
	field.Level({60, 72, 90, 76, 4});
	field.TaperEdges();
	const std::vector<uint8_t> levels = field.Quantize(14);

	CHECK(CountInvalidTiles(levels, W, H) == 0);
	for (uint32_t y = 72; y <= 76; ++y) for (uint32_t x = 60; x <= 90; ++x) CHECK(Level(levels, W, x, y) == 4);
	CHECK(Level(levels, W, 40, 40) == 0); // Central lake basin at sea level.
	CHECK(Level(levels, W, 40, 20) == 10); // Rim crest.
	CHECK(Level(levels, W, 56, 50) == 3); // Younger crater floor.
	CHECK(Level(levels, W, 100, 60) == 9); // Mesa top.
	for (uint32_t y = 0; y < H; ++y) CHECK((Level(levels, W, 0, y) == 0 && Level(levels, W, W - 1, y) == 0));
}

TEST_CASE("Quantize lowers cliffs that would form invalid slopes", "[alien-worlds]")
{
	constexpr uint32_t W = 16, H = 16;
	std::vector<uint8_t> raw(W * H, 1);
	raw[8 * W + 8] = 4;
	CHECK(CountInvalidTiles(raw, W, H) == 4);

	Field field(W, H);
	for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < W; ++x) field.At(x, y) = 1.0f;
	field.At(8, 8) = 4.0f;
	field.At(3, 3) = 30.0f;
	const std::vector<uint8_t> levels = field.Quantize(14);
	CHECK(CountInvalidTiles(levels, W, H) == 0);
	CHECK(Level(levels, W, 8, 8) == 2);
	CHECK(Level(levels, W, 3, 3) == 2); // Clamped to the height limit, then lowered.
	CHECK(Level(levels, W, 0, 0) == 1);
}

TEST_CASE("Script planet facts describe worlds and company stock without changing them", "[alien-worlds][script]")
{
	TwoWorlds();
	SetupCargoForClimate(LandscapeType::Temperate);
	_company_pool.CleanPool();
	REQUIRE(Company::CreateAtIndex(CompanyID{0}) != nullptr);
	StockpileManager::Reset();

	CHECK(ScriptPlanet::GetWorldCount() == 2);
	CHECK(ScriptPlanet::GetWorldID(1) == 1);
	CHECK(ScriptPlanet::GetWorldID(2) == -1);
	CHECK(ScriptPlanet::GetTileWorld(TileXY(10, 10)) == 0);
	CHECK(ScriptPlanet::GetTileWorld(TileXY(31, 10)) == -1);
	CHECK(ScriptPlanet::GetName(1) == std::optional<std::string>{"Kestrel Forge"});
	CHECK_FALSE(ScriptPlanet::GetName(7).has_value());
	CHECK(ScriptPlanet::GetBiome(0) == 1);
	CHECK(ScriptPlanet::GetBiome(1) == 4);
	CHECK(ScriptPlanet::GetPhase(0) == 3);
	CHECK(ScriptPlanet::GetPhase(-1) == -1);

	CargoType cargo = INVALID_CARGO;
	for (const CargoSpec *spec : CargoSpec::Iterate()) { cargo = spec->Index(); break; }
	REQUIRE(IsValidCargoType(cargo));
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, cargo, 25);
	CHECK(ScriptPlanet::GetStock(1, ScriptCompany::COMPANY_FIRST, cargo) == 25);
	CHECK(ScriptPlanet::GetStock(0, ScriptCompany::COMPANY_FIRST, cargo) == 0);
	CHECK(ScriptPlanet::GetStock(5, ScriptCompany::COMPANY_FIRST, cargo) == -1);
	CHECK(StockpileManager::GetStock(WorldID{1}, CompanyID{0}, cargo) == 25);

	StockpileManager::Reset();
	_company_pool.CleanPool();
	PlanetManager::Reset();
}

TEST_CASE("Volcanic worlds draw their bare ground as charcoal basalt", "[alien-worlds]")
{
	TwoWorlds();
	CHECK(GetBiomeGroundPalette(TileXY(40, 10)) == PALETTE_CRASH);
	CHECK(GetBiomeGroundPalette(TileXY(10, 10)) == PAL_NONE);
	CHECK(GetBiomeGroundPalette(TileXY(31, 10)) == PAL_NONE);
	PlanetManager::Reset();
}
