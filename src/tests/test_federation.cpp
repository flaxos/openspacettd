/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_federation.cpp Federation identity, content admission, and snapshot tests. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../map_func.h"
#include "../openttd.h"
#include "../portal/content_manifest.h"
#include "../portal/consist_snapshot.h"
#include "../portal/federation_identity.h"
#include "../settings_type.h"
#include "../train.h"
#include "../vehicle_base.h"

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
