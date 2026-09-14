/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_auth_widget.h Types related to the federation authentication and corporate charter widgets. */

#ifndef WIDGETS_FEDERATION_AUTH_WIDGET_H
#define WIDGETS_FEDERATION_AUTH_WIDGET_H

/** Widgets of the #FederationAuthWindow class. */
enum FederationAuthWidgets : WidgetID {
	WID_FA_CAPTION,          ///< Window caption.
	WID_FA_SESSION_PANEL,    ///< Active player session, username, GlobalPlayerID, token badge.
	WID_FA_LOGIN_REGISTER,   ///< Log In / Register button.
	WID_FA_CHARTER_LIST,     ///< Scrollable list of corporate charters.
	WID_FA_SCROLLBAR,        ///< Scrollbar for charter list.
	WID_FA_DETAILS_PANEL,    ///< Selected charter details (owner, delegates, active presences, treasury).
	WID_FA_BTN_NEW_CHARTER,  ///< Button: Charter new corporation.
	WID_FA_BTN_ADD_PRESENCE, ///< Button: Register presence on current world.
	WID_FA_BTN_AUTH_DELEGATE,///< Button: Authorize delegate player.
	WID_FA_STATUS_PANEL,     ///< Actionable feedback / error status banner.
};

#endif /* WIDGETS_FEDERATION_AUTH_WIDGET_H */
