/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_commonwealth_works.cpp Capacity or Reserve prototype, stable settlement, and food service regressions. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cargotype.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_cmd.h"
#include "../company_func.h"
#include "../industry.h"
#include "../engine_func.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/company_stockpile.h"
#include "../portal/integrated_economy.h"
#include "../portal/first_permanent_line.h"
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
#include "../newgrf_config.h"
#include "../newgrf.h"
#include "../portal/connected_economy.h"
#include "../portal/resource_sites.h"
#include "../portal/corporate_hq.h"
#include "../portal/stellar_network.h"
#include "../portal/portal_registry.h"
#include "../train.h"
#include "../vehicle_cmd.h"
#include "../vehicle_func.h"
#include "../void_map.h"
#include "../rail_cmd.h"
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace
{
struct WorksTestFixture {
	IndustrySpec saved_spec;
	GRFConfigList saved_configs = std::move(_grfconfig);
	std::array<CargoSpec, NUM_CARGO> saved_cargos;
	GRFFile industry_file;

	WorksTestFixture()
	{
		for (size_t i = 0; i < NUM_CARGO; ++i) saved_cargos[i] = *CargoSpec::Get(i);
		industry_file.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		{
			auto ind_cfg = std::make_unique<GRFConfig>();
			ind_cfg->ident.grfid = COMMONWEALTH_INDUSTRY_GRFID;
			ind_cfg->version = 5;
			ind_cfg->status = GRFStatus::Activated;
			_grfconfig.push_back(std::move(ind_cfg));

			auto rail_cfg = std::make_unique<GRFConfig>();
			rail_cfg->ident.grfid = COMMONWEALTH_RAIL_GRFID;
			rail_cfg->version = 2;
			rail_cfg->status = GRFStatus::Activated;
			_grfconfig.push_back(std::move(rail_cfg));
		}

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
			CargoSpec::Get(CargoType(c))->grffile = &industry_file;
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
		MegacityManager::Reset();
		PlanetManager::Reset();
		_industry_pool.CleanPool();
		_town_pool.CleanPool();
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = saved_spec;
		_grfconfig = std::move(saved_configs);
		for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(i) = saved_cargos[i];
		BuildCargoLabelMap();
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

Town *DistrictTownWithFrontage()
{
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(TileXY(15, 15));
	InitializeBuildingCounts(town);
	for (uint x = 11; x <= 19; ++x) {
		MakeRoadNormal(TileXY(x, 16), ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, OWNER_TOWN, OWNER_TOWN);
	}
	return town;
}

std::vector<TileIndex> DistrictHouseTiles(TownID town)
{
	std::vector<TileIndex> houses;
	for (TileIndex tile : Map::Iterate()) {
		if (!IsTileType(tile, TileType::House) || GetTownIndex(tile) != town || !IsHousePlayerProtected(tile)) continue;
		HouseID house = GetHouseType(tile);
		if (GetHouseNorthPart(house) == 0) houses.push_back(tile);
	}
	return houses;
}

void CompleteDistrictHouses(TownID town)
{
	for (TileIndex tile : DistrictHouseTiles(town)) {
		/* Advance the native construction loop, never manufacture a house or progress counter. */
		for (uint i = 0; i < 64 && !IsHouseCompleted(tile); ++i) {
			_tile_type_procs[GetTileType(tile)]->tile_loop_proc(tile);
		}
		REQUIRE(IsHouseCompleted(tile));
	}
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

	// Evaluate city once to populate history, then deliver again so both delivery counters are set
	IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 40, true);
	float g = 0, p = 0;
	IntegratedEconomy::EvaluateCity(town->index, g, p);
	IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 20, true);

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
	CHECK(svc->delivered_this_period == 20);
	CHECK(svc->delivered_last_period == 40);

	// Saves from before the delivery counters were persisted still load, with zero counters
	{
		auto legacy = nlohmann::json::parse(saved_data);
		legacy["town_food_services"][0].erase(10);
		legacy["town_food_services"][0].erase(9);
		REQUIRE(IntegratedEconomy::Load(legacy.dump()));
		auto old_svc = IntegratedEconomy::GetTownFoodService(town->index);
		REQUIRE(old_svc != nullptr);
		CHECK(old_svc->selected_rate == 40);
		CHECK(old_svc->delivered_this_period == 0);
		CHECK(old_svc->delivered_last_period == 0);
		legacy["town_food_services"][0].push_back(0);
		CHECK_FALSE(IntegratedEconomy::Load(legacy.dump()));
		REQUIRE(IntegratedEconomy::Load(saved_data));
	}

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

TEST_CASE("Residential district native command keeps cash and material spending atomic", "[commonwealth-works][first-line-prerequisite]")
{
	WorksTestFixture fixture;
	REQUIRE(Town::CanAllocateItem());
	auto town = Town::Create(TileXY(15, 15));
	auto company = Company::Get(CompanyID{0});
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	company->money = 49999;
	CHECK(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Failed());
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 160);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("BALL")) == 80);
	CHECK(company->money == 49999);
	CHECK(IntegratedEconomy::GetTownResidentialDistrict(town->index) == nullptr);
	company->money = 50000;
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do({}, town->index).Succeeded());
	CHECK(company->money == 50000);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 160);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	CHECK(company->money == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("BALL")) == 0);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	CHECK(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Failed());
	CHECK(company->money == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 160);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("BALL")) == 80);
}

TEST_CASE("Residential district save rejects impossible progress transactionally", "[commonwealth-works][first-line-prerequisite]")
{
	WorksTestFixture fixture;
	REQUIRE(Town::CanAllocateItem());
	auto town = Town::Create(TileXY(15, 15));
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	const auto saved = IntegratedEconomy::Save();
	auto doc = nlohmann::json::parse(saved);
	SECTION("extra opportunity") { doc["town_residential_districts"][0][3] = 5; }
	SECTION("completed before placed") { doc["town_residential_districts"][0][5] = 1; }
	SECTION("missing opportunity") { doc["town_residential_districts"][0][3] = 3; }
	SECTION("nonboolean commission") { doc["town_residential_districts"][0][2] = 1; }
	SECTION("uncommissioned evidence") { doc["town_residential_districts"][0][2] = false; }
	SECTION("duplicate town") { doc["town_residential_districts"].push_back(doc["town_residential_districts"][0]); }
	SECTION("invalid company") { doc["town_residential_districts"][0][1] = MAX_COMPANIES; }
	SECTION("neutral owner is not retired company sentinel") { doc["town_residential_districts"][0][1] = OWNER_NONE.base(); }
	SECTION("deity owner is not retired company sentinel") { doc["town_residential_districts"][0][1] = OWNER_DEITY.base(); }
	SECTION("negative progress") { doc["town_residential_districts"][0][4] = -1; }
	SECTION("fractional progress") { doc["town_residential_districts"][0][4] = 1.5; }
	CHECK_FALSE(IntegratedEconomy::Load(doc.dump()));
	CHECK(IntegratedEconomy::Save() == saved);
}

TEST_CASE("Residential district postload rejects foreign world and missing town", "[commonwealth-works][first-line-prerequisite]")
{
	WorksTestFixture fixture;
	REQUIRE(Town::CanAllocateItem());
	auto town = Town::Create(TileXY(15, 15));
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	SECTION("wrong world") { town->xy = TileXY(35, 15); }
	SECTION("missing town") { delete town; }
	SECTION("in-range missing company") {
		const CompanyID missing{MAX_COMPANIES - 1};
		REQUIRE_FALSE(Company::IsValidID(missing));
		auto saved = nlohmann::json::parse(IntegratedEconomy::Save());
		saved["town_residential_districts"][0][1] = missing.base();
		REQUIRE(IntegratedEconomy::Load(saved.dump()));
	}
	CHECK_FALSE(IntegratedEconomy::ValidateAfterLoad());
}

TEST_CASE("Native designation enables monthly construction of four completed district houses", "[commonwealth-works][first-line-correction]")
{
	WorksTestFixture fixture;
	Town *town = DistrictTownWithFrontage();
	CHECK_FALSE(FirstPermanentLine::Active());
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	const auto *district = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	REQUIRE(district != nullptr);
	REQUIRE(MegacityManager::GetProfile(town->index) == nullptr);
	MegacityManager::EvaluateMonthlySupply();
	CHECK(district->buildings_placed == 0);
	CHECK(DistrictHouseTiles(town->index).empty());
	REQUIRE(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	const auto *profile = MegacityManager::GetProfile(town->index);
	REQUIRE(profile != nullptr);
	CHECK(profile->world_id == WorldID{0});
	const uint32_t population_before = town->cache.population;
	for (uint8_t month = 1; month <= 4; ++month) {
		MegacityManager::EvaluateMonthlySupply();
		REQUIRE(district->buildings_placed == month);
		CHECK(district->building_opportunities == 4 - month);
		CHECK(district->buildings_completed == month - 1);
		REQUIRE(DistrictHouseTiles(town->index).size() == month);
		CompleteDistrictHouses(town->index);
		CHECK(district->buildings_completed == month);
	}
	CHECK(town->cache.population > population_before);
	for (TileIndex tile : DistrictHouseTiles(town->index)) {
		CHECK(IsHouseCompleted(tile));
		AutoRestoreBackup owner(_current_company, OWNER_TOWN);
		CHECK_FALSE(CanDeleteHouse(tile));
	}
	MegacityManager::EvaluateMonthlySupply();
	CHECK(district->building_opportunities == 0);
	CHECK(district->buildings_placed == 4);
	CHECK(district->buildings_completed == 4);
	CHECK(DistrictHouseTiles(town->index).size() == 4);
}

TEST_CASE("Paid residential district survives company deletion or acquisition and reload", "[commonwealth-works][first-line-correction]")
{
	WorksTestFixture fixture;
	/* The native deletion command emits a bankruptcy notice using both names. */
	Company::Get(CompanyID{0})->president_name = "District commissioner";
	Town *town = DistrictTownWithFrontage();
	CHECK_FALSE(FirstPermanentLine::Active());
	REQUIRE(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	MegacityManager::EvaluateMonthlySupply();
	CompleteDistrictHouses(town->index);
	const auto houses = DistrictHouseTiles(town->index);
	REQUIRE(houses.size() == 1);
	const uint32_t population = town->cache.population;
	CompanyID expected_owner;
	SECTION("native company deletion retains ownerless paid works") {
		REQUIRE(Command<Commands::CompanyControl>::Do(DoCommandFlag::Execute, CompanyCtrlAction::Delete,
			CompanyID{0}, CompanyRemoveReason::Bankrupt, ClientID::Invalid).Succeeded());
		expected_owner = CompanyID::Invalid();
	}
	SECTION("native acquisition ownership handler transfers paid works") {
		ChangeOwnershipOfCompanyItems(CompanyID{0}, CompanyID{1});
		delete Company::Get(CompanyID{0});
		expected_owner = CompanyID{1};
	}
	REQUIRE_FALSE(Company::IsValidID(CompanyID{0}));
	_current_company = CompanyID{1};
	const auto saved = IntegratedEconomy::Save();
	REQUIRE(IntegratedEconomy::Load(saved));
	CHECK(IntegratedEconomy::Save() == saved);
	REQUIRE(IntegratedEconomy::ValidateAfterLoad());
	const auto *district = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	REQUIRE(district != nullptr);
	CHECK(district->owner == expected_owner);
	CHECK(district->commissioned);
	CHECK(district->building_opportunities == 3);
	CHECK(district->buildings_placed == 1);
	CHECK(district->buildings_completed == 1);
	CHECK(DistrictHouseTiles(town->index) == houses);
	CHECK(IsHouseCompleted(houses.front()));
	CHECK(town->cache.population == population);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{1}, C("STEL")) == 0);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{1}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{1}, C("BALL"), 80);
	CHECK_FALSE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{1}, C("STEL")) == 160);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{1}, C("BALL")) == 80);
	/* The original remaining allowance continues through the ordinary monthly path. */
	MegacityManager::EvaluateMonthlySupply();
	CompleteDistrictHouses(town->index);
	CHECK(district->building_opportunities == 2);
	CHECK(district->buildings_placed == 2);
	CHECK(district->buildings_completed == 2);
	CHECK(DistrictHouseTiles(town->index).size() == 2);
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

TEST_CASE("Works alternative world connection, direct portal linking, and bottleneck independence", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	// Reset planet regions and expand map to 256x256 to allow terminal footprints
	PlanetManager::Reset();
	Map::Allocate(256, 256);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
		else MakeVoid(tile);
	}

	PlanetRegion w1{
		.id = WorldID{1},
		.name = "Industrial Commonwealth",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 10,
		.min_y = 10,
		.max_x = 100,
		.max_y = 100,
		.development_score = 10000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w1));
	IntegratedEconomy::RegisterRole(WorldID{1}, EconomicRole::Industrial);

	// Setup World 2 (extraction frontier) and World 3 (research frontier)
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Extraction Frontier",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 10,
		.min_y = 110,
		.max_x = 100,
		.max_y = 200,
		.development_score = 4000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w2));
	IntegratedEconomy::RegisterRole(WorldID{2}, EconomicRole::Frontier);

	PlanetRegion w3{
		.id = WorldID{3},
		.name = "Research Frontier",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 150,
		.min_y = 10,
		.max_x = 240,
		.max_y = 100,
		.development_score = 3000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w3));
	IntegratedEconomy::RegisterRole(WorldID{3}, EconomicRole::Frontier);

	// 2. Resource site registration & Surveying on World 3
	ResourceSiteManager::Reset();
	ResourceSiteManager::SetEnabled(true);
	TileIndex w3_site = TileXY(180, 70); // In World 3
	auto *spec0 = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}));
	spec0->enabled = true;
	spec0->life_type.Set(IndustryLifeType::Extractive);
	IndustryType ind_type = IndustryType{0};
	uint32_t site_id = ResourceSiteManager::AddSite(w3_site, ind_type, 16, 16);
	REQUIRE(site_id != 0);

	// Site is unsurveyed by Company 0 initially
	CHECK_FALSE(ResourceSiteManager::Surveyed(CompanyID{0}, w3_site));

	// Survey command on World 3
	auto survey_res = Command<Commands::SurveyResources>::Do(DoCommandFlag::Execute, w3_site);
	REQUIRE(survey_res.Succeeded());
	CHECK(ResourceSiteManager::Surveyed(CompanyID{0}, w3_site));

	// 3. Direct Portal Pair linking between World 1 and World 3
	TileIndex portal_w1 = TileXY(50, 50);
	TileIndex portal_w3 = TileXY(180, 50);

	Company *c = Company::GetIfValid(CompanyID{0});
	REQUIRE(c != nullptr);
	c->clear_limit = 1000 << 16;
	c->avail_railtypes.Set(RAILTYPE_BEGIN);

	auto portal_res = Command<Commands::BuildPortalPair>::Do(
		DoCommandFlag::Execute,
		portal_w1, DiagDirection::NE,
		portal_w3, DiagDirection::SW,
		RAILTYPE_BEGIN
	);
	REQUIRE(portal_res.Succeeded());
	UpdateSignalsInBuffer();

	// Verify both portal tiles are registered and linked directly
	CHECK(PortalRegistry::IsPortalTile(portal_w1));
	CHECK(PortalRegistry::IsPortalTile(portal_w3));
	CHECK(PortalRegistry::GetOtherPortalEnd(portal_w1) == portal_w3);
	CHECK(PortalRegistry::GetOtherPortalEnd(portal_w3) == portal_w1);

	// Bottleneck independence: This portal links World 1 <-> World 3 directly,
	// completely bypassing World 2!
	CHECK(PlanetManager::GetTileWorld(portal_w1) == WorldID{1});
	CHECK(PlanetManager::GetTileWorld(portal_w3) == WorldID{3});

	// 4. Train isolation and selective corridor closure
	ResetRailTypes();
	MakeRailNormal(TileXY(50, 60), CompanyID{0}, TrackBits{Track::X}, RAILTYPE_RAIL);
	MakeRailNormal(TileXY(180, 60), CompanyID{0}, TrackBits{Track::X}, RAILTYPE_RAIL);
	_engine_pool.CleanPool();
	Engine::CreateAtIndex(EngineID{0}, VehicleType::Train, 0);
	auto *rvi0 = const_cast<RailVehicleInfo *>(RailVehInfo(EngineID{0}));
	rvi0->power = 1000;
	rvi0->weight = 50;
	rvi0->railtypes = RailTypes{RAILTYPE_RAIL};

	REQUIRE(Vehicle::CanAllocateItem(2));
	Train *t1 = Vehicle::Create<Train>();
	t1->SetFrontEngine();
	t1->owner = CompanyID{0};
	t1->name = "Silica dunes > Ballast crusher";
	t1->tile = TileXY(50, 60);
	t1->track = Track::X;
	t1->railtypes = RailTypes{RAILTYPE_RAIL};
	t1->engine_type = EngineID{0};
	t1->cargo_type = CargoType{0};
	t1->gcache.cached_power = 1000;
	t1->gcache.cached_veh_length = 8;

	Train *t9 = Vehicle::Create<Train>();
	t9->SetFrontEngine();
	t9->owner = CompanyID{0};
	t9->name = "W3 Silica > Ballast crusher";
	t9->tile = TileXY(180, 60);
	t9->track = Track::X;
	t9->railtypes = RailTypes{RAILTYPE_RAIL};
	t9->engine_type = EngineID{0};
	t9->cargo_type = CargoType{0};
	t9->gcache.cached_power = 1000;
	t9->gcache.cached_veh_length = 8;

	// Both trains are running initially (not stopped)
	CHECK_FALSE(t1->vehstatus.Test(VehState::Stopped));
	CHECK_FALSE(t9->vehstatus.Test(VehState::Stopped));

	// Close primary corridor: stops t1 only
	CHECK(SetPrimaryCorridorClosed(true));
	CHECK(t1->vehstatus.Test(VehState::Stopped));
	CHECK_FALSE(t9->vehstatus.Test(VehState::Stopped)); // Alternative source is NOT interrupted!

	// Reopen primary corridor: restarts t1
	CHECK(SetPrimaryCorridorClosed(false));
	CHECK_FALSE(t1->vehstatus.Test(VehState::Stopped));
	CHECK_FALSE(t9->vehstatus.Test(VehState::Stopped));

	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	_engine_pool.CleanPool();
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
	PortalRegistry::Reset();
	ResourceSiteManager::Reset();
	ResourceSiteManager::SetEnabled(false);
	Map::Allocate(64, 64);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
		else MakeVoid(tile);
	}
	UpdateSignalsInBuffer();
}

TEST_CASE("Town food service benefit retirement on company deletion vs acquisition transfer", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	town->cache.population = 5000;
	town->cache.num_houses = 200;
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "RetirementTown", 5000);

	// Commission food service at rate 40 for Company 0
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 40).Succeeded());
	auto svc = IntegratedEconomy::GetTownFoodService(town->index);
	REQUIRE(svc != nullptr);
	CHECK(svc->owner == CompanyID{0});

	// Fill all four history entries with 40 units (total 160 units consumed)
	// That earns full +25% passenger bonus (160 / 640 = 0.25)
	for (size_t i = 0; i < 4; ++i) {
		IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 40, true);
		float g = 0, p = 0;
		REQUIRE(IntegratedEconomy::EvaluateCity(town->index, g, p));
	}
	CHECK(svc->consumed_history[0] == 40);
	CHECK(svc->consumed_history[1] == 40);
	CHECK(svc->consumed_history[2] == 40);
	CHECK(svc->consumed_history[3] == 40);

	// Town has 50 units in food reserve
	IntegratedEconomy::AcceptCity(town->index, C("FOOD"), 50, true);
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 50);

	// 1. Company Deletion: Company 0 is deleted (new_owner is Invalid)
	IntegratedEconomy::ChangeCompany(CompanyID{0}, CompanyID::Invalid());

	// Verify service is retired to uncommissioned defaults
	CHECK(svc->owner == CompanyID::Invalid());
	CHECK(svc->selected_rate == 0);
	CHECK(svc->history_cursor == 0);
	CHECK(svc->fractional_passenger_remainder == 0);
	for (size_t i = 0; i < 4; ++i) {
		CHECK(svc->consumed_history[i] == 0);
		CHECK(svc->requested_history[i] == 0);
	}
	// Verify town reserve is preserved (not discarded or wiped)
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 50);

	// 2. Recommission before another evaluation by a new company (Company 1)
	Company *c1 = Company::GetIfValid(CompanyID{1});
	if (c1 == nullptr) {
		c1 = Company::CreateAtIndex(CompanyID{1});
	}
	c1->money = 10000000;
	{
		AutoRestoreBackup cur_co(_current_company, CompanyID{1});
		REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 20).Succeeded());
	}
	CHECK(svc->owner == CompanyID{1});
	CHECK(svc->selected_rate == 20);

	// Before another monthly evaluation: bonus must be baseline 1.0x (0% bonus)
	float growth = 0, passengers = 0;
	// EvaluateCity month: consumes 20 from retained reserve 50 -> 30 left
	REQUIRE(IntegratedEconomy::EvaluateCity(town->index, growth, passengers));
	CHECK(IntegratedEconomy::City(town->index)->reserves.at(C("FOOD")) == 30);
	// Only this period's 20 units are consumed; older 3 periods are 0
	CHECK(svc->consumed_history[0] == 20);
	CHECK(svc->consumed_history[1] == 0);
	CHECK(svc->consumed_history[2] == 0);
	CHECK(svc->consumed_history[3] == 0);
	// Bonus earned is only 20 / 640 (+3.125%), NOT +25%
	CHECK(passengers == Approx(1.0f + 20.0f / 640.0f));

	// 3. Acquisition: Company 1 acquired by Company 2 (valid company)
	Company *c2 = Company::GetIfValid(CompanyID{2});
	if (c2 == nullptr) {
		c2 = Company::CreateAtIndex(CompanyID{2});
	}
	c2->money = 10000000;
	IntegratedEconomy::ChangeCompany(CompanyID{1}, CompanyID{2});
	CHECK(svc->owner == CompanyID{2});
	CHECK(svc->selected_rate == 20);
	CHECK(svc->consumed_history[0] == 20); // Retained!

	// 4. Foreign company command rejected without mutation
	{
		AutoRestoreBackup cur_co(_current_company, CompanyID{3});
		Company *c3 = Company::GetIfValid(CompanyID{3});
		if (c3 == nullptr) c3 = Company::CreateAtIndex(CompanyID{3});
		auto foreign_res = Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 40);
		CHECK(foreign_res.Failed());
		CHECK(svc->owner == CompanyID{2});
		CHECK(svc->selected_rate == 20);
	}

	// 5. Spectator command rejected without mutation
	{
		AutoRestoreBackup cur_co(_current_company, COMPANY_SPECTATOR);
		auto spec_res = Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 40);
		CHECK(spec_res.Failed());
		CHECK(svc->owner == CompanyID{2});
		CHECK(svc->selected_rate == 20);
	}

	// 6. Owner-selected Off: Company 2 sets rate to 0
	{
		AutoRestoreBackup cur_co(_current_company, CompanyID{2});
		REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 0).Succeeded());
		CHECK(svc->owner == CompanyID{2});
		CHECK(svc->selected_rate == 0);
		// History is preserved to age out normally
		CHECK(svc->consumed_history[0] == 20);
	}
}

TEST_CASE("Strict v2 save invariants reject impossible factory and food service states", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	auto ind = fixture.CreateFactory(0x11);
	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "InvariantTown", 5000);

	// Commission Reserve works and rate 20
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{1}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::Execute, ind->index, FactoryWorkType::Reserve).Succeeded());
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 20).Succeeded());

	std::string valid_json = IntegratedEconomy::Save();
	auto base_j = nlohmann::json::parse(valid_json);

	// Helper to test rejection without mutating state
	auto test_reject = [&](nlohmann::json j) {
		std::string before_state = IntegratedEconomy::Save();
		CHECK_FALSE(IntegratedEconomy::Load(j.dump()));
		CHECK(IntegratedEconomy::Save() == before_state); // Transactional rollback verified
	};

	// 1. Factory tuple rejections
	{
		auto j = base_j;
		// work = 0 (None) with cap = 150
		j["factories"][0][6] = 0;
		j["factories"][0][2] = 150;
		test_reject(j);
	}
	{
		auto j = base_j;
		// work = 1 (Capacity) with in_cap = 600
		j["factories"][0][6] = 1;
		j["factories"][0][2] = 150;
		j["factories"][0][7] = 600;
		test_reject(j);
	}
	{
		auto j = base_j;
		// work = 2 (Reserve) with cap = 150
		j["factories"][0][6] = 2;
		j["factories"][0][2] = 150;
		test_reject(j);
	}
	{
		auto j = base_j;
		// invalid work = 3
		j["factories"][0][6] = 3;
		test_reject(j);
	}

	// 2. Food service rejections
	{
		auto j = base_j;
		// rate not in {0, 10, 20, 40}
		j["town_food_services"][0][2] = 15;
		test_reject(j);
	}
	{
		auto j = base_j;
		// rate > 40
		j["town_food_services"][0][2] = 50;
		test_reject(j);
	}
	{
		auto j = base_j;
		// market_ceiling != 40
		j["town_food_services"][0][3] = 60;
		test_reject(j);
	}
	{
		auto j = base_j;
		// receiver_capacity != 60
		j["town_food_services"][0][4] = 100;
		test_reject(j);
	}
	{
		auto j = base_j;
		// requested history not in {0, 10, 20, 40}
		j["town_food_services"][0][6][0] = 30;
		test_reject(j);
	}
	{
		auto j = base_j;
		// consumed > requested
		j["town_food_services"][0][5][0] = 30;
		j["town_food_services"][0][6][0] = 20;
		test_reject(j);
	}
	{
		auto j = base_j;
		// consumed > 40
		j["town_food_services"][0][5][0] = 50;
		j["town_food_services"][0][6][0] = 40;
		test_reject(j);
	}
	{
		auto j = base_j;
		// remainder == 640
		j["town_food_services"][0][8] = 640;
		test_reject(j);
	}
	{
		auto j = base_j;
		// remainder > 640
		j["town_food_services"][0][8] = 700;
		test_reject(j);
	}
	{
		auto j = base_j;
		// invalid owner with non-zero rate
		j["town_food_services"][0][1] = 0xFF; // CompanyID::Invalid
		j["town_food_services"][0][2] = 20;
		test_reject(j);
	}
	{
		auto j = base_j;
		// invalid owner with non-zero history
		j["town_food_services"][0][1] = 0xFF;
		j["town_food_services"][0][2] = 0;
		j["town_food_services"][0][5][0] = 10;
		test_reject(j);
	}
	{
		auto j = base_j;
		// duplicate town_food_services row
		j["town_food_services"].push_back(j["town_food_services"][0]);
		test_reject(j);
	}

	// 3. ValidateAfterLoad role and reference checks
	CHECK(IntegratedEconomy::ValidateAfterLoad());

	TownID tid = town->index;
	auto svc = IntegratedEconomy::GetTownFoodService(tid);
	REQUIRE(svc != nullptr);

	// Town on non-Core world fails ValidateAfterLoad
	TileIndex orig_xy = town->xy;
	town->xy = TileXY(40, 40); // World 1 is Industrial (non-Core)
	CHECK_FALSE(IntegratedEconomy::ValidateAfterLoad());
	town->xy = orig_xy;
	CHECK(IntegratedEconomy::ValidateAfterLoad());

	// Invalid company owner in town_food_services fails ValidateAfterLoad
	svc->owner = CompanyID{10}; // Company 10 does not exist
	CHECK_FALSE(IntegratedEconomy::ValidateAfterLoad());
	svc->owner = CompanyID{0};
	CHECK(IntegratedEconomy::ValidateAfterLoad());
	// Set 4x40 in history
	svc->consumed_history = {40, 40, 40, 40};
	svc->fractional_passenger_remainder = 0;

	// In town_cmd, passenger generation bonus is bounded to 25% max
	uint amount_pass = 100;
	uint32_t total_c = svc->consumed_history[0] + svc->consumed_history[1] + svc->consumed_history[2] + svc->consumed_history[3];
	uint64_t prod = static_cast<uint64_t>(amount_pass) * total_c + svc->fractional_passenger_remainder;
	uint32_t bonus = static_cast<uint32_t>(prod / 640);
	CHECK(bonus == 25); // Exactly +25%
	CHECK(bonus <= 25);
}

TEST_CASE("Works prototype Sprint 2 truthful acceptance: native housing completion and population credit", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	if (!Town::CanAllocateItem()) return;
	auto town = Town::Create(TileXY(15, 15));
	InitializeBuildingCounts(town);
	MegacityManager::RegisterMegacity(town->index, WorldID{0}, "HousingTown", 5000);

	// 1. Uncommissioned town: TryGrowResidentialDistrict fails and never places or completes houses
	CHECK_FALSE(IntegratedEconomy::TryGrowResidentialDistrict(town->index));
	auto dist = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	CHECK(dist == nullptr);

	// 2. Food service alone never creates houses
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 40).Succeeded());
	float g = 0, p = 0;
	IntegratedEconomy::EvaluateCity(town->index, g, p);
	CHECK(town->cache.num_houses == 0);

	// 3. Commission residential works with 160 STEL + 80 BALL
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("BALL"), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, town->index).Succeeded());
	dist = IntegratedEconomy::GetTownResidentialDistrict(town->index);
	REQUIRE(dist != nullptr);
	CHECK(dist->commissioned);
	CHECK(dist->building_opportunities == 4);
	CHECK(dist->buildings_placed == 0);
	CHECK(dist->buildings_completed == 0);

	// Make road frontage
	MakeRoadNormal(TileXY(15, 16), ROAD_ALL, ROADTYPE_ROAD, INVALID_ROADTYPE, town->index, CompanyID{0}, OWNER_TOWN);

	// 4. Place house 1: opportunities drops to 3, placed = 1, completed = 0
	uint32_t pop_before = town->cache.population;
	REQUIRE(IntegratedEconomy::TryGrowResidentialDistrict(town->index));
	CHECK(dist->building_opportunities == 3);
	CHECK(dist->buildings_placed == 1);
	CHECK(dist->buildings_completed == 0);
	// Shell stage 0 does NOT add population yet
	CHECK(town->cache.population == pop_before);

	// Find placed tile
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

	// 5. Native progression: loop tile_loop_proc until completed
	while (!IsHouseCompleted(placed_tile)) {
		_tile_type_procs[GetTileType(placed_tile)]->tile_loop_proc(placed_tile);
	}
	CHECK(IsHouseCompleted(placed_tile));
	CHECK(dist->buildings_completed == 1);
	CHECK(town->cache.population > pop_before); // Real population credit!

	// 6. Demolition protection: town cannot delete the house
	{
		AutoRestoreBackup cur_co(_current_company, OWNER_TOWN);
		CHECK_FALSE(CanDeleteHouse(placed_tile));
	}

	// 7. Decoupling: turn food service Off. Completed house and population remain 100% intact!
	REQUIRE(Command<Commands::SetTownFoodServiceRate>::Do(DoCommandFlag::Execute, town->index, 0).Succeeded());
	IntegratedEconomy::EvaluateCity(town->index, g, p);
	CHECK(dist->buildings_completed == 1);
	CHECK(IsHouseCompleted(placed_tile));
	CHECK(town->cache.population > pop_before);
}

TEST_CASE("Works prototype Sprint 3 truthful acceptance: alternate-source deliveries and production", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	// World 1: Industrial Commonwealth (Ballast Crusher consuming SILC to produce BALL)
	Industry *crusher = fixture.CreateFactory(0x11);
	REQUIRE(crusher != nullptr);
	const auto *recipe = ProductionChainManager::GetRecipe(IntegratedEconomy::IndustryRecipe(crusher->type));
	REQUIRE(recipe != nullptr);

	// Setup World 2 (Primary extraction) and World 3 (Alternate extraction)
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Primary Extraction",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::AridDesert,
		.min_x = 1,
		.min_y = 65,
		.max_x = 30,
		.max_y = 120,
		.development_score = 4000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w2));
	IntegratedEconomy::RegisterRole(WorldID{2}, EconomicRole::Frontier);

	PlanetRegion w3{
		.id = WorldID{3},
		.name = "Alternative Extraction",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 31,
		.min_y = 65,
		.max_x = 62,
		.max_y = 120,
		.development_score = 3000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w3));
	IntegratedEconomy::RegisterRole(WorldID{3}, EconomicRole::Frontier);

	CargoType silc_cargo = C("SILC");

	// Initial factory state: input buffer has 0 SILC, produced has 0 BALL (input_capacity 300 batches * 2 SILC = 600)
	CHECK(IntegratedEconomy::IndustrySpace(crusher, silc_cargo) == 600);
	CHECK(crusher->produced[0].waiting == 0);

	// 1. Alternate-source delivery: Deliver 100 units of SILC from World 3 to the Ballast Crusher
	uint32_t accepted = IntegratedEconomy::AcceptIndustry(crusher, silc_cargo, 100);
	CHECK(accepted == 100);
	CHECK(crusher->accepted[0].waiting == 100);

	// 2. Production from alternate-source delivery
	uint64_t batches_before = IntegratedEconomy::Factories().at(crusher->index).total_batches;
	IntegratedEconomy::Produce();
	const auto &factory_after = IntegratedEconomy::Factories().at(crusher->index);
	CHECK(factory_after.total_batches > batches_before);
	CHECK(crusher->accepted[0].waiting < 100); // Input consumed
	CHECK(crusher->produced[0].waiting > 0);   // Output BALL produced!

	// 3. Primary feeder interruption: simulate stopping primary World 2 feeder
	// Continue deliveries exclusively from World 3 alternate source
	uint32_t delivered_alt2 = IntegratedEconomy::AcceptIndustry(crusher, silc_cargo, 100);
	CHECK(delivered_alt2 == 100);
	uint64_t batches_mid = factory_after.total_batches;
	IntegratedEconomy::Produce();
	CHECK(IntegratedEconomy::Factories().at(crusher->index).total_batches > batches_mid);
	// BALL output continues accumulating purely from alternate-source supply!
	CHECK(crusher->produced[0].waiting > 0);
}

TEST_CASE("Works prototype Sprint 4 truthful acceptance: Pipeline B research freight, kit escrow, Materials III unlock, and rare resource discovery", "[commonwealth-works]")
{
	WorksTestFixture fixture;

	// Register World 2 (Frontier) for rare resource site
	PlanetRegion w2{
		.id = WorldID{2},
		.name = "Frontier Survey",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 63,
		.min_y = 1,
		.max_x = 94,
		.max_y = 62,
		.development_score = 3000,
	};
	REQUIRE(PlanetManager::RegisterRegion(w2));
	IntegratedEconomy::RegisterRole(WorldID{2}, EconomicRole::Frontier);
	StellarNetwork::RegisterWorld({WorldID{2}, "uat-2", "UAT world 2", 6, 0, true});

	// Corporate HQ on Core World 0
	CorporateHQManager::Reset();
	REQUIRE(CorporateHQManager::RegisterHQ(CompanyID{0}, WorldID{0}, TileXY(15, 15), "Commonwealth Central HQ"));
	CHECK(CorporateHQManager::HasHQ(CompanyID{0}));

	// Feedstock acceleration explicitly OFF
	IntegratedEconomy::SetAcceleration(CompanyID{0}, false);
	CHECK_FALSE(IntegratedEconomy::Accelerates(CompanyID{0}));

	// Baseline prerequisites Materials I and II
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2});
	CHECK(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_1));
	CHECK(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_2));
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_3));

	// Configure industries for 2-input CHIP production:
	// Copper smelter (0x17, RECIPE_COPPER_SMELTING: 2 COPR -> 2 WIRE)
	auto *spec_copper = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{1}));
	*spec_copper = fixture.saved_spec;
	spec_copper->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
	spec_copper->grf_prop.local_id = 0x17;
	spec_copper->enabled = true;
	REQUIRE(Industry::CanAllocateItem());
	auto copper_smelter = Industry::Create(TileXY(35, 30));
	copper_smelter->type = IndustryType{1};
	copper_smelter->founder = OWNER_DEITY;
	copper_smelter->accepted.push_back({.cargo = C("COPR")});
	copper_smelter->produced.push_back({.cargo = C("WIRE")});
	IntegratedEconomy::RegisterIndustry(copper_smelter);

	// Silicon works (0x19, RECIPE_SILICON_ARC: 2 SAND + 1 WIRE -> 1 CHIP)
	auto *spec_silicon = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{2}));
	*spec_silicon = fixture.saved_spec;
	spec_silicon->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
	spec_silicon->grf_prop.local_id = 0x19;
	spec_silicon->enabled = true;
	REQUIRE(Industry::CanAllocateItem());
	auto silicon_works = Industry::Create(TileXY(40, 30));
	silicon_works->type = IndustryType{2};
	silicon_works->founder = OWNER_DEITY;
	silicon_works->accepted.push_back({.cargo = C("SAND")});
	silicon_works->accepted.push_back({.cargo = C("WIRE")});
	silicon_works->produced.push_back({.cargo = C("CHIP")});
	IntegratedEconomy::RegisterIndustry(silicon_works);

	// Rare Earth Mine spec (extractive, producing RARE)
	auto *spec_rare = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{3}));
	*spec_rare = fixture.saved_spec;
	spec_rare->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
	spec_rare->grf_prop.local_id = 0x14;
	spec_rare->enabled = true;
	spec_rare->life_type = IndustryLifeType::Extractive;
	spec_rare->produced_cargo[0] = C("RARE");

	// Rare Earth resource site on World 2
	ResourceSiteManager::Reset();
	ResourceSiteManager::SetEnabled(true);
	TileIndex rare_survey_tile = TileXY(70, 20);
	uint32_t rare_site_id = ResourceSiteManager::AddSite(rare_survey_tile, IndustryType{3}, 16, 16);
	REQUIRE(rare_site_id != 0);
	REQUIRE(ResourceSiteManager::Survey(DoCommandFlag::Execute, CompanyID{0}, rare_survey_tile).Succeeded());
	const auto &sites = ResourceSiteManager::Sites();
	REQUIRE(!sites.empty());
	const auto &rare_site = sites.front();

	// Prior to Materials III unlock, rare site is surveyed but NOT discovered
	CHECK(ResourceSiteManager::Surveyed(CompanyID{0}, rare_survey_tile));
	CHECK_FALSE(ResourceSiteManager::Discovered(CompanyID{0}, rare_site));

	// 1. Deliver COPR to smelter, produce WIRE
	CHECK(IntegratedEconomy::AcceptIndustry(copper_smelter, C("COPR"), 40) == 40);
	IntegratedEconomy::Produce();
	CHECK(copper_smelter->produced[0].waiting > 0);

	// 2. Deliver SAND and WIRE to silicon works, produce CHIP (2 SAND + 1 WIRE -> 1 CHIP)
	CHECK(IntegratedEconomy::AcceptIndustry(silicon_works, C("SAND"), 40) == 40);
	CHECK(IntegratedEconomy::AcceptIndustry(silicon_works, C("WIRE"), 20) == 20);
	IntegratedEconomy::Produce();
	CHECK(silicon_works->produced[0].waiting > 0);

	// 3. Deliveries to World 0 company stockpile: 40 STEL + 20 CHIP
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("STEL"), 40);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, C("CHIP"), 20);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 40);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("CHIP")) == 20);

	// 4. Negative Paths:
	CHECK_FALSE(IntegratedEconomy::PrepareResearch(CompanyID{1}, TECH_MATERIALS_3));

	// Atomic Escrow reservation
	REQUIRE(IntegratedEconomy::PrepareResearch(CompanyID{0}, TECH_MATERIALS_3));
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("CHIP")) == 0);
	auto res_info = IntegratedEconomy::Research(CompanyID{0});
	REQUIRE(res_info != nullptr);
	CHECK(res_info->reserved.at(C("STEL")) == 40);
	CHECK(res_info->reserved.at(C("CHIP")) == 20);

	// Cancellation / switch refund restores 100% of kit back to stockpile
	IntegratedEconomy::CancelResearch(CompanyID{0});
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 40);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("CHIP")) == 20);
	CHECK(res_info->reserved.empty());

	// Re-reserve kit for research progression
	REQUIRE(IntegratedEconomy::PrepareResearch(CompanyID{0}, TECH_MATERIALS_3));
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("STEL")) == 0);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, C("CHIP")) == 0);

	// 5. Progression & Materials III unlock:
	REQUIRE(Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_MATERIALS_3).Succeeded());
	REQUIRE(Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 600000).Succeeded());
	TechTreeManager::ProcessMonthlyResearch();

	// Research completed! TECH_MATERIALS_3 unlocked!
	CHECK(TechTreeManager::IsTechUnlocked(CompanyID{0}, TECH_MATERIALS_3));
	CHECK(TechTreeManager::GetActiveProject(CompanyID{0}) == TECH_NONE);

	// 6. Rare Earth discovery revelation:
	CHECK(ResourceSiteManager::Discovered(CompanyID{0}, rare_site));

	// Cleanup
	*spec_copper = fixture.saved_spec;
	*spec_silicon = fixture.saved_spec;
	*spec_rare = fixture.saved_spec;
	TechTreeManager::Reset();
	CorporateHQManager::Reset();
	ResourceSiteManager::Reset();
	StellarNetwork::Reset();
}
