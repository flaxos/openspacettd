/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file lore_competitor.cpp Implementation of autonomous lore-driven AI competitors. */

#include "../stdafx.h"
#include "lore_competitor.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../town.h"
#include "../blueprint/blueprint.h"
#include "../blueprint/blueprint_manager.h"
#include "../blueprint/blueprint_cmd.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "corporate_alliance.h"
#include "federation_staging.h"
#include "megacity_manager.h"
#include "production_chain.h"
#include "tech_tree.h"
#include "../signal_func.h"

#include <mutex>
#include <algorithm>

#include "../safeguards.h"

namespace {

static std::mutex _lore_mutex;
static bool _lore_enabled = true;
static uint32_t _next_transit_id = 0;
static std::map<CompetitorType, LoreCompetitorProfile> _competitors;
static std::vector<CompetitorTransitRecord> _transits;

static Company *FindOrCreateCompetitorCompany(CompanyID preferred_id, const std::string &name, const std::string &president, Colours colour, Money capital)
{
	Company *c = Company::GetIfValid(preferred_id);
	if (c == nullptr) {
		if (Company::CanAllocateItem()) {
			c = Company::CreateAtIndex(preferred_id);
		}
	} else if (c->name != name && preferred_id == CompanyID{0}) {
		/* Slot 0 belongs to player, find next free slot */
		c = nullptr;
		for (CompanyID id = CompanyID{1}; id < MAX_COMPANIES; ++id) {
			if (Company::GetIfValid(id) == nullptr) {
				if (Company::CanAllocateItem()) {
					c = Company::CreateAtIndex(id);
					break;
				}
			}
		}
	}

	if (c != nullptr) {
		c->name = name;
		c->president_name = president;
		c->colour = colour;
		c->money = capital;
		c->is_ai = true;
		c->avail_railtypes.Set(RAILTYPE_BEGIN);
		c->avail_railtypes.Set(RAILTYPE_ELECTRIC);
		c->clear_limit = 1000 << 16;
	}
	return c;
}

static const char *GetPrefabForCompetitorTier(CompetitorType type, uint32_t tier)
{
	switch (type) {
		case CompetitorType::CST: {
			static const char *cst_prefabs[] = {
				"CST Mainline Double Straight",
				"CST Dual-Track Passing Siding",
				"CST Industrial Bulk Balloon Loop",
				"CST Depot Maintenance Staging Yard"
			};
			return cst_prefabs[tier % 4];
		}
		case CompetitorType::GrandCentral: {
			static const char *gc_prefabs[] = {
				"CST Mainline Double Straight",
				"CST Ro-Ro 4-Platform Terminal Station Block",
				"CST High-Speed 3-Way Wye Junction",
				"CST 4-Way Compact Roundabout Junction"
			};
			return gc_prefabs[tier % 4];
		}
		case CompetitorType::InterWorld: {
			static const char *iw_prefabs[] = {
				"CST Portal Gate Approach Corridor",
				"CST Dual-Track Passing Siding",
				"CST Industrial Bulk Balloon Loop",
				"CST Mainline Double Straight"
			};
			return iw_prefabs[tier % 4];
		}
		default:
			return "CST Mainline Double Straight";
	}
}

} // namespace

void LoreCompetitorManager::Reset()
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	_competitors.clear();
	_transits.clear();
	_next_transit_id = 0;
	_lore_enabled = true;
}

void LoreCompetitorManager::Initialize()
{
	SpawnCompetitors();
}

bool LoreCompetitorManager::IsEnabled()
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	return _lore_enabled;
}

void LoreCompetitorManager::SetEnabled(bool enabled)
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	_lore_enabled = enabled;
}

bool LoreCompetitorManager::SpawnCompetitors()
{
	std::lock_guard<std::mutex> lock(_lore_mutex);

	BlueprintManager::Initialize();

	/* Find home worlds according to world phases */
	WorldID cst_world = INVALID_WORLD;
	WorldID gc_world = INVALID_WORLD;
	WorldID iw_world = INVALID_WORLD;

	for (const auto &reg : PlanetManager::GetAllRegions()) {
		if (reg.phase == WorldPhase::Phase2_Developed && cst_world == INVALID_WORLD) cst_world = reg.id;
		else if (reg.phase == WorldPhase::Phase1_Core && gc_world == INVALID_WORLD) gc_world = reg.id;
		else if (reg.phase == WorldPhase::Phase3_Frontier && iw_world == INVALID_WORLD) iw_world = reg.id;
	}

	if (cst_world == INVALID_WORLD) cst_world = WorldID{1};
	if (gc_world == INVALID_WORLD) gc_world = WorldID{0};
	if (iw_world == INVALID_WORLD) iw_world = WorldID{2};

	/* 1. Commonwealth Synergy Transport (CST) */
	{
		Company *c = FindOrCreateCompetitorCompany(CompanyID{1}, "Commonwealth Synergy Transport", "Nigel Sheldon", Colours::DarkGreen, 50000000);
		if (c != nullptr) {
			LoreCompetitorProfile &p = _competitors[CompetitorType::CST];
			p.type = CompetitorType::CST;
			p.company_id = c->index;
			p.name = c->name;
			p.president_name = c->president_name;
			p.colour = c->colour;
			p.home_world = cst_world;
			p.active = true;

			/* Establish baseline neutral diplomatic stance with player */
			CorporateAllianceManager::SetRelation(CompanyID{0}, c->index, CorporateRelation::Neutral);
		}
	}

	/* 2. Grand Central Trans-Portal */
	{
		Company *c = FindOrCreateCompetitorCompany(CompanyID{2}, "Grand Central Trans-Portal", "Mellonie Gardner", Colours::DarkBlue, 50000000);
		if (c != nullptr) {
			LoreCompetitorProfile &p = _competitors[CompetitorType::GrandCentral];
			p.type = CompetitorType::GrandCentral;
			p.company_id = c->index;
			p.name = c->name;
			p.president_name = c->president_name;
			p.colour = c->colour;
			p.home_world = gc_world;
			p.active = true;

			CorporateAllianceManager::SetRelation(CompanyID{0}, c->index, CorporateRelation::Neutral);
		}
	}

	/* 3. InterWorld Logistics */
	{
		Company *c = FindOrCreateCompetitorCompany(CompanyID{3}, "InterWorld Logistics", "Bradley Johansson", Colours::Yellow, 35000000);
		if (c != nullptr) {
			LoreCompetitorProfile &p = _competitors[CompetitorType::InterWorld];
			p.type = CompetitorType::InterWorld;
			p.company_id = c->index;
			p.name = c->name;
			p.president_name = c->president_name;
			p.colour = c->colour;
			p.home_world = iw_world;
			p.active = true;

			CorporateAllianceManager::SetRelation(CompanyID{0}, c->index, CorporateRelation::Neutral);
		}
	}

	return !_competitors.empty();
}

const LoreCompetitorProfile *LoreCompetitorManager::GetProfile(CompetitorType type)
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	auto it = _competitors.find(type);
	if (it != _competitors.end()) return &it->second;
	return nullptr;
}

const LoreCompetitorProfile *LoreCompetitorManager::GetProfileByCompany(CompanyID company)
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	for (const auto &[type, profile] : _competitors) {
		if (profile.company_id == company) return &profile;
	}
	return nullptr;
}

std::vector<LoreCompetitorProfile> LoreCompetitorManager::GetAllCompetitors()
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	std::vector<LoreCompetitorProfile> res;
	res.reserve(_competitors.size());
	for (const auto &[type, profile] : _competitors) {
		res.push_back(profile);
	}
	return res;
}

bool LoreCompetitorManager::TriggerMilestoneExpansion(CompetitorType type, TileIndex origin_tile, const std::string &prefab_name)
{
	CompanyID comp_id = CompanyID::Invalid();
	{
		std::lock_guard<std::mutex> lock(_lore_mutex);
		auto it = _competitors.find(type);
		if (it == _competitors.end() || !it->second.active) return false;
		comp_id = it->second.company_id;
	}

	const Blueprint *bp = BlueprintManager::FindBuiltin(prefab_name);
	if (bp == nullptr) {
		BlueprintManager::Initialize();
		bp = BlueprintManager::FindBuiltin(prefab_name);
		if (bp == nullptr) return false;
	}

	UpdateSignalsInBuffer();
	CompanyID prev_company = _current_company;
	_current_company = comp_id;
	CommandCost res = Command<Commands::PlaceBlueprint>::Do(
		DoCommandFlag::Execute, origin_tile, bp->ToJson(), RAILTYPE_BEGIN, false
	);
	UpdateSignalsInBuffer();
	_current_company = prev_company;

	if (res.Succeeded()) {
		std::lock_guard<std::mutex> lock(_lore_mutex);
		LoreCompetitorProfile &p = _competitors[type];
		p.prefabs_placed++;
		p.placed_prefabs_history.push_back(prefab_name);
		p.expansion_anchors.push_back(origin_tile);
		return true;
	}

	return false;
}

bool LoreCompetitorManager::AutonomousExpand(CompetitorType type, WorldID world_id, TileIndex anchor_tile)
{
	TileIndex origin = anchor_tile;
	std::string prefab_name;

	{
		std::lock_guard<std::mutex> lock(_lore_mutex);
		auto it = _competitors.find(type);
		if (it == _competitors.end() || !it->second.active) return false;
		LoreCompetitorProfile &p = it->second;

		if (origin == INVALID_TILE) {
			if (!p.expansion_anchors.empty()) {
				TileIndex last = p.expansion_anchors.back();
				origin = TileXY(TileX(last) + 12, TileY(last));
			} else {
				const PlanetRegion *reg = PlanetManager::GetRegion(world_id != INVALID_WORLD ? world_id : p.home_world);
				if (reg != nullptr) {
					origin = TileXY((reg->min_x + reg->max_x) / 2, (reg->min_y + reg->max_y) / 2);
				} else {
					origin = TileXY(30, 30);
				}
			}
		}

		prefab_name = GetPrefabForCompetitorTier(type, p.milestone_tier);
	}

	bool success = TriggerMilestoneExpansion(type, origin, prefab_name);
	if (success) {
		std::lock_guard<std::mutex> lock(_lore_mutex);
		_competitors[type].milestone_tier++;
	}
	return success;
}

void LoreCompetitorManager::CheckMilestoneTriggers()
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	if (!_lore_enabled) return;

	uint32_t total_pop = 0;
	for (const Town *t : Town::Iterate()) {
		total_pop += t->cache.population;
	}

	size_t portal_count = PortalRegistry::Count();

	for (auto &[type, profile] : _competitors) {
		if (!profile.active) continue;

		uint32_t current_tier = profile.milestone_tier;
		uint32_t target_tier = current_tier;

		if (total_pop >= 2500 && target_tier == 0) target_tier = 1;
		if (total_pop >= 5000 && target_tier == 1) target_tier = 2;
		if (portal_count > 0 && target_tier < 2) target_tier = 2;
		if (total_pop >= 10000 && target_tier == 2) target_tier = 3;
		if (TechTreeManager::IsTechUnlocked(profile.company_id, TECH_PORTAL_1) && target_tier < 3) target_tier = 3;
		if (total_pop >= 25000 && target_tier == 3) target_tier = 4;

		if (target_tier > current_tier) {
			profile.milestone_tier = target_tier;
		}
	}
}

void LoreCompetitorManager::OnMonthlyTick()
{
	if (!IsEnabled()) return;

	/* 1. Release or complete holding transits whose remote conditions recovered */
	{
		std::vector<CompetitorTransitRecord> pending_transits;
		{
			std::lock_guard<std::mutex> lock(_lore_mutex);
			pending_transits = _transits;
		}

		for (auto &rec : pending_transits) {
			if (rec.is_holding) {
				if (!FederationStagingManager::IsServerHoldingCondition(rec.dest_world)) {
					rec.is_holding = false;
					/* Deliver cargo upon staging release */
					for (const auto &m : MegacityManager::GetAllMegacities()) {
						if (m.world_id == rec.dest_world || rec.dest_world == INVALID_WORLD) {
							if (IsValidCargoType(rec.cargo)) {
								MegacityDemandTier tier = MegacityManager::ClassifyCargo(to_underlying(rec.cargo));
								if (rec.competitor == CompetitorType::InterWorld && tier == MegacityDemandTier::Tier2_Expansion) {
									tier = MegacityDemandTier::Tier1_Sustenance;
								}
								MegacityManager::RecordDelivery(m.town_id, tier, rec.cargo_units);
							} else {
								MegacityDemandTier tier = MegacityDemandTier::Tier2_Expansion;
								if (rec.competitor == CompetitorType::GrandCentral) tier = MegacityDemandTier::Tier3_Prosperity;
								else if (rec.competitor == CompetitorType::InterWorld) tier = MegacityDemandTier::Tier1_Sustenance;
								MegacityManager::RecordDelivery(m.town_id, tier, rec.cargo_units);
							}
							break;
						}
					}
					uint64_t revenue = static_cast<uint64_t>(rec.cargo_units) * 250;
					std::lock_guard<std::mutex> lock(_lore_mutex);
					auto it = _competitors.find(rec.competitor);
					if (it != _competitors.end()) {
						it->second.total_revenue_earned += revenue;
						it->second.total_cargo_delivered += rec.cargo_units;
						it->second.active_trains++;
						Company *c = Company::GetIfValid(it->second.company_id);
						if (c != nullptr) c->money += revenue;
					}
				}
			}
		}

		std::lock_guard<std::mutex> lock(_lore_mutex);
		_transits = pending_transits;
	}

	/* 2. Check milestone triggers */
	CheckMilestoneTriggers();

	/* 3. Autonomous monthly scheduling of competitor consists */
	std::vector<CompetitorType> active_types;
	{
		std::lock_guard<std::mutex> lock(_lore_mutex);
		for (const auto &[type, profile] : _competitors) {
			if (profile.active) active_types.push_back(type);
		}
	}

	for (CompetitorType type : active_types) {
		LoreCompetitorProfile p;
		{
			std::lock_guard<std::mutex> lock(_lore_mutex);
			p = _competitors[type];
		}

		WorldID dest = WorldID{0}; // Default to Core world
		CargoType cargo{0};
		uint32_t amount = 50 + (p.milestone_tier * 15);
		FreightPriority priority = FreightPriority::Standard;

		if (type == CompetitorType::CST) {
			cargo = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
			priority = FreightPriority::Bulk;
		} else if (type == CompetitorType::GrandCentral) {
			cargo = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::EncryptedConsumerCrystals);
			priority = FreightPriority::Express;
		} else if (type == CompetitorType::InterWorld) {
			cargo = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
			dest = WorldID{1}; // Developed world
			priority = FreightPriority::Bulk;
		}

		ScheduleGatewayTransit(type, p.home_world, dest, cargo, amount, priority);
	}
}

bool LoreCompetitorManager::ScheduleGatewayTransit(CompetitorType type, WorldID origin_world, WorldID dest_world, CargoType cargo, uint32_t amount, FreightPriority priority)
{
	CompanyID comp_id = CompanyID::Invalid();
	{
		std::lock_guard<std::mutex> lock(_lore_mutex);
		auto it = _competitors.find(type);
		if (it == _competitors.end() || !it->second.active) return false;
		comp_id = it->second.company_id;
	}

	bool is_holding = FederationStagingManager::IsServerHoldingCondition(dest_world);

	CompetitorTransitRecord rec;
	{
		std::lock_guard<std::mutex> lock(_lore_mutex);
		rec.transit_id = ++_next_transit_id;
		rec.competitor = type;
		rec.origin_world = origin_world;
		rec.dest_world = dest_world;
		rec.cargo = cargo;
		rec.cargo_units = amount;
		rec.priority = priority;
		rec.is_holding = is_holding;
		rec.dispatch_tick = 0;
		_transits.push_back(rec);
	}

	if (is_holding) {
		return true; // Diverted to staging holding
	}

	/* Deliver directly to destination Megacity */
	for (const auto &m : MegacityManager::GetAllMegacities()) {
		if (m.world_id == dest_world || dest_world == INVALID_WORLD) {
			if (IsValidCargoType(cargo)) {
				MegacityDemandTier tier = MegacityManager::ClassifyCargo(to_underlying(cargo));
				if (type == CompetitorType::InterWorld && tier == MegacityDemandTier::Tier2_Expansion) {
					tier = MegacityDemandTier::Tier1_Sustenance;
				}
				MegacityManager::RecordDelivery(m.town_id, tier, amount);
			} else {
				MegacityDemandTier tier = MegacityDemandTier::Tier2_Expansion;
				if (type == CompetitorType::GrandCentral) tier = MegacityDemandTier::Tier3_Prosperity;
				else if (type == CompetitorType::InterWorld) tier = MegacityDemandTier::Tier1_Sustenance;
				MegacityManager::RecordDelivery(m.town_id, tier, amount);
			}
			break;
		}
	}

	uint64_t unit_rate = (priority == FreightPriority::Express || priority == FreightPriority::PriorityUrgent) ? 400 : 250;
	uint64_t revenue = static_cast<uint64_t>(amount) * unit_rate;

	{
		std::lock_guard<std::mutex> lock(_lore_mutex);
		LoreCompetitorProfile &p = _competitors[type];
		p.total_revenue_earned += revenue;
		p.total_cargo_delivered += amount;
		p.active_trains++;
	}

	Company *c = Company::GetIfValid(comp_id);
	if (c != nullptr) {
		c->money += revenue;
	}

	return true;
}

std::vector<CompetitorTransitRecord> LoreCompetitorManager::GetActiveTransits()
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	return _transits;
}

void LoreCompetitorManager::RestoreCompetitor(const LoreCompetitorProfile &profile)
{
	std::lock_guard<std::mutex> lock(_lore_mutex);
	_competitors[profile.type] = profile;
}
