/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file universe_network_gui.cpp Remote world and station selection from native train orders. */
#include "../stdafx.h"
#include "universe_network.h"
#include "portal_registry.h"
#include "planet_manager.h"
#include "../core/backup_type.hpp"
#include "../map_func.h"
#include "../window_gui.h"
#include "../gfx_func.h"
#include "../strings_func.h"
#include "../command_func.h"
#include "../company_func.h"
#include "../train.h"
#include "../timer/timer_window.h"
#include "../timer/timer.h"
#include "../table/strings.h"
#include "../safeguards.h"
static std::string _universe_destination_draft;
enum UniverseDestinationWidgets : WidgetID { UD_INFO, UD_LIST, UD_SCROLL, UD_ADD, UD_VISIT, UD_SOURCE, UD_START, UD_ACTIVATE, UD_CANCEL, UD_MAP };
static constexpr auto _destination_widgets = std::to_array<NWidgetPart>({
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_CLOSEBOX, Colours::DarkBlue),
	NWidget(WWT_CAPTION, Colours::DarkBlue),
	SetStringTip(STR_STELLAR_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
	NWidget(WWT_STICKYBOX, Colours::DarkBlue),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkBlue, UD_INFO),
	SetMinimalSize(580, 72),
	SetFill(1, 0),
	SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_MATRIX, Colours::DarkBlue, UD_LIST),
	SetMatrixDataTip(1, 0),
	SetMinimalSize(560, 220),
	SetFill(1, 1),
	SetResize(1, 1),
	SetScrollbar(UD_SCROLL),
	NWidget(NWID_VSCROLLBAR, Colours::DarkBlue, UD_SCROLL),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_MAP),
	SetStringTip(STR_STELLAR_OPEN),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_SOURCE),
	SetStringTip(STR_STELLAR_SOURCE),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_START),
	SetStringTip(STR_STELLAR_START),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_ACTIVATE),
	SetStringTip(STR_STELLAR_ACTIVATE),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_CANCEL),
	SetStringTip(STR_STELLAR_CANCEL),
	SetFill(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_ADD),
	SetStringTip(STR_STELLAR_REMOTE_STOP),
	SetFill(1, 0),
	NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, UD_VISIT),
	SetStringTip(STR_STELLAR_VISIT),
	SetFill(1, 0),
	NWidget(WWT_RESIZEBOX, Colours::DarkBlue),
	EndContainer(),
});
static WindowDesc _destination_desc(WindowPosition::Automatic, "universe_destinations", 600, 350, WindowClass::UniverseDestinations, WindowClass::None, {},
									_destination_widgets);
struct UniverseDestinationsWindow : Window {
	VehicleID train;
	TileIndex source = INVALID_TILE;
	std::vector<TileIndex> Sources() const
	{
		std::vector<TileIndex> result;
		for (const auto &[tile, g] : PortalRegistry::GetUnlinkedGates())
			if (GetTileOwner(tile) == _local_company) result.push_back(tile);
		std::sort(result.begin(), result.end());
		return result;
	}
	std::optional<GlobalConsistID> identity;
	Scrollbar *scroll;
	std::vector<std::string> rows;
	std::string selected;
	UniverseDestinationsWindow(VehicleID vehicle) : Window(_destination_desc), train(vehicle)
	{
		if (auto v = Train::GetIfValid(vehicle)) identity = FederationIdentityRegistry::Find(v);
		selected = _universe_destination_draft;
		CreateNestedTree();
		scroll = GetScrollbar(UD_SCROLL);
		FinishInitNested(vehicle);
		Refresh();
	}
	void Refresh()
	{
		rows.clear();
		auto sources = Sources();
		if (std::find(sources.begin(), sources.end(), source) == sources.end()) source = sources.empty() ? INVALID_TILE : sources.front();
		for (const auto &[key, r] : UniverseNetwork::Directory()) {
			if (r.value("kind", "") == "world" || r.value("kind", "") == "station" || r.value("kind", "") == "gate" || r.value("kind", "") == "zone" ||
				r.value("kind", "") == "gateproject" || r.value("kind", "") == "train")
				rows.push_back(key);
		}
		scroll->SetCount(rows.size());
		auto it = UniverseNetwork::Directory().find(selected);
		auto v = Train::GetIfValid(train);
		bool compatible =
			it != UniverseNetwork::Directory().end() && it->second.value("online", false) && it->second.value("manifest", "") == UniverseNetwork::Manifest();
		bool valid_train = v && v->owner == _local_company && (!identity || FederationIdentityRegistry::Find(v) == identity);
		SetWidgetDisabledState(UD_ADD, !valid_train || it == UniverseNetwork::Directory().end() ||
										   (it->second.value("kind", "") != "station" && it->second.value("kind", "") != "gate") || !compatible);
		std::string kind = it == UniverseNetwork::Directory().end() ? "" : it->second.value("kind", "");
		SetWidgetDisabledState(UD_START, kind != "zone" || source == INVALID_TILE || !compatible ||
											 it->second.at("namespace") == UniverseNetwork::Namespace(FederationIdentityRegistry::GetNamespace()));
		SetWidgetDisabledState(UD_ACTIVATE, kind != "gateproject" || it->second.value("state", "") != "ready");
		SetWidgetDisabledState(UD_CANCEL, kind != "gateproject" || (it->second.value("state", "") != "ready" && it->second.value("state", "") != "supplying"));
		SetWidgetDisabledState(UD_VISIT, kind == "gateproject" || it == UniverseNetwork::Directory().end() || !compatible);
		SetDirty();
	}
	void UpdateWidgetSize(WidgetID widget, Dimension &size, const Dimension &padding, Dimension &, Dimension &resize) override
	{
		if (widget == UD_LIST) {
			resize.height = GetCharacterHeight(FontSize::Normal) + padding.height + 4;
			size.height = resize.height * 10;
		}
		if (widget == UD_INFO) size.height = GetCharacterHeight(FontSize::Normal) * 8 + padding.height;
	}
	void OnResize() override { scroll->SetCapacityFromWidget(this, UD_LIST); }
	void OnPaint() override { DrawWidgets(); }
	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		Rect box = r.Shrink(WidgetDimensions::scaled.framerect);
		if (widget == UD_INFO) {
			std::string text =
				fmt::format("Departure gate: {}. Select a registered world to visit, a station for a scheduled stop, or a gate to pin the route.\n",
							source == INVALID_TILE ? "none" : fmt::format("{}, {}", TileX(source), TileY(source)));
			auto it = UniverseNetwork::Directory().find(selected);
			if (it != UniverseNetwork::Directory().end() && it->second.value("kind", "") == "zone") {
				const auto &z = it->second;
				auto from = StellarNetwork::GetWorld(source == INVALID_TILE ? INVALID_WORLD : PlanetManager::GetTileWorld(source));
				if (from) {
					StellarWorld to{WorldID{z.at("world").get<uint32_t>()}, "", "", z.at("x").get<int32_t>(), z.at("y").get<int32_t>(), false};
					uint bands = StellarNetwork::DistanceBands(*from, to);
					text += fmt::format("Research range: {} stellar units. Deliver {} steel and {} machine modules to the departure logistics hub.\n",
										StellarNetwork::Range(_local_company), 200 * bands, 40 * bands);
					AutoRestoreBackup company(_current_company, _local_company);
					auto quote = RemoteGateProjects::Operate({}, source, selected, 0);
					text += quote.Succeeded() ? fmt::format("Reservation fee: {}. Arrival construction is charged on activation.\n", quote.GetCost())
											  : GetString(quote.GetErrorMessage()) + "\n";
				}
			}
			text += "Native orders control loading and unloading. Offline or incompatible destinations hold trains.\n" + UniverseNetwork::VisitStatus();
			DrawStringMultiLine(box, text, TextColour::White);
		} else if (widget == UD_LIST) {
			int y = box.top, step = GetWidget<NWidgetCore>(UD_LIST)->resize_y;
			for (int i = scroll->GetPosition(); i < int(rows.size()) && i < scroll->GetPosition() + scroll->GetCapacity(); ++i) {
				const auto &entry = UniverseNetwork::Directory().at(rows[i]);
				std::string name = entry.value("name", "");
				uint32_t world = entry.value("kind", "") == "world"			? entry.at("id").get<uint32_t>()
								 : entry.value("kind", "") == "gateproject" ? entry.at("target_world").get<uint32_t>()
																			: entry.at("world").get<uint32_t>();
				if (entry.value("kind", "") == "gateproject")
					name = fmt::format("{}: steel {}/{}, machines {}/{}", entry.value("state", ""), entry.at("steel").get<uint32_t>(),
									   200 * entry.at("bands").get<uint32_t>(), entry.at("machines").get<uint32_t>(), 40 * entry.at("bands").get<uint32_t>());
				if (entry.value("kind", "") == "gate")
					name = fmt::format("Gate {} — {}", entry.at("id").get<uint32_t>(), entry.value("public", false) ? "public" : "private");
				DrawString(box.left, box.right, y,
						   fmt::format("{} / World {} / {}{}", entry.value("kind", ""), world, name, entry.value("online", false) ? "" : " [offline]"),
						   rows[i] == selected ? TextColour::White : TextColour::Silver);
				y += step;
			}
		}
	}
	void OnClick(Point pt, WidgetID widget, int) override
	{
		if (widget == UD_MAP) ShowStellarNetwork();
		if (widget == UD_SOURCE) {
			auto list = Sources();
			auto it = std::find(list.begin(), list.end(), source);
			if (!list.empty()) source = it == list.end() || ++it == list.end() ? list.front() : *it;
		}
		if (widget == UD_LIST) {
			int row = scroll->GetScrolledRowFromWidget(pt.y, this, UD_LIST);
			if (row >= 0 && row < int(rows.size())) selected = _universe_destination_draft = rows[row];
		}
		auto it = UniverseNetwork::Directory().find(selected);
		if (it != UniverseNetwork::Directory().end()) {
			if (widget == UD_START) Command<Commands::RemoteGateProject>::Post(STR_ERROR_STELLAR_ACTION, source, selected, 0);
			if (widget == UD_ACTIVATE || widget == UD_CANCEL)
				Command<Commands::RemoteGateProject>::Post(STR_ERROR_STELLAR_ACTION, source, it->second.at("id").get<std::string>(),
														   widget == UD_CANCEL ? 2 : 1);
			if (widget == UD_VISIT && it->second.value("kind", "") == "train")
				UniverseNetwork::FollowTrain(selected);
			else if (widget == UD_VISIT)
				UniverseNetwork::Visit(it->second.at("namespace"),
									   WorldID{it->second.at(it->second.value("kind", "") == "world" ? "id" : "world").get<uint32_t>()});
			if (widget == UD_ADD)
				if (auto v = Train::GetIfValid(train))
					Command<Commands::AddUniverseOrder>::Post(STR_ERROR_STELLAR_ACTION, train, UniverseNetwork::OrderRevision(v), selected);
		}
		Refresh();
	}
	const IntervalTimer<TimerWindow> refresh_timer = {std::chrono::seconds(1), [this](auto) { Refresh(); }};
};
void ShowUniverseDestinations(VehicleID train)
{
	if (BringWindowToFrontById(WindowClass::UniverseDestinations, train)) return;
	new UniverseDestinationsWindow(train);
}
