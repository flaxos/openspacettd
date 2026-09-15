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
	WID_CHQ_TAB_TECH_TREE,      ///< Commonwealth Tech Tree & R&D projects tab.
	WID_CHQ_HEADER_PANEL,       ///< Corporate campus status header.
	WID_CHQ_MAIN_PANEL,         ///< Main content panel.
	WID_CHQ_SCROLLBAR,          ///< Scrollbar for lists/tables.
	WID_CHQ_LOCATE,             ///< Locate Corporate HQ button.
	WID_CHQ_UPGRADE,            ///< Upgrade Campus Tier button.
	WID_CHQ_FABRICATION_TOGGLE, ///< Toggle Fabrication mode button.
	WID_CHQ_TECH_RESEARCH_BTN,  ///< Select / Start research on project button.
	WID_CHQ_TECH_BUDGET_BTN,    ///< Cycle / Adjust monthly R&D budget button.
	WID_CHQ_STATUS_BAR,         ///< Footer status bar.
	WID_CHQ_BUILD_HQ,           ///< Select a map site for an owned headquarters.
	WID_CHQ_BUILD_HUB,          ///< Select an owned rail station for a logistics hub.
	WID_CHQ_SELECT_HUB,         ///< Choose an owned hub for reserve editing.
	WID_CHQ_SELECT_CARGO,       ///< Choose a cargo for reserve editing.
	WID_CHQ_SET_RESERVE,        ///< Set the selected hub cargo reserve.
};

#endif /* WIDGETS_CORPORATE_HQ_WIDGET_H */
