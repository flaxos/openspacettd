/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint_gui.cpp Rail blueprint library window implementation. */

#include "../stdafx.h"
#include "blueprint_gui.h"
#include "blueprint_manager.h"
#include "blueprint_cmd.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../strings_func.h"
#include "../viewport_func.h"
#include "../gfx_func.h"
#include "../widgets/blueprint_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"
#include "../tilehighlight_func.h"
#include "../rail_cmd.h"
#include "../rail_gui.h"
#include "../textbuf_gui.h"
#include "../command_func.h"
#include "../toolbar_gui.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../portal/fabrication_manager.h"
#include "../portal/portal_cmd.h"

#include <algorithm>

enum class BlueprintWindowMode : uint8_t {
	Normal,
	Capturing,
	Placing,
};

enum class BlueprintQuery : uint8_t { None, CaptureName, Rename, Export, Import };

static constexpr std::initializer_list<NWidgetPart> _nested_blueprint_library_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Brown),
		NWidget(WWT_CAPTION, Colours::Brown, WID_BPL_CAPTION),
		NWidget(WWT_SHADEBOX, Colours::Brown),
		NWidget(WWT_DEFSIZEBOX, Colours::Brown),
		NWidget(WWT_STICKYBOX, Colours::Brown),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_BPL_LIST_PANEL), SetMinimalSize(240, 200), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::Brown, WID_BPL_SCROLLBAR),
		NWidget(WWT_PANEL, Colours::Brown, WID_BPL_INFO_PANEL), SetMinimalSize(280, 200), SetFill(1, 1), SetResize(1, 1), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_CAPTURE), SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_CAPTURE, STR_BLUEPRINT_BUTTON_CAPTURE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_PLACE),   SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_PLACE, STR_BLUEPRINT_BUTTON_PLACE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_ROTATE),  SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_ROTATE, STR_BLUEPRINT_BUTTON_ROTATE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_FLIP),    SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_FLIP, STR_BLUEPRINT_BUTTON_FLIP_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_RENAME),  SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_RENAME, STR_BLUEPRINT_BUTTON_RENAME_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_DELETE),  SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_DELETE, STR_BLUEPRINT_BUTTON_DELETE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_EXPORT),  SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_EXPORT, STR_BLUEPRINT_BUTTON_EXPORT_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_IMPORT),  SetFill(1, 0), SetStringTip(STR_BLUEPRINT_BUTTON_IMPORT, STR_BLUEPRINT_BUTTON_IMPORT_TOOLTIP),
	EndContainer(),
	NWidget(WWT_PUSHTXTBTN, Colours::Brown, WID_BPL_FABRICATION_TOGGLE), SetFill(1, 0), SetStringTip(STR_FABRICATION_MODE_STOCKPILE, STR_FABRICATION_BTN_TOGGLE_TOOLTIP),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::Brown, WID_BPL_STATUS_BAR), SetMinimalSize(508, 24), SetFill(1, 0), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, Colours::Brown),
	EndContainer(),
};

static WindowDesc _blueprint_library_desc(
	WindowPosition::Automatic, "view_blueprint_library", 540, 290,
	WindowClass::BlueprintLibrary, WindowClass::None,
	{},
	_nested_blueprint_library_widgets
);

struct BlueprintLibraryWindow : Window {
	size_t selected_index = 0;
	BlueprintWindowMode mode = BlueprintWindowMode::Normal;
	std::string status_message = "Ready. Select a blueprint or capture a new rail layout.";
	Scrollbar *vscroll = nullptr;

	Blueprint working_bp;
	bool has_working_bp = false;
	BlueprintQuery pending_query = BlueprintQuery::None;
	size_t pending_index = 0;
	Blueprint pending_export;
	Blueprint pending_capture;

	BlueprintLibraryWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BPL_SCROLLBAR);
		std::string scan_error;
		if (!BlueprintManager::RescanLibrary(&scan_error)) this->status_message = "Library scan: " + scan_error;
		this->FinishInitNested(window_number);
		this->UpdateSelection();
	}

	void UpdateSelection()
	{
		const auto &list = BlueprintManager::GetBlueprints();
		if (this->vscroll != nullptr) {
			this->vscroll->SetCount(static_cast<uint>(list.size()));
		}
		if (this->selected_index >= list.size() && !list.empty()) {
			this->selected_index = list.size() - 1;
		}
		const Blueprint *bp = BlueprintManager::GetBlueprint(this->selected_index);
		if (bp != nullptr) {
			this->working_bp = *bp;
			this->has_working_bp = true;
		} else {
			this->has_working_bp = false;
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_BPL_FABRICATION_TOGGLE) return GetString(FabricationManager::IsFabricateFromStockpileEnabled(_local_company) ? STR_FABRICATION_MODE_STOCKPILE : STR_FABRICATION_MODE_CASH);
		if (widget == WID_BPL_STATUS_BAR) return this->status_message;
		if (widget == WID_BPL_CAPTION) {
			return GetString(STR_BLUEPRINT_VIEW_CAPTION);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnPaint() override
	{
		const auto &list = BlueprintManager::GetBlueprints();
		bool has_sel = (this->selected_index < list.size());
		bool is_builtin = has_sel && list[this->selected_index].is_builtin;

		this->SetWidgetDisabledState(WID_BPL_FABRICATION_TOGGLE, !Company::IsValidID(_local_company));
		this->SetWidgetDisabledState(WID_BPL_PLACE, !has_sel);
		this->SetWidgetDisabledState(WID_BPL_ROTATE, !has_sel);
		this->SetWidgetDisabledState(WID_BPL_FLIP, !has_sel);
		this->SetWidgetDisabledState(WID_BPL_RENAME, !has_sel || is_builtin);
		this->SetWidgetDisabledState(WID_BPL_DELETE, !has_sel || is_builtin);
		this->SetWidgetDisabledState(WID_BPL_EXPORT, !has_sel);

		this->DrawWidgets();

		/* Draw Blueprint List */
		const NWidgetBase *list_wid = this->GetWidget<NWidgetBase>(WID_BPL_LIST_PANEL);
		Rect r = list_wid->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		int line_height = GetCharacterHeight(FontSize::Normal) + 4;
		uint visible_lines = static_cast<uint>(r.Height() / line_height);
		if (this->vscroll != nullptr) {
			this->vscroll->SetCapacity(visible_lines);
			this->vscroll->SetCount(static_cast<uint>(list.size()));
		}

		int start = (this->vscroll != nullptr) ? this->vscroll->GetPosition() : 0;
		int y = r.top + 2;

		for (size_t i = start; i < list.size() && i < start + visible_lines; ++i) {
			const auto &bp = list[i];
			bool selected = (i == this->selected_index);

			if (selected) {
				GfxFillRect(r.left, y - 1, r.right, y + line_height - 3, PC_DARK_BLUE);
			}

			TextColour tc = bp.is_builtin ? TextColour::Gold : (selected ? TextColour::White : TextColour::Silver);
			std::string label = fmt::format("{}{} ({}x{})", bp.is_builtin ? "[CST] " : "", bp.name, bp.width, bp.height);
			DrawString(r.left + 4, r.right - 4, y, label, tc);
			y += line_height;
		}

		/* Draw Blueprint Details in Info Panel */
		const NWidgetBase *info_wid = this->GetWidget<NWidgetBase>(WID_BPL_INFO_PANEL);
		Rect ir = info_wid->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		int iy = ir.top + 4;

		if (this->has_working_bp) {
			const auto &bp = this->working_bp;
			DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Name: {}", bp.name), TextColour::White);
			iy += line_height;

			std::string type_str = bp.is_builtin ? "CST Prefab (Read-Only)" : "Player Blueprint";
			DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Type: {}", type_str), bp.is_builtin ? TextColour::Gold : TextColour::Silver);
			iy += line_height;

			if (!bp.author.empty()) {
				DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Author: {}", bp.author), TextColour::Silver);
				iy += line_height;
			}

			DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Footprint: {} x {} tiles", bp.width, bp.height), TextColour::Green);
			iy += line_height;

			DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Track Pieces: {}", bp.GetTrackPieceCount()), TextColour::LightBlue);
			iy += line_height;

			DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Signals: {}", bp.GetSignalCount()), TextColour::Yellow);
			iy += line_height;

			if (bp.GetStationCount() > 0) {
				DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Stations: {} tiles", bp.GetStationCount()), TextColour::Orange);
				iy += line_height;
			}

			if (bp.GetDepotCount() > 0) {
				DrawString(ir.left + 4, ir.right - 4, iy, fmt::format("Depots: {}", bp.GetDepotCount()), TextColour::Purple);
				iy += line_height;
			}

			if (!bp.description.empty()) {
				iy += 4;
				DrawStringMultiLine(Rect{ir.left + 4, iy, ir.right - 4, ir.bottom}, bp.description, TextColour::Silver);
			}
		} else {
			DrawString(ir.left + 4, ir.right - 4, iy + 20, "No blueprint selected.", TextColour::Silver);
		}

		/* Draw Status Bar */
		const NWidgetBase *status_wid = this->GetWidget<NWidgetBase>(WID_BPL_STATUS_BAR);
		Rect sr = status_wid->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
		TextColour status_tc = (this->mode == BlueprintWindowMode::Placing) ? TextColour::Yellow :
		                       ((this->mode == BlueprintWindowMode::Capturing) ? TextColour::Orange : TextColour::Silver);
		DrawString(sr.left + 4, sr.right - 4, sr.top + 4, this->GetWidgetString(WID_BPL_STATUS_BAR, STR_NULL), status_tc);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		/* A new library action cancels any earlier path/name query before its target changes. */
		CloseWindowByClass(WindowClass::QueryString);
		switch (widget) {
			case WID_BPL_FABRICATION_TOGGLE:
				if (Company::IsValidID(_local_company)) {
					Command<Commands::SetFabricationMode>::Post(!FabricationManager::IsFabricateFromStockpileEnabled(_local_company));
				}
				break;
			case WID_BPL_LIST_PANEL: {
				const NWidgetBase *list_wid = this->GetWidget<NWidgetBase>(WID_BPL_LIST_PANEL);
				Rect r = list_wid->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				int line_height = GetCharacterHeight(FontSize::Normal) + 4;
				int rel_y = pt.y - r.top;
				if (rel_y >= 0) {
					int start = (this->vscroll != nullptr) ? this->vscroll->GetPosition() : 0;
					size_t clicked_idx = start + (rel_y / line_height);
					const auto &list = BlueprintManager::GetBlueprints();
					if (clicked_idx < list.size()) {
						this->selected_index = clicked_idx;
						this->working_bp = list[clicked_idx];
						this->has_working_bp = true;
						this->status_message = fmt::format("Selected '{}'.", this->working_bp.name);
						if (this->mode == BlueprintWindowMode::Placing) {
							SetTileSelectSize(this->working_bp.width, this->working_bp.height);
						}
						this->SetDirty();
					}
				}
				break;
			}

			case WID_BPL_CAPTURE: {
				/* Installing a tool aborts the previous one, including our own. */
				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_RECT, this);
				this->mode = BlueprintWindowMode::Capturing;
				SetTileSelectSize(1, 1);
				this->status_message = "Capture mode: Click & drag a rectangle on map to capture rail blueprint.";
				this->SetDirty();
				break;
			}

			case WID_BPL_PLACE: {
				if (!this->has_working_bp) break;
				SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_RECT, this);
				this->mode = BlueprintWindowMode::Placing;
				SetTileSelectSize(this->working_bp.width, this->working_bp.height);
				this->status_message = fmt::format("Placing '{}' ({}x{}). Click map to stamp. Hotkeys: [R] Rotate, [F] Flip.",
					this->working_bp.name, this->working_bp.width, this->working_bp.height);
				this->SetDirty();
				break;
			}

			case WID_BPL_ROTATE: {
				if (!this->has_working_bp) break;
				this->working_bp = this->working_bp.Rotate(1);
				if (this->mode == BlueprintWindowMode::Placing) {
					SetTileSelectSize(this->working_bp.width, this->working_bp.height);
				}
				this->status_message = fmt::format("Rotated '{}' 90 deg clockwise ({}x{}).",
					this->working_bp.name, this->working_bp.width, this->working_bp.height);
				this->SetDirty();
				break;
			}

			case WID_BPL_FLIP: {
				if (!this->has_working_bp) break;
				this->working_bp = this->working_bp.Mirror();
				if (this->mode == BlueprintWindowMode::Placing) {
					SetTileSelectSize(this->working_bp.width, this->working_bp.height);
				}
				this->status_message = fmt::format("Flipped '{}' horizontally ({}x{}).",
					this->working_bp.name, this->working_bp.width, this->working_bp.height);
				this->SetDirty();
				break;
			}

			case WID_BPL_RENAME: {
				const Blueprint *selected = BlueprintManager::GetBlueprint(this->selected_index);
				if (selected == nullptr || selected->is_builtin) break;
				this->pending_query = BlueprintQuery::Rename;
				this->pending_index = this->selected_index;
				ShowQueryString(selected->name, STR_BLUEPRINT_RENAME_CAPTION, Blueprint::MAX_NAME_BYTES + 1, this, CS_ALPHANUMERAL, QueryStringFlag::AcceptUnchanged);
				break;
			}

			case WID_BPL_DELETE: {
				std::string error;
				if (BlueprintManager::DeleteBlueprint(this->selected_index, &error)) {
					this->status_message = "Blueprint deleted.";
					this->UpdateSelection();
				} else {
					this->status_message = "Delete failed: " + error;
				}
				this->SetDirty();
				break;
			}

			case WID_BPL_EXPORT: {
				if (!this->has_working_bp) break;
				this->pending_query = BlueprintQuery::Export;
				this->pending_export = this->working_bp;
				this->status_message = "Export: enter a new JSON file path. Existing files are preserved.";
				ShowQueryString(BlueprintManager::GetDefaultExportPath(this->working_bp), STR_BLUEPRINT_BUTTON_EXPORT, 4096, this, CS_ALPHANUMERAL, QueryStringFlag::AcceptUnchanged);
				this->SetDirty();
				break;
			}

			case WID_BPL_IMPORT: {
				this->pending_query = BlueprintQuery::Import;
				this->status_message = "Import: enter the path to a Blueprint JSON file.";
				ShowQueryString({}, STR_BLUEPRINT_BUTTON_IMPORT, 4096, this, CS_ALPHANUMERAL, {});
				this->SetDirty();
				break;
			}
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		BlueprintQuery query = this->pending_query;
		this->pending_query = BlueprintQuery::None;
		if (query == BlueprintQuery::None) return;
		if (!str.has_value() || str->empty()) {
			this->status_message = "Operation cancelled.";
			this->SetDirty();
			return;
		}
		std::string error;
		switch (query) {
			case BlueprintQuery::CaptureName:
				this->pending_capture.name = *str;
				if (BlueprintManager::SaveBlueprint(this->pending_capture, &error)) {
					this->selected_index = BlueprintManager::GetBlueprints().size() - 1;
					this->UpdateSelection();
					this->status_message = fmt::format("Captured and saved '{}' ({}x{}, {} pieces, {} signals).",
						this->working_bp.name, this->working_bp.width, this->working_bp.height,
						this->working_bp.GetTrackPieceCount(), this->working_bp.GetSignalCount());
				} else {
					this->status_message = "Capture could not be saved: " + error;
				}
				break;
			case BlueprintQuery::Rename:
				if (BlueprintManager::RenameBlueprint(this->pending_index, *str, &error)) {
					this->selected_index = this->pending_index;
					this->UpdateSelection();
					this->status_message = fmt::format("Renamed blueprint to '{}'.", *str);
				} else {
					this->status_message = "Rename failed: " + error;
				}
				break;
			case BlueprintQuery::Export:
				if (BlueprintManager::ExportToFile(this->pending_export, *str, &error)) {
					this->status_message = fmt::format("Exported '{}' to '{}'.", this->pending_export.name, *str);
				} else {
					this->status_message = "Export failed: " + error;
				}
				break;
			case BlueprintQuery::Import:
				if (BlueprintManager::ImportFromFile(*str, &error)) {
					this->selected_index = BlueprintManager::GetBlueprints().size() - 1;
					this->UpdateSelection();
					this->status_message = fmt::format("Imported '{}' from '{}'.", this->working_bp.name, *str);
				} else {
					this->status_message = "Import failed: " + error;
				}
				break;
			case BlueprintQuery::None: break;
		}
		this->SetDirty();
	}

	EventState OnKeyPress([[maybe_unused]] char32_t key, uint16_t keycode) override
	{
		if (keycode == 'r' || keycode == 'R') {
			if (this->has_working_bp) {
				this->working_bp = this->working_bp.Rotate(1);
				if (this->mode == BlueprintWindowMode::Placing) {
					SetTileSelectSize(this->working_bp.width, this->working_bp.height);
				}
				this->status_message = fmt::format("Rotated '{}' ({}x{}).", this->working_bp.name, this->working_bp.width, this->working_bp.height);
				this->SetDirty();
			}
			return EventState::Handled;
		} else if (keycode == 'f' || keycode == 'F') {
			if (this->has_working_bp) {
				this->working_bp = this->working_bp.Mirror();
				if (this->mode == BlueprintWindowMode::Placing) {
					SetTileSelectSize(this->working_bp.width, this->working_bp.height);
				}
				this->status_message = fmt::format("Flipped '{}' ({}x{}).", this->working_bp.name, this->working_bp.width, this->working_bp.height);
				this->SetDirty();
			}
			return EventState::Handled;
		}
		return EventState::NotHandled;
	}

	void OnPlaceDrag(ViewportPlaceMethod select_method, [[maybe_unused]] ViewportDragDropSelectionProcess select_proc, Point pt) override
	{
		if (this->mode == BlueprintWindowMode::Capturing && select_proc == DDSP_CAPTURE_BLUEPRINT) {
			VpSelectTilesWithMethod(pt.x, pt.y, select_method);
		}
	}

	void OnPlaceMouseUp([[maybe_unused]] ViewportPlaceMethod select_method, ViewportDragDropSelectionProcess select_proc, Point pt, TileIndex start_tile, TileIndex end_tile) override
	{
		if (this->mode == BlueprintWindowMode::Capturing && select_proc == DDSP_CAPTURE_BLUEPRINT && pt.x != -1) {
			auto bp = BlueprintManager::CaptureArea(start_tile, end_tile);
			this->mode = BlueprintWindowMode::Normal;
			ResetObjectToPlace();
			if (bp.has_value()) {
				this->pending_capture = *bp;
				this->pending_query = BlueprintQuery::CaptureName;
				this->status_message = "Capture ready: enter a name to save it.";
				ShowQueryString(bp->name, STR_BLUEPRINT_CAPTURE_NAME_CAPTION, Blueprint::MAX_NAME_BYTES + 1, this, CS_ALPHANUMERAL, QueryStringFlag::AcceptUnchanged);
			} else {
				this->status_message = "Capture failed: empty, foreign-owned, or larger than 64x64.";
			}
			this->SetDirty();
		}
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (this->mode == BlueprintWindowMode::Capturing) {
			VpStartPlaceSizing(tile, VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT);
			return;
		}
		if (this->mode == BlueprintWindowMode::Placing && this->has_working_bp) {
			RailType rt = (_last_built_railtype != INVALID_RAILTYPE) ? _last_built_railtype : RAILTYPE_RAIL;
			Command<Commands::PlaceBlueprint>::Post(
				STR_ERROR_CAN_T_PLACE_BLUEPRINT,
				CcPlaySound_CONSTRUCTION_RAIL,
				tile,
				this->working_bp.ToJson(),
				rt,
				false
			);
			this->status_message = fmt::format("Stamped '{}' at tile ({}, {}).", this->working_bp.name, TileX(tile), TileY(tile));
			this->SetDirty();
		}
	}

	void OnPlaceObjectAbort() override
	{
		this->mode = BlueprintWindowMode::Normal;
		this->status_message = "Map selection cancelled. Library ready.";
		this->SetDirty();
	}
};

void ShowBlueprintLibrary()
{
	AllocateWindowDescFront<BlueprintLibraryWindow>(_blueprint_library_desc, 0);
}
