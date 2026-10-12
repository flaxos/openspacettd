/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_permanent_line.h Bounded, read-only-observed mission evidence. */
#ifndef FIRST_PERMANENT_LINE_H
#define FIRST_PERMANENT_LINE_H

#include "../cargo_type.h"
#include "../company_type.h"
#include "../industry_type.h"
#include "../station_type.h"
#include "../town_type.h"
#include "../vehicle_type.h"
#include "../tile_type.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>

struct FirstPermanentLineBindings {
	CompanyID company = CompanyID::Invalid();
	TownID mito = TownID::Invalid();
	IndustryID steel_works = IndustryID::Invalid();
	IndustryID ballast_works = IndustryID::Invalid();
	IndustryID iron_source = IndustryID::Invalid();
	IndustryID silica_source = IndustryID::Invalid();
};

/** One train's evidence, bounded by the native vehicle pool and pruned on removal. */
struct FirstPermanentLineJourney {
	uint8_t phase = 0; ///< 0: none; 1: first deposit; 2: later source call; 3: later deposit.
	StationID last_arrival = StationID::Invalid();
	uint32_t visit = 0;
	uint32_t last_deposit_visit = 0;
};

struct FirstPermanentLineState {
	bool active = false;
	uint32_t economy_evaluations = 0;
	bool invalid = false;
	bool awarded = false;
	std::string reason;
	FirstPermanentLineBindings binding{};
	std::array<bool, 2> input_delivered{};
	std::array<bool, 2> produced_after_input{};
	std::array<uint32_t, 2> stored_units{};
	bool commissioned = false;
	std::array<TileIndex, 4> houses{INVALID_TILE, INVALID_TILE, INVALID_TILE, INVALID_TILE};
	std::array<bool, 4> house_completed{};
	uint8_t house_count = 0;
	std::map<VehicleID, FirstPermanentLineJourney> journeys;
};

class FirstPermanentLine {
public:
	static void Reset();
	static bool Active();
	/** Begin only on an empty, valid native baseline. Rejection preserves the old observer. */
	static bool Begin(const FirstPermanentLineBindings &binding);
	/** Pure snapshot; never advances, repairs, or awards a mission. */
	static const FirstPermanentLineState &State();
	static std::string Save();
	/** Missing chunk resets inactive. Malformed/unsupported chunks preserve prior state. */
	static bool Load(const std::string &data);
	static bool ValidateAfterLoad();
	static void OnArrival(VehicleID vehicle, StationID station);
	static void OnIndustryDelivery(VehicleID vehicle, CompanyID company, IndustryID source, IndustryID destination, CargoType cargo, uint32_t units);
	static void OnStoredDelivery(VehicleID vehicle, CompanyID company, IndustryID source, StationID destination, CargoType cargo, uint32_t units);
	static void OnProduction(IndustryID industry, uint32_t batches);
	/** Call once at the start of each enabled native IntegratedEconomy::Produce. */
	static void OnEconomyEvaluation();
	/** Call only after a successful native commissioning command has consumed its BOM. */
	static void OnCommission(CompanyID company, TownID town);
	/** Call only after native placement of a protected house. */
	static void OnHousePlaced(TownID town, TileIndex tile);
	static void OnHouseCompleted(TileIndex tile);
	/** Call before native demolition of any tile in a registered house footprint. */
	static void OnHouseRemoved(TileIndex tile);
	static void OnVehicleRemoved(VehicleID vehicle);
	/** Deterministic simulation-event evaluation. Queries never call this. */
	static void Evaluate();
};

#endif
