/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_stellar_network.cpp Native gate commissioning, conservation, routing and persistence regressions. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/stellar_network.h"
#include "../portal/world_gen.h"
#include "../portal/universe_directory_gui.h"
#include "../widgets/universe_directory_widget.h"
#include "../portal/universe_network.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/portal_registry.h"
#include "../portal/tech_tree.h"
#include "../portal/company_stockpile.h"
#include "../portal/logistics_hub.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../station_base.h"
#include "../train.h"
#include "../signal_func.h"
#include "../order_base.h"
#include "../station_map.h"
#include "../cargotype.h"
#include "../command_func.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../widgets/stellar_widget.h"
#include "../portal/transfer_journal.h"
#include "../order_cmd.h"
#include "../table/strings.h"
#include "../safeguards.h"
extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
extern void SaveReloadCommandAuthority();
namespace {
struct StellarFixture {
	TileIndex source = TileXY(10, 20), target = TileXY(40, 20);
	StationID station;
	CargoType machines = CargoType{10};
	StellarFixture()
	{
		StellarNetwork::Reset();
		UniverseNetwork::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
		source = TileXY(10, 20);
		target = TileXY(40, 20);
		PlanetManager::Reset();
		REQUIRE(PlanetManager::RegisterRegion(
			{.id = WorldID{0}, .name = "Mito", .phase = WorldPhase::Phase1_Core, .min_x = 1, .min_y = 1, .max_x = 31, .max_y = 62}));
		REQUIRE(PlanetManager::RegisterRegion(
			{.id = WorldID{1}, .name = "Chelva", .phase = WorldPhase::Phase4_Expansion, .min_x = 32, .min_y = 1, .max_x = 62, .max_y = 62}));
		StellarNetwork::RegisterWorld({WorldID{0}, "mito", "Mito", 0, 0, true});
		StellarNetwork::RegisterWorld({WorldID{1}, "chelva", "Chelva", 10, 0, false});
		StellarNetwork::RegisterZone({1, WorldID{1}, target, DiagDirection::NE});
		TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_PORTAL_1});
		Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_RAIL);
		CargoSpec::Get(machines)->label = CargoLabel{"MACH"};
		BuildCargoLabelMap();
		_price[Price::BuildTunnel] = 100;
		INFO("Gate construction error: " << Command<Commands::BuildPortalGate>::Do({}, source, DiagDirection::NE, RAILTYPE_RAIL).GetErrorMessage().base());
		REQUIRE(Command<Commands::BuildPortalGate>::Post(source, DiagDirection::NE, RAILTYPE_RAIL));
		REQUIRE(Station::CanAllocateItem());
		Station *st = Station::Create(TileXY(25, 32));
		station = st->index;
		st->owner = CompanyID{0};
		st->name = "Gate supply";
		st->town = PlanetManager::GetWorldPrimaryTown(WorldID{0});
		st->facilities.Set(StationFacility::Train);
		st->train_station = TileArea(st->xy, 1, 1);
		st->spread = st->train_station;
		MakeRailStation(st->xy, st->owner, station, Axis::X, 0, RAILTYPE_RAIL);
		REQUIRE(LogisticsHubManager::RegisterHub(st->xy, WorldID{0}, st->owner, station, "Supply") != 0);
	}
	~StellarFixture()
	{
		UpdateSignalsInBuffer();
		StellarNetwork::Reset();
		UniverseNetwork::Reset();
		SetupCargoForClimate(LandscapeType::Temperate);
		UnInitWindowSystem();
	}
};
} // namespace
TEST_CASE("Stellar projects reserve, consume and refund delivered equipment once", "[stellar]")
{
	StellarFixture f;
	Money before = Company::Get(CompanyID{0})->money;
	INFO("Project error: " << Command<Commands::StartGateProject>::Do({}, f.source, 1, f.station).GetErrorMessage().base());
	REQUIRE(Command<Commands::StartGateProject>::Do({}, f.source, 1, f.station).Succeeded());
	CHECK(StellarNetwork::Projects().empty());
	REQUIRE(Command<Commands::StartGateProject>::Post(f.source, 1, f.station));
	CHECK(Company::Get(CompanyID{0})->money == before - 1000);
	CHECK_FALSE(Command<Commands::StartGateProject>::Post(f.source, 1, f.station));
	CHECK(StellarNetwork::ReceiveDelivery(f.station, CompanyID{1}, GetCargoTypeByLabel(CT_STEEL), 200) == 0);
	CHECK(StellarNetwork::ReceiveDelivery(f.station, CompanyID{0}, GetCargoTypeByLabel(CT_STEEL), 300) == 200);
	CHECK(StellarNetwork::ReceiveDelivery(f.station, CompanyID{0}, f.machines, 10) == 10);
	CHECK_FALSE(Command<Commands::OperateGateProject>::Post(1, false));
	REQUIRE(Command<Commands::OperateGateProject>::Post(1, true));
	CHECK(StellarNetwork::Projects().at(1).steel == 0);
	CHECK(StellarNetwork::Projects().at(1).machines == 0);
	CHECK_FALSE(Command<Commands::OperateGateProject>::Post(1, true));
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, GetCargoTypeByLabel(CT_STEEL)) == 200);
	CHECK(StockpileManager::GetStock(WorldID{0}, CompanyID{0}, f.machines) == 10);
	CHECK_FALSE(StellarNetwork::WorldAccessible(WorldID{1}));
	CHECK(Company::Get(CompanyID{0})->money == before - 1000);
}
TEST_CASE("Stellar activation links real native gates and persists access through save reload", "[stellar]")
{
	StellarFixture f;
	REQUIRE(Command<Commands::StartGateProject>::Post(f.source, 1, f.station));
	StellarNetwork::ReceiveDelivery(f.station, CompanyID{0}, GetCargoTypeByLabel(CT_STEEL), 200);
	StellarNetwork::ReceiveDelivery(f.station, CompanyID{0}, f.machines, 40);
	REQUIRE(StellarNetwork::Projects().at(1).state == GateProjectState::Ready);
	REQUIRE(Command<Commands::OperateGateProject>::Post(1, false));
	CHECK(StellarNetwork::WorldAccessible(WorldID{1}));
	CHECK(PortalRegistry::GetOtherPortalEnd(f.source) == f.target);
	CHECK_FALSE(StellarNetwork::CanUseGate(f.source, CompanyID{1}));
	REQUIRE(Command<Commands::SetStellarGateAccess>::Post(f.source, true, Money{123}));
	CHECK(StellarNetwork::CanUseGate(f.source, CompanyID{1}));
	auto payer = Company::Get(CompanyID{1})->money;
	auto payee = Company::Get(CompanyID{0})->money;
	CHECK(StellarNetwork::AdmitTrain(f.source, VehicleID{123}, CompanyID{1}));
	CHECK(StellarNetwork::AdmitTrain(f.source, VehicleID{123}, CompanyID{1}));
	CHECK(Company::Get(CompanyID{1})->money == payer - 123);
	CHECK(Company::Get(CompanyID{0})->money == payee + 123);
	StellarNetwork::ReleaseTrain(VehicleID{123});
	SaveReloadCommandAuthority();
	CHECK(StellarNetwork::WorldAccessible(WorldID{1}));
	REQUIRE(StellarNetwork::Policy(f.source) != nullptr);
	CHECK(StellarNetwork::Policy(f.source)->toll == 123);
	CHECK(StellarNetwork::Projects().at(1).state == GateProjectState::Active);
	CHECK_FALSE(Command<Commands::OperateGateProject>::Post(1, false));
}
TEST_CASE("Stellar research range and closed construction are authoritative", "[stellar]")
{
	StellarFixture f;
	CHECK(StellarNetwork::CanReach(CompanyID{0}, WorldID{0}, WorldID{1}));
	CHECK_FALSE(StellarNetwork::CanReach(CompanyID{1}, WorldID{0}, WorldID{1}));
	StellarNetwork::RegisterWorld({WorldID{1}, "chelva", "Chelva", 10, 1, false});
	CHECK_FALSE(StellarNetwork::CanReach(CompanyID{0}, WorldID{0}, WorldID{1}));
	CHECK_FALSE(Command<Commands::StartGateProject>::Post(f.source, 1, f.station));
	TechTreeManager::RestoreCompanyTech(CompanyID{0}, TECH_NONE, 0, 0, {TECH_PORTAL_2});
	CHECK(StellarNetwork::CanReach(CompanyID{0}, WorldID{0}, WorldID{1}));
	CHECK(PlanetManager::CheckConstructionPlacement(f.target).GetErrorMessage() == STR_ERROR_STELLAR_CLOSED);
	CHECK_FALSE(Command<Commands::BuildPortalGate>::Post(f.target, DiagDirection::NE, RAILTYPE_RAIL));
}
TEST_CASE("Universe routes use compatible online public or owned gates deterministically", "[stellar][universe]")
{
	UniverseNetwork::Reset();
	const auto manifest = UniverseNetwork::Manifest();
	auto world = [&](uint32_t id) { return nlohmann::json{{"kind", "world"}, {"namespace", "1:1"}, {"manifest", manifest}, {"online", true}, {"id", id}}; };
	for (uint32_t i = 1; i <= 3; i++)
		REQUIRE(UniverseNetwork::Apply(fmt::format("w{}", i), world(i)));
	auto gate = [&](uint32_t a, uint32_t b, bool pub) {
		return nlohmann::json{{"kind", "gate"},	  {"namespace", "1:1"}, {"manifest", manifest}, {"online", true},	{"world", a},
							  {"destination", b}, {"rail", 0},			{"public", pub},		{"owner", "owner"}, {"toll", 7}};
	};
	REQUIRE(UniverseNetwork::Apply("a", gate(1, 2, true)));
	REQUIRE(UniverseNetwork::Apply("b", gate(2, 3, true)));
	REQUIRE(UniverseNetwork::Apply("private", gate(1, 3, false)));
	CHECK(UniverseNetwork::FindRoute(WorldID{1}, WorldID{3}, "visitor", RailTypes{RAILTYPE_RAIL}) == std::vector<std::string>{"a", "b"});
	CHECK(UniverseNetwork::FindRoute(WorldID{1}, WorldID{3}, "owner", RailTypes{RAILTYPE_RAIL}) == std::vector<std::string>{"private"});
	auto offline = world(2);
	offline["online"] = false;
	REQUIRE(UniverseNetwork::Apply("w2", offline));
	CHECK(UniverseNetwork::FindRoute(WorldID{1}, WorldID{3}, "visitor", RailTypes{RAILTYPE_RAIL}).empty());
	auto saved = UniverseNetwork::Save();
	UniverseNetwork::Reset();
	REQUIRE(UniverseNetwork::Load(saved));
	CHECK(UniverseNetwork::Save() == saved);
	UniverseNetwork::Reset();
}
TEST_CASE("Remote gate commitment resumes idempotently and consumes equipment once", "[stellar][universe]")
{
	StellarFixture f;
	const FederationNamespace source_ns{1, 1}, target_ns{2, 2};
	FederationIdentityRegistry::Reset();
	FederationIdentityRegistry::RestoreState(source_ns, 1);
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{0}, 1, source_ns));
	const auto manifest = UniverseNetwork::Manifest();
	nlohmann::json zone{{"kind", "zone"}, {"namespace", "2:2"}, {"manifest", manifest}, {"online", true}, {"id", 1}, {"world", 1}, {"x", 10}, {"y", 0}};
	REQUIRE(UniverseNetwork::Apply("zone", zone));
	REQUIRE(UniverseNetwork::Apply("2:2", {{"kind", "host"},
										   {"namespace", "2:2"},
										   {"manifest", manifest},
										   {"online", true},
										   {"companies", nlohmann::json::array({{{"local", 0}, {"identity", "1:1:1"}}})}}));
	REQUIRE(Command<Commands::RemoteGateProject>::Post(f.source, "zone", 0));
	REQUIRE(RemoteGateProjects::ReceiveDelivery(f.station, CompanyID{0}, GetCargoTypeByLabel(CT_STEEL), 200) == 200);
	REQUIRE(RemoteGateProjects::ReceiveDelivery(f.station, CompanyID{0}, f.machines, 40) == 40);
	auto source = RemoteGateProjects::Advertisements(false).at(0);
	auto id = source.at("id").get<std::string>();
	source["kind"] = "gateproject";
	source["online"] = true;
	source["manifest"] = manifest;
	FederationIdentityRegistry::RestoreState(target_ns, 1);
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{0}, 1, source_ns));
	RemoteGateProjects::Reconcile(source);
	REQUIRE(RemoteGateProjects::Advertisements(true).size() == 1);
	auto reply = RemoteGateProjects::Advertisements(true).at(0);
	CHECK(reply.at("state") == "reserved");
	reply["kind"] = "gatereply";
	reply["namespace"] = "2:2";
	reply["online"] = true;
	reply["manifest"] = manifest;
	FederationIdentityRegistry::RestoreState(source_ns, 1);
	REQUIRE(UniverseNetwork::Apply("reply", reply));
	REQUIRE(Command<Commands::RemoteGateProject>::Post(f.source, id, 1));
	CHECK_FALSE(Command<Commands::RemoteGateProject>::Post(f.source, id, 2));
	source = RemoteGateProjects::Advertisements(false).at(0);
	source["kind"] = "gateproject";
	source["online"] = true;
	source["manifest"] = manifest;
	auto persistent = RemoteGateProjects::Save();
	RemoteGateProjects::Reset();
	REQUIRE(RemoteGateProjects::Load(persistent));
	FederationIdentityRegistry::RestoreState(target_ns, 1);
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{0}, 1, source_ns));
	RemoteGateProjects::Reconcile(source);
	RemoteGateProjects::Reconcile(source);
	REQUIRE(PortalRegistry::IsInterServerPortal(f.target));
	CHECK(RemoteGateProjects::Advertisements(true).at(0).at("state") == "built");
	reply = RemoteGateProjects::Advertisements(true).at(0);
	reply["kind"] = "gatereply";
	reply["namespace"] = "2:2";
	reply["online"] = true;
	reply["manifest"] = manifest;
	FederationIdentityRegistry::RestoreState(source_ns, 1);
	RemoteGateProjects::Reconcile(reply);
	RemoteGateProjects::Reconcile(reply);
	REQUIRE(PortalRegistry::IsInterServerPortal(f.source));
	CHECK(RemoteGateProjects::Advertisements(false).at(0).at("state") == "active");
	CHECK(RemoteGateProjects::Advertisements(false).at(0).at("steel") == 0);
	CHECK(RemoteGateProjects::Advertisements(false).at(0).at("machines") == 0);
	CHECK_FALSE(Command<Commands::RemoteGateProject>::Post(f.source, id, 1));
	CHECK(Command<Commands::SetStellarGateAccess>::Do({}, f.target, true, Money{123}).Failed());
	REQUIRE(Command<Commands::SetStellarGateAccess>::Post(f.source, true, Money{123}));
	SaveReloadCommandAuthority();
	CHECK(RemoteGateProjects::Advertisements(false).at(0).at("state") == "active");
	CHECK(RemoteGateProjects::Advertisements(false).at(0).at("toll") == 123);
	CHECK(RemoteGateProjects::ValidateAfterLoad());
}
TEST_CASE("Remote station and explicit gate orders retain stable destinations through native reload", "[stellar][universe]")
{
	StellarFixture f;
	FederationIdentityRegistry::Reset();
	FederationIdentityRegistry::RestoreState({1, 1}, 1);
	GlobalStationID remote{{2, 2}, 17, WorldID{9}};
	REQUIRE(UniverseNetwork::Apply("remote", {{"kind", "station"},
											  {"namespace", "2:2"},
											  {"manifest", UniverseNetwork::Manifest()},
											  {"online", true},
											  {"sequence", 17},
											  {"world", 9},
											  {"name", "Far station"},
											  {"owner", "1:1:1"}}));
	auto station = UniverseNetwork::EnsureStation(remote);
	REQUIRE(station);
	auto gate = GlobalOrderDestinationID::ForPortalGate({2, 2}, 123, WorldID{8});
	auto pin = UniverseNetwork::EnsureGate(gate);
	REQUIRE(pin);
	Order order;
	order.MakeGoToStation(*pin);
	CHECK(FederationIdentityRegistry::GetOrCreateOrderDestination(order.GetDestination(), order.GetType()) == gate);
	CHECK(UniverseNetwork::EnsureStation(remote) == station);
	CHECK(UniverseNetwork::EnsureGate(gate) == pin);
	SaveReloadCommandAuthority();
	CHECK(FederationIdentityRegistry::FindStation(*station) == remote);
	CHECK(UniverseNetwork::GateOrder(*pin) == gate);
	CHECK(UniverseNetwork::IsRemoteStation(*station));
	CHECK(UniverseNetwork::IsRemoteStation(*pin));
}
TEST_CASE("Star map buttons commission a gate through native commands", "[stellar][gui]")
{
	StellarFixture f;
	ShowUniverseDirectory();
	Window *directory = FindWindowById(WindowClass::UniverseDirectory, 0);
	REQUIRE(directory);
	CHECK_FALSE(directory->IsWidgetDisabled(WID_UD_TRADE_GATE_BTN));
	directory->OnClick({}, WID_UD_TRADE_GATE_BTN, 1);
	Window *window = FindWindowById(WindowClass::StellarNetwork, 0);
	REQUIRE(window);
	auto map = window->GetWidget<NWidgetBase>(SW_MAP)->GetCurrentRect();
	window->OnClick({map.right - 1, map.bottom - 1}, SW_MAP, 1);
	window->OnClick({}, SW_START, 1);
	REQUIRE(StellarNetwork::Projects().size() == 1);
	StellarNetwork::ReceiveDelivery(f.station, CompanyID{0}, GetCargoTypeByLabel(CT_STEEL), 200);
	StellarNetwork::ReceiveDelivery(f.station, CompanyID{0}, f.machines, 40);
	auto list = window->GetWidget<NWidgetBase>(SW_PROJECTS)->GetCurrentRect();
	window->OnClick({list.left + 4, list.top + 4}, SW_PROJECTS, 1);
	window->OnClick({}, SW_ACTIVATE, 1);
	CHECK(PortalRegistry::GetOtherPortalEnd(f.source) == f.target);
	CHECK(StellarNetwork::WorldAccessible(WorldID{1}));
}
TEST_CASE("Schedule revisions reject stale edits and prepared departures freeze native order commands", "[stellar][universe]")
{
	StellarFixture f;
	TransferJournal::Reset();
	FederationIdentityRegistry::Reset();
	FederationIdentityRegistry::RestoreState({1, 1}, 1);
	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	train->owner = CompanyID{0};
	train->tile = TileXY(25, 32);
	train->track = Track::X;
	train->engine_type = EngineID{0};
	train->cargo_type = CargoType{0};
	train->direction = Direction::NE;
	train->SetFrontEngine();
	train->SetEngine();
	auto id = FederationIdentityRegistry::GetOrCreate(train);
	REQUIRE(id);
	Order order;
	order.MakeGoToStation(f.station);
	REQUIRE(OrderList::CanAllocateItem());
	train->orders = OrderList::Create(std::move(order), train);
	auto revision = UniverseNetwork::OrderRevision(train);
	train->GetOrder(0)->SetLoadType(OrderLoadType::FullLoad);
	CHECK(UniverseNetwork::OrderRevision(train) != revision);
	CHECK(UniverseNetwork::ScheduleEditable(train));
	TransferCheckpoint record;
	record.request_id = "DEP-test";
	record.namespace_high = id->name_space.high;
	record.namespace_low = id->name_space.low;
	record.consist_sequence = id->sequence;
	record.snapshot = {1};
	REQUIRE(TransferJournal::Prepare(record));
	CHECK_FALSE(UniverseNetwork::ScheduleEditable(train));
	CHECK(Command<Commands::DeleteOrder>::Do({}, train->index, 0).Failed());
	REQUIRE(TransferJournal::BindTransfer(0, "DEP-test", "test-transfer"));
	REQUIRE(TransferJournal::MarkDeparted(0, "DEP-test"));
	CHECK(UniverseNetwork::ScheduleEditable(train)); // A historical departure must not freeze a returned train.
	TransferJournal::Reset();
}

TEST_CASE("Federated generation assigns disjoint immutable world IDs", "[stellar]")
{
	MultiWorldGen::Config config;
	config.world_id_base = 100;
	config.world_count = 7;
	config.cst_sector = true;
	auto regions = MultiWorldGen::CalculateLayout(1024, 1024, config);
	REQUIRE(regions.size() == 7);
	CHECK(regions.front().id == WorldID{100});
	CHECK(regions.back().id == WorldID{106});
	CHECK(regions.front().name == "Mito");
	config.world_id_base = UINT32_MAX - 3;
	CHECK(MultiWorldGen::CalculateLayout(1024, 1024, config).empty());
}

TEST_CASE("Universe company provisioning is explicit, unique and persistent", "[stellar][universe]")
{
	StellarFixture f;
	TransferJournal::Reset();
	REQUIRE(Command<Commands::MapUniverseCompany>::Do(DoCommandFlag::Execute, CompanyID{0}, "abc:def", uint64_t{7}).Succeeded());
	CHECK(FederationIdentityRegistry::FindCompany(CompanyID{0}) == GlobalCompanyID{{0xabc, 0xdef}, 7});
	CHECK(Command<Commands::MapUniverseCompany>::Do({}, CompanyID{1}, "abc:def", uint64_t{7}).Failed());
	REQUIRE(Command<Commands::StartGateProject>::Post(f.source, 1, f.station));
	CHECK(Command<Commands::MapUniverseCompany>::Do({}, CompanyID{0}, "abc:def", uint64_t{8}).Failed());
	SaveReloadCommandAuthority();
	CHECK(FederationIdentityRegistry::FindCompany(CompanyID{0}) == GlobalCompanyID{{0xabc, 0xdef}, 7});
}
