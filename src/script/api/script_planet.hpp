/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file script_planet.hpp Read-only facts about logical worlds and company world stockpiles. */
#ifndef SCRIPT_PLANET_HPP
#define SCRIPT_PLANET_HPP
#include "script_company.hpp"
#include "../../cargo_type.h"
/**
 * Read-only facts about the logical worlds of the single global map.
 * Worlds are numbered by their saved world ID; nothing here changes the game.
 * @api game
 */
class ScriptPlanet : public ScriptObject {
public:
	/** @return Number of registered worlds. */
	static SQInteger GetWorldCount();
	/**
	 * @param index Position in world registration order, from 0.
	 * @return Saved world ID, or -1 for an out-of-range index.
	 */
	static SQInteger GetWorldID(SQInteger index);
	/**
	 * @param tile Any map tile.
	 * @return World ID owning the tile, or -1 for void and buffer tiles.
	 */
	static SQInteger GetTileWorld(TileIndex tile);
	/**
	 * @param world World ID.
	 * @return Display name, or no value for an unknown world.
	 */
	static std::optional<std::string> GetName(SQInteger world);
	/**
	 * @param world World ID.
	 * @return Biome: 0 temperate, 1 sub-arctic, 2 sub-tropic, 3 arid desert, 4 volcanic, 5 oceanic; -1 unknown.
	 */
	static SQInteger GetBiome(SQInteger world);
	/**
	 * @param world World ID.
	 * @return Phase: 1 core, 2 developed, 3 frontier, 4 expansion; -1 unknown.
	 */
	static SQInteger GetPhase(SQInteger world);
	/**
	 * @param world World ID.
	 * @param company Stock owner.
	 * @param cargo Cargo type.
	 * @return Units held in the company's stockpile on that world, or -1 for invalid input.
	 */
	static SQInteger GetStock(SQInteger world, ScriptCompany::CompanyID company, CargoType cargo);
};

#endif
