/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file world0_opening.h Saved Aster Vale identity and native freight evidence. */
#ifndef WORLD0_OPENING_H
#define WORLD0_OPENING_H

#include "../cargo_type.h"
#include "../company_type.h"
#include "../industry_type.h"
#include "../station_type.h"
#include "../tile_type.h"
#include "../town_type.h"
#include "../vehicle_type.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>

struct World0OpeningProfile {
	CompanyID company = CompanyID::Invalid();
	std::array<TownID, 3> towns{TownID::Invalid(), TownID::Invalid(), TownID::Invalid()};
	TileIndex crusher_plot = INVALID_TILE;
	uint32_t quarry_site = 0;
};

struct World0OpeningBindings {
	IndustryID quarry = IndustryID::Invalid();
	IndustryID crusher = IndustryID::Invalid();
	StationID yard = StationID::Invalid();
};

struct World0OpeningJourney {
	uint8_t phase = 0; ///< 0 none, 1 first deposit, 2 later source call, 3 later deposit.
	StationID last_arrival = StationID::Invalid();
	uint32_t visit = 0;
	uint32_t last_deposit_visit = 0;
};

struct World0OpeningState {
	bool ready = false; ///< Transient authoring permit; never saved without full bindings.
	bool active = false;
	bool unavailable = false;
	bool awarded = false;
	uint8_t losses = 0; ///< Explicit native deletion bits: quarry 1, crusher 2, yard 4, charter 8.
	std::string reason;
	World0OpeningProfile profile{};
	World0OpeningBindings binding{};
	StationID removed_yard_station = StationID::Invalid(); ///< Historical native hub tombstone, never a live service binding.
	TileIndex removed_yard_tile = INVALID_TILE;
	uint32_t removed_hub_id = 0;
	uint32_t economy_evaluations = 0;
	uint32_t accepted_silc = 0;
	uint32_t qualified_input_remaining = 0;
	uint32_t unqualified_input_remaining = 0;
	uint32_t qualified_batches = 0;
	uint32_t stored_ball = 0;
	bool input_goal = false;
	bool production_goal = false;
	bool storage_goal = false;
	std::map<VehicleID, World0OpeningJourney> journeys;
};

class World0Opening {
public:
	static void Reset();
	static bool Ready();
	static bool Active();
	/** Set one transient authored profile before ordinary crusher construction. */
	static bool BeginAuthoring(const World0OpeningProfile &profile);
	/** Bind existing native industries and yard; only complete bindings can be saved. */
	static bool Begin(const World0OpeningBindings &binding);
	/** Pure, narrowly bound role exception; all other command checks still apply. */
	static bool PermitsCrusher(TileIndex tile, IndustryType type, CompanyID company);
	/** Pure snapshot; never advances or awards progress. */
	static const World0OpeningState &State();
	static std::string Save();
	/** Missing chunk resets inactive. Malformed/unsupported data preserves prior state. */
	static bool Load(const std::string &data);
	/** Validate surviving native references; only explicit removal tombstones allow missing bindings. */
	static bool ValidateAfterLoad();
	static void OnIndustryDelivery(VehicleID vehicle, CompanyID company, IndustryID source, IndustryID destination, CargoType cargo, uint32_t units);
	static void OnProduction(IndustryID industry, uint32_t batches);
	static void OnArrival(VehicleID vehicle, StationID station);
	static void OnStoredDelivery(VehicleID vehicle, CompanyID company, IndustryID source, StationID destination, CargoType cargo, uint32_t units);
	static void OnVehicleRemoved(VehicleID vehicle);
	/** Explicit native removals revoke only the affected binding; stale references are never masked. */
	static void OnIndustryRemoved(IndustryID industry);
	static void OnYardRemoved(StationID station, uint32_t hub_id, TileIndex tile);
	static void OnCompanyChanged(CompanyID old_owner, CompanyID new_owner);
	static void OnEconomyEvaluation();
	/** Simulation event only; queries never evaluate. */
	static void Evaluate();
};

#endif /* WORLD0_OPENING_H */
