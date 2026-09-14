/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file tech_tree.h In-lore Commonwealth Tech Tree manager and R&D progression. */

#ifndef TECH_TREE_H
#define TECH_TREE_H

#include "../company_type.h"
#include "planet_type.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

/** Commonwealth Research Branches. */
enum class TechBranch : uint8_t {
	Traction      = 0, ///< Traction & Propulsion (locomotives, vactrains, vacuum maglev)
	PortalPhysics = 1, ///< Wormhole & Portal Physics (throat arrays, bandwidth, twin-arrays)
	Materials     = 2, ///< Materials & Fabrication (metallurgy, electronics, quantum neural fabs)
	Count         = 3,
};

using TechID = uint16_t;

/** Canonical Tech IDs. */
static constexpr TechID TECH_NONE               = 0;

/* Branch 0: Traction & Propulsion */
static constexpr TechID TECH_TRACTION_1         = 101; ///< High-Adhesion Steam (Vulcan 2-8-0 Hauler)
static constexpr TechID TECH_TRACTION_2         = 102; ///< Multi-Unit Heavy Diesel (Titan D-100 Twin-Engine)
static constexpr TechID TECH_TRACTION_3         = 103; ///< High-Voltage Electrics (CST E-40 Inter-World)
static constexpr TechID TECH_TRACTION_4         = 104; ///< CST Vacuum Maglev & Vactrains (1,000 km/h)

/* Branch 1: Wormhole & Portal Physics */
static constexpr TechID TECH_PORTAL_1           = 201; ///< Stable Gateway Links (Single-Track Portal Gates)
static constexpr TechID TECH_PORTAL_2           = 202; ///< Dual-Track Throat Arrays (2x Bandwidth Portals)
static constexpr TechID TECH_PORTAL_3           = 203; ///< Freight Corridor Bridges (Inter-Server Bulk Priority)
static constexpr TechID TECH_PORTAL_4           = 204; ///< Twin-Array Wormholes (High-Speed Continuous Transit)

/* Branch 2: Materials & Fabrication */
static constexpr TechID TECH_MATERIALS_1        = 301; ///< Blast Furnace Structural Steel (Structural Rails)
static constexpr TechID TECH_MATERIALS_2        = 302; ///< Copper & Silicon Electronics Fabs (Catenary & Signals)
static constexpr TechID TECH_MATERIALS_3        = 303; ///< Superalloys & Lightweight Composites (-10% BOM cost)
static constexpr TechID TECH_MATERIALS_4        = 304; ///< Quantum Neural Fabs & Crystal Imprinting (Autonomous AI)

/** Research Project Node specification in the Commonwealth Tech Tree. */
struct TechProjectNode {
	TechID id = TECH_NONE;
	TechBranch branch = TechBranch::Traction;
	uint8_t tier = 1;
	std::string name;
	std::string description;
	uint32_t cost_rp = 0;              ///< Research Points (RP) required for completion
	std::vector<TechID> prerequisites;  ///< Tech IDs required before this project can be selected
	std::string unlock_summary;        ///< Short description of gameplay bonus / unlock
};

/** Company research state and progress. */
struct CompanyTechState {
	CompanyID company_id = CompanyID::Invalid();
	TechID active_project = TECH_NONE; ///< Currently active R&D focus (0 if none)
	uint32_t accumulated_rp = 0;      ///< RP accumulated toward active project
	uint32_t monthly_budget = 0;       ///< Monthly cash budget allocated to R&D (in credits)
	std::set<TechID> unlocked_techs;   ///< Set of completed technology IDs
};

/**
 * Global manager for the Commonwealth Tech Tree and R&D progression.
 */
class TechTreeManager {
public:
	/** Reset all company research states and reinitialize registry. */
	static void Reset();

	/** Retrieve a project node by its ID. */
	static const TechProjectNode *GetNode(TechID id);

	/** Retrieve all project nodes in the registry. */
	static const std::vector<TechProjectNode> &GetAllNodes();

	/** Retrieve all project nodes belonging to a specific branch. */
	static std::vector<TechProjectNode> GetNodesByBranch(TechBranch branch);

	/** Check if a company has unlocked a specific technology. */
	static bool IsTechUnlocked(CompanyID company, TechID id);

	/** Get a company's currently active research project ID. */
	static TechID GetActiveProject(CompanyID company);

	/** Get a company's accumulated research points on the active project. */
	static uint32_t GetAccumulatedRP(CompanyID company);

	/** Get a company's allocated monthly research budget. */
	static uint32_t GetMonthlyBudget(CompanyID company);

	/**
	 * Check if a company is eligible to begin research on a project.
	 * Requires an active Corporate HQ campus on a Phase 1 Core world, and all prerequisites unlocked.
	 */
	static bool CanResearch(CompanyID company, TechID id, std::string &err_msg);

	/** Set active research project for a company. */
	static bool SetActiveProject(CompanyID company, TechID id);

	/** Set monthly research budget for a company. */
	static bool SetMonthlyBudget(CompanyID company, uint32_t budget);

	/** Add research points directly to a company's active project. */
	static void AddResearchPoints(CompanyID company, uint32_t rp);

	/**
	 * Process monthly research progression across all companies.
	 * Converts cash budget to RP, consumes Enriched Quantum Data Crystals and High-Tech Electronics
	 * from the company's Corporate HQ world stockpile, advances active projects, and unlocks completed tech.
	 */
	static void ProcessMonthlyResearch();

	/** Retrieve all company research states for serialization and UI. */
	static std::vector<CompanyTechState> GetAllCompanyTechStates();

	/** Restore a company research state during savegame deserialization. */
	static void RestoreCompanyTech(CompanyID company, TechID active_project, uint32_t accumulated_rp, uint32_t monthly_budget, const std::vector<TechID> &unlocked);
};

#endif /* TECH_TREE_H */
