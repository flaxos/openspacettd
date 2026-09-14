/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_directory_widget.h Types related to the universe directory browser widgets. */

#ifndef WIDGETS_UNIVERSE_DIRECTORY_WIDGET_H
#define WIDGETS_UNIVERSE_DIRECTORY_WIDGET_H

/** Widgets of the #UniverseDirectoryWindow class. */
enum UniverseDirectoryWidgets : WidgetID {
	WID_UD_CAPTION,        ///< Window caption.
	WID_UD_HEADER_PANEL,   ///< Top summary stats panel.
	WID_UD_WORLD_LIST,     ///< Main list panel drawing registered world servers.
	WID_UD_SCROLLBAR,      ///< Scrollbar for world list.
	WID_UD_DETAILS_PANEL,  ///< Detail panel for selected world server.
	WID_UD_REFRESH,        ///< Refresh directory button.
	WID_UD_JUMP_BTN,       ///< Jump main viewport to selected world.
	WID_UD_COLONIZE_BTN,   ///< Establish colonial outpost on selected wilderness world.
	WID_UD_PROMOTE_BTN,    ///< Promote world to next development tier when threshold is met.
	WID_UD_MEGACITY_BTN,   ///< Open Megacity Overview for Phase 1 Core Worlds.
};

#endif /* WIDGETS_UNIVERSE_DIRECTORY_WIDGET_H */
