/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_first_permanent_line.cpp Bounded mission persistence and inactive observer guards. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include "../portal/first_permanent_line.h"
#include "../portal/integrated_economy.h"
#include "../portal/planet_manager.h"
#include "../portal/company_stockpile.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/production_chain.h"
#include "../portal/logistics_hub.h"
#include "../portal/megacity_manager.h"
#include "../portal/portal_cmd.h"
#include "../command_func.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../waypoint_base.h"
#include "../depot_base.h"
#include "../rail_map.h"
#include "../order_base.h"
#include "../town_map.h"
#include "../tile_map.h"
#include "../map_func.h"
#include "../cargotype.h"
#include "../industry.h"
#include "../company_base.h"
#include "../town.h"
#include "../train.h"
#include "../newgrf.h"
#include "../newgrf_house.h"
#include "../vehicle_func.h"
#include "../window_func.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
namespace {
using Json = nlohmann::json;

Json BaselineChunk()
{
	return {{"schema", 1}, {"active", true}, {"economy_evaluations", 0}, {"invalid", false}, {"awarded", false}, {"reason", ""},
		{"binding", {0, 0, 0, 1, 2, 3}}, {"delivered", {false, false}}, {"produced", {false, false}},
		{"stored", {0, 0}}, {"commissioned", false}, {"house_count", 0}, {"houses", Json::array()}, {"journeys", Json::array()}};
}
}

TEST_CASE("First Permanent Line missing chunk and inactive events are inert", "[first-permanent-line]")
{
	FirstPermanentLine::Reset();
	REQUIRE_FALSE(FirstPermanentLine::Active());
	const std::string before = FirstPermanentLine::Save();
	FirstPermanentLine::OnIndustryDelivery(VehicleID{0}, CompanyID{0}, IndustryID{2}, IndustryID{0}, CargoType{0}, 100);
	FirstPermanentLine::OnStoredDelivery(VehicleID{0}, CompanyID{0}, IndustryID{0}, StationID{0}, CargoType{0}, 100);
	FirstPermanentLine::OnProduction(IndustryID{0}, 1);
	FirstPermanentLine::OnCommission(CompanyID{0}, TownID{0});
	FirstPermanentLine::OnArrival(VehicleID{0}, StationID{0});
	FirstPermanentLine::OnHousePlaced(TownID{0}, TileIndex{0});
	FirstPermanentLine::OnHouseCompleted(TileIndex{0});
	FirstPermanentLine::OnVehicleRemoved(VehicleID{0});
	FirstPermanentLine::Evaluate();
	CHECK(FirstPermanentLine::Save() == before);
	CHECK(FirstPermanentLine::Load(""));
	CHECK_FALSE(FirstPermanentLine::Active());
	CHECK(FirstPermanentLine::Load(R"({"schema":1,"active":false})"));
	CHECK_FALSE(FirstPermanentLine::Active());
}

TEST_CASE("First Permanent Line schema one round trips without query mutation", "[first-permanent-line]")
{
	FirstPermanentLine::Reset();
	REQUIRE(FirstPermanentLine::Load(BaselineChunk().dump()));
	FirstPermanentLine::OnEconomyEvaluation();
	FirstPermanentLine::OnEconomyEvaluation();
	CHECK(FirstPermanentLine::State().economy_evaluations == 2);
	const std::string saved = FirstPermanentLine::Save();
	CHECK(FirstPermanentLine::State().active);
	CHECK_FALSE(FirstPermanentLine::State().awarded);
	CHECK(FirstPermanentLine::Save() == saved);
	CHECK(FirstPermanentLine::Load(saved));
	CHECK(FirstPermanentLine::Save() == saved);
	FirstPermanentLine::Reset();
}

TEST_CASE("First Permanent Line rejects malformed, old and implausible chunks transactionally", "[first-permanent-line]")
{
	FirstPermanentLine::Reset();
	REQUIRE(FirstPermanentLine::Load(BaselineChunk().dump()));
	const std::string saved = FirstPermanentLine::Save();
	auto reject = [&](const Json &chunk) {
		CHECK_FALSE(FirstPermanentLine::Load(chunk.dump()));
		CHECK(FirstPermanentLine::Save() == saved);
	};
	Json old = BaselineChunk(); old["schema"] = 2; reject(old);
	Json old_inactive = {{"schema", 2}, {"active", false}}; reject(old_inactive);
	Json signed_value = BaselineChunk(); signed_value["stored"][0] = -1; reject(signed_value);
	Json fractional = BaselineChunk(); fractional["stored"][0] = 1.5; reject(fractional);
	Json excess = BaselineChunk(); excess["stored"][0] = 1000001; reject(excess);
	Json eval_excess = BaselineChunk(); eval_excess["economy_evaluations"] = uint64_t(UINT32_MAX) + 1; reject(eval_excess);
	Json wrong_type = BaselineChunk(); wrong_type["delivered"][0] = 1; reject(wrong_type);
	Json premature = BaselineChunk(); premature["commissioned"] = true; reject(premature);
	Json impossible = BaselineChunk(); impossible["produced"][0] = true; reject(impossible);
	Json duplicate_binding = BaselineChunk(); duplicate_binding["binding"][3] = 0; reject(duplicate_binding);
	Json extra_field = BaselineChunk(); extra_field["unexpected"] = 1; reject(extra_field);
	Json max_counter = BaselineChunk(); max_counter["economy_evaluations"] = UINT32_MAX;
	REQUIRE(FirstPermanentLine::Load(max_counter.dump()));
	FirstPermanentLine::OnEconomyEvaluation();
	CHECK(FirstPermanentLine::State().invalid);
	CHECK(FirstPermanentLine::State().reason == "evaluation-budget-exhausted");
	REQUIRE(FirstPermanentLine::Load(BaselineChunk().dump()));
	CHECK_FALSE(FirstPermanentLine::Load("{broken"));
	CHECK(FirstPermanentLine::Save() == saved);
	FirstPermanentLine::Reset();
}

TEST_CASE("First Permanent Line rejects duplicate houses and train identities", "[first-permanent-line]")
{
	FirstPermanentLine::Reset();
	Json complete = BaselineChunk();
	complete["delivered"] = {true, true};
	complete["produced"] = {true, true};
	complete["stored"] = {160, 80};
	complete["commissioned"] = true;
	complete["house_count"] = 2;
	complete["houses"] = {{100, true}, {100, false}};
	CHECK_FALSE(FirstPermanentLine::Load(complete.dump()));
	complete["houses"] = {{100, true}, {101, false}};
	complete["journeys"] = {{5, 0, 65535, 0, 0}, {5, 0, 65535, 0, 0}};
	CHECK_FALSE(FirstPermanentLine::Load(complete.dump()));
	complete["journeys"] = {{5, 3, 65535, 1, 1}};
	CHECK_FALSE(FirstPermanentLine::Load(complete.dump()));
	complete["journeys"] = {{5, 0, 65535, 0, 0}};
	REQUIRE(FirstPermanentLine::Load(complete.dump()));
	REQUIRE(FirstPermanentLine::State().journeys.size() == 1);
	FirstPermanentLine::OnVehicleRemoved(VehicleID{5});
	CHECK(FirstPermanentLine::State().journeys.empty());
	FirstPermanentLine::Reset();
	CHECK_FALSE(FirstPermanentLine::Active());
}

namespace {
struct FirstLineNativeFixture {
	std::array<IndustrySpec, 2> old_industry_specs;
	std::array<CargoSpec, NUM_CARGO> old_cargos;
	Industry *steel = nullptr;
	Industry *ballast = nullptr;
	Industry *iron = nullptr;
	Industry *silica = nullptr;
	Train *train = nullptr;
	Town *town = nullptr;
	Station *source_station = nullptr;
	Station *hub_station = nullptr;
	Station *unbound_station = nullptr;

	FirstLineNativeFixture(bool designate_megacity = true)
	{
		FirstPermanentLine::Reset();
		for (size_t i = 0; i < NUM_CARGO; ++i) old_cargos[i] = *CargoSpec::Get(CargoType(i));
		old_industry_specs[0] = *GetIndustrySpec(IndustryType{0});
		old_industry_specs[1] = *GetIndustrySpec(IndustryType{1});
		SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
		PlanetManager::Reset();
		MegacityManager::Reset();
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "Mito", .phase = WorldPhase::Phase1_Core,
			.min_x = 1, .min_y = 1, .max_x = 20, .max_y = 62}));
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{1}, .name = "Merredin", .phase = WorldPhase::Phase2_Developed,
			.min_x = 21, .min_y = 1, .max_x = 40, .max_y = 62}));
		REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{2}, .name = "Clonclurry", .phase = WorldPhase::Phase3_Frontier,
			.min_x = 41, .min_y = 1, .max_x = 62, .max_y = 62}));
		const char *labels[] = {"SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP", "SIGE", "MACH", "RARE",
			"ALLO", "OIL_", "POLY", "MGLA", "BCRY", "QCRY", "CCRY", "GRAI", "FOOD", "BALL"};
		for (uint8_t c = 0; c < std::size(labels); ++c) {
			CargoLabel label;
			std::copy_n(labels[c], 4, label.begin());
			CargoSpec::Get(CargoType(c))->label = label;
			CargoSpec::Get(CargoType(c))->bitnum = c;
		}
		BuildCargoLabelMap();
		IntegratedEconomy::Reset();
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Core);
		IntegratedEconomy::RegisterRole(WorldID{1}, EconomicRole::Industrial);
		IntegratedEconomy::RegisterRole(WorldID{2}, EconomicRole::Frontier);
		for (uint8_t type = 0; type < 2; ++type) {
			auto spec = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{type}));
			spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
			spec->grf_prop.local_id = type == 0 ? 0x13 : 0x11;
		}
		REQUIRE(Industry::CanAllocateItem());
		steel = Industry::Create(TileXY(27, 20));
		steel->type = IndustryType{0}; steel->founder = CompanyID{0}; steel->location = TileArea(TileXY(27, 20), 1, 1);
		steel->accepted.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"IRON"})});
		steel->produced.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"STEL"})});
		IntegratedEconomy::RegisterIndustry(steel);
		REQUIRE(Industry::CanAllocateItem());
		ballast = Industry::Create(TileXY(33, 20));
		ballast->type = IndustryType{1}; ballast->founder = CompanyID{0}; ballast->location = TileArea(TileXY(33, 20), 1, 1);
		ballast->accepted.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"SILC"})});
		ballast->produced.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"BALL"})});
		IntegratedEconomy::RegisterIndustry(ballast);
		REQUIRE(Industry::CanAllocateItem());
		iron = Industry::Create(TileXY(48, 20));
		iron->type = IndustryType{2}; iron->location = TileArea(TileXY(48, 20), 1, 1);
		iron->produced.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"IRON"})});
		REQUIRE(Industry::CanAllocateItem());
		silica = Industry::Create(TileXY(54, 20));
		silica->type = IndustryType{3}; silica->location = TileArea(TileXY(54, 20), 1, 1);
		silica->produced.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"SILC"})});
		town = Town::GetIfValid(TownID{0});
		REQUIRE(town != nullptr);
		if (designate_megacity) {
			REQUIRE(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, town->index).Succeeded());
			const MegacityProfile *profile = MegacityManager::GetProfile(town->index);
			REQUIRE(profile != nullptr);
			REQUIRE(profile->world_id == WorldID{0});
		}
		REQUIRE(Train::CanAllocateItem());
		train = Train::Create(); train->owner = CompanyID{0};
		train->SetFrontEngine(); train->SetEngine(); train->vehstatus.Reset(VehState::Stopped);
	}
	~FirstLineNativeFixture()
	{
		FirstPermanentLine::Reset();
		_vehicle_pool.CleanPool(); ResetVehicleHash();
		_orderlist_pool.CleanPool(); _station_pool.CleanPool(); _depot_pool.CleanPool();
		LogisticsHubManager::Reset();
		IntegratedEconomy::Reset(); MegacityManager::Reset(); PlanetManager::Reset();
		_industry_pool.CleanPool(); _town_pool.CleanPool();
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = old_industry_specs[0];
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{1})) = old_industry_specs[1];
		for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(CargoType(i)) = old_cargos[i];
		BuildCargoLabelMap();
		UnInitWindowSystem();
	}
	Station *MakeStation(TileIndex tile)
	{
		REQUIRE(Station::CanAllocateItem());
		Station *station = Station::Create(tile);
		station->owner = CompanyID{0}; station->town = town;
		station->facilities.Set(StationFacility::Train);
		station->train_station = TileArea(tile, 1, 1);
		station->spread = station->train_station;
		MakeRailStation(tile, CompanyID{0}, station->index, Axis::X, 0, RAILTYPE_RAIL);
		Company::Get(CompanyID{0})->infrastructure.station++;
		Company::Get(CompanyID{0})->infrastructure.rail[RAILTYPE_RAIL]++;
		RebuildStationKdtree(); station->RecomputeCatchment();
		return station;
	}
	void MakeService(bool auxiliary_orders = false, bool implicit_orders = false)
	{
		source_station = MakeStation(TileXY(27, 19));
		hub_station = MakeStation(TileXY(12, 15));
		unbound_station = MakeStation(TileXY(17, 15));
		REQUIRE(LogisticsHubManager::RegisterHub(hub_station->xy, WorldID{0}, CompanyID{0}, hub_station->index, "Mito Hub") != 0);
		Order pickup; pickup.MakeGoToStation(source_station->index);
		pickup.SetLoadType(OrderLoadType::FullLoad);
		pickup.SetUnloadType(OrderUnloadType::NoUnload);
		Order drop; drop.MakeGoToStation(hub_station->index);
		drop.SetLoadType(OrderLoadType::NoLoad);
		drop.SetUnloadType(OrderUnloadType::UnloadIfPossible);
		REQUIRE(OrderList::CanAllocateItem());
		std::vector<Order> schedule;
		schedule.push_back(std::move(pickup)); schedule.push_back(std::move(drop));
		if (auxiliary_orders) {
			REQUIRE(Waypoint::CanAllocateItem());
			TileIndex waypoint_tile = TileXY(25, 15);
			Waypoint *waypoint = Waypoint::Create(waypoint_tile);
			waypoint->owner = CompanyID{0}; waypoint->town = town;
			waypoint->facilities.Set(StationFacility::Train);
			waypoint->train_station = TileArea(waypoint_tile, 1, 1);
			MakeRailWaypoint(waypoint_tile, CompanyID{0}, waypoint->index, Axis::X, 0, RAILTYPE_RAIL);
			REQUIRE(Depot::CanAllocateItem());
			TileIndex depot_tile = TileXY(24, 15);
			Depot *depot = Depot::Create(depot_tile);
			MakeRailDepot(depot_tile, CompanyID{0}, depot->index, DiagDirection::SW, RAILTYPE_RAIL);
			Order via_station; via_station.MakeGoToStation(unbound_station->index);
			via_station.SetNonStopType(OrderNonStopFlag::GoVia);
			Order via_waypoint; via_waypoint.MakeGoToWaypoint(waypoint->index);
			Order service; service.MakeGoToDepot(depot->index, {});
			Order conditional; conditional.MakeConditional(VehicleOrderID{0});
			schedule.push_back(std::move(via_station));
			schedule.push_back(std::move(via_waypoint));
			schedule.push_back(std::move(service));
			schedule.push_back(std::move(conditional));
		}
		if (implicit_orders) {
			Order intermediate; intermediate.MakeImplicit(unbound_station->index);
			Order source_call; source_call.MakeImplicit(source_station->index);
			Order hub_call; hub_call.MakeImplicit(hub_station->index);
			schedule.push_back(std::move(intermediate));
			schedule.push_back(std::move(source_call));
			schedule.push_back(std::move(hub_call));
		}
		train->orders = OrderList::Create(std::move(schedule), train);
	}
	void Arrive(Station *station, VehicleOrderID index)
	{
		train->cur_real_order_index = index;
		train->current_order = *train->GetOrder(index);
		FirstPermanentLine::OnArrival(train->index, station->index);
	}
	void ImplicitArrive(Station *station, VehicleOrderID next_explicit_index)
	{
		/* Before native BeginLoading inserts tracking, current_order is still the next explicit stop. */
		const Order *next = train->GetOrder(next_explicit_index);
		REQUIRE(next != nullptr);
		REQUIRE(next->IsType(OT_GOTO_STATION));
		REQUIRE(next->GetDestination().ToStationID() != station->index);
		train->cur_real_order_index = next_explicit_index;
		train->current_order = *next;
		FirstPermanentLine::OnArrival(train->index, station->index);
		train->current_order.MakeLoading(false);
	}
	/** Observer fixture: represent four actual protected native house tiles and the native district counters. */
	void PlaceCompletedHouses()
	{
		auto *district = IntegratedEconomy::GetTownResidentialDistrict(town->index);
		REQUIRE(district != nullptr);
		for (uint x = 10; x < 14; ++x) {
			TileIndex tile = TileXY(x, 19);
			MakeHouseTile(tile, town->index, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true);
			FirstPermanentLine::OnHousePlaced(town->index, tile);
		}
		district->buildings_placed = 4;
		district->buildings_completed = 4;
		district->building_opportunities = 0;
	}
	FirstPermanentLineBindings Bindings() const
	{
		return {CompanyID{0}, town->index, steel->index, ballast->index, iron->index, silica->index};
	}
};
}

TEST_CASE("First Permanent Line requires a native Core Megacity profile at Begin", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture(false);
	CHECK(MegacityManager::GetProfile(fixture.town->index) == nullptr);
	CHECK_FALSE(FirstPermanentLine::Begin(fixture.Bindings()));
	CHECK_FALSE(FirstPermanentLine::Active());
	REQUIRE(MegacityManager::RegisterMegacity(fixture.town->index, WorldID{1}, "Wrong-world profile"));
	CHECK_FALSE(FirstPermanentLine::Begin(fixture.Bindings()));
	CHECK_FALSE(FirstPermanentLine::Active());
	REQUIRE(MegacityManager::UnregisterMegacity(fixture.town->index));
	REQUIRE(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, fixture.town->index).Succeeded());
	const MegacityProfile *profile = MegacityManager::GetProfile(fixture.town->index);
	REQUIRE(profile != nullptr);
	REQUIRE(profile->world_id == WorldID{0});
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	REQUIRE(MegacityManager::UnregisterMegacity(fixture.town->index));
	FirstPermanentLine::Evaluate();
	CHECK(FirstPermanentLine::State().invalid);
	CHECK(FirstPermanentLine::State().reason == "objective-binding-lost");
}

TEST_CASE("First Permanent Line retains service evidence across implicit intermediate stops", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture;
	fixture.MakeService(false, true);
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	const VehicleID id = fixture.train->index;
	const CargoType steel = GetCargoTypeByLabel(CargoLabel{"STEL"});
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	REQUIRE(FirstPermanentLine::State().journeys.at(id).visit == 1);
	fixture.ImplicitArrive(fixture.hub_station, VehicleOrderID{0});
	/* State-level negative: stale explicit-hub visit with an unordered loading marker. */
	fixture.train->cur_real_order_index = VehicleOrderID{1};
	fixture.train->current_order.MakeLoading(false);
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.steel->index, fixture.hub_station->index, steel, 1);
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 0); // implicit loading cannot use stale explicit visit
	fixture.ImplicitArrive(fixture.unbound_station, VehicleOrderID{1});
	FirstPermanentLine::Evaluate();
	REQUIRE(FirstPermanentLine::State().journeys.contains(id));
	CHECK(FirstPermanentLine::State().journeys.at(id).visit == 1);
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	fixture.train->current_order.MakeLoading(true);
	fixture.train->current_order.SetNonStopType({OrderNonStopFlag::NonStop, OrderNonStopFlag::GoVia});
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.steel->index, fixture.hub_station->index, steel, 1);
	REQUIRE(FirstPermanentLine::State().journeys.at(id).phase == 1); // ordered loading marker credits real stop
	fixture.ImplicitArrive(fixture.source_station, VehicleOrderID{1});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1); // implicit source cannot witness return
	FirstPermanentLine::Evaluate();
	REQUIRE(FirstPermanentLine::State().journeys.contains(id));
	const std::string unfinished = FirstPermanentLine::Save();
	REQUIRE(FirstPermanentLine::Load(unfinished));
	REQUIRE(FirstPermanentLine::ValidateAfterLoad());
	REQUIRE(FirstPermanentLine::State().journeys.contains(id));
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 2);
}

TEST_CASE("First Permanent Line permits owned auxiliary route orders", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture;
	fixture.MakeService(true);
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	const VehicleID id = fixture.train->index;
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.steel->index, fixture.hub_station->index,
		GetCargoTypeByLabel(CargoLabel{"STEL"}), 1);
	REQUIRE(FirstPermanentLine::State().journeys.at(id).phase == 1);
	fixture.train->GetOrder(0)->SetNonStopType(OrderNonStopFlag::GoVia);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1);
	fixture.train->GetOrder(0)->SetNonStopType(OrderNonStopFlag::NonStop);
	fixture.train->GetOrder(4)->SetDepotActionType(OrderDepotActionFlag::Halt);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1);
	fixture.train->GetOrder(4)->SetDepotActionType({});
	fixture.train->GetOrder(5)->SetConditionSkipToOrder(INVALID_VEH_ORDER_ID);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1);
	fixture.train->GetOrder(5)->SetConditionSkipToOrder(VehicleOrderID{0});
	fixture.train->GetOrder(4)->SetDepotActionType(OrderDepotActionFlag::NearestDepot);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 2); // nearest-depot and conditional are auxiliary
}

TEST_CASE("First Permanent Line accepts only exact native train inputs then later production", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture;
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	CHECK_FALSE(FirstPermanentLine::Begin(fixture.Bindings()));
	const CargoType iron = GetCargoTypeByLabel(CargoLabel{"IRON"});
	const CargoType silica = GetCargoTypeByLabel(CargoLabel{"SILC"});
	const CargoType steel = GetCargoTypeByLabel(CargoLabel{"STEL"});
	FirstPermanentLine::OnProduction(fixture.steel->index, 2);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{1}, fixture.iron->index, fixture.steel->index, iron, 5);
	REQUIRE(Train::CanAllocateItem());
	Train *foreign = Train::Create(); foreign->owner = CompanyID{1}; SetBit(foreign->subtype, GVSF_FRONT);
	FirstPermanentLine::OnIndustryDelivery(foreign->index, CompanyID{0}, fixture.iron->index, fixture.steel->index, iron, 5);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.silica->index, fixture.steel->index, iron, 5);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.steel->index, steel, 5);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.ballast->index, iron, 5);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.steel->index, iron, 0);
	CHECK_FALSE(FirstPermanentLine::State().input_delivered[0]);
	CHECK_FALSE(FirstPermanentLine::State().produced_after_input[0]);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.steel->index, iron, 1);
	CHECK(FirstPermanentLine::State().input_delivered[0]);
	CHECK_FALSE(FirstPermanentLine::State().produced_after_input[0]);
	FirstPermanentLine::OnProduction(fixture.steel->index, 0);
	CHECK_FALSE(FirstPermanentLine::State().produced_after_input[0]);
	FirstPermanentLine::OnProduction(fixture.steel->index, 1);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.silica->index, fixture.ballast->index, silica, 1);
	FirstPermanentLine::OnProduction(fixture.ballast->index, 1);
	CHECK(FirstPermanentLine::State().produced_after_input[0]);
	CHECK(FirstPermanentLine::State().produced_after_input[1]);
	CHECK_FALSE(FirstPermanentLine::State().commissioned);
	FirstPermanentLine::OnCommission(CompanyID{0}, fixture.town->index);
	CHECK(FirstPermanentLine::State().invalid);
	CHECK(FirstPermanentLine::State().reason == "premature-or-unsupported-commission");
}

TEST_CASE("First Permanent Line native warehouse journey spans commissioning and deduplicates visits", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture;
	fixture.MakeService();
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	const CargoType iron = GetCargoTypeByLabel(CargoLabel{"IRON"});
	const CargoType silica = GetCargoTypeByLabel(CargoLabel{"SILC"});
	const CargoType steel = GetCargoTypeByLabel(CargoLabel{"STEL"});
	const CargoType ballast = GetCargoTypeByLabel(CargoLabel{"BALL"});
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.steel->index, iron, 1);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.silica->index, fixture.ballast->index, silica, 1);
	FirstPermanentLine::OnProduction(fixture.steel->index, 1);
	FirstPermanentLine::OnProduction(fixture.ballast->index, 1);
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.steel->index, fixture.unbound_station->index, steel, 500);
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index, steel, 500);
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{1}, fixture.steel->index, fixture.hub_station->index, steel, 500);
	CHECK(FirstPermanentLine::State().stored_units == std::array<uint32_t, 2>{0, 0});
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.steel->index, fixture.hub_station->index, steel, 160);
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index, ballast, 80);
	const auto id = fixture.train->index;
	REQUIRE(FirstPermanentLine::State().journeys.at(id).phase == 1);
	CHECK(FirstPermanentLine::State().journeys.at(id).last_deposit_visit == FirstPermanentLine::State().journeys.at(id).visit);
	CHECK(FirstPermanentLine::State().stored_units == std::array<uint32_t, 2>{160, 80});
	FirstPermanentLine::OnArrival(id, fixture.unbound_station->index);
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1); // unscheduled wrong source
	/* A source call after the first deposit is real even before residential commissioning. */
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	REQUIRE(FirstPermanentLine::State().journeys.at(id).phase == 2);
	const std::string precommission = FirstPermanentLine::Save();
	REQUIRE(FirstPermanentLine::Load(precommission));
	CHECK(FirstPermanentLine::ValidateAfterLoad());
	CHECK(FirstPermanentLine::Save() == precommission);
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index, ballast, 1);
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 2); // second deposit was before M2
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, steel, 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, ballast, 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, fixture.town->index).Succeeded());
	/* The native command invokes OnCommission and Evaluate itself. */
	REQUIRE(FirstPermanentLine::State().commissioned);
	REQUIRE_FALSE(FirstPermanentLine::State().invalid);
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index, ballast, 1);
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 2); // same visit fragment
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 2); // return alone is not a deposit
	FirstPermanentLine::Evaluate();
	CHECK_FALSE(FirstPermanentLine::State().awarded);
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index, ballast, 1);
	REQUIRE(FirstPermanentLine::State().journeys.at(id).phase == 3); // BALL also counts
	/* The train may return to its source before the houses complete. */
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	fixture.PlaceCompletedHouses();
	REQUIRE(FirstPermanentLine::State().house_count == 4);
	fixture.train->vehstatus.Set(VehState::Stopped);
	FirstPermanentLine::Evaluate();
	CHECK_FALSE(FirstPermanentLine::State().awarded);
	fixture.train->vehstatus.Reset(VehState::Stopped);
	FirstPermanentLine::Evaluate(); // native completed state covers delayed OnHouseCompleted callbacks
	REQUIRE(FirstPermanentLine::State().awarded);
	const uint32_t prior_evaluations = FirstPermanentLine::State().economy_evaluations;
	FirstPermanentLine::OnEconomyEvaluation();
	CHECK(FirstPermanentLine::State().economy_evaluations == prior_evaluations + 1);
	CHECK(FirstPermanentLine::State().awarded);
	FirstPermanentLine::OnVehicleRemoved(id);
	REQUIRE(FirstPermanentLine::State().journeys.empty());
	const std::string latched = FirstPermanentLine::Save();
	FirstPermanentLine::Reset();
	REQUIRE(FirstPermanentLine::Load(latched));
	CHECK(FirstPermanentLine::ValidateAfterLoad());
	CHECK(FirstPermanentLine::State().awarded);
	CHECK(FirstPermanentLine::State().journeys.empty());
}

TEST_CASE("First Permanent Line native demolition and invalid orders cannot revive a mission", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture;
	fixture.MakeService();
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	const VehicleID id = fixture.train->index;
	FirstPermanentLine::OnStoredDelivery(id, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index,
		GetCargoTypeByLabel(CargoLabel{"BALL"}), 1);
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 1); // BALL may be the first deposit
	const uint32_t initial_visit = FirstPermanentLine::State().journeys.at(id).visit;
	fixture.train->GetOrder(0)->SetLoadType(OrderLoadType::NoLoad);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).visit == initial_visit);
	fixture.train->GetOrder(0)->SetLoadType(OrderLoadType::FullLoad);
	fixture.train->GetOrder(1)->SetUnloadType(OrderUnloadType::NoUnload);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).visit == initial_visit);
	fixture.train->GetOrder(1)->SetUnloadType(OrderUnloadType::UnloadIfPossible);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	CHECK(FirstPermanentLine::State().journeys.at(id).visit == initial_visit + 1);
	FirstPermanentLine::OnVehicleRemoved(id);
	CHECK(FirstPermanentLine::State().journeys.empty());
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	CHECK(FirstPermanentLine::State().journeys.at(id).phase == 0); // reused ID has no old deposit
	/* Establish the native residential district before testing demolition custody. */
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, GetCargoTypeByLabel(CargoLabel{"STEL"}), 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, GetCargoTypeByLabel(CargoLabel{"BALL"}), 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, fixture.town->index).Succeeded());
	/* House removal invalidates the registered anchor before a same-tile replacement. */
	Json commissioned = BaselineChunk();
	commissioned["delivered"] = {true, true}; commissioned["produced"] = {true, true};
	commissioned["stored"] = {160, 80}; commissioned["commissioned"] = true;
	commissioned["binding"] = {0, fixture.town->index.base(), fixture.steel->index.base(), fixture.ballast->index.base(), fixture.iron->index.base(), fixture.silica->index.base()};
	REQUIRE(FirstPermanentLine::Load(commissioned.dump()));
	TileIndex tile = TileXY(10, 19);
	MakeHouseTile(tile, fixture.town->index, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true);
	++fixture.town->cache.num_houses;
	fixture.town->cache.population += HouseSpec::Get(HouseID{0})->population;
	InitializeBuildingCounts(fixture.town);
	REQUIRE_FALSE(fixture.town->cache.building_counts.id_count.empty());
	IncreaseBuildingCount(fixture.town, HouseID{0});
	FirstPermanentLine::OnHousePlaced(fixture.town->index, tile);
	REQUIRE(FirstPermanentLine::State().house_count == 1);
	ClearTownHouse(fixture.town, tile); // Native removal invokes the observer and clears the tile.
	REQUIRE(IsTileType(tile, TileType::Clear));
	CHECK(FirstPermanentLine::State().invalid);
	CHECK(FirstPermanentLine::State().reason == "objective-house-removed");
	MakeHouseTile(tile, fixture.town->index, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, true);
	FirstPermanentLine::OnHousePlaced(fixture.town->index, tile);
	FirstPermanentLine::Evaluate();
	CHECK_FALSE(FirstPermanentLine::State().awarded);
}

TEST_CASE("First Permanent Line source return without a new positive hub deposit cannot win", "[first-permanent-line]")
{
	FirstLineNativeFixture fixture;
	fixture.MakeService();
	REQUIRE(FirstPermanentLine::Begin(fixture.Bindings()));
	const CargoType iron = GetCargoTypeByLabel(CargoLabel{"IRON"});
	const CargoType silica = GetCargoTypeByLabel(CargoLabel{"SILC"});
	const CargoType steel = GetCargoTypeByLabel(CargoLabel{"STEL"});
	const CargoType ballast = GetCargoTypeByLabel(CargoLabel{"BALL"});
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.steel->index, iron, 1);
	FirstPermanentLine::OnIndustryDelivery(fixture.train->index, CompanyID{0}, fixture.silica->index, fixture.ballast->index, silica, 1);
	FirstPermanentLine::OnProduction(fixture.steel->index, 1);
	FirstPermanentLine::OnProduction(fixture.ballast->index, 1);
	fixture.Arrive(fixture.hub_station, VehicleOrderID{1});
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.steel->index, fixture.hub_station->index, steel, 160);
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.ballast->index, fixture.hub_station->index, ballast, 80);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, steel, 160);
	StockpileManager::AddCargo(WorldID{0}, CompanyID{0}, ballast, 80);
	REQUIRE(Command<Commands::CommissionTownResidentialDistrict>::Do(DoCommandFlag::Execute, fixture.town->index).Succeeded());
	REQUIRE(FirstPermanentLine::State().commissioned);
	REQUIRE_FALSE(FirstPermanentLine::State().invalid);
	fixture.PlaceCompletedHouses();
	REQUIRE(FirstPermanentLine::State().house_count == 4);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	REQUIRE(FirstPermanentLine::State().journeys.at(fixture.train->index).phase == 2);
	FirstPermanentLine::Evaluate();
	CHECK_FALSE(FirstPermanentLine::State().awarded);
	fixture.Arrive(fixture.source_station, VehicleOrderID{0});
	FirstPermanentLine::Evaluate();
	CHECK_FALSE(FirstPermanentLine::State().awarded);
	FirstPermanentLine::OnStoredDelivery(fixture.train->index, CompanyID{0}, fixture.iron->index, fixture.hub_station->index, steel, 10);
	FirstPermanentLine::Evaluate();
	CHECK_FALSE(FirstPermanentLine::State().awarded);
}

TEST_CASE("First Permanent Line postload missing references invalidate only the mission", "[first-permanent-line]")
{
	FirstPermanentLine::Reset();
	REQUIRE(FirstPermanentLine::Load(BaselineChunk().dump()));
	CHECK(FirstPermanentLine::ValidateAfterLoad());
	CHECK(FirstPermanentLine::State().invalid);
	CHECK(FirstPermanentLine::State().reason == "objective-binding-lost");
	const std::string invalid = FirstPermanentLine::Save();
	FirstPermanentLine::Reset();
	REQUIRE(FirstPermanentLine::Load(invalid));
	CHECK(FirstPermanentLine::ValidateAfterLoad());
	CHECK(FirstPermanentLine::State().invalid);
	CHECK(FirstPermanentLine::Save() == invalid);
	FirstPermanentLine::Reset();
}
