/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_line_funding.cpp Ordinary native funding for the authored opening. */
#include "../stdafx.h"
#include "first_line_funding.h"
#include "first_permanent_line.h"
#include "planet_manager.h"
#include "resource_sites.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../genworld.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../string_func.h"
#include "../strings_func.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace {
using Json = nlohmann::json;
constexpr size_t MAX_SAMPLES = 16;
constexpr size_t MAX_REJECTION_KINDS = 64;
constexpr size_t MAX_SITE_GROUPS = 32;

struct Candidate {
	uint distance;
	TileIndex tile;
	uint32_t site_id = 0;
	TileIndex anchor = INVALID_TILE;
	size_t site_index = SIZE_MAX;
};

Json Error(CommandCost &cost)
{
	const StringID id = cost.GetErrorMessage(), extra = cost.GetExtraErrorMessage();
	/* Exact IDs survive localization; decoded text makes the native refusal readable. */
	std::string decoded;
	if (!cost.GetEncodedMessage().empty()) decoded = cost.GetEncodedMessage().GetDecodedString();
	else decoded = id == INVALID_STRING_ID ? "native command rejected" : GetString(id);
	std::string extra_text = extra == INVALID_STRING_ID ? "" : GetString(extra);
	decoded.resize(std::min<size_t>(decoded.size(), 160));
	extra_text.resize(std::min<size_t>(extra_text.size(), 160));
	// A byte limit can split a localized UTF-8 character; keep diagnostic JSON valid.
	StrMakeValidInPlace(decoded);
	StrMakeValidInPlace(extra_text);
	return {{"error_id", id.base()}, {"extra_id", extra.base()}, {"decoded", decoded},
		{"extra", extra_text}, {"cost", int64_t(cost.GetCost())}};
}

Json CandidateJson(const Candidate &candidate)
{
	return {{"site_id", candidate.site_id},
		{"anchor", candidate.anchor == INVALID_TILE ? Json(nullptr) : Json(candidate.anchor.base())},
		{"tile", candidate.tile.base()}, {"x", TileX(candidate.tile)}, {"y", TileY(candidate.tile)},
		{"distance", candidate.distance}};
}
} // namespace

Industry *FirstLineFundIndustry(IndustryType type, WorldID world, TileIndex near, bool primary, Json &report)
{
	const Company *company = Company::GetIfValid(_current_company);
	const bool valid_type = type < NUM_INDUSTRYTYPES;
	const bool valid_near = near < Map::Size();
	const bool valid_world = PlanetManager::GetRegion(world) != nullptr;
	const bool near_in_world = valid_near && valid_world && PlanetManager::GetTileWorld(near) == world;
	report = {{"schema", 1}, {"status", "invalid_request"}, {"stage", "admission"}, {"primary", primary},
		{"company", _current_company.base()}, {"funds", company == nullptr ? Json(nullptr) : Json(int64_t(company->money))},
		{"type", type}, {"world", world.base()}, {"near", near.base()}, {"identity", nullptr},
		{"admission", {{"actor_valid", company != nullptr}, {"normal_mode", _game_mode == GameMode::Normal},
			{"prebaseline", !FirstPermanentLine::Active()},
			{"not_generating", !_generating_world && !ResourceSiteManager::GeneratingSites()},
			{"resource_mode", ResourceSiteManager::Enabled()}, {"type_valid", valid_type},
			{"world_valid", valid_world}, {"near_in_world", near_in_world}}},
		{"sites", {{"total", ResourceSiteManager::Sites().size()}, {"matching", 0}, {"occupied", 0}, {"unoccupied", 0}}},
		{"site_catalog", Json::array()}, {"site_catalog_overflow", 0},
		{"candidate_count", 0}, {"attempted_candidates", 0}, {"survey_quotes", 0}, {"survey_executes", 0},
		{"survey_quoted_cost", 0}, {"survey_cost", 0},
		{"build_request", {{"first_layout", 0}, {"fund", false}, {"seed", 1}}},
		{"build_quotes", 0}, {"build_executes", 0}, {"samples", Json::array()},
		{"rejections", Json::array()}, {"rejection_overflow", 0}, {"cost", nullptr}, {"chosen", nullptr}};
	if (company == nullptr || FirstPermanentLine::Active() || _game_mode != GameMode::Normal || _generating_world || ResourceSiteManager::GeneratingSites() ||
		!ResourceSiteManager::Enabled() || !valid_type || !near_in_world) return nullptr;
	const IndustrySpec *spec = GetIndustrySpec(type);
	report["identity"] = {{"grfid", spec->grf_prop.grfid.AsString()},
		{"local_id", spec->grf_prop.local_id}, {"layouts", spec->layouts.size()},
		{"enabled", spec->enabled}, {"native_primary", ResourceSiteManager::IsPrimary(type)}};
	if (!spec->enabled || spec->layouts.empty() || ResourceSiteManager::IsPrimary(type) != primary) return nullptr;

	std::vector<Candidate> candidates;
	const auto &sites = ResourceSiteManager::Sites();
	using SiteGroup = std::pair<uint32_t, uint32_t>;
	std::map<SiteGroup, std::pair<uint32_t, uint32_t>> catalog;
	for (size_t i = 0; i < sites.size(); ++i) {
		const ResourceSite &site = sites[i];
		const IndustryType effective = ResourceSiteManager::ResolveType(site);
		const SiteGroup group{site.world.base(), effective};
		auto existing = catalog.find(group);
		if (existing != catalog.end()) {
			++existing->second.first;
			if (site.occupant != IndustryID::Invalid()) ++existing->second.second;
		} else if (catalog.size() < MAX_SITE_GROUPS) {
			catalog.emplace(group, std::pair<uint32_t, uint32_t>{1, site.occupant != IndustryID::Invalid() ? 1u : 0u});
		} else {
			report["site_catalog_overflow"] = report["site_catalog_overflow"].get<uint32_t>() + 1;
		}
		if (site.world != world || effective != type) continue;
		report["sites"]["matching"] = report["sites"]["matching"].get<size_t>() + 1;
		if (site.occupant != IndustryID::Invalid()) {
			report["sites"]["occupied"] = report["sites"]["occupied"].get<size_t>() + 1;
			continue;
		}
		report["sites"]["unoccupied"] = report["sites"]["unoccupied"].get<size_t>() + 1;
		if (!primary || site.anchor >= Map::Size() || site.width == 0 || site.height == 0 ||
			TileX(site.anchor) + site.width > Map::SizeX() || TileY(site.anchor) + site.height > Map::SizeY()) continue;
		for (uint y = TileY(site.anchor); y < TileY(site.anchor) + site.height; ++y) {
			for (uint x = TileX(site.anchor); x < TileX(site.anchor) + site.width; ++x) {
				TileIndex tile = TileXY(x, y);
				candidates.push_back({DistanceManhattan(tile, near), tile, site.id, site.anchor, i});
			}
		}
	}
	for (const auto &[key, counts] : catalog) report["site_catalog"].push_back({
		{"world", key.first}, {"type", key.second}, {"count", counts.first}, {"occupied", counts.second}});
	if (!primary) {
		for (int dy = -24; dy <= 24; ++dy) for (int dx = -24; dx <= 24; ++dx) {
			const int x = int(TileX(near)) + dx, y = int(TileY(near)) + dy;
			if (x <= 1 || y <= 1 || x >= int(Map::MaxX()) || y >= int(Map::MaxY())) continue;
			TileIndex tile = TileXY(x, y);
			if (PlanetManager::GetTileWorld(tile) == world) candidates.push_back({uint(std::abs(dx) + std::abs(dy)), tile});
		}
	}
	std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
		return std::tuple{a.distance, a.tile.base(), a.site_id} < std::tuple{b.distance, b.tile.base(), b.site_id};
	});
	report["candidate_count"] = candidates.size();
	if (candidates.empty()) { report["status"] = "no_candidates"; report["stage"] = primary ? "resource_sites" : "processor_tiles"; return nullptr; }

	using RejectionKey = std::tuple<std::string, uint32_t, uint32_t>;
	std::map<RejectionKey, std::pair<uint32_t, Json>> rejections;
	auto finish = [&](const char *status, const char *stage) {
		report["status"] = status; report["stage"] = stage;
		for (const auto &[key, entry] : rejections) {
			Json item = entry.second;
			item["stage"] = std::get<0>(key);
			item["count"] = entry.first;
			report["rejections"].push_back(std::move(item));
		}
	};
	auto rejected = [&](const char *stage, CommandCost &cost, Json &sample) {
		Json error = Error(cost);
		const RejectionKey key{stage, error["error_id"].get<uint32_t>(), error["extra_id"].get<uint32_t>()};
		auto it = rejections.find(key);
		if (it != rejections.end()) ++it->second.first;
		else if (rejections.size() < MAX_REJECTION_KINDS) rejections.emplace(key, std::pair<uint32_t, Json>{1, error});
		else report["rejection_overflow"] = report["rejection_overflow"].get<uint32_t>() + 1;
		sample["stage"] = stage; sample["error"] = error;
		report["last_error"] = std::move(error);
	};

	std::set<TileIndex> attempted_tiles;
	std::map<size_t, bool> surveyed_sites;
	for (const Candidate &candidate : candidates) {
		Json sample = CandidateJson(candidate);
		const bool keep_sample = report["samples"].size() < MAX_SAMPLES;
		if (primary) {
			auto [it, first_site_visit] = surveyed_sites.emplace(candidate.site_index, false);
			if (first_site_visit) {
				const TileIndex anchor = sites[candidate.site_index].anchor;
				report["survey_quotes"] = report["survey_quotes"].get<uint32_t>() + 1;
				CommandCost quote = Command<Commands::SurveyResources>::Do({}, anchor);
				if (quote.Failed()) {
					rejected("survey_quote", quote, sample);
					if (keep_sample) report["samples"].push_back(sample);
					continue;
				}
				sample["survey_quote_cost"] = int64_t(quote.GetCost());
				report["survey_quoted_cost"] = report["survey_quoted_cost"].get<int64_t>() + int64_t(quote.GetCost());
				report["survey_executes"] = report["survey_executes"].get<uint32_t>() + 1;
				CommandCost paid = Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, anchor);
				if (paid.Failed()) {
					rejected("survey_execute", paid, sample);
					if (keep_sample) report["samples"].push_back(sample);
					finish("survey_execute_failed", "survey_execute");
					return nullptr;
				}
				sample["survey_execute_cost"] = int64_t(paid.GetCost());
				report["survey_cost"] = report["survey_cost"].get<int64_t>() + int64_t(paid.GetCost());
				it->second = true;
			}
			if (!it->second) continue;
		}
		if (!attempted_tiles.insert(candidate.tile).second) continue;
		report["attempted_candidates"] = report["attempted_candidates"].get<uint32_t>() + 1;
		report["build_quotes"] = report["build_quotes"].get<uint32_t>() + 1;
		/* CmdBuildIndustry itself tries every native layout from index zero. */
		CommandCost quote = Command<Commands::BuildIndustry>::Do({}, candidate.tile, type, 0, false, 1);
		if (quote.Failed()) {
			rejected("build_quote", quote, sample);
			if (keep_sample) report["samples"].push_back(sample);
			continue;
		}
		sample["stage"] = "build_quote_ok";
		sample["quote_cost"] = int64_t(quote.GetCost());
		if (keep_sample) report["samples"].push_back(sample);
		report["cost"] = int64_t(quote.GetCost());
		report["build_executes"] = report["build_executes"].get<uint32_t>() + 1;
		CommandCost paid = Command<Commands::BuildIndustry>::Do(DoCommandFlag::Execute, candidate.tile, type, 0, false, 1);
		if (paid.Failed()) {
			Json execution = CandidateJson(candidate);
			rejected("build_execute", paid, execution);
			report["execute_failure"] = execution;
			finish("build_execute_failed", "build_execute");
			return nullptr;
		}
		report["cost"] = int64_t(paid.GetCost());
		Industry *found = nullptr;
		for (Industry *industry : Industry::Iterate()) {
			if (industry->type == type && industry->location.tile == candidate.tile && industry->founder == _current_company) {
				found = industry;
				break;
			}
		}
		if (found == nullptr) { finish("success_without_industry", "native_verification"); return nullptr; }
		if (PlanetManager::GetTileWorld(found->location.tile) != world ||
			(primary && sites[candidate.site_index].occupant != found->index)) {
			report["chosen"] = CandidateJson(candidate);
			report["chosen"]["industry"] = found->index.base();
			report["chosen"]["site_occupant"] = primary ? sites[candidate.site_index].occupant.base() : IndustryID::Invalid().base();
			finish("custody_mismatch", "native_verification");
			return nullptr;
		}
		report["chosen"] = CandidateJson(candidate);
		report["chosen"]["industry"] = found->index.base();
		finish("success", "native_verification");
		return found;
	}
	if (primary && report["build_quotes"].get<uint32_t>() == 0) finish("survey_quotes_exhausted", "survey_quote");
	else finish("build_quotes_exhausted", "build_quote");
	return nullptr;
}
