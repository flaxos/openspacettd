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

/** Runtime content state. Configured but incomplete packs must never use vanilla aliases. */
enum class CommonwealthContentMode : uint8_t { Vanilla, Active, Invalid };

struct CommonwealthContentStatus {
	CommonwealthContentMode mode = CommonwealthContentMode::Vanilla;
	std::string reason;
};

/** Catalog family identifiers. These are not loaded engine pool IDs or NewGRF local IDs. */
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
 * content admission, and 13-cargo closed delivery loops.
 */
class CommonwealthPackManager {
public:
	static void Initialize();
	static CommonwealthContentStatus GetContentStatus();
	static CargoLabel GetCargoLabel(CommonwealthCargoID cargo);
	static StringID GetVehicleAvailabilityError(CompanyID company, EngineID eid, WorldID world);

	/** Query whether a catalog ID corresponds to a registered CST locomotive family. */
	static bool IsCSTEngine(EngineID eid);

	/** Retrieve a CST locomotive specification by catalog family ID. */
	static const CSTRollingStockSpec *GetRollingStockSpec(EngineID eid);

	/** Retrieve all registered CST locomotive specifications. */
	static const std::vector<CSTRollingStockSpec> &GetAllCSTRollingStock();

	/** Check a loaded engine using its NewGRF identity and local ID (Tech Tree & Phase gating). */
	static bool IsVehicleBuildableForCompany(CompanyID company, EngineID eid, WorldID world);

	/** Evaluate a catalog family, independently of loaded engine pool IDs. */
	static bool IsRollingStockBuildableForCompany(CompanyID company, EngineID family, WorldID world);

	/** Query physical Bill of Materials for fabricating this CST vehicle. */
	static BillOfMaterials GetVehicleBOM(EngineID eid);

	/** Retrieve all closed delivery loops for the 13-cargo Commonwealth suite. */
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
