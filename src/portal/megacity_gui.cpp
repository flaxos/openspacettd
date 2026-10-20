/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file megacity_gui.cpp Implementation of Megacity overview window. */

#include "../stdafx.h"
#include "integrated_economy.h"
#include "../cargotype.h"
#include "megacity_gui.h"
#include "megacity_manager.h"
#include "planet_manager.h"
#include "portal_cmd.h"
#include "../command_func.h"
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
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_HEADER_PANEL),
	SetMinimalSize(520, 92),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_PREV_CITY),
	SetFill(1, 0),
	SetStringTip(STR_MEGACITY_BUTTON_PREV, STR_MEGACITY_BUTTON_PREV_TOOLTIP),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_NEXT_CITY),
	SetFill(1, 0),
	SetStringTip(STR_MEGACITY_BUTTON_NEXT, STR_MEGACITY_BUTTON_NEXT_TOOLTIP),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_LOCATE),
	SetFill(1, 0),
	SetStringTip(STR_MEGACITY_BUTTON_LOCATE, STR_MEGACITY_BUTTON_LOCATE_TOOLTIP),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_MCO_DESIGNATE),
	SetFill(1, 0),
	SetStringTip(STR_MEGACITY_BUTTON_DESIGNATE, STR_MEGACITY_BUTTON_DESIGNATE_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_TIER1_PANEL),
	SetMinimalSize(520, 74),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_TIER2_PANEL),
	SetMinimalSize(520, 74),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_TIER3_PANEL),
	SetMinimalSize(520, 74),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_PANEL, Colours::Brown, WID_MCO_STATUS_BAR),
	SetMinimalSize(508, 24),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _megacity_overview_desc(
	WindowPosition::Automatic, "view_megacity", 520, 420,
	WindowClass::MegacityOverview, WindowClass::None,
	{},
	_nested_megacity_overview_widgets
);

/**
 * Get the action button on the line below a tier panel's title, right-aligned and sized to its label.
 * Drawing and click handling both use this, so the clickable area always matches what is drawn.
 * @param r Panel rectangle.
 * @param label Button text.
 * @return Button rectangle.
 */
static Rect TierButtonRect(const Rect &r, std::string_view label)
{
	const Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
	const int line = GetCharacterHeight(FontSize::Normal);
	const int width = std::max<int>(ScaleGUITrad(120), GetStringBoundingBox(label).width + 2 * WidgetDimensions::scaled.framerect.Horizontal());
	const int top = tr.top + line + ScaleGUITrad(2);
	return Rect{tr.right - width, top, tr.right, top + line + WidgetDimensions::scaled.framerect.Vertical()};
}

/**
 * Text of the FOOD rate button as the local player sees it.
 * @param town Town whose service is shown.
 * @param[out] interactive Whether the local player may change the rate.
 * @return Button label.
 */
static std::string FoodRateButtonLabel(TownID town, bool &interactive)
{
	const TownFoodService *svc = IntegratedEconomy::GetTownFoodService(town);
	const uint32_t rate = svc ? svc->selected_rate : 0;
	const CompanyID owner = svc ? svc->owner : CompanyID::Invalid();
	const std::string rate_str = rate == 0 ? "Off" : fmt::format("{}/mo", rate);
	interactive = false;
	if (_current_company == COMPANY_SPECTATOR) return fmt::format("Rate: {} (spectator)", rate_str);
	if (owner != CompanyID::Invalid() && owner != _current_company) return fmt::format("Rate: {} (company {})", rate_str, owner.base() + 1);
	interactive = true;
	return fmt::format("Rate: {} (click to change)", rate_str);
}

/**
 * Text of the residential district button as the local player sees it.
 * @return Button label.
 */
static std::string CommissionButtonLabel()
{
	return _current_company == COMPANY_SPECTATOR ? "Spectator: view only" : "Commission district";
}

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

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		const bool works = IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2;
		uint lines = 0;
		switch (widget) {
			case WID_MCO_HEADER_PANEL: lines = 6; break;
			case WID_MCO_TIER1_PANEL: lines = works ? 8 : 0; break;
			case WID_MCO_TIER2_PANEL: lines = works ? 5 : 0; break;
			case WID_MCO_TIER3_PANEL: lines = works ? 2 : 0; break;
			case WID_MCO_STATUS_BAR: lines = 2; break;
			default: return;
		}
		/* The button line is taller than a text line by the button's frame. */
		const uint button = (widget == WID_MCO_TIER1_PANEL || widget == WID_MCO_TIER2_PANEL) && works ? WidgetDimensions::scaled.framerect.Vertical() + ScaleGUITrad(4) : 0;
		size.height = std::max<uint>(size.height, lines * (GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(2)) + button + WidgetDimensions::scaled.framerect.Vertical());
	}

	void OnPaint() override
	{
		bool is_mega = (this->current_town != TownID::Invalid()) && MegacityManager::IsMegacity(this->current_town);
		this->SetWidgetDisabledState(WID_MCO_DESIGNATE, is_mega || !Town::IsValidID(this->current_town));
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
					bool is_core = IntegratedEconomy::Enabled() && (IntegratedEconomy::Role(profile->world_id) == EconomicRole::Core || (town != nullptr && IntegratedEconomy::Role(PlanetManager::GetTileWorld(town->xy)) == EconomicRole::Core));
					StringID growth_str;
					if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2 && is_core) {
						growth_str = STR_MEGACITY_GROWTH_CORE_STABLE;
					} else if (is_core) {
						switch (profile->growth_state) {
							case MegacityGrowthState::Starvation:       growth_str = STR_MEGACITY_GROWTH_STARVATION; break;
							case MegacityGrowthState::Subsistence:      growth_str = STR_MEGACITY_GROWTH_CORE_SUBSISTENCE; break;
							case MegacityGrowthState::MetropolitanBoom: growth_str = STR_MEGACITY_GROWTH_CORE_BOOM; break;
							case MegacityGrowthState::HyperGrowth:      growth_str = STR_MEGACITY_GROWTH_HYPERGROWTH; break;
							default:                                    growth_str = STR_MEGACITY_GROWTH_CORE_SUBSISTENCE; break;
						}
					} else {
						switch (profile->growth_state) {
							case MegacityGrowthState::Starvation:       growth_str = STR_MEGACITY_GROWTH_STARVATION; break;
							case MegacityGrowthState::Subsistence:      growth_str = STR_MEGACITY_GROWTH_SUBSISTENCE; break;
							case MegacityGrowthState::MetropolitanBoom: growth_str = STR_MEGACITY_GROWTH_BOOM; break;
							case MegacityGrowthState::HyperGrowth:      growth_str = STR_MEGACITY_GROWTH_HYPERGROWTH; break;
							default:                                    growth_str = STR_MEGACITY_GROWTH_SUBSISTENCE; break;
						}
					}
					DrawString(tr, growth_str);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string supply_str = (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2)
						? fmt::format("Civilian Demand: Optional Food Service (Growth: {:.1f}x, Traffic: {:.1f}x)",
							profile->growth_multiplier, profile->passenger_multiplier)
						: fmt::format("Overall Commodity Satisfaction: {:.1f}% (Growth: {:.1f}x, Traffic: {:.1f}x)",
							profile->overall_supply_index * 100.0f, profile->growth_multiplier, profile->passenger_multiplier);
					DrawString(tr, supply_str, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);
					if (IntegratedEconomy::Enabled() && town != nullptr) DrawStringMultiLine(tr, IntegratedEconomy::CityStatus(town->index), TextColour::White);
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

				const bool works = IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2;
				if (works) tier_label = tier_idx == 0 ? STR_MEGACITY_TIER1_FOOD_SERVICE : tier_idx == 1 ? STR_MEGACITY_TIER2_RESIDENTIAL : STR_MEGACITY_TIER3_PROSPERITY_PILOT;
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				DrawString(tr, tier_label, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal) + 2;
				const int line_step = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(2);

				if (works) {
					if (tier_idx == 0) {
						auto svc = town != nullptr ? IntegratedEconomy::GetTownFoodService(town->index) : nullptr;
						auto city = town != nullptr ? IntegratedEconomy::City(town->index) : nullptr;
						CargoType food_cargo = GetCargoTypeByLabel(CargoLabel{"FOOD"});
						uint32_t stock = (city && city->reserves.contains(food_cargo)) ? city->reserves.at(food_cargo) : 0;
						uint32_t total_consumed = svc ? (svc->consumed_history[0] + svc->consumed_history[1] + svc->consumed_history[2] + svc->consumed_history[3]) : 0;
						float bonus_pct = (static_cast<float>(total_consumed) / 640.0f) * 100.0f;

						bool interactive = false;
						const std::string rate_label = town != nullptr ? FoodRateButtonLabel(town->index, interactive) : "Rate: Off";
						const Rect rate_btn = TierButtonRect(r, rate_label);
						GfxFillRect(rate_btn, GetColourGradient(Colours::Grey, interactive ? Shade::Normal : Shade::Dark));
						DrawString(rate_btn.Shrink(WidgetDimensions::scaled.framerect), rate_label, interactive ? TextColour::Yellow : TextColour::Grey, AlignmentH::Centre);
						tr.top = rate_btn.bottom + ScaleGUITrad(2);

						DrawString(tr, fmt::format("Reserve: {} / 60 FOOD{}", stock, stock >= 60 ? " (full)" : ""), TextColour::White);
						tr.top += line_step;
						DrawString(tr, fmt::format("Delivered: {} since the last check, {} in the month before",
							svc ? svc->delivered_this_period : 0, svc ? svc->delivered_last_period : 0), TextColour::White);
						tr.top += line_step;
						if (svc != nullptr) {
							const size_t last_slot = (svc->history_cursor + 3) % 4;
							const uint32_t last_ate = svc->consumed_history[last_slot];
							const uint32_t last_wanted = svc->requested_history[last_slot];
							std::string last_check = last_wanted == 0 ? "Last check: service was off"
								: fmt::format("Last check: ate {} of {}{}", last_ate, last_wanted, last_ate < last_wanted ? ", not enough food in reserve then" : "");
							DrawString(tr, last_check, last_ate < last_wanted ? TextColour::Orange : TextColour::White);
							tr.top += line_step;
							std::string hist_str = "Last 4 checks, oldest first: ";
							for (int i = 0; i < 4; ++i) {
								if (i > 0) hist_str += ", ";
								size_t slot = (svc->history_cursor + i) % 4;
								hist_str += svc->requested_history[slot] == 0 ? std::string("off") : fmt::format("{}/{}", svc->consumed_history[slot], svc->requested_history[slot]);
							}
							DrawString(tr, hist_str, TextColour::Silver);
							tr.top += line_step;
						}
						DrawString(tr, fmt::format("Passenger bonus: +{:.1f}% (checked on the 1st of each month)", bonus_pct), TextColour::Green);
					} else if (tier_idx == 1) {
						auto r_dist = town != nullptr ? IntegratedEconomy::GetTownResidentialDistrict(town->index) : nullptr;
						bool can_commission = (r_dist == nullptr || !r_dist->commissioned);

						if (can_commission) {
							bool is_spectator = (_current_company == COMPANY_SPECTATOR);
							const std::string label = CommissionButtonLabel();
							const Rect comm_btn = TierButtonRect(r, label);
							GfxFillRect(comm_btn, GetColourGradient(Colours::Grey, is_spectator ? Shade::Dark : Shade::Normal));
							DrawString(comm_btn.Shrink(WidgetDimensions::scaled.framerect), label, is_spectator ? TextColour::Grey : TextColour::Yellow, AlignmentH::Centre);

							tr.top = comm_btn.bottom + ScaleGUITrad(2);
							DrawString(tr, fmt::format("Kit: 160 STEL + 80 BALL and {}", GetString(STR_JUST_CURRENCY_LONG, Money{50000})), TextColour::White);
							tr.top += line_step;
							DrawString(tr, "Adds 4 protected houses where roads give access", TextColour::Silver);
						} else {
							if (r_dist->buildings_completed >= 4) {
								DrawString(tr, fmt::format("Residential works complete: {} protected houses", r_dist->buildings_completed), TextColour::Green);
							} else if (r_dist->site_blocked) {
								DrawString(tr, fmt::format("Waiting for road access: {} houses left", r_dist->building_opportunities), TextColour::Orange);
							} else {
								DrawString(tr, fmt::format("Building: {} left, {} placed, {} finished",
									r_dist->building_opportunities, r_dist->buildings_placed, r_dist->buildings_completed), TextColour::White);
							}
							tr.top += line_step;
							DrawString(tr, "Finished houses are never demolished by town redevelopment", TextColour::Silver);
						}
					} else {
						DrawString(tr, "Not part of this pilot", TextColour::Silver);
					}
					break;
				}

				if (IntegratedEconomy::Enabled() && town != nullptr && !IntegratedEconomy::CityDemand(town->index).empty()) {
					auto demand = IntegratedEconomy::CityDemand(town->index);
					auto city = IntegratedEconomy::City(town->index);
					for (auto [cargo, required] : demand) {
						auto label = CargoSpec::Get(cargo)->label;
						size_t tier =
							label == CargoLabel{"FOOD"} ? 0 : (label == CargoLabel{"STEL"} || label == CargoLabel{"BALL"} ? 1 : 2);
						if (tier != tier_idx) continue;
						uint32_t stock = 0, consumed = 0;
						if (city) {
							if (auto i = city->reserves.find(cargo); i != city->reserves.end()) stock = i->second;
							if (auto i = city->consumed.find(cargo); i != city->consumed.end()) consumed = i->second;
						}
						tr.top = DrawStringMultiLine(
									 tr,
									 fmt::format("{}: demand {}, reserve {} / {}; used {} last month{}", GetString(CargoSpec::Get(cargo)->name), required, stock,
												 required * (tier == 0 ? 3 : 1), consumed, stock < required ? " - delivery needed" : ""),
									 TextColour::White) +
								 2;
					}
					break;
				}
				uint32_t current = profile != nullptr ? profile->delivered_current[tier_idx] : 0;
				uint32_t quota   = profile != nullptr ? profile->monthly_quota[tier_idx] : (tier_idx == 0 ? 50 : (tier_idx == 1 ? 30 : 10));
				float pct        = profile != nullptr ? profile->satisfaction_pct[tier_idx] : 0.0f;

				Rect bar_rect = tr.WithHeight(ScaleGUITrad(14));
				this->DrawProgressBar(bar_rect, current, quota, pct);
				break;
			}

			case WID_MCO_STATUS_BAR: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				const std::string note = IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2
					? "Food service is checked on the 1st of each month."
					: "Monthly supply cycles evaluate on the 1st of each month. Balanced delivery across all 3 tiers drives HyperGrowth.";
				DrawStringMultiLine(tr, note, TextColour::Silver, AlignmentH::Centre);
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
					if (_current_company == COMPANY_SPECTATOR) {
						Command<Commands::DesignateMegacitySpectator>::Post(this->current_town);
					} else {
						Command<Commands::DesignateMegacity>::Post(this->current_town);
					}
				}
				break;
			}

			case WID_MCO_TIER1_PANEL: {
				if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2 && this->current_town != TownID::Invalid()) {
					if (_current_company == COMPANY_SPECTATOR) break;
					bool interactive = false;
					const std::string label = FoodRateButtonLabel(this->current_town, interactive);
					if (!TierButtonRect(this->GetWidget<NWidgetBase>(WID_MCO_TIER1_PANEL)->GetCurrentRect(), label).Contains(pt)) break;

					auto svc = IntegratedEconomy::GetTownFoodService(this->current_town);
					if (svc != nullptr && svc->owner != CompanyID::Invalid() && svc->owner != _current_company) break;

					uint32_t current_rate = svc ? svc->selected_rate : 0;
					uint32_t next_rate = (current_rate == 0) ? 10 : (current_rate == 10) ? 20 : (current_rate == 20) ? 40 : 0;
					Command<Commands::SetTownFoodServiceRate>::Post(this->current_town, next_rate);
					this->SetDirty();
				}
				break;
			}

			case WID_MCO_TIER2_PANEL: {
				if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2 && this->current_town != TownID::Invalid()) {
					if (_current_company == COMPANY_SPECTATOR) break;
					if (!TierButtonRect(this->GetWidget<NWidgetBase>(WID_MCO_TIER2_PANEL)->GetCurrentRect(), CommissionButtonLabel()).Contains(pt)) break;

					auto r_dist = IntegratedEconomy::GetTownResidentialDistrict(this->current_town);
					if (r_dist == nullptr || !r_dist->commissioned) {
						Command<Commands::CommissionTownResidentialDistrict>::Post(this->current_town);
						this->SetDirty();
					}
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
