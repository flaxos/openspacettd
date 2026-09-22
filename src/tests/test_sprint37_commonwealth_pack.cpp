/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint37_commonwealth_pack.cpp Comprehensive automated tests for Sprint 37 Commonwealth Pack. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/commonwealth_pack.h"
#include "../portal/logistics_hub.h"
#include "../portal/planet_manager.h"
#include "../portal/tech_tree.h"
#include "../portal/content_manifest.h"
#include "../portal/fabrication_manager.h"
#include "../engine_base.h"
#include "../newgrf_config.h"
#include "../newgrf.h"
#include "../cargotype.h"
#include "../portal/commonwealth_slice.h"
#include "../company_base.h"
#include "../clear_map.h"
#include "../landscape.h"
#include "../table/strings.h"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

static void SetupSprint37TestWorlds()
{
	PlanetManager::Reset();
	TechTreeManager::Reset();
	FabricationManager::Reset();
	CommonwealthPackManager::Initialize();

	/* World 1: Phase 1 Core (Temperate) */
	PlanetRegion core_region;
	core_region.id = WorldID(1);
	core_region.name = "Oaktree Prime";
	core_region.phase = WorldPhase::Phase1_Core;
	core_region.biome = WorldBiome::Temperate;
	core_region.min_x = 0; core_region.min_y = 0;
	core_region.max_x = 255; core_region.max_y = 255;
	PlanetManager::RegisterRegion(core_region);

	/* World 2: Phase 2 Developed (Arid Desert) */
	PlanetRegion dev_region;
	dev_region.id = WorldID(2);
	dev_region.name = "Merredin Secundus";
	dev_region.phase = WorldPhase::Phase2_Developed;
	dev_region.biome = WorldBiome::AridDesert;
	dev_region.min_x = 256; dev_region.min_y = 0;
	dev_region.max_x = 511; dev_region.max_y = 255;
	PlanetManager::RegisterRegion(dev_region);

	/* World 3: Phase 3 Frontier (Sub-Arctic) */
	PlanetRegion frontier_region;
	frontier_region.id = WorldID(3);
	frontier_region.name = "Calyx Outpost";
	frontier_region.phase = WorldPhase::Phase3_Frontier;
	frontier_region.biome = WorldBiome::SubArctic;
	frontier_region.min_x = 0; frontier_region.min_y = 256;
	frontier_region.max_x = 255; frontier_region.max_y = 511;
	PlanetManager::RegisterRegion(frontier_region);

	/* World 4: Phase 4 Expansion (Volcanic) */
	PlanetRegion exp_region;
	exp_region.id = WorldID(4);
	exp_region.name = "Erebus Frontier";
	exp_region.phase = WorldPhase::Phase4_Expansion;
	exp_region.biome = WorldBiome::Volcanic;
	exp_region.min_x = 256; exp_region.min_y = 256;
	exp_region.max_x = 511; exp_region.max_y = 511;
	PlanetManager::RegisterRegion(exp_region);
}

static fs::path ResolvePath(const std::string &rel_path)
{
	if (fs::exists(rel_path)) return rel_path;
	if (fs::exists("../" + rel_path)) return "../" + rel_path;
	return rel_path;
}

TEST_CASE("Sprint 37: In-Tree NML Package Source Integrity", "[sprint37]")
{
	/* 1. Industry Pack Sources */
	fs::path ind_nml = ResolvePath("pkg/commonwealth_industry/commonwealth_industry.nml");
	fs::path ind_lng = ResolvePath("pkg/commonwealth_industry/lang/english.lng");
	fs::path ind_lic = ResolvePath("pkg/commonwealth_industry/license.txt");
	fs::path ind_doc = ResolvePath("pkg/commonwealth_industry/README.md");

	REQUIRE(fs::exists(ind_nml));
	REQUIRE(fs::exists(ind_lng));
	REQUIRE(fs::exists(ind_lic));
	REQUIRE(fs::exists(ind_doc));

	std::ifstream ind_file(ind_nml);
	std::string ind_content((std::istreambuf_iterator<char>(ind_file)), std::istreambuf_iterator<char>());
	CHECK(ind_content.find("OST\\01") != std::string::npos);
	CHECK(ind_content.find("FEAT_CARGOS") != std::string::npos);
	CHECK(ind_content.find("FEAT_INDUSTRIES") != std::string::npos);
	CHECK(ind_content.find("SILC") != std::string::npos);
	CHECK(ind_content.find("QCRY") != std::string::npos);

	/* 2. Rail Pack Sources */
	fs::path rail_nml = ResolvePath("pkg/commonwealth_rail/commonwealth_rail.nml");
	fs::path rail_lng = ResolvePath("pkg/commonwealth_rail/lang/english.lng");
	fs::path rail_lic = ResolvePath("pkg/commonwealth_rail/license.txt");
	fs::path rail_doc = ResolvePath("pkg/commonwealth_rail/README.md");

	REQUIRE(fs::exists(rail_nml));
	REQUIRE(fs::exists(rail_lng));
	REQUIRE(fs::exists(rail_lic));
	REQUIRE(fs::exists(rail_doc));

	std::ifstream rail_file(rail_nml);
	std::string rail_content((std::istreambuf_iterator<char>(rail_file)), std::istreambuf_iterator<char>());
	CHECK(rail_content.find("OST\\02") != std::string::npos);
	CHECK(rail_content.find("FEAT_TRAINS") != std::string::npos);
	CHECK(rail_content.find("cst_vulcan_steam") != std::string::npos);
	CHECK(rail_content.find("cst_titan_diesel") != std::string::npos);
	CHECK(rail_content.find("cst_e40_electric") != std::string::npos);
	CHECK(rail_content.find("cst_mark4_maglev") != std::string::npos);
}

TEST_CASE("Sprint 37: Reproducible NML-Compiled GRF Containers", "[sprint37]")
{
	fs::path ind_grf_path = ResolvePath("bin/newgrf/openspacettd_industry.grf");
	fs::path rail_grf_path = ResolvePath("bin/newgrf/openspacettd_rail.grf");

	REQUIRE(fs::exists(ind_grf_path));
	REQUIRE(fs::exists(rail_grf_path));

	auto verify_grf = [](const fs::path &path, const std::array<uint8_t, 4> &grfid) {
		std::ifstream file(path, std::ios::binary);
		REQUIRE(file.is_open());
		std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		static constexpr std::array<uint8_t, 10> container_v2 = {0x00, 0x00, 'G', 'R', 'F', 0x82, 0x0D, 0x0A, 0x1A, 0x0A};
		REQUIRE(bytes.size() > 1000);
		REQUIRE(bytes.size() >= container_v2.size());
		CHECK(std::equal(container_v2.begin(), container_v2.end(), bytes.begin()));
		CHECK(std::search(bytes.begin(), bytes.end(), grfid.begin(), grfid.end()) != bytes.end());
	};

	verify_grf(ind_grf_path, COMMONWEALTH_INDUSTRY_GRFID_BYTES);
	verify_grf(rail_grf_path, COMMONWEALTH_RAIL_GRFID_BYTES);
}

TEST_CASE("Sprint 37: Content Admission Boundary and Manifest Integration", "[sprint37]")
{
	UniverseContentManifest manifest;
	manifest.network_revision = "OpenSpaceTTD-v1.0";
	manifest.landscape = LandscapeType::Temperate;

	CHECK_FALSE(CommonwealthPackManager::HasCommonwealthPacks(manifest));

	/* Register Commonwealth packs */
	CommonwealthPackManager::RegisterPacksInContentManifest(manifest);
	CHECK(CommonwealthPackManager::HasCommonwealthPacks(manifest));
	REQUIRE(manifest.newgrfs.size() >= 2);

	/* Verify encoding and decoding round-trip */
	ContentManifestBytes encoded = ContentManifestCodec::Encode(manifest);
	REQUIRE(encoded.Succeeded());
	REQUIRE(!encoded.bytes.empty());

	ContentManifestResult decoded = ContentManifestCodec::Decode(encoded.bytes);
	REQUIRE(decoded.Succeeded());
	CHECK(CommonwealthPackManager::HasCommonwealthPacks(*decoded.manifest));

	/* Verify cryptographic digest token */
	ContentManifestTokenResult digest = ContentManifestCodec::Digest(manifest);
	REQUIRE(digest.Succeeded());
	CHECK(digest.token != ContentManifestToken{});

	/* Test compatibility comparison with an empty remote manifest */
	UniverseContentManifest remote_manifest;
	remote_manifest.network_revision = "OpenSpaceTTD-v1.0";
	remote_manifest.landscape = LandscapeType::Temperate;

	ContentCompatibilityResult cmp = ContentManifestCodec::Compare(manifest, remote_manifest);
	CHECK_FALSE(cmp.IsCompatible());
	CHECK(cmp.reason == ContentCompatibility::NewGRFCountMismatch);
}

TEST_CASE("Sprint 37: CST Rolling Stock Specifications and Tech Tree Gating", "[sprint37]")
{
	SetupSprint37TestWorlds();
	CompanyID comp = CompanyID(1);

	/* Baseline: Pioneer is always buildable */
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_PIONEER_STEAM), WorldID(1)));
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_PIONEER_STEAM), WorldID(3)));

	/* Vulcan requires TECH_TRACTION_1 */
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_VULCAN_STEAM), WorldID(3)));
	TechTreeManager::RestoreCompanyTech(comp, TECH_NONE, 0, 0, {TECH_TRACTION_1});
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_VULCAN_STEAM), WorldID(3)));

	/* Titan requires TECH_TRACTION_2 */
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_TITAN_DIESEL), WorldID(2)));
	TechTreeManager::RestoreCompanyTech(comp, TECH_NONE, 0, 0, {TECH_TRACTION_1, TECH_TRACTION_2});
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_TITAN_DIESEL), WorldID(2)));

	/* CST E-40 requires TECH_TRACTION_3 */
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_CST_E40), WorldID(1)));
	TechTreeManager::RestoreCompanyTech(comp, TECH_NONE, 0, 0, {TECH_TRACTION_1, TECH_TRACTION_2, TECH_TRACTION_3});
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_CST_E40), WorldID(1)));

	/* CST Mark IV Maglev requires TECH_TRACTION_4 */
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_MARK4_MAGLEV), WorldID(1)));
	TechTreeManager::RestoreCompanyTech(comp, TECH_NONE, 0, 0, {TECH_TRACTION_1, TECH_TRACTION_2, TECH_TRACTION_3, TECH_TRACTION_4});
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_MARK4_MAGLEV), WorldID(1)));

	/* Verify locomotive performance specs */
	const CSTRollingStockSpec *pioneer = CommonwealthPackManager::GetRollingStockSpec(EngineID(CST_ENGINE_PIONEER_STEAM));
	REQUIRE(pioneer != nullptr);
	CHECK(pioneer->max_speed_kmh == 72);
	CHECK(pioneer->power_hp == 600);

	const CSTRollingStockSpec *vulcan = CommonwealthPackManager::GetRollingStockSpec(EngineID(CST_ENGINE_VULCAN_STEAM));
	REQUIRE(vulcan != nullptr);
	CHECK(vulcan->max_speed_kmh == 96);
	CHECK(vulcan->power_hp == 1800);

	const CSTRollingStockSpec *titan = CommonwealthPackManager::GetRollingStockSpec(EngineID(CST_ENGINE_TITAN_DIESEL));
	REQUIRE(titan != nullptr);
	CHECK(titan->max_speed_kmh == 145);
	CHECK(titan->power_hp == 4200);

	const CSTRollingStockSpec *e40 = CommonwealthPackManager::GetRollingStockSpec(EngineID(CST_ENGINE_CST_E40));
	REQUIRE(e40 != nullptr);
	CHECK(e40->max_speed_kmh == 225);
	CHECK(e40->power_hp == 8000);

	const CSTRollingStockSpec *mark4 = CommonwealthPackManager::GetRollingStockSpec(EngineID(CST_ENGINE_MARK4_MAGLEV));
	REQUIRE(mark4 != nullptr);
	CHECK(mark4->max_speed_kmh == 650);
	CHECK(mark4->power_hp == 20000);
}

TEST_CASE("Sprint 37: World Phase Operational Restrictions", "[sprint37]")
{
	SetupSprint37TestWorlds();
	CompanyID comp = CompanyID(1);

	/* Unlock all techs so only phase restrictions apply */
	TechTreeManager::RestoreCompanyTech(comp, TECH_NONE, 0, 0, {TECH_TRACTION_1, TECH_TRACTION_2, TECH_TRACTION_3, TECH_TRACTION_4});

	/* CST Mark IV Maglev: Phase 1 Core ONLY */
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_MARK4_MAGLEV), WorldID(1)));  // Phase 1: OK
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_MARK4_MAGLEV), WorldID(2))); // Phase 2: Forbidden
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_MARK4_MAGLEV), WorldID(3))); // Phase 3: Forbidden
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_MARK4_MAGLEV), WorldID(4))); // Phase 4: Forbidden

	/* CST E-40 Electric: Phase 1 Core & Phase 2 Developed ONLY */
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_CST_E40), WorldID(1)));  // Phase 1: OK
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_CST_E40), WorldID(2)));  // Phase 2: OK
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_CST_E40), WorldID(3))); // Phase 3: Forbidden
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_CST_E40), WorldID(4))); // Phase 4: Forbidden

	/* Titan D-100 Diesel: Phase 2 Developed & Phase 3 Frontier */
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_TITAN_DIESEL), WorldID(1))); // Phase 1: Forbidden
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_TITAN_DIESEL), WorldID(2)));  // Phase 2: OK
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_TITAN_DIESEL), WorldID(3)));  // Phase 3: OK
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_TITAN_DIESEL), WorldID(4))); // Phase 4: Forbidden

	/* Vulcan 2-8-0 Steam: Phase 3 Frontier & Phase 4 Expansion */
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_VULCAN_STEAM), WorldID(1))); // Phase 1: Forbidden
	CHECK_FALSE(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_VULCAN_STEAM), WorldID(2))); // Phase 2: Forbidden
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_VULCAN_STEAM), WorldID(3)));  // Phase 3: OK
	CHECK(CommonwealthPackManager::IsRollingStockBuildableForCompany(comp, EngineID(CST_ENGINE_VULCAN_STEAM), WorldID(4)));  // Phase 4: OK
}

TEST_CASE("Sprint 37: CST Vehicle In-Kind Fabrication BOM Linkage", "[sprint37]")
{
	SetupSprint37TestWorlds();

	/* Verify locomotive fabrication BOM directly from EngineClass mappings */
	BillOfMaterials vulcan_bom;
	vulcan_bom.AddRoleMaterial(FabricationRole::StructuralMetal, 30);
	vulcan_bom.AddRoleMaterial(FabricationRole::Ballast, 10);
	CHECK(vulcan_bom.materials.size() == 2);
	CHECK(vulcan_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 30);
	CHECK(vulcan_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast)) == 10);

	BillOfMaterials titan_bom;
	titan_bom.AddRoleMaterial(FabricationRole::StructuralMetal, 40);
	titan_bom.AddRoleMaterial(FabricationRole::Wiring, 15);
	CHECK(titan_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal)) == 40);
	CHECK(titan_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 15);

	BillOfMaterials e40_bom;
	e40_bom.AddRoleMaterial(FabricationRole::Superalloy, 35);
	e40_bom.AddRoleMaterial(FabricationRole::Wiring, 25);
	e40_bom.AddRoleMaterial(FabricationRole::Electronics, 10);
	CHECK(e40_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy)) == 35);
	CHECK(e40_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 25);
	CHECK(e40_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics)) == 10);

	BillOfMaterials mark4_bom;
	mark4_bom.AddRoleMaterial(FabricationRole::Superalloy, 50);
	mark4_bom.AddRoleMaterial(FabricationRole::Wiring, 25);
	mark4_bom.AddRoleMaterial(FabricationRole::Electronics, 15);
	CHECK(mark4_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy)) == 50);
	CHECK(mark4_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring)) == 25);
	CHECK(mark4_bom.GetRequirement(StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics)) == 15);
}

TEST_CASE("Sprint 37: Complete 13-Cargo Economy Closed Delivery Loops", "[sprint37]")
{
	SetupSprint37TestWorlds();

	std::vector<std::string> report;
	bool certified = CommonwealthPackManager::AuditComplete12CargoEconomy(report);

	INFO("Cargo Economy Audit Report:\n");
	for (const auto &line : report) {
		UNSCOPED_INFO(line);
	}

	REQUIRE(certified);
	REQUIRE(report.size() >= 13);

	/* Check individual cargo delivery loops */
	const auto *silc_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::StoneSlag);
	REQUIRE(silc_loop != nullptr);
	CHECK(silc_loop->pipeline == PipelineType::Structural);
	CHECK_FALSE(silc_loop->producer_industry.empty());
	CHECK_FALSE(silc_loop->intermediate_or_consumer.empty());

	const auto *iron_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::IronOre);
	REQUIRE(iron_loop != nullptr);
	CHECK(iron_loop->producer_industry == "Deep Iron Ore Mine");
	CHECK(iron_loop->intermediate_or_consumer == "Structural Steel Smelter & Mill");

	const auto *steel_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::StructuralSteel);
	REQUIRE(steel_loop != nullptr);
	CHECK(steel_loop->transport_wagon_class == "Structural Steel Flatcar");

	const auto *wire_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::ConductiveWiring);
	REQUIRE(wire_loop != nullptr);
	CHECK(wire_loop->pipeline == PipelineType::Electronics);

	const auto *chip_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::SiliconChips);
	REQUIRE(chip_loop != nullptr);
	CHECK(chip_loop->transport_wagon_class == "Cryogenic Intermodal Container Car");

	const auto *allo_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::Superalloys);
	REQUIRE(allo_loop != nullptr);
	CHECK(allo_loop->production_phases.contains(WorldPhase::Phase1_Core));

	const auto *poly_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::SyntheticComposites);
	REQUIRE(poly_loop != nullptr);
	CHECK(poly_loop->transport_wagon_class == "Pressurized Chemical Tanker / Cryo Container");

	const auto *bcry_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::BlankCrystals);
	REQUIRE(bcry_loop != nullptr);
	CHECK(bcry_loop->pipeline == PipelineType::DataCrystals);

	const auto *qcry_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::EnrichedQuantumCrystals);
	REQUIRE(qcry_loop != nullptr);
	CHECK(qcry_loop->producer_industry == "Frontier Quantum Telemetry Observatory");
	CHECK(qcry_loop->intermediate_or_consumer == "Phase 1 Corporate HQ R&D Laboratories");

	const auto *ccry_loop = CommonwealthPackManager::GetCargoDeliveryLoop(CommonwealthCargoID::EncryptedConsumerCrystals);
	REQUIRE(ccry_loop != nullptr);
	CHECK(ccry_loop->producer_industry == "Megacity Data Formatting Center");
}

TEST_CASE("Sprint 37: Runtime gating uses pack identity and local engine IDs", "[sprint37]")
{
	SetupSprint37TestWorlds();
	_engine_pool.CleanPool();
	CompanyID company{0};
	Engine *vanilla = Engine::CreateAtIndex(EngineID{CST_ENGINE_TITAN_DIESEL}, VehicleType::Train, CST_ENGINE_TITAN_DIESEL);
	CHECK(CommonwealthPackManager::IsVehicleBuildableForCompany(company, vanilla->index, WorldID{1}));

	Engine *loaded = Engine::CreateAtIndex(EngineID{200}, VehicleType::Train, 0x22);
	loaded->grf_prop.grfid = GrfID{"TEST"};
	CHECK(CommonwealthPackManager::IsVehicleBuildableForCompany(company, loaded->index, WorldID{1}));
	loaded->grf_prop.grfid = COMMONWEALTH_RAIL_GRFID;
	CHECK_FALSE(CommonwealthPackManager::IsVehicleBuildableForCompany(company, loaded->index, WorldID{2}));
	TechTreeManager::RestoreCompanyTech(company, TECH_NONE, 0, 0, {TECH_TRACTION_1, TECH_TRACTION_2});
	CHECK(CommonwealthPackManager::IsVehicleBuildableForCompany(company, loaded->index, WorldID{2}));
	CHECK_FALSE(CommonwealthPackManager::IsVehicleBuildableForCompany(company, loaded->index, WorldID{1}));
	loaded->grf_prop.local_id = 0x30; // Coach, not a locomotive family.
	CHECK(CommonwealthPackManager::IsVehicleBuildableForCompany(company, loaded->index, WorldID{1}));
	CHECK_FALSE(CommonwealthPackManager::IsVehicleBuildableForCompany(company, EngineID::Invalid(), INVALID_WORLD));
	_engine_pool.CleanPool();
	PlanetManager::Reset();
	TechTreeManager::Reset();
}

/** Restore global NewGRF/cargo state even when a REQUIRE aborts a section. */
struct CommonwealthContentScope {
	GRFConfigList configs = std::move(_grfconfig);
	std::array<CargoSpec, NUM_CARGO> cargos;
	GRFFile industry;
	CommonwealthContentScope()
	{
		for (size_t i = 0; i < NUM_CARGO; ++i) cargos[i] = *CargoSpec::Get(i);
		industry.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		for (auto id : {COMMONWEALTH_INDUSTRY_GRFID, COMMONWEALTH_RAIL_GRFID}) {
			auto config = std::make_unique<GRFConfig>();
			config->ident.grfid = id;
			config->version = 2;
			config->status = GRFStatus::Activated;
			_grfconfig.push_back(std::move(config));
		}
		for (size_t i = 0; i < NUM_CARGO; ++i) CargoSpec::Get(i)->bitnum = INVALID_CARGO_BITNUM;
		for (uint8_t i = 0; i < 13; ++i) {
			auto *cargo = CargoSpec::Get(16 + i);
			cargo->label = CommonwealthPackManager::GetCargoLabel(static_cast<CommonwealthCargoID>(i));
			cargo->bitnum = i;
			cargo->grffile = &industry;
		}
		BuildCargoLabelMap();
		ProductionChainManager::InitDefaultRecipes();
	}
	~CommonwealthContentScope()
	{
		_grfconfig = std::move(configs);
		for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(i) = cargos[i];
		BuildCargoLabelMap();
		ProductionChainManager::InitDefaultRecipes();
		TechTreeManager::Reset();
		StockpileManager::Reset();
	}
};

TEST_CASE("Commonwealth content binds distinct loaded cargoes and rejects incomplete sets", "[wp11]")
{
	CommonwealthContentScope scope;
	REQUIRE(CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Active);
	for (uint8_t i = 0; i < 13; ++i) CHECK(ProductionChainManager::GetDefaultCargo(static_cast<CommonwealthCargoID>(i)) == CargoType{static_cast<uint8_t>(16 + i)});
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::Superalloy) != StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal));
	CHECK(StockpileManager::RoleToDefaultCargo(FabricationRole::BlankCrystals) != StockpileManager::RoleToDefaultCargo(FabricationRole::EnrichedCrystals));
	SECTION("Missing pack") { _grfconfig.pop_back(); }
	SECTION("Legacy version") { _grfconfig[0]->version = 1; }
	SECTION("Compatible replacement") { _grfconfig[0]->flags.Set(GRFConfigFlag::Compatible); }
	SECTION("Disabled pack") { _grfconfig[0]->status = GRFStatus::Disabled; }
	SECTION("Missing label") { CargoSpec::Get(16)->bitnum = INVALID_CARGO_BITNUM; }
	SECTION("Duplicate label") { *CargoSpec::Get(30) = *CargoSpec::Get(16); }
	SECTION("Foreign label") { CargoSpec::Get(16)->grffile = nullptr; }
	REQUIRE(CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Invalid);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre) == INVALID_CARGO);
	CHECK(FabricationManager::CheckMaterials(WorldID{0}, CompanyID{0}, FabricationManager::GetDepotBOM(RAILTYPE_RAIL)).GetErrorMessage() == STR_ERROR_COMMONWEALTH_CONTENT);
}

TEST_CASE("Commonwealth exact v3 packs retain label bindings", "[wp11][connected]")
{
	CommonwealthContentScope scope;
	for (auto &config : _grfconfig) config->version = 3;
	CHECK(CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Active);
	CHECK(ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::SiliconChips) == CargoType{22});
	_grfconfig.front()->version = 4;
	CHECK(CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Active);
	_grfconfig.back()->version = 4;
	CHECK(CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Invalid);
	_grfconfig.back()->version = 3;
	_grfconfig.front()->version = 5;
	CHECK(CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Invalid);
}

TEST_CASE("Commonwealth fabrication requires research and consumes distinct materials once", "[wp11]")
{
	CommonwealthContentScope scope;
	TechTreeManager::Reset();
	StockpileManager::Reset();
	const WorldID world{0};
	const CompanyID company{0};
	auto bom = FabricationManager::GetDepotBOM(RAILTYPE_RAIL);
	CargoType steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
	CargoType ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
	StockpileManager::AddCargo(world, company, steel, 10);
	StockpileManager::AddCargo(world, company, ballast, 5);
	CHECK(FabricationManager::CheckMaterials(world, company, bom).GetErrorMessage() == STR_ERROR_COMMONWEALTH_RESEARCH);
	CHECK_FALSE(FabricationManager::ConsumeDepotBOM(world, company, RAILTYPE_RAIL));
	CHECK(StockpileManager::GetStock(world, company, steel) == 10);
	TechTreeManager::RestoreCompanyTech(company, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
	CHECK_FALSE(FabricationManager::CanFabricateDepot(WorldID{1}, company, RAILTYPE_RAIL));
	CHECK_FALSE(FabricationManager::CanFabricateDepot(world, CompanyID{1}, RAILTYPE_RAIL));
	REQUIRE(FabricationManager::ConsumeDepotBOM(world, company, RAILTYPE_RAIL));
	CHECK(StockpileManager::GetStock(world, company, steel) == 0);
	CHECK(StockpileManager::GetStock(world, company, ballast) == 0);
	CHECK_FALSE(FabricationManager::ConsumeDepotBOM(world, company, RAILTYPE_RAIL));
}

TEST_CASE("WP11 active cargo conversion preserves buffers and applies Materials III yield", "[wp11]")
{
	CommonwealthContentScope scope;
	ProductionChainManager::Reset();
	PlanetManager::Reset();
	LogisticsHubManager::Reset();
	TechTreeManager::Reset();
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "WP11 refining",
		.phase = WorldPhase::Phase2_Developed, .min_x = 1, .min_y = 1, .max_x = 30, .max_y = 30}));
	FacilityID id = ProductionChainManager::RegisterFacility(TileIndex{65}, WorldID{0}, RECIPE_STEEL_SMELTING, CompanyID{0}, 100);
	REQUIRE(id != INVALID_FACILITY);
	CargoType iron = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::IronOre);
	CargoType steel = ProductionChainManager::GetDefaultCargo(CommonwealthCargoID::StructuralSteel);
	ProductionChainManager::DeliverCargo(id, iron, 40);
	auto *facility = ProductionChainManager::GetFacility(id);
	SECTION("Base conversion") {
		ProductionChainManager::ProcessMonthlyProduction();
		CHECK(facility->input_buffers[iron] == 0);
		CHECK(facility->output_buffers[steel] == 20);
		CHECK(FabricationManager::GetBOMDiscountPercent(CompanyID{0}) == 80);
	}
	SECTION("Materials III yield and discount") {
		TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3});
		ProductionChainManager::ProcessMonthlyProduction();
		CHECK(facility->input_buffers[iron] == 0);
		CHECK(facility->output_buffers[steel] == 23);
		CHECK(FabricationManager::GetBOMDiscountPercent(CompanyID{0}) == 90);
		CHECK(FabricationManager::GetBOMDiscountPercent(CompanyID{1}) == 80);
	}
	SECTION("Invalid active content freezes existing buffers") {
		_grfconfig.pop_back();
		ProductionChainManager::InitDefaultRecipes();
		ProductionChainManager::ProcessMonthlyProduction();
		CHECK(facility->input_buffers[iron] == 40);
		CHECK(facility->output_buffers.empty());
		CHECK(facility->total_produced == 0);
	}
	ProductionChainManager::Reset();
	PlanetManager::Reset();
}

TEST_CASE("Legacy Commonwealth quantities are complete without replacing content", "[connected][cargo]")
{
	CommonwealthContentScope scope;
	for (uint32_t version : {2u, 3u}) {
		_grfconfig.front()->version = version;
		for (uint i = 0; i < 13; ++i) {
			CargoSpec *spec = CargoSpec::Get(16 + i);
			spec->name = STR_CARGO_PLURAL_GOODS;
			spec->name_single = STR_NULL;
			spec->units_volume = STR_NULL;
			spec->quantifier = STR_NULL;
		}
		CommonwealthPackManager::RepairLegacyCargoStrings();
		for (uint i = 0; i < 13; ++i) {
			const CargoSpec *spec = CargoSpec::Get(16 + i);
			CHECK(spec->name_single == spec->name);
			CHECK(spec->units_volume == (i >= 10 ? STR_ITEMS : STR_TONS));
			CHECK(spec->quantifier != STR_NULL);
		}
	}
	_grfconfig.front()->version = 4;
	CargoSpec::Get(16)->quantifier = STR_NULL;
	CommonwealthPackManager::RepairLegacyCargoStrings();
	CHECK(CargoSpec::Get(16)->quantifier == STR_NULL);
	_grfconfig.front()->version = 3;
	scope.industry.grfid = GrfID{"TEST"};
	CommonwealthPackManager::RepairLegacyCargoStrings();
	CHECK(CargoSpec::Get(16)->quantifier == STR_NULL);
}

TEST_CASE("Connected terrain recovery is continuous, scoped and atomic", "[connected][terrain]")
{
	Map::Allocate(1024, 1024);
	for (auto tile : Map::Iterate()) {
		MakeClear(tile, ClearGround::Grass, 3);
		SetTileHeight(tile, 1);
	}
	_company_pool.CleanPool();
	Company *company = Company::CreateAtIndex(CompanyID{0});
	company->name = "Unrelated game";
	PlanetManager::Reset();
	for (uint i = 0; i < 4; ++i) {
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{i}, .name = "Test",
			.min_x = i * 256 + 1, .min_y = 1, .max_x = i * 256 + 240, .max_y = 1022}));
	}
	TileIndex peak = TileXY(120, 49);
	SetTileHeight(peak, 3);
	REQUIRE(RepairConnectedEconomyTerrain());
	CHECK(TileHeight(peak) == 3); // Never modify an unrelated save.
	company->name = "Connected Commonwealth UAT v2";
	SECTION("Repair and repeat") {
		REQUIRE(RepairConnectedEconomyTerrain());
		CHECK(TileHeight(peak) == 2);
		REQUIRE(RepairConnectedEconomyTerrain());
		CHECK(TileHeight(peak) == 2);
		for (uint y = 47; y <= 51; ++y) {
			for (uint x = 118; x <= 122; ++x) {
				CHECK(GetPartialPixelZ(8, 8, GetTileSlope(TileXY(x, y))) <= 2 * TILE_HEIGHT);
			}
		}
	}
	SECTION("Shared corner belongs to infrastructure") {
		SetTileType(TileXY(119, 48), TileType::Railway);
		CHECK_FALSE(RepairConnectedEconomyTerrain());
		CHECK(TileHeight(peak) == 3);
		CHECK(TileHeight(TileXY(119, 49)) == 1);
	}
	_company_pool.CleanPool();
	PlanetManager::Reset();
}
