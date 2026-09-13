/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_saveload_planet.cpp Unit and integration tests for planetary savegame serialization. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/federation_identity.h"
#include "../saveload/saveload_func.h"
#include "../saveload/saveload.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../company_base.h"
#include "mock_environment.h"

#include <filesystem>

#include "../safeguards.h"

TEST_CASE("Planet SaveLoad - Multi-World Serialization Round-Trip")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_multiworld.sav").string();
	std::filesystem::remove(test_save_file);

	Map::Allocate(1024, 1024);
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	_company_pool.CleanPool();
	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	PlanetManager::Reset();
	PortalRegistry::Reset();

	/* Register 4 distinct planetary worlds with diverse properties */
	PlanetRegion core_world{
		.id = WorldID{0},
		.name = "Earth Prime",
		.phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate,
		.min_x = 0,
		.min_y = 0,
		.max_x = 399,
		.max_y = 399,
		.development_score = 12500,
	};

	PlanetRegion vulcan_world{
		.id = WorldID{1},
		.name = "Vulcan Forge",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Volcanic,
		.min_x = 500,
		.min_y = 0,
		.max_x = 899,
		.max_y = 399,
		.development_score = 6400,
	};

	PlanetRegion ceres_world{
		.id = WorldID{2},
		.name = "Ceres Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::SubArctic,
		.min_x = 0,
		.min_y = 500,
		.max_x = 399,
		.max_y = 899,
		.development_score = 1800,
	};

	PlanetRegion haven_world{
		.id = WorldID{3},
		.name = "Haven Rim",
		.phase = WorldPhase::Phase4_Expansion,
		.biome = WorldBiome::AridDesert,
		.min_x = 500,
		.min_y = 500,
		.max_x = 899,
		.max_y = 899,
		.development_score = 250,
	};

	REQUIRE(PlanetManager::RegisterRegion(core_world));
	REQUIRE(PlanetManager::RegisterRegion(vulcan_world));
	REQUIRE(PlanetManager::RegisterRegion(ceres_world));
	REQUIRE(PlanetManager::RegisterRegion(haven_world));
	CHECK(PlanetManager::Count() == 4);

	/* Register paired wormhole portals */
	TileIndex portal1_a = TileXY(100, 100);
	TileIndex portal1_b = TileXY(600, 100);
	PortalID pid1 = PortalRegistry::RegisterPortalPair(
		portal1_a, DiagDirection::NE, WorldID{0},
		portal1_b, DiagDirection::SW, WorldID{1},
		3, true
	);
	REQUIRE(pid1 != INVALID_PORTAL);

	TileIndex portal2_a = TileXY(200, 700);
	TileIndex portal2_b = TileXY(700, 700);
	PortalID pid2 = PortalRegistry::RegisterPortalPair(
		portal2_a, DiagDirection::SE, WorldID{2},
		portal2_b, DiagDirection::NW, WorldID{3},
		5, false
	);
	REQUIRE(pid2 != INVALID_PORTAL);
	CHECK(PortalRegistry::Count() == 2);

	/* Set in-flight transit progress */
	PortalRegistry::SetVehicleTransitProgress(VehicleID{10}, 18);
	PortalRegistry::SetVehicleTransitProgress(VehicleID{42}, 37);
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{10}) == 18);
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{42}) == 37);

	/* Persist the save namespace and allocator state. The deliberately stale
	 * mapping exercises post-load cleanup without requiring a live train. */
	FederationNamespace federation_namespace{0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
	FederationIdentityRegistry::RestoreState(federation_namespace, 8);
	REQUIRE(FederationIdentityRegistry::RestoreMapping(VehicleID{10}, 7));

	/* Save the game */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));
	REQUIRE(std::filesystem::file_size(test_save_file) > 0);

	/* Wipe game memory state completely */
	PlanetManager::Reset();
	PortalRegistry::Reset();
	CHECK(PlanetManager::Count() == 0);
	CHECK(PortalRegistry::Count() == 0);
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{10}) == 0);
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{42}) == 0);
	CHECK(FederationIdentityRegistry::GetNextSequence() == 1);

	/* Load the game back from disk */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);
	CHECK(FederationIdentityRegistry::GetNamespace() == federation_namespace);
	CHECK(FederationIdentityRegistry::GetNextSequence() == 8);
	CHECK(FederationIdentityRegistry::GetMappings().empty());

	/* 1. Verify all planetary regions restored with accurate attributes */
	CHECK(PlanetManager::Count() == 4);

	const PlanetRegion *r0 = PlanetManager::GetRegion(WorldID{0});
	REQUIRE(r0 != nullptr);
	CHECK(r0->name == "Earth Prime");
	CHECK(r0->phase == WorldPhase::Phase1_Core);
	CHECK(r0->biome == WorldBiome::Temperate);
	CHECK(r0->min_x == 0);
	CHECK(r0->min_y == 0);
	CHECK(r0->max_x == 399);
	CHECK(r0->max_y == 399);
	CHECK(r0->development_score == 12500);

	const PlanetRegion *r1 = PlanetManager::GetRegion(WorldID{1});
	REQUIRE(r1 != nullptr);
	CHECK(r1->name == "Vulcan Forge");
	CHECK(r1->phase == WorldPhase::Phase2_Developed);
	CHECK(r1->biome == WorldBiome::Volcanic);
	CHECK(r1->min_x == 500);
	CHECK(r1->min_y == 0);
	CHECK(r1->max_x == 899);
	CHECK(r1->max_y == 399);
	CHECK(r1->development_score == 6400);

	const PlanetRegion *r2 = PlanetManager::GetRegion(WorldID{2});
	REQUIRE(r2 != nullptr);
	CHECK(r2->name == "Ceres Outpost");
	CHECK(r2->phase == WorldPhase::Phase3_Frontier);
	CHECK(r2->biome == WorldBiome::SubArctic);
	CHECK(r2->min_x == 0);
	CHECK(r2->min_y == 500);
	CHECK(r2->max_x == 399);
	CHECK(r2->max_y == 899);
	CHECK(r2->development_score == 1800);

	const PlanetRegion *r3 = PlanetManager::GetRegion(WorldID{3});
	REQUIRE(r3 != nullptr);
	CHECK(r3->name == "Haven Rim");
	CHECK(r3->phase == WorldPhase::Phase4_Expansion);
	CHECK(r3->biome == WorldBiome::AridDesert);
	CHECK(r3->min_x == 500);
	CHECK(r3->min_y == 500);
	CHECK(r3->max_x == 899);
	CHECK(r3->max_y == 899);
	CHECK(r3->development_score == 250);

	/* 2. Verify spatial grid acceleration is hot and queries work in O(1) */
	const PlanetRegion *q0 = PlanetManager::GetRegionByTile(TileXY(200, 200));
	REQUIRE(q0 != nullptr);
	CHECK(q0->id == WorldID{0});
	CHECK(q0->name == "Earth Prime");

	const PlanetRegion *q1 = PlanetManager::GetRegionByTile(TileXY(700, 200));
	REQUIRE(q1 != nullptr);
	CHECK(q1->id == WorldID{1});
	CHECK(q1->name == "Vulcan Forge");

	const PlanetRegion *q2 = PlanetManager::GetRegionByTile(TileXY(200, 700));
	REQUIRE(q2 != nullptr);
	CHECK(q2->id == WorldID{2});
	CHECK(q2->name == "Ceres Outpost");

	const PlanetRegion *q3 = PlanetManager::GetRegionByTile(TileXY(700, 700));
	REQUIRE(q3 != nullptr);
	CHECK(q3->id == WorldID{3});
	CHECK(q3->name == "Haven Rim");

	const PlanetRegion *q_void = PlanetManager::GetRegionByTile(TileXY(450, 450));
	CHECK(q_void == nullptr);

	/* 3. Verify wormhole portal links restored accurately */
	CHECK(PortalRegistry::Count() == 2);

	CHECK(PortalRegistry::IsPortalTile(portal1_a));
	CHECK(PortalRegistry::IsPortalTile(portal1_b));
	CHECK(PortalRegistry::GetOtherPortalEnd(portal1_a) == portal1_b);
	CHECK(PortalRegistry::GetOtherPortalEnd(portal1_b) == portal1_a);
	CHECK(PortalRegistry::GetPortalVirtualLength(portal1_a) == 3);
	CHECK(PortalRegistry::GetPortalVirtualLength(portal1_b) == 3);

	const PortalLink *link1 = PortalRegistry::GetPortalLink(portal1_a);
	REQUIRE(link1 != nullptr);
	CHECK(link1->bidirectional == true);
	CHECK(link1->end_a.world_id == WorldID{0});
	CHECK(link1->end_b.world_id == WorldID{1});

	CHECK(PortalRegistry::IsPortalTile(portal2_a));
	CHECK(PortalRegistry::IsPortalTile(portal2_b));
	CHECK(PortalRegistry::GetOtherPortalEnd(portal2_a) == portal2_b);
	CHECK(PortalRegistry::GetPortalVirtualLength(portal2_a) == 5);

	const PortalLink *link2 = PortalRegistry::GetPortalLink(portal2_a);
	REQUIRE(link2 != nullptr);
	CHECK(link2->bidirectional == false);
	CHECK(link2->end_a.world_id == WorldID{2});
	CHECK(link2->end_b.world_id == WorldID{3});

	/* 4. Verify in-flight transit progress restored */
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{10}) == 18);
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{42}) == 37);
	CHECK(PortalRegistry::GetPortalTransitProgress(VehicleID{99}) == 0);

	/* Cleanup */
	std::filesystem::remove(test_save_file);
}

TEST_CASE("Planet SaveLoad - Empty / Single-World Game Round-Trip")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_empty.sav").string();
	std::filesystem::remove(test_save_file);

	Map::Allocate(128, 128);
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	_company_pool.CleanPool();
	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	PlanetManager::Reset();
	PortalRegistry::Reset();

	CHECK(PlanetManager::Count() == 0);
	CHECK(PortalRegistry::Count() == 0);

	/* Save empty multi-world state */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));

	/* Load back */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	/* Multi-world manager and portal registry remain clean and empty */
	CHECK(PlanetManager::Count() == 0);
	CHECK(PortalRegistry::Count() == 0);
	CHECK(PlanetManager::GetRegionByTile(TileXY(64, 64)) == nullptr);

	/* Cleanup */
	std::filesystem::remove(test_save_file);
}
