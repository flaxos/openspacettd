/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_auth_gui.cpp Implementation of Federation Player Authentication and Corporate Charters window. */

#include "../stdafx.h"
#include "federation_auth_gui.h"
#include "federation_player.h"
#include "planet_manager.h"
#include "../window_gui.h"
#include "../strings_func.h"
#include "../zoom_func.h"
#include "../palette_func.h"
#include "../gfx_func.h"
#include "../widgets/federation_auth_widget.h"
#include "../table/strings.h"
#include "../core/format.hpp"
#include "../core/string_consumer.hpp"
#include "../textbuf_gui.h"
#include "../string_func.h"

#include <algorithm>

#include "../safeguards.h"

enum class AuthQueryMode : uint8_t {
	None = 0,
	Login = 1,
	NewCharter = 2,
	AuthDelegate = 3,
};

static constexpr std::initializer_list<NWidgetPart> _nested_federation_auth_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, Colours::DarkBlue),
		NWidget(WWT_CAPTION, Colours::DarkBlue, WID_FA_CAPTION), SetStringTip(STR_FEDERATION_AUTH_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, Colours::DarkBlue),
		NWidget(WWT_DEFSIZEBOX, Colours::DarkBlue),
		NWidget(WWT_STICKYBOX, Colours::DarkBlue),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkBlue, WID_FA_SESSION_PANEL), SetMinimalSize(620, 36), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, WID_FA_LOGIN_REGISTER), SetMinimalSize(130, 24), SetStringTip(STR_FEDERATION_AUTH_BTN_LOGIN_REGISTER, STR_FEDERATION_AUTH_BTN_LOGIN_REGISTER_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, WID_FA_BTN_NEW_CHARTER), SetMinimalSize(150, 24), SetStringTip(STR_FEDERATION_AUTH_BTN_NEW_CHARTER, STR_FEDERATION_AUTH_BTN_NEW_CHARTER_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, WID_FA_BTN_ADD_PRESENCE), SetMinimalSize(170, 24), SetStringTip(STR_FEDERATION_AUTH_BTN_ADD_PRESENCE, STR_FEDERATION_AUTH_BTN_ADD_PRESENCE_TOOLTIP),
		NWidget(WWT_PUSHTXTBTN, Colours::DarkBlue, WID_FA_BTN_AUTH_DELEGATE), SetMinimalSize(140, 24), SetStringTip(STR_FEDERATION_AUTH_BTN_AUTH_DELEGATE, STR_FEDERATION_AUTH_BTN_AUTH_DELEGATE_TOOLTIP),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, Colours::DarkBlue, WID_FA_CHARTER_LIST), SetMinimalSize(608, 170), SetFill(1, 1), SetResize(1, 1), EndContainer(),
		NWidget(NWID_VSCROLLBAR, Colours::DarkBlue, WID_FA_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkBlue, WID_FA_DETAILS_PANEL), SetMinimalSize(620, 64), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(WWT_PANEL, Colours::DarkBlue, WID_FA_STATUS_PANEL), SetMinimalSize(620, 26), SetFill(1, 0), SetResize(1, 0), EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, Colours::DarkBlue),
	EndContainer(),
};

static WindowDesc _federation_auth_desc(
	WindowPosition::Automatic, "view_federation_auth", 620, 360,
	WindowClass::FederationAuth, WindowClass::None,
	{},
	_nested_federation_auth_widgets
);

struct FederationAuthWindow : Window {
	Scrollbar *vscroll = nullptr;
	GlobalCompanyID selected_company_id{};
	AuthQueryMode query_mode = AuthQueryMode::None;
	std::string status_feedback = "Ready. Authenticate or select a corporate charter.";

	FederationAuthWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_FA_SCROLLBAR);
		this->vscroll->SetStepSize(1);
		this->FinishInitNested(window_number);

		auto charters = FederationPlayerRegistry::GetAllCharters();
		if (!charters.empty()) {
			this->selected_company_id = charters.front().company_id;
		}
	}

	void OnPaint() override
	{
		auto charters = FederationPlayerRegistry::GetAllCharters();
		this->vscroll->SetCount(charters.size());

		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_FA_SESSION_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto session = FederationPlayerRegistry::GetActiveSession();

				if (session.has_value()) {
					std::string auth_line = fmt::format("Active Session: {}  |  Global Player ID: {:x}:{}  |  Auth Status: VALID TOKEN",
						session->username, session->player_id.name_space.low, session->player_id.sequence);
					DrawString(tr, auth_line, TextColour::Green);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "Authenticated across Federation Universe Authority cluster.", TextColour::Silver);
				} else {
					DrawString(tr, "Active Session: Unauthenticated / Guest  |  Central Authority: Online", TextColour::Yellow);
					tr.top += GetCharacterHeight(FontSize::Normal);
					DrawString(tr, "Click 'Log In / Register' to establish persistent identity and corporate ownership.", TextColour::Silver);
				}
				break;
			}

			case WID_FA_CHARTER_LIST: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				auto charters = FederationPlayerRegistry::GetAllCharters();

				if (charters.empty()) {
					DrawString(tr, STR_FEDERATION_AUTH_NO_CHARTERS, TextColour::Silver, AlignmentH::Centre);
					return;
				}

				std::string header = fmt::format("{:<24} {:<16} {:<16} {:<14} {:<12}",
					"Corporate Entity", "Company ID", "Owner Player", "Treasury (Cr)", "Presences");
				DrawString(tr, header, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(4);

				int pos = -this->vscroll->GetPosition();
				const int cap = this->vscroll->GetCapacity();
				const int item_height = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(6);

				for (const auto &c : charters) {
					if (IsInsideMM(pos, 0, cap)) {
						Rect item_rect = tr.WithHeight(item_height);
						bool is_selected = (c.company_id == this->selected_company_id);

						PixelColour bg = is_selected ?
							GetColourGradient(Colours::Blue, Shade::Darker) :
							GetColourGradient(Colours::Blue, Shade::Darkest);
						GfxFillRect(item_rect.Shrink(1), bg);

						std::string row_str = fmt::format("{:<24} {:x}:{:<12} {:x}:{:<12} {:>10} Cr  {:>4} worlds",
							c.company_name, c.company_id.name_space.low, c.company_id.sequence,
							c.owner_player_id.name_space.low, c.owner_player_id.sequence,
							c.global_treasury_credits, c.active_world_presences.size());

						DrawString(item_rect.Shrink(ScaleGUITrad(2)), row_str,
							is_selected ? TextColour::White : TextColour::Silver);
					}
					pos++;
					tr.top += item_height;
				}
				break;
			}

			case WID_FA_DETAILS_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				const auto *charter = FederationPlayerRegistry::GetCompanyCharter(this->selected_company_id);

				if (charter == nullptr) {
					DrawString(tr, "Select a corporation above to inspect ownership and multi-world presence.", TextColour::Silver);
					return;
				}

				std::string l1 = fmt::format("Charter: '{}' | Owner: {:x}:{} | Global Treasury: Cr {:L}",
					charter->company_name, charter->owner_player_id.name_space.low, charter->owner_player_id.sequence,
					charter->global_treasury_credits);
				DrawString(tr, l1, TextColour::Gold);
				tr.top += GetCharacterHeight(FontSize::Normal);

				std::string presences_str = "Active World Presences: ";
				if (charter->active_world_presences.empty()) {
					presences_str += "None (Chartered in Orbit)";
				} else {
					for (size_t i = 0; i < charter->active_world_presences.size(); ++i) {
						if (i > 0) presences_str += ", ";
						presences_str += fmt::format("World {}", charter->active_world_presences[i].base());
					}
				}
				DrawString(tr, presences_str, TextColour::White);
				tr.top += GetCharacterHeight(FontSize::Normal);

				std::string delegates_str = fmt::format("Authorized Operator Delegates ({}): ", charter->authorized_delegates.size());
				if (charter->authorized_delegates.empty()) {
					delegates_str += "None (Exclusive Owner Control)";
				} else {
					for (size_t i = 0; i < charter->authorized_delegates.size(); ++i) {
						if (i > 0) delegates_str += ", ";
						delegates_str += fmt::format("{:x}:{}", charter->authorized_delegates[i].name_space.low, charter->authorized_delegates[i].sequence);
					}
				}
				DrawString(tr, delegates_str, TextColour::Silver);
				break;
			}

			case WID_FA_STATUS_PANEL: {
				Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);
				bool is_error = this->status_feedback.starts_with("Error") || this->status_feedback.starts_with("Action failed");
				DrawString(tr, this->status_feedback, is_error ? TextColour::Red : TextColour::Green);
				break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_FA_LOGIN_REGISTER: {
				this->query_mode = AuthQueryMode::Login;
				ShowQueryString("", STR_FEDERATION_AUTH_LOGIN_CAPTION, 32, this, CS_ALPHANUMERAL, QueryStringFlag::EnableDefault);
				break;
			}

			case WID_FA_BTN_NEW_CHARTER: {
				auto session = FederationPlayerRegistry::GetActiveSession();
				if (!session.has_value()) {
					this->status_feedback = "Error: You must be authenticated to charter a corporation.";
					this->SetDirty();
					return;
				}
				this->query_mode = AuthQueryMode::NewCharter;
				ShowQueryString("", STR_FEDERATION_AUTH_CHARTER_CAPTION, 48, this, CS_ALPHANUMERAL, QueryStringFlag::EnableDefault);
				break;
			}

			case WID_FA_BTN_AUTH_DELEGATE: {
				auto session = FederationPlayerRegistry::GetActiveSession();
				if (!session.has_value()) {
					this->status_feedback = "Error: You must be authenticated to manage delegates.";
					this->SetDirty();
					return;
				}
				const auto *charter = FederationPlayerRegistry::GetCompanyCharter(this->selected_company_id);
				if (charter == nullptr) {
					this->status_feedback = "Error: No corporate charter selected.";
					this->SetDirty();
					return;
				}
				if (charter->owner_player_id != session->player_id) {
					this->status_feedback = fmt::format("Action failed: Only charter owner ({:x}:{}) can authorize delegates.",
						charter->owner_player_id.name_space.low, charter->owner_player_id.sequence);
					this->SetDirty();
					return;
				}
				this->query_mode = AuthQueryMode::AuthDelegate;
				ShowQueryString("", STR_FEDERATION_AUTH_DELEGATE_CAPTION, 12, this, CS_NUMERAL, QueryStringFlag::EnableDefault);
				break;
			}

			case WID_FA_BTN_ADD_PRESENCE: {
				auto session = FederationPlayerRegistry::GetActiveSession();
				if (!session.has_value()) {
					this->status_feedback = "Error: You must be authenticated to register world presences.";
					this->SetDirty();
					return;
				}
				const auto *charter = FederationPlayerRegistry::GetCompanyCharter(this->selected_company_id);
				if (charter == nullptr) {
					this->status_feedback = "Error: No corporate charter selected.";
					this->SetDirty();
					return;
				}
				if (!FederationPlayerRegistry::IsAuthorized(this->selected_company_id, session->player_id)) {
					this->status_feedback = "Action failed: Authenticated player is not authorized for this corporation.";
					this->SetDirty();
					return;
				}

				WorldID cur_world = (PlanetManager::Count() > 0) ? PlanetManager::GetAllRegions().front().id : WorldID{1};
				auto it = std::find(charter->active_world_presences.begin(), charter->active_world_presences.end(), cur_world);
				if (it != charter->active_world_presences.end()) {
					this->status_feedback = fmt::format("Corporation '{}' already possesses active presence on World {}.",
						charter->company_name, cur_world.base());
				} else {
					FederationPlayerRegistry::RegisterWorldPresence(this->selected_company_id, cur_world);
					this->status_feedback = fmt::format("Active presence on World {} registered for corporation '{}'.",
						cur_world.base(), charter->company_name);
				}
				this->SetDirty();
				break;
			}

			case WID_FA_CHARTER_LIST: {
				auto charters = FederationPlayerRegistry::GetAllCharters();
				if (charters.empty()) return;

				const int item_height = GetCharacterHeight(FontSize::Normal) + ScaleGUITrad(6);
				int row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_FA_CHARTER_LIST, WidgetDimensions::scaled.framerect.top, item_height);
				if (row >= 0 && static_cast<size_t>(row) < charters.size()) {
					this->selected_company_id = charters[row].company_id;
					this->SetDirty();
				}
				break;
			}
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		AuthQueryMode mode = this->query_mode;
		this->query_mode = AuthQueryMode::None;

		if (!str.has_value() || str->empty()) {
			this->status_feedback = "Input cancelled.";
			this->SetDirty();
			return;
		}

		switch (mode) {
			case AuthQueryMode::Login: {
				std::string uname = *str;
				/* Attempt authenticate with empty token first */
				auto existing = FederationPlayerRegistry::Authenticate(uname, "");
				if (existing.has_value()) {
					FederationPlayerRegistry::SetActiveSession(*existing);
					this->status_feedback = fmt::format("Authenticated as '{}' (Player ID {:x}:{}).",
						existing->username, existing->player_id.name_space.low, existing->player_id.sequence);
				} else {
					auto pid = FederationPlayerRegistry::RegisterPlayer(uname);
					if (pid.IsValid()) {
						const auto *acc = FederationPlayerRegistry::GetPlayer(pid);
						if (acc != nullptr) {
							FederationPlayerRegistry::SetActiveSession(*acc);
							this->status_feedback = fmt::format("Registered and authenticated as '{}' (Player ID {:x}:{}).",
								acc->username, pid.name_space.low, pid.sequence);
						}
					} else {
						this->status_feedback = fmt::format("Error: Failed to register account for '{}'.", uname);
					}
				}
				this->SetDirty();
				break;
			}

			case AuthQueryMode::NewCharter: {
				auto session = FederationPlayerRegistry::GetActiveSession();
				if (!session.has_value()) {
					this->status_feedback = "Error: Authenticated session expired.";
					this->SetDirty();
					return;
				}
				auto cid = FederationPlayerRegistry::CharterCompany(session->player_id, *str);
				if (cid.IsValid()) {
					this->selected_company_id = cid;
					this->status_feedback = fmt::format("Corporate charter granted: '{}' (ID {:x}:{}).",
						*str, cid.name_space.low, cid.sequence);
				} else {
					this->status_feedback = fmt::format("Error: Could not charter company '{}'.", *str);
				}
				this->SetDirty();
				break;
			}

			case AuthQueryMode::AuthDelegate: {
				auto session = FederationPlayerRegistry::GetActiveSession();
				if (!session.has_value()) {
					this->status_feedback = "Error: Authenticated session expired.";
					this->SetDirty();
					return;
				}
				auto parsed = ParseInteger(*str);
				if (!parsed.has_value() || *parsed <= 0) {
					this->status_feedback = "Error: Invalid player sequence ID number.";
					this->SetDirty();
					return;
				}
				GlobalPlayerID delegate_id{FederationNamespace{0, 1}, static_cast<uint64_t>(*parsed)};
				const auto *charter = FederationPlayerRegistry::GetCompanyCharter(this->selected_company_id);
				if (charter == nullptr) {
					this->status_feedback = "Error: No corporate charter selected.";
				} else if (FederationPlayerRegistry::AuthorizeDelegate(this->selected_company_id, delegate_id, session->player_id)) {
					this->status_feedback = fmt::format("Authorized delegate player {:x}:{} for '{}'.",
						delegate_id.name_space.low, delegate_id.sequence, charter->company_name);
				} else {
					this->status_feedback = "Action failed: Requester is not charter owner or delegate is already authorized.";
				}
				this->SetDirty();
				break;
			}

			case AuthQueryMode::None:
				break;
		}
	}
};

void ShowFederationAuth()
{
	AllocateWindowDescFront<FederationAuthWindow>(_federation_auth_desc, 0);
}
