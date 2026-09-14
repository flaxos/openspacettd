/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_hq_gui.h Corporate Headquarters and Planetary Stockpile GUI window. */

#ifndef CORPORATE_HQ_GUI_H
#define CORPORATE_HQ_GUI_H

#include "../company_type.h"

/**
 * Open or raise the Corporate Headquarters and Planetary Stockpile window.
 *
 * @param company Company to display (defaults to current/local company).
 */
void ShowCorporateHQ(CompanyID company = CompanyID::Invalid());

#endif /* CORPORATE_HQ_GUI_H */
