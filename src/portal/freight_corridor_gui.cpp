/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file freight_corridor_gui.cpp Implementation of Freight Corridor Monitor window. */

#include "../stdafx.h"
#include "freight_corridor_gui.h"
#include "universe_authority.h"
#include "planet_manager.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../widgets/freight_corridor_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"

#include "../safeguards.h"

static constexpr std::initializer_list<NWidgetPart> _nested_freight_corridor_monitor_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen, WID_FCM_CAPTION), SetStringTip(STR_CORRIDOR_MONITOR_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_FCM_SUMMARY_PANEL), SetMinimalSize(520, 80), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_FCM_CORRIDOR_LIST), SetMinimalSize(508, 220), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_FCM_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _freight_corridor_monitor_desc(
	WindowPosition::Automatic, "view_freight_corridors", 520, 340,
	WindowClass::FreightCorridorMonitor, WindowClass::None,
	{},
	_nested_freight_corridor_monitor_widgets
);

/** Window class for monitoring active inter-server freight corridors, traffic loads, and congestion delays. */
struct FreightCorridorMonitorWindow : Window {
	Scrollbar *vscroll = nullptr;

	FreightCorridorMonitorWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_FCM_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);
	}

	void OnPaint() override
	{
		auto corridors = UniverseAuthorityService::Instance().GetFreightCorridors();
		this->vscroll->SetCount(corridors.size());

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		auto &service = UniverseAuthorityService::Instance();

		switch (widget) {
			case WID_FCM_SUMMARY_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto corridors = service.GetFreightCorridors();
				auto matrix = service.GetEmpireSupplyChainMatrix();
				auto audit = service.GetCommodityAudit();

				std::string line1 = fmt::format("Active Corridors: {}  |  In-Transit Consists: {}  |  Total Dispatched: {}",
					corridors.size(), audit.total_transfers_in_transit, audit.total_transfers_completed + audit.total_transfers_in_transit);
				DrawString(tr, line1, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				std::string line2 = fmt::format("Supply Chain: Frontier->Refinery: {:L} | Refinery->Core: {:L} | Frontier->Core: {:L}",
					matrix.frontier_to_refinery_cargo, matrix.refinery_to_core_cargo, matrix.frontier_to_core_cargo);
				DrawString(tr, line2, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal);

				std::string line3 = fmt::format("Core Exports: {:L} units | Cumulative Trade Tariffs: Cr {:L}",
					matrix.core_export_cargo, matrix.total_tariffs_generated);
				DrawString(tr, line3, TextColour::LightBlue);
				tr.top += GetCharacterHeight(FontSize::Normal);

				std::string line4 = "Congestion Multipliers: Clear 1.0x | Moderate 1.2x | Congested 1.5x | Saturated 2.0x";
				DrawString(tr, line4, TextColour::Silver);
				break;
			}

			case WID_FCM_CORRIDOR_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto corridors = service.GetFreightCorridors();

				if (corridors.empty()) {
					DrawString(tr, STR_CORRIDOR_NO_ROUTES, TextColour::Silver, AlignmentH::Centre);
					return;
				}

				int pos = -this->vscroll->GetPosition();
				const int cap = this->vscroll->GetCapacity();
				const int item_height = GetCharacterHeight(FontSize::Normal) * 3 + ScaleGUITrad(12);

				for (const auto &route : corridors) {
					if (IsInsideMM(pos, 0, cap)) {
						Rect item_rect = tr.WithHeight(item_height);

						/* Background card */
						GfxFillRect(item_rect.Shrink(1), GetColourGradient(Colours::DarkGreen, Shade::Darkest));

						Rect text_rect = item_rect.Shrink(WidgetDimensions::scaled.framerect);

						/* Line 1: Route endpoints and priority */
						std::string priority_name = "Standard";
						if (route.priority == FreightPriority::Express) priority_name = "Express";
						else if (route.priority == FreightPriority::PriorityUrgent) priority_name = "Urgent";
						else if (route.priority == FreightPriority::Bulk) priority_name = "Bulk";

						std::string header = fmt::format("Corridor #{}: World {} (Gate {}) -> World {} (Gate {})  [{}]",
							route.route_id, route.source_world.base(), route.source_gate_id,
							route.dest_world.base(), route.dest_gate_id, priority_name);
						DrawString(text_rect, header, TextColour::White);
						text_rect.top += GetCharacterHeight(FontSize::Normal);

						/* Line 2: Utilization gauge and stats */
						float util = route.max_active_in_transit > 0 ?
							static_cast<float>(route.current_in_transit_count) / static_cast<float>(route.max_active_in_transit) : 0.0f;

						StringID cong_str = STR_CORRIDOR_STATUS_CLEAR;
						PixelColour gauge_col = GetColourGradient(Colours::Green, Shade::Normal);

						if (route.congestion_level == CorridorCongestionLevel::Moderate) {
							cong_str = STR_CORRIDOR_STATUS_MODERATE;
							gauge_col = GetColourGradient(Colours::Yellow, Shade::Normal);
						} else if (route.congestion_level == CorridorCongestionLevel::Congested) {
							cong_str = STR_CORRIDOR_STATUS_CONGESTED;
							gauge_col = GetColourGradient(Colours::Orange, Shade::Normal);
						} else if (route.congestion_level == CorridorCongestionLevel::Saturated) {
							cong_str = STR_CORRIDOR_STATUS_SATURATED;
							gauge_col = GetColourGradient(Colours::Red, Shade::Normal);
						}

						std::string stats = fmt::format("In-Transit: {}/{} trains ({:.0f}%) | Dispatched: {} | Status: ",
							route.current_in_transit_count, route.max_active_in_transit, util * 100.0f, route.total_trains_dispatched);
						DrawString(text_rect, stats, TextColour::Gold);

						int stats_width = GetStringBoundingBox(stats).width;
						Rect cong_rect = text_rect.Indent(stats_width, _current_text_dir != TD_RTL);
						DrawString(cong_rect, cong_str);
						text_rect.top += GetCharacterHeight(FontSize::Normal);

						/* Mini utilization gauge bar */
						Rect bar_rect = text_rect.WithHeight(ScaleGUITrad(6)).WithWidth(ScaleGUITrad(200), _current_text_dir != TD_RTL);
						GfxFillRect(bar_rect, GetColourGradient(Colours::Grey, Shade::Darkest));
						int fill_w = std::min(bar_rect.Width(), static_cast<int>(bar_rect.Width() * std::min(util, 1.0f)));
						if (fill_w > 0) {
							GfxFillRect(bar_rect.WithWidth(fill_w, _current_text_dir != TD_RTL), gauge_col);
						}

						tr.top += item_height + ScaleGUITrad(4);
					}
					pos++;
				}
				break;
			}
		}
	}
};

void ShowFreightCorridorMonitor()
{
	AllocateWindowDescFront<FreightCorridorMonitorWindow>(_freight_corridor_monitor_desc, 0);
}
