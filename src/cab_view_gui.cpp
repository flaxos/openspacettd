/*
 * This file is part of OpenSpaceTTD and is distributed under GPL version 2.
 * Cab camera concept and CPU renderer adapted from vaclavloubek/OpenTTD,
 * tag 15.3-cabview-alpha1, commit b7bd17b04dde3379b2be33be40ba4d6fc9f5d391.
 */

/** @file cab_view_gui.cpp A local, read-only camera on one owned primary train. */

#include "stdafx.h"
#include "cab_view.h"
#include "cab_view_3d.h"
#include "direction_func.h"
#include "map_func.h"
#include "window_gui.h"
#include "window_func.h"
#include "vehicle_base.h"
#include "train.h"
#include "company_func.h"
#include "openttd.h"
#include "bridge_map.h"
#include "tunnel_map.h"
#include "portal/portal_registry.h"
#include "portal/planet_manager.h"
#include "gfx_func.h"
#include "zoom_func.h"
#include "core/geometry_func.hpp"
#include "video/video_driver.hpp"
#include "blitter/factory.hpp"
#include "strings_func.h"
#include "hotkeys.h"
#include "settings_type.h"
#include "widgets/cab_view_widget.h"
#include "table/sprites.h"
#include "table/strings.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>

#include "safeguards.h"

bool CabViewLocalGateVisible(TileIndex tile, WorldID selected_world)
{
	if (!CabViewTileVisible(tile, selected_world)) return false;
	const PortalGateClassification gate = PortalRegistry::ClassifyGate(tile);
	if (gate.kind != PortalGateKind::LocalLinked || gate.local_endpoint.tile != tile) return false;
	if (gate.local_endpoint.world_id != selected_world ||
		PlanetManager::GetTileWorld(tile) != gate.local_endpoint.world_id) return false;
	const TileIndex other = gate.opposite_endpoint.tile;
	const WorldID other_world = gate.opposite_endpoint.world_id;
	return other != tile && CabViewTileVisible(other, other_world) &&
		PlanetManager::GetTileWorld(other) == other_world;
}

CabViewPresentation ResolveCabViewPresentation(const Vehicle *v, CompanyID owner)
{
	CabViewPresentation result;
	if (_game_mode != GameMode::Normal || _switch_mode != SwitchMode::None || v == nullptr) return result;
	if (v->type != VehicleType::Train || !v->IsPrimaryVehicle() || v->owner != owner || owner != _local_company) return result;
	if (!IsValidTile(v->tile) || v->IsInDepot()) return result;
	const WorldID world = PlanetManager::GetTileWorld(v->tile);
	if (!CabViewTileVisible(v->tile, world)) return result;
	const Direction moving = v->GetMovingDirection();
	if (!IsValidDirection(moving)) return result;
	const bool hidden = v->vehstatus.Test(VehState::Hidden);
	const bool wormhole = Train::From(v)->track == Track::Wormhole;
	const PortalGateClassification gate = PortalRegistry::ClassifyGate(v->tile);
	/* The scene does not draw bridges or the ground below them, so on or under an
	 * ordinary bridge the cab waits instead of closing. Gates keep their own rules. */
	const bool bridge = !hidden && gate.kind == PortalGateKind::None && !PortalRegistry::IsPortalTile(v->tile) &&
		(IsBridgeTile(v->tile) || IsBridgeAbove(v->tile));
	if (!bridge && (hidden || wormhole)) {
		if (!hidden || !wormhole || gate.kind != PortalGateKind::LocalLinked ||
			!CabViewLocalGateVisible(v->tile, world)) return result;
		result.phase = CabViewPhase::LocalTransit;
	} else {
		if (!bridge && (IsTunnelTile(v->tile) || PortalRegistry::IsPortalTile(v->tile) || gate.kind != PortalGateKind::None)) {
			if (!CabViewLocalGateVisible(v->tile, world)) return result;
		}
		if (v->x_pos < 0 || v->y_pos < 0 ||
			v->x_pos >= static_cast<int>(Map::SizeX() * TILE_SIZE) ||
			v->y_pos >= static_cast<int>(Map::SizeY() * TILE_SIZE)) return result;
		if (!CabViewTileVisible(TileVirtXY(v->x_pos, v->y_pos), world)) return result;
		result.phase = bridge ? CabViewPhase::Bridge : CabViewPhase::Scene;
	}
	result.world = world;
	result.tile = v->tile;
	result.x = v->x_pos;
	result.y = v->y_pos;
	result.z = v->z_pos;
	result.moving = moving;
	return result;
}

bool CabViewTransitionRequiresReset(const CabViewPresentation &before, const CabViewPresentation &after)
{
	if (before.phase != after.phase || before.world != after.world || before.moving != after.moving || before.z != after.z) return true;
	return std::abs(static_cast<int64_t>(before.x) - after.x) > 2 * TILE_SIZE ||
		std::abs(static_cast<int64_t>(before.y) - after.y) > 2 * TILE_SIZE;
}

bool CabViewTransitionResetsLook(const CabViewPresentation &before, const CabViewPresentation &after)
{
	/* Passing over or under a bridge is part of the same journey; keep the player's view. */
	const bool bridge = before.phase == CabViewPhase::Bridge || after.phase == CabViewPhase::Bridge;
	return (before.phase != after.phase && !bridge) || before.world != after.world ||
		std::abs(static_cast<int64_t>(before.x) - after.x) > 2 * TILE_SIZE ||
		std::abs(static_cast<int64_t>(before.y) - after.y) > 2 * TILE_SIZE;
}

bool CanShowCabView(const Vehicle *v)
{
	VideoDriver *video = VideoDriver::GetInstance();
	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	if (video == nullptr || !video->HasGUI() || blitter == nullptr || _screen.width <= 0 || _screen.height <= 0) return false;
	if (blitter->GetScreenDepth() != 8 && blitter->GetScreenDepth() != 32) return false;
	return ResolveCabViewPresentation(v, _local_company).phase != CabViewPhase::Unsupported;
}

static constexpr std::initializer_list<NWidgetPart> _nested_cab_view_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::Grey),
		NWidget(WWT_CAPTION, Colours::Grey, WID_CV_CAPTION),
		NWidget(WWT_DEFSIZEBOX, Colours::Grey),
		NWidget(WWT_STICKYBOX, Colours::Grey),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::Grey), SetFill(1, 1), SetResize(1, 1),
		NWidget(WWT_EMPTY, Colours::Invalid, WID_CV_VIEW), SetMinimalSize(400, 240), SetResize(1, 1), SetFill(1, 1), SetToolTip(STR_CAB_VIEW_VIEW_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_TEXTBTN, Colours::Grey, WID_CV_LEFT), SetStringTip(STR_CAB_VIEW_LOOK_LEFT, STR_CAB_VIEW_LOOK_LEFT),
		NWidget(WWT_TEXTBTN, Colours::Grey, WID_CV_AHEAD), SetStringTip(STR_CAB_VIEW_LOOK_AHEAD, STR_CAB_VIEW_LOOK_AHEAD),
		NWidget(WWT_TEXTBTN, Colours::Grey, WID_CV_RIGHT), SetStringTip(STR_CAB_VIEW_LOOK_RIGHT, STR_CAB_VIEW_LOOK_RIGHT),
		NWidget(WWT_TEXTBTN, Colours::Grey, WID_CV_EFFECT), SetStringTip(STR_CAB_VIEW_EFFECT_FULL, STR_CAB_VIEW_EFFECT_FULL),
		NWidget(WWT_TEXTBTN, Colours::Grey, WID_CV_HELP), SetStringTip(STR_CAB_VIEW_HELP, STR_CAB_VIEW_HELP_TOOLTIP),
		NWidget(WWT_PANEL, Colours::Grey), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, Colours::Grey),
	EndContainer(),
};

/** The opaque event surface is drawn only for an actual LocalTransit sample.
 * Off intentionally takes the accepted Stage 2 fill-and-label path verbatim. */
CabViewTransitStats DrawCabViewTransit(const Rect &r, CabViewEffectMode mode, uint32_t elapsed_ms)
{
	auto start = std::chrono::steady_clock::now();
	GfxFillRect(r, PC_BLACK);
	uint64_t effect_us = 0;
	if (mode != CabViewEffectMode::Off && r.Width() > 16 && r.Height() > 16) {
		const auto effect_start = std::chrono::steady_clock::now();
		const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
		const int max_radius = std::max(4, std::min(r.Width() / 3, r.Height() / 3));
		const bool full = mode == CabViewEffectMode::Full;
		const int drift = full ? static_cast<int>((elapsed_ms % 1200) * max_radius / 2400) : 0;
		for (int ring = 0; ring < 2; ++ring) {
			const int radius = max_radius / 2 + ring * max_radius / 4 + drift / 3;
			const PixelColour colour = ring == 0 ? PC_DARK_BLUE : PC_LIGHT_BLUE;
			for (int i = 0; i < 16; ++i) {
				const float a = static_cast<float>(i) * (2.0f * static_cast<float>(M_PI) / 16.0f);
				const float b = static_cast<float>(i + 1) * (2.0f * static_cast<float>(M_PI) / 16.0f);
				GfxDrawLine(cx + static_cast<int>(radius * std::cos(a)), cy + static_cast<int>(radius * std::sin(a)),
					cx + static_cast<int>(radius * std::cos(b)), cy + static_cast<int>(radius * std::sin(b)), colour);
			}
		}
		if (full) {
			for (int i = -3; i <= 3; ++i) {
				const int y = cy + i * max_radius / 8;
				const int span = max_radius / 3 + drift;
				GfxDrawLine(cx - span, y, cx - span / 2, y, PC_VERY_DARK_BLUE);
				GfxDrawLine(cx + span / 2, y, cx + span, y, PC_VERY_DARK_BLUE);
			}
		}
		effect_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - effect_start).count());
	}
	DrawString(r.left, r.right, CentreBounds(r.top, r.bottom, GetCharacterHeight(FontSize::Normal)),
		GetString(STR_CAB_VIEW_LOCAL_TRANSIT), TextColour::White, AlignmentH::Centre);
	return {effect_us, static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count())};
}

/**
 * Draw a short message centred on the view, on a dark box so it reads over any scene.
 * @param r The view.
 * @param text Message.
 */
static void DrawCabViewLabel(const Rect &r, std::string_view text)
{
	const Dimension size = GetStringBoundingBox(text);
	const int pad = ScaleGUITrad(4);
	const int x = (r.left + r.right - static_cast<int>(size.width)) / 2;
	const int y = CentreBounds(r.top, r.bottom, size.height);
	GfxFillRect(x - pad, y - pad, x + size.width + pad, y + size.height + pad, PC_BLACK);
	DrawString(x, x + size.width, y, text, TextColour::White);
}

/**
 * Draw the controls panel over the view.
 * @param r The view.
 */
static void DrawCabViewHelp(const Rect &r)
{
	const Rect box = r.Shrink(ScaleGUITrad(12));
	GfxFillRect(box, PALETTE_TO_TRANSPARENT, FillRectMode::Recolour);
	GfxFillRect(box, PALETTE_TO_TRANSPARENT, FillRectMode::Recolour);
	DrawStringMultiLine(box.Shrink(ScaleGUITrad(8)), GetString(STR_CAB_VIEW_HELP_TEXT), TextColour::White, AlignmentH::Start);
}

/** Keyboard look controls; they act only while the cab window has focus. */
enum class CabViewHotkey : int {
	LookLeft,
	LookRight,
	LookUp,
	LookDown,
	LookAhead,
	LookBehind,
	ZoomIn,
	ZoomOut,
	Effect,
};

struct CabViewWindow : Window {
	VehicleID vehicle_id;
	CompanyID company;
	std::chrono::steady_clock::time_point session_start;
	std::string savegame_id;
	mutable CabViewLook look; ///< Transient; a teleport or world change recentres it.
	mutable uint32_t frame_id = 0;
	mutable bool has_previous = false;
	mutable CabViewPresentation previous{};
	CabViewEffectMode effect_mode = CabViewEffectMode::Full; ///< Fresh window default; never persisted.
	mutable CabViewEffectTimeline timeline;
	mutable CabViewCamera held_camera{}; ///< Last scene camera, shown dimmed while the train is on a bridge.
	mutable uint32_t held_frame = 0; ///< Frame number of #held_camera, so the renderer can reuse its picture.
	mutable bool has_held = false;
	bool show_help = false; ///< Whether the controls panel covers the view.

	CabViewWindow(WindowDesc &desc, WindowNumber number) : Window(desc), vehicle_id(static_cast<VehicleID>(number))
	{
		const Vehicle *v = Vehicle::Get(this->vehicle_id);
		this->company = v->owner;
		this->session_start = _game_session_stats.start_time;
		this->savegame_id = _game_session_stats.savegame_id;
		this->CreateNestedTree();
		this->FinishInitNested(number);
		this->owner = v->owner;
	}

	void Close(int data = 0) override
	{
		/* Invalidate sprite and frame caches immediately, before another cab or
		 * a newly loaded game can reuse the renderer. Window deletion is deferred. */
		ResetCabView3D();
		this->Window::Close(data);
	}

	CabViewPresentation CurrentPresentation() const
	{
		if (_game_session_stats.start_time != this->session_start || _game_session_stats.savegame_id != this->savegame_id) return {};
		const Vehicle *v = Vehicle::GetIfValid(this->vehicle_id);
		if (v == nullptr || v->owner != this->company || !CanShowCabView(v)) return {};
		CabViewPresentation state = ResolveCabViewPresentation(v, this->company);
		if (state.phase == CabViewPhase::Unsupported) return {};
		const bool transition = this->has_previous && CabViewTransitionRequiresReset(this->previous, state);
		this->timeline.Observe(this->previous, state, this->has_previous, transition);
		if (transition) {
			if (CabViewTransitionResetsLook(this->previous, state)) this->look.ResetDirection();
			ResetCabView3D();
			this->frame_id = 0;
		}
		this->previous = state;
		this->has_previous = true;
		return state;
	}

	void OnPaint() override
	{
		if (this->CurrentPresentation().phase == CabViewPhase::Unsupported) { this->Close(); return; }
		this->DrawWidgets();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
		[[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_CV_EFFECT) return;
		Dimension widest = maxdim(GetStringBoundingBox(STR_CAB_VIEW_EFFECT_FULL), GetStringBoundingBox(STR_CAB_VIEW_EFFECT_REDUCED));
		widest = maxdim(widest, GetStringBoundingBox(STR_CAB_VIEW_EFFECT_OFF));
		widest.width += WidgetDimensions::scaled.framerect.Horizontal();
		widest.height += WidgetDimensions::scaled.framerect.Vertical();
		size = maxdim(size, widest);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_CV_VIEW) return;
		const CabViewPresentation state = this->CurrentPresentation();
		if (state.phase == CabViewPhase::Unsupported) { GfxFillRect(r, PixelColour{0}); return; }
		if (state.phase == CabViewPhase::LocalTransit) {
			DrawCabViewTransit(r, this->effect_mode, this->timeline.PhaseElapsed());
			return;
		}
		if (state.phase == CabViewPhase::Bridge) {
			/* Keep the last view of the line, dimmed, so passing a bridge is not a black blink. */
			if (this->has_held && this->held_camera.world == state.world) {
				DrawCabView3D(r, this->held_camera, this->held_frame, 12.0f);
				GfxFillRect(r, PALETTE_TO_TRANSPARENT, FillRectMode::Recolour);
			} else {
				GfxFillRect(r, PC_BLACK);
			}
			DrawCabViewLabel(r, GetString(STR_CAB_VIEW_BRIDGE));
			if (this->show_help) DrawCabViewHelp(r);
			return;
		}
		/* The camera is transient. Neither vehicle state nor game time is adjusted. */
		static constexpr std::array<std::pair<float, float>, 8> heading = {{{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}}};
		auto [dx, dy] = heading[to_underlying(state.moving)];
		CabViewCamera camera;
		camera.x = static_cast<float>(state.x);
		camera.y = static_cast<float>(state.y);
		camera.z = static_cast<float>(state.z) + 4.0f;
		camera.yaw = std::atan2(dy, dx) + this->look.yaw;
		camera.pitch = this->look.pitch;
		camera.fov = this->look.fov;
		camera.vehicle = this->vehicle_id;
		camera.world = state.world;
		camera.effect_mode = this->effect_mode;
		camera.scene_effect = this->timeline.ArrivalCue() ? CabViewSceneEffect::Arrival : CabViewSceneEffect::None;
		camera.effect_elapsed_ms = this->effect_mode == CabViewEffectMode::Full ?
			(camera.scene_effect == CabViewSceneEffect::Arrival ? this->timeline.PhaseElapsed() :
				static_cast<uint32_t>(std::min<uint64_t>(this->timeline.clock_ms, UINT32_MAX))) : 0;
		DrawCabView3D(r, camera, this->frame_id, 12.0f);
		this->held_camera = camera;
		this->held_frame = this->frame_id;
		this->has_held = true;
		if (this->show_help) DrawCabViewHelp(r);
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget == WID_CV_CAPTION) return GetString(STR_CAB_VIEW_CAPTION, this->vehicle_id);
		if (widget == WID_CV_EFFECT) {
			return GetString(this->effect_mode == CabViewEffectMode::Full ? STR_CAB_VIEW_EFFECT_FULL :
				this->effect_mode == CabViewEffectMode::Reduced ? STR_CAB_VIEW_EFFECT_REDUCED : STR_CAB_VIEW_EFFECT_OFF);
		}
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		if (this->CurrentPresentation().phase == CabViewPhase::Unsupported) { this->Close(); return; }
		switch (widget) {
			case WID_CV_VIEW:
				/* Left-drag looks around only where it also scrolls the map. */
				if (_settings_client.gui.scroll_mode != ViewportScrollMode::MapLMB) return;
				_scrolling_viewport = true;
				_cursor.fix_at = false;
				return;
			case WID_CV_LEFT: this->look.yaw = CabViewLook::SIDE; break;
			case WID_CV_AHEAD: this->look.ResetDirection(); break;
			case WID_CV_RIGHT: this->look.yaw = -CabViewLook::SIDE; break;
			case WID_CV_EFFECT: this->CycleEffect(); break;
			case WID_CV_HELP:
				this->show_help = !this->show_help;
				this->SetWidgetLoweredState(WID_CV_HELP, this->show_help);
				this->SetWidgetDirty(WID_CV_HELP);
				break;
			default: return;
		}
		this->LookChanged();
	}

	bool OnRightClick([[maybe_unused]] Point pt, WidgetID widget) override
	{
		if (widget != WID_CV_VIEW || _scrolling_viewport || _settings_client.gui.scroll_mode == ViewportScrollMode::MapLMB) return false;
		_scrolling_viewport = true;
		_cursor.fix_at = _settings_client.gui.scroll_mode == ViewportScrollMode::ViewportRMBFixed ||
			_settings_client.gui.scroll_mode == ViewportScrollMode::MapRMBFixed;
		return true;
	}

	void OnScroll(Point delta) override
	{
		if (this->CurrentPresentation().phase == CabViewPhase::Unsupported) { this->Close(); return; }
		this->look.Scroll(delta.x, delta.y, this->GetWidget<NWidgetBase>(WID_CV_VIEW)->current_x);
		this->LookChanged();
	}

	void OnMouseWheel(int wheel, WidgetID widget) override
	{
		if (widget != WID_CV_VIEW || _settings_client.gui.scrollwheel_scrolling == ScrollWheelScrolling::Off) return;
		this->look.Zoom(wheel);
		this->LookChanged();
	}

	EventState OnHotkey(int hotkey) override
	{
		if (_focused_window != this) return EventState::NotHandled;
		switch (static_cast<CabViewHotkey>(hotkey)) {
			case CabViewHotkey::LookLeft: this->look.Turn(CabViewLook::KEY_STEP, 0); break;
			case CabViewHotkey::LookRight: this->look.Turn(-CabViewLook::KEY_STEP, 0); break;
			case CabViewHotkey::LookUp: this->look.Turn(0, CabViewLook::KEY_STEP); break;
			case CabViewHotkey::LookDown: this->look.Turn(0, -CabViewLook::KEY_STEP); break;
			case CabViewHotkey::LookAhead: this->look.ResetDirection(); break;
			case CabViewHotkey::LookBehind: this->look.yaw = CabViewLook::PI; this->look.pitch = 0; break;
			case CabViewHotkey::ZoomIn: this->look.Zoom(-1); break;
			case CabViewHotkey::ZoomOut: this->look.Zoom(1); break;
			case CabViewHotkey::Effect: this->CycleEffect(); break;
			default: return EventState::NotHandled;
		}
		this->LookChanged();
		return EventState::Handled;
	}

	/** Step the arrival effect through Full, Reduced and Off. */
	void CycleEffect()
	{
		this->effect_mode = this->effect_mode == CabViewEffectMode::Full ? CabViewEffectMode::Reduced :
			this->effect_mode == CabViewEffectMode::Reduced ? CabViewEffectMode::Off : CabViewEffectMode::Full;
		this->timeline.CancelCue();
		this->SetWidgetDirty(WID_CV_EFFECT);
	}

	/** Repaint the view after a local look, zoom or effect change. */
	void LookChanged()
	{
		this->frame_id++;
		this->SetWidgetDirty(WID_CV_VIEW);
	}

	static inline HotkeyList hotkeys{"cab_view", {
		Hotkey('A', "look_left", CabViewHotkey::LookLeft),
		Hotkey('D', "look_right", CabViewHotkey::LookRight),
		Hotkey('W', "look_up", CabViewHotkey::LookUp),
		Hotkey('S', "look_down", CabViewHotkey::LookDown),
		Hotkey('R', "look_ahead", CabViewHotkey::LookAhead),
		Hotkey('B', "look_behind", CabViewHotkey::LookBehind),
		Hotkey({WKC_EQUALS, WKC_NUM_PLUS}, "zoom_in", CabViewHotkey::ZoomIn),
		Hotkey({WKC_MINUS, WKC_NUM_MINUS}, "zoom_out", CabViewHotkey::ZoomOut),
		Hotkey('E', "effect", CabViewHotkey::Effect),
	}};

	void OnRealtimeTick(uint delta_ms) override
	{
		if (this->CurrentPresentation().phase == CabViewPhase::Unsupported) { this->Close(); return; }
		/* Observe first: a transition discovered by this tick or an earlier paint
		 * must not spend the old phase's accumulated callback delta on arrival. */
		this->timeline.Advance(delta_ms);
		this->frame_id++;
		this->SetWidgetDirty(WID_CV_VIEW);
	}
};

static WindowDesc _cab_view_desc(
	WindowPosition::Automatic, "cab_view_train", 480, 300,
	WindowClass::VehicleCabView, WindowClass::VehicleView,
	{}, _nested_cab_view_widgets,
	&CabViewWindow::hotkeys
);

void ShowCabViewWindow(const Vehicle *v)
{
	if (!CanShowCabView(v)) return;
	CloseWindowByClass(WindowClass::VehicleCabView);
	AllocateWindowDescFront<CabViewWindow>(_cab_view_desc, v->index);
}

CabViewLook GetCabViewLook(VehicleID vehicle)
{
	const CabViewWindow *w = dynamic_cast<const CabViewWindow *>(FindWindowById(WindowClass::VehicleCabView, vehicle));
	return w != nullptr ? w->look : CabViewLook{};
}
