/* This file is part of OpenSpaceTTD. Licensed under GPL-2.0. */
/** @file connected_economy.cpp Reproducible offline connected production and megacity acceptance fixture. */
#include "../stdafx.h"
#include "connected_economy.h"
#include "integrated_economy.h"
#include "../economy_func.h"
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
#include "../openttd.h"
#include "../network/network.h"
#include "../network/network_base.h"
#include "../core/backup_type.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include <queue>
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
 * Native tile observations shared by generated terminals and advertised arrival zones.
 * @param tile Native map tile to observe without changing it.
 * @return Captured bounds, ownership, terrain and rail observations.
 */
Json GenerationTile(TileIndex tile)
{
	Json r = {{"tile", tile.base()}, {"valid", IsValidTile(tile) && IsInnerTile(tile)}};
	if (!r["valid"].get<bool>()) return r;
	r["world"] = PlanetManager::GetTileWorld(tile).base();
	r["type"] = uint(GetTileType(tile)); r["owner"] = GetTileOwner(tile).base();
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
		if (report["total_quote"].get<int64_t>() >= company->money) {
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

/**
 * Find a legal rail path using native previews without editing terrain or neutral infrastructure.
 * @param industry Generated producer or consumer to serve.
 * @param gate Public terminal endpoint in the industry's world.
 * @param[out] leg Feasible station, depot and connecting rail plan when found.
 * @return Whether the bounded search found a feasible connection.
 */
bool PlanFreightLeg(const Industry *industry, const PortalEndpoint &gate, FreightLeg &leg)
{
	auto terminal = PortalTerminal::Plan(gate.tile, gate.enter_dir, gate.world_id);
	if (!terminal) return false;
	DiagDirection outward = terminal->outward_dir;
	TileIndex target = TileAddByDiagDir(terminal->connection_tile, outward);
	std::vector<std::tuple<uint, TileIndex, Axis>> candidates;
	for (int dy = -4; dy <= int(industry->location.h) + 3; ++dy) for (int dx = -4; dx <= int(industry->location.w) + 3; ++dx) {
		int x = int(TileX(industry->location.tile)) + dx, y = int(TileY(industry->location.tile)) + dy;
		if (x <= 0 || y <= 0 || x + 2 >= int(Map::MaxX()) || y + 2 >= int(Map::MaxY())) continue;
		for (Axis axis : {Axis::X, Axis::Y}) {
			TileIndex tile = TileXY(x, y);
			if (Command<Commands::BuildRailStation>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, axis, 1, 2, STAT_CLASS_DFLT, 0, StationID::Invalid(), false).Succeeded())
				candidates.emplace_back(DistanceManhattan(tile, target), tile, axis);
		}
	}
	std::sort(candidates.begin(), candidates.end());
	std::vector<int8_t> preview(Map::Size() * 6, -1);
	uint attempts = 0;
	for (auto [distance, station, axis] : candidates) {
		if (++attempts > 16) break;
		DiagDirection positive = axis == Axis::X ? DiagDirection::SW : DiagDirection::SE;
		DiagDirection dir = axis == Axis::X ? (TileX(target) > TileX(station) ? positive : ReverseDiagDir(positive)) :
			(TileY(target) > TileY(station) ? positive : ReverseDiagDir(positive));
		TileIndex station_near = dir == positive ? TileAddByDiagDir(station, positive) : station;
		TileIndex station_far = dir == positive ? station : TileAddByDiagDir(station, positive);
		TileIndex start = TileAddByDiagDir(station_near, dir), depot = TileAddByDiagDir(station_far, ReverseDiagDir(dir));
		if (!Command<Commands::BuildRailDepot>::Do(DoCommandFlag::QueryCost, depot, RAILTYPE_RAIL, dir).Succeeded()) continue;
		/* Weighted A* is a bounded feasible-route search, not a minimum-cost claim. State retains native track direction; ties are stable. */
		using Q = std::tuple<uint, int, uint>;
		std::priority_queue<Q, std::vector<Q>, std::greater<Q>> queue;
		std::map<uint, std::tuple<uint, Track, TileIndex>> previous;
		std::map<uint, uint> costs;
		uint first = start.base() * 16 + uint(DiagDirToDiagTrackdir(dir));
		queue.emplace(3 * DistanceManhattan(start, target), 0, first); costs[first] = 0;
		uint last = UINT_MAX, closest = UINT_MAX;
		while (!queue.empty() && previous.size() < 30000) {
			auto [estimate, negative_cost, key] = queue.top(); queue.pop();
			uint cost = uint(-negative_cost);
			if (cost != costs[key]) continue;
			TileIndex tile{key / 16}; Trackdir previous_dir = Trackdir(key % 16);
			DiagDirection enter = ReverseDiagDir(TrackdirToExitdir(previous_dir));
			closest = std::min(closest, DistanceManhattan(tile, target));
			if (PlanetManager::GetTileWorld(tile) != gate.world_id || tile == depot || tile == station || tile == TileAddByDiagDir(station, positive)) continue;
			if (!IsTileType(tile, TileType::Clear) && !IsTileType(tile, TileType::Trees) && !IsTileType(tile, TileType::Road) && !(IsPlainRailTile(tile) && GetTileOwner(tile) == CompanyID{0})) continue;
			for (DiagDirection exit : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
				Track track = Corner(enter, exit);
				if (!IsValidTrack(track)) continue;
				Trackdir next_dir = TrackExitdirToTrackdir(track, exit);
				if (IsValidTrackdir(next_dir) && TrackdirCrossesTrackdirs(previous_dir).Test(next_dir)) continue;
				auto &legal = preview[tile.base() * 6 + uint(track)];
				if (legal == -1) legal = Command<Commands::BuildRail>::Do(DoCommandFlag::QueryCost, tile, RAILTYPE_RAIL, track, false).Succeeded();
				if (!legal) continue;
				if (tile == target && exit == ReverseDiagDir(outward)) { previous[UINT_MAX] = {key, track, INVALID_TILE}; last = key; break; }
				TileIndex next = TileAddByDiagDir(tile, exit);
				if (next >= Map::Size()) continue;
				uint nk = next.base() * 16 + uint(next_dir);
				if (costs.contains(nk) && costs[nk] <= cost + 1) continue;
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
			if (!TrackdirCrossesTrackdirs(previous_dir).Test(bridge_dir) &&
				(!next_is_ground || !Command<Commands::BuildRail>::Do({}, straight_next, RAILTYPE_RAIL, straight, false).Succeeded())) {
				TileIndex end = tile;
				for (uint span = 1; span <= 16; ++span) {
					end = TileAddByDiagDir(end, travel);
					if (end >= Map::Size() || PlanetManager::GetTileWorld(end) != gate.world_id) break;
					if (span < 2 || end == depot || end == station || end == TileAddByDiagDir(station, positive) || end == target) continue;
					if (!IsTileType(end, TileType::Clear) && !IsTileType(end, TileType::Trees)) continue;
					if (!Command<Commands::BuildBridge>::Do({}, end, tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE).Succeeded()) continue;
					TileIndex next = TileAddByDiagDir(end, travel);
					if (next >= Map::Size()) continue;
					uint nk = next.base() * 16 + uint(bridge_dir), nc = cost + span + 12;
					if (costs.contains(nk) && costs[nk] <= nc) continue;
					costs[nk] = nc; previous[nk] = {key, straight, end}; queue.emplace(nc + 3 * DistanceManhattan(next, target), -int(nc), nk);
				}
			}
			if (last != UINT_MAX) break;
		}
		if (last == UINT_MAX) { IConsolePrint(CC_DEFAULT, "FREIGHT search industry={} station={} visited={} closest={} target={}", industry->index.base(), station.base(), previous.size(), closest, target.base()); continue; }
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

} // namespace

void ResetConnectedEconomyProof()
{
	generation_contract_fresh = false;
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
	if (company == nullptr || company->name != DEMO_NAME || PlanetManager::Count() != 4) {
		IConsolePrint(CC_ERROR, "CONNECTED FAIL not a connected fixture");
		return true;
	}
	AutoRestoreBackup owner(_current_company, CompanyID{0});
	ConfigureIntegratedNodes();
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
