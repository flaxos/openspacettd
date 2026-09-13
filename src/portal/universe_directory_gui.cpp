/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_directory_gui.cpp Implementation of Universe Directory browser window. */

#include "../stdafx.h"
#include "universe_directory_gui.h"
#include "universe_authority.h"
#include "planet_manager.h"
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
		NWidget(WWT_PANEL, Colours::Blue, WID_UD_HEADER_PANEL), SetMinimalSize(360, 36), SetFill(1, 0), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_JUMP_BTN), SetMinimalSize(80, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_JUMP, STR_UNIVERSE_DIRECTORY_JUMP_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Blue, WID_UD_REFRESH), SetMinimalSize(80, 36), SetFill(0, 0), SetResize(0, 0), SetStringTip(STR_UNIVERSE_DIRECTORY_REFRESH, STR_UNIVERSE_DIRECTORY_REFRESH_TOOLTIP),
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
		if (!worlds.empty()) {
			this->selected_world = worlds.front().world_id;
		}
	}

	void OnPaint() override
	{
		auto worlds = UniverseAuthorityService::Instance().GetWorldDirectory();
		this->vscroll->SetCount(worlds.size());

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

						/* Line 1: World ID, Name, and Status Badge */
						StringID status_str = STR_UNIVERSE_DIRECTORY_ONLINE;
						if (world.status == WorldOnlineStatus::Maintenance) status_str = STR_UNIVERSE_DIRECTORY_MAINTENANCE;
						else if (world.status == WorldOnlineStatus::Unreachable) status_str = STR_UNIVERSE_DIRECTORY_UNREACHABLE;

						std::string world_header = fmt::format("World #{}: {} [{}]",
							world.world_id.base(), world.name, PlanetManager::GetWorldPhaseName(world.phase));
						DrawString(text_rect, world_header, is_selected ? TextColour::White : TextColour::Gold);

						int header_w = GetStringBoundingBox(world_header).width;
						Rect status_rect = text_rect.Indent(header_w + ScaleGUITrad(8), _current_text_dir != TD_RTL);
						DrawString(status_rect, status_str);
						text_rect.top += GetCharacterHeight(FontSize::Normal);

						/* Line 2: Address, Clients, Active Trains */
						std::string metrics = fmt::format("Addr: {}  |  Clients: {}/{}  |  Active Trains: {}",
							world.address.empty() ? "Local / In-Process" : world.address,
							world.active_clients, world.max_clients, world.active_trains);
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

				std::string line1 = fmt::format("Selected: World #{} ({}) - {}",
					selected->world_id.base(), selected->name,
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

				std::string line3 = fmt::format("Inter-World Trade Balance: Cr {:L} (Exports: {} types, Imports: {} types)",
					trade.net_trade_balance_credits, trade.exported_cargo.size(), trade.imported_cargo.size());
				DrawString(tr, line3, trade.net_trade_balance_credits >= 0 ? TextColour::LightBlue : TextColour::Orange);
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
