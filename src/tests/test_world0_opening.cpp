/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_world0_opening.cpp Fixed native World 0 observer fixtures; no campaign generation. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include "../portal/world0_opening.h"
#include "../portal/first_permanent_line.h"
#include "../portal/integrated_economy.h"
#include "../portal/logistics_hub.h"
#include "../portal/planet_manager.h"
#include "../portal/resource_sites.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/connected_economy.h"
#include "../portal/production_chain.h"
#include "../cargotype.h"
#include "../company_base.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../town.h"
#include "../train.h"
#include "../vehicle_func.h"
#include "../window_func.h"
#include <algorithm>
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

namespace {
using Json = nlohmann::json;
CargoType C(const char (&label)[5]) { return GetCargoTypeByLabel(CargoLabel{label}); }

struct ValeFixture {
	IndustrySpec old_quarry = *GetIndustrySpec(IndustryType{0});
	IndustrySpec old_crusher = *GetIndustrySpec(IndustryType{2});
	std::array<CargoSpec, NUM_CARGO> old_cargos;
	World0OpeningProfile profile;
	Industry *quarry = nullptr;
	Industry *crusher = nullptr;
	Station *source = nullptr;
	Station *yard = nullptr;
	Train *train = nullptr;

	ValeFixture()
	{
		for (size_t i = 0; i < NUM_CARGO; ++i) old_cargos[i] = *CargoSpec::Get(CargoType(i));
		World0Opening::Reset(); FirstPermanentLine::Reset();
		ResourceSiteManager::Reset(); IntegratedEconomy::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 2000);
		ResetIndustries();
		Town *greybank = Town::Get(TownID{0}); greybank->cache.population = 1200;
		REQUIRE(Town::CanAllocateItem());
		Town *stonecross = Town::Create(TileXY(40, 10)); stonecross->cache.population = 350;
		REQUIRE(Town::CanAllocateItem());
		Town *merefield = Town::Create(TileXY(20, 48)); merefield->cache.population = 240;
		for (auto [cargo, label] : {std::pair{CargoType{0}, CargoLabel{"SILC"}},
				std::pair{CargoType{1}, CargoLabel{"BALL"}}}) CargoSpec::Get(cargo)->label = label;
		BuildCargoLabelMap();
		auto *quarry_spec = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}));
		quarry_spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		quarry_spec->grf_prop.local_id = 0x10;
		quarry_spec->produced_cargo.fill(INVALID_CARGO);
		quarry_spec->produced_cargo[0] = C("SILC");
		auto *crusher_spec = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{2}));
		crusher_spec->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		crusher_spec->grf_prop.local_id = 0x11;
		crusher_spec->accepts_cargo.fill(INVALID_CARGO);
		crusher_spec->accepts_cargo[0] = C("SILC");
		crusher_spec->produced_cargo.fill(INVALID_CARGO);
		crusher_spec->produced_cargo[0] = C("BALL");
		uint32_t site = ResourceSiteManager::AddSite(TileXY(38, 23), IndustryType{0}, 8, 8);
		REQUIRE(site != 0);
		profile = {CompanyID{0}, {greybank->index, stonecross->index, merefield->index}, TileXY(30, 30), site};
	}
	~ValeFixture()
	{
		World0Opening::Reset(); FirstPermanentLine::Reset();
		_vehicle_pool.CleanPool(); ResetVehicleHash();
		_orderlist_pool.CleanPool(); _station_pool.CleanPool();
		LogisticsHubManager::Reset(); ResourceSiteManager::Reset();
		IntegratedEconomy::Reset(); PlanetManager::Reset();
		_industry_pool.CleanPool(); _town_pool.CleanPool();
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = old_quarry;
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{2})) = old_crusher;
		for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(CargoType(i)) = old_cargos[i];
		BuildCargoLabelMap(); UnInitWindowSystem();
	}
	void Stage()
	{
		REQUIRE(World0Opening::BeginAuthoring(profile));
		IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
		IntegratedEconomy::SetEnabled(true);
		IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Frontier);
		ResourceSiteManager::SetEnabled(true);
		IntegratedEconomy::ConfigureRecipes();
	}
	Station *MakeStation(TileIndex tile, TownID town)
	{
		REQUIRE(Station::CanAllocateItem());
		Station *station = Station::Create(tile);
		station->owner = CompanyID{0}; station->town = Town::Get(town);
		station->facilities.Set(StationFacility::Train);
		station->train_station = TileArea(tile, 1, 1); station->spread = station->train_station;
		MakeRailStation(tile, CompanyID{0}, station->index, Axis::X, 0, RAILTYPE_RAIL);
		Company::Get(CompanyID{0})->infrastructure.station++;
		Company::Get(CompanyID{0})->infrastructure.rail[RAILTYPE_RAIL]++;
		RebuildStationKdtree(); station->RecomputeCatchment();
		return station;
	}
	void Bind()
	{
		Stage();
		REQUIRE(Industry::CanAllocateItem());
		quarry = Industry::Create(TileXY(40, 25));
		quarry->type = IndustryType{0}; quarry->founder = CompanyID{0};
		quarry->location = TileArea(TileXY(40, 25), 1, 1);
		quarry->produced.push_back({.cargo = C("SILC")});
		ResourceSiteManager::Occupy(quarry->location.tile, quarry->type, GetIndustrySpec(quarry->type)->layouts[0], quarry->index);
		ResourceSiteManager::RestoreSurvey(CompanyID{0}, TileXY(38, 23));
		REQUIRE(Industry::CanAllocateItem());
		crusher = Industry::Create(profile.crusher_plot);
		crusher->type = IndustryType{2}; crusher->founder = CompanyID{0};
		crusher->location = TileArea(profile.crusher_plot, 1, 1);
		crusher->accepted.push_back({.cargo = C("SILC")}); crusher->produced.push_back({.cargo = C("BALL")});
		IntegratedEconomy::RegisterIndustry(crusher);
		source = MakeStation(TileXY(30, 29), profile.towns[1]);
		yard = MakeStation(TileXY(12, 15), profile.towns[0]);
		REQUIRE(LogisticsHubManager::RegisterHub(yard->xy, WorldID{0}, CompanyID{0}, yard->index, "Greybank Yard") != 0);
		REQUIRE(Train::CanAllocateItem());
		train = Train::Create(); train->owner = CompanyID{0}; train->SetFrontEngine(); train->SetEngine();
		train->vehstatus.Reset(VehState::Stopped);
		Order pickup; pickup.MakeGoToStation(source->index); pickup.SetLoadType(OrderLoadType::FullLoad);
		Order drop; drop.MakeGoToStation(yard->index); drop.SetUnloadType(OrderUnloadType::UnloadIfPossible);
		REQUIRE(OrderList::CanAllocateItem());
		std::vector<Order> schedule; schedule.push_back(std::move(pickup)); schedule.push_back(std::move(drop));
		train->orders = OrderList::Create(std::move(schedule), train);
		REQUIRE(World0Opening::Begin({quarry->index, crusher->index, yard->index}));
	}
	void Arrive(Station *station, VehicleOrderID index)
	{
		train->cur_real_order_index = index;
		train->current_order = *train->GetOrder(index);
		World0Opening::OnArrival(train->index, station->index);
	}
	void Deposit(uint32_t units)
	{
		Arrive(yard, VehicleOrderID{1});
		train->current_order.MakeLoading(true);
		/* Vehicle::BeginLoading marks an ordered stop after MakeLoading. */
		train->current_order.SetNonStopType({OrderNonStopFlag::NonStop, OrderNonStopFlag::GoVia});
		REQUIRE(LogisticsHubManager::DepositToStockpile(yard->xy, CompanyID{0}, C("BALL"), units));
		World0Opening::OnStoredDelivery(train->index, CompanyID{0}, crusher->index, yard->index, C("BALL"), units);
		World0Opening::Evaluate();
	}
};
}

TEST_CASE("World 0 missing chunk and strict schema are inert and transactional", "[world0-opening]")
{
	World0Opening::Reset();
	CHECK_FALSE(World0Opening::Active());
	CHECK(World0Opening::Save().empty());
	World0Opening::OnEconomyEvaluation(); World0Opening::Evaluate();
	CHECK(World0Opening::Save().empty());
	CHECK(World0Opening::Load(""));
	CHECK(World0Opening::Load(R"({"schema":1,"active":false})"));
	REQUIRE_FALSE(World0Opening::Load(R"({"schema":0,"active":false})"));
	REQUIRE_FALSE(World0Opening::Load(R"({"schema":1,"active":false,"profile":[]})"));
	REQUIRE_FALSE(World0Opening::Load(R"({"schema":1,"schema":1,"active":false})"));
}

TEST_CASE("World 0 exact authoring permit never changes ordinary role or foreign plots", "[world0-opening]")
{
	ValeFixture f;
	CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
	REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase4_Expansion));
	CHECK_FALSE(World0Opening::BeginAuthoring(f.profile));
	REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase3_Frontier));
	CHECK_FALSE(World0Opening::BeginAuthoring({CompanyID{0}, {f.profile.towns[0], f.profile.towns[0], f.profile.towns[2]}, f.profile.crusher_plot, f.profile.quarry_site}));
	CHECK_FALSE(World0Opening::BeginAuthoring({CompanyID{1}, f.profile.towns, f.profile.crusher_plot, 999}));
	REQUIRE(World0Opening::BeginAuthoring(f.profile));
	CHECK(World0Opening::Ready()); CHECK_FALSE(World0Opening::Active()); CHECK(World0Opening::Save().empty());
	CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
	IntegratedEconomy::SetEnabled(true); IntegratedEconomy::RegisterRole(WorldID{0}, EconomicRole::Frontier);
	CHECK(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
	CHECK_FALSE(World0Opening::PermitsCrusher(TileXY(31, 30), IndustryType{2}, CompanyID{0}));
	CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{1}));
	CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{0}, CompanyID{0}));
	const std::string before = World0Opening::Save();
	const Money money = Company::Get(CompanyID{0})->money;
	const uint64_t map = GetConnectedEconomyMapFingerprint();
	CHECK(IntegratedEconomy::CheckIndustry(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}).Succeeded());
	CHECK_FALSE(IntegratedEconomy::CheckIndustry(TileXY(31, 30), IndustryType{2}, CompanyID{0}).Succeeded());
	CHECK(World0Opening::Save() == before);
	CHECK(Company::Get(CompanyID{0})->money == money);
	CHECK(GetConnectedEconomyMapFingerprint() == map);
}

TEST_CASE("World 0 and First Permanent Line cannot both own one saved game", "[world0-opening]")
{
	ValeFixture f;
	const Json other_mission = {{"schema", 1}, {"active", true}, {"economy_evaluations", 0},
		{"invalid", false}, {"awarded", false}, {"reason", ""}, {"binding", {0, 0, 0, 1, 2, 3}},
		{"delivered", {false, false}}, {"produced", {false, false}}, {"stored", {0, 0}},
		{"commissioned", false}, {"house_count", 0}, {"houses", Json::array()}, {"journeys", Json::array()}};
	REQUIRE(FirstPermanentLine::Load(other_mission.dump()));
	CHECK_FALSE(World0Opening::BeginAuthoring(f.profile));
	FirstPermanentLine::Reset();
	f.Bind();
	Json invalid = other_mission;
	invalid["invalid"] = true; invalid["reason"] = "objective-unavailable";
	Json awarded = other_mission;
	awarded["awarded"] = true;
	awarded["delivered"] = {true, true}; awarded["produced"] = {true, true};
	awarded["stored"] = {160, 80}; awarded["commissioned"] = true;
	awarded["house_count"] = 4;
	awarded["houses"] = {{100, true}, {101, true}, {102, true}, {103, true}};
	for (const Json &other : {invalid, awarded}) {
		REQUIRE(FirstPermanentLine::Load(other.dump()));
		CHECK(FirstPermanentLine::ValidateAfterLoad());
		CHECK_FALSE(World0Opening::Ready());
		CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
		CHECK_FALSE(World0Opening::ValidateAfterLoad());
		FirstPermanentLine::Reset();
	}
	CHECK(World0Opening::ValidateAfterLoad());
}

TEST_CASE("World 0 qualifying native input and subsequent batches credit only bound SILC", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	CHECK(World0Opening::Active());
	f.quarry->founder = CompanyID{1};
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	f.quarry->founder = CompanyID{0};
	CHECK(World0Opening::ValidateAfterLoad());
	CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 1) == 1);
	World0Opening::OnIndustryDelivery(VehicleID::Invalid(), CompanyID{1}, f.quarry->index, f.crusher->index, C("SILC"), 1);
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 1) == 1);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{0}, f.quarry->index, f.crusher->index, C("SILC"), 1);
	IntegratedEconomy::Produce(); // One native batch consumes one unrelated and one qualified unit.
	CHECK(World0Opening::State().qualified_batches == 0);
	CHECK(World0Opening::State().qualified_input_remaining == 0);
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 80) == 80);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{0}, f.quarry->index, f.crusher->index, C("SILC"), 80);
	IntegratedEconomy::Produce();
	CHECK(World0Opening::State().accepted_silc == 81);
	CHECK(World0Opening::State().qualified_batches == 40);
	CHECK(World0Opening::State().input_goal);
	CHECK(World0Opening::State().production_goal);
	CHECK(World0Opening::State().economy_evaluations == 2);
	const std::string saved = World0Opening::Save();
	CHECK(World0Opening::Load(saved));
	CHECK(World0Opening::Save() == saved);
	CHECK(World0Opening::ValidateAfterLoad());
}

TEST_CASE("World 0 loaded counters cannot exceed native crusher custody or actual batches", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	const std::string baseline = World0Opening::Save();
	Json false_input = Json::parse(baseline);
	false_input["accepted_silc"] = 80;
	false_input["qualified_input_remaining"] = 80;
	false_input["input_goal"] = true;
	REQUIRE(World0Opening::Load(false_input.dump()));
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	REQUIRE(World0Opening::Load(baseline));
	Json false_batches = Json::parse(baseline);
	false_batches["accepted_silc"] = 80;
	false_batches["qualified_batches"] = 40;
	false_batches["input_goal"] = true;
	false_batches["production_goal"] = true;
	REQUIRE(World0Opening::Load(false_batches.dump()));
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	REQUIRE(World0Opening::Load(baseline));
	CHECK(World0Opening::ValidateAfterLoad());
}

TEST_CASE("World 0 load refuses understated unrelated SILC before capacity-limited production", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 100) == 100);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{0}, f.quarry->index, f.crusher->index, C("SILC"), 100);
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 500) == 500);
	World0Opening::OnIndustryDelivery(VehicleID::Invalid(), CompanyID{1}, f.quarry->index, f.crusher->index, C("SILC"), 500);
	CHECK(IntegratedEconomy::OutputBatchLimit(f.crusher) == 100); // Only 200 of the 600 waiting units can be consumed now.
	const std::string honest = World0Opening::Save();
	REQUIRE(World0Opening::ValidateAfterLoad());
	Json understated = Json::parse(honest);
	understated["unqualified_input_remaining"] = 0;
	REQUIRE(World0Opening::Load(understated.dump()));
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	REQUIRE(World0Opening::Load(honest));
	CHECK(World0Opening::ValidateAfterLoad());
}

TEST_CASE("World 0 fragmented yard unloads need a later source call and live service", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 80) == 80);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{0}, f.quarry->index, f.crusher->index, C("SILC"), 80);
	IntegratedEconomy::Produce();
	f.Deposit(20);
	World0Opening::OnStoredDelivery(f.train->index, CompanyID{0}, f.crusher->index, f.yard->index, C("BALL"), 5);
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 1);
	f.Arrive(f.source, VehicleOrderID{0});
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 2);
	f.Deposit(20);
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 3);
	CHECK(World0Opening::State().awarded);
	const std::string saved = World0Opening::Save();
	World0Opening::OnVehicleRemoved(f.train->index);
	CHECK(World0Opening::State().awarded);
	CHECK(World0Opening::Load(saved));
	CHECK(World0Opening::ValidateAfterLoad());
}

TEST_CASE("World 0 recoverable orders and stops suspend service without erasing its real visits", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	f.Deposit(5);
	f.Arrive(f.source, VehicleOrderID{0});
	REQUIRE(World0Opening::State().journeys.at(f.train->index).phase == 2);
	f.train->GetOrder(1)->SetUnloadType(OrderUnloadType::NoUnload);
	World0Opening::Evaluate();
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 2);
	f.Deposit(5);
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 2);
	const std::string suspended = World0Opening::Save();
	CHECK(World0Opening::Load(suspended));
	CHECK(World0Opening::ValidateAfterLoad());
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 2);
	f.train->GetOrder(1)->SetUnloadType(OrderUnloadType::UnloadIfPossible);
	f.train->vehstatus.Set(VehState::Stopped);
	World0Opening::Evaluate();
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 2);
	f.Arrive(f.source, VehicleOrderID{0});
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 2);
	f.train->vehstatus.Reset(VehState::Stopped);
	f.Deposit(5);
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 3);
}

TEST_CASE("World 0 load rejects foreign, wagon and missing journey identities", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	f.Deposit(5);
	const std::string saved = World0Opening::Save();
	f.train->owner = CompanyID{1};
	REQUIRE(World0Opening::Load(saved));
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	World0Opening::Evaluate();
	CHECK(World0Opening::State().journeys.empty());
	f.train->owner = CompanyID{0};
	Json missing = Json::parse(saved);
	missing["journeys"][0][0] = VehicleID::End().base() - 1;
	REQUIRE(World0Opening::Load(missing.dump()));
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	REQUIRE(Train::CanAllocateItem());
	Train *wagon = Train::Create(); wagon->owner = CompanyID{0};
	Json nonfront = Json::parse(saved);
	nonfront["journeys"][0][0] = wagon->index.base();
	REQUIRE(World0Opening::Load(nonfront.dump()));
	CHECK_FALSE(World0Opening::ValidateAfterLoad());
	REQUIRE(World0Opening::Load(saved));
	CHECK(World0Opening::ValidateAfterLoad());
}

TEST_CASE("World 0 rejects foreign or wrong-source credits and retains bounded journeys", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 80) == 80);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{1}, f.quarry->index, f.crusher->index, C("SILC"), 80);
	REQUIRE(IntegratedEconomy::AcceptIndustry(f.crusher, C("SILC"), 80) == 80);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{0}, f.crusher->index, f.crusher->index, C("SILC"), 80);
	World0Opening::OnIndustryDelivery(f.train->index, CompanyID{0}, f.quarry->index, f.crusher->index, C("BALL"), 80);
	CHECK(World0Opening::State().accepted_silc == 0);
	World0Opening::OnStoredDelivery(f.train->index, CompanyID{1}, f.crusher->index, f.yard->index, C("BALL"), 40);
	World0Opening::OnStoredDelivery(f.train->index, CompanyID{0}, f.quarry->index, f.yard->index, C("BALL"), 40);
	CHECK(World0Opening::State().stored_ball == 0);
	f.Deposit(5);
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 1);
	World0Opening::OnStoredDelivery(f.train->index, CompanyID{0}, f.crusher->index, f.yard->index, C("BALL"), 5);
	CHECK(World0Opening::State().journeys.at(f.train->index).phase == 1);
	World0Opening::OnVehicleRemoved(f.train->index);
	CHECK(World0Opening::State().journeys.empty());
	const std::string saved = World0Opening::Save();
	Json repeated = Json::parse(saved);
	repeated["journeys"] = Json::array({Json::array({f.train->index.base(), 1, f.yard->index.base(), 1, 1}),
		Json::array({f.train->index.base(), 1, f.yard->index.base(), 1, 1})});
	CHECK_FALSE(World0Opening::Load(repeated.dump()));
	CHECK(World0Opening::Save() == saved);
	repeated = Json::parse(saved); repeated["stored_ball"] = true;
	CHECK_FALSE(World0Opening::Load(repeated.dump()));
	CHECK(World0Opening::Save() == saved);
}

TEST_CASE("World 0 explicit deletion tombstone and missing-reference load differ", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	const std::string live = World0Opening::Save();
	delete f.crusher; f.crusher = nullptr; // Native industry destructor writes the tombstone and releases its factory.
	CHECK(World0Opening::State().unavailable);
	CHECK(World0Opening::State().binding.crusher == IndustryID::Invalid());
	CHECK(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
	const std::string removed = World0Opening::Save();
	CHECK(World0Opening::Load(removed));
	CHECK(World0Opening::ValidateAfterLoad());
	CHECK_FALSE(World0Opening::Load(R"({"schema":2,"active":true})"));
	CHECK(World0Opening::Save() == removed);
	REQUIRE(World0Opening::Load(live));
	CHECK_FALSE(World0Opening::ValidateAfterLoad()); // A dangling live ID without its native tombstone is corrupt.
	World0Opening::OnCompanyChanged(CompanyID{0}, CompanyID{1});
	CHECK(World0Opening::State().unavailable);
	CHECK(World0Opening::State().profile.company == CompanyID::Invalid());
	CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
}

TEST_CASE("World 0 yard tombstone identifies the removed hub, not later free-play replacements", "[world0-opening]")
{
	ValeFixture f; f.Bind();
	const LogisticsHub *hub = LogisticsHubManager::GetHubForStation(f.yard->index);
	REQUIRE(hub != nullptr);
	const uint32_t removed_id = hub->hub_id;
	const StationID station_id = hub->station_id;
	const TileIndex tile = hub->tile;
	REQUIRE(LogisticsHubManager::RemoveHub(removed_id));
	CHECK(World0Opening::State().unavailable);
	CHECK(World0Opening::State().binding.yard == StationID::Invalid());
	CHECK(World0Opening::State().removed_yard_station == station_id);
	CHECK(World0Opening::State().removed_yard_tile == tile);
	CHECK(World0Opening::State().removed_hub_id == removed_id);
	const std::string removed = World0Opening::Save();
	REQUIRE(World0Opening::Load(removed));
	CHECK(World0Opening::ValidateAfterLoad());
	const uint32_t replacement = LogisticsHubManager::RegisterHub(tile, WorldID{0}, CompanyID{0}, station_id, "New yard");
	REQUIRE(replacement != 0);
	CHECK(replacement != removed_id);
	CHECK(World0Opening::ValidateAfterLoad());
	LogisticsHubManager::Reset(); // A cold LHUB load restores live hubs but not the next-ID cursor.
	const uint32_t reused = LogisticsHubManager::RegisterHub(tile, WorldID{0}, CompanyID{0}, station_id, "Reissued yard");
	REQUIRE(reused == removed_id);
	REQUIRE(World0Opening::Load(removed));
	CHECK(World0Opening::ValidateAfterLoad());
	CHECK(World0Opening::State().binding.yard == StationID::Invalid());
	Json forged = Json::parse(removed);
	forged["yard_tombstone"][0] = StationID::Invalid().base();
	CHECK_FALSE(World0Opening::Load(forged.dump()));
	CHECK(World0Opening::Save() == removed);
}

TEST_CASE("World 0 native industry removals remain unavailable through ordinary free-play replacements", "[world0-opening]")
{
	SECTION("Crusher at the permitted plot") {
		ValeFixture f; f.Bind();
		delete f.crusher; f.crusher = nullptr;
		const std::string removed = World0Opening::Save();
		REQUIRE(World0Opening::State().unavailable);
		REQUIRE(Industry::CanAllocateItem());
		f.crusher = Industry::Create(f.profile.crusher_plot);
		f.crusher->type = IndustryType{2}; f.crusher->founder = CompanyID{0};
		f.crusher->location = TileArea(f.profile.crusher_plot, 1, 1);
		f.crusher->accepted.push_back({.cargo = C("SILC")}); f.crusher->produced.push_back({.cargo = C("BALL")});
		IntegratedEconomy::RegisterIndustry(f.crusher);
		REQUIRE(World0Opening::Load(removed));
		CHECK(World0Opening::ValidateAfterLoad());
		CHECK(World0Opening::State().unavailable);
		CHECK(World0Opening::State().binding.crusher == IndustryID::Invalid());
		CHECK_FALSE(World0Opening::PermitsCrusher(f.profile.crusher_plot, IndustryType{2}, CompanyID{0}));
	}
	SECTION("Silica quarry at its surveyed site") {
		ValeFixture f; f.Bind();
		delete f.quarry; f.quarry = nullptr;
		const std::string removed = World0Opening::Save();
		REQUIRE(World0Opening::State().unavailable);
		REQUIRE(Industry::CanAllocateItem());
		f.quarry = Industry::Create(TileXY(40, 25));
		f.quarry->type = IndustryType{0}; f.quarry->founder = CompanyID{0};
		f.quarry->location = TileArea(TileXY(40, 25), 1, 1);
		f.quarry->produced.push_back({.cargo = C("SILC")});
		ResourceSiteManager::Occupy(f.quarry->location.tile, f.quarry->type,
			GetIndustrySpec(f.quarry->type)->layouts[0], f.quarry->index);
		REQUIRE(World0Opening::Load(removed));
		CHECK(World0Opening::ValidateAfterLoad());
		CHECK(World0Opening::State().binding.quarry == IndustryID::Invalid());
	}
}
