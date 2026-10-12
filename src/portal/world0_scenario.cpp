/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file world0_scenario.cpp Fresh authored world and finite offline native diagnostics. */
#include "../stdafx.h"
#include "world0_scenario.h"
#include "world0_opening.h"
#include "first_permanent_line.h"
#include "first_line_funding.h"
#include "connected_economy.h"
#include "commonwealth_slice.h"
#include "commonwealth_pack.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_cmd.h"
#include "stellar_network.h"
#include "integrated_economy.h"
#include "resource_sites.h"
#include "fabrication_manager.h"
#include "company_stockpile.h"
#include "corporate_hq.h"
#include "logistics_hub.h"
#include "tech_tree.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_cmd.h"
#include "../console_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../genworld.h"
#include "../progress.h"
#include "../water_map.h"
#include "../tree_cmd.h"
#include "../road_cmd.h"
#include "../town.h"
#include "../town_cmd.h"
#include "../industry.h"
#include "../newgrf_industries.h"
#include "../station_base.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../engine_base.h"
#include "../engine_gui.h"
#include "../gfx_func.h"
#include "../train.h"
#include "../train_cmd.h"
#include "../vehicle_cmd.h"
#include "../vehicle_gui.h"
#include "../widgets/build_vehicle_widget.h"
#include "../screenshot.h"
#include "../order_base.h"
#include "../order_cmd.h"
#include "../story_base.h"
#include "../story_cmd.h"
#include "../goal_base.h"
#include "../goal_cmd.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../language.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../network/network.h"
#include "../openttd.h"
#include "../timer/timer_game_tick.h"
#include "../video/video_driver.hpp"
#include "../core/backup_type.hpp"
#include "../core/random_func.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include <charconv>
#include <filesystem>
#include "../safeguards.h"
extern Company *DoStartupNewCompany(bool, CompanyID);

std::vector<std::pair<TileIndex, Track>> GetWorld0RailRoute(uint route)
{
	std::vector<std::pair<TileIndex, Track>> pieces;
	auto line = [&](int x1, int y1, int x2, int y2, Track track) {
		for (int y = y1; y <= y2; ++y) for (int x = x1; x <= x2; ++x) pieces.emplace_back(TileXY(x, y), track);
	};
	if (route == 0) {
		line(47, 70, 47, 70, Track::X); line(52, 70, 91, 70, Track::X);
		line(92, 70, 92, 70, Track::Right); line(92, 71, 92, 115, Track::Y);
		line(92, 116, 92, 116, Track::Left); line(93, 116, 95, 116, Track::X);
		line(100, 116, 101, 116, Track::X);
	} else if (route == 1) {
		line(103, 123, 103, 123, Track::X); line(108, 123, 131, 123, Track::X);
		line(132, 123, 132, 123, Track::Right); line(132, 124, 132, 145, Track::Y);
		line(132, 146, 132, 146, Track::Left); line(133, 146, 146, 146, Track::X);
		line(151, 146, 152, 146, Track::X);
	}
	return pieces;
}

namespace {
using Json = nlohmann::json;
constexpr uint LENGTH = 4;
constexpr std::array<std::pair<uint, uint>, 4> PLATFORMS{{{48, 70}, {96, 116}, {104, 123}, {147, 146}}};
constexpr std::array<const char *, 4> STOPS{"Stonecross Quarry", "Stonecross Works Input", "Stonecross Works Output", "Greybank Yard"};
constexpr std::array<const char *, 3> GOALS{"Deliver 80 SILC and make 40 native ballast batches", "Bring 40 BALL home to Greybank Yard", "Return to the works and make another useful ballast delivery"};
constexpr const char *BRIEF = "You have founded Vale Railway in Aster Vale. Greybank, Stonecross and Merefield already have homes and roads. Stonecross Works needs a railway: bring silica from the quarry, then take its ballast to your yard beside Greybank.";
constexpr const char *ASSISTANCE = "Aster Vale is Phase 3 Frontier; its economic role is Frontier. Your charter permits one ballast works on its named plot. Founding assistance: GBP 750,000, imported 80 STEL, Materials I, an empty Pioneer with two SILC hoppers, the quarry and works, short platforms and depot stubs, and an empty Greybank yard. Both through railways are yours to build. Every SILC and BALL store starts empty.";
constexpr const char *INSTRUCTIONS = "Finish Quarry to Works input, give the Pioneer those two orders and start it. Collect BALL from the separate Works output platform and unload it at Greybank Yard. The yard stores your materials; warehouse freight pays no sale income. Rail fabrication starts off, so normal pioneer track is cash-built. With BALL and the finite imported STEL you may later enable fabrication.";
constexpr const char *RECOVERY = "No deadline or starvation loop applies. Empty input means send silica; full output means collect ballast. A stopped train, wrong orders or full buffer can be fixed. Two useful ballast arrivals separated by a return to the works establish the railway. Keep the company and continue free play.";
bool bridge_pending = false;

bool Failure(std::string_view reason) { IConsolePrint(CC_ERROR, "VALE FAIL {}", reason); return false; }

/** The startup script may run before modal generation cleanup. This predicate never authors state. */
bool BridgeAdmission()
{
	if (_networking || _network_dedicated || _settings_game.difficulty.infinite_money ||
		_settings_game.game_creation.generation_seed != 11 || _settings_game.game_creation.landscape != LandscapeType::Temperate ||
		_settings_game.game_creation.cst_sector || Map::SizeX() != 256 || Map::SizeY() != 256 ||
		FirstPermanentLine::Active() || StellarNetwork::Enabled() || !PortalRegistry::GetAllPortals().empty() ||
		!PortalRegistry::GetUnlinkedGates().empty() || !PortalRegistry::GetAllInterServerPortals().empty()) return false;
	if (_game_mode == GameMode::Editor) {
		if (PlanetManager::Count() != 0 || Company::GetNumItems() != 0 || Town::GetNumItems() != 0 ||
			Station::GetNumItems() != 0 || Industry::GetNumItems() != 0 || Vehicle::GetNumItems() != 0 ||
			!ResourceSiteManager::Sites().empty() || !CorporateHQManager::GetAllHQ().empty() ||
			!LogisticsHubManager::GetAllHubs().empty() || !StockpileManager::GetAllStockpiles().empty() ||
			IntegratedEconomy::Enabled() || World0Opening::State().ready || World0Opening::Active() ||
			World0Opening::State().profile.company != CompanyID::Invalid() ||
			!IntegratedEconomy::ContentReady()) return false;
		for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
			TileType type = GetTileType(tile);
			if (type == TileType::Void && TileX(tile) != 0 && TileY(tile) != 0 &&
				TileX(tile) != Map::MaxX() && TileY(tile) != Map::MaxY()) return false;
			if (type != TileType::Clear && type != TileType::Water && type != TileType::Void) return false;
		}
		return true;
	}
	if (_game_mode != GameMode::Normal || !_pause_mode.Test(PauseMode::Normal) ||
		!World0Opening::Active() || PlanetManager::Count() != 1 ||
		!IntegratedEconomy::Enabled() ||
		IntegratedEconomy::ActiveRules() != EconomyRulesVersion::V2 || IntegratedEconomy::Role(WorldID{0}) != EconomicRole::Frontier ||
		!World0Opening::ValidateAfterLoad()) return false;
	const PlanetRegion *world = PlanetManager::GetRegion(WorldID{0});
	return world != nullptr && world->phase == WorldPhase::Phase3_Frontier && World0Opening::State().profile.crusher_plot != INVALID_TILE;
}

/** Reject parser metacharacters, noncanonical paths, symlinks and non-FIFOs before queuing and again before exec. */
bool BridgePath(std::string_view raw)
{
	if (raw.empty() || raw.size() > 1024) return false;
	for (unsigned char c : raw) if (c < 0x20 || c == 0x7f || c == '"' || c == '\'' || c == '\\') return false;
	std::filesystem::path path{std::string(raw)};
	if (!path.is_absolute() || path.lexically_normal() != path) return false;
	std::error_code error;
	std::filesystem::path part;
	for (const auto &component : path) {
		part /= component;
		auto status = std::filesystem::symlink_status(part, error);
		if (error || std::filesystem::is_symlink(status)) return false;
	}
	return std::filesystem::is_fifo(std::filesystem::status(path, error)) && !error;
}

void RunBridge(std::string path)
{
	VideoDriver *driver = VideoDriver::GetInstance();
	if (driver == nullptr) {
		bridge_pending = false;
		Debug(misc, 0, "VALE FAIL startup video queue disappeared");
		Failure("startup video queue disappeared"); return;
	}
	if (HasModalProgress() || _generating_world) {
		driver->QueueOnMainThread([path = std::move(path)]() mutable { RunBridge(std::move(path)); });
		return;
	}
	bridge_pending = false;
	if (!BridgeAdmission() || !BridgePath(path)) {
		Debug(misc, 0, "VALE FAIL startup bridge admission changed before exec");
		Failure("startup bridge admission changed before exec"); return;
	}
	IConsoleCmdExec(fmt::format("exec \"{}\"", path));
}
bool Result(CommandCost cost, std::string_view action)
{
	if (cost.Succeeded()) return true;
	const StringID id = cost.GetErrorMessage();
	return Failure(fmt::format("{}: error {} {}", action, id.base(), cost.GetEncodedMessage().empty() ? (id == INVALID_STRING_ID ? "command rejected" : GetString(id)) : cost.GetEncodedMessage().GetDecodedString()));
}
CargoType Cargo(const char *label) { CargoLabel value; std::copy_n(label, 4, value.begin()); return GetCargoTypeByLabel(value); }
EngineID EngineLocal(uint local, GrfID grf = COMMONWEALTH_RAIL_GRFID)
{
	for (const Engine *engine : Engine::Iterate()) if (engine->type == VehicleType::Train && engine->grf_prop.grfid == grf && engine->grf_prop.local_id == local) return engine->index;
	return EngineID::Invalid();
}
Station *Stop(size_t n)
{
	for (Station *station : Station::Iterate()) if (station->owner == World0Opening::State().profile.company && station->name == STOPS[n]) return station;
	return nullptr;
}
Train *Starter() { for (Train *train : Train::Iterate()) if (train->IsFrontEngine() && train->owner == World0Opening::State().profile.company && train->tile == TileXY(46, 70)) return train; return nullptr; }
bool Catches(const Station *station, const Industry *industry) { if (station == nullptr || industry == nullptr) return false; for (TileIndex tile : industry->location) if (station->catchment_tiles.HasTile(tile)) return true; return false; }

/** Fresh terrain only. Height is a shared corner field, blended around roads, settlements and rail. */
bool Landscape()
{
	std::vector<std::pair<int, int>> flat;
	for (uint route = 0; route < 2; ++route) for (auto [tile, track] : GetWorld0RailRoute(route)) flat.emplace_back(TileX(tile), TileY(tile));
	for (auto [x, y] : PLATFORMS) for (uint dx = 0; dx <= LENGTH; ++dx) flat.emplace_back(x + dx, y);
	for (auto [cx, cy, radius] : {std::tuple{160, 145, 19}, {82, 86, 11}, {192, 74, 10}, {48, 72, 7}, {100, 118, 9}})
		for (int y = cy - radius; y <= cy + radius; ++y) for (int x = cx - radius; x <= cx + radius; ++x) flat.emplace_back(x, y);
	for (int y = 86; y <= 102; ++y) flat.emplace_back(82, y);
	for (int x = 52; x <= 174; ++x) flat.emplace_back(x, 102);
	for (int y = 74; y <= 145; ++y) { flat.emplace_back(160, y); flat.emplace_back(174, y); }
	for (int x = 174; x <= 192; ++x) flat.emplace_back(x, 74);
	for (int y = 76; y <= 102; ++y) flat.emplace_back(52, y);
	for (int y = 102; y <= 118; ++y) flat.emplace_back(108, y);
	std::vector<std::pair<int, int>> water;
	for (int y = 100; y <= 215; ++y) {
		int x = 185 + (y - 100) / 3 + ((y / 12) % 2);
		water.emplace_back(x, y); water.emplace_back(x + 1, y);
	}
	for (int y = 202; y <= 230; ++y) for (int x = 210; x <= 238; ++x) if (abs(x - 224) + abs(y - 216) <= 18) water.emplace_back(x, y);
	/* Each height field is 1-Lipschitz; minima preserve legal shared edges. */
	for (uint y = 0; y < Map::SizeY(); ++y) for (uint x = 0; x < Map::SizeX(); ++x) {
		TileIndex tile = TileXY(x, y);
		int ridge = 2 + std::max(0, 7 - (abs(int(x) - 38) + abs(int(y) - 35)) / 13);
		int h = std::min(ridge, int(std::min({x, y, Map::MaxX() - x, Map::MaxY() - y})));
		int d = 1000; for (auto [fx, fy] : flat) d = std::min(d, abs(int(x) - fx) + abs(int(y) - fy));
		h = std::min(h, 2 + d / 4);
		d = 1000; for (auto [wx, wy] : water) d = std::min(d, abs(int(x) - wx) + abs(int(y) - wy));
		h = std::min(h, std::max(0, d - 1) / 2);
		SetTileHeight(tile, h);
		if (x == 0 || y == 0 || x == Map::MaxX() || y == Map::MaxY()) MakeVoid(tile); else MakeClear(tile, ClearGround::Grass, 3);
	}
	for (auto [x, y] : water) { TileIndex tile = TileXY(x, y); if (GetTileSlope(tile) == SLOPE_FLAT) MakeRiver(tile, uint8_t((x * 19 + y * 7) & 255)); }
	return true;
}
bool Road(uint x1, uint y1, uint x2, uint y2)
{
	if (!Result(Command<Commands::BuildRoadLong>::Do(DoCommandFlag::Execute, TileXY(x2, y2), TileXY(x1, y1), ROADTYPE_ROAD, x1 == x2 ? Axis::Y : Axis::X, {}, false, false, false), "regional road")) return false;
	return true;
}
bool Wagon(TileIndex depot, VehicleID front, uint local, CargoType cargo, GrfID grf = COMMONWEALTH_RAIL_GRFID)
{
	auto [cost, id, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineLocal(local, grf), false, cargo, ClientID::Invalid);
	return Result(cost, "empty hopper") && Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, id, front, false), "ordinary coupling");
}
bool Rail(TileIndex tile, Track track, bool execute, int64_t &sum)
{
	if (IsPlainRailTile(tile) && GetTileOwner(tile) == _current_company && GetRailType(tile) == RAILTYPE_RAIL && GetTrackBits(tile).Test(track)) return true;
	auto quote = Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_RAIL, track, false);
	if (!Result(quote, fmt::format("rail quote tile {}", tile.base()))) return false;
	sum += int64_t(quote.GetCost());
	if (execute) {
		auto paid = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, track, false);
		if (!Result(paid, "paid rail") || paid.GetCost() != quote.GetCost()) return Failure("rail quote/execution differ");
	}
	return true;
}
bool Routes(bool execute, int64_t &sum)
{
	for (uint route = 0; route < 2; ++route) for (auto [tile, track] : GetWorld0RailRoute(route)) if (!Rail(tile, track, execute, sum)) return false;
	return true;
}
Json Snapshot()
{
	Json r = GetCoreBasketObservation();
	const auto &s = World0Opening::State();
	r["mission"] = s.active ? Json::parse(World0Opening::Save()) : Json{};
	r["economy"] = Json::parse(IntegratedEconomy::Save());
	const Company *company = Company::GetIfValid(s.profile.company);
	r["money"] = company == nullptr ? int64_t{0} : int64_t(company->money);
	r["tick"] = uint64_t(TimerGameTick::counter); r["map_fnv1a64"] = GetConnectedEconomyMapFingerprint();
	r["rng"] = {_random.state[0], _random.state[1]};
	r["worlds"] = Json::array(); for (const auto &world : PlanetManager::GetAllRegions()) r["worlds"].push_back({{"id", world.id.base()}, {"name", world.name}, {"phase", uint(world.phase)}, {"economic_role", uint(IntegratedEconomy::Role(world.id))}, {"bounds", {world.min_x, world.min_y, world.max_x, world.max_y}}});
	r["towns"] = Json::array(); for (const Town *town : Town::Iterate()) r["towns"].push_back({{"id", town->index.base()}, {"name", town->name}, {"tile", town->xy.base()}, {"population", town->cache.population}, {"houses", town->cache.num_houses}});
	r["story_pages"] = Json::array(); for (const StoryPage *page : StoryPage::Iterate()) r["story_pages"].push_back({page->index.base(), page->company.base(), page->title.GetDecodedString()});
	r["story_elements"] = Json::array(); for (const StoryPageElement *element : StoryPageElement::Iterate()) r["story_elements"].push_back({element->index.base(), element->page.base(), uint(element->type), element->referenced_id, element->text.GetDecodedString()});
	r["goals"] = Json::array(); for (const Goal *goal : Goal::Iterate()) r["goals"].push_back({goal->index.base(), goal->company.base(), goal->text.GetDecodedString(), goal->completed, goal->progress.GetDecodedString()});
	r["industries"] = Json::array();
	for (const Industry *industry : Industry::Iterate()) {
		Json row{{"id", industry->index.base()}, {"tile", industry->location.tile.base()}, {"owner", industry->founder.base()}, {"recipe", IntegratedEconomy::IndustryRecipe(industry->type)}, {"inputs", Json::array()}, {"outputs", Json::array()}};
		for (const auto &cargo : industry->accepted) row["inputs"].push_back({cargo.cargo, cargo.waiting});
		for (const auto &cargo : industry->produced) row["outputs"].push_back({cargo.cargo, cargo.waiting});
		r["industries"].push_back(row);
	}
	r["stations"] = Json::array();
	for (const Station *station : Station::Iterate()) {
		std::array<uint64_t, NUM_CARGO> cargo{}; for (CargoType c{0}; c < NUM_CARGO; ++c) cargo[c] = station->goods[c].TotalCount();
		r["stations"].push_back({{"id", station->index.base()}, {"tile", station->xy.base()}, {"owner", station->owner.base()}, {"name", station->name}, {"cargo", cargo}});
	}
	r["trains"] = Json::array();
	for (const Train *train : Train::Iterate()) {
		Json orders = Json::array(); if (train->IsFrontEngine()) for (const auto &order : train->Orders()) orders.push_back({order.GetType(), order.GetDestination().base(), order.GetLoadType(), order.GetUnloadType(), order.GetNonStopType().base()});
		r["trains"].push_back({{"id", train->index.base()}, {"front", train->IsFrontEngine()}, {"owner", train->owner.base()}, {"tile", train->tile.base()}, {"cargo_type", train->cargo_type}, {"cargo", train->cargo.StoredCount()}, {"capacity", train->cargo_cap}, {"speed", train->cur_speed}, {"order_index", train->cur_real_order_index}, {"orders", orders}, {"stopped", train->vehstatus.Test(VehState::Stopped)}, {"crashed", train->vehstatus.Test(VehState::Crashed)}, {"lost", train->vehicle_flags.Test(VehicleFlag::PathfinderLost)}});
	}
	r["stocks"] = Json::array(); for (const auto &stock : StockpileManager::GetAllStockpiles()) r["stocks"].push_back({stock.world_id.base(), stock.company_id.base(), stock.inventory});
	r["research"] = Json::array(); for (const auto &tech : TechTreeManager::GetAllCompanyTechStates()) r["research"].push_back({tech.active_project, tech.accumulated_rp, tech.monthly_budget, tech.unlocked_techs});
	r["quarry_source_station"] = Stop(0) == nullptr ? StationID::Invalid().base() : Stop(0)->index.base();
	r["crusher_source_station"] = Stop(2) == nullptr ? StationID::Invalid().base() : Stop(2)->index.base();
	r["permit"] = World0Opening::PermitsCrusher(s.profile.crusher_plot, MapNewGRFIndustryType(0x91, COMMONWEALTH_INDUSTRY_GRFID), s.profile.company);
	return r;
}
bool Briefing()
{
	AutoRestoreBackup deity(_current_company, OWNER_DEITY);
	auto [cost, page] = Command<Commands::CreateStoryPage>::Do(DoCommandFlag::Execute, World0Opening::State().profile.company, GetEncodedString(STR_JUST_RAW_STRING, std::string_view("The Vale Railway")));
	if (!Result(cost, "founding Story Book")) return false;
	for (const char *text : {BRIEF, ASSISTANCE, INSTRUCTIONS, RECOVERY}) {
		auto [element_cost, id] = Command<Commands::CreateStoryPageElement>::Do(DoCommandFlag::Execute, TileIndex{0}, page, StoryPageElementType::Text, 0, GetEncodedString(STR_JUST_RAW_STRING, std::string_view(text)));
		if (!Result(element_cost, "briefing paragraph")) return false;
	}
	std::array<TileIndex, 6> locations{Industry::Get(World0Opening::State().binding.quarry)->location.tile, World0Opening::State().profile.crusher_plot, Station::Get(World0Opening::State().binding.yard)->xy};
	for (uint i = 0; i < 3; ++i) locations[i + 3] = Town::Get(World0Opening::State().profile.towns[i])->xy;
	const std::array<const char *, 6> labels{"Stonecross Quarry", "Stonecross Works", "Greybank Yard", "Greybank", "Stonecross", "Merefield"};
	for (uint i = 0; i < locations.size(); ++i) {
		auto [element_cost, id] = Command<Commands::CreateStoryPageElement>::Do(DoCommandFlag::Execute, locations[i], page, StoryPageElementType::Location, 0, GetEncodedString(STR_JUST_RAW_STRING, std::string_view(labels[i])));
		if (!Result(element_cost, "briefing location")) return false;
	}
	for (const char *label : GOALS) {
		auto [goal_cost, goal] = Command<Commands::CreateGoal>::Do(DoCommandFlag::Execute, World0Opening::State().profile.company, GoalType::None, GoalTypeID{0}, GetEncodedString(STR_JUST_RAW_STRING, std::string_view(label)));
		if (!Result(goal_cost, "founding goal")) return false;
		auto [element_cost, id] = Command<Commands::CreateStoryPageElement>::Do(DoCommandFlag::Execute, TileIndex{0}, page, StoryPageElementType::Goal, goal.base(), EncodedString{});
		if (!Result(element_cost, "briefing goal")) return false;
	}
	return Result(Command<Commands::ShowStoryPage>::Do(DoCommandFlag::Execute, page), "show founding charter");
}
bool Prepare()
{
	if (_game_mode != GameMode::Editor || Map::SizeX() != 256 || Map::SizeY() != 256 || _generating_world ||
		Town::GetNumItems() != 0 || Company::GetNumItems() != 0 || Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0 || Industry::GetNumItems() != 0 ||
		IntegratedEconomy::Enabled() || World0Opening::Ready() || FirstPermanentLine::Active() || StellarNetwork::Enabled() || !PortalRegistry::GetAllPortals().empty() ||
		_settings_game.game_creation.cst_sector || !IntegratedEconomy::ContentReady()) return Failure("fresh empty 256-tile editor map with content and no CST required");
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) if (!IsTileType(tile, TileType::Clear) && !IsTileType(tile, TileType::Water) && !IsTileType(tile, TileType::Void)) return Failure("nonempty editor terrain");
	_pause_mode.Set(PauseMode::Normal);
	PlanetManager::Reset();
	PlanetRegion world; world.id = WorldID{0}; world.name = "Aster Vale"; world.phase = WorldPhase::Phase3_Frontier; world.biome = WorldBiome::Temperate; world.min_x = world.min_y = 1; world.max_x = world.max_y = 254;
	PlanetManager::RegisterRegion(world);
	if (!Landscape()) return false;
	World0OpeningProfile profile;
	{
		AutoRestoreBackup deity(_current_company, OWNER_DEITY);
		/* Public roads precede real native town growth; buildings grow from their street network. */
		/* Native FoundTown requires the center itself clear; roads meet its neighbours. */
		if (!Road(82, 87, 82, 102) || !Road(52, 102, 174, 102) || !Road(160, 102, 160, 144) || !Road(174, 74, 174, 102) || !Road(174, 74, 191, 74) || !Road(52, 76, 52, 102) || !Road(108, 102, 108, 118)) return false;
		const std::array<std::tuple<uint, uint, TownSize, bool, const char *>, 3> towns{{{160, 145, TownSize::Large, true, "Greybank"}, {82, 86, TownSize::Medium, false, "Stonecross"}, {192, 74, TownSize::Small, false, "Merefield"}}};
		for (size_t n = 0; n < towns.size(); ++n) {
			auto [x, y, size, city, name] = towns[n];
			auto [cost, money, town] = Command<Commands::FoundTown>::Do(DoCommandFlag::Execute, TileXY(x, y), size, city, TownLayout::BetterRoads, false, 0, std::string(name));
			if (!Result(cost, "native populated town")) return false;
			profile.towns[n] = town;
			if (Town::Get(town)->cache.population == 0) return Failure("native town has no population");
		}
	}
	/* Empty editor authoring is complete; all company development below uses normal commands. */
	_game_mode = GameMode::Normal;
	Company *company = DoStartupNewCompany(false, CompanyID{0});
	if (company == nullptr) return Failure("player company creation");
	SetLocalCompany(company->index);
	AutoRestoreBackup owner(_current_company, company->index);
	company->money = 100000000; // Removed before baseline; all purchased starter assets are disclosed assistance.
	company->avail_railtypes.Set(RAILTYPE_RAIL);
	profile.company = company->index; profile.crusher_plot = TileXY(100, 118);
	ResourceSiteManager::SetEnabled(true);
	IndustryType quarry_type = MapNewGRFIndustryType(0x90, COMMONWEALTH_INDUSTRY_GRFID), crusher_type = MapNewGRFIndustryType(0x91, COMMONWEALTH_INDUSTRY_GRFID);
	profile.quarry_site = ResourceSiteManager::AddSite(TileXY(48, 72), quarry_type, 8, 8);
	if (!World0Opening::BeginAuthoring(profile) || !IntegratedEconomy::StartAuthoredWorld0(profile.towns)) return Failure("guarded authored v2 initializer");
	TechTreeManager::RestoreCompanyTech(company->index, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	FabricationManager::SetFabricateFromStockpile(company->index, false);
	_settings_game.economy.multiple_industry_per_town = true;
	Json funding;
	Industry *quarry = FirstLineFundIndustry(quarry_type, WorldID{0}, TileXY(48, 72), true, funding); IConsolePrint(CC_DEFAULT, "VALE funding {}", funding.dump());
	if (quarry == nullptr) return Failure("paid native quarry funding");
	Industry *crusher = FirstLineFundIndustry(crusher_type, WorldID{0}, profile.crusher_plot, false, funding); IConsolePrint(CC_DEFAULT, "VALE funding {}", funding.dump());
	if (crusher == nullptr || crusher->location.tile != profile.crusher_plot) return Failure("paid native exact-plot crusher funding");
	std::array<Station *, 4> stops{};
	for (uint n = 0; n < stops.size(); ++n) {
		auto [x, y] = PLATFORMS[n]; TileIndex tile = TileXY(x, y);
		if (!Result(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Axis::X, 1, LENGTH, STAT_CLASS_DFLT, 0, NEW_STATION, true), "empty four-tile platform")) return false;
		stops[n] = Station::Get(GetStationIndex(tile));
		if (!Result(Command<Commands::RenameStation>::Do(DoCommandFlag::Execute, stops[n]->index, STOPS[n]), "station name")) return false;
	}
	if (!Catches(stops[0], quarry) || !Catches(stops[1], crusher) || !Catches(stops[2], crusher) || Catches(stops[1], quarry) || Catches(stops[2], quarry) || Catches(stops[3], crusher) || Catches(stops[3], quarry)) return Failure("separate native platform catchments");
	if (!Result(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, stops[3]->xy, stops[3]->index, "Greybank Rail Yard"), "empty Greybank hub")) return false;
	if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, TileXY(46, 70), RAILTYPE_RAIL, DiagDirection::SW), "starter depot")) return false;
	int64_t ignored = 0;
	for (auto [x, y] : {std::pair{47u, 70u}, {95u, 116u}, {100u, 116u}, {103u, 123u}, {108u, 123u}, {146u, 146u}, {151u, 146u}}) if (!Rail(TileXY(x, y), Track::X, true, ignored)) return false;
	auto [train_cost, train, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, TileXY(46, 70), EngineLocal(0x20), false, INVALID_CARGO, ClientID::Invalid);
	if (!Result(train_cost, "empty Pioneer") || !Wagon(TileXY(46, 70), train, 0x32, Cargo("SILC")) || !Wagon(TileXY(46, 70), train, 0x32, Cargo("SILC"))) return false;
	int64_t route_quote = 0; if (!Routes(false, route_quote)) return false;
	auto depot_quote = Command<Commands::BuildRailDepot>::Do({}, TileXY(102, 123), RAILTYPE_RAIL, DiagDirection::SW);
	auto [engine_quote, eid, ec, em, ecs] = Command<Commands::BuildVehicle>::Do({}, TileXY(46, 70), EngineLocal(0x20), false, INVALID_CARGO, ClientID::Invalid);
	auto [wagon_quote, wid, wc, wm, wcs] = Command<Commands::BuildVehicle>::Do({}, TileXY(46, 70), EngineLocal(0x38, COMMONWEALTH_INDUSTRY_GRFID), false, Cargo("BALL"), ClientID::Invalid);
	if (!Result(depot_quote, "future output depot quote") || !Result(engine_quote, "second Pioneer quote") || !Result(wagon_quote, "BALL hopper quote")) return false;
	const int64_t service_quote = route_quote + int64_t(depot_quote.GetCost()) + int64_t(engine_quote.GetCost()) + 2 * int64_t(wagon_quote.GetCost());
	if (service_quote > 500000) return Failure("two-service quote exceeds founding capital contingency");
	company->money = 750000;
	StockpileManager::AddCargo(WorldID{0}, company->index, Cargo("STEL"), 80);
	if (!Result(Command<Commands::RenameCompany>::Do(DoCommandFlag::Execute, "Vale Railway"), "charter name") || !World0Opening::Begin({quarry->index, crusher->index, stops[3]->index})) return Failure("native empty opening binding");
	/* Clustered existing trees outside the reserved corridors and actual assets. */
	for (uint y = 4; y < Map::MaxY(); ++y) for (uint x = 4; x < Map::MaxX(); ++x) {
		TileIndex tile = TileXY(x, y); if (!IsTileType(tile, TileType::Clear)) continue;
		bool reserved = false; for (uint route = 0; route < 2; ++route) for (auto [r, track] : GetWorld0RailRoute(route)) if (DistanceManhattan(tile, r) < 5) reserved = true;
		if (reserved || DistanceManhattan(tile, TileXY(102, 123)) < 5) continue;
		uint32_t noise = x * 73856093u ^ y * 19349663u;
		if (((x < 72 && y < 58) || (x > 180 && y > 112)) && noise % 7 < 3) PlaceTree(tile, noise, false);
	}
	return FinishWorld0Authoring(route_quote, service_quote);
}
bool Build()
{
	const auto &s = World0Opening::State(); Train *starter = Starter();
	if (!s.active || s.unavailable || s.economy_evaluations != 0 || starter == nullptr || starter->GetNumOrders() != 0 || !starter->IsInDepot() || !starter->vehstatus.Test(VehState::Stopped) || Train::GetNumItems() != 3) return Failure("disposable route construction requires untouched opening");
	const int64_t money_before = int64_t(Company::Get(s.profile.company)->money);
	int64_t rail_quote = 0; if (!Routes(true, rail_quote)) return false;
	if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, TileXY(102, 123), RAILTYPE_RAIL, DiagDirection::SW), "paid export depot")) return false;
	auto [cost, export_train, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, TileXY(102, 123), EngineLocal(0x20), false, INVALID_CARGO, ClientID::Invalid);
	if (!Result(cost, "paid export Pioneer") || !Wagon(TileXY(102, 123), export_train, 0x38, Cargo("BALL"), COMMONWEALTH_INDUSTRY_GRFID) || !Wagon(TileXY(102, 123), export_train, 0x38, Cargo("BALL"), COMMONWEALTH_INDUSTRY_GRFID)) return false;
	for (auto [vehicle, source, destination] : {std::tuple{starter->index, 0u, 1u}, std::tuple{export_train, 2u, 3u}}) {
		for (uint8_t n = 0; n < 2; ++n) {
			Station *station = Stop(n == 0 ? source : destination); if (station == nullptr) return Failure("missing route platform");
			Order order; order.MakeGoToStation(station->index); order.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
			if (n == 0) { order.SetUnloadType(OrderUnloadType::NoUnload); order.SetLoadType(OrderLoadType::FullLoadAny); }
			else order.SetLoadType(OrderLoadType::NoLoad);
			if (!Result(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, vehicle, VehicleOrderID{n}, order), "ordinary recurring order")) return false;
		}
		if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, vehicle, false), "ordinary train departure")) return false;
	}
	IConsolePrint(CC_DEFAULT, "VALE built {}", Json{{"rail_quote", rail_quote}, {"native_total_debit", money_before - int64_t(Company::Get(s.profile.company)->money)}, {"feeder", starter->index.base()}, {"export", export_train.base()}, {"state", Snapshot()}}.dump());
	return true;
}
Json Audit()
{
	Json r{{"invalid_slopes", 0}, {"invalid_foundations", 0}, {"pixel_queries", 0}, {"viewport_queries", 0}, {"language", GetCurrentLanguageIsoCode()}, {"cargos", Json::array()}};
	for (uint y = 0; y < Map::MaxY(); ++y) for (uint x = 0; x < Map::MaxX(); ++x) {
		TileIndex tile = TileXY(x, y); int n = TileHeight(tile), w = TileHeight(TileXY(x + 1, y)), e = TileHeight(TileXY(x, y + 1)), south = TileHeight(TileXY(x + 1, y + 1));
		if (abs(n - w) > 1 || abs(n - e) > 1 || abs(south - w) > 1 || abs(south - e) > 1) r["invalid_slopes"] = r["invalid_slopes"].get<uint>() + 1;
		if (IsPlainRailTile(tile) && GetRailFoundation(GetTileSlope(tile), GetTrackBits(tile)) == Foundation::Invalid) r["invalid_foundations"] = r["invalid_foundations"].get<uint>() + 1;
	}
	if (r["invalid_slopes"] == 0) for (uint y = 0; y < Map::MaxY(); ++y) for (uint x = 0; x < Map::MaxX(); ++x) {
		for (int offset : {0, 8, 15}) GetSlopePixelZ(x * TILE_SIZE + offset, y * TILE_SIZE + offset);
		Point point = RemapCoords2(x * TILE_SIZE + 8, y * TILE_SIZE + 8); InverseRemapCoords2(point.x, point.y, true);
	}
	r["pixel_queries"] = r["invalid_slopes"] == 0 ? uint64_t(Map::MaxX()) * Map::MaxY() * 3 : 0;
	r["viewport_queries"] = r["invalid_slopes"] == 0 ? uint64_t(Map::MaxX()) * Map::MaxY() : 0;
	for (const CargoSpec *cargo : CargoSpec::Iterate()) r["cargos"].push_back({{"label", cargo->label.AsString()}, {"name", GetString(cargo->name)}, {"single", GetString(cargo->name_single)}, {"units", GetString(cargo->units_volume, 100)}, {"quantity", GetString(cargo->quantifier, 100)}, {"zero", GetString(cargo->quantifier, 0)}, {"large", GetString(cargo->quantifier, 100000)}, {"abbreviation", GetString(cargo->abbrev)}});
	return r;
}
bool Advance()
{
	const uint start = World0Opening::State().economy_evaluations;
	if (start >= 16) return Failure("absolute diagnostic ceiling: 12 plus 4 reload evaluations");
	if (World0Opening::State().unavailable) return Failure("native bound asset is unavailable");
	Json before = Snapshot(); CommonwealthSliceAudit audit; AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
	uint ticks = 0; bool train_failure = false;
	{
		AutoRestoreBackup pause(_pause_mode); AutoRestoreBackup tick_owner(_current_company, _local_company); _pause_mode.Reset(); UpdateSignalsInBuffer();
		while (World0Opening::State().economy_evaluations == start && ticks++ < 3000 && !World0Opening::State().unavailable) {
			StateGameLoop();
			for (const Train *train : Train::Iterate()) if (train->owner == World0Opening::State().profile.company && train->IsFrontEngine() &&
				(train->vehstatus.Test(VehState::Crashed) || train->vehicle_flags.Test(VehicleFlag::PathfinderLost))) train_failure = true;
			if (train_failure) break; // Retain this first failure sample; execute no later native ticks.
		}
	}
	World0Opening::Evaluate(); Json after = Snapshot();
	after["audit"] = {{"produced", audit.produced}, {"raw_produced", audit.raw_produced}, {"raw_removed", audit.raw_removed}, {"unallocated", audit.unallocated}, {"discarded", audit.discarded}, {"consumed", audit.consumed}, {"vehicle_deliveries", audit.vehicle_deliveries}, {"arrivals", audit.arrivals}, {"processor_cargo", audit.processor_cargo}, {"cash_debits", audit.cash_debits}, {"cash_transactions", audit.cash_transactions}, {"payments", audit.payments}, {"cash_payments", audit.cash_payments}, {"gate_tolls", audit.gate_tolls}};
	after["cash_conserved"] = after["money"].get<int64_t>() == before["money"].get<int64_t>() - audit.cash_debits;
	after["cargo_errors"] = Json::array();
	for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) {
		int64_t expected = before["custody"]["total"][cargo].get<int64_t>() + audit.produced[cargo] + audit.raw_produced[cargo] - audit.raw_removed[cargo] - audit.unallocated[cargo] - audit.discarded[cargo] - audit.consumed[cargo];
		if (expected != after["custody"]["total"][cargo].get<int64_t>()) after["cargo_errors"].push_back({cargo, expected, after["custody"]["total"][cargo]});
	}
	after["evaluations_advanced"] = World0Opening::State().economy_evaluations - start;
	after["train_integrity_failure"] = train_failure;
	IConsolePrint(CC_DEFAULT, "VALE advance {}", after.dump());
	return true;
}
}

bool FinishWorld0Authoring(int64_t route_quote, int64_t service_quote)
{
	const auto &state = World0Opening::State();
	if (_game_mode != GameMode::Normal || !_pause_mode.Test(PauseMode::Normal) || Map::SizeX() != 256 || Map::SizeY() != 256 ||
		!state.active || state.unavailable || FirstPermanentLine::Active() || !World0Opening::ValidateAfterLoad() ||
		state.economy_evaluations != 0 || state.accepted_silc != 0 || state.qualified_input_remaining != 0 ||
		state.unqualified_input_remaining != 0 || state.qualified_batches != 0 || state.stored_ball != 0 ||
		state.input_goal || state.production_goal || state.storage_goal || state.awarded || !state.journeys.empty() ||
		StoryPage::GetNumItems() != 0 || StoryPageElement::GetNumItems() != 0 || Goal::GetNumItems() != 0)
		return Failure("native empty Vale opening required for completion");
	/* A headless editor has no main viewport. The briefing and receipt still complete. */
	Window *main = FindWindowById(WindowClass::MainWindow, 0);
	if (main != nullptr && main->viewport != nullptr) {
		/* Editor authoring changed modes without recreating its editor toolbar. */
		HideVitalWindows();
		ShowVitalWindows();
	}
	if (!Briefing()) return false;
	if (main != nullptr && main->viewport != nullptr) ScrollMainWindowToTile(TileXY(155, 145), true);
	IConsolePrint(CC_DEFAULT, "VALE prepared {}", Json{{"stake", 750000}, {"imported_stel", 80}, {"route_quote", route_quote}, {"two_service_quote", service_quote}, {"baseline", Snapshot()}}.dump());
	return true;
}

bool ConWorld0Scenario(std::span<std::string_view> argv)
{
	if (_networking || _network_dedicated || _settings_game.difficulty.infinite_money || argv.size() < 2 || argv.size() > 3) { Failure("offline bounded scenario adapter required"); return true; }
	if (argv[1] == "bridge" && argv.size() == 3) {
		if (bridge_pending || VideoDriver::GetInstance() == nullptr || !BridgePath(argv[2]) || !BridgeAdmission()) {
			Failure("Vale startup bridge requires a fresh empty editor or validated Vale save and an absolute FIFO"); return true;
		}
		/* The next draw tick may run a game loop before draining this queue. */
		_pause_mode.Set(PauseMode::Normal);
		bridge_pending = true;
		std::string path(argv[2]);
		VideoDriver::GetInstance()->QueueOnMainThread([path = std::move(path)]() mutable { RunBridge(std::move(path)); });
		return true;
	}
	if (argv[1] == "prepare" && argv.size() == 2) { Prepare(); return true; }
	if (_game_mode != GameMode::Normal || !World0Opening::Active()) { Failure("no active Vale opening"); return true; }
	if (argv[1] == "status" && argv.size() == 2) { IConsolePrint(CC_DEFAULT, "VALE state {}", Snapshot().dump()); return true; }
	if (!Company::IsValidID(World0Opening::State().profile.company)) { Failure("charter owner unavailable; free play remains"); return true; }
	AutoRestoreBackup owner(_current_company, World0Opening::State().profile.company);
	if (argv[1] == "purchase-ui" && argv.size() == 2) {
		VideoDriver *driver = VideoDriver::GetInstance();
		const TileIndex depot = TileXY(46, 70);
		if (driver == nullptr || !BridgeAdmission() || _local_company != _current_company ||
			!IsRailDepotTile(depot) || GetTileOwner(depot) != _current_company || GetRailType(depot) != RAILTYPE_RAIL || !IsValidCargoType(Cargo("BALL"))) {
			Failure("purchase UI proof requires paused Vale owner and its conventional starter depot"); return true;
		}
		/* Exec must reach EOF before the video queue can render. No economy tick is requested. */
		driver->QueueOnMainThread([depot] {
			auto finish = [] { IConsoleCmdExec("script"); IConsoleCmdExec("quit"); };
			auto admitted = [depot] {
				return BridgeAdmission() && _local_company == World0Opening::State().profile.company &&
					IsRailDepotTile(depot) && GetTileOwner(depot) == _local_company && GetRailType(depot) == RAILTYPE_RAIL;
			};
			if (!admitted()) {
				Failure("purchase UI admission changed"); finish(); return;
			}
			AutoRestoreBackup owner(_current_company, _local_company);
			IConsoleClose();
			ShowBuildVehicleWindow(depot, VehicleType::Train);
			Window *window = FindWindowById(WindowClass::BuildVehicle, depot.base());
			if (window == nullptr) { Failure("native purchase window missing"); finish(); return; }
			window->OnDropdownSelect(WID_BV_CARGO_FILTER_DROPDOWN, Cargo("BALL"), 0);
			/* A changed train filter deliberately clears selection. Select its first
			 * visible row through the same list-click handler a player uses. */
			DrawDirtyBlocks();
			const Rect row = window->GetWidget<NWidgetBase>(WID_BV_LIST)->GetCurrentRect().Shrink(WidgetDimensions::scaled.matrix);
			window->OnClick({(row.left + row.right) / 2, row.top + int(GetEngineListHeight(VehicleType::Train)) / 2}, WID_BV_LIST, 1);
			MakeScreenshot(SC_VIEWPORT, "vale-ball-purchase-before");
			VideoDriver::GetInstance()->QueueOnMainThread([depot, finish, admitted] {
				Window *window = FindWindowById(WindowClass::BuildVehicle, depot.base());
				if (!admitted() || window == nullptr || window->IsWidgetDisabled(WID_BV_BUILD)) {
					Failure("rendered BALL purchase button unavailable"); finish(); return;
				}
				AutoRestoreBackup owner(_current_company, _local_company);
				IConsolePrint(CC_DEFAULT, "VALE purchase-ui before {}", Snapshot().dump());
				/* Native unpause/buy/pause occur within one callback, with no intervening game tick.
				 * This respects the owner's normal construction-during-pause setting. */
				IConsoleCmdExec("unpause");
				if (_pause_mode.Any()) { Failure("native unpause rejected purchase"); finish(); return; }
				window->OnClick({}, WID_BV_BUILD, 1);
				IConsoleCmdExec("pause");
				IConsolePrint(CC_DEFAULT, "VALE purchase-ui after {}", Snapshot().dump());
				MakeScreenshot(SC_VIEWPORT, "vale-ball-purchase-after");
				VideoDriver::GetInstance()->QueueOnMainThread(finish);
			});
		});
		IConsolePrint(CC_DEFAULT, "VALE purchase-ui queued");
	} else if (argv[1] == "audit" && argv.size() == 2) {
		const LanguageMetadata *prior = _current_language; Json languages;
		for (const char *filename : {"english_US.lng", "english.lng"}) {
			const auto it = std::ranges::find_if(_languages, [filename](const auto &language) { return language.file.filename() == filename; });
			if (it == _languages.end() || !ReadLanguagePack(&*it)) { Failure("required English audit pack missing"); return true; }
			languages[filename] = Audit();
		}
		if (!ReadLanguagePack(prior)) { Failure("English audit restoration failed"); return true; }
		IConsolePrint(CC_DEFAULT, "VALE audit {}", languages.dump());
	} else if (argv[1] == "build" && argv.size() == 2) Build();
	else if (argv[1] == "advance" && argv.size() == 2) Advance();
	else if ((argv[1] == "stop" || argv[1] == "restart") && argv.size() == 3) {
		uint32_t id; auto [end, error] = std::from_chars(argv[2].data(), argv[2].data() + argv[2].size(), id);
		Train *train = error == std::errc{} && end == argv[2].data() + argv[2].size() ? Train::GetIfValid(VehicleID{id}) : nullptr;
		if (train == nullptr || !train->IsFrontEngine() || train->owner != _current_company || train->vehstatus.Test(VehState::Crashed)) { Failure("owned live front train required"); return true; }
		bool stopped = argv[1] == "stop";
		if (train->vehstatus.Test(VehState::Stopped) != stopped && !Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, train->index, false), "ordinary stop/restart")) return true;
		IConsolePrint(CC_DEFAULT, "VALE train {}", Snapshot().dump());
	} else Failure("operation: bridge PATH, prepare, status, audit, build, advance, stop ID, restart ID");
	return true;
}
