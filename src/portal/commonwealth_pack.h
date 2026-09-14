/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file commonwealth_pack.h Canonical Commonwealth economy, rolling-stock pack, and content admission. */

#ifndef COMMONWEALTH_PACK_H
#define COMMONWEALTH_PACK_H

#include "../engine_type.h"
#include "../company_type.h"
#include "../newgrf_type.h"
#include "tech_tree.h"
#include "planet_type.h"
#include "production_chain.h"
#include "content_manifest.h"
#include "fabrication_manager.h"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

/** Canonical Commonwealth GRF Identifiers. */
static constexpr std::array<uint8_t, 4> COMMONWEALTH_INDUSTRY_GRFID_BYTES = {'O', 'S', 'T', 0x01};
static constexpr std::array<uint8_t, 4> COMMONWEALTH_RAIL_GRFID_BYTES     = {'O', 'S', 'T', 0x02};

static const GrfID COMMONWEALTH_INDUSTRY_GRFID{"OST\x01"};
static const GrfID COMMONWEALTH_RAIL_GRFID    {"OST\x02"};

/** Canonical CST Locomotive Indices in standard OpenTTD train pool. */
static constexpr uint16_t CST_ENGINE_PIONEER_STEAM = 0;   ///< CST Pioneer 0-6-0 'Surveyor'
static constexpr uint16_t CST_ENGINE_VULCAN_STEAM  = 7;   ///< Vulcan 2-8-0 'Frontier Hauler'
static constexpr uint16_t CST_ENGINE_TITAN_DIESEL  = 18;  ///< Titan D-100 Twin-Engine Hauler
static constexpr uint16_t CST_ENGINE_CST_E40       = 24;  ///< CST E-40 Inter-World Catenary Hauler
static constexpr uint16_t CST_ENGINE_MARK4_MAGLEV  = 87;  ///< CST Mark IV 'Chimaera' Hyper-Maglev

/** Complete operational specification of a Commonwealth CST locomotive family. */
struct CSTRollingStockSpec {
	EngineID engine_id = EngineID::Invalid();
	std::string name;
	EngineClass engclass = EngineClass::Steam;
	TechID tech_required = TECH_NONE;
	std::set<WorldPhase> allowed_phases;
	uint16_t max_speed_kmh = 0;
	uint16_t power_hp = 0;
	uint16_t weight_tons = 0;
	uint16_t tractive_effort_kn = 0;
};

/** Complete specification of a closed delivery loop for a Commonwealth cargo. */
struct CargoDeliveryLoop {
	CommonwealthCargoID cargo_id = CommonwealthCargoID::StoneSlag;
	std::string cargo_name;
	PipelineType pipeline = PipelineType::Structural;
	std::string producer_industry;
	std::set<WorldPhase> production_phases;
	std::string transport_wagon_class;
	std::string intermediate_or_consumer;
	std::string economic_or_rd_role;
};

/**
 * Central manager for Commonwealth in-tree content packages, vehicle gating,
 * content admission, and 12-cargo closed delivery loops.
 */
class CommonwealthPackManager {
public:
	static void Initialize();

	/** Query whether an engine ID corresponds to a registered CST locomotive family. */
	static bool IsCSTEngine(EngineID eid);

	/** Retrieve full specification of a CST locomotive. */
	static const CSTRollingStockSpec *GetRollingStockSpec(EngineID eid);

	/** Retrieve all registered CST locomotive specifications. */
	static const std::vector<CSTRollingStockSpec> &GetAllCSTRollingStock();

	/** Check whether a company can construct a given vehicle on a given world (Tech Tree & Phase gating). */
	static bool IsVehicleBuildableForCompany(CompanyID company, EngineID eid, WorldID world);

	/** Query physical Bill of Materials for fabricating this CST vehicle. */
	static BillOfMaterials GetVehicleBOM(EngineID eid);

	/** Retrieve all closed delivery loops for the 12-cargo Commonwealth suite. */
	static const std::vector<CargoDeliveryLoop> &GetCargoDeliveryLoops();

	/** Retrieve delivery loop for a specific cargo. */
	static const CargoDeliveryLoop *GetCargoDeliveryLoop(CommonwealthCargoID cargo_id);

	/**
	 * Systematically audit that every Commonwealth cargo has a valid producing,
	 * transporting, and consuming role in a closed delivery loop.
	 */
	static bool AuditComplete12CargoEconomy(std::vector<std::string> &report);

	/** Register Commonwealth Industry and Rail packages into a UniverseContentManifest. */
	static void RegisterPacksInContentManifest(UniverseContentManifest &manifest);

	/** Check whether a UniverseContentManifest includes both Commonwealth packages. */
	static bool HasCommonwealthPacks(const UniverseContentManifest &manifest);
};

#endif /* COMMONWEALTH_PACK_H */
