/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_world0_routes.cpp Native fixed corridor movement; no authored scenario or cargo grants. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/world0_scenario.h"
#include "../portal/world0_opening.h"
#include "../portal/first_permanent_line.h"
#include "../portal/integrated_economy.h"
#include "../portal/planet_manager.h"
#include "../portal/logistics_hub.h"
#include "../portal/resource_sites.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../command_func.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../town.h"
#include "../town_cmd.h"
#include "../road_cmd.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../train.h"
#include "../vehicle_cmd.h"
#include "../order_cmd.h"
#include "../order_base.h"
#include "../economy_func.h"
#include "../strings_func.h"
#include "../vehicle_func.h"
#include "../core/backup_type.hpp"
#include "../timer/timer_game_calendar.h"
#include "../safeguards.h"
extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
namespace {
void Setup()
{
	World0Opening::Reset(); FirstPermanentLine::Reset(); IntegratedEconomy::Reset(); ResourceSiteManager::Reset(); LogisticsHubManager::Reset();
	SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 2000);
	UpdateSignalsInBuffer(); Map::Allocate(256, 256);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) { SetTileHeight(tile, 2); if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3); else MakeVoid(tile); }
	PlanetManager::Reset(); PlanetRegion world; world.id = WorldID{0}; world.name = "Route unit fixture"; world.phase = WorldPhase::Phase3_Frontier; world.max_x = world.max_y = 255; PlanetManager::RegisterRegion(world);
	RebuildTownKdtree(); RebuildStationKdtree();
	Company::Get(_current_company)->money = 100000000;
	Company::Get(_current_company)->clear_limit = 1000 << 16;
	Company::Get(_current_company)->avail_railtypes.Set(RAILTYPE_RAIL);
	ResetRailTypes(); StationClass::Reset();
	_settings_game.station.station_spread = 12;
	for (Town *town : Town::Iterate()) town->ratings.fill(RATING_INITIAL);
	_game_mode = GameMode::Normal;
}
}
TEST_CASE("Vale town foundations require clear native centers beside their public roads", "[world0-routes]")
{
	Setup(); AutoRestoreBackup deity(_current_company, OWNER_DEITY); AutoRestoreBackup mode(_game_mode, GameMode::Editor);
	const TileIndex center = TileXY(160, 145), neighbour = TileXY(160, 144);
	REQUIRE(Command<Commands::BuildRoad>::Do(DoCommandFlag::Execute, neighbour, ROAD_Y, ROADTYPE_ROAD, {}, TownID::Invalid()).Succeeded());
	auto [clear_cost, money, town] = Command<Commands::FoundTown>::Do({}, center, TownSize::Large, true, TownLayout::BetterRoads, false, 0, std::string("Clear Greybank fixture"));
	CHECK(clear_cost.Succeeded());
	REQUIRE(Command<Commands::BuildRoad>::Do(DoCommandFlag::Execute, center, ROAD_Y, ROADTYPE_ROAD, {}, TownID::Invalid()).Succeeded());
	auto [road_cost, road_money, road_town] = Command<Commands::FoundTown>::Do({}, center, TownSize::Large, true, TownLayout::BetterRoads, false, 0, std::string("Road Greybank fixture"));
	CHECK(road_cost.Failed());
}
TEST_CASE("Vale ordinary corridors carry a native train out and back with 90-degree restriction", "[world0-routes]")
{
	const uint route = GENERATE(0u, 1u);
	Setup();
	const TileIndex source = route == 0 ? TileXY(48, 70) : TileXY(104, 123), destination = route == 0 ? TileXY(96, 116) : TileXY(147, 146);
	const TileIndex depot = route == 0 ? TileXY(46, 70) : TileXY(102, 123);
	for (TileIndex tile : {source, destination}) {
		auto cost = Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Axis::X, 1, 4, STAT_CLASS_DFLT, 0, NEW_STATION, true);
		INFO("station tile=" << tile.base() << " error=" << cost.GetErrorMessage().base());
		REQUIRE(cost.Succeeded());
	}
	for (auto [tile, track] : GetWorld0RailRoute(route)) REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, track, false).Succeeded());
	REQUIRE(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, DiagDirection::SW).Succeeded());
	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	SetupCargoForClimate(LandscapeType::Temperate); _engine_mngr.ResetToDefaultMapping(); SetupEngines();
	for (Engine *engine : Engine::Iterate()) if (const CargoLabel *label = std::get_if<CargoLabel>(&engine->info.cargo_label)) { CargoType cargo = GetCargoTypeByLabel(*label); if (IsValidCargoType(cargo)) engine->info.cargo_type = cargo; }
	StartupEngines(); _settings_game.vehicle.max_trains = 10; _settings_game.vehicle.max_train_length = 10;
	_settings_game.pf.path_backoff_interval = 1; _settings_game.pf.forbid_90_deg = true;
	auto [cost, id, capacity, mail, capacities] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineID{0}, false, INVALID_CARGO, ClientID::Invalid);
	REQUIRE(cost.Succeeded()); Train *train = Train::Get(id);
	for (uint8_t n = 0; n < 2; ++n) {
		Order order; order.MakeGoToStation(GetStationIndex(n == 0 ? destination : source)); order.SetLoadType(OrderLoadType::NoLoad); order.SetUnloadType(OrderUnloadType::NoUnload);
		REQUIRE(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, id, VehicleOrderID{n}, order).Succeeded());
	}
	train->current_order = *train->GetOrder(0); train->dest_tile = destination;
	REQUIRE(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, id, false).Succeeded());
	bool out = false, back = false, alive = true; uint ticks = 0;
	for (; ticks < 20000 && !back; ++ticks) {
		if (!(alive = train->Tick())) break;
		for (Station *station : Station::Iterate()) LoadUnloadStation(station);
		UpdateSignalsInBuffer();
		if (train->current_order.IsType(OT_LOADING)) {
			if (train->last_station_visited == GetStationIndex(destination)) out = true;
			if (out && train->last_station_visited == GetStationIndex(source)) back = true;
		}
		if (train->vehstatus.Test(VehState::Crashed) || train->vehicle_flags.Test(VehicleFlag::PathfinderLost)) break;
	}
	INFO("route=" << route << " ticks=" << ticks << " tile=" << train->tile.base());
	CHECK(alive); CHECK(out); CHECK(back); CHECK_FALSE(train->vehstatus.Test(VehState::Crashed)); CHECK_FALSE(train->vehicle_flags.Test(VehicleFlag::PathfinderLost));
}
