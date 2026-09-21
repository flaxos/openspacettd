/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_directory_gui.cpp Implementation of Universe Directory browser window and Commonwealth Galaxy Map. */

#include "../stdafx.h"
#include "universe_directory_gui.h"
#include "universe_authority.h"
#include "universe_graph.h"
#include "prebuilt_trade.h"
#include "planet_manager.h"
#include "portal_cmd.h"
#include "portal_registry.h"
#include "megacity_gui.h"
#include "megacity_manager.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../town.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../widgets/universe_directory_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"

#include "../safeguards.h"

enum class UniverseDirectoryTab : uint8_t {
	Servers = 0,
	GalaxyMap = 1,
};

enum class GalaxyPhaseFilter : uint8_t {
	All = 0,
	Phase1 = 1,
	Phase2 = 2,
	Phase3 = 3,
	Phase4 = 4,
};

static constexpr std::initializer_list<NWidgetPart> _nested_universe_directory_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Blue),
		NWidget(WWT_CAPTION, Colours::Blue, WID_UD_CAPTION), SetStringTip(STR_UNIVERSE_DIRECTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::Blue),
		NWidget(WWT_DEFSIZEBOX, Colours::Blue),
		NWidget(WWT_STICKYBOX, Colours::Blue),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Blue, WID_UD_HEADER_PANEL), SetMinimalSize(110, 36), SetFill(1, 0), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_TAB_SERVERS), SetMinimalSize(70, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_TAB_SERVERS, STR_UNIVERSE_DIRECTORY_TAB_SERVERS_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_TAB_GALAXY_MAP), SetMinimalSize(85, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_TAB_GALAXY_MAP, STR_UNIVERSE_DIRECTORY_TAB_GALAXY_MAP_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_REFRESH), SetMinimalSize(65, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_REFRESH, STR_UNIVERSE_DIRECTORY_REFRESH_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_FILTER_ALL), SetMinimalSize(40, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_FILTER_ALL, STR_UNIVERSE_DIRECTORY_FILTER_ALL_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_FILTER_P1), SetMinimalSize(65, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_FILTER_P1, STR_UNIVERSE_DIRECTORY_FILTER_P1_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_FILTER_P2), SetMinimalSize(85, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_FILTER_P2, STR_UNIVERSE_DIRECTORY_FILTER_P2_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_FILTER_P3), SetMinimalSize(80, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_FILTER_P3, STR_UNIVERSE_DIRECTORY_FILTER_P3_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_FILTER_P4), SetMinimalSize(85, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_FILTER_P4, STR_UNIVERSE_DIRECTORY_FILTER_P4_TOOLTIP),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_TRADE_GATE_BTN), SetMinimalSize(95, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_TRADE_GATE, STR_UNIVERSE_DIRECTORY_TRADE_GATE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_MEGACITY_BTN), SetMinimalSize(85, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_MEGACITY, STR_UNIVERSE_DIRECTORY_MEGACITY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_PROMOTE_BTN), SetMinimalSize(85, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_PROMOTE, STR_UNIVERSE_DIRECTORY_PROMOTE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_COLONIZE_BTN), SetMinimalSize(85, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_COLONIZE, STR_UNIVERSE_DIRECTORY_COLONIZE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_JUMP_BTN), SetMinimalSize(75, 22), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_JUMP, STR_UNIVERSE_DIRECTORY_JUMP_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, Colours::Invalid, WID_UD_STACK_MAIN),
			NWidget(WWT_PANEL, Colours::Blue, WID_UD_WORLD_LIST), SetMinimalSize(508, 200), SetFill(1, 1), SetResize(1, 1), EndContainer(),
			NWidget(WWT_PANEL, Colours::Blue, WID_UD_GALAXY_MAP), SetMinimalSize(508, 200), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Blue, WID_UD_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Blue, WID_UD_DETAILS_PANEL), SetMinimalSize(520, 84), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::Blue),
	EndContainer(),
};

static WindowDesc _universe_directory_desc(
	WindowPosition::Automatic, "view_universe_directory", 520, 370,
	WindowClass::UniverseDirectory, WindowClass::None,
	{},
	_nested_universe_directory_widgets
);

/** Window class for browsing Universe Authority registered worlds, online nodes, and Commonwealth galaxy map. */
struct UniverseDirectoryWindow : Window {
	Scrollbar *vscroll = nullptr;
	WorldID selected_world = INVALID_WORLD;
	UniverseDirectoryTab active_tab = UniverseDirectoryTab::Servers;
	GalaxyPhaseFilter phase_filter = GalaxyPhaseFilter::All;
	std::string selected_galaxy_node_id = "world_earth";
	std::vector<const UniverseNode *> displayed_galaxy_nodes{};

	UniverseDirectoryWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		UniverseGraphManager::Instance().EnsureLoaded();
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_UD_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);

		auto worlds = UniverseAuthorityService::Instance().GetWorldDirectoryForGUI();
		if (!worlds.empty()) {
			this->selected_world = worlds.front().world_id;
		}

		this->UpdateGalaxyNodeList();
		this->GetWidget<NWidgetStacked>(WID_UD_STACK_MAIN)->SetDisplayedPlane(0);
	}

	/**
	 * Update list of filtered galaxy nodes based on active phase filter.
	 */
	void UpdateGalaxyNodeList()
	{
		this->displayed_galaxy_nodes.clear();
		auto &mgr = UniverseGraphManager::Instance();
		mgr.EnsureLoaded();
		auto traversed = mgr.TraverseTreeFromRoot("world_earth", GraphFilterMode::PLAYABLE_COMPLETION);

		for (const auto *node : traversed) {
			if (this->phase_filter == GalaxyPhaseFilter::All) {
				this->displayed_galaxy_nodes.push_back(node);
			} else if (this->phase_filter == GalaxyPhaseFilter::Phase1 && node->phase == WorldPhase::Phase1_Core) {
				this->displayed_galaxy_nodes.push_back(node);
			} else if (this->phase_filter == GalaxyPhaseFilter::Phase2 && node->phase == WorldPhase::Phase2_Developed) {
				this->displayed_galaxy_nodes.push_back(node);
			} else if (this->phase_filter == GalaxyPhaseFilter::Phase3 && node->phase == WorldPhase::Phase3_Frontier) {
				this->displayed_galaxy_nodes.push_back(node);
			} else if (this->phase_filter == GalaxyPhaseFilter::Phase4 && node->phase == WorldPhase::Phase4_Expansion) {
				this->displayed_galaxy_nodes.push_back(node);
			}
		}

		if (!this->displayed_galaxy_nodes.empty()) {
			bool found = false;
			for (const auto *n : this->displayed_galaxy_nodes) {
				if (n->world_id == this->selected_galaxy_node_id) {
					found = true;
					break;
				}
			}
			if (!found) {
				this->selected_galaxy_node_id = this->displayed_galaxy_nodes.front()->world_id;
			}
		}
	}

	/**
	 * Switch active directory tab.
	 * @param tab Tab to activate.
	 */
	void SetTab(UniverseDirectoryTab tab)
	{
		this->active_tab = tab;
		this->GetWidget<NWidgetStacked>(WID_UD_STACK_MAIN)->SetDisplayedPlane(static_cast<int>(tab));
		if (tab == UniverseDirectoryTab::GalaxyMap) {
			this->UpdateGalaxyNodeList();
		}
		this->vscroll->SetPosition(0);
		this->SetDirty();
	}

	/**
	 * Set galaxy phase filter.
	 * @param filter Evidentiary phase filter to apply.
	 */
	void SetPhaseFilter(GalaxyPhaseFilter filter)
	{
		this->phase_filter = filter;
		this->UpdateGalaxyNodeList();
		this->vscroll->SetPosition(0);
		this->SetDirty();
	}

	/**
	 * Select a galaxy node for inspection.
	 * @param world_id Canonical world identifier.
	 */
	void SelectGalaxyNode(const std::string &world_id)
	{
		this->selected_galaxy_node_id = world_id;
		this->SetDirty();
	}

	void OnPaint() override
	{
		this->SetWidgetLoweredState(WID_UD_TAB_SERVERS, this->active_tab == UniverseDirectoryTab::Servers);
		this->SetWidgetLoweredState(WID_UD_TAB_GALAXY_MAP, this->active_tab == UniverseDirectoryTab::GalaxyMap);

		if (this->active_tab == UniverseDirectoryTab::Servers) {
			auto worlds = UniverseAuthorityService::Instance().GetWorldDirectoryForGUI();
			this->vscroll->SetCount(worlds.size());

			const RegisteredWorld *selected = nullptr;
			for (const RegisteredWorld &world : worlds) if (world.world_id == this->selected_world) { selected = &world; break; }
			const PlanetRegion *local_reg = (this->selected_world != INVALID_WORLD) ? PlanetManager::GetRegion(this->selected_world) : nullptr;
			WorldPhase phase = (selected != nullptr) ? selected->phase : (local_reg != nullptr ? local_reg->phase : WorldPhase::Phase3_Frontier);
			bool local_action = local_reg != nullptr && Company::IsValidID(_local_company);
			bool can_colonize = local_action && (phase == WorldPhase::Phase4_Expansion);
			bool can_promote = local_action && PlanetManager::CanPromoteWorld(this->selected_world) && (phase != WorldPhase::Phase4_Expansion);
			bool can_view_megacity = (this->selected_world != INVALID_WORLD) && (phase == WorldPhase::Phase1_Core || (selected != nullptr && selected->is_megacity));
			if (!can_view_megacity && this->selected_world != INVALID_WORLD) {
				for (const auto &prof : MegacityManager::GetAllMegacities()) {
					if (prof.world_id == this->selected_world) {
						can_view_megacity = true;
						break;
					}
				}
			}

			this->SetWidgetDisabledState(WID_UD_COLONIZE_BTN, !can_colonize);
			this->SetWidgetDisabledState(WID_UD_PROMOTE_BTN, !can_promote);
			this->SetWidgetDisabledState(WID_UD_MEGACITY_BTN, !can_view_megacity);
			this->SetWidgetDisabledState(WID_UD_JUMP_BTN, this->selected_world == INVALID_WORLD || PlanetManager::GetRegion(this->selected_world) == nullptr);

			this->SetWidgetDisabledState(WID_UD_FILTER_ALL, true);
			this->SetWidgetDisabledState(WID_UD_FILTER_P1, true);
			this->SetWidgetDisabledState(WID_UD_FILTER_P2, true);
			this->SetWidgetDisabledState(WID_UD_FILTER_P3, true);
			this->SetWidgetDisabledState(WID_UD_FILTER_P4, true);
			this->SetWidgetDisabledState(WID_UD_TRADE_GATE_BTN, true);
		} else {
			this->UpdateGalaxyNodeList();
			this->vscroll->SetCount(this->displayed_galaxy_nodes.size());

			this->SetWidgetDisabledState(WID_UD_FILTER_ALL, false);
			this->SetWidgetDisabledState(WID_UD_FILTER_P1, false);
			this->SetWidgetDisabledState(WID_UD_FILTER_P2, false);
			this->SetWidgetDisabledState(WID_UD_FILTER_P3, false);
			this->SetWidgetDisabledState(WID_UD_FILTER_P4, false);
			this->SetWidgetDisabledState(WID_UD_TRADE_GATE_BTN, this->selected_galaxy_node_id.empty());

			this->SetWidgetLoweredState(WID_UD_FILTER_ALL, this->phase_filter == GalaxyPhaseFilter::All);
			this->SetWidgetLoweredState(WID_UD_FILTER_P1, this->phase_filter == GalaxyPhaseFilter::Phase1);
			this->SetWidgetLoweredState(WID_UD_FILTER_P2, this->phase_filter == GalaxyPhaseFilter::Phase2);
			this->SetWidgetLoweredState(WID_UD_FILTER_P3, this->phase_filter == GalaxyPhaseFilter::Phase3);
			this->SetWidgetLoweredState(WID_UD_FILTER_P4, this->phase_filter == GalaxyPhaseFilter::Phase4);

			this->SetWidgetDisabledState(WID_UD_COLONIZE_BTN, true);
			this->SetWidgetDisabledState(WID_UD_PROMOTE_BTN, true);
			this->SetWidgetDisabledState(WID_UD_MEGACITY_BTN, true);
			this->SetWidgetDisabledState(WID_UD_JUMP_BTN, true);
		}

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		auto &service = UniverseAuthorityService::Instance();
		auto worlds = service.GetWorldDirectoryForGUI();

		switch (widget) {
			case WID_UD_HEADER_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (this->active_tab == UniverseDirectoryTab::Servers) {
					size_t online_count = 0;
					int64_t total_cleared = 0;
					for (const auto &w : worlds) {
						if (w.status == WorldOnlineStatus::Online) online_count++;
						total_cleared += w.total_cleared_revenue;
					}

					std::string line1 = fmt::format("Registered Worlds: {}  |  Online Servers: {} / {}  |  Federation Cleared Rev: Cr {:L}",
						worlds.size(), online_count, worlds.size(), total_cleared);
					DrawString(tr, line1, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);

					DrawString(tr, "Central Universe Authority Node: Active  |  Transport Protocol: F4 Lockstep  |  Holding Loops: Automated", TextColour::Silver);
				} else {
					auto &mgr = UniverseGraphManager::Instance();
					std::string line1 = fmt::format("Commonwealth Galaxy Topology: {} Canonical Worlds | 16 Special Bodies | {} Connection Edges",
						mgr.GetNodeCount(), mgr.GetConnectionCount());
					DrawString(tr, line1, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string line2 = fmt::format("Filter Mode: {} | Showing: {} Worlds | Sol Root Tree: Fully Connected",
						GraphFilterModeToString(mgr.GetFilterMode()), this->displayed_galaxy_nodes.size());
					DrawString(tr, line2, TextColour::Gold);
				}
				break;
			}

			case WID_UD_WORLD_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

				if (worlds.empty()) {
					DrawString(tr, STR_UNIVERSE_DIRECTORY_NO_WORLDS, TextColour::Silver, AlignmentH::Centre);
					return;
				}

				int pos = -this->vscroll->GetPosition();
				const int cap = this->vscroll->GetCapacity();
				const int item_height = GetCharacterHeight(FontSize::Normal) * 2 + ScaleGUITrad(8);

				for (const auto &world : worlds) {
					if (IsInsideMM(pos, 0, cap)) {
						Rect item_rect = tr.WithHeight(item_height);
						bool is_selected = (world.world_id == this->selected_world);

						PixelColour bg_col = is_selected ?
							GetColourGradient(Colours::Blue, Shade::Darker) :
							GetColourGradient(Colours::Blue, Shade::Darkest);
						GfxFillRect(item_rect.Shrink(1), bg_col);

						Rect text_rect = item_rect.Shrink(WidgetDimensions::scaled.framerect);

						StringID status_str = STR_UNIVERSE_DIRECTORY_ONLINE;
						TextColour status_col = TextColour::Green;
						if (world.status == WorldOnlineStatus::Maintenance) {
							status_str = STR_UNIVERSE_DIRECTORY_MAINTENANCE;
							status_col = TextColour::Yellow;
						} else if (world.status == WorldOnlineStatus::Unreachable) {
							status_str = STR_UNIVERSE_DIRECTORY_UNREACHABLE;
							status_col = TextColour::Red;
						}

						std::string world_header = fmt::format("World #{}: {} [{} | {}]",
							world.world_id.base(), world.name,
							PlanetManager::GetWorldPhaseName(world.phase),
							PlanetManager::GetWorldBiomeName(world.biome));
						DrawString(text_rect, world_header, is_selected ? TextColour::White : TextColour::Gold);

						int header_w = GetStringBoundingBox(world_header).width;
						Rect status_rect = text_rect.Indent(header_w + ScaleGUITrad(8), _current_text_dir != TD_RTL);
						DrawString(status_rect, status_str, status_col);

						int status_w = GetStringBoundingBox(status_str).width;
						Rect ping_rect = status_rect.Indent(status_w + ScaleGUITrad(6), _current_text_dir != TD_RTL);
						std::string ping_str;
						TextColour ping_col = TextColour::Green;
						if (world.status == WorldOnlineStatus::Online) {
							if (world.ping_ms < 50) ping_col = TextColour::Green;
							else if (world.ping_ms <= 150) ping_col = TextColour::Yellow;
							else ping_col = TextColour::Red;
							ping_str = fmt::format("[{}ms]", world.ping_ms);
						} else {
							ping_col = TextColour::Silver;
							ping_str = "[-]";
						}
						DrawString(ping_rect, ping_str, ping_col);

						bool has_holding = false;
						for (const auto &[ptile, plink] : PortalRegistry::GetAllInterServerPortals()) {
							if (plink.remote_world == world.world_id && plink.is_holding_active) {
								has_holding = true;
								break;
							}
						}
						if (has_holding) {
							int ping_w = GetStringBoundingBox(ping_str).width;
							Rect hold_rect = ping_rect.Indent(ping_w + ScaleGUITrad(6), _current_text_dir != TD_RTL);
							DrawString(hold_rect, "[HOLDING]", TextColour::Red);
						}

						text_rect.top += GetCharacterHeight(FontSize::Normal);

						uint32_t pop = world.population > 0 ? world.population : PlanetManager::GetWorldPopulation(world.world_id);
						std::string metrics = fmt::format("Addr: {}  |  Clients: {}/{}  |  Load: {:.0f}%  |  Trains: {}",
							world.address.empty() ? "Local / In-Process" : world.address,
							world.active_clients, world.max_clients, world.traffic_load_pct, world.active_trains);
						if (world.financial_clearing_balance != 0 || world.total_cleared_revenue != 0) {
							metrics += fmt::format("  |  Clearing: Cr {:+L}", world.financial_clearing_balance);
						}
						if (pop > 0) {
							metrics += fmt::format("  |  Pop: {:L}", pop);
						}
						const MegacityProfile *p_mega = nullptr;
						for (const auto &p : MegacityManager::GetAllMegacities()) {
							if (p.world_id == world.world_id) { p_mega = &p; break; }
						}
						if (p_mega != nullptr) {
							const char *g_str = "Subsistence";
							switch (p_mega->growth_state) {
								case MegacityGrowthState::Starvation:       g_str = "Starvation"; break;
								case MegacityGrowthState::Subsistence:      g_str = "Subsistence"; break;
								case MegacityGrowthState::MetropolitanBoom: g_str = "Boom 1.5x"; break;
								case MegacityGrowthState::HyperGrowth:      g_str = "HyperGrowth 2.0x"; break;
							}
							metrics += fmt::format("  |  [{}]", g_str);
						}
						DrawString(text_rect, metrics, TextColour::Silver);

						tr.top += item_height + ScaleGUITrad(2);
					}
					pos++;
				}
				break;
			}

			case WID_UD_GALAXY_MAP: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (this->displayed_galaxy_nodes.empty()) {
					DrawString(tr, "No Commonwealth worlds match the selected phase filter.", TextColour::Silver, AlignmentH::Centre);
					return;
				}

				int pos = -this->vscroll->GetPosition();
				const int cap = this->vscroll->GetCapacity();
				const int item_height = GetCharacterHeight(FontSize::Normal) * 2 + ScaleGUITrad(8);

				for (const auto *node : this->displayed_galaxy_nodes) {
					if (IsInsideMM(pos, 0, cap)) {
						Rect item_rect = tr.WithHeight(item_height);
						bool is_selected = (node->world_id == this->selected_galaxy_node_id);

						PixelColour bg_col = is_selected ?
							GetColourGradient(Colours::Blue, Shade::Darker) :
							GetColourGradient(Colours::Blue, Shade::Darkest);
						GfxFillRect(item_rect.Shrink(1), bg_col);

						Rect text_rect = item_rect.Shrink(WidgetDimensions::scaled.framerect);

						/* Phase coloring: P1 Gold, P2 Cyan, P3 Green, P4 Grey */
						TextColour phase_col = TextColour::Silver;
						switch (node->phase) {
							case WorldPhase::Phase1_Core:       phase_col = TextColour::Gold; break;
							case WorldPhase::Phase2_Developed:  phase_col = TextColour::LightBlue; break;
							case WorldPhase::Phase3_Frontier:   phase_col = TextColour::Green; break;
							case WorldPhase::Phase4_Expansion:  phase_col = TextColour::Silver; break;
						}

						std::string badge = "[" + EvidenceClassToBadge(node->phase_evidence) + "]";
						std::string conn_badge = "[" + EvidenceClassToBadge(node->connection_evidence) + "]";

						std::string prefix;
						if (node->world_id == "world_earth") {
							prefix = "[SOL ROOT] ";
						} else if (node->connects_to == "world_earth") {
							prefix = "|-- ";
						} else {
							prefix = "|   |-- ";
						}

						std::string line1 = fmt::format("{}{} {}  [{}]  (System: {})",
							prefix, badge, node->display_name,
							PlanetManager::GetWorldPhaseName(node->phase),
							node->system);
						DrawString(text_rect, line1, phase_col);

						text_rect.top += GetCharacterHeight(FontSize::Normal);

						std::string line2 = fmt::format("    Connects: {} via {} {}  |  Role: {}  |  Tariff Mult: {:.2f}x",
							node->connects_to.empty() ? "None" : node->connects_to,
							ConnectionModeToString(node->connection_mode),
							conn_badge,
							node->economic_profile.role,
							node->economic_profile.tariff_multiplier);
						DrawString(text_rect, line2, TextColour::Silver);

						tr.top += item_height + ScaleGUITrad(2);
					}
					pos++;
				}
				break;
			}

			case WID_UD_DETAILS_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				if (this->active_tab == UniverseDirectoryTab::Servers) {
					const RegisteredWorld *selected = nullptr;
					for (const RegisteredWorld &world : worlds) if (world.world_id == this->selected_world) { selected = &world; break; }

					if (selected == nullptr) {
						DrawString(tr, "Select a world from the directory above to inspect server details.", TextColour::Silver);
						return;
					}

					auto trade = service.GetWorldTradeBalance(this->selected_world);

					std::string line1 = fmt::format("Selected: World #{} ({}) [{}] - {}",
						selected->world_id.base(), selected->name,
						PlanetManager::GetWorldBiomeName(selected->biome),
						selected->description.empty() ? "No description provided." : selected->description);
					DrawString(tr, line1, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string token_str;
					for (size_t i = 0; i < 4 && i < selected->content_manifest.size(); ++i) {
						token_str += fmt::format("{:02X}", selected->content_manifest[i]);
					}

					std::string line2 = fmt::format("Manifest Token: 0x{}...  |  Last Heartbeat Tick: {}",
						token_str, selected->last_heartbeat_tick);
					DrawString(tr, line2, TextColour::Gold);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string line_telemetry = fmt::format("Telemetry: Ping {}ms ({})  |  Corridor Traffic Load: {:.1f}%  |  Clearing Balance: Cr {:+L}  (Revenue: Cr {:L})",
						selected->ping_ms,
						selected->ping_ms < 50 ? "Optimal" : (selected->ping_ms <= 150 ? "Fair" : "High Latency"),
						selected->traffic_load_pct,
						selected->financial_clearing_balance,
						selected->total_cleared_revenue);
					DrawString(tr, line_telemetry, selected->ping_ms > 150 ? TextColour::Yellow : TextColour::LightBlue);
					tr.top += GetCharacterHeight(FontSize::Normal);

					bool has_siding = false;
					bool holding_active = false;
					for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
						if (link.remote_world == selected->world_id) {
							if (link.staging_siding_tile != INVALID_TILE) has_siding = true;
							if (link.is_holding_active) holding_active = true;
						}
					}
					if (has_siding || holding_active) {
						std::string line_siding = fmt::format("Holding Loop / Staging Siding: {}  |  Status: {}",
							has_siding ? "Configured" : "Unset",
							holding_active ? "Holding Active (Diverting Traffic to Siding)" : "Normal (Route Clear)");
						DrawString(tr, line_siding, holding_active ? TextColour::Red : TextColour::Green);
						tr.top += GetCharacterHeight(FontSize::Normal);
					}

					const PlanetRegion *local_reg = (this->selected_world != INVALID_WORLD) ? PlanetManager::GetRegion(this->selected_world) : nullptr;
					uint32_t dev_score = local_reg != nullptr ? local_reg->development_score : 0;
					uint32_t threshold = PlanetManager::GetPromotionThreshold(selected->phase);

					if (selected->phase == WorldPhase::Phase4_Expansion) {
						std::string line3 = "Colonization Status: Virgin Wilderness (Phase 4). Click 'Found Colony' to establish a pioneer outpost.";
						DrawString(tr, line3, TextColour::Yellow);
					} else if (selected->phase == WorldPhase::Phase1_Core) {
						const MegacityProfile *p_mega = nullptr;
						for (const auto &p : MegacityManager::GetAllMegacities()) {
							if (p.world_id == selected->world_id) { p_mega = &p; break; }
						}
						std::string mega_str = "Phase 1 Core Metropolis";
						if (p_mega != nullptr) {
							mega_str = fmt::format("Megacity '{}' (Pop: {:L}, Satisf: {:.0f}%)",
								p_mega->town_name, p_mega->population, p_mega->overall_supply_index * 100.0f);
						}
						std::string line3 = fmt::format("Dev Score: {:L} pts | {} | Net Trade: Cr {:L}",
							dev_score, mega_str, trade.net_trade_balance_credits);
						DrawString(tr, line3, TextColour::Green);
					} else {
						std::string line3 = fmt::format("Development: {:L} / {:L} pts ({}) | Net Trade: Cr {:L}",
							dev_score, threshold,
							dev_score >= threshold ? "Ready for Promotion!" : "Deliver cargo to advance",
							trade.net_trade_balance_credits);
						DrawString(tr, line3, dev_score >= threshold ? TextColour::Green : TextColour::LightBlue);
					}
				} else {
					const UniverseNode *sel = UniverseGraphManager::Instance().FindNode(this->selected_galaxy_node_id);
					if (sel == nullptr && !this->displayed_galaxy_nodes.empty()) {
						sel = this->displayed_galaxy_nodes.front();
					}
					if (sel == nullptr) {
						DrawString(tr, "Select a world from the galaxy map to inspect Commonwealth lore and economic profiles.", TextColour::Silver);
						return;
					}

					TextColour phase_col = TextColour::Silver;
					switch (sel->phase) {
						case WorldPhase::Phase1_Core:       phase_col = TextColour::Gold; break;
						case WorldPhase::Phase2_Developed:  phase_col = TextColour::LightBlue; break;
						case WorldPhase::Phase3_Frontier:   phase_col = TextColour::Green; break;
						case WorldPhase::Phase4_Expansion:  phase_col = TextColour::Silver; break;
					}

					std::string line1 = fmt::format("Node: {} ({}) [{}]  |  Biome: {}  |  System: {}  |  Era: {}",
						sel->display_name, sel->canonical_name, sel->world_id,
						PlanetManager::GetWorldBiomeName(sel->biome),
						sel->system, sel->era);
					DrawString(tr, line1, TextColour::White);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string line2 = fmt::format("Phase: {} [Evidence: {}]  |  Link: {} via {} [Evidence: {}] (Rule: {})",
						PlanetManager::GetWorldPhaseName(sel->phase),
						EvidenceClassToString(sel->phase_evidence),
						sel->connects_to.empty() ? "None" : sel->connects_to,
						ConnectionModeToString(sel->connection_mode),
						EvidenceClassToString(sel->connection_evidence),
						sel->source_rule);
					DrawString(tr, line2, phase_col);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string imports_str = sel->economic_profile.primary_imports.empty() ? "None" : "";
					for (const auto &imp : sel->economic_profile.primary_imports) {
						if (!imports_str.empty()) imports_str += ", ";
						imports_str += imp;
					}
					std::string exports_str = sel->economic_profile.primary_exports.empty() ? "None" : "";
					for (const auto &exp : sel->economic_profile.primary_exports) {
						if (!exports_str.empty()) exports_str += ", ";
						exports_str += exp;
					}

					std::string line3 = fmt::format("Economics: Role: {}  |  Base Pop: {:L}  |  Megacity: {}  |  Tariff Mult: {:.2f}x",
						sel->economic_profile.role, sel->economic_profile.base_population,
						sel->economic_profile.megacity_eligible ? "Eligible" : "No",
						sel->economic_profile.tariff_multiplier);
					DrawString(tr, line3, TextColour::LightBlue);
					tr.top += GetCharacterHeight(FontSize::Normal);

					std::string line4 = fmt::format("Imports: [{}]  |  Exports: [{}]", imports_str, exports_str);
					DrawString(tr, line4, TextColour::Silver);
				}
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto &service = UniverseAuthorityService::Instance();

		switch (widget) {
			case WID_UD_REFRESH: {
				this->SetDirty();
				break;
			}

			case WID_UD_TAB_SERVERS: {
				this->SetTab(UniverseDirectoryTab::Servers);
				break;
			}

			case WID_UD_TAB_GALAXY_MAP: {
				this->SetTab(UniverseDirectoryTab::GalaxyMap);
				break;
			}

			case WID_UD_FILTER_ALL: {
				this->SetPhaseFilter(GalaxyPhaseFilter::All);
				break;
			}

			case WID_UD_FILTER_P1: {
				this->SetPhaseFilter(GalaxyPhaseFilter::Phase1);
				break;
			}

			case WID_UD_FILTER_P2: {
				this->SetPhaseFilter(GalaxyPhaseFilter::Phase2);
				break;
			}

			case WID_UD_FILTER_P3: {
				this->SetPhaseFilter(GalaxyPhaseFilter::Phase3);
				break;
			}

			case WID_UD_FILTER_P4: {
				this->SetPhaseFilter(GalaxyPhaseFilter::Phase4);
				break;
			}

			case WID_UD_TRADE_GATE_BTN: {
				if (!this->selected_galaxy_node_id.empty()) {
					TileIndex p_tile = INVALID_TILE;
					WorldID local_w = DEFAULT_WORLD;
					for (const auto &[id, link] : PortalRegistry::GetAllPortals()) {
						if (link.end_a.IsValid()) {
							p_tile = link.end_a.tile;
							local_w = link.end_a.world_id;
							break;
						}
					}
					if (p_tile != INVALID_TILE) {
						PrebuiltTradeManager::Instance().RegisterTradeGateway(p_tile, this->selected_galaxy_node_id, local_w, 32);
					}
					this->SetDirty();
				}
				break;
			}

			case WID_UD_JUMP_BTN: {
				if (this->selected_world != INVALID_WORLD && PlanetManager::GetRegion(this->selected_world) != nullptr) {
					PlanetManager::JumpToPlanet(this->selected_world);
				}
				break;
			}

			case WID_UD_COLONIZE_BTN: {
				if (this->selected_world != INVALID_WORLD && Company::IsValidID(_local_company)) {
					const PlanetRegion *local_reg = PlanetManager::GetRegion(this->selected_world);
					if (local_reg != nullptr && local_reg->phase == WorldPhase::Phase4_Expansion) {
						TileIndex outpost_tile = TileXY((local_reg->min_x + local_reg->max_x) / 2, (local_reg->min_y + local_reg->max_y) / 2);
						Command<Commands::ColonizeOutpost>::Post(STR_ERROR_CAN_T_COLONIZE_OUTPOST, outpost_tile, "");
					}
				}
				break;
			}

			case WID_UD_PROMOTE_BTN: {
				if (this->selected_world != INVALID_WORLD && Company::IsValidID(_local_company) && PlanetManager::GetRegion(this->selected_world) != nullptr && PlanetManager::CanPromoteWorld(this->selected_world)) {
					Command<Commands::PromoteWorld>::Post(STR_ERROR_CAN_T_PROMOTE_WORLD, this->selected_world);
				}
				break;
			}

			case WID_UD_MEGACITY_BTN: {
				if (this->selected_world != INVALID_WORLD) {
					TownID target_town = TownID::Invalid();
					for (const auto &prof : MegacityManager::GetAllMegacities()) {
						if (prof.world_id == this->selected_world) {
							target_town = prof.town_id;
							break;
						}
					}
					if (target_town == TownID::Invalid()) {
						Town *primary = PlanetManager::GetWorldPrimaryTown(this->selected_world);
						if (primary != nullptr) target_town = primary->index;
					}
					if (target_town != TownID::Invalid()) {
						ShowMegacityOverview(target_town);
					}
				}
				break;
			}

			case WID_UD_WORLD_LIST: {
				auto worlds = service.GetWorldDirectoryForGUI();
				if (worlds.empty()) return;

				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_UD_WORLD_LIST, WidgetDimensions::scaled.framerect.top);
				if (row >= 0 && static_cast<size_t>(row) < worlds.size()) {
					this->selected_world = worlds[row].world_id;
					this->SetDirty();
				}
				break;
			}

			case WID_UD_GALAXY_MAP: {
				if (this->displayed_galaxy_nodes.empty()) return;

				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_UD_GALAXY_MAP, WidgetDimensions::scaled.framerect.top);
				if (row >= 0 && static_cast<size_t>(row) < this->displayed_galaxy_nodes.size()) {
					this->selected_galaxy_node_id = this->displayed_galaxy_nodes[row]->world_id;
					this->SetDirty();
				}
				break;
			}
		}
	}
};

void ShowUniverseDirectory()
{
	AllocateWindowDescFront<UniverseDirectoryWindow>(_universe_directory_desc, 0);
}
