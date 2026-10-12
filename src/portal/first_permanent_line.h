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

/** @brief Native company, town, works, and source identities fixed at mission admission. */
struct FirstPermanentLineBindings {
	CompanyID company = CompanyID::Invalid(); ///< Player company holding the mission charter.
	TownID mito = TownID::Invalid(); ///< Bound Core megacity for commissioning and houses.
	IndustryID steel_works = IndustryID::Invalid(); ///< Player-founded Merredin steel factory.
	IndustryID ballast_works = IndustryID::Invalid(); ///< Player-founded Merredin ballast factory.
	IndustryID iron_source = IndustryID::Invalid(); ///< Bound Clonclurry iron producer.
	IndustryID silica_source = IndustryID::Invalid(); ///< Bound Clonclurry silica producer.
};

/** One train's evidence, bounded by the native vehicle pool and pruned on removal. */
struct FirstPermanentLineJourney {
	uint8_t phase = 0; ///< 0: none; 1: first deposit; 2: later source call; 3: later deposit.
	StationID last_arrival = StationID::Invalid(); ///< Last explicit station visit observed.
	uint32_t visit = 0; ///< Monotonic count of qualified visits by this train.
	uint32_t last_deposit_visit = 0; ///< Visit already credited with a positive deposit.
};

/** @brief Bounded persistent native-event evidence and latched mission milestones. */
struct FirstPermanentLineState {
	bool active = false; ///< Complete bound observer is installed.
	uint32_t economy_evaluations = 0; ///< Actual native economy production invocations.
	bool invalid = false; ///< Irrecoverable mission violation has been latched.
	bool awarded = false; ///< Victory was awarded once by simulation evaluation.
	std::string reason; ///< Stable reason for invalidation.
	FirstPermanentLineBindings binding{}; ///< Native identities admitted at the empty baseline.
	std::array<bool, 2> input_delivered{}; ///< Qualified IRON and SILC factory deliveries.
	std::array<bool, 2> produced_after_input{}; ///< Positive factory production after each input.
	std::array<uint32_t, 2> stored_units{}; ///< Cumulative qualifying STEL and BALL deposits; frozen after award.
	bool commissioned = false; ///< Bound residential district was commissioned natively.
	std::array<TileIndex, 4> houses{INVALID_TILE, INVALID_TILE, INVALID_TILE, INVALID_TILE}; ///< New protected house anchors.
	std::array<bool, 4> house_completed{}; ///< Native completion evidence for each anchor.
	uint8_t house_count = 0; ///< Number of recorded new house anchors.
	std::map<VehicleID, FirstPermanentLineJourney> journeys; ///< Per-train return-service evidence.
};

/** @brief Observe native freight, housing, and train events for one persistent mission. */
class FirstPermanentLine {
public:
	static void Reset();
	static bool Active();
	/** @brief Bind one valid empty native baseline without replacing an existing mission.
	 * @param binding Company, town, factory, and source identities.
	 * @return True when all required native assets and empty stores validate.
	 */
	static bool Begin(const FirstPermanentLineBindings &binding);
	/** @brief Inspect the observer without advancing or awarding it.
	 * @return Current mission state by const reference.
	 */
	static const FirstPermanentLineState &State();
	static std::string Save();
	/** @brief Restore a bounded mission payload transactionally; an absent chunk deactivates it.
	 * @param data Serialized schema-one JSON or an empty string for no mission.
	 * @return True when the payload was accepted without malformed fields.
	 */
	static bool Load(const std::string &data);
	static bool ValidateAfterLoad();
	static void OnArrival(VehicleID vehicle, StationID station);
	static void OnIndustryDelivery(VehicleID vehicle, CompanyID company, IndustryID source, IndustryID destination, CargoType cargo, uint32_t units);
	static void OnStoredDelivery(VehicleID vehicle, CompanyID company, IndustryID source, StationID destination, CargoType cargo, uint32_t units);
	static void OnProduction(IndustryID industry, uint32_t batches);
	/** Call once at the start of each enabled native IntegratedEconomy::Produce. */
	static void OnEconomyEvaluation();
	/** @brief Record a successful native district commission after its bill of materials.
	 * @param company Company issuing the commission.
	 * @param town Commissioned town.
	 */
	static void OnCommission(CompanyID company, TownID town);
	/** @brief Record a newly placed protected house in the bound town.
	 * @param town Town receiving the house.
	 * @param tile Native house anchor.
	 */
	static void OnHousePlaced(TownID town, TileIndex tile);
	static void OnHouseCompleted(TileIndex tile);
	/** @brief Invalidate a registered house before native demolition.
	 * @param tile Demolished house anchor.
	 */
	static void OnHouseRemoved(TileIndex tile);
	static void OnVehicleRemoved(VehicleID vehicle);
	/** Deterministic simulation-event evaluation. Queries never call this. */
	static void Evaluate();
};

#endif
