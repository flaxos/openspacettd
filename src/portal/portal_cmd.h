/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file portal_cmd.h Commands related to gateway portal construction, linking, and demolition. */

#ifndef PORTAL_CMD_H
#define PORTAL_CMD_H

#include "../command_type.h"
#include "../direction_type.h"
#include "../rail_type.h"
#include "../station_type.h"
#include "../cargo_type.h"

/**
 * Build a single unlinked portal gate head on a tile.
 *
 * @param flags Command flags.
 * @param tile Map tile to build portal head on.
 * @param dir Direction the portal gate head faces.
 * @param railtype Rail type for the gateway tracks.
 * @return Command cost or failure.
 */
CommandCost CmdBuildPortalGate(DoCommandFlags flags, TileIndex tile, DiagDirection dir, RailType railtype);

/**
 * Link two unlinked portal gates across worlds into an active wormhole link.
 *
 * @param flags Command flags.
 * @param tile_a Entrance tile of gate A.
 * @param tile_b Entrance tile of gate B.
 * @return Command cost or failure.
 */
CommandCost CmdLinkPortalGates(DoCommandFlags flags, TileIndex tile_a, TileIndex tile_b);

/**
 * Build and link two portal gates atomically across worlds.
 *
 * @param flags Command flags.
 * @param tile_a Entrance tile of gate A.
 * @param dir_a Direction of gate A.
 * @param tile_b Entrance tile of gate B.
 * @param dir_b Direction of gate B.
 * @param railtype Rail type for both gates.
 * @return Command cost or failure.
 */
CommandCost CmdBuildPortalPair(DoCommandFlags flags, TileIndex tile_a, DiagDirection dir_a, TileIndex tile_b, DiagDirection dir_b, RailType railtype);

/**
 * Demolish a portal gate head.
 *
 * @param flags Command flags.
 * @param tile Tile of the portal gate to demolish.
 * @param demolish_both If true and the portal is linked, demolishes both ends. If false, demolishes this gate and converts opposite end to unlinked.
 * @return Command cost or failure.
 */
CommandCost CmdDestroyPortalGate(DoCommandFlags flags, TileIndex tile, bool demolish_both);

/**
 * Designate/upgrade an airport station as an interplanetary spaceport.
 *
 * @param flags Command flags.
 * @param station Station ID to designate as spaceport.
 * @return Command cost or failure.
 */
CommandCost CmdDesignateSpaceport(DoCommandFlags flags, StationID station);

/**
 * Build an edge extraction conduit on a world perimeter tile adjacent to void space.
 *
 * @param flags Command flags.
 * @param tile Tile to build conduit on.
 * @param dir Direction the conduit portal/drillhead faces.
 * @param cargo Type of mineral to extract.
 * @param railtype Rail type for direct freight connection.
 * @return Command cost or failure.
 */
CommandCost CmdBuildEdgeConduit(DoCommandFlags flags, TileIndex tile, DiagDirection dir, CargoType cargo, RailType railtype);

/**
 * Demolish an edge extraction conduit.
 *
 * @param flags Command flags.
 * @param tile Tile of the conduit to demolish.
 * @return Command cost or failure.
 */
CommandCost CmdDestroyEdgeConduit(DoCommandFlags flags, TileIndex tile);

/** GUI completion callback for portal gate linking. */
CommandCallback CcPortalLink;

DEF_CMD_TRAIT(Commands::BuildPortalGate,   CmdBuildPortalGate,   CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::LinkPortalGates,  CmdLinkPortalGates,   {},                                                      CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildPortalPair,   CmdBuildPortalPair,   CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::DestroyPortalGate, CmdDestroyPortalGate, CommandFlag::Auto,                                     CommandType::LandscapeConstruction)

DEF_CMD_TRAIT(Commands::DesignateSpaceport, CmdDesignateSpaceport, {},                                                   CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::BuildEdgeConduit,   CmdBuildEdgeConduit,   CommandFlags({CommandFlag::Auto, CommandFlag::NoWater}), CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::DestroyEdgeConduit, CmdDestroyEdgeConduit, CommandFlag::Auto,                                     CommandType::LandscapeConstruction)

#endif /* PORTAL_CMD_H */
