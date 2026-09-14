/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file trade_ledger_widget.h Types related to the supply chain matrix and trade ledger widgets. */

#ifndef WIDGETS_TRADE_LEDGER_WIDGET_H
#define WIDGETS_TRADE_LEDGER_WIDGET_H

/** Widgets of the #TradeLedgerWindow class. */
enum TradeLedgerWidgets : WidgetID {
	WID_TL_CAPTION,          ///< Window caption.
	WID_TL_TAB_SUPPLY_CHAIN, ///< Switch to Supply Chain Matrix view.
	WID_TL_TAB_TRADE_LEDGER,  ///< Switch to Trade Balances & Commodity Conservation view.
	WID_TL_HEADER_PANEL,     ///< Macro summary indicators panel.
	WID_TL_MAIN_PANEL,       ///< Primary data table/panel.
	WID_TL_SCROLLBAR,        ///< Scrollbar for lists.
	WID_TL_DETAILS_PANEL,    ///< Detailed breakdown/audit panel.
	WID_TL_REFRESH,          ///< Refresh metrics button.
};

#endif /* WIDGETS_TRADE_LEDGER_WIDGET_H */
