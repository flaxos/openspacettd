/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file lore_competitor.h Autonomous lore-driven AI competitors management. */

#ifndef LORE_COMPETITOR_H
#define LORE_COMPETITOR_H

#include "../company_type.h"
#include "../tile_type.h"
#include "../cargo_type.h"
#include "portal_type.h"
#include "universe_authority.h"

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>

/**
 * Canonical lore competitor archetypes from the Commonwealth universe.
 */
enum class CompetitorType : uint8_t {
	CST          = 0, ///< Commonwealth Synergy Transport (Nigel Sheldon, Phase 2 Developed worlds, heavy bulk freight).
	GrandCentral = 1, ///< Grand Central Trans-Portal (Mellonie Gardner, Phase 1 Core worlds, high-frequency commuter & crystals).
	InterWorld   = 2, ///< InterWorld Logistics (Bradley Johansson, Phase 3 Frontier worlds, rugged mining & edge conduits).
	End          = 3,
};

/**
 * Milestone conditions monitored by AI competitors for dynamic expansion.
 */
enum class CompetitorMilestone : uint8_t {
	None            = 0,
	Population_2500 = 1,
	Population_5000 = 2,
	Population_10k  = 3,
	Population_25k  = 4,
	GatewayLinked   = 5,
	TechTier1       = 6,
	TechTier2       = 7,
};

/**
 * Record of a scheduled or active gateway transit consist by a competitor.
 */
struct CompetitorTransitRecord {
	uint32_t transit_id = 0;
	CompetitorType competitor = CompetitorType::CST;
	WorldID origin_world = INVALID_WORLD;
	WorldID dest_world = INVALID_WORLD;
	CargoType cargo = CargoType{0};
	uint32_t cargo_units = 0;
	FreightPriority priority = FreightPriority::Standard;
	bool is_holding = false;
	uint64_t dispatch_tick = 0;
};

/**
 * Profile tracking one canonical lore competitor empire.
 */
struct LoreCompetitorProfile {
	CompetitorType type = CompetitorType::CST;
	CompanyID company_id = CompanyID::Invalid();
	std::string name;
	std::string president_name;
	Colours colour = Colours::DarkGreen;
	WorldID home_world = INVALID_WORLD;
	uint32_t milestone_tier = 0;
	uint32_t prefabs_placed = 0;
	uint32_t active_trains = 0;
	uint64_t total_cargo_delivered = 0;
	uint64_t total_revenue_earned = 0;
	bool active = false;
	std::vector<std::string> placed_prefabs_history{};
	std::vector<TileIndex> expansion_anchors{};

	bool IsValid() const { return this->company_id != CompanyID::Invalid() && this->active; }
};

/**
 * Manager for lore-driven competitor empires, deterministic CST prefab expansion,
 * gateway transit dispatching, and Megacity supply competition.
 */
class LoreCompetitorManager {
public:
	static void Reset();
	static void Initialize();

	static bool IsEnabled();
	static void SetEnabled(bool enabled);

	/** Spawn the 3 canonical lore competitors in the company pool. */
	static bool SpawnCompetitors();

	/** Retrieve competitor profile by type. */
	static const LoreCompetitorProfile *GetProfile(CompetitorType type);

	/** Retrieve competitor profile by company ID. */
	static const LoreCompetitorProfile *GetProfileByCompany(CompanyID company);

	/** Retrieve all competitor profiles. */
	static std::vector<LoreCompetitorProfile> GetAllCompetitors();

	/**
	 * Server-authoritatively stamp a specific CST prefab on behalf of a competitor.
	 * Executes via Commands::PlaceBlueprint under the competitor's company identity.
	 */
	static bool TriggerMilestoneExpansion(CompetitorType type, TileIndex origin_tile, const std::string &prefab_name);

	/**
	 * Select and stamp the archetype-appropriate CST prefab for the competitor's current tier.
	 */
	static bool AutonomousExpand(CompetitorType type, WorldID world_id, TileIndex anchor_tile);

	/** Check world populations, portal links, and tech unlocks, advancing milestones. */
	static void CheckMilestoneTriggers();

	/** Monthly simulation handler for expansion, gateway bidding, and Megacity deliveries. */
	static void OnMonthlyTick();

	/**
	 * Schedule a competitor freight or express consist across portal wormholes.
	 * If corridor is saturated or holding, diverts to staging siding / holding state.
	 * If clear, delivers commodities to destination Megacity and credits quotas & revenue.
	 */
	static bool ScheduleGatewayTransit(CompetitorType type, WorldID origin_world, WorldID dest_world, CargoType cargo, uint32_t amount, FreightPriority priority = FreightPriority::Standard);

	/** Get all active or held transit consists. */
	static std::vector<CompetitorTransitRecord> GetActiveTransits();

	/** Restore competitor profile during savegame loading. */
	static void RestoreCompetitor(const LoreCompetitorProfile &profile);
};

#endif /* LORE_COMPETITOR_H */
