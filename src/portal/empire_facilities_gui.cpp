/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file empire_facilities_gui.cpp Implementation of Empire Industrial Facilities and Supply Chain dashboard window. */

#include "../stdafx.h"
#include "empire_facilities_gui.h"
#include "production_chain.h"
#include "integrated_economy.h"
#include "portal_cmd.h"
#include "planet_manager.h"
#include "logistics_hub.h"
#include "company_stockpile.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../station_base.h"
#include "../station_func.h"
#include "../industry.h"
#include "../viewport_func.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../textbuf_gui.h"
#include "../cargotype.h"
#include "../widgets/empire_facilities_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"

#include "../safeguards.h"

enum class EmpireFacilityFilter : uint8_t {
	All = 0,
	Structural = 1,
	Electronics = 2,
	Propulsion = 3,
	DataCrystals = 4,
};

static void SafeScrollMainWindowToTile(TileIndex tile)
{
	if (tile == INVALID_TILE) return;
	if (FindWindowById(WindowClass::MainWindow, 0) == nullptr) return;
	ScrollMainWindowToTile(tile);
}

static std::string GetFacilityCargoName(CargoType cargo)
{
	if (cargo < CargoSpec::GetArraySize() && CargoSpec::Get(cargo)->IsValid()) {
		return GetString(CargoSpec::Get(cargo)->name);
	}
	return "Cargo";
}

/**
 * Draw a label in the middle of a row button.
 * @param btn Button rectangle.
 * @param label Text.
 * @param colour Text colour.
 */
static void DrawButtonLabel(const Rect &btn, std::string_view label, TextColour colour)
{
	DrawString(btn.left, btn.right, CentreBounds(btn.top, btn.bottom, GetCharacterHeight(FontSize::Normal)), label, colour, AlignmentH::Centre);
}

/**
 * Get the managed industry behind an integrated-economy facility row.
 * Legacy station facilities number their own IDs, so they never map to an industry.
 * @param f Facility row.
 * @return The managed industry, or nullptr for a legacy facility.
 */
static const Industry *GetFacilityIndustry(const ProcessingFacility &f)
{
	if (!IntegratedEconomy::Enabled() || f.id == INVALID_FACILITY) return nullptr;
	return Industry::GetIfValid(IndustryID{uint16_t(f.id - 1)});
}

/**
 * Get the units waiting in a facility's machine buffer.
 * @param f Facility row.
 * @param cargo Cargo to look up.
 * @param input True for the input buffer, false for the output buffer.
 * @return Units waiting.
 */
uint32_t GetFacilityBufferedUnits(const ProcessingFacility &f, CargoType cargo, bool input)
{
	if (const Industry *ind = GetFacilityIndustry(f); ind != nullptr) {
		if (input) {
			auto it = ind->GetCargoAccepted(cargo);
			return (it != ind->accepted.end()) ? it->waiting : 0;
		}
		auto it = ind->GetCargoProduced(cargo);
		return (it != ind->produced.end()) ? it->waiting : 0;
	}
	const auto &buffers = input ? f.input_buffers : f.output_buffers;
	auto it = buffers.find(cargo);
	return (it != buffers.end()) ? it->second : 0;
}

/**
 * Query the cost of commissioning works without executing or checking cash.
 * @param industry Factory to commission.
 * @param company Company asking.
 * @return The authoritative command cost, or std::nullopt when commissioning would be rejected.
 */
std::optional<Money> QueryWorksCommissionPrice(IndustryID industry, CompanyID company)
{
	AutoRestoreBackup cur_company(_current_company, company);
	auto res = Command<Commands::CommissionEconomyWork>::Do(DoCommandFlag::QueryCost, industry, FactoryWorkType::Capacity);
	if (res.Failed()) return std::nullopt;
	return res.GetCost();
}

/**
 * Distance between text lines; at least 16 traditional pixels, more when the font needs it.
 * @param line_height Normal font height.
 * @return Line pitch in pixels.
 */
int FacilityRowLayout::LineStep(int line_height)
{
	return std::max(line_height + ScaleGUITrad(2), ScaleGUITrad(16));
}

/**
 * Height of one facility row: its text lines plus padding.
 * @param line_height Normal font height.
 * @return Row pitch in pixels.
 */
int FacilityRowLayout::Height(int line_height)
{
	return LINES * LineStep(line_height) + ScaleGUITrad(4);
}

/**
 * Lay out one facility row: short text lines on the left, then a 190 traditional pixel action
 * column with three works/retire buttons over two navigation buttons.
 * @param left Left edge of the list.
 * @param right Right edge of the list.
 * @param top Top of this row.
 * @param line_height Normal font height.
 */
FacilityRowLayout::FacilityRowLayout(int left, int right, int top, int line_height)
{
	const int pad = ScaleGUITrad(2);
	const int gap = ScaleGUITrad(4);
	const int step = LineStep(line_height);
	this->row = {left, top, right, top + Height(line_height) - pad};

	const int col_left = right - ScaleGUITrad(190);
	const int col_right = right - gap;
	this->text = {left + gap, top + pad, col_left - ScaleGUITrad(6), this->row.bottom};
	for (int i = 0; i < LINES; ++i) this->lines[i] = {this->text.left, top + pad + i * step, this->text.right, top + (i + 1) * step};

	const int third = (col_right - col_left - 2 * gap) / 3;
	const int upper_top = top + gap;
	const int upper_bottom = top + pad + 2 * step - gap;
	this->works_first = {col_left, upper_top, col_left + third, upper_bottom};
	this->works_second = {this->works_first.right + gap, upper_top, this->works_first.right + gap + third, upper_bottom};
	this->works_span = {col_left, upper_top, this->works_second.right, upper_bottom};
	this->retire = {this->works_second.right + gap, upper_top, col_right, upper_bottom};

	const int half = (col_right - col_left - gap) / 2;
	const int lower_top = top + pad + 2 * step;
	const int lower_bottom = top + pad + 4 * step - gap;
	this->plant = {col_left, lower_top, col_left + half, lower_bottom};
	this->station = {this->plant.right + gap, lower_top, col_right, lower_bottom};

	this->platform = {this->text.right - ScaleGUITrad(120), this->lines[LINES - 1].top - pad, this->text.right, this->lines[LINES - 1].bottom};
}

FacilityBottleneck DiagnoseFacilityBottleneck(const ProcessingFacility &f, const ProductionRecipe *rec, uint32_t throughput, uint32_t in_cap, uint32_t out_cap)
{
	FacilityBottleneck b;
	if (rec == nullptr) {
		b.badge = "[No Recipe]";
		b.colour = TextColour::Grey;
		return b;
	}

	if (!IntegratedEconomy::Enabled()) {
		FacilityStatus status = ProductionChainManager::GetFacilityStatus(&f);
		switch (status) {
			case FacilityStatus::Overflow:
				b.badge = fmt::format("[Overflow: {} t to Hub]", f.last_month_hub_overflow);
				b.colour = TextColour::Gold;
				break;
			case FacilityStatus::Active:
				b.badge = fmt::format("[Active: {}/{} t/mo]", f.last_month_production, f.monthly_capacity);
				b.colour = TextColour::White;
				break;
			case FacilityStatus::Starved:
				b.badge = "[Starved: Need Inputs]";
				b.colour = TextColour::Red;
				break;
			case FacilityStatus::Idle:
			default:
				b.badge = fmt::format("[Idle (Cap: {} t/mo)]", f.monthly_capacity);
				b.colour = TextColour::Grey;
				break;
		}
		return b;
	}

	const Industry *ind = GetFacilityIndustry(f);

	/* 1. Tech lock */
	TechID tech = IntegratedEconomy::RecipeTech(rec->id);
	if (Company::IsValidID(f.owner) && tech != TECH_NONE && !TechTreeManager::IsTechUnlocked(f.owner, tech)) {
		const TechProjectNode *node = TechTreeManager::GetNode(tech);
		std::string tech_name = (node != nullptr) ? node->name : fmt::format("Tech {}", tech);
		b.badge = fmt::format("[Locked: Requires {}]", tech_name);
		b.colour = TextColour::Red;
		return b;
	}

	/* 2. Blocked output */
	if (ind != nullptr && IntegratedEconomy::OutputBatchLimit(ind) == 0) {
		uint32_t yield = (Company::IsValidID(f.owner) && TechTreeManager::IsTechUnlocked(f.owner, TECH_MATERIALS_3)) ? 115 : 100;
		for (auto [cargo, units] : rec->outputs) {
			auto out_it = ind->GetCargoProduced(cargo);
			uint32_t waiting = (out_it != ind->produced.end()) ? out_it->waiting : 0;
			uint32_t cap = (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2)
				? std::min(65535u, (out_cap * units * yield + 99) / 100)
				: std::min(65535u, (throughput * units * 3 * yield + 99) / 100);
			if (waiting >= cap || (cap - waiting) < units) {
				std::string cargo_name = GetFacilityCargoName(cargo);
				b.badge = fmt::format("[Blocked: Output {} Full ({}/{})]", cargo_name, waiting, cap);
				b.colour = TextColour::Gold;
				return b;
			}
		}
		b.badge = "[Output blocked]";
		b.colour = TextColour::Gold;
		return b;
	}

	/* 3. Starved inputs (missing or insufficient) */
	CargoType missing_cargo = NUM_CARGO;
	uint32_t missing_req = 0;
	CargoType insufficient_cargo = NUM_CARGO;
	uint32_t insufficient_have = 0;
	uint32_t insufficient_req = 0;
	uint32_t batches_permitted = UINT32_MAX;

	for (auto [cargo, req] : rec->inputs) {
		uint32_t waiting = GetFacilityBufferedUnits(f, cargo, true);

		uint32_t c_cap = (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2)
			? in_cap * req
			: throughput * req * 3;
		if (req > 0) {
			batches_permitted = std::min(batches_permitted, std::min(waiting, c_cap) / req);
		}

		if (waiting == 0 && missing_cargo == NUM_CARGO) {
			missing_cargo = cargo;
			missing_req = req;
		} else if (waiting < req && insufficient_cargo == NUM_CARGO) {
			insufficient_cargo = cargo;
			insufficient_have = waiting;
			insufficient_req = req;
		}
	}
	if (batches_permitted == UINT32_MAX) batches_permitted = 0;
	b.batches_permitted = batches_permitted;

	if (missing_cargo != NUM_CARGO) {
		std::string cargo_name = GetFacilityCargoName(missing_cargo);
		b.badge = fmt::format("[Starved: Missing {} (0/{} per batch)]", cargo_name, missing_req);
		b.colour = TextColour::Red;
		return b;
	}

	if (insufficient_cargo != NUM_CARGO) {
		std::string cargo_name = GetFacilityCargoName(insufficient_cargo);
		b.badge = fmt::format("[Starved: Insufficient {} ({}/{} for batch)]", cargo_name, insufficient_have, insufficient_req);
		b.colour = TextColour::Red;
		return b;
	}

	/* 4. Active or Ready */
	if (f.last_month_production > 0) {
		b.badge = fmt::format("[Active: {}/{} batches/mo]", f.last_month_production, throughput);
		b.colour = TextColour::Green;
		return b;
	}

	if (batches_permitted > 0) {
		b.badge = fmt::format("[Ready: {} {} buffered]", batches_permitted, batches_permitted == 1 ? "batch" : "batches");
		b.colour = TextColour::Green;
		return b;
	}

	b.badge = fmt::format("[Idle: Cap {} batches/mo]", throughput);
	b.colour = TextColour::Grey;
	return b;
}

static constexpr std::initializer_list<NWidgetPart> _nested_empire_facilities_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkGreen),
		NWidget(WWT_CAPTION, Colours::DarkGreen, WID_EF_CAPTION), SetStringTip(STR_EMPIRE_FACILITIES_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkGreen),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkGreen),
		NWidget(WWT_STICKYBOX, Colours::DarkGreen),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_ALL), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_ALL, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_A), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_A, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_B), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_B, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_C), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_C, STR_NULL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_TAB_PIPE_D), SetMinimalSize(110, 20), SetStringTip(STR_EMPIRE_FACILITIES_TAB_PIPE_D, STR_NULL),
		NWidget(WWT_PANEL, Colours::DarkGreen), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_REFRESH), SetMinimalSize(80, 20), SetStringTip(STR_EMPIRE_FACILITIES_REFRESH, STR_EMPIRE_FACILITIES_REFRESH_TOOLTIP),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkGreen, WID_EF_LIST_PANEL), SetMinimalSize(838, 350), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkGreen, WID_EF_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkGreen, WID_EF_SUMMARY_PANEL), SetMinimalSize(850, 40), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkGreen, WID_EF_UPGRADE_ALL_BTN), SetMinimalSize(300, 22), SetStringTip(STR_EMPIRE_FACILITIES_UPGRADE_ALL, STR_EMPIRE_FACILITIES_UPGRADE_ALL_TOOLTIP),
		NWidget(WWT_PANEL, Colours::DarkGreen), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, Colours::DarkGreen),
	EndContainer(),
};

static WindowDesc _empire_facilities_desc(
	WindowPosition::Automatic, "view_empire_facilities", 860, 520,
	WindowClass::EmpireFacilities, WindowClass::None,
	{},
	_nested_empire_facilities_widgets
);

struct EmpireFacilitiesWindow : Window {
	EmpireFacilityFilter active_filter = EmpireFacilityFilter::All;
	Scrollbar *vscroll = nullptr;
	CompanyID target_company = CompanyID::Invalid();

	EmpireFacilitiesWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_EF_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);
		this->target_company = _local_company;
		this->SetWidgetDisabledState(WID_EF_UPGRADE_ALL_BTN, IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2);
	}

	IndustryID retire_industry = IndustryID::Invalid(); ///< Integrated factory awaiting retirement confirmation.
	StationID retire_station = StationID::Invalid(); ///< Legacy facility station awaiting retirement confirmation.

	/**
	 * Ask before retiring a facility; retiring demolishes it and discards its buffered cargo.
	 * @param f Facility to retire.
	 */
	void ConfirmRetire(const ProcessingFacility &f)
	{
		std::string place;
		if (const Industry *ind = GetFacilityIndustry(f)) {
			this->retire_industry = ind->index;
			this->retire_station = StationID::Invalid();
			place = GetString(STR_INDUSTRY_NAME, ind->index);
		} else if (!IntegratedEconomy::Enabled() && Station::IsValidID(f.linked_station)) {
			this->retire_industry = IndustryID::Invalid();
			this->retire_station = f.linked_station;
			place = GetString(STR_STATION_NAME, f.linked_station);
		} else {
			return;
		}
		const ProductionRecipe *rec = ProductionChainManager::GetRecipe(f.recipe_id);
		std::string name = (rec != nullptr) ? fmt::format("{} at {}", rec->name, place) : place;
		ShowQuery(GetEncodedString(STR_EMPIRE_FACILITIES_RETIRE_CAPTION), GetEncodedString(STR_EMPIRE_FACILITIES_RETIRE_QUERY, std::move(name)),
			this, EmpireFacilitiesWindow::RetireConfirmed);
	}

	/**
	 * Retire the facility chosen in ConfirmRetire once the player agrees.
	 * @param w The facilities window.
	 * @param confirmed Whether the player chose Yes.
	 */
	static void RetireConfirmed(Window *w, bool confirmed)
	{
		auto *window = static_cast<EmpireFacilitiesWindow *>(w);
		if (confirmed && window->retire_industry != IndustryID::Invalid()) {
			Command<Commands::ManageEconomyFactory>::Post(window->retire_industry, true);
		} else if (confirmed && window->retire_station != StationID::Invalid()) {
			Command<Commands::RemoveProcessingFacility>::Post(STR_ERROR_CAN_T_REMOVE_FACILITY, window->retire_station);
		}
		window->retire_industry = IndustryID::Invalid();
		window->retire_station = StationID::Invalid();
		window->SetDirty();
	}

	void OnResize() override
	{
		const NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_EF_LIST_PANEL);
		const int height = static_cast<int>(nwi->current_y) - WidgetDimensions::scaled.framerect.Vertical();
		this->vscroll->SetCapacity(std::max(0, height) / FacilityRowLayout::Height(GetCharacterHeight(FontSize::Normal)));
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		/* Commonwealth economies upgrade in batches, legacy station facilities in tonnes. */
		if (widget == WID_EF_UPGRADE_ALL_BTN && IntegratedEconomy::Enabled()) return GetString(STR_EMPIRE_FACILITIES_UPGRADE_ALL_BATCHES);
		return this->Window::GetWidgetString(widget, stringid);
	}

	void OnInit() override
	{
		this->SetWidgetDisabledState(WID_EF_UPGRADE_ALL_BTN, IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2);
	}

	std::vector<ProcessingFacility> GetFilteredFacilities() const
	{
		std::vector<ProcessingFacility> all = ProductionChainManager::GetAllFacilities();
		std::vector<ProcessingFacility> filtered;
		filtered.reserve(all.size());

		for (const auto &f : all) {
			if (this->target_company != CompanyID::Invalid() && f.owner != this->target_company) {
				continue;
			}
			const ProductionRecipe *rec = ProductionChainManager::GetRecipe(f.recipe_id);
			if (rec == nullptr) continue;

			switch (this->active_filter) {
				case EmpireFacilityFilter::All:
					filtered.push_back(f);
					break;
				case EmpireFacilityFilter::Structural:
					if (rec->pipeline == PipelineType::Structural) filtered.push_back(f);
					break;
				case EmpireFacilityFilter::Electronics:
					if (rec->pipeline == PipelineType::Electronics) filtered.push_back(f);
					break;
				case EmpireFacilityFilter::Propulsion:
					if (rec->pipeline == PipelineType::Propulsion) filtered.push_back(f);
					break;
				case EmpireFacilityFilter::DataCrystals:
					if (rec->pipeline == PipelineType::DataCrystals) filtered.push_back(f);
					break;
			}
		}
		return filtered;
	}

	void OnPaint() override
	{
		auto filtered = this->GetFilteredFacilities();
		this->vscroll->SetCount(filtered.size());

		this->SetWidgetLoweredState(WID_EF_TAB_ALL,    this->active_filter == EmpireFacilityFilter::All);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_A, this->active_filter == EmpireFacilityFilter::Structural);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_B, this->active_filter == EmpireFacilityFilter::Electronics);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_C, this->active_filter == EmpireFacilityFilter::Propulsion);
		this->SetWidgetLoweredState(WID_EF_TAB_PIPE_D, this->active_filter == EmpireFacilityFilter::DataCrystals);

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (_cur_dpi == nullptr || _screen.dst_ptr == nullptr) return;

		switch (widget) {
			case WID_EF_LIST_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto filtered = this->GetFilteredFacilities();
				if (filtered.empty()) {
					DrawString(tr, "No industrial facilities registered for current filter.", TextColour::Grey);
					return;
				}

				const int line_height = GetCharacterHeight(FontSize::Normal);
				const int row_height = FacilityRowLayout::Height(line_height);
				size_t start = this->vscroll->GetPosition();
				size_t end = std::min(filtered.size(), start + (tr.Height() / row_height));

				int y = tr.top;
				for (size_t i = start; i < end; ++i) {
					const auto &f = filtered[i];
					const ProductionRecipe *rec = ProductionChainManager::GetRecipe(f.recipe_id);
					const Station *st = Station::GetIfValid(f.linked_station);
					const PlanetRegion *region = PlanetManager::GetRegion(f.world_id);

					const FacilityRowLayout layout(tr.left, tr.right, y, line_height);
					GfxFillRect(layout.row, (i % 2 == 0) ? GetColourGradient(Colours::DarkGreen, Shade::Darkest) : GetColourGradient(Colours::DarkGreen, Shade::Dark));

					const Rect &tr_text = layout.text;

					uint32_t throughput = f.monthly_capacity;
					uint32_t in_cap = 300, out_cap = 300;
					FactoryWorkType work = FactoryWorkType::None;
					IndustryID ind_id{uint16_t(f.id - 1)};
					if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2) {
						auto f_it = IntegratedEconomy::Factories().find(ind_id);
						if (f_it != IntegratedEconomy::Factories().end()) {
							work = f_it->second.work;
							throughput = f_it->second.capacity;
							in_cap = f_it->second.input_capacity;
							out_cap = f_it->second.output_capacity;
						}
					}

					FacilityBottleneck bottleneck = DiagnoseFacilityBottleneck(f, rec, throughput, in_cap, out_cap);

					/* Line 1: plant and its world, with role and phase kept separate. */
					EconomicRole role = IntegratedEconomy::Role(f.world_id);
					StringID role_str_id = (role == EconomicRole::Core) ? STR_EMPIRE_FACILITIES_ROLE_CORE :
					                       (role == EconomicRole::Industrial) ? STR_EMPIRE_FACILITIES_ROLE_INDUSTRIAL :
					                       STR_EMPIRE_FACILITIES_ROLE_FRONTIER;
					std::string role_name = GetString(role_str_id);

					std::string phase_name;
					if (region != nullptr) {
						StringID phase_str_id = (region->phase == WorldPhase::Phase1_Core) ? STR_EMPIRE_FACILITIES_PHASE_1 :
						                        (region->phase == WorldPhase::Phase2_Developed) ? STR_EMPIRE_FACILITIES_PHASE_2 :
						                        (region->phase == WorldPhase::Phase3_Frontier) ? STR_EMPIRE_FACILITIES_PHASE_3 :
						                        STR_EMPIRE_FACILITIES_PHASE_4;
						phase_name = GetString(phase_str_id);
					} else {
						phase_name = GetString(STR_EMPIRE_FACILITIES_PHASE_1);
					}

					std::string world_str = (region != nullptr)
						? fmt::format("World {}: {} ({}, {})", f.world_id.base() + 1, region->name, role_name, phase_name)
						: fmt::format("World {} ({})", f.world_id.base() + 1, role_name);

					const Station *target_st = st;
					if (target_st == nullptr) {
						if (const Industry *ind = GetFacilityIndustry(f)) {
							if (ind->neutral_station != nullptr) {
								target_st = ind->neutral_station;
							} else if (!ind->stations_near.empty()) {
								target_st = *ind->stations_near.begin();
							}
						}
					}

					std::string st_name = (target_st != nullptr)
						? fmt::format("{} [{}, {}]", target_st->name, TileX(f.tile), TileY(f.tile))
						: fmt::format("Tile [{}, {}]", TileX(f.tile), TileY(f.tile));
					DrawString(layout.lines[0], fmt::format("{}  -  {}", st_name, world_str), TextColour::Yellow);

					/* Line 2: status badge and recipe name. */
					int badge_width = GetStringBoundingBox(bottleneck.badge).width;
					Rect badge_r = Rect{tr_text.left, layout.lines[1].top, std::min(tr_text.right, tr_text.left + badge_width), layout.lines[1].bottom};
					DrawString(badge_r, bottleneck.badge, bottleneck.colour);
					int recipe_x = tr_text.left + badge_width + ScaleGUITrad(8);
					if (recipe_x < tr_text.right) {
						DrawString(Rect{recipe_x, layout.lines[1].top, tr_text.right, layout.lines[1].bottom}, rec != nullptr ? rec->name : "No recipe", TextColour::White);
					}

					/* Line 3: exact recipe ratio. */
					std::string in_ratio;
					if (rec != nullptr) {
						for (size_t idx = 0; idx < rec->inputs.size(); ++idx) {
							if (idx > 0) in_ratio += " + ";
							in_ratio += fmt::format("{} {}", rec->inputs[idx].second, GetFacilityCargoName(rec->inputs[idx].first));
						}
					}
					std::string out_ratio;
					if (rec != nullptr) {
						for (size_t idx = 0; idx < rec->outputs.size(); ++idx) {
							if (idx > 0) out_ratio += " + ";
							out_ratio += fmt::format("{} {}", rec->outputs[idx].second, GetFacilityCargoName(rec->outputs[idx].first));
						}
					}
					if (rec != nullptr) DrawString(layout.lines[2], fmt::format("Recipe: {} -> {}", in_ratio, out_ratio), TextColour::White);

					/* Line 4: rated throughput and the last completed month. */
					std::string rate_line;
					if (IntegratedEconomy::Enabled()) {
						rate_line = fmt::format("Rated: {} batches/mo  |  Last month: {} batches", throughput, f.last_month_production);
					} else {
						rate_line = fmt::format("Capacity: {} t/mo  |  Last month: {}", throughput,
							f.last_month_production > 0 ? fmt::format("{} t", f.last_month_production) : GetString(STR_EMPIRE_FACILITIES_UNAVAILABLE));
					}
					DrawString(layout.lines[3], rate_line, TextColour::White);

					/* Lines 5-6: machine input and output buffers with the batches they allow. */
					std::string in_desc = "Input buffer: ";
					if (rec != nullptr) {
						for (size_t idx = 0; idx < rec->inputs.size(); ++idx) {
							auto [in_c, in_req] = rec->inputs[idx];
							uint32_t waiting = GetFacilityBufferedUnits(f, in_c, true);
							uint32_t c_cap = (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2)
								? in_cap * in_req
								: throughput * in_req * 3;
							uint32_t b_perm = (in_req > 0) ? (waiting / in_req) : 0;
							if (idx > 0) in_desc += "; ";
							in_desc += fmt::format("{} {}/{} (enough for {} batches)", GetFacilityCargoName(in_c), waiting, c_cap, b_perm);
						}
					}
					DrawString(layout.lines[4], in_desc, TextColour::Silver);
					std::string out_desc = "Output buffer: ";
					if (rec != nullptr) {
						uint32_t yield = (Company::IsValidID(f.owner) && TechTreeManager::IsTechUnlocked(f.owner, TECH_MATERIALS_3)) ? 115 : 100;
						for (size_t idx = 0; idx < rec->outputs.size(); ++idx) {
							auto [out_c, out_yield] = rec->outputs[idx];
							uint32_t waiting = GetFacilityBufferedUnits(f, out_c, false);
							uint32_t c_cap = (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2)
								? std::min(65535u, (out_cap * out_yield * yield + 99) / 100)
								: std::min(65535u, (throughput * out_yield * 3 * yield + 99) / 100);
							uint32_t b_space = (out_yield > 0 && c_cap > waiting) ? ((c_cap - waiting) / out_yield) : 0;
							if (idx > 0) out_desc += "; ";
							out_desc += fmt::format("{} {}/{} (room for {} batches)", GetFacilityCargoName(out_c), waiting, c_cap, b_space);
						}
					}
					DrawString(layout.lines[5], out_desc, TextColour::Silver);

					/* Line 7: works and the planetary warehouse stock that pays for them. */
					WorldID world = (f.tile != INVALID_TILE) ? PlanetManager::GetTileWorld(f.tile) : WorldID{0};
					CargoType stel_c = StockpileManager::RoleToDefaultCargo(FabricationRole::StructuralMetal);
					CargoType ball_c = StockpileManager::RoleToDefaultCargo(FabricationRole::Ballast);
					uint32_t stel_stock = (stel_c != INVALID_CARGO) ? StockpileManager::GetStock(world, _local_company, stel_c) : 0;
					uint32_t ball_stock = (ball_c != INVALID_CARGO) ? StockpileManager::GetStock(world, _local_company, ball_c) : 0;
					const std::string stock_str = fmt::format("World stock: {} STEL, {} BALL", stel_stock, ball_stock);

					bool is_owner = (Company::IsValidID(_local_company) && f.owner == _local_company);
					Rect works_r = layout.lines[FacilityRowLayout::LINES - 1];
					std::string works_line;
					if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2) {
						if (work == FactoryWorkType::None) {
							std::optional<Money> queried_price = QueryWorksCommissionPrice(ind_id, _local_company);
							works_line = queried_price.has_value()
								? fmt::format("Works: none (cost {} + 160 STEL + 80 BALL)  |  {}", GetString(STR_JUST_CURRENCY_LONG, *queried_price), stock_str)
								: fmt::format("Works: none (cannot commission now)  |  {}", stock_str);
						} else if (work == FactoryWorkType::Capacity) {
							works_line = fmt::format("Works: Capacity (150 batches/mo)  |  {}", stock_str);
						} else {
							works_line = fmt::format("Works: Reserve (input buffer 600 batches)  |  {}", stock_str);
						}
					} else {
						works_line = stock_str;
					}
					if (!IntegratedEconomy::Enabled()) {
						/* Legacy station facilities: platform holding target cycles 0 -> 50 -> 100 -> 200 t. */
						const Rect &btn_platform = layout.platform;
						GfxFillRect(btn_platform, GetColourGradient(Colours::Grey, is_owner ? Shade::Normal : Shade::Dark));
						std::string platform_label = (f.platform_capacity == 0) ? "Platform: 0 (Hub)" : fmt::format("Platform: {} t", f.platform_capacity);
						DrawButtonLabel(btn_platform, platform_label, is_owner ? TextColour::White : TextColour::Grey);
						works_r.right = btn_platform.left - ScaleGUITrad(6);
					}
					DrawString(works_r, works_line, TextColour::Silver);

					/* Right Action Column: Upper half works/controls, Lower half navigation */
					if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2) {
						if (work == FactoryWorkType::None) {
							const Rect &btn_cap = layout.works_first;
							GfxFillRect(btn_cap, GetColourGradient(Colours::Grey, is_owner ? Shade::Normal : Shade::Dark));
							DrawButtonLabel(btn_cap, GetString(STR_WORKS_CAPACITY_BTN), is_owner ? TextColour::Yellow : TextColour::Grey);

							const Rect &btn_res = layout.works_second;
							GfxFillRect(btn_res, GetColourGradient(Colours::Grey, is_owner ? Shade::Normal : Shade::Dark));
							DrawButtonLabel(btn_res, GetString(STR_WORKS_RESERVE_BTN), is_owner ? TextColour::Yellow : TextColour::Grey);
						} else if (work == FactoryWorkType::Capacity) {
							DrawButtonLabel(layout.works_span, GetString(STR_WORKS_STATUS_CAPACITY), TextColour::Green);
						} else {
							DrawButtonLabel(layout.works_span, GetString(STR_WORKS_STATUS_RESERVE), TextColour::Green);
						}

						const Rect &btn_retire = layout.retire;
						GfxFillRect(btn_retire, GetColourGradient(Colours::Red, is_owner ? Shade::Normal : Shade::Dark));
						DrawButtonLabel(btn_retire, GetString(STR_EMPIRE_FACILITIES_RETIRE_BTN), is_owner ? TextColour::White : TextColour::Grey);
					} else {
						const Rect &btn_upgrade = layout.works_span;
						GfxFillRect(btn_upgrade, GetColourGradient(Colours::Grey, is_owner ? Shade::Normal : Shade::Dark));
						DrawButtonLabel(btn_upgrade, "Upgrade (+50)", is_owner ? TextColour::Yellow : TextColour::Grey);

						const Rect &btn_retire = layout.retire;
						GfxFillRect(btn_retire, GetColourGradient(Colours::Red, is_owner ? Shade::Normal : Shade::Dark));
						DrawButtonLabel(btn_retire, GetString(STR_EMPIRE_FACILITIES_RETIRE_BTN), is_owner ? TextColour::White : TextColour::Grey);
					}

					/* Navigation buttons */
					const Rect &btn_plant = layout.plant;
					GfxFillRect(btn_plant, GetColourGradient(Colours::Grey, Shade::Normal));
					DrawButtonLabel(btn_plant, GetString(STR_EMPIRE_FACILITIES_LOCATE_PLANT), TextColour::White);

					const Rect &btn_station = layout.station;
					GfxFillRect(btn_station, GetColourGradient(Colours::Grey, Shade::Normal));
					DrawButtonLabel(btn_station, target_st != nullptr ? GetString(STR_EMPIRE_FACILITIES_LOCATE_STATION) : GetString(STR_EMPIRE_FACILITIES_LOCATE_HUB), TextColour::White);

					y += row_height;
				}
				break;
			}

			case WID_EF_SUMMARY_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto all_facs = ProductionChainManager::GetAllFacilities();
				uint32_t total_active = 0;
				uint32_t total_starved = 0;
				uint32_t total_monthly_prod = 0;

				for (const auto &f : all_facs) {
					if (this->target_company != CompanyID::Invalid() && f.owner != this->target_company) continue;
					total_monthly_prod += f.last_month_production;
					FacilityStatus status = ProductionChainManager::GetFacilityStatus(&f);
					if (status == FacilityStatus::Starved) total_starved++;
					else if (status == FacilityStatus::Active || status == FacilityStatus::Overflow) total_active++;
				}

				uint64_t total_overflow = ProductionChainManager::GetTotalHubOverflow();
				uint32_t last_overflow = ProductionChainManager::GetLastMonthHubOverflow();

				std::string summary = IntegratedEconomy::Enabled()
					? fmt::format("Active: {}  |  Starved: {}  |  Output last month: {} batches", total_active, total_starved, total_monthly_prod)
					: fmt::format("Active: {}  |  Starved: {}  |  Monthly Output: {} t  |  Hub Overflow: {} t (All-Time: {} t)",
						total_active, total_starved, total_monthly_prod, last_overflow, total_overflow);
				DrawString(tr, summary, TextColour::White, AlignmentH::Centre);
				break;
			}

			default:
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_EF_TAB_ALL:
				this->active_filter = EmpireFacilityFilter::All;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_A:
				this->active_filter = EmpireFacilityFilter::Structural;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_B:
				this->active_filter = EmpireFacilityFilter::Electronics;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_C:
				this->active_filter = EmpireFacilityFilter::Propulsion;
				this->SetDirty();
				break;
			case WID_EF_TAB_PIPE_D:
				this->active_filter = EmpireFacilityFilter::DataCrystals;
				this->SetDirty();
				break;
			case WID_EF_REFRESH:
				this->SetDirty();
				break;

			case WID_EF_UPGRADE_ALL_BTN: {
				if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2) {
					// Generic upgrade is disabled in v2; works require an explicit choice.
					break;
				}
				auto filtered = this->GetFilteredFacilities();
				for (const auto &f : filtered) {
					if (IntegratedEconomy::Enabled()) {
						Command<Commands::ManageEconomyFactory>::Post(IndustryID{uint16_t(f.id - 1)}, false);
						continue;
					}
					if (f.linked_station != StationID::Invalid()) {
						Command<Commands::UpgradeProcessingFacility>::Post(STR_ERROR_CAN_T_UPGRADE_FACILITY, f.linked_station, 50);
					}
				}
				this->SetDirty();
				break;
			}

			case WID_EF_LIST_PANEL: {
				NWidgetBase *nwi = this->GetWidget<NWidgetBase>(WID_EF_LIST_PANEL);
				Rect r = nwi->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				if (pt.y < r.top || pt.y >= r.bottom) return;

				const int line_height = GetCharacterHeight(FontSize::Normal);
				const int row_height = FacilityRowLayout::Height(line_height);
				const int row = (pt.y - r.top) / row_height;
				size_t idx = this->vscroll->GetPosition() + row;
				auto filtered = this->GetFilteredFacilities();
				if (idx >= filtered.size()) return;

				const auto &f = filtered[idx];
				const FacilityRowLayout layout(r.left, r.right, r.top + row * row_height, line_height);
				bool is_owner = (Company::IsValidID(_local_company) && f.owner == _local_company);

				const Station *st = Station::GetIfValid(f.linked_station);
				if (st == nullptr) {
					if (const Industry *ind = GetFacilityIndustry(f)) {
						if (ind->neutral_station != nullptr) {
							st = ind->neutral_station;
						} else if (!ind->stations_near.empty()) {
							st = *ind->stations_near.begin();
						}
					}
				}

				/* Legacy platform holding target: 0 -> 50 -> 100 -> 200 -> 0 */
				if (!IntegratedEconomy::Enabled() && layout.platform.Contains(pt)) {
					if (is_owner && f.linked_station != StationID::Invalid()) {
						uint32_t next_target = (f.platform_capacity == 0) ? 50 : (f.platform_capacity == 50) ? 100 : (f.platform_capacity == 100) ? 200 : 0;
						Command<Commands::SetFacilityPlatformCapacity>::Post(STR_ERROR_CAN_T_SET_PLATFORM_CAP, f.linked_station, next_target);
						this->SetDirty();
					}
					return;
				}

				if (layout.retire.Contains(pt)) {
					if (is_owner) this->ConfirmRetire(f);
					return;
				}

				if (layout.works_span.Contains(pt)) {
					if (!is_owner) return;
					if (IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2) {
						IndustryID ind_id{uint16_t(f.id - 1)};
						auto f_it = IntegratedEconomy::Factories().find(ind_id);
						FactoryWorkType work = (f_it != IntegratedEconomy::Factories().end()) ? f_it->second.work : FactoryWorkType::None;
						if (work == FactoryWorkType::None && layout.works_first.Contains(pt)) {
							Command<Commands::CommissionEconomyWork>::Post(ind_id, FactoryWorkType::Capacity);
						} else if (work == FactoryWorkType::None && layout.works_second.Contains(pt)) {
							Command<Commands::CommissionEconomyWork>::Post(ind_id, FactoryWorkType::Reserve);
						}
					} else if (IntegratedEconomy::Enabled()) {
						Command<Commands::ManageEconomyFactory>::Post(IndustryID{uint16_t(f.id - 1)}, false);
					} else if (f.linked_station != StationID::Invalid()) {
						Command<Commands::UpgradeProcessingFacility>::Post(STR_ERROR_CAN_T_UPGRADE_FACILITY, f.linked_station, 50);
					}
					this->SetDirty();
					return;
				}

				if (layout.plant.Contains(pt)) {
					if (f.tile != INVALID_TILE) SafeScrollMainWindowToTile(f.tile);
					return;
				}

				if (layout.station.Contains(pt)) {
					if (st != nullptr && st->xy != INVALID_TILE) {
						SafeScrollMainWindowToTile(st->xy);
						return;
					}
					TileIndex hub_tile = INVALID_TILE;
					for (const auto &hub : LogisticsHubManager::GetAllHubs()) {
						if (hub.world_id == f.world_id && hub.company_id == _local_company) {
							hub_tile = hub.tile;
							break;
						}
					}
					if (hub_tile != INVALID_TILE) {
						SafeScrollMainWindowToTile(hub_tile);
					} else if (f.tile != INVALID_TILE) {
						SafeScrollMainWindowToTile(f.tile);
					}
					return;
				}

				/* Click elsewhere on row: scroll viewport to plant */
				if (f.tile != INVALID_TILE) {
					SafeScrollMainWindowToTile(f.tile);
				}
				break;
			}

			default:
				break;
		}
	}
};

void ShowEmpireFacilitiesWindow(CompanyID company)
{
	if (!Company::IsValidID(company)) {
		company = _local_company;
	}
	AllocateWindowDescFront<EmpireFacilitiesWindow>(_empire_facilities_desc, company.base());
}
