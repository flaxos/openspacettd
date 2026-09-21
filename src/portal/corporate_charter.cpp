/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_charter.cpp Gate access policies and corporate diplomatic charters implementation for Sprint 50 (WP-50.4). */

#include "../stdafx.h"
#include "corporate_charter.h"
#include "universe_graph.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../economy_func.h"
#include "../command_func.h"
#include "../strings_func.h"
#include "../map_func.h"
#include "../table/strings.h"
#include "../3rdparty/fmt/format.h"

#include "../safeguards.h"

CorporateCharterManager &CorporateCharterManager::Instance()
{
	static CorporateCharterManager instance;
	return instance;
}

void CorporateCharterManager::Reset()
{
	this->_gate_policies.clear();
	this->_gate_tolls.clear();
	this->_active_charters.clear();
}

bool CorporateCharterManager::IsPrivateWorld(const std::string &world_id)
{
	if (world_id == "world_cressat" || world_id == "world_solidade" ||
	    world_id == "world_hardrock" || world_id == "world_ozzies_asteroid") {
		return true;
	}
	const UniverseNode *node = UniverseGraphManager::Instance().FindNode(world_id);
	if (node != nullptr && node->connection_mode == ConnectionMode::Private) {
		return true;
	}
	return false;
}

Money CorporateCharterManager::GetDefaultCharterCost(const std::string &world_id)
{
	if (world_id == "world_ozzies_asteroid") return 2000000; // 2,000,000 Cr for Ozzie's private enclave
	if (world_id == "world_solidade") return 750000;
	return DEFAULT_CHARTER_COST;
}

void CorporateCharterManager::SetGatePolicy(TileIndex portal_tile, GateAccessPolicy policy, Money toll)
{
	this->_gate_policies[portal_tile] = policy;
	if (policy == GateAccessPolicy::TollRequired) {
		this->_gate_tolls[portal_tile] = (toll > 0) ? toll : DEFAULT_TOLL_AMOUNT;
	} else {
		this->_gate_tolls.erase(portal_tile);
	}
}

GateAccessPolicy CorporateCharterManager::GetGatePolicy(TileIndex portal_tile) const
{
	auto it = this->_gate_policies.find(portal_tile);
	return (it != this->_gate_policies.end()) ? it->second : GateAccessPolicy::Public;
}

Money CorporateCharterManager::GetGateToll(TileIndex portal_tile) const
{
	auto it = this->_gate_tolls.find(portal_tile);
	return (it != this->_gate_tolls.end()) ? it->second : Money(0);
}

bool CorporateCharterManager::HasCharter(CompanyID company, const std::string &target_world_id) const
{
	auto it = this->_active_charters.find(company);
	if (it == this->_active_charters.end()) return false;
	return it->second.find(target_world_id) != it->second.end();
}

bool CorporateCharterManager::GrantCharter(CompanyID company, const std::string &target_world_id)
{
	if (target_world_id.empty()) return false;
	this->_active_charters[company].insert(target_world_id);
	return true;
}

bool CorporateCharterManager::RevokeCharter(CompanyID company, const std::string &target_world_id)
{
	auto it = this->_active_charters.find(company);
	if (it == this->_active_charters.end()) return false;
	return it->second.erase(target_world_id) > 0;
}

std::vector<std::string> CorporateCharterManager::GetCompanyCharters(CompanyID company) const
{
	std::vector<std::string> res;
	auto it = this->_active_charters.find(company);
	if (it != this->_active_charters.end()) {
		for (const auto &w : it->second) res.push_back(w);
	}
	return res;
}

GateAccessResult CorporateCharterManager::CheckAndProcessAccess(
	CompanyID company,
	TileIndex portal_tile,
	const std::string &target_world_id)
{
	GateAccessResult res;
	GateAccessPolicy policy = GetGatePolicy(portal_tile);
	bool private_world = IsPrivateWorld(target_world_id);

	/* If private world and no explicit override policy is configured, default to CharterRequired */
	if (private_world && policy == GateAccessPolicy::Public) {
		policy = GateAccessPolicy::CharterRequired;
	}

	switch (policy) {
		case GateAccessPolicy::Public:
			res.allowed = true;
			return res;

		case GateAccessPolicy::ReputationRestricted: {
			if (HasCharter(company, target_world_id)) {
				res.allowed = true;
				return res;
			}
			Company *c = Company::GetIfValid(company);
			int score = 0;
			if (c != nullptr) {
				score = UpdateCompanyRatingAndValue(c, false);
				if (c->old_economy[0].performance_history > score) {
					score = c->old_economy[0].performance_history;
				}
			}
			if (score >= MIN_REPUTATION_RATING) {
				res.allowed = true;
				return res;
			}
			res.allowed = false;
			res.reason = fmt::format("Company reputation too low ({:.1f}% < 80.0%)", static_cast<double>(score) / 10.0);
			return res;
		}

		case GateAccessPolicy::CharterRequired: {
			/* Check charter first */
			if (HasCharter(company, target_world_id)) {
				res.allowed = true;
				return res;
			}
			/* Alternatively, company with high reputation (>= 80%) is admitted to private enclaves */
			Company *c = Company::GetIfValid(company);
			int score = 0;
			if (c != nullptr) {
				score = UpdateCompanyRatingAndValue(c, false);
				if (c->old_economy[0].performance_history > score) {
					score = c->old_economy[0].performance_history;
				}
			}
			if (score >= MIN_REPUTATION_RATING) {
				res.allowed = true;
				return res;
			}
			res.allowed = false;
			res.reason = fmt::format("Corporate diplomatic charter required for {} (or >= 80% reputation)", target_world_id);
			return res;
		}

		case GateAccessPolicy::TollRequired: {
			Money toll = GetGateToll(portal_tile);
			Company *c = Company::GetIfValid(company);
			if (c != nullptr && c->money < toll) {
				res.allowed = false;
				res.reason = fmt::format("Insufficient company funds for gate toll ({})", toll);
				return res;
			}
			if (c != nullptr && toll > 0) {
				c->money -= toll;
				res.toll_charged = toll;
			}
			res.allowed = true;
			return res;
		}
	}

	res.allowed = true;
	return res;
}

CommandCost CmdSetGateAccessPolicy(DoCommandFlags flags, TileIndex tile, GateAccessPolicy policy, Money toll)
{
	if (tile >= Map::Size()) return CMD_ERROR;
	if (flags.Test(DoCommandFlag::Execute)) {
		CorporateCharterManager::Instance().SetGatePolicy(tile, policy, toll);
	}
	return CommandCost();
}

CommandCost CmdPurchaseDiplomaticCharter(DoCommandFlags flags, CompanyID company, const std::string &target_world_id)
{
	if (!Company::IsValidID(company) || target_world_id.empty()) return CMD_ERROR;
	Money cost = CorporateCharterManager::GetDefaultCharterCost(target_world_id);

	Company *c = Company::GetIfValid(company);
	if (c == nullptr || c->money < cost) {
		return CommandCost(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		CorporateCharterManager::Instance().GrantCharter(company, target_world_id);
	}

	return CommandCost(ExpensesType::Other, cost);
}
