/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License, version 2.
 */

/** @file resource_sites_gui.cpp Paid area surveying and company discovery browser. */
#include "../stdafx.h"
#include "resource_sites.h"
#include "planet_manager.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../gfx_func.h"
#include "../viewport_func.h"
#include "../tilehighlight_func.h"
#include "../map_func.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../core/random_func.hpp"
#include "../timer/timer_window.h"
#include "../timer/timer.h"
#include "../table/strings.h"
#include "../table/sprites.h"
#include <map>
#include "../safeguards.h"

/** Widgets in the resource discovery browser. */
enum ResourceWidgets : WidgetID { RW_LIST, RW_SCROLL, RW_INFO, RW_SURVEY, RW_GOTO, RW_BUILD };
static std::map<TileIndex, uint8_t> _resource_overlay;
static CompanyID _resource_overlay_company = CompanyID::Invalid();
uint8_t GetResourceOverlay(TileIndex tile)
{
	if (_resource_overlay_company != _local_company || !ResourceSiteManager::Enabled()) return 0;
	auto it = _resource_overlay.find(tile);
	return it == _resource_overlay.end() ? 0 : it->second;
}
static constexpr auto _resource_widgets = std::to_array<NWidgetPart>({
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen), SetStringTip(STR_RESOURCE_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, RW_INFO), SetMinimalSize(520, 100), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, Colours::DarkGreen, RW_LIST), SetMatrixDataTip(1, 0, STR_RESOURCE_HELP), SetMinimalSize(500, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(RW_SCROLL),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, RW_SCROLL),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, RW_SURVEY), SetStringTip(STR_RESOURCE_SURVEY, STR_RESOURCE_HELP), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, RW_GOTO), SetStringTip(STR_RESOURCE_GOTO, STR_RESOURCE_HELP), SetFill(1, 0),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, RW_BUILD), SetStringTip(STR_RESOURCE_BUILD, STR_RESOURCE_HELP), SetFill(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
});
static WindowDesc _resource_desc(WindowPosition::Automatic, "resource_surveys", 540, 360,
	WindowClass::ResourceSurvey, WindowClass::None, WindowDefaultFlag::Construction, _resource_widgets);

/** The browser deliberately never puts undiscovered sites into its list or overlay. */
struct ResourceSurveyWindow : Window {
	Scrollbar *scroll = nullptr;
	std::vector<uint32_t> visible;
	uint32_t selected = 0;
	CompanyID company;
	bool surveying = false;

	ResourceSurveyWindow() : Window(_resource_desc), company(_local_company)
	{
		this->CreateNestedTree();
		this->scroll = this->GetScrollbar(RW_SCROLL);
		this->FinishInitNested(0);
		this->Refresh();
	}
	~ResourceSurveyWindow() override { _resource_overlay.clear(); MarkWholeScreenDirty(); }
	const ResourceSite *Selected() const
	{
		for (const auto &site : ResourceSiteManager::Sites()) if (site.id == selected && ResourceSiteManager::Discovered(company, site)) return &site;
		return nullptr;
	}
	void Refresh()
	{
		_resource_overlay_company = company;
		visible.clear();
		_resource_overlay.clear();
		if (company != _local_company) { this->Close(); return; }
		for (const auto &site : ResourceSiteManager::Sites()) {
			if (!ResourceSiteManager::Discovered(company, site)) continue;
			visible.push_back(site.id);
			uint8_t colour = Industry::IsValidID(site.occupant) ? 2 : 1;
			uint sx = TileX(site.anchor), sy = TileY(site.anchor);
			for (uint x = sx; x < sx + site.width; ++x) {
				_resource_overlay[TileXY(x, sy)] = colour;
				_resource_overlay[TileXY(x, sy + site.height - 1)] = colour;
			}
			for (uint y = sy; y < sy + site.height; ++y) {
				_resource_overlay[TileXY(sx, y)] = colour;
				_resource_overlay[TileXY(sx + site.width - 1, y)] = colour;
			}
		}
		scroll->SetCount(visible.size());
		const auto *site = Selected();
		this->SetWidgetDisabledState(RW_GOTO, site == nullptr);
		this->SetWidgetDisabledState(RW_BUILD, site == nullptr || Industry::IsValidID(site->occupant));
		this->SetDirty();
		MarkWholeScreenDirty();
	}
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &, Dimension &resize) override
	{
		if (widget == RW_LIST) { resize.height = GetCharacterHeight(FontSize::Normal) + padding.height + 4; size.height = resize.height * 8; }
		if (widget == RW_INFO) size.height = std::max(8 * GetCharacterHeight(FontSize::Normal), 2 * GetCharacterHeight(FontSize::Normal) + GetStringHeight(GetString(STR_RESOURCE_INSTRUCTIONS, ResourceSiteManager::SurveyPrice()), std::max(300u, size.width) - padding.width)) + padding.height;
	}
	void OnResize() override { scroll->SetCapacityFromWidget(this, RW_LIST); }
	void OnPaint() override { this->DrawWidgets(); }
	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		Rect ir = r.Shrink(WidgetDimensions::scaled.framerect);
		if (widget == RW_INFO) {
			ir.top = DrawStringMultiLine(ir, GetString(STR_RESOURCE_INSTRUCTIONS, ResourceSiteManager::SurveyPrice()), TextColour::Black);
			if (const auto *site = Selected()) {
				DrawString(ir, GetString(STR_FUND_INDUSTRY_INDUSTRY_BUILD_COST, GetIndustrySpec(ResourceSiteManager::ResolveType(*site))->GetConstructionCost()));
			}
		} else if (widget == RW_LIST) {
			int y = ir.top;
			int step = this->GetWidget<NWidgetCore>(RW_LIST)->resize_y;
			for (int i = scroll->GetPosition(); i < static_cast<int>(visible.size()) && i < scroll->GetPosition() + scroll->GetCapacity(); ++i) {
				const ResourceSite *site = nullptr;
				for (const auto &candidate : ResourceSiteManager::Sites()) if (candidate.id == visible[i]) { site = &candidate; break; }
				if (site == nullptr) continue;
				const auto *region = PlanetManager::GetRegion(site->world);
				DrawString(ir.left, ir.right, y, GetString(STR_RESOURCE_ROW,
					GetIndustrySpec(ResourceSiteManager::ResolveType(*site))->name,
					region == nullptr ? GetString(STR_RESOURCE_MAIN_WORLD) : region->name,
					TileX(site->anchor), TileY(site->anchor),
					Industry::IsValidID(site->occupant) ? STR_RESOURCE_OCCUPIED_LABEL : STR_RESOURCE_AVAILABLE),
					site->id == selected ? TextColour::White : TextColour::Black);
				y += step;
			}
		}
	}
	void OnClick(Point pt, WidgetID widget, int) override
	{
		if (company != _local_company) return;
		switch (widget) {
			case RW_LIST: {
				auto row = scroll->GetScrolledRowFromWidget(pt.y, this, RW_LIST);
				if (row >= 0 && row < static_cast<int>(visible.size())) selected = visible[row];
				this->Refresh();
				break;
			}
			case RW_SURVEY:
				surveying = true;
				HandlePlacePushButton(this, RW_SURVEY, SPR_CURSOR_INDUSTRY, HT_RECT);
				SetTileSelectSize(ResourceSiteManager::SURVEY_SIZE, ResourceSiteManager::SURVEY_SIZE);
				break;
			case RW_GOTO: if (const auto *site = Selected()) ScrollMainWindowToTile(site->anchor); break;
			case RW_BUILD:
				if (const auto *site = Selected()) {
					surveying = false;
					ScrollMainWindowToTile(site->anchor);
					HandlePlacePushButton(this, RW_BUILD, SPR_CURSOR_INDUSTRY, HT_RECT);
					SetTileSelectSize(1, 1);
				}
				break;
		}
	}
	void OnPlaceObject(Point, TileIndex tile) override
	{
		if (company != _local_company) return;
		if (surveying) {
			Command<Commands::SurveyResources>::Post(STR_RESOURCE_SURVEY_FAILED, tile);
		} else if (const auto *site = Selected()) {
			Command<Commands::BuildIndustry>::Post(STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY, tile, ResourceSiteManager::ResolveType(*site), 0, true, InteractiveRandom());
		}
		this->Refresh();
	}
	void OnPlaceObjectAbort() override { this->RaiseButtons(); }
	void OnInvalidateData(int = 0, bool gui_scope = true) override { if (gui_scope) this->Refresh(); }
	const IntervalTimer<TimerWindow> refresh_interval = {std::chrono::seconds(1), [this](auto) { this->Refresh(); }};
};
void ShowResourceSurveyWindow()
{
	if (!ResourceSiteManager::Enabled() || !Company::IsValidID(_local_company)) return;
	if (BringWindowToFrontById(WindowClass::ResourceSurvey, 0)) return;
	new ResourceSurveyWindow();
}
