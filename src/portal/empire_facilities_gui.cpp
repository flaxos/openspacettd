/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file empire_facilities_gui.cpp Implementation of Empire Industrial Facilities and Supply Chain dashboard window. */

#include "../stdafx.h"
#include "empire_facilities_gui.h"
#include "production_chain.h"
#include "portal_cmd.h"
#include "planet_manager.h"
#include "logistics_hub.h"
#include "company_stockpile.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../viewport_func.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../cargotype.h"
#include "../widgets/empire_facilities_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"

#include "../safeguards.h"

enum class EmpireFacilityFilter : uint8_t {
	All = 0,
	Structural = 1,
	Electronics = 2,
	Propulsion = 3,
	DataCrystals = 4,
};

static constexpr uint ROW_HEIGHT = 48;

static constexpr std::initializer_list<NWidgetPart> _nested_empire_facilities_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen, WID_EF_CAPTION), SetStringTip(STR_EMPIRE_FACILITIES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_ALL), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_ALL, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_A), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_A, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_B), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_B, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_C), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_C, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_D), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_D, STR_EMPTY),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_REFRESH), SetMinimalSize(80, 20), SetStringTip(STR_EMPIRE_FACILITIES_REFRESH, STR_EMPIRE_FACILITIES_REFRESH_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_EF_LIST_PANEL), SetMinimalSize(738, 300), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_EF_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_EF_SUMMARY_PANEL), SetMinimalSize(750, 40), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_UPGRADE_ALL_BTN), SetMinimalSize(240, 22), SetStringTip(STR_EMPIRE_FACILITIES_UPGRADE_ALL, STR_EMPIRE_FACILITIES_UPGRADE_ALL_TOOLTIP),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _empire_facilities_desc(
	WindowPosition::Automatic, "view_empire_facilities", 760, 440,
	WindowClass::EmpireFacilities, WindowClass::None,
	{},
	_nested_empire_facilities_widgets
);

struct EmpireFacilitiesWindow : Window {
	EmpireFacilityFilter active_filter = EmpireFacilityFilter::All;
	Scrollbar *vscroll = nullptr;
	CompanyID target_company = CompanyID::Invalid();

	EmpireFacilitiesWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_EF_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);
		this->target_company = _local_company;
	}

	std::vector<ProcessingFacility> GetFilteredFacilities() const
	{
		std::vector<ProcessingFacility> all = ProductionChainManager::GetAllFacilities();
		std::vector<ProcessingFacility> filtered;
		filtered.reserve(all.size());

		for (const auto &f : all) {
			if (this->target_company != CompanyID::Invalid() && f.owner != this->target_company) {
				continue;
			}
			const ProductionRecipe *rec = ProductionChainManager::GetRecipe(f.recipe_id);
			if (rec == nullptr) continue;

			switch (this->active_filter) {
				case EmpireFacilityFilter::All:
					filtered.push_back(f);
					break;
				case EmpireFacilityFilter::Structural:
					if (rec->pipeline == PipelineType::Structural) filtered.push_back(f);
					break;
				case EmpireFacilityFilter::Electronics:
					if (rec->pipeline == PipelineType::Electronics) filtered.push_back(f);
					break;
				case EmpireFacilityFilter::Propulsion:
					if (rec->pipeline == PipelineType::Propulsion) filtered.push_back(f);
					break;
				case EmpireFacilityFilter::DataCrystals:
					if (rec->pipeline == PipelineType::DataCrystals) filtered.push_back(f);
					break;
			}
		}
		return filtered;
	}

	void OnPaint() override
	{
		auto filtered = this->GetFilteredFacilities();
		this->vscroll->SetCount(filtered.size());

		this->SetWidgetLoweredState(WID_EF_TAB_ALL,    this->active_filter == EmpireFacilityFilter::All);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_A, this->active_filter == EmpireFacilityFilter::Structural);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_B, this->active_filter == EmpireFacilityFilter::Electronics);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_C, this->active_filter == EmpireFacilityFilter::Propulsion);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_D, this->active_filter == EmpireFacilityFilter::DataCrystals);

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_EF_LIST_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto filtered = this->GetFilteredFacilities();
				if (filtered.empty()) {
					DrawString(tr, "No industrial facilities registered for current filter.", TextColour::Grey);
					return;
				}

				size_t start = this->vscroll->GetPosition();
				size_t end = std::min(filtered.size(), start + (tr.Height() / ROW_HEIGHT));

				int y = tr.top;
				for (size_t i = start; i < end; ++i) {
					const auto &f = filtered[i];
					const ProductionRecipe *rec = ProductionChainManager::GetRecipe(f.recipe_id);
					const Station *st = Station::GetIfValid(f.linked_station);
					const PlanetRegion *region = PlanetManager::GetRegion(f.world_id);

					Rect row_rect = Rect{tr.left, y, tr.right, y + static_cast<int>(ROW_HEIGHT) - 2};
					GfxFillRect(row_rect, (i % 2 == 0) ? GetColourGradient(Colours::DarkGreen, Shade::Darkest) : GetColourGradient(Colours::DarkGreen, Shade::Dark));

					/* Line 1: World + Station name + Status */
					std::string world_str = (region != nullptr) ? fmt::format("World {}: {} ({})", f.world_id.base() + 1, region->name,
						region->phase == WorldPhase::Phase1_Core ? "Core" :
						region->phase == WorldPhase::Phase2_Developed ? "Developed" :
						region->phase == WorldPhase::Phase3_Frontier ? "Frontier" : "Expansion") : fmt::format("World {}", f.world_id.base() + 1);

					std::string st_name = (st != nullptr) ? fmt::format("{} [{}, {}]", st->name, TileX(f.tile), TileY(f.tile)) : fmt::format("Tile [{}, {}]", TileX(f.tile), TileY(f.tile));

					std::string line1 = fmt::format("{}  >>  {}", world_str, st_name);
					Rect line1_r = row_rect.Shrink(2);
					DrawString(line1_r, line1, TextColour::Yellow);

					/* Status Badge on right */
					FacilityStatus status = ProductionChainManager::GetFacilityStatus(&f);
					std::string status_badge;
					TextColour status_col = TextColour::Green;
					switch (status) {
						case FacilityStatus::Overflow:
							status_badge = fmt::format("[Overflow: {} t to Hub]", f.last_month_hub_overflow);
							status_col = TextColour::Gold;
							break;
						case FacilityStatus::Active:
							status_badge = fmt::format("[Active: {}/{} t/mo]", f.last_month_production, f.monthly_capacity);
							status_col = TextColour::White;
							break;
						case FacilityStatus::Starved:
							status_badge = "[Starved: Need Inputs]";
							status_col = TextColour::Red;
							break;
						case FacilityStatus::Idle:
						default:
							status_badge = fmt::format("[Idle (Cap: {} t/mo)]", f.monthly_capacity);
							status_col = TextColour::Grey;
							break;
					}

					Rect badge_r = Rect{row_rect.right - 290, y + 2, row_rect.right - 155, y + 16};
					DrawString(badge_r, status_badge, status_col);

					/* Line 2: Recipe Name & Inputs/Outputs */
					std::string line2 = (rec != nullptr) ? fmt::format("Recipe: {} (Cap: {} t/mo)", rec->name, f.monthly_capacity) : "Recipe: None";
					Rect line2_r = Rect{row_rect.left + 2, y + 16, row_rect.right - 155, y + 30};
					DrawString(line2_r, line2, TextColour::White);

					/* Line 3: Input/Output Buffers */
					std::string in_desc = "In: ";
					if (rec != nullptr) {
						for (const auto &[in_c, in_amt] : rec->inputs) {
							auto it = f.input_buffers.find(in_c);
							uint32_t buf = (it != f.input_buffers.end()) ? it->second : 0;
							in_desc += fmt::format("{} {} (buf: {}); ", in_amt, GetString(CargoSpec::Get(in_c)->name), buf);
						}
					}

					std::string out_desc = fmt::format("Plat Target: {} t", f.platform_capacity);
					std::string line3 = fmt::format("{} | {}", in_desc, out_desc);
					Rect line3_r = Rect{row_rect.left + 2, y + 30, row_rect.right - 155, y + 46};
					DrawString(line3_r, line3, TextColour::Silver);

					/* Interactive Buttons on right */
					Rect btn_upgrade = Rect{row_rect.right - 150, y + 4, row_rect.right - 58, y + 22};
					GfxFillRect(btn_upgrade, GetColourGradient(Colours::Grey, Shade::Normal));
					DrawString(btn_upgrade, "Upgrade (+50)", TextColour::Yellow, AlignmentH::Centre);

					Rect btn_retire = Rect{row_rect.right - 54, y + 4, row_rect.right - 4, y + 22};
					GfxFillRect(btn_retire, GetColourGradient(Colours::Red, Shade::Normal));
					DrawString(btn_retire, GetString(STR_EMPIRE_FACILITIES_RETIRE_BTN), TextColour::White, AlignmentH::Centre);

					Rect btn_target = Rect{row_rect.right - 150, y + 26, row_rect.right - 4, y + 44};
					GfxFillRect(btn_target, GetColourGradient(Colours::Grey, Shade::Normal));
					std::string target_label = (f.platform_capacity == 0) ? "Platform: 0 (Hub)" : fmt::format("Platform: {} t", f.platform_capacity);
					DrawString(btn_target, target_label, TextColour::White, AlignmentH::Centre);

					y += ROW_HEIGHT;
				}
				break;
			}

			case WID_EF_SUMMARY_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto all_facs = ProductionChainManager::GetAllFacilities();
				uint32_t total_active = 0;
				uint32_t total_starved = 0;
				uint32_t total_monthly_prod = 0;

				for (const auto &f : all_facs) {
					if (this->target_company != CompanyID::Invalid() && f.owner != this->target_company) continue;
					total_monthly_prod += f.last_month_production;
					FacilityStatus status = ProductionChainManager::GetFacilityStatus(&f);
					if (status == FacilityStatus::Starved) total_starved++;
					else if (status == FacilityStatus::Active || status == FacilityStatus::Overflow) total_active++;
				}

				uint64_t total_overflow = ProductionChainManager::GetTotalHubOverflow();
				uint32_t last_overflow = ProductionChainManager::GetLastMonthHubOverflow();

				std::string summary = fmt::format("Active: {}  |  Starved: {}  |  Monthly Output: {} t  |  Hub Overflow: {} t (All-Time: {} t)",
					total_active, total_starved, total_monthly_prod, last_overflow, total_overflow);
				DrawString(tr, summary, TextColour::White, AlignmentH::Centre);
				break;
			}

			default:
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_EF_TAB_ALL:
				this->active_filter = EmpireFacilityFilter::All;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_A:
				this->active_filter = EmpireFacilityFilter::Structural;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_B:
				this->active_filter = EmpireFacilityFilter::Electronics;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_C:
				this->active_filter = EmpireFacilityFilter::Propulsion;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_D:
				this->active_filter = EmpireFacilityFilter::DataCrystals;
				this->SetDirty();
				break;
			case WID_EF_REFRESH:
				this->SetDirty();
				break;

			case WID_EF_UPGRADE_ALL_BTN: {
				auto filtered = this->GetFilteredFacilities();
				for (const auto &f : filtered) {
					if (f.linked_station != StationID::Invalid()) {
						Command<Commands::UpgradeProcessingFacility>::Post(STR_ERROR_CAN_T_UPGRADE_FACILITY, f.linked_station, 50);
					}
				}
				this->SetDirty();
				break;
			}

			case WID_EF_LIST_PANEL: {
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_EF_LIST_PANEL);
				Rect r = nwi->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				if (pt.y < r.top || pt.y >= r.bottom) return;

				size_t idx = this->vscroll->GetPosition() + (pt.y - r.top) / ROW_HEIGHT;
				auto filtered = this->GetFilteredFacilities();
				if (idx >= filtered.size()) return;

				const auto &f = filtered[idx];
				int rel_y = (pt.y - r.top) % ROW_HEIGHT;

				/* Check button clicks on right side */
				if (pt.x >= r.right - 150 && pt.x <= r.right - 4) {
					if (rel_y >= 4 && rel_y <= 22) {
						if (pt.x <= r.right - 58) {
							/* Upgrade button */
							if (f.linked_station != StationID::Invalid()) {
								Command<Commands::UpgradeProcessingFacility>::Post(STR_ERROR_CAN_T_UPGRADE_FACILITY, f.linked_station, 50);
							}
							return;
						} else {
							/* Retire button */
							if (f.linked_station != StationID::Invalid()) {
								Command<Commands::RemoveProcessingFacility>::Post(STR_ERROR_CAN_T_REMOVE_FACILITY, f.linked_station);
							}
							return;
						}
					} else if (rel_y >= 26 && rel_y <= 44) {
						/* Platform target cycle: 0 -> 50 -> 100 -> 200 -> 0 */
						uint32_t next_target = (f.platform_capacity == 0) ? 50 : (f.platform_capacity == 50) ? 100 : (f.platform_capacity == 100) ? 200 : 0;
						if (f.linked_station != StationID::Invalid()) {
							Command<Commands::SetFacilityPlatformCapacity>::Post(STR_ERROR_CAN_T_SET_PLATFORM_CAP, f.linked_station, next_target);
						}
						return;
					}
				}

				/* Click elsewhere on row: scroll viewport to station */
				if (f.tile != INVALID_TILE) {
					ScrollMainWindowToTile(f.tile);
				}
				break;
			}

			default:
				break;
		}
	}
};

void ShowEmpireFacilitiesWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) {
		company = _local_company;
	}
	AllocateWindowDescFront<EmpireFacilitiesWindow>(_empire_facilities_desc, company.base());
}
