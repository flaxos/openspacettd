/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License, version 2.
 */

/** @file resource_sites.h Persistent resource sites and company area surveys. */
#ifndef RESOURCE_SITES_H
#define RESOURCE_SITES_H
#include "../command_type.h"
#include "../industry_type.h"
#include "../industrytype.h"
#include "planet_type.h"
#include "tech_tree.h"
#include <map>
#include <set>
#include <span>
#include <string_view>
#include <vector>

/** Stable content identity and placement bounds of an undepleted resource site. */
struct ResourceSite {
	uint32_t id = 0;
	TileIndex anchor{};
	uint16_t width = 0;
	uint16_t height = 0;
	WorldID world = INVALID_WORLD;
	GrfID grfid{};
	uint16_t local_id = 0;
	IndustryID occupant = IndustryID::Invalid();
};

/** All survey squares are persisted; newly researched resources reveal automatically. */
class ResourceSiteManager {
public:
	static constexpr uint SURVEY_SIZE = 16;
	static void Reset();
	static bool Enabled();
	static void SetEnabled(bool enabled);
	static bool IsPrimary(IndustryType type);
	static TechID RequiredTech(IndustryType type);
	static bool CanDiscover(CompanyID company, IndustryType type);
	static IndustryType ResolveType(const ResourceSite &site);
	static const std::vector<ResourceSite> &Sites();
	static const std::map<CompanyID, std::set<TileIndex>> &Surveys();
	static bool Surveyed(CompanyID company, TileIndex tile);
	static bool Discovered(CompanyID company, const ResourceSite &site);
	static CommandCost Survey(DoCommandFlags flags, CompanyID company, TileIndex tile);
	static Money SurveyPrice();
	static uint32_t AddSite(TileIndex anchor, IndustryType type, uint width, uint height, IndustryID occupant = IndustryID::Invalid());
	static void RestoreSite(const ResourceSite &site);
	static void RestoreSurvey(CompanyID company, TileIndex anchor);
	static CommandCost CheckPlacement(CompanyID company, TileIndex tile, IndustryType type, const IndustryTileLayout &layout, bool require_survey);
	static void Occupy(TileIndex tile, IndustryType type, const IndustryTileLayout &layout, IndustryID industry);
	static void Release(IndustryID industry);
	static void ChangeCompany(CompanyID old_company, CompanyID new_company);
	static void ValidateAfterLoad();
};

CommandCost CmdSurveyResources(DoCommandFlags flags, TileIndex tile);
DEF_CMD_TRAIT(Commands::SurveyResources, CmdSurveyResources, {}, CommandType::OtherManagement)

/** Probe native placement without building, then generate hidden sites. */
void GenerateResourceSites(uint target);
void ShowResourceSurveyWindow();
/** Operator diagnostics; contains no undiscovered site positions. */
bool ConResourceSites(std::span<std::string_view> argv);
/** Local UI overlay: 0 hidden, 1 free, 2 occupied. */
uint8_t GetResourceOverlay(TileIndex tile);
#endif
