/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file prompt_scenario_gui.cpp In-game GUI window for Prompt-to-Savegame narrative scenario synthesis. */

#include "../stdafx.h"
#include "prompt_scenario_gui.h"
#include "prompt_scenario_generator.h"
#include "../widgets/prompt_scenario_widget.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../gfx_func.h"
#include "../fios.h"

#include <string>

static const std::string PRESET_1 = "Generate a 3-world system where an arid mining colony is striking over water shortages while a greedy core world demands superalloys";
static const std::string PRESET_2 = "Generate a 4-world system where a glacial science outpost formats quantum data crystals for high-density core megacities";
static const std::string PRESET_3 = "Generate a 2-world system where a volcanic foundry exports cryo-bogies to an expanding industrial hub";

static constexpr NWidgetPart _nested_prompt_scenario_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen, WID_PSG_CAPTION), SetStringTip(STR_PROMPT_SCENARIO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_PSG_PROMPT_LABEL), SetMinimalSize(700, 32), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_PSG_PRESET_1), SetMinimalSize(220, 22), SetStringTip(STR_PROMPT_SCENARIO_PRESET_1, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_PSG_PRESET_2), SetMinimalSize(230, 22), SetStringTip(STR_PROMPT_SCENARIO_PRESET_2, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_PSG_PRESET_3), SetMinimalSize(230, 22), SetStringTip(STR_PROMPT_SCENARIO_PRESET_3, STR_EMPTY),
		NWidget(NWID_SPACER), SetFill(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_PSG_PROMPT_EDIT), SetMinimalSize(700, 48), SetFill(1, 0), EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_PSG_OUTPUT_LABEL), SetMinimalSize(700, 24), SetFill(1, 0), EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_PSG_OUTPUT_EDIT), SetMinimalSize(700, 26), SetFill(1, 0), EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_PSG_ANALYSIS_PANEL), SetMinimalSize(700, 160), SetFill(1, 1), SetResize(1, 1), EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_PSG_STATUS_PANEL), SetMinimalSize(700, 26), SetFill(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_PSG_GENERATE_BTN), SetMinimalSize(260, 24), SetStringTip(STR_PROMPT_SCENARIO_GENERATE_BTN, STR_EMPTY),
		NWidget(NWID_SPACER), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _prompt_scenario_desc(
	WindowPosition::Automatic, "prompt_scenario_generator", 720, 420,
	WindowClass::PromptScenarioGenerator, WindowClass::None,
	{},
	_nested_prompt_scenario_widgets
);

struct PromptScenarioWindow : Window {
	std::string current_prompt = PRESET_1;
	std::string output_path = "demo/Arid-Mining-Vs-Greedy-Core.sav";
	std::string status_message = "Ready. Click Synthesize & Generate Scenario to build and export .sav.";
	PromptScenarioSpec parsed_spec;

	PromptScenarioWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(window_number);
		this->UpdateParsedSpec();
	}

	void UpdateParsedSpec()
	{
		this->parsed_spec = PromptScenarioGenerator::ParsePrompt(this->current_prompt);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_PSG_PRESET_1:
				this->current_prompt = PRESET_1;
				this->output_path = "demo/Arid-Mining-Vs-Greedy-Core.sav";
				this->UpdateParsedSpec();
				this->SetDirty();
				break;

			case WID_PSG_PRESET_2:
				this->current_prompt = PRESET_2;
				this->output_path = "demo/Glacial-Science-Quantum.sav";
				this->UpdateParsedSpec();
				this->SetDirty();
				break;

			case WID_PSG_PRESET_3:
				this->current_prompt = PRESET_3;
				this->output_path = "demo/Volcanic-Foundry-Maglev.sav";
				this->UpdateParsedSpec();
				this->SetDirty();
				break;

			case WID_PSG_GENERATE_BTN: {
				this->status_message = "Synthesizing scenario...";
				this->SetDirty();

				auto res = PromptScenarioGenerator::SynthesizeAndSave(this->parsed_spec, this->output_path);
				if (res.success) {
					this->status_message = fmt::format("Success! Built {} worlds, {} corridors, {} active fleets. Exported to {}",
						res.worlds_created, res.corridors_built, res.trains_spawned, this->output_path);
				} else {
					this->status_message = fmt::format("Synthesis error: {}", res.error_message);
				}
				this->SetDirty();
				break;
			}

			default:
				break;
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_PSG_PROMPT_LABEL:
				DrawString(r.left + 8, r.right - 8, r.top + 6, "Narrative Prompt Description (Peter F. Hamilton Commonwealth Synthesis):", TextColour::Gold);
				break;

			case WID_PSG_PROMPT_EDIT: {
				GfxFillRect(r.left + 4, r.top + 4, r.right - 4, r.bottom - 4, PC_BLACK);
				DrawStringMultiLine(r.left + 8, r.right - 8, r.top + 8, r.bottom - 8, this->current_prompt, TextColour::White);
				break;
			}

			case WID_PSG_OUTPUT_LABEL:
				DrawString(r.left + 8, r.right - 8, r.top + 4, "Output Scenario Savegame Path (.sav):", TextColour::Gold);
				break;

			case WID_PSG_OUTPUT_EDIT: {
				GfxFillRect(r.left + 4, r.top + 2, r.right - 4, r.bottom - 2, PC_BLACK);
				DrawString(r.left + 8, r.right - 8, r.top + 6, this->output_path, TextColour::Yellow);
				break;
			}

			case WID_PSG_ANALYSIS_PANEL: {
				GfxFillRect(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2, PC_DARK_BLUE);
				int y = r.top + 6;
				DrawString(r.left + 8, r.right - 8, y, fmt::format("Semantic Analysis: {} Worlds Detected | Rival Stance: {}",
					this->parsed_spec.world_count,
					this->parsed_spec.rival_relation == CorporateRelation::Hostile ? "Hostile (Trade Interdiction)" :
					this->parsed_spec.rival_relation == CorporateRelation::Allied ? "Allied (Open Trackage Rights)" : "Neutral"), TextColour::White);
				y += 18;

				for (size_t i = 0; i < this->parsed_spec.worlds.size(); ++i) {
					const auto &w = this->parsed_spec.worlds[i];
					std::string biome_str = (w.biome == WorldBiome::AridDesert) ? "Arid Desert" :
						(w.biome == WorldBiome::SubArctic) ? "Sub-Arctic Glacial" :
						(w.biome == WorldBiome::Volcanic) ? "Volcanic Barren" :
						(w.biome == WorldBiome::Oceanic) ? "Oceanic Deep Water" : "Temperate Core";

					std::string strike_tag = w.is_striking ? " [STRIKING: Water Scarcity]" : "";
					std::string line = fmt::format("World {}: {} ({}, Phase {}){}",
						i, w.name, biome_str, static_cast<int>(w.phase), strike_tag);
					DrawString(r.left + 12, r.right - 8, y, line, w.is_striking ? TextColour::Red : TextColour::Silver);
					y += 14;

					std::string sub = fmt::format("  Role: {} | Pop: {}", w.role_description, w.population);
					DrawString(r.left + 12, r.right - 8, y, sub, TextColour::Grey);
					y += 16;
				}

				DrawString(r.left + 12, r.right - 8, y + 4, "Infrastructure: Pre-built CST Corridors + Interplanetary Wormhole Gates + Active Running Trains", TextColour::Green);
				break;
			}

			case WID_PSG_STATUS_PANEL:
				DrawString(r.left + 8, r.right - 8, r.top + 6, this->status_message, TextColour::Gold);
				break;

			default:
				break;
		}
	}
};

void ShowPromptScenarioWindow()
{
	AllocateWindowDescFront<PromptScenarioWindow>(_prompt_scenario_desc, 0);
}
