/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_alliance.cpp Implementation of corporate alliance and shared trackage rights. */

#include "../stdafx.h"
#include "corporate_alliance.h"
#include "../company_base.h"

#include <mutex>

#include "../safeguards.h"

namespace {

static std::mutex _alliance_mutex;
static std::map<std::pair<CompanyID, CompanyID>, CorporateRelation> _relations;

static inline std::pair<CompanyID, CompanyID> NormalizeKey(CompanyID c1, CompanyID c2)
{
	if (c1 > c2) std::swap(c1, c2);
	return {c1, c2};
}

} // namespace

void CorporateAllianceManager::Reset()
{
	std::lock_guard<std::mutex> lock(_alliance_mutex);
	_relations.clear();
}

CorporateRelation CorporateAllianceManager::GetRelation(CompanyID c1, CompanyID c2)
{
	if (c1 == c2) return CorporateRelation::Allied;
	if (c1 >= MAX_COMPANIES || c2 >= MAX_COMPANIES || c1 == CompanyID::Invalid() || c2 == CompanyID::Invalid()) return CorporateRelation::Neutral;

	std::lock_guard<std::mutex> lock(_alliance_mutex);
	auto it = _relations.find(NormalizeKey(c1, c2));
	if (it != _relations.end()) return it->second;
	return CorporateRelation::Neutral;
}

void CorporateAllianceManager::SetRelation(CompanyID c1, CompanyID c2, CorporateRelation relation)
{
	if (c1 == c2 || c1 >= MAX_COMPANIES || c2 >= MAX_COMPANIES || c1 == CompanyID::Invalid() || c2 == CompanyID::Invalid()) return;

	std::lock_guard<std::mutex> lock(_alliance_mutex);
	auto key = NormalizeKey(c1, c2);
	if (relation == CorporateRelation::Neutral) {
		_relations.erase(key);
	} else {
		_relations[key] = relation;
	}
}

bool CorporateAllianceManager::CanTraverseTrack(CompanyID train_owner, Owner tile_owner)
{
	if (tile_owner == train_owner) return true;
	if (tile_owner == OWNER_NONE || tile_owner == OWNER_DEITY) return true;
	if (tile_owner >= MAX_COMPANIES || train_owner >= MAX_COMPANIES) return false;

	return GetRelation(train_owner, static_cast<CompanyID>(tile_owner)) == CorporateRelation::Allied;
}

bool CorporateAllianceManager::CanUseWaypoint(CompanyID company, Owner waypoint_owner)
{
	if (waypoint_owner == company) return true;
	if (waypoint_owner == OWNER_NONE || waypoint_owner == OWNER_DEITY) return true;
	if (waypoint_owner >= MAX_COMPANIES || company >= MAX_COMPANIES) return false;

	return GetRelation(company, static_cast<CompanyID>(waypoint_owner)) == CorporateRelation::Allied;
}

bool CorporateAllianceManager::CanUseStation(CompanyID company, Owner station_owner)
{
	if (station_owner == company) return true;
	if (station_owner == OWNER_NONE || station_owner == OWNER_DEITY) return true;
	if (station_owner >= MAX_COMPANIES || company >= MAX_COMPANIES) return false;

	return GetRelation(company, static_cast<CompanyID>(station_owner)) == CorporateRelation::Allied;
}

std::vector<CorporateAllianceRecord> CorporateAllianceManager::GetAllRelations()
{
	std::lock_guard<std::mutex> lock(_alliance_mutex);
	std::vector<CorporateAllianceRecord> list;
	list.reserve(_relations.size());
	for (const auto &[pair, rel] : _relations) {
		list.push_back(CorporateAllianceRecord{pair.first, pair.second, rel});
	}
	return list;
}

void CorporateAllianceManager::RestoreRelation(CompanyID c1, CompanyID c2, CorporateRelation relation)
{
	SetRelation(c1, c2, relation);
}
