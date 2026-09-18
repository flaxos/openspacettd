/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file empire_facilities_widget.h Types related to the Empire Industrial Facilities dashboard widgets. */

#ifndef WIDGETS_EMPIRE_FACILITIES_WIDGET_H
#define WIDGETS_EMPIRE_FACILITIES_WIDGET_H

#include "../window_type.h"

/** Widgets of the #EmpireFacilitiesWindow class. */
enum EmpireFacilitiesWidgets : WidgetID {
	WID_EF_CAPTION,          ///< Window caption.
	WID_EF_TAB_ALL,          ///< Filter: All pipelines.
	WID_EF_TAB_PIPE_A,       ///< Filter: Pipeline A (Structural).
	WID_EF_TAB_PIPE_B,       ///< Filter: Pipeline B (Electronics).
	WID_EF_TAB_PIPE_C,       ///< Filter: Pipeline C (Propulsion).
	WID_EF_TAB_PIPE_D,       ///< Filter: Pipeline D (Data Crystals).
	WID_EF_REFRESH,          ///< Refresh metrics button.
	WID_EF_LIST_PANEL,       ///< Scrollable list of facilities.
	WID_EF_SCROLLBAR,        ///< Vertical scrollbar for facility list.
	WID_EF_SUMMARY_PANEL,    ///< Macro summary metrics panel.
	WID_EF_UPGRADE_ALL_BTN,  ///< Batch upgrade all facilities button.
};

#endif /* WIDGETS_EMPIRE_FACILITIES_WIDGET_H */
