/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file town_cmd.h Command definitions related to towns. */

#ifndef TOWN_CMD_H
#define TOWN_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "town.h"
#include "town_type.h"

enum class TownAcceptanceEffect : uint8_t;
using HouseID = uint16_t;

std::tuple<CommandCost, Money, TownID> CmdFoundTown(DoCommandFlags flags, TileIndex tile, TownSize size, bool city, TownLayout layout, bool random_location, uint32_t townnameparts, const std::string &text);
CommandCost CheckFrontierTownSite(TileIndex tile, std::string_view name);
Town *FoundFrontierTownAtSite(TileIndex tile, std::string_view name);

/** Outcome of the bounded first-Core-town search in an ordinary integrated new game. */
enum class IntegratedCoreTownFailure : uint8_t {
	None, ///< The search succeeded, or was not applicable.
	NoCoreRegion, ///< No initialized economic Core region exists.
	NameUnavailable, ///< Native town-name generation failed.
	TownPoolFull, ///< The native town pool cannot allocate a candidate.
	ProbeBudget, ///< All 10,000 distinct aligned center probes were consumed.
	CreationBudget, ///< All 20 native creation attempts were consumed.
	SitesExhausted, ///< All aligned centers were visited without a qualifying town.
	CleanupFailed, ///< Native deletion rejected a failed candidate; generation must stop.
};

/** Read-only, nonpersistent diagnostics for the most recent GenerateTowns call. */
struct IntegratedCoreTownGenerationStats {
	bool active = false; ///< Whether that call used the integrated ordinary-new-game contract.
	uint target = 0; ///< Native global target after density scaling and pool clamping.
	uint city_offset = 0; ///< The single native city-frequency offset for the call.
	uint probes = 0; ///< Distinct aligned final centers charged before validation.
	uint coastal_probes = 0; ///< Charged centers considered by native-sized coastal relocation.
	uint creation_attempts = 0; ///< Calls to native DoCreateTown for the reserved Core slot.
	uint deleted_candidates = 0; ///< Failed candidates removed by native DeleteTown.
	uint zero_population_candidates = 0; ///< Failed native candidates without positive population.
	uint no_core_house_candidates = 0; ///< Populated candidates without an own house in their center's Core world.
	uint64_t probe_hash = 1469598103934665603ULL; ///< Ordered fingerprint of charged tile indices, for deterministic evidence.
	TownID core_town = TownID::Invalid(); ///< The successful reserved town, if any.
	IntegratedCoreTownFailure failure = IntegratedCoreTownFailure::None; ///< Bounded search failure reason.
};

/**
 * Clear transient town-generation diagnostics when initializing a game or starting town generation.
 * This does not change native gameplay or persistent save state.
 */
void ResetIntegratedCoreTownGenerationStats();

/**
 * Retrieve town-generation diagnostics for the current initialized game, without saved-state mutation.
 * @return Nonpersistent diagnostics, reset at game initialization and never restored from a save.
 */
const IntegratedCoreTownGenerationStats &GetIntegratedCoreTownGenerationStats();

/**
 * Check native town/house state against the integrated initial Core-town predicate.
 * @return true if a positive-population town has a Core center and an own house in that same Core world.
 */
bool HasValidIntegratedCoreTown();
CommandCost CmdRenameTown(DoCommandFlags flags, TownID town_id, const std::string &text);
CommandCost CmdDoTownAction(DoCommandFlags flags, TownID town_id, TownAction action);
CommandCost CmdTownGrowthRate(DoCommandFlags flags, TownID town_id, uint16_t growth_rate);
CommandCost CmdTownRating(DoCommandFlags flags, TownID town_id, CompanyID company_id, int16_t rating);
CommandCost CmdTownCargoGoal(DoCommandFlags flags, TownID town_id, TownAcceptanceEffect tae, uint32_t goal);
CommandCost CmdTownSetText(DoCommandFlags flags, TownID town_id, const EncodedString &text);
CommandCost CmdExpandTown(DoCommandFlags flags, TownID town_id, uint32_t grow_amount, TownExpandModes modes);
CommandCost CmdDeleteTown(DoCommandFlags flags, TownID town_id);
CommandCost CmdPlaceHouse(DoCommandFlags flags, TileIndex tile, HouseID house, bool house_protected, bool replace);
CommandCost CmdPlaceHouseArea(DoCommandFlags flags, TileIndex tile, TileIndex start_tile, HouseID house, bool is_protected, bool replace, bool diagonal);

DEF_CMD_TRAIT(Commands::FoundTown, CmdFoundTown, CommandFlags({CommandFlag::Deity, CommandFlag::NoTest}), CommandType::LandscapeConstruction) // founding random town can fail only in exec run
DEF_CMD_TRAIT(Commands::RenameTown, CmdRenameTown, CommandFlags({CommandFlag::Deity, CommandFlag::Server}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::TownAction, CmdDoTownAction, CommandFlags({CommandFlag::Location}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::TownCargoGoal, CmdTownCargoGoal, CommandFlags({CommandFlag::Deity}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::TownGrowthRate, CmdTownGrowthRate, CommandFlags({CommandFlag::Deity}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::TownRating, CmdTownRating, CommandFlags({CommandFlag::Deity}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::TownSetText, CmdTownSetText, CommandFlags({CommandFlag::Deity, CommandFlag::StrCtrl}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::ExpandTown, CmdExpandTown, CommandFlags({CommandFlag::Deity}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::DeleteTown, CmdDeleteTown, CommandFlags({CommandFlag::Offline}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::PlaceHouse, CmdPlaceHouse, CommandFlags({CommandFlag::Deity}), CommandType::OtherManagement)
DEF_CMD_TRAIT(Commands::PlaceHouseArea, CmdPlaceHouseArea, CommandFlags({ CommandFlag::Deity }), CommandType::OtherManagement)


CommandCallback CcFoundTown;
void CcFoundRandomTown(Commands cmd, const CommandCost &result, Money, TownID town_id);

#endif /* TOWN_CMD_H */
