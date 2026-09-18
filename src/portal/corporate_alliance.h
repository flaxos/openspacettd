/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_alliance.h Corporate alliance and track sharing management. */

#ifndef CORPORATE_ALLIANCE_H
#define CORPORATE_ALLIANCE_H

#include "../company_type.h"
#include <map>
#include <vector>
#include <utility>

/**
 * Diplomatic treaty standing between two corporations.
 */
enum class CorporateRelation : uint8_t {
	Hostile = 0, ///< Territory interdicted; tracks and facilities strictly inaccessible.
	Neutral = 1, ///< Standard competitor relations; private competitor tracks, public CST tracks open.
	Allied  = 2, ///< Reciprocal running powers; mutual track, waypoint, and station sharing.
};

/**
 * Record of an established alliance treaty between two corporations.
 */
struct CorporateAllianceRecord {
	CompanyID company_a = CompanyID::Invalid();
	CompanyID company_b = CompanyID::Invalid();
	CorporateRelation relation = CorporateRelation::Neutral;
};

/**
 * Authority manager for corporate diplomatic treaties, shared trackage rights,
 * and neutral Commonwealth Space Transit (CST) infrastructure routing.
 */
class CorporateAllianceManager {
public:
	/** Reset all corporate alliances and treaties (used in tests and game reset). */
	static void Reset();

	/**
	 * Get the diplomatic relation between two companies.
	 * If c1 == c2, always returns Allied.
	 * If no treaty is recorded, defaults to Neutral.
	 */
	static CorporateRelation GetRelation(CompanyID c1, CompanyID c2);

	/**
	 * Set the diplomatic relation between two companies (symmetric treaty).
	 */
	static void SetRelation(CompanyID c1, CompanyID c2, CorporateRelation relation);

	/**
	 * Check if a train owned by train_owner is authorized to traverse a tile owned by tile_owner.
	 * Public neutral track (OWNER_NONE) and owned track are always authorized.
	 * Competitor track requires an Allied relationship.
	 */
	static bool CanTraverseTrack(CompanyID train_owner, Owner tile_owner);

	/**
	 * Check if a company can schedule orders to a waypoint owned by waypoint_owner.
	 * Neutral waypoints (OWNER_NONE) and allied waypoints are accessible.
	 */
	static bool CanUseWaypoint(CompanyID company, Owner waypoint_owner);

	/**
	 * Check if a company can schedule orders to a station owned by station_owner.
	 * Neutral stations (OWNER_NONE) and allied stations are accessible.
	 */
	static bool CanUseStation(CompanyID company, Owner station_owner);

	/** Retrieve all established non-neutral treaties for serialization. */
	static std::vector<CorporateAllianceRecord> GetAllRelations();

	/** Restore a relation from savegame. */
	static void RestoreRelation(CompanyID c1, CompanyID c2, CorporateRelation relation);
};

#endif /* CORPORATE_ALLIANCE_H */
