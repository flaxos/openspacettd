/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file trade_ledger_gui.cpp Implementation of Empire Supply Chain Matrix and Trade Ledger window. */

#include "../stdafx.h"
#include "trade_ledger_gui.h"
#include "universe_authority.h"
#include "planet_manager.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../widgets/trade_ledger_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"

#include "../safeguards.h"

enum class TradeLedgerTab : uint8_t {
	SupplyChain = 0,
	TradeLedger = 1,
};

static constexpr std::initializer_list<NWidgetPart> _nested_trade_ledger_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen, WID_TL_CAPTION), SetStringTip(STR_TRADE_LEDGER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_TL_TAB_SUPPLY_CHAIN), SetMinimalSize(150, 20), SetStringTip(STR_TRADE_LEDGER_TAB_SUPPLY_CHAIN, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_TL_TAB_TRADE_LEDGER), SetMinimalSize(180, 20), SetStringTip(STR_TRADE_LEDGER_TAB_TRADE_LEDGER, STR_EMPTY),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_TL_REFRESH), SetMinimalSize(80, 20), SetStringTip(STR_TRADE_LEDGER_REFRESH, STR_TRADE_LEDGER_REFRESH_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_TL_HEADER_PANEL), SetMinimalSize(620, 48), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_TL_MAIN_PANEL), SetMinimalSize(608, 220), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_TL_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_TL_DETAILS_PANEL), SetMinimalSize(620, 80), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _trade_ledger_desc(
	WindowPosition::Automatic, "view_trade_ledger", 620, 420,
	WindowClass::TradeLedger, WindowClass::None,
	{},
	_nested_trade_ledger_widgets
);

struct TradeLedgerWindow : Window {
	TradeLedgerTab active_tab = TradeLedgerTab::SupplyChain;
	Scrollbar *vscroll = nullptr;
	WorldID selected_world = INVALID_WORLD;

	TradeLedgerWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_TL_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);

		auto balances = UniverseAuthorityService::Instance().GetAllTradeBalances();
		if (!balances.empty()) {
			this->selected_world = balances.begin()->first;
		}
	}

	void OnPaint() override
	{
		auto &service = UniverseAuthorityService::Instance();
		if (this->active_tab == TradeLedgerTab::TradeLedger) {
			auto balances = service.GetAllTradeBalances();
			this->vscroll->SetCount(balances.size());
		} else {
			this->vscroll->SetCount(0);
		}

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		auto &service = UniverseAuthorityService::Instance();

		switch (widget) {
			case WID_TL_HEADER_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto matrix = service.GetEmpireSupplyChainMatrix();
				auto audit = service.GetCommodityAudit();

				std::string line1 = fmt::format("Empire Total Interplanetary Flow: {:L} units  |  Cumulative Trade Tariffs: Cr {:L}",
					matrix.total_interplanetary_cargo, matrix.total_tariffs_generated);
				DrawString(tr, line1, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				bool conserved = audit.IsConserved();
				std::string status_str = conserved ?
					fmt::format("Ledger Status: CONSERVED (Zero Duplication/Loss) - Initial: {:L}, Completed: {:L}, In-Transit: {:L}",
						audit.total_cargo_initiated, audit.total_cargo_completed, audit.total_cargo_in_transit) :
					fmt::format("Ledger Status: VIOLATION DETECTED - Initial: {:L} != Completed: {:L} + In-Transit: {:L}",
						audit.total_cargo_initiated, audit.total_cargo_completed, audit.total_cargo_in_transit);
				DrawString(tr, status_str, conserved ? TextColour::Green : TextColour::Red);
				break;
			}

			case WID_TL_MAIN_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

				if (this->active_tab == TradeLedgerTab::SupplyChain) {
					auto matrix = service.GetEmpireSupplyChainMatrix();

					DrawString(tr, "Empire Developmental Phase Distribution & Infrastructure Feeder Flows:", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(4);

					std::string p3_2 = fmt::format("  • Frontier -> Refinery (Phase 3 -> 2): {:L} units  [Raw Mineral & Biomass Feeder]",
						matrix.frontier_to_refinery_cargo);
					DrawString(tr, p3_2, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(2);

					std::string p2_1 = fmt::format("  • Refinery -> Core (Phase 2 -> 1):     {:L} units  [Refined Superalloys & Chemical Inputs]",
						matrix.refinery_to_core_cargo);
					DrawString(tr, p2_1, TextColour::LightBlue);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(2);

					std::string p3_1 = fmt::format("  • Frontier -> Core (Phase 3 -> 1):     {:L} units  [Direct Megacity Sustenance & Aggregates]",
						matrix.frontier_to_core_cargo);
					DrawString(tr, p3_1, TextColour::Orange);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(2);

					std::string core_exp = fmt::format("  • Core Megacity Exports:               {:L} units  [Consumer Goods, Avionics & Technology]",
						matrix.core_export_cargo);
					DrawString(tr, core_exp, TextColour::Yellow);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(6);

					DrawString(tr, "Dedicated Extraction & Interstellar Feeder Facilities:", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(4);

					std::string sp = fmt::format("  • Spaceport Launch Infrastructure:      {:L} units",
						matrix.spaceport_throughput_cargo);
					DrawString(tr, sp, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(2);

					std::string ec = fmt::format("  • Edge Conduit Bulk Pipelines:         {:L} units",
						matrix.edge_conduit_throughput_cargo);
					DrawString(tr, ec, TextColour::White);
				} else {
					/* Trade Ledger tab */
					auto balances = service.GetAllTradeBalances();
					if (balances.empty()) {
						DrawString(tr, STR_TRADE_LEDGER_NO_WORLDS, TextColour::Silver, AlignmentH::Centre);
						return;
					}

					/* Draw column headers */
					std::string header = fmt::format("{:<12} {:<24} {:<20} {:<20}",
						"World", "Net Trade Balance", "Total Exported", "Total Imported");
					DrawString(tr, header, TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(4);

					int pos = -this->vscroll->GetPosition();
					const int cap = this->vscroll->GetCapacity();
					const int item_height = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(6);

					for (const auto &[wid, summary] : balances) {
						if (IsInsideMM(pos, 0, cap)) {
							Rect item_rect = tr.WithHeight(item_height);
							bool is_selected = (wid == this->selected_world);

							PixelColour bg = is_selected ?
								GetColourGradient(Colours::DarkGreen, Shade::Darker) :
								GetColourGradient(Colours::DarkGreen, Shade::Darkest);
							GfxFillRect(item_rect.Shrink(1), bg);

							uint64_t total_exp = 0;
							for (const auto &[_, c] : summary.exported_cargo) total_exp += c;
							uint64_t total_imp = 0;
							for (const auto &[_, c] : summary.imported_cargo) total_imp += c;

							std::string row_str = fmt::format("World {:<6} {:>12} Cr {:>18} units {:>18} units",
								wid.base(), summary.net_trade_balance_credits, total_exp, total_imp);
							DrawString(item_rect.Shrink(ScaleGUITrad(2)), row_str,
								is_selected ? TextColour::White : TextColour::Silver);
						}
						pos++;
						tr.top += item_height;
					}
				}
				break;
			}

			case WID_TL_DETAILS_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

				if (this->active_tab == TradeLedgerTab::SupplyChain) {
					DrawString(tr, "Commonwealth Interplanetary Economy Architecture:", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "Phase 3 frontier worlds harvest raw resources feeding Phase 2 refineries; manufactured components supply Phase 1 megacity demands.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "Tariffs generate interstellar transfer revenue at 10 Cr/unit across wormhole gateway corridors.", TextColour::Silver);
				} else {
					auto detailed = service.GetDetailedCommodityAudit();
					std::string audit_header = fmt::format("Detailed Conservation Audit Across Active Types (Count: {}):",
						detailed.cargo_initiated.size());
					DrawString(tr, audit_header, TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal);

					if (detailed.cargo_initiated.empty()) {
						DrawString(tr, "No cargo transfers recorded in authority ledger.", TextColour::Silver);
					} else {
						std::string details_line;
						size_t count = 0;
						for (const auto &[cargo, init] : detailed.cargo_initiated) {
							if (count++ >= 4) break;
							uint64_t comp = detailed.cargo_completed.contains(cargo) ? detailed.cargo_completed.at(cargo) : 0;
							uint64_t trans = detailed.cargo_in_transit.contains(cargo) ? detailed.cargo_in_transit.at(cargo) : 0;
							bool ok = (init == comp + trans);
							std::string entry = fmt::format("[Cargo #{:02d}: Init {:L}, Comp {:L}, Trans {:L} - {}] ",
								cargo, init, comp, trans, ok ? "OK" : "LEAK");
							details_line += entry;
						}
						DrawString(tr, details_line, TextColour::White);
					}
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_TL_TAB_SUPPLY_CHAIN:
				this->active_tab = TradeLedgerTab::SupplyChain;
				this->SetDirty();
				break;

			case WID_TL_TAB_TRADE_LEDGER:
				this->active_tab = TradeLedgerTab::TradeLedger;
				this->SetDirty();
				break;

			case WID_TL_REFRESH:
				this->SetDirty();
				break;

			case WID_TL_MAIN_PANEL: {
				if (this->active_tab == TradeLedgerTab::TradeLedger) {
					auto balances = UniverseAuthorityService::Instance().GetAllTradeBalances();
					if (balances.empty()) return;

					const int item_height = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(6);
					int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_TL_MAIN_PANEL, WidgetDimensions::scaled.framerect.top, item_height);
					if (row >= 0 && static_cast<size_t>(row) < balances.size()) {
						auto it = balances.begin();
						std::advance(it, row);
						this->selected_world = it->first;
						this->SetDirty();
					}
				}
				break;
			}
		}
	}
};

void ShowTradeLedger()
{
	AllocateWindowDescFront<TradeLedgerWindow>(_trade_ledger_desc, 0);
}
