/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_directory_gui.cpp Implementation of Universe Directory browser window. */

#include "../stdafx.h"
#include "universe_directory_gui.h"
#include "universe_authority.h"
#include "planet_manager.h"
#include "portal_cmd.h"
#include "megacity_gui.h"
#include "megacity_manager.h"
#include "../command_func.h"
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
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_MEGACITY_BTN), SetMinimalSize(85, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_MEGACITY, STR_UNIVERSE_DIRECTORY_MEGACITY_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_PROMOTE_BTN), SetMinimalSize(85, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_PROMOTE, STR_UNIVERSE_DIRECTORY_PROMOTE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_COLONIZE_BTN), SetMinimalSize(85, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_COLONIZE, STR_UNIVERSE_DIRECTORY_COLONIZE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_JUMP_BTN), SetMinimalSize(75, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_JUMP, STR_UNIVERSE_DIRECTORY_JUMP_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_REFRESH), SetMinimalSize(70, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_REFRESH, STR_UNIVERSE_DIRECTORY_REFRESH_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Blue, WID_UD_WORLD_LIST), SetMinimalSize(508, 200), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Blue, WID_UD_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Blue, WID_UD_DETAILS_PANEL), SetMinimalSize(520, 68), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::Blue),
	EndContainer(),
};

static WindowDesc _universe_directory_desc(
	WindowPosition::Automatic, "view_universe_directory", 520, 350,
	WindowClass::UniverseDirectory, WindowClass::None,
	{},
	_nested_universe_directory_widgets
);

/** Window class for browsing Universe Authority registered worlds, online nodes, and cluster metrics. */
struct UniverseDirectoryWindow : Window {
	Scrollbar *vscroll = nullptr;
	WorldID selected_world = INVALID_WORLD;

	UniverseDirectoryWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_UD_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);

		auto worlds = UniverseAuthorityService::Instance().GetWorldDirectory();
		if (worlds.empty() && PlanetManager::Count() > 0) {
			for (const auto &reg : PlanetManager::GetAllRegions()) {
				RegisteredWorld rw{
					.world_id = reg.id,
					.phase = reg.phase,
					.name = reg.name,
					.last_heartbeat_tick = 1,
					.address = "Local Node",
					.description = fmt::format("{} ecosystem", PlanetManager::GetWorldBiomeName(reg.biome)),
					.status = WorldOnlineStatus::Online,
					.biome = reg.biome,
				};
				UniverseAuthorityService::Instance().RegisterWorld(rw);
			}
			worlds = UniverseAuthorityService::Instance().GetWorldDirectory();
		}
		if (!worlds.empty()) {
			this->selected_world = worlds.front().world_id;
		}
	}

	void OnPaint() override
	{
		auto worlds = UniverseAuthorityService::Instance().GetWorldDirectory();
		this->vscroll->SetCount(worlds.size());

		const RegisteredWorld *selected = UniverseAuthorityService::Instance().GetWorld(this->selected_world);
		const PlanetRegion *local_reg = (this->selected_world != INVALID_WORLD) ? PlanetManager::GetRegion(this->selected_world) : nullptr;
		WorldPhase phase = (selected != nullptr) ? selected->phase : (local_reg != nullptr ? local_reg->phase : WorldPhase::Phase3_Frontier);
		bool can_colonize = (phase == WorldPhase::Phase4_Expansion);
		bool can_promote = (this->selected_world != INVALID_WORLD) && PlanetManager::CanPromoteWorld(this->selected_world) && (phase != WorldPhase::Phase4_Expansion);
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

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		auto &service = UniverseAuthorityService::Instance();
		auto worlds = service.GetWorldDirectory();

		switch (widget) {
			case WID_UD_HEADER_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				size_t online_count = 0;
				for (const auto &w : worlds) {
					if (w.status == WorldOnlineStatus::Online) online_count++;
				}

				std::string line1 = fmt::format("Registered Worlds: {}  |  Online Servers: {} / {}",
					worlds.size(), online_count, worlds.size());
				DrawString(tr, line1, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				DrawString(tr, "Central Universe Authority Node: Active  |  Transport Protocol: F4 Lockstep", TextColour::Silver);
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

						/* Background card */
						PixelColour bg_col = is_selected ?
							GetColourGradient(Colours::Blue, Shade::Darker) :
							GetColourGradient(Colours::Blue, Shade::Darkest);
						GfxFillRect(item_rect.Shrink(1), bg_col);

						Rect text_rect = item_rect.Shrink(WidgetDimensions::scaled.framerect);

						/* Line 1: World ID, Name, Biome, and Status Badge */
						StringID status_str = STR_UNIVERSE_DIRECTORY_ONLINE;
						if (world.status == WorldOnlineStatus::Maintenance) status_str = STR_UNIVERSE_DIRECTORY_MAINTENANCE;
						else if (world.status == WorldOnlineStatus::Unreachable) status_str = STR_UNIVERSE_DIRECTORY_UNREACHABLE;

						std::string world_header = fmt::format("World #{}: {} [{} | {}]",
							world.world_id.base(), world.name,
							PlanetManager::GetWorldPhaseName(world.phase),
							PlanetManager::GetWorldBiomeName(world.biome));
						DrawString(text_rect, world_header, is_selected ? TextColour::White : TextColour::Gold);

						int header_w = GetStringBoundingBox(world_header).width;
						Rect status_rect = text_rect.Indent(header_w + ScaleGUITrad(8), _current_text_dir != TD_RTL);
						DrawString(status_rect, status_str);
						text_rect.top += GetCharacterHeight(FontSize::Normal);

						/* Line 2: Address, Clients, Active Trains, Population, Megacity status */
						uint32_t pop = world.population > 0 ? world.population : PlanetManager::GetWorldPopulation(world.world_id);
						std::string metrics = fmt::format("Addr: {}  |  Clients: {}/{}  |  Active Trains: {}",
							world.address.empty() ? "Local / In-Process" : world.address,
							world.active_clients, world.max_clients, world.active_trains);
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

			case WID_UD_DETAILS_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				const RegisteredWorld *selected = service.GetWorld(this->selected_world);

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
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto &service = UniverseAuthorityService::Instance();

		switch (widget) {
			case WID_UD_REFRESH: {
				service.PruneStaleWorlds(0, 300);
				this->SetDirty();
				break;
			}

			case WID_UD_JUMP_BTN: {
				if (this->selected_world != INVALID_WORLD) {
					PlanetManager::JumpToPlanet(this->selected_world);
				}
				break;
			}

			case WID_UD_COLONIZE_BTN: {
				if (this->selected_world != INVALID_WORLD) {
					const PlanetRegion *local_reg = PlanetManager::GetRegion(this->selected_world);
					if (local_reg != nullptr && local_reg->phase == WorldPhase::Phase4_Expansion) {
						TileIndex outpost_tile = TileXY((local_reg->min_x + local_reg->max_x) / 2, (local_reg->min_y + local_reg->max_y) / 2);
						Command<Commands::ColonizeOutpost>::Post(STR_ERROR_CAN_T_COLONIZE_OUTPOST, outpost_tile, "");
					}
					service.ColonizeWorld(this->selected_world);
					this->SetDirty();
				}
				break;
			}

			case WID_UD_PROMOTE_BTN: {
				if (this->selected_world != INVALID_WORLD) {
					Command<Commands::PromoteWorld>::Post(STR_ERROR_CAN_T_PROMOTE_WORLD, this->selected_world);
					service.PromoteWorld(this->selected_world);
					this->SetDirty();
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
				auto worlds = service.GetWorldDirectory();
				if (worlds.empty()) return;

				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_UD_WORLD_LIST, WidgetDimensions::scaled.framerect.top);
				if (row >= 0 && static_cast<size_t>(row) < worlds.size()) {
					this->selected_world = worlds[row].world_id;
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
