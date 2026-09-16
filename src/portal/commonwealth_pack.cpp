/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file commonwealth_pack.cpp Implementation of Commonwealth economy, rolling-stock pack, and content admission. */

#include "../stdafx.h"
#include "commonwealth_pack.h"
#include "planet_manager.h"
#include "../engine_base.h"

#include <mutex>
#include <sstream>

static std::vector<CSTRollingStockSpec> _cst_specs;
static std::map<uint16_t, size_t> _cst_index_map;
static std::vector<CargoDeliveryLoop> _cargo_loops;
static std::mutex _cst_mutex;
static bool _cst_initialized = false;

void CommonwealthPackManager::Initialize()
{
	std::lock_guard<std::mutex> lock(_cst_mutex);
	if (_cst_initialized) return;

	_cst_specs.clear();
	_cst_index_map.clear();
	_cargo_loops.clear();

	/* 1. Register CST Locomotive Families */
	{
		CSTRollingStockSpec pioneer;
		pioneer.engine_id = EngineID(CST_ENGINE_PIONEER_STEAM);
		pioneer.name = "CST Pioneer 0-6-0 'Surveyor' (Steam)";
		pioneer.engclass = EngineClass::Steam;
		pioneer.tech_required = TECH_NONE; // Baseline unlocked
		pioneer.allowed_phases = {
			WorldPhase::Phase1_Core,
			WorldPhase::Phase2_Developed,
			WorldPhase::Phase3_Frontier,
			WorldPhase::Phase4_Expansion
		};
		pioneer.max_speed_kmh = 72;
		pioneer.power_hp = 600;
		pioneer.weight_tons = 45;
		pioneer.tractive_effort_kn = 90;
		_cst_index_map[CST_ENGINE_PIONEER_STEAM] = _cst_specs.size();
		_cst_specs.push_back(pioneer);
	}

	{
		CSTRollingStockSpec vulcan;
		vulcan.engine_id = EngineID(CST_ENGINE_VULCAN_STEAM);
		vulcan.name = "Vulcan 2-8-0 'Frontier Hauler' (Steam)";
		vulcan.engclass = EngineClass::Steam;
		vulcan.tech_required = TECH_TRACTION_1;
		vulcan.allowed_phases = {
			WorldPhase::Phase3_Frontier,
			WorldPhase::Phase4_Expansion
		};
		vulcan.max_speed_kmh = 96;
		vulcan.power_hp = 1800;
		vulcan.weight_tons = 110;
		vulcan.tractive_effort_kn = 160;
		_cst_index_map[CST_ENGINE_VULCAN_STEAM] = _cst_specs.size();
		_cst_specs.push_back(vulcan);
	}

	{
		CSTRollingStockSpec titan;
		titan.engine_id = EngineID(CST_ENGINE_TITAN_DIESEL);
		titan.name = "Titan D-100 Twin-Engine Hauler (Diesel)";
		titan.engclass = EngineClass::Diesel;
		titan.tech_required = TECH_TRACTION_2;
		titan.allowed_phases = {
			WorldPhase::Phase2_Developed,
			WorldPhase::Phase3_Frontier
		};
		titan.max_speed_kmh = 145;
		titan.power_hp = 4200;
		titan.weight_tons = 180;
		titan.tractive_effort_kn = 420;
		_cst_index_map[CST_ENGINE_TITAN_DIESEL] = _cst_specs.size();
		_cst_specs.push_back(titan);
	}

	{
		CSTRollingStockSpec e40;
		e40.engine_id = EngineID(CST_ENGINE_CST_E40);
		e40.name = "CST E-40 Inter-World Catenary Hauler (Electric)";
		e40.engclass = EngineClass::Electric;
		e40.tech_required = TECH_TRACTION_3;
		e40.allowed_phases = {
			WorldPhase::Phase1_Core,
			WorldPhase::Phase2_Developed
		};
		e40.max_speed_kmh = 225;
		e40.power_hp = 8000;
		e40.weight_tons = 125;
		e40.tractive_effort_kn = 500;
		_cst_index_map[CST_ENGINE_CST_E40] = _cst_specs.size();
		_cst_specs.push_back(e40);
	}

	{
		CSTRollingStockSpec mark4;
		mark4.engine_id = EngineID(CST_ENGINE_MARK4_MAGLEV);
		mark4.name = "CST Mark IV 'Chimaera' Hyper-Maglev (Maglev)";
		mark4.engclass = EngineClass::Maglev;
		mark4.tech_required = TECH_TRACTION_4;
		mark4.allowed_phases = {
			WorldPhase::Phase1_Core
		};
		mark4.max_speed_kmh = 650;
		mark4.power_hp = 20000;
		mark4.weight_tons = 90;
		mark4.tractive_effort_kn = 600;
		_cst_index_map[CST_ENGINE_MARK4_MAGLEV] = _cst_specs.size();
		_cst_specs.push_back(mark4);
	}

	/* 2. Register Closed Delivery Loops for the 13-Cargo Suite */
	_cargo_loops = {
		{
			CommonwealthCargoID::StoneSlag,
			"Silicates & Ballast Slag",
			PipelineType::Structural,
			"Quarries & Slag Heaps",
			{WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion},
			"Heavy Mineral & Ore Hopper",
			"Ballast Crusher Plant / Stockpile In-Kind Ballast",
			"Trackbed ballast, concrete bridge foundations & depot pads"
		},
		{
			CommonwealthCargoID::IronOre,
			"Iron Ore",
			PipelineType::Structural,
			"Deep Iron Ore Mine",
			{WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion},
			"Heavy Mineral & Ore Hopper",
			"Structural Steel Smelter & Mill",
			"Primary metallic feedstock for structural steel refining"
		},
		{
			CommonwealthCargoID::StructuralSteel,
			"Structural Steel",
			PipelineType::Structural,
			"Structural Steel Smelter & Mill",
			{WorldPhase::Phase2_Developed},
			"Structural Steel Flatcar",
			"Planetary Stockpiles & Megacity Tier 2 Expansion",
			"Rails, bridge trusses, depot frames & locomotive boilers"
		},
		{
			CommonwealthCargoID::CopperOre,
			"Copper Ore",
			PipelineType::Electronics,
			"Surface Copper Mine",
			{WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion},
			"Heavy Mineral & Ore Hopper",
			"Conductive Wire Smelter",
			"Primary mineral feedstock for electrical conductors"
		},
		{
			CommonwealthCargoID::ConductiveWiring,
			"Conductive Wiring & Coils",
			PipelineType::Electronics,
			"Conductive Wire Smelter",
			{WorldPhase::Phase2_Developed},
			"Structural Steel Flatcar / Cryo Container",
			"Catenary Electrification & Electronics Fabs",
			"Electrified catenary overhead lines, traction motors & inductive coils"
		},
		{
			CommonwealthCargoID::SilicaSand,
			"Silica Sand & Quartz",
			PipelineType::Electronics,
			"Quartz & Silica Dunes",
			{WorldPhase::Phase2_Developed, WorldPhase::Phase3_Frontier},
			"Heavy Mineral & Ore Hopper",
			"Silicon Chip Arcology & Monocrystal Fabs",
			"Raw crystalline substrate for semiconductor wafers and data crystals"
		},
		{
			CommonwealthCargoID::SiliconChips,
			"Silicon Wafers & Chips",
			PipelineType::Electronics,
			"Silicon Chip Arcology",
			{WorldPhase::Phase1_Core, WorldPhase::Phase2_Developed},
			"Cryogenic Intermodal Container Car",
			"PBS Signals, Vehicle Computers & Megacity Prosperity",
			"Dynamic path signalling logic, train telemetry & Megacity Tier 3"
		},
		{
			CommonwealthCargoID::RareEarthMinerals,
			"Rare Earth Minerals",
			PipelineType::Structural,
			"Rare Earth Extraction Mine",
			{WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion},
			"Heavy Mineral & Ore Hopper",
			"Superalloy Foundry & Monocrystal Fabs",
			"Crystal lattice doping, high-flux magnets & superalloy metallurgy"
		},
		{
			CommonwealthCargoID::Superalloys,
			"Superalloys & Superconductors",
			PipelineType::Structural,
			"High-Temperature Superalloy Foundry",
			{WorldPhase::Phase1_Core, WorldPhase::Phase2_Developed},
			"Cryogenic Intermodal Container Car / Maglev Pod",
			"CST Maglev Guideways, Cryo-Bogies & Megacity Tier 2",
			"High-temperature superconductor rails, vacuum seals & cryo-bogies"
		},
		{
			CommonwealthCargoID::SyntheticComposites,
			"Synthetic Polymers & Composites",
			PipelineType::Propulsion,
			"Synthetic Polymer Complex",
			{WorldPhase::Phase2_Developed},
			"Pressurized Chemical Tanker / Cryo Container",
			"High-Speed Train Fairings & Megacity Tier 2",
			"Aerodynamic train body shells, passenger pods & vacuum hulls"
		},
		{
			CommonwealthCargoID::BlankCrystals,
			"Blank Data Crystals",
			PipelineType::DataCrystals,
			"Monocrystal Substrate Fab",
			{WorldPhase::Phase2_Developed},
			"Data Crystal & Valuables Vault Van",
			"Quantum Observatories & Megacity Formatting Centers",
			"Unformatted optical substrate awaiting mathematical proof enrichment"
		},
		{
			CommonwealthCargoID::EnrichedQuantumCrystals,
			"Enriched Quantum Crystals",
			PipelineType::DataCrystals,
			"Frontier Quantum Telemetry Observatory",
			{WorldPhase::Phase3_Frontier, WorldPhase::Phase4_Expansion},
			"Data Crystal & Valuables Vault Van",
			"Phase 1 Corporate HQ R&D Laboratories",
			"High-value mathematical proofs feeding Tech Tree research advancement"
		},
		{
			CommonwealthCargoID::EncryptedConsumerCrystals,
			"Encrypted Consumer Crystals",
			PipelineType::DataCrystals,
			"Megacity Data Formatting Center",
			{WorldPhase::Phase1_Core, WorldPhase::Phase2_Developed},
			"Data Crystal & Valuables Vault Van",
			"Towns & Megacities across Commonwealth Worlds",
			"High-security encrypted civil correspondence driving Megacity Prosperity"
		}
	};

	_cst_initialized = true;
}

bool CommonwealthPackManager::IsCSTEngine(EngineID eid)
{
	Initialize();
	std::lock_guard<std::mutex> lock(_cst_mutex);
	return _cst_index_map.find(eid.base()) != _cst_index_map.end();
}

const CSTRollingStockSpec *CommonwealthPackManager::GetRollingStockSpec(EngineID eid)
{
	Initialize();
	std::lock_guard<std::mutex> lock(_cst_mutex);
	auto it = _cst_index_map.find(eid.base());
	if (it == _cst_index_map.end()) return nullptr;
	return &_cst_specs[it->second];
}

const std::vector<CSTRollingStockSpec> &CommonwealthPackManager::GetAllCSTRollingStock()
{
	Initialize();
	return _cst_specs;
}

bool CommonwealthPackManager::IsVehicleBuildableForCompany(CompanyID company, EngineID eid, WorldID world)
{
	const Engine *engine = Engine::GetIfValid(eid);
	if (engine == nullptr) return false;
	if (engine->type != VehicleType::Train || engine->grf_prop.grfid != COMMONWEALTH_RAIL_GRFID) return true;

	/* NewGRF local IDs are stable; engine pool IDs depend on the loaded content. */
	static constexpr std::array<uint16_t, 5> families = {
		CST_ENGINE_PIONEER_STEAM, CST_ENGINE_VULCAN_STEAM, CST_ENGINE_TITAN_DIESEL,
		CST_ENGINE_CST_E40, CST_ENGINE_MARK4_MAGLEV,
	};
	uint16_t local_id = engine->grf_prop.local_id;
	if (local_id < 0x20 || local_id >= 0x20 + families.size()) return true;
	return IsRollingStockBuildableForCompany(company, EngineID{families[local_id - 0x20]}, world);
}

bool CommonwealthPackManager::IsRollingStockBuildableForCompany(CompanyID company, EngineID eid, WorldID world)
{
	Initialize();
	const CSTRollingStockSpec *spec = GetRollingStockSpec(eid);
	if (spec == nullptr) {
		/* Non-CST engine: no Commonwealth restrictions */
		return true;
	}

	/* 1. Tech Tree gating */
	if (spec->tech_required != TECH_NONE) {
		if (company != CompanyID::Invalid() && company.base() != 0xFF) {
			if (!TechTreeManager::IsTechUnlocked(company, spec->tech_required)) {
				return false;
			}
		}
	}

	/* 2. World Phase gating */
	if (world != INVALID_WORLD) {
		const PlanetRegion *region = PlanetManager::GetRegion(world);
		if (region != nullptr) {
			if (!spec->allowed_phases.contains(region->phase)) {
				return false;
			}
		}
	}

	return true;
}

BillOfMaterials CommonwealthPackManager::GetVehicleBOM(EngineID eid)
{
	const Engine *e = Engine::GetIfValid(eid);
	if (e != nullptr) {
		return FabricationManager::GetVehicleBOM(e);
	}
	return BillOfMaterials{};
}

const std::vector<CargoDeliveryLoop> &CommonwealthPackManager::GetCargoDeliveryLoops()
{
	Initialize();
	return _cargo_loops;
}

const CargoDeliveryLoop *CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID cargo_id)
{
	Initialize();
	for (const auto &loop : _cargo_loops) {
		if (loop.cargo_id == cargo_id) return &loop;
	}
	return nullptr;
}

bool CommonwealthPackManager::AuditComplete12CargoEconomy(std::vector<std::string> &report)
{
	Initialize();
	report.clear();
	bool all_passed = true;

	if (_cargo_loops.size() < static_cast<size_t>(CommonwealthCargoID::Count)) {
		report.push_back("FAIL: Incomplete cargo registry count.");
		all_passed = false;
	}

	for (const auto &loop : _cargo_loops) {
		std::ostringstream ss;
		ss << "Cargo [" << loop.cargo_name << "]: ";

		bool valid = true;
		if (loop.producer_industry.empty()) {
			ss << "[Missing Producer] ";
			valid = false;
		}
		if (loop.production_phases.empty()) {
			ss << "[Missing Allowed Phases] ";
			valid = false;
		}
		if (loop.transport_wagon_class.empty()) {
			ss << "[Missing Transport Spec] ";
			valid = false;
		}
		if (loop.intermediate_or_consumer.empty()) {
			ss << "[Missing Consumer] ";
			valid = false;
		}
		if (loop.economic_or_rd_role.empty()) {
			ss << "[Missing Economic Role] ";
			valid = false;
		}

		if (valid) {
			ss << "PASSED (Producer: " << loop.producer_industry
			   << " -> Consumer: " << loop.intermediate_or_consumer << ")";
		} else {
			all_passed = false;
		}
		report.push_back(ss.str());
	}

	return all_passed;
}

void CommonwealthPackManager::RegisterPacksInContentManifest(UniverseContentManifest &manifest)
{
	/* Register Industry Pack */
	bool has_ind = false;
	bool has_rail = false;
	for (const auto &grf : manifest.newgrfs) {
		if (grf.grfid == COMMONWEALTH_INDUSTRY_GRFID_BYTES) has_ind = true;
		if (grf.grfid == COMMONWEALTH_RAIL_GRFID_BYTES) has_rail = true;
	}

	if (!has_ind) {
		ContentManifestGRF ind_grf;
		ind_grf.grfid = COMMONWEALTH_INDUSTRY_GRFID_BYTES;
		ind_grf.flags = static_cast<uint8_t>(ContentManifestGRFFlag::Static);
		manifest.newgrfs.push_back(ind_grf);
	}

	if (!has_rail) {
		ContentManifestGRF rail_grf;
		rail_grf.grfid = COMMONWEALTH_RAIL_GRFID_BYTES;
		rail_grf.flags = static_cast<uint8_t>(ContentManifestGRFFlag::Static);
		manifest.newgrfs.push_back(rail_grf);
	}
}

bool CommonwealthPackManager::HasCommonwealthPacks(const UniverseContentManifest &manifest)
{
	bool has_ind = false;
	bool has_rail = false;
	for (const auto &grf : manifest.newgrfs) {
		if (grf.grfid == COMMONWEALTH_INDUSTRY_GRFID_BYTES) has_ind = true;
		if (grf.grfid == COMMONWEALTH_RAIL_GRFID_BYTES) has_rail = true;
	}
	return has_ind && has_rail;
}
