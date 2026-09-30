/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_megacity_gui.cpp Unit and regression tests for Megacity, Freight Corridor, and Universe Directory GUIs. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"

#include "../window_gui.h"
#include "../window_func.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../town.h"
#include "../town_map.h"
#include "../newgrf_house.h"
#include "../town_kdtree.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../station_map.h"
#include "../station_kdtree.h"
#include "../network/network.h"
#include "../network/network_internal.h"
#include "../network/network_client.h"
#include "../network/core/packet.h"
#include "../widgets/town_widget.h"
#include "../widgets/megacity_widget.h"
#include "../widgets/freight_corridor_widget.h"
#include "../widgets/universe_directory_widget.h"
#include "../portal/megacity_manager.h"
#include "../portal/megacity_gui.h"
#include "../portal/portal_cmd.h"
#include "../portal/integrated_economy.h"
#include "../portal/corporate_hq.h"
#include "../cargotype.h"
#include "../portal/universe_authority.h"
#include "../portal/planet_manager.h"

#include <algorithm>
#include <vector>

#include "../safeguards.h"

extern std::vector<WindowDesc*> *_window_descs;
extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
extern void SaveReloadCommandAuthority();

namespace {
struct MegacityCommandFixture {
	TownID town;
	MegacityCommandFixture()
	{
		IntegratedEconomy::Reset();
		SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 0);
		CorporateHQManager::Reset();
		town = (*Town::Iterate().begin())->index;
		Company::Get(CompanyID{0})->money = 0;
	}
	~MegacityCommandFixture()
	{
		_networking = _network_server = false;
		_current_company = _local_company = CompanyID{0};
		NetworkFreeLocalCommandQueue();
		UnInitWindowSystem();
		MegacityManager::Reset();
		IntegratedEconomy::Reset();
		SetupCargoForClimate(LandscapeType::Temperate);
	}
};

/** Capture the real client Post path before it reaches a socket. */
struct MegacityPacketClient : ClientNetworkGameSocketHandler {
	std::vector<uint8_t> command_packet;
	MegacityPacketClient() : ClientNetworkGameSocketHandler(INVALID_SOCKET, "megacity-post-test") {}
	void SendPacket(std::unique_ptr<Packet> &&packet) override
	{
		REQUIRE(command_packet.empty());
		packet->PrepareToSend();
		command_packet.resize(packet->Size());
		size_t offset = 0;
		while (offset < command_packet.size()) {
			REQUIRE(packet->TransferOut([&](std::span<const uint8_t> part) -> ssize_t {
				std::copy(part.begin(), part.end(), command_packet.begin() + offset);
				offset += part.size();
				return static_cast<ssize_t>(part.size());
			}) > 0);
		}
	}
	CommandPacket ReadCommand()
	{
		Packet packet(this, size_t{65536});
		size_t offset = 0;
		auto transfer = [&](std::span<uint8_t> part) -> ssize_t {
			REQUIRE(offset + part.size() <= command_packet.size());
			std::copy_n(command_packet.begin() + offset, part.size(), part.begin());
			offset += part.size();
			return static_cast<ssize_t>(part.size());
		};
		REQUIRE(packet.TransferIn(transfer) == 2);
		REQUIRE(packet.ParsePacketSize());
		REQUIRE(packet.Size() == command_packet.size());
		while (offset < command_packet.size()) REQUIRE(packet.TransferIn(transfer) > 0);
		REQUIRE(packet.PrepareToRead());
		REQUIRE(packet.Recv_uint8() == to_underlying(PacketGameType::ClientCommand));
		CommandPacket command;
		REQUIRE_FALSE(this->ReceiveCommand(packet, command).has_value());
		return command;
	}
};
} // namespace

TEST_CASE("Megacity designation queries reject invalid towns without mutation and preserve free eligibility", "[megacity-designation]")
{
	MegacityCommandFixture fixture;
	const auto query = Command<Commands::DesignateMegacity>::Do({}, fixture.town);
	REQUIRE(query.Succeeded());
	CHECK(query.GetCost() == 0);
	CHECK(MegacityManager::GetAllMegacities().empty());
	CHECK(Company::Get(CompanyID{0})->money == 0);
	for (TownID invalid : {TownID::Invalid(), TownID{200}}) {
		CHECK(Command<Commands::DesignateMegacity>::Do({}, invalid).Failed());
		CHECK(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, invalid).Failed());
	}
	CHECK(MegacityManager::GetAllMegacities().empty());

	/* Query arguments contain only town identity; execute uses the live town. */
	Town *town = Town::Get(fixture.town);
	town->name = "Live renamed town";
	town->cache.population = 1240;
	PlanetManager::Reset();
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{7}, .name = "Frontier designation", .phase = WorldPhase::Phase3_Frontier,
		.min_x = 1, .min_y = 1, .max_x = 62, .max_y = 62}));
	_current_company = _local_company = CompanyID{1};
	const Money other_balance = Company::Get(CompanyID{1})->money;
	REQUIRE(Command<Commands::DesignateMegacity>::Post(fixture.town));
	const MegacityProfile *profile = MegacityManager::GetProfile(fixture.town);
	REQUIRE(profile != nullptr);
	CHECK(profile->world_id == WorldID{7});
	CHECK(profile->town_name == town->name);
	CHECK(profile->population == 1240);
	CHECK(profile->monthly_quota == std::array<uint32_t, 3>{62, 31, 12});
	CHECK(Company::Get(CompanyID{0})->money == 0);
	CHECK(Company::Get(CompanyID{1})->money == other_balance);
	MegacityManager::RecordDelivery(fixture.town, MegacityDemandTier::Tier1_Sustenance, 17);
	CHECK(Command<Commands::DesignateMegacity>::Do({}, fixture.town).Failed());
	CHECK(Command<Commands::DesignateMegacity>::Do(DoCommandFlag::Execute, fixture.town).Failed());
	CHECK_FALSE(Command<Commands::DesignateMegacity>::Post(fixture.town));
	CHECK(profile->delivered_current[0] == 17);
	CHECK(MegacityManager::GetAllMegacities().size() == 1);
	SaveReloadCommandAuthority();
	profile = MegacityManager::GetProfile(fixture.town);
	REQUIRE(profile != nullptr);
	CHECK(profile->world_id == WorldID{7});
	CHECK(profile->town_name == "Live renamed town");
	CHECK(profile->population == 1240);
	CHECK(profile->delivered_current[0] == 17);
	CHECK(profile->monthly_quota == std::array<uint32_t, 3>{62, 31, 12});
	CHECK(Company::Get(CompanyID{0})->money == 0);
	CHECK(Company::Get(CompanyID{1})->money == other_balance);
}

TEST_CASE("Megacity designation preserves the existing world zero fallback", "[megacity-designation]")
{
	MegacityCommandFixture fixture;
	PlanetManager::Reset();
	REQUIRE(Command<Commands::DesignateMegacity>::Post(fixture.town));
	CHECK(MegacityManager::GetProfile(fixture.town)->world_id == WorldID{0});
}

TEST_CASE("Megacity designation preserves spectator eligibility through the native command", "[megacity-designation][command-authority]")
{
	MegacityCommandFixture fixture;
	_current_company = _local_company = COMPANY_SPECTATOR;
	REQUIRE(Command<Commands::DesignateMegacitySpectator>::Do({}, fixture.town).Succeeded());
	CHECK_FALSE(MegacityManager::IsMegacity(fixture.town));
	ShowMegacityOverview(fixture.town);
	Window *window = FindWindowById(WindowClass::MegacityOverview, fixture.town.base());
	REQUIRE(window != nullptr);
	window->OnClick({}, WID_MCO_DESIGNATE, 1);
	CHECK(MegacityManager::IsMegacity(fixture.town));
	CHECK(Company::Get(CompanyID{0})->money == 0);
	CHECK(Company::Get(CompanyID{1})->money == 10000000);
	CHECK(_current_company == COMPANY_SPECTATOR);
}

TEST_CASE("Megacity Designate GUI waits for native command execution", "[megacity-designation][command-authority]")
{
	MegacityCommandFixture fixture;
	ShowMegacityOverview(fixture.town);
	Window *window = FindWindowById(WindowClass::MegacityOverview, fixture.town.base());
	REQUIRE(window != nullptr);
	_networking = _network_server = true;
	_frame_counter = _frame_counter_max = 0;
	window->OnClick({}, WID_MCO_DESIGNATE, 1);
	CHECK(NetworkPendingCommandCount() == 1);
	CHECK_FALSE(MegacityManager::IsMegacity(fixture.town));
	NetworkDistributeCommands();
	CHECK_FALSE(MegacityManager::IsMegacity(fixture.town));
	++_frame_counter;
	NetworkExecuteLocalCommandQueue();
	CHECK(NetworkPendingCommandCount() == 0);
	REQUIRE(MegacityManager::IsMegacity(fixture.town));
	CHECK(MegacityManager::GetProfile(fixture.town)->town_name == Town::Get(fixture.town)->name);
	CHECK(Company::Get(CompanyID{0})->money == 0);
	/* An already designated town cannot reset demand. */
	MegacityManager::RecordDelivery(fixture.town, MegacityDemandTier::Tier1_Sustenance, 23);
	window->OnClick({}, WID_MCO_DESIGNATE, 1);
	_frame_counter_max = _frame_counter;
	NetworkDistributeCommands();
	++_frame_counter;
	NetworkExecuteLocalCommandQueue();
	CHECK(MegacityManager::GetProfile(fixture.town)->delivered_current[0] == 23);
}

TEST_CASE("Megacity GUI client packets preserve normal company and spectator sender identities", "[megacity-designation][command-authority]")
{
	MegacityCommandFixture fixture;
	const CompanyID client_playas = GENERATE(CompanyID{0}, COMPANY_SPECTATOR);
	_current_company = _local_company = client_playas;
	_networking = true;
	_network_server = false;
	MegacityPacketClient client;
	ShowMegacityOverview(fixture.town);
	Window *window = FindWindowById(WindowClass::MegacityOverview, fixture.town.base());
	REQUIRE(window != nullptr);
	window->OnClick({}, WID_MCO_DESIGNATE, 1);
	CHECK_FALSE(MegacityManager::IsMegacity(fixture.town));
	REQUIRE_FALSE(client.command_packet.empty());
	const CommandPacket command = client.ReadCommand();
	/* Full native server receive rejects company != client_playas; test the
	 * actual GUI packet, not a manually assigned relay packet. */
	CHECK(command.company == client_playas);
	CHECK(command.cmd == (client_playas == COMPANY_SPECTATOR ? Commands::DesignateMegacitySpectator : Commands::DesignateMegacity));
	CHECK(EndianBufferReader::ToValue<CommandTraits<Commands::DesignateMegacity>::Args>(command.data) == std::make_tuple(fixture.town));
	CHECK(_current_company == client_playas);
	CHECK(Company::Get(CompanyID{0})->money == 0);
}

TEST_CASE("Designated town consumer stations require a house belonging to their own town", "[megacity-designation][catchment]")
{
	MegacityCommandFixture fixture;
	const CargoType food{1};
	CargoSpec::Get(food)->label = CargoLabel{"FOOD"};
	BuildCargoLabelMap();
	Town *town = Town::Get(fixture.town);
	REQUIRE(Station::CanAllocateItem());
	Station *station = Station::Create(TileXY(12, 10));
	station->name = "Town receiving station";
	station->owner = CompanyID{0};
	station->town = town;
	station->facilities.Set(StationFacility::Train);
	station->train_station = TileArea(station->xy, 1, 1);
	station->spread = station->train_station;
	MakeRailStation(station->xy, station->owner, station->index, Axis::X, 0, RAILTYPE_RAIL);
	RebuildStationKdtree();
	station->RecomputeCatchment();
	CHECK_FALSE(MegacityManager::IsConsumerStation(station));
	REQUIRE(Command<Commands::DesignateMegacity>::Post(fixture.town));
	CHECK_FALSE(MegacityManager::IsConsumerStation(station));
	UpdateStationAcceptance(station, false);
	CHECK_FALSE(station->goods[food].status.Test(GoodsEntry::State::Acceptance));

	REQUIRE(Town::CanAllocateItem());
	Town *other = Town::Create(TileXY(20, 20));
	other->name = "Other catchment town";
	other->townnametype = SPECSTR_TOWNNAME_START;
	RebuildTownKdtree();
	MakeHouseTile(TileXY(12, 11), other->index, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, false);
	CHECK_FALSE(MegacityManager::IsConsumerStation(station));
	MakeHouseTile(TileXY(10, 11), fixture.town, 0, TOWN_HOUSE_COMPLETED, HouseID{0}, 0, false);
	REQUIRE(MegacityManager::IsConsumerStation(station));
	UpdateStationAcceptance(station, false);
	CHECK(station->goods[food].status.Test(GoodsEntry::State::Acceptance));
	CHECK(station->always_accepted.Test(food));
	station->town = other;
	CHECK_FALSE(MegacityManager::IsConsumerStation(station));
	station->town = town;
	station->catchment_tiles.Reset();
	CHECK_FALSE(MegacityManager::IsConsumerStation(station));
}

class MegacityGuiFixture {
private:
	MockEnvironment &mock = MockEnvironment::Instance();
};

TEST_CASE_METHOD(MegacityGuiFixture, "Sprint 18 GUI - WindowDesc Registration & Widget Tree Validity")
{
	REQUIRE(_window_descs != nullptr);

	const WindowDesc *megacity_desc = nullptr;
	const WindowDesc *corridor_desc = nullptr;
	const WindowDesc *directory_desc = nullptr;

	for (const WindowDesc *desc : *_window_descs) {
		if (desc->cls == WindowClass::MegacityOverview) megacity_desc = desc;
		if (desc->cls == WindowClass::FreightCorridorMonitor) corridor_desc = desc;
		if (desc->cls == WindowClass::UniverseDirectory) directory_desc = desc;
	}

	SECTION("Megacity Overview WindowDesc is registered and valid")
	{
		REQUIRE(megacity_desc != nullptr);
		CHECK(megacity_desc->ini_key == "view_megacity");
		CHECK(megacity_desc->GetDefaultWidth() == 440);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(megacity_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}

	SECTION("Freight Corridor Monitor WindowDesc is registered and valid")
	{
		REQUIRE(corridor_desc != nullptr);
		CHECK(corridor_desc->ini_key == "view_freight_corridors");
		CHECK(corridor_desc->GetDefaultWidth() == 520);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(corridor_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}

	SECTION("Universe Directory WindowDesc is registered and valid")
	{
		REQUIRE(directory_desc != nullptr);
		CHECK(directory_desc->ini_key == "view_universe_directory");
		CHECK(directory_desc->GetDefaultWidth() == 520);

		NWidgetStacked *shade_select = nullptr;
		std::unique_ptr<NWidgetBase> root = nullptr;
		REQUIRE_NOTHROW(root = MakeWindowNWidgetTree(directory_desc->nwid_parts, &shade_select));
		REQUIRE(root != nullptr);
	}
}

TEST_CASE("Sprint 18 GUI - Megacity Profile Presentation and Growth Multipliers")
{
	MegacityManager::Reset();
	TownID tid1{10};
	TownID tid2{20};

	REQUIRE(MegacityManager::RegisterMegacity(tid1, WorldID{1}, "Metropolis Earth", 20000));
	REQUIRE(MegacityManager::RegisterMegacity(tid2, WorldID{2}, "Novosibirsk Mars", 5000));

	SECTION("Initial quotas and zero delivery lead to Starvation")
	{
		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->population == 20000);
		CHECK(p1->monthly_quota[0] == 1000); // 20000 / 20
		CHECK(p1->monthly_quota[1] == 500);  // 20000 / 40
		CHECK(p1->monthly_quota[2] == 200);  // 20000 / 100

		MegacityManager::EvaluateMonthlySupply();
		CHECK(p1->growth_state == MegacityGrowthState::Starvation);
		CHECK(p1->growth_multiplier == 0.0f);
	}

	SECTION("Tier 1 partial satisfaction enables Subsistence")
	{
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 600); // 60% of 1000
		MegacityManager::EvaluateMonthlySupply();

		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->growth_state == MegacityGrowthState::Subsistence);
		CHECK(p1->growth_multiplier == 1.0f);
	}

	SECTION("Tier 1 & Tier 2 complete satisfaction enables Metropolitan Boom")
	{
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 1000);
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 500);
		MegacityManager::EvaluateMonthlySupply();

		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->growth_state == MegacityGrowthState::MetropolitanBoom);
		CHECK(p1->growth_multiplier == 1.5f);
	}

	SECTION("All 3 Tiers met enables HyperGrowth with traffic boost")
	{
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier1_Sustenance, 1200);
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier2_Expansion, 600);
		MegacityManager::RecordDelivery(tid1, MegacityDemandTier::Tier3_Prosperity, 250);
		MegacityManager::EvaluateMonthlySupply();

		const MegacityProfile *p1 = MegacityManager::GetProfile(tid1);
		REQUIRE(p1 != nullptr);
		CHECK(p1->growth_state == MegacityGrowthState::HyperGrowth);
		CHECK(p1->growth_multiplier == 2.0f);
		CHECK(p1->passenger_multiplier == 1.5f);
	}

	SECTION("Listing all megacities preserves entries")
	{
		auto all = MegacityManager::GetAllMegacities();
		CHECK(all.size() == 2);
	}
}

TEST_CASE("Sprint 18 GUI - Freight Corridor Monitor Utilization and Delays")
{
	auto &service = UniverseAuthorityService::Instance();
	service.Reset();

	InterServerRoute route{
		.route_id = 42,
		.source_world = WorldID{1},
		.source_gate_id = 101,
		.dest_world = WorldID{2},
		.dest_gate_id = 202,
		.transit_duration_ticks = 100,
		.max_bandwidth_trains_per_min = 12,
		.max_active_in_transit = 10,
		.priority = FreightPriority::Standard,
	};
	REQUIRE(service.RegisterRoute(route));

	SECTION("Clear Corridor has 1.0x transit duration")
	{
		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Clear);
		auto corridors = service.GetFreightCorridors();
		REQUIRE(corridors.size() == 1);
		CHECK(corridors[0].current_in_transit_count == 0);
	}

	SECTION("Moderate and Congested transitions with train departures")
	{
		ConsistSnapshotBytes snap{.error = ConsistSnapshotError::None, .bytes = {1, 2, 3, 4}};
		std::vector<std::string> ids;
		for (size_t i = 0; i < 6; ++i) { // 6 / 10 = 60% -> Moderate
			std::string tid = service.InitiateTransfer(WorldID{1}, WorldID{2}, 101, 202, snap, 100);
			service.DepartTransfer(tid, 10);
			ids.push_back(tid);
		}

		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Moderate);

		// Add 3 more: 9 / 10 = 90% -> Congested
		for (size_t i = 0; i < 3; ++i) {
			std::string tid = service.InitiateTransfer(WorldID{1}, WorldID{2}, 101, 202, snap, 100);
			service.DepartTransfer(tid, 10);
			ids.push_back(tid);
		}

		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Congested);

		// Add 2 more: 11 / 10 = 110% -> Saturated
		for (size_t i = 0; i < 2; ++i) {
			std::string tid = service.InitiateTransfer(WorldID{1}, WorldID{2}, 101, 202, snap, 100);
			service.DepartTransfer(tid, 10);
			ids.push_back(tid);
		}

		CHECK(service.EvaluateCorridorCongestion(42) == CorridorCongestionLevel::Saturated);
	}
}

TEST_CASE("Sprint 18 GUI - Universe Directory World Status and Heartbeats")
{
	auto &service = UniverseAuthorityService::Instance();
	service.Reset();

	RegisteredWorld w1{
		.world_id = WorldID{1},
		.phase = WorldPhase::Phase1_Core,
		.name = "Earth Core",
		.last_heartbeat_tick = 100,
		.address = "127.0.0.1:3979",
		.description = "Federation Capital",
		.active_clients = 12,
		.max_clients = 32,
		.active_trains = 85,
		.status = WorldOnlineStatus::Online,
	};

	RegisteredWorld w2{
		.world_id = WorldID{2},
		.phase = WorldPhase::Phase3_Frontier,
		.name = "Haven Frontier",
		.last_heartbeat_tick = 50,
		.address = "127.0.0.1:3981",
		.description = "Deep Mining Colony",
		.active_clients = 2,
		.max_clients = 16,
		.active_trains = 14,
		.status = WorldOnlineStatus::Online,
	};

	REQUIRE(service.RegisterWorld(w1));
	REQUIRE(service.RegisterWorld(w2));

	SECTION("Directory contains both registered worlds with correct metrics")
	{
		auto dir = service.GetWorldDirectory();
		REQUIRE(dir.size() == 2);
		CHECK(dir[0].name == "Earth Core");
		CHECK(dir[1].name == "Haven Frontier");
	}

	SECTION("Pruning stale worlds marks unreached nodes as Unreachable")
	{
		// Tick 400: w2 has tick 50 (diff 350 > 300) -> pruned to Unreachable
		size_t pruned = service.PruneStaleWorlds(400, 300);
		CHECK(pruned == 1);

		const RegisteredWorld *p2 = service.GetWorld(WorldID{2});
		REQUIRE(p2 != nullptr);
		CHECK(p2->status == WorldOnlineStatus::Unreachable);

		const RegisteredWorld *p1 = service.GetWorld(WorldID{1});
		REQUIRE(p1 != nullptr);
		CHECK(p1->status == WorldOnlineStatus::Online);
	}
}

TEST_CASE("Sprint 18 GUI - TownView Megacity Status Button Widget")
{
	CHECK(WID_TV_MEGACITY_STATUS > WID_TV_GRAPH);
}
