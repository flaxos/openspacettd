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

/** @brief Authored Vale charter and exact one-world construction identities. */
struct World0OpeningProfile {
	CompanyID company = CompanyID::Invalid(); ///< Founding player company.
	std::array<TownID, 3> towns{TownID::Invalid(), TownID::Invalid(), TownID::Invalid()}; ///< Three populated Vale towns.
	TileIndex crusher_plot = INVALID_TILE; ///< Sole permitted ballast-crusher plot.
	uint32_t quarry_site = 0; ///< Authored silica resource-site identifier.
};

/** @brief Complete native quarry, crusher, and yard identities admitted after authoring. */
struct World0OpeningBindings {
	IndustryID quarry = IndustryID::Invalid(); ///< Player-founded silica quarry.
	IndustryID crusher = IndustryID::Invalid(); ///< Player-founded ballast works.
	StationID yard = StationID::Invalid(); ///< Owned Greybank logistics-hub station.
};

/** @brief Bounded per-train evidence for the positive deposit, return, deposit sequence. */
struct World0OpeningJourney {
	uint8_t phase = 0; ///< 0 none, 1 first deposit, 2 later source call, 3 later deposit.
	StationID last_arrival = StationID::Invalid(); ///< Last qualifying explicit station arrival.
	uint32_t visit = 0; ///< Monotonic qualified visit count for this train.
	uint32_t last_deposit_visit = 0; ///< Visit already credited with a positive deposit.
};

/** @brief Persistent Vale identity, custody accounting, losses, and milestones. */
struct World0OpeningState {
	bool ready = false; ///< Transient authoring permit; never saved without full bindings.
	bool active = false; ///< Complete native binding has been installed.
	bool unavailable = false; ///< Native asset or charter loss suspended the mission.
	bool awarded = false; ///< One-time native victory latch.
	uint8_t losses = 0; ///< Explicit native deletion bits: quarry 1, crusher 2, yard 4, charter 8.
	std::string reason; ///< Stable explanation for unavailable free play.
	World0OpeningProfile profile{}; ///< Authored charter and plot identity.
	World0OpeningBindings binding{}; ///< Surviving live asset identifiers.
	StationID removed_yard_station = StationID::Invalid(); ///< Historical native hub tombstone, never a live service binding.
	TileIndex removed_yard_tile = INVALID_TILE; ///< Historical removed-hub tile, not a live binding.
	uint32_t removed_hub_id = 0; ///< Historical removed-hub ID; reuse is permitted.
	uint32_t economy_evaluations = 0; ///< Actual native economy production invocations.
	uint32_t accepted_silc = 0; ///< SILC accepted from the bound quarry by charter trains.
	uint32_t qualified_input_remaining = 0; ///< Qualified SILC still physically in the crusher buffer.
	uint32_t unqualified_input_remaining = 0; ///< Other SILC still physically in the crusher buffer.
	uint32_t qualified_batches = 0; ///< Conservatively attributed native ballast batches.
	uint32_t stored_ball = 0; ///< Cumulative qualifying BALL deposits; frozen after award.
	bool input_goal = false; ///< Qualified SILC delivery threshold reached.
	bool production_goal = false; ///< Qualified ballast production threshold reached.
	bool storage_goal = false; ///< Rail-stored BALL threshold reached.
	std::map<VehicleID, World0OpeningJourney> journeys; ///< Live train journey evidence by vehicle ID.
};

/** @brief Observe native Vale freight and asset lifecycle without granting resources. */
class World0Opening {
public:
	static void Reset();
	static bool Ready();
	static bool Active();
	/** @brief Stage one authored profile before ordinary crusher construction.
	 * @param profile Charter, towns, quarry site, and exact crusher plot.
	 * @return True when the disabled-economy authored geometry is valid.
	 */
	static bool BeginAuthoring(const World0OpeningProfile &profile);
	/** @brief Bind existing native industries and yard after authoring.
	 * @param binding Quarry, crusher, and yard identifiers.
	 * @return True when assets and all relevant native cargo stores start empty.
	 */
	static bool Begin(const World0OpeningBindings &binding);
	/** @brief Check the exact-plot crusher permit without changing native commands.
	 * @param tile Requested crusher anchor.
	 * @param type Requested native industry type.
	 * @param company Requesting company.
	 * @return True for an eligible authored charter's exact plot while no crusher exists.
	 */
	static bool PermitsCrusher(TileIndex tile, IndustryType type, CompanyID company);
	/** @brief Inspect the opening without advancing or awarding progress.
	 * @return Current opening state by const reference.
	 */
	static const World0OpeningState &State();
	static std::string Save();
	/** @brief Restore complete bounded schema-one evidence transactionally.
	 * @param data Serialized JSON, or empty string for a missing chunk.
	 * @return True when the payload was accepted without malformed fields.
	 */
	static bool Load(const std::string &data);
	/** @brief Validate live references and explicit native-removal tombstones after load.
	 * @return True when the restored opening is coherent with native entities.
	 */
	static bool ValidateAfterLoad();
	static void OnIndustryDelivery(VehicleID vehicle, CompanyID company, IndustryID source, IndustryID destination, CargoType cargo, uint32_t units);
	static void OnProduction(IndustryID industry, uint32_t batches);
	static void OnArrival(VehicleID vehicle, StationID station);
	static void OnStoredDelivery(VehicleID vehicle, CompanyID company, IndustryID source, StationID destination, CargoType cargo, uint32_t units);
	static void OnVehicleRemoved(VehicleID vehicle);
	/** @brief Revoke only the bound native industry when it is removed.
	 * @param industry Removed native industry identifier.
	 */
	static void OnIndustryRemoved(IndustryID industry);
	static void OnYardRemoved(StationID station, uint32_t hub_id, TileIndex tile);
	static void OnCompanyChanged(CompanyID old_owner, CompanyID new_owner);
	static void OnEconomyEvaluation();
	/** Simulation event only; queries never evaluate. */
	static void Evaluate();
};

#endif /* WORLD0_OPENING_H */
