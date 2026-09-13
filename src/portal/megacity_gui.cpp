/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file megacity_gui.cpp Implementation of Megacity overview window. */

#include "../stdafx.h"
#include "megacity_gui.h"
#include "megacity_manager.h"
#include "planet_manager.h"
#include "../town.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../viewport_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../widgets/megacity_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"

#include "../safeguards.h"

static constexpr std::initializer_list<NWidgetPart> _nested_megacity_overview_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_MCO_CAPTION),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_DEFSIZEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_HEADER_PANEL), SetMinimalSize(440, 72), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_PREV_CITY), SetFill(1, 0), SetStringTip(STR_MEGACITY_BUTTON_PREV, STR_MEGACITY_BUTTON_PREV_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_NEXT_CITY), SetFill(1, 0), SetStringTip(STR_MEGACITY_BUTTON_NEXT, STR_MEGACITY_BUTTON_NEXT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_LOCATE), SetFill(1, 0), SetStringTip(STR_MEGACITY_BUTTON_LOCATE, STR_MEGACITY_BUTTON_LOCATE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_DESIGNATE), SetFill(1, 0), SetStringTip(STR_MEGACITY_BUTTON_DESIGNATE, STR_MEGACITY_BUTTON_DESIGNATE_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_TIER1_PANEL), SetMinimalSize(440, 52), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_TIER2_PANEL), SetMinimalSize(440, 52), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_TIER3_PANEL), SetMinimalSize(440, 52), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_MCO_STATUS_BAR), SetMinimalSize(428, 24), SetFill(1, 0), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _megacity_overview_desc(
	WindowPosition::Automatic, "view_megacity", 440, 310,
	WindowClass::MegacityOverview, WindowClass::None,
	{},
	_nested_megacity_overview_widgets
);

/** Window class for displaying and managing Megacity commodity profiles and growth status. */
struct MegacityOverviewWindow : Window {
	TownID current_town = TownID::Invalid();

	MegacityOverviewWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->current_town = static_cast<TownID>(window_number);

		if (this->current_town == TownID::Invalid()) {
			auto megacities = MegacityManager::GetAllMegacities();
			if (!megacities.empty()) {
				this->current_town = megacities.front().town_id;
			}
		}

		this->FinishInitNested(this->current_town.base());
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_MCO_CAPTION) {
			if (this->current_town != TownID::Invalid() && Town::IsValidID(this->current_town)) {
				return fmt::format("Megacity: {}", Town::Get(this->current_town)->name);
			}
			return GetString(STR_MEGACITY_VIEW_CAPTION);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnPaint() override
	{
		bool is_mega = (this->current_town != TownID::Invalid()) && MegacityManager::IsMegacity(this->current_town);
		this->SetWidgetDisabledState(WID_MCO_DESIGNATE, is_mega || this->current_town == TownID::Invalid());
		this->SetWidgetDisabledState(WID_MCO_LOCATE, this->current_town == TownID::Invalid() || !Town::IsValidID(this->current_town));

		auto all_megas = MegacityManager::GetAllMegacities();
		this->SetWidgetDisabledState(WID_MCO_PREV_CITY, all_megas.size() <= 1);
		this->SetWidgetDisabledState(WID_MCO_NEXT_CITY, all_megas.size() <= 1);

		this->DrawWidgets();
	}

	void DrawProgressBar(const Rect &r, uint32_t current, uint32_t target, float pct) const
	{
		Rect bar_rect = r.Shrink(WidgetDimensions::scaled.bevel);
		GfxFillRect(bar_rect, GetColourGradient(Colours::Grey, Shade::Darkest));

		int bar_width = bar_rect.Width();
		int bar_height = bar_rect.Height();
		if (bar_width <= 2 || bar_height <= 2) return;

		float clamped_pct = std::min(std::max(pct, 0.0f), 1.0f);
		int fill_width = static_cast<int>(bar_width * clamped_pct);

		PixelColour fill_col;
		if (pct >= 1.0f) {
			fill_col = GetColourGradient(Colours::Green, Shade::Normal);
		} else if (pct >= 0.5f) {
			fill_col = GetColourGradient(Colours::Yellow, Shade::Normal);
		} else {
			fill_col = GetColourGradient(Colours::Red, Shade::Normal);
		}

		if (fill_width > 0) {
			Rect filled = bar_rect.WithWidth(fill_width, _current_text_dir != TD_RTL);
			GfxFillRect(filled, fill_col);
		}

		std::string pct_text = fmt::format("{:.1f}% ({} / {} units)", pct * 100.0f, current, target);
		DrawString(bar_rect, pct_text, TextColour::White, AlignmentH::Centre);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const MegacityProfile *profile = MegacityManager::GetProfile(this->current_town);
		const Town *town = (this->current_town != TownID::Invalid() && Town::IsValidID(this->current_town)) ? Town::Get(this->current_town) : nullptr;

		switch (widget) {
			case WID_MCO_HEADER_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (profile == nullptr && town == nullptr) {
					DrawString(tr, STR_MEGACITY_NO_MEGACITIES, TextColour::Silver, AlignmentH::Centre);
					return;
				}

				std::string name_str = town != nullptr ? town->name : (profile != nullptr ? profile->town_name : "Unknown");
				std::string world_str = "Global";
				if (town != nullptr && PlanetManager::Count() > 0) {
					const PlanetRegion *region = PlanetManager::GetRegionByTile(town->xy);
					if (region != nullptr) {
						world_str = fmt::format("{} ({})", region->name, PlanetManager::GetWorldPhaseName(region->phase));
					}
				}

				uint32_t pop = town != nullptr ? town->cache.population : (profile != nullptr ? profile->population : 0);
				std::string line1 = fmt::format("Metropolis: {}  |  World: {}", name_str, world_str);
				DrawString(tr, line1, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				std::string line2 = fmt::format("Population: {:L}", pop);
				DrawString(tr, line2, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal);

				if (profile != nullptr) {
					StringID growth_str;
					switch (profile->growth_state) {
						case MegacityGrowthState::Starvation:       growth_str = STR_MEGACITY_GROWTH_STARVATION; break;
						case MegacityGrowthState::Subsistence:      growth_str = STR_MEGACITY_GROWTH_SUBSISTENCE; break;
						case MegacityGrowthState::MetropolitanBoom: growth_str = STR_MEGACITY_GROWTH_BOOM; break;
						case MegacityGrowthState::HyperGrowth:      growth_str = STR_MEGACITY_GROWTH_HYPERGROWTH; break;
						default:                                    growth_str = STR_MEGACITY_GROWTH_SUBSISTENCE; break;
					}
					DrawString(tr, growth_str);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string supply_str = fmt::format("Overall Commodity Satisfaction: {:.1f}% (Growth: {:.1f}x, Traffic: {:.1f}x)",
						profile->overall_supply_index * 100.0f, profile->growth_multiplier, profile->passenger_multiplier);
					DrawString(tr, supply_str, TextColour::White);
				} else {
					DrawString(tr, "Status: Not registered as Megacity. Click 'Designate Megacity' to activate quotas.", TextColour::Silver);
				}
				break;
			}

			case WID_MCO_TIER1_PANEL:
			case WID_MCO_TIER2_PANEL:
			case WID_MCO_TIER3_PANEL: {
				size_t tier_idx = 0;
				StringID tier_label = STR_MEGACITY_TIER1_SUSTENANCE;
				if (widget == WID_MCO_TIER2_PANEL) {
					tier_idx = 1;
					tier_label = STR_MEGACITY_TIER2_EXPANSION;
				} else if (widget == WID_MCO_TIER3_PANEL) {
					tier_idx = 2;
					tier_label = STR_MEGACITY_TIER3_PROSPERITY;
				}

				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				DrawString(tr, tier_label, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal) + 2;

				uint32_t current = profile != nullptr ? profile->delivered_current[tier_idx] : 0;
				uint32_t quota   = profile != nullptr ? profile->monthly_quota[tier_idx] : (tier_idx == 0 ? 50 : (tier_idx == 1 ? 30 : 10));
				float pct        = profile != nullptr ? profile->satisfaction_pct[tier_idx] : 0.0f;

				Rect bar_rect = tr.WithHeight(ScaleGUITrad(14));
				this->DrawProgressBar(bar_rect, current, quota, pct);
				break;
			}

			case WID_MCO_STATUS_BAR: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				DrawString(tr, "Monthly supply cycles evaluate on the 1st of each month. Balanced delivery across all 3 tiers drives HyperGrowth.", TextColour::Silver, AlignmentH::Centre);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto all_megas = MegacityManager::GetAllMegacities();

		switch (widget) {
			case WID_MCO_PREV_CITY: {
				if (all_megas.empty()) return;
				size_t cur_idx = 0;
				for (size_t i = 0; i < all_megas.size(); ++i) {
					if (all_megas[i].town_id == this->current_town) {
						cur_idx = i;
						break;
					}
				}
				size_t next_idx = (cur_idx == 0) ? all_megas.size() - 1 : cur_idx - 1;
				this->current_town = all_megas[next_idx].town_id;
				this->SetDirty();
				break;
			}

			case WID_MCO_NEXT_CITY: {
				if (all_megas.empty()) return;
				size_t cur_idx = 0;
				for (size_t i = 0; i < all_megas.size(); ++i) {
					if (all_megas[i].town_id == this->current_town) {
						cur_idx = i;
						break;
					}
				}
				size_t next_idx = (cur_idx + 1) % all_megas.size();
				this->current_town = all_megas[next_idx].town_id;
				this->SetDirty();
				break;
			}

			case WID_MCO_LOCATE: {
				if (this->current_town != TownID::Invalid() && Town::IsValidID(this->current_town)) {
					ScrollMainWindowToTile(Town::Get(this->current_town)->xy);
				}
				break;
			}

			case WID_MCO_DESIGNATE: {
				if (this->current_town != TownID::Invalid() && Town::IsValidID(this->current_town)) {
					const Town *t = Town::Get(this->current_town);
					WorldID wid = WorldID{0};
					if (PlanetManager::Count() > 0) {
						const PlanetRegion *region = PlanetManager::GetRegionByTile(t->xy);
						if (region != nullptr) wid = region->id;
					}
					MegacityManager::RegisterMegacity(this->current_town, wid, t->name, t->cache.population);
					this->SetDirty();
				}
				break;
			}
		}
	}
};

void ShowMegacityOverview(TownID town_id)
{
	AllocateWindowDescFront<MegacityOverviewWindow>(_megacity_overview_desc, town_id.base());
}
