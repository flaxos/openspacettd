/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_sprint45_visual_overhaul.cpp Unit tests for Sprint 45 Unified Commonwealth Visual Overhaul Pack. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/visual_overhaul.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/production_chain.h"
#include "../portal/commonwealth_pack.h"
#include "../newgrf_station.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../station_base.h"
#include "../rail_map.h"
#include "../rail.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../signal_func.h"
#include "../depot_base.h"
#include "../vehicle_base.h"
#include "../town.h"
#include "../town_kdtree.h"
#include "../core/pool_type.hpp"
#include "../linkgraph/linkgraphschedule.h"
#include "../map_func.h"
#include "../saveload/saveload.h"
#include "../saveload/saveload_func.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../economy_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "mock_environment.h"

#include <filesystem>
#include <vector>
#include <algorithm>

#include "../safeguards.h"

static void SetupTestEnvironment(uint32_t map_w = 256, uint32_t map_h = 256)
{
	UpdateSignalsInBuffer();
	Map::Allocate(map_w, map_h);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}

	ResetRailTypes();
	StationClass::Reset();
	LinkGraphSchedule::Clear();
	PoolBase::Clean(PoolType::Normal);

	if (Town::CanAllocateItem()) {
		Town *t = Town::Create(TileXY(10, 10));
		t->name = "Visual Overhaul Town";
		t->townnametype = SPECSTR_TOWNNAME_START;
		RebuildTownKdtree();
	}

	if (_current_language == nullptr) {
		extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
		auto saved_paths = _valid_searchpaths;
		auto saved_binary = _searchpaths[Searchpath::BinaryDir];
		_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
		_valid_searchpaths = {Searchpath::BinaryDir};
		InitializeLanguagePacks();
		_valid_searchpaths = std::move(saved_paths);
		_searchpaths[Searchpath::BinaryDir] = std::move(saved_binary);
	}

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	PlanetManager::Reset();
	PlanetRegion w0{
		.id = WorldID{0},
		.name = "Terra Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 79,
		.max_y = 255,
	};
	PlanetRegion w1{
		.id = WorldID{1},
		.name = "Merredin Rust",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 80,
		.min_y = 0,
		.max_x = 159,
		.max_y = 255,
	};
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Calyx Tundra",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 160,
		.min_y = 0,
		.max_x = 209,
		.max_y = 255,
	};
	PlanetRegion w3{
		.id = WorldID{3},
		.name = "Vulcan Barren",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::Volcanic,
		.min_x = 210,
		.min_y = 0,
		.max_x = 255,
		.max_y = 255,
	};
	PlanetManager::RegisterRegion(w0);
	PlanetManager::RegisterRegion(w1);
	PlanetManager::RegisterRegion(w2);
	PlanetManager::RegisterRegion(w3);
}

TEST_CASE("Sprint 45 - Monumental Portal 18-Tile Footprint & Component Layout")
{
	SetupTestEnvironment();
	VisualOverhaulManager::Reset();

	TileIndex gate = TileXY(40, 40);
	MonumentalPortalLayout layout = VisualOverhaulManager::GenerateMonumentalPortalLayout(gate, DiagDirection::NE, WorldID{0});

	REQUIRE(layout.IsValid());
	CHECK(layout.tiles.size() == 18);
	CHECK(layout.gate_tile == gate);
	CHECK(layout.direction == DiagDirection::NE);
	CHECK(layout.world_id == WorldID{0});

	/* Verify specific component mappings */
	bool found_core = false;
	bool found_pylon_west = false;
	bool found_pylon_east = false;
	bool found_arch = false;
	bool found_catenary = false;
	bool found_signals = false;
	bool found_substation = false;

	for (const auto &pt : layout.tiles) {
		CHECK(VisualOverhaulManager::IsMonumentalPortalTile(pt.tile));
		MonumentalPortalComponent comp = VisualOverhaulManager::GetComponentAtTile(pt.tile);
		CHECK(comp == pt.component);

		if (comp == MonumentalPortalComponent::EventHorizonCore) found_core = true;
		if (comp == MonumentalPortalComponent::PylonWest) found_pylon_west = true;
		if (comp == MonumentalPortalComponent::PylonEast) found_pylon_east = true;
		if (comp == MonumentalPortalComponent::ArchCrown) found_arch = true;
		if (comp == MonumentalPortalComponent::ApproachCatenary) found_catenary = true;
		if (comp == MonumentalPortalComponent::SignalingGantry) found_signals = true;
		if (comp == MonumentalPortalComponent::PowerSubstation) found_substation = true;
	}

	CHECK(found_core);
	CHECK(found_pylon_west);
	CHECK(found_pylon_east);
	CHECK(found_arch);
	CHECK(found_catenary);
	CHECK(found_signals);
	CHECK(found_substation);

	/* Non-portal tile check */
	TileIndex outside = TileXY(10, 10);
	CHECK_FALSE(VisualOverhaulManager::IsMonumentalPortalTile(outside));
	CHECK(VisualOverhaulManager::GetComponentAtTile(outside) == MonumentalPortalComponent::TrackBed);
}

TEST_CASE("Sprint 45 - Event Horizon State Shimmer & Quantum Transit Particles")
{
	SetupTestEnvironment();
	VisualOverhaulManager::Reset();
	PortalRegistry::Reset();

	TileIndex gate_a = TileXY(40, 40);
	TileIndex gate_b = TileXY(100, 40);

	/* 1. Unlinked standby gate: Amber/Yellow excitation */
	PortalRegistry::RegisterUnlinkedGate(gate_a, DiagDirection::SW, WorldID{0});
	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_a) == WormholeVisualState::StandbyAmber);

	/* 2. Linked active portal: Cyan-blue pulse */
	PortalRegistry::RegisterPortalPair(gate_a, DiagDirection::SW, WorldID{0}, gate_b, DiagDirection::NE, WorldID{1}, 10, true);
	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_a) == WormholeVisualState::ActiveCyanPulse);

	/* 3. Train transit trigger: White-hot shimmer ripple & QuantumRipple particle */
	CHECK(VisualOverhaulManager::GetActiveParticleCount() == 0);
	VisualOverhaulManager::OnTrainTraversePortal(gate_a, VehicleID{1});

	CHECK(VisualOverhaulManager::GetEventHorizonVisualState(gate_a) == WormholeVisualState::TransitRipple);
	CHECK(VisualOverhaulManager::GetActiveParticleCount() == 1);

	const auto &particles = VisualOverhaulManager::GetActiveParticles();
	REQUIRE(!particles.empty());
	CHECK(particles[0].origin_tile == gate_a);
	CHECK(particles[0].type == PortalParticleType::QuantumRipple);
	CHECK(particles[0].duration_ticks == 40);

	/* 4. Deterministic 8-frame cyclic pulse animation */
	for (uint32_t tick = 0; tick < 32; ++tick) {
		uint8_t frame = VisualOverhaulManager::GetEventHorizonAnimationFrame(gate_a, tick);
		CHECK(frame == (tick / 4) % 8);
	}

	/* 5. Particle aging and pruning */
	VisualOverhaulManager::OnGameTick(100);
	CHECK(VisualOverhaulManager::GetActiveParticleCount() == 0);
}

TEST_CASE("Sprint 45 - Distinct World Biome Surfaces & Environmental Emitters")
{
	SetupTestEnvironment();
	VisualOverhaulManager::Reset();

	/* 1. Core Temperate */
	const BiomeVisualProfile &core = VisualOverhaulManager::GetBiomeProfile(WorldBiome::Temperate);
	CHECK(core.display_name == "Core Temperate");
	CHECK(core.base_ground_palette == PAL_NONE);
	CHECK(core.ambient_particle == PortalParticleType::ArcologyBeacon);

	/* 2. Arid Rust Frontier */
	const BiomeVisualProfile &arid = VisualOverhaulManager::GetBiomeProfile(WorldBiome::AridDesert);
	CHECK(arid.display_name == "Arid Rust Frontier");
	CHECK(arid.base_ground_palette == PALETTE_TO_STRUCT_YELLOW);
	CHECK(arid.has_cracked_salt_flats);
	CHECK(arid.ambient_particle == PortalParticleType::IndustrialHaze);

	/* 3. Boreal Glacial Tundra */
	const BiomeVisualProfile &boreal = VisualOverhaulManager::GetBiomeProfile(WorldBiome::SubArctic);
	CHECK(boreal.display_name == "Boreal Glacial Tundra");
	CHECK(boreal.base_ground_palette == PALETTE_TO_STRUCT_BLUE);
	CHECK(boreal.has_permafrost_shimmer);
	CHECK(boreal.ambient_particle == PortalParticleType::GeothermalSteam);

	/* 4. Volcanic Barren */
	const BiomeVisualProfile &volcanic = VisualOverhaulManager::GetBiomeProfile(WorldBiome::Volcanic);
	CHECK(volcanic.display_name == "Volcanic Barren");
	CHECK(volcanic.base_ground_palette == PALETTE_CRASH);
	CHECK(volcanic.has_basalt_lava_glow);
	CHECK(volcanic.ambient_particle == PortalParticleType::SulfurPlume);

	/* Shimmer & Glow tile predicates */
	TileIndex snow_tile = TileXY(180, 50);
	MakeClear(snow_tile, ClearGround::Grass, 3);
	MakeSnow(snow_tile, 3);
	CHECK(VisualOverhaulManager::ShouldRenderPermafrostShimmer(snow_tile));

	TileIndex volcanic_tile = TileXY(220, 50);
	CHECK(VisualOverhaulManager::ShouldRenderBasaltGlow(volcanic_tile));
}

TEST_CASE("Sprint 45 - Megacity Arcology Evolution & Illuminated Architecture")
{
	SetupTestEnvironment();
	VisualOverhaulManager::Reset();

	TownID town_id{1};

	/* Baseline: None */
	CHECK(VisualOverhaulManager::GetArcologyTier(town_id) == ArcologyTier::None);
	CHECK(VisualOverhaulManager::GetArcologyBuildingPalette(town_id, HouseID{0}) == PAL_NONE);

	/* Tier 1: Spire Foundations (Pop >= 500, Satisfaction >= 50%) */
	VisualOverhaulManager::UpdateArcologyEvolution(town_id, 0.55f, 600);
	CHECK(VisualOverhaulManager::GetArcologyTier(town_id) == ArcologyTier::Tier1_SpireFoundation);

	/* Tier 2: Interconnected Grid Skybridges (Pop >= 1000, Satisfaction >= 70%) */
	VisualOverhaulManager::UpdateArcologyEvolution(town_id, 0.75f, 1100);
	CHECK(VisualOverhaulManager::GetArcologyTier(town_id) == ArcologyTier::Tier2_InterconnectedGrid);
	CHECK(VisualOverhaulManager::GetArcologyBuildingPalette(town_id, HouseID{0}) == PALETTE_TO_STRUCT_YELLOW);

	/* Tier 3: Commonwealth Arcology Citadel (Pop >= 1500, Satisfaction >= 90%) */
	VisualOverhaulManager::UpdateArcologyEvolution(town_id, 0.95f, 1800);
	CHECK(VisualOverhaulManager::GetArcologyTier(town_id) == ArcologyTier::Tier3_CommonwealthCitadel);
	CHECK(VisualOverhaulManager::GetArcologyBuildingPalette(town_id, HouseID{0}) == PALETTE_TO_STRUCT_BLUE);
}

TEST_CASE("Sprint 45 - Custom 12-Cargo Rolling Stock Fleet & Corporate Liveries")
{
	SetupTestEnvironment();

	/* 1. Locomotive Visual Profiles */
	RollingStockVisualProfile titan = VisualOverhaulManager::GetRollingStockProfile(EngineID{CST_ENGINE_TITAN_DIESEL});
	CHECK(titan.model_name == "Titan D-100 Twin-Engine Hauler");
	CHECK(titan.visual_class_label == "10,000-hp Heavy Diesel Hauler");

	RollingStockVisualProfile e40 = VisualOverhaulManager::GetRollingStockProfile(EngineID{CST_ENGINE_CST_E40});
	CHECK(e40.model_name == "CST E-40 Inter-World Catenary Hauler");
	CHECK(e40.has_pantograph);
	CHECK(e40.visual_class_label == "High-Power Electric Freight");

	RollingStockVisualProfile maglev = VisualOverhaulManager::GetRollingStockProfile(EngineID{CST_ENGINE_MARK4_MAGLEV});
	CHECK(maglev.model_name == "CST Mark IV 'Chimaera' Hyper-Maglev");
	CHECK(maglev.is_hyper_maglev);
	CHECK(maglev.visual_class_label == "400 km/h Vacuum Hyper-Maglev");

	/* 2. Company Liveries */
	Company::CreateAtIndex(CompanyID{1});
	Company *c1 = Company::GetIfValid(CompanyID{1});
	REQUIRE(c1 != nullptr);
	c1->name = "Commonwealth Synergy Transport";
	CHECK(VisualOverhaulManager::GetCompanyFleetLivery(CompanyID{1}) == FleetLiveryTheme::CST_EmeraldGold);

	Company::CreateAtIndex(CompanyID{2});
	Company *c2 = Company::GetIfValid(CompanyID{2});
	REQUIRE(c2 != nullptr);
	c2->name = "Grand Central Trans-Portal";
	CHECK(VisualOverhaulManager::GetCompanyFleetLivery(CompanyID{2}) == FleetLiveryTheme::GrandCentral_NavySilver);

	/* 3. Specialized Cargo Wagons */
	CHECK(VisualOverhaulManager::GetCargoWagonVisualClass(CommonwealthCargoID::EnrichedQuantumCrystals) == "Armored Quantum Containment Van");
	CHECK(VisualOverhaulManager::GetCargoWagonVisualClass(CommonwealthCargoID::SyntheticComposites) == "Cryogenic Pressurized Tanker");
	CHECK(VisualOverhaulManager::GetCargoWagonVisualClass(CommonwealthCargoID::IronOre) == "Reinforced Triple-Axle Mineral Hopper");
	CHECK(VisualOverhaulManager::GetCargoWagonVisualClass(CommonwealthCargoID::StructuralSteel) == "Heavy Industrial Flatbed Wagon");
	CHECK(VisualOverhaulManager::GetCargoWagonVisualClass(CommonwealthCargoID::SiliconChips) == "High-Security Sealed Cargo Van");
}

TEST_CASE("Sprint 45 - Visual Overhaul Save/Load Round-Trip Serialization (VISU Chunk)")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_visual_overhaul.sav").string();
	std::filesystem::remove(test_save_file);

	SetupTestEnvironment();
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	Company::CreateAtIndex(CompanyID{0});

	VisualOverhaulManager::Reset();
	VisualOverhaulManager::SetArcologyTier(TownID{1}, ArcologyTier::Tier1_SpireFoundation);
	VisualOverhaulManager::SetArcologyTier(TownID{2}, ArcologyTier::Tier2_InterconnectedGrid);
	VisualOverhaulManager::SetArcologyTier(TownID{3}, ArcologyTier::Tier3_CommonwealthCitadel);

	CHECK(VisualOverhaulManager::GetAllArcologyTiers().size() == 3);

	LinkGraphSchedule::Clear();
	/* Save game */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));

	/* Reset in-memory arcology state */
	VisualOverhaulManager::Reset();
	CHECK(VisualOverhaulManager::GetAllArcologyTiers().empty());

	/* Load game */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	/* Verify restored tiers */
	CHECK(VisualOverhaulManager::GetArcologyTier(TownID{1}) == ArcologyTier::Tier1_SpireFoundation);
	CHECK(VisualOverhaulManager::GetArcologyTier(TownID{2}) == ArcologyTier::Tier2_InterconnectedGrid);
	CHECK(VisualOverhaulManager::GetArcologyTier(TownID{3}) == ArcologyTier::Tier3_CommonwealthCitadel);

	std::filesystem::remove(test_save_file);
}
