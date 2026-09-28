/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file stellar_network_gui.cpp Interactive stellar routes and delivered-equipment gate projects. */
#include "../stdafx.h"
#include "stellar_network.h"
#include "integrated_economy.h"
#include "empire_facilities_gui.h"
#include "corporate_hq_gui.h"
#include "megacity_gui.h"
#include "../town.h"
#include "universe_network.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "logistics_hub.h"
#include "../window_gui.h"
#include "../widgets/stellar_widget.h"
#include "../gfx_func.h"
#include "../querystring_gui.h"
#include <charconv>
#include "../zoom_func.h"
#include "../strings_func.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../station_base.h"
#include "../map_func.h"
#include "../viewport_func.h"
#include "../timer/timer_window.h"
#include "../timer/timer.h"
#include "../table/strings.h"
#include "../safeguards.h"

static constexpr auto _stellar_widgets = std::to_array<NWidgetPart>({
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_CLOSEBOX, Colours::DarkBlue),
	NWidget(WWT_CAPTION, Colours::DarkBlue),
	SetStringTip(STR_STELLAR_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_STICKYBOX, Colours::DarkBlue),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkBlue, SW_MAP),
	SetMinimalSize(650, 250),
	SetFill(1, 1),
	SetResize(1, 1),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkBlue, SW_INFO),
	SetMinimalSize(650, 120),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_REMOTE),
	SetStringTip(STR_STELLAR_REMOTE_WORLDS),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_SOURCE),
	SetStringTip(STR_STELLAR_SOURCE),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_ZONE),
	SetStringTip(STR_STELLAR_ZONE),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_START),
	SetStringTip(STR_STELLAR_START),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_VISIT),
	SetStringTip(STR_STELLAR_VISIT),
	SetFill(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_FACTORIES),
	SetStringTip(STR_ECONOMY_FACTORIES),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_CITY),
	SetStringTip(STR_ECONOMY_CITY),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_HQ),
	SetStringTip(STR_ECONOMY_HQ),
	SetFill(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_MATRIX, Colours::DarkBlue, SW_PROJECTS),
	SetMatrixDataTip(1, 0),
	SetMinimalSize(630, 100),
	SetFill(1, 1),
	SetResize(1, 1),
	SetScrollbar(SW_SCROLL),
	NWidget(NWID_VSCROLLBAR, Colours::DarkBlue, SW_SCROLL),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_ACTIVATE),
	SetStringTip(STR_STELLAR_ACTIVATE),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_CANCEL),
	SetStringTip(STR_STELLAR_CANCEL),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_ACCESS),
	SetStringTip(STR_STELLAR_ACCESS),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, SW_TOLL),
	SetStringTip(STR_STELLAR_TOLL),
	SetFill(1, 0),
	NWidget(WWT_RESIZEBOX, Colours::DarkBlue),
	EndContainer(),
});
static WindowDesc _stellar_desc(WindowPosition::Automatic, "stellar_network", 670, 560, WindowClass::StellarNetwork, WindowClass::None, {}, _stellar_widgets);

struct StellarNetworkWindow : Window {
	CompanyID company = _local_company;
	WorldID selected = INVALID_WORLD;
	TileIndex source = INVALID_TILE;
	TileIndex toll_gate = INVALID_TILE;
	uint32_t zone = 0, project = 0;
	Scrollbar *scroll;
	std::vector<uint32_t> rows;
	StellarNetworkWindow() : Window(_stellar_desc)
	{
		CreateNestedTree();
		scroll = GetScrollbar(SW_SCROLL);
		FinishInitNested(0);
		Refresh();
	}
	Point Position(const Rect &r, const StellarWorld &w) const
	{
		int min_x = -5, max_x = 70, min_y = -10, max_y = 15;
		for (const auto &[id, p] : StellarNetwork::Worlds()) {
			min_x = std::min(min_x, p.x - 5);
			max_x = std::max(max_x, p.x + 5);
			min_y = std::min(min_y, p.y - 5);
			max_y = std::max(max_y, p.y + 5);
		}
		return {r.left + (w.x - min_x) * r.Width() / (max_x - min_x), r.top + (w.y - min_y) * r.Height() / (max_y - min_y)};
	}
	std::vector<TileIndex> Sources() const
	{
		std::vector<TileIndex> result;
		for (const auto &[tile, gate] : PortalRegistry::GetUnlinkedGates())
			if (GetTileOwner(tile) == company) result.push_back(tile);
		for (const auto &[id, gate] : PortalRegistry::GetAllPortals()) {
			if (GetTileOwner(gate.end_a.tile) == company) result.push_back(gate.end_a.tile);
			if (GetTileOwner(gate.end_b.tile) == company) result.push_back(gate.end_b.tile);
		}
		for (const auto &[tile, g] : PortalRegistry::GetAllInterServerPortals())
			if (GetTileOwner(tile) == company) result.push_back(tile);
		std::sort(result.begin(), result.end());
		return result;
	}
	std::vector<uint32_t> ArrivalZones() const
	{
		std::vector<uint32_t> result;
		for (const auto &[id, z] : StellarNetwork::Zones())
			if (z.world == selected) result.push_back(id);
		return result;
	}
	StationID SupplyStation() const
	{
		if (source == INVALID_TILE) return StationID::Invalid();
		StationID result = StationID::Invalid();
		uint distance = 33;
		for (const auto &hub : LogisticsHubManager::GetAllHubs()) {
			if (hub.company_id != company || hub.world_id != PlanetManager::GetTileWorld(source)) continue;
			const Station *st = Station::GetIfValid(hub.station_id);
			if (!st || !LogisticsHubManager::ValidateForStation(hub)) continue;
			uint d = DistanceManhattan(source, st->xy);
			if (d < distance || (d == distance && hub.station_id < result)) {
				distance = d;
				result = hub.station_id;
			}
		}
		return result;
	}
	void Refresh()
	{
		if (company != _local_company) {
			Close();
			return;
		}
		if (selected == INVALID_WORLD && !StellarNetwork::Worlds().empty()) selected = StellarNetwork::Worlds().begin()->first;
		auto sources = Sources();
		if (std::find(sources.begin(), sources.end(), source) == sources.end()) source = sources.empty() ? INVALID_TILE : sources.front();
		auto arrival = ArrivalZones();
		if (std::find(arrival.begin(), arrival.end(), zone) == arrival.end()) zone = arrival.empty() ? 0 : arrival.front();
		rows.clear();
		for (const auto &[id, p] : StellarNetwork::Projects())
			if (p.owner == company) rows.push_back(id);
		scroll->SetCount(rows.size());
		auto it = StellarNetwork::Projects().find(project);
		SetWidgetDisabledState(SW_ACTIVATE, it == StellarNetwork::Projects().end() || it->second.state != GateProjectState::Ready);
		SetWidgetDisabledState(SW_CANCEL, it == StellarNetwork::Projects().end() || it->second.state > GateProjectState::Ready);
		SetWidgetDisabledState(SW_START, source == INVALID_TILE || zone == 0 || !PortalRegistry::IsUnlinkedGate(source) ||
											 SupplyStation() == StationID::Invalid() ||
											 !StellarNetwork::CanReach(company, PlanetManager::GetTileWorld(source), selected));
		SetWidgetDisabledState(SW_VISIT, selected == INVALID_WORLD || !StellarNetwork::WorldAccessible(selected));
		SetWidgetDisabledState(SW_TOLL, source == INVALID_TILE || StellarNetwork::Policy(source) == nullptr ||
											StellarNetwork::Policy(source)->owner != company || RemoteGateProjects::IsArrivalGate(source));
		SetWidgetDisabledState(SW_ACCESS, IsWidgetDisabled(SW_TOLL));
		SetDirty();
	}
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &, Dimension &resize) override
	{
		if (widget == SW_PROJECTS) {
			resize.height = GetCharacterHeight(FontSize::Normal) + padding.height + 4;
			size.height = resize.height * 5;
		}
		if (widget == SW_INFO) size.height = GetCharacterHeight(FontSize::Normal) * 12 + padding.height;
	}
	void OnResize() override { scroll->SetCapacityFromWidget(this, SW_PROJECTS); }
	void OnPaint() override { DrawWidgets(); }
	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		Rect box = r.Shrink(WidgetDimensions::scaled.framerect);
		if (widget == SW_MAP) {
			GfxFillRect(box, GetColourGradient(Colours::DarkBlue, Shade::Darkest));
			if (!StellarNetwork::Enabled()) {
				DrawStringMultiLine(box, "Choose the Mito–Merredin world preset when creating a new game. Existing saves retain their current gate rules.",
									TextColour::White);
				return;
			}
			for (const auto &[id, link] : PortalRegistry::GetAllPortals()) {
				auto a = StellarNetwork::GetWorld(link.end_a.world_id), b = StellarNetwork::GetWorld(link.end_b.world_id);
				if (!a || !b) continue;
				Point pa = Position(box, *a), pb = Position(box, *b);
				auto policy = StellarNetwork::Policy(link.end_a.tile);
				GfxDrawLine(pa.x, pa.y, pb.x, pb.y, GetColourGradient(policy && policy->owner == company ? Colours::Green : Colours::LightBlue, Shade::Light),
							2);
			}
			WorldID from = source == INVALID_TILE ? INVALID_WORLD : PlanetManager::GetTileWorld(source);
			for (const auto &[id, w] : StellarNetwork::Worlds()) {
				Point p = Position(box, w);
				TextColour col = id == selected								   ? TextColour::White
								 : StellarNetwork::CanReach(company, from, id) ? TextColour::Green
								 : w.opened									   ? TextColour::Gold
																			   : TextColour::Silver;
				DrawString(p.x - ScaleGUITrad(55), p.x + ScaleGUITrad(55), p.y, w.name, col, AlignmentH::Centre);
				DrawString(p.x - ScaleGUITrad(50), p.x + ScaleGUITrad(50), p.y + GetCharacterHeight(FontSize::Normal), w.opened ? "Open" : "Frontier", col,
						   AlignmentH::Centre);
			}
		} else if (widget == SW_INFO) {
			std::string text = fmt::format("Portal research range: {} stellar units. Green destinations are in range. Coordinates are game geography.\n",
										   StellarNetwork::Range(company));
			if (source == INVALID_TILE)
				text += "Build an unlinked departure gate and an owned logistics hub within 32 tiles, then select Source gate.\n";
			else
				text += fmt::format("Departure: ({}, {}). Supply station: {}.\n", TileX(source), TileY(source),
									SupplyStation() == StationID::Invalid() ? "none nearby" : GetString(STR_STATION_NAME, SupplyStation()));
			if (auto w = StellarNetwork::GetWorld(selected)) {
				text += fmt::format("Destination: {}. Arrival zone {}. {}\n", w->name, zone,
									w->opened ? "Established world" : "Gate access does not found a colony or reveal deposits.");
				if (IntegratedEconomy::Enabled()) {
					auto region = PlanetManager::GetRegion(selected);
					text += fmt::format("Economic role: {}. Development level: {}.\n", IntegratedEconomy::RoleName(selected),
										region ? uint(region->phase) : 0);
					uint factories = 0, batches = 0;
					for (const auto &f : ProductionChainManager::GetAllFacilities())
						if (f.world_id == selected && f.owner == company) {
							++factories;
							batches += f.last_month_production;
						}
					text += fmt::format("Local factories: {}; last month: {} batches. ", factories, batches);
					if (auto town = PlanetManager::GetWorldPrimaryTown(selected)) text += IntegratedEconomy::CityStatus(town->index);
					text += "\n";
				}
				auto from = StellarNetwork::GetWorld(source == INVALID_TILE ? INVALID_WORLD : PlanetManager::GetTileWorld(source));
				if (from && from != w) {
					uint bands = StellarNetwork::DistanceBands(*from, *w);
					text += fmt::format("Deliver {} steel and {} machine modules to the supply station. ", 200 * bands, 40 * bands);
					AutoRestoreBackup company_scope(_current_company, company);
					auto cost = StellarNetwork::Start({}, source, zone, SupplyStation());
					text += cost.Succeeded() ? fmt::format("Reservation fee: {}. Arrival construction is charged on activation.", cost.GetCost())
											 : GetString(cost.GetErrorMessage());
				}
			}
			if (auto policy = StellarNetwork::Policy(source))
				text += fmt::format("\nDeparture access: {}; toll {} per train.", policy->public_access ? "Public" : "Company only", policy->toll);
			if (RemoteGateProjects::IsArrivalGate(source)) text += " Access settings are managed at the commissioning end.";
			DrawStringMultiLine(box, text, TextColour::White);
		} else if (widget == SW_PROJECTS) {
			int y = box.top, step = GetWidget<NWidgetCore>(SW_PROJECTS)->resize_y;
			for (int i = scroll->GetPosition(); i < int(rows.size()) && i < scroll->GetPosition() + scroll->GetCapacity(); ++i) {
				const auto &p = StellarNetwork::Projects().at(rows[i]);
				static const char *states[]{"Supplying", "Ready to activate", "Active", "Cancelled"};
				DrawString(box.left, box.right, y,
						   fmt::format("#{}: {} — steel {}/{}, machinery {}/{}", p.id, states[to_underlying(p.state)], p.steel, 200 * p.bands, p.machines,
									   40 * p.bands),
						   p.id == project ? TextColour::White : TextColour::Silver);
				y += step;
			}
		}
	}
	void OnClick(Point pt, WidgetID widget, int) override
	{
		switch (widget) {
		case SW_REMOTE:
			ShowUniverseDestinations(VehicleID::Invalid());
			break;
		case SW_MAP: {
			Rect r = GetWidget<NWidgetBase>(SW_MAP)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
			int best = INT32_MAX;
			for (const auto &[id, w] : StellarNetwork::Worlds()) {
				Point p = Position(r, w);
				int d = (pt.x - p.x) * (pt.x - p.x) + (pt.y - p.y) * (pt.y - p.y);
				if (d < best) {
					best = d;
					selected = id;
				}
			}
			break;
		}
		case SW_SOURCE: {
			auto list = Sources();
			auto it = std::find(list.begin(), list.end(), source);
			if (!list.empty()) source = it == list.end() || ++it == list.end() ? list.front() : *it;
			break;
		}
		case SW_ZONE: {
			auto list = ArrivalZones();
			auto it = std::find(list.begin(), list.end(), zone);
			if (!list.empty()) zone = it == list.end() || ++it == list.end() ? list.front() : *it;
			break;
		}
		case SW_START:
			Command<Commands::StartGateProject>::Post(STR_ERROR_STELLAR_ACTION, source, zone, SupplyStation());
			break;
		case SW_ACTIVATE:
			Command<Commands::OperateGateProject>::Post(STR_ERROR_STELLAR_ACTION, project, false);
			break;
		case SW_CANCEL:
			Command<Commands::OperateGateProject>::Post(STR_ERROR_STELLAR_ACTION, project, true);
			break;
		case SW_FACTORIES:
			ShowEmpireFacilitiesWindow(company);
			break;
		case SW_HQ:
			ShowCorporateHQ(company);
			break;
		case SW_CITY:
			if (auto town = PlanetManager::GetWorldPrimaryTown(selected)) ShowMegacityOverview(town->index);
			break;
		case SW_VISIT:
			if (StellarNetwork::WorldAccessible(selected)) PlanetManager::JumpToPlanet(selected);
			break;
		case SW_TOLL:
			if (auto p = StellarNetwork::Policy(source)) {
				toll_gate = source;
				ShowQueryString(fmt::format("{}", p->toll), STR_STELLAR_TOLL, 12, this, CS_NUMERAL, QueryStringFlag::EnableDefault);
			}
			break;
		case SW_ACCESS:
			if (auto p = StellarNetwork::Policy(source))
				Command<Commands::SetStellarGateAccess>::Post(STR_ERROR_STELLAR_ACTION, source, !p->public_access, p->toll);
			break;
		case SW_PROJECTS: {
			int row = scroll->GetScrolledRowFromWidget(pt.y, this, SW_PROJECTS);
			if (row >= 0 && row < int(rows.size())) project = rows[row];
			break;
		}
		}
		Refresh();
	}
	void OnQueryTextFinished(std::optional<std::string> text) override
	{
		if (!text) return;
		int64_t toll = 0;
		auto parsed = std::from_chars(text->data(), text->data() + text->size(), toll);
		if (parsed.ec != std::errc{} || parsed.ptr != text->data() + text->size()) return;
		if (auto p = StellarNetwork::Policy(toll_gate))
			Command<Commands::SetStellarGateAccess>::Post(STR_ERROR_STELLAR_ACTION, toll_gate, p->public_access, Money{toll});
	}
	const IntervalTimer<TimerWindow> refresh_timer = {std::chrono::seconds(1), [this](auto) { Refresh(); }};
};
void ShowStellarNetwork()
{
	if (BringWindowToFrontById(WindowClass::StellarNetwork, 0)) return;
	new StellarNetworkWindow();
}
