/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */

/** @file test_resource_sites.cpp Native command, research, persistence and generation regressions for resource surveying. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/resource_sites.h"
#include "../portal/planet_manager.h"
#include "../portal/tech_tree.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../economy_base.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../cargotype.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../gui.h"
#include "../widgets/industry_widget.h"
#include "../core/random_func.hpp"
#include "../genworld.h"
#include "../table/strings.h"
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
extern void SaveReloadCommandAuthority();

/** Restore global state so combined and isolated CTest execution agree. */
struct SurveyFixture {
	uint16_t previous_industry_scale = _settings_game.economy.industry_cargo_scale;
	uint16_t previous_town_scale = _settings_game.economy.town_cargo_scale;
	SurveyFixture()
	{
		ResourceSiteManager::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase2_Developed, 2000);
		Map::CountLandTiles();
		_settings_game.economy.industry_cargo_scale = 100;
		_settings_game.economy.town_cargo_scale = 100;
		ResetIndustries();
		ResourceSiteManager::SetEnabled(true);
		_price[Price::BuildIndustryRaw] = 100000;
		_price[Price::BuildIndustry] = 100000;
	}
	~SurveyFixture()
	{
		UnInitWindowSystem();
		ResourceSiteManager::Reset();
		SetupCargoForClimate(LandscapeType::Temperate);
		_settings_game.economy.industry_cargo_scale = previous_industry_scale;
		_settings_game.economy.town_cargo_scale = previous_town_scale;
	}
	IndustryType Raw(bool organic = false)
	{
		for (IndustryType type = 0; type < NUM_INDUSTRYTYPES; ++type) {
			const auto *spec = GetIndustrySpec(type);
			if (spec->enabled && spec->IsRawIndustry() && !spec->layouts.empty() && !spec->behaviour.Test(IndustryBehaviour::BuiltOnWater) &&
				(organic ? spec->life_type.Test(IndustryLifeType::Organic) : spec->life_type.Test(IndustryLifeType::Extractive))) return type;
		}
		FAIL("No native primary type available");
		return IT_INVALID;
	}
};

TEST_CASE("Resource survey is paid once, company-specific, and atomic", "[resource-sites]")
{
	SurveyFixture fixture;
	TileIndex anchor = TileXY(25, 25);
	auto type = fixture.Raw();
	REQUIRE(ResourceSiteManager::AddSite(anchor, type, 16, 16) != 0);
	Money money = Company::Get(CompanyID{0})->money;
	auto estimate = Command<Commands::SurveyResources>::Do({}, anchor);
	REQUIRE(estimate.Succeeded());
	CHECK(estimate.GetCost() == 1000);
	CHECK(ResourceSiteManager::Surveys().empty());
	REQUIRE(Command<Commands::SurveyResources>::Post(anchor));
	CHECK(Company::Get(CompanyID{0})->money == money - 1000);
	CHECK(ResourceSiteManager::Discovered(CompanyID{0}, ResourceSiteManager::Sites().front()));
	CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{1}, ResourceSiteManager::Sites().front()));
	REQUIRE(Command<Commands::SurveyResources>::Post(anchor));
	CHECK(Company::Get(CompanyID{0})->money == money - 1000);
	REQUIRE(Command<Commands::SurveyResources>::Post(TileXY(26, 25)));
	CHECK(Company::Get(CompanyID{0})->money == money - 2000);
	CHECK_FALSE(Command<Commands::SurveyResources>::Post(TileXY(60, 60)));
	CHECK(Company::Get(CompanyID{0})->money == money - 2000);
	Company::Get(CompanyID{0})->money = 0;
	CHECK_FALSE(Command<Commands::SurveyResources>::Post(TileXY(1, 1)));
	CHECK_FALSE(ResourceSiteManager::Surveyed(CompanyID{0}, TileXY(1, 1)));
}

TEST_CASE("Research reveals advanced resources in previously surveyed areas", "[resource-sites]")
{
	SurveyFixture fixture;
	IndustryType type = fixture.Raw();
	CargoType cargo = GetIndustrySpec(type)->produced_cargo[0];
	REQUIRE(IsValidCargoType(cargo));
	for (auto [label, tech] : {std::pair{CargoLabel{"COPR"}, TECH_MATERIALS_2}, {CargoLabel{"SAND"}, TECH_MATERIALS_2}, {CargoLabel{"RARE"}, TECH_MATERIALS_3}}) {
		CargoSpec::Get(cargo)->label = label;
		ResourceSiteManager::Reset(); ResourceSiteManager::SetEnabled(true);
		TechTreeManager::Reset();
		TileIndex anchor = TileXY(25, 25);
		REQUIRE(ResourceSiteManager::AddSite(anchor, type, 16, 16) != 0);
		REQUIRE(Command<Commands::SurveyResources>::Post(anchor));
		CHECK(ResourceSiteManager::RequiredTech(type) == tech);
		CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{0}, ResourceSiteManager::Sites().front()));
		CHECK(Command<Commands::BuildIndustry>::Do({}, anchor, type, 0, true, 1).GetErrorMessage() == STR_RESOURCE_RESEARCH_REQUIRED);
		TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {tech});
		CHECK(ResourceSiteManager::Discovered(CompanyID{0}, ResourceSiteManager::Sites().front()));
		CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{1}, ResourceSiteManager::Sites().front()));
	}
}

TEST_CASE("Native primary construction requires matching survey and claims only on success", "[resource-sites]")
{
	SurveyFixture fixture;
	auto type = fixture.Raw(GENERATE(false, true));
	TileIndex anchor = TileXY(30, 30);
	REQUIRE(ResourceSiteManager::AddSite(anchor, type, 16, 16) != 0);
	CHECK(Command<Commands::BuildIndustry>::Do({}, anchor, type, 0, true, 1).GetErrorMessage() == STR_RESOURCE_NOT_SURVEYED);
	REQUIRE(Command<Commands::SurveyResources>::Post(anchor));
	CHECK(Command<Commands::BuildIndustry>::Do({}, TileXY(20, 20), type, 0, true, 1).GetErrorMessage() == STR_RESOURCE_SITE_REQUIRED);
	REQUIRE(Command<Commands::BuildIndustry>::Do({}, anchor, type, 0, true, 1).Succeeded());
	CHECK(ResourceSiteManager::Sites().front().occupant == IndustryID::Invalid());
	REQUIRE(Command<Commands::BuildIndustry>::Post(anchor, type, 0, true, 1));
	const auto id = ResourceSiteManager::Sites().front().occupant;
	REQUIRE(Industry::IsValidID(id));
	CHECK(Industry::Get(id)->type == type);
	auto count = Industry::GetNumItems();
	CHECK_FALSE(Command<Commands::BuildIndustry>::Post(anchor, type, 0, true, 1));
	CHECK(Industry::GetNumItems() == count);
	_current_company = _local_company = CompanyID{1};
	REQUIRE(Command<Commands::SurveyResources>::Post(anchor));
	CHECK(Command<Commands::BuildIndustry>::Do({}, anchor, type, 0, true, 1).GetErrorMessage() == STR_RESOURCE_OCCUPIED);
	delete Industry::Get(id);
	CHECK(ResourceSiteManager::Sites().front().occupant == IndustryID::Invalid());
	REQUIRE(Command<Commands::BuildIndustry>::Post(anchor, type, 0, true, 1));
	CHECK(Industry::GetNumItems() == count);
}

TEST_CASE("Survey sites preserve mode, private coverage, occupancy and research across real reload", "[resource-sites]")
{
	SurveyFixture fixture;
	const auto type = fixture.Raw();
	const TileIndex anchor = TileXY(30, 30);
	REQUIRE(ResourceSiteManager::AddSite(anchor, type, 16, 16) != 0);
	REQUIRE(Command<Commands::SurveyResources>::Post(anchor));
	REQUIRE(Command<Commands::BuildIndustry>::Post(anchor, type, 0, true, 1));
	const auto id = ResourceSiteManager::Sites().front().occupant;
	const auto money = Company::Get(CompanyID{0})->money;
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_2});
	SaveReloadCommandAuthority();
	CHECK(ResourceSiteManager::Enabled());
	REQUIRE(ResourceSiteManager::Sites().size() == 1);
	CHECK(ResourceSiteManager::Sites().front().occupant == id);
	CHECK(ResourceSiteManager::ResolveType(ResourceSiteManager::Sites().front()) == type);
	CHECK(ResourceSiteManager::Surveyed(CompanyID{0}, anchor));
	CHECK_FALSE(ResourceSiteManager::Surveyed(CompanyID{1}, anchor));
	CHECK(Company::Get(CompanyID{0})->money == money);
	CHECK(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_2));
	ResourceSiteManager::Reset();
	SaveReloadCommandAuthority();
	CHECK_FALSE(ResourceSiteManager::Enabled());
	CHECK(ResourceSiteManager::Sites().empty());
}

TEST_CASE("Resource surveys cannot cross worlds and company merger transfers knowledge", "[resource-sites]")
{
	SurveyFixture fixture;
	REQUIRE(ResourceSiteManager::Survey(DoCommandFlag::Execute, CompanyID{0}, TileXY(20, 20)).Succeeded());
	ResourceSiteManager::ChangeCompany(CompanyID{0}, CompanyID{1});
	CHECK_FALSE(ResourceSiteManager::Surveyed(CompanyID{0}, TileXY(20, 20)));
	CHECK(ResourceSiteManager::Surveyed(CompanyID{1}, TileXY(20, 20)));
	ResourceSiteManager::ChangeCompany(CompanyID{1}, CompanyID::Invalid());
	CHECK(ResourceSiteManager::Surveys().empty());
	PlanetManager::Reset();
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "Left", .phase = WorldPhase::Phase3_Frontier, .min_x = 1, .min_y = 1, .max_x = 31, .max_y = 62}));
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{1}, .name = "Right", .phase = WorldPhase::Phase3_Frontier, .min_x = 32, .min_y = 1, .max_x = 62, .max_y = 62}));
	CHECK(ResourceSiteManager::Survey({}, CompanyID{0}, TileXY(25, 20)).Failed());
	CHECK(ResourceSiteManager::AddSite(TileXY(25, 20), fixture.Raw(), 16, 16) == 0);
}

TEST_CASE("Hidden site generation is deterministic and adds no operating industry", "[resource-sites]")
{
	SurveyFixture fixture;
	auto generate = [&]() {
		ResourceSiteManager::Reset(); ResourceSiteManager::SetEnabled(true);
		SetRandomSeed(71239);
		_generating_world = true;
		GenerateResourceSites(8);
		_generating_world = false;
		std::vector<std::pair<TileIndex, IndustryType>> result;
		for (const auto &site : ResourceSiteManager::Sites()) {
			CHECK(site.occupant == IndustryID::Invalid());
			CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{0}, site));
			result.emplace_back(site.anchor, ResourceSiteManager::ResolveType(site));
		}
		return result;
	};
	auto first = generate();
	REQUIRE_FALSE(first.empty());
	CHECK(first == generate());
	CHECK(Industry::GetNumItems() == 0);
	for (uint i = 0; i < 100; ++i) { _industry_builder.EconomyMonthlyLoop(); _industry_builder.TryBuildNewIndustry(); }
	CHECK(Industry::GetNumItems() == 0);
}

TEST_CASE("Industry UI exposes surveying without opening it during layout", "[resource-sites][gui]")
{
	SurveyFixture fixture;
	ShowBuildIndustryWindow();
	Window *industry = FindWindowById(WindowClass::BuildIndustry, 0);
	REQUIRE(industry != nullptr);
	CHECK(FindWindowById(WindowClass::ResourceSurvey, 0) == nullptr);
	industry->OnClick({}, WID_DPI_SURVEY_WIDGET, 1);
	REQUIRE(FindWindowById(WindowClass::ResourceSurvey, 0) != nullptr);
	CHECK(GetResourceOverlay(TileXY(30, 30)) == 0);
}

TEST_CASE("Player-built generation creates basic primaries only at all density presets", "[resource-sites]")
{
	SurveyFixture fixture;
	const auto density = GENERATE(IndustryDensity::FundedOnly, IndustryDensity::Low, IndustryDensity::Custom);
	auto old_density = _settings_game.game_creation.resource_density;
	auto old_count = _settings_game.game_creation.custom_industry_number;
	_settings_game.game_creation.resource_density = density;
	_settings_game.game_creation.custom_industry_number = 3;
	SetRandomSeed(821);
	_generating_world = true;
	extern void GenerateIndustries();
	GenerateIndustries();
	_generating_world = false;
	if (density == IndustryDensity::FundedOnly) CHECK(Industry::GetNumItems() == 0);
	else CHECK(Industry::GetNumItems() > 0);
	for (const Industry *industry : Industry::Iterate()) {
		CHECK(ResourceSiteManager::IsPrimary(industry->type));
		CHECK(ResourceSiteManager::RequiredTech(industry->type) == TECH_NONE);
	}
	CHECK_FALSE(ResourceSiteManager::Sites().empty());
	_settings_game.game_creation.resource_density = old_density;
	_settings_game.game_creation.custom_industry_number = old_count;
}
