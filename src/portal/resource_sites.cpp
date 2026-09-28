/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License, version 2.
 */

/** @file resource_sites.cpp Deterministic resource-site placement and company surveying. */
#include "../stdafx.h"
#include "resource_sites.h"
#include "stellar_network.h"
#include "planet_manager.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../industry.h"
#include "../map_func.h"
#include "../cargotype.h"
#include "../economy_base.h"
#include "../window_func.h"
#include "../gfx_func.h"
#include "../console_func.h"
#include "../3rdparty/nlohmann/json.hpp"
#include "../table/strings.h"
#include <algorithm>
#include "../safeguards.h"

static bool _resource_economy = false;
static bool _generating_sites = false;
bool ResourceSiteManager::GeneratingSites() { return _generating_sites; }
void ResourceSiteManager::SetGeneratingSites(bool value) { _generating_sites = value; }
static std::vector<ResourceSite> _resource_sites;
static std::map<CompanyID, std::set<TileIndex>> _resource_surveys;

void ResourceSiteManager::Reset() { _resource_economy = false; _resource_sites.clear(); _resource_surveys.clear(); }
bool ResourceSiteManager::Enabled() { return _resource_economy; }
void ResourceSiteManager::SetEnabled(bool enabled) { _resource_economy = enabled; }
const std::vector<ResourceSite> &ResourceSiteManager::Sites() { return _resource_sites; }
const std::map<CompanyID, std::set<TileIndex>> &ResourceSiteManager::Surveys() { return _resource_surveys; }

bool ResourceSiteManager::IsPrimary(IndustryType type)
{
	if (type >= NUM_INDUSTRYTYPES) return false;
	const auto *spec = GetIndustrySpec(type);
	return spec->IsRawIndustry() || spec->behaviour.Test(IndustryBehaviour::CutTrees);
}
TechID ResourceSiteManager::RequiredTech(IndustryType type)
{
	if (type >= NUM_INDUSTRYTYPES) return TECH_NONE;
	const auto *spec = GetIndustrySpec(type);
	if (!IsPrimary(type)) return TECH_NONE;
	TechID required = TECH_NONE;
	for (CargoType cargo : spec->produced_cargo) {
		if (!IsValidCargoType(cargo)) continue;
		CargoLabel label = CargoSpec::Get(cargo)->label;
		if (label == CargoLabel{"RARE"}) return TECH_MATERIALS_3;
		if (label == CargoLabel{"COPR"} || label == CargoLabel{"SAND"}) required = TECH_MATERIALS_2;
	}
	return required;
}
bool ResourceSiteManager::CanDiscover(CompanyID company, IndustryType type)
{
	TechID tech = RequiredTech(type);
	return tech == TECH_NONE || TechTreeManager::IsTechUnlocked(company, tech);
}
IndustryType ResourceSiteManager::ResolveType(const ResourceSite &site)
{
	for (IndustryType type = 0; type < NUM_INDUSTRYTYPES; ++type) {
		const auto *spec = GetIndustrySpec(type);
		if (!spec->enabled || !IsPrimary(type)) continue;
		if (spec->grf_prop.grfid == site.grfid && (spec->grf_prop.HasGrfFile() ? spec->grf_prop.local_id : type) == site.local_id) return type;
	}
	return IT_INVALID;
}
static bool Contains(const ResourceSite &site, TileIndex tile)
{
	return TileX(tile) >= TileX(site.anchor) && TileY(tile) >= TileY(site.anchor) &&
		TileX(tile) < TileX(site.anchor) + site.width && TileY(tile) < TileY(site.anchor) + site.height;
}
static bool InSurvey(TileIndex anchor, TileIndex tile)
{
	return TileX(tile) >= TileX(anchor) && TileY(tile) >= TileY(anchor) &&
		TileX(tile) < TileX(anchor) + ResourceSiteManager::SURVEY_SIZE && TileY(tile) < TileY(anchor) + ResourceSiteManager::SURVEY_SIZE;
}
bool ResourceSiteManager::Surveyed(CompanyID company, TileIndex tile)
{
	auto it = _resource_surveys.find(company);
	if (it == _resource_surveys.end()) return false;
	return std::any_of(it->second.begin(), it->second.end(), [tile](TileIndex anchor) { return InSurvey(anchor, tile); });
}
bool ResourceSiteManager::Discovered(CompanyID company, const ResourceSite &site)
{
	IndustryType type = ResolveType(site);
	return type != IT_INVALID && CanDiscover(company, type) && Surveyed(company, site.anchor);
}
Money ResourceSiteManager::SurveyPrice() { return std::max<Money>(1, (_price[Price::BuildIndustryRaw] + 99) / 100); }
CommandCost ResourceSiteManager::Survey(DoCommandFlags flags, CompanyID company, TileIndex tile)
{
	if (!Enabled() || !Company::IsValidID(company) || tile >= Map::Size()) return CMD_ERROR;
	WorldID world = PlanetManager::GetTileWorld(tile);
	if (!StellarNetwork::WorldAccessible(world)) return CommandCost(STR_ERROR_STELLAR_CLOSED);
	bool covered = true;
	for (uint y = TileY(tile); y < std::min(Map::SizeY(), TileY(tile) + SURVEY_SIZE); ++y) {
		for (uint x = TileX(tile); x < std::min(Map::SizeX(), TileX(tile) + SURVEY_SIZE); ++x) {
			TileIndex t = TileXY(x, y);
			if (IsTileType(t, TileType::Void) || (PlanetManager::Count() != 0 && (world == INVALID_WORLD || PlanetManager::GetTileWorld(t) != world))) return CommandCost(STR_RESOURCE_SURVEY_BOUNDARY);
			covered &= Surveyed(company, t);
		}
	}
	if (flags.Test(DoCommandFlag::Execute) && !covered) {
		_resource_surveys[company].insert(tile);
		InvalidateWindowClassesData(WindowClass::ResourceSurvey);
		MarkWholeScreenDirty();
	}
	return CommandCost(ExpensesType::Other, covered ? Money{0} : SurveyPrice());
}
CommandCost CmdSurveyResources(DoCommandFlags flags, TileIndex tile) { return ResourceSiteManager::Survey(flags, _current_company, tile); }

uint32_t ResourceSiteManager::AddSite(TileIndex anchor, IndustryType type, uint width, uint height, IndustryID occupant)
{
	if (!IsPrimary(type) || anchor >= Map::Size() || width == 0 || height == 0 || TileX(anchor) + width > Map::SizeX() || TileY(anchor) + height > Map::SizeY()) return 0;
	WorldID world = PlanetManager::GetTileWorld(anchor);
	for (uint y = 0; y < height; ++y) for (uint x = 0; x < width; ++x) {
		TileIndex tile = TileXY(TileX(anchor) + x, TileY(anchor) + y);
		if (IsTileType(tile, TileType::Void) || (PlanetManager::Count() && (world == INVALID_WORLD || PlanetManager::GetTileWorld(tile) != world))) return 0;
	}
	for (const auto &site : _resource_sites) {
		if (TileX(anchor) < TileX(site.anchor) + site.width && TileX(anchor) + width > TileX(site.anchor) &&
						TileY(anchor) < TileY(site.anchor) + site.height && TileY(anchor) + height > TileY(site.anchor)) return 0;
	}
	const auto *spec = GetIndustrySpec(type);
	uint32_t id = _resource_sites.empty() ? 1 : _resource_sites.back().id + 1;
	_resource_sites.push_back({id, anchor, static_cast<uint16_t>(width), static_cast<uint16_t>(height), world,
		spec->grf_prop.grfid, static_cast<uint16_t>(spec->grf_prop.HasGrfFile() ? spec->grf_prop.local_id : type), occupant});
	return id;
}
void ResourceSiteManager::RestoreSite(const ResourceSite &site) { _resource_sites.push_back(site); }
void ResourceSiteManager::RestoreSurvey(CompanyID company, TileIndex anchor) { _resource_surveys[company].insert(anchor); }
CommandCost ResourceSiteManager::CheckPlacement(CompanyID company, TileIndex tile, IndustryType type, const IndustryTileLayout &layout, bool require_survey)
{
	if (!Enabled()) return CommandCost();
	WorldID world = PlanetManager::GetTileWorld(tile);
	for (const auto &entry : layout) {
		int x = static_cast<int>(TileX(tile)) + entry.ti.x;
		int y = static_cast<int>(TileY(tile)) + entry.ti.y;
		if (x < 0 || y < 0 || x >= static_cast<int>(Map::SizeX()) || y >= static_cast<int>(Map::SizeY())) return CommandCost(STR_RESOURCE_SURVEY_BOUNDARY);
		TileIndex t = TileXY(x, y);
		auto policy = PlanetManager::Count() == 0 ? CommandCost() : PlanetManager::CheckConstructionPlacement(t);
		if (policy.Failed()) return policy;
		if (IsTileType(t, TileType::Void) || PlanetManager::GetTileWorld(t) != world) return CommandCost(STR_RESOURCE_SURVEY_BOUNDARY);
	}
	if (!IsPrimary(type) || !require_survey) return CommandCost();
	if (!CanDiscover(company, type)) return CommandCost(STR_RESOURCE_RESEARCH_REQUIRED);
	for (const auto &site : _resource_sites) {
		if (ResolveType(site) != type || !Contains(site, tile)) continue;
		if (!Discovered(company, site)) return CommandCost(STR_RESOURCE_NOT_SURVEYED);
		if (Industry::IsValidID(site.occupant)) return CommandCost(STR_RESOURCE_OCCUPIED);
		bool fits = true;
		for (const auto &entry : layout) {
			if (entry.gfx == GFX_WATERTILE_SPECIALCHECK) continue;
			fits &= Contains(site, TileXY(TileX(tile) + entry.ti.x, TileY(tile) + entry.ti.y));
		}
		if (fits) return CommandCost();
	}
	return CommandCost(STR_RESOURCE_SITE_REQUIRED);
}
void ResourceSiteManager::Occupy(TileIndex tile, IndustryType type, const IndustryTileLayout &layout, IndustryID industry)
{
	if (!Enabled() || !IsPrimary(type)) return;
	for (auto &site : _resource_sites) {
		if (ResolveType(site) == type && Contains(site, tile) && !Industry::IsValidID(site.occupant)) { site.occupant = industry; return; }
	}
	uint width = 1, height = 1;
	for (const auto &entry : layout) {
		if (entry.gfx == GFX_WATERTILE_SPECIALCHECK) continue;
		width = std::max(width, static_cast<uint>(entry.ti.x + 1));
		height = std::max(height, static_cast<uint>(entry.ti.y + 1));
	}
	AddSite(tile, type, width, height, industry);
}
void ResourceSiteManager::Release(IndustryID industry)
{
	for (auto &site : _resource_sites) if (site.occupant == industry) site.occupant = IndustryID::Invalid();
}
void ResourceSiteManager::ChangeCompany(CompanyID old_company, CompanyID new_company)
{
	auto it = _resource_surveys.find(old_company);
	if (it == _resource_surveys.end() || old_company == new_company) return;
	if (Company::IsValidID(new_company)) _resource_surveys[new_company].insert(it->second.begin(), it->second.end());
	_resource_surveys.erase(it);
}
void ResourceSiteManager::ValidateAfterLoad()
{
	std::erase_if(_resource_sites, [](ResourceSite &site) {
		if (site.anchor >= Map::Size() || site.width == 0 || site.height == 0 || TileX(site.anchor) + site.width > Map::SizeX() || TileY(site.anchor) + site.height > Map::SizeY() || ResolveType(site) == IT_INVALID || PlanetManager::GetTileWorld(site.anchor) != site.world) return true;
		for (uint y = TileY(site.anchor); y < TileY(site.anchor) + site.height; ++y) {
			for (uint x = TileX(site.anchor); x < TileX(site.anchor) + site.width; ++x) {
				TileIndex tile = TileXY(x, y);
				if (IsTileType(tile, TileType::Void) || PlanetManager::GetTileWorld(tile) != site.world) return true;
			}
		}
		const Industry *industry = Industry::GetIfValid(site.occupant);
		if (industry == nullptr || industry->type != ResolveType(site) || !Contains(site, industry->location.tile)) site.occupant = IndustryID::Invalid();
		return false;
	});
	std::erase_if(_resource_surveys, [](const auto &entry) { return !Company::IsValidID(entry.first); });
	for (auto &[company, squares] : _resource_surveys) std::erase_if(squares, [](TileIndex anchor) { return anchor >= Map::Size(); });
	std::sort(_resource_sites.begin(), _resource_sites.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
}

/** Read-only diagnostics used by reproducible fresh-process acceptance. */
bool ConResourceSites(std::span<std::string_view> argv)
{
	if (argv.size() != 1) { IConsolePrint(CC_HELP, "resource_sites: show economy mode and resource counts"); return true; }
	nlohmann::json result{{"player_built", ResourceSiteManager::Enabled()}, {"sites", ResourceSiteManager::Sites().size()},
		{"industries", Industry::GetNumItems()}, {"unoccupied_sites", 0}, {"processing", 0}, {"advanced", 0}, {"survey_companies", ResourceSiteManager::Surveys().size()}};
	for (const auto &site : ResourceSiteManager::Sites()) {
		if (!Industry::IsValidID(site.occupant)) result["unoccupied_sites"] = result["unoccupied_sites"].get<uint>() + 1;
	}
	for (const Industry *industry : Industry::Iterate()) {
		if (!ResourceSiteManager::IsPrimary(industry->type)) result["processing"] = result["processing"].get<uint>() + 1;
		if (ResourceSiteManager::RequiredTech(industry->type) != TECH_NONE) result["advanced"] = result["advanced"].get<uint>() + 1;
	}
	if (StellarNetwork::Enabled()) {
  auto stellar=nlohmann::json::parse(StellarNetwork::Save());
  result["site_worlds"]=nlohmann::json::object();
  for (const auto &site : ResourceSiteManager::Sites()) { auto key=fmt::format("{}",site.world.base()); result["site_worlds"][key]=result["site_worlds"].value(key,0u)+1; }
  result["stellar_worlds"]=stellar.at("worlds"); result["landing_zones"]=stellar.at("zones"); result["gate_policies"]=stellar.at("policies");
  uint32_t bad=0;
  for (uint32_t y=0;y<Map::SizeY();++y) for (uint32_t x=0;x<Map::SizeX();++x) {
   int h=TileHeight(TileXY(x,y));
   if (x+1<Map::SizeX() && std::abs(h-int(TileHeight(TileXY(x+1,y))))>1) ++bad;
   if (y+1<Map::SizeY() && std::abs(h-int(TileHeight(TileXY(x,y+1))))>1) ++bad;
  }
  result["invalid_height_edges"]=bad;
  result["machine_cargo"]=GetCargoTypeByLabel(CargoLabel{"MACH"});
 }
	IConsolePrint(CC_DEFAULT, "RESOURCE state {}", result.dump());
	return true;
}
