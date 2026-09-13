/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file megacity_manager.h Megacity sustained multi-commodity demand mechanics for Phase F4. */

#ifndef MEGACITY_MANAGER_H
#define MEGACITY_MANAGER_H

#include "../town_type.h"
#include "portal_type.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/** Three-tier commodity demand classification for metropolitan core worlds. */
enum class MegacityDemandTier : uint8_t {
	Tier1_Sustenance  = 0, ///< Food, Water, and basic survival consumables.
	Tier2_Expansion   = 1, ///< Steel, Goods, Building materials, and structural components.
	Tier3_Prosperity  = 2, ///< High-Tech, Electronics, Data Crystals, and Valuables.
	End               = 3,
};

/** Metropolitan growth stage determined by multi-tier supply satisfaction. */
enum class MegacityGrowthState : uint8_t {
	Starvation        = 0, ///< Tier 1 < 50%: Stagnation, growth frozen, possible population decay.
	Subsistence       = 1, ///< Tier 1 met (>= 50%), Tier 2 incomplete: Normal baseline growth.
	MetropolitanBoom  = 2, ///< Tier 1 & 2 met (>= 100%): Accelerated expansion (+50% growth rate).
	HyperGrowth       = 3, ///< All 3 tiers met (>= 100%): Maximum hyper-growth (+100% growth, +50% passenger/mail generation).
};

/** Status and historical delivery metrics for one registered Megacity. */
struct MegacityProfile {
	TownID town_id = TownID::Invalid();
	WorldID world_id = INVALID_WORLD;
	std::string town_name;
	uint32_t population = 1000;

	std::array<uint32_t, 3> monthly_quota{50, 30, 10};
	std::array<uint32_t, 3> delivered_current{0, 0, 0};
	std::array<uint32_t, 3> delivered_last{0, 0, 0};
	std::array<float, 3> satisfaction_pct{0.0f, 0.0f, 0.0f};

	float overall_supply_index = 0.0f; ///< Average satisfaction percentage (0.0 to 1.0+).
	MegacityGrowthState growth_state = MegacityGrowthState::Subsistence;
	float growth_multiplier = 1.0f;
	float passenger_multiplier = 1.0f;

	bool IsValid() const { return this->town_id != TownID::Invalid(); }
};

/** Manager for Megacity commodity demand profiles, monthly supply cycles, and growth state calculation. */
class MegacityManager {
public:
	static void Reset();

	/* Registration & Inspection */
	static bool RegisterMegacity(TownID town_id, WorldID world_id, const std::string &town_name, uint32_t population = 1000);
	static void RestoreMegacity(const MegacityProfile &profile);
	static bool UnregisterMegacity(TownID town_id);
	static bool IsMegacity(TownID town_id);
	static const MegacityProfile *GetProfile(TownID town_id);
	static std::vector<MegacityProfile> GetAllMegacities();

	/* Deliveries & Quotas */
	static void UpdatePopulation(TownID town_id, uint32_t population);
	static void SetCustomQuotas(TownID town_id, uint32_t t1_quota, uint32_t t2_quota, uint32_t t3_quota);
	static void RecordDelivery(TownID town_id, MegacityDemandTier tier, uint32_t amount);
	static void RecordDeliveryByCargo(TownID town_id, uint8_t cargo_type, uint32_t amount);

	/* Monthly Evaluation */
	static void EvaluateMonthlySupply();

	/* Cargo Classification */
	static MegacityDemandTier ClassifyCargo(uint8_t cargo_type);

private:
	static std::map<uint32_t, MegacityProfile> _megacities;
};

#endif /* MEGACITY_MANAGER_H */
