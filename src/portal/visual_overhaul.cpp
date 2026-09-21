/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file visual_overhaul.cpp Implementation of Sprint 45 Unified Commonwealth Visual Overhaul Pack. */

#include "../stdafx.h"
#include "visual_overhaul.h"
#include "portal_registry.h"
#include "planet_manager.h"
#include "federation_staging.h"
#include "megacity_manager.h"
#include "commonwealth_pack.h"
#include "../table/sprites.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../company_base.h"
#include "../company_func.h"

#include <mutex>
#include <algorithm>

#include "../safeguards.h"

std::map<TileIndex, MonumentalPortalComponent> VisualOverhaulManager::_monumental_tiles;
std::map<TileIndex, MonumentalPortalLayout> VisualOverhaulManager::_monumental_layouts;
std::vector<PortalParticleEffect> VisualOverhaulManager::_particles;
uint32_t VisualOverhaulManager::_next_particle_id = 0;
std::map<uint32_t, ArcologyTier> VisualOverhaulManager::_arcology_tiers;
std::map<TileIndex, uint32_t> VisualOverhaulManager::_last_transit_tick;

static std::mutex _visual_mutex;

void VisualOverhaulManager::Reset()
{
	std::lock_guard<std::mutex> lock(_visual_mutex);
	_monumental_tiles.clear();
	_monumental_layouts.clear();
	_particles.clear();
	_next_particle_id = 0;
	_arcology_tiers.clear();
	_last_transit_tick.clear();
}

MonumentalPortalLayout VisualOverhaulManager::GenerateMonumentalPortalLayout(TileIndex gate_tile, DiagDirection dir, WorldID world_id)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);

	MonumentalPortalLayout layout;
	layout.gate_tile = gate_tile;
	layout.direction = dir;
	layout.world_id = world_id;

	if (gate_tile == INVALID_TILE) return layout;

	uint gx = TileX(gate_tile);
	uint gy = TileY(gate_tile);

	bool axis_x = (DiagDirToAxis(dir) == Axis::X);

	/* 18-tile footprint: 6 tiles along length (-2 to +3), 3 tiles along width (-1 to +1) */
	layout.tiles.reserve(18);

	for (int8_t l = -2; l <= 3; ++l) {
		for (int8_t w = -1; w <= 1; ++w) {
			int8_t rx = axis_x ? l : w;
			int8_t ry = axis_x ? w : l;

			int tx = static_cast<int>(gx) + rx;
			int ty = static_cast<int>(gy) + ry;

			if (tx < 0 || ty < 0 || static_cast<uint>(tx) >= Map::SizeX() || static_cast<uint>(ty) >= Map::SizeY()) {
				continue;
			}

			TileIndex t = TileXY(tx, ty);
			MonumentalPortalComponent comp = MonumentalPortalComponent::TrackBed;

			if (w == -1) {
				if (l == 0) comp = MonumentalPortalComponent::PylonWest;
				else if (l == -1) comp = MonumentalPortalComponent::ArchCrown;
				else if (l == 1) comp = MonumentalPortalComponent::ApproachCatenary;
				else if (l == 2) comp = MonumentalPortalComponent::SignalingGantry;
				else comp = MonumentalPortalComponent::PowerSubstation;
			} else if (w == 1) {
				if (l == 0) comp = MonumentalPortalComponent::PylonEast;
				else if (l == -1) comp = MonumentalPortalComponent::ArchCrown;
				else if (l == 1) comp = MonumentalPortalComponent::ApproachCatenary;
				else if (l == 2) comp = MonumentalPortalComponent::SignalingGantry;
				else comp = MonumentalPortalComponent::PowerSubstation;
			} else {
				/* Central corridor (w == 0) */
				if (l == 0) comp = MonumentalPortalComponent::EventHorizonCore;
				else if (l == -1) comp = MonumentalPortalComponent::ArchCrown;
				else if (l == 1) comp = MonumentalPortalComponent::ApproachCatenary;
				else if (l == 2) comp = MonumentalPortalComponent::SignalingGantry;
				else comp = MonumentalPortalComponent::TrackBed;
			}

			MonumentalPortalTile pt{
				.tile = t,
				.rel_x = rx,
				.rel_y = ry,
				.component = comp,
			};
			layout.tiles.push_back(pt);
			_monumental_tiles[t] = comp;
		}
	}

	_monumental_layouts[gate_tile] = layout;
	return layout;
}

bool VisualOverhaulManager::IsMonumentalPortalTile(TileIndex tile)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);
	return _monumental_tiles.find(tile) != _monumental_tiles.end();
}

MonumentalPortalComponent VisualOverhaulManager::GetComponentAtTile(TileIndex tile)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);
	auto it = _monumental_tiles.find(tile);
	if (it != _monumental_tiles.end()) return it->second;
	return MonumentalPortalComponent::TrackBed;
}

WormholeVisualState VisualOverhaulManager::GetEventHorizonVisualState(TileIndex gate_tile)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);

	if (!PortalRegistry::IsPortalTile(gate_tile)) {
		if (PortalRegistry::IsUnlinkedGate(gate_tile)) {
			return WormholeVisualState::StandbyAmber;
		}
		return WormholeVisualState::StandbyAmber;
	}

	/* Check if holding condition or quarantine is active */
	if (PortalRegistry::IsInterServerPortal(gate_tile)) {
		const InterServerPortalLink *link = PortalRegistry::GetInterServerPortal(gate_tile);
		if (link != nullptr && link->is_holding_active) {
			return WormholeVisualState::QuarantineRed;
		}
	}

	/* Check if train recently traversed (active transit ripple within 30 ticks) */
	auto it = _last_transit_tick.find(gate_tile);
	if (it != _last_transit_tick.end()) {
		/* Assuming 30 ticks duration of high-intensity quantum transit ripple */
		return WormholeVisualState::TransitRipple;
	}

	return WormholeVisualState::ActiveCyanPulse;
}

uint8_t VisualOverhaulManager::GetEventHorizonAnimationFrame(TileIndex gate_tile, uint32_t tick)
{
	(void)gate_tile;
	/* Deterministic 8-frame cyclic pulse: frame updates every 4 ticks */
	return static_cast<uint8_t>((tick / 4) % 8);
}

void VisualOverhaulManager::OnTrainTraversePortal(TileIndex gate_tile, VehicleID vehicle_id)
{
	(void)vehicle_id;
	std::lock_guard<std::mutex> lock(_visual_mutex);
	_last_transit_tick[gate_tile] = 0; // recorded active
	EmitParticle(gate_tile, PortalParticleType::QuantumRipple, 40, 255);
}

void VisualOverhaulManager::EmitParticle(TileIndex tile, PortalParticleType type, uint32_t duration_ticks, uint8_t intensity)
{
	if (tile == INVALID_TILE) return;

	/* In multi-threaded context, ensure bounded buffer */
	if (_particles.size() >= 512) {
		_particles.erase(_particles.begin());
	}

	PortalParticleEffect effect{
		.id = ++_next_particle_id,
		.origin_tile = tile,
		.type = type,
		.start_tick = 0,
		.duration_ticks = duration_ticks,
		.intensity = intensity,
	};
	_particles.push_back(effect);
}

void VisualOverhaulManager::OnGameTick(uint32_t current_tick)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);

	/* Age and prune particles */
	auto it = std::remove_if(_particles.begin(), _particles.end(), [current_tick](const PortalParticleEffect &p) {
		return current_tick >= p.start_tick + p.duration_ticks;
	});
	_particles.erase(it, _particles.end());

	/* Prune expired transit ripple state */
	for (auto pit = _last_transit_tick.begin(); pit != _last_transit_tick.end();) {
		if (pit->second > 0 && current_tick >= pit->second + 30) {
			pit = _last_transit_tick.erase(pit);
		} else {
			++pit;
		}
	}
}

const std::vector<PortalParticleEffect> &VisualOverhaulManager::GetActiveParticles()
{
	return _particles;
}

size_t VisualOverhaulManager::GetActiveParticleCount()
{
	std::lock_guard<std::mutex> lock(_visual_mutex);
	return _particles.size();
}

const BiomeVisualProfile &VisualOverhaulManager::GetBiomeProfile(WorldBiome biome)
{
	static const BiomeVisualProfile profiles[] = {
		/* Temperate */
		{
			.biome = WorldBiome::Temperate,
			.display_name = "Core Temperate",
			.surface_description = "Lush agricultural terraces, emerald valleys, and manicured civic gardens.",
			.base_ground_palette = PAL_NONE,
			.ambient_particle = PortalParticleType::ArcologyBeacon,
			.has_cracked_salt_flats = false,
			.has_permafrost_shimmer = false,
			.has_basalt_lava_glow = false,
		},
		/* SubArctic */
		{
			.biome = WorldBiome::SubArctic,
			.display_name = "Boreal Glacial Tundra",
			.surface_description = "Crystalline permafrost ice sheets, glacial crevasses, and geothermal vents.",
			.base_ground_palette = PALETTE_TO_STRUCT_BLUE,
			.ambient_particle = PortalParticleType::GeothermalSteam,
			.has_cracked_salt_flats = false,
			.has_permafrost_shimmer = true,
			.has_basalt_lava_glow = false,
		},
		/* SubTropic */
		{
			.biome = WorldBiome::SubTropic,
			.display_name = "Equatorial Savanna",
			.surface_description = "Dense synthetic biospheres, dry savannas, and oasis settlements.",
			.base_ground_palette = PALETTE_TO_STRUCT_YELLOW,
			.ambient_particle = PortalParticleType::IndustrialHaze,
			.has_cracked_salt_flats = false,
			.has_permafrost_shimmer = false,
			.has_basalt_lava_glow = false,
		},
		/* AridDesert */
		{
			.biome = WorldBiome::AridDesert,
			.display_name = "Arid Rust Frontier",
			.surface_description = "Oxidized red sand dunes, cracked salt flats, and industrial dust haze.",
			.base_ground_palette = PALETTE_TO_STRUCT_YELLOW,
			.ambient_particle = PortalParticleType::IndustrialHaze,
			.has_cracked_salt_flats = true,
			.has_permafrost_shimmer = false,
			.has_basalt_lava_glow = false,
		},
		/* Volcanic */
		{
			.biome = WorldBiome::Volcanic,
			.display_name = "Volcanic Barren",
			.surface_description = "Dark basalt flows, obsidian ridges, and degassing sulfur plumes.",
			.base_ground_palette = PALETTE_CRASH,
			.ambient_particle = PortalParticleType::SulfurPlume,
			.has_cracked_salt_flats = false,
			.has_permafrost_shimmer = false,
			.has_basalt_lava_glow = true,
		},
		/* Oceanic */
		{
			.biome = WorldBiome::Oceanic,
			.display_name = "Pelagic Archipelago",
			.surface_description = "Deep cobalt waters, floating aquaculture rigs, and archipelagic atolls.",
			.base_ground_palette = PAL_NONE,
			.ambient_particle = PortalParticleType::GeothermalSteam,
			.has_cracked_salt_flats = false,
			.has_permafrost_shimmer = false,
			.has_basalt_lava_glow = false,
		},
	};

	uint idx = static_cast<uint>(biome);
	if (idx >= 6) idx = 0;
	return profiles[idx];
}

uint16_t VisualOverhaulManager::GetBiomeTerrainPalette(WorldBiome biome)
{
	return GetBiomeProfile(biome).base_ground_palette;
}

bool VisualOverhaulManager::ShouldRenderPermafrostShimmer(TileIndex tile)
{
	if (!IsValidTile(tile)) return false;
	return PlanetManager::GetTileBiome(tile) == WorldBiome::SubArctic && IsSnowTile(tile);
}

bool VisualOverhaulManager::ShouldRenderBasaltGlow(TileIndex tile)
{
	if (!IsValidTile(tile)) return false;
	return PlanetManager::GetTileBiome(tile) == WorldBiome::Volcanic;
}

ArcologyTier VisualOverhaulManager::GetArcologyTier(TownID town_id)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);
	auto it = _arcology_tiers.find(town_id.base());
	if (it != _arcology_tiers.end()) return it->second;
	return ArcologyTier::None;
}

void VisualOverhaulManager::SetArcologyTier(TownID town_id, ArcologyTier tier)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);
	_arcology_tiers[town_id.base()] = tier;
}

const std::map<uint32_t, ArcologyTier> &VisualOverhaulManager::GetAllArcologyTiers()
{
	return _arcology_tiers;
}

void VisualOverhaulManager::UpdateArcologyEvolution(TownID town_id, float overall_satisfaction_index, uint32_t population)
{
	std::lock_guard<std::mutex> lock(_visual_mutex);

	ArcologyTier tier = ArcologyTier::None;

	if (population >= 1500 && overall_satisfaction_index >= 0.90f) {
		tier = ArcologyTier::Tier3_CommonwealthCitadel;
	} else if (population >= 1000 && overall_satisfaction_index >= 0.70f) {
		tier = ArcologyTier::Tier2_InterconnectedGrid;
	} else if (population >= 500 && overall_satisfaction_index >= 0.50f) {
		tier = ArcologyTier::Tier1_SpireFoundation;
	}

	_arcology_tiers[town_id.base()] = tier;
}

uint16_t VisualOverhaulManager::GetArcologyBuildingPalette(TownID town_id, HouseID house_id)
{
	(void)house_id;
	ArcologyTier tier = GetArcologyTier(town_id);
	switch (tier) {
		case ArcologyTier::Tier3_CommonwealthCitadel:
			return PALETTE_TO_STRUCT_BLUE; // Gleaming cyan-blue citadel excitation
		case ArcologyTier::Tier2_InterconnectedGrid:
			return PALETTE_TO_STRUCT_YELLOW; // Amber illuminated skyway corridors
		case ArcologyTier::Tier1_SpireFoundation:
		case ArcologyTier::None:
		default:
			return PAL_NONE;
	}
}

RollingStockVisualProfile VisualOverhaulManager::GetRollingStockProfile(EngineID engine_id)
{
	RollingStockVisualProfile prof;
	prof.engine_id = engine_id;
	prof.default_livery = FleetLiveryTheme::CST_EmeraldGold;
	prof.length_in_eighths = 8;
	prof.has_pantograph = false;
	prof.is_hyper_maglev = false;
	prof.is_armored_containment = false;

	uint16_t id = engine_id.base();
	switch (id) {
		case CST_ENGINE_PIONEER_STEAM:
			prof.model_name = "CST Pioneer 0-6-0 'Surveyor'";
			prof.visual_class_label = "Pioneer Steam Shunter";
			break;
		case CST_ENGINE_VULCAN_STEAM:
			prof.model_name = "Vulcan 2-8-0 'Frontier Hauler'";
			prof.visual_class_label = "Heavy Steam Freight";
			break;
		case CST_ENGINE_TITAN_DIESEL:
			prof.model_name = "Titan D-100 Twin-Engine Hauler";
			prof.visual_class_label = "10,000-hp Heavy Diesel Hauler";
			break;
		case CST_ENGINE_CST_E40:
			prof.model_name = "CST E-40 Inter-World Catenary Hauler";
			prof.has_pantograph = true;
			prof.visual_class_label = "High-Power Electric Freight";
			break;
		case CST_ENGINE_MARK4_MAGLEV:
			prof.model_name = "CST Mark IV 'Chimaera' Hyper-Maglev";
			prof.is_hyper_maglev = true;
			prof.visual_class_label = "400 km/h Vacuum Hyper-Maglev";
			break;
		default:
			prof.model_name = "Commonwealth Standard Unit";
			prof.visual_class_label = "Standard Rolling Stock";
			break;
	}

	return prof;
}

FleetLiveryTheme VisualOverhaulManager::GetCompanyFleetLivery(CompanyID company_id)
{
	Company *c = Company::GetIfValid(company_id);
	if (c != nullptr) {
		if (c->name.find("Commonwealth Synergy") != std::string::npos) {
			return FleetLiveryTheme::CST_EmeraldGold;
		}
		if (c->name.find("Grand Central") != std::string::npos) {
			return FleetLiveryTheme::GrandCentral_NavySilver;
		}
		if (c->name.find("InterWorld") != std::string::npos) {
			return FleetLiveryTheme::InterWorld_HazardYellow;
		}
	}

	switch (company_id.base() % 3) {
		case 1: return FleetLiveryTheme::CST_EmeraldGold;
		case 2: return FleetLiveryTheme::GrandCentral_NavySilver;
		default: return FleetLiveryTheme::InterWorld_HazardYellow;
	}
}

std::string VisualOverhaulManager::GetCargoWagonVisualClass(CommonwealthCargoID cargo_id)
{
	switch (cargo_id) {
		case CommonwealthCargoID::BlankCrystals:
		case CommonwealthCargoID::EnrichedQuantumCrystals:
		case CommonwealthCargoID::EncryptedConsumerCrystals:
			return "Armored Quantum Containment Van";

		case CommonwealthCargoID::SyntheticComposites:
			return "Cryogenic Pressurized Tanker";

		case CommonwealthCargoID::StoneSlag:
		case CommonwealthCargoID::IronOre:
		case CommonwealthCargoID::CopperOre:
		case CommonwealthCargoID::SilicaSand:
		case CommonwealthCargoID::RareEarthMinerals:
			return "Reinforced Triple-Axle Mineral Hopper";

		case CommonwealthCargoID::StructuralSteel:
		case CommonwealthCargoID::ConductiveWiring:
		case CommonwealthCargoID::Superalloys:
			return "Heavy Industrial Flatbed Wagon";

		case CommonwealthCargoID::SiliconChips:
			return "High-Security Sealed Cargo Van";

		default:
			return "Standard Box Wagon";
	}
}
