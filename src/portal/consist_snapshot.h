/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file consist_snapshot.h Deterministic transfer envelope for train consists. */

#ifndef CONSIST_SNAPSHOT_H
#define CONSIST_SNAPSHOT_H

#include "content_manifest.h"
#include "federation_identity.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct Train;

static constexpr uint16_t CONSIST_SNAPSHOT_VERSION = 3;
static constexpr size_t CONSIST_SNAPSHOT_MAX_UNITS = 256;
static constexpr size_t CONSIST_SNAPSHOT_MAX_ORDERS = 256;
static constexpr size_t CONSIST_SNAPSHOT_MAX_BYTES = 1024 * 1024;

/** One native packet's quantity, age, payment vector and portable provenance. */
struct ConsistSnapshotPacket {
	uint16_t count = 0;
	uint16_t periods_in_transit = 0;
	int64_t feeder_share = 0;
	int32_t travelled_x = 0;
	int32_t travelled_y = 0;
	uint32_t source_x = UINT32_MAX;
	uint32_t source_y = UINT32_MAX;
	GlobalCargoSourceID source{};
	auto operator<=>(const ConsistSnapshotPacket &) const = default;
};

/** Portable, non-spatial state for one engine, wagon, or articulated part. */
struct ConsistSnapshotUnit {
	uint16_t engine_type = UINT16_MAX;
	uint8_t subtype = 0;
	uint8_t cargo_type = UINT8_MAX;
	uint8_t cargo_subtype = 0;
	uint16_t cargo_capacity = 0;
	uint16_t refit_capacity = 0;
	uint32_t cargo_count = 0;
	int32_t build_year = 0;
	int32_t age = 0;
	int32_t max_age = 0;
	int64_t value = 0;
	uint16_t reliability = 0;
	uint16_t reliability_speed_decrease = 0;
	uint8_t breakdown_counter = 0;
	uint8_t breakdown_delay = 0;
	uint8_t breakdowns_since_service = 0;
	uint8_t breakdown_chance = 0;
	uint16_t random_bits = 0;
	bool cargo_provenance_unresolved = false;
	GlobalCargoSourceID cargo_source{}; ///< Resolved global cargo provenance.
	std::vector<ConsistSnapshotPacket> packets{}; ///< V3 native packet boundaries and payment state.

	auto operator<=>(const ConsistSnapshotUnit &) const = default;
};

/** Version-independent in-memory representation of the consist transfer core. */
struct ConsistSnapshot {
	ContentManifestToken content_manifest{};
	GlobalConsistID consist_id{};
	GlobalOwnerToken owner{};
	GlobalCompanyID company_id{}; ///< Resolved global company identity.
	uint8_t direction = UINT8_MAX;
	uint16_t speed = 0;
	uint8_t subspeed = 0;
	uint8_t acceleration = 0;
	bool stopped = false;
	bool driving_backwards = false;
	std::vector<ConsistSnapshotUnit> units;
	std::vector<GlobalOrderDestinationID> orders{}; ///< Captured portable order destinations.
	std::vector<uint16_t> station_order_flags{};   ///< V3 loading/unloading, non-stop and platform position.
	uint16_t current_order_index = 0;               ///< Active order index at time of transfer.

	auto operator<=>(const ConsistSnapshot &) const = default;
};

enum class ConsistSnapshotError : uint8_t {
	None,
	InvalidConsist,
	InvalidIdentity,
	TooManyUnits,
	TooLarge,
	Truncated,
	InvalidMagic,
	UnsupportedVersion,
	LengthMismatch,
	ChecksumMismatch,
	ManifestMismatch,
	ManifestUnavailable,
	InvalidField,
};

struct ConsistSnapshotResult {
	ConsistSnapshotError error = ConsistSnapshotError::None;
	std::optional<ConsistSnapshot> snapshot;

	bool Succeeded() const { return this->error == ConsistSnapshotError::None && this->snapshot.has_value(); }
};

struct ConsistSnapshotBytes {
	ConsistSnapshotError error = ConsistSnapshotError::None;
	std::vector<uint8_t> bytes;

	bool Succeeded() const { return this->error == ConsistSnapshotError::None; }
};

class ConsistSnapshotCodec {
public:
	/** Capture supported state without changing the train or consuming RNG. */
	static ConsistSnapshotResult Capture(const Train *train, const ContentManifestToken &manifest, const GlobalOwnerToken &owner);
	/** Capture using the strict manifest derived from the active game content. */
	static ConsistSnapshotResult CaptureForCurrentContent(const Train *train, const GlobalOwnerToken &owner);

	/** Encode using the canonical little-endian Sprint 12 wire format. */
	static ConsistSnapshotBytes Encode(const ConsistSnapshot &snapshot);

	/** Decode and validate against the receiver's expected content manifest. */
	static ConsistSnapshotResult Decode(std::span<const uint8_t> bytes, const ContentManifestToken &expected_manifest);
	/** Decode and admit only snapshots matching the active game content. */
	static ConsistSnapshotResult DecodeForCurrentContent(std::span<const uint8_t> bytes);
};

#endif /* CONSIST_SNAPSHOT_H */
