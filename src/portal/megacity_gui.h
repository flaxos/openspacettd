/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file megacity_gui.h Megacity overview window and management GUI for Phase F4. */

#ifndef MEGACITY_GUI_H
#define MEGACITY_GUI_H

#include "../town_type.h"
#include "../window_type.h"

/**
 * Open or raise the Megacity Overview window.
 *
 * @param town_id Town to focus on, or TownID::Invalid() to show the first registered megacity.
 */
void ShowMegacityOverview(TownID town_id = TownID::Invalid());

#endif /* MEGACITY_GUI_H */
