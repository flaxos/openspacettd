/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file tech_tree.cpp Implementation of Commonwealth Tech Tree and R&D progression. */

#include "../stdafx.h"
#include "tech_tree.h"
#include "corporate_hq.h"
#include "company_stockpile.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../core/format.hpp"

#include <algorithm>
#include <mutex>

#include "../safeguards.h"

static std::vector<TechProjectNode> _tech_nodes;
static std::map<CompanyID, CompanyTechState> _company_techs;
static std::mutex _tech_mutex;
static bool _nodes_initialized = false;

static void EnsureNodesInitialized()
{
	if (_nodes_initialized) return;

	_tech_nodes.clear();

	/* Branch 0: Traction & Propulsion */
	_tech_nodes.push_back({
		TECH_TRACTION_1,
		TechBranch::Traction,
		1,
		"High-Adhesion Steam",
		"Heavy-gauge steam traction utilizing high-friction planetary wheelsets and boiler superheaters.",
		100,
		{},
		"Unlocks Vulcan heavy-adhesion steam locomotive series."
	});
	_tech_nodes.push_back({
		TECH_TRACTION_2,
		TechBranch::Traction,
		2,
		"Multi-Unit Heavy Diesel",
		"Coupled diesel-electric traction units with synchronized governor throttles for high-tonnage freight.",
		250,
		{TECH_TRACTION_1},
		"Unlocks Titan heavy diesel locomotive sets and high-torque freight operations."
	});
	_tech_nodes.push_back({
		TECH_TRACTION_3,
		TechBranch::Traction,
		3,
		"High-Voltage Electrics",
		"High-capacity 50kV catenary infrastructure and solid-state IGBT power converters for trans-world consists.",
		600,
		{TECH_TRACTION_2},
		"Unlocks CST E-40 Inter-World high-speed electric locomotives and catenary efficiency."
	});
	_tech_nodes.push_back({
		TECH_TRACTION_4,
		TechBranch::Traction,
		4,
		"CST Vacuum Maglev & Vactrains",
		"Cryogenic superconductor levitation guideways enclosed in evacuated vacuum tubes exceeding 1,000 km/h.",
		1500,
		{TECH_TRACTION_3},
		"Unlocks 1,000 km/h CST vacuum-tube maglev trainsets and orbital transit guideways."
	});

	/* Branch 1: Wormhole & Portal Physics */
	_tech_nodes.push_back({
		TECH_PORTAL_1,
		TechBranch::PortalPhysics,
		1,
		"Stable Gateway Links",
		"Sheldon-Ozzie metric tensor coils establishing stable single-track artificial Einstein-Rosen bridges.",
		100,
		{},
		"Stabilizes single-track wormhole portal gateway links."
	});
	_tech_nodes.push_back({
		TECH_PORTAL_2,
		TechBranch::PortalPhysics,
		2,
		"Dual-Track Throat Arrays",
		"Harmonic resonance dampeners allowing parallel dual-track gateway throat arrays without field interference.",
		300,
		{TECH_PORTAL_1},
		"Enables dual-track portal throat arrays, doubling throughput."
	});
	_tech_nodes.push_back({
		TECH_PORTAL_3,
		TechBranch::PortalPhysics,
		3,
		"Freight Corridor Bridges",
		"Synchronized inter-server priority signaling for high-frequency bulk commodity transit.",
		750,
		{TECH_PORTAL_2},
		"Establishes inter-server priority corridors for bulk raw material transit."
	});
	_tech_nodes.push_back({
		TECH_PORTAL_4,
		TechBranch::PortalPhysics,
		4,
		"Twin-Array Wormholes",
		"Monumental twin-array macroscopic wormholes supporting continuous, high-speed multi-track transit.",
		2000,
		{TECH_PORTAL_3},
		"Unlocks continuous twin-array wormholes for high-frequency trans-galactic corridors."
	});

	/* Branch 2: Materials & Fabrication */
	_tech_nodes.push_back({
		TECH_MATERIALS_1,
		TechBranch::Materials,
		1,
		"Blast Furnace Structural Steel",
		"Continuous-cast high-tensile structural steel alloy formulations for resilient planetary trackbeds.",
		100,
		{},
		"Enables in-kind fabrication of heavy structural steel rails and depot frames."
	});
	_tech_nodes.push_back({
		TECH_MATERIALS_2,
		TechBranch::Materials,
		2,
		"Copper & Silicon Electronics Fabs",
		"High-purity monocrystalline silicon and induction-drawn copper wire fabrication for signalling networks.",
		250,
		{TECH_MATERIALS_1},
		"Enables in-kind fabrication of catenary wiring, inductive coils, and signalling microchips."
	});
	_tech_nodes.push_back({
		TECH_MATERIALS_3,
		TechBranch::Materials,
		3,
		"Superalloys & Lightweight Composites",
		"Carbon-nanotube reinforced polymers and niobium-tin superalloys reducing rolling stock mass.",
		600,
		{TECH_MATERIALS_2},
		"Unlocks lightweight composite rolling stock and grants an extra 10% BOM fabrication discount."
	});
	_tech_nodes.push_back({
		TECH_MATERIALS_4,
		TechBranch::Materials,
		4,
		"Quantum Neural Fabs & Crystal Imprinting",
		"Frontier quantum telemetry lattices and autonomous train heuristics imprinted on enriched data crystals.",
		1500,
		{TECH_MATERIALS_3},
		"Unlocks autonomous telemetry logic, AI routing, and frontier quantum crystal enrichment."
	});

	_nodes_initialized = true;
}

void TechTreeManager::Reset()
{
	std::lock_guard<std::mutex> lock(_tech_mutex);
	EnsureNodesInitialized();
	_company_techs.clear();
}

const TechProjectNode *TechTreeManager::GetNode(TechID id)
{
	EnsureNodesInitialized();
	for (const auto &node : _tech_nodes) {
		if (node.id == id) return &node;
	}
	return nullptr;
}

const std::vector<TechProjectNode> &TechTreeManager::GetAllNodes()
{
	EnsureNodesInitialized();
	return _tech_nodes;
}

std::vector<TechProjectNode> TechTreeManager::GetNodesByBranch(TechBranch branch)
{
	EnsureNodesInitialized();
	std::vector<TechProjectNode> res;
	for (const auto &node : _tech_nodes) {
		if (node.branch == branch) res.push_back(node);
	}
	return res;
}

bool TechTreeManager::IsTechUnlocked(CompanyID company, TechID id)
{
	if (company == CompanyID::Invalid() || id == TECH_NONE) return false;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto it = _company_techs.find(company);
	if (it == _company_techs.end()) return false;

	return it->second.unlocked_techs.find(id) != it->second.unlocked_techs.end();
}

TechID TechTreeManager::GetActiveProject(CompanyID company)
{
	if (company == CompanyID::Invalid()) return TECH_NONE;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto it = _company_techs.find(company);
	if (it == _company_techs.end()) return TECH_NONE;

	return it->second.active_project;
}

uint32_t TechTreeManager::GetAccumulatedRP(CompanyID company)
{
	if (company == CompanyID::Invalid()) return 0;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto it = _company_techs.find(company);
	if (it == _company_techs.end()) return 0;

	return it->second.accumulated_rp;
}

uint32_t TechTreeManager::GetMonthlyBudget(CompanyID company)
{
	if (company == CompanyID::Invalid()) return 0;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto it = _company_techs.find(company);
	if (it == _company_techs.end()) return 0;

	return it->second.monthly_budget;
}

bool TechTreeManager::CanResearch(CompanyID company, TechID id, std::string &err_msg)
{
	if (company == CompanyID::Invalid()) {
		err_msg = "Invalid company.";
		return false;
	}

	const TechProjectNode *node = GetNode(id);
	if (node == nullptr) {
		err_msg = "Unknown technology project.";
		return false;
	}

	if (!CorporateHQManager::HasHQ(company)) {
		err_msg = "Must establish an active Corporate Headquarters on a Phase 1 Core World first.";
		return false;
	}

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto &state = _company_techs[company];

	if (state.unlocked_techs.find(id) != state.unlocked_techs.end()) {
		err_msg = "Technology already researched.";
		return false;
	}

	for (TechID prereq : node->prerequisites) {
		if (state.unlocked_techs.find(prereq) == state.unlocked_techs.end()) {
			const TechProjectNode *pnode = GetNode(prereq);
			std::string pname = (pnode != nullptr) ? pnode->name : fmt::format("Tech {}", prereq);
			err_msg = fmt::format("Prerequisite technology '{}' not yet researched.", pname);
			return false;
		}
	}

	return true;
}

bool TechTreeManager::SetActiveProject(CompanyID company, TechID id)
{
	if (company == CompanyID::Invalid()) return false;

	if (id == TECH_NONE) {
		std::lock_guard<std::mutex> lock(_tech_mutex);
		auto &state = _company_techs[company];
		state.active_project = TECH_NONE;
		return true;
	}

	std::string err_msg;
	if (!CanResearch(company, id, err_msg)) return false;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto &state = _company_techs[company];
	state.company_id = company;
	state.active_project = id;
	return true;
}

bool TechTreeManager::SetMonthlyBudget(CompanyID company, uint32_t budget)
{
	if (company == CompanyID::Invalid()) return false;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto &state = _company_techs[company];
	state.company_id = company;
	state.monthly_budget = budget;
	return true;
}

void TechTreeManager::AddResearchPoints(CompanyID company, uint32_t rp)
{
	if (company == CompanyID::Invalid() || rp == 0) return;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	auto it = _company_techs.find(company);
	if (it == _company_techs.end()) return;

	auto &state = it->second;
	if (state.active_project == TECH_NONE) return;

	const TechProjectNode *node = GetNode(state.active_project);
	if (node == nullptr) return;

	state.accumulated_rp += rp;

	if (state.accumulated_rp >= node->cost_rp) {
		state.unlocked_techs.insert(state.active_project);
		state.active_project = TECH_NONE;
		state.accumulated_rp = 0;
	}
}

void TechTreeManager::ProcessMonthlyResearch()
{
	EnsureNodesInitialized();

	std::vector<CompanyID> companies_to_process;
	{
		std::lock_guard<std::mutex> lock(_tech_mutex);
		for (const auto &[cid, state] : _company_techs) {
			if (state.active_project != TECH_NONE) {
				companies_to_process.push_back(cid);
			}
		}
	}

	for (CompanyID cid : companies_to_process) {
		if (!CorporateHQManager::HasHQ(cid)) continue;
		const CorporateHQProfile *hq = CorporateHQManager::GetHQ(cid);
		if (hq == nullptr || hq->world_id == INVALID_WORLD) continue;

		uint32_t budget = 0;
		{
			std::lock_guard<std::mutex> lock(_tech_mutex);
			auto it = _company_techs.find(cid);
			if (it == _company_techs.end() || it->second.active_project == TECH_NONE) continue;
			budget = it->second.monthly_budget;
		}

		uint32_t generated_rp = 0;

		/* 1. Process cash budget (1 RP per 1,000 credits) */
		if (budget > 0) {
			Company *comp = Company::GetIfValid(cid);
			if (comp != nullptr && comp->money >= static_cast<Money>(budget)) {
				SubtractMoneyFromCompany(cid, CommandCost(ExpensesType::Other, static_cast<Money>(budget)));
				generated_rp += budget / 1000;
			}
		}

		/* 2. Process scientific feedstock from Corporate HQ world stockpile */
		/* Enriched Quantum Data Crystals: 10 RP per unit, up to 5 units per month */
		CargoType cr_cargo = StockpileManager::RoleToDefaultCargo(FabricationRole::EnrichedCrystals);
		uint32_t avail_cr = StockpileManager::GetStock(hq->world_id, cid, cr_cargo);
		uint32_t burn_cr = std::min(avail_cr, 5u);
		if (burn_cr > 0) {
			StockpileManager::WithdrawCargo(hq->world_id, cid, cr_cargo, burn_cr);
			generated_rp += burn_cr * 10;
		}

		/* High-Tech Electronics: 5 RP per unit, up to 10 units per month */
		CargoType el_cargo = StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics);
		uint32_t avail_el = StockpileManager::GetStock(hq->world_id, cid, el_cargo);
		uint32_t burn_el = std::min(avail_el, 10u);
		if (burn_el > 0) {
			StockpileManager::WithdrawCargo(hq->world_id, cid, el_cargo, burn_el);
			generated_rp += burn_el * 5;
		}

		if (generated_rp > 0) {
			AddResearchPoints(cid, generated_rp);
		}
	}
}

std::vector<CompanyTechState> TechTreeManager::GetAllCompanyTechStates()
{
	std::lock_guard<std::mutex> lock(_tech_mutex);
	std::vector<CompanyTechState> result;
	result.reserve(_company_techs.size());
	for (const auto &[cid, state] : _company_techs) {
		result.push_back(state);
	}
	return result;
}

void TechTreeManager::RestoreCompanyTech(CompanyID company, TechID active_project, uint32_t accumulated_rp, uint32_t monthly_budget, const std::vector<TechID> &unlocked)
{
	if (company == CompanyID::Invalid()) return;

	std::lock_guard<std::mutex> lock(_tech_mutex);
	EnsureNodesInitialized();
	auto &state = _company_techs[company];
	state.company_id = company;
	state.active_project = active_project;
	state.accumulated_rp = accumulated_rp;
	state.monthly_budget = monthly_budget;
	state.unlocked_techs.clear();
	for (TechID tid : unlocked) {
		state.unlocked_techs.insert(tid);
	}
}
