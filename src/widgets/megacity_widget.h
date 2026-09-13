/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file megacity_widget.h Types related to the megacity overview widgets. */

#ifndef WIDGETS_MEGACITY_WIDGET_H
#define WIDGETS_MEGACITY_WIDGET_H

/** Widgets of the #MegacityOverviewWindow class. */
enum MegacityOverviewWidgets : WidgetID {
	WID_MCO_CAPTION,       ///< Window caption.
	WID_MCO_HEADER_PANEL,  ///< General town, world, and growth stage header panel.
	WID_MCO_PREV_CITY,     ///< Select previous registered megacity.
	WID_MCO_NEXT_CITY,     ///< Select next registered megacity.
	WID_MCO_LOCATE,        ///< Scroll viewport to selected megacity.
	WID_MCO_DESIGNATE,     ///< Designate current town as megacity if not registered.
	WID_MCO_TIER1_PANEL,   ///< Progress panel for Tier 1 Sustenance commodities.
	WID_MCO_TIER2_PANEL,   ///< Progress panel for Tier 2 Expansion commodities.
	WID_MCO_TIER3_PANEL,   ///< Progress panel for Tier 3 Prosperity commodities.
	WID_MCO_STATUS_BAR,    ///< Bottom status text summary.
};

#endif /* WIDGETS_MEGACITY_WIDGET_H */
