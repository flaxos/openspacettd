/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file gate_sprites.h Runtime resolver and lifecycle manager for Commonwealth wormhole gates. */

#ifndef GATE_SPRITES_H
#define GATE_SPRITES_H

#include "../gfx_type.h"
#include "../direction_type.h"
#include "../newgrf_type.h"
#include "visual_overhaul.h"

/** The Commonwealth wormhole gate NewGRF ID: "OST\x07" */
static constexpr GrfID COMMONWEALTH_GATES_GRFID{"OST\x07"};

/** Total real sprites provided in the Commonwealth gate NewGRF Action A pack. */
static constexpr uint32_t COMMONWEALTH_GATES_SPRITE_COUNT = 16;

/**
 * Manages runtime gate sprite resolution and NewGRF lifecycle for Commonwealth wormhole gates.
 */
class PortalGateSprites {
public:
	/** Reset the gate sprite base (called on NewGRF reset or sprite cache clear). */
	static void Reset();

	/** Query the loaded Commonwealth gates NewGRF and resolve the allocated base sprite ID. */
	static bool Load();

	/** @return Whether the gate NewGRF is loaded and active with a valid base sprite. */
	static bool IsActive();

	/**
	 * Get the sprite ID for a gate rear or front part.
	 * @param dir The portal orientation (NE, SE, SW, NW).
	 * @param state The current event horizon visual state.
	 * @param front Whether to return the overhead front arch (true) or rear aperture collar (false).
	 * @return The resolved SpriteID, or 0 if inactive/unsupported.
	 */
	static SpriteID GetSprite(DiagDirection dir, WormholeVisualState state, bool front);

	/** @return The resolved base sprite ID (for inspection/testing). */
	static SpriteID GetBase();

	/** Set base sprite manually (for unit testing without full NewGRF activation). */
	static void SetBaseForTesting(SpriteID base);

private:
	static inline SpriteID _base = 0;
};

#endif /* GATE_SPRITES_H */
