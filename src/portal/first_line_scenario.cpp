/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_line_scenario.cpp Authored empty opening and native scenario diagnostics. */
#include "../stdafx.h"
#include "first_line_scenario.h"
#include "first_line_funding.h"
#include "first_line_approach.h"
#include "first_permanent_line.h"
#include "connected_economy.h"
#include "commonwealth_slice.h"
#include "commonwealth_pack.h"
#include "integrated_economy.h"
#include "megacity_manager.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "portal_cmd.h"
#include "stellar_network.h"
#include "resource_sites.h"
#include "tech_tree.h"
#include "fabrication_manager.h"
#include "logistics_hub.h"
#include "company_stockpile.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_cmd.h"
#include "../console_func.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../newgrf_industries.h"
#include "../station_base.h"
#include "../station_cmd.h"
#include "../station_map.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../rail.h"
#include "../engine_base.h"
#include "../newgrf_engine.h"
#include "../train.h"
#include "../train_cmd.h"
#include "../vehicle_cmd.h"
#include "../order_cmd.h"
#include "../order_base.h"
#include "../signs_cmd.h"
#include "../town.h"
#include "../town_map.h"
#include "../town_cmd.h"
#include "../story_base.h"
#include "../goal_base.h"
#include "../map_func.h"
#include "../track_func.h"
#include "../strings_func.h"
#include "../string_func.h"
#include "../language.h"
#include "../landscape.h"
#include "../network/network.h"
#include "../openttd.h"
#include "../timer/timer_game_tick.h"
#include "../core/backup_type.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include <queue>
#include "../safeguards.h"
extern Company *DoStartupNewCompany(bool, CompanyID);
namespace {
using Json = nlohmann::json;
constexpr std::array<const char *, 4> STOPS{"Clonclurry Iron", "Clonclurry Silica", "Merredin Commission Works", "Mito Commission Warehouse"};
constexpr uint LENGTH = 4;

bool Failure(std::string_view reason)
{
	IConsolePrint(CC_DEFAULT, "FIRSTLINE setup {}", Json{{"stage", "failure"}, {"reason", reason}}.dump());
	IConsolePrint(CC_ERROR, "FIRSTLINE FAIL {}", reason);
	return false;
}
Json SetupCostError(CommandCost &cost)
{
	StringID id = cost.GetErrorMessage(), extra = cost.GetExtraErrorMessage();
	std::string decoded = !cost.GetEncodedMessage().empty() ? cost.GetEncodedMessage().GetDecodedString() :
		id == INVALID_STRING_ID ? "native command rejected" : GetString(id);
	std::string extra_text = extra == INVALID_STRING_ID ? "" : GetString(extra);
	decoded.resize(std::min<size_t>(160, decoded.size())); extra_text.resize(std::min<size_t>(160, extra_text.size()));
	StrMakeValidInPlace(decoded); StrMakeValidInPlace(extra_text);
	return {{"error_id", id.base()}, {"extra_id", extra.base()}, {"encoded_message_present", !cost.GetEncodedMessage().empty()},
		{"decoded", decoded}, {"extra", extra_text},
		{"cost", int64_t(cost.GetCost())}};
}
bool Result(CommandCost cost, std::string_view action)
{
	if (cost.Succeeded()) return true;
	Json error = SetupCostError(cost);
	IConsolePrint(CC_DEFAULT, "FIRSTLINE setup {}", Json{{"stage", "native_command"}, {"action", action}, {"error", error}}.dump());
	return Failure(fmt::format("{}: {}", action, error["decoded"].get<std::string>()));
}
CargoType Cargo(const char *label) { CargoLabel v; std::copy_n(label, 4, v.begin()); return GetCargoTypeByLabel(v); }
const PortalEndpoint *Endpoint(WorldID world, WorldID other)
{
	for (const auto &[id, link] : PortalRegistry::GetAllPortals()) {
		if (link.end_a.world_id == world && link.end_b.world_id == other) return &link.end_a;
		if (link.end_b.world_id == world && link.end_a.world_id == other) return &link.end_b;
	}
	return nullptr;
}
Station *Stop(size_t n) { for (Station *s : Station::Iterate()) if (s->owner == CompanyID{0} && s->name == STOPS[n]) return s; return nullptr; }
/** Put the starter depot beyond the platform end opposite its public approach. */
std::optional<std::pair<TileIndex, DiagDirection>> StarterDepot(TileIndex platform, Axis axis)
{
	const auto *gate = Endpoint(WorldID{2}, WorldID{1});
	if (gate == nullptr) return std::nullopt;
	auto terminal = PortalTerminal::Plan(gate->tile, gate->enter_dir, gate->world_id);
	if (!terminal) return std::nullopt;
	TileIndex target = TileAddByDiagDir(terminal->connection_tile, terminal->outward_dir);
	DiagDirection positive = axis == Axis::X ? DiagDirection::SW : DiagDirection::SE;
	bool gate_positive = axis == Axis::X ? TileX(target) > TileX(platform) : TileY(target) > TileY(platform);
	if (gate_positive) return std::pair{TileAddByDiagDir(platform, ReverseDiagDir(positive)), positive};
	for (uint i = 1; i < LENGTH; ++i) platform = TileAddByDiagDir(platform, positive);
	return std::pair{TileAddByDiagDir(platform, positive), ReverseDiagDir(positive)};
}
EngineID EngineLocal(uint local, GrfID grf = COMMONWEALTH_RAIL_GRFID)
{
	for (const Engine *e : Engine::Iterate()) if (e->type == VehicleType::Train && e->grf_prop.grfid == grf && e->grf_prop.local_id == local) return e->index;
	return EngineID::Invalid();
}
Train *Starter() { for (Train *t : Train::Iterate()) if (t->IsFrontEngine() && t->owner == CompanyID{0}) return t; return nullptr; }
/** Print the bounded native-command evidence for every authored asset. */
Industry *Fund(IndustryType type, WorldID world, TileIndex near, bool primary)
{
	Json report;
	Industry *industry = FirstLineFundIndustry(type, world, near, primary, report);
	IConsolePrint(CC_DEFAULT, "FIRSTLINE funding {}", report.dump());
	if (industry == nullptr) Failure(fmt::format("native industry funding {} at stage {}", report.value("status", "failed"), report.value("stage", "unknown")));
	return industry;
}
bool Catches(const Station *s, const Industry *i) { for (TileIndex tile : i->location) if (s->catchment_tiles.HasTile(tile)) return true; return false; }
/** A prospective four-tile shell; no mutation precedes approach selection. */
struct ShellChoice { TileIndex tile = INVALID_TILE; Axis axis = Axis::X; TileIndex depot = INVALID_TILE; DiagDirection depot_dir = DiagDirection::Invalid; };
struct ApproachChoice { size_t stop = 0; WorldID world{0}; WorldID other{0}; };
constexpr std::array<ApproachChoice, 5> APPROACHES{{
	{0, WorldID{2}, WorldID{1}}, {1, WorldID{2}, WorldID{1}},
	{2, WorldID{1}, WorldID{2}}, {2, WorldID{1}, WorldID{0}}, {3, WorldID{0}, WorldID{1}}}};
Json IndustryContext(const Industry *industry)
{
	if (industry == nullptr) return nullptr;
	Json site = nullptr;
	for (const ResourceSite &record : ResourceSiteManager::Sites()) if (record.occupant == industry->index) {
		site = {{"id", record.id}, {"anchor", record.anchor.base()}, {"x", TileX(record.anchor)},
			{"y", TileY(record.anchor)}, {"width", record.width}, {"height", record.height}};
		break;
	}
	return {{"industry_id", industry->index.base()}, {"tile", industry->location.tile.base()},
		{"world", PlanetManager::GetTileWorld(industry->location.tile).base()}, {"site", site}};
}
std::array<Json, 4> ApproachContext(const Industry *iron, const Industry *silica, const Industry *steel, const Industry *ballast)
{
	return {Json::array({IndustryContext(iron)}), Json::array({IndustryContext(silica)}),
		Json::array({IndustryContext(steel), IndustryContext(ballast)}), Json::array()};
}
/** Native station/depot quotes and bounded 37x37x2 enumeration; first 32 feasible per shell. */
std::vector<ShellChoice> ShellCandidates(size_t n, WorldID world, TileIndex near,
	const std::vector<const Industry *> &industries, FirstLineApproachBudget &budget, Json &report)
{
	report = {{"stage", "shell_candidates"}, {"stop", n}, {"name", STOPS[n]}, {"world", world.base()},
		{"near", near.base()}, {"enumerated", 0}, {"previewed", 0}, {"valid", 0}, {"budget_exhausted", false},
		{"quote_errors", Json::object()}, {"samples", Json::array()}};
	auto refused = [&](const char *stage, TileIndex tile, CommandCost &cost) {
		Json error = SetupCostError(cost);
		std::string key = fmt::format("{}:{}:{}", stage, error["error_id"].get<uint32_t>(), error["extra_id"].get<uint32_t>());
		if (report["quote_errors"].contains(key) || report["quote_errors"].size() < 64) {
			if (!report["quote_errors"].contains(key)) report["quote_errors"][key] = {{"stage", stage}, {"error", error}, {"count", 0}};
			report["quote_errors"][key]["count"] = report["quote_errors"][key]["count"].get<uint32_t>() + 1;
		}
		if (report["samples"].size() < 16) report["samples"].push_back({{"stage", stage}, {"tile", tile.base()}, {"error", error}});
	};
	std::vector<std::pair<uint, TileIndex>> tiles;
	for (int dy = -18; dy <= 18; ++dy) for (int dx = -18; dx <= 18; ++dx) {
		int x = int(TileX(near)) + dx, y = int(TileY(near)) + dy;
		if (x > 1 && y > 1 && x + int(LENGTH) < int(Map::MaxX()) && y + int(LENGTH) < int(Map::MaxY())) tiles.emplace_back(abs(dx) + abs(dy), TileXY(x, y));
	}
	std::sort(tiles.begin(), tiles.end());
	std::vector<ShellChoice> candidates;
	for (auto [distance, tile] : tiles) for (Axis axis : {Axis::X, Axis::Y}) {
		report["enumerated"] = report["enumerated"].get<uint32_t>() + 1;
		bool legal = true;
		for (uint j = 0; j < LENGTH; ++j) {
			TileIndex part = tile + (axis == Axis::X ? TileDiffXY(j, 0) : TileDiffXY(0, j));
			legal &= PlanetManager::GetTileWorld(part) == world;
		}
		for (const Industry *i : industries) {
			bool covers = false;
			for (TileIndex part : i->location) {
				int x = int(TileX(part)) - int(TileX(tile)), y = int(TileY(part)) - int(TileY(tile));
				covers |= x >= -4 && y >= -4 && x <= int(axis == Axis::X ? LENGTH + 3 : 4) && y <= int(axis == Axis::Y ? LENGTH + 3 : 4);
			}
			legal &= covers;
		}
		if (!legal) continue;
		if (!budget.Query()) { report["budget_exhausted"] = true; return candidates; }
		report["previewed"] = report["previewed"].get<uint32_t>() + 1;
		auto quote = Command<Commands::BuildRailStation>::Do({}, tile, RAILTYPE_RAIL, axis, 1, LENGTH, STAT_CLASS_DFLT, 0, NEW_STATION, true);
		if (quote.Failed()) { refused("station_quote", tile, quote); continue; }
		ShellChoice choice{tile, axis};
		if (n == 0) {
			auto depot = StarterDepot(tile, axis);
			if (!depot) continue;
			choice.depot = depot->first; choice.depot_dir = depot->second;
			if (!budget.Query()) { report["budget_exhausted"] = true; return candidates; }
			auto depot_quote = Command<Commands::BuildRailDepot>::Do({}, choice.depot, RAILTYPE_RAIL, choice.depot_dir);
			if (depot_quote.Failed()) { refused("depot_quote", choice.depot, depot_quote); continue; }
		}
		candidates.push_back(choice); report["valid"] = candidates.size();
		if (candidates.size() == 32) return candidates;
	}
	return candidates;
}
/** Full footprint reservation and all five quotes use one shared authoring budget. */
bool PlanApproaches(const std::array<ShellChoice, 4> &shells, FirstLineApproachBudget &budget,
	const std::array<Json, 4> &industries, std::array<FirstLineApproachPlan, 5> &plans,
	std::array<Json, 5> &reports, size_t &failed_stop, int64_t &quote)
{
	for (Json &report : reports) report = nullptr;
	std::set<TileIndex> blocked;
	for (size_t i = 0; i < shells.size(); ++i) for (uint j = 0; j < LENGTH; ++j) {
		TileIndex part = shells[i].tile + (shells[i].axis == Axis::X ? TileDiffXY(j, 0) : TileDiffXY(0, j));
		if (!blocked.insert(part).second) {
			failed_stop = i; reports[0] = {{"status", "overlapping_shells"}, {"stop", i}, {"tile", part.base()}}; return false;
		}
	}
	if (shells[0].depot != INVALID_TILE && !blocked.insert(shells[0].depot).second) {
		failed_stop = 0; reports[0] = {{"status", "depot_overlaps_shell"}, {"tile", shells[0].depot.base()}}; return false;
	}
	std::map<TileIndex, TrackBits> rails;
	FirstLineBridgeReservations bridges;
	quote = 0;
	for (size_t leg = 0; leg < APPROACHES.size(); ++leg) {
		const auto &choice = APPROACHES[leg]; failed_stop = choice.stop;
		const PortalEndpoint *gate = Endpoint(choice.world, choice.other);
		const auto *policy = gate == nullptr ? nullptr : StellarNetwork::Policy(gate->tile);
		Json context{{"stop", choice.stop}, {"name", STOPS[choice.stop]}, {"world", choice.world.base()},
			{"other", choice.other.base()}, {"platform", shells[choice.stop].tile.base()},
			{"axis", uint(shells[choice.stop].axis)}, {"depot", shells[choice.stop].depot == INVALID_TILE ? Json(nullptr) : Json(shells[choice.stop].depot.base())},
			{"industries", industries[choice.stop]}};
		if (gate != nullptr) {
			const PortalLink *link = PortalRegistry::GetPortalLink(gate->tile);
			context["gate_id"] = link == nullptr ? Json(nullptr) : Json(link->id.base());
			context["gate_tile"] = gate->tile.base(); context["gate_dir"] = uint(gate->enter_dir);
		}
		if (gate == nullptr || policy == nullptr || !policy->public_access || Company::IsValidID(policy->owner)) {
			reports[leg] = context; reports[leg]["status"] = "missing_neutral_public_gate";
			return false;
		}
		if (!FirstLineQuoteApproach(shells[choice.stop].tile, shells[choice.stop].axis, LENGTH, *gate,
			blocked, rails, bridges, budget, plans[leg], reports[leg])) {
			reports[leg].update(context); return false;
		}
		reports[leg].update(context);
		quote += plans[leg].cost;
	}
	return true;
}
/** Bound candidate changes as well as graph states. All four footprints are known before each attempt. */
bool ChooseShells(const std::array<std::vector<ShellChoice>, 4> &candidates, FirstLineApproachBudget &budget,
	const std::array<Json, 4> &industries, std::array<ShellChoice, 4> &shells, std::array<FirstLineApproachPlan, 5> &plans,
	std::array<Json, 5> &reports, int64_t &quote)
{
	for (const auto &list : candidates) if (list.empty()) return false;
	std::array<size_t, 4> selected{};
	uint32_t combinations = 0;
	while (!budget.exhausted && combinations < 128) {
		++combinations;
		for (size_t i = 0; i < shells.size(); ++i) shells[i] = candidates[i][selected[i]];
		size_t failed = 0;
		if (PlanApproaches(shells, budget, industries, plans, reports, failed, quote)) {
			for (Json &r : reports) IConsolePrint(CC_DEFAULT, "FIRSTLINE approach {}", r.dump());
			return true;
		}
		if (budget.exhausted) break;
		/* First vary the failed shell, then higher-index shells. The shared state/query
		 * caps stop this finite search even if all 32^4 combinations would fail. */
		size_t index = failed;
		for (; index < selected.size(); ++index) {
			if (++selected[index] < candidates[index].size()) break;
			selected[index] = 0;
		}
		if (index == selected.size()) break;
	}
	for (const Json &r : reports) if (!r.is_null()) IConsolePrint(CC_DEFAULT, "FIRSTLINE approach {}", r.dump());
	IConsolePrint(CC_DEFAULT, "FIRSTLINE setup {}", Json{{"stage", "joint_search"}, {"combinations", combinations}, {"combination_cap", 128},
		{"states", budget.states}, {"queries", budget.queries}, {"budget_exhausted", budget.exhausted}}.dump());
	return false;
}
Station *BuildShell(size_t n, const ShellChoice &choice, const std::vector<const Industry *> &industries)
{
	auto built = Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute, choice.tile, RAILTYPE_RAIL, choice.axis, 1, LENGTH, STAT_CLASS_DFLT, 0, NEW_STATION, true);
	if (!Result(built, fmt::format("station shell {} tile {}", STOPS[n], choice.tile.base()))) return nullptr;
	Station *station = Station::Get(GetStationIndex(choice.tile));
	if (!Result(Command<Commands::RenameStation>::Do(DoCommandFlag::Execute, station->index, STOPS[n]), "station sign")) return nullptr;
	for (const Industry *industry : industries) if (!Catches(station, industry)) { Failure(fmt::format("native shell {} catchment differs at industry {}", STOPS[n], industry->index.base())); return nullptr; }
	return station;
}
/** Replan against saved native shells before ordinary paid construction. */
bool Connections(bool execute, int64_t &quote)
{
	std::array<ShellChoice, 4> shells;
	for (size_t i = 0; i < shells.size(); ++i) {
		Station *station = Stop(i);
		if (station == nullptr) return Failure(fmt::format("missing shell {}", STOPS[i]));
		shells[i] = {station->train_station.tile, GetRailStationAxis(station->train_station.tile)};
	}
	auto depot = StarterDepot(shells[0].tile, shells[0].axis);
	if (!depot) return Failure("missing starter depot plan");
	shells[0].depot = depot->first; shells[0].depot_dir = depot->second;
	FirstLineApproachBudget budget;
	std::array<FirstLineApproachPlan, 5> plans;
	std::array<Json, 5> reports;
	size_t failed = 0;
	const auto &binding = FirstPermanentLine::State().binding;
	auto context = ApproachContext(Industry::GetIfValid(binding.iron_source), Industry::GetIfValid(binding.silica_source),
		Industry::GetIfValid(binding.steel_works), Industry::GetIfValid(binding.ballast_works));
	if (!PlanApproaches(shells, budget, context, plans, reports, failed, quote)) {
		for (const Json &r : reports) if (!r.is_null()) IConsolePrint(CC_DEFAULT, "FIRSTLINE approach {}", r.dump());
		return Failure(fmt::format("native approach quote rejected near {}", STOPS[failed]));
	}
	for (const Json &r : reports) IConsolePrint(CC_DEFAULT, "FIRSTLINE approach {}", r.dump());
	if (!execute) return true;
	for (size_t i = 0; i < plans.size(); ++i) {
		Json receipt;
		bool okay = FirstLineExecuteApproach(plans[i], receipt);
		receipt["stop"] = APPROACHES[i].stop; receipt["name"] = STOPS[APPROACHES[i].stop];
		IConsolePrint(CC_DEFAULT, "FIRSTLINE approach execution {}", receipt.dump());
		if (!okay) return Failure(fmt::format("native paid approach rejected at leg {}: {}", i, receipt.dump()));
	}
	return true;
}
bool Wagon(TileIndex depot, VehicleID front, uint local, const char *label, GrfID grf = COMMONWEALTH_RAIL_GRFID)
{
	EngineID e = EngineLocal(local, grf); if (e == EngineID::Invalid()) return Failure("missing loaded wagon");
	auto [cost, id, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, e, false, Cargo(label), ClientID::Invalid);
	return Result(cost, "empty wagon") && Result(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, id, front, false), "ordinary coupling");
}
Json Snapshot()
{
	Json r = GetCoreBasketObservation();
	r["mission"] = FirstPermanentLine::Active() ? Json::parse(FirstPermanentLine::Save()) : Json{};
	r["economy"] = Json::parse(IntegratedEconomy::Save());
	r["money"] = int64_t(Company::Get(CompanyID{0})->money); r["tick"] = uint64_t(TimerGameTick::counter);
	r["map_fnv1a64"] = GetConnectedEconomyMapFingerprint();
	r["progress_bits"] = 0;
	r["story_pages"] = StoryPage::GetNumItems();
	r["story_elements"] = StoryPageElement::GetNumItems();
	r["goals"] = Goal::GetNumItems();
	r["mito_kit"] = {StockpileManager::GetStock(WorldID{0}, CompanyID{0}, Cargo("STEL")), StockpileManager::GetStock(WorldID{0}, CompanyID{0}, Cargo("BALL"))};
	r["stocks"] = Json::array();
	for (const auto &stock : StockpileManager::GetAllStockpiles()) r["stocks"].push_back({stock.world_id.base(), stock.company_id.base(), stock.inventory});
	r["industries"] = Json::array();
	for (const Industry *i : Industry::Iterate()) {
		Json row{{"id", i->index.base()}, {"tile", i->location.tile.base()}, {"world", PlanetManager::GetTileWorld(i->location.tile).base()}, {"owner", i->founder.base()}, {"recipe", IntegratedEconomy::IndustryRecipe(i->type)}, {"inputs", Json::array()}, {"outputs", Json::array()}};
		for (const auto &cargo : i->accepted) row["inputs"].push_back({cargo.cargo, cargo.waiting});
		for (const auto &cargo : i->produced) row["outputs"].push_back({cargo.cargo, cargo.waiting});
		r["industries"].push_back(row);
	}
	r["stations"] = Json::array();
	for (const Station *station : Station::Iterate()) {
		std::array<uint64_t, NUM_CARGO> cargo{};
		for (CargoType c{0}; c < NUM_CARGO; ++c) cargo[c] = station->goods[c].TotalCount();
		r["stations"].push_back({{"id", station->index.base()}, {"tile", station->xy.base()}, {"owner", station->owner.base()}, {"name", station->name}, {"world", PlanetManager::GetTileWorld(station->xy).base()}, {"cargo", cargo}});
	}
	r["trains"] = Json::array();
	for (const Train *train : Train::Iterate()) {
		Json orders = Json::array();
		if (train->IsFrontEngine()) for (const auto &order : train->Orders()) orders.push_back({order.GetType(), order.GetDestination().base(), order.GetLoadType(), order.GetUnloadType(), order.GetNonStopType().base()});
		r["trains"].push_back({{"id", train->index.base()}, {"owner", train->owner.base()}, {"front", train->IsFrontEngine()}, {"tile", train->tile.base()}, {"x", train->x_pos}, {"y", train->y_pos}, {"engine", train->engine_type.base()}, {"cargo_type", train->cargo_type}, {"cargo", train->cargo.StoredCount()}, {"capacity", train->cargo_cap}, {"speed", train->cur_speed}, {"orders", orders}, {"order_index", train->cur_real_order_index}, {"stopped", train->vehstatus.Test(VehState::Stopped)}, {"crashed", train->vehstatus.Test(VehState::Crashed)}, {"lost", train->vehicle_flags.Test(VehicleFlag::PathfinderLost)}});
	}
	r["district_houses"] = Json::array();
	const auto &mission = FirstPermanentLine::State();
	for (size_t i = 0; i < mission.house_count; ++i) {
		TileIndex tile = mission.houses[i];
		bool exists = tile < Map::Size() && IsTileType(tile, TileType::House);
		r["district_houses"].push_back({{"tile", tile.base()}, {"exists", exists}, {"completed", exists && IsHouseCompleted(tile)}, {"protected", exists && IsHousePlayerProtected(tile)}, {"town", exists ? GetTownIndex(tile).base() : TownID::Invalid().base()}});
	}
	r["research"] = Json::array();
	for (const auto &tech : TechTreeManager::GetAllCompanyTechStates()) r["research"].push_back({tech.active_project, tech.accumulated_rp, tech.monthly_budget, tech.unlocked_techs});
	return r;
}
bool Prepare()
{
	const Json admission{{"active", FirstPermanentLine::Active()}, {"vehicles", Vehicle::GetNumItems()},
		{"stations", Station::GetNumItems()}, {"industries", Industry::GetNumItems()}, {"companies", Company::GetNumItems()},
		{"content_ready", IntegratedEconomy::ContentReady()}, {"stellar_enabled", StellarNetwork::Enabled()}};
	if (FirstPermanentLine::Active() || Vehicle::GetNumItems() != 0 || Station::GetNumItems() != 0 || Industry::GetNumItems() != 0 || Company::GetNumItems() > 1 || !IntegratedEconomy::ContentReady() || !StellarNetwork::Enabled()) return Failure(fmt::format("empty opening admission rejected: {}", admission.dump()));
	const std::array<const PlanetRegion *, 3> sector{
		PlanetManager::GetRegion(WorldID{0}), PlanetManager::GetRegion(WorldID{1}), PlanetManager::GetRegion(WorldID{2})};
	Json observed = Json::array();
	for (size_t i = 0; i < sector.size(); ++i) observed.push_back(sector[i] == nullptr ? Json{{"id", i}, {"present", false}} :
		Json{{"id", i}, {"present", true}, {"name", sector[i]->name}, {"phase", uint(sector[i]->phase)}});
	if (sector[0] == nullptr || sector[1] == nullptr || sector[2] == nullptr ||
		sector[0]->name != "Mito" || sector[1]->name != "Merredin" || sector[2]->name != "Clonclurry" ||
		sector[0]->phase != WorldPhase::Phase1_Core || sector[1]->phase != WorldPhase::Phase2_Developed ||
		sector[2]->phase != WorldPhase::Phase3_Frontier) {
		IConsolePrint(CC_DEFAULT, "FIRSTLINE setup {}", Json{{"stage", "sector_identity"}, {"observed", observed}}.dump());
		return Failure("required Core/Industrial/Frontier sector IDs or phases differ");
	}
	Company *c = Company::GetIfValid(CompanyID{0}); if (c == nullptr) c = DoStartupNewCompany(false, CompanyID{0});
	if (c == nullptr) return Failure("player company missing");
	AutoRestoreBackup owner(_current_company, c->index); _pause_mode.Set(PauseMode::Normal);
	if (!IntegratedEconomy::StartNewWorksPrototype()) return Failure("v2 content unavailable");
	/* Founding assets and unlock are authored once before the observed baseline. */
	c->money = 100000000; // Temporary asset-authoring allowance, removed before baseline.
	TechTreeManager::RestoreCompanyTech(c->index, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	FabricationManager::SetFabricateFromStockpile(c->index, false);
	const auto *frontier = Endpoint(WorldID{2}, WorldID{1});
	const auto *merredin_a = Endpoint(WorldID{1}, WorldID{2}), *merredin_b = Endpoint(WorldID{1}, WorldID{0});
	if (frontier == nullptr || merredin_a == nullptr || merredin_b == nullptr) return Failure("required public sector links absent");
	Industry *iron = Fund(MapNewGRFIndustryType(0x80 | 0x12, COMMONWEALTH_INDUSTRY_GRFID), WorldID{2}, frontier->tile, true);
	if (!iron) return false;
	Industry *silica = Fund(MapNewGRFIndustryType(0x80 | 0x10, COMMONWEALTH_INDUSTRY_GRFID), WorldID{2}, frontier->tile, true);
	if (!silica) return false;
	TileIndex middle = TileXY((TileX(merredin_a->tile) + TileX(merredin_b->tile)) / 2, (TileY(merredin_a->tile) + TileY(merredin_b->tile)) / 2);
	Industry *steel = Fund(MapNewGRFIndustryType(0x80 | 0x13, COMMONWEALTH_INDUSTRY_GRFID), WorldID{1}, middle, false);
	if (!steel) return false;
	Industry *ballast = Fund(MapNewGRFIndustryType(0x80 | 0x11, COMMONWEALTH_INDUSTRY_GRFID), WorldID{1}, middle + TileDiffXY(7, 0), false);
	if (!ballast) return false;
	Town *town = nullptr; for (Town *t : Town::Iterate()) if (PlanetManager::GetTileWorld(t->xy) == WorldID{0}) { town = t; break; }
	if (town == nullptr) return Failure("native Mito Core town missing");
	if (!Result(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, town->index), "Mito monthly settlement registration")) return false;
	const MegacityProfile *profile = MegacityManager::GetProfile(town->index);
	if (profile == nullptr || profile->town_id != town->index || profile->world_id != WorldID{0}) return Failure("native Mito Core monthly profile missing");
	FirstLineApproachBudget approach_budget;
	std::array<Json, 4> shell_reports;
	std::array<std::vector<ShellChoice>, 4> shell_candidates{
		ShellCandidates(0, WorldID{2}, iron->location.tile, {iron}, approach_budget, shell_reports[0]),
		ShellCandidates(1, WorldID{2}, silica->location.tile, {silica}, approach_budget, shell_reports[1]),
		ShellCandidates(2, WorldID{1}, middle, {steel, ballast}, approach_budget, shell_reports[2]),
		ShellCandidates(3, WorldID{0}, town->xy, {}, approach_budget, shell_reports[3])};
	for (const Json &r : shell_reports) IConsolePrint(CC_DEFAULT, "FIRSTLINE setup {}", r.dump());
	if (approach_budget.exhausted) return Failure("shared shell/approach native-query budget exhausted");
	std::array<ShellChoice, 4> chosen_shells;
	std::array<FirstLineApproachPlan, 5> chosen_approaches;
	std::array<Json, 5> approach_reports;
	int64_t quote = 0;
	auto approach_context = ApproachContext(iron, silica, steel, ballast);
	if (!ChooseShells(shell_candidates, approach_budget, approach_context, chosen_shells, chosen_approaches, approach_reports, quote))
		return Failure(fmt::format("no jointly legal four-shell/five-approach opening; states={} queries={} exhausted={}", approach_budget.states, approach_budget.queries, approach_budget.exhausted));
	Station *stops[4];
	stops[0] = BuildShell(0, chosen_shells[0], {iron}); if (!stops[0]) return false;
	stops[1] = BuildShell(1, chosen_shells[1], {silica}); if (!stops[1]) return false;
	stops[2] = BuildShell(2, chosen_shells[2], {steel, ballast}); if (!stops[2]) return false;
	stops[3] = BuildShell(3, chosen_shells[3], {}); if (!stops[3]) return false;
	if (!Result(Command<Commands::BuildLogisticsHub>::Do(DoCommandFlag::Execute, stops[3]->xy, stops[3]->index, "Mito Cooperative Warehouse"), "empty Mito warehouse")) return false;
	TileIndex depot = chosen_shells[0].depot;
	DiagDirection dir = chosen_shells[0].depot_dir;
	if (!Result(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, dir), "company depot")) return false;
	auto [cost, train, capacity, mail, cargos] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, EngineLocal(0x20), false, INVALID_CARGO, ClientID::Invalid);
	if (!Result(cost, "empty Pioneer") || !Wagon(depot, train, 0x32, "IRON") || !Wagon(depot, train, 0x32, "IRON") || !Wagon(depot, train, 0x32, "SILC")) return false;
	c->money = 500000 + quote * 2; // Cash-only stake covers quoted approaches and contingency.
	if (!Result(Command<Commands::RenameCompany>::Do(DoCommandFlag::Execute, "First Permanent Line"), "company name")) return false;
	Json before = Snapshot(); for (auto units : before["custody"]["total"]) if (units != 0) return Failure("nonempty baseline custody");
	FirstPermanentLineBindings b{c->index, town->index, steel->index, ballast->index, iron->index, silica->index};
	if (!FirstPermanentLine::Begin(b)) return Failure("native observer rejected opening");
	IConsolePrint(CC_DEFAULT, "FIRSTLINE prepared {}", Json{{"stake", int64_t(c->money)}, {"construction_quote", quote}, {"unlock", "Materials I"}, {"assets", "2 native works, 2 legal extractors, 4 empty platforms, warehouse, depot, Pioneer + 2 IRON hoppers + 1 SILC hopper; native Mito monthly settlement registration"}, {"baseline", Snapshot()}}.dump()); return true;
}
bool Build()
{
	const auto &s = FirstPermanentLine::State(); Train *train = Starter();
	if (!s.active || s.invalid || s.economy_evaluations != 0 || train == nullptr || train->GetNumOrders() != 0 || !train->vehstatus.Test(VehState::Stopped) || !train->IsInDepot()) return Failure("proof construction requires untouched authored baseline");
	int64_t quote = 0; if (!Connections(true, quote)) return false;
	/* One ordinary mixed service avoids imposing a fleet size or scripted freight. */
	TileIndex depot = train->tile;
	if (!Wagon(depot, train->index, 0x33, "STEL") || !Wagon(depot, train->index, 0x33, "STEL") || !Wagon(depot, train->index, 0x38, "BALL", COMMONWEALTH_INDUSTRY_GRFID) || !Wagon(depot, train->index, 0x38, "BALL", COMMONWEALTH_INDUSTRY_GRFID)) return false;
	for (uint8_t n = 0; n < 4; ++n) {
		Order order; order.MakeGoToStation(Stop(n)->index); order.SetNonStopType(OrderNonStopFlags{OrderNonStopFlag::NonStop});
		if (n < 2) order.SetUnloadType(OrderUnloadType::NoUnload);
		if (n == 3) order.SetLoadType(OrderLoadType::NoLoad);
		if (!Result(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute, train->index, VehicleOrderID{n}, order), "recurring native order")) return false;
	}
	if (!Result(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, train->index, true), "ordinary departure")) return false;
	IConsolePrint(CC_DEFAULT, "FIRSTLINE built {}", Snapshot().dump()); return true;
}
Json Audit()
{
	Json r{{"invalid_slopes", 0}, {"invalid_foundations", 0}, {"pixel_queries", 0}, {"viewport_queries", 0}, {"language", GetCurrentLanguageIsoCode()}, {"cargos", Json::array()}};
	for (uint y = 0; y < Map::MaxY(); ++y) for (uint x = 0; x < Map::MaxX(); ++x) {
		TileIndex t = TileXY(x, y); int n = TileHeight(t), w = TileHeight(TileXY(x + 1, y)), e = TileHeight(TileXY(x, y + 1)), south = TileHeight(TileXY(x + 1, y + 1));
		if (abs(n - w) > 1 || abs(n - e) > 1 || abs(south - w) > 1 || abs(south - e) > 1) r["invalid_slopes"] = r["invalid_slopes"].get<uint>() + 1;
		if (IsPlainRailTile(t) && GetRailFoundation(GetTileSlope(t), GetTrackBits(t)) == Foundation::Invalid) r["invalid_foundations"] = r["invalid_foundations"].get<uint>() + 1;
	}
	if (r["invalid_slopes"] == 0) for (uint y = 0; y < Map::MaxY(); ++y) for (uint x = 0; x < Map::MaxX(); ++x) {
		for (int offset : {0, 8, 15}) GetSlopePixelZ(x * TILE_SIZE + offset, y * TILE_SIZE + offset);
		Point p = RemapCoords2(x * TILE_SIZE + 8, y * TILE_SIZE + 8); InverseRemapCoords2(p.x, p.y, true);
	}
	r["pixel_queries"] = r["invalid_slopes"] == 0 ? uint64_t(Map::MaxX()) * Map::MaxY() * 3 : 0;
	r["viewport_queries"] = r["invalid_slopes"] == 0 ? uint64_t(Map::MaxX()) * Map::MaxY() : 0;
	for (const CargoSpec *c : CargoSpec::Iterate()) r["cargos"].push_back({{"label", c->label.AsString()}, {"name", GetString(c->name)}, {"single", GetString(c->name_single)}, {"units", GetString(c->units_volume, 100)}, {"quantity", GetString(c->quantifier, 100)}, {"zero", GetString(c->quantifier, 0)}, {"large", GetString(c->quantifier, 100000)}, {"abbreviation", GetString(c->abbrev)}});
	return r;
}
}
bool ConFirstLineScenario(std::span<std::string_view> argv)
{
	if (argv.size() != 2 || _game_mode != GameMode::Normal || _networking || _network_dedicated || PlanetManager::Count() != 7 || Map::SizeX() != 1024 || Map::SizeY() != 1024 || _settings_game.game_creation.generation_seed != 11 || _settings_game.difficulty.infinite_money) { Failure("isolated offline seed11 sector required"); return true; }
	if (argv[1] == "prepare") { Prepare(); return true; }
	if (!FirstPermanentLine::Active() || !Company::IsValidID(FirstPermanentLine::State().binding.company)) { Failure("no authored mission"); return true; }
	AutoRestoreBackup owner(_current_company, FirstPermanentLine::State().binding.company);
	if (argv[1] == "status") IConsolePrint(CC_DEFAULT, "FIRSTLINE state {}", Snapshot().dump());
	else if (argv[1] == "audit") {
		const LanguageMetadata *prior = _current_language;
		Json languages;
		for (const char *filename : {"english_US.lng", "english.lng"}) {
			const auto it = std::ranges::find_if(_languages, [filename](const auto &language) { return language.file.filename() == filename; });
			if (it == _languages.end() || !ReadLanguagePack(&*it)) { Failure("required English audit pack unavailable"); return true; }
			languages[filename] = Audit();
		}
		if (!ReadLanguagePack(prior)) { Failure("cannot restore audit language"); return true; }
		IConsolePrint(CC_DEFAULT, "FIRSTLINE audit {}", languages.dump());
	}
	else if (argv[1] == "build") Build();
	else if (argv[1] == "commission") {
		const auto &s = FirstPermanentLine::State(); auto before = Snapshot();
		auto quote = Command<Commands::CommissionTownResidentialDistrict>::Do({}, s.binding.mito);
		if (!Result(quote, "district quote")) return true;
		auto cost = Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, s.binding.mito);
		if (!Result(cost, "native district commission")) return true;
		auto after = Snapshot(); IConsolePrint(CC_DEFAULT, "FIRSTLINE commissioned {}", Json{{"before", before}, {"after", after}, {"cost", int64_t(cost.GetCost())}}.dump());
	} else if (argv[1] == "advance") {
		const auto start = FirstPermanentLine::State().economy_evaluations;
		if (start >= 28) { Failure("proof absolute evaluation ceiling"); return true; }
		Json before = Snapshot(); CommonwealthSliceAudit audit; AutoRestoreBackup observer(_commonwealth_slice_audit, &audit);
		AutoRestoreBackup pause(_pause_mode); AutoRestoreBackup tick_owner(_current_company, _local_company); _pause_mode.Reset(); UpdateSignalsInBuffer();
		uint ticks = 0; while (FirstPermanentLine::State().economy_evaluations == start && ticks++ < 3000 && !FirstPermanentLine::State().invalid) StateGameLoop();
		FirstPermanentLine::Evaluate(); Json after = Snapshot();
		after["audit"] = {{"produced", audit.produced}, {"raw_produced", audit.raw_produced}, {"raw_removed", audit.raw_removed}, {"unallocated", audit.unallocated}, {"discarded", audit.discarded}, {"consumed", audit.consumed}, {"vehicle_deliveries", audit.vehicle_deliveries}, {"arrivals", audit.arrivals}, {"processor_cargo", audit.processor_cargo}, {"cash_debits", audit.cash_debits}, {"cash_transactions", audit.cash_transactions}, {"gate_tolls", audit.gate_tolls}};
		after["cash_conserved"] = after["money"].get<int64_t>() == before["money"].get<int64_t>() - audit.cash_debits;
		after["cargo_errors"] = Json::array();
		for (CargoType cargo{0}; cargo < NUM_CARGO; ++cargo) {
			int64_t expected = before["custody"]["total"][cargo].get<int64_t>() + audit.produced[cargo] + audit.raw_produced[cargo] - audit.raw_removed[cargo] - audit.unallocated[cargo] - audit.discarded[cargo] - audit.consumed[cargo];
			if (expected != after["custody"]["total"][cargo].get<int64_t>()) after["cargo_errors"].push_back({cargo, expected, after["custody"]["total"][cargo]});
		}
		after["evaluations_advanced"] = FirstPermanentLine::State().economy_evaluations - start;
		IConsolePrint(CC_DEFAULT, "FIRSTLINE advance {}", after.dump());
	} else Failure("unknown operation");
	return true;
}
