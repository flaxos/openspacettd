/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file commonwealth_slice.cpp Bounded offline acceptance using real commands and simulation ticks. */
#include "../stdafx.h"
#include "commonwealth_slice.h"
#include "commonwealth_pack.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_cmd.h"
#include "logistics_hub.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../industry_cmd.h"
#include "../industrytype.h"
#include "../newgrf_industries.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../rail.h"
#include "../table/strings.h"
#include "../timer/timer_game_calendar.h"
#include "../signal_func.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../station_base.h"
#include "../vehicle_cmd.h"
#include "../train_cmd.h"
#include "../order_cmd.h"
#include "../order_base.h"
#include "../train.h"
#include "../clear_map.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../network/network.h"
#include "../network/network_base.h"
#include "../core/backup_type.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include "../strings_func.h"
#include "../safeguards.h"

/** Active audit collector for the bounded WP11 slice run. */
CommonwealthSliceAudit *_commonwealth_slice_audit = nullptr;
/** Marker name used to identify the disposable WP11 fixture company. */
static constexpr std::string_view SLICE_NAME = "WP11 Steel Slice v1";

/**
 * Report a failed command as a WP11 console failure.
 * @param cost Command result to inspect.
 * @param action Human-readable action name for the console message.
 * @return True when the command succeeded.
 */
static bool SliceResult(const CommandCost &cost, std::string_view action)
{
	if (cost.Succeeded()) return true;
	IConsolePrint(CC_ERROR, "WP11 FAIL {}: {}", action, GetString(cost.GetErrorMessage()));
	return false;
}

/**
 * Inspect physical cargo once: station totals include reservations, so vehicles contribute stored cargo only.
 * @return JSON snapshot of trains, stations, stockpiles, facilities, cash and fixture state.
 */
static nlohmann::json SliceSnapshot()
{
	nlohmann::json result;
	std::array<uint64_t, NUM_CARGO> held{};
	for (const Station *st : Station::Iterate()) for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) held[cargo] += st->goods[cargo].TotalCount();
	result["stations"] = nlohmann::json::array();
	for (const Station *station : Station::Iterate()) {
		std::array<uint64_t, NUM_CARGO> waiting{};
		for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) waiting[cargo] = station->goods[cargo].TotalCount();
		result["stations"].push_back({{"id", station->index.base()}, {"owner", station->owner.base()}, {"cargo", waiting}});
	}
	result["transit"] = PortalRegistry::GetAllVehicleTransit();
	result["trains"] = nlohmann::json::array();
	for (const Train *train : Train::Iterate()) {
		if (train->cargo_type < NUM_CARGO) held[train->cargo_type] += train->cargo.StoredCount();
		result["trains"].push_back({{"id", train->index.base()}, {"x", TileX(train->tile)}, {"y", TileY(train->tile)},
			{"cargo", train->cargo.StoredCount()}, {"speed", train->cur_speed}, {"front", train->IsFrontEngine()}});
	}
	for (const auto &stock : StockpileManager::GetAllStockpiles()) for (const auto &[cargo, amount] : stock.inventory) if (cargo < NUM_CARGO) held[cargo] += amount;
	uint64_t batches = 0;
	result["facilities"] = nlohmann::json::array();
	for (const auto &facility : ProductionChainManager::GetAllFacilities()) {
		batches += facility.total_produced;
		result["facilities"].push_back({{"id", facility.id}, {"owner", facility.owner.base()}, {"world", facility.world_id.base()},
			{"inputs", facility.input_buffers}, {"outputs", facility.output_buffers}, {"batches", facility.total_produced}});
		for (const auto &[cargo, amount] : facility.input_buffers) if (cargo < NUM_CARGO) held[cargo] += amount;
		for (const auto &[cargo, amount] : facility.output_buffers) if (cargo < NUM_CARGO) held[cargo] += amount;
	}
	result["industries"] = nlohmann::json::array();
	for (const Industry *industry : Industry::Iterate()) for (const auto &p : industry->produced) {
		result["industries"].push_back({{"cargo", p.cargo}, {"rate", p.rate}, {"waiting", p.waiting}, {"near", industry->stations_near.size()}, {"counter", industry->counter}});
	}
	result["held"] = held;
	result["batches"] = batches;
	const Company *company = Company::GetIfValid(CompanyID{0});
	result["money"] = company == nullptr ? 0 : static_cast<int64_t>(company->money);
	result["expenses"] = nlohmann::json::array();
	if (company != nullptr) for (const auto &year : company->yearly_expenses) for (Money expense : year) result["expenses"].push_back(static_cast<int64_t>(expense));
	result["steel_stock"] = StockpileManager::GetStock(WorldID{2}, CompanyID{0}, ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel));
	result["ballast_stock"] = StockpileManager::GetStock(WorldID{2}, CompanyID{0}, ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StoneSlag));
	result["deposited"] = 0;
	for (const auto &hub : LogisticsHubManager::GetAllHubs()) result["deposited"] = result["deposited"].get<uint64_t>() + hub.total_deposited;
	result["depot"] = IsRailDepotTile(TileXY(283, 45));
	return result;
}

/**
 * Create the disposable three-world WP11 fixture in a fresh empty game.
 * @return True when the fixture was prepared.
 */
static bool PrepareSlice()
{
	if (Map::SizeX() != 512 || Map::SizeY() != 128 || Company::GetNumItems() != 0 ||
			Industry::GetNumItems() != 0 || Vehicle::GetNumItems() != 0 || PlanetManager::Count() > 1 || PortalRegistry::Count() != 0) {
		IConsolePrint(CC_ERROR, "WP11 FAIL setup requires fresh empty 512x128: actual {}x{}, companies {}, industries {}, vehicles {}, worlds {}", Map::SizeX(), Map::SizeY(), Company::GetNumItems(), Industry::GetNumItems(), Vehicle::GetNumItems(), PlanetManager::Count());
		return false;
	}
	/* Only this disposable fixture's initial landscape is levelled. Refuse to overwrite developed tiles. */
	for (uint y = 25; y <= 85; ++y) for (uint x = 5; x <= 310; ++x) {
		auto type = GetTileType(TileXY(x, y));
		if (type != TileType::Clear && type != TileType::Trees && type != TileType::Water) {
			IConsolePrint(CC_ERROR, "WP11 FAIL initial construction pad contains developed tiles");
			return false;
		}
	}
	for (uint y = 25; y <= 85; ++y) for (uint x = 5; x <= 310; ++x) {
		TileIndex tile = TileXY(x, y);
		MakeClear(tile, ClearGround::Grass, 3);
		SetTileHeight(tile, 1);
	}
	extern Company *DoStartupNewCompany(bool is_ai, CompanyID company);
	Company *company = DoStartupNewCompany(false, CompanyID{0});
	if (company == nullptr) return false;
	company->name = SLICE_NAME;
	company->money = 100000000;
	company->clear_limit = 10000 << 16;
	company->terraform_limit = 10000 << 16;
	company->avail_railtypes.Set(RAILTYPE_RAIL);
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	PlanetManager::Reset();
	for (uint16_t i = 0; i < 3; ++i) {
		PlanetRegion region{.id = WorldID{i}, .name = i == 0 ? "WP11 Extraction" : i == 1 ? "WP11 Refining" : "WP11 Core",
			.phase = i == 0 ? WorldPhase::Phase3_Frontier : i == 1 ? WorldPhase::Phase2_Developed : WorldPhase::Phase1_Core,
			.biome = WorldBiome::Temperate, .min_x = static_cast<uint>(i * 110 + 1), .min_y = 1,
			.max_x = static_cast<uint>(i * 110 + 100), .max_y = 126};
		if (!PlanetManager::RegisterRegion(region)) return false;
	}
	TechTreeManager::RestoreCompanyTech(company->index, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	StockpileManager::AddCargo(WorldID{2}, company->index, ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StoneSlag), 5);
	/* Bit 7 selects a GRF-local industry ID rather than a native industry type. */
	IndustryType mine = MapNewGRFIndustryType(0x80 | 0x12, COMMONWEALTH_INDUSTRY_GRFID);
	if (!SliceResult(Command<Commands::BuildIndustry>::Do(DoCommandFlag::Execute, TileXY(20, 35), mine, 0, false, 1), "iron mine")) return false;
	if (Industry::GetNumItems() != 1 || !Industry::Get(IndustryID{0})->IsCargoProduced(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre))) {
		IConsolePrint(CC_ERROR, "WP11 FAIL expected one producing Commonwealth iron mine");
		return false;
	}
	_pause_mode.Set(PauseMode::Normal);
	return true;
}

/**
 * Build the ore and steel routes, portals, stations, trains, furnace and hub.
 * @return True when the route was built and both trains were started.
 */
static bool BuildSliceRoute()
{
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0) return false;
	/* Keep portal holding lanes clear of both stations and the other route. */
	if (!SliceResult(Command<Commands::BuildPortalPair>::Do(DoCommandFlag::Execute, TileXY(70, 40), DiagDirection::SW, TileXY(112, 40), DiagDirection::NE, RAILTYPE_RAIL), "ore portal")) return false;
	if (!SliceResult(Command<Commands::BuildPortalPair>::Do(DoCommandFlag::Execute, TileXY(190, 45), DiagDirection::SW, TileXY(232, 45), DiagDirection::NE, RAILTYPE_RAIL), "steel portal")) return false;
	auto station = [](uint x, uint y, StationID join = NEW_STATION) {
		return SliceResult(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, TileXY(x, y), RAILTYPE_RAIL, Axis::X, 1, 3, STAT_CLASS_DFLT, 0, join, true), "station");
	};
	if (!station(20, 40) || !station(145, 40)) return false;
	StationID furnace = GetStationIndex(TileXY(145, 40));
	if (!station(145, 45, furnace) || !station(280, 45)) return false;
	if (!SliceResult(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, furnace, RECIPE_STEEL_SMELTING), "furnace")) return false;
	StationID hub = GetStationIndex(TileXY(280, 45));
	if (!SliceResult(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, TileXY(280, 45), hub, std::string("WP11 Core Hub")), "hub")) return false;
	auto line = [](uint start, uint end, uint y) {
		for (uint x = start; x <= end; ++x) {
			TileIndex tile = TileXY(x, y);
			if (IsTileType(tile, TileType::Station) || IsTileType(tile, TileType::Railway)) continue;
			if (!SliceResult(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, Track::X, false), "rail")) return false;
		}
		return true;
	};
	if (!line(11, 52, 40) || !line(130, 147, 40) || !line(136, 172, 45) || !line(250, 282, 45)) return false;
	for (auto [x, y] : {std::pair{10, 40}, std::pair{135, 45}}) {
		if (!SliceResult(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, TileXY(x, y), RAILTYPE_RAIL, DiagDirection::SW), "depot")) return false;
	}
	auto engine_id = [](uint local) {
		for (const Engine *engine : Engine::Iterate()) if (engine->type == VehicleType::Train && engine->grf_prop.grfid == COMMONWEALTH_RAIL_GRFID && engine->grf_prop.local_id == local) return engine->index;
		return EngineID::Invalid();
	};
	auto train = [&](TileIndex depot, uint wagon_local, CargoType cargo, StationID source, StationID dest) {
		auto [cost, locomotive, n, capacity, extra] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, engine_id(0x20), false, INVALID_CARGO, ClientID::Invalid);
		if (!SliceResult(cost, "locomotive")) return false;
		auto [wcost, wagon, wn, wc, we] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, engine_id(wagon_local), false, cargo, ClientID::Invalid);
		if (!SliceResult(wcost, "wagon")) return false;
		if (Train::Get(wagon)->First()->index != locomotive && !SliceResult(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, wagon, locomotive, false), "coupling")) return false;
		Order pickup; pickup.MakeGoToStation(source); pickup.SetLoadType(OrderLoadType::FullLoadAny);
		Order dropoff; dropoff.MakeGoToStation(dest); dropoff.SetLoadType(OrderLoadType::NoLoad);
		if (!SliceResult(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, locomotive, VehicleOrderID{0}, pickup), "pickup order")) return false;
		if (!SliceResult(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, locomotive, VehicleOrderID{1}, dropoff), "delivery order")) return false;
		return SliceResult(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, locomotive, true), "start train");
	};
	if (!train(TileXY(10, 40), 0x32, ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre), GetStationIndex(TileXY(20, 40)), furnace)) return false;
	return train(TileXY(135, 45), 0x33, ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel), furnace, hub);
}

/** Console-command entry point for the offline WP11 slice harness. */
bool ConCommonwealthSlice(std::span<std::string_view> argv)
{
	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "wp11_slice prepare|build|advance|status|availability|fabricate|block|unblock: disposable offline acceptance only");
		return true;
	}
	if (_game_mode != GameMode::Normal || (_networking && (!_network_dedicated || NetworkClientInfo::GetNumItems() > 1))) {
		IConsolePrint(CC_ERROR, "WP11 FAIL requires an offline game or an isolated dedicated server"); return true;
	}
	if (CommonwealthPackManager::GetContentStatus().mode != CommonwealthContentMode::Active) {
		IConsolePrint(CC_ERROR, "WP11 FAIL requires valid active Commonwealth content"); return true;
	}
	if (argv[1] == "prepare") {
		if (PrepareSlice()) IConsolePrint(CC_DEFAULT, "WP11 prepared");
		return true;
	}
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (company == nullptr || company->name != SLICE_NAME || PlanetManager::Count() != 3) {
		IConsolePrint(CC_ERROR, "WP11 FAIL this is not the disposable WP11 fixture"); return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (argv[1] == "build") {
		if (BuildSliceRoute()) IConsolePrint(CC_DEFAULT, "WP11 built");
	} else if (argv[1] == "status") {
		IConsolePrint(CC_DEFAULT, "WP11 state {}", SliceSnapshot().dump());
	} else if (argv[1] == "advance") {
		const auto before = SliceSnapshot();
		CommonwealthSliceAudit audit;
		AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
		AutoRestoreBackup pause(_pause_mode);
		AutoRestoreBackup tick_owner(_current_company, _local_company);
		_pause_mode.Reset();
		UpdateSignalsInBuffer();
		for (uint i = 0; i < 1024; ++i) StateGameLoop();
		auto after = SliceSnapshot();
		int64_t batches = after["batches"].get<int64_t>() - before["batches"].get<int64_t>();
		bool conserved = true;
		CargoType iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
		CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
		for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) {
			int64_t transformed = cargo == iron ? -2 * batches : cargo == steel ? batches : 0;
			int64_t expected = before["held"][cargo].get<int64_t>() + audit.produced[cargo] - audit.unallocated[cargo] - audit.discarded[cargo] + transformed;
			if (expected != after["held"][cargo].get<int64_t>()) conserved = false;
		}
		after["cash_debits"] = audit.cash_debits;
		after["cash_conserved"] = before["money"].get<int64_t>() - after["money"].get<int64_t>() == audit.cash_debits;
		after["conserved"] = conserved;
		after["produced"] = audit.produced;
		after["unallocated"] = audit.unallocated;
		after["discarded"] = audit.discarded;
		IConsolePrint(conserved ? CC_DEFAULT : CC_ERROR, "WP11 advance {}", after.dump());
	} else if (argv[1] == "availability") {
		EngineID vulcan = EngineID::Invalid();
		for (const Engine *engine : Engine::Iterate()) if (engine->type == VehicleType::Train &&
				engine->grf_prop.grfid == COMMONWEALTH_RAIL_GRFID && engine->grf_prop.local_id == 0x21) vulcan = engine->index;
		auto query = [&](TileIndex tile) {
			return ExtractCommandCost(Command<Commands::BuildVehicle>::Do({}, tile, vulcan, false, INVALID_CARGO, ClientID::Invalid));
		};
		bool passed = query(TileXY(10, 40)).GetErrorMessage() == STR_ERROR_COMMONWEALTH_RESEARCH;
		TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_TRACTION_1});
		passed &= query(TileXY(10, 40)).Succeeded();
		passed &= query(TileXY(135, 45)).GetErrorMessage() == STR_ERROR_COMMONWEALTH_PHASE;
		TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
		passed &= query(TileXY(10, 40)).GetErrorMessage() == STR_ERROR_COMMONWEALTH_RESEARCH;
		IConsolePrint(CC_DEFAULT, "WP11 availability {}", nlohmann::json({{"passed", passed}, {"year", TimerGameCalendar::year.base()}}).dump());
	} else if (argv[1] == "fabricate") {
		auto before = SliceSnapshot();
		CommandCost cash_quote = Command<Commands::BuildRailDepot>::Do({}, TileXY(283, 45), RAILTYPE_RAIL, DiagDirection::NE);
		FabricationManager::SetFabricateFromStockpile(CompanyID{0}, true);
		CommandCost quote = Command<Commands::BuildRailDepot>::Do({}, TileXY(283, 45), RAILTYPE_RAIL, DiagDirection::NE);
		CommandCost cost = Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, TileXY(283, 45), RAILTYPE_RAIL, DiagDirection::NE);
		FabricationManager::SetFabricateFromStockpile(CompanyID{0}, false);
		if (!SliceResult(cost, "fabrication")) return true;
		auto after = SliceSnapshot();
		bool exact = before["steel_stock"].get<int64_t>() - after["steel_stock"].get<int64_t>() == 10 &&
			before["ballast_stock"].get<int64_t>() - after["ballast_stock"].get<int64_t>() == 5 && cost.GetCost() == quote.GetCost() && cost.GetCost() == cash_quote.GetCost() - _price[Price::BuildDepotTrain] - RailBuildCost(RAILTYPE_RAIL) +
			_price[Price::BuildDepotTrain] * 20 / 100 + RailBuildCost(RAILTYPE_RAIL) * 20 / 100 &&
			before["money"].get<int64_t>() - after["money"].get<int64_t>() == static_cast<int64_t>(cost.GetCost());
		after["exact"] = exact;
		after["discount_percent"] = FabricationManager::GetBOMDiscountPercent(CompanyID{0});
		after["cash_quote"] = static_cast<int64_t>(cash_quote.GetCost());
		after["cost"] = static_cast<int64_t>(cost.GetCost());
		IConsolePrint(exact ? CC_DEFAULT : CC_ERROR, "WP11 fabricated {}", after.dump());
	} else if (argv[1] == "block" || argv[1] == "unblock") {
		bool block = argv[1] == "block";
		CommandCost cost = block ? Command<Commands::RemoveRail>::Do(DoCommandFlag::Execute, TileXY(260, 45), Track::X) :
			Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, TileXY(260, 45), RAILTYPE_RAIL, Track::X, false);
		if (SliceResult(cost, "obstruction")) IConsolePrint(CC_DEFAULT, "WP11 {}", block ? "blocked" : "unblocked");
	} else {
		IConsolePrint(CC_ERROR, "WP11 FAIL unknown action");
	}
	return true;
}
