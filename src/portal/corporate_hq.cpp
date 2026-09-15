/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file corporate_hq.cpp Implementation of corporate headquarters management and validation. */

#include "../stdafx.h"
#include "corporate_hq.h"
#include "planet_manager.h"
#include "federation_identity.h"
#include "federation_player.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../station_base.h"
#include "../timer/timer_game_calendar.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <set>

static std::map<CompanyID, CorporateHQProfile> _corporate_hqs;
static std::mutex _hq_mutex;

void CorporateHQManager::Reset()
{
	std::lock_guard<std::mutex> lock(_hq_mutex);
	_corporate_hqs.clear();
}

bool CorporateHQManager::HasHQ(CompanyID company)
{
	if (company == CompanyID::Invalid()) return false;
	std::lock_guard<std::mutex> lock(_hq_mutex);
	return _corporate_hqs.count(company) > 0;
}

const CorporateHQProfile *CorporateHQManager::GetHQ(CompanyID company)
{
	if (company == CompanyID::Invalid()) return nullptr;
	std::lock_guard<std::mutex> lock(_hq_mutex);
	auto it = _corporate_hqs.find(company);
	return (it != _corporate_hqs.end()) ? &it->second : nullptr;
}

const CorporateHQProfile *CorporateHQManager::GetHQAtTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return nullptr;
	std::lock_guard<std::mutex> lock(_hq_mutex);
	for (const auto &[comp, hq] : _corporate_hqs) {
		if (hq.tile == tile) {
			return &hq;
		}
	}
	return nullptr;
}

bool CorporateHQManager::CanPlaceHQ(CompanyID company, TileIndex tile, std::string &err_msg)
{
	if (company == CompanyID::Invalid()) {
		err_msg = "Invalid company";
		return false;
	}

	/* Check if already founded */
	{
		std::lock_guard<std::mutex> lock(_hq_mutex);
		if (_corporate_hqs.count(company) > 0) {
			err_msg = "Company already has an active Corporate Headquarters";
			return false;
		}
	}

	if (tile == INVALID_TILE) {
		err_msg = "Invalid tile location";
		return false;
	}

	/* 1. Phase 1 Core world requirement */
	const PlanetRegion *region = PlanetManager::GetRegionByTile(tile);
	if (region == nullptr) {
		err_msg = "Headquarters must be placed within a registered world boundary";
		return false;
	}
	if (region->phase != WorldPhase::Phase1_Core) {
		err_msg = "Corporate Headquarters must be founded on a Phase 1 Core World";
		return false;
	}

	/* 2. Capital requirement (Minimum 5,000,000 Cr) */
	Company *c = Company::GetIfValid(company);
	if (c != nullptr && c->money < 5000000) {
		err_msg = "Insufficient funds: Corporate Headquarters requires at least 5,000,000 Cr";
		return false;
	}

	/* 3. Multi-world presence (Active across >= 3 distinct world phases) */
	std::set<WorldPhase> phases_present;
	phases_present.insert(region->phase); // Currently on Phase 1

	/* Check federation corporate charter if registered */
	auto global_comp = FederationIdentityRegistry::FindCompany(company);
	if (global_comp.has_value()) {
		const CorporateCharter *charter = FederationPlayerRegistry::GetCompanyCharter(*global_comp);
		if (charter != nullptr) {
			for (WorldID wid : charter->active_world_presences) {
				const PlanetRegion *r = PlanetManager::GetRegion(wid);
				if (r != nullptr) {
					phases_present.insert(r->phase);
				}
			}
		}
	}

	/* Local operations count only when this company owns a live rail station. */
	for (const Station *station : Station::Iterate()) {
		if (station->owner != company || !station->facilities.Test(StationFacility::Train)) continue;
		const PlanetRegion *operated = PlanetManager::GetRegionByTile(station->xy);
		if (operated != nullptr && operated->phase != WorldPhase::Phase4_Expansion) phases_present.insert(operated->phase);
	}

	if (phases_present.size() < 3) {
		err_msg = "Corporate network must span operations across at least 3 distinct world phases";
		return false;
	}

	return true;
}

bool CorporateHQManager::RegisterHQ(CompanyID company, WorldID world, TileIndex tile, const std::string &name)
{
	if (company == CompanyID::Invalid() || world == INVALID_WORLD || tile == INVALID_TILE) return false;

	std::lock_guard<std::mutex> lock(_hq_mutex);
	CorporateHQProfile hq{
		.company_id = company,
		.world_id = world,
		.tile = tile,
		.tier = CorporateHQTier::RegionalBranch,
		.campus_name = name.empty() ? "Corporate HQ Campus" : name,
		.founding_date = static_cast<uint64_t>(TimerGameCalendar::date.base()),
	};
	_corporate_hqs[company] = hq;
	return true;
}

bool CorporateHQManager::UpgradeHQTier(CompanyID company)
{
	std::lock_guard<std::mutex> lock(_hq_mutex);
	auto it = _corporate_hqs.find(company);
	if (it == _corporate_hqs.end()) return false;

	uint8_t next = static_cast<uint8_t>(it->second.tier) + 1;
	if (next <= static_cast<uint8_t>(CorporateHQTier::CST_Arcology)) {
		it->second.tier = static_cast<CorporateHQTier>(next);
		return true;
	}
	return false;
}

std::vector<CorporateHQProfile> CorporateHQManager::GetAllHQ()
{
	std::lock_guard<std::mutex> lock(_hq_mutex);
	std::vector<CorporateHQProfile> result;
	result.reserve(_corporate_hqs.size());
	for (const auto &[comp, hq] : _corporate_hqs) {
		result.push_back(hq);
	}
	return result;
}

void CorporateHQManager::RestoreHQ(const CorporateHQProfile &hq)
{
	std::lock_guard<std::mutex> lock(_hq_mutex);
	_corporate_hqs[hq.company_id] = hq;
}
