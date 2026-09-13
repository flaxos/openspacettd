/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_identity.h Stable identities for future inter-server consist transfer. */

#ifndef FEDERATION_IDENTITY_H
#define FEDERATION_IDENTITY_H

#include "../vehicle_type.h"

#include <cstdint>
#include <map>
#include <optional>

struct Train;

/** Save-scoped 128-bit namespace. */
struct FederationNamespace {
	uint64_t high = 0;
	uint64_t low = 0;

	bool IsValid() const { return this->high != 0 || this->low != 0; }
	auto operator<=>(const FederationNamespace &) const = default;
};

/** Globally portable identity of one logical train consist. */
struct GlobalConsistID {
	FederationNamespace name_space{};
	uint64_t sequence = 0;

	bool IsValid() const { return this->name_space.IsValid() && this->sequence != 0; }
	auto operator<=>(const GlobalConsistID &) const = default;
};

/**
 * Owns save-scoped consist identities without changing OpenTTD pool IDs.
 *
 * An identity is anchored to the train unit that headed the consist when the
 * identity was allocated. It consequently survives ordinary reordering and
 * follows that unit when a consist is split.
 */
class FederationIdentityRegistry {
public:
	static FederationNamespace GetNamespace();
	static uint64_t GetNextSequence();
	static std::optional<GlobalConsistID> Find(const Train *train);
	static std::optional<GlobalConsistID> GetOrCreate(const Train *train);

	/** Keep the destination identity and retire any other identity after a merge. */
	static void ReconcileConsistChange(const Train *source, const Train *destination,
			std::optional<GlobalConsistID> preferred_destination = std::nullopt);

	/** Remove an identity anchored to a vehicle that is being destroyed. */
	static void ReleaseVehicle(VehicleID vehicle);

	/** Deterministic entries used by save/load. */
	static const std::map<uint32_t, uint64_t> &GetMappings();

	/** Save/load restoration API. */
	static void RestoreState(FederationNamespace name_space, uint64_t next_sequence);
	static bool RestoreMapping(VehicleID anchor, uint64_t sequence);
	static void PruneStaleMappings();

	/** Explicit deterministic namespace construction for new games and tests. */
	static FederationNamespace DeriveNamespace(uint32_t generation_seed, uint32_t map_x, uint32_t map_y, int32_t starting_year);
	static void Reset();

private:
	static void EnsureNamespace();
};

#endif /* FEDERATION_IDENTITY_H */
