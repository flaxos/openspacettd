/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_hq_widget.h Types related to Corporate Headquarters & Planetary Stockpile widgets. */

#ifndef WIDGETS_CORPORATE_HQ_WIDGET_H
#define WIDGETS_CORPORATE_HQ_WIDGET_H

/** Widgets of the #CorporateHQWindow class. */
enum CorporateHQWidgets : WidgetID {
	WID_CHQ_CAPTION,            ///< Window caption.
	WID_CHQ_TAB_OVERVIEW,       ///< Overview tab.
	WID_CHQ_TAB_STOCKPILES,     ///< Planetary stockpiles matrix tab.
	WID_CHQ_TAB_LOGISTICS_HUBS, ///< Logistics Hubs tab.
	WID_CHQ_TAB_FABRICATION,    ///< In-Kind Fabrication BOM catalog & toggle tab.
	WID_CHQ_HEADER_PANEL,       ///< Corporate campus status header.
	WID_CHQ_MAIN_PANEL,         ///< Main content panel.
	WID_CHQ_SCROLLBAR,          ///< Scrollbar for lists/tables.
	WID_CHQ_LOCATE,             ///< Locate Corporate HQ button.
	WID_CHQ_UPGRADE,            ///< Upgrade Campus Tier button.
	WID_CHQ_FABRICATION_TOGGLE, ///< Toggle Fabrication mode button.
	WID_CHQ_STATUS_BAR,         ///< Footer status bar.
};

#endif /* WIDGETS_CORPORATE_HQ_WIDGET_H */
