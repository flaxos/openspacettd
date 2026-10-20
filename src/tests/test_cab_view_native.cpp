/* This file is part of OpenSpaceTTD. Distributed under GNU GPL version 2. */
/** @file test_cab_view_native.cpp Isolated, real SDL offscreen Stage 1 cab proof. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../cab_view.h"
#include "../cab_view_3d.h"
#include "../base_media_graphics.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../console_internal.h"
#include "../core/random_func.hpp"
#include "../map_func.h"
#include "../openttd.h"
#include "../order_cmd.h"
#include "../portal/planet_manager.h"
#include "../screenshot.h"
#include "../timer/timer_game_calendar.h"
#include "../train.h"
#include "../vehicle_base.h"
#include "../vehicle_gui.h"
#include "../video/video_driver.hpp"
#include "../viewport_func.h"
#include "../widgets/cab_view_widget.h"
#include "../widgets/vehicle_widget.h"
#include "../window_func.h"
#include "../window_gui.h"
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

enum class CabProofStage {
	WaitGame, WaitPause, Normal, Open, CabAhead, LookLeft, LookRight,
	Resize, Closed, Reopen, OwnerLoss, WaitMoving, Moving, Finish,
};

struct CabProofSnapshot {
	uint32_t random0 = 0;
	uint32_t random1 = 0;
	decltype(TimerGameCalendar::date) date{};
	Money money = 0;
	int32_t x = 0;
	int32_t y = 0;
	uint orders = 0;
	uint cargo = 0;
};

struct NativeCabProof {
	fs::path output;
	Clock::time_point start = Clock::now();
	CabProofStage stage = CabProofStage::WaitGame;
	VehicleID train_id = VehicleID::Invalid();
	CompanyID owner = CompanyID::Invalid();
	WorldID world = INVALID_WORLD;
	CabProofSnapshot baseline{};
	std::string awaiting;
	std::string failure;
	std::vector<std::string> records;
	unsigned callbacks = 0;
	unsigned moving_callbacks = 0;
	unsigned requested_game_ticks = 0;
	int initial_cab_width = 0;
	int initial_cab_height = 0;
	int resized_cab_width = 0;
	int resized_cab_height = 0;
	int32_t initial_train_x = 0;
	int32_t initial_train_y = 0;
	bool started = false;
	bool movement_stimulus_sent = false;
	bool finished = false;
	bool witnessed_motion = false;
	bool checked_foreign_mask = false;
	TileIndex foreign_tile = INVALID_TILE;

	Train *TrainNow() const { return Train::GetIfValid(this->train_id); }

	void Record(std::string_view event)
	{
		const Train *v = this->TrainNow();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - this->start).count();
		this->records.push_back(fmt::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", event, ms,
			v == nullptr ? -1 : static_cast<int>(v->x_pos), v == nullptr ? -1 : static_cast<int>(v->y_pos),
			static_cast<unsigned>(TimerGameCalendar::date.base()), _random.state[0], _random.state[1],
			Company::IsValidID(this->owner) ? Company::Get(this->owner)->money : Money{0}));
	}

	void WriteManifest() const
	{
		std::ofstream file(this->output / "manifest.tsv", std::ios::trunc);
		file << "event\telapsed_ms\ttrain_x\ttrain_y\tcalendar_date\trng0\trng1\tcompany_money\n";
		for (const auto &record : this->records) file << record << '\n';
		file << "result\t" << (this->finished && this->failure.empty() ? "PASS" : "FAIL") << '\n';
		if (!this->failure.empty()) file << "error\t" << this->failure << '\n';
		file << "callbacks\t" << this->callbacks << '\n';
		file << "foreign_mask_checked\t" << this->checked_foreign_mask << '\n';
		file << "train_moved\t" << this->witnessed_motion << '\n';
		file << "explicit_state_game_loop_ticks\t" << this->requested_game_ticks << '\n';
		file << "skip_to_next_order_stimulus\t" << this->movement_stimulus_sent << '\n';
		file << "cab_size_before\t" << this->initial_cab_width << "x" << this->initial_cab_height << '\n';
		file << "cab_size_after\t" << this->resized_cab_width << "x" << this->resized_cab_height << '\n';
		file << "train_displacement_squared\t" << ([&] {
			const Train *v = this->TrainNow();
			if (v == nullptr) return int64_t{-1};
			const int64_t dx = static_cast<int64_t>(v->x_pos) - this->initial_train_x;
			const int64_t dy = static_cast<int64_t>(v->y_pos) - this->initial_train_y;
			return dx * dx + dy * dy;
		}()) << '\n';
		file << "selected_train_id\t" << this->train_id.base() << '\n';
		file << "selected_world_id\t" << this->world.base() << '\n';
		file << "foreign_masked_tile\t" << this->foreign_tile.base() << '\n';
		if (const auto *graphics = BaseGraphics::GetUsedSet(); graphics != nullptr) {
			file << "base_graphics\t" << graphics->name << '\n';
			for (const auto &entry : graphics->files) file << "base_file\t" << entry.filename << '\n';
		}
	}

	void Fail(std::string_view why)
	{
		this->failure = why;
		this->Record("failed");
		CloseWindowByClass(WindowClass::VehicleCabView);
		this->WriteManifest();
		_exit_game = true;
	}

	void Schedule()
	{
		VideoDriver::GetInstance()->QueueOnMainThread([this] { this->Tick(); });
	}

	bool Capture(std::string_view label)
	{
		/* Screenshot execution and the follow-up callback are queued in that
		 * order for the NEXT locked-buffer draw tick. The game draws the action
		 * once before this capture; no generated or synthetic image is used. */
		this->awaiting = fmt::format("cab_native_{}", label);
		this->Record(fmt::format("capture_{}", label));
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
		this->Record(fmt::format("saved_{}", target.filename().string()));
		this->awaiting.clear();
		return true;
	}

	CabProofSnapshot Snapshot() const
	{
		const Train *v = this->TrainNow();
		return {_random.state[0], _random.state[1], TimerGameCalendar::date,
			Company::Get(this->owner)->money, v->x_pos, v->y_pos, v->GetNumOrders(), v->cargo.TotalCount()};
	}

	bool StillPure()
	{
		if (!_pause_mode.Test(PauseMode::Normal) || this->TrainNow() == nullptr) {
			this->Fail("camera proof lost the paused owned train");
			return false;
		}
		const auto now = this->Snapshot();
		if (now.random0 != this->baseline.random0 || now.random1 != this->baseline.random1 ||
			now.date != this->baseline.date || now.money != this->baseline.money ||
			now.x != this->baseline.x || now.y != this->baseline.y ||
			now.orders != this->baseline.orders || now.cargo != this->baseline.cargo) {
			this->Fail("cab camera changed RNG, date, cash, order count, front-unit cargo, or train position while paused");
			return false;
		}
		return true;
	}

	Window *Cab() const { return FindWindowById(WindowClass::VehicleCabView, this->train_id); }

	void Tick()
	{
		if (this->finished || !this->failure.empty()) return;
		if (++this->callbacks > 300 || Clock::now() - this->start > std::chrono::seconds(55)) {
			this->Fail("native draw callback/time budget exhausted");
			return;
		}
		if (!this->CollectScreenshot()) return;
		if (this->stage != CabProofStage::WaitGame && this->stage != CabProofStage::WaitPause &&
			this->stage != CabProofStage::WaitMoving && this->stage != CabProofStage::Moving &&
			!this->StillPure()) return;
		switch (this->stage) {
			case CabProofStage::WaitGame: {
				if (_game_mode != GameMode::Normal || _switch_mode != SwitchMode::None) { this->Schedule(); return; }
				Train *best = nullptr;
				for (Train *v : Train::Iterate()) {
					if (!CanShowCabView(v)) continue;
					if (best == nullptr || v->cur_speed > best->cur_speed) best = v;
				}
				if (best == nullptr) { this->Fail("loaded game has no eligible owned train"); return; }
				this->train_id = best->index;
				this->owner = best->owner;
				this->world = PlanetManager::GetTileWorld(best->tile);
				this->Record("selected_train");
				IConsoleCmdExec("pause");
				this->stage = CabProofStage::WaitPause;
				this->Schedule();
				return;
			}
			case CabProofStage::WaitPause:
				if (!_pause_mode.Test(PauseMode::Normal)) { this->Schedule(); return; }
				if (!CanShowCabView(this->TrainNow())) { this->Fail("train became ineligible before cab proof"); return; }
				this->baseline = this->Snapshot();
				this->initial_train_x = this->baseline.x;
				this->initial_train_y = this->baseline.y;
				ScrollMainWindowToTile(this->TrainNow()->tile, true);
				/* Require an actual other-world tile to be masked in this WP11 save. */
				for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
					WorldID found = PlanetManager::GetTileWorld(tile);
					if (found != INVALID_WORLD && found != this->world && !CabViewTileVisible(tile, this->world)) {
						this->checked_foreign_mask = true;
						this->foreign_tile = tile;
						break;
					}
				}
				if (!this->checked_foreign_mask || !CabViewTileVisible(this->TrainNow()->tile, this->world)) {
					this->Fail("WP11 same-world terrain mask was not demonstrated"); return;
				}
				this->stage = CabProofStage::Normal;
				this->Capture("normal_viewport");
				return;
			case CabProofStage::Normal:
				ShowVehicleViewWindow(this->TrainNow());
				this->stage = CabProofStage::Open;
				this->Schedule();
				return;
			case CabProofStage::Open: {
				Window *vehicle = FindWindowById(WindowClass::VehicleView, this->train_id);
				if (vehicle == nullptr || vehicle->IsWidgetDisabled(WID_VV_CAB_VIEW)) { this->Fail("real vehicle Cab button unavailable"); return; }
				vehicle->OnClick({}, WID_VV_CAB_VIEW, 1);
				if (this->Cab() == nullptr) { this->Fail("vehicle Cab button did not open the cab"); return; }
				this->initial_cab_width = this->Cab()->width;
				this->initial_cab_height = this->Cab()->height;
				this->stage = CabProofStage::CabAhead;
				this->Capture("cab_ahead");
				return;
			}
			case CabProofStage::CabAhead:
				this->Cab()->OnClick({}, WID_CV_LEFT, 1);
				this->stage = CabProofStage::LookLeft;
				this->Capture("look_left");
				return;
			case CabProofStage::LookLeft:
				this->Cab()->OnClick({}, WID_CV_RIGHT, 1);
				this->stage = CabProofStage::LookRight;
				this->Capture("look_right");
				return;
			case CabProofStage::LookRight:
				this->Cab()->OnClick({}, WID_CV_AHEAD, 1);
				ResizeWindow(this->Cab(), 160, 100, false, false);
				this->resized_cab_width = this->Cab()->width;
				this->resized_cab_height = this->Cab()->height;
				if (this->resized_cab_width < this->initial_cab_width + 160 || this->resized_cab_height < this->initial_cab_height + 100) {
					this->Fail("cab did not resize in both dimensions"); return;
				}
				this->stage = CabProofStage::Resize;
				this->Capture("resized_ahead");
				return;
			case CabProofStage::Resize:
				CloseWindowByClass(WindowClass::VehicleCabView);
				if (this->Cab() != nullptr) { this->Fail("cab did not close"); return; }
				CloseWindowById(WindowClass::VehicleView, this->train_id);
				ScrollMainWindowToTile(this->TrainNow()->tile, true);
				this->stage = CabProofStage::Closed;
				this->Capture("closed_normal_viewport");
				return;
			case CabProofStage::Closed:
				ShowCabViewWindow(this->TrainNow());
				if (this->Cab() == nullptr) { this->Fail("cab did not reopen"); return; }
				this->stage = CabProofStage::Reopen;
				this->Capture("reopened_cab");
				return;
			case CabProofStage::Reopen: {
				/* A transient local-owner loss must close the existing cab. Restore
				 * immediately; no persistent company or save state is edited. */
				CompanyID saved = _local_company;
				_local_company = COMPANY_SPECTATOR;
				this->Cab()->OnRealtimeTick(0);
				bool closed = this->Cab() == nullptr;
				_local_company = saved;
				if (!closed) { this->Fail("cab survived owner loss"); return; }
				this->stage = CabProofStage::OwnerLoss;
				this->Capture("owner_loss_closed");
				return;
			}
			case CabProofStage::OwnerLoss:
				ShowCabViewWindow(this->TrainNow());
				if (this->Cab() == nullptr) { this->Fail("cab could not reopen after owner restoration"); return; }
				IConsoleCmdExec("unpause");
				this->stage = CabProofStage::WaitMoving;
				this->Schedule();
				return;
			case CabProofStage::WaitMoving: {
				Train *v = this->TrainNow();
				if (v == nullptr) { this->Fail("moving train was deleted"); return; }
				if (_pause_mode.Any()) { this->Schedule(); return; }
				if (!this->movement_stimulus_sent) {
					if (v->GetNumOrders() < 2) { this->Fail("train has no next order for movement stimulus"); return; }
					VehicleOrderID next = static_cast<VehicleOrderID>((v->cur_implicit_order_index + 1) % v->GetNumOrders());
					if (!Command<Commands::SkipToOrder>::Post(v->tile, this->train_id, next)) {
						this->Fail("native SkipToOrder fixture stimulus failed"); return;
					}
					this->movement_stimulus_sent = true;
					this->Record("fixture_command_skip_to_next_order");
				}
				const int64_t dx = static_cast<int64_t>(v->x_pos) - this->baseline.x;
				const int64_t dy = static_cast<int64_t>(v->y_pos) - this->baseline.y;
				if (dx * dx + dy * dy >= static_cast<int64_t>(4 * TILE_SIZE) * (4 * TILE_SIZE)) {
					this->witnessed_motion = true;
					this->Record("moving_train");
					IConsoleCmdExec("pause");
					this->stage = CabProofStage::Moving;
					this->Schedule();
					return;
				}
				/* Advance only in the intentional unpaused movement phase. This
				 * exercises real train physics when the offscreen driver outruns its
				 * normal 27 ms game timer. Camera-only stages never call this. */
				if (++this->moving_callbacks > 130) { this->Fail("eligible train did not move in 1040 explicit StateGameLoop calls"); return; }
				for (unsigned i = 0; i < 8; ++i) { StateGameLoop(); ++this->requested_game_ticks; }
				this->Schedule();
				return;
			}
			case CabProofStage::Moving:
				if (!_pause_mode.Test(PauseMode::Normal) || this->Cab() == nullptr) { this->Schedule(); return; }
				/* The authorized movement changed position, RNG and date. Compare
				 * final paint/cleanup against a new paused snapshot. */
				this->baseline = this->Snapshot();
				this->stage = CabProofStage::Finish;
				this->Capture("moving_train_cab");
				return;
			case CabProofStage::Finish:
				CloseWindowByClass(WindowClass::VehicleCabView);
				if (this->Cab() != nullptr) { this->Fail("final cab cleanup failed"); return; }
				this->Record("cab_closed_cleanly");
				this->finished = true;
				this->WriteManifest();
				_exit_game = true;
				return;
		}
	}
};

NativeCabProof *active_native_proof = nullptr;

bool StartNativeCabProof(std::span<std::string_view> args)
{
	if (args.size() != 1 || active_native_proof == nullptr || active_native_proof->started) return false;
	active_native_proof->started = true;
	active_native_proof->Schedule();
	return true;
}

} // namespace

TEST_CASE("Native SDL offscreen cab window paints moves masks and closes", "[.][cab-view][native]")
{
	const char *config = std::getenv("OSTTD_CAB_NATIVE_CONFIG");
	const char *save = std::getenv("OSTTD_CAB_NATIVE_SAVE");
	const char *output = std::getenv("OSTTD_CAB_NATIVE_OUTPUT");
	const char *binary = std::getenv("OSTTD_CAB_NATIVE_BINARY");
	REQUIRE(config != nullptr);
	REQUIRE(save != nullptr);
	REQUIRE(output != nullptr);
	REQUIRE(binary != nullptr);
	REQUIRE(fs::is_regular_file(config));
	REQUIRE(fs::is_regular_file(save));
	REQUIRE(fs::is_regular_file(binary));
	fs::create_directories(output);
	NativeCabProof proof;
	proof.output = fs::canonical(output);
	active_native_proof = &proof;
	IConsole::CmdRegister("cab_native_start", StartNativeCabProof);
	std::array<std::string, 14> arguments = {binary, "-v", "sdl", "-b", "32bpp-anim", "-s", "null", "-m", "null", "-r", "1280x800", "-x", "-c", config};
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
		if (!watchdog_condition.wait_for(lock, std::chrono::seconds(60), [&] { return stopped; })) {
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
	active_native_proof = nullptr;
	if (!proof.finished || !proof.failure.empty()) proof.WriteManifest();
	INFO("Native proof manifest: " << (proof.output / "manifest.tsv"));
	INFO("Native proof failure: " << proof.failure);
	CHECK_FALSE(timed_out);
	CHECK(exit_code == 0);
	CHECK(proof.started);
	CHECK(proof.finished);
	CHECK(proof.checked_foreign_mask);
	CHECK(proof.witnessed_motion);
	CHECK(proof.failure.empty());
}
