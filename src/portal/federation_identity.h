/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_identity.h Stable identities for future inter-server consist transfer. */

#ifndef FEDERATION_IDENTITY_H
#define FEDERATION_IDENTITY_H

#include "../company_type.h"
#include "../order_type.h"
#include "../source_type.h"
#include "../station_type.h"
#include "../vehicle_type.h"
#include "portal_type.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>

struct Train;

using GlobalOwnerToken = std::array<uint8_t, 16>;

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

/** Globally portable identity of one company / corporate entity across servers. */
struct GlobalCompanyID {
	FederationNamespace name_space{};
	uint64_t sequence = 0;

	bool IsValid() const { return this->name_space.IsValid() && this->sequence != 0; }
	auto operator<=>(const GlobalCompanyID &) const = default;

	GlobalOwnerToken ToOwnerToken() const;
};

/** Globally portable identity of one station or waypoint. */
struct GlobalStationID {
	FederationNamespace name_space{};
	uint64_t sequence = 0;
	WorldID world_id{0};

	bool IsValid() const { return this->name_space.IsValid() && this->sequence != 0; }
	auto operator<=>(const GlobalStationID &) const = default;
};

/** Globally portable provenance of cargo packets in transit across worlds. */
struct GlobalCargoSourceID {
	FederationNamespace name_space{};
	GlobalStationID origin_station{};
	SourceType source_type = SourceType::Industry;
	uint64_t source_sequence = 0;
	WorldID origin_world{0};
	uint32_t origin_tile_x = 0;
	uint32_t origin_tile_y = 0;

	bool IsValid() const
	{
		return this->name_space.IsValid() && (this->origin_station.IsValid() || this->source_sequence != 0);
	}
	auto operator<=>(const GlobalCargoSourceID &) const = default;
};

/** Destination classification for portable orders. */
enum class OrderDestinationType : uint8_t {
	None = 0,
	Station = 1,
	Waypoint = 2,
	Depot = 3,
	PortalGate = 4,
};

/** Globally portable order destination across server and world boundaries. */
struct GlobalOrderDestinationID {
	FederationNamespace name_space{};
	OrderDestinationType type = OrderDestinationType::None;
	GlobalStationID station_id{};
	uint64_t destination_sequence = 0;
	WorldID target_world{0};

	bool IsValid() const
	{
		return this->name_space.IsValid() && this->type != OrderDestinationType::None &&
			(this->station_id.IsValid() || this->destination_sequence != 0);
	}
	auto operator<=>(const GlobalOrderDestinationID &) const = default;

	static GlobalOrderDestinationID ForStation(const GlobalStationID &station, bool is_waypoint = false)
	{
		return GlobalOrderDestinationID{
			.name_space = station.name_space,
			.type = is_waypoint ? OrderDestinationType::Waypoint : OrderDestinationType::Station,
			.station_id = station,
			.destination_sequence = station.sequence,
			.target_world = station.world_id,
		};
	}

	static GlobalOrderDestinationID ForDepot(FederationNamespace ns, uint64_t depot_seq, WorldID world)
	{
		return GlobalOrderDestinationID{
			.name_space = ns,
			.type = OrderDestinationType::Depot,
			.station_id = {},
			.destination_sequence = depot_seq,
			.target_world = world,
		};
	}

	static GlobalOrderDestinationID ForPortalGate(FederationNamespace ns, uint64_t gate_id, WorldID world)
	{
		return GlobalOrderDestinationID{
			.name_space = ns,
			.type = OrderDestinationType::PortalGate,
			.station_id = {},
			.destination_sequence = gate_id,
			.target_world = world,
		};
	}
};

/**
 * Owns save-scoped consist, company, station, source, and order identities
 * without changing OpenTTD pool IDs.
 *
 * Consist identity is anchored to the train unit that headed the consist when the
 * identity was allocated. It consequently survives ordinary reordering and
 * follows that unit when a consist is split.
 */
class FederationIdentityRegistry {
public:
	static FederationNamespace GetNamespace();
	static uint64_t GetNextSequence();
	static uint64_t GetNextCompanySequence();
	static uint64_t GetNextStationSequence();
	static uint64_t GetNextSourceSequence();

	/* Consist identity */
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

	/* Company identity */
	static std::optional<GlobalCompanyID> FindCompany(CompanyID company);
	static std::optional<GlobalCompanyID> GetOrCreateCompany(CompanyID company);
	static void ReleaseCompany(CompanyID company);
	static const std::map<uint8_t, uint64_t> &GetCompanyMappings();
	static bool RestoreCompanyMapping(CompanyID company, uint64_t sequence);
	static void PruneStaleCompanyMappings();

	/* Station identity */
	static std::optional<GlobalStationID> FindStation(StationID station);
	static std::optional<GlobalStationID> GetOrCreateStation(StationID station);
	static void ReleaseStation(StationID station);
	static const std::map<uint32_t, uint64_t> &GetStationMappings();
	static bool RestoreStationMapping(StationID station, uint64_t sequence);
	static void PruneStaleStationMappings();

	/* Cargo source identity */
	static GlobalCargoSourceID CreateCargoSource(StationID first_station, Source source, TileIndex source_xy);
	static const std::map<uint32_t, uint64_t> &GetSourceMappings();
	static bool RestoreSourceMapping(uint32_t source_key, uint64_t sequence);
	static void PruneStaleSourceMappings();

	/* Order destination identity and resolution */
	static std::optional<GlobalOrderDestinationID> FindOrderDestination(DestinationID dest, OrderType order_type);
	static std::optional<GlobalOrderDestinationID> GetOrCreateOrderDestination(DestinationID dest, OrderType order_type);
	static std::optional<StationID> FindStationBySequence(uint64_t sequence);
	static std::optional<StationID> ResolveStation(const GlobalStationID &global_st);
	static std::optional<DestinationID> ResolveOrderDestination(const GlobalOrderDestinationID &order, WorldID current_world);

	/* Consist master schedule management */
	static void SetConsistSchedule(uint64_t consist_seq, std::vector<GlobalOrderDestinationID> schedule);
	static std::optional<std::vector<GlobalOrderDestinationID>> GetConsistSchedule(uint64_t consist_seq);
	static void ClearConsistSchedule(uint64_t consist_seq);

	/* Counters and restoration */
	static void RestoreCounters(uint64_t next_company, uint64_t next_station, uint64_t next_source);

	/** Explicit deterministic namespace construction for new games and tests. */
	static FederationNamespace DeriveNamespace(uint32_t generation_seed, uint32_t map_x, uint32_t map_y, int32_t starting_year);
	static void Reset();

private:
	static void EnsureNamespace();
};

#endif /* FEDERATION_IDENTITY_H */
