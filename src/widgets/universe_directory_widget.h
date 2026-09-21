/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_directory_widget.h Types related to the universe directory browser widgets. */

#ifndef WIDGETS_UNIVERSE_DIRECTORY_WIDGET_H
#define WIDGETS_UNIVERSE_DIRECTORY_WIDGET_H

/** Widgets of the #UniverseDirectoryWindow class. */
enum UniverseDirectoryWidgets : WidgetID {
	WID_UD_CAPTION,        ///< Window caption (0).
	WID_UD_HEADER_PANEL,   ///< Top summary stats panel (1).
	WID_UD_WORLD_LIST,     ///< Main list panel drawing registered world servers (2).
	WID_UD_SCROLLBAR,      ///< Scrollbar for world list / galaxy map (3).
	WID_UD_DETAILS_PANEL,  ///< Detail panel for selected world server (4).
	WID_UD_REFRESH,        ///< Refresh directory button (5).
	WID_UD_JUMP_BTN,       ///< Jump main viewport to selected world (6).
	WID_UD_COLONIZE_BTN,   ///< Establish colonial outpost on selected wilderness world (7).
	WID_UD_PROMOTE_BTN,    ///< Promote world to next development tier when threshold is met (8).
	WID_UD_MEGACITY_BTN,   ///< Open Megacity Overview for Phase 1 Core Worlds (9).
	WID_UD_TAB_SERVERS,    ///< Tab: Registered World Servers (10).
	WID_UD_TAB_GALAXY_MAP, ///< Tab: Commonwealth 108-World Galaxy Map (11).
	WID_UD_FILTER_ALL,     ///< Galaxy filter: All worlds (12).
	WID_UD_FILTER_P1,      ///< Galaxy filter: Phase 1 Core (13).
	WID_UD_FILTER_P2,      ///< Galaxy filter: Phase 2 Developed (14).
	WID_UD_FILTER_P3,      ///< Galaxy filter: Phase 3 Frontier (15).
	WID_UD_FILTER_P4,      ///< Galaxy filter: Phase 4 Wilderness (16).
	WID_UD_STACK_MAIN,     ///< Stacked container for Server List vs Galaxy Map (17).
	WID_UD_GALAXY_MAP,     ///< Visual tree browser for Commonwealth Galaxy Map (18).
	WID_UD_TRADE_GATE_BTN, ///< Link selected Commonwealth node as trade gateway (19).
};

#endif /* WIDGETS_UNIVERSE_DIRECTORY_WIDGET_H */
