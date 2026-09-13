/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file megacity_manager.cpp Implementation of Megacity sustained commodity demand mechanics. */

#include "../stdafx.h"
#include "megacity_manager.h"
#include <algorithm>

#include "../safeguards.h"

std::map<uint32_t, MegacityProfile> MegacityManager::_megacities;

void MegacityManager::Reset()
{
	_megacities.clear();
}

bool MegacityManager::RegisterMegacity(TownID town_id, WorldID world_id, const std::string &town_name, uint32_t population)
{
	if (town_id == TownID::Invalid()) return false;

	MegacityProfile profile;
	profile.town_id = town_id;
	profile.world_id = world_id;
	profile.town_name = town_name;
	profile.population = population;

	/* Dynamic quota formula based on population scale */
	profile.monthly_quota[0] = std::max<uint32_t>(50, population / 20);  // Tier 1 Sustenance
	profile.monthly_quota[1] = std::max<uint32_t>(30, population / 40);  // Tier 2 Expansion
	profile.monthly_quota[2] = std::max<uint32_t>(10, population / 100); // Tier 3 Prosperity

	profile.delivered_current.fill(0);
	profile.delivered_last.fill(0);
	profile.satisfaction_pct.fill(0.0f);
	profile.overall_supply_index = 0.0f;
	profile.growth_state = MegacityGrowthState::Subsistence;
	profile.growth_multiplier = 1.0f;
	profile.passenger_multiplier = 1.0f;

	_megacities[town_id.base()] = std::move(profile);
	return true;
}

bool MegacityManager::UnregisterMegacity(TownID town_id)
{
	if (town_id == TownID::Invalid()) return false;
	return _megacities.erase(town_id.base()) > 0;
}

bool MegacityManager::IsMegacity(TownID town_id)
{
	if (town_id == TownID::Invalid()) return false;
	return _megacities.contains(town_id.base());
}

const MegacityProfile *MegacityManager::GetProfile(TownID town_id)
{
	if (town_id == TownID::Invalid()) return nullptr;
	auto it = _megacities.find(town_id.base());
	return it != _megacities.end() ? &it->second : nullptr;
}

std::vector<MegacityProfile> MegacityManager::GetAllMegacities()
{
	std::vector<MegacityProfile> result;
	result.reserve(_megacities.size());
	for (const auto &[_, profile] : _megacities) {
		result.push_back(profile);
	}
	return result;
}

void MegacityManager::UpdatePopulation(TownID town_id, uint32_t population)
{
	auto it = _megacities.find(town_id.base());
	if (it == _megacities.end()) return;

	it->second.population = population;
	it->second.monthly_quota[0] = std::max<uint32_t>(50, population / 20);
	it->second.monthly_quota[1] = std::max<uint32_t>(30, population / 40);
	it->second.monthly_quota[2] = std::max<uint32_t>(10, population / 100);
}

void MegacityManager::SetCustomQuotas(TownID town_id, uint32_t t1_quota, uint32_t t2_quota, uint32_t t3_quota)
{
	auto it = _megacities.find(town_id.base());
	if (it == _megacities.end()) return;

	it->second.monthly_quota[0] = t1_quota;
	it->second.monthly_quota[1] = t2_quota;
	it->second.monthly_quota[2] = t3_quota;
}

void MegacityManager::RecordDelivery(TownID town_id, MegacityDemandTier tier, uint32_t amount)
{
	if (amount == 0) return;
	auto it = _megacities.find(town_id.base());
	if (it == _megacities.end()) return;

	size_t idx = static_cast<size_t>(tier);
	if (idx < it->second.delivered_current.size()) {
		it->second.delivered_current[idx] += amount;
	}
}

void MegacityManager::RecordDeliveryByCargo(TownID town_id, uint8_t cargo_type, uint32_t amount)
{
	MegacityDemandTier tier = ClassifyCargo(cargo_type);
	RecordDelivery(town_id, tier, amount);
}

MegacityDemandTier MegacityManager::ClassifyCargo(uint8_t cargo_type)
{
	switch (cargo_type) {
		case 4:  // Livestock
		case 6:  // Grain
		case 11: // Food
		case 12: // Water
			return MegacityDemandTier::Tier1_Sustenance;

		case 5:  // Goods
		case 7:  // Wood
		case 9:  // Steel
		case 8:  // Iron Ore
			return MegacityDemandTier::Tier2_Expansion;

		case 10: // Valuables / Diamonds / Data Crystals
		case 3:  // Oil / Chemicals
			return MegacityDemandTier::Tier3_Prosperity;

		default:
			return static_cast<MegacityDemandTier>(cargo_type % 3);
	}
}

void MegacityManager::EvaluateMonthlySupply()
{
	for (auto &[_, profile] : _megacities) {
		profile.delivered_last = profile.delivered_current;
		profile.delivered_current.fill(0);

		float total_sat = 0.0f;
		for (size_t i = 0; i < 3; i++) {
			if (profile.monthly_quota[i] > 0) {
				profile.satisfaction_pct[i] = static_cast<float>(profile.delivered_last[i]) / static_cast<float>(profile.monthly_quota[i]);
			} else {
				profile.satisfaction_pct[i] = 1.0f;
			}
			total_sat += profile.satisfaction_pct[i];
		}
		profile.overall_supply_index = total_sat / 3.0f;

		/* Growth state & multiplier determination */
		if (profile.satisfaction_pct[0] < 0.5f) {
			profile.growth_state = MegacityGrowthState::Starvation;
			profile.growth_multiplier = 0.0f;
			profile.passenger_multiplier = 0.5f;
		} else if (profile.satisfaction_pct[0] >= 1.0f && profile.satisfaction_pct[1] >= 1.0f && profile.satisfaction_pct[2] >= 1.0f) {
			profile.growth_state = MegacityGrowthState::HyperGrowth;
			profile.growth_multiplier = 2.0f;
			profile.passenger_multiplier = 1.5f;
		} else if (profile.satisfaction_pct[0] >= 1.0f && profile.satisfaction_pct[1] >= 1.0f) {
			profile.growth_state = MegacityGrowthState::MetropolitanBoom;
			profile.growth_multiplier = 1.5f;
			profile.passenger_multiplier = 1.25f;
		} else {
			profile.growth_state = MegacityGrowthState::Subsistence;
			profile.growth_multiplier = 1.0f;
			profile.passenger_multiplier = 1.0f;
		}
	}
}
