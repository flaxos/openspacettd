/* This file is part of OpenSpaceTTD. Distributed under GNU GPL version 2. */
/** @file test_cab_view_gate_native.cpp Isolated native SDL local-gate cab proof. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../3rdparty/nlohmann/json.hpp"
#include "../cab_view.h"
#include "../cab_view_3d.h"
#include "../base_media_graphics.h"
#include "../blitter/factory.hpp"
#include "../company_base.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../console_internal.h"
#include "../core/random_func.hpp"
#include "../core/backup_type.hpp"
#include "../direction_func.h"
#include "../gfx_func.h"
#include "../map_func.h"
#include "../misc/endian_buffer.hpp"
#include "../newgrf_config.h"
#include "../openttd.h"
#include "../order_base.h"
#include "../order_cmd.h"
#include "../palette_func.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/stellar_network.h"
#include "../saveload/saveload_func.h"
#include "../screenshot.h"
#include "../string_func.h"
#include "../table/sprites.h"
#include "../table/palettes.h"
#include "../safeguards.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "../train.h"
#include "../train_cmd.h"
#include "../tunnel_map.h"
#include "../tunnelbridge_map.h"
#include "../vehicle_cmd.h"
#include "../vehicle_func.h"
#include "../vehicle_gui.h"
#include "../video/video_driver.hpp"
#include "../viewport_func.h"
#include "../widgets/cab_view_widget.h"
#include "../window_func.h"
#include "../window_gui.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
TileIndex GateA() { return TileXY(70, 40); }
TileIndex GateB() { return TileXY(112, 40); }
constexpr unsigned PASS_LIMIT = 8192;
constexpr unsigned TOTAL_LIMIT = 24576;

Json FrameStatsJson(const CabViewFrameStats &s)
{
	return {{"rendered", s.rendered}, {"cache_hit", s.cache_hit}, {"frame_id", s.frame_id},
		{"view_width", s.view_width}, {"view_height", s.view_height},
		{"internal_width", s.internal_width}, {"internal_height", s.internal_height}, {"scale", s.scale},
		{"visible_tiles", s.visible_tiles}, {"triangles", s.triangles}, {"threads", s.threads},
		{"prepare_us", s.prepare_us}, {"raster_us", s.raster_us}, {"effect_us", s.effect_us},
		{"output_us", s.output_us}, {"total_us", s.total_us}};
}

Json ProfileSampleJson(const CabViewProfileSample &sample)
{
	return {{"external_total_us", sample.wall_us}, {"renderer", FrameStatsJson(sample.frame)}};
}

Json CacheStatsJson(const CabViewCacheStats &s)
{
	return {{"raw_sprites", s.raw_sprites}, {"ground_textures", s.ground_textures},
		{"vehicle_textures", s.vehicle_textures}, {"box_textures", s.box_textures},
		{"building_shapes", s.building_shapes}, {"cache_entries", s.cache_entries},
		{"raw_sprite_payload_bytes", s.raw_sprite_payload_bytes}, {"texture_payload_bytes", s.texture_payload_bytes},
		{"vehicle_texture_payload_bytes", s.vehicle_texture_payload_bytes},
		{"texture_key_payload_bytes", s.texture_key_payload_bytes}, {"cache_capacity_payload_bytes", s.cache_capacity_payload_bytes},
		{"max_vehicle_union_width", s.max_vehicle_union_width}, {"max_vehicle_union_height", s.max_vehicle_union_height},
		{"max_vehicle_union_pixels", s.max_vehicle_union_pixels},
		{"frame_capacity_bytes", s.frame_capacity_bytes}, {"depth_capacity_bytes", s.depth_capacity_bytes},
		{"scratch_capacity_bytes", s.scratch_capacity_bytes}, {"effect_scratch_capacity_bytes", s.effect_scratch_capacity_bytes}};
}

/** Save an offscreen diagnostic render without changing the SDL framebuffer. */
bool WriteVehicleProfileImage(const fs::path &path, const std::vector<Colour> &pixels, int width, int height)
{
	std::ofstream image(path, std::ios::binary | std::ios::trunc);
	if (!image) return false;
	image << "P6\n" << width << ' ' << height << "\n255\n";
	for (const Colour &pixel : pixels) {
		const char rgb[3] = {static_cast<char>(pixel.r), static_cast<char>(pixel.g), static_cast<char>(pixel.b)};
		image.write(rgb, sizeof(rgb));
	}
	return image.good();
}

Json PaletteRGBA(const Palette &palette)
{
	Json colours = Json::array();
	for (const Colour &c : palette.palette) colours.push_back({c.r, c.g, c.b, c.a});
	return colours;
}

bool SamePalette(const Palette &a, const Palette &b)
{
	if (a.first_dirty != b.first_dirty || a.count_dirty != b.count_dirty) return false;
	for (size_t i = 0; i < std::size(a.palette); ++i) {
		const Colour &x = a.palette[i], &y = b.palette[i];
		if (x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a) return false;
	}
	return true;
}

Json OrderState(const Order &o)
{
	/* Exact command serialization includes packed type/flags, preserving every
	 * non-stop, stop-location and timetable bit for cold/paused equality. */
	return {{"packed", EndianBufferWriter<>::FromValue(o)},
		{"type", static_cast<int>(o.GetType())}, {"destination", o.GetDestination().value},
		{"load", static_cast<int>(o.GetLoadType())}, {"unload", static_cast<int>(o.GetUnloadType())},
		{"non_stop", o.GetNonStopType().base()}, {"stop_location", static_cast<int>(o.GetStopLocation())},
		{"depot_type", o.GetDepotOrderType().base()}, {"depot_action", o.GetDepotActionType().base()},
		{"conditional_variable", static_cast<int>(o.GetConditionVariable())},
		{"conditional_comparator", static_cast<int>(o.GetConditionComparator())},
		{"conditional_skip", o.GetConditionSkipToOrder()}, {"conditional_value", o.GetConditionValue()},
		{"refit", static_cast<int>(o.GetRefitCargo())}, {"wait", o.GetWaitTime()},
		{"travel", o.GetTravelTime()}, {"wait_timetabled", o.IsWaitTimetabled()},
		{"travel_timetabled", o.IsTravelTimetabled()}, {"max_speed", o.GetMaxSpeed()}};
}

Json ConsistState(const Train *front)
{
	Json cars = Json::array();
	for (const Train *car = front; car != nullptr; car = car->Next()) {
		Json packets = Json::array();
		for (const CargoPacket *packet : *car->cargo.Packets()) {
			const Source source = packet->GetSource();
			packets.push_back({{"count", packet->Count()}, {"age", packet->GetPeriodsInTransit()},
				{"source_id", source.id}, {"source_type", static_cast<int>(source.type)},
				{"source_tile", packet->GetSourceXY().base()}, {"first_station", packet->GetFirstStation().base()},
				{"feeder", packet->GetFeederShare().base()}});
		}
		cars.push_back({{"id", car->index.base()}, {"engine", car->engine_type.base()},
			{"owner", car->owner.base()}, {"tile", car->tile.base()}, {"world", PlanetManager::GetTileWorld(car->tile).base()},
			{"x", car->x_pos}, {"y", car->y_pos}, {"z", car->z_pos},
			{"direction", static_cast<int>(car->direction)}, {"moving", static_cast<int>(car->GetMovingDirection())},
			{"track", car->track.base()}, {"hidden", car->vehstatus.Test(VehState::Hidden)},
			{"stopped", car->vehstatus.Test(VehState::Stopped)}, {"speed", car->cur_speed},
			{"cargo_type", static_cast<int>(car->cargo_type)}, {"cargo_capacity", car->cargo_cap},
			{"cargo_stored", car->cargo.StoredCount()}, {"cargo_reserved", car->cargo.ReservedCount()},
			{"cargo_total", car->cargo.TotalCount()}, {"cargo_feeder", car->cargo.GetFeederShare().base()},
			{"packets", std::move(packets)}});
	}
	return cars;
}

Json FrozenState(const Train *front)
{
	Json orders = Json::array();
	for (int i = 0; i < front->GetNumOrders(); ++i) orders.push_back(OrderState(*front->GetOrder(i)));
	Json worlds = Json::array();
	for (const auto &[id, world] : StellarNetwork::Worlds()) worlds.push_back({{"id", id.base()},
		{"name", world.name}, {"opened", world.opened}, {"x", world.x}, {"y", world.y}});
	Json regions = Json::array();
	for (const PlanetRegion &r : PlanetManager::GetAllRegions()) regions.push_back({{"id", r.id.base()},
		{"min_x", r.min_x}, {"max_x", r.max_x}, {"min_y", r.min_y}, {"max_y", r.max_y}});
	Json transit = Json::array();
	for (const auto &[vehicle, distance] : PortalRegistry::GetAllVehicleTransit()) transit.push_back({vehicle, distance});
	std::sort(transit.begin(), transit.end());
	const PortalLink *link = PortalRegistry::GetPortalLink(GateA());
	Json gate = nullptr;
	if (link != nullptr) gate = {{"id", link->id.base()}, {"a_tile", link->end_a.tile.base()},
		{"a_world", link->end_a.world_id.base()}, {"a_dir", static_cast<int>(link->end_a.enter_dir)},
		{"b_tile", link->end_b.tile.base()}, {"b_world", link->end_b.world_id.base()},
		{"b_dir", static_cast<int>(link->end_b.enter_dir)}, {"bidirectional", link->bidirectional},
		{"virtual_length", link->virtual_length}};
	return {{"train_id", front->index.base()}, {"owner", front->owner.base()},
		{"tick", TimerGameTick::counter}, {"date", TimerGameCalendar::date.base()},
		{"date_fract", TimerGameCalendar::date_fract}, {"rng", {_random.state[0], _random.state[1]}},
		{"money", Company::Get(front->owner)->money.base()}, {"current_order", OrderState(front->current_order)},
		{"real_order_index", front->cur_real_order_index}, {"implicit_order_index", front->cur_implicit_order_index},
		{"orders", std::move(orders)}, {"cars", ConsistState(front)}, {"gate", gate},
		{"transit", std::move(transit)}, {"worlds", std::move(worlds)}, {"regions", std::move(regions)}};
}

enum class Phase { Start, Warmup, Open, Outbound, OutboundSafe, Return, Finish,
	NormalBefore, ReopenForRecovery, NormalAfter };
struct GateNativeProof {
	fs::path output;
	fs::path expected;
	Clock::time_point started_at = Clock::now();
	Phase phase = Phase::Start;
	VehicleID id = VehicleID::Invalid();
	WorldID source = INVALID_WORLD, destination = INVALID_WORLD;
	Json manifest = {{"passed", false}, {"events", Json::array()}, {"screenshots", Json::array()}};
	Json paused_baseline;
	std::string pending_shot, pending_profile, pending_vehicle_profile, pending_effect_label, failure;
	unsigned effect_capture_step = 0;
	unsigned callbacks = 0, total_calls = 0, pass_calls = 0;
	unsigned pass = 0;
	bool pass_started[2]{};
	bool cold = false, started = false, finished = false, profile = false, vehicle_profile = false, stage3c_diagnostic = false, stage3d_paired = false;
	bool pending_stage3c_timing = false, pending_stage3c_palette = false;
	bool pending_stage3d_paired = false;
	unsigned profile_rotation = 0;
	bool approach[2]{}, near_approach[2]{}, hidden[2]{}, arrived[2]{}, mixed[2]{};
	bool cab_opened = false, grew_consist = false, cold_equal = false;
	bool saw_normal_recovery = false;
	bool check_pending_purity = false;
	unsigned warmup_calls = 0;

	Train *Front() const { return Train::GetIfValid(this->id); }
	Window *Cab() const { return FindWindowById(WindowClass::VehicleCabView, this->id); }
	void Event(std::string_view label)
	{
		const Train *front = this->Front();
		this->manifest["events"].push_back({{"name", label}, {"elapsed_ms", std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - this->started_at).count()},
			{"explicit_calls", this->total_calls}, {"game_tick", TimerGameTick::counter},
			{"train_x", front == nullptr ? -1 : front->x_pos}, {"train_y", front == nullptr ? -1 : front->y_pos},
			{"train_tile", front == nullptr ? -1 : static_cast<int>(front->tile.base())}});
	}
	void Write() const
	{
		std::ofstream file(this->output / "gate-native-manifest.json", std::ios::trunc);
		file << this->manifest.dump(2) << '\n';
	}
	void Fail(std::string_view message)
	{
		this->failure = message;
		this->manifest["failure"] = message;
		this->Event("failed");
		CloseWindowByClass(WindowClass::VehicleCabView);
		this->Write();
		_exit_game = true;
	}
	void Schedule() { VideoDriver::GetInstance()->QueueOnMainThread([this] { this->Tick(); }); }
	bool Capture(std::string_view name)
	{
		if (Window *cab = this->Cab(); cab != nullptr) {
			const Rect view = cab->GetWidget<NWidgetBase>(WID_CV_VIEW)->GetCurrentRect().Translate(cab->left, cab->top);
			this->manifest["cab_view_rect"] = {{"left", view.left}, {"top", view.top},
				{"right", view.right}, {"bottom", view.bottom}, {"width", view.Width()}, {"height", view.Height()}};
		}
		this->pending_shot = fmt::format("gate_{}_{}", this->cold ? "cold" : "roundtrip", name);
		this->Event(fmt::format("capture_{}", name));
		if (!MakeScreenshot(SC_VIEWPORT, this->pending_shot)) { this->Fail("native screenshot could not be queued"); return false; }
		this->Schedule();
		return true;
	}
	bool Collect()
	{
		if (this->pending_shot.empty()) return true;
		const fs::path source_path(_full_screenshot_path);
		if (!fs::is_regular_file(source_path) || !source_path.filename().string().starts_with(this->pending_shot + ".")) {
			this->Fail("native screenshot missing"); return false;
		}
		const fs::path target = this->output / (this->pending_shot + source_path.extension().string());
		std::error_code error;
		fs::copy_file(source_path, target, fs::copy_options::overwrite_existing, error);
		if (error || fs::file_size(target) == 0) { this->Fail("native screenshot copy failed"); return false; }
		this->manifest["screenshots"].push_back(target.filename().string());
		const Train *front = this->Front();
		if (this->Cab() != nullptr && front != nullptr &&
			ResolveCabViewPresentation(front, front->owner).phase == CabViewPhase::Scene) {
			/* A GUI-scale clue for the last painted cab frame, not an assertion
			 * that this was the framebuffer's exact screenshot render. */
			this->manifest["capture_frame_stats"][this->pending_shot] = FrameStatsJson(GetLastCabViewFrameStats());
		} else {
			/* Opaque transit has no scene frame; after close there is no cab. */
			this->manifest["capture_frame_stats"][this->pending_shot] = nullptr;
		}
		this->pending_shot.clear();
		return true;
	}
	bool PauseAndCapture(std::string_view name)
	{
		if (!_pause_mode.Test(PauseMode::Normal)) IConsoleCmdExec("pause");
		if (!_pause_mode.Test(PauseMode::Normal)) { this->Fail("could not pause observed native phase"); return false; }
		this->paused_baseline = FrozenState(this->Front());
		if (this->Cab() != nullptr) this->Cab()->OnRealtimeTick(0);
		if (!this->Pure()) return false;
		this->check_pending_purity = true;
		if (this->cold && this->stage3c_diagnostic) {
			if (name == "back_arrival_mixed") this->pending_stage3c_timing = true;
			if (name == "back_safe") this->pending_stage3c_palette = true;
		}
		if (this->cold && this->stage3d_paired && name == "back_arrival_mixed") this->pending_stage3d_paired = true;
		if (this->profile && (name == "out_approach" || name == "out_near_approach" || name == "out_transit" || name == "out_arrival_mixed" ||
			name == "back_approach" || name == "back_near_approach" || name == "back_transit" || name == "back_arrival_mixed")) this->pending_profile = name;
		if (name == "out_near_approach" || name == "out_transit" || name == "out_arrival_mixed" ||
			name == "back_near_approach" || name == "back_transit" || name == "back_arrival_mixed") {
			this->pending_effect_label = name;
			this->effect_capture_step = 0;
		}
		return this->Capture(name);
	}
	bool CaptureEffectSequence()
	{
		if (this->pending_effect_label.empty()) return false;
		Window *cab = this->Cab();
		if (cab == nullptr || !this->Pure()) { if (this->failure.empty()) this->Fail("effect capture lost paused cab"); return true; }
		auto click = [&]() { cab->OnClick({0, 0}, WID_CV_EFFECT, 1); };
		std::string suffix;
		switch (this->effect_capture_step++) {
			case 0: cab->OnRealtimeTick(0); cab->OnRealtimeTick(80); suffix = "full_80"; break;
			case 1: cab->OnRealtimeTick(80); suffix = "full_160"; break;
			case 2: click(); click(); suffix = "off"; break;
			case 3: click(); click(); suffix = "reduced_0"; break;
			case 4: cab->OnRealtimeTick(80); suffix = "reduced_80"; break;
			case 5: click(); suffix = "off_again"; break;
			default:
				click(); /* Off -> Full for the continuing journey. */
				this->manifest["effect_captures"].push_back({{"label", this->pending_effect_label},
					{"diagnostic_gui_elapsed_ms", 240}, {"frozen_state_unchanged", true}});
				this->pending_effect_label.clear();
				return false;
		}
		if (!this->Pure()) return true;
		this->Capture(fmt::format("{}_{}", this->pending_effect_label, suffix));
		return true;
	}
	bool Unpause()
	{
		if (_pause_mode.Test(PauseMode::Normal)) IConsoleCmdExec("unpause");
		if (_pause_mode.Any()) { this->Fail("native game stayed paused"); return false; }
		return true;
	}
	bool Advance()
	{
		if (this->pass_calls >= PASS_LIMIT || this->total_calls >= TOTAL_LIMIT) { this->Fail("explicit StateGameLoop budget exhausted"); return false; }
		++this->pass_calls;
		++this->total_calls;
		StateGameLoop();
		return true;
	}
	bool Pure()
	{
		if (!_pause_mode.Test(PauseMode::Normal) || this->Front() == nullptr || FrozenState(this->Front()) != this->paused_baseline) {
			this->Fail("paused camera changed train/orders/cargo, gate/world, RNG/date/tick or cash"); return false;
		}
		return true;
	}
	/** One fixed-pose diagnostic at the naturally paused cold mixed arrival. */
	bool Stage3CTiming()
	{
		Train *front = this->Front();
		if (!this->cold || front == nullptr || this->Cab() == nullptr || !this->Pure()) return false;
		const CabViewPresentation state = ResolveCabViewPresentation(front, front->owner);
		if (state.phase != CabViewPhase::Scene || state.world != this->source) {
			this->Fail("Stage 3C timing did not observe the cold return scene"); return false;
		}
		static constexpr std::array<std::pair<float, float>, 8> heading =
			{{{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}}};
		const auto [dx, dy] = heading[to_underlying(state.moving)];
		CabViewCamera camera;
		camera.x = state.x; camera.y = state.y; camera.z = state.z + 4.0f;
		camera.yaw = std::atan2(dy, dx); camera.vehicle = this->id; camera.world = state.world;
		camera.scene_effect = CabViewSceneEffect::Arrival; camera.effect_elapsed_ms = 0;
		constexpr int width = 640, height = 372;
		const Rect rect{0, 0, width - 1, height - 1};
		std::vector<Colour> pixels(static_cast<size_t>(width) * height);
		DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
		const DrawPixelInfo old_screen = _screen;
		DrawPixelInfo *const old_dpi = _cur_dpi;
		const bool old_anim = _screen_disable_anim;
		const Palette old_palette = _cur_palette;
		const Json frozen = FrozenState(front);
		Json diagnostic = {{"capture_label", "back_arrival_mixed"}, {"view_width", width}, {"view_height", height},
			{"scene_effect", "arrival"}, {"effect_elapsed_ms", 0}, {"warmup_per_mode", 20}, {"frames_per_block", 25},
			{"process_counter_scope", "block wall interval includes out-of-frame pixel comparisons, image copies and cache scans"},
			{"block_order", {"off", "reduced", "reduced", "off", "reduced", "off", "off", "reduced"}},
			{"palette_scope", "canonical_static_private"}, {"incoming_palette_rgba", PaletteRGBA(old_palette)},
			{"canonical_palette_rgba", PaletteRGBA(_palette)}, {"frozen_snapshot", frozen},
			{"pose", {{"x", state.x}, {"y", state.y}, {"z", state.z}, {"tile", state.tile.base()},
				{"moving", static_cast<int>(state.moving)}, {"yaw", camera.yaw}, {"world", state.world.base()}}},
			{"blocks", Json::array()}};
		bool valid = true;
		bool unique_frame_ids = true;
		uint32_t previous_frame_id = 0;
		{
			AutoRestoreBackup palette_backup(_cur_palette, _palette);
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			const CabViewCounterbalancedProbe probe = ProbeCabViewCounterbalanced(rect, camera,
				[&] { return _pause_mode.Test(PauseMode::Normal) && FrozenState(front) == frozen &&
					_cur_dpi == &dpi && _screen.dst_ptr == dpi.dst_ptr && _screen.pitch == dpi.pitch &&
					_screen.left == dpi.left && _screen.top == dpi.top && _screen.width == dpi.width &&
					_screen.height == dpi.height && _screen.zoom == dpi.zoom && _screen_disable_anim; });
			valid = probe.blocks.size() == 8 && probe.exact_cross_mode_geometry && probe.exact_cross_mode_scene_cache && probe.exact_cross_mode_image;
			diagnostic["exact_cross_mode_geometry"] = probe.exact_cross_mode_geometry;
			diagnostic["exact_cross_mode_scene_cache"] = probe.exact_cross_mode_scene_cache;
			diagnostic["exact_cross_mode_image"] = probe.exact_cross_mode_image;
			for (size_t i = 0; i < probe.blocks.size(); ++i) {
				const CabViewCounterbalancedBlock &block = probe.blocks[i];
				const char *mode = block.mode == CabViewEffectMode::Off ? "off" : "reduced";
				const std::string filename = fmt::format("stage3c_block_{}_{}.ppm", i, mode);
				if (!WriteVehicleProfileImage(this->output / filename, block.end_pixels, width, height)) {
					this->Fail("Stage 3C block image could not be written"); return false;
				}
				auto counters = [](const CabViewProcessCounters &c) -> Json {
					return {{"supported", c.supported}, {"user_cpu_us", c.user_cpu_us}, {"system_cpu_us", c.system_cpu_us},
						{"voluntary_context_switches", c.voluntary_context_switches},
						{"involuntary_context_switches", c.involuntary_context_switches}};
				};
				Json samples = Json::array();
				for (const CabViewCounterbalancedSample &sample : block.samples) {
					unique_frame_ids &= sample.frame.frame_id > previous_frame_id;
					previous_frame_id = sample.frame.frame_id;
					samples.push_back({{"relative_start_us", sample.relative_start_us}, {"wall_us", sample.wall_us},
						{"frame", FrameStatsJson(sample.frame)}});
				}
				diagnostic["blocks"].push_back({{"index", i}, {"mode", mode}, {"relative_start_us", block.relative_start_us},
					{"before", counters(block.before)}, {"after", counters(block.after)}, {"samples", std::move(samples)},
					{"image", filename}, {"cache", CacheStatsJson(block.cache)},
					{"exact_same_mode_image", block.exact_same_mode_image}, {"exact_geometry", block.exact_geometry},
					{"exact_same_mode_cache", block.exact_same_mode_cache},
					{"frozen_state_unchanged", block.frozen_state_unchanged}});
				valid &= block.samples.size() == 25 && block.exact_same_mode_image && block.exact_geometry &&
					block.exact_same_mode_cache && block.frozen_state_unchanged;
			}
		}
		diagnostic["unique_measured_frame_ids"] = unique_frame_ids;
		valid &= unique_frame_ids;
		diagnostic["palette_restored"] = SamePalette(_cur_palette, old_palette);
		diagnostic["dpi_restored"] = _cur_dpi == old_dpi && _screen.dst_ptr == old_screen.dst_ptr &&
			_screen.pitch == old_screen.pitch && _screen.left == old_screen.left && _screen.top == old_screen.top &&
			_screen.width == old_screen.width && _screen.height == old_screen.height && _screen.zoom == old_screen.zoom;
		diagnostic["animation_restored"] = _screen_disable_anim == old_anim;
		diagnostic["frozen_state_unchanged"] = FrozenState(front) == frozen;
		this->manifest["stage3c_diagnostic"]["timing"] = std::move(diagnostic);
		if (!valid || !this->manifest["stage3c_diagnostic"]["timing"]["palette_restored"].get<bool>() ||
			!this->manifest["stage3c_diagnostic"]["timing"]["dpi_restored"].get<bool>() ||
			!this->manifest["stage3c_diagnostic"]["timing"]["animation_restored"].get<bool>() || !this->Pure()) {
			if (this->failure.empty()) this->Fail("Stage 3C timing changed pixels, geometry, cache, state or render globals");
			return false;
		}
		return true;
	}
	/** One same-renderer paired mode/control experiment, at the cold mixed return. */
	bool Stage3DPaired()
	{
		Train *front = this->Front();
		if (!this->cold || front == nullptr || this->Cab() == nullptr || !this->Pure()) return false;
		const CabViewPresentation state = ResolveCabViewPresentation(front, front->owner);
		if (state.phase != CabViewPhase::Scene || state.world != this->source) {
			this->Fail("Stage 3D pair experiment lost the cold return scene"); return false;
		}
		static constexpr std::array<std::pair<float, float>, 8> heading =
			{{{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}}};
		const auto [dx, dy] = heading[to_underlying(state.moving)];
		CabViewCamera camera;
		camera.x = state.x; camera.y = state.y; camera.z = state.z + 4.0f;
		camera.yaw = std::atan2(dy, dx); camera.vehicle = this->id; camera.world = state.world;
		camera.scene_effect = CabViewSceneEffect::Arrival; camera.effect_elapsed_ms = 0;
		constexpr int width = 640, height = 372;
		const Rect rect{0, 0, width - 1, height - 1};
		std::vector<Colour> pixels(static_cast<size_t>(width) * height);
		DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
		const DrawPixelInfo old_screen = _screen;
		DrawPixelInfo *const old_dpi = _cur_dpi;
		const bool old_anim = _screen_disable_anim;
		const Palette old_palette = _cur_palette;
		const Json frozen = FrozenState(front);
		Json diagnostic = {{"capture_label", "back_arrival_mixed"}, {"view_width", width}, {"view_height", height},
			{"scene_effect", "arrival"}, {"effect_elapsed_ms", 0}, {"warmup_off", 20}, {"warmup_reduced", 20},
			{"cycles_declared", 50}, {"pairs_declared", 200}, {"frames_declared", 400},
			{"even_pair_order", {"OR", "AB", "RO", "BA"}}, {"odd_pair_order", {"BA", "RO", "AB", "OR"}},
			{"palette_scope", "canonical_static_private"}, {"incoming_palette_rgba", PaletteRGBA(old_palette)},
			{"canonical_palette_rgba", PaletteRGBA(_palette)}, {"frozen_snapshot", frozen},
			{"pose", {{"x", state.x}, {"y", state.y}, {"z", state.z}, {"tile", state.tile.base()},
				{"moving", static_cast<int>(state.moving)}, {"yaw", camera.yaw}, {"world", state.world.base()}}},
			{"pairs", Json::array()}, {"cycles", Json::array()}, {"representative_images", Json::object()}};
		bool valid = true;
		{
			AutoRestoreBackup palette_backup(_cur_palette, _palette);
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			const CabViewPairedProbe probe = ProbeCabViewPaired(rect, camera,
				[&] { return _pause_mode.Test(PauseMode::Normal) && FrozenState(front) == frozen &&
					SamePalette(_cur_palette, _palette) &&
					_cur_dpi == &dpi && _screen.dst_ptr == dpi.dst_ptr && _screen.pitch == dpi.pitch &&
					_screen.left == dpi.left && _screen.top == dpi.top && _screen.width == dpi.width &&
					_screen.height == dpi.height && _screen.zoom == dpi.zoom && _screen_disable_anim; });
			auto pair_name = [](CabViewPairLabel label) -> const char * {
				switch (label) {
					case CabViewPairLabel::OR: return "OR";
					case CabViewPairLabel::AB: return "AB";
					case CabViewPairLabel::RO: return "RO";
					case CabViewPairLabel::BA: return "BA";
				}
				return "";
			};
			diagnostic["completed_pairs"] = probe.completed_pairs;
			diagnostic["completed_cycles"] = probe.completed_cycles;
			diagnostic["unique_measured_frame_ids"] = probe.unique_measured_frame_ids;
			diagnostic["warm_cache"] = CacheStatsJson(probe.warm_cache);
			valid = probe.completed_pairs == 200 && probe.completed_cycles == 50 && probe.unique_measured_frame_ids;
			for (uint32_t i = 0; i < probe.completed_pairs; ++i) {
				const CabViewPairedPair &pair = probe.pairs[i];
				Json arms = Json::array();
				for (const CabViewPairedArm &arm : pair.arms) {
					arms.push_back({{"label", std::string(1, arm.label)},
						{"actual_mode", arm.actual_mode == CabViewEffectMode::Reduced ? "reduced" : "off"},
						{"relative_start_us", arm.sample.relative_start_us}, {"wall_us", arm.sample.wall_us},
						{"frame", FrameStatsJson(arm.sample.frame)}});
				}
				diagnostic["pairs"].push_back({{"cycle", pair.cycle}, {"position", pair.position},
					{"label", pair_name(pair.label)}, {"arms", std::move(arms)},
					{"first_image_exact", pair.first_image_exact}, {"second_image_exact", pair.second_image_exact},
					{"pair_images_equal", pair.pair_images_equal},
					{"first_geometry_exact", pair.first_geometry_exact}, {"second_geometry_exact", pair.second_geometry_exact}});
				valid &= pair.first_image_exact && pair.second_image_exact && pair.pair_images_equal &&
					pair.first_geometry_exact && pair.second_geometry_exact;
			}
			for (uint32_t i = 0; i < probe.completed_cycles; ++i) {
				const CabViewPairedCycle &cycle = probe.cycles[i];
				diagnostic["cycles"].push_back({{"index", i}, {"cache_unchanged", cycle.cache_unchanged},
					{"frozen_state_unchanged", cycle.frozen_state_unchanged}, {"cache", CacheStatsJson(cycle.cache)}});
				valid &= cycle.cache_unchanged && cycle.frozen_state_unchanged;
			}
			for (unsigned i = 0; i < probe.representative_images.size(); ++i) {
				const char *label = pair_name(static_cast<CabViewPairLabel>(i));
				const std::string filename = fmt::format("stage3d_pair_{}.ppm", label);
				if (!WriteVehicleProfileImage(this->output / filename, probe.representative_images[i], width, height)) {
					this->Fail("Stage 3D representative image could not be written"); return false;
				}
				diagnostic["representative_images"][label] = filename;
			}
		}
		diagnostic["palette_restored"] = SamePalette(_cur_palette, old_palette);
		diagnostic["dpi_restored"] = _cur_dpi == old_dpi && _screen.dst_ptr == old_screen.dst_ptr &&
			_screen.pitch == old_screen.pitch && _screen.left == old_screen.left && _screen.top == old_screen.top &&
			_screen.width == old_screen.width && _screen.height == old_screen.height && _screen.zoom == old_screen.zoom;
		diagnostic["animation_restored"] = _screen_disable_anim == old_anim;
		diagnostic["frozen_state_unchanged"] = FrozenState(front) == frozen;
		valid &= diagnostic["palette_restored"].get<bool>() && diagnostic["dpi_restored"].get<bool>() &&
			diagnostic["animation_restored"].get<bool>() && diagnostic["frozen_state_unchanged"].get<bool>();
		this->manifest["stage3d_paired"] = std::move(diagnostic);
		if (!valid || !this->Pure()) {
			if (this->failure.empty()) this->Fail("Stage 3D paired experiment changed image, work, cache, state or render globals");
			return false;
		}
		return true;
	}
	/** Inspect only naturally visible cars at the cold return safe endpoint. */
	bool Stage3CPalettePreflight()
	{
		Train *front = this->Front();
		if (!this->cold || front == nullptr || this->Cab() == nullptr || !this->Pure()) return false;
		const CabViewPresentation state = ResolveCabViewPresentation(front, front->owner);
		if (state.phase != CabViewPhase::Scene || state.world != this->source) {
			this->Fail("Stage 3C palette preflight lost the cold safe scene"); return false;
		}
		const Json frozen = FrozenState(front);
		const Palette incoming_palette = _cur_palette;
		Json cars = Json::array();
		bool complete = true, candidate_actual_sw = false, candidate_other_direction = false;
		unsigned visible_cars = 0;
		for (const Train *car = front; car != nullptr; car = car->Next()) {
			/* This predicate must precede every NewGRF callback and raw decode. */
			if (!CabViewVehicleVisible(car, state.world)) continue;
			++visible_cars;
			Json directions = Json::array();
			for (unsigned ordinal = 0; ordinal < 8; ++ordinal) {
				const Direction query = static_cast<Direction>(ordinal);
				VehicleSpriteSeq seq;
				car->GetImage(query, EngineImageType::OnMap, &seq);
				if (seq.count > seq.seq.size()) {
					this->Fail("Stage 3C native sprite sequence exceeded eight layers"); return false;
				}
				PaletteID default_pal = PAL_NONE;
				if (car->vehstatus.Test(VehState::DefaultPalette)) {
					default_pal = car->vehstatus.Test(VehState::Crashed) ? PALETTE_CRASH : GetVehiclePalette(car);
				}
				const bool crashed = car->vehstatus.Test(VehState::Crashed);
				const CabViewAnimatedSequenceProbe probe = ProbeCabViewAnimatedSequence(seq, default_pal, crashed);
				complete &= probe.complete;
				Json layers = Json::array();
				for (size_t i = 0; i < probe.layers.size(); ++i) {
					const CabViewAnimatedLayerProbe &layer = probe.layers[i];
					layers.push_back({{"sprite", layer.sprite}, {"base_sprite", layer.sprite & SPRITE_MASK},
						{"palette_override", seq.seq[i].pal}, {"effective_palette", layer.palette},
						{"raw_bounds", {{"left", layer.left}, {"top", layer.top}, {"width", layer.width}, {"height", layer.height}}},
						{"raw_animated_pixels", layer.raw_animated_pixels},
						{"eligible_source_contributions", layer.eligible_animated_pixels},
						{"eligible_slot_counts_227_254", layer.eligible_slot_counts},
						{"zero_remap_pixels", layer.zero_remap_pixels},
						{"current_brightness_zero_pixels", layer.brightness_zero_pixels},
						{"all_black", layer.black}, {"destination_dependent", layer.skipped_destination_dependent},
						{"text_recolour", layer.skipped_text_recolour}, {"raw_missing", layer.raw_missing}});
				}
				const bool is_actual_sw = query == Direction::SW;
				if (probe.eligible_animated_pixels > 0) {
					if (is_actual_sw) candidate_actual_sw = true;
					else candidate_other_direction = true;
				}
				directions.push_back({{"query_direction", ordinal}, {"is_actual_cab_sw", is_actual_sw},
					{"image_type", "on_map"}, {"raw_sequence_count", seq.count}, {"default_palette", default_pal},
					{"complete", probe.complete}, {"rejected_oversize", probe.rejected_oversize},
					{"raw_animated_pixels", probe.raw_animated_pixels},
					{"eligible_source_contributions", probe.eligible_animated_pixels},
					{"layers", std::move(layers)}});
				if (FrozenState(front) != frozen || !SamePalette(_cur_palette, incoming_palette)) {
					this->Fail("Stage 3C vehicle callback or raw inspection changed frozen game or palette state"); return false;
				}
			}
			cars.push_back({{"vehicle_id", car->index.base()}, {"engine_id", car->engine_type.base()},
				{"world", PlanetManager::GetTileWorld(car->tile).base()}, {"tile", car->tile.base()},
				{"actual_vehicle_direction", static_cast<int>(car->direction)},
				{"actual_renderer_query_direction", static_cast<int>(Direction::SW)},
				{"default_palette_enabled", car->vehstatus.Test(VehState::DefaultPalette)},
				{"crashed", car->vehstatus.Test(VehState::Crashed)}, {"directions", std::move(directions)}});
		}
		Json diagnostic = {{"capture_label", "back_safe"}, {"world", state.world.base()},
			{"visible_car_count", visible_cars}, {"scan_complete", complete && visible_cars > 0},
			{"candidate_actual_sw", candidate_actual_sw}, {"candidate_other_direction", candidate_other_direction},
			{"eligible_count_semantics", "source contributions surviving later supported layer alpha; not projected cab pixels or observed palette change"},
			{"negative_scope", "this visible consist, current livery and cargo state, eight OnMap query directions only"},
			{"palette_anim_start", PALETTE_ANIM_START}, {"palette_anim_size", PALETTE_ANIM_SIZE},
			{"incoming_palette_rgba", PaletteRGBA(incoming_palette)}, {"frozen_snapshot", frozen},
			{"cars", std::move(cars)}, {"frozen_state_unchanged", FrozenState(front) == frozen},
			{"palette_state_unchanged", SamePalette(_cur_palette, incoming_palette)}};
		this->manifest["stage3c_diagnostic"]["vehicle_palette_preflight"] = std::move(diagnostic);
		if (visible_cars == 0 || !this->Pure()) {
			if (this->failure.empty()) this->Fail("Stage 3C preflight found no visible consist cars");
			return false;
		}
		return true;
	}
	bool ProfilePaused(std::string_view label)
	{
		Train *front = this->Front();
		Window *cab = this->Cab();
		if (front == nullptr || cab == nullptr || !_pause_mode.Test(PauseMode::Normal)) {
			this->Fail("profiling lost paused train or cab"); return false;
		}
		const CabViewPresentation state = ResolveCabViewPresentation(front, front->owner);
		const bool transit = label == "out_transit" || label == "back_transit";
		if (state.phase != (transit ? CabViewPhase::LocalTransit : CabViewPhase::Scene)) {
			this->Fail("profiling phase differs from captured native phase"); return false;
		}
		static constexpr std::array<std::pair<float, float>, 8> heading =
			{{{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}}};
		const auto [dx, dy] = heading[to_underlying(state.moving)];
		CabViewCamera camera;
		camera.x = static_cast<float>(state.x);
		camera.y = static_cast<float>(state.y);
		camera.z = static_cast<float>(state.z) + 4.0f;
		camera.yaw = std::atan2(dy, dx);
		camera.vehicle = this->id;
		camera.world = state.world;
		const TileIndex gate = this->pass == 0 ? GateA() : GateB();
		const DiagDirection gate_direction = GetTunnelBridgeDirection(gate);
		const int face_x = static_cast<int>(TileX(gate) * TILE_SIZE) + (gate_direction == DiagDirection::SW ? static_cast<int>(TILE_SIZE) : 0);
		const int face_y = static_cast<int>(TileY(gate) * TILE_SIZE) + static_cast<int>(TILE_SIZE) / 2;
		const int signed_face_distance = gate_direction == DiagDirection::SW ? face_x - state.x : state.x - face_x;
		const int lateral_offset = std::abs(state.y - face_y);
		static constexpr std::array<CabViewEffectMode, 3> modes = {CabViewEffectMode::Off, CabViewEffectMode::Reduced, CabViewEffectMode::Full};
		for (const auto &[width, height] : {std::pair{480, 272}, std::pair{640, 372}}) {
			const Json frozen = FrozenState(front);
			const Rect rect{0, 0, width - 1, height - 1};
			for (unsigned order = 0; order < modes.size(); ++order) {
				const CabViewEffectMode mode = modes[(order + this->profile_rotation) % modes.size()];
				const char *mode_name = mode == CabViewEffectMode::Off ? "off" : mode == CabViewEffectMode::Reduced ? "reduced" : "full";
				const uint32_t elapsed_ms = mode == CabViewEffectMode::Full ?
					(transit || label.find("near_approach") != std::string_view::npos ? 600 : label.find("arrival") != std::string_view::npos ? 125 : 0) : 0;
				camera.effect_mode = mode;
				camera.scene_effect = label.find("arrival") != std::string_view::npos ? CabViewSceneEffect::Arrival : CabViewSceneEffect::None;
				camera.effect_elapsed_ms = elapsed_ms;
				std::vector<Colour> pixels(static_cast<size_t>(width) * height);
				DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
				Json cell = {{"capture_label", label}, {"phase", transit ? "transit" : "scene"},
					{"mode", mode_name}, {"mode_rotation", this->profile_rotation}, {"effect_elapsed_ms", elapsed_ms},
					{"view_width", width}, {"view_height", height}, {"warmup_count", 20},
					{"sample_count", 100}, {"selected_world", state.world.base()},
					{"source_world", (this->pass == 0 ? this->source : this->destination).base()},
					{"gate", {{"tile", gate.base()}, {"world", (this->pass == 0 ? this->source : this->destination).base()},
						{"face_x", face_x}, {"face_y", face_y}, {"signed_face_distance", signed_face_distance},
						{"lateral_offset", lateral_offset}}},
					{"frozen_snapshot", frozen},
					{"pose", {{"x", state.x}, {"y", state.y}, {"z", state.z},
						{"tile", state.tile.base()}, {"moving", static_cast<int>(state.moving)},
						{"camera_yaw", camera.yaw}}}, {"samples", Json::array()}};
				const Palette incoming_palette = _cur_palette;
				cell["palette_scope"] = this->vehicle_profile ? "canonical_static_private" : "actual_current";
				cell["incoming_palette_rgba"] = PaletteRGBA(incoming_palette);
				cell["palette_rgba"] = PaletteRGBA(this->vehicle_profile ? _palette : incoming_palette);
				cell["incoming_palette_dirty"] = {{"first", incoming_palette.first_dirty}, {"count", incoming_palette.count_dirty}};
				/* The private framebuffer and blitter state are restored before
				 * checking the frozen simulation snapshot. Scene samples use a
				 * separate renderer, with its own initially full-size scale/cache. */
				{
					AutoRestoreBackup palette_backup(_cur_palette, this->vehicle_profile ? _palette : incoming_palette);
					AutoRestoreBackup screen_backup(_screen, dpi);
					AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
					AutoRestoreBackup anim_backup(_screen_disable_anim, true);
					if (transit) {
						cell["diagnostic_image"] = nullptr;
						auto draw = [&]() -> Json {
							auto external_start = Clock::now();
							const CabViewTransitStats stats = DrawCabViewTransit(rect, mode, elapsed_ms);
							const uint64_t external_us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - external_start).count();
							return {{"external_total_us", external_us}, {"effect_us", stats.effect_us},
								{"total_us", stats.total_us}, {"renderer", nullptr}, {"scene_geometry", nullptr}};
						};
						cell["cold_sample"] = draw();
						for (int i = 0; i < 20; ++i) (void)draw();
						for (int i = 0; i < 100; ++i) cell["samples"].push_back(draw());
						cell["cache"] = nullptr;
					} else {
						CabViewProfileBatch batch = ProfileCabView3D(rect, camera, 20, 100);
						cell["cold_sample"] = ProfileSampleJson(batch.cold);
						cell["cached_sample"] = ProfileSampleJson(batch.cached);
						cell["cache"] = CacheStatsJson(batch.cache);
						if (!batch.cached.frame.cache_hit || batch.cached.frame.rendered ||
							batch.cached.frame.prepare_us != 0 || batch.cached.frame.raster_us != 0 || batch.cached.frame.effect_us != 0) {
							this->Fail("private profile did not reuse the final scene frame"); return false;
						}
						for (const CabViewProfileSample &sample : batch.samples) {
							const CabViewFrameStats &s = sample.frame;
							if (!s.rendered || s.cache_hit || s.internal_width != width || s.internal_height != height || s.scale != 1.0f) {
								this->Fail("private profile did not render a new full-size scene frame"); return false;
							}
							cell["samples"].push_back(ProfileSampleJson(sample));
						}
						if (mode == CabViewEffectMode::Off) {
							const std::string filename = fmt::format("scene_{}_{}_{}x{}_off.ppm", this->cold ? "cold" : "roundtrip", label, width, height);
							if (!WriteVehicleProfileImage(this->output / filename, pixels, width, height)) {
								this->Fail("fixed-scale scene diagnostic image could not be written"); return false;
							}
							cell["diagnostic_image"] = filename;
						} else {
							cell["diagnostic_image"] = nullptr;
						}
					}
				}
				if (!SamePalette(_cur_palette, incoming_palette)) { this->Fail("private scene profile did not restore full palette"); return false; }
				cell["palette_state_restored"] = true;
				if (FrozenState(front) != frozen || !this->Pure()) {
					if (this->failure.empty()) this->Fail("profiling changed frozen game state");
					return false;
				}
				cell["frozen_state_unchanged"] = true;
				this->manifest["profiles"].push_back(std::move(cell));
			}
		}
		return true;
	}
	/** Measure the existing single-layer vehicle path beside a naturally arrived car.
	 * This diagnostic camera includes that car; it never changes the live cab. */
	bool ProfileVisibleVehicle(std::string_view label)
	{
		Train *front = this->Front();
		const WorldID target = this->pass == 0 ? this->destination : this->source;
		if (front == nullptr || this->Cab() == nullptr || !_pause_mode.Test(PauseMode::Normal) ||
			ResolveCabViewPresentation(front, front->owner).phase != CabViewPhase::Scene) {
			this->Fail("vehicle profiling lost paused arrival scene"); return false;
		}
		Train *visible = nullptr;
		for (Train *car = front; car != nullptr; car = car->Next()) {
			if (CabViewVehicleVisible(car, target)) { visible = car; break; }
		}
		if (visible == nullptr) { this->Fail("vehicle profiling has no visible arrived car"); return false; }
		const Json frozen = FrozenState(front);
		CabViewCamera camera;
		camera.x = static_cast<float>(visible->x_pos);
		camera.y = static_cast<float>(visible->y_pos) + 2.5f * TILE_SIZE;
		camera.z = static_cast<float>(visible->z_pos) + 7.0f;
		camera.yaw = -static_cast<float>(M_PI) / 2;
		camera.world = target;
		camera.vehicle = VehicleID::Invalid(); // Include the selected car in the diagnostic scene.
		static constexpr std::array<CabViewEffectMode, 3> modes = {CabViewEffectMode::Off, CabViewEffectMode::Reduced, CabViewEffectMode::Full};
		Json cells = Json::array();
		for (const auto &[width, height] : {std::pair{480, 272}, std::pair{640, 372}}) {
			const Rect rect{0, 0, width - 1, height - 1};
			for (unsigned order = 0; order < modes.size(); ++order) {
				const CabViewEffectMode mode = modes[(order + this->profile_rotation) % modes.size()];
				const char *mode_name = mode == CabViewEffectMode::Off ? "off" : mode == CabViewEffectMode::Reduced ? "reduced" : "full";
				camera.effect_mode = mode;
				camera.scene_effect = CabViewSceneEffect::None;
				camera.effect_elapsed_ms = 0;
				std::vector<Colour> pixels(static_cast<size_t>(width) * height);
				DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
				Json cell = {{"capture_label", label}, {"phase", "scene"}, {"mode", mode_name},
					{"mode_rotation", this->profile_rotation}, {"effect_elapsed_ms", 0},
					{"view_width", width}, {"view_height", height}, {"warmup_count", 20}, {"sample_count", 100},
					{"selected_world", target.base()}, {"visible_car_id", visible->index.base()},
					{"pose", {{"x", camera.x}, {"y", camera.y}, {"z", camera.z}, {"camera_yaw", camera.yaw},
						{"tile", visible->tile.base()}, {"direction", static_cast<int>(visible->direction)}}},
					{"diagnostic_camera", {{"x", camera.x}, {"y", camera.y}, {"z", camera.z},
						{"yaw", camera.yaw}, {"world", target.base()}, {"own_exclusion", nullptr},
						{"offset_from_visible_car", {0.0f, 2.5f * TILE_SIZE, 7.0f}}}},
					{"observed_train_pose", {{"front_x", front->x_pos}, {"front_y", front->y_pos},
						{"front_z", front->z_pos}, {"visible_x", visible->x_pos}, {"visible_y", visible->y_pos},
						{"visible_z", visible->z_pos}, {"visible_direction", static_cast<int>(visible->direction)}}},
					{"frozen_snapshot", frozen}, {"samples", Json::array()}};
				const Palette incoming_palette = _cur_palette;
				cell["palette_scope"] = "canonical_static_private";
				cell["incoming_palette_rgba"] = PaletteRGBA(incoming_palette);
				cell["palette_rgba"] = PaletteRGBA(_palette);
				cell["incoming_palette_dirty"] = {{"first", incoming_palette.first_dirty}, {"count", incoming_palette.count_dirty}};
				{
					AutoRestoreBackup palette_backup(_cur_palette, _palette);
					AutoRestoreBackup screen_backup(_screen, dpi);
					AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
					AutoRestoreBackup anim_backup(_screen_disable_anim, true);
					CabViewProfileBatch batch = ProfileCabView3D(rect, camera, 20, 100);
					cell["cold_sample"] = ProfileSampleJson(batch.cold);
					cell["cached_sample"] = ProfileSampleJson(batch.cached);
					cell["cache"] = CacheStatsJson(batch.cache);
					if (!batch.cached.frame.cache_hit || batch.cached.frame.rendered ||
						batch.cached.frame.prepare_us != 0 || batch.cached.frame.raster_us != 0 || batch.cached.frame.effect_us != 0) {
						this->Fail("vehicle profile did not reuse the final scene frame"); return false;
					}
					for (const CabViewProfileSample &sample : batch.samples) {
						const CabViewFrameStats &s = sample.frame;
						if (!s.rendered || s.cache_hit || s.internal_width != width || s.internal_height != height || s.scale != 1.0f) {
							this->Fail("vehicle profile did not render a new full-size scene frame"); return false;
						}
						cell["samples"].push_back(ProfileSampleJson(sample));
					}
					const std::vector<Colour> included = pixels;
					if (mode == CabViewEffectMode::Off) {
						const std::string filename = fmt::format("vehicle_{}_{}_{}x{}_off.ppm", this->cold ? "cold" : "roundtrip", label, width, height);
						if (!WriteVehicleProfileImage(this->output / filename, included, width, height)) {
							this->Fail("vehicle diagnostic image could not be written"); return false;
						}
						cell["diagnostic_image"] = filename;
					} else {
						cell["diagnostic_image"] = nullptr;
					}
					camera.vehicle = visible->index;
					(void)ProfileCabView3D(rect, camera, 0, 1); // Positive control: omit this one visible car.
					camera.vehicle = VehicleID::Invalid();
					size_t changed = 0;
					for (size_t i = 0; i < pixels.size(); ++i) changed += pixels[i] != included[i];
					if (changed == 0) { this->Fail("vehicle profile cannot see its selected car"); return false; }
					cell["visible_pixel_control"] = {{"omitted_car_id", visible->index.base()}, {"different_pixels", changed}};
				}
				if (!SamePalette(_cur_palette, incoming_palette)) { this->Fail("vehicle profile did not restore full palette"); return false; }
				cell["palette_state_restored"] = true;
				if (FrozenState(front) != frozen || !this->Pure()) {
					if (this->failure.empty()) this->Fail("vehicle profiling changed frozen game state");
					return false;
				}
				cell["frozen_state_unchanged"] = true;
				cells.push_back(std::move(cell));
			}
		}
		/* Query only visible cars, and only after the cold image timings. A masked
		 * car's NewGRF GetImage callback must not run for this evidence. */
		Json sequences = Json::array();
		for (const Train *car = front; car != nullptr; car = car->Next()) {
			if (!CabViewVehicleVisible(car, target)) continue;
			VehicleSpriteSeq seq;
			car->GetImage(Direction::SW, EngineImageType::OnMap, &seq);
			if (seq.count > seq.seq.size()) { this->Fail("vehicle sprite sequence exceeds native capacity"); return false; }
			PaletteID default_pal = PAL_NONE;
			if (car->vehstatus.Test(VehState::DefaultPalette)) {
				default_pal = car->vehstatus.Test(VehState::Crashed) ? PALETTE_CRASH : GetVehiclePalette(car);
			}
			Json layers = Json::array();
			for (uint i = 0; i < seq.count; ++i) {
				const PaletteID effective = (seq.seq[i].pal == 0 || car->vehstatus.Test(VehState::Crashed)) ? default_pal : seq.seq[i].pal;
				layers.push_back({{"sprite", seq.seq[i].sprite}, {"base_sprite", seq.seq[i].sprite & SPRITE_MASK},
					{"palette_override", seq.seq[i].pal}, {"effective_palette", effective}});
			}
			sequences.push_back({{"id", car->index.base()}, {"type", "train"}, {"engine", car->engine_type.base()},
				{"tile", car->tile.base()}, {"world", PlanetManager::GetTileWorld(car->tile).base()},
				{"direction", static_cast<int>(car->direction)}, {"sprite_direction", static_cast<int>(Direction::SW)},
				{"image_type", "on_map"}, {"default_palette_enabled", car->vehstatus.Test(VehState::DefaultPalette)},
				{"crashed", car->vehstatus.Test(VehState::Crashed)}, {"default_palette", default_pal},
				{"raw_sequence_count", seq.count}, {"layers", std::move(layers)}});
		}
		if (FrozenState(front) != frozen || !this->Pure()) {
			if (this->failure.empty()) this->Fail("vehicle sequence lookup changed frozen game state");
			return false;
		}
		for (Json &cell : cells) {
			cell["vehicle_sequence"] = sequences;
			this->manifest["vehicle_profiles"].push_back(std::move(cell));
		}
		return true;
	}
	bool ProbeRenderedMixed(WorldID target)
	{
		/* A private, real-base CPU render at the frozen native game state.
		 * Excluding an already-masked car must leave every pixel unchanged;
		 * excluding an arrived visible car must change pixels. This exercises
		 * AddVehicles through the actual rasterizer, beyond the policy helper. */
		if (!_pause_mode.Test(PauseMode::Normal)) IConsoleCmdExec("pause");
		if (!_pause_mode.Test(PauseMode::Normal)) { this->Fail("mixed render could not pause"); return false; }
		Train *visible = nullptr;
		for (Train *car = this->Front(); car != nullptr; car = car->Next()) {
			if (CabViewVehicleVisible(car, target)) { visible = car; break; }
		}
		if (visible == nullptr) { this->Fail("mixed render has no arrived positive control"); return false; }
		const Json frozen = FrozenState(this->Front());
		CabViewCamera camera;
		camera.x = visible->x_pos;
		camera.y = visible->y_pos + 2.5f * TILE_SIZE;
		camera.z = visible->z_pos + 7;
		camera.yaw = -static_cast<float>(M_PI) / 2;
		camera.world = target;
		constexpr int width = 160, height = 100;
		auto paint = [&](VehicleID omitted) {
			std::vector<Colour> pixels(width * height);
			DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
			camera.vehicle = omitted;
			{
				/* 32bpp MoveTo uses _screen.pitch, while 32bpp-anim otherwise
				 * addresses the live animation buffer. Match the engine's own
				 * offscreen convention and restore all three globals. */
				AutoRestoreBackup screen_backup(_screen, dpi);
				AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
				AutoRestoreBackup anim_backup(_screen_disable_anim, true);
				ResetCabView3D();
				DrawCabView3D({0, 0, width - 1, height - 1}, camera, 1, 0);
			}
			return pixels;
		};
		const auto baseline = paint(VehicleID::Invalid());
		unsigned masked = 0;
		for (const Train *car = this->Front(); car != nullptr; car = car->Next()) {
			if (CabViewVehicleVisible(car, target)) continue;
			if (paint(car->index) != baseline) { this->Fail("masked car changed actual CPU render pixels"); return false; }
			++masked;
		}
		if (masked < 2 || paint(visible->index) == baseline) {
			this->Fail("mixed CPU render lacked masked cars or visible-car positive control"); return false;
		}
		ResetCabView3D();
		if (FrozenState(this->Front()) != frozen) { this->Fail("diagnostic CPU render changed frozen game state"); return false; }
		this->manifest["mixed_render_proof"].push_back({{"pass", this->pass}, {"masked_car_count", masked},
			{"visible_positive_control", visible->index.base()}, {"width", width}, {"height", height}});
		return true;
	}
	bool CheckColdAtStartup()
	{
		/* The startup script runs as the load completes, before a queued draw
		 * callback or an explicit StateGameLoop call can advance the game. */
		if (FindWindowByClass(WindowClass::VehicleCabView) != nullptr) { this->Fail("cab unexpectedly persisted after cold load"); return false; }
		Train *front = Train::GetIfValid(VehicleID{0});
		if (front == nullptr || front->owner != _local_company) { this->Fail("cold primary train missing"); return false; }
		std::ifstream file(this->expected);
		if (!file) { this->Fail("cold expected snapshot missing"); return false; }
		Json expected = Json::parse(file);
		Json now = FrozenState(front);
		if (now != expected) {
			std::ofstream(this->output / "cold-actual-snapshot.json") << now.dump(2) << '\n';
			this->Fail("cold load differs from frozen full orders/consist/cargo/world/gate/RNG/date/cash snapshot"); return false;
		}
		this->cold_equal = true;
		this->Event("cold_snapshot_equal_before_advance");
		if (!_pause_mode.Test(PauseMode::Normal)) IConsoleCmdExec("pause");
		return true;
	}
	bool SetupFourCars()
	{
		Train *front = Train::GetIfValid(VehicleID{0});
		if (front == nullptr || !front->IsInDepot() || front->owner != _local_company) { this->Fail("fixture train 0 is not owned in depot"); return false; }
		this->id = front->index;
		Train *wagon = front->Next();
		if (wagon == nullptr || wagon->Next() != nullptr) { this->Fail("fixture train needs exactly one initial wagon"); return false; }
		if (!front->vehstatus.Test(VehState::Stopped) &&
			Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, front->index, false).Failed()) {
			this->Fail("fixture stop command failed"); return false;
		}
		for (unsigned i = 0; i < 2; ++i) {
			auto [cost, new_id, capacity, mail, cargo] = Command<Commands::BuildVehicle>::Do(
				DoCommandFlag::Execute, front->tile, wagon->engine_type, false, wagon->cargo_type, ClientID::Invalid);
			if (cost.Failed()) { this->Fail("fixture wagon build command failed"); return false; }
			if (Train::Get(new_id)->First()->index != front->index &&
				Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, new_id, front->Last()->index, false).Failed()) {
				this->Fail("fixture wagon build/couple command failed"); return false;
			}
			this->Event("fixture_build_and_couple_wagon");
		}
		unsigned count = 0;
		for (const Train *car = front; car != nullptr; car = car->Next()) ++count;
		if (count != 4) { this->Fail("fixture did not create four-car consist"); return false; }
		this->grew_consist = true;
		if (Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, front->index, false).Failed()) {
			this->Fail("fixture restart command failed"); return false;
		}
		this->Event("fixture_start_train");
		return true;
	}
	bool StartPass(unsigned index)
	{
		Train *front = this->Front();
		if (front == nullptr || front->GetNumOrders() < 2) { this->Fail("fixture train/order missing"); return false; }
		if (!this->Unpause()) return false;
		if (index == 1) {
			if (Command<Commands::ReverseTrainDirection>::Do(DoCommandFlag::Execute, front->index, false).Failed()) {
				this->Fail("native reverse command failed"); return false;
			}
			this->Event("fixture_reverse_train_command");
		}
		if (Command<Commands::SkipToOrder>::Do(DoCommandFlag::Execute, front->index,
			static_cast<VehicleOrderID>(index == 0 ? 1 : 0)).Failed()) {
			this->Fail("native skip-to-order command failed"); return false;
		}
		this->Event("fixture_skip_to_order_command");
		this->pass = index;
		/* The initial depot warmup is charged to the first directional pass. */
		if (index != 0) this->pass_calls = 0;
		this->pass_started[index] = true;
		return true;
	}
	void Observe()
	{
		Train *front = this->Front();
		if (front == nullptr || this->Cab() == nullptr) { this->Fail("native primary train/cab vanished"); return; }
		const CabViewPresentation state = ResolveCabViewPresentation(front, front->owner);
		const WorldID origin = this->pass == 0 ? this->source : this->destination;
		const WorldID target = this->pass == 0 ? this->destination : this->source;
		const TileIndex gate = this->pass == 0 ? GateA() : GateB();
		if (!this->approach[this->pass] && state.phase == CabViewPhase::Scene && state.world == origin &&
			state.moving == DiagDirToDir(GetTunnelBridgeDirection(gate)) &&
			std::abs(front->x_pos - static_cast<int>(TileX(gate) * TILE_SIZE)) < 10 * static_cast<int>(TILE_SIZE)) {
			this->approach[this->pass] = true; this->Event("observed_approach");
			this->PauseAndCapture(this->pass == 0 ? "out_approach" : "back_approach"); return;
		}
		/* Match the renderer's SW/NE physical face, not merely the gate tile
		 * centre. Capture a naturally reached scene within one tile ahead of
		 * that face, aligned with its entrance and heading toward it. */
		const DiagDirection gate_direction = GetTunnelBridgeDirection(gate);
		const int face_x = static_cast<int>(TileX(gate) * TILE_SIZE) + (gate_direction == DiagDirection::SW ? static_cast<int>(TILE_SIZE) : 0);
		const int face_y = static_cast<int>(TileY(gate) * TILE_SIZE) + static_cast<int>(TILE_SIZE) / 2;
		const int signed_face_distance = gate_direction == DiagDirection::SW ? face_x - front->x_pos : front->x_pos - face_x;
		const int lateral_offset = std::abs(front->y_pos - face_y);
		if (this->approach[this->pass] && !this->near_approach[this->pass] &&
			state.phase == CabViewPhase::Scene && state.world == origin && CabViewLocalGateVisible(gate, origin) &&
			state.moving == DiagDirToDir(gate_direction) && signed_face_distance > 0 &&
			signed_face_distance <= static_cast<int>(TILE_SIZE) && lateral_offset <= static_cast<int>(TILE_SIZE) / 2) {
			this->near_approach[this->pass] = true;
			this->Event("observed_near_approach");
			this->PauseAndCapture(this->pass == 0 ? "out_near_approach" : "back_near_approach"); return;
		}
		if (!this->hidden[this->pass] && state.phase == CabViewPhase::LocalTransit) {
			if (!this->near_approach[this->pass]) { this->Fail("gate transit had no visible one-tile approach sample"); return; }
			this->hidden[this->pass] = true; this->Event("observed_opaque_transit");
			this->PauseAndCapture(this->pass == 0 ? "out_transit" : "back_transit"); return;
		}
		const bool new_arrival = !this->arrived[this->pass] && state.phase == CabViewPhase::Scene && state.world == target;
		if (new_arrival) { this->arrived[this->pass] = true; this->Event("observed_arrival"); }
		bool new_mixed = false;
		if (this->arrived[this->pass] && !this->mixed[this->pass]) {
			unsigned target_units = 0, hidden_units = 0, origin_units = 0;
			Json mask = Json::array();
			for (Train *car = front; car != nullptr; car = car->Next()) {
				const WorldID car_world = PlanetManager::GetTileWorld(car->tile);
				const bool hidden = car->vehstatus.Test(VehState::Hidden) || car->track == Track::Wormhole;
				if (hidden) ++hidden_units;
				else if (car_world == target) ++target_units;
				else if (car_world == origin) ++origin_units;
				const bool visible = CabViewVehicleVisible(car, target);
				if (visible != (!hidden && car_world == target)) { this->Fail("per-car native world mask differs from expected state"); return; }
				mask.push_back({{"id", car->index.base()}, {"world", car_world.base()}, {"hidden", hidden}, {"visible", visible}});
			}
			if (target_units > 0 && hidden_units > 0 && origin_units > 0) {
				this->mixed[this->pass] = true; this->manifest["mixed_world_mask"].push_back(mask);
				this->Event("observed_three_state_consist");
				new_mixed = true;
			}
		}
		/* Arrival and all three car states can coexist for only one native
		 * tick. Inspect both before pausing for their shared rendered frame. */
		if (new_arrival || new_mixed) {
			if (new_mixed && !this->ProbeRenderedMixed(target)) return;
			const std::string_view name = this->pass == 0 ?
				(new_arrival && new_mixed ? "out_arrival_mixed" : new_arrival ? "out_arrival" : "out_mixed") :
				(new_arrival && new_mixed ? "back_arrival_mixed" : new_arrival ? "back_arrival" : "back_mixed");
			if (this->PauseAndCapture(name) && new_mixed && this->vehicle_profile) {
				this->pending_vehicle_profile = this->pass == 0 ? "out_vehicle_arrival" : "back_vehicle_arrival";
			}
			return;
		}
		if (!this->approach[this->pass] || !this->hidden[this->pass] || !this->arrived[this->pass] || !this->mixed[this->pass]) return;
		bool all_target = true;
		for (const Train *car = front; car != nullptr; car = car->Next()) {
			if (car->vehstatus.Test(VehState::Hidden) || PlanetManager::GetTileWorld(car->tile) != target) all_target = false;
		}
		if (!all_target || state.phase != CabViewPhase::Scene ||
			std::abs(front->x_pos - static_cast<int>((TileX(this->pass == 0 ? GateB() : GateA())) * TILE_SIZE)) < 5 * static_cast<int>(TILE_SIZE)) return;
		this->Event("observed_safe_endpoint");
		if (!this->PauseAndCapture(this->pass == 0 ? "out_safe" : "back_safe")) return;
		this->phase = this->pass == 0 ? Phase::OutboundSafe : Phase::Finish;
	}
	void Tick()
	{
		if (this->finished || !this->failure.empty()) return;
		if (++this->callbacks > 3000 || Clock::now() - this->started_at > std::chrono::seconds(180)) {
			this->Fail("native callback or 180-second controller budget exhausted"); return;
		}
		if (!this->Collect()) return;
		if (this->check_pending_purity) {
			if (!this->Pure()) return;
			this->check_pending_purity = false;
		}
		if (this->CaptureEffectSequence()) return;
		if (this->pending_stage3c_timing) {
			this->pending_stage3c_timing = false;
			if (!this->Stage3CTiming()) return;
		}
		if (this->pending_stage3c_palette) {
			this->pending_stage3c_palette = false;
			if (!this->Stage3CPalettePreflight()) return;
		}
		if (this->pending_stage3d_paired) {
			this->pending_stage3d_paired = false;
			if (!this->Stage3DPaired()) return;
		}
		if (!this->pending_profile.empty()) {
			const std::string label = std::exchange(this->pending_profile, {});
			if (!this->ProfilePaused(label)) return;
		}
		if (!this->pending_vehicle_profile.empty()) {
			const std::string label = std::exchange(this->pending_vehicle_profile, {});
			if (!this->ProfileVisibleVehicle(label)) return;
		}
		if (this->phase == Phase::Start) {
			if (_game_mode != GameMode::Normal || _switch_mode != SwitchMode::None) { this->Schedule(); return; }
			Train *front = Train::GetIfValid(VehicleID{0});
			if (front == nullptr || front->owner != _local_company) { this->Fail("owned train 0 missing"); return; }
			this->id = front->index;
			this->source = PlanetManager::GetTileWorld(GateA());
			this->destination = PlanetManager::GetTileWorld(GateB());
			if (this->source == INVALID_WORLD || this->destination == INVALID_WORLD || this->source == this->destination) {
				this->Fail("local gate worlds missing"); return;
			}
			if (this->cold) {
				if (!this->cold_equal) { this->Fail("cold startup comparison did not execute before first callback"); return; }
				this->phase = Phase::Open;
			} else {
				if (!this->SetupFourCars()) return;
				this->phase = Phase::Warmup;
			}
			this->Schedule(); return;
		}
		if (this->phase == Phase::Warmup) {
			Train *front = this->Front();
			if (front == nullptr) { this->Fail("warmup lost train"); return; }
			if (CanShowCabView(front) && front->x_pos >= 18 * static_cast<int>(TILE_SIZE)) {
				IConsoleCmdExec("pause");
				this->phase = Phase::Open; this->Event("warmup_visible_ground"); this->Schedule(); return;
			}
			if (!this->Unpause() || !this->Advance()) return;
			++this->warmup_calls;
			if (this->pending_shot.empty()) this->Schedule();
			return;
		}
		if (this->phase == Phase::Open) {
			if (!_pause_mode.Test(PauseMode::Normal)) IConsoleCmdExec("pause");
			if (!CanShowCabView(this->Front())) { this->Fail("safe native cab open was rejected"); return; }
			this->paused_baseline = FrozenState(this->Front());
			ShowCabViewWindow(this->Front());
			if (this->Cab() == nullptr || !this->Pure()) { if (this->failure.empty()) this->Fail("native cab did not open"); return; }
			this->cab_opened = true;
			this->phase = this->cold ? Phase::Return : Phase::Outbound;
			this->check_pending_purity = true;
			this->Capture(this->cold ? "cold_reopened" : "initial_cab"); return;
		}
		if (this->phase == Phase::OutboundSafe) {
			if (!this->Pure()) return;
			const fs::path save = this->output / "gate-checkpoint.sav";
			if (SaveOrLoad(save.string(), SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) != SaveLoadResult::Ok) {
				this->Fail("native checkpoint save failed"); return;
			}
			std::ofstream(this->output / "gate-checkpoint.json") << FrozenState(this->Front()).dump(2) << '\n';
			this->Event("native_checkpoint_saved");
			this->phase = Phase::Return;
			this->Schedule(); return;
		}
		if (this->phase == Phase::Finish) {
			if (!this->Pure()) return;
			CloseWindowByClass(WindowClass::VehicleCabView);
			if (this->Cab() != nullptr) { this->Fail("cab failed to close after gate return"); return; }
			/* Native arrival news may be sliding along the bottom of the screen.
			 * Remove that GUI overlay once before both paired viewport captures. */
			CloseWindowByClass(WindowClass::News);
			ScrollMainWindowToTile(this->Front()->tile, true);
			this->phase = Phase::NormalBefore;
			this->Capture("normal_before_reopen");
			return;
		}
		if (this->phase == Phase::NormalBefore) {
			if (!this->Pure()) return;
			ShowCabViewWindow(this->Front());
			if (this->Cab() == nullptr) { this->Fail("cab failed recovery reopen"); return; }
			this->phase = Phase::ReopenForRecovery;
			this->Capture("reopened_for_recovery");
			return;
		}
		if (this->phase == Phase::ReopenForRecovery) {
			if (!this->Pure()) return;
			CloseWindowByClass(WindowClass::VehicleCabView);
			if (this->Cab() != nullptr) { this->Fail("cab failed recovery reclose"); return; }
			this->phase = Phase::NormalAfter;
			this->Capture("normal_after_reopen_close");
			return;
		}
		if (this->phase == Phase::NormalAfter) {
			if (!this->Pure()) return;
			this->saw_normal_recovery = true;
			this->finished = true;
			this->manifest["passed"] = true;
			this->manifest["explicit_state_game_loop_calls"] = this->total_calls;
			this->manifest["automatic_game_tick_counter"] = TimerGameTick::counter;
			this->manifest["base_graphics"] = BaseGraphics::GetUsedSet() == nullptr ? "" : BaseGraphics::GetUsedSet()->name;
			Json active_grfs = Json::array();
			for (const auto &config : _grfconfig) {
				if (config->status != GRFStatus::Activated) continue;
				active_grfs.push_back({{"grfid", FormatArrayAsHex(config->ident.grfid)},
					{"md5", FormatArrayAsHex(config->ident.md5sum)}, {"filename", config->filename},
					{"version", config->version}, {"system", config->flags.Test(GRFConfigFlag::System)}});
			}
			this->manifest["active_grfs"] = std::move(active_grfs);
			Blitter *blitter = BlitterFactory::GetCurrentBlitter();
			this->manifest["active_blitter"] = blitter == nullptr ? "" : std::string(blitter->GetName());
			this->manifest["active_blitter_depth"] = blitter == nullptr ? 0 : blitter->GetScreenDepth();
			this->Write();
			_exit_game = true;
			return;
		}
		if (this->phase == Phase::Outbound || this->phase == Phase::Return) {
			const unsigned index = this->phase == Phase::Outbound ? 0 : 1;
			if (!this->pass_started[index] && !this->StartPass(index)) return;
			if (_pause_mode.Test(PauseMode::Normal) && !this->Unpause()) return;
			/* Every explicit game tick is inspected. The native transit can last
			 * fewer ticks than a draw callback, so never batch unobserved ticks. */
			for (unsigned i = 0; i < 16 && this->pending_shot.empty() && this->phase != Phase::Finish && this->phase != Phase::OutboundSafe; ++i) {
				if (!this->Advance()) return;
				this->Observe();
				if (!this->failure.empty()) return;
			}
			if (this->pending_shot.empty()) this->Schedule();
			return;
		}
	}
};

GateNativeProof *active_gate_proof = nullptr;
bool StartGateProof(std::span<std::string_view> args)
{
	if (args.size() != 1 || active_gate_proof == nullptr || active_gate_proof->started) return false;
	active_gate_proof->started = true;
	if (active_gate_proof->cold && !active_gate_proof->CheckColdAtStartup()) return false;
	active_gate_proof->Schedule();
	return true;
}
} // namespace

TEST_CASE("Native SDL local gate cab round trip and cold reload", "[.][cab-view][gate-native]")
{
	const char *config = std::getenv("OSTTD_CAB_NATIVE_CONFIG");
	const char *save = std::getenv("OSTTD_CAB_NATIVE_SAVE");
	const char *output = std::getenv("OSTTD_CAB_NATIVE_OUTPUT");
	const char *binary = std::getenv("OSTTD_CAB_NATIVE_BINARY");
	const char *phase = std::getenv("OSTTD_CAB_GATE_PHASE");
	REQUIRE(config != nullptr); REQUIRE(save != nullptr); REQUIRE(output != nullptr); REQUIRE(binary != nullptr); REQUIRE(phase != nullptr);
	REQUIRE(fs::is_regular_file(config)); REQUIRE(fs::is_regular_file(save)); REQUIRE(fs::is_regular_file(binary));
	GateNativeProof proof;
	proof.output = fs::canonical(output);
	proof.cold = std::string_view(phase) == "cold";
	REQUIRE((proof.cold || std::string_view(phase) == "roundtrip"));
	if (proof.cold) {
		const char *expected = std::getenv("OSTTD_CAB_GATE_EXPECTED");
		REQUIRE(expected != nullptr);
		proof.expected = expected;
	}
	proof.manifest["phase"] = phase;
	proof.manifest["mixed_world_mask"] = Json::array();
	proof.manifest["mixed_render_proof"] = Json::array();
	proof.manifest["effect_captures"] = Json::array();
	proof.manifest["capture_frame_stats"] = Json::object();
	const char *profile = std::getenv("OSTTD_CAB_PROFILE");
	proof.profile = profile != nullptr && std::string_view(profile) == "1";
	const char *vehicle_profile = std::getenv("OSTTD_CAB_VEHICLE_PROFILE");
	proof.vehicle_profile = vehicle_profile != nullptr && std::string_view(vehicle_profile) == "1";
	const char *stage3c_diagnostic = std::getenv("OSTTD_CAB_STAGE3C_DIAGNOSTIC");
	proof.stage3c_diagnostic = stage3c_diagnostic != nullptr && std::string_view(stage3c_diagnostic) == "1";
	if (proof.stage3c_diagnostic) proof.manifest["stage3c_diagnostic"] = Json::object();
	const char *stage3d_paired = std::getenv("OSTTD_CAB_STAGE3D_PAIRED");
	proof.stage3d_paired = stage3d_paired != nullptr && std::string_view(stage3d_paired) == "1";
	if (proof.stage3d_paired) proof.manifest["stage3d_paired"] = Json::object();
	REQUIRE((!proof.vehicle_profile || proof.profile));
	if (proof.profile) {
		proof.manifest["profiles"] = Json::array();
		const char *rotation = std::getenv("OSTTD_CAB_PROFILE_ROTATION");
		if (rotation != nullptr && std::string_view(rotation).size() == 1 && rotation[0] >= '0' && rotation[0] <= '2') proof.profile_rotation = rotation[0] - '0';
		proof.manifest["mode_rotation"] = proof.profile_rotation;
	}
	if (proof.vehicle_profile) proof.manifest["vehicle_profiles"] = Json::array();
	active_gate_proof = &proof;
	IConsole::CmdRegister("cab_gate_native_start", StartGateProof);
	std::array<std::string, 14> arguments = {binary, "-v", "sdl", "-b", "32bpp-anim", "-s", "null", "-m", "null", "-r", "1280x800", "-x", "-c", config};
	std::vector<std::string_view> views;
	for (const std::string &argument : arguments) views.push_back(argument);
	views.push_back("-g"); views.push_back(save);
	std::mutex mutex;
	std::condition_variable condition;
	bool stopped = false;
	std::atomic<bool> timed_out{false};
	std::thread watchdog([&] {
		std::unique_lock lock(mutex);
		if (!condition.wait_for(lock, std::chrono::seconds(185), [&] { return stopped; })) {
			timed_out = true;
			_exit_game = true;
		}
	});
	const int exit_code = openttd_main(views);
	{
		std::lock_guard lock(mutex);
		stopped = true;
	}
	condition.notify_one(); watchdog.join();
	active_gate_proof = nullptr;
	if (!proof.finished || !proof.failure.empty()) proof.Write();
	INFO("Stage 2 native manifest: " << (proof.output / "gate-native-manifest.json"));
	INFO("Stage 2 native failure: " << proof.failure);
	CHECK_FALSE(timed_out); CHECK(exit_code == 0); CHECK(proof.started); CHECK(proof.finished);
	CHECK(proof.failure.empty()); CHECK(proof.cab_opened);
	CHECK(proof.approach[1]); CHECK(proof.near_approach[1]); CHECK(proof.hidden[1]); CHECK(proof.arrived[1]); CHECK(proof.mixed[1]);
	if (!proof.cold) {
		CHECK(proof.grew_consist); CHECK(proof.approach[0]); CHECK(proof.near_approach[0]); CHECK(proof.hidden[0]);
		CHECK(proof.arrived[0]); CHECK(proof.mixed[0]);
	} else {
		CHECK(proof.cold_equal);
	}
	CHECK(proof.saw_normal_recovery);
}
