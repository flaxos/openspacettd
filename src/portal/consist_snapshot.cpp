/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file consist_snapshot.cpp Deterministic transfer envelope for train consists. */

#include "../stdafx.h"
#include "consist_snapshot.h"

#include "../base_consist.h"
#include "../cargo_type.h"
#include "../direction_type.h"
#include "../train.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

namespace {

static constexpr std::array<uint8_t, 4> SNAPSHOT_MAGIC{'O', 'S', 'C', 'S'};
static constexpr size_t SNAPSHOT_LENGTH_OFFSET = 8;

class ByteWriter {
public:
	std::vector<uint8_t> data;

	void U8(uint8_t value) { this->data.push_back(value); }
	void U16(uint16_t value)
	{
		this->U8(static_cast<uint8_t>(value));
		this->U8(static_cast<uint8_t>(value >> 8));
	}
	void U32(uint32_t value)
	{
		for (uint shift = 0; shift < 32; shift += 8) this->U8(static_cast<uint8_t>(value >> shift));
	}
	void U64(uint64_t value)
	{
		for (uint shift = 0; shift < 64; shift += 8) this->U8(static_cast<uint8_t>(value >> shift));
	}
	void I32(int32_t value) { this->U32(static_cast<uint32_t>(value)); }
	void I64(int64_t value) { this->U64(static_cast<uint64_t>(value)); }

	template <size_t N>
	void Bytes(const std::array<uint8_t, N> &value)
	{
		this->data.insert(this->data.end(), value.begin(), value.end());
	}
};

class ByteReader {
public:
	explicit ByteReader(std::span<const uint8_t> bytes) : bytes(bytes) {}

	bool U8(uint8_t &value)
	{
		if (this->offset >= this->bytes.size()) return false;
		value = this->bytes[this->offset++];
		return true;
	}
	bool U16(uint16_t &value)
	{
		uint8_t a, b;
		if (!this->U8(a) || !this->U8(b)) return false;
		value = static_cast<uint16_t>(a) | (static_cast<uint16_t>(b) << 8);
		return true;
	}
	bool U32(uint32_t &value)
	{
		value = 0;
		for (uint shift = 0; shift < 32; shift += 8) {
			uint8_t byte;
			if (!this->U8(byte)) return false;
			value |= static_cast<uint32_t>(byte) << shift;
		}
		return true;
	}
	bool U64(uint64_t &value)
	{
		value = 0;
		for (uint shift = 0; shift < 64; shift += 8) {
			uint8_t byte;
			if (!this->U8(byte)) return false;
			value |= static_cast<uint64_t>(byte) << shift;
		}
		return true;
	}
	bool I32(int32_t &value)
	{
		uint32_t raw;
		if (!this->U32(raw)) return false;
		value = std::bit_cast<int32_t>(raw);
		return true;
	}
	bool I64(int64_t &value)
	{
		uint64_t raw;
		if (!this->U64(raw)) return false;
		value = std::bit_cast<int64_t>(raw);
		return true;
	}

	template <size_t N>
	bool Bytes(std::array<uint8_t, N> &value)
	{
		if (this->offset + N > this->bytes.size()) return false;
		std::copy_n(this->bytes.begin() + this->offset, N, value.begin());
		this->offset += N;
		return true;
	}

	size_t Remaining() const { return this->bytes.size() - this->offset; }

private:
	std::span<const uint8_t> bytes;
	size_t offset = 0;
};

static uint32_t SnapshotChecksum(std::span<const uint8_t> bytes)
{
	uint32_t crc = 0xFFFFFFFFU;
	for (uint8_t byte : bytes) {
		crc ^= byte;
		for (uint bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

static bool IsSnapshotValid(const ConsistSnapshot &snapshot)
{
	if (!snapshot.consist_id.IsValid() || snapshot.units.empty() || snapshot.units.size() > CONSIST_SNAPSHOT_MAX_UNITS) return false;
	if (snapshot.direction >= to_underlying(Direction::End)) return false;
	for (const ConsistSnapshotUnit &unit : snapshot.units) {
		if (unit.engine_type == EngineID::Invalid().base()) return false;
		if (unit.cargo_type >= NUM_CARGO) return false;
		if (unit.cargo_count > unit.cargo_capacity) return false;
		if (unit.cargo_provenance_unresolved != (unit.cargo_count != 0)) return false;
	}
	return true;
}

} // namespace

ConsistSnapshotResult ConsistSnapshotCodec::Capture(const Train *train, const ContentManifestToken &manifest, const GlobalOwnerToken &owner)
{
	if (train == nullptr || train->First() != train || !train->IsFrontEngine()) return {ConsistSnapshotError::InvalidConsist, std::nullopt};

	std::optional<GlobalConsistID> identity = FederationIdentityRegistry::GetOrCreate(train);
	if (!identity.has_value()) return {ConsistSnapshotError::InvalidIdentity, std::nullopt};

	ConsistSnapshot snapshot;
	snapshot.content_manifest = manifest;
	snapshot.consist_id = *identity;
	snapshot.owner = owner;
	snapshot.direction = to_underlying(train->direction);
	snapshot.speed = train->cur_speed;
	snapshot.subspeed = train->subspeed;
	snapshot.acceleration = train->acceleration;
	snapshot.stopped = train->vehstatus.Test(VehState::Stopped);
	snapshot.driving_backwards = train->vehicle_flags.Test(VehicleFlag::DrivingBackwards);

	for (const Train *unit = train; unit != nullptr; unit = unit->Next()) {
		if (snapshot.units.size() == CONSIST_SNAPSHOT_MAX_UNITS) return {ConsistSnapshotError::TooManyUnits, std::nullopt};
		if (unit->engine_type == EngineID::Invalid() || !IsValidCargoType(unit->cargo_type)) {
			return {ConsistSnapshotError::InvalidConsist, std::nullopt};
		}
		snapshot.units.push_back({
			.engine_type = unit->engine_type.base(),
			.subtype = unit->subtype,
			.cargo_type = to_underlying(unit->cargo_type),
			.cargo_subtype = unit->cargo_subtype,
			.cargo_capacity = unit->cargo_cap,
			.refit_capacity = unit->refit_cap,
			.cargo_count = unit->cargo.StoredCount(),
			.build_year = unit->build_year.base(),
			.age = unit->age.base(),
			.max_age = unit->max_age.base(),
			.value = unit->value.base(),
			.reliability = unit->reliability,
			.reliability_speed_decrease = unit->reliability_spd_dec,
			.breakdown_counter = unit->breakdown_ctr,
			.breakdown_delay = unit->breakdown_delay,
			.breakdowns_since_service = unit->breakdowns_since_last_service,
			.breakdown_chance = unit->breakdown_chance,
			.random_bits = unit->random_bits,
			.cargo_provenance_unresolved = unit->cargo.StoredCount() != 0,
		});
	}

	if (!IsSnapshotValid(snapshot)) return {ConsistSnapshotError::InvalidConsist, std::nullopt};
	return {ConsistSnapshotError::None, std::move(snapshot)};
}

ConsistSnapshotResult ConsistSnapshotCodec::CaptureForCurrentContent(const Train *train, const GlobalOwnerToken &owner)
{
	ContentManifestResult current = ContentManifestCodec::CaptureCurrent();
	if (!current.Succeeded()) return {ConsistSnapshotError::ManifestUnavailable, std::nullopt};
	ContentManifestTokenResult token = ContentManifestCodec::Digest(*current.manifest);
	if (!token.Succeeded()) return {ConsistSnapshotError::ManifestUnavailable, std::nullopt};
	return Capture(train, token.token, owner);
}

ConsistSnapshotBytes ConsistSnapshotCodec::Encode(const ConsistSnapshot &snapshot)
{
	if (snapshot.units.size() > CONSIST_SNAPSHOT_MAX_UNITS) return {ConsistSnapshotError::TooManyUnits, {}};
	if (!IsSnapshotValid(snapshot)) return {ConsistSnapshotError::InvalidField, {}};

	ByteWriter writer;
	for (uint8_t byte : SNAPSHOT_MAGIC) writer.U8(byte);
	writer.U16(CONSIST_SNAPSHOT_VERSION);
	writer.U16(0);
	writer.U32(0); // Filled after the payload is complete.
	writer.Bytes(snapshot.content_manifest);
	writer.U64(snapshot.consist_id.name_space.high);
	writer.U64(snapshot.consist_id.name_space.low);
	writer.U64(snapshot.consist_id.sequence);
	writer.Bytes(snapshot.owner);
	writer.U8(snapshot.direction);
	uint8_t flags = (snapshot.stopped ? 1 : 0) | (snapshot.driving_backwards ? 2 : 0);
	writer.U8(flags);
	writer.U16(snapshot.speed);
	writer.U8(snapshot.subspeed);
	writer.U8(snapshot.acceleration);
	writer.U16(static_cast<uint16_t>(snapshot.units.size()));

	for (const ConsistSnapshotUnit &unit : snapshot.units) {
		writer.U16(unit.engine_type);
		writer.U8(unit.subtype);
		writer.U8(unit.cargo_type);
		writer.U8(unit.cargo_subtype);
		writer.U16(unit.cargo_capacity);
		writer.U16(unit.refit_capacity);
		writer.U32(unit.cargo_count);
		writer.I32(unit.build_year);
		writer.I32(unit.age);
		writer.I32(unit.max_age);
		writer.I64(unit.value);
		writer.U16(unit.reliability);
		writer.U16(unit.reliability_speed_decrease);
		writer.U8(unit.breakdown_counter);
		writer.U8(unit.breakdown_delay);
		writer.U8(unit.breakdowns_since_service);
		writer.U8(unit.breakdown_chance);
		writer.U16(unit.random_bits);
		writer.U8(unit.cargo_provenance_unresolved ? 1 : 0);
	}

	if (writer.data.size() + sizeof(uint32_t) > CONSIST_SNAPSHOT_MAX_BYTES) return {ConsistSnapshotError::TooLarge, {}};
	uint32_t total_size = static_cast<uint32_t>(writer.data.size() + sizeof(uint32_t));
	for (uint shift = 0; shift < 32; shift += 8) writer.data[SNAPSHOT_LENGTH_OFFSET + shift / 8] = static_cast<uint8_t>(total_size >> shift);
	writer.U32(SnapshotChecksum(writer.data));
	return {ConsistSnapshotError::None, std::move(writer.data)};
}

ConsistSnapshotResult ConsistSnapshotCodec::Decode(std::span<const uint8_t> bytes, const ContentManifestToken &expected_manifest)
{
	if (bytes.size() > CONSIST_SNAPSHOT_MAX_BYTES) return {ConsistSnapshotError::TooLarge, std::nullopt};
	if (bytes.size() < 16) return {ConsistSnapshotError::Truncated, std::nullopt};

	uint32_t stored_checksum = 0;
	for (uint shift = 0; shift < 32; shift += 8) stored_checksum |= static_cast<uint32_t>(bytes[bytes.size() - 4 + shift / 8]) << shift;
	if (SnapshotChecksum(bytes.first(bytes.size() - 4)) != stored_checksum) return {ConsistSnapshotError::ChecksumMismatch, std::nullopt};

	ByteReader reader(bytes.first(bytes.size() - 4));
	std::array<uint8_t, 4> magic{};
	uint16_t version, reserved;
	uint32_t total_size;
	if (!reader.Bytes(magic) || !reader.U16(version) || !reader.U16(reserved) || !reader.U32(total_size)) {
		return {ConsistSnapshotError::Truncated, std::nullopt};
	}
	if (magic != SNAPSHOT_MAGIC) return {ConsistSnapshotError::InvalidMagic, std::nullopt};
	if (version != CONSIST_SNAPSHOT_VERSION) return {ConsistSnapshotError::UnsupportedVersion, std::nullopt};
	if (reserved != 0) return {ConsistSnapshotError::InvalidField, std::nullopt};
	if (total_size != bytes.size()) return {ConsistSnapshotError::LengthMismatch, std::nullopt};

	ConsistSnapshot snapshot;
	uint8_t flags;
	uint16_t unit_count;
	if (!reader.Bytes(snapshot.content_manifest) ||
			!reader.U64(snapshot.consist_id.name_space.high) || !reader.U64(snapshot.consist_id.name_space.low) ||
			!reader.U64(snapshot.consist_id.sequence) || !reader.Bytes(snapshot.owner) ||
			!reader.U8(snapshot.direction) || !reader.U8(flags) || !reader.U16(snapshot.speed) ||
			!reader.U8(snapshot.subspeed) || !reader.U8(snapshot.acceleration) || !reader.U16(unit_count)) {
		return {ConsistSnapshotError::Truncated, std::nullopt};
	}
	if (snapshot.content_manifest != expected_manifest) return {ConsistSnapshotError::ManifestMismatch, std::nullopt};
	if ((flags & ~3U) != 0 || unit_count == 0 || unit_count > CONSIST_SNAPSHOT_MAX_UNITS) return {ConsistSnapshotError::InvalidField, std::nullopt};
	snapshot.stopped = (flags & 1) != 0;
	snapshot.driving_backwards = (flags & 2) != 0;
	snapshot.units.reserve(unit_count);

	for (uint i = 0; i < unit_count; ++i) {
		ConsistSnapshotUnit unit;
		uint8_t unresolved;
		if (!reader.U16(unit.engine_type) || !reader.U8(unit.subtype) || !reader.U8(unit.cargo_type) ||
				!reader.U8(unit.cargo_subtype) || !reader.U16(unit.cargo_capacity) || !reader.U16(unit.refit_capacity) ||
				!reader.U32(unit.cargo_count) || !reader.I32(unit.build_year) || !reader.I32(unit.age) ||
				!reader.I32(unit.max_age) || !reader.I64(unit.value) || !reader.U16(unit.reliability) ||
				!reader.U16(unit.reliability_speed_decrease) || !reader.U8(unit.breakdown_counter) ||
				!reader.U8(unit.breakdown_delay) || !reader.U8(unit.breakdowns_since_service) ||
				!reader.U8(unit.breakdown_chance) || !reader.U16(unit.random_bits) || !reader.U8(unresolved)) {
			return {ConsistSnapshotError::Truncated, std::nullopt};
		}
		if (unresolved > 1) return {ConsistSnapshotError::InvalidField, std::nullopt};
		unit.cargo_provenance_unresolved = unresolved != 0;
		snapshot.units.push_back(unit);
	}

	if (reader.Remaining() != 0) return {ConsistSnapshotError::LengthMismatch, std::nullopt};
	if (!IsSnapshotValid(snapshot)) return {ConsistSnapshotError::InvalidField, std::nullopt};
	return {ConsistSnapshotError::None, std::move(snapshot)};
}

ConsistSnapshotResult ConsistSnapshotCodec::DecodeForCurrentContent(std::span<const uint8_t> bytes)
{
	ContentManifestResult current = ContentManifestCodec::CaptureCurrent();
	if (!current.Succeeded()) return {ConsistSnapshotError::ManifestUnavailable, std::nullopt};
	ContentManifestTokenResult token = ContentManifestCodec::Digest(*current.manifest);
	if (!token.Succeeded()) return {ConsistSnapshotError::ManifestUnavailable, std::nullopt};
	return Decode(bytes, token.token);
}
