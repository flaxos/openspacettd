/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file production_chain.cpp Commonwealth multi-world production chains and industrial processing facilities. */

#include "../stdafx.h"
#include "production_chain.h"
#include "planet_manager.h"
#include "logistics_hub.h"
#include "tech_tree.h"
#include "../cargotype.h"

#include <algorithm>
#include <map>
#include <vector>

static std::map<RecipeID, ProductionRecipe> _recipes;
static std::map<FacilityID, ProcessingFacility> _facilities;
static FacilityID _next_facility_id = 1;

void ProductionChainManager::Reset()
{
	_recipes.clear();
	_facilities.clear();
	_next_facility_id = 1;
	InitDefaultRecipes();
}

CargoType ProductionChainManager::GetDefaultCargo(CommonwealthCargoID cargo_id)
{
	CargoType c = INVALID_CARGO;
	switch (cargo_id) {
		case CommonwealthCargoID::StoneSlag:
			c = GetCargoTypeByLabel(CT_COAL);
			return (c != INVALID_CARGO) ? c : CargoType{1};
		case CommonwealthCargoID::IronOre:
			c = GetCargoTypeByLabel(CT_IRON_ORE);
			return (c != INVALID_CARGO) ? c : CargoType{8};
		case CommonwealthCargoID::StructuralSteel:
			c = GetCargoTypeByLabel(CT_STEEL);
			return (c != INVALID_CARGO) ? c : CargoType{9};
		case CommonwealthCargoID::CopperOre:
			c = GetCargoTypeByLabel(CT_COPPER_ORE);
			if (c == INVALID_CARGO) c = GetCargoTypeByLabel(CT_IRON_ORE);
			return (c != INVALID_CARGO) ? c : CargoType{4};
		case CommonwealthCargoID::ConductiveWiring:
			c = GetCargoTypeByLabel(CT_GOODS);
			return (c != INVALID_CARGO) ? c : CargoType{5};
		case CommonwealthCargoID::SilicaSand:
			c = GetCargoTypeByLabel(CT_GRAIN);
			return (c != INVALID_CARGO) ? c : CargoType{6};
		case CommonwealthCargoID::SiliconChips:
			c = GetCargoTypeByLabel(CT_VALUABLES);
			if (c == INVALID_CARGO) c = GetCargoTypeByLabel(CT_GOLD);
			return (c != INVALID_CARGO) ? c : CargoType{10};
		case CommonwealthCargoID::RareEarthMinerals:
			c = GetCargoTypeByLabel(CT_GOLD);
			if (c == INVALID_CARGO) c = GetCargoTypeByLabel(CT_DIAMONDS);
			return (c != INVALID_CARGO) ? c : CargoType{7};
		case CommonwealthCargoID::Superalloys:
			c = GetCargoTypeByLabel(CT_STEEL);
			return (c != INVALID_CARGO) ? c : CargoType{9};
		case CommonwealthCargoID::SyntheticComposites:
			c = GetCargoTypeByLabel(CT_GOODS);
			return (c != INVALID_CARGO) ? c : CargoType{5};
		case CommonwealthCargoID::BlankCrystals:
		case CommonwealthCargoID::EnrichedQuantumCrystals:
		case CommonwealthCargoID::EncryptedConsumerCrystals:
			c = GetCargoTypeByLabel(CT_MAIL);
			return (c != INVALID_CARGO) ? c : CargoType{2};
		default:
			return CargoType{0};
	}
}

void ProductionChainManager::InitDefaultRecipes()
{
	CargoType c_stone = GetDefaultCargo(CommonwealthCargoID::StoneSlag);
	CargoType c_iron  = GetDefaultCargo(CommonwealthCargoID::IronOre);
	CargoType c_steel = GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	CargoType c_copper= GetDefaultCargo(CommonwealthCargoID::CopperOre);
	CargoType c_wire  = GetDefaultCargo(CommonwealthCargoID::ConductiveWiring);
	CargoType c_sand  = GetDefaultCargo(CommonwealthCargoID::SilicaSand);
	CargoType c_chips = GetDefaultCargo(CommonwealthCargoID::SiliconChips);
	CargoType c_rare  = GetDefaultCargo(CommonwealthCargoID::RareEarthMinerals);
	CargoType c_alloy = GetDefaultCargo(CommonwealthCargoID::Superalloys);
	CargoType c_comp  = GetDefaultCargo(CommonwealthCargoID::SyntheticComposites);
	CargoType c_blank = GetDefaultCargo(CommonwealthCargoID::BlankCrystals);
	CargoType c_enrich= GetDefaultCargo(CommonwealthCargoID::EnrichedQuantumCrystals);
	CargoType c_mail  = GetDefaultCargo(CommonwealthCargoID::EncryptedConsumerCrystals);

	/* Pipeline A: Structural & Track Infrastructure */
	RegisterRecipe({
		.id = RECIPE_BALLAST_CRUSHING,
		.pipeline = PipelineType::Structural,
		.name = "Crushed Ballast & Concrete",
		.description = "Crushes raw quarry stone and heavy furnace slag into aggregate ballast.",
		.inputs = { {c_stone, 2} },
		.outputs = { {c_stone, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed, WorldPhase::Phase3_Frontier }
	});

	RegisterRecipe({
		.id = RECIPE_STEEL_SMELTING,
		.pipeline = PipelineType::Structural,
		.name = "Blast Furnace Structural Steel",
		.description = "Smelts deep-mined iron ore into heavy structural steel girders and rails.",
		.inputs = { {c_iron, 2} },
		.outputs = { {c_steel, 1} },
		.allowed_phases = { WorldPhase::Phase2_Developed }
	});

	RegisterRecipe({
		.id = RECIPE_SUPERALLOY_FOUNDRY,
		.pipeline = PipelineType::Structural,
		.name = "Superalloy & Superconductor Arcology",
		.description = "Combines structural steel with rare earth lanthanides to cast cryo-magnetic alloys.",
		.inputs = { {c_steel, 2}, {c_rare, 1} },
		.outputs = { {c_alloy, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed }
	});

	/* Pipeline B: Electronics, Signalling & Catenary */
	RegisterRecipe({
		.id = RECIPE_COPPER_SMELTING,
		.pipeline = PipelineType::Electronics,
		.name = "Electrolytic Copper Smelting",
		.description = "Refines raw copper ore into drawn conductive wire and inductive coils.",
		.inputs = { {c_copper, 2} },
		.outputs = { {c_wire, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed }
	});

	RegisterRecipe({
		.id = RECIPE_SILICON_ARC,
		.pipeline = PipelineType::Electronics,
		.name = "High-Purity Silicon Arcology",
		.description = "Refines quartz sand dunes into monocrystalline silicon wafers and microchips.",
		.inputs = { {c_sand, 2} },
		.outputs = { {c_chips, 1} },
		.allowed_phases = { WorldPhase::Phase2_Developed }
	});

	RegisterRecipe({
		.id = RECIPE_SIGNALLING_ASSEMBLY,
		.pipeline = PipelineType::Electronics,
		.name = "Telemetry & PBS Signalling Works",
		.description = "Assembles silicon logic chips and conductive wiring into trackside signalling relays.",
		.inputs = { {c_chips, 1}, {c_wire, 1} },
		.outputs = { {c_chips, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed, WorldPhase::Phase1_Core }
	});

	/* Pipeline C: Advanced Train Propulsion */
	RegisterRecipe({
		.id = RECIPE_POLYMER_SYNTHESIS,
		.pipeline = PipelineType::Propulsion,
		.name = "Petrochemical Polymer Complex",
		.description = "Synthesizes advanced polymers and lightweight composite rolling stock aerobodies.",
		.inputs = { {c_wire, 2} }, // Proxy input (hydrocarbons/goods)
		.outputs = { {c_comp, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed }
	});

	RegisterRecipe({
		.id = RECIPE_MAGLEV_WORKS,
		.pipeline = PipelineType::Propulsion,
		.name = "CST Maglev Propulsion Works",
		.description = "Integrates superconducting superalloys and inductive wiring into maglev bogies and guideways.",
		.inputs = { {c_alloy, 2}, {c_wire, 2} },
		.outputs = { {c_alloy, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed, WorldPhase::Phase1_Core }
	});

	/* Pipeline D: Data Crystals & Scientific R&D */
	RegisterRecipe({
		.id = RECIPE_MONOCRYSTAL_SYNTHESIS,
		.pipeline = PipelineType::DataCrystals,
		.name = "Monocrystalline Synthesis Fab",
		.description = "Synthesizes unformatted blank data crystal matrices from high-purity silica and rare earths.",
		.inputs = { {c_sand, 2}, {c_rare, 1} },
		.outputs = { {c_blank, 2} },
		.allowed_phases = { WorldPhase::Phase2_Developed }
	});

	RegisterRecipe({
		.id = RECIPE_QUANTUM_ENRICHMENT,
		.pipeline = PipelineType::DataCrystals,
		.name = "Deep-Space Quantum Telemetry Array",
		.description = "Imprints blank crystals with frontier cosmological tensors and singularity mathematical proofs.",
		.inputs = { {c_blank, 2} },
		.outputs = { {c_enrich, 2} },
		.allowed_phases = { WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion }
	});

	RegisterRecipe({
		.id = RECIPE_CONSUMER_CRYSTAL_FORMAT,
		.pipeline = PipelineType::DataCrystals,
		.name = "Metropolitan Crystal Formatting",
		.description = "Formats blank data crystals into encrypted human communications for civil transit.",
		.inputs = { {c_blank, 2} },
		.outputs = { {c_mail, 2} },
		.allowed_phases = { WorldPhase::Phase1_Core }
	});
}

void ProductionChainManager::RegisterRecipe(const ProductionRecipe &recipe)
{
	_recipes[recipe.id] = recipe;
}

const ProductionRecipe *ProductionChainManager::GetRecipe(RecipeID id)
{
	auto it = _recipes.find(id);
	return (it != _recipes.end()) ? &it->second : nullptr;
}

std::vector<ProductionRecipe> ProductionChainManager::GetAllRecipes()
{
	std::vector<ProductionRecipe> res;
	res.reserve(_recipes.size());
	for (const auto &[id, r] : _recipes) {
		res.push_back(r);
	}
	return res;
}

std::vector<ProductionRecipe> ProductionChainManager::GetRecipesByPipeline(PipelineType pipeline)
{
	std::vector<ProductionRecipe> res;
	for (const auto &[id, r] : _recipes) {
		if (r.pipeline == pipeline) {
			res.push_back(r);
		}
	}
	return res;
}

bool ProductionChainManager::CanConstructFacility(WorldID world, RecipeID recipe_id, std::string &err_msg)
{
	const ProductionRecipe *rec = GetRecipe(recipe_id);
	if (rec == nullptr) {
		err_msg = "Unknown production recipe";
		return false;
	}

	if (PlanetManager::Count() == 0) {
		return true; // Single-world fallback mode
	}

	const PlanetRegion *reg = PlanetManager::GetRegion(world);
	if (reg == nullptr) {
		err_msg = "World region not found";
		return false;
	}

	WorldPhase phase = reg->phase;
	if (rec->allowed_phases.find(phase) == rec->allowed_phases.end()) {
		err_msg = "World phase does not support this facility type";
		return false;
	}

	return true;
}

FacilityID ProductionChainManager::RegisterFacility(TileIndex tile, WorldID world, RecipeID recipe, CompanyID owner, uint32_t capacity, StationID station)
{
	std::string err;
	if (!CanConstructFacility(world, recipe, err)) {
		return INVALID_FACILITY;
	}

	FacilityID id = _next_facility_id++;
	ProcessingFacility f{
		.id = id,
		.tile = tile,
		.world_id = world,
		.recipe_id = recipe,
		.owner = owner,
		.linked_station = station,
		.monthly_capacity = capacity,
		.last_month_production = 0,
		.total_produced = 0,
		.input_buffers = {},
		.output_buffers = {},
	};

	_facilities[id] = f;
	return id;
}

bool ProductionChainManager::UnregisterFacility(FacilityID id)
{
	return _facilities.erase(id) > 0;
}

ProcessingFacility *ProductionChainManager::GetFacility(FacilityID id)
{
	auto it = _facilities.find(id);
	return (it != _facilities.end()) ? &it->second : nullptr;
}

ProcessingFacility *ProductionChainManager::GetFacilityAtTile(TileIndex tile)
{
	for (auto &[id, f] : _facilities) {
		if (f.tile == tile) return &f;
	}
	return nullptr;
}

std::vector<ProcessingFacility> ProductionChainManager::GetAllFacilities()
{
	std::vector<ProcessingFacility> res;
	res.reserve(_facilities.size());
	for (const auto &[id, f] : _facilities) {
		res.push_back(f);
	}
	return res;
}

void ProductionChainManager::DeliverCargo(FacilityID id, CargoType cargo, uint32_t amount)
{
	ProcessingFacility *f = GetFacility(id);
	if (f != nullptr && amount > 0) {
		f->input_buffers[cargo] += amount;
	}
}

uint32_t ProductionChainManager::WithdrawOutput(FacilityID id, CargoType cargo, uint32_t amount)
{
	ProcessingFacility *f = GetFacility(id);
	if (f == nullptr || amount == 0) return 0;

	auto it = f->output_buffers.find(cargo);
	if (it == f->output_buffers.end() || it->second == 0) return 0;

	uint32_t avail = it->second;
	uint32_t withdrawn = std::min(avail, amount);
	it->second -= withdrawn;
	if (it->second == 0) {
		f->output_buffers.erase(it);
	}
	return withdrawn;
}

void ProductionChainManager::ProcessMonthlyProduction()
{
	for (auto &[id, f] : _facilities) {
		const ProductionRecipe *rec = GetRecipe(f.recipe_id);
		if (rec == nullptr) continue;

		/* Determine max batches possible given available input materials */
		uint32_t max_batches = f.monthly_capacity;
		for (const auto &[in_cargo, in_req] : rec->inputs) {
			auto it = f.input_buffers.find(in_cargo);
			uint32_t avail = (it != f.input_buffers.end()) ? it->second : 0;
			max_batches = std::min(max_batches, avail / in_req);
		}

		if (max_batches == 0) {
			f.last_month_production = 0;
			continue;
		}

		/* Deduct inputs from buffer */
		for (const auto &[in_cargo, in_req] : rec->inputs) {
			f.input_buffers[in_cargo] -= max_batches * in_req;
		}

		/* Calculate output yield: +15% bonus if owner has TECH_MATERIALS_3 */
		uint32_t yield_mult_percent = 100;
		if (f.owner != CompanyID::Invalid() && TechTreeManager::IsTechUnlocked(f.owner, TECH_MATERIALS_3)) {
			yield_mult_percent = 115;
		}

		for (const auto &[out_cargo, out_base] : rec->outputs) {
			uint32_t produced_units = (max_batches * out_base * yield_mult_percent) / 100;

			/* Check if output can automatically buffer into company's planetary Logistics Hub */
			if (f.owner != CompanyID::Invalid() && LogisticsHubManager::HasLogisticsHub(f.world_id, f.owner)) {
				StockpileManager::AddCargo(f.world_id, f.owner, out_cargo, produced_units);
			} else {
				f.output_buffers[out_cargo] += produced_units;
			}
		}

		f.last_month_production = max_batches;
		f.total_produced += max_batches;
	}
}

void ProductionChainManager::RestoreFacility(FacilityID id, TileIndex tile, WorldID world, RecipeID recipe,
                                            CompanyID owner, StationID station, uint32_t capacity,
                                            uint32_t last_prod, uint32_t total_prod,
                                            const std::map<CargoType, uint32_t> &inputs,
                                            const std::map<CargoType, uint32_t> &outputs)
{
	ProcessingFacility f{
		.id = id,
		.tile = tile,
		.world_id = world,
		.recipe_id = recipe,
		.owner = owner,
		.linked_station = station,
		.monthly_capacity = capacity,
		.last_month_production = last_prod,
		.total_produced = total_prod,
		.input_buffers = inputs,
		.output_buffers = outputs,
	};

	_facilities[id] = f;
	if (id >= _next_facility_id) {
		_next_facility_id = id + 1;
	}
}

#include "../safeguards.h"
