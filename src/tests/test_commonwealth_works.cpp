/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_commonwealth_works.cpp Capacity or Reserve prototype, stable settlement, and food service regressions. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cargotype.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../industry.h"
#include "../engine_func.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/company_stockpile.h"
#include "../portal/integrated_economy.h"
#include "../portal/megacity_manager.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/production_chain.h"
#include "../portal/tech_tree.h"
#include "../town.h"
#include "../town_cmd.h"
#include "../town_map.h"
#include "../road_map.h"
#include "../clear_map.h"
#include "../newgrf_house.h"
#include "../tile_cmd.h"
#include "../window_func.h"
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace
{
struct WorksTestFixture {
	IndustrySpec saved_spec;
	WorksTestFixture()
	{
		InitWindowSystem();
		IntegratedEconomy::Reset();
		PlanetManager::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
		PlanetManager::Reset();
		PlanetManager::RegisterRegion({.id = WorldID{0},
		        .name = "Core Commonwealth",
		        .phase = WorldPhase::Phase1_Core,
		        .biome = WorldBiome::Temperate,
		        .min_x = 1,
		        .min_y = 1,
		        .max_x = 30,
		        .max_y = 62,
		        .development_score = 10000});
		PlanetManager::RegisterRegion({.id = WorldID{1},
		        .name = "Industrial Commonwealth",
		        .phase = WorldPhase::Phase2_Developed,
		        .biome = WorldBiome::Temperate,
		        .min_x = 31,
		        .min_y = 1,
		        .max_x = 62,
		        .max_y = 62,
		        .development_score = 10000});

		const char *labels[] = {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
								"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"};
		for (uint8_t c = 0; c < std::size(labels); ++c) {
			CargoLabel label;
			std::copy_n(labels[c], 4, label.begin());
			CargoSpec::Get(CargoType(c))->label = label;
			CargoSpec::Get(CargoType(c))->bitnum = c;
		}
		BuildCargoLabelMap();

		Company *c = Company::GetIfValid(CompanyID{0});
		if (c != nullptr) {
			c->money = 100000000;
		}
		_current_company = CompanyID{0};
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
		IntegratedEconomy::RegisterRole(WorldID{1}, EconomicRole::Industrial);

		saved_spec = *GetIndustrySpec(IndustryType{0});
	}
	~WorksTestFixture()
	{
		IntegratedEconomy::Reset();
		PlanetManager::Reset();
		_industry_pool.CleanPool();
		_town_pool.CleanPool();
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = saved_spec;
		SetupCargoForClimate(LandscapeType::Temperate);
		ProductionChainManager::InitDefaultRecipes();
		UnInitWindowSystem();
	}

	Industry *CreateFactory(uint8_t local_id, CompanyID founder = CompanyID{0})
	{
		auto spec = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}));
		spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		spec->grf_prop.local_id = local_id;
		REQUIRE(Industry::CanAllocateItem());
		auto i = Industry::Create(TileXY(35, 30)); // Tile in World 1
		i->type = IndustryType{0};
		i->founder = founder;
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

TEST_CASE("Works prototype rules isolation and compatibility", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	// Reset state
	IntegratedEconomy::Reset();
	CHECK(IntegratedEconomy::ActiveRules() == EconomyRulesVersion::Legacy);

	// Test V1 rules
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V1);
	IntegratedEconomy::SetEnabled(true);
	IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
	IntegratedEconomy::RegisterRole(WorldID{1}, EconomicRole::Industrial);
	CHECK(IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V1);

	auto ind = fixture.CreateFactory(0x11); // Ballast crusher
	REQUIRE(ind != nullptr);

	// Under V1, CommissionEconomyWork and SetTownFoodServiceRate must fail
	auto comm_cost = Command<Commands::CommissionEconomyWork>::Do({}, ind->index, FactoryWorkType::Capacity);
	CHECK(comm_cost.Failed());

	auto rate_cost = Command<Commands::SetTownFoodServiceRate>::Do({}, TownID{0}, 20);
	CHECK(rate_cost.Failed());

	// Under V1, generic upgrade +50 works as usual
	auto upgrade_cost = Command<Commands::ManageEconomyFactory>::Do(DoCommandFlag::Execute, ind->index, false);
	CHECK(upgrade_cost.Succeeded());
	CHECK(IntegratedEconomy::Factories().at(ind->index).capacity == 150);

	// Test V2 rules
	IntegratedEconomy::Reset();
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
	IntegratedEconomy::SetEnabled(true);
	IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
	IntegratedEconomy::RegisterRole(WorldID{1}, EconomicRole::Industrial);
	CHECK(IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2);

	auto ind_v2 = fixture.CreateFactory(0x11);
	REQUIRE(ind_v2 != nullptr);
	CHECK(IntegratedEconomy::Factories().at(ind_v2->index).capacity == 100);

	// Under V2, old generic upgrade bypass is strictly blocked
	auto v2_upgrade = Command<Commands::ManageEconomyFactory>::Do(DoCommandFlag::Execute, ind_v2->index, false);
	CHECK(v2_upgrade.Failed());
	CHECK(IntegratedEconomy::Factories().at(ind_v2->index).capacity == 100);
}

TEST_CASE("Core town stable baseline and optional FOOD service benefit", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	// Create town on World 0 (Core)
	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	town->cache.population = 5000;
	town->cache.num_houses = 200;

	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "Test Metropolis", 5000);

	// Register food service slot initially Off
	auto reg_cost = Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 0);
	REQUIRE(reg_cost.Succeeded());

	auto svc = IntegratedEconomy::GetTownFoodService(town->index);
	REQUIRE(svc != nullptr);
	CHECK(svc->selected_rate == 0);
	CHECK(svc->receiver_capacity == 60);

	// Rate 0 evaluation: baseline passenger 1.0, growth 0.0f (Stable)
	float growth = 99.0f, passengers = 0.0f;
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(growth == 0.0f);
	CHECK(passengers == 1.0f);
	// Recorded at cursor 0: 0 consumed, cursor becomes 1
	CHECK(svc->consumed_history[0] == 0);

	// Rate validation: 10, 20, 40, 0 are valid; other numbers fail
	CHECK(Command<Commands::SetTownFoodServiceRate>::Do({}, town->index, 10).Succeeded());
	CHECK(Command<Commands::SetTownFoodServiceRate>::Do({}, town->index, 20).Succeeded());
	CHECK(Command<Commands::SetTownFoodServiceRate>::Do({}, town->index, 40).Succeeded());
	CHECK(Command<Commands::SetTownFoodServiceRate>::Do({}, town->index, 15).Failed());
	CHECK(Command<Commands::SetTownFoodServiceRate>::Do({}, town->index, 50).Failed());

	// With rate Off, existing food in reserve is retained and not consumed
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 30, true) == 0); // Rejection because rate is 0!
	auto city = IntegratedEconomy::City(town->index);
	CHECK((city == nullptr || !city->reserves.contains(C("FOOD")) || city->reserves.at(C("FOOD")) == 0));

	// Now set rate to 20
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 20).Succeeded());

	// Accept delivery: 30 units accepted
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 30, true) == 30);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 30);

	// Month 1 evaluation: consumes 20, 10 left in reserve
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(growth == 0.0f);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 10);
	CHECK(svc->consumed_history[1] == 20);
	CHECK(passengers == Approx(1.0f + 20.0f / 640.0f));

	// Month 2: deliver 40 units (cap is 60: 60 - 10 = 50 space, 40 accepted -> reserve 50)
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 40, true) == 40);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 50);

	// Month 2 eval with rate 40: consumes 40, 10 left in reserve
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 40).Succeeded());
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(growth == 0.0f);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 10);
	CHECK(svc->consumed_history[2] == 40);
	CHECK(passengers == Approx(1.0f + 60.0f / 640.0f));

	// Month 3: no delivery, reserve 10. Rate 40 consumes 10 (partial consumption!)
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(growth == 0.0f);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 0);
	CHECK(svc->consumed_history[3] == 10);
	CHECK(passengers == Approx(1.0f + 70.0f / 640.0f));

	// Month 4: starvation gap (0 food delivered, 0 in reserve). Consumes 0.
	// CRITICAL: growth remains 0.0f, town does NOT freeze or suffer population decay!
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(growth == 0.0f);
	CHECK(svc->consumed_history[0] == 0); // Overwrites month 0's entry
	CHECK(passengers == Approx(1.0f + 70.0f / 640.0f));
	CHECK(passengers >= 1.0f);

	// Month 5: history rotates (cursor wraps around to 1). Consumes 0.
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(svc->consumed_history[1] == 0); // Overwrites month 1's 20
	CHECK(passengers == Approx(1.0f + 50.0f / 640.0f));
}

TEST_CASE("Town food acceptance and train custody retention", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "Port Metropolis", 5000);

	// Managed city cargo verification
	CHECK(IntegratedEconomy::IsManagedCityCargo(town->index, C("FOOD")));
	CHECK(IntegratedEconomy::IsManagedCityCargo(town->index, C("STEL")));
	CHECK(IntegratedEconomy::IsManagedCityCargo(town->index, C("BALL")));
	CHECK(IntegratedEconomy::IsManagedCityCargo(town->index, C("CHIP")));
	CHECK(IntegratedEconomy::IsManagedCityCargo(town->index, C("CCRY")));
	CHECK_FALSE(IntegratedEconomy::IsManagedCityCargo(town->index, C("COPR")));

	// With rate Off: delivery space is 0
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 50, false) == 0);

	// Set rate to 20
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 20).Succeeded());

	// Receiver capacity is 60 regardless of rate
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 70, false) == 60);
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 45, true) == 45);

	// Receiver has 45 units in reserve. Free space is 60 - 45 = 15
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 30, false) == 15);
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 30, true) == 15);

	// Receiver is now full (60 units). Further delivery accepts 0
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 10, false) == 0);
	CHECK(IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 10, true) == 0);
}

TEST_CASE("Factory Works commissioning consumes BOM atomically with duplicate protection", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	auto ind = fixture.CreateFactory(0x11); // Ballast crusher (recipe 401)
	REQUIRE(ind != nullptr);

	auto &f = IntegratedEconomy::Factories().at(ind->index);
	CHECK(f.capacity == 100);
	CHECK(f.work == FactoryWorkType::None);
	CHECK(f.input_capacity == 300);
	CHECK(f.output_capacity == 300);

	// Attempt commissioning with empty stockpile: fails with NOT_ENOUGH_MATERIALS
	auto fail_no_mat = Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Capacity);
	CHECK(fail_no_mat.Failed());
	CHECK(f.work == FactoryWorkType::None);

	// Add partial materials: 160 STEL, 79 BALL (lacking 1 BALL)
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("BALL"), 79);
	CHECK(Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Capacity).Failed());

	// Add remaining 1 BALL (now 160 STEL, 80 BALL)
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("BALL"), 1);

	// Insufficient company cash
	Company::Get(CompanyID{0})->money = 49999;
	CHECK(Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Capacity).Failed());

	// Sufficient cash £50,000
	Company::Get(CompanyID{0})->money = 100000;
	auto success = Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Capacity);
	REQUIRE(success.Succeeded());

	// Check factory updated to Capacity Works
	CHECK(f.work == FactoryWorkType::Capacity);
	CHECK(f.capacity == 150);
	CHECK(f.input_capacity == 300);
	CHECK(f.output_capacity == 300);

	// Materials atomically consumed from World 1 stockpile
	CHECK(StockpileManager::GetStock(WorldID{1}, CompanyID{0}, C("STEL")) == 0);
	CHECK(StockpileManager::GetStock(WorldID{1}, CompanyID{0}, C("BALL")) == 0);

	// Cash debited (£50,000)
	CHECK(Company::Get(CompanyID{0})->money == 50000);

	// Duplicate commissioning attempt is cleanly rejected: no second debit, no error corruption
	auto dup = Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Capacity);
	CHECK(dup.Failed());
	CHECK(Company::Get(CompanyID{0})->money == 50000);

	// Attempting to switch to Reserve is also rejected
	auto switch_attempt = Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Reserve);
	CHECK(switch_attempt.Failed());
	CHECK(f.work == FactoryWorkType::Capacity);
}

TEST_CASE("Factory Capacity vs Reserve input buffer and throughput models", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	auto ind_cap = fixture.CreateFactory(0x11);
	auto ind_res = fixture.CreateFactory(0x11);

	// Commission Capacity on ind_cap
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("STEL"), 320);
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("BALL"), 160);

	REQUIRE(Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind_cap->index, FactoryWorkType::Capacity).Succeeded());
	REQUIRE(Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind_res->index, FactoryWorkType::Reserve).Succeeded());

	// Ballast crushing recipe (401): 2 SILC -> 2 BALL
	// Input storage cap:
	// Capacity Works: input_capacity 300 * 2 = 600 SILC
	// Reserve Works:  input_capacity 600 * 2 = 1200 SILC
	CHECK(IntegratedEconomy::IndustrySpace(ind_cap, C("SILC")) == 600);
	CHECK(IntegratedEconomy::IndustrySpace(ind_res, C("SILC")) == 1200);

	// Output batch limit with empty buffers:
	// Capacity Works: 150 batches (rate 150)
	// Reserve Works:  100 batches (rate 100)
	CHECK(IntegratedEconomy::OutputBatchLimit(ind_cap) == 150);
	CHECK(IntegratedEconomy::OutputBatchLimit(ind_res) == 100);

	// Large freight feeder delivery (1200 SILC):
	// Capacity Works only accepts 600 SILC (retains custody on train of remaining 600)
	CHECK(IntegratedEconomy::AcceptIndustry(ind_cap, C("SILC"), 1200) == 600);
	// Reserve Works accepts all 1200 SILC!
	CHECK(IntegratedEconomy::AcceptIndustry(ind_res, C("SILC"), 1200) == 1200);

	// Simulate feeder disruption:
	// Month 1:
	IntegratedEconomy::Produce();
	CHECK(IntegratedEconomy::Factories().at(ind_cap->index).last_batches == 150); // 300 SILC used, 300 remain
	CHECK(IntegratedEconomy::Factories().at(ind_res->index).last_batches == 100); // 200 SILC used, 1000 remain

	// Clear outputs so production is not blocked
	ind_cap->GetCargoProduced(C("BALL"))->waiting = 0;
	ind_res->GetCargoProduced(C("BALL"))->waiting = 0;

	// Month 2:
	IntegratedEconomy::Produce();
	CHECK(IntegratedEconomy::Factories().at(ind_cap->index).last_batches == 150); // 300 SILC used, 0 remain
	CHECK(IntegratedEconomy::Factories().at(ind_res->index).last_batches == 100); // 200 SILC used, 800 remain

	ind_cap->GetCargoProduced(C("BALL"))->waiting = 0;
	ind_res->GetCargoProduced(C("BALL"))->waiting = 0;

	// Month 3: Capacity Works has run out of SILC and starves; Reserve Works continues!
	IntegratedEconomy::Produce();
	CHECK(IntegratedEconomy::Factories().at(ind_cap->index).last_batches == 0);   // Starved!
	CHECK(IntegratedEconomy::Factories().at(ind_res->index).last_batches == 100); // Still producing!
}

TEST_CASE("Works prototype save and load round-trip and version boundary", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	auto ind = fixture.CreateFactory(0x11);
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Reserve).Succeeded());

	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "SaveTown", 5000);
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 40).Succeeded());

	// Evaluate city once to populate history
	IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 40, true);
	float g = 0, p = 0;
	IntegratedEconomy::EvaluateCity(town->index, g, p);

	// Also commission residential district on town
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());

	// Save to JSON
	std::string saved_data = IntegratedEconomy::Save();
	auto j = nlohmann::json::parse(saved_data);
	CHECK(j.at("version") == 2);
	CHECK(j.contains("town_food_services"));
	CHECK(j.contains("town_residential_districts"));

	// Clean and reload
	IntegratedEconomy::Reset();
	CHECK(IntegratedEconomy::ActiveRules() == EconomyRulesVersion::Legacy);

	REQUIRE(IntegratedEconomy::Load(saved_data));
	CHECK(IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2);

	// Verify factory state preserved
	const auto &f = IntegratedEconomy::Factories().at(ind->index);
	CHECK(f.work == FactoryWorkType::Reserve);
	CHECK(f.capacity == 100);
	CHECK(f.input_capacity == 600);
	CHECK(f.output_capacity == 300);

	// Verify town food service preserved
	auto svc = IntegratedEconomy::GetTownFoodService(town->index);
	REQUIRE(svc != nullptr);
	CHECK(svc->selected_rate == 40);
	CHECK(svc->consumed_history[0] == 40);

	// Verify town residential district preserved
	auto dist = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	REQUIRE(dist != nullptr);
	CHECK(dist->commissioned);
	CHECK(dist->building_opportunities == 4);
	CHECK(dist->owner == CompanyID{0});

	// Verify rejection of future version 3
	j["version"] = 3;
	CHECK_FALSE(IntegratedEconomy::Load(j.dump()));
}

TEST_CASE("Works prototype residential district commissioning, materials, and limits", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "PilotTown", 5000);

	// Company starts with money, but 0 materials
	CHECK(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Failed());

	// Partial materials (only STEL)
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	CHECK(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Failed());

	// Add BALL to complete kit
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);

	// Commission succeeds
	auto res = Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index);
	REQUIRE(res.Succeeded());
	CHECK(res.GetCost() == 50000);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("BALL")) == 0);

	auto dist = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	REQUIRE(dist != nullptr);
	CHECK(dist->commissioned);
	CHECK(dist->building_opportunities == 4);
	CHECK(dist->buildings_placed == 0);
	CHECK(dist->buildings_completed == 0);
	CHECK_FALSE(dist->site_blocked);

	// Cannot double-commission
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	CHECK(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Failed());
}

TEST_CASE("Works prototype residential house placement, stage progression, protection, and decoupling", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	InitializeBuildingCounts(town);
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "PilotTown", 5000);

	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());

	auto dist = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	REQUIRE(dist != nullptr);

	SECTION("1. Blocked site handling") {
		// Town center (15, 15) has no road frontage on surrounding tiles.
		// TryGrowResidentialDistrict fails gracefully and marks site_blocked = true without consuming budget.
		for (int dy = -5; dy <= 5; ++dy) {
			for (int dx = -5; dx <= 5; ++dx) {
				TileIndex t = TileXY(15 + dx, 15 + dy);
				if (IsValidTile(t)) MakeClear(t, ClearGround::Grass, 0);
			}
		}
		CHECK_FALSE(IntegratedEconomy::TryGrowResidentialDistrict(town->index));
		CHECK(dist->site_blocked);
		CHECK(dist->building_opportunities == 4);
		CHECK(dist->buildings_placed == 0);
	}

	SECTION("2. Unblock site, house placement, progression, and protection") {
		for (int dy = -5; dy <= 5; ++dy) {
			for (int dx = -5; dx <= 5; ++dx) {
				TileIndex t = TileXY(15 + dx, 15 + dy);
				if (IsValidTile(t)) MakeClear(t, ClearGround::Grass, 0);
			}
		}
		MakeRoadNormal(TileXY(15, 16), ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, CompanyID{0}, OWNER_TOWN);

		uint32_t pop_before = town->cache.population;
		REQUIRE(IntegratedEconomy::TryGrowResidentialDistrict(town->index));
		CHECK_FALSE(dist->site_blocked);
		CHECK(dist->building_opportunities == 3);
		CHECK(dist->buildings_placed == 1);
		CHECK(dist->buildings_completed == 0);

		// Find the newly placed house tile
		TileIndex placed_tile = INVALID_TILE;
		for (int dy = -10; dy <= 10; ++dy) {
			for (int dx = -10; dx <= 10; ++dx) {
				TileIndex t = TileXY(15 + dx, 15 + dy);
				if (IsValidTile(t) && IsTileType(t, TileType::House)) {
					placed_tile = t;
					break;
				}
			}
			if (placed_tile != INVALID_TILE) break;
		}
		REQUIRE(placed_tile != INVALID_TILE);
		CHECK(IsHousePlayerProtected(placed_tile));
		CHECK_FALSE(IsHouseCompleted(placed_tile));
		// Shell starts at stage 0, so population was NOT added yet!
		CHECK(town->cache.population == pop_before);

		// 3. Native stage progression and completion:
		// Advance construction until complete
		while (!IsHouseCompleted(placed_tile)) {
			_tile_type_procs[GetTileType(placed_tile)]->tile_loop_proc(placed_tile);
		}
		CHECK(IsHouseCompleted(placed_tile));
		CHECK(dist->buildings_completed == 1);
		CHECK(town->cache.population > pop_before);

		// 4. Demolition immunity:
		// CanDeleteHouse must return false when the town attempts redevelopment demolition because house is protected
		{
			AutoRestoreBackup cur_company(_current_company, OWNER_TOWN);
			CHECK_FALSE(CanDeleteHouse(placed_tile));
		}

		// 5. Food service decoupling:
		// Food service rate is 0 (off), but completed house and population remain 100% intact!
		auto svc = IntegratedEconomy::GetTownFoodService(town->index);
		CHECK((svc == nullptr || svc->selected_rate == 0));
		CHECK(dist->buildings_completed == 1);
		CHECK(IsTileType(placed_tile, TileType::House));
	}
}
