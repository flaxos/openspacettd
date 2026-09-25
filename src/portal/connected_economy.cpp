/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file connected_economy.cpp Reproducible offline connected production and megacity acceptance fixture. */
#include "../stdafx.h"
#include "commonwealth_slice.h"
#include "commonwealth_pack.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_cmd.h"
#include "megacity_manager.h"
#include "corporate_hq.h"
#include "logistics_hub.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../industry_cmd.h"
#include "../newgrf_industries.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../rail.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../station_base.h"
#include "../train_cmd.h"
#include "../vehicle_cmd.h"
#include "../order_cmd.h"
#include "../signs_cmd.h"
#include "../blueprint/blueprint_manager.h"
#include "../blueprint/blueprint_cmd.h"
#include "../timetable_cmd.h"
#include "../order_base.h"
#include "../train.h"
#include "../town_cmd.h"
#include "../town.h"
#include "../clear_map.h"
#include "../landscape.h"
#include "../void_map.h"
#include "../map_func.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../strings_func.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "../openttd.h"
#include "../network/network.h"
#include "../network/network_base.h"
#include "../core/backup_type.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include <queue>
#include "../safeguards.h"

extern Company *DoStartupNewCompany(bool, CompanyID);

namespace
{
/** Marker restricting mutations to this isolated fixture. */
constexpr std::string_view DEMO_NAME = "Connected Commonwealth UAT v2";
/** Primary collection platform row. */
constexpr uint TOP = 80;
/** A named station and its optional processing recipe. */
struct DemoNode {
	const char *name; ///< Player-facing station name.
	uint x; ///< Station platform column.
	RecipeID recipe = 0; ///< Processing recipe, or zero for a transport station.
};
/** Nodes with real rail platforms shared by dedicated freight services. */
const std::vector<DemoNode> nodes = {{"HQ warehouse", 70}, {"Megacity consumers", 116},
	{"Consumer crystals", 168, RECIPE_CONSUMER_CRYSTAL_FORMAT}, {"Food works", 148}, {"Ballast crusher", 300, RECIPE_BALLAST_CRUSHING},
	{"Steel furnace", 318, RECIPE_STEEL_SMELTING}, {"Alloy foundry", 336, RECIPE_SUPERALLOY_FOUNDRY},
	{"Copper works", 354, RECIPE_COPPER_SMELTING}, {"Silicon works", 372, RECIPE_SILICON_ARC},
	{"Signal works", 390, RECIPE_SIGNALLING_ASSEMBLY}, {"Polymer works", 408, RECIPE_POLYMER_SYNTHESIS},
	{"Propulsion works", 426, RECIPE_MAGLEV_WORKS}, {"Blank crystal fab", 444, RECIPE_MONOCRYSTAL_SYNTHESIS}, {"Stone quarry", 560},
	{"Iron mine", 590}, {"Copper mine", 620}, {"Silica dunes", 650}, {"Rare earth mine", 680}, {"Farm", 710},
	{"Quantum observatory", 820, RECIPE_QUANTUM_ENRICHMENT}, {"Frontier passengers", 875}, {"City distribution", 132},
	{"Farm export warehouse", 730}, {"Stone export", 576}, {"Iron export", 606}, {"Copper export", 636}, {"Silica export", 666},
	{"Rare earth export", 696}};
/** JSON representation of measured simulation state. */
using Json = nlohmann::json;

/**
 * Report construction errors in the console.
 * @param cost Command result.
 * @param action Description of the attempted action.
 * @return Whether the command succeeded.
 */
bool Result(const CommandCost &cost, std::string_view action)
{
	if (cost.Succeeded()) return true;
	IConsolePrint(CC_ERROR, "CONNECTED FAIL {}: {}", action,
	    cost.GetErrorMessage() == INVALID_STRING_ID ? "unspecified command failure" : GetString(cost.GetErrorMessage()));
	return false;
}
/**
 * Find a node's station through its permanent primary platform.
 * @param node Index in the node catalogue.
 * @return Station attached to that platform.
 */
StationID Stop(size_t node) { return GetStationIndex(TileXY(nodes[node].x, TOP)); }
/**
 * Resolve a Commonwealth cargo using the active content bindings.
 * @param cargo Commonwealth cargo identity.
 * @return Loaded cargo slot, or INVALID_CARGO.
 */
CargoType Cargo(CommonwealthCargoID cargo) { return ProductionChainManager::GetDefaultCargo(cargo); }

/**
 * Find the track joining two tile edges.
 * @param a First edge direction.
 * @param b Second edge direction.
 * @return Connecting track, or Track::Invalid.
 */
Track Corner(DiagDirection a, DiagDirection b)
{
	for (Track t : TRACK_BIT_ALL) {
		auto td = TrackToTrackdir(t);
		auto x = TrackdirToExitdir(td), y = TrackdirToExitdir(ReverseTrackdir(td));
		if ((a == x && b == y) || (a == y && b == x)) return t;
	}
	return Track::Invalid;
}
/**
 * Build one rail segment while preserving station and portal tiles.
 * @param tile Construction tile.
 * @param track Requested track segment.
 * @return Whether the track already exists or construction succeeds.
 */
bool Rail(TileIndex tile, Track track)
{
	if (PortalRegistry::IsPortalTile(tile) || IsTileType(tile, TileType::Station)) return true;
	if (IsPlainRailTile(tile) && GetTrackBits(tile).Test(track)) return true;
	if (IsPlainRailTile(tile) && HasSignals(tile)) {
		if (!Result(Command<Commands::RemoveSignal>::Do(DoCommandFlag::Execute, tile, FindFirstTrack(GetTrackBits(tile))),
		        "junction signal removal"))
			return false;
	}
	return Result(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, track, false),
	    fmt::format("rail {},{}", TileX(tile), TileY(tile)));
}

/**
 * Find a rolling-stock definition in the active Commonwealth rail pack.
 * @param local GRF-local vehicle identifier.
 * @return Loaded engine identifier, or an invalid identifier.
 */
EngineID EngineLocal(uint local)
{
	for (const Engine *e : Engine::Iterate())
		if (e->type == VehicleType::Train && e->grf_prop.grfid == COMMONWEALTH_RAIL_GRFID && e->grf_prop.local_id == local) return e->index;
	return EngineID::Invalid();
}

/** Number of dedicated long-distance corridors already constructed. */
uint route_number = 0;
/**
 * Build and start a cargo service using ordinary construction and order commands.
 * @param source Source node index.
 * @param destination Destination node index.
 * @param cargo Principal cargo; farm consists also carry livestock.
 * @param wagons Number of wagons to attach.
 * @param service Duplicate service number, or zero for the first service.
 * @return Whether the complete service was constructed and started.
 */
bool TrainRoute(size_t source, size_t destination, CargoType cargo, uint wagons, uint service = 0)
{
	if (!IsValidCargoType(cargo)) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL route cargo missing");
		return false;
	}
	bool feeder = source >= 13 && source <= 18 && destination >= 22;
	uint y = feeder ? TOP : TOP + 2 + 2 * route_number++;
	uint left = std::min(nodes[source].x, nodes[destination].x), right = std::max(nodes[source].x, nodes[destination].x);
	for (uint x : {220u, 476u, 732u})
		if (left < x && right > x) {
			if (!Result(Command<Commands::BuildPortalPair>::Do(
			                DoCommandFlag::Execute, TileXY(x, y), DiagDirection::SW, TileXY(x + 58, y), DiagDirection::NE, RAILTYPE_RAIL),
			        "route portals"))
				return false;
		}
	for (uint x = left - 3; x <= right + (feeder ? 4 : 16); ++x)
		if (PlanetManager::GetTileWorld(TileXY(x, y)) != INVALID_WORLD && !Rail(TileXY(x, y), Track::X)) return false;
	if (!feeder)
		for (size_t node : {source, destination}) {
			if (!Result(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, TileXY(nodes[node].x, y), RAILTYPE_RAIL, Axis::X, 1,
			                14, STAT_CLASS_DFLT, 0, Stop(node), true),
			        "route platform"))
				return false;
		}
	uint depot_x = left >= 256 ? left - 2 : right - 2;
	if (right < 256) depot_x = left - 2;
	if (service != 0) depot_x = 452;
	TileIndex depot = TileXY(depot_x, y - 1);
	if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, DiagDirection::SE), "route depot"))
		return false;
	if (!Rail(TileXY(depot_x, y), Corner(DiagDirection::NW, DiagDirection::SW))) return false;
	EngineID wagon = EngineID::Invalid();
	for (const Engine *e : Engine::Iterate()) {
		if (e->type != VehicleType::Train || e->grf_prop.grfid != COMMONWEALTH_RAIL_GRFID ||
		    e->VehInfo<RailVehicleInfo>().railveh_type != RailVehicleType::Wagon)
			continue;
		if (e->GetDefaultCargoType() != cargo && !e->info.refit_mask.Test(cargo)) continue;
		auto query = Command<Commands::BuildVehicle>::Do({}, depot, e->index, false, cargo, ClientID::Invalid);
		if (ExtractCommandCost(query).Succeeded()) {
			wagon = e->index;
			break;
		}
	}
	if (wagon == EngineID::Invalid()) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL no wagon for {}", GetString(CargoSpec::Get(cargo)->name));
		return false;
	}
	{
		auto [ec, engine, a, b, c] = Command<Commands::BuildVehicle>::Do(
		    DoCommandFlag::Execute, depot, EngineLocal(right < 256 ? 0x20 : 0x22), false, INVALID_CARGO, ClientID::Invalid);
		if (!Result(ec, "locomotive")) return false;
		for (uint w = 0; w < wagons; ++w) {
			bool livestock = (source == 18 || source == 22) && w >= wagons / 2;
			CargoType requested = livestock ? GetCargoTypeByLabel(CargoLabel{"LVST"}) : cargo;
			EngineID wagon_engine = livestock ? EngineLocal(0x37) : wagon;
			auto [wc, id, d, e, f] =
			    Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, wagon_engine, false, requested, ClientID::Invalid);
			if (!Result(wc, "cargo wagon")) return false;
			if (Train::Get(id)->cargo_type != requested) {
				IConsolePrint(CC_ERROR, "CONNECTED FAIL wagon cargo mismatch");
				return false;
			}
			if (Train::Get(id)->First()->index != engine &&
			    !Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, id, engine, false), "coupling"))
				return false;
		}
		std::string name = fmt::format("{:.12} > {:.12}{}", nodes[source].name, nodes[destination].name,
		    service == 0 ? std::string{} : fmt::format(" {}", service + 1));
		if (source == 21) {
			name = cargo == GetCargoTypeByLabel(CargoLabel{"FOOD"})    ? "Food to megacity"
			       : cargo == Cargo(CommonwealthCargoID::SiliconChips) ? "Chips to megacity"
			                                                           : "Composites to megacity";
		}
		if (!Result(Command<Commands::RenameVehicle>::Do(DoCommandFlag::Execute, engine, name), "train name")) return false;
		Order pickup;
		pickup.MakeGoToStation(Stop(source));
		pickup.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
		pickup.SetStopLocation(OrderStopLocation::NearEnd);
		pickup.SetUnloadType(OrderUnloadType::NoUnload);
		Order drop;
		drop.MakeGoToStation(Stop(destination));
		drop.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
		drop.SetStopLocation(OrderStopLocation::NearEnd);
		drop.SetLoadType(OrderLoadType::NoLoad);
		for (auto [index, order] : {std::pair{0, pickup}, std::pair{1, drop}})
			if (!Result(
			        Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, engine, VehicleOrderID{static_cast<uint8_t>(index)}, order),
			        "route order"))
				return false;
		if (source == 5 && destination == 6 &&
		    !Result(Command<Commands::ChangeTimetable>::Do(DoCommandFlag::Execute, engine, VehicleOrderID{0}, MTF_WAIT_TIME, 3000),
		        "foundry dispatch interval"))
			return false;
		if (source == 21 &&
		    !Result(Command<Commands::ChangeTimetable>::Do(DoCommandFlag::Execute, engine, VehicleOrderID{0}, MTF_WAIT_TIME, 1400),
		        "city dispatch interval"))
			return false;
		if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, engine, true), "start train")) return false;
	}
	return true;
}

/**
 * Lower exposed terrain into a continuous height field, without touching infrastructure.
 * A corner is shared by four tiles, so all four must be safe before any change is applied.
 * @return Whether the entire repair can be applied safely (failure leaves the map unchanged).
 */
bool SmoothTerrain()
{
	std::vector<uint8_t> heights(Map::Size());
	std::queue<TileIndex> pending;
	for (const TileIndex tile : Map::Iterate()) {
		heights[tile.base()] = TileHeight(tile);
		pending.push(tile);
	}
	while (!pending.empty()) {
		TileIndex tile = pending.front();
		pending.pop();
		uint x = TileX(tile), y = TileY(tile);
		for (auto [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
			int nx = static_cast<int>(x) + dx, ny = static_cast<int>(y) + dy;
			if (nx < 0 || ny < 0 || nx >= static_cast<int>(Map::SizeX()) || ny >= static_cast<int>(Map::SizeY())) continue;
			TileIndex next = TileXY(nx, ny);
			if (heights[next.base()] <= heights[tile.base()] + 1) continue;
			heights[next.base()] = heights[tile.base()] + 1;
			pending.push(next);
		}
	}
	for (const TileIndex tile : Map::Iterate()) {
		if (heights[tile.base()] == TileHeight(tile)) continue;
		for (int dy = -1; dy <= 0; ++dy) {
			for (int dx = -1; dx <= 0; ++dx) {
				int x = static_cast<int>(TileX(tile)) + dx, y = static_cast<int>(TileY(tile)) + dy;
				if (x < 0 || y < 0) continue;
				TileIndex affected = TileXY(x, y);
				TileType type = GetTileType(affected);
				if (type != TileType::Clear && type != TileType::Trees && type != TileType::Void) return false;
			}
		}
	}
	for (const TileIndex tile : Map::Iterate()) SetTileHeight(tile, heights[tile.base()]);
	return true;
}

/**
 * Construct the isolated four-world fixture on a fresh map.
 * @return Whether all construction and configuration commands succeeded.
 */
bool Prepare()
{
	if (Map::SizeX() != 1024 || Map::SizeY() != 1024 || Company::GetNumItems() != 0 || Industry::GetNumItems() != 0 ||
	    Vehicle::GetNumItems() != 0) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL requires fresh empty 1024x1024 world");
		return false;
	}
	/* Only a new disposable scenario may be levelled. Preserve all existing towns. */
	for (uint y = 50; y <= 230; ++y)
		for (uint x = 1; x < 1023; ++x) {
			TileIndex tile = TileXY(x, y);
			if (IsTileType(tile, TileType::House) || IsTileType(tile, TileType::Road)) {
				IConsolePrint(CC_ERROR, "CONNECTED FAIL generated town overlaps construction band");
				return false;
			}
		}
	PlanetManager::Reset();
	const char *names[] = {"Core Commonwealth", "Industrial Commonwealth", "Extraction frontier", "Research frontier"};
	for (uint i = 0; i < 4; ++i) {
		WorldPhase phase = i == 0 ? WorldPhase::Phase1_Core : i == 1 ? WorldPhase::Phase2_Developed : WorldPhase::Phase3_Frontier;
		if (!PlanetManager::RegisterRegion({.id = WorldID{i},
		        .name = names[i],
		        .phase = phase,
		        .biome = WorldBiome::Temperate,
		        .min_x = i * 256 + 1,
		        .min_y = 1,
		        .max_x = std::min<uint>(i * 256 + 240, 1022),
		        .max_y = 1022,
		        .development_score = 10000}))
			return false;
	}
	for (uint y = 50; y <= 230; ++y)
		for (uint x = 1; x < 1023; ++x) {
			TileIndex tile = TileXY(x, y);
			MakeClear(tile, ClearGround::Grass, 3);
			SetTileHeight(tile, 1);
			if (PlanetManager::GetTileWorld(tile) == INVALID_WORLD) MakeVoid(tile);
		}
	if (!SmoothTerrain()) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL unsafe terrain transition");
		return false;
	}
	Company *company = DoStartupNewCompany(false, CompanyID{0});
	if (company == nullptr) return false;
	company->name = DEMO_NAME;
	company->money = 100000000;
	company->clear_limit = 10000 << 16;
	company->avail_railtypes.Set(RAILTYPE_RAIL);
	AutoRestoreBackup owner(_current_company, company->index);
	_settings_game.station.station_spread = 64;
	_settings_game.economy.multiple_industry_per_town = true;
	_settings_game.vehicle.max_trains = 500;
	_settings_game.economy.town_growth_rate = 2;
	_settings_game.game_creation.snow_line_height = 10;
	FabricationManager::SetFabricateFromStockpile(company->index, false);
	TechTreeManager::RestoreCompanyTech(company->index, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_TRACTION_1, TECH_TRACTION_2});
	route_number = 0;
	for (auto [x, y, name] : {std::tuple{120u, 74u, "Commonwealth Metropolis"}, std::tuple{880u, 74u, "Research Settlement"}}) {
		AutoRestoreBackup deity(_current_company, OWNER_DEITY);
		auto [cost, money, town] = Command<Commands::FoundTown>::Do(
		    DoCommandFlag::Execute, TileXY(x, y), TownSize::Medium, true, TownLayout::Grid3x3, false, 0, std::string(name));
		if (!Result(cost, "town foundation")) return false;
		if (x == 120) {
			Town *t = Town::Get(town);
			MegacityManager::RegisterMegacity(town, WorldID{0}, name, t->cache.population);
		}
	}
	for (size_t i = 0; i < nodes.size(); ++i) {
		const auto &node = nodes[i];
		if (!Result(Command<Commands::BuildRailStation>::Do(
		                DoCommandFlag::Execute, TileXY(node.x, TOP), RAILTYPE_RAIL, Axis::X, 1, 4, STAT_CLASS_DFLT, 0, NEW_STATION, true),
		        node.name))
			return false;
		if (!Result(Command<Commands::RenameStation>::Do(DoCommandFlag::Execute, Stop(i), std::string(node.name)), "station name"))
			return false;
		if (node.recipe != 0) {
			if (!Result(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, Stop(i), node.recipe), node.name))
				return false;
			if (!Result(Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlag::Execute, Stop(i), 4096), "factory rail dispatch"))
				return false;
		}
	}
	for (auto [node, local] :
	    {std::pair{13u, 0x10u}, std::pair{14u, 0x12u}, std::pair{15u, 0x16u}, std::pair{16u, 0x18u}, std::pair{17u, 0x14u}}) {
		IndustryType type = MapNewGRFIndustryType(0x80 | local, COMMONWEALTH_INDUSTRY_GRFID);
		if (!Result(Command<Commands::BuildIndustry>::Do(DoCommandFlag::Execute, TileXY(nodes[node].x, TOP - 5), type, 0, false, 1),
		        "raw extraction"))
			return false;
	}
	// Native Arctic farm and food processor supply actual food, not proxy minerals.
	for (auto [node, type] : {std::pair{18u, IndustryType{9}}, std::pair{3u, IndustryType{13}}}) {
		bool built = false;
		for (uint dy = 5; dy <= 12 && !built; ++dy)
			for (uint layout = 0; layout < GetIndustrySpec(type)->layouts.size() && !built; ++layout) {
				TileIndex site = TileXY(nodes[node].x, TOP - dy);
				if (ExtractCommandCost(Command<Commands::BuildIndustry>::Do({}, site, type, layout, false, 1)).Failed()) continue;
				built = Result(Command<Commands::BuildIndustry>::Do(DoCommandFlag::Execute, site, type, layout, false, 1), "food industry");
			}
		if (!built) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL no food site for {}", nodes[node].name);
			return false;
		}
	}
	if (!Result(Command<Commands::BuildLogisticsHub>::Do(
	                DoCommandFlag::Execute, TileXY(nodes[0].x, TOP), Stop(0), std::string("Construction and research warehouse")),
	        "HQ warehouse"))
		return false;
	if (!Result(Command<Commands::BuildLogisticsHub>::Do(
	                DoCommandFlag::Execute, TileXY(nodes[21].x, TOP), Stop(21), std::string("City supply warehouse")),
	        "city warehouse"))
		return false;
	if (!Result(Command<Commands::BuildLogisticsHub>::Do(
	                DoCommandFlag::Execute, TileXY(nodes[22].x, TOP), Stop(22), std::string("Farm export warehouse")),
	        "farm warehouse"))
		return false;
	for (size_t i = 23; i < nodes.size(); ++i)
		if (!Result(Command<Commands::BuildLogisticsHub>::Do(
		                DoCommandFlag::Execute, TileXY(nodes[i].x, TOP), Stop(i), std::string(nodes[i].name)),
		        "raw export warehouse"))
			return false;
	if (!Result(Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, TileXY(70, 60), std::string("Commonwealth HQ")),
	        "Corporate HQ"))
		return false;
	if (!Result(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_PORTAL_1), "research project")) return false;
	if (!Result(Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 25000), "research budget")) return false;
	using C = CommonwealthCargoID;
	const std::vector<std::tuple<size_t, size_t, C>> flows = {{23, 4, C::StoneSlag}, {4, 0, C::StoneSlag}, {24, 5, C::IronOre},
	    {5, 6, C::StructuralSteel}, {5, 0, C::StructuralSteel}, {27, 6, C::RareEarthMinerals}, {6, 11, C::Superalloys},
	    {11, 0, C::Superalloys}, {25, 7, C::CopperOre}, {7, 9, C::ConductiveWiring}, {7, 10, C::ConductiveWiring},
	    {7, 11, C::ConductiveWiring}, {7, 0, C::ConductiveWiring}, {26, 8, C::SilicaSand}, {8, 9, C::SiliconChips}, {9, 0, C::SiliconChips},
	    {21, 1, C::SiliconChips}, {21, 1, C::SyntheticComposites}, {10, 0, C::SyntheticComposites}, {26, 12, C::SilicaSand},
	    {27, 12, C::RareEarthMinerals}, {12, 19, C::BlankCrystals}, {12, 2, C::BlankCrystals}, {19, 0, C::EnrichedQuantumCrystals},
	    {2, 1, C::EncryptedConsumerCrystals}};
	for (auto [from, to, cargo] : std::vector<std::tuple<size_t, size_t, C>>{
	         {13, 23, C::StoneSlag}, {14, 24, C::IronOre}, {15, 25, C::CopperOre}, {16, 26, C::SilicaSand}, {17, 27, C::RareEarthMinerals}})
		if (!TrainRoute(from, to, Cargo(cargo), 3)) return false;
	for (auto [from, to, cargo] : flows)
		if (!TrainRoute(from, to, Cargo(cargo),
		        from == 21                                            ? 1
		        : from == 5 && to == 6                                ? 1
		        : from == 7 && to != 0                                ? 1
		        : from == 26 && to == 12                              ? 4
		        : from == 24 || from == 25 || (from == 26 && to == 8) ? 12
		                                                              : 8))
			return false;
	CargoType farm_cargo = GetCargoTypeByLabel(CargoLabel{"WHEA"});
	if (!IsValidCargoType(farm_cargo)) farm_cargo = GetCargoTypeByLabel(CargoLabel{"GRAI"});
	if (!TrainRoute(22, 3, farm_cargo, 12) || !TrainRoute(18, 22, farm_cargo, 2) ||
	    !TrainRoute(3, 21, GetCargoTypeByLabel(CargoLabel{"FOOD"}), 2) || !TrainRoute(21, 1, GetCargoTypeByLabel(CargoLabel{"FOOD"}), 2))
		return false;
	if (!TrainRoute(1, 20, GetCargoTypeByLabel(CargoLabel{"PASS"}), 4) || !TrainRoute(20, 1, GetCargoTypeByLabel(CargoLabel{"PASS"}), 4))
		return false;
	if (!TrainRoute(22, 3, farm_cargo, 12, 1)) return false;
	for (auto [x, y, name] : {std::tuple{120u, 60u, "START: City growth"}, std::tuple{100u, 198u, "CST prefab test area"},
	         std::tuple{70u, 60u, "HQ: research and stockpiles"}}) {
		if (!Result(ExtractCommandCost(Command<Commands::PlaceSign>::Do(DoCommandFlag::Execute, TileXY(x, y), std::string(name))),
		        "guide sign"))
			return false;
	}
	UpdateSignalsInBuffer();
	_pause_mode.Set(PauseMode::Normal);
	return true;
}

/**
 * Observe cargo, vehicles, facilities, town growth and research without mutation.
 * @return JSON snapshot of the current fixture.
 */
Json Snapshot()
{
	Json r = {{"tick", TimerGameTick::counter}, {"year", TimerGameCalendar::year.base()}, {"month", TimerGameCalendar::month},
	    {"date", TimerGameCalendar::date.base()}, {"money", static_cast<int64_t>(Company::Get(CompanyID{0})->money)}};
	std::array<uint64_t, NUM_CARGO> held{};
	for (const Station *st : Station::Iterate())
		for (CargoType c{0}; c < NUM_CARGO; ++c)
			held[c] += st->goods[c].TotalCount();
	r["trains"] = Json::array();
	for (const Train *t : Train::Iterate()) {
		if (t->cargo_type < NUM_CARGO) held[t->cargo_type] += t->cargo.StoredCount();
		if (!t->IsFrontEngine()) continue;
		uint units = 0;
		std::set<CargoType> types;
		for (const Train *v = t; v != nullptr; v = v->Next()) {
			units += v->cargo.StoredCount();
			if (v->cargo_cap > 0) types.insert(v->cargo_type);
		}
		r["trains"].push_back({{"id", t->index.base()}, {"name", t->name}, {"tile", t->tile.base()}, {"cargo", units},
		    {"cargo_types", types}, {"speed", t->cur_speed}, {"direction", static_cast<uint>(t->direction)},
		    {"order", t->current_order.GetDestination().base()}, {"lost", t->vehicle_flags.Test(VehicleFlag::PathfinderLost)},
		    {"stopped", t->vehstatus.Test(VehState::Stopped)}, {"crashed", t->vehstatus.Test(VehState::Crashed)}});
	}
	r["facilities"] = Json::array();
	for (const auto &f : ProductionChainManager::GetAllFacilities()) {
		r["facilities"].push_back({{"id", f.id}, {"recipe", f.recipe_id}, {"batches", f.total_produced}, {"inputs", f.input_buffers},
		    {"outputs", f.output_buffers}});
		for (auto [c, n] : f.input_buffers)
			held[c] += n;
		for (auto [c, n] : f.output_buffers)
			held[c] += n;
	}
	r["stocks"] = Json::array();
	for (const auto &s : StockpileManager::GetAllStockpiles()) {
		for (auto [c, n] : s.inventory)
			held[c] += n;
		r["stocks"].push_back({{"world", s.world_id.base()}, {"inventory", s.inventory}});
	}
	r["held"] = held;
	r["research"] = Json::array();
	for (const auto &tech : TechTreeManager::GetAllCompanyTechStates())
		r["research"].push_back({{"project", tech.active_project}, {"rp", tech.accumulated_rp}, {"budget", tech.monthly_budget},
		    {"unlocked", tech.unlocked_techs}});
	r["cities"] = Json::array();
	for (const auto &m : MegacityManager::GetAllMegacities()) {
		const Town *t = Town::Get(m.town_id);
		r["cities"].push_back(
		    {{"id", m.town_id.base()}, {"population", t->cache.population}, {"houses", t->cache.num_houses}, {"quotas", m.monthly_quota},
		        {"last", m.delivered_last}, {"current", m.delivered_current}, {"state", static_cast<uint>(m.growth_state)}});
	}
	r["stations"] = Json::array();
	for (size_t i = 0; i < nodes.size(); ++i)
		r["stations"].push_back(
		    {{"id", Stop(i).base()}, {"name", nodes[i].name}, {"consumer", MegacityManager::IsConsumerStation(Station::Get(Stop(i)))}});
	return r;
}
} // namespace

bool RepairConnectedEconomyTerrain()
{
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (Map::SizeX() != 1024 || Map::SizeY() != 1024 || company == nullptr || company->name != DEMO_NAME ||
		PlanetManager::Count() != 4) return true;
	return SmoothTerrain();
}

bool SmoothExposedUATTerrain()
{
	return SmoothTerrain();
}

bool ConConnectedEconomy(std::span<std::string_view> argv)
{
	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "connected_economy prepare|status|audit|advance|stop-food|start-food|fabricate|research: isolated demo only");
		return true;
	}
	if (_game_mode != GameMode::Normal || (_networking && (!_network_dedicated || NetworkClientInfo::GetNumItems() > 1)) ||
	    CommonwealthPackManager::GetContentStatus().mode != CommonwealthContentMode::Active) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL requires isolated game with active Commonwealth content");
		return true;
	}
	if (argv[1] == "prepare") {
		if (Prepare()) IConsolePrint(CC_DEFAULT, "CONNECTED prepared");
		return true;
	}
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (company == nullptr || company->name != DEMO_NAME || PlanetManager::Count() != 4) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL not a connected fixture");
		return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (argv[1] == "audit") {
		Json result = {{"invalid_slopes", Json::array()}, {"cargo", Json::array()}};
		for (uint y = 0; y < Map::MaxY(); ++y) {
			for (uint x = 0; x < Map::MaxX(); ++x) {
				TileIndex tile = TileXY(x, y);
				int n = TileHeight(tile), w = TileHeight(TileXY(x + 1, y));
				int e = TileHeight(TileXY(x, y + 1)), s = TileHeight(TileXY(x + 1, y + 1));
				if (abs(n - w) > 1 || abs(n - e) > 1 || abs(s - w) > 1 || abs(s - e) > 1) {
					result["invalid_slopes"].push_back({x, y, n, w, e, s, static_cast<uint>(GetTileSlope(tile))});
				}
			}
		}
		for (CargoType c{0}; c < NUM_CARGO; ++c) {
			const CargoSpec &cs = *CargoSpec::Get(c);
			if (!cs.IsValid()) continue;
			result["cargo"].push_back({{"id", cs.Index()}, {"name", GetString(cs.name)},
				{"single", GetString(cs.name_single)}, {"units", GetString(cs.units_volume, 100)},
				{"quantity", GetString(cs.quantifier, 100)}, {"zero_quantity", GetString(cs.quantifier, 0)},
				{"large_quantity", GetString(cs.quantifier, 100000)}, {"abbreviation", GetString(cs.abbrev)}});
		}
		result["language"] = GetCurrentLanguageIsoCode();
		result["pixel_queries"] = 0;
		result["viewport_queries"] = 0;
		if (result["invalid_slopes"].empty()) {
			uint64_t pixels = 0, viewports = 0;
			for (uint y = 0; y < Map::MaxY(); ++y) {
				for (uint x = 0; x < Map::MaxX(); ++x) {
					for (int offset : {0, 8, 15}) {
						GetSlopePixelZ(x * TILE_SIZE + offset, y * TILE_SIZE + offset);
						++pixels;
					}
					Point screen = RemapCoords2(x * TILE_SIZE + 8, y * TILE_SIZE + 8);
					InverseRemapCoords2(screen.x, screen.y, true);
					++viewports;
				}
			}
			result["pixel_queries"] = pixels;
			result["viewport_queries"] = viewports;
		}
		IConsolePrint(CC_DEFAULT, "CONNECTED audit {}", result.dump());
	} else if (argv[1] == "status")
		IConsolePrint(CC_DEFAULT, "CONNECTED state {}", Snapshot().dump());
	else if (argv[1] == "advance") {
		Json before = Snapshot();
		CommonwealthSliceAudit audit;
		AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
		AutoRestoreBackup pause(_pause_mode);
		AutoRestoreBackup tick_owner(_current_company, _local_company);
		_pause_mode.Reset();
		for (uint i = 0; i < 2048; ++i)
			StateGameLoop();
		Json after = Snapshot();
		std::array<int64_t, NUM_CARGO> net{};
		for (size_t i = 0; i < after["facilities"].size(); ++i) {
			const auto &f = after["facilities"][i];
			uint64_t batches = f["batches"].get<uint64_t>() - before["facilities"][i]["batches"].get<uint64_t>();
			const auto *recipe = ProductionChainManager::GetRecipe(f["recipe"].get<RecipeID>());
			for (auto [c, n] : recipe->inputs)
				net[c] -= batches * n;
			for (auto [c, n] : recipe->outputs)
				net[c] += batches * n;
		}
		after["conserved"] = true;
		after["errors"] = Json::array();
		for (CargoType c{0}; c < NUM_CARGO; ++c) {
			int64_t expected = before["held"][c].get<int64_t>() + audit.produced[c] - audit.unallocated[c] - audit.discarded[c] -
			                   audit.consumed[c] + net[c];
			if (expected != after["held"][c].get<int64_t>()) {
				after["conserved"] = false;
				after["errors"].push_back({c, expected, after["held"][c]});
			}
		}
		after["deliveries"] = audit.deliveries;
		after["vehicle_deliveries"] = audit.vehicle_deliveries;
		after["research_consumed"] = audit.research_consumed;
		IConsolePrint(CC_DEFAULT, "CONNECTED advance {}", after.dump());
	} else if (argv[1] == "fabricate") {
		const Blueprint *bp = BlueprintManager::FindBuiltin("CST Mainline Double Straight");
		if (bp == nullptr) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL missing CST prefab");
			return true;
		}
		Json before = Snapshot();
		std::array<uint64_t, NUM_CARGO> required{};
		for (const auto &tile : bp->tiles) {
			for (auto [c, n] : FabricationManager::GetTrackBOM(RAILTYPE_RAIL).materials)
				required[c] += n * tile.trackbits.Count();
			for (auto [c, n] : FabricationManager::GetSignalBOM().materials)
				required[c] += n * tile.signals.size();
		}
		if (!Result(Command<Commands::SetFabricationMode>::Do(DoCommandFlag::Execute, true), "in-kind mode")) return true;
		auto quote = Command<Commands::PlaceBlueprint>::Do({}, TileXY(100, 200), bp->ToJson(), RAILTYPE_RAIL, false);
		if (!Result(quote, "CST material quote")) return true;
		auto cost = Command<Commands::PlaceBlueprint>::Do(DoCommandFlag::Execute, TileXY(100, 200), bp->ToJson(), RAILTYPE_RAIL, false);
		if (!Result(cost, "CST fabrication")) return true;
		Json after = Snapshot();
		bool exact = cost.GetCost() == quote.GetCost();
		for (CargoType c{0}; c < NUM_CARGO; ++c)
			exact &= before["held"][c].get<uint64_t>() - after["held"][c].get<uint64_t>() == required[c];
		for (const auto &tile : bp->tiles)
			exact &= IsPlainRailTile(TileXY(100 + tile.dx, 200 + tile.dy));
		if (!Result(Command<Commands::SetFabricationMode>::Do(DoCommandFlag::Execute, false), "cash mode")) return true;
		IConsolePrint(CC_DEFAULT, "CONNECTED fabricated {}",
		    Json({{"exact", exact}, {"required", required}, {"quote", static_cast<int64_t>(quote.GetCost())},
		             {"cost", static_cast<int64_t>(cost.GetCost())}})
		        .dump());
	} else if (argv[1] == "research") {
		TechID project = TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_PORTAL_1) ? TECH_PORTAL_2 : TECH_PORTAL_1;
		if (!Result(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, project), "research demonstration")) return true;
		IConsolePrint(CC_DEFAULT, "CONNECTED research selected");
	} else if (argv[1] == "stop-food" || argv[1] == "start-food") {
		bool stop = argv[1] == "stop-food";
		for (Train *t : Train::Iterate())
			if (t->IsFrontEngine() && t->name == "Food to megacity" && t->Next() != nullptr &&
			    t->Next()->cargo_type == GetCargoTypeByLabel(CargoLabel{"FOOD"}) && t->vehstatus.Test(VehState::Stopped) != stop) {
				if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, t->index, true), "food service")) return true;
			}
		IConsolePrint(CC_DEFAULT, "CONNECTED food {}", stop ? "stopped" : "started");
	} else
		IConsolePrint(CC_ERROR, "CONNECTED FAIL unknown action");
	return true;
}
