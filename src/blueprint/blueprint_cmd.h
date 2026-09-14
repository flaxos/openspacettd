/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint_cmd.h Deterministic command declarations for blueprint placement. */

#ifndef BLUEPRINT_CMD_H
#define BLUEPRINT_CMD_H

#include "../command_type.h"
#include "../rail_type.h"
#include <string>

/**
 * Deterministically validate or stamp a player rail blueprint onto the map.
 *
 * @param flags Command flags (Execute, Auto, QueryCost, etc.).
 * @param origin_tile Map tile corresponding to the blueprint's (0, 0) top-left anchor.
 * @param blueprint_json Serialized JSON representation of the blueprint.
 * @param railtype_override RailType to force, or INVALID_RAILTYPE to retain blueprint's recorded railtypes.
 * @param clear_first Whether to clear non-rail obstacles before construction.
 * @return Total construction and clearing cost, or failure reason.
 */
CommandCost CmdPlaceBlueprint(DoCommandFlags flags, TileIndex origin_tile, const std::string &blueprint_json, RailType railtype_override, bool clear_first);

DEF_CMD_TRAIT(Commands::PlaceBlueprint, CmdPlaceBlueprint, CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)

#endif /* BLUEPRINT_CMD_H */
