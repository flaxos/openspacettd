/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file freight_corridor_widget.h Types related to the freight corridor monitor widgets. */

#ifndef WIDGETS_FREIGHT_CORRIDOR_WIDGET_H
#define WIDGETS_FREIGHT_CORRIDOR_WIDGET_H

/** Widgets of the #FreightCorridorMonitorWindow class. */
enum FreightCorridorWidgets : WidgetID {
	WID_FCM_CAPTION,        ///< Window caption.
	WID_FCM_SUMMARY_PANEL,  ///< Empire supply chain and transit totals summary panel.
	WID_FCM_CORRIDOR_LIST,   ///< Main list panel drawing active routes, utilization, and congestion.
	WID_FCM_SCROLLBAR,       ///< Scrollbar for corridor list.
	WID_FCM_TELEMETRY_PANEL, ///< Live telemetry panel for active consists on selected corridor.
	WID_FCM_LOCATE_GATE,     ///< Button to scroll viewport to source gate.
};

#endif /* WIDGETS_FREIGHT_CORRIDOR_WIDGET_H */
