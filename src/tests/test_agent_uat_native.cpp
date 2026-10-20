/* This file is part of OpenSpaceTTD. Distributed under GNU GPL version 2. */
/** @file test_agent_uat_native.cpp Scripted SDL offscreen walkthrough of player-facing windows, captured as real screenshots for review. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../console_func.h"
#include "../console_internal.h"
#include "../gfx_func.h"
#include "../openttd.h"
#include "../portal/empire_facilities_gui.h"
#include "../portal/megacity_gui.h"
#include "../portal/megacity_manager.h"
#include "../portal/production_chain.h"
#include "../screenshot.h"
#include "../video/video_driver.hpp"
#include "../widgets/empire_facilities_widget.h"
#include "../widgets/misc_widget.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_type.h"
#ifdef WITH_CAB_VIEW
#include "../cab_view.h"
#include "../hotkeys.h"
#include "../train.h"
#include "../widgets/cab_view_widget.h"
#endif
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

enum class UatStage {
	WaitGame, WaitPause, Facilities, RetireAsked, RetireDeclined, Megacity, ScaleUp, FacilitiesLarge, MegacityLarge, ScaleDown,
#ifdef WITH_CAB_VIEW
	CabAhead, CabLookLeft, CabDrag, CabZoom, BridgeSearch, BridgeFound, BridgeHeld, BridgeLeaving, BridgeExited,
#endif
	Finish,
};

/** One scripted pass over the windows a player would use, recording real screenshots. */
struct AgentUat {
	fs::path output;
	Clock::time_point start = Clock::now();
	UatStage stage = UatStage::WaitGame;
	std::string awaiting;
	std::string failure;
	std::vector<std::string> records;
	unsigned callbacks = 0;
	bool started = false;
	bool finished = false;
	TownID megacity = TownID::Invalid();
	size_t facilities_before_retire = 0;
#ifdef WITH_CAB_VIEW
	VehicleID cab_train = VehicleID::Invalid();
	VehicleID bridge_train = VehicleID::Invalid();
	unsigned bridge_callbacks = 0;
	std::string bridge_result = "not attempted";
#endif

	void Record(std::string_view event, std::string_view detail = {})
	{
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - this->start).count();
		this->records.push_back(detail.empty() ? fmt::format("{}\t{}", event, ms) : fmt::format("{}\t{}\t{}", event, ms, detail));
	}

	void WriteManifest() const
	{
		std::ofstream file(this->output / "manifest.tsv", std::ios::trunc);
		file << "event\telapsed_ms\tdetail\n";
		for (const auto &record : this->records) file << record << '\n';
		file << "result\t" << (this->finished && this->failure.empty() ? "PASS" : "FAIL") << '\n';
		if (!this->failure.empty()) file << "error\t" << this->failure << '\n';
		file << "callbacks\t" << this->callbacks << '\n';
#ifdef WITH_CAB_VIEW
		file << "bridge\t" << this->bridge_result << '\n';
#else
		file << "bridge\tcab view not built\n";
#endif
	}

	void Fail(std::string_view why)
	{
		this->failure = why;
		this->Record("failed", why);
		this->WriteManifest();
		_exit_game = true;
	}

	void Schedule()
	{
		VideoDriver::GetInstance()->QueueOnMainThread([this] { this->Tick(); });
	}

	bool Capture(std::string_view label)
	{
		this->awaiting = fmt::format("agent_uat_{}", label);
		this->Record("capture", label);
		if (!MakeScreenshot(SC_VIEWPORT, this->awaiting)) {
			this->Fail("screenshot could not be queued");
			return false;
		}
		this->Schedule();
		return true;
	}

	bool CollectScreenshot()
	{
		if (this->awaiting.empty()) return true;
		fs::path source(_full_screenshot_path);
		if (!fs::is_regular_file(source) || !source.filename().string().starts_with(this->awaiting + ".")) {
			this->Fail(fmt::format("missing real screenshot {}", this->awaiting));
			return false;
		}
		fs::path target = this->output / fmt::format("{}{}", this->awaiting, source.extension().string());
		std::error_code error;
		fs::copy_file(source, target, fs::copy_options::overwrite_existing, error);
		if (error || fs::file_size(target) == 0) {
			this->Fail(fmt::format("cannot copy real screenshot {}", this->awaiting));
			return false;
		}
		this->awaiting.clear();
		return true;
	}

	/** Change the interface scale the way the settings window does. */
	void SetScale(int scale)
	{
		_gui_scale_cfg = scale;
		if (AdjustGUIZoom(false)) ReInitAllWindows(true);
		this->Record("gui_scale", fmt::format("{}", _gui_scale));
	}

	Window *Facilities() const { return FindWindowById(WindowClass::EmpireFacilities, _local_company.base()); }

	size_t OwnedFacilities() const
	{
		size_t count = 0;
		for (const auto &f : ProductionChainManager::GetAllFacilities()) count += f.owner == _local_company;
		return count;
	}

#ifdef WITH_CAB_VIEW
	Window *Cab() const { return FindWindowById(WindowClass::VehicleCabView, this->cab_train); }

	void CabKey(uint16_t keycode)
	{
		Window *cab = this->Cab();
		SetFocusedWindow(cab);
		cab->OnHotkey(cab->window_desc.hotkeys->CheckMatch(keycode));
	}

	/** Run the game briefly while unpaused; the offscreen driver may outrun its own timer. */
	void Advance()
	{
		for (unsigned i = 0; i < 8; ++i) StateGameLoop();
	}

	VehicleID TrainOnBridge() const
	{
		for (const Train *v : Train::Iterate()) {
			if (v->IsPrimaryVehicle() && ResolveCabViewPresentation(v, _local_company).phase == CabViewPhase::Bridge) return v->index;
		}
		return VehicleID::Invalid();
	}
#endif

	void Tick()
	{
		if (this->finished || !this->failure.empty()) return;
		if (++this->callbacks > 2500 || Clock::now() - this->start > std::chrono::seconds(200)) {
			this->Fail("native draw callback/time budget exhausted");
			return;
		}
		if (!this->CollectScreenshot()) return;
		switch (this->stage) {
			case UatStage::WaitGame:
				if (_game_mode != GameMode::Normal || _switch_mode != SwitchMode::None) { this->Schedule(); return; }
				IConsoleCmdExec("pause");
				this->stage = UatStage::WaitPause;
				this->Schedule();
				return;

			case UatStage::WaitPause: {
				if (!_pause_mode.Test(PauseMode::Normal)) { this->Schedule(); return; }
				const auto cities = MegacityManager::GetAllMegacities();
				if (!cities.empty()) this->megacity = cities.front().town_id;
				this->facilities_before_retire = this->OwnedFacilities();
				this->Record("owned_facilities", fmt::format("{}", this->facilities_before_retire));
				ShowEmpireFacilitiesWindow(_local_company);
				if (this->Facilities() == nullptr) { this->Fail("Facilities window did not open"); return; }
				this->stage = UatStage::Facilities;
				this->Capture("facilities_1x");
				return;
			}

			case UatStage::Facilities: {
				if (this->facilities_before_retire == 0) {
					this->Record("retire", "skipped: no facility owned by the local company");
					this->stage = UatStage::RetireDeclined;
					this->Schedule();
					return;
				}
				Window *w = this->Facilities();
				const Rect r = w->GetWidget<NWidgetBase>(WID_EF_LIST_PANEL)->GetCurrentRect().Shrink(WidgetDimensions::scaled.framerect);
				const FacilityRowLayout layout(r.left, r.right, r.top, GetCharacterHeight(FontSize::Normal));
				w->OnClick({(layout.retire.left + layout.retire.right) / 2, (layout.retire.top + layout.retire.bottom) / 2}, WID_EF_LIST_PANEL, 1);
				if (FindWindowByClass(WindowClass::ConfirmPopupQuery) == nullptr) { this->Fail("Retire did not ask for confirmation"); return; }
				if (this->OwnedFacilities() != this->facilities_before_retire) { this->Fail("Retire acted before confirmation"); return; }
				this->stage = UatStage::RetireAsked;
				this->Capture("retire_confirmation");
				return;
			}

			case UatStage::RetireAsked:
				FindWindowByClass(WindowClass::ConfirmPopupQuery)->OnClick({}, WID_Q_NO, 1);
				this->stage = UatStage::RetireDeclined;
				this->Capture("retire_declined");
				return;

			case UatStage::RetireDeclined:
				if (this->OwnedFacilities() != this->facilities_before_retire) { this->Fail("answering No still retired the facility"); return; }
				if (this->facilities_before_retire != 0) this->Record("retire_no_kept_facility");
				CloseWindowByClass(WindowClass::EmpireFacilities);
				if (this->megacity == TownID::Invalid()) { this->Record("megacity", "none in save"); this->stage = UatStage::Megacity; this->Schedule(); return; }
				ShowMegacityOverview(this->megacity);
				this->stage = UatStage::Megacity;
				this->Capture("megacity_1x");
				return;

			case UatStage::Megacity:
				CloseWindowByClass(WindowClass::MegacityOverview);
				this->SetScale(200);
				ShowEmpireFacilitiesWindow(_local_company);
				this->stage = UatStage::ScaleUp;
				this->Schedule();
				return;

			case UatStage::ScaleUp:
				this->stage = UatStage::FacilitiesLarge;
				this->Capture("facilities_2x");
				return;

			case UatStage::FacilitiesLarge:
				CloseWindowByClass(WindowClass::EmpireFacilities);
				if (this->megacity != TownID::Invalid()) {
					ShowMegacityOverview(this->megacity);
					this->stage = UatStage::MegacityLarge;
					this->Capture("megacity_2x");
					return;
				}
				this->stage = UatStage::MegacityLarge;
				this->Schedule();
				return;

			case UatStage::MegacityLarge:
				CloseWindowByClass(WindowClass::MegacityOverview);
				this->SetScale(100);
				this->stage = UatStage::ScaleDown;
				this->Schedule();
				return;

			case UatStage::ScaleDown:
#ifdef WITH_CAB_VIEW
			{
				const Train *best = nullptr;
				for (const Train *v : Train::Iterate()) {
					if (!CanShowCabView(v)) continue;
					if (best == nullptr || v->cur_speed > best->cur_speed) best = v;
				}
				if (best == nullptr) { this->Fail("no owned train can show a cab"); return; }
				this->cab_train = best->index;
				ShowCabViewWindow(best);
				if (this->Cab() == nullptr) { this->Fail("cab did not open"); return; }
				this->stage = UatStage::CabAhead;
				this->Capture("cab_ahead");
				return;
			}

			case UatStage::CabAhead:
				this->CabKey('A');
				this->CabKey('A');
				this->stage = UatStage::CabLookLeft;
				this->Capture("cab_keys_look_left");
				return;

			case UatStage::CabLookLeft:
				this->CabKey('R');
				this->Cab()->OnScroll({-180, 40});
				this->stage = UatStage::CabDrag;
				this->Capture("cab_drag_right_and_up");
				return;

			case UatStage::CabDrag:
				this->CabKey('R');
				this->Cab()->OnMouseWheel(-3, WID_CV_VIEW);
				this->stage = UatStage::CabZoom;
				this->Capture("cab_wheel_zoom_in");
				return;

			case UatStage::CabZoom:
				this->Cab()->OnMouseWheel(3, WID_CV_VIEW);
				IConsoleCmdExec("unpause");
				this->stage = UatStage::BridgeSearch;
				this->Schedule();
				return;

			case UatStage::BridgeSearch: {
				if (_pause_mode.Any()) { this->Schedule(); return; }
				this->bridge_train = this->TrainOnBridge();
				if (this->bridge_train != VehicleID::Invalid()) {
					IConsoleCmdExec("pause");
					this->stage = UatStage::BridgeFound;
					this->Schedule();
					return;
				}
				if (++this->bridge_callbacks > 1500) {
					this->bridge_result = "no owned train reached a bridge within 12000 ticks";
					IConsoleCmdExec("pause");
					this->stage = UatStage::Finish;
					this->Schedule();
					return;
				}
				this->Advance();
				this->Schedule();
				return;
			}

			case UatStage::BridgeFound:
				if (!_pause_mode.Test(PauseMode::Normal)) { this->Schedule(); return; }
				this->cab_train = this->bridge_train;
				ShowCabViewWindow(Train::Get(this->bridge_train));
				if (this->Cab() == nullptr) { this->Fail("cab could not open for the train on a bridge"); return; }
				this->Record("bridge_train", fmt::format("{}", this->bridge_train.base()));
				this->stage = UatStage::BridgeHeld;
				this->Capture("cab_on_bridge");
				return;

			case UatStage::BridgeHeld:
				IConsoleCmdExec("unpause");
				this->bridge_callbacks = 0;
				this->stage = UatStage::BridgeLeaving;
				this->Schedule();
				return;

			case UatStage::BridgeLeaving: {
				if (_pause_mode.Any()) { this->Schedule(); return; }
				const Train *v = Train::GetIfValid(this->bridge_train);
				if (v == nullptr) { this->Fail("bridge train was deleted"); return; }
				const CabViewPhase phase = ResolveCabViewPresentation(v, _local_company).phase;
				if (phase == CabViewPhase::Bridge) {
					if (this->Cab() == nullptr) { this->Fail("cab closed while the train was on a bridge"); return; }
					if (++this->bridge_callbacks > 600) { this->Fail("train did not leave the bridge"); return; }
					this->Advance();
					this->Schedule();
					return;
				}
				IConsoleCmdExec("pause");
				this->bridge_result = phase == CabViewPhase::Scene ? "held on the bridge, resumed after it" :
					"held on the bridge; left it into a phase that closes the cab";
				this->stage = UatStage::BridgeExited;
				this->Schedule();
				return;
			}

			case UatStage::BridgeExited:
				if (!_pause_mode.Test(PauseMode::Normal)) { this->Schedule(); return; }
				this->stage = UatStage::Finish;
				if (this->Cab() != nullptr) {
					this->Capture("cab_after_bridge");
					return;
				}
				this->Schedule();
				return;
#else
				this->stage = UatStage::Finish;
				this->Schedule();
				return;
#endif

			case UatStage::Finish:
				CloseAllNonVitalWindows();
				this->Record("walkthrough_complete");
				this->finished = true;
				this->WriteManifest();
				_exit_game = true;
				return;
		}
	}
};

AgentUat *active_agent_uat = nullptr;

bool StartAgentUat(std::span<std::string_view> args)
{
	if (args.size() != 1 || active_agent_uat == nullptr || active_agent_uat->started) return false;
	active_agent_uat->started = true;
	active_agent_uat->Schedule();
	return true;
}

} // namespace

TEST_CASE("Agent UAT walkthrough captures player-facing windows", "[.][agent-uat][native]")
{
	const char *config = std::getenv("OSTTD_AGENT_UAT_CONFIG");
	const char *save = std::getenv("OSTTD_AGENT_UAT_SAVE");
	const char *output = std::getenv("OSTTD_AGENT_UAT_OUTPUT");
	const char *binary = std::getenv("OSTTD_AGENT_UAT_BINARY");
	REQUIRE(config != nullptr);
	REQUIRE(save != nullptr);
	REQUIRE(output != nullptr);
	REQUIRE(binary != nullptr);
	REQUIRE(fs::is_regular_file(config));
	REQUIRE(fs::is_regular_file(save));
	fs::create_directories(output);
	AgentUat walkthrough;
	walkthrough.output = fs::canonical(output);
	active_agent_uat = &walkthrough;
	IConsole::CmdRegister("agent_uat_start", StartAgentUat);
	std::array<std::string, 14> arguments = {binary, "-v", "sdl", "-b", "32bpp-anim", "-s", "null", "-m", "null", "-r", "1920x1200", "-x", "-c", config};
	std::vector<std::string_view> views;
	for (const std::string &argument : arguments) views.push_back(argument);
	views.push_back("-g");
	views.push_back(save);
	std::mutex watchdog_mutex;
	std::condition_variable watchdog_condition;
	bool stopped = false;
	std::atomic<bool> timed_out{false};
	std::thread watchdog([&] {
		std::unique_lock lock(watchdog_mutex);
		if (!watchdog_condition.wait_for(lock, std::chrono::seconds(240), [&] { return stopped; })) {
			timed_out = true;
			_exit_game = true;
		}
	});
	const int exit_code = openttd_main(views);
	{
		std::lock_guard lock(watchdog_mutex);
		stopped = true;
	}
	watchdog_condition.notify_one();
	watchdog.join();
	active_agent_uat = nullptr;
	if (!walkthrough.finished || !walkthrough.failure.empty()) walkthrough.WriteManifest();
	INFO("Walkthrough manifest: " << (walkthrough.output / "manifest.tsv"));
	INFO("Walkthrough failure: " << walkthrough.failure);
	CHECK_FALSE(timed_out);
	CHECK(exit_code == 0);
	CHECK(walkthrough.started);
	CHECK(walkthrough.finished);
	CHECK(walkthrough.failure.empty());
}
