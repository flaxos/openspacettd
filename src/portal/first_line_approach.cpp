/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_line_approach.cpp Paid, bounded ordinary rail/bridge gate approaches. */
#include "../stdafx.h"
#include "first_line_approach.h"
#include "connected_economy.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "../tunnelbridge_cmd.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../genworld.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../rail_cmd.h"
#include "../rail_map.h"
#include "../road_map.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../track_func.h"
#include "../tunnelbridge_map.h"
#include <algorithm>
#include <map>
#include <queue>
#include <tuple>
#include "../safeguards.h"

namespace {
using Json = nlohmann::json;
constexpr uint32_t LEG_STATES = 30000;
constexpr size_t MAX_SAMPLES = 16;
constexpr size_t MAX_ERRORS = 64;

Json Error(CommandCost &cost)
{
	StringID id = cost.GetErrorMessage(), extra = cost.GetExtraErrorMessage();
	std::string decoded = !cost.GetEncodedMessage().empty() ? cost.GetEncodedMessage().GetDecodedString() :
		id == INVALID_STRING_ID ? "native command rejected" : GetString(id);
	std::string extra_text = extra == INVALID_STRING_ID ? "" : GetString(extra);
	decoded.resize(std::min<size_t>(160, decoded.size())); extra_text.resize(std::min<size_t>(160, extra_text.size()));
	StrMakeValidInPlace(decoded); StrMakeValidInPlace(extra_text);
	return {{"error_id", id.base()}, {"extra_id", extra.base()}, {"encoded_message_present", !cost.GetEncodedMessage().empty()}, {"decoded", decoded},
		{"extra", extra_text}, {"cost", int64_t(cost.GetCost())}};
}

void Rejected(Json &report, const char *stage, TileIndex tile, CommandCost &cost)
{
	Json error = Error(cost);
	std::string kind = fmt::format("{}:{}:{}", stage, error["error_id"].get<uint32_t>(), error["extra_id"].get<uint32_t>());
	if (!report["errors"].contains(kind) && report["errors"].size() >= MAX_ERRORS) {
		report["error_overflow"] = report["error_overflow"].get<uint32_t>() + 1;
	} else {
		if (!report["errors"].contains(kind)) report["errors"][kind] = {{"stage", stage}, {"error", error}, {"count", 0}};
		report["errors"][kind]["count"] = report["errors"][kind]["count"].get<uint32_t>() + 1;
	}
	if (report["samples"].size() < MAX_SAMPLES) report["samples"].push_back({{"stage", stage}, {"tile", tile.base()}, {"x", TileX(tile)}, {"y", TileY(tile)}, {"error", error}});
}

Track ApproachTrackCorner(DiagDirection a, DiagDirection b)
{
	for (Track track : TRACK_BIT_ALL) {
		Trackdir td = TrackToTrackdir(track);
		DiagDirection x = TrackdirToExitdir(td), y = TrackdirToExitdir(ReverseTrackdir(td));
		if ((x == a && y == b) || (x == b && y == a)) return track;
	}
	return Track::Invalid;
}

bool Ground(TileIndex tile)
{
	return IsTileType(tile, TileType::Clear) || IsTileType(tile, TileType::Trees) || IsTileType(tile, TileType::Road) ||
		(IsPlainRailTile(tile) && GetTileOwner(tile) == CompanyID{0});
}

/* A normal rail build on any other road shape removes town road pieces and can
 * consume town authority over successive paid commands. Keep only the native
 * full, perpendicular level-crossing shape; the command still decides legality. */
bool PreservesRoad(TileIndex tile, Track track)
{
	if (!IsTileType(tile, TileType::Road)) return true;
	if (IsLevelCrossingTile(tile)) return GetTileOwner(tile) == CompanyID{0} && GetRailType(tile) == RAILTYPE_RAIL &&
		GetCrossingRailTrack(tile) == track &&
		((track == Track::X && GetCrossingRoadBits(tile) == ROAD_Y) ||
		 (track == Track::Y && GetCrossingRoadBits(tile) == ROAD_X));
	return IsNormalRoadTile(tile) &&
		((track == Track::X && GetAllRoadBits(tile) == ROAD_Y) ||
		 (track == Track::Y && GetAllRoadBits(tile) == ROAD_X));
}

bool ClearBridgeHead(TileIndex tile)
{
	return IsTileType(tile, TileType::Clear) || IsTileType(tile, TileType::Trees);
}

Owner ObservedOwner(TileIndex tile)
{
	if (IsTileType(tile, TileType::Void) || IsTileType(tile, TileType::House) || IsTileType(tile, TileType::Industry)) return OWNER_NONE;
	return GetTileOwner(tile);
}

struct Prior { uint32_t parent; FirstLineApproachPiece piece; };
}

bool FirstLineQuoteApproach(TileIndex platform, Axis axis, uint length, const PortalEndpoint &gate,
	const std::set<TileIndex> &blocked, std::map<TileIndex, TrackBits> &planned_rails,
	FirstLineBridgeReservations &reserved_bridges, FirstLineApproachBudget &budget,
	FirstLineApproachPlan &plan, Json &report)
{
	report = {{"status", "no_route"}, {"platform", platform.base()}, {"axis", uint(axis)}, {"length", length},
		{"gate", {{"tile", gate.tile.base()}, {"world", gate.world_id.base()}, {"enter_dir", uint(gate.enter_dir)}}},
		{"states", 0}, {"queries", 0}, {"shared_states", budget.states}, {"shared_queries", budget.queries},
		{"state_cap", LEG_STATES}, {"shared_state_cap", FirstLineApproachBudget::MAX_STATES},
		{"query_cap", FirstLineApproachBudget::MAX_QUERIES}, {"closest_distance", nullptr},
		{"queue_exhausted", false}, {"leg_cap_hit", false}, {"budget_exhausted", false}, {"rejections", Json::object()},
		{"errors", Json::object()}, {"error_overflow", 0}, {"samples", Json::array()},
		{"rail_pieces", 0}, {"bridge_pieces", 0}, {"reused_bridge_pieces", 0},
		{"route", Json::array()}, {"route_truncated", 0}, {"cost", nullptr}};
	plan = {};
	auto reject = [&](const char *kind) { report["rejections"][kind] = report["rejections"].value(kind, 0u) + 1; };
	auto tile_reject = [&](const char *kind, TileIndex tile) {
		reject(kind);
		if (report["samples"].size() < MAX_SAMPLES) report["samples"].push_back({{"stage", kind},
			{"tile", tile.base()}, {"x", TileX(tile)}, {"y", TileY(tile)},
			{"type", uint(GetTileType(tile))}, {"owner", ObservedOwner(tile).base()}, {"slope", uint(GetTileSlope(tile))}});
	};
	auto queried = [&]() {
		if (!budget.Query()) { report["budget_exhausted"] = true; report["status"] = "budget_exhausted"; return false; }
		report["queries"] = report["queries"].get<uint32_t>() + 1;
		report["shared_queries"] = budget.queries;
		return true;
	};
	if (platform >= Map::Size() || gate.tile >= Map::Size() || (axis != Axis::X && axis != Axis::Y) ||
		length == 0 || length > 8 || budget.exhausted || _game_mode != GameMode::Normal || _generating_world ||
		_current_company != CompanyID{0} || !Company::IsValidID(_current_company) ||
		PlanetManager::GetRegion(gate.world_id) == nullptr || PlanetManager::GetTileWorld(platform) != gate.world_id) {
		report["status"] = "invalid_request"; return false;
	}
	auto terminal = PortalTerminal::Plan(gate.tile, gate.enter_dir, gate.world_id);
	if (!terminal) { report["status"] = "invalid_terminal"; return false; }
	TileIndex target = TileAddByDiagDir(terminal->connection_tile, terminal->outward_dir);
	DiagDirection positive = axis == Axis::X ? DiagDirection::SW : DiagDirection::SE;
	report["gate"]["join"] = target.base(); report["gate"]["outward_dir"] = uint(terminal->outward_dir);
	report["start"] = nullptr; report["target"] = target.base();
	if (!IsInnerTile(target) || PlanetManager::GetTileWorld(target) != gate.world_id) {
		report["status"] = "blocked_start_or_target"; return false;
	}
	using Q = std::tuple<uint32_t, uint32_t, uint32_t>; // estimate, cost, direction-state key
	std::priority_queue<Q, std::vector<Q>, std::greater<Q>> queue;
	std::map<uint32_t, Prior> previous;
	std::map<uint32_t, uint32_t> costs;
	std::map<uint32_t, TileIndex> roots;
	TileIndex far_edge = platform;
	for (uint i = 1; i < length; ++i) far_edge = TileAddByDiagDir(far_edge, positive);
	for (auto [edge, outward] : {std::pair{platform, ReverseDiagDir(positive)}, std::pair{far_edge, positive}}) {
		TileIndex candidate = TileAddByDiagDir(edge, outward);
		if (!IsInnerTile(candidate) || blocked.contains(candidate) ||
			PlanetManager::GetTileWorld(candidate) != gate.world_id) continue;
		uint32_t key = candidate.base() * 16 + uint(DiagDirToDiagTrackdir(outward));
		if (!budget.State()) { report["status"] = "budget_exhausted"; report["budget_exhausted"] = true; report["shared_states"] = budget.states; return false; }
		roots[key] = candidate;
		costs[key] = 0; queue.emplace(3 * DistanceManhattan(candidate, target), 0, key);
	}
	if (roots.empty()) { report["status"] = "blocked_start_or_target"; return false; }
	uint32_t last = UINT32_MAX, closest = UINT32_MAX, visited = 0;
	FirstLineApproachPiece terminal_piece;
	while (!queue.empty() && visited < LEG_STATES) {
		auto [estimate, cost, key] = queue.top(); queue.pop();
		if (cost != costs[key]) continue;
		++visited;
		TileIndex tile{key / 16}; Trackdir prior = Trackdir(key % 16);
		if (!IsInnerTile(tile)) { reject("void"); continue; }
		closest = std::min(closest, DistanceManhattan(tile, target));
		if (PlanetManager::GetTileWorld(tile) != gate.world_id) { tile_reject("world", tile); continue; }
		if (blocked.contains(tile) || PortalRegistry::IsPortalTile(tile)) { tile_reject("blockers", tile); continue; }
		/* A second leg may share a previously quoted or already built owned bridge.
		 * Only a head entered in its exact longitudinal direction is traversable. */
		TileIndex other = INVALID_TILE;
		if (auto it = reserved_bridges.find(tile); it != reserved_bridges.end()) {
			if (tile == it->second.first) other = it->second.second;
			if (tile == it->second.second) other = it->second.first;
		} else if (IsBridgeTile(tile) && GetTileOwner(tile) == CompanyID{0} &&
			GetTunnelBridgeTransportType(tile) == TransportType::Rail && GetRailType(tile) == RAILTYPE_RAIL) {
			TileIndex end = GetOtherBridgeEnd(tile);
			if (end < Map::Size() && IsBridgeTile(end) && GetTileOwner(end) == CompanyID{0} &&
				GetTunnelBridgeTransportType(end) == TransportType::Rail && GetRailType(end) == RAILTYPE_RAIL &&
				GetOtherBridgeEnd(end) == tile && PlanetManager::GetTileWorld(end) == gate.world_id) other = end;
		}
		if (other != INVALID_TILE) {
			DiagDirection travel = TrackdirToExitdir(prior);
			if (DiagdirBetweenTiles(tile, other) == travel && PlanetManager::GetTileWorld(other) == gate.world_id) {
				TileIndex next = TileAddByDiagDir(other, travel);
				if (IsInnerTile(next)) {
					uint32_t nk = next.base() * 16 + uint(DiagDirToDiagTrackdir(travel));
					uint32_t nc = cost + DistanceManhattan(tile, other);
					if (!costs.contains(nk) || costs[nk] > nc) {
						if (!previous.contains(nk) && previous.size() + roots.size() >= LEG_STATES) {
							report["leg_cap_hit"] = true; continue;
						}
						if (!previous.contains(nk) && !budget.State()) { report["status"] = "budget_exhausted"; report["budget_exhausted"] = true; goto search_done; }
						costs[nk] = nc; previous[nk] = {key, {FirstLineApproachPiece::Kind::ReuseBridge, tile, Track::Invalid, other}};
						queue.emplace(nc + 3 * DistanceManhattan(next, target), nc, nk);
					}
				}
			}
			continue;
		}
		if (reserved_bridges.contains(tile) || IsBridgeTile(tile)) { tile_reject("bridge_span", tile); continue; }
		if (IsPlainRailTile(tile) && GetTileOwner(tile) != CompanyID{0}) { tile_reject("ownership", tile); continue; }
		if (!Ground(tile)) { tile_reject(IsTileType(tile, TileType::Water) ? "water" : "obstruction", tile); continue; }
		for (DiagDirection exit : {DiagDirection::NE, DiagDirection::SE, DiagDirection::SW, DiagDirection::NW}) {
			Track track = ApproachTrackCorner(ReverseDiagDir(TrackdirToExitdir(prior)), exit);
			if (!IsValidTrack(track)) continue;
			Trackdir next_dir = TrackExitdirToTrackdir(track, exit);
			if (!IsValidTrackdir(next_dir) || TrackdirCrossesTrackdirs(prior).Test(next_dir)) continue;
			if (!PreservesRoad(tile, track)) { tile_reject("road_demolition", tile); continue; }
			TrackBits future = planned_rails.contains(tile) ? planned_rails.at(tile) : TrackBits{};
			if (future.Any() && IsTileType(tile, TileType::Road) && !future.Test(track)) { reject("road"); continue; }
			if (!future.Test(track)) {
				if (!queried()) goto search_done;
				auto quote = QueryCoreBasketRail(tile, track, future);
				if (quote.Failed()) { Rejected(report, "rail", tile, quote); reject(IsTileType(tile, TileType::Road) ? "road" : GetTileSlope(tile) != SLOPE_FLAT ? "slope" : "native_rail"); continue; }
			}
			if (tile == target && exit == ReverseDiagDir(terminal->outward_dir)) {
				last = key; terminal_piece = {FirstLineApproachPiece::Kind::Rail, tile, track, INVALID_TILE}; break;
			}
			TileIndex next = TileAddByDiagDir(tile, exit);
			if (!IsInnerTile(next)) { reject("void"); continue; }
			uint32_t nk = next.base() * 16 + uint(next_dir), nc = cost + 1;
			if (costs.contains(nk) && costs[nk] <= nc) continue;
			if (!previous.contains(nk)) {
				if (previous.size() + roots.size() >= LEG_STATES) { report["leg_cap_hit"] = true; continue; }
				if (!budget.State()) { report["status"] = "budget_exhausted"; report["budget_exhausted"] = true; goto search_done; }
			}
			costs[nk] = nc; previous[nk] = {key, {FirstLineApproachPiece::Kind::Rail, tile, track, INVALID_TILE}};
			queue.emplace(nc + 3 * DistanceManhattan(next, target), nc, nk);
		}
		if (last != UINT32_MAX) break;
		/* Native bridge quote includes ordinary foundations and the full span. */
		DiagDirection travel = TrackdirToExitdir(prior);
		Track straight = DiagDirToDiagTrack(travel);
		Trackdir bridge_dir = DiagDirToDiagTrackdir(travel);
		TileIndex next_ground = TileAddByDiagDir(tile, travel);
		bool next_legal = false;
		bool road_before_water = false;
		if (IsInnerTile(next_ground) && Ground(next_ground)) {
			if (!queried()) goto search_done;
			auto native = QueryCoreBasketRail(next_ground, straight);
			next_legal = native.Succeeded();
			if (IsTileType(next_ground, TileType::Road)) {
				TileIndex after_road = TileAddByDiagDir(next_ground, travel);
				road_before_water = IsInnerTile(after_road) && IsTileType(after_road, TileType::Water);
			}
		}
		if (!ClearBridgeHead(tile) || planned_rails.contains(tile) ||
			(next_legal && !road_before_water) ||
			TrackdirCrossesTrackdirs(prior).Test(bridge_dir)) continue;
		TileIndex end = tile;
		for (uint span = 1; span <= 16; ++span) {
			end = TileAddByDiagDir(end, travel);
			if (!IsInnerTile(end)) { reject("void"); break; }
			if (PlanetManager::GetTileWorld(end) != gate.world_id) { reject("world"); break; }
			if (span < 2 || end == target || !ClearBridgeHead(end)) continue;
			bool conflict = false;
			for (TileIndex part = tile;; part = TileAddByDiagDir(part, travel)) {
				if (blocked.contains(part) || reserved_bridges.contains(part) || planned_rails.contains(part) ||
					PortalRegistry::IsPortalTile(part) || IsPlainRailTile(part) || IsTileType(part, TileType::Void)) conflict = true;
				if (part == end || conflict) break;
			}
			if (conflict) { reject("blockers"); continue; }
			if (!queried()) goto search_done;
			auto bridge = Command<Commands::BuildBridge>::Do({}, end, tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE);
			if (bridge.Failed()) { Rejected(report, "bridge", tile, bridge); reject("native_bridge"); continue; }
			TileIndex next = TileAddByDiagDir(end, travel);
			if (!IsInnerTile(next)) continue;
			uint32_t nk = next.base() * 16 + uint(bridge_dir), nc = cost + span + 12;
			if (costs.contains(nk) && costs[nk] <= nc) continue;
			if (!previous.contains(nk)) {
				if (previous.size() + roots.size() >= LEG_STATES) { report["leg_cap_hit"] = true; continue; }
				if (!budget.State()) { report["status"] = "budget_exhausted"; report["budget_exhausted"] = true; goto search_done; }
			}
			costs[nk] = nc; previous[nk] = {key, {FirstLineApproachPiece::Kind::Bridge, tile, Track::Invalid, end}};
			queue.emplace(nc + 3 * DistanceManhattan(next, target), nc, nk);
		}
	}
search_done:
	report["states"] = visited; report["shared_states"] = budget.states; report["shared_queries"] = budget.queries;
	report["closest_distance"] = closest == UINT32_MAX ? Json(nullptr) : Json(closest);
	report["queue_exhausted"] = queue.empty();
	report["leg_cap_hit"] = report["leg_cap_hit"].get<bool>() || visited >= LEG_STATES || previous.size() + roots.size() >= LEG_STATES;
	if (last == UINT32_MAX) {
		if (budget.exhausted) { report["status"] = "budget_exhausted"; report["budget_exhausted"] = true; }
		else if (report["leg_cap_hit"].get<bool>()) report["status"] = "leg_state_cap";
		return false;
	}
	std::vector<FirstLineApproachPiece> pieces{terminal_piece};
	uint32_t root = last;
	for (; !roots.contains(root);) {
		auto it = previous.find(root);
		if (it == previous.end()) { report["status"] = "route_reconstruction_failed"; return false; }
		pieces.push_back(it->second.piece); root = it->second.parent;
	}
	TileIndex start = roots.at(root);
	report["start"] = start.base();
	std::reverse(pieces.begin(), pieces.end());
	auto local_rails = planned_rails;
	auto local_bridges = reserved_bridges;
	int64_t total = 0;
	for (const auto &piece : pieces) {
		if (piece.kind == FirstLineApproachPiece::Kind::Rail) {
			if (local_bridges.contains(piece.tile)) { report["status"] = "rail_over_bridge_span"; return false; }
			if (!PreservesRoad(piece.tile, piece.track)) { report["status"] = "road_demolition"; return false; }
			TrackBits previous = local_rails.contains(piece.tile) ? local_rails.at(piece.tile) : TrackBits{};
			if (previous.Test(piece.track)) continue; // Exact prior paid quote, including a native road crossing.
			if (previous.Any() && IsTileType(piece.tile, TileType::Road)) { report["status"] = "future_road_conflict"; return false; }
			if (!queried()) { report["shared_queries"] = budget.queries; return false; }
			auto quote = QueryCoreBasketRail(piece.tile, piece.track, previous);
			if (quote.Failed()) { Rejected(report, "combined_rail", piece.tile, quote); report["status"] = "combined_quote_failed"; return false; }
			total += int64_t(quote.GetCost()); local_rails[piece.tile] |= TrackBits{piece.track};
		} else if (piece.kind == FirstLineApproachPiece::Kind::Bridge) {
			DiagDirection travel = TileX(piece.tile) == TileX(piece.end) ?
				(TileY(piece.tile) < TileY(piece.end) ? DiagDirection::SE : DiagDirection::NW) :
				(TileX(piece.tile) < TileX(piece.end) ? DiagDirection::SW : DiagDirection::NE);
			for (TileIndex part = piece.tile;; part = TileAddByDiagDir(part, travel)) {
				if (local_bridges.contains(part) || local_rails.contains(part)) { report["status"] = "combined_bridge_conflict"; return false; }
				local_bridges[part] = {piece.tile, piece.end};
				if (part == piece.end) break;
			}
			if (!queried()) { report["shared_queries"] = budget.queries; return false; }
			auto quote = Command<Commands::BuildBridge>::Do({}, piece.end, piece.tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE);
			if (quote.Failed()) { Rejected(report, "combined_bridge", piece.tile, quote); report["status"] = "combined_quote_failed"; return false; }
			total += int64_t(quote.GetCost());
		} else {
			const auto it = local_bridges.find(piece.tile);
			if (it == local_bridges.end() && !(IsBridgeTile(piece.tile) && GetTileOwner(piece.tile) == CompanyID{0} &&
				GetTunnelBridgeTransportType(piece.tile) == TransportType::Rail && GetRailType(piece.tile) == RAILTYPE_RAIL &&
				GetOtherBridgeEnd(piece.tile) == piece.end)) {
				report["status"] = "reused_bridge_changed"; return false;
			}
		}
	}
	planned_rails = std::move(local_rails); reserved_bridges = std::move(local_bridges);
	plan = {platform, start, target, axis, std::move(pieces), total};
	for (const auto &piece : plan.pieces) {
		const char *key = piece.kind == FirstLineApproachPiece::Kind::Rail ? "rail_pieces" :
			piece.kind == FirstLineApproachPiece::Kind::Bridge ? "bridge_pieces" : "reused_bridge_pieces";
		if (!report.contains(key)) report[key] = 0;
		report[key] = report[key].get<uint32_t>() + 1;
		if (report["route"].size() < 2048) report["route"].push_back({{"kind", key}, {"tile", piece.tile.base()},
			{"track", piece.kind == FirstLineApproachPiece::Kind::Rail ? Json(uint(piece.track)) : Json(nullptr)},
			{"end", piece.kind == FirstLineApproachPiece::Kind::Rail ? Json(nullptr) : Json(piece.end.base())}});
		else report["route_truncated"] = report["route_truncated"].get<uint32_t>() + 1;
	}
	report["status"] = "quoted"; report["cost"] = total;
	report["shared_queries"] = budget.queries;
	return true;
}

bool FirstLineExecuteApproach(const FirstLineApproachPlan &plan, Json &report)
{
	report = {{"status", "executing"}, {"quoted_cost", plan.cost}, {"actual_cost", 0}, {"pieces", plan.pieces.size()}};
	int64_t spent = 0;
	for (size_t i = 0; i < plan.pieces.size(); ++i) {
		const auto &piece = plan.pieces[i];
		if (piece.kind == FirstLineApproachPiece::Kind::ReuseBridge) {
			if (!IsBridgeTile(piece.tile) || GetTileOwner(piece.tile) != CompanyID{0} ||
				GetTunnelBridgeTransportType(piece.tile) != TransportType::Rail || GetRailType(piece.tile) != RAILTYPE_RAIL ||
				GetOtherBridgeEnd(piece.tile) != piece.end || piece.end >= Map::Size() || !IsBridgeTile(piece.end) || GetTileOwner(piece.end) != CompanyID{0} ||
				GetTunnelBridgeTransportType(piece.end) != TransportType::Rail || GetRailType(piece.end) != RAILTYPE_RAIL ||
				PlanetManager::GetTileWorld(piece.tile) != PlanetManager::GetTileWorld(piece.end)) {
				report["status"] = "reused_bridge_missing"; report["failed_piece"] = i; return false;
			}
			continue;
		}
		if (piece.kind == FirstLineApproachPiece::Kind::Bridge &&
			(!ClearBridgeHead(piece.tile) || !ClearBridgeHead(piece.end))) {
			report["status"] = "bridge_head_changed"; report["failed_piece"] = i; report["tile"] = piece.tile.base(); return false;
		}
		CommandCost quote = piece.kind == FirstLineApproachPiece::Kind::Rail ?
			QueryCoreBasketRail(piece.tile, piece.track) :
			Command<Commands::BuildBridge>::Do({}, piece.end, piece.tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE);
		if (quote.Failed()) { report["status"] = "quote_failed"; report["failed_piece"] = i; report["tile"] = piece.tile.base(); report["error"] = Error(quote); return false; }
		if (piece.kind == FirstLineApproachPiece::Kind::Rail && (IsPlainRailTile(piece.tile) || IsLevelCrossingTile(piece.tile)) &&
			GetTileOwner(piece.tile) == CompanyID{0} && GetRailType(piece.tile) == RAILTYPE_RAIL &&
			((IsPlainRailTile(piece.tile) && GetTrackBits(piece.tile).Test(piece.track)) ||
			 (IsLevelCrossingTile(piece.tile) && GetCrossingRailTrack(piece.tile) == piece.track))) continue;
		CommandCost done = piece.kind == FirstLineApproachPiece::Kind::Rail ?
			Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, piece.tile, RAILTYPE_RAIL, piece.track, false) :
			Command<Commands::BuildBridge>::Do(DoCommandFlag::Execute, piece.end, piece.tile, TransportType::Rail, 0, RAILTYPE_RAIL, INVALID_ROADTYPE);
		if (done.Failed()) { report["status"] = "execute_failed"; report["failed_piece"] = i; report["tile"] = piece.tile.base(); report["error"] = Error(done); report["cash_quote"] = int64_t(quote.GetCost()); return false; }
		spent += int64_t(done.GetCost());
		report["actual_cost"] = spent;
		if (done.GetCost() != quote.GetCost()) { report["status"] = "native_cost_changed"; report["failed_piece"] = i; report["cash_quote"] = int64_t(quote.GetCost()); report["cash_actual"] = int64_t(done.GetCost()); return false; }
	}
	report["actual_cost"] = spent;
	report["status"] = spent == plan.cost ? "executed" : "total_cost_changed";
	return spent == plan.cost;
}
