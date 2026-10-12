/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */
/** @file test_first_line_funding.cpp Native command fixtures for bounded scenario funding; no generated game. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include "../portal/first_line_funding.h"
#include "../portal/first_permanent_line.h"
#include "../portal/resource_sites.h"
#include "../portal/integrated_economy.h"
#include "../portal/planet_manager.h"
#include "../portal/stellar_network.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/tech_tree.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../company_base.h"
#include "../command_func.h"
#include "../economy_base.h"
#include "../map_func.h"
#include "../tile_map.h"
#include "../cargotype.h"
#include "../genworld.h"
#include "../core/backup_type.hpp"
#include "../table/strings.h"
#include "../window_func.h"
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace {
using Json = nlohmann::json;
constexpr IndustryType IRON = 18; // Native iron-mine layout inherited by Commonwealth 0x12.
constexpr IndustryType SILICA = 0; // Native coal-mine layouts inherited by Commonwealth 0x10.
constexpr IndustryType STEEL = 8; // Native steel-mill layouts inherited by Commonwealth 0x13.
constexpr IndustryType BALLAST = 2; // Native sawmill layouts inherited by Commonwealth 0x11.
constexpr std::array<IndustryType, 4> TYPES{IRON, SILICA, STEEL, BALLAST};

/** Real map/industry/command state, with the pack's declared identities on native substitute layouts.
 * No GenerateWorld/GenerateResourceSites, production evaluation, GameLoop or engine process is used.
 * AddSite here supplies the unit fixture's persistent bounds; production authoring cannot create sites.
 */
struct FundingFixture {
	GameSettings old_settings = _settings_game;
	std::array<IndustrySpec, 4> old_specs;
	std::array<CargoSpec, NUM_CARGO> old_cargos;
	Money old_raw_price = _price[Price::BuildIndustryRaw];
	Money old_processing_price = _price[Price::BuildIndustry];

	FundingFixture()
	{
		for (size_t n = 0; n < TYPES.size(); ++n) old_specs[n] = *GetIndustrySpec(TYPES[n]);
		for (size_t n = 0; n < NUM_CARGO; ++n) old_cargos[n] = *CargoSpec::Get(CargoType(n));
		FirstPermanentLine::Reset();
		ResourceSiteManager::Reset();
		StellarNetwork::Reset();
		IntegratedEconomy::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 2000);
		ResetIndustries();
		Map::CountLandTiles();
		_settings_game.economy.multiple_industry_per_town = true;
		_settings_game.economy.industry_cargo_scale = 100;
		_settings_game.game_creation.land_generator = LG_TERRAGENESIS;
		_settings_game.construction.industry_platform = 1;
		_price[Price::BuildIndustryRaw] = _price[Price::BuildIndustry] = 100000;
		ResourceSiteManager::SetEnabled(true);
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Frontier);
		for (auto [cargo, label] : {std::pair{CargoType{0}, CargoLabel{"IRON"}}, {CargoType{1}, CargoLabel{"SILC"}},
				std::pair{CargoType{2}, CargoLabel{"STEL"}}, {CargoType{3}, CargoLabel{"BALL"}}}) {
			CargoSpec::Get(cargo)->label = label;
		}
		BuildCargoLabelMap();
		for (auto [type, local, input, output] : {
				std::tuple{IRON, uint16_t{0x12}, INVALID_CARGO, CargoType{0}},
				std::tuple{SILICA, uint16_t{0x10}, INVALID_CARGO, CargoType{1}},
				std::tuple{STEEL, uint16_t{0x13}, CargoType{0}, CargoType{2}},
				std::tuple{BALLAST, uint16_t{0x11}, CargoType{1}, CargoType{3}}}) {
			auto *spec = const_cast<IndustrySpec *>(GetIndustrySpec(type));
			spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
			spec->grf_prop.local_id = local;
			spec->produced_cargo.fill(INVALID_CARGO); spec->produced_cargo[0] = output;
			spec->accepts_cargo.fill(INVALID_CARGO); spec->accepts_cargo[0] = input;
			REQUIRE(spec->enabled);
			REQUIRE_FALSE(spec->layouts.empty());
		}
	}
	~FundingFixture()
	{
		UnInitWindowSystem();
		ResourceSiteManager::Reset();
		IntegratedEconomy::Reset();
		StellarNetwork::Reset();
		for (size_t n = 0; n < TYPES.size(); ++n) *const_cast<IndustrySpec *>(GetIndustrySpec(TYPES[n])) = old_specs[n];
		for (size_t n = 0; n < NUM_CARGO; ++n) *CargoSpec::Get(CargoType(n)) = old_cargos[n];
		BuildCargoLabelMap();
		_settings_game = old_settings;
		_price[Price::BuildIndustryRaw] = old_raw_price;
		_price[Price::BuildIndustry] = old_processing_price;
	}
	uint32_t Site(IndustryType type, TileIndex tile = TileXY(30, 30), uint size = 16)
	{
		const auto id = ResourceSiteManager::AddSite(tile, type, size, size);
		REQUIRE(id != 0);
		return id;
	}
	void Role(EconomicRole role)
	{
		// RegisterRole deliberately retains an existing role; reset this fixture's role table.
		IntegratedEconomy::Reset();
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
		IntegratedEconomy::RegisterRole(WorldID{0}, role);
	}
};

bool HasError(const Json &report, std::string_view stage, StringID id)
{
	for (const auto &r : report.at("rejections")) {
		if (r.at("stage") == stage && r.at("error_id") == id.base()) return true;
	}
	return false;
}
}

TEST_CASE("First Line funding exposes missing source candidates without granting sites", "[first-line-funding]")
{
	FundingFixture fixture;
	Json report;
	const Money before = Company::Get(CompanyID{0})->money;
	CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
	CHECK(report.at("candidate_count") == 0);
	CHECK(ResourceSiteManager::Sites().empty());
	CHECK(ResourceSiteManager::Surveys().empty());
	CHECK(Industry::GetNumItems() == 0);
	CHECK(Company::Get(CompanyID{0})->money == before);
}

TEST_CASE("First Line native iron generation probe and ordinary funding disagree on a refused slope", "[first-line-funding]")
{
	FundingFixture fixture;
	const TileIndex anchor = TileXY(30, 30);
	fixture.Site(IRON, anchor);
	SetTileHeight(TileXY(31, 31), 1);
	const Money before = Company::Get(CompanyID{0})->money;
	{
		// Only a query on the fixed fixture; no world or site generation and no platform leveling.
		AutoRestoreBackup generating(_generating_world, true);
		REQUIRE(Command<Commands::BuildIndustry>::Do({}, anchor, IRON, 0, false, 1).Succeeded());
	}
	CHECK(TileHeight(TileXY(31, 31)) == 1);
	CHECK(Industry::GetNumItems() == 0);
	CHECK(Company::Get(CompanyID{0})->money == before);
	REQUIRE(Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, anchor).Succeeded());
	const auto quote = Command<Commands::BuildIndustry>::Do({}, anchor, IRON, 0, false, 1);
	CHECK(quote.GetErrorMessage() == STR_ERROR_SITE_UNSUITABLE);
	CHECK(Industry::GetNumItems() == 0);
}

TEST_CASE("First Line native resource search finds legal interior and occupies only its original site", "[first-line-funding]")
{
	for (IndustryType type : {IRON, SILICA}) {
		CAPTURE(type);
		FundingFixture fixture;
		const TileIndex anchor = TileXY(30, 30);
		const auto site_id = fixture.Site(type, anchor);
		SetTileHeight(TileXY(31, 31), 1);
		REQUIRE(Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, anchor).Succeeded());
		REQUIRE(Command<Commands::BuildIndustry>::Do({}, anchor, type, 0, false, 1).Failed());
		const Money before = Company::Get(CompanyID{0})->money;
		Json report;
		Industry *industry = FirstLineFundIndustry(type, WorldID{0}, anchor, true, report);
		INFO(report.dump());
		REQUIRE(industry != nullptr);
		CHECK(industry->location.tile != anchor);
		CHECK(industry->founder == CompanyID{0});
		CHECK(industry->construction_type == IndustryConstructionType::Gameplay);
		REQUIRE(ResourceSiteManager::Sites().size() == 1);
		const auto &site = ResourceSiteManager::Sites().front();
		CHECK(site.id == site_id);
		CHECK(site.anchor == anchor);
		CHECK(site.occupant == industry->index);
		CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{1}, site));
		CHECK(Company::Get(CompanyID{0})->money == before - GetIndustrySpec(type)->GetConstructionCost());
		// The successful ordinary native build may level its surrounding platform normally.
		CHECK(HasError(report, "build_quote", STR_ERROR_SITE_UNSUITABLE));
		CHECK(report.at("attempted_candidates").get<uint>() > 1);
	}
}

TEST_CASE("First Line source diagnostics distinguish survey boundary and cash failures", "[first-line-funding]")
{
	FundingFixture fixture;
	SECTION("Native survey rejects a small edge site crossing the world boundary") {
		fixture.Site(IRON, TileXY(55, 55), 4);
		Json report;
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(55, 55), true, report) == nullptr);
		INFO(report.dump());
		CHECK(HasError(report, "survey_quote", STR_RESOURCE_SURVEY_BOUNDARY));
	}
	SECTION("Normal survey quote checks cash before mutation") {
		fixture.Site(IRON);
		Company::Get(CompanyID{0})->money = 0;
		Json report;
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
		INFO(report.dump());
		CHECK(HasError(report, "survey_quote", STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY));
		CHECK(Company::Get(CompanyID{0})->money == 0);
	}
	CHECK(ResourceSiteManager::Surveys().empty());
	CHECK(Industry::GetNumItems() == 0);
}

TEST_CASE("First Line funding retains native research world-role and cash rejection", "[first-line-funding]")
{
	FundingFixture fixture;
	fixture.Site(IRON);
	REQUIRE(Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, TileXY(30, 30)).Succeeded());
	Json report;
	SECTION("Wrong economic role") {
		fixture.Role(EconomicRole::Core);
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
		CHECK(HasError(report, "build_quote", STR_ERROR_PRODUCTION_WORLD));
	}
	SECTION("Advanced resource research remains mandatory") {
		CargoSpec::Get(CargoType{0})->label = CargoLabel{"RARE"};
		BuildCargoLabelMap();
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
		CHECK(HasError(report, "build_quote", STR_ERROR_COMMONWEALTH_RESEARCH));
	}
	SECTION("Build quote checks cash even after paid survey") {
		Company::Get(CompanyID{0})->money = 0;
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
		CHECK(HasError(report, "build_quote", STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY));
	}
	INFO(report.dump());
	CHECK(Industry::GetNumItems() == 0);
	CHECK(report.at("samples").size() <= 16);
}

TEST_CASE("First Line processor search obeys Materials I and ballast native funding", "[first-line-funding]")
{
	for (IndustryType type : {STEEL, BALLAST}) {
		CAPTURE(type);
		FundingFixture fixture;
		fixture.Role(EconomicRole::Industrial);
		Json report;
		if (type == STEEL) {
			CHECK(FirstLineFundIndustry(type, WorldID{0}, TileXY(30, 30), false, report) == nullptr);
			CHECK(HasError(report, "build_quote", STR_ERROR_COMMONWEALTH_RESEARCH));
			TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1});
		}
		Industry *industry = FirstLineFundIndustry(type, WorldID{0}, TileXY(30, 30), false, report);
		INFO(report.dump());
		REQUIRE(industry != nullptr);
		CHECK(industry->type == type);
		CHECK(industry->founder == CompanyID{0});
		CHECK(ResourceSiteManager::Sites().empty());
		CHECK(ResourceSiteManager::Surveys().empty());
	}
}

TEST_CASE("First Line funding rejects invalid actors requests and generation-mode bypass", "[first-line-funding]")
{
	FundingFixture fixture;
	fixture.Site(IRON);
	Json report;
	for (CompanyID actor : {COMPANY_SPECTATOR, OWNER_DEITY, CompanyID::Invalid()}) {
		AutoRestoreBackup company(_current_company, actor);
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
	}
	CHECK(FirstLineFundIndustry(IT_INVALID, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
	CHECK(FirstLineFundIndustry(IRON, INVALID_WORLD, TileXY(30, 30), true, report) == nullptr);
	CHECK(FirstLineFundIndustry(IRON, WorldID{0}, INVALID_TILE, true, report) == nullptr);
	CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), false, report) == nullptr);
	{
		AutoRestoreBackup generating(_generating_world, true);
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
	}
	CHECK(ResourceSiteManager::Surveys().empty());
	CHECK(Industry::GetNumItems() == 0);
}

TEST_CASE("First Line paid survey and occupied site cannot be duplicated by another company", "[first-line-funding]")
{
	FundingFixture fixture;
	fixture.Site(IRON);
	const Money before = Company::Get(CompanyID{0})->money;
	Json report;
	Industry *industry = FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report);
	REQUIRE(industry != nullptr);
	CHECK(report.at("survey_cost") == 1000);
	CHECK(Company::Get(CompanyID{0})->money == before - 1000 - GetIndustrySpec(IRON)->GetConstructionCost());
	CHECK(ResourceSiteManager::Surveys().at(CompanyID{0}).size() == 1);
	const Money other_before = Company::Get(CompanyID{1})->money;
	{
		AutoRestoreBackup company(_current_company, CompanyID{1});
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
	}
	CHECK(report.at("status") == "no_candidates");
	CHECK(report.at("sites").at("matching") == 1);
	CHECK(report.at("sites").at("occupied") == 1);
	CHECK(report.at("build_executes") == 0);
	CHECK(report.at("survey_executes") == 0);
	CHECK(Company::Get(CompanyID{1})->money == other_before);
	CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{1}, ResourceSiteManager::Sites().front()));
	CHECK(Industry::GetNumItems() == 1);
}

TEST_CASE("First Line exhausted native terrain diagnostics are deterministic and bounded", "[first-line-funding]")
{
	std::string first_report;
	for (int repeat = 0; repeat < 2; ++repeat) {
		FundingFixture fixture;
		fixture.Site(IRON);
		for (uint y = 28; y < 50; ++y) for (uint x = 28; x < 50; ++x) SetTileHeight(TileXY(x, y), (x + y) % 2);
		Json report;
		CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
		INFO(report.dump());
		CHECK(report.at("status") == "build_quotes_exhausted");
		CHECK(report.at("candidate_count") == 256);
		CHECK(report.at("attempted_candidates") == 256);
		CHECK(report.at("survey_executes") == 1);
		CHECK(report.at("build_executes") == 0);
		CHECK(report.at("samples").size() == 16);
		CHECK(HasError(report, "build_quote", STR_ERROR_SITE_UNSUITABLE));
		CHECK(report.at("site_catalog").size() == 1);
		CHECK(Industry::GetNumItems() == 0);
		if (repeat == 0) first_report = report.dump();
		else CHECK(report.dump() == first_report);
	}
}

TEST_CASE("First Line funding cannot add authored assets after its baseline", "[first-line-funding]")
{
	FundingFixture fixture;
	fixture.Site(IRON);
	REQUIRE(FirstPermanentLine::Load(R"({"schema":1,"active":true,"economy_evaluations":0,"invalid":false,"awarded":false,"reason":"","binding":[0,0,0,1,2,3],"delivered":[false,false],"produced":[false,false],"stored":[0,0],"commissioned":false,"house_count":0,"houses":[],"journeys":[]})"));
	const Money before = Company::Get(CompanyID{0})->money;
	Json report;
	CHECK(FirstLineFundIndustry(IRON, WorldID{0}, TileXY(30, 30), true, report) == nullptr);
	CHECK_FALSE(report.at("admission").at("prebaseline").get<bool>());
	CHECK(ResourceSiteManager::Surveys().empty());
	CHECK(Industry::GetNumItems() == 0);
	CHECK(Company::Get(CompanyID{0})->money == before);
	FirstPermanentLine::Reset();
}
