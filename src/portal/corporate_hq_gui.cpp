/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file corporate_hq_gui.cpp Implementation of Corporate Headquarters and Planetary Stockpile window. */

#include "../stdafx.h"
#include "corporate_hq_gui.h"
#include "corporate_hq.h"
#include "company_stockpile.h"
#include "logistics_hub.h"
#include "fabrication_manager.h"
#include "portal_cmd.h"
#include "planet_manager.h"
#include "tech_tree.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../viewport_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../widgets/corporate_hq_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"
#include "../tilehighlight_func.h"
#include "../station_map.h"
#include "../cargotype.h"
#include "../textbuf_gui.h"
#include "../table/sprites.h"

#include <charconv>
#include <algorithm>

#include "../safeguards.h"

enum class CorporateHQTab : uint8_t {
	Overview = 0,
	Stockpiles = 1,
	LogisticsHubs = 2,
	Fabrication = 3,
	TechTree = 4,
};

enum class CorporatePlacement : uint8_t { None, HQ, Hub };

static constexpr std::initializer_list<NWidgetPart> _nested_corporate_hq_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen, WID_CHQ_CAPTION), SetStringTip(STR_CORPORATE_HQ_VIEW_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TAB_OVERVIEW), SetMinimalSize(70, 20), SetStringTip(STR_CORPORATE_HQ_TAB_OVERVIEW, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TAB_STOCKPILES), SetMinimalSize(90, 20), SetStringTip(STR_CORPORATE_HQ_TAB_STOCKPILES, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TAB_LOGISTICS_HUBS), SetMinimalSize(85, 20), SetStringTip(STR_CORPORATE_HQ_TAB_LOGISTICS_HUBS, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TAB_FABRICATION), SetMinimalSize(85, 20), SetStringTip(STR_CORPORATE_HQ_TAB_FABRICATION, STR_EMPTY),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TAB_TECH_TREE), SetMinimalSize(110, 20), SetStringTip(STR_CORPORATE_HQ_TAB_TECH_TREE, STR_EMPTY),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TECH_RESEARCH_BTN), SetMinimalSize(85, 20), SetStringTip(STR_TECH_TREE_BTN_START_RESEARCH, STR_TECH_TREE_BTN_START_RESEARCH_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_TECH_BUDGET_BTN), SetMinimalSize(85, 20), SetStringTip(STR_TECH_TREE_BTN_SET_BUDGET, STR_TECH_TREE_BTN_SET_BUDGET_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_FABRICATION_TOGGLE), SetMinimalSize(65, 20), SetStringTip(STR_FABRICATION_BTN_TOGGLE, STR_FABRICATION_BTN_TOGGLE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_LOCATE), SetMinimalSize(55, 20), SetStringTip(STR_CORPORATE_HQ_BTN_LOCATE, STR_CORPORATE_HQ_BTN_LOCATE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_UPGRADE), SetMinimalSize(55, 20), SetStringTip(STR_CORPORATE_HQ_BTN_UPGRADE, STR_CORPORATE_HQ_BTN_UPGRADE_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_BUILD_HQ), SetFill(1, 0), SetStringTip(STR_CORPORATE_HQ_BTN_BUILD_HQ, STR_CORPORATE_HQ_BTN_BUILD_HQ_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_BUILD_HUB), SetFill(1, 0), SetStringTip(STR_CORPORATE_HQ_BTN_BUILD_HUB, STR_CORPORATE_HQ_BTN_BUILD_HUB_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_SELECT_HUB), SetFill(1, 0), SetStringTip(STR_CORPORATE_HQ_BTN_SELECT_HUB, STR_CORPORATE_HQ_BTN_SELECT_HUB_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_SELECT_CARGO), SetFill(1, 0), SetStringTip(STR_CORPORATE_HQ_BTN_SELECT_CARGO, STR_CORPORATE_HQ_BTN_SELECT_CARGO_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_CHQ_SET_RESERVE), SetFill(1, 0), SetStringTip(STR_CORPORATE_HQ_BTN_SET_RESERVE, STR_CORPORATE_HQ_BTN_SET_RESERVE_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_CHQ_HEADER_PANEL), SetMinimalSize(740, 60), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_CHQ_MAIN_PANEL), SetMinimalSize(728, 280), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_CHQ_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_CHQ_STATUS_BAR), SetMinimalSize(728, 24), SetFill(1, 0), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _corporate_hq_desc(
	WindowPosition::Automatic, "view_corporate_hq", 740, 400,
	WindowClass::CorporateHQ, WindowClass::None,
	{},
	_nested_corporate_hq_widgets
);

struct CorporateHQWindow : Window {
	CompanyID company = CompanyID::Invalid();
	CorporateHQTab active_tab = CorporateHQTab::Overview;
	TechID selected_tech = TECH_TRACTION_1;
	Scrollbar *vscroll = nullptr;
	CorporatePlacement placement = CorporatePlacement::None;
	uint32_t selected_hub_id = 0;
	CargoType selected_cargo = CargoType{0};
	uint32_t pending_reserve_hub_id = 0;
	CargoType pending_reserve_cargo = CargoType{0};
	std::string status_message = "Select a Core World site or owned rail station to establish facilities.";

	CorporateHQWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_CHQ_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->company = (window_number < MAX_COMPANIES) ?
			CompanyID(static_cast<uint8_t>(window_number)) : _local_company;
		if (this->company == CompanyID::Invalid() || !Company::IsValidID(this->company)) {
			this->company = _local_company;
		}
		this->FinishInitNested(this->company.base());
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_CHQ_STATUS_BAR) return this->status_message;
		if (widget == WID_CHQ_CAPTION) {
			const Company *c = Company::GetIfValid(this->company);
			std::string comp_name = (c != nullptr) ? c->name : "Interstellar Corporation";
			return fmt::format("Corporate Headquarters — {}", comp_name);
		}
		if (widget == WID_CHQ_TECH_BUDGET_BTN) {
			uint32_t b = TechTreeManager::GetMonthlyBudget(this->company);
			if (b == 0) return "Budget: 0 Cr";
			return fmt::format("Budget: {:L} Cr", b);
		}
		if (widget == WID_CHQ_SELECT_HUB && this->selected_hub_id != 0) return fmt::format("Hub #{}", this->selected_hub_id);
		if (widget == WID_CHQ_SELECT_CARGO && IsValidCargoType(this->selected_cargo)) {
			return fmt::format("Cargo: {}", GetString(CargoSpec::Get(this->selected_cargo)->name));
		}
		if (widget == WID_CHQ_SET_RESERVE && this->selected_hub_id != 0 && IsValidCargoType(this->selected_cargo)) {
			return fmt::format("Reserve: {}", LogisticsHubManager::GetReserveFloor(this->selected_hub_id, this->selected_cargo));
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnPaint() override
	{
		bool has_hq = CorporateHQManager::HasHQ(this->company);
		const CorporateHQProfile *profile = CorporateHQManager::GetHQ(this->company);
		bool own_company = Company::IsValidID(_local_company) && this->company == _local_company;
		const LogisticsHub *selected_hub = LogisticsHubManager::GetHub(this->selected_hub_id);
		if (selected_hub == nullptr || selected_hub->company_id != this->company) this->selected_hub_id = 0;
		bool has_owned_hub = false;
		for (const auto &hub : LogisticsHubManager::GetAllHubs()) if (hub.company_id == this->company) { has_owned_hub = true; break; }

		this->SetWidgetDisabledState(WID_CHQ_LOCATE, !has_hq || profile == nullptr || profile->tile == INVALID_TILE);
		this->SetWidgetDisabledState(WID_CHQ_UPGRADE, !own_company || !has_hq || (profile != nullptr && profile->tier >= CorporateHQTier::CST_Arcology));
		this->SetWidgetDisabledState(WID_CHQ_TECH_RESEARCH_BTN, !own_company || this->active_tab != CorporateHQTab::TechTree || !has_hq);
		this->SetWidgetDisabledState(WID_CHQ_TECH_BUDGET_BTN, !own_company || this->active_tab != CorporateHQTab::TechTree || !has_hq);
		this->SetWidgetDisabledState(WID_CHQ_FABRICATION_TOGGLE, !own_company);
		this->SetWidgetDisabledState(WID_CHQ_BUILD_HQ, !own_company || has_hq);
		this->SetWidgetDisabledState(WID_CHQ_BUILD_HUB, !own_company);
		this->SetWidgetDisabledState(WID_CHQ_SELECT_HUB, !own_company || !has_owned_hub);
		this->SetWidgetDisabledState(WID_CHQ_SELECT_CARGO, !own_company || this->selected_hub_id == 0);
		this->SetWidgetDisabledState(WID_CHQ_SET_RESERVE, !own_company || this->selected_hub_id == 0 || !IsValidCargoType(this->selected_cargo));

		if (this->active_tab == CorporateHQTab::Stockpiles) {
			auto stockpiles = StockpileManager::GetAllStockpiles();
			size_t comp_count = 0;
			for (const auto &sp : stockpiles) {
				if (sp.company_id == this->company) comp_count++;
			}
			this->vscroll->SetCount(comp_count);
		} else if (this->active_tab == CorporateHQTab::LogisticsHubs) {
			auto hubs = LogisticsHubManager::GetAllHubs();
			size_t comp_count = 0;
			for (const auto &h : hubs) {
				if (h.company_id == this->company) comp_count++;
			}
			this->vscroll->SetCount(comp_count);
		} else if (this->active_tab == CorporateHQTab::TechTree) {
			this->vscroll->SetCount(TechTreeManager::GetAllNodes().size());
		} else {
			this->vscroll->SetCount(0);
		}
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const CorporateHQProfile *profile = CorporateHQManager::GetHQ(this->company);
		const Company *comp = Company::GetIfValid(this->company);

		switch (widget) {
			case WID_CHQ_HEADER_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (profile == nullptr) {
					DrawString(tr, STR_CORPORATE_HQ_STATUS_NOT_FOUNDED, TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;
					DrawString(tr, "Core World HQ: costs 2,500,000 Cr; requires 5,000,000 Cr cash before building.", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;
					DrawString(tr, "Presence: own rail stations on Developed and Frontier Worlds, or register equivalent charter presence.", TextColour::White);
					return;
				}

				std::string tier_str = "Regional Branch";
				switch (profile->tier) {
					case CorporateHQTier::RegionalBranch: tier_str = "Tier 1: Regional Branch Campus"; break;
					case CorporateHQTier::PlanetaryHQ:    tier_str = "Tier 2: Planetary Headquarters"; break;
					case CorporateHQTier::Interstellar:   tier_str = "Tier 3: Interstellar Corporation Campus"; break;
					case CorporateHQTier::CST_Arcology:   tier_str = "Tier 4: Commonwealth CST Arcology Nexus"; break;
				}

				const PlanetRegion *region = PlanetManager::GetRegion(profile->world_id);
				std::string world_name = (region != nullptr) ? region->name : "Core World";

				std::string line1 = fmt::format("Campus: {}  |  Advancement: {}", profile->campus_name, tier_str);
				DrawString(tr, line1, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal);

				int64_t money = comp != nullptr ? static_cast<int64_t>(comp->money) : 0;
				std::string line2 = fmt::format("Founded World: {} (Tile {})  |  Corporate Treasury: {:L} Cr", world_name, profile->tile.base(), money);
				DrawString(tr, line2, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				bool fab_active = FabricationManager::IsFabricateFromStockpileEnabled(this->company);
				std::string line3 = fab_active ?
					"In-Kind Fabrication: ACTIVE (80% cash discount from local planetary stockpile)" :
					"In-Kind Fabrication: INACTIVE (Standard commercial cash purchase)";
				DrawString(tr, line3, fab_active ? TextColour::Green : TextColour::Silver);
				break;
			}

			case WID_CHQ_MAIN_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

				if (this->active_tab == CorporateHQTab::Overview) {
					DrawString(tr, "Corporate Overview & Production Architecture", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;

					DrawString(tr, "1. Dual Construction Accounting Mode (Factorio / Captain of Industry Style):", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - Standard builds draw cash from company treasury.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - If planetary stockpile holds required Bill of Materials (BOM), in-kind fabrication grants 70-85% cash discount!", TextColour::Green);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;

					DrawString(tr, "2. Bi-Directional Dedicated Logistics Hubs (Company Warehouses):", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - Attached to rail stations. Consists unloading deposit cargo into the planetary stockpile ledger.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - Outgoing export consists draw surplus inventory above the configurable reserve floor.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;
					DrawString(tr, "   - Use normal unload/load orders for stockpile exchange; Transfer and No Unload retain their native meaning.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;

					DrawString(tr, "3. Two-Tier Data Crystal Life Cycle:", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - Blank Monocrystalline Substrate is synthesized from high-purity silicon and rare silicates.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - Tier A (Consumer): Encrypted mail replacement delivering planetary prosperity to Metropolises.", TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "   - Tier B (Research): Quantum-enriched crystals imprinted with advanced math proofs at frontier observatories.", TextColour::Gold);
				} else if (this->active_tab == CorporateHQTab::Stockpiles) {
					DrawString(tr, "World              | Steel (Tons) | Ballast (Tons) | Goods/Wiring | Valuables/Chips | Crystals", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + 4;

					auto stockpiles = StockpileManager::GetAllStockpiles();
					size_t row = 0;
					for (const auto &sp : stockpiles) {
						if (sp.company_id != this->company) continue;

						const PlanetRegion *region = PlanetManager::GetRegion(sp.world_id);
						std::string wname = (region != nullptr) ? region->name : fmt::format("World #{}", sp.world_id.base());

						CargoType c_steel = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
						CargoType c_ballast = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
						CargoType c_wire = StockpileManager::RoleToDefaultCargo(FabricationRole::Wiring);
						CargoType c_chips = StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics);
						CargoType c_crystals = StockpileManager::RoleToDefaultCargo(FabricationRole::EnrichedCrystals);

						std::string row_str = fmt::format("{:<18} | {:<12} | {:<14} | {:<12} | {:<15} | {:<10}",
							wname, sp.GetStock(c_steel), sp.GetStock(c_ballast), sp.GetStock(c_wire), sp.GetStock(c_chips), sp.GetStock(c_crystals));

						DrawString(tr, row_str, TextColour::White);
						tr.top += GetCharacterHeight(FontSize::Normal) + 2;
						row++;
					}

					if (row == 0) {
						DrawString(tr, "No active planetary stockpiles with material inventory. Deliver cargo to a Logistics Hub to stock supplies.", TextColour::Silver);
					}
				} else if (this->active_tab == CorporateHQTab::LogisticsHubs) {
					DrawString(tr, "Hub ID | Station Name             | World          | Total Inflow | Total Outflow | Buffer Reserve", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + 4;

					auto hubs = LogisticsHubManager::GetAllHubs();
					size_t row = 0;
					for (const auto &h : hubs) {
						if (h.company_id != this->company) continue;

						const PlanetRegion *region = PlanetManager::GetRegion(h.world_id);
						std::string wname = (region != nullptr) ? region->name : fmt::format("World #{}", h.world_id.base());

						std::string row_str = fmt::format("#{:<5} | {:<24} | {:<14} | {:<12} | {:<13} | Active Protected",
							h.hub_id, h.name, wname, h.total_deposited, h.total_dispatched);

						DrawString(tr, row_str, TextColour::White);
						tr.top += GetCharacterHeight(FontSize::Normal) + 2;
						row++;
					}

					if (row == 0) {
						DrawString(tr, "No dedicated logistics hubs registered for this company. Build Logistics Hubs at freight stations to buffer cargo.", TextColour::Silver);
					}
				} else if (this->active_tab == CorporateHQTab::Fabrication) {
					bool fab_active = FabricationManager::IsFabricateFromStockpileEnabled(this->company);
					std::string mode_status = fab_active ?
						"Status: ACTIVE — Construction automatically draws physical BOM from local world stockpile (80% cash discount)" :
						"Status: INACTIVE — Standard commercial purchase (100% cash from corporate bank balance)";
					DrawString(tr, mode_status, fab_active ? TextColour::Green : TextColour::Silver);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;

					DrawString(tr, "Standard Infrastructure & Rolling Stock Bill of Materials (BOM) Catalog:", TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + 4;

					DrawString(tr, "• Pioneer Standard Rail:     2 Ballast (Gravel/Stone) + 1 Structural Metal (Steel)  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• Catenary Electric Rail:    2 Ballast + 1 Structural Metal + 1 Wiring (Copper Wire)  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• High-Speed Monorail:       4 Ballast + 2 Structural Metal + 1 Wiring  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• CST Vacuum Maglev:         2 Superalloys + 2 Wiring + 1 Electronics (Microchips)  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• Track Signals:             1 Structural Metal + 1 Wiring  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• Train Maintenance Depots:  10 Structural Metal + 5 Ballast  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• Locomotives & Traction:    30–50 Metal/Superalloys + 10–25 Wiring/Electronics  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 2;

					DrawString(tr, "• Freight & Passenger Cars:  10 Structural Metal + 2 Composites  [80% cash discount]", TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;

					DrawString(tr, "Tip: Use Logistics Hubs at regional terminals to ingest freight into the world's company stockpile.", TextColour::Gold);
				} else if (this->active_tab == CorporateHQTab::TechTree) {
					TechID active = TechTreeManager::GetActiveProject(this->company);
					const TechProjectNode *active_node = TechTreeManager::GetNode(active);
					uint32_t accumulated = TechTreeManager::GetAccumulatedRP(this->company);
					uint32_t cost = active_node ? active_node->cost_rp : 100;
					uint32_t pct = (cost > 0) ? (accumulated * 100) / cost : 0;
					std::string status_line = (active == TECH_NONE) ?
						"Active Research: None. Select an available technology below and click 'Start Research'." :
						fmt::format("Active Research: {}  [Progress: {} / {} RP ({}%)]", active_node->name, accumulated, cost, pct);
					DrawString(tr, status_line, (active == TECH_NONE) ? TextColour::Silver : TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal) + 4;

					const CorporateHQProfile *hq = CorporateHQManager::GetHQ(this->company);
					WorldID hq_world = hq ? hq->world_id : INVALID_WORLD;
					uint32_t cr_stock = (hq_world != INVALID_WORLD) ? StockpileManager::GetStock(hq_world, this->company, StockpileManager::RoleToDefaultCargo(FabricationRole::EnrichedCrystals)) : 0;
					uint32_t el_stock = (hq_world != INVALID_WORLD) ? StockpileManager::GetStock(hq_world, this->company, StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics)) : 0;
					uint32_t budget = TechTreeManager::GetMonthlyBudget(this->company);
					DrawString(tr, fmt::format("HQ Stockpile: {} Enriched Crystals (10 RP/ea) | {} Electronics (5 RP/ea) | Budget: {:L} Cr/mo (1 RP/1k Cr)", cr_stock, el_stock, budget), TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal) + 6;

					auto render_branch = [&](TechBranch branch, const char *title) {
						DrawString(tr, fmt::format("─── {} ───", title), TextColour::Gold);
						tr.top += GetCharacterHeight(FontSize::Normal) + 2;
						auto nodes = TechTreeManager::GetNodesByBranch(branch);
						for (const auto &node : nodes) {
							bool unlocked = TechTreeManager::IsTechUnlocked(this->company, node.id);
							bool is_active = (active == node.id);
							std::string err;
							bool can_res = TechTreeManager::CanResearch(this->company, node.id, err);

							std::string state_tag;
							TextColour color = TextColour::Silver;
							if (unlocked) {
								state_tag = "[RESEARCHED]";
								color = TextColour::Green;
							} else if (is_active) {
								state_tag = fmt::format("[RESEARCHING - {}%]", pct);
								color = TextColour::Gold;
							} else if (can_res) {
								state_tag = fmt::format("[AVAILABLE - {} RP]", node.cost_rp);
								color = TextColour::White;
							} else {
								state_tag = "[LOCKED]";
								color = TextColour::Grey;
							}

							std::string sel_marker = (this->selected_tech == node.id) ? "► " : "  ";
							DrawString(tr, fmt::format("{}{:<26} {:<22} — {}", sel_marker, node.name, state_tag, node.unlock_summary), color);
							tr.top += GetCharacterHeight(FontSize::Normal) + 1;
						}
						tr.top += 4;
					};

					render_branch(TechBranch::Traction, "Traction & Propulsion");
					render_branch(TechBranch::PortalPhysics, "Wormhole & Portal Physics");
					render_branch(TechBranch::Materials, "Materials & Fabrication");
				}
				break;
			}

			case WID_CHQ_STATUS_BAR: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				DrawString(tr, this->status_message, TextColour::Silver, AlignmentH::Centre);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if ((widget == WID_CHQ_UPGRADE || widget == WID_CHQ_TECH_RESEARCH_BTN || widget == WID_CHQ_TECH_BUDGET_BTN || widget == WID_CHQ_FABRICATION_TOGGLE ||
			widget == WID_CHQ_BUILD_HQ || widget == WID_CHQ_BUILD_HUB || widget == WID_CHQ_SELECT_HUB || widget == WID_CHQ_SELECT_CARGO || widget == WID_CHQ_SET_RESERVE) &&
			(!Company::IsValidID(_local_company) || this->company != _local_company)) return;
		switch (widget) {
			case WID_CHQ_BUILD_HQ:
				/* Installing a tool aborts the previous one, including our own. */
				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_RECT, this);
				this->placement = CorporatePlacement::HQ;
				this->status_message = "HQ: 2,500,000 Cr. Select a Core World site; requires 5,000,000 Cr cash and three phases of presence.";
				this->SetDirty();
				break;
			case WID_CHQ_BUILD_HUB:
				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_RECT, this);
				this->placement = CorporatePlacement::Hub;
				this->status_message = "Hub: 75,000 Cr. Select one of your rail station platforms.";
				this->SetDirty();
				break;
			case WID_CHQ_SELECT_HUB: {
				std::vector<uint32_t> ids;
				for (const auto &hub : LogisticsHubManager::GetAllHubs()) if (hub.company_id == this->company) ids.push_back(hub.hub_id);
				if (ids.empty()) break;
				auto it = std::find(ids.begin(), ids.end(), this->selected_hub_id);
				this->selected_hub_id = it == ids.end() || ++it == ids.end() ? ids.front() : *it;
				this->status_message = fmt::format("Selected Hub #{}.", this->selected_hub_id);
				this->SetDirty();
				break;
			}
			case WID_CHQ_SELECT_CARGO:
				for (uint i = 1; i <= NUM_CARGO; ++i) {
					CargoType candidate{static_cast<uint8_t>((this->selected_cargo + i) % NUM_CARGO)};
					if (IsValidCargoType(candidate)) { this->selected_cargo = candidate; break; }
				}
				this->SetDirty();
				break;
			case WID_CHQ_SET_RESERVE:
				if (this->selected_hub_id != 0 && IsValidCargoType(this->selected_cargo)) {
					this->pending_reserve_hub_id = this->selected_hub_id;
					this->pending_reserve_cargo = this->selected_cargo;
					ShowQueryString(fmt::format("{}", LogisticsHubManager::GetReserveFloor(this->selected_hub_id, this->selected_cargo)),
						STR_CORPORATE_HQ_RESERVE_CAPTION, 11, this, CS_NUMERAL, QueryStringFlag::AcceptUnchanged);
				}
				break;
			case WID_CHQ_TAB_OVERVIEW:
				this->active_tab = CorporateHQTab::Overview;
				this->SetDirty();
				break;

			case WID_CHQ_TAB_STOCKPILES:
				this->active_tab = CorporateHQTab::Stockpiles;
				this->SetDirty();
				break;

			case WID_CHQ_TAB_LOGISTICS_HUBS:
				this->active_tab = CorporateHQTab::LogisticsHubs;
				this->SetDirty();
				break;

			case WID_CHQ_TAB_FABRICATION:
				this->active_tab = CorporateHQTab::Fabrication;
				this->SetDirty();
				break;

			case WID_CHQ_TAB_TECH_TREE:
				this->active_tab = CorporateHQTab::TechTree;
				this->SetDirty();
				break;

			case WID_CHQ_MAIN_PANEL:
				if (this->active_tab == CorporateHQTab::TechTree) {
					const auto &nodes = TechTreeManager::GetAllNodes();
					if (!nodes.empty()) {
						size_t cur = 0;
						for (size_t i = 0; i < nodes.size(); ++i) {
							if (nodes[i].id == this->selected_tech) {
								cur = i;
								break;
							}
						}
						this->selected_tech = nodes[(cur + 1) % nodes.size()].id;
						this->SetDirty();
					}
				}
				break;

			case WID_CHQ_TECH_RESEARCH_BTN:
				if (this->selected_tech != TECH_NONE) {
					Command<Commands::SelectResearchProject>::Post(this->selected_tech);
				}
				break;

			case WID_CHQ_TECH_BUDGET_BTN: {
				uint32_t cur_b = TechTreeManager::GetMonthlyBudget(this->company);
				uint32_t next_b = 0;
				if (cur_b == 0) next_b = 25000;
				else if (cur_b <= 25000) next_b = 50000;
				else if (cur_b <= 50000) next_b = 100000;
				else if (cur_b <= 100000) next_b = 250000;
				else next_b = 0;
				Command<Commands::SetResearchBudget>::Post(next_b);
				break;
			}

			case WID_CHQ_FABRICATION_TOGGLE: {
				bool cur = FabricationManager::IsFabricateFromStockpileEnabled(this->company);
				Command<Commands::SetFabricationMode>::Post(!cur);
				break;
			}

			case WID_CHQ_LOCATE: {
				const CorporateHQProfile *profile = CorporateHQManager::GetHQ(this->company);
				if (profile != nullptr && profile->tile != INVALID_TILE) {
					ScrollMainWindowToTile(profile->tile);
				}
				break;
			}

			case WID_CHQ_UPGRADE: {
				const CorporateHQProfile *profile = CorporateHQManager::GetHQ(this->company);
				if (profile != nullptr && profile->tier < CorporateHQTier::CST_Arcology) {
					CorporateHQTier next = static_cast<CorporateHQTier>(static_cast<uint8_t>(profile->tier) + 1);
					Command<Commands::UpgradeCorporateHQ>::Post(this->company, next);
				}
				break;
			}
		}
	}

	void OnQueryTextFinished(std::optional<std::string> text) override
	{
		if (!text.has_value()) return;
		uint32_t amount = 0;
		const auto parsed = std::from_chars(text->data(), text->data() + text->size(), amount);
		if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size()) {
			this->status_message = "Reserve must be a whole number from 0 to 4294967295.";
		} else if (this->pending_reserve_hub_id != 0 && IsValidCargoType(this->pending_reserve_cargo)) {
			if (Command<Commands::SetLogisticsHubReserve>::Post(STR_ERROR_CAN_T_SET_LOGISTICS_RESERVE,
				this->pending_reserve_hub_id, this->pending_reserve_cargo, amount)) {
				this->status_message = fmt::format("Reserve change requested: {} units for Hub #{}.", amount, this->pending_reserve_hub_id);
			}
		}
		this->SetDirty();
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (this->company != _local_company) return;
		CorporatePlacement action = this->placement;
		this->placement = CorporatePlacement::None;
		ResetObjectToPlace();
		if (action == CorporatePlacement::HQ) {
			if (Command<Commands::PlaceCorporateHQ>::Post(STR_ERROR_CAN_T_PLACE_CORPORATE_HQ, tile, "Corporate HQ Campus")) {
				this->status_message = "HQ placement requested; check the map and company treasury.";
			} else {
				this->status_message = "HQ site rejected; check Core World, presence and funds.";
			}
		} else if (action == CorporatePlacement::Hub) {
			if (!IsTileType(tile, TileType::Station) || !IsRailStation(tile) || GetTileOwner(tile) != this->company) {
				this->status_message = "Select one of your rail station platform tiles.";
			} else if (Command<Commands::BuildLogisticsHub>::Post(STR_ERROR_CAN_T_BUILD_LOGISTICS_HUB,
				tile, GetStationIndex(tile), "")) {
				this->status_message = "Hub construction requested for the selected station.";
			} else {
				this->status_message = "Hub site rejected; check station ownership and attachment.";
			}
		}
		this->SetDirty();
	}

	void OnPlaceObjectAbort() override
	{
		if (this->placement != CorporatePlacement::None) this->status_message = "Facility site selection cancelled.";
		this->placement = CorporatePlacement::None;
		this->SetDirty();
	}
};

void ShowCorporateHQ(CompanyID company)
{
	if (company == CompanyID::Invalid()) company = _local_company;
	AllocateWindowDescFront<CorporateHQWindow>(_corporate_hq_desc, company.base());
}
