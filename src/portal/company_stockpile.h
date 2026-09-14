/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file company_stockpile.h Domain model and ledger for company-owned planetary stockpiles. */

#ifndef COMPANY_STOCKPILE_H
#define COMPANY_STOCKPILE_H

#include "../cargo_type.h"
#include "../company_type.h"
#include "planet_type.h"

#include <cstdint>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

/** Abstract fabrication and manufacturing roles mapped to canonical game cargos. */
enum class FabricationRole : uint8_t {
	Ballast         = 0, ///< Trackbed gravel, stone, slag, high-strength concrete.
	StructuralMetal = 1, ///< Structural steel, bridge girders, depot frames, vehicle chassis.
	Wiring          = 2, ///< Conductive copper wire, inductive coils, dynamos, overhead catenary.
	Electronics     = 3, ///< Silicon wafers, microchips, signal logic, telemetry processors.
	Superalloy      = 4, ///< High-performance exotic alloys, cryo-bogies, maglev guideways.
	Composites      = 5, ///< Synthetic polymers, aerodynamic body shells.
	BlankCrystals   = 6, ///< Unformatted monocrystalline substrate.
	EnrichedCrystals= 7, ///< Imprinted quantum crystals with advanced mathematical proofs.
	Count           = 8,
};

/**
 * Company-owned material ledger for a specific colonized world.
 */
struct CompanyWorldStockpile {
	WorldID world_id = INVALID_WORLD;
	CompanyID company_id = CompanyID::Invalid();

	/* Dynamic cargo inventory mapping: CargoType -> quantity */
	std::map<CargoType, uint32_t> inventory;

	/** Get quantity of specific cargo held in this stockpile. */
	uint32_t GetStock(CargoType cargo) const
	{
		auto it = inventory.find(cargo);
		return (it != inventory.end()) ? it->second : 0;
	}

	/** Add cargo to stockpile. */
	void AddCargo(CargoType cargo, uint32_t amount)
	{
		if (amount > 0) {
			inventory[cargo] += amount;
		}
	}

	/** Withdraw cargo from stockpile. Returns amount actually withdrawn. */
	uint32_t WithdrawCargo(CargoType cargo, uint32_t amount)
	{
		auto it = inventory.find(cargo);
		if (it == inventory.end() || it->second == 0 || amount == 0) return 0;
		uint32_t available = it->second;
		uint32_t withdrawn = std::min(available, amount);
		it->second -= withdrawn;
		if (it->second == 0) {
			inventory.erase(it);
		}
		return withdrawn;
	}
};

/**
 * Global singleton manager for planetary company stockpiles.
 */
class StockpileManager {
public:
	/** Reset all stockpiles across all worlds and companies (e.g. on new game or load). */
	static void Reset();

	/**
	 * Add cargo to a company's stockpile on a specific world.
	 */
	static void AddCargo(WorldID world, CompanyID company, CargoType cargo, uint32_t amount);

	/**
	 * Withdraw cargo from a company's stockpile on a specific world.
	 * Returns the amount successfully withdrawn (up to requested amount).
	 */
	static uint32_t WithdrawCargo(WorldID world, CompanyID company, CargoType cargo, uint32_t amount);

	/**
	 * Get current quantity of a specific cargo held by a company on a world.
	 */
	static uint32_t GetStock(WorldID world, CompanyID company, CargoType cargo);

	/**
	 * Check if a company on a world has sufficient materials for a required bill of materials (BOM).
	 */
	static bool HasSufficient(WorldID world, CompanyID company, const std::map<CargoType, uint32_t> &bom);

	/**
	 * Deduct a bill of materials (BOM) from a company's stockpile on a world.
	 * Precondition: HasSufficient must return true.
	 * Returns true if deduction succeeded.
	 */
	static bool ConsumeBOM(WorldID world, CompanyID company, const std::map<CargoType, uint32_t> &bom);

	/**
	 * Retrieve all active stockpiles for save/load serialization and UI matrix display.
	 */
	static std::vector<CompanyWorldStockpile> GetAllStockpiles();

	/**
	 * Restore a stockpile entry during savegame deserialization.
	 */
	static void RestoreStockpile(WorldID world, CompanyID company, const std::map<CargoType, uint32_t> &inv);

	/**
	 * Map an abstract fabrication role to default vanilla cargo types.
	 */
	static CargoType RoleToDefaultCargo(FabricationRole role);
};

#endif /* COMPANY_STOCKPILE_H */
