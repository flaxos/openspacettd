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

#include "../safeguards.h"

enum class CorporateHQTab : uint8_t {
	Overview = 0,
	Stockpiles = 1,
	LogisticsHubs = 2,
	Fabrication = 3,
	TechTree = 4,
};

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

	CorporateHQWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_CHQ_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->company = (window_number != 0 && window_number < MAX_COMPANIES) ?
			CompanyID(static_cast<uint8_t>(window_number)) : _local_company;
		if (this->company == CompanyID::Invalid() || !Company::IsValidID(this->company)) {
			this->company = _local_company;
		}
		this->FinishInitNested(this->company.base());
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
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
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnPaint() override
	{
		bool has_hq = CorporateHQManager::HasHQ(this->company);
		const CorporateHQProfile *profile = CorporateHQManager::GetHQ(this->company);

		this->SetWidgetDisabledState(WID_CHQ_LOCATE, !has_hq || profile == nullptr || profile->tile == INVALID_TILE);
		this->SetWidgetDisabledState(WID_CHQ_UPGRADE, !has_hq || (profile != nullptr && profile->tier >= CorporateHQTier::CST_Arcology));
		this->SetWidgetDisabledState(WID_CHQ_TECH_RESEARCH_BTN, this->active_tab != CorporateHQTab::TechTree || !has_hq);
		this->SetWidgetDisabledState(WID_CHQ_TECH_BUDGET_BTN, this->active_tab != CorporateHQTab::TechTree || !has_hq);

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
					DrawString(tr, "Requirements: Phase 1 Core World, >= 3 distinct world phases presence, >= 5,000,000 Cr net worth.", TextColour::White);
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
				DrawString(tr, "Commonwealth Corporate Directorate — Logistics Hub inventory guarantees local fabrication reserves before export.", TextColour::Silver, AlignmentH::Centre);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
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
				CorporateHQManager::UpgradeHQTier(this->company);
				this->SetDirty();
				break;
			}
		}
	}
};

void ShowCorporateHQ(CompanyID company)
{
	if (company == CompanyID::Invalid()) company = _local_company;
	AllocateWindowDescFront<CorporateHQWindow>(_corporate_hq_desc, company.base());
}
