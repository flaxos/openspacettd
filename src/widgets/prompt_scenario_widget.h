/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file prompt_scenario_widget.h Widget definitions for the Prompt-to-Savegame Generator window. */

#ifndef WIDGETS_PROMPT_SCENARIO_WIDGET_H
#define WIDGETS_PROMPT_SCENARIO_WIDGET_H

#include "../window_type.h"

/** Widgets of the #PromptScenarioWindow class. */
enum PromptScenarioWidgets : WidgetID {
	WID_PSG_CAPTION,        ///< Window title bar caption.
	WID_PSG_PROMPT_LABEL,   ///< Narrative prompt input instructions.
	WID_PSG_PRESET_1,       ///< Preset 1: Arid Mining Strike vs Greedy Core.
	WID_PSG_PRESET_2,       ///< Preset 2: Glacial Science & Quantum Crystals.
	WID_PSG_PRESET_3,       ///< Preset 3: Volcanic Heavy Forges & Maglev.
	WID_PSG_PROMPT_EDIT,    ///< Text edit box for user narrative prompt.
	WID_PSG_OUTPUT_LABEL,   ///< Output filename label.
	WID_PSG_OUTPUT_EDIT,    ///< Text edit box for output savegame path.
	WID_PSG_ANALYSIS_PANEL, ///< Live narrative extraction analysis panel.
	WID_PSG_GENERATE_BTN,   ///< Synthesize & Generate scenario button.
	WID_PSG_STATUS_PANEL,   ///< Status / completion reporting panel.
};

#endif /* WIDGETS_PROMPT_SCENARIO_WIDGET_H */
