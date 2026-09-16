/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file company_stockpile.cpp Implementation of planetary company stockpile ledger and manager. */

#include "../stdafx.h"
#include "company_stockpile.h"
#include "../cargotype.h"
#include "production_chain.h"

#include <map>
#include <mutex>

static std::map<std::pair<WorldID, CompanyID>, CompanyWorldStockpile> _company_stockpiles;
static std::mutex _stockpile_mutex;

void StockpileManager::Reset()
{
	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	_company_stockpiles.clear();
}

void StockpileManager::AddCargo(WorldID world, CompanyID company, CargoType cargo, uint32_t amount)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid() || cargo >= NUM_CARGO || amount == 0) return;

	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	auto &stockpile = _company_stockpiles[{world, company}];
	stockpile.world_id = world;
	stockpile.company_id = company;
	stockpile.AddCargo(cargo, amount);
}

uint32_t StockpileManager::WithdrawCargo(WorldID world, CompanyID company, CargoType cargo, uint32_t amount)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid() || cargo >= NUM_CARGO || amount == 0) return 0;

	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	auto it = _company_stockpiles.find({world, company});
	if (it == _company_stockpiles.end()) return 0;

	return it->second.WithdrawCargo(cargo, amount);
}

uint32_t StockpileManager::GetStock(WorldID world, CompanyID company, CargoType cargo)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return 0;

	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	auto it = _company_stockpiles.find({world, company});
	if (it == _company_stockpiles.end()) return 0;

	return it->second.GetStock(cargo);
}

bool StockpileManager::HasSufficient(WorldID world, CompanyID company, const std::map<CargoType, uint32_t> &bom)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;

	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	auto it = _company_stockpiles.find({world, company});
	if (it == _company_stockpiles.end()) return bom.empty();

	for (const auto &[cargo, required] : bom) {
		if (cargo >= NUM_CARGO || it->second.GetStock(cargo) < required) {
			return false;
		}
	}
	return true;
}

bool StockpileManager::ConsumeBOM(WorldID world, CompanyID company, const std::map<CargoType, uint32_t> &bom)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;

	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	auto it = _company_stockpiles.find({world, company});
	if (it == _company_stockpiles.end()) return bom.empty();

	/* Verify full BOM availability before deducting any item */
	for (const auto &[cargo, required] : bom) {
		if (cargo >= NUM_CARGO || it->second.GetStock(cargo) < required) {
			return false;
		}
	}

	/* Perform deduction */
	for (const auto &[cargo, required] : bom) {
		it->second.WithdrawCargo(cargo, required);
	}
	return true;
}

std::vector<CompanyWorldStockpile> StockpileManager::GetAllStockpiles()
{
	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	std::vector<CompanyWorldStockpile> result;
	result.reserve(_company_stockpiles.size());
	for (const auto &[key, sp] : _company_stockpiles) {
		if (!sp.inventory.empty()) {
			result.push_back(sp);
		}
	}
	return result;
}

void StockpileManager::RestoreStockpile(WorldID world, CompanyID company, const std::map<CargoType, uint32_t> &inv)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return;

	std::lock_guard<std::mutex> lock(_stockpile_mutex);
	auto &stockpile = _company_stockpiles[{world, company}];
	stockpile.world_id = world;
	stockpile.company_id = company;
	stockpile.inventory = inv;
}

CargoType StockpileManager::RoleToDefaultCargo(FabricationRole role)
{
	static constexpr CommonwealthCargoID roles[] = {
		CommonwealthCargoID::StoneSlag, CommonwealthCargoID::StructuralSteel,
		CommonwealthCargoID::ConductiveWiring, CommonwealthCargoID::SiliconChips,
		CommonwealthCargoID::Superalloys, CommonwealthCargoID::SyntheticComposites,
		CommonwealthCargoID::BlankCrystals, CommonwealthCargoID::EnrichedQuantumCrystals,
	};
	return role < FabricationRole::Count ? ProductionChainManager::GetDefaultCargo(roles[static_cast<size_t>(role)]) : INVALID_CARGO;
}
