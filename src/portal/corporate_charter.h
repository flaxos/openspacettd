/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_charter.h Gate access policies and corporate diplomatic charters for Sprint 50 (WP-50.4). */

#ifndef CORPORATE_CHARTER_H
#define CORPORATE_CHARTER_H

#include "../command_type.h"
#include "../company_type.h"
#include "../economy_type.h"
#include "../tile_type.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

/** Gate access policy governing cross-world transit. */
enum class GateAccessPolicy : uint8_t {
	Public               = 0, ///< Unrestricted access for all registered companies.
	ReputationRestricted = 1, ///< Company performance rating must be >= 80% (score >= 800).
	CharterRequired      = 2, ///< Requires an active corporate diplomatic charter for the target world.
	TollRequired         = 3, ///< Requires payment of a per-train transit toll.
};

/** Diagnostic outcome of a gate access check. */
struct GateAccessResult {
	bool allowed = false;
	Money toll_charged = 0;
	std::string reason;
};

/**
 * Manager handling corporate diplomatic charters, gate access policies,
 * company reputation verification, and per-train toll collection.
 */
class CorporateCharterManager {
public:
	static constexpr int MIN_REPUTATION_RATING = 800; ///< 80.0% performance rating required for restricted worlds.
	static constexpr Money DEFAULT_CHARTER_COST = 500000; ///< 500,000 Cr standard charter purchase cost.
	static constexpr Money DEFAULT_TOLL_AMOUNT = 5000; ///< 5,000 Cr standard per-train toll.

	static CorporateCharterManager &Instance();
	void Reset();

	void SetGatePolicy(TileIndex portal_tile, GateAccessPolicy policy, Money toll = 0);
	GateAccessPolicy GetGatePolicy(TileIndex portal_tile) const;
	Money GetGateToll(TileIndex portal_tile) const;

	bool HasCharter(CompanyID company, const std::string &target_world_id) const;
	bool GrantCharter(CompanyID company, const std::string &target_world_id);
	bool RevokeCharter(CompanyID company, const std::string &target_world_id);
	std::vector<std::string> GetCompanyCharters(CompanyID company) const;

	static bool IsPrivateWorld(const std::string &world_id);
	static Money GetDefaultCharterCost(const std::string &world_id);

	/**
	 * Verify if a company has permission to send rolling stock to a target world
	 * through the given portal gate. Automatically processes toll payment if applicable.
	 */
	GateAccessResult CheckAndProcessAccess(
		CompanyID company,
		TileIndex portal_tile,
		const std::string &target_world_id
	);

private:
	std::map<TileIndex, GateAccessPolicy> _gate_policies;
	std::map<TileIndex, Money> _gate_tolls;
	std::map<CompanyID, std::set<std::string>> _active_charters;
};

/**
 * Command to set or update gate access policy and toll on a portal tile.
 */
CommandCost CmdSetGateAccessPolicy(DoCommandFlags flags, TileIndex tile, GateAccessPolicy policy, Money toll);

/**
 * Command for a company to purchase a corporate diplomatic charter to a target world.
 */
CommandCost CmdPurchaseDiplomaticCharter(DoCommandFlags flags, CompanyID company, const std::string &target_world_id);

#endif /* CORPORATE_CHARTER_H */
