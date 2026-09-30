/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_integrated_economy.cpp Integrated economy conservation and progression regressions. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cargotype.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../industry.h"
#include "../engine_func.h"
#include "../train.h"
#include "../vehicle_func.h"
#include "../depot_base.h"
#include "../signal_func.h"
#include "../train_cmd.h"
#include "../vehicle_cmd.h"
#include "../autoreplace_cmd.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/company_stockpile.h"
#include "../portal/fabrication_manager.h"
#include "../portal/integrated_economy.h"
#include "../portal/federation_identity.h"
#include "../portal/universe_network.h"
#include "../portal/portal_cmd.h"
#include "../portal/planet_manager.h"
#include "../rail_cmd.h"
#include "../blueprint/blueprint.h"
#include "../blueprint/blueprint_cmd.h"
#include "../rail_map.h"
#include "../settings_type.h"
#include "../town.h"
#include "../window_func.h"
#include "../safeguards.h"
extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
namespace
{
struct EconomyFixture {
	IndustrySpec saved_spec;
	EconomyFixture()
	{
		IntegratedEconomy::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
		const char *labels[] = {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
								"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"};
		for (uint8_t c = 0; c < std::size(labels); ++c) {
			CargoLabel label;
			std::copy_n(labels[c], 4, label.begin());
			CargoSpec::Get(CargoType(c))->label = label;
			CargoSpec::Get(CargoType(c))->bitnum = c;
		}
		BuildCargoLabelMap();
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
		saved_spec = *GetIndustrySpec(IndustryType{0});
	}
	~EconomyFixture()
	{
		UpdateSignalsInBuffer();
		_vehicle_pool.CleanPool();
		ResetVehicleHash();
		_depot_pool.CleanPool();
		IntegratedEconomy::Reset();
		_industry_pool.CleanPool();
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = saved_spec;
		SetupCargoForClimate(LandscapeType::Temperate);
		ProductionChainManager::InitDefaultRecipes();
		UnInitWindowSystem();
	}
	Industry *Factory(uint8_t local)
	{
		auto spec = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}));
		spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		spec->grf_prop.local_id = local;
		REQUIRE(Industry::CanAllocateItem());
		auto i = Industry::Create(TileXY(30, 30));
		i->type = IndustryType{0};
		i->founder = CompanyID{0};
		auto recipe = ProductionChainManager::GetRecipe(IntegratedEconomy::IndustryRecipe(i->type));
		REQUIRE(recipe != nullptr);
		for (auto [c, n] : recipe->inputs)
			i->accepted.push_back({.cargo = c});
		for (auto [c, n] : recipe->outputs)
			i->produced.push_back({.cargo = c});
		IntegratedEconomy::RegisterIndustry(i);
		return i;
	}
};
CargoType C(const char (&label)[5])
{
	return GetCargoTypeByLabel(CargoLabel{label});
}
} // namespace
TEST_CASE("Integrated factories conserve cargo and never deposit remotely", "[integrated-economy]")
{
	EconomyFixture fixture;
	auto id = GENERATE(0x11, 0x13, 0x15, 0x17, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21);
	auto i = fixture.Factory(id);
	auto technology = IntegratedEconomy::RecipeTech(IntegratedEconomy::IndustryRecipe(i->type));
	if (technology != TECH_NONE) TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {technology});
	auto recipe = ProductionChainManager::GetRecipe(IntegratedEconomy::IndustryRecipe(i->type));
	for (auto [c, n] : recipe->inputs)
		CHECK(IntegratedEconomy::AcceptIndustry(i, c, 100000) == 300 * n);
	for (int month = 0; month < 3; ++month)
		IntegratedEconomy::Produce();
	for (auto [c, n] : recipe->inputs)
		CHECK(i->GetCargoAccepted(c)->waiting == 0);
	for (auto [c, n] : recipe->outputs) {
		CHECK(i->GetCargoProduced(c)->waiting == (technology == TECH_MATERIALS_3 ? 345 : 300) * n);
		CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, c) == 0);
	}
	for (auto [c, n] : recipe->inputs)
		IntegratedEconomy::AcceptIndustry(i, c, 100000);
	IntegratedEconomy::Produce();
	for (auto [c, n] : recipe->inputs)
		CHECK(i->GetCargoAccepted(c)->waiting == 300 * n);
	CHECK(IntegratedEconomy::Factories().at(i->index).last_batches == 0);
}
TEST_CASE("Integrated cities consume complete baskets and retain unused goods", "[integrated-economy]")
{
	EconomyFixture fixture;
	auto town = *Town::Iterate().begin();
	town->cache.population = 1001;
	auto demand = IntegratedEconomy::CityDemand(town->index);
	CHECK(demand.at(C("FOOD")) == 51);
	float growth = 99, passengers = 99;
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(growth == 0);
	CHECK(passengers == 0.5f);
	for (auto [c, n] : demand)
		if (c != C("FOOD")) CHECK(IntegratedEconomy::AcceptCity(town->index, c, 10000, true) == n);
	IntegratedEconomy::EvaluateCity(town->index, growth, passengers);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("STEL")) == 20);
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 10000, false) == 153);
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 10000, true) == 153);
	IntegratedEconomy::EvaluateCity(town->index, growth, passengers);
	CHECK(growth == 2);
	CHECK(passengers == 1.5f);
	IntegratedEconomy::EvaluateCity(town->index, growth, passengers);
	CHECK(growth == 0);
	CHECK(passengers == 1);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 51);
	auto state = IntegratedEconomy::Save();
	IntegratedEconomy::Reset();
	REQUIRE(IntegratedEconomy::Load(state));
	CHECK(IntegratedEconomy::Save() == state);
}
TEST_CASE("Integrated research waits for atomic kit and completes without "
		  "further spending",
		  "[integrated-economy]")
{
	EconomyFixture fixture;
	const CompanyID company{0};
	const WorldID world{0};
	TechTreeManager::RestoreCompanyTech(company, TECH_MATERIALS_3, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2});
	TechTreeManager::AddResearchPoints(company, 1000000);
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(company, TECH_MATERIALS_3));
	StockpileManager::AddCargo(world, company, C("STEL"), 40);
	CHECK_FALSE(IntegratedEconomy::PrepareResearch(company, TECH_MATERIALS_3));
	CHECK(StockpileManager::GetStock(world, company, C("STEL")) == 40);
	StockpileManager::AddCargo(world, company, C("CHIP"), 20);
	REQUIRE(IntegratedEconomy::PrepareResearch(company, TECH_MATERIALS_3));
	CHECK(StockpileManager::GetStock(world, company, C("STEL")) == 0);
	IntegratedEconomy::CancelResearch(company);
	CHECK(StockpileManager::GetStock(world, company, C("STEL")) == 40);
	TechTreeManager::ProcessMonthlyResearch();
	CHECK(TechTreeManager::IsTechUnlocked(company, TECH_MATERIALS_3));
	CHECK(StockpileManager::GetStock(world, company, C("STEL")) == 0);
	TechTreeManager::ProcessMonthlyResearch();
	CHECK(StockpileManager::GetStock(world, company, C("CHIP")) == 0);
	CHECK_FALSE(IntegratedEconomy::Accelerates(company));
	CHECK(FabricationManager::UseForRail(company, RAILTYPE_ELECTRIC));
	CHECK_FALSE(FabricationManager::UseForRail(company, RAILTYPE_RAIL));
}
TEST_CASE("Integrated roles remain immutable and legacy rules do not require kits", "[integrated-economy]")
{
	EconomyFixture fixture;
	IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Frontier);
	CHECK(IntegratedEconomy::Role(WorldID{0}) == EconomicRole::Core);
	IntegratedEconomy::Reset();
	CHECK(IntegratedEconomy::ResearchKit(TECH_MATERIALS_4).empty());
	CHECK_FALSE(FabricationManager::UseForRail(CompanyID{0}, RAILTYPE_ELECTRIC));
}

TEST_CASE("Integrated yield is independent of batch splitting and survives reload", "[integrated-economy]")
{
	EconomyFixture fixture;
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3});
	auto i = fixture.Factory(0x13);
	const bool split = GENERATE(false, true);
	uint output = 0;
	for (uint step = 0; step < (split ? 20u : 1u); ++step) {
		REQUIRE(IntegratedEconomy::AcceptIndustry(i, C("IRON"), split ? 2 : 40) == (split ? 2 : 40));
		IntegratedEconomy::Produce();
		output += i->GetCargoProduced(C("STEL"))->waiting;
		i->GetCargoProduced(C("STEL"))->waiting = 0;
		auto saved = IntegratedEconomy::Save();
		REQUIRE(IntegratedEconomy::Load(saved));
	}
	CHECK(output == 23);
	CHECK(IntegratedEconomy::Factories().at(i->index).remainder.at(C("STEL")) == 0);
}
TEST_CASE("Integrated chip production waits for every physical input", "[integrated-economy]")
{
	EconomyFixture fixture;
	auto i = fixture.Factory(0x19);
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_2});
	IntegratedEconomy::AcceptIndustry(i, C("SAND"), 200);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("WIRE"), 100);
	IntegratedEconomy::Produce();
	CHECK(i->GetCargoAccepted(C("SAND"))->waiting == 200);
	CHECK(i->GetCargoProduced(C("CHIP"))->waiting == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("WIRE")) == 100);
	IntegratedEconomy::AcceptIndustry(i, C("WIRE"), 1);
	IntegratedEconomy::Produce();
	CHECK(i->GetCargoProduced(C("CHIP"))->waiting == 1);
	CHECK(i->GetCargoAccepted(C("SAND"))->waiting == 198);
}
TEST_CASE("Integrated malformed state is rejected without changing the live economy", "[integrated-economy]")
{
	EconomyFixture fixture;
	fixture.Factory(0x13);
	auto saved = IntegratedEconomy::Save();
	auto doc = nlohmann::json::parse(saved);
	SECTION("unknown version")
	{
		doc["version"] = 2;
	}
	SECTION("wrapped role")
	{
		doc["roles"][0][1] = 256;
	}
	SECTION("duplicate identity")
	{
		doc["factories"].push_back(doc["factories"][0]);
	}
	SECTION("invalid capacity")
	{
		doc["factories"][0][2] = 99;
	}
	SECTION("negative cargo")
	{
		doc["factories"][0][4] = nlohmann::json::array({{-1, 0}});
	}
	CHECK_FALSE(IntegratedEconomy::Load(doc.dump()));
	CHECK(IntegratedEconomy::Save() == saved);
}

TEST_CASE("Integrated cargo map bounds preserve exact values and reject overflow atomically", "[integrated-economy][ci-bounds]")
{
	EconomyFixture fixture;
	auto doc = nlohmann::json::parse(IntegratedEconomy::Save());
	doc["cities"] = nlohmann::json::array({{0, {{C("FOOD"), UINT32_MAX}}, {{C("FOOD"), UINT32_MAX}}}});
	REQUIRE(IntegratedEconomy::Load(doc.dump()));
	const auto saved = IntegratedEconomy::Save();
	const auto roundtrip = nlohmann::json::parse(saved);
	CHECK(roundtrip["cities"][0][1][0][1] == UINT32_MAX);
	CHECK(roundtrip["cities"][0][2][0][1] == UINT32_MAX);
	for (size_t field : {1u, 2u}) {
		for (uint64_t overflow : {uint64_t{UINT32_MAX} + 1, UINT64_MAX}) {
			auto invalid = roundtrip;
			invalid["cities"][0][field][0][1] = overflow;
			CHECK_FALSE(IntegratedEconomy::Load(invalid.dump()));
			CHECK(IntegratedEconomy::Save() == saved);
		}
	}
}

TEST_CASE("Integrated electric construction and conversion have atomic "
		  "material previews",
		  "[integrated-economy]")
{
	EconomyFixture fixture;
	CompanyID company{0};
	WorldID world{0};
	auto tile = TileXY(35, 35);
	ResetRailTypes();
	Company::Get(company)->clear_limit = 1000 << 16;
	Company::Get(company)->avail_railtypes.Set({RAILTYPE_RAIL, RAILTYPE_ELECTRIC});
	TechTreeManager::RestoreCompanyTech(company, TECH_NONE, 0, 0,
										{TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_TRACTION_1, TECH_TRACTION_2, TECH_TRACTION_3});
	CHECK_FALSE(Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_ELECTRIC, Track::X, false).Succeeded());
	CHECK_FALSE(IsPlainRailTile(tile));
	auto bill = FabricationManager::GetTrackBOM(RAILTYPE_ELECTRIC);
	for (auto [c, n] : bill.materials)
		StockpileManager::AddCargo(world, company, c, n);
	INFO(Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_ELECTRIC, Track::X, false).GetErrorMessage().base());
	REQUIRE(Command<Commands::BuildRail>::Do({}, tile, RAILTYPE_ELECTRIC, Track::X, false).Succeeded());
	for (auto [c, n] : bill.materials)
		CHECK(StockpileManager::GetStock(world, company, c) == n);
	REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, tile, RAILTYPE_ELECTRIC, Track::X, false).Succeeded());
	for (auto [c, n] : bill.materials)
		CHECK(StockpileManager::GetStock(world, company, c) == 0);
	auto first = TileXY(37, 35), second = TileXY(38, 35);
	REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, first, RAILTYPE_RAIL, Track::X, false).Succeeded());
	REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute, second, RAILTYPE_RAIL, Track::X, false).Succeeded());
	for (auto [c, n] : bill.materials)
		StockpileManager::AddCargo(world, company, c, n);
	CHECK_FALSE(Command<Commands::ConvertRail>::Do({}, first, second, RAILTYPE_ELECTRIC, false).Succeeded());
	CHECK(GetRailType(first) == RAILTYPE_RAIL);
	CHECK(GetRailType(second) == RAILTYPE_RAIL);
	for (auto [c, n] : bill.materials)
		CHECK(StockpileManager::GetStock(world, company, c) == n);
	for (auto [c, n] : bill.materials)
		StockpileManager::AddCargo(world, company, c, n);
	REQUIRE(Command<Commands::ConvertRail>::Do(DoCommandFlag::Execute, first, second, RAILTYPE_ELECTRIC, false).Succeeded());
	for (auto [c, n] : bill.materials)
		CHECK(StockpileManager::GetStock(world, company, c) == 0);
}
TEST_CASE("Integrated extraction permission survives development promotion", "[integrated-economy]")
{
	EconomyFixture fixture;
	IntegratedEconomy::Reset();
	IntegratedEconomy::SetEnabled(true);
	IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Frontier);
	const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}))->life_type = IndustryLifeType::Extractive;
	REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase3_Frontier));
	CHECK(PlanetManager::CheckEconomicIndustry(TileXY(30, 30), IndustryType{0}, CompanyID{0}).Succeeded());
	REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase1_Core));
	CHECK(IntegratedEconomy::Role(WorldID{0}) == EconomicRole::Frontier);
	CHECK(PlanetManager::CheckEconomicIndustry(TileXY(30, 30), IndustryType{0}, CompanyID{0}).Succeeded());
}

TEST_CASE("Integrated accounting distinguishes reservations, consumption and failed bills", "[integrated-economy]")
{
	EconomyFixture fixture;
	const auto initial = IntegratedEconomy::Accounting();
	CHECK_FALSE(StockpileManager::ConsumeBOM(WorldID{0}, CompanyID{0}, {{C("STEL"), 1}}));
	CHECK(IntegratedEconomy::Accounting() == initial);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 40);
	REQUIRE(StockpileManager::ConsumeBOM(WorldID{0}, CompanyID{0}, {{C("STEL"), 20}}, false));
	CHECK(IntegratedEconomy::Accounting() == initial);
	REQUIRE(StockpileManager::ConsumeBOM(WorldID{0}, CompanyID{0}, {{C("STEL"), 20}}));
	CHECK(IntegratedEconomy::Accounting()[static_cast<size_t>(EconomyFlow::Consumed)][C("STEL")] == 20);
	const auto saved = IntegratedEconomy::Save();
	IntegratedEconomy::Reset();
	REQUIRE(IntegratedEconomy::Load(saved));
	CHECK(IntegratedEconomy::Accounting()[static_cast<size_t>(EconomyFlow::Consumed)][C("STEL")] == 20);
}

TEST_CASE("Integrated factory upgrades and ownership transfer preserve physical stock", "[integrated-economy]")
{
	EconomyFixture fixture;
	auto industry = fixture.Factory(0x11);
	REQUIRE(IntegratedEconomy::AcceptIndustry(industry, C("SILC"), 700) == 600);
	auto before = IntegratedEconomy::Save();
	REQUIRE(Command<Commands::ManageEconomyFactory>::Do({}, industry->index, false).Succeeded());
	CHECK(IntegratedEconomy::Save() == before);
	REQUIRE(Command<Commands::ManageEconomyFactory>::Do(DoCommandFlag::Execute, industry->index, false).Succeeded());
	CHECK(IntegratedEconomy::IndustrySpace(industry, C("SILC")) == 300);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 12);
	IntegratedEconomy::ChangeCompany(CompanyID{0}, CompanyID{1});
	CHECK(industry->founder == CompanyID{1});
	CHECK(IntegratedEconomy::Factories().at(industry->index).owner == CompanyID{1});
	CHECK(industry->GetCargoAccepted(C("SILC"))->waiting == 600);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{1}, C("STEL")) == 12);
	CHECK(Command<Commands::ManageEconomyFactory>::Do(DoCommandFlag::Execute, industry->index, false).Failed());
	IntegratedEconomy::ChangeCompany(CompanyID{1}, INVALID_OWNER);
	CHECK(industry->founder == OWNER_NONE);
	CHECK(IntegratedEconomy::Factories().at(industry->index).owner == OWNER_NONE);
	CHECK(industry->GetCargoAccepted(C("SILC"))->waiting == 600);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{1}, C("STEL")) == 0);
	CHECK(IntegratedEconomy::Accounting()[static_cast<size_t>(EconomyFlow::Discarded)][C("STEL")] == 12);
	REQUIRE(IntegratedEconomy::Load(IntegratedEconomy::Save()));
}

TEST_CASE("Integrated confirmed research is monotonic and survives an offline reload", "[integrated-economy]")
{
	EconomyFixture fixture;
	UniverseNetwork::Reset();
	FederationIdentityRegistry::Reset();
	FederationIdentityRegistry::RestoreState({2, 2}, 1);
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{0}, 1, {1, 1}));
	nlohmann::json record = {{"id", "1:1:1"}, {"home", "1:1"},	{"namespace", "1:1"},
							 {"ruleset", 1u}, {"revision", 1u}, {"unlocks", {TECH_MATERIALS_1}}};
	REQUIRE(IntegratedEconomy::ApplyResearch(record, false));
	CHECK_FALSE(IntegratedEconomy::SharedUnlock(CompanyID{0}, TECH_MATERIALS_1));
	REQUIRE(IntegratedEconomy::ApplyResearch(record, true));
	CHECK_FALSE(IntegratedEconomy::CanConductResearch(CompanyID{0}));
	CHECK(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_1));
	const auto before = IntegratedEconomy::Save();
	REQUIRE(IntegratedEconomy::ApplyResearch(record, true));
	CHECK(IntegratedEconomy::Save() == before);
	auto cached = record;
	cached["kind"] = "research";
	cached["manifest"] = "manifest-not-yet-loaded";
	cached["online"] = false;
	const std::string key = "1:1/research/1:1:1";
	CHECK_FALSE(UniverseNetwork::Apply(key, cached, false));
	auto directory_save = nlohmann::json::parse(UniverseNetwork::Save());
	directory_save["records"][key] = cached;
	REQUIRE(UniverseNetwork::Load(directory_save.dump()));
	CHECK(IntegratedEconomy::Save() == before);
	auto malformed_directory = directory_save;
	malformed_directory["records"][key]["online"] = "true";
	CHECK_FALSE(UniverseNetwork::Load(malformed_directory.dump()));
	malformed_directory = directory_save;
	malformed_directory["records"][key].erase("manifest");
	CHECK_FALSE(UniverseNetwork::Load(malformed_directory.dump()));
	CHECK(IntegratedEconomy::SharedUnlock(CompanyID{0}, TECH_MATERIALS_1));
	auto conflict = record;
	conflict["home"] = "2:2";
	conflict["namespace"] = "2:2";
	CHECK_FALSE(IntegratedEconomy::ApplyResearch(conflict, true));
	auto rollback = record;
	rollback["revision"] = 0u;
	rollback["unlocks"] = nlohmann::json::array();
	CHECK_FALSE(IntegratedEconomy::ApplyResearch(rollback, true));
	REQUIRE(IntegratedEconomy::Load(before));
	CHECK(IntegratedEconomy::SharedUnlock(CompanyID{0}, TECH_MATERIALS_1));
	CHECK_FALSE(IntegratedEconomy::CanConductResearch(CompanyID{0}));
	FederationIdentityRegistry::Reset();
	UniverseNetwork::Reset();
}

TEST_CASE("Legacy research completion leaves the integrated chunk disabled and reloadable", "[integrated-economy]")
{
	EconomyFixture fixture;
	IntegratedEconomy::Reset();
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_MATERIALS_1, 0, 0, {});
	TechTreeManager::AddResearchPoints(CompanyID{0}, 100);
	CHECK(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_1));
	auto saved = IntegratedEconomy::Save();
	REQUIRE(IntegratedEconomy::Load(saved));
	CHECK_FALSE(IntegratedEconomy::Enabled());
	CHECK(nlohmann::json::parse(saved)["research"].empty());
}

TEST_CASE("Integrated consist cloning and replacement quote all engines atomically", "[integrated-economy]")
{
	EconomyFixture fixture;
	ResetRailTypes();
	Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_RAIL);
	Company::Get(CompanyID{0})->clear_limit = 1000 << 16;
	_settings_game.vehicle.max_trains = 20;
	_settings_game.vehicle.max_train_length = 20;
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_TRACTION_1});
	for (auto engine : Engine::Iterate()) engine->info.cargo_type = C("SILC");
	StartupEngines();
	std::vector<EngineID> engines;
	for (const auto engine : Engine::Iterate()) {
		if (engine->type != VehicleType::Train || !IsEngineBuildable(engine->index, VehicleType::Train, CompanyID{0})) continue;
		const auto &info = engine->VehInfo<RailVehicleInfo>();
		if (info.railveh_type == RailVehicleType::Singlehead && info.engclass == EngineClass::Steam) engines.push_back(engine->index);
	}
	REQUIRE(engines.size() >= 2);
	const auto depot = TileXY(30, 35);
	REQUIRE(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, DiagDirection::SW).Succeeded());
	auto build = [&](EngineID engine) {
		auto [cost, id, capacity, mail, capacities] = Command<Commands::BuildVehicle>::Do(DoCommandFlag::Execute, depot, engine, false, INVALID_CARGO, ClientID::Invalid);
		INFO("build engine=" << engine.base() << " error=" << cost.GetErrorMessage().base());
		REQUIRE(cost.Succeeded());
		return id;
	};
	auto first = build(engines[0]);
	auto second = build(engines[0]);
	REQUIRE(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, second, first, false).Succeeded());
	FabricationManager::SetFabricateFromStockpile(CompanyID{0}, true);
	const bool replace = GENERATE(false, true);
	if (replace) REQUIRE(Command<Commands::SetAutoreplace>::Do(DoCommandFlag::Execute, ALL_GROUP, engines[0], engines[1], false).Succeeded());
	auto bill = FabricationManager::GetVehicleBOM(Engine::Get(engines[replace ? 1 : 0]));
	auto command = [&](DoCommandFlags flags) {
		return replace ? Command<Commands::AutoreplaceVehicle>::Do(flags, first) : ExtractCommandCost(Command<Commands::CloneVehicle>::Do(flags, depot, first, false));
	};
	for (auto [cargo, units] : bill.materials) StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, cargo, units);
	auto before = IntegratedEconomy::Accounting();
	/* A non-default refit preview temporarily creates native vehicles internally. */
	Command<Commands::BuildVehicle>::Do({}, depot, engines[replace ? 1 : 0], false, C("WIRE"), ClientID::Invalid);
	CHECK(IntegratedEconomy::Accounting() == before);
	for (auto [cargo, units] : bill.materials) CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, cargo) == units);
	CHECK(command({}).Failed());
	CHECK(IntegratedEconomy::Accounting() == before);
	for (auto [cargo, units] : bill.materials) {
		CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, cargo) == units);
		StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, cargo, units);
	}
	INFO("replacement=" << replace);
	REQUIRE(command({}).Succeeded());
	CHECK(IntegratedEconomy::Accounting() == before);
	for (auto [cargo, units] : bill.materials) CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, cargo) == units * 2);
	REQUIRE(command(DoCommandFlag::Execute).Succeeded());
	for (auto [cargo, units] : bill.materials) CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, cargo) == 0);
}

TEST_CASE("Integrated electric blueprints cannot reuse one tile's material bill", "[integrated-economy]")
{
	EconomyFixture fixture;
	ResetRailTypes();
	Company::Get(CompanyID{0})->clear_limit = 1000 << 16;
	Company::Get(CompanyID{0})->avail_railtypes.Set({RAILTYPE_RAIL, RAILTYPE_ELECTRIC});
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_TRACTION_3});
	Blueprint bp;
	bp.name = "Electric material preview";
	bp.width = 2;
	bp.height = 1;
	for (int x = 0; x < 2; ++x) {
		BlueprintTile tile;
		tile.dx = x;
		tile.trackbits = TrackBits{Track::X};
		bp.tiles.push_back(tile);
	}
	auto bill = FabricationManager::GetTrackBOM(RAILTYPE_ELECTRIC);
	for (auto [cargo, units] : bill.materials) StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, cargo, units);
	auto state = IntegratedEconomy::Save();
	auto run = [&](DoCommandFlags flags) { return Command<Commands::PlaceBlueprint>::Do(flags, TileXY(35, 35), bp.ToJson(), RAILTYPE_ELECTRIC, false); };
	CHECK(run({}).Failed());
	CHECK(run(DoCommandFlag::Execute).Failed());
	CHECK_FALSE(IsPlainRailTile(TileXY(35, 35)));
	CHECK(IntegratedEconomy::Save() == state);
	for (auto [cargo, units] : bill.materials) {
		CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, cargo) == units);
		StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, cargo, units);
	}
	REQUIRE(run({}).Succeeded());
	CHECK(IntegratedEconomy::Save() == state);
	REQUIRE(run(DoCommandFlag::Execute).Succeeded());
	CHECK(GetRailType(TileXY(35, 35)) == RAILTYPE_ELECTRIC);
	CHECK(GetRailType(TileXY(36, 35)) == RAILTYPE_ELECTRIC);
	for (auto [cargo, units] : bill.materials) CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, cargo) == 0);
}

TEST_CASE("Integrated city capacity reductions retain unused expansion reserves", "[integrated-economy]")
{
	EconomyFixture fixture;
	auto town = *Town::Iterate().begin();
	town->cache.population = 10000;
	auto demand = IntegratedEconomy::CityDemand(town->index);
	for (auto [cargo, units] : demand) IntegratedEconomy::AcceptCity(town->index, cargo, units, true);
	town->cache.population = 1000;
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("STEL"), 1, true) == 0);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("STEL")) == 100);
	float growth, passengers;
	IntegratedEconomy::EvaluateCity(town->index, growth, passengers);
	CHECK(growth == 2);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("STEL")) == 80);
	/* Prosperity runs out before expansion. The latter still enables ordinary growth. */
	for (int month = 0; month < 4; ++month) IntegratedEconomy::EvaluateCity(town->index, growth, passengers);
	CHECK(growth == 2);
	CHECK(passengers == 1.5f);
	IntegratedEconomy::AcceptCity(town->index, C("STEL"), 20, true);
	IntegratedEconomy::AcceptCity(town->index, C("BALL"), 20, true);
	IntegratedEconomy::EvaluateCity(town->index, growth, passengers);
	CHECK(growth == 1);
	CHECK(passengers == 1);
}
