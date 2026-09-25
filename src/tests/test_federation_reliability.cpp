/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_federation_reliability.cpp Explicit remote identities and native cargo-state regressions. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cargopacket.h"
#include "../company_base.h"
#include "../engine_base.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../portal/consist_snapshot.h"
#include "../portal/federation_cargo.h"
#include "../portal/federation_orders.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnel_map.h"
#include "mock_environment.h"
#include "../safeguards.h"

TEST_CASE("Federation scheduled stations require explicit full-identity mappings")
{
	MockEnvironment::Instance();
	_vehicle_pool.CleanPool();
	_station_pool.CleanPool();
	Map::Allocate(64, 64);
	PlanetManager::Reset();
	PortalRegistry::Reset();
	FederationIdentityRegistry::Reset();
	const FederationNamespace local{11, 12}, remote{21, 22};
	FederationIdentityRegistry::RestoreState(local, 1);
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{1}, .name = "Local", .phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate, .min_x = 1, .min_y = 1, .max_x = 62, .max_y = 62}));
	REQUIRE(Station::CanAllocateItem(2));
	Station *origin = Station::Create(TileXY(10, 10));
	Station *proxy = Station::Create(TileXY(30, 10));
	const auto origin_id = FederationIdentityRegistry::GetOrCreateStation(origin->index);
	REQUIRE(origin_id);
	const GlobalStationID destination{remote, 1, WorldID{2}};
	/* Pool slot 1 exists, but that never establishes a foreign station's identity. */
	CHECK_FALSE(FederationIdentityRegistry::ResolveStation(destination));
	REQUIRE(FederationIdentityRegistry::RestoreStationMapping(proxy->index, 1, remote, WorldID{2}));
	CHECK(FederationIdentityRegistry::ResolveStation(destination) == proxy->index);
	CHECK(FederationIdentityRegistry::ResolveStation(*origin_id) == origin->index);
	CHECK_FALSE(FederationIdentityRegistry::ResolveStation({remote, 1, WorldID{3}}));
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{0}, 7, remote));
	CHECK(FederationIdentityRegistry::FindCompany(CompanyID{0}) == GlobalCompanyID{remote, 7});

	REQUIRE(Vehicle::CanAllocateItem());
	if (!Company::IsValidID(CompanyID{0})) Company::CreateAtIndex(CompanyID{0});
	if (!Engine::IsValidID(EngineID{0})) Engine::CreateAtIndex(EngineID{0}, VehicleType::Train, 0);
	Train *train = Vehicle::Create<Train>();
	train->owner = CompanyID{0};
	train->engine_type = EngineID{0};
	train->cargo_type = CargoType{0};
	train->SetFrontEngine();
	train->SetEngine();
	train->tile = origin->xy;
	Order order;
	order.MakeGoToStation(proxy->index);
	CHECK(GetFederationOrderGate(train, &order) == INVALID_TILE);
	const auto gate = TileXY(40, 10);
	MakeRailTunnel(gate, CompanyID{0}, DiagDirection::SW, RAILTYPE_RAIL);
	PortalRegistry::RegisterInterServerPortal(gate, DiagDirection::SW, WorldID{1}, WorldID{2}, 20, 5);
	CHECK(GetFederationOrderGate(train, &order) == gate);
	CHECK_FALSE(order.ShouldStopAtStation(train, proxy->index));
	order.MakeGoToStation(origin->index);
	CHECK_FALSE(GetFederationOrderGate(train, &order));

	const GlobalConsistID first{local, 1}, second{remote, 1};
	FederationIdentityRegistry::SetConsistSchedule(first, {GlobalOrderDestinationID::ForStation(*origin_id)});
	FederationIdentityRegistry::SetConsistSchedule(second, {GlobalOrderDestinationID::ForStation(destination)});
	CHECK(FederationIdentityRegistry::GetConsistSchedule(first) != FederationIdentityRegistry::GetConsistSchedule(second));
	delete train;
	_station_pool.CleanPool();
	FederationIdentityRegistry::Reset();
	PlanetManager::Reset();
	PortalRegistry::Reset();
}

TEST_CASE("Federation cargo provenance survives splitting and prevents incorrect merging")
{
	FederationCargoRegistry::Reset();
	const GlobalCargoSourceID source{{11, 12}, {{11, 12}, 1, WorldID{1}}, SourceType::Industry, 1, WorldID{1}, 10, 10};
	REQUIRE(CargoPacket::CanAllocateItem());
	CargoPacket *packet = CargoPacket::Create(20, 4, StationID::Invalid(), TileIndex{10}, Money{200});
	FederationCargoRegistry::Set(packet->index.base(), source);
	CargoPacket *split = packet->Split(8);
	REQUIRE(split);
	REQUIRE(FederationCargoRegistry::Find(split->index.base()));
	CHECK(*FederationCargoRegistry::Find(split->index.base()) == source);
	CHECK(packet->Count() + split->Count() == 20);
	CHECK(packet->GetFeederShare() + split->GetFeederShare() == Money{200});
	CHECK(VehicleCargoList::AreMergable(packet, split));
	auto different = source;
	different.origin_world = WorldID{2};
	FederationCargoRegistry::Set(split->index.base(), different);
	CHECK_FALSE(VehicleCargoList::AreMergable(packet, split));
	CHECK_FALSE(StationCargoList::AreMergable(packet, split));
	const auto id = packet->index;
	delete packet;
	CHECK(FederationCargoRegistry::Find(id.base()) == nullptr);
	delete split;
	CHECK(FederationCargoRegistry::GetAll().empty());
}

TEST_CASE("Federation V3 packets preserve quantities age feeder share and payment vectors")
{
	ConsistSnapshot snapshot;
	snapshot.consist_id = {{11, 12}, 1};
	snapshot.direction = to_underlying(Direction::NE);
	const GlobalCargoSourceID source{{11, 12}, {{11, 12}, 1, WorldID{1}}, SourceType::Industry, 1, WorldID{1}, 10, 10};
	ConsistSnapshotUnit unit;
	unit.engine_type = 0;
	unit.cargo_type = 1;
	unit.cargo_capacity = 40;
	unit.cargo_count = 20;
	unit.cargo_source = source;
	unit.packets = {{12, 4, 120, -20, 30, 10, 10, source}, {8, 9, 80, -20, 30, 10, 10, source}};
	snapshot.units.push_back(unit);
	const auto encoded = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(encoded.Succeeded());
	CHECK(encoded.bytes[4] == 3);
	const auto decoded = ConsistSnapshotCodec::Decode(encoded.bytes, snapshot.content_manifest);
	REQUIRE(decoded.Succeeded());
	CHECK(*decoded.snapshot == snapshot);
	snapshot.units[0].packets[0].count++;
	CHECK_FALSE(ConsistSnapshotCodec::Encode(snapshot).Succeeded());
}
