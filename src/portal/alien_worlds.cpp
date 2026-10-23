/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file alien_worlds.cpp Alien Frontier terrain toolkit, the authored Hollowfrost survey map, and its bounded offline tools. */
#include "../stdafx.h"
#include "alien_worlds.h"
#include "commonwealth_pack.h"
#include "company_stockpile.h"
#include "fabrication_manager.h"
#include "first_line_funding.h"
#include "first_permanent_line.h"
#include "integrated_economy.h"
#include "logistics_hub.h"
#include "megacity_manager.h"
#include "planet_manager.h"
#include "portal_cmd.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "resource_sites.h"
#include "stellar_network.h"
#include "tech_tree.h"
#include "world0_opening.h"
#include "world_gen.h"
#include "../clear_map.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_cmd.h"
#include "../company_func.h"
#include "../console_func.h"
#include "../engine_base.h"
#include "../genworld.h"
#include "../gfx_func.h"
#include "../goal_base.h"
#include "../industry.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../newgrf_industries.h"
#include "../object_base.h"
#include "../order_base.h"
#include "../order_cmd.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../screenshot.h"
#include "../signal_func.h"
#include "../signs_base.h"
#include "../signs_cmd.h"
#include "../station_base.h"
#include "../station_cmd.h"
#include "../story_base.h"
#include "../strings_func.h"
#include "../tile_map.h"
#include "../town.h"
#include "../town_cmd.h"
#include "../train.h"
#include "../train_cmd.h"
#include "../tree_map.h"
#include "../vehicle_cmd.h"
#include "../viewport_func.h"
#include "../void_map.h"
#include "../water.h"
#include "../water_map.h"
#include "../window_func.h"
#include "../progress.h"
#include "../video/video_driver.hpp"
#include "../core/backup_type.hpp"
#include "../network/network.h"
#include "../network/network_base.h"
#include "../timer/timer_game_calendar.h"
#include "../3rdparty/nlohmann/json.hpp"
#include <cmath>
#include "../safeguards.h"

extern Company *DoStartupNewCompany(bool, CompanyID);

namespace AlienTerrain {

Field::Field(uint32_t width, uint32_t height) : width(width), height(height), heights(size_t(width) * height, 0.0f) {}

void Field::AddCrater(const Crater &crater)
{
	const int reach = crater.radius + crater.rim + 1;
	for (int y = std::max(0, crater.y - reach); y <= std::min(int(this->height) - 1, crater.y + reach); ++y) {
		for (int x = std::max(0, crater.x - reach); x <= std::min(int(this->width) - 1, crater.x + reach); ++x) {
			const float d = std::hypot(float(x - crater.x), float(y - crater.y));
			float &h = this->At(x, y);
			if (d < crater.radius) {
				/* The impact replaces older terrain inside its rim. */
				float inner = std::max(float(crater.floor), crater.rim - (crater.radius - d));
				if (crater.lake > 0) inner = std::min(inner, std::max(0.0f, d - crater.lake));
				h = inner;
			} else {
				h = std::max(h, crater.rim - (d - crater.radius));
			}
		}
	}
}

void Field::AddMesa(const Mesa &mesa)
{
	const int reach = mesa.radius + mesa.top + 1;
	for (int y = std::max(0, mesa.y - reach); y <= std::min(int(this->height) - 1, mesa.y + reach); ++y) {
		for (int x = std::max(0, mesa.x - reach); x <= std::min(int(this->width) - 1, mesa.x + reach); ++x) {
			const float d = std::hypot(float(x - mesa.x), float(y - mesa.y));
			float &h = this->At(x, y);
			h = std::max(h, mesa.top - std::max(0.0f, d - mesa.radius));
		}
	}
}

void Field::AddRidge(const Ridge &ridge)
{
	const float dx = float(ridge.x2 - ridge.x1), dy = float(ridge.y2 - ridge.y1);
	const float length = std::max(1.0f, dx * dx + dy * dy);
	for (uint32_t y = 0; y < this->height; ++y) {
		for (uint32_t x = 0; x < this->width; ++x) {
			const float t = std::clamp(((float(x) - ridge.x1) * dx + (float(y) - ridge.y1) * dy) / length, 0.0f, 1.0f);
			const float s = std::hypot(float(x) - (ridge.x1 + t * dx), float(y) - (ridge.y1 + t * dy));
			if (s > 2.0f * ridge.crest) continue;
			float &h = this->At(x, y);
			h = std::max(h, ridge.crest - s * 0.5f);
		}
	}
}

void Field::Level(const Pad &pad)
{
	/* A half-level grade reaches any terrain height within this distance. */
	const int reach = 32;
	for (int y = std::max(0, pad.y0 - reach); y <= std::min(int(this->height) - 1, pad.y1 + reach); ++y) {
		for (int x = std::max(0, pad.x0 - reach); x <= std::min(int(this->width) - 1, pad.x1 + reach); ++x) {
			const int dx = x < pad.x0 ? pad.x0 - x : (x > pad.x1 ? x - pad.x1 : 0);
			const int dy = y < pad.y0 ? pad.y0 - y : (y > pad.y1 ? y - pad.y1 : 0);
			const float grade = 0.5f * float(dx + dy);
			float &h = this->At(x, y);
			h = std::clamp(h, pad.level - grade, pad.level + grade);
		}
	}
}

void Field::TaperEdges()
{
	for (uint32_t y = 0; y < this->height; ++y) {
		for (uint32_t x = 0; x < this->width; ++x) {
			const uint32_t edge = std::min({x, y, this->width - 1 - x, this->height - 1 - y});
			float &h = this->At(x, y);
			h = std::min(h, float(edge));
		}
	}
}

std::vector<uint8_t> Field::Quantize(int max_height) const
{
	std::vector<int> q(this->heights.size());
	for (size_t i = 0; i < q.size(); ++i) q[i] = std::clamp(int(std::lround(this->heights[i])), 0, max_height);
	auto index = [this](uint32_t x, uint32_t y) { return size_t(y) * this->width + x; };
	/* Separable passes give the highest field below q whose neighbours differ by at most one level. */
	for (uint32_t y = 0; y < this->height; ++y) {
		for (uint32_t x = 1; x < this->width; ++x) q[index(x, y)] = std::min(q[index(x, y)], q[index(x - 1, y)] + 1);
		for (uint32_t x = this->width - 1; x-- > 0;) q[index(x, y)] = std::min(q[index(x, y)], q[index(x + 1, y)] + 1);
	}
	for (uint32_t x = 0; x < this->width; ++x) {
		for (uint32_t y = 1; y < this->height; ++y) q[index(x, y)] = std::min(q[index(x, y)], q[index(x, y - 1)] + 1);
		for (uint32_t y = this->height - 1; y-- > 0;) q[index(x, y)] = std::min(q[index(x, y)], q[index(x, y + 1)] + 1);
	}
	return std::vector<uint8_t>(q.begin(), q.end());
}

uint32_t CountInvalidTiles(const std::vector<uint8_t> &heights, uint32_t width, uint32_t height)
{
	uint32_t invalid = 0;
	auto at = [&](uint32_t x, uint32_t y) { return int(heights[size_t(y) * width + x]); };
	for (uint32_t y = 0; y + 1 < height; ++y) {
		for (uint32_t x = 0; x + 1 < width; ++x) {
			const int n = at(x, y), w = at(x + 1, y), e = at(x, y + 1), s = at(x + 1, y + 1);
			if (std::abs(n - w) > 1 || std::abs(n - e) > 1 || std::abs(s - w) > 1 || std::abs(s - e) > 1) ++invalid;
		}
	}
	return invalid;
}

} // namespace AlienTerrain

namespace {
using Json = nlohmann::json;
using namespace AlienTerrain;

/** @brief Hollowfrost map width in tiles. */
constexpr uint32_t MAP_X = 512;
/** @brief Hollowfrost map height in tiles. */
constexpr uint32_t MAP_Y = 256;
/** @brief Highest authored terrain level. */
constexpr int MAX_LEVEL = 14;
/** @brief The frozen crater frontier. */
constexpr WorldID FROST{0};
/** @brief The basalt industrial world beyond the gate. */
constexpr WorldID FORGE{1};
/** @brief Level of the surveyed Hollowfrost line and its sites. */
constexpr int FROST_LEVEL = 4;
/** @brief Level of the surveyed Kestrel Forge line and its sites. */
constexpr int FORGE_LEVEL = 3;
/** @brief Row of both surveyed lines and their platforms. */
constexpr uint32_t LINE = 128;
/** @brief Length of every authored platform. */
constexpr uint8_t PLATFORM = 7;
/** @brief Names of the two worlds, which also identify an authored Hollowfrost save. */
constexpr std::array<const char *, 2> WORLD_NAMES{"Hollowfrost", "Kestrel Forge"};

/** @brief One authored freight platform. */
struct StopSpec {
	const char *name; ///< Player-facing station name.
	uint32_t x; ///< West end of the platform on the surveyed line.
};
/** @brief Mine, silica, observatory, fab and institute platforms in line order. */
constexpr std::array<StopSpec, 5> STOPS{{{"Hollow Mine", 96}, {"Glass Flats", 140}, {"Rimehold Observatory", 172}, {"Kestrel Fab", 330}, {"Kestrel Survey Institute", 360}}};
/** @brief Rail depots at the west and east ends of the line, with the tile joining each to its platform. */
constexpr std::array<std::tuple<uint32_t, DiagDirection, uint32_t>, 2> DEPOTS{{{94, DiagDirection::SW, 95}, {368, DiagDirection::NE, 367}}};
/** @brief Gate heads: Hollowfrost enters south-west, Kestrel Forge north-east, both on the line. */
constexpr std::array<std::pair<uint32_t, DiagDirection>, 2> GATES{{{214, DiagDirection::SW}, {296, DiagDirection::NE}}};

/** @brief Older and younger impacts on Hollowfrost; later entries overwrite earlier ones inside their rims. */
constexpr std::array<Crater, 17> CRATERS{{
	{40, 196, 14, 8, 3, 6}, {118, 222, 9, 7, 3, 3}, {186, 214, 12, 8, 3, 5}, {152, 238, 6, 6, 3, 0},
	{60, 30, 11, 8, 4, 4}, {24, 132, 8, 7, 3, 0}, {132, 34, 10, 8, 4, 3}, {226, 60, 9, 7, 4, 0},
	{212, 176, 6, 6, 4, 0}, {80, 170, 7, 6, 3, 2}, {20, 236, 9, 7, 3, 4}, {236, 236, 8, 7, 3, 0},
	{108, 58, 12, 8, 4, 0}, {196, 22, 7, 6, 4, 2}, {58, 226, 5, 5, 3, 0},
	/* The Hollow, whose southern wall the line breaches, and Rimehold Crater above the observatory. */
	{84, 100, 34, 10, 4, 14}, {176, 96, 15, 9, 5, 5},
}};
/** @brief Hollowfrost ice ridges. */
constexpr std::array<Ridge, 2> RIDGES{{{10, 250, 70, 170, 9}, {150, 4, 240, 40, 8}}};
/** @brief Kestrel Forge mesas. */
constexpr std::array<Mesa, 8> MESAS{{{330, 60, 16, 9}, {420, 76, 20, 10}, {350, 196, 14, 8}, {460, 206, 18, 9}, {300, 226, 10, 7}, {492, 150, 12, 8}, {400, 232, 9, 7}, {280, 36, 8, 6}}};
/** @brief Level construction pads: lines, gates, sites, towns and the ash flats. */
constexpr std::array<Pad, 15> PADS{{
	{92, 126, 200, 131, FROST_LEVEL}, {188, 120, 222, 137, FROST_LEVEL}, {92, 117, 110, 131, FROST_LEVEL},
	{134, 125, 152, 139, FROST_LEVEL}, {166, 117, 184, 131, FROST_LEVEL}, {130, 94, 150, 114, FROST_LEVEL},
	{104, 138, 124, 154, FROST_LEVEL},
	{300, 126, 372, 131, FORGE_LEVEL}, {288, 120, 322, 137, FORGE_LEVEL}, {324, 117, 342, 131, FORGE_LEVEL},
	{354, 122, 374, 134, FORGE_LEVEL}, {376, 138, 396, 158, FORGE_LEVEL}, {448, 96, 464, 112, FORGE_LEVEL},
	{410, 160, 470, 190, FORGE_LEVEL - 1}, {300, 100, 312, 112, FORGE_LEVEL},
}};
/** @brief Authored towns: centre, size and name. */
constexpr std::array<std::tuple<uint32_t, uint32_t, TownSize, const char *>, 4> TOWNS{{
	{140, 104, TownSize::Small, "Rimehold"}, {114, 146, TownSize::Small, "Hollow Camp"},
	{386, 148, TownSize::Medium, "Kestrel"}, {456, 104, TownSize::Small, "Ochre Point"},
}};
/** @brief Named landmarks shown as map signs. */
constexpr std::array<std::tuple<uint32_t, uint32_t, const char *>, 8> LANDMARKS{{
	{84, 100, "The Hollow"}, {112, 124, "The Breach"}, {176, 92, "Rimehold Crater"}, {40, 210, "Frozen Spine"},
	{186, 214, "Southmere Crater"}, {420, 72, "Kestrel Mesas"}, {440, 175, "Ash Flats"}, {205, 124, "CST Gate to Kestrel Forge"},
}};

/** @brief Print an Alien Frontier failure receipt.
 * @param reason Human-readable failure context.
 * @return Always false for direct propagation.
 */
bool Failure(std::string_view reason) { IConsolePrint(CC_ERROR, "ALIEN FAIL {}", reason); return false; }

/** @brief Report the exact native command rejection for an authoring action.
 * @param cost Native quote or execution result.
 * @param action Action name for the diagnostic receipt.
 * @return True when the native command succeeded.
 */
bool Result(CommandCost cost, std::string_view action)
{
	if (cost.Succeeded()) return true;
	const StringID id = cost.GetErrorMessage();
	return Failure(fmt::format("{}: error {} {}", action, id.base(), cost.GetEncodedMessage().empty() ? (id == INVALID_STRING_ID ? "command rejected" : GetString(id)) : cost.GetEncodedMessage().GetDecodedString()));
}

/** @brief Resolve a four-character catalogue cargo label.
 * @param label Four-character cargo label.
 * @return Native cargo type, or INVALID_CARGO when absent.
 */
CargoType Cargo(const char *label) { CargoLabel value; std::copy_n(label, 4, value.begin()); return GetCargoTypeByLabel(value); }

/** @brief Find a train engine by its GRF and local identifier.
 * @param local Local engine identifier in the GRF.
 * @param grf Source GRF identifier.
 * @return Matching native engine identifier, or invalid when absent.
 */
EngineID EngineLocal(uint local, GrfID grf = COMMONWEALTH_RAIL_GRFID)
{
	for (const Engine *engine : Engine::Iterate()) if (engine->type == VehicleType::Train && engine->grf_prop.grfid == grf && engine->grf_prop.local_id == local) return engine->index;
	return EngineID::Invalid();
}

/** @brief Find the authored platform with a given name.
 * @param name Exact station name.
 * @return Matching station, or null.
 */
Station *Stop(std::string_view name)
{
	for (Station *station : Station::Iterate()) if (station->name == name) return station;
	return nullptr;
}

/** @return Whether the loaded map is an authored Hollowfrost survey. */
bool IsHollowfrost()
{
	if (Map::SizeX() != MAP_X || Map::SizeY() != MAP_Y || PlanetManager::Count() != 2) return false;
	const PlanetRegion *frost = PlanetManager::GetRegion(FROST), *forge = PlanetManager::GetRegion(FORGE);
	return frost != nullptr && forge != nullptr && frost->name == WORLD_NAMES[0] && forge->name == WORLD_NAMES[1];
}

/** @return Whether only this process can observe the game: offline, or a dedicated server without clients. */
bool Isolated()
{
	return _game_mode == GameMode::Normal && (!_networking || (_network_dedicated && NetworkClientInfo::GetNumItems() <= 1));
}

/** @return The company founded by the authorer, or null. */
Company *Charter()
{
	return IsHollowfrost() ? Company::GetIfValid(CompanyID{0}) : nullptr;
}

/** @brief Compose the Hollowfrost and Kestrel Forge corner heights.
 * @return Quantized corner levels for the whole map.
 */
std::vector<uint8_t> Terrain()
{
	Field field(MAP_X, MAP_Y);
	for (uint32_t y = 0; y < MAP_Y; ++y) {
		for (uint32_t x = 0; x < MAP_X; ++x) {
			/* Gentle swells on Hollowfrost and low dunes on Kestrel Forge; craters and mesas carry the drama. */
			field.At(x, y) = x < 256
					? 4.0f + 0.8f * std::sin(x * 0.061f + 0.8f) * std::cos(y * 0.047f - 0.3f) + 0.4f * std::sin((x + y) * 0.033f)
					: 3.0f + 0.9f * std::sin(x * 0.05f) * std::cos(y * 0.07f);
		}
	}
	for (const Ridge &ridge : RIDGES) field.AddRidge(ridge);
	for (const Crater &crater : CRATERS) field.AddCrater(crater);
	for (const Mesa &mesa : MESAS) field.AddMesa(mesa);
	for (const Pad &pad : PADS) field.Level(pad);
	field.TaperEdges();
	return field.Quantize(MAX_LEVEL);
}

/** @brief Plant deterministic groves: crystalline sub-arctic stands on Hollowfrost, sparse obsidian spines on Kestrel Forge. */
void Groves()
{
	for (uint32_t y = 2; y < MAP_Y - 2; ++y) {
		for (uint32_t x = 2; x < MAP_X - 2; ++x) {
			TileIndex tile = TileXY(x, y);
			if (!IsTileType(tile, TileType::Clear)) continue;
			bool reserved = false;
			for (const Pad &pad : PADS) {
				if (int(x) >= pad.x0 - 3 && int(x) <= pad.x1 + 3 && int(y) >= pad.y0 - 3 && int(y) <= pad.y1 + 3) reserved = true;
			}
			if (reserved) continue;
			const uint32_t noise = x * 73856093u ^ y * 19349663u;
			const float grove = std::sin(x * 0.13f) * std::cos(y * 0.11f) + std::sin((float(x) - float(y)) * 0.07f);
			const WorldID world = PlanetManager::GetTileWorld(tile);
			if (world == FROST && grove > 1.1f && noise % 3 == 0) {
				MakeTree(tile, TreeType(TREE_SUB_ARCTIC + noise % TREE_COUNT_SUB_ARCTIC), noise % 4, TreeGrowthStage::Grown, TreeGround::SnowOrDesert, 3);
			} else if (world == FORGE && grove > 1.4f && noise % 5 == 0) {
				MakeTree(tile, TREE_CACTUS, 0, TreeGrowthStage::Grown, TreeGround::Rough, 3);
			}
		}
	}
}

/** @brief Expose rock where the relief should read: crater walls, rims and mesa cliffs, and bare gravel left at sea level. */
void Surface()
{
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (!IsTileType(tile, TileType::Clear)) continue;
		const WorldID world = PlanetManager::GetTileWorld(tile);
		if (world == INVALID_WORLD) continue;
		const int low = GetTileZ(tile), high = GetTileMaxZ(tile);
		if (low != 0 && (high == low || high < (world == FROST ? 6 : 5))) continue;
		/* Snowy rock on Hollowfrost; bare basalt on Kestrel Forge. The snow bit survives the ground change.
		 * Snowless Hollowfrost shores use the desert-zone rock drawing, which reads as icy gravel. */
		SetClearGroundDensity(tile, ClearGround::Rocks, IsSnowTile(tile) ? GetClearDensity(tile) : 3);
		if (world == FROST && !IsSnowTile(tile)) SetTropicZone(tile, TropicZone::Desert);
	}
}

/** @brief Admit only a fresh, unowned, isolated 512x256 map with integrated content.
 * Generated towns and objects are allowed; Prepare removes them with the native editor command.
 */
bool Admit()
{
	if (!Isolated() || _generating_world || Map::SizeX() != MAP_X || Map::SizeY() != MAP_Y) return Failure("fresh isolated 512x256 game required");
	if (Company::GetNumItems() != 0 || Industry::GetNumItems() != 0 || Station::GetNumItems() != 0 || Vehicle::GetNumItems() != 0 ||
		Sign::GetNumItems() != 0) return Failure("generated map must have no companies, industries, stations, vehicles or signs");
	if (!PortalRegistry::GetAllPortals().empty() || StellarNetwork::Enabled() || FirstPermanentLine::Active() || World0Opening::Active() ||
		_settings_game.game_creation.cst_sector || _settings_game.game_creation.landscape != LandscapeType::Temperate) return Failure("temperate map without gates, CST sector or another authored mission required");
	if (!IntegratedEconomy::ContentReady()) return Failure("integrated Commonwealth content is not active");
	return true;
}

/** @brief Remove the generator's towns, and with the last of them its objects, through the native editor command.
 * @return True when only clear land, water, trees and void remain.
 */
bool ClearGenerated()
{
	{
		AutoRestoreBackup mode(_game_mode, GameMode::Editor);
		AutoRestoreBackup editor(_current_company, OWNER_NONE);
		while (Town::GetNumItems() != 0) {
			const Town *town = *Town::Iterate().begin();
			if (!Result(Command<Commands::DeleteTown>::Do(DoCommandFlag::Execute, town->index), "generated town removal")) return false;
		}
	}
	if (Object::GetNumItems() != 0) return Failure("generated objects remain");
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		TileType type = GetTileType(tile);
		if (type != TileType::Clear && type != TileType::Water && type != TileType::Trees && type != TileType::Void) return Failure("generated map has construction");
	}
	return true;
}

/** @brief Build a platform, give it its authored name, and return it.
 * @param spec Authored platform.
 * @return Built station, or null after a failure receipt.
 */
Station *Platform(const StopSpec &spec)
{
	TileIndex tile = TileXY(spec.x, LINE);
	if (!Result(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Axis::X, 1, PLATFORM, STAT_CLASS_DFLT, 0, NEW_STATION, true), spec.name)) return nullptr;
	Station *station = Station::Get(GetStationIndex(tile));
	if (!Result(Command<Commands::RenameStation>::Do(DoCommandFlag::Execute, station->index, spec.name), "station name")) return nullptr;
	return station;
}

/** @brief Purchase one empty wagon and couple it to a train.
 * @param depot Depot holding the train.
 * @param front Front engine.
 * @param local Wagon local GRF identifier.
 * @param cargo Wagon cargo.
 * @return True when purchase and ordinary coupling both succeed.
 */
bool Wagon(TileIndex depot, VehicleID front, uint local, CargoType cargo)
{
	auto [cost, id, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineLocal(local), false, cargo, ClientID::Invalid);
	return Result(cost, "empty wagon") && Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, id, front, false), "ordinary coupling");
}

/** @brief Fund one industry through ordinary survey and construction commands.
 * @param local Local industry identifier in the integrated GRF.
 * @param world Owning world.
 * @param x Preferred north tile X.
 * @param y Preferred north tile Y.
 * @param primary Whether the industry extracts from a resource site.
 * @return The funded industry, or null after a failure receipt.
 */
Industry *Fund(uint local, WorldID world, uint32_t x, uint32_t y, bool primary)
{
	/* Bit 7 selects the GRF's own local industry ID rather than an original industry type. */
	const IndustryType type = MapNewGRFIndustryType(0x80 | local, COMMONWEALTH_INDUSTRY_GRFID);
	if (primary) ResourceSiteManager::AddSite(TileXY(x, y), type, 6, 4);
	Json funding;
	Industry *industry = FirstLineFundIndustry(type, world, TileXY(x, y), primary, funding);
	if (industry == nullptr) Failure(fmt::format("industry {:#x} funding {}", local, funding.dump()));
	return industry;
}

/** @brief Author the complete Hollowfrost survey on the admitted map.
 * @return True after the prepared receipt is printed.
 */
bool Prepare()
{
	if (!Admit()) return false;
	_pause_mode.Set(PauseMode::Normal);
	if (!ClearGenerated()) return false;
	ResourceSiteManager::Reset();
	IntegratedEconomy::Reset();
	MegacityManager::Reset();
	PortalRegistry::Reset();
	PlanetManager::Reset();
	if (!PlanetManager::RegisterRegion({.id = FROST, .name = WORLD_NAMES[0], .phase = WorldPhase::Phase3_Frontier, .biome = WorldBiome::SubArctic,
			.min_x = 1, .min_y = 1, .max_x = 245, .max_y = MAP_Y - 3, .development_score = 100}) ||
		!PlanetManager::RegisterRegion({.id = FORGE, .name = WORLD_NAMES[1], .phase = WorldPhase::Phase2_Developed, .biome = WorldBiome::Volcanic,
			.min_x = 266, .min_y = 1, .max_x = MAP_X - 3, .max_y = MAP_Y - 3, .development_score = 10000})) return Failure("world registration");

	/* Terrain: every tile is rewritten; void buffers keep their corner heights for the neighbouring world tiles. */
	const std::vector<uint8_t> levels = Terrain();
	const uint32_t invalid = CountInvalidTiles(levels, MAP_X, MAP_Y);
	if (invalid != 0) return Failure(fmt::format("{} invalid terrain tiles", invalid));
	for (uint32_t y = 0; y < MAP_Y; ++y) {
		for (uint32_t x = 0; x < MAP_X; ++x) {
			TileIndex tile = TileXY(x, y);
			if (PlanetManager::GetTileWorld(tile) != INVALID_WORLD) MakeClear(tile, ClearGround::Grass, 3); else MakeVoid(tile);
			SetTileHeight(tile, levels[size_t(y) * MAP_X + x]);
		}
	}
	/* Flat crater basins at sea level become still meltwater: river water neither floods nor grows a grassy coast.
	 * The taper keeps every map edge above sea level, so only crater lakes qualify. */
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsTileType(tile, TileType::Clear) && GetTileSlope(tile) == SLOPE_FLAT && GetTileZ(tile) == 0) MakeRiver(tile, uint8_t((TileX(tile) * 19 + TileY(tile) * 7) & 255));
	}
	for (const PlanetRegion &region : PlanetManager::GetAllRegions()) MultiWorldGen::ApplyBiomeStyling(region);
	Surface();
	Groves();
	/* Snow lies down to the crater lakes; the authored groves keep their places. */
	_settings_game.game_creation.snow_line_height = MIN_SNOWLINE_HEIGHT;
	_settings_game.construction.extra_tree_placement = 0;
	_settings_game.economy.multiple_industry_per_town = true;

	std::array<PortalTerminalLayout, 2> terminals;
	for (size_t n = 0; n < GATES.size(); ++n) {
		auto [x, dir] = GATES[n];
		std::optional<PortalTerminalLayout> layout = PortalTerminal::Plan(TileXY(x, LINE), dir, n == 0 ? FROST : FORGE);
		if (!layout || TileY(TileAddByDiagDir(layout->connection_tile, layout->outward_dir)) != LINE) return Failure("gate terminal does not join the surveyed line");
		terminals[n] = *layout;
	}
	MultiWorldGen::BuildPublicGatePair(terminals[0], terminals[1], 266 - 245 - 1, false);
	/* Neutral terminal signals must settle before any company-owned track joins the buffer. */
	UpdateSignalsInBuffer();

	{
		AutoRestoreBackup deity(_current_company, OWNER_DEITY);
		for (auto [x, y, size, name] : TOWNS) {
			auto [cost, money, town] = Command<Commands::FoundTown>::Do(DoCommandFlag::Execute, TileXY(x, y), size, false, TownLayout::BetterRoads, false, 0, std::string(name));
			if (!Result(cost, name)) return false;
			if (Town::Get(town)->cache.population == 0) return Failure(fmt::format("{} has no population", name));
		}
		for (auto [x, y, text] : LANDMARKS) {
			if (!Result(std::get<0>(Command<Commands::PlaceSign>::Do(DoCommandFlag::Execute, TileXY(x, y), std::string(text))), text)) return false;
		}
	}

	Company *company = DoStartupNewCompany(false, CompanyID{0});
	if (company == nullptr) return Failure("survey company creation");
	if (!_network_dedicated) SetLocalCompany(company->index);
	AutoRestoreBackup owner(_current_company, company->index);
	company->money = 100000000; // Replaced by the declared charter cash once the starter assets are bought.
	company->avail_railtypes.Set(RAILTYPE_RAIL);
	if (!Result(Command<Commands::RenameCompany>::Do(DoCommandFlag::Execute, "Frostline Survey Company"), "charter name")) return false;
	if (!IntegratedEconomy::StartNewWorksPrototype()) return Failure("Works rules for the two authored worlds");
	TechTreeManager::RestoreCompanyTech(company->index, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3, TECH_MATERIALS_4, TECH_TRACTION_1});
	FabricationManager::SetFabricateFromStockpile(company->index, false);

	/* Imported construction kits for the fab and observatory; their commissioning consumes them all. */
	for (WorldID world : {FROST, FORGE}) {
		StockpileManager::AddCargo(world, company->index, Cargo("STEL"), 50);
		StockpileManager::AddCargo(world, company->index, Cargo("MACH"), 10);
	}
	/* North-side works sit close enough to the line for any layout to fall inside the platform catchment. */
	const std::array<Industry *, 4> works{Fund(0x14, FROST, 98, 123, true), Fund(0x18, FROST, 141, 131, true), Fund(0x1C, FROST, 173, 123, false), Fund(0x1B, FORGE, 331, 123, false)};
	if (std::ranges::find(works, nullptr) != works.end()) return false;
	for (WorldID world : {FROST, FORGE}) {
		if (StockpileManager::GetStock(world, company->index, Cargo("STEL")) != 0 || StockpileManager::GetStock(world, company->index, Cargo("MACH")) != 0) return Failure("construction kits were not fully consumed");
	}

	for (size_t n = 0; n < STOPS.size(); ++n) {
		const Station *station = Platform(STOPS[n]);
		if (station == nullptr) return false;
		if (n >= works.size()) continue;
		bool served = false;
		for (TileIndex tile : works[n]->location) served |= station->catchment_tiles.HasTile(tile);
		if (!served) return Failure(fmt::format("{} does not serve its works", STOPS[n].name));
	}
	Station *institute = Stop(STOPS[4].name);
	if (!Result(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, institute->xy, institute->index, STOPS[4].name), "empty institute warehouse")) return false;
	for (auto [x, dir, join] : DEPOTS) {
		if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, TileXY(x, LINE), RAILTYPE_RAIL, dir), "line-end depot") ||
			!Result(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, TileXY(join, LINE), RAILTYPE_RAIL, Track::X, false), "depot throat")) return false;
	}
	const TileIndex depot = TileXY(std::get<0>(DEPOTS[0]), LINE);
	auto [cost, starter, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineLocal(0x21), false, INVALID_CARGO, ClientID::Invalid);
	/* A complete survey consist: silica and rare earths in the fab's two-to-one ratio, two blank vaults and one crystal vault. */
	if (!Result(cost, "starter Vulcan")) return false;
	for (auto [local, label] : {std::pair{0x32u, "SAND"}, {0x32u, "SAND"}, {0x32u, "RARE"}, {0x31u, "BCRY"}, {0x31u, "BCRY"}, {0x31u, "QCRY"}}) {
		if (!Wagon(depot, starter, local, Cargo(label))) return false;
	}
	company->money = 2000000;
	UpdateSignalsInBuffer();
	Json receipt;
	receipt["map"] = "hollowfrost";
	receipt["starter"] = starter.base();
	IConsolePrint(CC_DEFAULT, "ALIEN prepared {}", receipt.dump());
	return true;
}

/** @brief Describe one native cargo store as label to units.
 * @param cargo Cargo type.
 * @return Four-character label.
 */
std::string Label(CargoType cargo) { return IsValidCargoType(cargo) ? CargoSpec::Get(cargo)->label.AsString() : std::string("----"); }

/** @return Native mission observations without game-state mutation. */
Json Snapshot()
{
	const Company *company = Charter();
	Json r{{"map", "hollowfrost"}, {"date", TimerGameCalendar::date.base()}, {"paused", _pause_mode.Any()},
		{"company", company != nullptr ? Json(company->name) : Json(nullptr)}, {"money", company != nullptr ? int64_t(company->money) : int64_t{0}},
		{"rules", int(IntegratedEconomy::ActiveRules())}, {"worlds", Json::array()}, {"industries", Json::array()}, {"stations", Json::array()},
		{"trains", Json::array()}, {"stock", Json::object()}, {"goals", Json::array()}, {"pages", Json::array()}, {"portals", PortalRegistry::GetAllPortals().size()}};
	for (const PlanetRegion &world : PlanetManager::GetAllRegions()) {
		r["worlds"].push_back({{"id", world.id.base()}, {"name", world.name}, {"biome", int(world.biome)}, {"phase", int(world.phase)}, {"role", int(IntegratedEconomy::Role(world.id))}});
	}
	for (const Industry *industry : Industry::Iterate()) {
		Json row{{"id", industry->index.base()}, {"local", GetIndustrySpec(industry->type)->grf_prop.local_id}, {"tile", industry->location.tile.base()},
			{"world", PlanetManager::GetTileWorld(industry->location.tile).base()}, {"founder", industry->founder.base()}, {"accepted", Json::object()}, {"produced", Json::object()}};
		for (const auto &a : industry->accepted) if (IsValidCargoType(a.cargo)) row["accepted"][Label(a.cargo)] = a.waiting;
		for (const auto &p : industry->produced) if (IsValidCargoType(p.cargo)) row["produced"][Label(p.cargo)] = p.waiting;
		r["industries"].push_back(row);
	}
	for (const Station *station : Station::Iterate()) {
		Json row{{"id", station->index.base()}, {"name", station->name}, {"xy", station->xy.base()}, {"world", PlanetManager::GetTileWorld(station->xy).base()}, {"waiting", Json::object()}};
		for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) if (station->goods[cargo].HasData() && station->goods[cargo].GetData().cargo.TotalCount() > 0) row["waiting"][Label(cargo)] = station->goods[cargo].GetData().cargo.TotalCount();
		r["stations"].push_back(row);
	}
	for (const Train *train : Train::Iterate()) {
		if (!train->IsFrontEngine()) continue;
		Json row{{"id", train->index.base()}, {"tile", train->tile.base()}, {"world", PlanetManager::GetTileWorld(train->tile).base()}, {"orders", train->GetNumOrders()},
			{"stopped", train->vehstatus.Test(VehState::Stopped)}, {"crashed", train->vehstatus.Test(VehState::Crashed)}, {"lost", train->vehicle_flags.Test(VehicleFlag::PathfinderLost)}, {"load", Json::object()}};
		for (const Vehicle *part = train; part != nullptr; part = part->Next()) if (IsValidCargoType(part->cargo_type)) row["load"][Label(part->cargo_type)] = row["load"].value(Label(part->cargo_type), 0) + part->cargo.TotalCount();
		r["trains"].push_back(row);
	}
	if (company != nullptr) {
		for (WorldID world : {FROST, FORGE}) {
			for (const char *label : {"SAND", "RARE", "BCRY", "QCRY", "STEL", "MACH"}) r["stock"][fmt::format("{}", world.base())][label] = StockpileManager::GetStock(world, company->index, Cargo(label));
		}
		for (const Goal *goal : Goal::Iterate()) {
			if (goal->company != company->index) continue;
			r["goals"].push_back({{"text", goal->text.GetDecodedString()}, {"progress", goal->progress.GetDecodedString()}, {"completed", goal->completed}});
		}
		for (const StoryPage *page : StoryPage::Iterate()) if (page->company == company->index) r["pages"].push_back(page->title.GetDecodedString());
	}
	return r;
}

/** @return Native terrain, biome surface and mission cargo text observations. */
Json Audit()
{
	Json r{{"invalid_slopes", 0}, {"water", 0}, {"snow", 0}, {"rough", 0}, {"rocks", 0}, {"trees", 0}, {"void", 0}, {"language", GetCurrentLanguageIsoCode()}, {"cargos", Json::array()}};
	for (uint32_t y = 0; y < Map::MaxY(); ++y) {
		for (uint32_t x = 0; x < Map::MaxX(); ++x) {
			TileIndex tile = TileXY(x, y);
			const int n = TileHeight(tile), w = TileHeight(TileXY(x + 1, y)), e = TileHeight(TileXY(x, y + 1)), s = TileHeight(TileXY(x + 1, y + 1));
			if (std::abs(n - w) > 1 || std::abs(n - e) > 1 || std::abs(s - w) > 1 || std::abs(s - e) > 1) r["invalid_slopes"] = r["invalid_slopes"].get<uint32_t>() + 1;
			if (IsTileType(tile, TileType::Water)) r["water"] = r["water"].get<uint32_t>() + 1;
			if (IsTileType(tile, TileType::Trees)) r["trees"] = r["trees"].get<uint32_t>() + 1;
			if (IsTileType(tile, TileType::Void)) r["void"] = r["void"].get<uint32_t>() + 1;
			if (IsTileType(tile, TileType::Clear) && IsSnowTile(tile)) r["snow"] = r["snow"].get<uint32_t>() + 1;
			if (IsTileType(tile, TileType::Clear) && !IsSnowTile(tile) && IsClearGround(tile, ClearGround::Rough)) r["rough"] = r["rough"].get<uint32_t>() + 1;
			if (IsTileType(tile, TileType::Clear) && IsClearGround(tile, ClearGround::Rocks)) r["rocks"] = r["rocks"].get<uint32_t>() + 1;
		}
	}
	for (const char *label : {"SAND", "RARE", "BCRY", "QCRY"}) {
		const CargoSpec *cargo = CargoSpec::Get(Cargo(label));
		r["cargos"].push_back({{"label", label}, {"name", GetString(cargo->name)}, {"units", GetString(cargo->units_volume, 100)}, {"quantity", GetString(cargo->quantifier, 100)}, {"abbreviation", GetString(cargo->abbrev)}});
	}
	return r;
}

/** @brief Lay one straight run of ordinary rail along the surveyed line.
 * @param x1 First tile X.
 * @param x2 Last tile X.
 * @param total Accumulated native cost.
 * @return True when every missing piece was built.
 */
bool Line(uint32_t x1, uint32_t x2, int64_t &total)
{
	for (uint32_t x = x1; x <= x2; ++x) {
		TileIndex tile = TileXY(x, LINE);
		if (IsRailStationTile(tile) || (IsPlainRailTile(tile) && GetTrackBits(tile).Test(Track::X))) continue;
		CommandCost cost = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Track::X, false);
		if (!Result(cost, fmt::format("reference rail at {}", x))) return false;
		total += int64_t(cost.GetCost());
	}
	return true;
}

/** @brief Build the reference service used by the headless proof: both lines and orders for the starter.
 * Not part of the player's opening; it runs only on a disposable copy.
 * @return True after the built receipt is printed.
 */
bool Build()
{
	Company *company = Charter();
	if (company == nullptr || !Isolated() || Train::GetNumItems() != 7) return Failure("reference service requires the untouched Hollowfrost opening");
	Train *starter = nullptr;
	for (Train *train : Train::Iterate()) if (train->IsFrontEngine() && train->GetNumOrders() == 0 && train->IsInDepot()) starter = train;
	if (starter == nullptr) return Failure("starter train is not idle in its depot");
	AutoRestoreBackup owner(_current_company, company->index);
	const int64_t money_before = int64_t(company->money);
	int64_t rail = 0;
	TileIndex frost_join = INVALID_TILE, forge_join = INVALID_TILE;
	for (const auto &[id, portal] : PortalRegistry::GetAllPortals()) {
		for (const auto &end : {portal.end_a, portal.end_b}) {
			std::optional<PortalTerminalLayout> layout = PortalTerminal::Plan(end.tile, end.enter_dir, end.world_id);
			if (!layout) return Failure("gate terminal plan");
			(end.world_id == FROST ? frost_join : forge_join) = TileAddByDiagDir(layout->connection_tile, layout->outward_dir);
		}
	}
	if (frost_join == INVALID_TILE || forge_join == INVALID_TILE) return Failure("gate joins");
	if (!Line(STOPS[0].x + PLATFORM, TileX(frost_join), rail) || !Line(TileX(forge_join), STOPS[4].x - 1, rail)) return false;
	/* The Institute warehouse stores everything unloaded there, so the train reaches it carrying only
	 * quantum crystals: ore off at the fab first, then blanks on at the fab on the way home. */
	const std::array<std::tuple<size_t, OrderUnloadType, OrderLoadType>, 6> plan{{
		{0, OrderUnloadType::NoUnload, OrderLoadType::LoadIfPossible}, {1, OrderUnloadType::NoUnload, OrderLoadType::LoadIfPossible},
		{3, OrderUnloadType::UnloadIfPossible, OrderLoadType::NoLoad}, {4, OrderUnloadType::UnloadIfPossible, OrderLoadType::NoLoad},
		{3, OrderUnloadType::NoUnload, OrderLoadType::LoadIfPossible}, {2, OrderUnloadType::UnloadIfPossible, OrderLoadType::LoadIfPossible}}};
	for (uint8_t n = 0; n < plan.size(); ++n) {
		auto [stop, unload, load] = plan[n];
		Order order; order.MakeGoToStation(Stop(STOPS[stop].name)->index);
		order.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
		order.SetUnloadType(unload);
		order.SetLoadType(load);
		if (!Result(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, starter->index, VehicleOrderID{n}, order), "reference order")) return false;
	}
	if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, starter->index, false), "reference departure")) return false;
	UpdateSignalsInBuffer();
	IConsolePrint(CC_DEFAULT, "ALIEN built {}", Json{{"rail_cost", rail}, {"debit", money_before - int64_t(company->money)}, {"train", starter->index.base()}}.dump());
	return true;
}

/** @brief Named review views: crater, breach, observatory, gate, fab, mesas, ash flats, southern craters. */
constexpr std::array<std::tuple<const char *, uint32_t, uint32_t>, 8> VIEWS{{
	{"hollow", 84, 104}, {"breach", 112, 126}, {"rimehold", 176, 108}, {"gate", 206, 126},
	{"kestrel-fab", 332, 124}, {"mesas", 420, 84}, {"ash-flats", 440, 168}, {"southmere", 180, 210},
}};

/** @brief Capture one review view per frame, then quit.
 * @param n Next view index.
 * @param waits Frames already spent waiting for the main window.
 */
void Shot(size_t n, uint32_t waits)
{
	VideoDriver *driver = VideoDriver::GetInstance();
	if (FindWindowById(WindowClass::MainWindow, 0) == nullptr || HasModalProgress()) {
		if (waits < 600) driver->QueueOnMainThread([n, waits] { Shot(n, waits + 1); }); else Failure("main window never appeared for screenshots");
		return;
	}
	if (n == VIEWS.size()) {
		IConsolePrint(CC_DEFAULT, "ALIEN shots done");
		IConsoleCmdExec("quit");
		return;
	}
	auto [name, x, y] = VIEWS[n];
	ScrollMainWindowToTile(TileXY(x, y), true);
	/* Screenshots are queued too, so each capture runs before the next view scrolls. */
	driver->QueueOnMainThread([n, name] {
		MakeScreenshot(SC_DEFAULTZOOM, fmt::format("hollowfrost-{}", name), 1600, 1000);
		VideoDriver::GetInstance()->QueueOnMainThread([n] { Shot(n + 1, 0); });
	});
}
} // namespace

bool ConAlienWorld(std::span<std::string_view> argv)
{
	if (argv.size() < 2 || argv.size() > 3) { Failure("operation: prepare hollowfrost, status, audit, build, speed N, shots"); return true; }
	if (argv[1] == "prepare" && argv.size() == 3 && argv[2] == "hollowfrost") { Prepare(); return true; }
	if (!IsHollowfrost()) { Failure("no authored Hollowfrost survey"); return true; }
	if (argv[1] == "status" && argv.size() == 2) {
		IConsolePrint(CC_DEFAULT, "ALIEN state {}", Snapshot().dump());
	} else if (argv[1] == "audit" && argv.size() == 2) {
		IConsolePrint(CC_DEFAULT, "ALIEN audit {}", Audit().dump());
	} else if (argv[1] == "build" && argv.size() == 2) {
		Build();
	} else if (argv[1] == "speed" && argv.size() == 3) {
		uint32_t speed;
		auto [end, error] = std::from_chars(argv[2].data(), argv[2].data() + argv[2].size(), speed);
		/* Proof runs only: an isolated process may run its own clock as fast as it can (0) or at a percentage. */
		if (error != std::errc{} || end != argv[2].data() + argv[2].size() || speed > 10000 || !Isolated()) { Failure("speed requires an isolated game and 0..10000"); return true; }
		_game_speed = uint16_t(speed);
		IConsolePrint(CC_DEFAULT, "ALIEN speed {}", speed);
	} else if (argv[1] == "shots" && argv.size() == 2) {
		/* Review captures need a real video driver with a GUI; the process quits afterwards. */
		if (_networking || VideoDriver::GetInstance() == nullptr || !VideoDriver::GetInstance()->HasGUI()) { Failure("shots require an offline game with a GUI video driver"); return true; }
		VideoDriver::GetInstance()->QueueOnMainThread([] { Shot(0, 0); });
		IConsolePrint(CC_DEFAULT, "ALIEN shots queued");
	} else {
		Failure("operation: prepare hollowfrost, status, audit, build, speed N, shots");
	}
	return true;
}
