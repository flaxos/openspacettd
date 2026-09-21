/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file visual_overhaul.h Sprint 45 Unified Commonwealth Visual Overhaul Pack architecture. */

#ifndef VISUAL_OVERHAUL_H
#define VISUAL_OVERHAUL_H

#include "../tile_type.h"
#include "../direction_type.h"
#include "../company_type.h"
#include "../engine_type.h"
#include "../vehicle_type.h"
#include "../town_type.h"
#include "../house_type.h"
#include "planet_type.h"
#include "portal_type.h"
#include "production_chain.h"

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <set>

/** Components of an 18-tile monumental portal arch terminal complex. */
enum class MonumentalPortalComponent : uint8_t {
	PylonWest        = 0, ///< West reinforced composite foundation pylon.
	PylonEast        = 1, ///< East reinforced composite foundation pylon.
	ArchCrown        = 2, ///< Upper monumental arch overhead beam.
	EventHorizonCore = 3, ///< Active wormhole singularity / event horizon boundary.
	ApproachCatenary = 4, ///< High-clearance electrification overhead catenary gantry.
	SignalingGantry  = 5, ///< Integrated optical corridor signaling and PBS gantry.
	PowerSubstation  = 6, ///< Superconductor magnetic confinement substation.
	TrackBed         = 7, ///< Reinforced ballast dual-track portal approach bed.
};

/** Tile layout descriptor for a single tile within an 18-tile monumental portal footprint. */
struct MonumentalPortalTile {
	TileIndex tile = INVALID_TILE;
	int8_t rel_x = 0;   ///< Offset relative to portal origin.
	int8_t rel_y = 0;   ///< Offset relative to portal origin.
	MonumentalPortalComponent component = MonumentalPortalComponent::TrackBed;
};

/** Complete 18-tile footprint geometry and orientation for a monumental portal complex. */
struct MonumentalPortalLayout {
	TileIndex gate_tile = INVALID_TILE;
	DiagDirection direction = DiagDirection::NE;
	WorldID world_id = INVALID_WORLD;
	std::vector<MonumentalPortalTile> tiles;

	bool IsValid() const { return gate_tile != INVALID_TILE && tiles.size() == 18; }
};

/** Operational excitation and shimmer state of a portal's wormhole event horizon. */
enum class WormholeVisualState : uint8_t {
	StandbyAmber     = 0, ///< Unlinked gate or holding state (amber/yellow standby excitation).
	ActiveCyanPulse  = 1, ///< Linked active gate with stable quantum conduit (cyan-blue pulse).
	TransitRipple    = 2, ///< Active train transit in progress (intense white/cyan shimmering waves).
	QuarantineRed    = 3, ///< Siding divert or corridor quarantine active (pulsing crimson alert).
};

/** Particle effect types emitted across planetary worlds and industrial corridors. */
enum class PortalParticleType : uint8_t {
	QuantumRipple    = 0, ///< Expanding energetic rings emitted when a train enters/exits wormhole.
	GeothermalSteam  = 1, ///< Crystalline thermal plumes on Boreal Glacial Tundra worlds.
	SulfurPlume      = 2, ///< Volcanic degassing plumes on Volcanic Barren worlds.
	IndustrialHaze   = 3, ///< Atmospheric dust haze and slag furnace exhaust on Arid Rust worlds.
	ArcologyBeacon   = 4, ///< High-intensity navigation beacon atop Tier 3 Arcology Citadels.
};

/** An active runtime particle emitter record. */
struct PortalParticleEffect {
	uint32_t id = 0;
	TileIndex origin_tile = INVALID_TILE;
	PortalParticleType type = PortalParticleType::QuantumRipple;
	uint32_t start_tick = 0;
	uint32_t duration_ticks = 60;
	uint8_t intensity = 255;
};

/** Visual profile attributes for distinct planetary biomes. */
struct BiomeVisualProfile {
	WorldBiome biome = WorldBiome::Temperate;
	std::string display_name;
	std::string surface_description;
	uint16_t base_ground_palette = 0;
	PortalParticleType ambient_particle = PortalParticleType::ArcologyBeacon;
	bool has_cracked_salt_flats = false;
	bool has_permafrost_shimmer = false;
	bool has_basalt_lava_glow = false;
};

/** Arcology urban evolution stages for metropolitan core worlds. */
enum class ArcologyTier : uint8_t {
	None                     = 0, ///< Standard municipal town houses.
	Tier1_SpireFoundation    = 1, ///< Foundation structural pylons, heavy cranes, high-tech substructures.
	Tier2_InterconnectedGrid = 2, ///< Multi-level interconnected arcology blocks with glass skybridges.
	Tier3_CommonwealthCitadel= 3, ///< Colossal gleaming Commonwealth Arcology Citadel with integrated rail terminals.
};

/** Visual livery theme for Commonwealth corporate rolling stock. */
enum class FleetLiveryTheme : uint8_t {
	CST_EmeraldGold          = 0, ///< Commonwealth Synergy Transport: Dark Green with Imperial Gold trim.
	GrandCentral_NavySilver  = 1, ///< Grand Central Trans-Portal: Deep Navy with Polished Silver trim.
	InterWorld_HazardYellow  = 2, ///< InterWorld Logistics: Industrial Hazard Yellow & Matte Charcoal.
	CommonwealthStandard     = 3, ///< Neutral / Federated Common Carrier livery.
};

/** Visual specification and model profile for rolling stock units. */
struct RollingStockVisualProfile {
	EngineID engine_id = EngineID::Invalid();
	std::string model_name;
	FleetLiveryTheme default_livery = FleetLiveryTheme::CST_EmeraldGold;
	uint16_t length_in_eighths = 8;
	bool has_pantograph = false;
	bool is_hyper_maglev = false;
	bool is_armored_containment = false;
	std::string visual_class_label;
};

/**
 * Authoritative manager for the Sprint 45 Unified Commonwealth Visual Overhaul Pack.
 *
 * Implements monumental 18-tile portal architecture, deterministic event horizon
 * animation cycles, particle emission, planetary biome styling, Megacity arcology
 * evolution, and rolling stock livery classification.
 */
class VisualOverhaulManager {
public:
	static void Reset();

	/* Monumental Portal Architecture */
	static MonumentalPortalLayout GenerateMonumentalPortalLayout(TileIndex gate_tile, DiagDirection dir, WorldID world_id);
	static bool IsMonumentalPortalTile(TileIndex tile);
	static MonumentalPortalComponent GetComponentAtTile(TileIndex tile);
	static WormholeVisualState GetEventHorizonVisualState(TileIndex gate_tile);
	static uint8_t GetEventHorizonAnimationFrame(TileIndex gate_tile, uint32_t tick);
	static void OnTrainTraversePortal(TileIndex gate_tile, VehicleID vehicle_id);

	/* Particle Effects & Tick Engine */
	static void EmitParticle(TileIndex tile, PortalParticleType type, uint32_t duration_ticks = 60, uint8_t intensity = 255);
	static void OnGameTick(uint32_t current_tick);
	static const std::vector<PortalParticleEffect> &GetActiveParticles();
	static size_t GetActiveParticleCount();

	/* Planetary Biomes */
	static const BiomeVisualProfile &GetBiomeProfile(WorldBiome biome);
	static uint16_t GetBiomeTerrainPalette(WorldBiome biome);
	static bool ShouldRenderPermafrostShimmer(TileIndex tile);
	static bool ShouldRenderBasaltGlow(TileIndex tile);

	/* Megacity Arcology Urban Evolution */
	static ArcologyTier GetArcologyTier(TownID town_id);
	static void UpdateArcologyEvolution(TownID town_id, float overall_satisfaction_index, uint32_t population);
	static uint16_t GetArcologyBuildingPalette(TownID town_id, HouseID house_id);

	/* Custom Rolling Stock Fleet */
	static RollingStockVisualProfile GetRollingStockProfile(EngineID engine_id);
	static FleetLiveryTheme GetCompanyFleetLivery(CompanyID company_id);
	static std::string GetCargoWagonVisualClass(CommonwealthCargoID cargo_id);

	/* Save/Load helpers */
	static const std::map<uint32_t, ArcologyTier> &GetAllArcologyTiers();
	static void SetArcologyTier(TownID town_id, ArcologyTier tier);

private:
	static std::map<TileIndex, MonumentalPortalComponent> _monumental_tiles;
	static std::map<TileIndex, MonumentalPortalLayout> _monumental_layouts;
	static std::vector<PortalParticleEffect> _particles;
	static uint32_t _next_particle_id;
	static std::map<uint32_t, ArcologyTier> _arcology_tiers;
	static std::map<TileIndex, uint32_t> _last_transit_tick;
};

#endif /* VISUAL_OVERHAUL_H */
