/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file corporate_hq.h Corporate Headquarters campus manager and Phase 1 world requirements. */

#ifndef CORPORATE_HQ_H
#define CORPORATE_HQ_H

#include "../company_type.h"
#include "../tile_type.h"
#include "planet_type.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <optional>

/** Headquarters campus advancement tier. */
enum class CorporateHQTier : uint8_t {
	RegionalBranch = 1,  ///< Initial corporate presence.
	PlanetaryHQ    = 2,  ///< Consolidated administrative center.
	Interstellar   = 3,  ///< Multi-world corporate campus.
	CST_Arcology   = 4,  ///< Commonwealth CST Arcology Tower nexus.
};

/** Profile of a player company's central Corporate Headquarters campus. */
struct CorporateHQProfile {
	CompanyID company_id = CompanyID::Invalid();
	WorldID world_id = INVALID_WORLD;
	TileIndex tile = INVALID_TILE;
	CorporateHQTier tier = CorporateHQTier::RegionalBranch;
	std::string campus_name;
	uint64_t founding_date = 0;
};

/**
 * Manager for corporate headquarters campuses across the multi-world universe.
 */
class CorporateHQManager {
public:
	static void Reset();

	/** Check if a company already owns an active corporate headquarters. */
	static bool HasHQ(CompanyID company);

	/** Retrieve the corporate headquarters profile for a company. */
	static const CorporateHQProfile *GetHQ(CompanyID company);

	/** Retrieve the headquarters located at a specific tile, if any. */
	static const CorporateHQProfile *GetHQAtTile(TileIndex tile);

	/**
	 * Validate whether a company is eligible to construct a corporate HQ on the given tile.
	 * Checks Phase 1 Core world restriction, multi-world presence (>= 3 phases), and capital funds.
	 * @return true if placement is allowed; false if rejected (with failure reason in err_msg).
	 */
	static bool CanPlaceHQ(CompanyID company, TileIndex tile, std::string &err_msg);

	/**
	 * Register and establish a corporate headquarters campus.
	 */
	static bool RegisterHQ(CompanyID company, WorldID world, TileIndex tile, const std::string &name);

	/**
	 * Upgrade headquarters tier.
	 */
	static bool UpgradeHQTier(CompanyID company);

	/**
	 * Retrieve all established headquarters for serialization and GUI display.
	 */
	static std::vector<CorporateHQProfile> GetAllHQ();

	/**
	 * Restore a headquarters during savegame loading.
	 */
	static void RestoreHQ(const CorporateHQProfile &hq);
};

#endif /* CORPORATE_HQ_H */
