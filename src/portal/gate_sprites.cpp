/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file gate_sprites.cpp Implementation of runtime resolver for Commonwealth wormhole gates. */

#include "../stdafx.h"
#include "gate_sprites.h"
#include "../newgrf.h"
#include "../newgrf/newgrf_internal.h"
#include "../spritecache.h"
#include "../debug.h"

void PortalGateSprites::Reset()
{
	_base = 0;
}

bool PortalGateSprites::Load()
{
	_base = 0;
	const GRFFile *file = GetFileByGRFID(COMMONWEALTH_GATES_GRFID);
	if (file == nullptr) {
		Debug(sprite, 2, "PortalGateSprites: Commonwealth gates NewGRF not present");
		return false;
	}

	uint32_t param10 = file->GetParam(10);
	if (param10 == 0 || param10 >= 0xFFFFFF) {
		Debug(sprite, 1, "PortalGateSprites: invalid reserved sprite base {}", param10);
		return false;
	}

	/* Validate that all 16 sprites are present in spritecache */
	for (uint32_t i = 0; i < COMMONWEALTH_GATES_SPRITE_COUNT; ++i) {
		SpriteID spr = param10 + i;
		if (!SpriteExists(spr)) {
			Debug(sprite, 1, "PortalGateSprites: sprite {} missing from cache", spr);
			return false;
		}
	}

	_base = param10;
	Debug(sprite, 2, "PortalGateSprites: activated Commonwealth gates with base sprite {}", _base);
	return true;
}

bool PortalGateSprites::IsActive()
{
	return _base != 0;
}

SpriteID PortalGateSprites::GetBase()
{
	return _base;
}

void PortalGateSprites::SetBaseForTesting(SpriteID base)
{
	_base = base;
}

SpriteID PortalGateSprites::GetSprite(DiagDirection dir, WormholeVisualState state, bool front)
{
	if (_base == 0 || dir >= DiagDirection::End) return 0;

	/* Only ActiveCyanPulse (0..7) and StandbyAmber (8..15) are in the 16-sprite pack */
	uint32_t variant_offset = 0;
	if (state == WormholeVisualState::StandbyAmber) {
		variant_offset = 8;
	} else if (state == WormholeVisualState::ActiveCyanPulse) {
		variant_offset = 0;
	} else {
		/* TransitRipple and QuarantineRed fall back to native tunnels */
		return 0;
	}

	uint32_t dir_offset = to_underlying(dir) * 2;
	uint32_t part_offset = front ? 1 : 0;

	return _base + variant_offset + dir_offset + part_offset;
}
