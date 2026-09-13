/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file freight_corridor_gui.cpp Implementation of Freight Corridor Monitor window. */

#include "../stdafx.h"
#include "freight_corridor_gui.h"
#include "universe_authority.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "../viewport_func.h"
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
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_FCM_CORRIDOR_LIST), SetMinimalSize(508, 160), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_FCM_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_FCM_TELEMETRY_PANEL), SetMinimalSize(420, 80), SetFill(1, 0), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_FCM_LOCATE_GATE), SetMinimalSize(100, 80), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_CORRIDOR_LOCATE_GATE, STR_CORRIDOR_LOCATE_GATE_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _freight_corridor_monitor_desc(
	WindowPosition::Automatic, "view_freight_corridors", 520, 420,
	WindowClass::FreightCorridorMonitor, WindowClass::None,
	{},
	_nested_freight_corridor_monitor_widgets
);

/** Window class for monitoring active inter-server freight corridors, traffic loads, and congestion delays. */
struct FreightCorridorMonitorWindow : Window {
	Scrollbar *vscroll = nullptr;
	uint32_t selected_route_id = 0;

	FreightCorridorMonitorWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_FCM_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);

		auto corridors = UniverseAuthorityService::Instance().GetFreightCorridors();
		if (!corridors.empty()) {
			this->selected_route_id = corridors.front().route_id;
		}
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
						bool is_selected = (route.route_id == this->selected_route_id);
						GfxFillRect(item_rect.Shrink(1), GetColourGradient(Colours::DarkGreen, is_selected ? Shade::Normal : Shade::Darkest));

						Rect text_rect = item_rect.Shrink(WidgetDimensions::scaled.framerect);

						/* Line 1: Route endpoints and priority */
						std::string priority_name = "Standard";
						if (route.priority == FreightPriority::Express) priority_name = "Express";
						else if (route.priority == FreightPriority::PriorityUrgent) priority_name = "Urgent";
						else if (route.priority == FreightPriority::Bulk) priority_name = "Bulk";

						std::string header = fmt::format("Corridor #{}: World {} (Gate {}) -> World {} (Gate {})  [{}]",
							route.route_id, route.source_world.base(), route.source_gate_id,
							route.dest_world.base(), route.dest_gate_id, priority_name);
						if (route.is_twin_array) header += "  [Twin Array 2x]";
						DrawString(text_rect, header, is_selected ? TextColour::Gold : TextColour::White);
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

			case WID_FCM_TELEMETRY_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (this->selected_route_id == 0) {
					DrawString(tr, "Select a freight corridor above to inspect in-transit consist telemetry.", TextColour::Silver);
					break;
				}

				const InterServerRoute *route = service.GetRoute(this->selected_route_id);
				if (route == nullptr) {
					DrawString(tr, "Corridor route not found.", TextColour::Silver);
					break;
				}

				auto in_transit = service.GetInTransitTransfersForRoute(this->selected_route_id);
				std::string title = fmt::format("Telemetry - Corridor #{}: World {} (Gate {}) -> World {} (Gate {})  [{} active]{}",
					route->route_id, route->source_world.base(), route->source_gate_id,
					route->dest_world.base(), route->dest_gate_id, in_transit.size(),
					route->is_twin_array ? "  [Twin Gateway Array - 2x Bandwidth]" : "");
				DrawString(tr, title, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				if (in_transit.empty()) {
					DrawString(tr, "No consists currently in transit along this freight corridor.", TextColour::Silver);
				} else {
					size_t shown = 0;
					for (const auto &rec : in_transit) {
						if (shown >= 2) {
							std::string more = fmt::format("... and {} more in-transit consist(s)", in_transit.size() - shown);
							DrawString(tr, more, TextColour::Silver);
							break;
						}
						std::string prio_str = "Standard";
						if (rec.priority == FreightPriority::Express) prio_str = "Express";
						else if (rec.priority == FreightPriority::PriorityUrgent) prio_str = "Urgent";
						else if (rec.priority == FreightPriority::Bulk) prio_str = "Bulk";

						std::string info = fmt::format("Consist {}: {} cargo units | Priority: {} | Est. Transit: {} ticks",
							rec.transfer_id, rec.total_cargo_units, prio_str, rec.effective_transit_ticks);
						DrawString(tr, info, TextColour::Gold);
						tr.top += GetCharacterHeight(FontSize::Normal);
						shown++;
					}
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto &service = UniverseAuthorityService::Instance();

		switch (widget) {
			case WID_FCM_CORRIDOR_LIST: {
				auto corridors = service.GetFreightCorridors();
				if (corridors.empty()) return;

				const int item_height = GetCharacterHeight(FontSize::Normal) * 3 + ScaleGUITrad(12) + ScaleGUITrad(4);
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_FCM_CORRIDOR_LIST, WidgetDimensions::scaled.framerect.top, item_height);
				if (row >= 0 && static_cast<size_t>(row) < corridors.size()) {
					this->selected_route_id = corridors[row].route_id;
					this->SetDirty();
				}
				break;
			}

			case WID_FCM_LOCATE_GATE: {
				if (this->selected_route_id != 0) {
					const InterServerRoute *route = service.GetRoute(this->selected_route_id);
					if (route != nullptr) {
						TileIndex gate_tile = PortalRegistry::ResolveGateTile(route->source_gate_id, route->source_world);
						if (gate_tile != INVALID_TILE) {
							ScrollMainWindowToTile(gate_tile);
						}
					}
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
