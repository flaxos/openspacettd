/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file connected_economy.cpp Reproducible offline connected production and megacity acceptance fixture. */
#include "../stdafx.h"
#include "connected_economy.h"
#include "integrated_economy.h"
#include "../economy_func.h"
#include "../economy_base.h"
#include "commonwealth_slice.h"
#include "commonwealth_pack.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_cmd.h"
#include "megacity_manager.h"
#include "corporate_hq.h"
#include "logistics_hub.h"
#include "../command_func.h"
#include "../settings_cmd.h"
#include "../console_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../engine_func.h"
#include "../newgrf_engine.h"
#include "../video/video_driver.hpp"
#include "../progress.h"
#include "../industry_cmd.h"
#include "../newgrf_industries.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../road_map.h"
#include "../rail.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../depot_base.h"
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
#include "../town_map.h"
#include "../clear_map.h"
#include "../landscape.h"
#include "../void_map.h"
#include "../map_func.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../strings_func.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "../timer/timer_game_economy.h"
#include "../openttd.h"
#include "../network/network.h"
#include "../network/network_base.h"
#include "../core/backup_type.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include <queue>
#include <fstream>
#include "../newgrf_config.h"
#include "resource_sites.h"
#include "stellar_network.h"
#include "portal_terminal.h"
#include "../company_cmd.h"
#include "../misc_cmd.h"
#include "../tunnelbridge_cmd.h"
#include "../tunnelbridge_map.h"
#include "../bridge_map.h"
#include "../direction_func.h"
#include "../core/string_consumer.hpp"
#include "../core/random_func.hpp"
#include "../pathfinder/follow_track.hpp"
#include "../game/game.hpp"

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
const std::vector<DemoNode> legacy_nodes = {{"HQ warehouse", 70},
											{"Megacity consumers", 116},
											{"Consumer crystals", 168, RECIPE_CONSUMER_CRYSTAL_FORMAT},
											{"Food works", 148},
											{"Ballast crusher", 300, RECIPE_BALLAST_CRUSHING},
											{"Steel furnace", 318, RECIPE_STEEL_SMELTING},
											{"Alloy foundry", 336, RECIPE_SUPERALLOY_FOUNDRY},
											{"Copper works", 354, RECIPE_COPPER_SMELTING},
											{"Silicon works", 372, RECIPE_SILICON_ARC},
											{"Signal works", 390, RECIPE_SIGNALLING_ASSEMBLY},
											{"Polymer works", 408, RECIPE_POLYMER_SYNTHESIS},
											{"Propulsion works", 426, RECIPE_MAGLEV_WORKS},
											{"Blank crystal fab", 444, RECIPE_MONOCRYSTAL_SYNTHESIS},
											{"Stone quarry", 560},
											{"Iron mine", 590},
											{"Copper mine", 620},
											{"Silica dunes", 650},
											{"Rare earth mine", 680},
											{"Farm", 710},
											{"Quantum observatory", 820, RECIPE_QUANTUM_ENRICHMENT},
											{"Frontier passengers", 875},
											{"City distribution", 132},
											{"Farm export warehouse", 730},
											{"Stone export", 576},
											{"Iron export", 606},
											{"Copper export", 636},
											{"Silica export", 666},
											{"Rare earth export", 696}};
std::vector<DemoNode> nodes = legacy_nodes;
void ConfigureIntegratedNodes()
{
	nodes = legacy_nodes;
	if (!IntegratedEconomy::Enabled()) return;
	nodes[3] = {"Food works", 450, 404};
	for (uint i = 4; i <= 12; ++i)
		nodes[i].x = 300 + 15 * (i - 4);
	nodes.push_back({"Oil wells", 736});
	nodes.push_back({"Machine assembly", 435, RECIPE_MACHINE_MODULES});
	nodes.push_back({"Industrial warehouse", 282});
	nodes.push_back({"Research frontier warehouse", 850});
}
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
std::vector<std::vector<std::pair<uint, uint>>> integrated_corridors;
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
	if (IntegratedEconomy::Enabled()) {
		size_t row = 0;
		for (; row < integrated_corridors.size(); ++row) {
			if (std::ranges::none_of(integrated_corridors[row],
									 [left, right](auto range) { return left - 3 <= range.second + 2 && right + 16 + 2 >= range.first; }))
				break;
		}
		if (row == integrated_corridors.size()) integrated_corridors.emplace_back();
		integrated_corridors[row].push_back({left - 3, right + 16});
		/* Preserve the 64-tile offset limit before narrowing the row to coordinate arithmetic. */
		if (row >= (64 - 2) / 2) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL too many overlapping freight corridors");
			return false;
		}
		y = TOP + 2 + 2 * static_cast<uint>(row);
	}

	for (uint x : {220u, 476u, 732u})
		if (left < x && right > x + (IntegratedEconomy::Enabled() ? 58 : 0)) {
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
																IntegratedEconomy::Enabled() ? 4 : 14, STAT_CLASS_DFLT, 0, Stop(node),
																true),
						fmt::format("route platform {} at {},{} ({} -> {})", node, nodes[node].x, y, source, destination)))
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
		if (e->type != VehicleType::Train ||
			(e->grf_prop.grfid != COMMONWEALTH_RAIL_GRFID &&
			 !(IntegratedEconomy::Enabled() && e->grf_prop.grfid == COMMONWEALTH_INDUSTRY_GRFID)) ||
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
			bool livestock = !IntegratedEconomy::Enabled() && (source == 18 || source == 22) && w >= wagons / 2;
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
		if (IntegratedEconomy::Enabled()) pickup.SetLoadType(OrderLoadType::FullLoad);
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

/** Build a real factory or extraction site through the same command used by Fund Industry. */
bool FundIntegratedNode(size_t node, IndustryType type)
{
	if (type == IT_INVALID) return false;
	for (auto industry : Industry::Iterate())
		if (industry->type == type && DistanceManhattan(industry->location.tile, TileXY(nodes[node].x, TOP)) < 20) return true;
	bool primary = ResourceSiteManager::IsPrimary(type);
	auto technology =
		primary ? ResourceSiteManager::RequiredTech(type) : IntegratedEconomy::RecipeTech(IntegratedEconomy::IndustryRecipe(type));
	if (technology != TECH_NONE && !TechTreeManager::IsTechUnlocked(CompanyID{0}, technology)) return false;
	if (primary) {
		TileIndex site = TileXY(nodes[node].x, TOP - 12);
		ResourceSiteManager::AddSite(site, type, 16, 24);
		auto survey = Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, site);
		if (survey.Failed()) return false;
	}
	StringID last_error = INVALID_STRING_ID;
	for (uint dy = 5; dy <= 12; ++dy)
		/* NewGRF layout counts are byte-sized; use the construction command's index type. */
		for (uint32_t layout = 0; layout < GetIndustrySpec(type)->layouts.size(); ++layout) {
			TileIndex tile = TileXY(nodes[node].x, TOP - dy);
			auto quote = Command<Commands::BuildIndustry>::Do({}, tile, type, layout, false, 1);
			if (quote.Failed()) {
				last_error = quote.GetErrorMessage();
				continue;
			}
			if (quote.GetCost() > Company::Get(CompanyID{0})->money) continue;
			auto result = Command<Commands::BuildIndustry>::Do(DoCommandFlag::Execute, tile, type, layout, false, 1);
			if (result.Failed()) return false;
			SubtractMoneyFromCompany(CompanyID{0}, result);
			return true;
		}
	if (last_error != INVALID_STRING_ID) IConsolePrint(CC_DEFAULT, "CONNECTED pending {}: {}", nodes[node].name, GetString(last_error));
	return false;
}

void ProgressIntegrated()
{
	if (StellarNetwork::Enabled() && TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_PORTAL_1)) {
		if (StellarNetwork::Projects().empty()) {
			auto result = Command<Commands::StartGateProject>::Do(DoCommandFlag::Execute, TileXY(84, 65), 1, Stop(0));
			if (result.Succeeded()) SubtractMoneyFromCompany(CompanyID{0}, result);
		}
		for (const auto &[id, project] : StellarNetwork::Projects())
			if (project.state == GateProjectState::Ready) {
				auto result = Command<Commands::OperateGateProject>::Do(DoCommandFlag::Execute, id, false);
				if (result.Succeeded()) SubtractMoneyFromCompany(CompanyID{0}, result);
			}
	}
	for (size_t node = 0; node < nodes.size(); ++node) {
		if (nodes[node].recipe == RECIPE_NONE) continue;
		for (IndustryType type = 0; type < NUM_INDUSTRYTYPES; ++type) {
			if (IntegratedEconomy::IndustryRecipe(type) == nodes[node].recipe) {
				FundIntegratedNode(node, type);
				break;
			}
		}
	}
	for (auto [node, local] : {std::pair{15u, 0x16u}, std::pair{16u, 0x18u}, std::pair{17u, 0x14u}, std::pair{18u, 0x22u}})
		FundIntegratedNode(node, MapNewGRFIndustryType(0x80 | local, COMMONWEALTH_INDUSTRY_GRFID));
	for (IndustryType type = 0; type < NUM_INDUSTRYTYPES; ++type) {
		auto spec = GetIndustrySpec(type);
		if (spec->enabled && ResourceSiteManager::IsPrimary(type) && !spec->behaviour.Test(IndustryBehaviour::BuiltOnWater) &&
			std::ranges::find(spec->produced_cargo, GetCargoTypeByLabel(CargoLabel{"OIL_"})) != spec->produced_cargo.end()) {
			FundIntegratedNode(28, type);
			break;
		}
	}
	if (TechTreeManager::GetActiveProject(CompanyID{0}) == TECH_NONE) {
		for (auto tech : {TECH_PORTAL_1, TECH_MATERIALS_2, TECH_MATERIALS_3, TECH_MATERIALS_4, TECH_TRACTION_3, TECH_PORTAL_2,
						  TECH_PORTAL_3, TECH_TRACTION_4, TECH_PORTAL_4}) {
			if (TechTreeManager::IsTechUnlocked(CompanyID{0}, tech)) continue;
			std::string error;
			if (TechTreeManager::CanResearch(CompanyID{0}, tech, error)) {
				Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, tech);
				Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 250000);
			}
			break;
		}
	}
}

bool PrepareIntegratedServices()
{
	for (size_t node : {30u, 31u})
		if (!Result(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, TileXY(nodes[node].x, TOP), Stop(node),
															 std::string(nodes[node].name)),
					"construction warehouse"))
			return false;
	ProgressIntegrated();
	struct Flow {
		size_t from;
		size_t to;
		const char *label;
	};
	const Flow flows[] = {{13, 4, "SILC"},	{14, 5, "IRON"},  {15, 7, "COPR"}, {16, 8, "SAND"}, {18, 3, "GRAI"},  {28, 10, "OIL_"},
						  {4, 0, "BALL"},	{4, 1, "BALL"},	  {5, 0, "STEL"},  {5, 1, "STEL"},	{5, 6, "STEL"},	  {5, 29, "STEL"},
						  {5, 30, "STEL"},	{5, 31, "STEL"},  {7, 8, "WIRE"},  {7, 9, "WIRE"},	{7, 29, "WIRE"},  {7, 11, "WIRE"},
						  {7, 0, "WIRE"},	{8, 0, "CHIP"},	  {8, 1, "CHIP"},  {8, 9, "CHIP"},	{8, 29, "CHIP"},  {29, 0, "MACH"},
						  {29, 30, "MACH"}, {29, 31, "MACH"}, {9, 0, "SIGE"},  {17, 6, "RARE"}, {17, 12, "RARE"}, {16, 12, "SAND"},
						  {6, 11, "ALLO"},	{6, 0, "ALLO"},	  {10, 0, "POLY"}, {11, 0, "MGLA"}, {12, 0, "BCRY"},  {12, 2, "BCRY"},
						  {12, 19, "BCRY"}, {19, 0, "QCRY"},  {2, 1, "CCRY"},  {3, 1, "FOOD"}};
	for (const auto &flow : flows) {
		CargoLabel label;
		std::copy_n(flow.label, 4, label.begin());
		uint wagons = (flow.from == 3 || (flow.from >= 13 && flow.from <= 18) || flow.from == 28) ? 6 : 2;
		if (!TrainRoute(flow.from, flow.to, GetCargoTypeByLabel(label), wagons)) return false;
	}
	for (uint i = 0; i < 4; ++i)
		StellarNetwork::RegisterWorld({WorldID{i}, fmt::format("uat-{}", i), fmt::format("UAT world {}", i), int32_t(i * 3), 0, true});
	StellarNetwork::RegisterZone({1, WorldID{3}, TileXY(900, 200), DiagDirection::NE});
	if (!Result(Command<Commands::BuildPortalGate>::Do(DoCommandFlag::Execute, TileXY(84, 65), DiagDirection::SW, RAILTYPE_RAIL),
				"project source gate"))
		return false;
	return true;
}

/**
 * Construct the isolated four-world fixture on a fresh map.
 * @return Whether all construction and configuration commands succeeded.
 */
bool Prepare(bool resources = false, bool integrated = false)
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
	ResourceSiteManager::Reset();
	ResourceSiteManager::SetEnabled(resources);
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
	if (integrated) {
		IntegratedEconomy::Reset();
		if (!IntegratedEconomy::StartNewGame()) return false;
	}
	ConfigureIntegratedNodes();
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
	integrated_corridors.clear();
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
		if (node.recipe != 0 && !integrated) {
			if (!Result(Command<Commands::BuildProcessingFacility>::Do(DoCommandFlag::Execute, Stop(i), node.recipe), node.name))
				return false;
			if (!Result(Command<Commands::SetFacilityPlatformCapacity>::Do(DoCommandFlag::Execute, Stop(i), 4096), "factory rail dispatch"))
				return false;
		}
	}
	if (resources) {
		if (!Result(Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, TileXY(70, 60), std::string("Commonwealth HQ")), "survey research HQ")) return false;
		for (TechID tech : integrated ? std::vector<TechID>{} : std::vector<TechID>{TECH_MATERIALS_2, TECH_MATERIALS_3}) {
			if (!Result(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, tech), "resource research")) return false;
			if (!Result(Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, TechTreeManager::GetNode(tech)->cost_rp * 1000), "resource research budget")) return false;
			TechTreeManager::ProcessMonthlyResearch();
			if (!TechTreeManager::IsTechUnlocked(company->index, tech)) return false;
		}
	}
	for (auto [node, local] :
	    {std::pair{13u, 0x10u}, std::pair{14u, 0x12u}, std::pair{15u, 0x16u}, std::pair{16u, 0x18u}, std::pair{17u, 0x14u}}) {
		IndustryType type = MapNewGRFIndustryType(0x80 | local, COMMONWEALTH_INDUSTRY_GRFID);
		if (integrated && ResourceSiteManager::RequiredTech(type) != TECH_NONE) continue;
		if (resources) {
			TileIndex site = TileXY(nodes[node].x, TOP - 5);
			if (ResourceSiteManager::AddSite(site, type, 16, 16) == 0 || !Result(Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, site), "resource survey")) return false;
		}
		if (!Result(Command<Commands::BuildIndustry>::Do(DoCommandFlag::Execute, TileXY(nodes[node].x, TOP - 5), type, 0, false, 1),
		        "raw extraction"))
			return false;
	}
	// Native Arctic farm and food processor supply actual food, not proxy minerals.
	for (auto [node, type] : integrated ? std::vector<std::pair<uint, IndustryType>>{}
										: std::vector<std::pair<uint, IndustryType>>{{18u, IndustryType{9}}, {3u, IndustryType{13}}}) {
		if (resources && ResourceSiteManager::IsPrimary(type)) {
			TileIndex site = TileXY(nodes[node].x, TOP - 12);
			if (ResourceSiteManager::AddSite(site, type, 16, 24) == 0 || !Result(Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, site), "farm survey")) return false;
		}
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
	for (size_t i = 23; i < legacy_nodes.size(); ++i)
		if (!Result(Command<Commands::BuildLogisticsHub>::Do(
		                DoCommandFlag::Execute, TileXY(nodes[i].x, TOP), Stop(i), std::string(nodes[i].name)),
		        "raw export warehouse"))
			return false;
	if (!resources && !Result(Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, TileXY(70, 60), std::string("Commonwealth HQ")),
	        "Corporate HQ"))
		return false;
	if (!Result(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_PORTAL_1), "research project")) return false;
	if (!Result(Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 25000), "research budget")) return false;
	if (!integrated) {
		using C = CommonwealthCargoID;
		const std::vector<std::tuple<size_t, size_t, C>> flows = {{23, 4, C::StoneSlag},
																  {4, 0, C::StoneSlag},
																  {24, 5, C::IronOre},
																  {5, 6, C::StructuralSteel},
																  {5, 0, C::StructuralSteel},
																  {27, 6, C::RareEarthMinerals},
																  {6, 11, C::Superalloys},
																  {11, 0, C::Superalloys},
																  {25, 7, C::CopperOre},
																  {7, 9, C::ConductiveWiring},
																  {7, 10, C::ConductiveWiring},
																  {7, 11, C::ConductiveWiring},
																  {7, 0, C::ConductiveWiring},
																  {26, 8, C::SilicaSand},
																  {8, 9, C::SiliconChips},
																  {9, 0, C::SiliconChips},
																  {21, 1, C::SiliconChips},
																  {21, 1, C::SyntheticComposites},
																  {10, 0, C::SyntheticComposites},
																  {26, 12, C::SilicaSand},
																  {27, 12, C::RareEarthMinerals},
																  {12, 19, C::BlankCrystals},
																  {12, 2, C::BlankCrystals},
																  {19, 0, C::EnrichedQuantumCrystals},
																  {2, 1, C::EncryptedConsumerCrystals}};
		for (auto [from, to, cargo] : std::vector<std::tuple<size_t, size_t, C>>{{13, 23, C::StoneSlag},
																				 {14, 24, C::IronOre},
																				 {15, 25, C::CopperOre},
																				 {16, 26, C::SilicaSand},
																				 {17, 27, C::RareEarthMinerals}})
			if (!TrainRoute(from, to, Cargo(cargo), 3)) return false;
		for (auto [from, to, cargo] : flows)
			if (!TrainRoute(from, to, Cargo(cargo),
							from == 21											  ? 1
							: from == 5 && to == 6								  ? 1
							: from == 7 && to != 0								  ? 1
							: from == 26 && to == 12							  ? 4
							: from == 24 || from == 25 || (from == 26 && to == 8) ? 12
																				  : 8))
				return false;
		CargoType farm_cargo = GetCargoTypeByLabel(CargoLabel{"WHEA"});
		if (!IsValidCargoType(farm_cargo)) farm_cargo = GetCargoTypeByLabel(CargoLabel{"GRAI"});
		if (!TrainRoute(22, 3, farm_cargo, 12) || !TrainRoute(18, 22, farm_cargo, 2) ||
			!TrainRoute(3, 21, GetCargoTypeByLabel(CargoLabel{"FOOD"}), 2) ||
			!TrainRoute(21, 1, GetCargoTypeByLabel(CargoLabel{"FOOD"}), 2))
			return false;
		if (!TrainRoute(1, 20, GetCargoTypeByLabel(CargoLabel{"PASS"}), 4) ||
			!TrainRoute(20, 1, GetCargoTypeByLabel(CargoLabel{"PASS"}), 4))
			return false;
		if (!TrainRoute(22, 3, farm_cargo, 12, 1)) return false;
	} else {
		if (!PrepareIntegratedServices()) return false;
	}
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
		if (!IntegratedEconomy::Enabled())
			for (auto [c, n] : f.input_buffers)
				held[c] += n;
		if (!IntegratedEconomy::Enabled())
			for (auto [c, n] : f.output_buffers)
				held[c] += n;
	}
	r["stocks"] = Json::array();
	for (const auto &s : StockpileManager::GetAllStockpiles()) {
		for (auto [c, n] : s.inventory)
			held[c] += n;
		r["stocks"].push_back({{"world", s.world_id.base()}, {"inventory", s.inventory}});
	}
	if (IntegratedEconomy::Enabled()) {
		r["economy"] = Json::parse(IntegratedEconomy::Save());
		r["stellar"] = Json::parse(StellarNetwork::Save());
		for (const auto &[id, project] : StellarNetwork::Projects()) {
			if (project.state == GateProjectState::Supplying || project.state == GateProjectState::Ready) {
				held[GetCargoTypeByLabel(CargoLabel{"STEL"})] += project.steel;
				held[GetCargoTypeByLabel(CargoLabel{"MACH"})] += project.machines;
			}
		}
		for (auto i : Industry::Iterate()) {
			if (!IntegratedEconomy::Managed(i)) continue;
			for (const auto &c : i->accepted)
				if (IsValidCargoType(c.cargo)) held[c.cargo] += c.waiting;
			for (const auto &c : i->produced)
				if (IsValidCargoType(c.cargo)) held[c.cargo] += c.waiting;
		}
		for (auto town : Town::Iterate())
			if (auto city = IntegratedEconomy::City(town->index))
				for (auto [c, n] : city->reserves)
					held[c] += n;
		if (auto research = IntegratedEconomy::Research(CompanyID{0}))
			for (auto [c, n] : research->reserved)
				held[c] += n;
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
/**
 * Capture ordinary-start state independently of the authored showcase nodes.
 * @return Observed native cargo, infrastructure, orders and economy state.
 */
Json FreightSnapshot()
{
	Json r = {{"seed", _settings_game.game_creation.generation_seed}, {"tick", TimerGameTick::counter},
		{"date", TimerGameCalendar::date.base()}, {"economy", Json::parse(IntegratedEconomy::Save())},
		{"stellar", Json::parse(StellarNetwork::Save())}, {"held", std::array<uint64_t, NUM_CARGO>{}}};
	auto held = std::array<uint64_t, NUM_CARGO>{};
	r["industries"] = Json::array();
	for (const Industry *i : Industry::Iterate()) {
		Json row = {{"id", i->index.base()}, {"tile", i->location.tile.base()}, {"width", i->location.w}, {"height", i->location.h},
			{"world", PlanetManager::GetTileWorld(i->location.tile).base()}, {"recipe", IntegratedEconomy::IndustryRecipe(i->type)},
			{"owner", i->founder.base()}, {"inputs", Json::array()}, {"outputs", Json::array()},
			{"output_batch_limit", IntegratedEconomy::OutputBatchLimit(i)}};
		for (const auto &c : i->accepted) if (IsValidCargoType(c.cargo)) {
			row["inputs"].push_back({c.cargo, c.waiting, c.waiting + IntegratedEconomy::IndustrySpace(i, c.cargo)});
			if (IntegratedEconomy::Managed(i)) held[c.cargo] += c.waiting;
		}
		for (const auto &c : i->produced) if (IsValidCargoType(c.cargo)) {
			uint32_t cap = 0;
			if (auto f = IntegratedEconomy::Factories().find(i->index); f != IntegratedEconomy::Factories().end()) {
				if (auto recipe = ProductionChainManager::GetRecipe(IntegratedEconomy::IndustryRecipe(i->type))) {
					uint yield = TechTreeManager::IsTechUnlocked(f->second.owner, TECH_MATERIALS_3) ? 115 : 100;
					for (auto [cargo, units] : recipe->outputs) if (cargo == c.cargo) cap = std::min(65535u, (f->second.capacity * units * 3 * yield + 99) / 100);
				}
			}
			row["outputs"].push_back({c.cargo, c.waiting, c.rate, cap});
			if (IntegratedEconomy::Managed(i)) held[c.cargo] += c.waiting;
		}
		r["industries"].push_back(row);
	}
	r["stations"] = Json::array();
	for (const Station *s : Station::Iterate()) {
		std::array<uint64_t, NUM_CARGO> cargo{};
		for (CargoType c{0}; c < NUM_CARGO; ++c) held[c] += cargo[c] = s->goods[c].TotalCount();
		r["stations"].push_back({{"id", s->index.base()}, {"tile", s->xy.base()}, {"owner", s->owner.base()}, {"cargo", cargo}});
	}
	r["trains"] = Json::array();
	for (const Train *t : Train::Iterate()) {
		if (IsValidCargoType(t->cargo_type)) held[t->cargo_type] += t->cargo.StoredCount();
		Json orders = Json::array();
		if (t->IsFrontEngine()) for (const auto &o : t->Orders()) orders.push_back({o.GetType(), o.GetDestination().base(), o.GetLoadType(), o.GetUnloadType(), o.GetNonStopType().base(), o.GetStopLocation()});
		r["trains"].push_back({{"id", t->index.base()}, {"engine", t->engine_type.base()}, {"tile", t->tile.base()}, {"x", t->x_pos}, {"y", t->y_pos},
			{"z", t->z_pos}, {"track", t->track.base()}, {"direction", uint(t->direction)}, {"speed", t->cur_speed}, {"cargo_type", t->cargo_type},
			{"cargo", t->cargo.StoredCount()}, {"capacity", t->cargo_cap}, {"front", t->IsFrontEngine()}, {"orders", orders},
			{"order_index", t->cur_real_order_index}, {"current_order", t->current_order.GetDestination().base()},
			{"lost", t->vehicle_flags.Test(VehicleFlag::PathfinderLost)}, {"crashed", t->vehstatus.Test(VehState::Crashed)}});
	}
	r["stocks"] = Json::array();
	for (const auto &s : StockpileManager::GetAllStockpiles()) {
		r["stocks"].push_back({s.world_id.base(), s.company_id.base(), s.inventory});
		for (auto [c, n] : s.inventory) held[c] += n;
	}
	for (const Town *t : Town::Iterate()) if (auto city = IntegratedEconomy::City(t->index))
		for (auto [c, n] : city->reserves) held[c] += n;
	if (auto research = IntegratedEconomy::Research(CompanyID{0})) for (auto [c, n] : research->reserved) held[c] += n;
	r["held"] = held;
	r["research"] = Json::array();
	for (const auto &t : TechTreeManager::GetAllCompanyTechStates()) r["research"].push_back({t.active_project, t.accumulated_rp, t.monthly_budget, t.unlocked_techs});
	r["rail"] = Json::array();
	for (uint tile = 0; tile < Map::Size(); ++tile) if (IsPlainRailTile(TileIndex{tile}))
		r["rail"].push_back({tile, GetTileOwner(TileIndex{tile}).base(), GetTrackBits(TileIndex{tile}).base()});
	r["structures"] = Json::array();
	for (uint tile = 0; tile < Map::Size(); ++tile) {
		TileIndex t{tile};
		if (IsRailDepotTile(t)) r["structures"].push_back({tile, GetTileOwner(t).base(), uint(GetRailDepotDirection(t))});
		if (IsBridgeTile(t)) r["structures"].push_back({tile, GetTileOwner(t).base(), uint(GetTunnelBridgeDirection(t)), GetOtherBridgeEnd(t).base()});
	}
	r["hubs"] = Json::array();
	for (const auto &hub : LogisticsHubManager::GetAllHubs()) r["hubs"].push_back({hub.hub_id, hub.tile.base(), hub.station_id.base(), hub.total_deposited});
	r["gates"] = Json::array();
	std::map<uint32_t, PortalLink> sorted(PortalRegistry::GetAllPortals().begin(), PortalRegistry::GetAllPortals().end());
	for (const auto &[id, p] : sorted) {
		Json ends = Json::array();
		for (auto e : {p.end_a, p.end_b}) {
			auto policy = StellarNetwork::Policy(e.tile);
			ends.push_back({{"tile", e.tile.base()}, {"world", e.world_id.base()}, {"dir", uint(e.enter_dir)},
				{"public", policy != nullptr && policy->public_access}, {"toll", policy == nullptr ? 0 : int64_t(policy->toll)}});
		}
		r["gates"].push_back({{"id", id}, {"ends", ends}});
	}
	const Company *c = Company::GetIfValid(CompanyID{0});
	if (c != nullptr) {
		r["money"] = int64_t(c->money); r["loan"] = int64_t(c->current_loan); r["max_loan"] = int64_t(c->GetMaxLoan());
		r["expenses"] = Json::array();
		for (const auto &year : c->yearly_expenses) {
			Json row = Json::array(); for (Money n : year) row.push_back(int64_t(n)); r["expenses"].push_back(row);
		}
	}
	r["cargo_labels"] = Json::object();
	for (auto label : {CargoLabel{"IRON"}, CargoLabel{"STEL"}, CargoLabel{"GRAI"}, CargoLabel{"FOOD"}}) r["cargo_labels"][label.AsString()] = GetCargoTypeByLabel(label);
	return r;
}

/**
 * Normalize tile types whose native metadata has no ownership field.
 * @param tile In-range map tile to observe without changing it.
 * @return Native owner, or OWNER_NONE for Void, House and Industry tiles.
 */
Owner ObservedTileOwner(TileIndex tile)
{
	if (IsTileType(tile, TileType::Void) || IsTileType(tile, TileType::House) || IsTileType(tile, TileType::Industry)) return OWNER_NONE;
	return GetTileOwner(tile);
}

/**
 * Native tile observations shared by generated terminals and advertised arrival zones.
 * @param tile Native map tile to observe without changing it.
 * @return Captured bounds, ownership, terrain and rail observations.
 */
Json GenerationTile(TileIndex tile)
{
	Json r = {{"tile", tile.base()}, {"valid", IsValidTile(tile) && IsInnerTile(tile)}};
	if (!r["valid"].get<bool>()) return r;
	r["world"] = PlanetManager::GetTileWorld(tile).base();
	r["type"] = uint(GetTileType(tile)); r["owner"] = ObservedTileOwner(tile).base();
	r["slope"] = uint(GetTileSlope(tile)); r["height"] = TileHeight(tile);
	if (IsPlainRailTile(tile)) {
		r["tracks"] = GetTrackBits(tile).base(); r["railtype"] = uint(GetRailType(tile));
	}
	if (IsTileType(tile, TileType::TunnelBridge) && GetTunnelBridgeTransportType(tile) == TransportType::Rail)
		r["railtype"] = uint(GetRailType(tile));
	return r;
}

/**
 * Read-only geometry projection; does not impose clear joins on loaded or paid states.
 * @param head Native terminal head or advertised arrival-zone tile.
 * @param dir Planned inward terminal direction.
 * @param world Required immutable world identity.
 * @return Native geometry and expected rail/signal layout observations.
 */
Json GenerationTerminal(TileIndex head, DiagDirection dir, WorldID world)
{
	Json r = {{"head", GenerationTile(head)}, {"world", world.base()}, {"dir", uint(dir)},
		{"rails", Json::array()}, {"signals", Json::array()}};
	if (IsTileType(head, TileType::TunnelBridge)) r["head"]["dir"] = uint(GetTunnelBridgeDirection(head));
	auto layout = PortalTerminal::Plan(head, dir, world);
	r["planned"] = layout.has_value();
	if (!layout) return r;
	r["connection"] = layout->connection_tile.base(); r["outward"] = uint(layout->outward_dir);
	r["join"] = GenerationTile(TileAddByDiagDir(layout->connection_tile, layout->outward_dir));
	for (const auto &rail : layout->tiles) {
		Json row = GenerationTile(rail.tile); row["expected_tracks"] = rail.tracks.base(); r["rails"].push_back(row);
	}
	for (const auto &signal : layout->signals) {
		Json row = {{"tile", signal.tile.base()}, {"track", uint(signal.track)}, {"travel", uint(signal.travel_dir)},
			{"expected_present", SignalAlongTrackdir(DiagDirToDiagTrackdir(signal.travel_dir))},
			{"expected_type", uint(SignalType::PathOneWay)}, {"expected_variant", uint(SignalVariant::Electric)}};
		bool present = IsPlainRailTile(signal.tile) && HasSignalOnTrack(signal.tile, signal.track);
		row["present"] = present;
		if (present) {
			row["type"] = uint(GetSignalType(signal.tile, signal.track));
			row["variant"] = uint(GetSignalVariant(signal.tile, signal.track));
			row["present_bits"] = GetPresentSignals(signal.tile); row["state_bits"] = GetSignalStates(signal.tile);
		}
		r["signals"].push_back(row);
	}
	return r;
}

/**
 * Capture generation semantics separately so retained FreightSnapshot equality remains unchanged.
 * @return Captured native economy, RNG, geometry, town and station observations.
 */
Json GenerationSnapshot()
{
	Json r = FreightSnapshot();
	r["rng"] = {_random.state[0], _random.state[1]};
	r["worlds"] = Json::array();
	for (const auto &world : PlanetManager::GetAllRegions()) r["worlds"].push_back({{"id", world.id.base()},
		{"name", world.name}, {"phase", uint(world.phase)}, {"biome", uint(world.biome)},
		{"bounds", {world.min_x, world.min_y, world.max_x, world.max_y}},
		{"role", IntegratedEconomy::RoleName(world.id)}, {"development", world.development_score}});
	r["terminals"] = Json::array();
	std::map<uint32_t, PortalLink> sorted(PortalRegistry::GetAllPortals().begin(), PortalRegistry::GetAllPortals().end());
	for (const auto &[id, portal] : sorted) for (const auto &end : {portal.end_a, portal.end_b}) {
		Json row = GenerationTerminal(end.tile, end.enter_dir, end.world_id); row["gate"] = id;
		r["terminals"].push_back(row);
	}
	r["zones"] = Json::array();
	for (const auto &[id, zone] : StellarNetwork::Zones()) {
		Json row = GenerationTerminal(zone.tile, zone.direction, zone.world); row["id"] = id;
		r["zones"].push_back(row);
	}
	r["towns"] = Json::array();
	for (const Town *town : Town::Iterate()) {
		Json houses = Json::array();
		for (uint n = 0; n < Map::Size(); ++n) {
			TileIndex tile{n};
			if (IsTileType(tile, TileType::House) && GetTownIndex(tile) == town->index)
				houses.push_back({{"tile", n}, {"world", PlanetManager::GetTileWorld(tile).base()}, {"type", GetHouseType(tile)}});
		}
		r["towns"].push_back({{"id", town->index.base()}, {"tile", town->xy.base()},
			{"world", PlanetManager::GetTileWorld(town->xy).base()}, {"population", town->cache.population},
			{"house_count", town->cache.num_houses}, {"houses", houses}, {"megacity", MegacityManager::IsMegacity(town->index)}});
	}
	r["core_town_valid"] = HasValidIntegratedCoreTown();
	for (Json &row : r["stations"]) {
		const Station *station = Station::Get(StationID{row["id"].get<uint16_t>()});
		row["town"] = station->town == nullptr ? UINT16_MAX : station->town->index.base();
		row["warehouse"] = LogisticsHubManager::GetHubForStation(station->index) != nullptr;
		row["consumer"] = !station->catchment_tiles.IsEmpty() && MegacityManager::IsConsumerStation(station);
		row["catchment_houses"] = Json::array();
		if (!station->catchment_tiles.IsEmpty()) {
			for (BitmapTileIterator it(station->catchment_tiles); *it != INVALID_TILE; ++it) {
				TileIndex tile = *it;
				if (IsTileType(tile, TileType::House)) row["catchment_houses"].push_back({{"tile", tile.base()},
					{"town", GetTownIndex(tile).base()}, {"world", PlanetManager::GetTileWorld(tile).base()}});
			}
		}
	}
	return r;
}

/** Nonpersistent fresh-process authorization; no load adapter restores it. */
bool generation_contract_fresh = false;
/** Disposable company marker used only by the guarded native proof. */
constexpr std::string_view GENERATION_COMPANY = "Generation contract acceptance";

/**
 * Refuse spending after any player construction, progression, grant or borrowing.
 * @return True when the disposable company still has untouched ordinary starting state.
 */
bool PristineGenerationCompany()
{
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (Company::GetNumItems() != 1 || company == nullptr || company->name != GENERATION_COMPANY ||
		company->money != 100000 || company->current_loan != 100000 || company->GetMaxLoan() != 300000 || _settings_game.difficulty.infinite_money ||
		Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0 || !LogisticsHubManager::GetAllHubs().empty() ||
		!MegacityManager::GetAllMegacities().empty() || !StockpileManager::GetAllStockpiles().empty() ||
		!TechTreeManager::GetAllCompanyTechStates().empty()) return false;
	for (uint n = 0; n < Map::Size(); ++n) {
		TileIndex tile{n};
		if ((IsPlainRailTile(tile) || IsRailDepotTile(tile)) && GetTileOwner(tile) != OWNER_NONE) return false;
	}
	return true;
}

/** A bounded native two-tile station probe close to an actual own Core house. */
struct GenerationStation {
	TileIndex tile = INVALID_TILE; ///< First tile of the prospective two-tile station.
	Axis axis = Axis::X; ///< Native station platform axis.
	TownID town = TownID::Invalid(); ///< Expected station town under ordinary native assignment.
	TileIndex house = INVALID_TILE; ///< Own Core house that must be caught after paid construction.
};

/**
 * Quote all native exterior joins and one ordinary Core station without spending or advancing.
 * @param[out] report Native quotes, bounded candidates and selected station observations.
 * @param[out] station Selected legal house-catching station when a candidate succeeds.
 * @return True when all joins and a bounded station candidate have positive native quotes.
 */
bool GenerationQueries(Json &report, GenerationStation &station)
{
	report = {{"joins", Json::array()}, {"station_candidates", Json::array()}};
	std::set<TileIndex> claimed;
	Money total = 0;
	std::map<uint32_t, PortalLink> sorted(PortalRegistry::GetAllPortals().begin(), PortalRegistry::GetAllPortals().end());
	for (const auto &[id, portal] : sorted) for (const auto &end : {portal.end_a, portal.end_b}) {
		auto layout = PortalTerminal::Plan(end.tile, end.enter_dir, end.world_id);
		if (!layout) { IConsolePrint(CC_ERROR, "CONNECTED FAIL generation terminal plan invalid"); return false; }
		TileIndex join = TileAddByDiagDir(layout->connection_tile, layout->outward_dir);
		claimed.insert(end.tile); claimed.insert(join);
		for (const auto &rail : layout->tiles) claimed.insert(rail.tile);
		auto quote = Command<Commands::BuildRail>::Do({}, join, RAILTYPE_RAIL, DiagDirToDiagTrack(layout->outward_dir), false);
		if (!Result(quote, "generation exterior join query") || quote.GetCost() <= 0) return false;
		total += quote.GetCost();
		report["joins"].push_back({{"gate", id}, {"head", end.tile.base()}, {"connection", layout->connection_tile.base()},
			{"join", join.base()}, {"outward", uint(layout->outward_dir)}, {"track", uint(DiagDirToDiagTrack(layout->outward_dir))},
			{"quote", int64_t(quote.GetCost())}});
	}
	for (const auto &[id, zone] : StellarNetwork::Zones()) {
		auto layout = PortalTerminal::Plan(zone.tile, zone.direction, zone.world);
		if (!layout) continue;
		claimed.insert(zone.tile); claimed.insert(TileAddByDiagDir(layout->connection_tile, layout->outward_dir));
		for (const auto &rail : layout->tiles) claimed.insert(rail.tile);
	}
	std::vector<std::tuple<uint, TileIndex, Axis, TownID, TileIndex>> candidates;
	std::set<std::pair<TileIndex, Axis>> seen;
	for (uint n = 0; n < Map::Size(); ++n) {
		TileIndex house{n};
		if (!IsTileType(house, TileType::House)) continue;
		const Town *town = Town::Get(GetTownIndex(house)); WorldID world = PlanetManager::GetTileWorld(town->xy);
		if (town->cache.population == 0 || IntegratedEconomy::Role(world) != EconomicRole::Core || PlanetManager::GetTileWorld(house) != world) continue;
		for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx) {
			int x = int(TileX(house)) + dx, y = int(TileY(house)) + dy;
			if (x <= 0 || y <= 0 || x + 2 >= int(Map::MaxX()) || y + 2 >= int(Map::MaxY())) continue;
			for (Axis axis : {Axis::X, Axis::Y}) {
				TileIndex tile = TileXY(x, y), second = TileAddByDiagDir(tile, axis == Axis::X ? DiagDirection::SW : DiagDirection::SE);
				if (seen.contains({tile, axis}) || claimed.contains(tile) || claimed.contains(second) ||
					PlanetManager::GetTileWorld(tile) != world || PlanetManager::GetTileWorld(second) != world ||
					!IsTileType(tile, TileType::Clear) || !IsTileType(second, TileType::Clear) ||
					GetTileSlope(tile) != SLOPE_FLAT || GetTileSlope(second) != SLOPE_FLAT || TileHeight(tile) != TileHeight(second) ||
					ClosestTownFromTile(tile, UINT_MAX) != town) continue;
				seen.insert({tile, axis}); candidates.emplace_back(DistanceManhattan(tile, house), tile, axis, town->index, house);
			}
		}
	}
	std::sort(candidates.begin(), candidates.end()); uint attempts = 0;
	for (const auto &[distance, tile, axis, town, house] : candidates) {
		if (++attempts > 16) break;
		auto quote = Command<Commands::BuildRailStation>::Do({}, tile, RAILTYPE_RAIL, axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
		report["station_candidates"].push_back({{"tile", tile.base()}, {"axis", uint(axis)}, {"legal", quote.Succeeded()}});
		if (quote.Failed() || quote.GetCost() <= 0) continue;
		station = {tile, axis, town, house}; total += quote.GetCost();
		report["station"] = {{"tile", tile.base()}, {"axis", uint(axis)}, {"town", town.base()}, {"house", house.base()}, {"quote", int64_t(quote.GetCost())}};
		report["total_quote"] = int64_t(total); return true;
	}
	IConsolePrint(CC_ERROR, "CONNECTED FAIL bounded 16-candidate Core station query exhausted"); return false;
}

/**
 * Exercise the real train follower across every paid neutral/player boundary in both directions.
 * @return Native bidirectional follower results for each public terminal.
 */
Json GenerationFollow()
{
	Json report = Json::array();
	std::map<uint32_t, PortalLink> sorted(PortalRegistry::GetAllPortals().begin(), PortalRegistry::GetAllPortals().end());
	for (const auto &[id, portal] : sorted) for (const auto &end : {portal.end_a, portal.end_b}) {
		auto layout = PortalTerminal::Plan(end.tile, end.enter_dir, end.world_id);
		if (!layout) continue;
		TileIndex join = TileAddByDiagDir(layout->connection_tile, layout->outward_dir);
		TrackBits needed{DiagDirToDiagTrack(layout->outward_dir)};
		bool ready = IsPlainRailTile(join) && GetTileOwner(join) == CompanyID{0} && GetTrackBits(join).All(needed) &&
			IsPlainRailTile(layout->connection_tile) && GetTileOwner(layout->connection_tile) == OWNER_NONE && GetTrackBits(layout->connection_tile).All(needed);
		Json row = {{"gate", id}, {"head", end.tile.base()}, {"join", join.base()}, {"ready", ready}};
		if (ready) {
			CFollowTrackRail entering(CompanyID{0}, RailTypes{RAILTYPE_RAIL}), exiting(CompanyID{0}, RailTypes{RAILTYPE_RAIL});
			row["enter"] = entering.Follow(join, DiagDirToDiagTrackdir(ReverseDiagDir(layout->outward_dir))) && entering.new_tile == layout->connection_tile;
			row["exit"] = exiting.Follow(layout->connection_tile, DiagDirToDiagTrackdir(layout->outward_dir)) && exiting.new_tile == join;
			row["enter_error"] = uint(entering.err); row["exit_error"] = uint(exiting.err);
			row["enter_tracks"] = entering.new_td_bits.base(); row["exit_tracks"] = exiting.new_td_bits.base();
		}
		report.push_back(row);
	}
	return report;
}

/**
 * Guarded ordinary generation proof; spending is confined to the fresh disposable process.
 * @param argv Native console command and requested generation proof phase.
 * @return True after reporting the accepted phase or a guarded rejection.
 */
bool GenerationContract(std::span<std::string_view> argv)
{
	if (argv.size() != 2 || (argv[1] != "generation-contract-start" && argv[1] != "generation-contract-status" &&
		argv[1] != "generation-contract-queries" && argv[1] != "generation-contract-paid" && argv[1] != "generation-contract-follow")) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL invalid generation-contract arguments"); return true;
	}
	if (_game_mode != GameMode::Normal || (_networking && (!_network_dedicated || NetworkClientInfo::GetNumItems() > 1)) ||
		!IntegratedEconomy::Enabled() || PlanetManager::Count() != 7) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL generation contract requires isolated seven-world integrated game"); return true;
	}
	/* Read-only retained-save evidence must not require or write a disposable-company marker. */
	if (argv[1] == "generation-contract-status") {
		IConsolePrint(CC_DEFAULT, "CONNECTED generation-state {}", GenerationSnapshot().dump()); return true;
	}
	if (argv[1] == "generation-contract-start") {
		generation_contract_fresh = false;
		if (Company::GetNumItems() != 0 || Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0 ||
			!GetIntegratedCoreTownGenerationStats().active || !HasValidIntegratedCoreTown()) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL generation contract requires pristine fresh generation"); return true;
		}
		for (uint n = 0; n < Map::Size(); ++n) if (IsPlainRailTile(TileIndex{n}) && GetTileOwner(TileIndex{n}) != OWNER_NONE) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL generation contract found player assets"); return true;
		}
		if (DoStartupNewCompany(false, CompanyID{0}) == nullptr) return true;
		AutoRestoreBackup owner(_current_company, CompanyID{0});
		if (!Result(Command<Commands::RenameCompany>::Do(DoCommandFlag::Execute, std::string(GENERATION_COMPANY)), "generation company marker")) return true;
		_pause_mode.Set(PauseMode::Normal); generation_contract_fresh = true;
	}
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (company == nullptr || company->name != GENERATION_COMPANY) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL missing isolated generation company marker"); return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (argv[1] == "generation-contract-queries" || argv[1] == "generation-contract-paid") {
		if (!PristineGenerationCompany() || (argv[1] == "generation-contract-paid" && !generation_contract_fresh)) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL generation spending/query requires pristine company; spending requires fresh process"); return true;
		}
		Json before = GenerationSnapshot(), report; GenerationStation station;
		if (!GenerationQueries(report, station)) return true;
		if (GenerationSnapshot() != before) { IConsolePrint(CC_ERROR, "CONNECTED FAIL generation query mutated native state"); return true; }
		if (argv[1] == "generation-contract-queries") { IConsolePrint(CC_DEFAULT, "CONNECTED generation-query {}", report.dump()); return true; }
		generation_contract_fresh = false;
		if (report["total_quote"].get<int64_t>() >= int64_t(company->money)) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL generation proof unaffordable with ordinary starting cash"); return true;
		}
		for (Json &join : report["joins"]) {
			auto result = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, TileIndex{join["join"].get<uint32_t>()},
				RAILTYPE_RAIL, Track(join["track"].get<uint>()), false);
			if (!Result(result, "paid generation exterior join")) return true;
			join["paid"] = int64_t(result.GetCost());
		}
		auto result = Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, station.tile, RAILTYPE_RAIL, station.axis,
			1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
		if (!Result(result, "paid Core catchment station")) return true;
		report["station"]["paid"] = int64_t(result.GetCost());
		const Station *built = Station::Get(GetStationIndex(station.tile));
		if (built->town == nullptr || built->town->index != station.town || !built->catchment_tiles.HasTile(station.house) ||
			LogisticsHubManager::GetHubForStation(built->index) != nullptr || MegacityManager::IsMegacity(station.town)) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL paid station did not catch its assigned Core town house"); return true;
		}
		report["follow"] = GenerationFollow(); report["state"] = GenerationSnapshot();
		IConsolePrint(CC_DEFAULT, "CONNECTED generation-paid {}", report.dump()); return true;
	}
	if (argv[1] == "generation-contract-follow") {
		Json before = GenerationSnapshot(), report = GenerationFollow();
		if (GenerationSnapshot() != before) { IConsolePrint(CC_ERROR, "CONNECTED FAIL native generation follower mutated state"); return true; }
		IConsolePrint(CC_DEFAULT, "CONNECTED generation-follow {}", report.dump()); return true;
	}
	Json r = GenerationSnapshot();
	if (argv[1] == "generation-contract-start") {
		const auto &stats = GetIntegratedCoreTownGenerationStats();
		r["town_generation"] = {{"active", stats.active}, {"target", stats.target}, {"city_offset", stats.city_offset},
			{"probes", stats.probes}, {"coastal_probes", stats.coastal_probes}, {"creation_attempts", stats.creation_attempts},
			{"deleted_candidates", stats.deleted_candidates}, {"zero_population_candidates", stats.zero_population_candidates},
			{"no_core_house_candidates", stats.no_core_house_candidates}, {"probe_hash", stats.probe_hash},
			{"core_town", stats.core_town.base()}, {"failure", uint(stats.failure)}};
	}
	IConsolePrint(CC_DEFAULT, "CONNECTED generation-state {}", r.dump()); return true;
}

/** One prospective player-built station, depot and connection to a generated public throat. */
struct FreightLeg {
	TileIndex station = INVALID_TILE; ///< First tile of the prospective two-tile station.
	TileIndex depot = INVALID_TILE; ///< Prospective depot tile behind the station.
	Axis axis = Axis::X; ///< Station platform axis.
	DiagDirection depot_dir = DiagDirection::Invalid; ///< Depot exit toward the station.
	std::vector<std::pair<TileIndex, Track>> rails; ///< Native track pieces connecting the station to the terminal.
	std::vector<std::pair<TileIndex, TileIndex>> bridges; ///< Pairs of native bridge endpoint tiles.
};

/** @cond CoreBasketPrivatePlanner */
/** Extra constraints used only by the loaded construction-basket adapter. */
struct BasketLegOptions {
	CoreBasketSearchBudget *budget = nullptr;
	const std::map<TileIndex, TrackBits> *planned_rails = nullptr;
	const std::set<TileIndex> *blocked = nullptr;
	TileIndex fixed_station = INVALID_TILE;
	Axis fixed_axis = Axis::X;
	bool depot = true;
	uint layout = 0;
};
/** @endcond */

/**
 * Find a legal rail path using native previews without editing terrain or neutral infrastructure.
 * @param industry Generated producer or consumer to serve.
 * @param gate Public terminal endpoint in the industry's world.
 * @param[out] leg Feasible station, depot and connecting rail plan when found.
 * @param town Optional live town to serve instead of an industry.
 * @param options Optional bounded loaded-checkpoint planner constraints.
 * @return Whether the bounded search found a feasible connection.
 */
bool PlanFreightLeg(const Industry *industry, const PortalEndpoint &gate, FreightLeg &leg, const Town *town = nullptr,
	const BasketLegOptions *options = nullptr)
{
	auto terminal = PortalTerminal::Plan(gate.tile, gate.enter_dir, gate.world_id);
	if (!terminal) return false;
	DiagDirection outward = terminal->outward_dir;
	TileIndex target = TileAddByDiagDir(terminal->connection_tile, outward);
	std::vector<std::tuple<uint, TileIndex, Axis>> candidates;
	auto candidate = [&](int x, int y, Axis axis) {
		if (x <= 0 || y <= 0 || x + 2 >= int(Map::MaxX()) || y + 2 >= int(Map::MaxY())) return;
		TileIndex tile = TileXY(x, y);
		if (options != nullptr) {
			TileIndex second = TileAddByDiagDir(tile, axis == Axis::X ? DiagDirection::SW : DiagDirection::SE);
			for (TileIndex part : {tile, second}) {
				if (options->blocked->contains(part) || options->planned_rails->contains(part) ||
					(!IsTileType(part, TileType::Clear) && !IsTileType(part, TileType::Trees))) return;
			}
		}
		if (town != nullptr && ClosestTownFromTile(tile, UINT_MAX) != town) return;
		if (options != nullptr) {
			candidates.emplace_back(DistanceManhattan(tile, target), tile, axis);
			return;
		}
		if (Command<Commands::BuildRailStation>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false).Succeeded())
			candidates.emplace_back(DistanceManhattan(tile, target), tile, axis);
	};
	if (options != nullptr && options->fixed_station != INVALID_TILE) {
		candidates.emplace_back(0, options->fixed_station, options->fixed_axis);
	} else if (town == nullptr) {
		for (int dy = -4; dy <= int(industry->location.h) + 3; ++dy) for (int dx = -4; dx <= int(industry->location.w) + 3; ++dx)
			for (Axis axis : {Axis::X, Axis::Y}) candidate(int(TileX(industry->location.tile)) + dx, int(TileY(industry->location.tile)) + dy, axis);
	} else {
		for (uint n = 0; n < Map::Size(); ++n) {
			TileIndex house{n};
			if (!IsTileType(house, TileType::House) || GetTownIndex(house) != town->index || PlanetManager::GetTileWorld(house) != gate.world_id) continue;
			for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx)
				for (Axis axis : {Axis::X, Axis::Y}) candidate(int(TileX(house)) + dx, int(TileY(house)) + dy, axis);
		}
	}
	std::sort(candidates.begin(), candidates.end());
	candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
	if (options != nullptr && options->layout == 1) {
		std::stable_sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
			return std::tuple{std::get<2>(a) != Axis::Y, std::get<0>(a), std::get<1>(a)} <
				std::tuple{std::get<2>(b) != Axis::Y, std::get<0>(b), std::get<1>(b)};
		});
	}
	std::vector<int8_t> preview(Map::Size() * 6, -1);
	uint attempts = 0;
	for (auto [distance, station, axis] : candidates) {
		if (++attempts > 16) break;
		if (options != nullptr) ++options->budget->candidates;
		if (options != nullptr && options->fixed_station == INVALID_TILE &&
			Command<Commands::BuildRailStation>::Do(DoCommandFlag::QueryCost, station, RAILTYPE_RAIL, axis, 1, 2,
				STAT_CLASS_DFLT, 0, NEW_STATION, true).Failed()) continue;
		DiagDirection positive = axis == Axis::X ? DiagDirection::SW : DiagDirection::SE;
		DiagDirection dir = axis == Axis::X ? (TileX(target) > TileX(station) ? positive : ReverseDiagDir(positive)) :
			(TileY(target) > TileY(station) ? positive : ReverseDiagDir(positive));
		TileIndex station_near = dir == positive ? TileAddByDiagDir(station, positive) : station;
		TileIndex station_far = dir == positive ? station : TileAddByDiagDir(station, positive);
		TileIndex start = TileAddByDiagDir(station_near, dir), depot = TileAddByDiagDir(station_far, ReverseDiagDir(dir));
		if (options != nullptr && ((!IsTileType(start, TileType::Clear) && !IsTileType(start, TileType::Trees) &&
			!IsPlainRailTile(start)) || (IsPlainRailTile(start) && TracksOverlap(GetTrackBits(start))))) continue;
		if (options == nullptr || options->depot) {
			if (options != nullptr && (options->blocked->contains(depot) || options->planned_rails->contains(depot) ||
				(!IsTileType(depot, TileType::Clear) && !IsTileType(depot, TileType::Trees)))) continue;
			if (!Command<Commands::BuildRailDepot>::Do(DoCommandFlag::QueryCost, depot, RAILTYPE_RAIL, dir).Succeeded()) continue;
		}
		/* Weighted A* is a bounded feasible-route search, not a minimum-cost claim. State retains native track direction; ties are stable. */
		using Q = std::tuple<uint, int, uint>;
		std::priority_queue<Q, std::vector<Q>, std::greater<Q>> queue;
		std::map<uint, std::tuple<uint, Track, TileIndex>> previous;
		std::map<uint, uint> costs;
		uint first = start.base() * 16 + uint(DiagDirToDiagTrackdir(dir));
		if (options != nullptr && !options->budget->TryState()) return false;
		queue.emplace(3 * DistanceManhattan(start, target), 0, first); costs[first] = 0;
		uint last = UINT_MAX, closest = UINT_MAX;
		while (!queue.empty() && previous.size() < 30000) {
			auto [estimate, negative_cost, key] = queue.top(); queue.pop();
			uint cost = uint(-negative_cost);
			if (cost != costs[key]) continue;
			TileIndex tile{key / 16}; Trackdir previous_dir = Trackdir(key % 16);
			DiagDirection enter = ReverseDiagDir(TrackdirToExitdir(previous_dir));
			closest = std::min(closest, DistanceManhattan(tile, target));
			if (PlanetManager::GetTileWorld(tile) != gate.world_id || ((options == nullptr || options->depot) && tile == depot) || tile == station || tile == TileAddByDiagDir(station, positive)) continue;
			if (options != nullptr && options->blocked->contains(tile)) continue;
			/* Reuse a legal owned bridge as a directed edge; never modify its ramp or neutral portals. */
			if (options != nullptr && IsBridgeTile(tile) && GetTileOwner(tile) == CompanyID{0} &&
				GetTunnelBridgeTransportType(tile) == TransportType::Rail && GetRailType(tile) == RAILTYPE_RAIL &&
				GetTunnelBridgeDirection(tile) == TrackdirToExitdir(previous_dir)) {
				TileIndex end = GetOtherBridgeEnd(tile);
				DiagDirection travel = GetTunnelBridgeDirection(tile);
				TileIndex next = TileAddByDiagDir(end, travel);
				if (next < Map::Size() && GetTileOwner(end) == CompanyID{0} && PlanetManager::GetTileWorld(end) == gate.world_id) {
					uint nk = next.base() * 16 + uint(DiagDirToDiagTrackdir(travel)), nc = cost + DistanceManhattan(tile, end);
					if (!costs.contains(nk) || costs[nk] > nc) {
						if (!previous.contains(nk) && !options->budget->TryState()) return false;
						costs[nk] = nc; previous[nk] = {key, DiagDirToDiagTrack(travel), end};
						queue.emplace(nc + 3 * DistanceManhattan(next, target), -int(nc), nk);
					}
				}
				continue;
			}
			if (!IsTileType(tile, TileType::Clear) && !IsTileType(tile, TileType::Trees) && !IsTileType(tile, TileType::Road) && !(IsPlainRailTile(tile) && GetTileOwner(tile) == CompanyID{0})) continue;
			for (DiagDirection exit : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
				Track track = Corner(enter, exit);
				if (!IsValidTrack(track)) continue;
				Trackdir next_dir = TrackExitdirToTrackdir(track, exit);
				if (IsValidTrackdir(next_dir) && TrackdirCrossesTrackdirs(previous_dir).Test(next_dir)) continue;
				auto &legal = preview[tile.base() * 6 + uint(track)];
				if (legal == -1) {
					TrackBits prospective{};
					if (options != nullptr && options->planned_rails->contains(tile)) prospective = options->planned_rails->at(tile);
					legal = options == nullptr ? Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, track, false).Succeeded() :
						QueryCoreBasketRail(tile, track, prospective).Succeeded();
				}
				if (!legal) continue;
				if (tile == target && exit == ReverseDiagDir(outward)) {
					if (options != nullptr && !options->budget->TryState()) return false;
					previous[UINT_MAX] = {key, track, INVALID_TILE}; last = key; break;
				}
				TileIndex next = TileAddByDiagDir(tile, exit);
				if (next >= Map::Size()) continue;
				uint nk = next.base() * 16 + uint(next_dir);
				if (costs.contains(nk) && costs[nk] <= cost + 1) continue;
				if (!previous.contains(nk) && previous.size() >= 29999) continue; // Reserve one terminal sentinel within the hard 30,000 cap.
				if (options != nullptr && !previous.contains(nk) && !options->budget->TryState()) return false;
				costs[nk] = cost + 1; previous[nk] = {key, track, INVALID_TILE}; queue.emplace(cost + 1 + 3 * DistanceManhattan(next, target), -int(cost + 1), nk);
			}
			/* Bounded ordinary bridges cross terrain/river barriers; preview native foundations and cost. */
			DiagDirection travel = TrackdirToExitdir(previous_dir);
			Track straight = DiagDirToDiagTrack(travel);
			Trackdir bridge_dir = DiagDirToDiagTrackdir(travel);
			TileIndex straight_next = TileAddByDiagDir(tile, travel);
			/* Native rail quotes may succeed by including river clearance. This
			 * ground-only search excludes water, so consider a bridge even then. */
			bool next_is_ground = straight_next < Map::Size() &&
				(IsTileType(straight_next, TileType::Clear) || IsTileType(straight_next, TileType::Trees) || IsTileType(straight_next, TileType::Road) ||
					(IsPlainRailTile(straight_next) && GetTileOwner(straight_next) == CompanyID{0}));
			if ((options == nullptr || (tile != start && !IsPlainRailTile(tile))) && !TrackdirCrossesTrackdirs(previous_dir).Test(bridge_dir) &&
				(!next_is_ground || !Command<Commands::BuildRail>::Do({}, straight_next, RAILTYPE_RAIL, straight, false).Succeeded())) {
				TileIndex end = tile;
				for (uint span = 1; span <= 16; ++span) {
					end = TileAddByDiagDir(end, travel);
					if (end >= Map::Size() || PlanetManager::GetTileWorld(end) != gate.world_id) break;
					if (span < 2 || end == depot || end == station || end == TileAddByDiagDir(station, positive) || end == target) continue;
					if (!IsTileType(end, TileType::Clear) && !IsTileType(end, TileType::Trees)) continue;
					if (options != nullptr) {
						bool conflict = false;
						for (TileIndex part = tile;; part = TileAddByDiagDir(part, travel)) {
							conflict |= options->blocked->contains(part) || options->planned_rails->contains(part) ||
								PortalRegistry::IsPortalTile(part) || (IsPlainRailTile(part) && GetTileOwner(part) == OWNER_NONE);
							if (part == end) break;
							if (conflict) break;
						}
						if (conflict) continue;
					}
					if (!Command<Commands::BuildBridge>::Do({}, end, tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE).Succeeded()) continue;
					TileIndex next = TileAddByDiagDir(end, travel);
					if (next >= Map::Size()) continue;
					uint nk = next.base() * 16 + uint(bridge_dir), nc = cost + span + 12;
					if (costs.contains(nk) && costs[nk] <= nc) continue;
					if (!previous.contains(nk) && previous.size() >= 29999) continue;
					if (options != nullptr && !previous.contains(nk) && !options->budget->TryState()) return false;
					costs[nk] = nc; previous[nk] = {key, straight, end}; queue.emplace(nc + 3 * DistanceManhattan(next, target), -int(nc), nk);
				}
			}
			if (last != UINT_MAX) break;
		}
		if (last == UINT_MAX) { IConsolePrint(CC_DEFAULT, "FREIGHT search industry={} station={} visited={} closest={} target={}", industry != nullptr ? industry->index.base() : UINT16_MAX, station.base(), previous.size(), closest, target.base()); continue; }
		leg = {station, depot, axis, dir, {{TileIndex{last / 16}, std::get<1>(previous[UINT_MAX])}}, {}};
		while (last != first) {
			auto [parent, track, bridge_end] = previous.at(last);
			if (bridge_end == INVALID_TILE) leg.rails.emplace_back(TileIndex{parent / 16}, track);
			else leg.bridges.emplace_back(TileIndex{parent / 16}, bridge_end);
			last = parent;
		}
		std::reverse(leg.rails.begin(), leg.rails.end());
		return true;
	}
	return false;
}

/**
 * Borrow native intervals for a quote plus one interval of working cash for tolls and monthly costs.
 * @param cost Next ordinary command's quoted cost.
 * @return Whether normal borrowing provides the required funds within the loan limit.
 */
bool FreightFunds(Money cost)
{
	const Company *c = Company::Get(CompanyID{0});
	while (c->money < cost + LOAN_INTERVAL) {
		if (!Result(Command<Commands::IncreaseLoan>::Do(DoCommandFlag::Execute, LoanCommand::Interval, Money{0}), "ordinary borrowing")) return false;
	}
	return true;
}

/**
 * Build a small local output service on its own track with ordinary commands.
 * @param industry Managed processor whose real output the service will transport.
 * @return Whether a paid output service was built and started successfully.
 */
bool FreightOnward(const Industry *industry)
{
	if (!IntegratedEconomy::Managed(industry) || industry->produced.empty()) {
		IConsolePrint(CC_ERROR, "FREIGHT FAIL output service requires a managed producing processor"); return false;
	}
	CargoType cargo = industry->produced.front().cargo;
	if (!IsValidCargoType(cargo)) { IConsolePrint(CC_ERROR, "FREIGHT FAIL invalid output cargo"); return false; }
	std::vector<std::tuple<uint, TileIndex, Axis, uint>> candidates;
	for (int dy = -4; dy <= int(industry->location.h) + 3; ++dy) for (int dx = -4; dx <= int(industry->location.w) + 3; ++dx) {
		int x = int(TileX(industry->location.tile)) + dx, y = int(TileY(industry->location.tile)) + dy;
		if (x <= 32 || y <= 32 || x + 32 >= int(Map::MaxX()) || y + 32 >= int(Map::MaxY())) continue;
		for (Axis axis : {Axis::X, Axis::Y}) for (uint length : {12u, 16u, 20u})
			candidates.emplace_back(length, TileXY(x, y), axis, length);
	}
	std::sort(candidates.begin(), candidates.end());
	for (auto [priority, pickup, axis, length] : candidates) {
		DiagDirection dir = axis == Axis::X ? DiagDirection::SW : DiagDirection::SE;
		TileIndex drop = pickup;
		for (uint i = 0; i < length; ++i) drop = TileAddByDiagDir(drop, dir);
		TileIndex depot = TileAddByDiagDir(pickup, ReverseDiagDir(dir));
		bool legal = true; Money cost = 75000;
		for (TileIndex tile = depot;; tile = TileAddByDiagDir(tile, dir)) {
			if (PlanetManager::GetTileWorld(tile) != PlanetManager::GetTileWorld(pickup) ||
				(!IsTileType(tile, TileType::Clear) && !IsTileType(tile, TileType::Trees))) { legal = false; break; }
			if (tile == TileAddByDiagDir(drop, dir)) break;
		}
		if (!legal) continue;
		for (auto station : {pickup, drop}) {
			auto quote = Command<Commands::BuildRailStation>::Do(DoCommandFlag::QueryCost, station, RAILTYPE_RAIL, axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false);
			if (!quote.Succeeded()) { legal = false; break; } cost += quote.GetCost();
		}
		for (TileIndex tile = TileAddByDiagDir(TileAddByDiagDir(pickup, dir), dir); tile != drop; tile = TileAddByDiagDir(tile, dir)) {
			auto quote = Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, DiagDirToDiagTrack(dir), false);
			if (!quote.Succeeded()) { legal = false; break; } cost += quote.GetCost();
		}
		auto depot_quote = Command<Commands::BuildRailDepot>::Do(DoCommandFlag::QueryCost, depot, RAILTYPE_RAIL, dir);
		if (!legal || !depot_quote.Succeeded()) continue;
		cost += depot_quote.GetCost();
		if (!FreightFunds(cost)) return false;
		for (auto station : {pickup, drop}) if (!Result(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, station, RAILTYPE_RAIL, axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false), "output station")) return false;
		for (TileIndex tile = TileAddByDiagDir(TileAddByDiagDir(pickup, dir), dir); tile != drop; tile = TileAddByDiagDir(tile, dir))
			if (!Result(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, DiagDirToDiagTrack(dir), false), "output rail")) return false;
		if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, dir), "output depot")) return false;
		if (!Result(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, drop, GetStationIndex(drop), "First freight output reserve"), "output warehouse")) return false;
		EngineID locomotive = EngineID::Invalid(), wagon = EngineID::Invalid(); Money engine_cost = INT64_MAX, wagon_cost = INT64_MAX;
		for (const Engine *e : Engine::Iterate()) {
			if (e->type != VehicleType::Train) continue;
			bool is_wagon = e->VehInfo<RailVehicleInfo>().railveh_type == RailVehicleType::Wagon;
			if (is_wagon && e->GetDefaultCargoType() != cargo && !e->info.refit_mask.Test(cargo)) continue;
			CargoType requested = is_wagon ? cargo : INVALID_CARGO;
			auto query = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, depot, e->index, false, requested, ClientID::Invalid);
			const auto &quote = ExtractCommandCost(query);
			if (!quote.Succeeded()) continue;
			if (is_wagon && quote.GetCost() < wagon_cost) { wagon = e->index; wagon_cost = quote.GetCost(); }
			if (!is_wagon && e->GetPower() > 0 && quote.GetCost() < engine_cost) { locomotive = e->index; engine_cost = quote.GetCost(); }
		}
		if (locomotive == EngineID::Invalid() || wagon == EngineID::Invalid()) { IConsolePrint(CC_ERROR, "FREIGHT FAIL no ordinary output consist"); return false; }
		if (!FreightFunds(engine_cost + wagon_cost)) return false;
		auto [ec, engine, a, b, c] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, locomotive, false, INVALID_CARGO, ClientID::Invalid);
		if (!Result(ec, "output locomotive")) return false;
		auto [wc, vehicle, d, e, f] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, wagon, false, cargo, ClientID::Invalid);
		if (!Result(wc, "output wagon") || !Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, vehicle, engine, false), "output coupling")) return false;
		for (uint8_t index = 0; index < 2; ++index) {
			Order o; o.MakeGoToStation(GetStationIndex(index == 0 ? pickup : drop));
			o.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
			if (index == 0) { o.SetLoadType(OrderLoadType::FullLoad); o.SetUnloadType(OrderUnloadType::NoUnload); } else o.SetLoadType(OrderLoadType::NoLoad);
			if (!Result(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, engine, VehicleOrderID{index}, o), "output order")) return false;
		}
		if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, engine, true), "output start")) return false;
		IConsolePrint(CC_DEFAULT, "FREIGHT onward {}", Json({{"cargo", cargo}, {"pickup", pickup.base()}, {"drop", drop.base()},
			{"depot", depot.base()}, {"engine", locomotive.base()}, {"wagon", wagon.base()}, {"construction_quote", int64_t(cost)},
			{"vehicle_quote", int64_t(engine_cost + wagon_cost)}}).dump());
		return true;
	}
	IConsolePrint(CC_ERROR, "FREIGHT FAIL bounded local output-service search exhausted"); return false;
}

/**
 * Execute the normal-start extension of the existing acceptance harness.
 * @param argv Console command, ordinary-start operation and its arguments.
 * @return True after handling the command, including reporting an invalid operation or failed proof step.
 */
bool FirstFreight(std::span<std::string_view> argv)
{
	bool simple = argv[1] == "first-freight-start" || argv[1] == "first-freight-status" || argv[1] == "first-freight-advance";
	bool route = argv[1] == "first-freight-plan" || argv[1] == "first-freight-build";
	if ((!simple && !route && argv[1] != "first-freight-onward") || (simple && argv.size() != 2) ||
		(route && argv.size() != 5) || (argv[1] == "first-freight-onward" && argv.size() != 3)) {
		IConsolePrint(CC_ERROR, "FREIGHT FAIL invalid ordinary-start harness arguments"); return true;
	}

	if (_game_mode != GameMode::Normal || (_networking && (!_network_dedicated || NetworkClientInfo::GetNumItems() > 1)) ||
		!IntegratedEconomy::Enabled() || PlanetManager::Count() != 7) {
		IConsolePrint(CC_ERROR, "FREIGHT FAIL requires isolated seven-world integrated game"); return true;
	}
	if (argv[1] == "first-freight-start") {
		if (Company::GetNumItems() != 0 || Vehicle::GetNumItems() != 0) { IConsolePrint(CC_ERROR, "FREIGHT FAIL requires fresh unowned game"); return true; }
		for (uint tile = 0; tile < Map::Size(); ++tile) if (IsPlainRailTile(TileIndex{tile}) && GetTileOwner(TileIndex{tile}) != OWNER_NONE) {
			IConsolePrint(CC_ERROR, "FREIGHT FAIL player assets at start"); return true;
		}
		if (DoStartupNewCompany(false, CompanyID{0}) == nullptr) return true;
		AutoRestoreBackup owner(_current_company, CompanyID{0});
		if (!Result(Command<Commands::RenameCompany>::Do(DoCommandFlag::Execute, "First Freight acceptance"), "company marker")) return true;
		_pause_mode.Set(PauseMode::Normal);
	}
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (company == nullptr || company->name != "First Freight acceptance") { IConsolePrint(CC_ERROR, "FREIGHT FAIL missing isolated company marker"); return true; }
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (argv[1] == "first-freight-onward" && argv.size() == 3) {
		const Industry *industry = Industry::GetIfValid(IndustryID{ParseInteger<uint16_t>(argv[2]).value_or(UINT16_MAX)});
		if (industry == nullptr) { IConsolePrint(CC_ERROR, "FREIGHT FAIL invalid processor"); return true; }
		FreightOnward(industry); return true;
	}
	if (argv[1] == "first-freight-advance") {
		Json before = FreightSnapshot(); CommonwealthSliceAudit audit;
		AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
		AutoRestoreBackup pause(_pause_mode); AutoRestoreBackup tick_owner(_current_company, _local_company);
		UpdateSignalsInBuffer(); _pause_mode.Reset();
		for (uint i = 0; i < 2048; ++i) StateGameLoop();
		Json after = FreightSnapshot(); after["audit"] = {{"produced", audit.produced}, {"unallocated", audit.unallocated},
			{"discarded", audit.discarded}, {"consumed", audit.consumed}, {"vehicle_deliveries", audit.vehicle_deliveries},
			{"cash_debits", audit.cash_debits}, {"gate_tolls", audit.gate_tolls}};
		after["cash_conserved"] = after["money"].get<int64_t>() == before["money"].get<int64_t>() - audit.cash_debits;
		after["cargo_errors"] = Json::array();
		for (CargoType c{0}; c < NUM_CARGO; ++c) {
			int64_t expected = before["held"][c].get<int64_t>() + audit.produced[c] - audit.unallocated[c] - audit.discarded[c] - audit.consumed[c];
			if (expected != after["held"][c].get<int64_t>()) after["cargo_errors"].push_back({c, expected, after["held"][c]});
		}
		IConsolePrint(CC_DEFAULT, "FREIGHT state {}", after.dump()); return true;
	}
	if ((argv[1] == "first-freight-plan" || argv[1] == "first-freight-build") && argv.size() == 5) {
		const Industry *source = Industry::GetIfValid(IndustryID{ParseInteger<uint16_t>(argv[2]).value_or(UINT16_MAX)});
		const Industry *dest = Industry::GetIfValid(IndustryID{ParseInteger<uint16_t>(argv[3]).value_or(UINT16_MAX)});
		const PortalLink *p = PortalRegistry::GetPortalLinkByID(PortalID{ParseInteger<uint32_t>(argv[4]).value_or(UINT32_MAX)});
		if (!source || !dest || !p || IntegratedEconomy::Role(PlanetManager::GetTileWorld(source->location.tile)) != EconomicRole::Frontier ||
			IntegratedEconomy::Role(PlanetManager::GetTileWorld(dest->location.tile)) != EconomicRole::Industrial || !IntegratedEconomy::Managed(dest)) {
			IConsolePrint(CC_ERROR, "FREIGHT FAIL invalid generated producer/processor route"); return true;
		}
		for (const auto &end : {p->end_a, p->end_b}) {
			auto policy = StellarNetwork::Policy(end.tile);
			if (!policy || !policy->public_access || Company::IsValidID(policy->owner)) { IConsolePrint(CC_ERROR, "FREIGHT FAIL requires neutral public backbone"); return true; }
		}
		if (argv[1] == "first-freight-build" && Vehicle::GetNumItems() != 0) { IConsolePrint(CC_ERROR, "FREIGHT FAIL starter service already exists"); return true; }
		CargoType cargo = INVALID_CARGO;
		for (const auto &c : source->produced) if (dest->IsCargoAccepted(c.cargo)) cargo = c.cargo;
		if (!IsValidCargoType(cargo)) { IConsolePrint(CC_ERROR, "FREIGHT FAIL incompatible cargo"); return true; }
		PortalEndpoint a = p->end_a, b = p->end_b;
		if (a.world_id != PlanetManager::GetTileWorld(source->location.tile)) std::swap(a, b);
		FreightLeg legs[2];
		if (b.world_id != PlanetManager::GetTileWorld(dest->location.tile) || !PlanFreightLeg(source, a, legs[0]) || !PlanFreightLeg(dest, b, legs[1])) {
			IConsolePrint(CC_DEFAULT, "FREIGHT plan {}", Json({{"legal", false}}).dump()); return true;
		}
		Json report = {{"legal", true}, {"cargo", cargo}, {"legs", Json::array()}}; Money cost = 0;
		for (const auto &leg : legs) {
			for (auto [start, end] : leg.bridges) cost += Command<Commands::BuildBridge>::Do({}, end, start, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE).GetCost();
			Json rails = Json::array();
			for (auto [tile, track] : leg.rails) { cost += Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_RAIL, track, false).GetCost(); rails.push_back({tile.base(), uint(track)}); }
			cost += Command<Commands::BuildRailStation>::Do({}, leg.station, RAILTYPE_RAIL, leg.axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false).GetCost();
		}
		cost += Command<Commands::BuildRailDepot>::Do({}, legs[0].depot, RAILTYPE_RAIL, legs[0].depot_dir).GetCost();
		report["construction_quote"] = int64_t(cost);
		for (const auto &leg : legs) { Json rails = Json::array(); for (auto [tile, track] : leg.rails) rails.push_back({tile.base(), uint(track)});
			Json bridges = Json::array(); for (auto [start, end] : leg.bridges) bridges.push_back({start.base(), end.base()});
			report["legs"].push_back({{"bridges", bridges}, {"station", leg.station.base()}, {"axis", uint(leg.axis)}, {"depot", leg.depot.base()}, {"depot_dir", uint(leg.depot_dir)}, {"rails", rails}}); }
		if (argv[1] == "first-freight-plan") { IConsolePrint(CC_DEFAULT, "FREIGHT plan {}", report.dump()); return true; }
		if (!FreightFunds(cost)) return true;
		for (const auto &leg : legs) {
			if (!Result(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, leg.station, RAILTYPE_RAIL, leg.axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false), "freight station")) return true;
			for (auto [start, end] : leg.bridges) {
				auto quote = Command<Commands::BuildBridge>::Do({}, end, start, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE);
				if (!FreightFunds(quote.GetCost()) || !Result(Command<Commands::BuildBridge>::Do(DoCommandFlag::Execute, end, start, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE), "freight bridge")) return true;
			}
			for (auto [tile, track] : leg.rails) if (!Result(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, track, false), "freight rail")) return true;
		}
		TileIndex depot = legs[0].depot;
		if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, legs[0].depot_dir), "freight depot")) return true;
		auto engine_quote = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, depot, EngineLocal(0x20), false, INVALID_CARGO, ClientID::Invalid);
		if (!Result(ExtractCommandCost(engine_quote), "engine quote") || !FreightFunds(ExtractCommandCost(engine_quote).GetCost())) return true;
		auto [ec, engine, x, y, z] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineLocal(0x20), false, INVALID_CARGO, ClientID::Invalid);
		if (!Result(ec, "Pioneer steam")) return true;
		/* Pioneer plus three ordinary half-tile hoppers fits the two-tile
		 * platforms. Capacity amortises the same gate trips and loan interest. */
		for (uint wagon_index = 0; wagon_index < 3; ++wagon_index) {
			auto wagon_quote = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, depot, EngineLocal(0x32), false, cargo, ClientID::Invalid);
			if (!Result(ExtractCommandCost(wagon_quote), "wagon quote") || !FreightFunds(ExtractCommandCost(wagon_quote).GetCost())) return true;
			auto [wc, wagon, q, v, w] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineLocal(0x32), false, cargo, ClientID::Invalid);
			if (!Result(wc, "hopper") || !Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, wagon, engine, false), "couple")) return true;
		}
		for (uint8_t index = 0; index < 2; ++index) {
			Order o; o.MakeGoToStation(GetStationIndex(legs[index].station)); o.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
			if (index == 0) { o.SetLoadType(OrderLoadType::FullLoad); o.SetUnloadType(OrderUnloadType::NoUnload); } else o.SetLoadType(OrderLoadType::NoLoad);
			if (!Result(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, engine, VehicleOrderID{index}, o), "freight order")) return true;
		}
		if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, engine, true), "freight start")) return true;
	}
	IConsolePrint(CC_DEFAULT, "FREIGHT state {}", FreightSnapshot().dump()); return true;
}

/** Exact marker and finite cash assistance for the reviewed offline functional proof. */
constexpr std::string_view FUNCTIONAL_COMPANY = "ASSISTED FUNCTIONAL Core Supply";
constexpr int64_t FUNCTIONAL_GRANT = 6000000; ///< Single owner-approved virtual cash allowance, in native GBP.
bool functional_fresh = false; ///< Revoked at every game start/load; never serialized.

/**
 * Full read-only semantics; retain the older ordinary FreightSnapshot contract.
 * @return Captured native state for accounting and cold/replay comparisons.
 */
Json FunctionalSnapshot()
{
	Json r = GenerationSnapshot();
	std::array<uint64_t, NUM_CARGO> raw{};
	for (const Industry *industry : Industry::Iterate()) if (!IntegratedEconomy::Managed(industry))
		for (const auto &p : industry->produced) if (IsValidCargoType(p.cargo)) raw[p.cargo] += p.waiting;
	r["raw_waiting"] = raw;
	r["all_held"] = r["held"];
	for (CargoType c{0}; c < NUM_CARGO; ++c) r["all_held"][c] = r["held"][c].get<uint64_t>() + raw[c];
	r["calendar_clock"] = {TimerGameCalendar::year.base(), TimerGameCalendar::month, TimerGameCalendar::date_fract, TimerGameCalendar::sub_date_fract};
	r["economy_clock"] = {TimerGameEconomy::year.base(), TimerGameEconomy::month, TimerGameEconomy::date.base(), TimerGameEconomy::date_fract, TimerGameEconomy::days_since_last_month};
	r["map_type_height_owner_fnv1a64"] = GetConnectedEconomyMapFingerprint();
	r["company"] = Company::Get(CompanyID{0})->name;
	r["money_fraction"] = Company::Get(CompanyID{0})->money_fraction;
	r["hqs"] = Json::array();
	for (const auto &hq : CorporateHQManager::GetAllHQ()) r["hqs"].push_back({hq.company_id.base(), hq.world_id.base(), hq.tile.base(), uint(hq.tier), hq.campus_name, hq.founding_date});
	r["tech_company_ids"] = Json::array();
	for (const auto &tech : TechTreeManager::GetAllCompanyTechStates()) r["tech_company_ids"].push_back(tech.company_id.base());
	r["pending_payments"] = Json::array();
	for (const CargoPayment *payment : CargoPayment::Iterate()) r["pending_payments"].push_back({payment->index.base(), payment->front->index.base(), int64_t(payment->route_profit), int64_t(payment->visual_profit), int64_t(payment->visual_transfer)});
	r["cargo_packets"] = Json::array();
	for (const CargoPacket *packet : CargoPacket::Iterate()) r["cargo_packets"].push_back({packet->index.base(), packet->Count(), packet->GetPeriodsInTransit(),
		int64_t(packet->GetFeederShare()), packet->GetFirstStation().base(), packet->GetNextHop().base(), packet->GetSourceXY().base()});
	for (Json &row : r["stations"]) {
		const Station *station = Station::Get(StationID{row["id"].get<uint16_t>()});
		row["packet_custody"] = Json::array();
		for (CargoType c{0}; c < NUM_CARGO; ++c) if (station->goods[c].HasData()) {
			const auto &cargo = station->goods[c].GetData().cargo;
			Json packets = Json::array();
			for (const auto &[next, list] : *cargo.Packets()) for (const CargoPacket *packet : list) packets.push_back({next.base(), packet->index.base()});
			if (cargo.TotalCount() > 0) row["packet_custody"].push_back({c, cargo.AvailableCount(), cargo.ReservedCount(), packets});
		}
	}
	for (Json &row : r["towns"]) {
		TownID id{row["id"].get<uint16_t>()};
		row["demand"] = IntegratedEconomy::CityDemand(id);
		const auto *city = IntegratedEconomy::City(id);
		row["reserves"] = city == nullptr ? std::map<CargoType, uint32_t>{} : city->reserves;
		row["consumed"] = city == nullptr ? std::map<CargoType, uint32_t>{} : city->consumed;
		const auto *profile = MegacityManager::GetProfile(id);
		row["profile"] = profile == nullptr ? Json{} : Json{{"name", profile->town_name}, {"world", profile->world_id.base()},
			{"population", profile->population}, {"quota", profile->monthly_quota}, {"current", profile->delivered_current},
			{"last", profile->delivered_last}, {"growth", profile->growth_multiplier}, {"passengers", profile->passenger_multiplier}};
	}
	for (Json &row : r["trains"]) {
		const Train *train = Train::Get(VehicleID{row["id"].get<uint32_t>()});
		row["owner"] = train->owner.base(); row["next"] = train->Next() == nullptr ? UINT32_MAX : train->Next()->index.base();
		row["cargo_reserved"] = train->cargo.ReservedCount();
		row["cargo_packets"] = Json::array();
		for (const CargoPacket *packet : *train->cargo.Packets()) row["cargo_packets"].push_back(packet->index.base());
		row["cargo_actions"] = {train->cargo.ActionCount(VehicleCargoList::MoveToAction::Transfer), train->cargo.ActionCount(VehicleCargoList::MoveToAction::Deliver),
			train->cargo.ActionCount(VehicleCargoList::MoveToAction::Keep), train->cargo.ActionCount(VehicleCargoList::MoveToAction::Load)};
		row["current_order_flags"] = {train->current_order.GetType(), train->current_order.GetLoadType(), train->current_order.GetUnloadType(), train->current_order.GetNonStopType().base(), train->current_order.GetStopLocation()};
		row["stopped"] = train->vehstatus.Test(VehState::Stopped);
		row["running_cost"] = int64_t(train->GetDisplayRunningCost());
		row["profit_this_year"] = int64_t(train->profit_this_year);
		row["profit_last_year"] = int64_t(train->profit_last_year);
	}
	return r;
}

/**
 * Serialize observer-only counters, including individual native cash entries.
 * @param a Counters collected while the optional observer was armed.
 * @return Native cargo, cash and monthly-consumption evidence.
 */
Json FunctionalAudit(const CommonwealthSliceAudit &a)
{
	return {{"produced", a.produced}, {"raw_produced", a.raw_produced}, {"raw_removed", a.raw_removed}, {"unallocated", a.unallocated}, {"discarded", a.discarded}, {"consumed", a.consumed},
		{"deliveries", a.deliveries}, {"vehicle_deliveries", a.vehicle_deliveries}, {"payments", a.payments}, {"cash_payments", a.cash_payments}, {"arrivals", a.arrivals},
		{"city_months", a.city_months}, {"processor_cargo", a.processor_cargo}, {"gate_tolls", a.gate_tolls}, {"vehicle_tolls", a.vehicle_tolls},
		{"service_income", a.service_income}, {"service_running", a.service_running}, {"expenses", a.expenses},
		{"cash_transactions", a.cash_transactions}, {"cash_debits", a.cash_debits}};
}

/**
 * Quote a bounded leg without introducing a depot or modifying the generated map.
 * @param leg Planned station, rails, bridges and optional depot.
 * @param depot Whether to include the leg's depot in the native quote.
 * @return Leg geometry, native cost and command-query eligibility.
 */
Json FunctionalLegQuote(const FreightLeg &leg, bool depot)
{
	Money cost = 0; bool legal = true;
	auto add = [&](const CommandCost &q) { legal &= q.Succeeded(); if (q.Succeeded()) cost += q.GetCost(); };
	Json rails = Json::array(), bridges = Json::array();
	for (auto [tile, track] : leg.rails) {
		add(Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_RAIL, track, false)); rails.push_back({tile.base(), uint(track)});
	}
	for (auto [start, end] : leg.bridges) {
		add(Command<Commands::BuildBridge>::Do({}, end, start, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE)); bridges.push_back({start.base(), end.base()});
	}
	add(Command<Commands::BuildRailStation>::Do({}, leg.station, RAILTYPE_RAIL, leg.axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false));
	if (depot) add(Command<Commands::BuildRailDepot>::Do({}, leg.depot, RAILTYPE_RAIL, leg.depot_dir));
	return {{"legal", legal}, {"cost", int64_t(cost)}, {"station", leg.station.base()}, {"axis", uint(leg.axis)},
		{"depot", leg.depot.base()}, {"depot_dir", uint(leg.depot_dir)}, {"rails", rails}, {"bridges", bridges}};
}

/**
 * Read loaded fixed-pack vehicle prices before there is a depot. These three
 * railv3 families have no callbacks/articulated parts. Exact native BuildVehicle
 * queries are still mandatory after paying for the depot, before buying a vehicle.
 * @param id Loaded fixed-pack engine to inspect.
 * @param cargo Desired refit cargo, or INVALID_CARGO for the default cargo.
 * @param world World used for native vehicle-availability checks.
 * @return Vehicle eligibility, cost, capacity and inherited running cost.
 */
Json FunctionalVehicleQuote(EngineID id, CargoType cargo, WorldID world)
{
	const Engine *e = Engine::Get(id);
	bool legal = e->type == VehicleType::Train && e->info.callback_mask == VehicleCallbackMasks{} &&
		IsEngineBuildable(id, VehicleType::Train, CompanyID{0}) &&
		CommonwealthPackManager::GetVehicleAvailabilityError(CompanyID{0}, id, world) == StringID{};
	Money cost = e->GetCost(); uint capacity = e->GetDisplayDefaultCapacity();
	CargoType base = e->GetDefaultCargoType();
	if (IsValidCargoType(cargo) && cargo != base) {
		legal &= e->info.refit_mask.Test(cargo);
		cost += GetPrice(Price::BuildVehicleWagon, e->info.refit_cost << 1, e->GetGRF(), -10);
		uint multiplier = e->info.misc_flags.Test(EngineMiscFlag::NoDefaultCargoMultiplier) ? 0x100 : CargoSpec::Get(base)->multiplier;
		capacity = (e->VehInfo<RailVehicleInfo>().capacity * CargoSpec::Get(cargo)->multiplier + multiplier / 2) / multiplier;
	}
	return {{"legal", legal}, {"id", id.base()}, {"cargo", cargo}, {"cost", int64_t(cost)}, {"capacity", capacity},
		{"refit_factor", e->info.refit_cost}, {"callback_mask", e->info.callback_mask.base()}, {"yearly_running_cost", int64_t(e->GetRunningCost())}};
}

/**
 * Shared pristine check for both arming and applying the one offline allowance.
 * @param company Existing canonical company whose finances are checked.
 * @return Whether fresh generation and unspent company state remain pristine.
 */
bool FunctionalPristine(const Company *company)
{
	if (!GetIntegratedCoreTownGenerationStats().active || !HasValidIntegratedCoreTown() || company->money != 100000 ||
		TimerGameCalendar::year.base() != 1950 || Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0 ||
		!CorporateHQManager::GetAllHQ().empty() || !MegacityManager::GetAllMegacities().empty() ||
		!LogisticsHubManager::GetAllHubs().empty() || !StockpileManager::GetAllStockpiles().empty() || !TechTreeManager::GetAllCompanyTechStates().empty()) return false;
	for (uint n = 0; n < Map::Size(); ++n) if ((IsPlainRailTile(TileIndex{n}) || IsRailDepotTile(TileIndex{n})) && GetTileOwner(TileIndex{n}) != OWNER_NONE) return false;
	for (const auto &year : company->yearly_expenses) for (Money amount : year) if (amount != 0) return false;
	return true;
}

/**
 * Wait for native startup completion before freezing the clock in the FIFO reader.
 * @param path Console input path to execute once modal generation is complete.
 */
void FunctionalBridge(const std::string &path)
{
	if (HasModalProgress()) {
		VideoDriver::GetInstance()->QueueOnMainThread([path]() { FunctionalBridge(path); });
		return;
	}
	IConsoleCmdExec(fmt::format("exec \"{}\"", path));
}

/**
 * Strict offline proof operations; no loan, industry, technology or cargo authoring.
 * @param argv Console command tokens including the requested proof operation.
 * @return True when handled, or false for unrecognized operation syntax.
 */
bool FunctionalCore(std::span<std::string_view> argv)
{
	/* A saved offline basket checkpoint retains development infinite money.
	 * Only its startup FIFO bridge may cross this earlier functional-proof gate. */
	bool basket_startup_bridge = argv.size() == 3 && argv[1] == "functional-bridge";
	if (argv.size() < 2 || _game_mode != GameMode::Normal || _networking || _network_dedicated ||
		!IntegratedEconomy::Enabled() || PlanetManager::Count() != 7 || Map::SizeX() != 1024 || Map::SizeY() != 1024 ||
		_settings_game.game_creation.generation_seed != 11 || (_settings_game.difficulty.infinite_money && !basket_startup_bridge)) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL functional proof requires isolated offline canonical seed11"); return true;
	}
	if (argv[1] == "functional-bridge" && argv.size() == 3) {
		/* Defer FIFO exec until native startup/generation has completed. */
		std::string path(argv[2]);
		if (path.find_first_of("\"\n\r") != std::string::npos) return false;
		_pause_mode.Set(PauseMode::Normal);
		VideoDriver::GetInstance()->QueueOnMainThread([path]() { FunctionalBridge(path); });
		return true;
	}
	Company *company = Company::GetIfValid(CompanyID{0});
	if (Company::GetNumItems() != 1 || company == nullptr || company->current_loan != 100000 || company->GetMaxLoan() != 300000) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL functional company/debt contract differs"); return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (argv[1] == "functional-start" && argv.size() == 2) {
		if (functional_fresh || !FunctionalPristine(company)) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL functional setup requires pristine fresh generation"); return true;
		}
		if (!Result(Command<Commands::RenameCompany>::Do(DoCommandFlag::Execute, std::string(FUNCTIONAL_COMPANY)), "functional company marker")) return true;
		functional_fresh = true; _pause_mode.Set(PauseMode::Normal);
	}
	if (company->name != FUNCTIONAL_COMPANY || !_pause_mode.Test(PauseMode::Normal)) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL missing paused ASSISTED FUNCTIONAL marker"); return true;
	}
	CommonwealthSliceAudit audit; AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
	Json before = FunctionalSnapshot(), report;
	if (argv[1] == "functional-start" || argv[1] == "functional-status") {
		IConsolePrint(CC_DEFAULT, "CONNECTED functional-state {}", before.dump()); return true;
	}
	if (argv[1] == "functional-grant" && argv.size() == 2) {
		if (!functional_fresh || !FunctionalPristine(company)) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL one grant requires fresh pristine offline setup, never reload"); return true;
		}
		functional_fresh = false; // Consume before posting; even a failure cannot authorize another grant.
		if (!Command<Commands::MoneyCheat>::Post(Money{FUNCTIONAL_GRANT})) { IConsolePrint(CC_ERROR, "CONNECTED FAIL offline native MoneyCheat rejected"); return true; }
		report = {{"label", "ASSISTED FUNCTIONAL"}, {"amount", FUNCTIONAL_GRANT}, {"source", "native offline MoneyCheat; virtual in-game GBP; owner-approved once"}};
	} else if (argv[1] == "functional-advance" && argv.size() == 2) {
		AutoRestoreBackup pause(_pause_mode); AutoRestoreBackup tick_owner(_current_company, _local_company);
		UpdateSignalsInBuffer(); _pause_mode.Reset();
		for (uint i = 0; i < 2048; ++i) StateGameLoop();
	} else if ((argv[1] == "functional-stop" || argv[1] == "functional-restart") && argv.size() == 3) {
		Train *train = Train::GetIfValid(VehicleID{ParseInteger<uint32_t>(argv[2]).value_or(UINT32_MAX)});
		if (train == nullptr || !train->IsFrontEngine() || train->owner != CompanyID{0}) { IConsolePrint(CC_ERROR, "CONNECTED FAIL invalid functional FOOD train"); return true; }
		bool stop = argv[1] == "functional-stop";
		if (train->vehstatus.Test(VehState::Stopped) != stop && !Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, train->index, true), "functional FOOD interruption")) return true;
	} else if (argv[1] == "functional-unlock" && argv.size() == 2) {
		if (CorporateHQManager::HasHQ(CompanyID{0})) { IConsolePrint(CC_ERROR, "CONNECTED FAIL HQ already placed"); return true; }
		Json query_before = FunctionalSnapshot();
		bool denied = Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_MATERIALS_1).Failed();
		if (!denied || FunctionalSnapshot() != query_before) { IConsolePrint(CC_ERROR, "CONNECTED FAIL no-HQ research rejection mutated state"); return true; }
		const Town *town = nullptr;
		for (const Town *candidate : Town::Iterate()) if (IntegratedEconomy::Role(PlanetManager::GetTileWorld(candidate->xy)) == EconomicRole::Core) { town = candidate; break; }
		if (town == nullptr) { IConsolePrint(CC_ERROR, "CONNECTED FAIL no living Core HQ world"); return true; }
		auto quote = Command<Commands::PlaceCorporateHQ>::Do({}, town->xy, "Core Supply HQ");
		if (!Result(quote, "functional HQ query") || !Result(Command<Commands::PlaceCorporateHQ>::Do(DoCommandFlag::Execute, town->xy, "Core Supply HQ"), "functional HQ purchase")) return true;
		report["hq_quote"] = int64_t(quote.GetCost()); report["no_hq_rejected"] = true;
		query_before = FunctionalSnapshot();
		if (Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_MATERIALS_2).Succeeded() || FunctionalSnapshot() != query_before) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL Materials II prerequisite rejection mutated state"); return true;
		}
		report["materials_ii_rejected"] = true;
		if (!Result(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_MATERIALS_1), "functional Materials I selection") ||
			!Result(Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 100000), "functional research budget")) return true;
	} else if (argv[1] == "functional-eligibility" && argv.size() == 2) {
		report["materials_i_unlocked"] = TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_1);
		report["materials_ii_selectable"] = Command<Commands::SelectResearchProject>::Do({}, TECH_MATERIALS_2).Succeeded();
		if (FunctionalSnapshot() != before) { IConsolePrint(CC_ERROR, "CONNECTED FAIL eligibility query mutated state"); return true; }
	} else if (argv[1] == "functional-budget-off" && argv.size() == 2) {
		if (!Result(Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 0), "functional research budget off")) return true;
	} else if ((argv[1] == "functional-plan" || argv[1] == "functional-build") && argv.size() == 7) {
		const Industry *producer = Industry::GetIfValid(IndustryID{ParseInteger<uint16_t>(argv[2]).value_or(UINT16_MAX)});
		const Industry *processor = Industry::GetIfValid(IndustryID{ParseInteger<uint16_t>(argv[3]).value_or(UINT16_MAX)});
		const Town *town = Town::GetIfValid(TownID{ParseInteger<uint16_t>(argv[4]).value_or(UINT16_MAX)});
		const PortalLink *links[2] = {
			PortalRegistry::GetPortalLinkByID(PortalID{ParseInteger<uint32_t>(argv[5]).value_or(UINT32_MAX)}),
			PortalRegistry::GetPortalLinkByID(PortalID{ParseInteger<uint32_t>(argv[6]).value_or(UINT32_MAX)})};
		CargoType grain = GetCargoTypeByLabel(CargoLabel{"GRAI"}), food = GetCargoTypeByLabel(CargoLabel{"FOOD"});
		if (producer == nullptr || processor == nullptr || town == nullptr || links[0] == nullptr || links[1] == nullptr ||
			producer->GetCargoProduced(grain) == producer->produced.end() || !IntegratedEconomy::Managed(processor) ||
			!processor->IsCargoAccepted(grain) || processor->GetCargoProduced(food) == processor->produced.end() || town->cache.population == 0 ||
			IntegratedEconomy::Role(PlanetManager::GetTileWorld(producer->location.tile)) != EconomicRole::Frontier ||
			IntegratedEconomy::Role(PlanetManager::GetTileWorld(processor->location.tile)) != EconomicRole::Industrial ||
			IntegratedEconomy::Role(PlanetManager::GetTileWorld(town->xy)) != EconomicRole::Core || Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL invalid fresh generated functional chain"); return true;
		}
		PortalEndpoint ends[4] = {links[0]->end_a, links[0]->end_b, links[1]->end_a, links[1]->end_b};
		WorldID industrial = PlanetManager::GetTileWorld(processor->location.tile);
		if (ends[0].world_id != PlanetManager::GetTileWorld(producer->location.tile)) std::swap(ends[0], ends[1]);
		if (ends[2].world_id != industrial) std::swap(ends[2], ends[3]);
		bool legal = ends[0].world_id == PlanetManager::GetTileWorld(producer->location.tile) && ends[1].world_id == industrial &&
			ends[2].world_id == industrial && ends[3].world_id == PlanetManager::GetTileWorld(town->xy);
		for (const auto &end : ends) { const auto *policy = StellarNetwork::Policy(end.tile); legal &= policy != nullptr && policy->public_access && !Company::IsValidID(policy->owner); }
		FreightLeg legs[4];
		legal = legal && PlanFreightLeg(producer, ends[0], legs[0]) && PlanFreightLeg(processor, ends[1], legs[1]) &&
			PlanFreightLeg(processor, ends[2], legs[2]) && PlanFreightLeg(nullptr, ends[3], legs[3], town);
		report = {{"legal", legal}, {"producer", producer->index.base()}, {"processor", processor->index.base()}, {"town", town->index.base()},
			{"grain_gate", links[0]->id.base()}, {"food_gate", links[1]->id.base()}, {"legs", Json::array()}, {"vehicles", Json::array()}};
		Money construction = 0, purchases = 0;
		if (legal) {
			std::set<TileIndex> occupied;
			for (uint n = 0; n < 4; ++n) {
				Json q = FunctionalLegQuote(legs[n], n % 2 == 0); construction += q["cost"].get<int64_t>(); legal &= q["legal"].get<bool>(); report["legs"].push_back(q);
				std::set<TileIndex> footprint{legs[n].station, TileAddByDiagDir(legs[n].station, legs[n].axis == Axis::X ? DiagDirection::SW : DiagDirection::SE)};
				if (n % 2 == 0) footprint.insert(legs[n].depot);
				for (auto [tile, track] : legs[n].rails) legal &= footprint.insert(tile).second;
				for (auto [start, end] : legs[n].bridges) {
					DiagDirection dir = TileX(end) > TileX(start) ? DiagDirection::SW : TileX(end) < TileX(start) ? DiagDirection::NE : TileY(end) > TileY(start) ? DiagDirection::SE : DiagDirection::NW;
					for (TileIndex tile = start;; tile = TileAddByDiagDir(tile, dir)) { legal &= footprint.insert(tile).second; if (tile == end) break; }
				}
				for (TileIndex tile : footprint) legal &= occupied.insert(tile).second;
			}
			EngineID engines[3] = {EngineLocal(0x20), EngineLocal(0x32), EngineLocal(0x34)};
			for (uint n = 0; n < 3; ++n) {
				Json q = FunctionalVehicleQuote(engines[n], n == 0 ? INVALID_CARGO : n == 1 ? grain : food, ends[n == 1 ? 0 : 2].world_id);
				legal &= q["legal"].get<bool>(); purchases += q["cost"].get<int64_t>() * (n == 0 ? 2 : 3); report["vehicles"].push_back(q);
			}
			/* The quoted Core station must cover a real own house before designation. */
			report["receiver_house"] = UINT32_MAX;
			for (uint n = 0; n < Map::Size(); ++n) {
				TileIndex house{n};
				if (IsTileType(house, TileType::House) && GetTownIndex(house) == town->index && PlanetManager::GetTileWorld(house) == ends[3].world_id &&
					int(TileX(house)) >= int(TileX(legs[3].station)) - 4 && TileX(house) <= TileX(legs[3].station) + 4 + (legs[3].axis == Axis::X) &&
					int(TileY(house)) >= int(TileY(legs[3].station)) - 4 && TileY(house) <= TileY(legs[3].station) + 4 + (legs[3].axis == Axis::Y)) { report["receiver_house"] = n; break; }
			}
			legal &= report["receiver_house"].get<uint32_t>() != UINT32_MAX;
		}
		report["legal"] = legal; report["construction_quote"] = int64_t(construction); report["vehicle_quote"] = int64_t(purchases);
		report["total_quote"] = int64_t(construction + purchases); report["preview_unchanged"] = FunctionalSnapshot() == before;
		if (argv[1] == "functional-plan") { IConsolePrint(CC_DEFAULT, "CONNECTED functional-plan {}", report.dump()); return true; }
		if (!legal || !report["preview_unchanged"].get<bool>() || construction + purchases > 1000000 || company->money < construction + purchases + 5100000 || MegacityManager::IsMegacity(town->index)) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL complete functional preflight rejected {}", report.dump()); return true;
		}
		if (!Result(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, town->index), "functional replicated designation")) return true;
		for (uint n = 0; n < 4; ++n) {
			const auto &leg = legs[n];
			if (!Result(Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, leg.station, RAILTYPE_RAIL, leg.axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false), "functional paid station")) return true;
			for (auto [start, end] : leg.bridges) if (!Result(Command<Commands::BuildBridge>::Do(DoCommandFlag::Execute, end, start, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE), "functional paid bridge")) return true;
			for (auto [tile, track] : leg.rails) if (!Result(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_RAIL, track, false), "functional paid rail")) return true;
			if (n % 2 == 0 && !Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, leg.depot, RAILTYPE_RAIL, leg.depot_dir), "functional paid depot")) return true;
		}
		const Station *drop = Station::Get(GetStationIndex(legs[3].station));
		if (drop->town != town || !drop->catchment_tiles.HasTile(TileIndex{report["receiver_house"].get<uint32_t>()}) || !MegacityManager::IsConsumerStation(drop) || LogisticsHubManager::GetHubForStation(drop->index) != nullptr) {
			IConsolePrint(CC_ERROR, "CONNECTED FAIL functional own-house receiver/custody differs"); return true;
		}
		report["services"] = Json::array();
		for (uint service = 0; service < 2; ++service) {
			uint origin = service * 2; VehicleID front = VehicleID::Invalid();
			for (uint n = 0; n < 4; ++n) {
				const Json &expected = report["vehicles"][n == 0 ? 0 : service + 1]; EngineID id{expected["id"].get<uint16_t>()};
				CargoType cargo = n == 0 ? INVALID_CARGO : service == 0 ? grain : food;
				auto query = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, legs[origin].depot, id, false, cargo, ClientID::Invalid);
				if (!Result(ExtractCommandCost(query), "functional native vehicle query") || int64_t(ExtractCommandCost(query).GetCost()) != expected["cost"].get<int64_t>() || std::get<2>(query) != expected["capacity"].get<uint>()) {
					IConsolePrint(CC_ERROR, "CONNECTED FAIL native vehicle cost/capacity differs from pinned preflight"); return true;
				}
				auto [cost, vehicle, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, legs[origin].depot, id, false, cargo, ClientID::Invalid);
				if (!Result(cost, "functional paid vehicle") || cost.GetCost() != ExtractCommandCost(query).GetCost() || capacity != std::get<2>(query)) return true;
				if (n == 0) front = vehicle;
				else if (!Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, vehicle, front, false), "functional coupling")) return true;
			}
			for (uint8_t n = 0; n < 2; ++n) {
				Order order; order.MakeGoToStation(GetStationIndex(legs[origin + n].station)); order.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
				if (n == 0) { order.SetLoadType(OrderLoadType::FullLoad); order.SetUnloadType(OrderUnloadType::NoUnload); } else order.SetLoadType(OrderLoadType::NoLoad);
				if (!Result(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, front, VehicleOrderID{n}, order), "functional orders")) return true;
			}
			if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, front, true), "functional service start")) return true;
			report["services"].push_back({{"train", front.base()}, {"cargo", service == 0 ? grain : food}, {"pickup", GetStationIndex(legs[origin].station).base()}, {"drop", GetStationIndex(legs[origin + 1].station).base()}});
		}
		if (audit.cash_debits != int64_t(construction + purchases)) { IConsolePrint(CC_ERROR, "CONNECTED FAIL functional construction query/execute total differs"); return true; }
	} else {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL invalid functional proof operation"); return true;
	}
	Json after = FunctionalSnapshot(); report["state"] = after; report["audit"] = FunctionalAudit(audit);
	report["cash_conserved"] = after["money"].get<int64_t>() == before["money"].get<int64_t>() - audit.cash_debits;
	report["cargo_errors"] = Json::array();
	for (CargoType c{0}; c < NUM_CARGO; ++c) {
		int64_t expected = before["all_held"][c].get<int64_t>() + audit.produced[c] + audit.raw_produced[c] - audit.raw_removed[c] - audit.unallocated[c] - audit.discarded[c] - audit.consumed[c];
		if (expected != after["all_held"][c].get<int64_t>()) report["cargo_errors"].push_back({c, expected, after["all_held"][c]});
	}
	IConsolePrint(CC_DEFAULT, "CONNECTED functional-result {}", report.dump()); return true;
}

/** @cond CoreBasketProofInternals */
constexpr std::string_view BASKET_SOURCE = "206acdd9485bed64927197af89e07a70926e0b1e386ed51cb93744df2c78e2f7";

/** Frozen native construction operation, retaining reused pieces as explicit zero quotes. */
struct BasketCommand {
	enum class Kind { Station, Depot, Rail, Bridge, Signal } kind;
	TileIndex tile = INVALID_TILE, end = INVALID_TILE;
	Track track = Track::Invalid;
	Axis axis = Axis::X;
	DiagDirection direction = DiagDirection::Invalid;
	uint8_t signal = 0;
	int64_t cost = 0;
	bool reuse = false;
};
struct BasketLayout {
	std::array<FreightLeg, 8> legs;
	std::vector<BasketCommand> commands;
	Json report;
	Json state;
};
/** Deliberately transient: loading never restores initial spending authorization. */
struct BasketSession {
	bool armed = false, built = false, cold = false, failed = false;
	uint layouts = 0, invocations = 0, advances = 0, initial_advances = 0;
	int64_t gross = 0, quoted_total = 0, tick_reserve = 0;
	std::string error;
	Json expected, services = Json::array();
	std::array<std::optional<BasketLayout>, 2> plans;
} basket;

bool BasketFail(std::string_view error)
{
	if (!basket.failed) basket.error = error;
	basket.failed = true;
	return false;
}

/** Exact published content and fixed canonical native profile; no compatible substitution. */
bool BasketProfile()
{
	if (!IntegratedEconomy::ContentReady() || PlanetManager::Count() != 7 || Map::SizeX() != 1024 || Map::SizeY() != 1024 ||
		_settings_game.game_creation.generation_seed != 11 || !_settings_game.difficulty.infinite_money ||
		!_settings_game.game_creation.player_built_economy || FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{0}) ||
		!_settings_game.game_creation.cst_sector || _settings_game.game_creation.landscape != LandscapeType::Temperate ||
		_settings_game.difficulty.disasters || to_underlying(_settings_game.difficulty.vehicle_breakdowns) != 0 ||
		_settings_game.difficulty.vehicle_costs != 0 || _settings_game.difficulty.construction_cost != 0 || _settings_game.economy.inflation ||
		to_underlying(_settings_game.economy.timekeeping_units) != 0 || to_underlying(_settings_game.vehicle.train_acceleration_model) != 1 ||
		!_settings_game.pf.forbid_90_deg || !_settings_game.order.improved_load || !_settings_game.order.gradual_loading || !_settings_game.order.no_servicing_if_no_breakdowns ||
		!_settings_game.station.modified_catchment || _grfconfig.size() != 3) return false;
	/* Native NGRF identity hashes cover the GRF data section, not the whole file.
	 * Whole-file SHA256 pins remain in the wrapper's immutable source manifest. */
	const std::array<uint8_t, 16> industry_md5 = {0x3c, 0x8d, 0x54, 0xab, 0x58, 0x86, 0xb2, 0xa5, 0x6a, 0xab, 0x10, 0x83, 0xc5, 0x2b, 0x24, 0xbb};
	const std::array<uint8_t, 16> rail_md5 = {0x6a, 0xb2, 0x37, 0x72, 0xe9, 0x34, 0x26, 0x06, 0x59, 0xdf, 0x68, 0x36, 0x68, 0x63, 0x17, 0xb9};
	const std::array<uint8_t, 16> equipment_md5 = {0x6c, 0x02, 0xe7, 0xb5, 0xf8, 0x98, 0xef, 0x6b, 0x16, 0xfe, 0x70, 0xb3, 0x3d, 0x11, 0x22, 0xb0};
	const auto *industry = GetGRFConfig(COMMONWEALTH_INDUSTRY_GRFID), *rail = GetGRFConfig(COMMONWEALTH_RAIL_GRFID);
	const auto *equipment = GetGRFConfig(GrfID{"OST\x05"});
	return industry != nullptr && rail != nullptr && equipment != nullptr && industry->version == 5 && rail->version == 3 && equipment->version == 1 &&
		industry->ident.md5sum == industry_md5 && rail->ident.md5sum == rail_md5 && equipment->ident.md5sum == equipment_md5 &&
		equipment->status == GRFStatus::Activated && !equipment->flags.Any({GRFConfigFlag::Invalid, GRFConfigFlag::Compatible}) && Game::GetInstance() == nullptr;
}

/** Read-only evidence kept outside the unchanged historical FunctionalSnapshot. */
Json BasketObservation()
{
	Json result = {{"paused", _pause_mode.Test(PauseMode::Normal)}, {"pause_mode", _pause_mode.base()},
		{"cargo_labels", Json::object()}, {"towns", Json::array()}, {"private_rail", Json::array()}, {"private_structures", Json::array()}};
	for (TileIndex tile : Map::Iterate()) {
		if (IsPlainRailTile(tile) && GetTileOwner(tile) == CompanyID{0}) {
			Json signals = Json::array();
			if (HasSignals(tile)) for (Track track : GetTrackBits(tile)) if (HasSignalOnTrack(tile, track))
				signals.push_back({uint(track), uint(GetSignalType(tile, track)), uint(GetSignalVariant(tile, track))});
			result["private_rail"].push_back({{"tile", tile.base()}, {"railtype", uint(GetRailType(tile))}, {"tracks", GetTrackBits(tile).base()},
				{"signal_present", HasSignals(tile) ? GetPresentSignals(tile) : 0}, {"signal_state", HasSignals(tile) ? GetSignalStates(tile) : 0},
				{"reservation", GetRailReservationTrackBits(tile).base()}, {"signals", signals}});
		}
		if (IsRailDepotTile(tile) && GetTileOwner(tile) == CompanyID{0}) result["private_structures"].push_back({{"tile", tile.base()}, {"type", "depot"},
			{"railtype", uint(GetRailType(tile))}, {"dir", uint(GetRailDepotDirection(tile))}, {"reservation", HasDepotReservation(tile)}});
		if (IsLevelCrossingTile(tile) && GetTileOwner(tile) == CompanyID{0}) result["private_structures"].push_back({{"tile", tile.base()}, {"type", "crossing"},
			{"railtype", uint(GetRailType(tile))}, {"track", uint(GetCrossingRailTrack(tile))}, {"reservation", HasCrossingReservation(tile)}});
		if (IsBridgeTile(tile) && GetTileOwner(tile) == CompanyID{0} && GetTunnelBridgeTransportType(tile) == TransportType::Rail)
			result["private_structures"].push_back({{"tile", tile.base()}, {"type", "bridge"}, {"railtype", uint(GetRailType(tile))},
				{"dir", uint(GetTunnelBridgeDirection(tile))}, {"other_end", GetOtherBridgeEnd(tile).base()}, {"reservation", HasTunnelBridgeReservation(tile)}});
	}
	for (const CargoSpec *cargo : CargoSpec::Iterate()) result["cargo_labels"][cargo->label.AsString()] = cargo->Index();
	for (const Town *town : Town::Iterate()) {
		const auto *profile = MegacityManager::GetProfile(town->index);
		const auto *city = IntegratedEconomy::City(town->index);
		result["towns"].push_back({{"id", town->index.base()}, {"population", town->cache.population}, {"house_count", town->cache.num_houses},
			{"native_growth_rate", town->growth_rate}, {"native_grow_counter", town->grow_counter}, {"native_flags", town->flags.base()},
			{"native_growth_enabled", town->flags.Test(TownFlag::IsGrowing) && town->growth_rate != TOWN_GROWTH_RATE_NONE},
			{"native_growth_hook_enabled", !town->flags.Test(TownFlag::CustomGrowth) && town->growth_rate != TOWN_GROWTH_RATE_NONE},
			{"growth_state", profile == nullptr ? UINT8_MAX : uint(profile->growth_state)},
			{"growth", profile == nullptr ? 0.0f : profile->growth_multiplier}, {"passengers", profile == nullptr ? 0.0f : profile->passenger_multiplier},
			{"demand", IntegratedEconomy::CityDemand(town->index)}, {"reserves", city == nullptr ? std::map<CargoType, uint32_t>{} : city->reserves},
			{"consumed", city == nullptr ? std::map<CargoType, uint32_t>{} : city->consumed}});
	}
	using Custody = std::array<uint64_t, NUM_CARGO>;
	std::map<std::string, Custody> custody;
	for (const char *name : {"industry_inputs", "industry_outputs", "stations", "trains", "cities", "stocks", "research", "total"}) custody[name] = {};
	for (const Industry *industry : Industry::Iterate()) {
		for (const auto &cargo : industry->accepted) if (IsValidCargoType(cargo.cargo)) custody["industry_inputs"][cargo.cargo] += cargo.waiting;
		for (const auto &cargo : industry->produced) if (IsValidCargoType(cargo.cargo)) custody["industry_outputs"][cargo.cargo] += cargo.waiting;
	}
	for (const Station *station : Station::Iterate()) for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) custody["stations"][cargo] += station->goods[cargo].TotalCount();
	for (const Train *train : Train::Iterate()) if (IsValidCargoType(train->cargo_type)) custody["trains"][train->cargo_type] += train->cargo.StoredCount();
	for (const Town *town : Town::Iterate()) if (auto city = IntegratedEconomy::City(town->index)) for (auto [cargo, units] : city->reserves) custody["cities"][cargo] += units;
	for (const auto &stock : StockpileManager::GetAllStockpiles()) for (auto [cargo, units] : stock.inventory) custody["stocks"][cargo] += units;
	for (const auto &tech : TechTreeManager::GetAllCompanyTechStates()) if (auto research = IntegratedEconomy::Research(tech.company_id))
		for (auto [cargo, units] : research->reserved) custody["research"][cargo] += units;
	for (const auto &[name, units] : custody) if (name != "total") for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) custody["total"][cargo] += units[cargo];
	result["custody"] = custody;
	const Company *company = Company::Get(CompanyID{0});
	result["finance_guard"] = {{"interest_rate", _economy.interest_rate}, {"station_value", int64_t(_price[Price::StationValue])},
		{"infrastructure_maintenance", _settings_game.economy.infrastructure_maintenance}, {"autorenew", company->settings.engine_renew},
		{"autorenew_months", company->settings.engine_renew_months}, {"autorenew_money", company->settings.engine_renew_money},
		{"development_unlimited_money", _settings_game.difficulty.infinite_money},
		{"replacement_rules", company->engine_renew_list != nullptr}, {"ai", company->is_ai}};
	result["vehicle_guard"] = Json::array();
	for (const Train *train : Train::Iterate()) result["vehicle_guard"].push_back({{"id", train->index.base()}, {"age", train->age.base()},
		{"max_age", train->max_age.base()}, {"speed", train->cur_speed},
		{"max_speed", train->IsFrontEngine() ? train->vcache.cached_max_speed : 0}, {"progress", train->progress}});
	result["adapter"] = {{"armed", basket.armed}, {"built", basket.built}, {"cold", basket.cold}, {"failed", basket.failed}, {"error", basket.error},
		{"layout_candidates", basket.layouts}, {"endpoint_invocations", basket.invocations}, {"advances", basket.advances}, {"initial_advances", basket.initial_advances},
		{"gross_debits", basket.gross}, {"quoted_total", basket.quoted_total}, {"tick_debit_reserve", basket.tick_reserve}, {"development_unlimited_money", _settings_game.difficulty.infinite_money}};
	return result;
}

bool BasketReceiver()
{
	const Station *station = Station::GetIfValid(StationID{3});
	const Town *town = Town::GetIfValid(TownID{0});
	if (town == nullptr || station == nullptr || station->owner != CompanyID{0} || station->town != town ||
		LogisticsHubManager::GetHubForStation(station->index) != nullptr || !MegacityManager::IsConsumerStation(station) || station->train_station.IsEmpty()) return false;
	for (BitmapTileIterator it(station->catchment_tiles); *it != INVALID_TILE; ++it)
		if (IsTileType(*it, TileType::House) && GetTownIndex(*it) == TownID{0} && PlanetManager::GetTileWorld(*it) == WorldID{0}) return true;
	return false;
}

bool BasketChain()
{
	const std::array<uint16_t, 4> ids = {6, 7, 2, 4};
	const std::array<TileIndex, 4> tiles = {TileXY(600, 335), TileXY(597, 222), TileXY(816, 518), TileXY(819, 237)};
	const std::array<WorldID, 4> worlds = {WorldID{2}, WorldID{1}, WorldID{3}, WorldID{1}};
	const std::array<const char *, 4> labels = {"IRON", "STEL", "SILC", "BALL"};
	for (uint i = 0; i < ids.size(); ++i) {
		const Industry *industry = Industry::GetIfValid(IndustryID{ids[i]});
		CargoLabel label; std::copy_n(labels[i], 4, label.begin()); CargoType cargo = GetCargoTypeByLabel(label);
		if (industry == nullptr || industry->location.tile != tiles[i] || PlanetManager::GetTileWorld(tiles[i]) != worlds[i] ||
			industry->GetCargoProduced(cargo) == industry->produced.end() || Company::IsValidID(industry->founder)) return false;
		if (i % 2 == 1 && (!IntegratedEconomy::Managed(industry) || !industry->IsCargoAccepted(GetCargoTypeByLabel(CargoLabel{i == 1 ? "IRON" : "SILC"})))) return false;
	}
	for (uint gate : {1, 2, 3}) {
		const auto *link = PortalRegistry::GetPortalLinkByID(PortalID{gate});
		if (link == nullptr) return false;
		for (const auto &end : {link->end_a, link->end_b}) {
			const auto *policy = StellarNetwork::Policy(end.tile);
			auto terminal = PortalTerminal::Plan(end.tile, end.enter_dir, end.world_id);
			if (policy == nullptr || !policy->public_access || Company::IsValidID(policy->owner) || !terminal || GetTileOwner(end.tile) != OWNER_NONE) return false;
			for (const auto &part : terminal->tiles) if (!IsPlainRailTile(part.tile) || GetTileOwner(part.tile) != OWNER_NONE || !GetTrackBits(part.tile).All(part.tracks)) return false;
			for (const auto &signal : terminal->signals) if (!HasSignalOnTrack(signal.tile, signal.track) ||
				GetSignalType(signal.tile, signal.track) != SignalType::PathOneWay || GetSignalVariant(signal.tile, signal.track) != SignalVariant::Electric ||
				GetPresentSignals(signal.tile) != SignalAlongTrackdir(DiagDirToDiagTrackdir(signal.travel_dir))) return false;
		}
	}
	return BasketReceiver();
}

CommandCost BasketNativeCommand(const BasketCommand &command, DoCommandFlags flags)
{
	switch (command.kind) {
		case BasketCommand::Kind::Station: return Command<Commands::BuildRailStation>::Do(flags, command.tile, RAILTYPE_RAIL, command.axis, 1, 2, STAT_CLASS_DFLT, 0, NEW_STATION, true);
		case BasketCommand::Kind::Depot: return Command<Commands::BuildRailDepot>::Do(flags, command.tile, RAILTYPE_RAIL, command.direction);
		case BasketCommand::Kind::Rail: return Command<Commands::BuildRail>::Do(flags, command.tile, RAILTYPE_RAIL, command.track, false);
		case BasketCommand::Kind::Bridge: return Command<Commands::BuildBridge>::Do(flags, command.end, command.tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE);
		case BasketCommand::Kind::Signal: return Command<Commands::BuildSignal>::Do(flags, command.tile, command.track, SignalType::Path, SignalVariant::Electric,
			false, false, false, SignalType::Path, SignalType::Path, 0, command.signal);
	}
	return CMD_ERROR;
}

/** Freeze every future footprint and incremental native quote, including shared junctions. */
bool PlanBasket(uint layout, BasketLayout &plan)
{
	plan.state = FunctionalSnapshot();
	plan.report = {{"version", 1}, {"layout", layout}, {"legal", false}, {"frozen", false}, {"legs", Json::array()},
		{"search", Json::array()}, {"services", Json::array()}, {"commands", Json::array()}, {"reason", "future-footprint-geometry"},
		{"construction_quote", 0}, {"vehicle_quote", 0}, {"total_quote", 0}};
	std::map<TileIndex, TrackBits> rails;
	std::set<TileIndex> blocked;
	std::map<TileIndex, std::pair<Track, uint8_t>> signals;
	Money construction = 0, vehicles = 0;
	auto add = [&](BasketCommand command, const CommandCost &quote) {
		if (quote.Failed()) {
			plan.report["failed_quote"] = {{"kind", uint(command.kind)}, {"tile", command.tile.base()}, {"track", uint(command.track)},
				{"error", quote.GetErrorMessage() == INVALID_STRING_ID ? "native command failure" : GetString(quote.GetErrorMessage())}};
			plan.report["reason"] = "native-construction-quote";
			return false;
		}
		command.cost = int64_t(quote.GetCost()); construction += quote.GetCost();
		plan.report["construction_quote"] = int64_t(construction); plan.report["total_quote"] = int64_t(construction + vehicles);
		plan.commands.push_back(command);
		plan.report["commands"].push_back({{"kind", uint(command.kind)}, {"tile", command.tile.base()}, {"end", command.end.base()},
			{"track", uint(command.track)}, {"axis", uint(command.axis)}, {"direction", uint(command.direction)}, {"signal", command.signal},
			{"cost", command.cost}, {"reuse", command.reuse}});
		return true;
	};
	const std::array<uint16_t, 8> industries = {6, 7, 7, UINT16_MAX, 2, 4, 4, UINT16_MAX};
	const std::array<uint, 8> gates = {2, 2, 1, 1, 3, 3, 1, 1};
	const std::array<WorldID, 8> worlds = {WorldID{2}, WorldID{1}, WorldID{1}, WorldID{0}, WorldID{3}, WorldID{1}, WorldID{1}, WorldID{0}};
	const Station *receiver = Station::Get(StationID{3});
	for (uint i = 0; i < 8; ++i) {
		++basket.invocations;
		const PortalLink *link = PortalRegistry::GetPortalLinkByID(PortalID{gates[i]});
		const PortalEndpoint &end = link->end_a.world_id == worlds[i] ? link->end_a : link->end_b;
		if (end.world_id != worlds[i]) return false;
		CoreBasketSearchBudget budget;
		BasketLegOptions options{&budget, &rails, &blocked, i % 4 == 3 ? receiver->train_station.tile : INVALID_TILE,
			i % 4 == 3 ? GetRailStationAxis(receiver->train_station.tile) : Axis::X, i % 2 == 0, layout};
		bool found = PlanFreightLeg(Industry::GetIfValid(IndustryID{industries[i]}), end, plan.legs[i], i % 4 == 3 ? Town::Get(TownID{0}) : nullptr, &options);
		plan.report["search"].push_back({{"endpoint", i}, {"candidates", budget.candidates}, {"states", budget.states}, {"cap", CoreBasketSearchBudget::LIMIT},
			{"exhausted", budget.exhausted}, {"found", found}});
		if (!found) { plan.report["reason"] = budget.exhausted ? "endpoint-state-budget" : "bounded-endpoint-no-route"; return false; }
		const FreightLeg &leg = plan.legs[i];
		if (i % 4 != 3) {
			const Industry *industry = Industry::Get(IndustryID{industries[i]});
			TileIndex second = TileAddByDiagDir(leg.station, leg.axis == Axis::X ? DiagDirection::SW : DiagDirection::SE);
			bool catches = false;
			for (TileIndex tile : industry->location) if (IsTileType(tile, TileType::Industry) && GetIndustryIndex(tile) == industry->index)
				catches |= std::min(DistanceMax(tile, leg.station), DistanceMax(tile, second)) <= CA_TRAIN;
			if (!catches) return false;
			for (TileIndex part : {leg.station, TileAddByDiagDir(leg.station, leg.axis == Axis::X ? DiagDirection::SW : DiagDirection::SE)})
				if (blocked.contains(part) || rails.contains(part) || PlanetManager::GetTileWorld(part) != worlds[i] || !blocked.insert(part).second) return false;
			BasketCommand station{BasketCommand::Kind::Station, leg.station}; station.axis = leg.axis;
			if (!add(station, BasketNativeCommand(station, {}))) return false;
			if (i % 2 == 0) {
				if (blocked.contains(leg.depot) || rails.contains(leg.depot) || PlanetManager::GetTileWorld(leg.depot) != worlds[i] || !blocked.insert(leg.depot).second) return false;
				BasketCommand depot{BasketCommand::Kind::Depot, leg.depot}; depot.direction = leg.depot_dir;
				for (const auto &[gate, portal] : PortalRegistry::GetAllPortals()) for (const auto &head : {portal.end_a, portal.end_b})
					if (DistanceMax(depot.tile, head.tile) <= 2) return false;
				if (!add(depot, BasketNativeCommand(depot, {}))) return false;
			}
		}
		for (auto [start, finish] : leg.bridges) {
			bool reuse = IsBridgeTile(start) && GetTileOwner(start) == CompanyID{0} && GetOtherBridgeEnd(start) == finish &&
				GetTileOwner(finish) == CompanyID{0} && GetTunnelBridgeTransportType(start) == TransportType::Rail && GetRailType(start) == RAILTYPE_RAIL;
			if (!reuse) {
				if (IsPlainRailTile(start) || IsPlainRailTile(finish)) return false;
				DiagDirection direction = TileX(finish) > TileX(start) ? DiagDirection::SW : TileX(finish) < TileX(start) ? DiagDirection::NE : TileY(finish) > TileY(start) ? DiagDirection::SE : DiagDirection::NW;
				if (DistanceManhattan(start, finish) > 16) return false;
				for (TileIndex part = start;; part = TileAddByDiagDir(part, direction)) {
					if (blocked.contains(part) || rails.contains(part) || PortalRegistry::IsPortalTile(part) ||
						(IsPlainRailTile(part) && GetTileOwner(part) == OWNER_NONE)) return false;
					blocked.insert(part); if (part == finish) break;
				}
			}
			BasketCommand bridge{BasketCommand::Kind::Bridge, start, finish}; bridge.reuse = reuse;
			if (!add(bridge, reuse ? CommandCost() : BasketNativeCommand(bridge, {}))) return false;
		}
		for (auto [tile, track] : leg.rails) {
			if (blocked.contains(tile)) return false;
			TrackBits previous = rails[tile];
			bool reuse = previous.Test(track) || (IsPlainRailTile(tile) && GetTileOwner(tile) == CompanyID{0} && GetTrackBits(tile).Test(track)) ||
				(IsLevelCrossingTile(tile) && GetTileOwner(tile) == CompanyID{0} && GetCrossingRailTrack(tile) == track);
			BasketCommand rail{BasketCommand::Kind::Rail, tile}; rail.track = track; rail.reuse = reuse;
			if (!add(rail, QueryCoreBasketRail(tile, track, previous))) return false;
			rails[tile].Set(track);
		}
		plan.report["legs"].push_back({{"station", leg.station.base()}, {"axis", uint(leg.axis)}, {"depot", leg.depot.base()},
			{"depot_dir", uint(leg.depot_dir)}, {"existing_station", i % 4 == 3}, {"depot_required", i % 2 == 0}, {"industry", industries[i]}});
		/* A back-passable outbound path signal protects each private station exit.
		 * No opposing/interior signal can admit a train into a blocked single-track stub. */
		if (leg.rails.empty()) return false;
		auto [tile, track] = leg.rails.front();
		Trackdir travel = TrackEnterdirToTrackdir(track, leg.depot_dir);
		if (!IsValidTrackdir(travel)) return false;
		uint8_t present = SignalAlongTrackdir(travel);
		if (signals.contains(tile) && signals.at(tile) != std::pair{track, present}) return false;
		signals[tile] = {track, present};
	}
	/* Existing grain/FOOD platforms use the same protected shared junctions. */
	for (uint16_t station_id = 0; station_id < 4; ++station_id) {
		const Station *station = Station::Get(StationID{station_id});
		TileIndex platform = station->train_station.tile;
		DiagDirection positive = GetRailStationAxis(platform) == Axis::X ? DiagDirection::SW : DiagDirection::SE;
		TileIndex opposite_platform_end = TileAddByDiagDir(platform, positive);
		bool found = false;
		for (auto [tile, enter] : {std::pair{TileAddByDiagDir(platform, ReverseDiagDir(positive)), ReverseDiagDir(positive)}, std::pair{TileAddByDiagDir(opposite_platform_end, positive), positive}}) {
			if (!IsPlainRailTile(tile) || GetTileOwner(tile) != CompanyID{0} || GetTrackBits(tile).Count() != 1) continue;
			Track track = FindFirstTrack(GetTrackBits(tile)); Trackdir direction = TrackEnterdirToTrackdir(track, enter);
			if (!IsValidTrackdir(direction)) continue;
			uint8_t present = SignalAlongTrackdir(direction);
			if (signals.contains(tile) && signals.at(tile) != std::pair{track, present}) return false;
			signals[tile] = {track, present}; found = true;
		}
		if (!found) return false;
	}
	for (auto [tile, signal] : signals) {
		auto [track, present] = signal;
		TrackBits bits = rails.contains(tile) ? rails.at(tile) : TrackBits{};
		if (IsPlainRailTile(tile)) bits |= GetTrackBits(tile);
		if (!bits.Test(track) || TracksOverlap(bits)) return false;
		BasketCommand command{BasketCommand::Kind::Signal, tile}; command.track = track; command.signal = present;
		bool existing = IsPlainRailTile(tile) && HasSignalOnTrack(tile, track);
		if (existing && (GetSignalType(tile, track) != SignalType::Path || GetSignalVariant(tile, track) != SignalVariant::Electric || GetPresentSignals(tile) != present)) return false;
		command.reuse = existing;
		CommandCost quote = existing ? CommandCost() : IsPlainRailTile(tile) ? BasketNativeCommand(command, {}) :
			CommandCost(ExpensesType::Construction, GetNewRailSignalCost(PlanetManager::GetTileWorld(tile), CompanyID{0}));
		if (!add(command, quote)) return false;
	}
	plan.report["receiver_capacity"] = Json::array();
	for (auto label : {CargoLabel{"STEL"}, CargoLabel{"BALL"}}) {
		CargoType cargo = GetCargoTypeByLabel(label);
		uint32_t capacity = IntegratedEconomy::AcceptCity(TownID{0}, cargo, UINT32_MAX, false);
		plan.report["receiver_capacity"].push_back({cargo, capacity});
		if (capacity == 0) { plan.report["reason"] = "receiver-capacity"; return false; }
	}
	const std::array<const char *, 4> labels = {"IRON", "STEL", "SILC", "BALL"};
	const std::array<uint, 4> locals = {0x32, 0x33, 0x32, 0x38};
	const std::array<uint, 4> wagons = {3, 1, 3, 1};
	TileIndex query_depot = TileXY(336, 199);
	for (uint service = 0; service < 4; ++service) {
		CargoLabel label; std::copy_n(labels[service], 4, label.begin()); CargoType cargo = GetCargoTypeByLabel(label);
		EngineID locomotive = EngineLocal(0x20), wagon = EngineID::Invalid();
		for (const Engine *engine : Engine::Iterate()) if (engine->type == VehicleType::Train && engine->grf_prop.local_id == locals[service] &&
			engine->grf_prop.grfid == (service == 3 ? COMMONWEALTH_INDUSTRY_GRFID : COMMONWEALTH_RAIL_GRFID)) { wagon = engine->index; break; }
		if (locomotive == EngineID::Invalid() || wagon == EngineID::Invalid()) return false;
		Json quotes = Json::array();
		for (auto [engine, requested] : {std::pair{locomotive, INVALID_CARGO}, std::pair{wagon, cargo}}) {
			if (CommonwealthPackManager::GetVehicleAvailabilityError(CompanyID{0}, engine, worlds[service * 2]) != StringID{}) return false;
			auto query = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, query_depot, engine, false, requested, ClientID::Invalid);
			if (ExtractCommandCost(query).Failed()) return false;
			quotes.push_back({{"id", engine.base()}, {"cargo", requested}, {"cost", int64_t(ExtractCommandCost(query).GetCost())},
				{"capacity", std::get<2>(query)}, {"query_depot", query_depot.base()}});
		}
		if (quotes[1]["capacity"].get<uint>() == 0) return false;
		int64_t cost = quotes[0]["cost"].get<int64_t>() + wagons[service] * quotes[1]["cost"].get<int64_t>(); vehicles += cost;
		plan.report["vehicle_quote"] = int64_t(vehicles); plan.report["total_quote"] = int64_t(construction + vehicles);
		plan.report["services"].push_back({{"name", fmt::format("{} construction basket", labels[service])}, {"label", labels[service]}, {"cargo", cargo},
			{"pickup_leg", service * 2}, {"drop_leg", service * 2 + 1}, {"wagons", wagons[service]}, {"locomotive", quotes[0]}, {"wagon", quotes[1]},
			{"vehicle_quote", cost}, {"pickup_tile", plan.legs[service * 2].station.base()}, {"drop_tile", plan.legs[service * 2 + 1].station.base()},
			{"orders", Json::array({Json::array({1, plan.legs[service * 2].station.base(), 2, 4, 1, 0}),
				Json::array({1, plan.legs[service * 2 + 1].station.base(), 4, 0, 1, 0})})}});
	}
	plan.report["construction_quote"] = int64_t(construction); plan.report["vehicle_quote"] = int64_t(vehicles); plan.report["total_quote"] = int64_t(construction + vehicles);
	plan.report["preview_unchanged"] = FunctionalSnapshot() == plan.state;
	int64_t total = int64_t(construction + vehicles);
	bool legal = plan.report["preview_unchanged"].get<bool>() && total > 0 && BasketChain();
	plan.report["reason"] = legal ? "" : !plan.report["preview_unchanged"].get<bool>() ? "query-mutated-state" :
		"chain-integrity";
	plan.report["legal"] = legal; plan.report["frozen"] = legal;
	return legal;
}

bool BuildBasket(BasketLayout &plan)
{
	if (FunctionalSnapshot() != plan.state || !plan.report.value("frozen", false) || !BasketChain()) return BasketFail("frozen-plan-state");
	for (const auto &command : plan.commands) {
		if (command.reuse) continue;
		CommandCost quote = command.kind == BasketCommand::Kind::Rail ? QueryCoreBasketRail(command.tile, command.track) : BasketNativeCommand(command, {});
		if (quote.Failed() || int64_t(quote.GetCost()) != command.cost) return BasketFail(fmt::format("construction-query-parity kind={} tile={}", uint(command.kind), command.tile.base()));
		auto paid = BasketNativeCommand(command, DoCommandFlag::Execute);
		if (paid.Failed() || int64_t(paid.GetCost()) != command.cost) return BasketFail(fmt::format("construction-execute-parity kind={} tile={}", uint(command.kind), command.tile.base()));
	}
	if (!BasketReceiver()) return BasketFail("receiver-custody-after-build");
	/* Verify actual industry catchment and distinct native station identities. */
	std::set<StationID> stations;
	const std::array<uint16_t, 8> ids = {6, 7, 7, UINT16_MAX, 2, 4, 4, UINT16_MAX};
	for (uint i = 0; i < 8; ++i) {
		const Station *station = Station::Get(GetStationIndex(plan.legs[i].station));
		if (station->owner != CompanyID{0} || LogisticsHubManager::GetHubForStation(station->index) != nullptr) return BasketFail("station-custody");
		if (i % 4 == 3) { if (station->index != StationID{3}) return BasketFail("core-receiver-identity"); continue; }
		if (station->index.base() < 4 || !stations.insert(station->index).second) return BasketFail("processor-station-identity");
		const Industry *industry = Industry::Get(IndustryID{ids[i]});
		bool catches = false;
		for (TileIndex tile : industry->location) catches |= station->catchment_tiles.HasTile(tile);
		if (!catches) return BasketFail("industry-catchment");
	}
	basket.services = plan.report["services"];
	for (uint service = 0; service < 4; ++service) {
		Json &record = basket.services[service];
		TileIndex depot = plan.legs[service * 2].depot; VehicleID front = VehicleID::Invalid();
		for (uint n = 0; n <= record["wagons"].get<uint>(); ++n) {
			const Json &expected = record[n == 0 ? "locomotive" : "wagon"];
			EngineID id{expected["id"].get<uint16_t>()}; CargoType cargo = expected["cargo"].get<CargoType>();
			auto query = Command<Commands::BuildVehicle>::Do(DoCommandFlag::QueryCost, depot, id, false, cargo, ClientID::Invalid);
			if (ExtractCommandCost(query).Failed() || int64_t(ExtractCommandCost(query).GetCost()) != expected["cost"].get<int64_t>() ||
				std::get<2>(query) != expected["capacity"].get<uint>()) return BasketFail("vehicle-native-depot-query-parity");
			auto [cost, vehicle, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, id, false, cargo, ClientID::Invalid);
			if (cost.Failed() || int64_t(cost.GetCost()) != expected["cost"].get<int64_t>() || capacity != expected["capacity"].get<uint>()) return BasketFail("vehicle-native-execute-parity");
			if (n == 0) { front = vehicle; record["train"] = front.base(); }
			else {
				if (Command<Commands::MoveRailVehicle>::Do({}, vehicle, front, false).Failed() ||
					Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, vehicle, front, false).Failed()) return BasketFail("vehicle-coupling");
			}
		}
		for (uint8_t n = 0; n < 2; ++n) {
			StationID station = GetStationIndex(plan.legs[service * 2 + n].station);
			Order order; order.MakeGoToStation(station); order.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
			if (n == 0) { order.SetLoadType(OrderLoadType::FullLoad); order.SetUnloadType(OrderUnloadType::NoUnload); } else order.SetLoadType(OrderLoadType::NoLoad);
			if (Command<Commands::InsertOrder>::Do({}, front, VehicleOrderID{n}, order).Failed() ||
				Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, front, VehicleOrderID{n}, order).Failed()) return BasketFail("vehicle-orders");
			record[n == 0 ? "pickup" : "drop"] = station.base();
		}
		if (Command<Commands::StartStopVehicle>::Do({}, front, true).Failed() ||
			Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, front, true).Failed()) return BasketFail("vehicle-start");
	}
	basket.quoted_total = plan.report["total_quote"].get<int64_t>(); basket.built = true;
	return true;
}

/** Check frozen cold service identities, consists and ordinary orders without repairs. */
bool BasketServicesValid(const Json &services)
{
	try {
		if (!services.is_array() || services.size() != 4) return false;
		const std::array<const char *, 4> labels = {"IRON", "STEL", "SILC", "BALL"};
		const std::array<uint, 4> wagons = {3, 1, 3, 1}; std::set<VehicleID> fronts;
		for (uint i = 0; i < 4; ++i) {
			const auto &service = services[i]; CargoLabel label; std::copy_n(labels[i], 4, label.begin());
			CargoType cargo = GetCargoTypeByLabel(label);
			if (service.at("label") != labels[i] || service.at("cargo") != cargo || service.at("wagons") != wagons[i]) return false;
			VehicleID id{service.at("train").get<uint32_t>()}; const Train *front = Train::GetIfValid(id);
			if (front == nullptr || id.base() < 8 || !front->IsFrontEngine() || front->owner != CompanyID{0} || !fronts.insert(id).second) return false;
			const Train *vehicle = front;
			for (uint n = 0; n <= wagons[i]; ++n) {
				const Json &expected = service.at(n == 0 ? "locomotive" : "wagon");
				if (vehicle == nullptr || vehicle->owner != CompanyID{0} || vehicle->engine_type.base() != expected.at("id") ||
					(n != 0 && (vehicle->cargo_type != cargo || vehicle->cargo_cap != expected.at("capacity")))) return false;
				vehicle = vehicle->Next();
			}
			if (vehicle != nullptr) return false;
			uint n = 0;
			for (const Order &order : front->Orders()) {
				if (n > 1 || order.GetType() != OT_GOTO_STATION || order.GetDestination().base() != service.at(n == 0 ? "pickup" : "drop") ||
					order.GetNonStopType() != OrderNonStopFlags{OrderNonStopFlag::NonStop} ||
					order.GetLoadType() != (n == 0 ? OrderLoadType::FullLoad : OrderLoadType::NoLoad) ||
					order.GetUnloadType() != (n == 0 ? OrderUnloadType::NoUnload : OrderUnloadType::UnloadIfPossible)) return false;
				++n;
			}
			if (n != 2 || (i % 2 == 1 && service.at("drop") != 3)) return false;
		}
		return true;
	} catch (const Json::exception &) { return false; }
}

bool BasketIntegrity()
{
	const Company *company = Company::GetIfValid(CompanyID{0});
	if (company == nullptr || company->current_loan != 100000 || TechTreeManager::GetActiveProject(CompanyID{0}) != TECH_NONE ||
		TechTreeManager::GetAccumulatedRP(CompanyID{0}) != 0 || TechTreeManager::GetMonthlyBudget(CompanyID{0}) != 0) return BasketFail("debt-research-integrity");
	uint units = 0;
	for (const Train *train : Train::Iterate()) {
		++units;
		if (train->vehstatus.Test(VehState::Crashed) || train->vehicle_flags.Test(VehicleFlag::PathfinderLost)) return BasketFail("crashed-or-lost-train");
	}
	if (basket.built && (units != 20 || !BasketServicesValid(basket.services) || !BasketReceiver())) return BasketFail("fleet-orders-custody-integrity");
	return true;
}

/**
 * Conservative next-tick debit bound using the ordinary native fee primitives.
 * Native train ticks run two movement handlers. Each handler advances at most
 * (3*max_speed/4 + byte progress)/192 steps; charging the maximum public toll at
 * every step overcounts admissions safely. Monthly interest/property/rail fees
 * are reserved on every tick. The native train day charge includes a fractional
 * company carry, so reserve its next possible whole-pound debit for each front.
 * Any approaching automatic renewal stops before its temporary GBP 100k debit;
 * an ordinary inward depot entry also reserves the native temporary renewal fee.
 */
bool BasketTickReserve(int64_t &reserve)
{
	const Company *company = Company::Get(CompanyID{0});
	if (company->is_ai || company->engine_renew_list != nullptr ||
		company->infrastructure.GetRoadTotal() != 0 || company->infrastructure.GetTramTotal() != 0 || company->infrastructure.water != 0 || company->infrastructure.airport != 0)
		return BasketFail("unbounded-company-automatic-debits");
	int64_t yearly_interest = int64_t(company->current_loan) * _economy.interest_rate / 100;
	reserve = (yearly_interest + 11) / 12 + std::max<int64_t>(0, int64_t(_price[Price::StationValue] >> 2));
	if (_settings_game.economy.infrastructure_maintenance) {
		uint32_t total = company->infrastructure.GetRailTotal();
		for (RailType type : EnumRange(RAILTYPE_END)) if (company->infrastructure.rail[type] != 0)
			reserve += std::max<int64_t>(0, int64_t(RailMaintenanceCost(type, company->infrastructure.rail[type], total)));
		reserve += std::max<int64_t>(0, int64_t(SignalMaintenanceCost(company->infrastructure.signal))) +
			std::max<int64_t>(0, int64_t(StationMaintenanceCost(company->infrastructure.station))) +
			std::max<int64_t>(0, int64_t(PortalRegistry::GetCompanyPortalMaintenanceCost(CompanyID{0})));
	}
	int64_t toll = 0;
	for (const auto &[id, link] : PortalRegistry::GetAllPortals()) for (const auto &end : {link.end_a, link.end_b})
		if (auto policy = StellarNetwork::Policy(end.tile)) toll = std::max(toll, int64_t(policy->toll));
	std::vector<TileIndex> depots;
	for (const Depot *depot : Depot::Iterate()) if (IsRailDepotTile(depot->xy) && GetTileOwner(depot->xy) == CompanyID{0}) depots.push_back(depot->xy);
	uint fronts = 0;
	for (const Vehicle *vehicle : Vehicle::Iterate()) if (vehicle->type != VehicleType::Train && vehicle->type != VehicleType::Effect) return BasketFail("non-train-automatic-debits");
	for (const Train *front : Train::Iterate()) if (front->IsFrontEngine()) {
		++fronts;
		if (front->vcache.cached_max_speed > 128 || front->cur_speed > 128) return BasketFail("unbounded-fleet-speed");
		/* Train::OnNewEconomyDay charges a 1/256-pound annual cost scaled by
		 * running ticks / (365 * 74); one tick can add at most one running tick.
		 * SubtractMoneyFromCompanyFract may round up through its existing carry. */
		constexpr int64_t denominator = int64_t(CalendarTime::DAYS_IN_YEAR) * Ticks::DAY_TICKS * 256;
		int64_t yearly_running = int64_t(front->GetRunningCost());
		uint running_ticks = uint(front->running_ticks) + 1;
		if (yearly_running < 0 || yearly_running > (INT64_MAX - denominator) / running_ticks) return BasketFail("unbounded-fleet-running-cost");
		reserve += (yearly_running * running_ticks + denominator - 1) / denominator;
		if (company->settings.engine_renew && int64_t(front->age.base()) + 1 - front->max_age.base() >= int64_t(company->settings.engine_renew_months) * 30)
			return BasketFail("approaching-automatic-renewal-bound");
		uint speed = std::max<uint>(front->cur_speed, front->vcache.cached_max_speed);
		reserve += 2 * ((3 * speed / 4 + UINT8_MAX) / TILE_AXIAL_DISTANCE + 1) * toll;
		bool entry = front->current_order.IsType(OT_GOTO_DEPOT);
		if (!front->IsChainInDepot()) for (const Train *part = front; part != nullptr; part = part->Next()) for (TileIndex depot : depots) {
			if (DistanceMax(part->tile, depot) > 1) continue;
			/* Outward departure cannot trigger VehicleEnterDepot. Curves and any
			 * other nearby movement conservatively reserve a possible entry. */
			entry |= DirToDiagDir(part->GetMovingDirection()) != GetRailDepotDirection(depot);
		}
		if (entry) reserve += company->settings.engine_renew_money;
	}
	if (fronts != 6) return BasketFail("six-service-debit-bound");
	return true;
}

/** Loaded-checkpoint console adapter with no grant, borrowing, research or authoring operation. */
bool CoreBasket(std::span<std::string_view> argv)
{
	if (_game_mode != GameMode::Normal || _networking || _network_dedicated || Company::GetIfValid(CompanyID{0}) == nullptr) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL basket requires an offline loaded company"); return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	CommonwealthSliceAudit audit; std::vector<Json> months;
	AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
	AutoRestoreBackup monthly(_integrated_city_month_audit, &months);
	Json before = FunctionalSnapshot(), report = {{"version", 1}, {"operation", argv[1]}};
	if (argv[1] == "basket-dev-money" && argv.size() == 2) {
		if (basket.armed || basket.failed || !_pause_mode.Test(PauseMode::Normal) ||
			before.at("tick") != 167168 || before.at("money") != 3705864 || _settings_game.difficulty.infinite_money ||
			!BasketChain()) BasketFail("development-money-source-boundary");
		else if (Command<Commands::ChangeSetting>::Do(DoCommandFlag::Execute, "difficulty.infinite_money", 1).Failed() ||
			!_settings_game.difficulty.infinite_money) BasketFail("development-money-setting");
		report["development_unlimited_money"] = _settings_game.difficulty.infinite_money;
		report["failed"] = basket.failed; report["reason"] = basket.error;
		report["state"] = FunctionalSnapshot(); report["observation"] = BasketObservation();
		report["actual_ticks"] = 0; report["audit"] = FunctionalAudit(audit);
		report["audit"]["basket_months"] = months;
		report["services"] = basket.services;
		report["cash_conserved"] = report["state"]["money"] == before["money"];
		report["cargo_errors"] = Json::array();
		IConsolePrint(CC_DEFAULT, "CONNECTED basket-result {}", report.dump()); return true;
	}
	if (argv[1] == "basket-status" && argv.size() == 2) {
		report["state"] = before; report["observation"] = BasketObservation(); report["services"] = basket.services;
		IConsolePrint(CC_DEFAULT, "CONNECTED basket-state {}", report.dump()); return true;
	}
	if (argv[1] == "basket-arm" && argv.size() == 3) {
		if (basket.armed || basket.failed) BasketFail("already-armed-or-stopped");
		else {
			try {
				std::ifstream file(std::string(argv[2]), std::ios::binary);
				if (!file) throw std::runtime_error("frozen-state-file");
				file.seekg(0, std::ios::end); auto length = file.tellg();
				if (length <= 0 || length > 16 * 1024 * 1024) throw std::runtime_error("frozen-state-size");
				file.seekg(0); Json frozen = Json::parse(file);
				bool cold = frozen.is_object() && frozen.contains("basket");
				const Json &state = cold ? frozen.at("state") : frozen;
				CoreBasketAdmissionContext context{true, true, _pause_mode.Test(PauseMode::Normal), BasketProfile(),
					!GetIntegratedCoreTownGenerationStats().active && !generation_contract_fresh && !functional_fresh, uint32_t(Company::GetNumItems())};
				std::string error = GetCoreBasketAdmissionError(before, context, cold);
				if (!error.empty()) BasketFail(error);
				else if (state != before) BasketFail("frozen-state-equality");
				else if (!BasketChain()) BasketFail("fixed-chain-public-gates-receiver");
				else if (cold) {
					const auto &ledger = frozen.at("basket");
					uint advances = ledger.at("initial_advances").get<uint>(); int64_t gross = ledger.at("gross_debits").get<int64_t>();
					int64_t quote = ledger.at("quoted_total").get<int64_t>();
					Json observation = BasketObservation(); observation.erase("adapter");
					if (frozen.at("observation") != observation) BasketFail("cold-full-observation-equality");
					else if (ledger.at("source_sha256") != Json(std::string(BASKET_SOURCE)) || advances < 15 || advances > 240 ||
						before.at("tick").get<uint64_t>() != 167168 + uint64_t(advances - 4) * 2048 || gross < quote ||
						quote <= 0 || !BasketServicesValid(ledger.at("services"))) BasketFail("cold-ledger-services");
					else { basket.cold = !ledger.value("resume_initial", false); basket.built = true; basket.initial_advances = advances; basket.gross = gross;
						basket.quoted_total = quote; basket.services = ledger.at("services"); basket.armed = true; }
				} else basket.armed = true;
			} catch (const std::exception &) { BasketFail("invalid-frozen-state-file-or-json"); }
		}
		basket.expected = before;
	} else if (!basket.armed || basket.failed || !_pause_mode.Test(PauseMode::Normal) || !BasketProfile()) {
		BasketFail("unarmed-stopped-or-profile");
	} else if (before != basket.expected) {
		BasketFail("unexpected-paused-state-change");
	} else if ((argv[1] == "basket-plan" || argv[1] == "basket-build") && argv.size() == 3) {
		auto layout = ParseInteger<uint>(argv[2]);
		if (!layout || *layout > 1 || basket.built || basket.cold) BasketFail("layout-or-built-contract");
		else if (argv[1] == "basket-plan") {
			if (basket.plans[*layout] || basket.layouts >= 2 || (*layout == 1 && !basket.plans[0])) BasketFail("stable-layout-attempt-bound");
			else {
				++basket.layouts; BasketLayout plan; bool legal = PlanBasket(*layout, plan);
				plan.report["preview_unchanged"] = FunctionalSnapshot() == before;
				std::string reason = plan.report.value("reason", "complete-layout-legality-affordability");
				bool geometry_miss = reason == "bounded-endpoint-no-route" || reason == "future-footprint-geometry";
				if (!plan.report["preview_unchanged"].get<bool>()) BasketFail("query-mutated-state");
				else if (!legal && (!geometry_miss || basket.layouts == 2)) BasketFail(reason);
				plan.report["failed"] = basket.failed;
				if (basket.failed) plan.report["reason"] = basket.error;
				plan.report["observation"] = BasketObservation();
				Json output = plan.report;
				if (!basket.failed) basket.plans[*layout] = std::move(plan);
				IConsolePrint(CC_DEFAULT, "CONNECTED basket-plan {}", output.dump()); return true;
			}
		} else if (!basket.plans[*layout]) BasketFail("missing-frozen-layout");
		else {
			BasketLayout &plan = *basket.plans[*layout]; report["plan"] = plan.report;
			if (BuildBasket(plan) && audit.cash_debits != plan.report["total_quote"].get<int64_t>()) BasketFail("construction-total-query-execute-parity");
		}
	} else if (argv[1] == "basket-advance" && argv.size() == 2) {
		if (!basket.built || basket.advances >= (basket.cold ? 120u : 240u) || basket.initial_advances + basket.advances >= 360) BasketFail("advance-bound-or-unbuilt");
		else {
			++basket.advances;
			AutoRestoreBackup pause(_pause_mode); AutoRestoreBackup tick_owner(_current_company, _local_company);
			UpdateSignalsInBuffer(); _pause_mode.Reset(); size_t transactions = 0; int64_t gross = basket.gross;
			for (uint i = 0; i < 2048; ++i) {
				if (!BasketIntegrity()) break;
				int64_t reserve = 0;
				if (!BasketTickReserve(reserve)) break;
				basket.tick_reserve = reserve;
				StateGameLoop();
				while (transactions < audit.cash_transactions.size()) gross += std::max<int64_t>(0, audit.cash_transactions[transactions++][2]);
			}
		}
	} else if ((argv[1] == "basket-stop" || argv[1] == "basket-restart") && argv.size() == 3) {
		VehicleID id{ParseInteger<uint32_t>(argv[2]).value_or(UINT32_MAX)}; Train *train = Train::GetIfValid(id);
		bool authorized = id == VehicleID{4};
		for (const auto &service : basket.services) authorized |= service.value("train", UINT32_MAX) == id.base();
		if (!basket.built || train == nullptr || !train->IsFrontEngine() || train->owner != CompanyID{0} || !authorized) BasketFail("control-train-contract");
		else if (train->vehstatus.Test(VehState::Stopped) != (argv[1] == "basket-stop")) {
			if (Command<Commands::StartStopVehicle>::Do({}, id, true).Failed() || Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, id, true).Failed()) BasketFail("normal-vehicle-control");
		}
	} else BasketFail("invalid-operation");
	Json after = FunctionalSnapshot();
	report["actual_ticks"] = after["tick"].get<uint64_t>() - before["tick"].get<uint64_t>();
	for (const auto &transaction : audit.cash_transactions) basket.gross += std::max<int64_t>(0, transaction[2]);
	report["cash_conserved"] = after["money"].get<int64_t>() == before["money"].get<int64_t>() - audit.cash_debits;
	report["cargo_errors"] = Json::array();
	for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) {
		int64_t expected = before["all_held"][cargo].get<int64_t>() + audit.produced[cargo] + audit.raw_produced[cargo] - audit.raw_removed[cargo] - audit.unallocated[cargo] - audit.discarded[cargo] - audit.consumed[cargo];
		if (expected != after["all_held"][cargo].get<int64_t>()) report["cargo_errors"].push_back({cargo, expected, after["all_held"][cargo]});
	}
	if (!report["cash_conserved"].get<bool>() || !report["cargo_errors"].empty()) BasketFail("cash-or-cargo-conservation");
	if (basket.armed && !basket.failed) BasketIntegrity();
	if (BasketObservation()["custody"]["total"] != after["all_held"]) BasketFail("all-cargo-custody-projection");
	report["state"] = after; report["observation"] = BasketObservation(); report["audit"] = FunctionalAudit(audit);
	report["audit"]["basket_months"] = months; report["services"] = basket.services; report["failed"] = basket.failed; report["reason"] = basket.error;
	basket.expected = after;
	IConsolePrint(CC_DEFAULT, "CONNECTED basket-result {}", report.dump());
	return true;
}
/** @endcond */

} // namespace

nlohmann::json GetCoreBasketObservation()
{
	return Company::GetIfValid(CompanyID{0}) == nullptr ? Json::object() : BasketObservation();
}

CommandCost QueryCoreBasketRail(TileIndex tile, Track track, TrackBits prospective)
{
	if (!IsValidTile(tile) || !IsInnerTile(tile) || !IsValidTrack(track) || _current_company != CompanyID{0}) return CMD_ERROR;
	TrackBits existing{};
	if (IsPlainRailTile(tile)) {
		/* Reuse means retaining an owned compatible piece, never adopting public rail. */
		if (GetTileOwner(tile) != CompanyID{0} || GetRailType(tile) != RAILTYPE_RAIL) return CMD_ERROR;
		existing = GetTrackBits(tile);
	} else if (IsLevelCrossingTile(tile)) {
		if (GetTileOwner(tile) != CompanyID{0} || GetRailType(tile) != RAILTYPE_RAIL) return CMD_ERROR;
		existing = TrackBits{GetCrossingRailTrack(tile)};
	} else if (prospective.Any() && !IsTileType(tile, TileType::Clear) && !IsTileType(tile, TileType::Trees)) {
		return CMD_ERROR;
	}
	if ((existing | prospective).Test(track)) return CommandCost();
	/* Every new piece must also pass the unmodified native placement, ownership
	 * and train-occupancy checks against the paused map. Future costing must not
	 * bypass those guards merely because another piece is already planned. */
	auto native = Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, track, false);
	if (native.Failed() || prospective.None()) return native;
	/* Later pieces on a future owned junction pay incremental foundation/track costs.
	 * The first piece already owns the native clearing quote. These are the same
	 * primitives as CmdBuildSingleRail and the ordinary blueprint preflight. */
	if (IsPlainRailTile(tile) && HasSignals(tile) && TracksOverlap(existing | prospective | track)) return CMD_ERROR;
	auto slope = CheckRailSlope(GetTileSlope(tile), TrackBits{track}, existing | prospective, tile);
	if (slope.Failed()) return slope;
	slope.AddCost(GetNewRailTrackCost(RAILTYPE_RAIL, PlanetManager::GetTileWorld(tile), CompanyID{0}));
	return slope;
}

std::string GetCoreBasketAdmissionError(const nlohmann::json &state, const CoreBasketAdmissionContext &context, bool continuation)
{
	if (!context.normal) return "game-mode";
	if (!context.offline) return "network-offline";
	if (!context.loaded) return "fresh-generation";
	if (!context.paused) return "pause";
	if (!context.profile) return "profile-content";
	if (context.companies != 1) return "company-count";
	try {
		if (state.at("company") != Json(std::string(FUNCTIONAL_COMPANY)) || state.at("loan") != 100000 || state.at("max_loan") != 300000) return "company-debt";
		if (state.at("seed") != 11 || (!continuation && state.at("tick") != 167168) ||
			(continuation && (state.at("tick").get<uint64_t>() <= 167168 || state.at("tick").get<uint64_t>() > 167168 + 240 * 2048))) return "tick-seed";
		if (!continuation && state.at("money") != 3705864) return "cash";
		if (state.at("research") != Json::array({Json::array({0, 0, 0, Json::array({TECH_MATERIALS_1})})}) ||
			state.at("tech_company_ids") != Json::array({0})) return "research";
		const auto &worlds = state.at("worlds");
		const std::array<const char *, 7> roles = {"Core", "Industrial", "Frontier", "Frontier", "Frontier", "Industrial", "Frontier"};
		const std::array<uint, 7> phases = {1, 2, 3, 3, 4, 4, 4};
		if (worlds.size() != roles.size()) return "roles";
		for (uint i = 0; i < roles.size(); ++i) if (worlds[i].at("id") != i || worlds[i].at("role") != roles[i] || worlds[i].at("phase") != phases[i]) return "roles";
		const auto &towns = state.at("towns");
		auto town = std::find_if(towns.begin(), towns.end(), [](const auto &row) { return row.at("id") == 0; });
		if (town == towns.end() || (*town).at("world") != 0 || !(*town).at("megacity").template get<bool>() ||
			(*town).at("population").template get<uint32_t>() == 0 || !(*town).at("profile").is_object()) return "core-profile";
		const auto &stations = state.at("stations");
		auto receiver = std::find_if(stations.begin(), stations.end(), [](const auto &row) { return row.at("id") == 3; });
		if (receiver == stations.end() || (*receiver).at("town") != 0 || (*receiver).at("owner") != 0 ||
			(*receiver).at("warehouse") != false || (*receiver).at("consumer") != true) return "receiver";
		bool house = false;
		for (const auto &row : (*receiver).at("catchment_houses")) house |= row.at("town") == 0 && row.at("world") == 0;
		if (!house) return "receiver-house";
		if (!state.at("hubs").empty() || !state.at("stocks").empty()) return "cargo-custody";
		const auto &trains = state.at("trains");
		if ((!continuation && trains.size() != 8) || (continuation && trains.size() != 20)) return "fleet";
		for (uint i = 0; i < 8; ++i) {
			auto train = std::find_if(trains.begin(), trains.end(), [i](const auto &row) { return row.at("id") == i; });
			if (train == trains.end() || (*train).at("owner") != 0 || (*train).at("crashed") != false || (*train).at("lost") != false ||
				(*train).at("engine") != (i % 4 == 0 ? 32 : i < 4 ? 50 : 52)) return "inherited-fleet";
			if (i == 0 || i == 4) {
				Json orders = Json::array({Json::array({1, i == 0 ? 0 : 2, 2, 4, 1, 0}), Json::array({1, i == 0 ? 1 : 3, 4, 0, 1, 0})});
				if ((*train).at("orders") != orders) return "inherited-orders";
			}
		}
		return {};
	} catch (const nlohmann::json::exception &) {
		return "checkpoint-json";
	}
}

/**
 * Capture every map tile, including the native void border, without mutation.
 * @return Deterministic FNV-1a projection of tile type, height and meaningful owner.
 */
uint64_t GetConnectedEconomyMapFingerprint()
{
	uint64_t terrain = 14695981039346656037ULL;
	for (TileIndex tile : Map::Iterate()) {
		for (uint value : {uint(GetTileType(tile)), uint(TileHeight(tile)), uint(ObservedTileOwner(tile).base())}) {
			terrain ^= value; terrain *= 1099511628211ULL;
		}
	}
	return terrain;
}

void ResetConnectedEconomyProof()
{
	generation_contract_fresh = false;
	functional_fresh = false;
	basket = BasketSession{};
}

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
	if (argv.size() >= 2 && argv[1].starts_with("basket-")) return CoreBasket(argv);
	if (argv.size() >= 2 && argv[1].starts_with("functional-")) return FunctionalCore(argv);
	if (argv.size() >= 2 && argv[1].starts_with("generation-contract-")) return GenerationContract(argv);
	if (argv.size() >= 2 && argv[1].starts_with("first-freight-")) return FirstFreight(argv);
	if (argv.size() != 2) {
		IConsolePrint(CC_HELP, "connected_economy prepare|prepare-surveys|status|audit|advance|stop-food|start-food|fabricate|research: isolated demo only");
		return true;
	}
	if (_game_mode != GameMode::Normal || (_networking && (!_network_dedicated || NetworkClientInfo::GetNumItems() > 1)) ||
	    CommonwealthPackManager::GetContentStatus().mode != CommonwealthContentMode::Active) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL requires isolated game with active Commonwealth content");
		return true;
	}
	if (argv[1] == "prepare" || argv[1] == "prepare-surveys" || argv[1] == "prepare-integrated") {
		if (Prepare(argv[1] != "prepare", argv[1] == "prepare-integrated")) IConsolePrint(CC_DEFAULT, "CONNECTED prepared");
		return true;
	}
	const Company *company = Company::GetIfValid(CompanyID{0});
	bool functional_audit = argv[1] == "audit" && !_networking && !_network_dedicated && company != nullptr && company->name == FUNCTIONAL_COMPANY && PlanetManager::Count() == 7;
	if (!functional_audit && (company == nullptr || company->name != DEMO_NAME || PlanetManager::Count() != 4)) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL not a connected fixture");
		return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	if (!functional_audit) ConfigureIntegratedNodes();
	if (argv[1] == "progress" && IntegratedEconomy::Enabled()) {
		ProgressIntegrated();
		UpdateSignalsInBuffer();
		IConsolePrint(CC_DEFAULT, "CONNECTED state {}", Snapshot().dump());
	} else if (argv[1] == "audit") {
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
		if (IntegratedEconomy::Enabled()) {
			result["freight_refits"] = Json::array();
			for (const char *label : {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "RARE", "ALLO", "POLY",
									  "BCRY", "QCRY", "CCRY", "BALL", "SIGE", "MGLA", "MACH", "OIL_", "GRAI", "FOOD"}) {
				CargoLabel cargo_label;
				std::copy_n(label, 4, cargo_label.begin());
				CargoType cargo = GetCargoTypeByLabel(cargo_label);
				Json engines = Json::array();
				for (auto engine : Engine::Iterate()) {
					if (engine->type != VehicleType::Train || engine->VehInfo<RailVehicleInfo>().railveh_type != RailVehicleType::Wagon)
						continue;
					if (engine->GetDefaultCargoType() == cargo || engine->info.refit_mask.Test(cargo))
						engines.push_back(engine->index.base());
				}
				result["freight_refits"].push_back({{"label", label}, {"engines", engines}});
			}
		}
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
		/* Console authoring calls native Do directly: flush queued signal edits before ticking. */
		UpdateSignalsInBuffer();
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
		for (size_t i = 0; !IntegratedEconomy::Enabled() && i < after["facilities"].size(); ++i) {
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
			                   audit.consumed[c] + net[c] + audit.processing_bonus[c];
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
	} else if (argv[1] == "electric" && IntegratedEconomy::Enabled()) {
		if (!TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_TRACTION_3)) return false;
		const TileIndex tile = TileXY(100, 210);
		auto before = Snapshot()["stocks"];
		auto quote = Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_ELECTRIC, Track::X, false);
		if (!Result(quote, "electric material preview")) return true;
		bool unchanged = before == Snapshot()["stocks"];
		auto result = Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_ELECTRIC, Track::X, false);
		if (!Result(result, "electric construction")) return true;
		SubtractMoneyFromCompany(CompanyID{0}, result);
		IConsolePrint(CC_DEFAULT, "CONNECTED electric {}",
					  Json({{"preview_unchanged", unchanged},
							{"cash_mode", !FabricationManager::IsFabricateFromStockpileEnabled(CompanyID{0})},
							{"materials_consumed", before != Snapshot()["stocks"]},
							{"built", IsPlainRailTile(tile) && GetRailType(tile) == RAILTYPE_ELECTRIC}})
						  .dump());
	} else if (argv[1] == "research") {
		TechID project = TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_PORTAL_1) ? TECH_PORTAL_2 : TECH_PORTAL_1;
		if (!Result(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, project), "research demonstration")) return true;
		IConsolePrint(CC_DEFAULT, "CONNECTED research selected");
	} else if (argv[1] == "stop-food" || argv[1] == "start-food") {
		bool stop = argv[1] == "stop-food";
		for (Train *t : Train::Iterate())
			if (t->IsFrontEngine() &&
				(t->name == "Food to megacity" || (IntegratedEconomy::Enabled() && t->name.starts_with("Food works >"))) &&
				t->Next() != nullptr && t->Next()->cargo_type == GetCargoTypeByLabel(CargoLabel{"FOOD"}) &&
				t->vehstatus.Test(VehState::Stopped) != stop) {
				if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, t->index, true), "food service")) return true;
			}
		IConsolePrint(CC_DEFAULT, "CONNECTED food {}", stop ? "stopped" : "started");
	} else
		IConsolePrint(CC_ERROR, "CONNECTED FAIL unknown action");
	return true;
}
