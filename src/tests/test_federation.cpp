/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation.cpp Federation identity, content admission, and snapshot tests. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../company_base.h"
#include "../map_func.h"
#include "../openttd.h"
#include "../order_base.h"
#include "../portal/content_manifest.h"
#include "../portal/consist_snapshot.h"
#include "../portal/federation_identity.h"
#include "../portal/planet_manager.h"
#include "../saveload/saveload_func.h"
#include "../saveload/saveload.h"
#include "../settings_type.h"
#include "../station_base.h"
#include "../train.h"
#include "../vehicle_base.h"
#include "../gfx_func.h"
#include "../table/sprites.h"
#include "../fileio_func.h"
#include "mock_environment.h"

#include <filesystem>

#include "../safeguards.h"

static Train *MakeSnapshotTrain(bool front = true)
{
	Train *train = Vehicle::Create<Train>();
	if (front) {
		train->SetFrontEngine();
		train->SetEngine();
	} else {
		train->ClearFrontEngine();
		train->SetWagon();
	}
	train->engine_type = EngineID{0};
	train->cargo_type = CargoType{0};
	train->cargo_cap = 40;
	train->direction = Direction::NE;
	train->owner = Owner{0};
	return train;
}

static UniverseContentManifest MakeContentManifest()
{
	UniverseContentManifest manifest;
	manifest.network_revision = "openspacettd-sprint-13";
	manifest.landscape = LandscapeType::Tropic;
	manifest.dynamic_engines = true;
	ContentManifestGRF grf;
	grf.grfid = {'T', 'E', 'S', 'T'};
	for (size_t i = 0; i < grf.md5sum.size(); ++i) grf.md5sum[i] = static_cast<uint8_t>(i);
	grf.palette = 3;
	grf.flags = static_cast<uint8_t>(ContentManifestGRFFlag::Static);
	grf.parameters = {0, 0x11223344, UINT32_MAX};
	manifest.newgrfs.push_back(std::move(grf));
	return manifest;
}

static uint32_t TestChecksum(std::span<const uint8_t> bytes)
{
	uint32_t crc = 0xFFFFFFFFU;
	for (uint8_t byte : bytes) {
		crc ^= byte;
		for (uint bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

static void WriteTestU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
	for (uint shift = 0; shift < 32; shift += 8) bytes[offset + shift / 8] = static_cast<uint8_t>(value >> shift);
}

static void RefreshTestChecksum(std::vector<uint8_t> &bytes)
{
	WriteTestU32(bytes, bytes.size() - 4, TestChecksum(std::span<const uint8_t>(bytes).first(bytes.size() - 4)));
}

TEST_CASE("Content Manifest - canonical codec and digest")
{
	UniverseContentManifest manifest = MakeContentManifest();
	ContentManifestBytes first = ContentManifestCodec::Encode(manifest);
	ContentManifestBytes second = ContentManifestCodec::Encode(manifest);
	REQUIRE(first.Succeeded());
	REQUIRE(second.Succeeded());
	CHECK(first.bytes == second.bytes);
	CHECK(first.bytes.size() < CONTENT_MANIFEST_MAX_BYTES);

	ContentManifestResult decoded = ContentManifestCodec::Decode(first.bytes);
	REQUIRE(decoded.Succeeded());
	CHECK(*decoded.manifest == manifest);
	UniverseContentManifest empty;
	empty.network_revision = manifest.network_revision;
	ContentManifestBytes empty_bytes = ContentManifestCodec::Encode(empty);
	REQUIRE(empty_bytes.Succeeded());
	ContentManifestResult empty_decoded = ContentManifestCodec::Decode(empty_bytes.bytes);
	REQUIRE(empty_decoded.Succeeded());
	CHECK(*empty_decoded.manifest == empty);

	ContentManifestTokenResult first_token = ContentManifestCodec::Digest(manifest);
	ContentManifestTokenResult second_token = ContentManifestCodec::Digest(manifest);
	REQUIRE(first_token.Succeeded());
	REQUIRE(second_token.Succeeded());
	CHECK(first_token.token == second_token.token);
	CHECK(first_token.token != ContentManifestToken{});

	std::vector<uint8_t> corrupt = first.bytes;
	corrupt.at(20) ^= 0x80;
	CHECK(ContentManifestCodec::Decode(corrupt).error == ContentManifestError::ChecksumMismatch);
	CHECK(ContentManifestCodec::Decode(std::span<const uint8_t>(first.bytes.data(), 8)).error == ContentManifestError::Truncated);
	std::vector<uint8_t> bad_magic = first.bytes;
	bad_magic[0] = 'X';
	RefreshTestChecksum(bad_magic);
	CHECK(ContentManifestCodec::Decode(bad_magic).error == ContentManifestError::InvalidMagic);
	std::vector<uint8_t> bad_version = first.bytes;
	bad_version[4] = CONTENT_MANIFEST_VERSION + 1;
	RefreshTestChecksum(bad_version);
	CHECK(ContentManifestCodec::Decode(bad_version).error == ContentManifestError::UnsupportedVersion);
	std::vector<uint8_t> bad_length = first.bytes;
	WriteTestU32(bad_length, 8, static_cast<uint32_t>(bad_length.size() + 1));
	RefreshTestChecksum(bad_length);
	CHECK(ContentManifestCodec::Decode(bad_length).error == ContentManifestError::LengthMismatch);

	std::vector<uint8_t> trailing = first.bytes;
	trailing.insert(trailing.end() - 4, 0);
	WriteTestU32(trailing, 8, static_cast<uint32_t>(trailing.size()));
	RefreshTestChecksum(trailing);
	CHECK(ContentManifestCodec::Decode(trailing).error == ContentManifestError::LengthMismatch);
	std::vector<uint8_t> oversized(CONTENT_MANIFEST_MAX_BYTES + 1);
	CHECK(ContentManifestCodec::Decode(oversized).error == ContentManifestError::TooLarge);

	UniverseContentManifest invalid = manifest;
	invalid.network_revision.clear();
	CHECK(ContentManifestCodec::Encode(invalid).error == ContentManifestError::InvalidField);
	invalid = manifest;
	invalid.newgrfs.front().parameters.resize(CONTENT_MANIFEST_MAX_PARAMETERS + 1);
	CHECK(ContentManifestCodec::Encode(invalid).error == ContentManifestError::TooManyParameters);
	invalid = manifest;
	invalid.newgrfs.resize(CONTENT_MANIFEST_MAX_NEWGRFS + 1, manifest.newgrfs.front());
	CHECK(ContentManifestCodec::Encode(invalid).error == ContentManifestError::TooManyNewGRFs);
}

TEST_CASE("Content Manifest - strict compatibility diagnostics")
{
	UniverseContentManifest local = MakeContentManifest();
	CHECK(ContentManifestCodec::Compare(local, local).IsCompatible());
	UniverseContentManifest other_version = local;
	++other_version.version;
	CHECK(ContentManifestCodec::Compare(local, other_version).reason == ContentCompatibility::VersionMismatch);

	auto CheckMismatch = [&](ContentCompatibility reason, auto mutate) {
		UniverseContentManifest remote = local;
		mutate(remote);
		ContentCompatibilityResult result = ContentManifestCodec::Compare(local, remote);
		CHECK(result.reason == reason);
		ContentManifestTokenResult local_token = ContentManifestCodec::Digest(local);
		ContentManifestTokenResult remote_token = ContentManifestCodec::Digest(remote);
		REQUIRE(local_token.Succeeded());
		REQUIRE(remote_token.Succeeded());
		CHECK(local_token.token != remote_token.token);
	};

	CheckMismatch(ContentCompatibility::BuildRevisionMismatch, [](auto &m) { m.network_revision += "-other"; });
	CheckMismatch(ContentCompatibility::LandscapeMismatch, [](auto &m) { m.landscape = LandscapeType::Arctic; });
	CheckMismatch(ContentCompatibility::DynamicEnginesMismatch, [](auto &m) { m.dynamic_engines = false; });
	CheckMismatch(ContentCompatibility::NewGRFCountMismatch, [](auto &m) { m.newgrfs.clear(); });
	CheckMismatch(ContentCompatibility::NewGRFIdentityMismatch, [](auto &m) { m.newgrfs[0].grfid[0] ^= 1; });
	CheckMismatch(ContentCompatibility::NewGRFChecksumMismatch, [](auto &m) { m.newgrfs[0].md5sum[0] ^= 1; });
	CheckMismatch(ContentCompatibility::NewGRFPaletteMismatch, [](auto &m) { ++m.newgrfs[0].palette; });
	CheckMismatch(ContentCompatibility::NewGRFFlagsMismatch, [](auto &m) { m.newgrfs[0].flags |= static_cast<uint8_t>(ContentManifestGRFFlag::System); });
	CheckMismatch(ContentCompatibility::NewGRFParametersMismatch, [](auto &m) { ++m.newgrfs[0].parameters[0]; });

	UniverseContentManifest ordered = local;
	ordered.newgrfs.push_back(ordered.newgrfs.front());
	ordered.newgrfs[1].grfid = {'N', 'E', 'X', 'T'};
	UniverseContentManifest reordered = ordered;
	std::swap(reordered.newgrfs[0], reordered.newgrfs[1]);
	ContentCompatibilityResult order_result = ContentManifestCodec::Compare(ordered, reordered);
	CHECK(order_result.reason == ContentCompatibility::NewGRFIdentityMismatch);
	CHECK(order_result.newgrf_index == 0);
}

TEST_CASE("Federation Identity - deterministic namespace and consist lifecycle")
{
	Map::Allocate(64, 64);
	_vehicle_pool.CleanPool();
	FederationIdentityRegistry::Reset();
	_settings_game.game_creation.generation_seed = 0x12345678;
	_settings_game.game_creation.starting_year = TimerGameCalendar::Year{1950};
	std::string old_savegame_id = _game_session_stats.savegame_id;
	_game_session_stats.savegame_id.clear();

	FederationNamespace expected = FederationIdentityRegistry::DeriveNamespace(0x12345678, 64, 64, 1950);
	CHECK(FederationIdentityRegistry::GetNamespace() == expected);
	CHECK(expected.IsValid());

	REQUIRE(Vehicle::CanAllocateItem(3));
	Train *source = MakeSnapshotTrain();
	Train *source_wagon = MakeSnapshotTrain(false);
	source->SetNext(source_wagon);
	auto source_id = FederationIdentityRegistry::GetOrCreate(source);
	REQUIRE(source_id.has_value());
	CHECK(source_id->sequence == 1);
	CHECK(FederationIdentityRegistry::GetOrCreate(source_wagon) == source_id);

	/* Reordering preserves the ID because the original anchor stays present. */
	source->SetNext(nullptr);
	source_wagon->SetFrontEngine();
	source_wagon->SetEngine();
	source_wagon->SetNext(source);
	source->ClearFrontEngine();
	CHECK(FederationIdentityRegistry::Find(source_wagon) == source_id);

	/* A split leaves the identity with the original anchor. */
	source_wagon->SetNext(nullptr);
	source->SetFrontEngine();
	source->SetEngine();
	CHECK(FederationIdentityRegistry::Find(source) == source_id);
	CHECK_FALSE(FederationIdentityRegistry::Find(source_wagon).has_value());

	/* A merge explicitly keeps the destination identity. */
	Train *destination = MakeSnapshotTrain();
	auto destination_id = FederationIdentityRegistry::GetOrCreate(destination);
	REQUIRE(destination_id.has_value());
	destination->SetNext(source);
	source->ClearFrontEngine();
	FederationIdentityRegistry::ReconcileConsistChange(nullptr, destination, destination_id);
	CHECK(FederationIdentityRegistry::Find(destination) == destination_id);
	CHECK(FederationIdentityRegistry::GetMappings().size() == 1);

	VehicleID destination_anchor = destination->index;
	_vehicle_pool.CleanPool();
	CHECK(FederationIdentityRegistry::GetMappings().find(destination_anchor.base()) == FederationIdentityRegistry::GetMappings().end());
	FederationIdentityRegistry::Reset();

	_game_session_stats.savegame_id = "sprint-12-save-a";
	FederationNamespace save_a = FederationIdentityRegistry::GetNamespace();
	FederationIdentityRegistry::Reset();
	_game_session_stats.savegame_id = "sprint-12-save-b";
	CHECK(FederationIdentityRegistry::GetNamespace() != save_a);
	FederationIdentityRegistry::Reset();
	_game_session_stats.savegame_id = old_savegame_id;
}

TEST_CASE("Consist Snapshot - canonical round trip and validation")
{
	FederationIdentityRegistry::Reset();
	ConsistSnapshot snapshot;
	for (size_t i = 0; i < snapshot.content_manifest.size(); ++i) snapshot.content_manifest[i] = static_cast<uint8_t>(i);
	for (size_t i = 0; i < snapshot.owner.size(); ++i) snapshot.owner[i] = static_cast<uint8_t>(0xF0 + i);
	snapshot.consist_id = {{0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL}, 42};
	snapshot.direction = to_underlying(Direction::SW);
	snapshot.speed = 173;
	snapshot.subspeed = 9;
	snapshot.acceleration = 4;
	snapshot.stopped = true;
	snapshot.driving_backwards = true;
	snapshot.units.push_back({
		.engine_type = 7,
		.subtype = 9,
		.cargo_type = 2,
		.cargo_subtype = 3,
		.cargo_capacity = 120,
		.refit_capacity = 8,
		.cargo_count = 77,
		.build_year = 2045,
		.age = 1200,
		.max_age = 7300,
		.value = 987654321,
		.reliability = 60000,
		.reliability_speed_decrease = 21,
		.breakdown_counter = 3,
		.breakdown_delay = 4,
		.breakdowns_since_service = 5,
		.breakdown_chance = 6,
		.random_bits = 0xBEEF,
		.cargo_provenance_unresolved = true,
	});

	ConsistSnapshotBytes first = ConsistSnapshotCodec::Encode(snapshot);
	ConsistSnapshotBytes second = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(first.Succeeded());
	REQUIRE(second.Succeeded());
	CHECK(first.bytes == second.bytes);
	CHECK(first.bytes.size() < CONSIST_SNAPSHOT_MAX_BYTES);

	ConsistSnapshotResult decoded = ConsistSnapshotCodec::Decode(first.bytes, snapshot.content_manifest);
	REQUIRE(decoded.Succeeded());
	CHECK(*decoded.snapshot == snapshot);

	ContentManifestToken wrong_manifest{};
	CHECK(ConsistSnapshotCodec::Decode(first.bytes, wrong_manifest).error == ConsistSnapshotError::ManifestMismatch);

	std::vector<uint8_t> corrupt = first.bytes;
	corrupt.at(20) ^= 0x80;
	CHECK(ConsistSnapshotCodec::Decode(corrupt, snapshot.content_manifest).error == ConsistSnapshotError::ChecksumMismatch);

	ConsistSnapshot invalid = snapshot;
	invalid.units.front().cargo_count = invalid.units.front().cargo_capacity + 1;
	CHECK(ConsistSnapshotCodec::Encode(invalid).error == ConsistSnapshotError::InvalidField);
	invalid = snapshot;
	invalid.units.front().cargo_type = UINT8_MAX;
	CHECK(ConsistSnapshotCodec::Encode(invalid).error == ConsistSnapshotError::InvalidField);
	invalid = snapshot;
	invalid.units.resize(CONSIST_SNAPSHOT_MAX_UNITS + 1, snapshot.units.front());
	CHECK(ConsistSnapshotCodec::Encode(invalid).error == ConsistSnapshotError::TooManyUnits);
	CHECK(ConsistSnapshotCodec::Decode(std::span<const uint8_t>(first.bytes.data(), 8), snapshot.content_manifest).error == ConsistSnapshotError::Truncated);
}

TEST_CASE("Consist Snapshot - capture is non-spatial and leaves the train unchanged")
{
	Map::Allocate(64, 64);
	_vehicle_pool.CleanPool();
	FederationIdentityRegistry::Reset();
	_settings_game.game_creation.generation_seed = 99;
	_settings_game.game_creation.starting_year = TimerGameCalendar::Year{2000};

	REQUIRE(Vehicle::CanAllocateItem(2));
	Train *engine = MakeSnapshotTrain();
	Train *wagon = MakeSnapshotTrain(false);
	engine->SetNext(wagon);
	engine->cur_speed = 88;
	engine->subspeed = 7;
	engine->acceleration = 3;
	engine->vehstatus.Set(VehState::Stopped);
	engine->vehicle_flags.Set(VehicleFlag::DrivingBackwards);
	wagon->cargo_subtype = 2;
	wagon->cargo_cap = 55;
	wagon->reliability = 4321;
	wagon->random_bits = 0x1234;
	TileIndex original_tile = engine->tile;
	uint16_t original_random_bits = wagon->random_bits;

	ContentManifestToken manifest{};
	manifest.fill(0x5A);
	GlobalOwnerToken owner{};
	owner.fill(0xA5);
	ConsistSnapshotResult captured = ConsistSnapshotCodec::Capture(engine, manifest, owner);
	REQUIRE(captured.Succeeded());
	CHECK(captured.snapshot->units.size() == 2);
	CHECK(captured.snapshot->speed == 88);
	CHECK(captured.snapshot->stopped);
	CHECK(captured.snapshot->driving_backwards);
	CHECK(captured.snapshot->units[1].cargo_capacity == 55);
	CHECK(captured.snapshot->units[1].reliability == 4321);
	CHECK(captured.snapshot->units[1].random_bits == 0x1234);
	CHECK(engine->tile == original_tile);
	CHECK(wagon->random_bits == original_random_bits);

	ContentManifestResult current_manifest = ContentManifestCodec::CaptureCurrent();
	REQUIRE(current_manifest.Succeeded());
	ConsistSnapshotResult current_capture = ConsistSnapshotCodec::CaptureForCurrentContent(engine, owner);
	REQUIRE(current_capture.Succeeded());
	ConsistSnapshotBytes current_bytes = ConsistSnapshotCodec::Encode(*current_capture.snapshot);
	REQUIRE(current_bytes.Succeeded());
	CHECK(ConsistSnapshotCodec::DecodeForCurrentContent(current_bytes.bytes).Succeeded());
	std::vector<uint8_t> mismatched_bytes = current_bytes.bytes;
	mismatched_bytes[12] ^= 1;
	WriteTestU32(mismatched_bytes, mismatched_bytes.size() - 4,
		TestChecksum(std::span<const uint8_t>(mismatched_bytes).first(mismatched_bytes.size() - 4)));
	CHECK(ConsistSnapshotCodec::DecodeForCurrentContent(mismatched_bytes).error == ConsistSnapshotError::ManifestMismatch);

	_vehicle_pool.CleanPool();
	FederationIdentityRegistry::Reset();
}

static std::vector<uint8_t> BuildV1SnapshotBytes(const ConsistSnapshot &snapshot)
{
	std::vector<uint8_t> data;
	auto U8 = [&](uint8_t v) { data.push_back(v); };
	auto U16 = [&](uint16_t v) { U8(static_cast<uint8_t>(v)); U8(static_cast<uint8_t>(v >> 8)); };
	auto U32 = [&](uint32_t v) { for (uint s = 0; s < 32; s += 8) U8(static_cast<uint8_t>(v >> s)); };
	auto U64 = [&](uint64_t v) { for (uint s = 0; s < 64; s += 8) U8(static_cast<uint8_t>(v >> s)); };
	auto I32 = [&](int32_t v) { U32(static_cast<uint32_t>(v)); };
	auto I64 = [&](int64_t v) { U64(static_cast<uint64_t>(v)); };

	for (uint8_t b : std::array<uint8_t, 4>{'O', 'S', 'C', 'S'}) U8(b);
	U16(1); // Wire Version 1
	U16(0);
	U32(0); // placeholder
	for (uint8_t b : snapshot.content_manifest) U8(b);
	U64(snapshot.consist_id.name_space.high);
	U64(snapshot.consist_id.name_space.low);
	U64(snapshot.consist_id.sequence);
	for (uint8_t b : snapshot.owner) U8(b);
	U8(snapshot.direction);
	uint8_t flags = (snapshot.stopped ? 1 : 0) | (snapshot.driving_backwards ? 2 : 0);
	U8(flags);
	U16(snapshot.speed);
	U8(snapshot.subspeed);
	U8(snapshot.acceleration);
	U16(static_cast<uint16_t>(snapshot.units.size()));

	for (const ConsistSnapshotUnit &unit : snapshot.units) {
		U16(unit.engine_type);
		U8(unit.subtype);
		U8(unit.cargo_type);
		U8(unit.cargo_subtype);
		U16(unit.cargo_capacity);
		U16(unit.refit_capacity);
		U32(unit.cargo_count);
		I32(unit.build_year);
		I32(unit.age);
		I32(unit.max_age);
		I64(unit.value);
		U16(unit.reliability);
		U16(unit.reliability_speed_decrease);
		U8(unit.breakdown_counter);
		U8(unit.breakdown_delay);
		U8(unit.breakdowns_since_service);
		U8(unit.breakdown_chance);
		U16(unit.random_bits);
		U8(unit.cargo_provenance_unresolved ? 1 : 0);
	}

	uint32_t total = static_cast<uint32_t>(data.size() + 4);
	for (uint s = 0; s < 32; s += 8) data[8 + s / 8] = static_cast<uint8_t>(total >> s);
	uint32_t crc = TestChecksum(std::span<const uint8_t>(data));
	U32(crc);
	return data;
}

TEST_CASE("Federation Identity - company and station lifecycle")
{
	Map::Allocate(64, 64);
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	FederationIdentityRegistry::Reset();
	_company_pool.CleanPool();
	_station_pool.CleanPool();
	PlanetManager::Reset();

	/* 1. Global Company lifecycle */
	REQUIRE(Company::CanAllocateItem());
	Company *comp = Company::Create();
	REQUIRE(comp != nullptr);
	CompanyID comp_id = comp->index;

	auto global_comp = FederationIdentityRegistry::GetOrCreateCompany(comp_id);
	REQUIRE(global_comp.has_value());
	CHECK(global_comp->IsValid());
	CHECK(global_comp->sequence == 1);
	CHECK(global_comp->name_space == FederationIdentityRegistry::GetNamespace());
	CHECK(FederationIdentityRegistry::FindCompany(comp_id) == global_comp);

	/* Token conversion */
	GlobalOwnerToken owner_tok = global_comp->ToOwnerToken();
	CHECK(owner_tok != GlobalOwnerToken{});

	/* Company release on deletion */
	delete comp;
	CHECK_FALSE(FederationIdentityRegistry::FindCompany(comp_id).has_value());
	CHECK(FederationIdentityRegistry::GetCompanyMappings().empty());

	/* 2. Global Station lifecycle and world attribution */
	PlanetRegion reg{
		.id = WorldID{2},
		.name = "Industrial World",
		.phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate,
		.min_x = 0, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg);

	REQUIRE(Station::CanAllocateItem());
	Station *st = Station::Create(TileXY(10, 10));
	REQUIRE(st != nullptr);
	st->owner = CompanyID{0};
	StationID st_id = st->index;

	auto global_st = FederationIdentityRegistry::GetOrCreateStation(st_id);
	REQUIRE(global_st.has_value());
	CHECK(global_st->IsValid());
	CHECK(global_st->sequence == 1);
	CHECK(global_st->world_id == WorldID{2});
	CHECK(FederationIdentityRegistry::FindStation(st_id) == global_st);

	/* Station release on deletion */
	delete st;
	CHECK_FALSE(FederationIdentityRegistry::FindStation(st_id).has_value());
	CHECK(FederationIdentityRegistry::GetStationMappings().empty());

	/* 3. Pruning stale mappings */
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{5}, 99));
	REQUIRE(FederationIdentityRegistry::RestoreStationMapping(StationID{8}, 77));
	CHECK(FederationIdentityRegistry::GetCompanyMappings().size() == 1);
	CHECK(FederationIdentityRegistry::GetStationMappings().size() == 1);

	FederationIdentityRegistry::PruneStaleCompanyMappings();
	FederationIdentityRegistry::PruneStaleStationMappings();
	CHECK(FederationIdentityRegistry::GetCompanyMappings().empty());
	CHECK(FederationIdentityRegistry::GetStationMappings().empty());

	PlanetManager::Reset();
	FederationIdentityRegistry::Reset();
}

TEST_CASE("Federation Identity - cargo source and order destination provenance")
{
	Map::Allocate(64, 64);
	FederationIdentityRegistry::Reset();
	PlanetManager::Reset();

	PlanetRegion reg{
		.id = WorldID{3},
		.name = "Frontier Outpost",
		.phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::AridDesert,
		.min_x = 0, .min_y = 0, .max_x = 63, .max_y = 63,
	};
	PlanetManager::RegisterRegion(reg);

	GlobalStationID st_id{FederationIdentityRegistry::GetNamespace(), 42, WorldID{3}};

	/* 1. GlobalCargoSourceID provenance creation */
	Source ind_source{15, SourceType::Industry};
	GlobalCargoSourceID cargo_src = FederationIdentityRegistry::CreateCargoSource(
		StationID{1}, ind_source, TileXY(25, 25));

	CHECK(cargo_src.IsValid());
	CHECK(cargo_src.name_space == FederationIdentityRegistry::GetNamespace());
	CHECK(cargo_src.source_type == SourceType::Industry);
	CHECK(cargo_src.source_sequence == 1);
	CHECK(cargo_src.origin_world == WorldID{3});
	CHECK(cargo_src.origin_tile_x == 25);
	CHECK(cargo_src.origin_tile_y == 25);

	Source town_source{7, SourceType::Town};
	GlobalCargoSourceID town_cargo_src = FederationIdentityRegistry::CreateCargoSource(
		StationID::Invalid(), town_source, INVALID_TILE);

	CHECK(town_cargo_src.IsValid());
	CHECK(town_cargo_src.source_type == SourceType::Town);
	CHECK(town_cargo_src.source_sequence == 2);
	CHECK(cargo_src != town_cargo_src);

	/* 2. GlobalOrderDestinationID creation across destination types */
	GlobalOrderDestinationID dest_st = GlobalOrderDestinationID::ForStation(st_id, false);
	CHECK(dest_st.IsValid());
	CHECK(dest_st.type == OrderDestinationType::Station);
	CHECK(dest_st.station_id == st_id);
	CHECK(dest_st.target_world == WorldID{3});

	GlobalOrderDestinationID dest_wp = GlobalOrderDestinationID::ForStation(st_id, true);
	CHECK(dest_wp.IsValid());
	CHECK(dest_wp.type == OrderDestinationType::Waypoint);

	GlobalOrderDestinationID dest_dp = GlobalOrderDestinationID::ForDepot(
		FederationIdentityRegistry::GetNamespace(), 12, WorldID{1});
	CHECK(dest_dp.IsValid());
	CHECK(dest_dp.type == OrderDestinationType::Depot);
	CHECK(dest_dp.destination_sequence == 12);
	CHECK(dest_dp.target_world == WorldID{1});

	GlobalOrderDestinationID dest_gate = GlobalOrderDestinationID::ForPortalGate(
		FederationIdentityRegistry::GetNamespace(), 101, WorldID{2});
	CHECK(dest_gate.IsValid());
	CHECK(dest_gate.type == OrderDestinationType::PortalGate);
	CHECK(dest_gate.destination_sequence == 101);
	CHECK(dest_gate.target_world == WorldID{2});

	PlanetManager::Reset();
	FederationIdentityRegistry::Reset();
}

TEST_CASE("Consist Snapshot - v2 encode/decode with company, cargo source, and orders")
{
	FederationIdentityRegistry::Reset();
	ConsistSnapshot snapshot;
	for (size_t i = 0; i < snapshot.content_manifest.size(); ++i) snapshot.content_manifest[i] = static_cast<uint8_t>(i);
	for (size_t i = 0; i < snapshot.owner.size(); ++i) snapshot.owner[i] = static_cast<uint8_t>(0xF0 + i);
	snapshot.consist_id = {{0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL}, 42};
	snapshot.company_id = {{0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL}, 7};
	snapshot.direction = to_underlying(Direction::SW);
	snapshot.speed = 180;
	snapshot.subspeed = 3;
	snapshot.acceleration = 5;
	snapshot.stopped = false;
	snapshot.driving_backwards = false;

	/* Locomotive: empty cargo */
	snapshot.units.push_back({
		.engine_type = 1,
		.subtype = 0,
		.cargo_type = 0,
		.cargo_subtype = 0,
		.cargo_capacity = 0,
		.refit_capacity = 0,
		.cargo_count = 0,
		.build_year = 2050,
		.age = 500,
		.max_age = 7300,
		.value = 1000000,
		.reliability = 65000,
		.reliability_speed_decrease = 10,
		.breakdown_counter = 0,
		.breakdown_delay = 0,
		.breakdowns_since_service = 0,
		.breakdown_chance = 1,
		.random_bits = 0x1234,
		.cargo_provenance_unresolved = false,
		.cargo_source = {},
	});

	/* Wagon with resolved cargo source provenance */
	GlobalStationID st_id{snapshot.consist_id.name_space, 10, WorldID{1}};
	GlobalCargoSourceID cargo_src{
		.name_space = snapshot.consist_id.name_space,
		.origin_station = st_id,
		.source_type = SourceType::Industry,
		.source_sequence = 99,
		.origin_world = WorldID{1},
		.origin_tile_x = 45,
		.origin_tile_y = 60,
	};
	REQUIRE(cargo_src.IsValid());

	snapshot.units.push_back({
		.engine_type = 5,
		.subtype = 0,
		.cargo_type = 2,
		.cargo_subtype = 1,
		.cargo_capacity = 80,
		.refit_capacity = 0,
		.cargo_count = 60,
		.build_year = 2051,
		.age = 400,
		.max_age = 7300,
		.value = 250000,
		.reliability = 64000,
		.reliability_speed_decrease = 5,
		.breakdown_counter = 0,
		.breakdown_delay = 0,
		.breakdowns_since_service = 0,
		.breakdown_chance = 0,
		.random_bits = 0x5678,
		.cargo_provenance_unresolved = false,
		.cargo_source = cargo_src,
	});

	/* Global order destinations */
	snapshot.orders.push_back(GlobalOrderDestinationID::ForStation(st_id, false));
	snapshot.orders.push_back(GlobalOrderDestinationID::ForDepot(snapshot.consist_id.name_space, 3, WorldID{2}));

	/* Encode as v2 */
	ConsistSnapshotBytes encoded = ConsistSnapshotCodec::Encode(snapshot);
	REQUIRE(encoded.Succeeded());
	/* Remove the V3 empty packet lists to retain a genuine V2 regression. */
	encoded.bytes.erase(encoded.bytes.end() - 4 - snapshot.units.size() * 2 - 2, encoded.bytes.end() - 4);
	encoded.bytes[4] = 2;
	WriteTestU32(encoded.bytes, 8, static_cast<uint32_t>(encoded.bytes.size()));
	RefreshTestChecksum(encoded.bytes);
	CHECK(encoded.bytes[4] == 2); // Version 2
	CHECK(encoded.bytes[5] == 0);

	/* Decode v2 */
	ConsistSnapshotResult decoded = ConsistSnapshotCodec::Decode(encoded.bytes, snapshot.content_manifest);
	REQUIRE(decoded.Succeeded());
	CHECK(*decoded.snapshot == snapshot);
	CHECK(decoded.snapshot->company_id == snapshot.company_id);
	CHECK(decoded.snapshot->orders.size() == 2);
	CHECK(decoded.snapshot->orders[0] == snapshot.orders[0]);
	CHECK(decoded.snapshot->orders[1] == snapshot.orders[1]);
	CHECK(decoded.snapshot->units[1].cargo_source == cargo_src);
	CHECK_FALSE(decoded.snapshot->units[1].cargo_provenance_unresolved);

	FederationIdentityRegistry::Reset();
}

TEST_CASE("Consist Snapshot - v1 wire format backward compatibility decoding")
{
	ConsistSnapshot v1_proto;
	for (size_t i = 0; i < v1_proto.content_manifest.size(); ++i) v1_proto.content_manifest[i] = static_cast<uint8_t>(0x33 + i);
	for (size_t i = 0; i < v1_proto.owner.size(); ++i) v1_proto.owner[i] = static_cast<uint8_t>(0x77 + i);
	v1_proto.consist_id = {{0xFEEDFACECAFEBEEFULL, 0x0123456789ABCDEFULL}, 88};
	v1_proto.direction = to_underlying(Direction::NW);
	v1_proto.speed = 120;
	v1_proto.units.push_back({
		.engine_type = 3,
		.subtype = 0,
		.cargo_type = 1,
		.cargo_subtype = 0,
		.cargo_capacity = 50,
		.refit_capacity = 0,
		.cargo_count = 35,
		.build_year = 2030,
		.age = 2000,
		.max_age = 7300,
		.value = 500000,
		.reliability = 60000,
		.reliability_speed_decrease = 15,
		.breakdown_counter = 1,
		.breakdown_delay = 2,
		.breakdowns_since_service = 3,
		.breakdown_chance = 4,
		.random_bits = 0x4321,
		.cargo_provenance_unresolved = true,
		.cargo_source = {},
	});

	std::vector<uint8_t> v1_bytes = BuildV1SnapshotBytes(v1_proto);
	REQUIRE(v1_bytes[4] == 1); // Confirm version 1 byte

	ConsistSnapshotResult decoded = ConsistSnapshotCodec::Decode(v1_bytes, v1_proto.content_manifest);
	REQUIRE(decoded.Succeeded());
	CHECK(decoded.snapshot->consist_id == v1_proto.consist_id);
	CHECK(decoded.snapshot->owner == v1_proto.owner);
	CHECK(decoded.snapshot->company_id == GlobalCompanyID{});
	CHECK(decoded.snapshot->orders.empty());
	CHECK(decoded.snapshot->units.size() == 1);
	CHECK(decoded.snapshot->units.front().cargo_provenance_unresolved);
	CHECK_FALSE(decoded.snapshot->units.front().cargo_source.IsValid());
}

TEST_CASE("Federation Identity - savegame FIDS round-trip with all mappings")
{
	const std::string test_save_file = (std::filesystem::temp_directory_path() / "test_openspacettd_fids_mappings.sav").string();
	std::filesystem::remove(test_save_file);

	Map::Allocate(64, 64);
	MockEnvironment &mock = MockEnvironment::Instance();
	(void)mock;
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);

	if (_valid_searchpaths.empty()) {
		_valid_searchpaths.push_back(Searchpath::WorkingDir);
	}

	_company_pool.CleanPool();
	REQUIRE(Company::CanAllocateItem());
	Company *c = Company::Create();
	REQUIRE(c != nullptr);

	FederationIdentityRegistry::Reset();
	FederationNamespace federation_namespace{0x123456789ABCDEF0ULL, 0x0FEDCBA987654321ULL};
	FederationIdentityRegistry::RestoreState(federation_namespace, 50);
	FederationIdentityRegistry::RestoreCounters(20, 30, 40);
	REQUIRE(FederationIdentityRegistry::RestoreCompanyMapping(CompanyID{2}, 15));
	REQUIRE(FederationIdentityRegistry::RestoreStationMapping(StationID{4}, 25));
	REQUIRE(FederationIdentityRegistry::RestoreSourceMapping(0x00010005, 35));

	CHECK(FederationIdentityRegistry::GetNextSequence() == 50);
	CHECK(FederationIdentityRegistry::GetNextCompanySequence() == 20);
	CHECK(FederationIdentityRegistry::GetNextStationSequence() == 30);
	CHECK(FederationIdentityRegistry::GetNextSourceSequence() == 40);

	/* Save the game */
	SaveLoadResult save_res = SaveOrLoad(test_save_file, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(save_res == SaveLoadResult::Ok);
	REQUIRE(std::filesystem::exists(test_save_file));

	/* Reset identity registry */
	FederationIdentityRegistry::Reset();
	CHECK(FederationIdentityRegistry::GetNextSequence() == 1);
	CHECK(FederationIdentityRegistry::GetNextCompanySequence() == 1);
	CHECK(FederationIdentityRegistry::GetCompanyMappings().empty());

	/* Load the game back */
	SaveLoadResult load_res = SaveOrLoad(test_save_file, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false);
	REQUIRE(load_res == SaveLoadResult::Ok);

	CHECK(FederationIdentityRegistry::GetNamespace() == federation_namespace);
	CHECK(FederationIdentityRegistry::GetNextSequence() == 50);
	CHECK(FederationIdentityRegistry::GetNextCompanySequence() == 20);
	CHECK(FederationIdentityRegistry::GetNextStationSequence() == 30);
	CHECK(FederationIdentityRegistry::GetNextSourceSequence() == 40);

	/* Stale mappings were pruned during Load() because no live pools exist */
	CHECK(FederationIdentityRegistry::GetCompanyMappings().empty());
	CHECK(FederationIdentityRegistry::GetStationMappings().empty());

	std::filesystem::remove(test_save_file);
	FederationIdentityRegistry::Reset();
}
