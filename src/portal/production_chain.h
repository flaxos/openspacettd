/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file production_chain.h Factorio-scale multi-world production chains and industrial processing facilities. */

#ifndef PRODUCTION_CHAIN_H
#define PRODUCTION_CHAIN_H

#include "../cargo_type.h"
#include "../station_type.h"
#include "../company_type.h"
#include "portal_type.h"
#include "planet_type.h"
#include "company_stockpile.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

/** Canonical Commonwealth Cargo IDs across the 12-cargo industrial suite. */
enum class CommonwealthCargoID : uint8_t {
	StoneSlag                 = 0,  ///< Raw bulk: Stone, slag, trackbed ballast.
	IronOre                   = 1,  ///< Raw bulk: Deep-mined iron ore.
	StructuralSteel           = 2,  ///< Refined metal: Structural steel, bridge girders, rails.
	CopperOre                 = 3,  ///< Raw mineral: Copper ore.
	ConductiveWiring          = 4,  ///< Electrical intermediate: Drawn copper wire, inductive coils.
	SilicaSand                = 5,  ///< Raw mineral: Quartz and silica sand dunes.
	SiliconChips              = 6,  ///< High-tech component: Silicon wafers, microchips, signal logic.
	RareEarthMinerals         = 7,  ///< Exotic raw: Lanthanide, neodymium, and crystal-doping minerals.
	Superalloys               = 8,  ///< Advanced material: High-temperature superconductors, cryo-bogies.
	SyntheticComposites       = 9,  ///< Advanced material: Aerodynamic polymer body shells.
	BlankCrystals             = 10, ///< Precision intermediate: Unformatted monocrystalline storage substrate.
	EnrichedQuantumCrystals   = 11, ///< Scientific feedstock: Quantum observatory mathematical proofs.
	EncryptedConsumerCrystals = 12, ///< Civil express freight: High-security encrypted human mail.
	Count                     = 13,
};

/** The Four Interlocking Production Pipelines. */
enum class PipelineType : uint8_t {
	Structural = 0, ///< Pipeline A: Stone/Slag, Iron Ore, Steel, Superalloys.
	Electronics= 1, ///< Pipeline B: Copper, Silica Sand, Chips, Signalling logic.
	Propulsion = 2, ///< Pipeline C: Hydrocarbons, Synthetic Polymers, Maglev guideways.
	DataCrystals=3, ///< Pipeline D: Monocrystal synthesis, Quantum enrichment, Consumer formatting.
	Count      = 4,
};

using RecipeID = uint16_t;
static constexpr RecipeID RECIPE_NONE                     = 0;

/* Pipeline A: Structural & Track Infrastructure */
static constexpr RecipeID RECIPE_BALLAST_CRUSHING         = 101; ///< 2 Stone/Slag -> 2 Ballast & Concrete
static constexpr RecipeID RECIPE_STEEL_SMELTING           = 102; ///< 2 Iron Ore -> 1 Structural Steel
static constexpr RecipeID RECIPE_SUPERALLOY_FOUNDRY       = 103; ///< 2 Structural Steel + 1 Rare Earth Minerals -> 2 Superalloys

/* Pipeline B: Electronics, Signalling & Catenary */
static constexpr RecipeID RECIPE_COPPER_SMELTING          = 201; ///< 2 Copper Ore -> 2 Conductive Wiring
static constexpr RecipeID RECIPE_SILICON_ARC              = 202; ///< 2 Silica Sand -> 1 Silicon Chips
static constexpr RecipeID RECIPE_SIGNALLING_ASSEMBLY      = 203; ///< 1 Silicon Chips + 1 Conductive Wiring -> 2 Telemetry & Signalling Logic

/* Pipeline C: Advanced Train Propulsion */
static constexpr RecipeID RECIPE_POLYMER_SYNTHESIS        = 301; ///< 2 Hydrocarbons -> 2 Synthetic Composites
static constexpr RecipeID RECIPE_MAGLEV_WORKS             = 302; ///< 2 Superalloys + 2 Conductive Wiring -> 2 Cryo-Bogies & Guideways

/* Pipeline D: Data Crystals & Scientific R&D */
static constexpr RecipeID RECIPE_MONOCRYSTAL_SYNTHESIS    = 401; ///< 2 Silica Sand + 1 Rare Earth Minerals -> 2 Blank Data Crystals
static constexpr RecipeID RECIPE_QUANTUM_ENRICHMENT       = 402; ///< 2 Blank Data Crystals -> 2 Enriched Quantum Crystals (Observatory telemetry)
static constexpr RecipeID RECIPE_CONSUMER_CRYSTAL_FORMAT  = 403; ///< 2 Blank Data Crystals -> 2 Encrypted Consumer Crystals (Megacity formatting)

/** Specification for a production recipe. */
struct ProductionRecipe {
	RecipeID id = RECIPE_NONE;
	PipelineType pipeline = PipelineType::Structural;
	std::string name;
	std::string description;
	std::vector<std::pair<CargoType, uint32_t>> inputs;
	std::vector<std::pair<CargoType, uint32_t>> outputs;

	/* World Phase rules: bitmask or allowed phase set */
	std::set<WorldPhase> allowed_phases;
};

using FacilityID = uint32_t;
static constexpr FacilityID INVALID_FACILITY = 0;

/** Runtime state of a registered industrial processing facility. */
struct ProcessingFacility {
	FacilityID id = INVALID_FACILITY;
	TileIndex tile = INVALID_TILE;
	WorldID world_id = INVALID_WORLD;
	RecipeID recipe_id = RECIPE_NONE;
	CompanyID owner = CompanyID::Invalid();
	StationID linked_station = StationID::Invalid();

	uint32_t monthly_capacity = 100;
	uint32_t last_month_production = 0;
	uint32_t total_produced = 0;

	std::map<CargoType, uint32_t> input_buffers;
	std::map<CargoType, uint32_t> output_buffers;
};

/** Manager for Commonwealth multi-world industrial production chains. */
class ProductionChainManager {
public:
	static void Reset();
	static void InitDefaultRecipes();

	/* Recipe catalog */
	static void RegisterRecipe(const ProductionRecipe &recipe);
	static const ProductionRecipe *GetRecipe(RecipeID id);
	static std::vector<ProductionRecipe> GetAllRecipes();
	static std::vector<ProductionRecipe> GetRecipesByPipeline(PipelineType pipeline);

	/* Facility lifecycle */
	static FacilityID RegisterFacility(TileIndex tile, WorldID world, RecipeID recipe, CompanyID owner, uint32_t capacity = 100, StationID station = StationID::Invalid());
	static bool UnregisterFacility(FacilityID id);
	static ProcessingFacility *GetFacility(FacilityID id);
	static ProcessingFacility *GetFacilityAtTile(TileIndex tile);
	static ProcessingFacility *GetFacilityForStation(StationID station);
	/** Retire a station attachment and salvage buffered material to its owner's stockpile. */
	static void RemoveForStation(StationID station);
	static void ChangeCompanyOwner(CompanyID old_owner, CompanyID new_owner);
	static uint32_t DeliverToStation(StationID station, CargoType cargo, uint32_t amount);
	static bool AcceptsCargo(StationID station, CargoType cargo);
	/** Publish buffered output as ordinary waiting station cargo, retaining it if the packet pool is full. */
	static void PublishStationOutput(ProcessingFacility &facility);
	static std::vector<ProcessingFacility> GetAllFacilities();

	/* Cargo delivery & buffering */
	static void DeliverCargo(FacilityID id, CargoType cargo, uint32_t amount);
	static uint32_t WithdrawOutput(FacilityID id, CargoType cargo, uint32_t amount);

	/* Monthly simulation tick */
	static void ProcessMonthlyProduction();

	/* Verification & validation helpers */
	static bool CanConstructFacility(WorldID world, RecipeID recipe, std::string &err_msg);
	static CargoType GetDefaultCargo(CommonwealthCargoID cargo_id);

	/* Save/Load restoration helpers */
	static void RestoreFacility(FacilityID id, TileIndex tile, WorldID world, RecipeID recipe,
	                            CompanyID owner, StationID station, uint32_t capacity,
	                            uint32_t last_prod, uint32_t total_prod,
	                            const std::map<CargoType, uint32_t> &inputs,
	                            const std::map<CargoType, uint32_t> &outputs);
};

#endif /* PRODUCTION_CHAIN_H */
