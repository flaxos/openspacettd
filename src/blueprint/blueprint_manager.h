/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint_manager.h Central repository, disk persistence, and area capture for blueprints. */

#ifndef BLUEPRINT_MANAGER_H
#define BLUEPRINT_MANAGER_H

#include "blueprint.h"
#include "../tile_type.h"
#include <vector>
#include <string>
#include <optional>

class BlueprintManager {
public:
	/** Initialize the manager, register built-ins, and scan disk storage. */
	static void Initialize();

	/** Rescan the blueprint directory on disk. */
	static void RescanLibrary();

	/** Retrieve all currently available blueprints (built-in and player-saved). */
	static const std::vector<Blueprint> &GetBlueprints();

	/** Retrieve a specific blueprint by index. */
	static const Blueprint *GetBlueprint(size_t index);

	/** Save or update a player blueprint to disk. */
	static bool SaveBlueprint(const Blueprint &bp);

	/** Delete a player blueprint by index (built-ins cannot be deleted). */
	static bool DeleteBlueprint(size_t index);

	/** Rename a player blueprint by index. */
	static bool RenameBlueprint(size_t index, const std::string &new_name);

	/** Capture the rail infrastructure within the bounding box between start_tile and end_tile. */
	static std::optional<Blueprint> CaptureArea(TileIndex start_tile, TileIndex end_tile, const std::string &name = "");

	/** Export a blueprint as a raw JSON string for clipboard sharing. */
	static std::string ExportToString(size_t index);

	/** Import a blueprint from a raw JSON string and save to library. */
	static bool ImportFromString(const std::string &json_data, std::string *error_msg = nullptr);

	/** Register a built-in read-only prefab blueprint. */
	static void RegisterBuiltin(const Blueprint &bp);

	/** Clear all loaded blueprints (for testing or reset). */
	static void Reset();
};

#endif /* BLUEPRINT_MANAGER_H */
