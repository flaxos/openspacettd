/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file fabrication_manager.cpp Implementation of in-kind fabrication engine and recipe registry. */

#include "../stdafx.h"
#include "fabrication_manager.h"
#include "tech_tree.h"
#include "../engine_base.h"
#include "../rail_type.h"
#include "../train.h"

#include <mutex>
#include <map>

static std::map<CompanyID, bool> _company_fabrication_modes;
static std::mutex _fabrication_mutex;

void FabricationManager::Reset()
{
	std::lock_guard<std::mutex> lock(_fabrication_mutex);
	_company_fabrication_modes.clear();
}

bool FabricationManager::IsFabricateFromStockpileEnabled(CompanyID company)
{
	if (company == CompanyID::Invalid()) return false;
	std::lock_guard<std::mutex> lock(_fabrication_mutex);
	auto it = _company_fabrication_modes.find(company);
	return (it != _company_fabrication_modes.end()) ? it->second : false;
}

void FabricationManager::SetFabricateFromStockpile(CompanyID company, bool enabled)
{
	if (company == CompanyID::Invalid()) return;
	std::lock_guard<std::mutex> lock(_fabrication_mutex);
	_company_fabrication_modes[company] = enabled;
}

uint8_t FabricationManager::GetBOMDiscountPercent(CompanyID company)
{
	if (TechTreeManager::IsTechUnlocked(company, TECH_MATERIALS_3)) {
		return 90;
	}
	return 80;
}

BillOfMaterials FabricationManager::GetTrackBOM(RailType railtype)
{
	BillOfMaterials bom;
	switch (railtype) {
		case RAILTYPE_ELECTRIC:
			bom.AddRoleMaterial(FabricationRole::Ballast, 2);
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 1);
			bom.AddRoleMaterial(FabricationRole::Wiring, 1);
			break;

		case RAILTYPE_MONO:
			bom.AddRoleMaterial(FabricationRole::Ballast, 4);
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 2);
			bom.AddRoleMaterial(FabricationRole::Wiring, 1);
			break;

		case RAILTYPE_MAGLEV:
			bom.AddRoleMaterial(FabricationRole::Superalloy, 2);
			bom.AddRoleMaterial(FabricationRole::Wiring, 2);
			bom.AddRoleMaterial(FabricationRole::Electronics, 1);
			break;

		case RAILTYPE_RAIL:
		default:
			bom.AddRoleMaterial(FabricationRole::Ballast, 2);
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 1);
			break;
	}
	return bom;
}

BillOfMaterials FabricationManager::GetSignalBOM()
{
	BillOfMaterials bom;
	bom.AddRoleMaterial(FabricationRole::StructuralMetal, 1);
	bom.AddRoleMaterial(FabricationRole::Wiring, 1);
	return bom;
}

BillOfMaterials FabricationManager::GetDepotBOM(RailType railtype)
{
	BillOfMaterials bom;
	switch (railtype) {
		case RAILTYPE_MONO:
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 15);
			bom.AddRoleMaterial(FabricationRole::Ballast, 8);
			bom.AddRoleMaterial(FabricationRole::Wiring, 2);
			break;

		case RAILTYPE_MAGLEV:
			bom.AddRoleMaterial(FabricationRole::Superalloy, 15);
			bom.AddRoleMaterial(FabricationRole::Ballast, 8);
			bom.AddRoleMaterial(FabricationRole::Wiring, 4);
			bom.AddRoleMaterial(FabricationRole::Electronics, 2);
			break;

		case RAILTYPE_RAIL:
		case RAILTYPE_ELECTRIC:
		default:
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 10);
			bom.AddRoleMaterial(FabricationRole::Ballast, 5);
			break;
	}
	return bom;
}

BillOfMaterials FabricationManager::GetVehicleBOM(const Engine *e)
{
	BillOfMaterials bom;
	if (e == nullptr || e->type != VehicleType::Train) return bom;

	const RailVehicleInfo *rvi = &e->VehInfo<RailVehicleInfo>();
	if (rvi->railveh_type == RailVehicleType::Wagon) {
		if (rvi->engclass == EngineClass::Maglev || rvi->engclass == EngineClass::Monorail) {
			bom.AddRoleMaterial(FabricationRole::Superalloy, 15);
			bom.AddRoleMaterial(FabricationRole::Composites, 5);
		} else {
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 10);
			bom.AddRoleMaterial(FabricationRole::Composites, 2);
		}
		return bom;
	}

	switch (rvi->engclass) {
		case EngineClass::Steam:
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 30);
			bom.AddRoleMaterial(FabricationRole::Ballast, 10);
			break;

		case EngineClass::Diesel:
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 40);
			bom.AddRoleMaterial(FabricationRole::Wiring, 15);
			break;

		case EngineClass::Electric:
			bom.AddRoleMaterial(FabricationRole::Superalloy, 35);
			bom.AddRoleMaterial(FabricationRole::Wiring, 25);
			bom.AddRoleMaterial(FabricationRole::Electronics, 10);
			break;

		case EngineClass::Monorail:
			bom.AddRoleMaterial(FabricationRole::Superalloy, 40);
			bom.AddRoleMaterial(FabricationRole::Wiring, 25);
			bom.AddRoleMaterial(FabricationRole::Electronics, 15);
			break;

		case EngineClass::Maglev:
			bom.AddRoleMaterial(FabricationRole::Superalloy, 50);
			bom.AddRoleMaterial(FabricationRole::Wiring, 25);
			bom.AddRoleMaterial(FabricationRole::Electronics, 15);
			break;

		default:
			bom.AddRoleMaterial(FabricationRole::StructuralMetal, 35);
			bom.AddRoleMaterial(FabricationRole::Wiring, 15);
			break;
	}
	return bom;
}

bool FabricationManager::CanFabricateTrack(WorldID world, CompanyID company, RailType railtype)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;
	BillOfMaterials bom = GetTrackBOM(railtype);
	return StockpileManager::HasSufficient(world, company, bom.materials);
}

bool FabricationManager::ConsumeTrackBOM(WorldID world, CompanyID company, RailType railtype)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;
	BillOfMaterials bom = GetTrackBOM(railtype);
	return StockpileManager::ConsumeBOM(world, company, bom.materials);
}

bool FabricationManager::CanFabricateSignal(WorldID world, CompanyID company)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;
	BillOfMaterials bom = GetSignalBOM();
	return StockpileManager::HasSufficient(world, company, bom.materials);
}

bool FabricationManager::ConsumeSignalBOM(WorldID world, CompanyID company)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;
	BillOfMaterials bom = GetSignalBOM();
	return StockpileManager::ConsumeBOM(world, company, bom.materials);
}

bool FabricationManager::CanFabricateDepot(WorldID world, CompanyID company, RailType railtype)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;
	BillOfMaterials bom = GetDepotBOM(railtype);
	return StockpileManager::HasSufficient(world, company, bom.materials);
}

bool FabricationManager::ConsumeDepotBOM(WorldID world, CompanyID company, RailType railtype)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid()) return false;
	BillOfMaterials bom = GetDepotBOM(railtype);
	return StockpileManager::ConsumeBOM(world, company, bom.materials);
}

bool FabricationManager::CanFabricateVehicle(WorldID world, CompanyID company, const Engine *e)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid() || e == nullptr) return false;
	BillOfMaterials bom = GetVehicleBOM(e);
	return StockpileManager::HasSufficient(world, company, bom.materials);
}

bool FabricationManager::ConsumeVehicleBOM(WorldID world, CompanyID company, const Engine *e)
{
	if (world == INVALID_WORLD || company == CompanyID::Invalid() || e == nullptr) return false;
	BillOfMaterials bom = GetVehicleBOM(e);
	return StockpileManager::ConsumeBOM(world, company, bom.materials);
}

std::map<CompanyID, bool> FabricationManager::GetAllCompanyModes()
{
	std::lock_guard<std::mutex> lock(_fabrication_mutex);
	return _company_fabrication_modes;
}

void FabricationManager::RestoreCompanyMode(CompanyID company, bool enabled)
{
	if (company == CompanyID::Invalid()) return;
	std::lock_guard<std::mutex> lock(_fabrication_mutex);
	_company_fabrication_modes[company] = enabled;
}
