/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file fabrication_manager.h Domain model, BOM registry, and in-kind fabrication engine. */

#ifndef FABRICATION_MANAGER_H
#define FABRICATION_MANAGER_H

#include "../cargo_type.h"
#include "../command_type.h"
#include "../company_type.h"
#include "../engine_base.h"
#include "../rail_type.h"
#include "company_stockpile.h"
#include "planet_type.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/** Physical Bill of Materials required for infrastructure or vehicle fabrication. */
struct BillOfMaterials {
	std::map<CargoType, uint32_t> materials; ///< CargoType -> quantity
	uint8_t discount_percent = 80;          ///< Standard 80% cash discount (leaving 20% labor fee)

	void AddRoleMaterial(FabricationRole role, uint32_t count)
	{
		CargoType c = StockpileManager::RoleToDefaultCargo(role);
		materials[c] += count;
	}

	void AddCargoMaterial(CargoType cargo, uint32_t count)
	{
		materials[cargo] += count;
	}

	uint32_t GetRequirement(CargoType cargo) const
	{
		auto it = materials.find(cargo);
		return (it != materials.end()) ? it->second : 0;
	}

	bool IsEmpty() const
	{
		return materials.empty();
	}
};

/**
 * Global singleton manager for in-kind fabrication recipes, company modes, and build interception.
 */
class FabricationManager {
public:
	/** Reset all company fabrication modes (e.g. on new game or load). */
	static void Reset();

	/**
	 * Shared content, research and inventory preflight for native and Blueprint builds.
	 * @param world World whose stockpile would supply the materials.
	 * @param company Company attempting the build.
	 * @param bom Physical materials required by the build.
	 * @return Success when content, research and stockpile requirements are satisfied; otherwise an explanatory command error.
	 */
	static CommandCost CheckMaterials(WorldID world, CompanyID company, const BillOfMaterials &bom);

	/** Check whether a company has "Fabricate from Stockpile" mode enabled. */
	static bool IsFabricateFromStockpileEnabled(CompanyID company);

	/** Set whether a company has "Fabricate from Stockpile" mode enabled. */
	static void SetFabricateFromStockpile(CompanyID company, bool enabled);

	/** Get effective cash discount percentage (80% base, or 90% if TECH_MATERIALS_3 is unlocked). */
	static uint8_t GetBOMDiscountPercent(CompanyID company);

	/** Get Bill of Materials for a specific rail type track piece. */
	static BillOfMaterials GetTrackBOM(RailType railtype);

	/** Get Bill of Materials for building a signal on a track. */
	static BillOfMaterials GetSignalBOM();

	/** Get Bill of Materials for building a rail depot. */
	static BillOfMaterials GetDepotBOM(RailType railtype);

	/** Get Bill of Materials for constructing a rail vehicle (locomotive or wagon). */
	static BillOfMaterials GetVehicleBOM(const Engine *e);

	/**
	 * Check whether a company has sufficient stockpile materials on a world to fabricate track.
	 */
	static bool CanFabricateTrack(WorldID world, CompanyID company, RailType railtype);

	/**
	 * Consume track BOM materials from the world stockpile.
	 * Precondition: CanFabricateTrack must return true.
	 */
	static bool ConsumeTrackBOM(WorldID world, CompanyID company, RailType railtype);

	/**
	 * Check whether a company has sufficient stockpile materials on a world to fabricate a signal.
	 */
	static bool CanFabricateSignal(WorldID world, CompanyID company);

	/**
	 * Consume signal BOM materials from the world stockpile.
	 */
	static bool ConsumeSignalBOM(WorldID world, CompanyID company);

	/**
	 * Check whether a company has sufficient stockpile materials on a world to fabricate a depot.
	 */
	static bool CanFabricateDepot(WorldID world, CompanyID company, RailType railtype);

	/**
	 * Consume depot BOM materials from the world stockpile.
	 */
	static bool ConsumeDepotBOM(WorldID world, CompanyID company, RailType railtype);

	/**
	 * Check whether a company has sufficient stockpile materials on a world to fabricate a vehicle.
	 */
	static bool CanFabricateVehicle(WorldID world, CompanyID company, const Engine *e);

	/**
	 * Consume vehicle BOM materials from the world stockpile.
	 */
	static bool ConsumeVehicleBOM(WorldID world, CompanyID company, const Engine *e);

	/** Retrieve all company fabrication mode settings for serialization and UI. */
	static std::map<CompanyID, bool> GetAllCompanyModes();

	/** Restore a company fabrication mode setting from savegame deserialization. */
	static void RestoreCompanyMode(CompanyID company, bool enabled);
};

#endif /* FABRICATION_MANAGER_H */
