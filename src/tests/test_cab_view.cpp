/* This file is part of OpenSpaceTTD. Distributed under GNU GPL version 2. */
/** @file test_cab_view.cpp Regression checks for the Stage 1 train cab view. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"
#include "../cab_view_3d.h"
#include "../cab_view.h"
#include "../viewport_func.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../direction_func.h"
#include "../tunnel_map.h"
#include "../bridge_map.h"
#include "../portal/portal_registry.h"
#include "../portal/planet_manager.h"
#include "../portal/stellar_network.h"
#include "../blitter/factory.hpp"
#include "../gfx_func.h"
#include "../spritecache.h"
#include "../palette_func.h"
#include "../gfxinit.h"
#include "../base_media_graphics.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../cargotype.h"
#include "../town.h"
#include "../newgrf_house.h"
#include "../road.h"
#include "../settings_type.h"
#include "../table/sprites.h"
#include "../core/random_func.hpp"
#include "../core/backup_type.hpp"
#include "../timer/timer_game_calendar.h"
#include "../vehicle_base.h"
#include "../train.h"
#include "../company_func.h"
#include "../openttd.h"
#include "../video/null_v.h"
#include "../driver.h"
#include "../fileio_func.h"
#include "../language.h"
#include "../fontcache.h"
#include "../gfx_layout.h"
#include "../strings_func.h"
#include "../table/strings.h"
#include "../widgets/cab_view_widget.h"
#include "../hotkeys.h"
#include "../safeguards.h"
#include "../window_func.h"
#include "../window_gui.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;

static void SetupCabMap()
{
	(void)MockEnvironment::Instance();
	_vehicle_pool.CleanPool();
	PortalRegistry::Reset();
	Map::Allocate(64, 64);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile) && TileX(tile) != 0 && TileY(tile) != 0) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}
	StellarNetwork::Reset();
	PlanetManager::Reset();
}

/** Physical heads and world regions, not a registry-only fictitious pair. */
static std::pair<TileIndex, TileIndex> SetupCabLocalGate()
{
	SetupCabMap();
	PlanetRegion a;
	a.id = WorldID{0};
	a.min_x = a.min_y = 1;
	a.max_x = 20;
	a.max_y = 40;
	PlanetRegion b;
	b.id = WorldID{1};
	b.min_x = 22;
	b.max_x = 42;
	b.min_y = 1;
	b.max_y = 40;
	REQUIRE(PlanetManager::RegisterRegion(a));
	REQUIRE(PlanetManager::RegisterRegion(b));
	StellarNetwork::RegisterWorld({WorldID{0}, "home", "Home", 0, 0, true});
	StellarNetwork::RegisterWorld({WorldID{1}, "other", "Other", 10, 0, true});
	const TileIndex entry = TileXY(10, 10);
	const TileIndex exit = TileXY(30, 10);
	MakeRailTunnel(entry, Owner{0}, DiagDirection::SW, RAILTYPE_BEGIN);
	MakeRailTunnel(exit, Owner{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterPortalPair(entry, DiagDirection::SW, WorldID{0},
		exit, DiagDirection::NE, WorldID{1}) != INVALID_PORTAL);
	return {entry, exit};
}

class CabTestVideoDriver final : public VideoDriver_Null {
public:
	std::optional<std::string_view> Start(const StringList &) override { return std::nullopt; }
	bool HasGUI() const override { return true; }
};

class CabTestVideoDriverFactory final : public DriverFactoryBase {
public:
	CabTestVideoDriverFactory() : DriverFactoryBase(Driver::Type::Video, 0, "cabtest", "Cab test video driver") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<CabTestVideoDriver>(); }
};

TEST_CASE("Cab entry rejects foreign nonprimary and unsupported train states", "[.][cab-view][gui]")
{
	SetupCabMap();
	CabTestVideoDriverFactory video_factory;
	DriverFactoryBase::SelectDriver("cabtest", Driver::Type::Video);
	Blitter *blitter = BlitterFactory::SelectBlitter("32bpp-simple");
	DrawPixelInfo old_screen = _screen;
	GameMode old_mode = _game_mode;
	SwitchMode old_switch = _switch_mode;
	CompanyID old_company = _local_company;
	_screen.width = _screen.height = 100;
	_game_mode = GameMode::Normal;
	_switch_mode = SwitchMode::None;
	_local_company = CompanyID{0};
	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	train->SetFrontEngine();
	train->owner = CompanyID{0};
	train->tile = TileXY(8, 8);
	train->x_pos = train->y_pos = 8 * TILE_SIZE + 8;
	train->direction = Direction::SW;
	train->track = Track::X;
	train->vehstatus.Reset(VehState::Hidden);
	if (blitter != nullptr) {
		CHECK(CanShowCabView(train));
		train->owner = CompanyID{1};
		CHECK_FALSE(CanShowCabView(train));
		train->owner = CompanyID{0};
		train->ClearFrontEngine();
		CHECK_FALSE(CanShowCabView(train));
		train->SetFrontEngine();
		train->tile = TileXY(0, 0);
		CHECK_FALSE(CanShowCabView(train));
		train->tile = TileXY(8, 8);
		_game_mode = GameMode::Menu;
		CHECK_FALSE(CanShowCabView(train));
		_game_mode = GameMode::Normal;
	}
	CHECK(blitter != nullptr);
	_vehicle_pool.CleanPool();
	_local_company = old_company;
	_game_mode = old_mode;
	_switch_mode = old_switch;
	_screen = old_screen;
	BlitterFactory::SelectBlitter("null");
	DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
}

TEST_CASE("Cab window is singleton and closes on session change or train deletion", "[.][cab-view][gui]")
{
	SetupCabMap();
	_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
	_valid_searchpaths = {Searchpath::BinaryDir};
	InitializeLanguagePacks();
	CabTestVideoDriverFactory video_factory;
	DriverFactoryBase::SelectDriver("cabtest", Driver::Type::Video);
	Blitter *blitter = BlitterFactory::SelectBlitter("32bpp-simple");
	REQUIRE(blitter != nullptr);
	const DrawPixelInfo old_screen = _screen;
	const GameMode old_mode = _game_mode;
	const SwitchMode old_switch = _switch_mode;
	const CompanyID old_company = _local_company;
	const auto old_start = _game_session_stats.start_time;
	_screen.width = _screen.pitch = 640;
	_screen.height = 480;
	ScreenSizeChanged();
	InitWindowSystem();
	_game_mode = GameMode::Normal;
	_switch_mode = SwitchMode::None;
	_local_company = CompanyID{0};
	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	train->SetFrontEngine();
	train->owner = CompanyID{0};
	train->tile = TileXY(8, 8);
	train->x_pos = train->y_pos = 8 * TILE_SIZE + 8;
	train->direction = Direction::SW;
	train->track = Track::X;
	train->vehstatus.Reset(VehState::Hidden);
	ShowCabViewWindow(train);
	Window *first = FindWindowById(WindowClass::VehicleCabView, train->index);
	REQUIRE(first != nullptr);
	CHECK(first->GetWidgetString(WID_CV_EFFECT, STR_NULL) == GetString(STR_CAB_VIEW_EFFECT_FULL));
	first->OnClick({0, 0}, WID_CV_EFFECT, 1);
	CHECK(first->GetWidgetString(WID_CV_EFFECT, STR_NULL) == GetString(STR_CAB_VIEW_EFFECT_REDUCED));
	first->OnClick({0, 0}, WID_CV_EFFECT, 1);
	CHECK(first->GetWidgetString(WID_CV_EFFECT, STR_NULL) == GetString(STR_CAB_VIEW_EFFECT_OFF));
	first->OnClick({0, 0}, WID_CV_EFFECT, 1);
	CHECK(first->GetWidgetString(WID_CV_EFFECT, STR_NULL) == GetString(STR_CAB_VIEW_EFFECT_FULL));

	/* Keyboard look controls act only while the cab window has focus. */
	REQUIRE(first->window_desc.hotkeys != nullptr);
	auto key = [&](uint16_t keycode) { return first->OnHotkey(first->window_desc.hotkeys->CheckMatch(keycode)); };
	REQUIRE(_focused_window == first);
	CHECK(key('A') == EventState::Handled);
	CHECK(GetCabViewLook(train->index).yaw == Approx(CabViewLook::KEY_STEP));
	CHECK(key('W') == EventState::Handled);
	CHECK(GetCabViewLook(train->index).pitch == Approx(CabViewLook::KEY_STEP));
	CHECK(key(WKC_EQUALS) == EventState::Handled);
	CHECK(GetCabViewLook(train->index).fov == CabViewLook::DEFAULT_FOV - CabViewLook::FOV_STEP);
	CHECK(key('B') == EventState::Handled);
	CHECK(GetCabViewLook(train->index).yaw == Approx(CabViewLook::PI));
	CHECK(key('E') == EventState::Handled);
	CHECK(first->GetWidgetString(WID_CV_EFFECT, STR_NULL) == GetString(STR_CAB_VIEW_EFFECT_REDUCED));
	CHECK(key('R') == EventState::Handled);
	CHECK(GetCabViewLook(train->index).yaw == 0);
	CHECK(GetCabViewLook(train->index).pitch == 0);
	CHECK(GetCabViewLook(train->index).fov == CabViewLook::DEFAULT_FOV - CabViewLook::FOV_STEP);
	SetFocusedWindow(nullptr);
	CHECK(key('A') == EventState::NotHandled);
	CHECK(GetCabViewLook(train->index).yaw == 0);
	SetFocusedWindow(first);

	/* The wheel zooms and a scroll event turns the view; the Look ahead button recentres it. */
	first->OnMouseWheel(1, WID_CV_VIEW);
	CHECK(GetCabViewLook(train->index).fov == CabViewLook::DEFAULT_FOV);
	first->OnMouseWheel(1, WID_CV_EFFECT);
	CHECK(GetCabViewLook(train->index).fov == CabViewLook::DEFAULT_FOV);
	first->OnScroll({-20, 0});
	CHECK(GetCabViewLook(train->index).yaw > 0);
	first->OnClick({0, 0}, WID_CV_AHEAD, 1);
	CHECK(GetCabViewLook(train->index).yaw == 0);

	/* Passing under a bridge keeps the window open and the player's look direction. */
	CHECK(key('A') == EventState::Handled);
	SetBridgeMiddle(train->tile, Axis::Y);
	first->OnRealtimeTick(0);
	CHECK(FindWindowById(WindowClass::VehicleCabView, train->index) == first);
	ClearBridgeMiddle(train->tile);
	first->OnRealtimeTick(0);
	CHECK(FindWindowById(WindowClass::VehicleCabView, train->index) == first);
	CHECK(GetCabViewLook(train->index).yaw == Approx(CabViewLook::KEY_STEP));
	first->OnClick({0, 0}, WID_CV_AHEAD, 1);
	CHECK(first->OnRightClick({0, 0}, WID_CV_VIEW) == (_settings_client.gui.scroll_mode != ViewportScrollMode::MapLMB));
	_scrolling_viewport = false;
	_cursor.fix_at = false;
	first->OnClick({0, 0}, WID_CV_EFFECT, 1);
	first->OnClick({0, 0}, WID_CV_EFFECT, 1);
	CHECK(first->GetWidgetString(WID_CV_EFFECT, STR_NULL) == GetString(STR_CAB_VIEW_EFFECT_FULL));
	ShowCabViewWindow(train);
	Window *second = FindWindowById(WindowClass::VehicleCabView, train->index);
	REQUIRE(second != nullptr);
	CHECK(second != first);
	_game_session_stats.start_time += std::chrono::milliseconds(1);
	second->OnRealtimeTick(0);
	CHECK(FindWindowById(WindowClass::VehicleCabView, train->index) == nullptr);
	_game_session_stats.start_time = old_start;
	ShowCabViewWindow(train);
	const VehicleID old_id = train->index;
	REQUIRE(FindWindowById(WindowClass::VehicleCabView, old_id) != nullptr);
	_vehicle_pool.CleanPool();
	CHECK(FindWindowById(WindowClass::VehicleCabView, old_id) == nullptr);
	REQUIRE(Vehicle::CanAllocateItem());
	Train *replacement = Vehicle::Create<Train>();
	REQUIRE(replacement->index == old_id);
	CHECK(FindWindowById(WindowClass::VehicleCabView, replacement->index) == nullptr);
	_vehicle_pool.CleanPool();
	UnInitWindowSystem();
	_game_session_stats.start_time = old_start;
	_local_company = old_company;
	_game_mode = old_mode;
	_switch_mode = old_switch;
	_screen = old_screen;
	DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
}

TEST_CASE("Cab view masks foreign closed and unregistered tiles", "[cab-view]")
{
	SetupCabMap();
	TileIndex home = TileXY(8, 8);
	TileIndex gap = TileXY(21, 8);
	CHECK(CabViewTileVisible(home, INVALID_WORLD)); // ordinary OpenTTD map
	CHECK_FALSE(CabViewTileVisible(TileXY(0, 0), INVALID_WORLD));

	PlanetRegion a;
	a.id = WorldID{0};
	a.min_x = a.min_y = 1;
	a.max_x = a.max_y = 20;
	PlanetRegion b;
	b.id = WorldID{1};
	b.min_x = 22;
	b.max_x = 40;
	b.min_y = 1;
	b.max_y = 20;
	REQUIRE(PlanetManager::RegisterRegion(a));
	REQUIRE(PlanetManager::RegisterRegion(b));
	StellarNetwork::RegisterWorld({WorldID{0}, "home", "Home", 0, 0, true});
	StellarNetwork::RegisterWorld({WorldID{1}, "closed", "Closed", 10, 0, false});
	CHECK(CabViewTileVisible(home, WorldID{0}));
	CHECK_FALSE(CabViewTileVisible(TileXY(24, 8), WorldID{0}));
	CHECK_FALSE(CabViewTileVisible(TileXY(24, 8), WorldID{1}));
	CHECK_FALSE(CabViewTileVisible(gap, WorldID{0}));
	CHECK_FALSE(CabViewTileVisible(gap, INVALID_WORLD));
	PlanetManager::Reset();
	StellarNetwork::Reset();
}

TEST_CASE("Cab local gate snapshots follow authoritative transit arrival and backing", "[cab-view][gate]")
{
	const auto [entry, exit] = SetupCabLocalGate();
	const GameMode old_mode = _game_mode;
	const SwitchMode old_switch = _switch_mode;
	const CompanyID old_company = _local_company;
	_game_mode = GameMode::Normal;
	_switch_mode = SwitchMode::None;
	_local_company = CompanyID{0};
	REQUIRE(CabViewLocalGateVisible(entry, WorldID{0}));
	REQUIRE(CabViewLocalGateVisible(exit, WorldID{1}));
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{1}));
	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	train->SetFrontEngine();
	train->owner = CompanyID{0};
	train->direction = Direction::SW;
	train->tile = TileXY(8, 10);
	train->x_pos = 8 * TILE_SIZE + 8;
	train->y_pos = 10 * TILE_SIZE + 8;
	train->z_pos = 0;
	train->track = Track::X;
	train->vehstatus.Reset(VehState::Hidden);
	const auto approach = ResolveCabViewPresentation(train, CompanyID{0});
	REQUIRE(approach.phase == CabViewPhase::Scene);
	CHECK(approach.moving == train->GetMovingDirection());
	train->tile = entry;
	train->x_pos = TileX(entry) * TILE_SIZE + 8;
	train->y_pos = TileY(entry) * TILE_SIZE + 8;
	const auto visible_head = ResolveCabViewPresentation(train, CompanyID{0});
	CHECK(visible_head.phase == CabViewPhase::Scene);
	train->track = Track::Wormhole;
	train->vehstatus.Set(VehState::Hidden);
	PortalRegistry::SetVehicleTransitProgress(train->index, 0);
	const auto transit = ResolveCabViewPresentation(train, CompanyID{0});
	CHECK(transit.phase == CabViewPhase::LocalTransit);
	CHECK(CabViewTransitionRequiresReset(visible_head, transit));
	CHECK(CabViewTransitionResetsLook(visible_head, transit));
	train->tile = exit;
	train->x_pos = TileX(exit) * TILE_SIZE + 8;
	train->y_pos = TileY(exit) * TILE_SIZE + 8;
	train->track = Track::X;
	train->vehstatus.Reset(VehState::Hidden);
	const auto arrival = ResolveCabViewPresentation(train, CompanyID{0});
	CHECK(arrival.phase == CabViewPhase::Scene);
	CHECK(arrival.world == WorldID{1});
	CHECK(CabViewTransitionRequiresReset(transit, arrival));
	CHECK(CabViewTransitionRequiresReset(visible_head, arrival)); // hidden phase missed between GUI samples
	CHECK(CabViewTransitionResetsLook(visible_head, arrival));
	train->vehicle_flags.Set(VehicleFlag::DrivingBackwards);
	const auto backing = ResolveCabViewPresentation(train, CompanyID{0});
	CHECK(backing.moving == ReverseDir(train->direction));
	CHECK(CabViewTransitionRequiresReset(arrival, backing));
	train->vehicle_flags.Reset(VehicleFlag::DrivingBackwards);
	train->direction = Direction::NE;
	const auto turned = ResolveCabViewPresentation(train, CompanyID{0});
	CHECK(CabViewTransitionRequiresReset(arrival, turned));
	train->direction = Direction::SW;
	train->z_pos += 1;
	CHECK(CabViewTransitionRequiresReset(arrival, ResolveCabViewPresentation(train, CompanyID{0})));
	train->z_pos = 0;
	train->x_pos += 2 * TILE_SIZE + 1;
	CHECK(CabViewTransitionRequiresReset(arrival, ResolveCabViewPresentation(train, CompanyID{0})));
	CHECK(CabViewTransitionResetsLook(arrival, ResolveCabViewPresentation(train, CompanyID{0})));
	train->x_pos = TileX(exit) * TILE_SIZE + 8;
	train->owner = CompanyID{1};
	CHECK(ResolveCabViewPresentation(train, CompanyID{0}).phase == CabViewPhase::Unsupported);
	train->owner = CompanyID{0};
	_local_company = COMPANY_SPECTATOR;
	CHECK(ResolveCabViewPresentation(train, CompanyID{0}).phase == CabViewPhase::Unsupported);
	_local_company = CompanyID{0};
	StellarNetwork::RegisterWorld({WorldID{0}, "home", "Home", 0, 0, true});
	StellarNetwork::RegisterWorld({WorldID{1}, "other", "Other", 10, 0, false});
	CHECK(ResolveCabViewPresentation(train, CompanyID{0}).phase == CabViewPhase::Unsupported);
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{0}));
	PortalRegistry::Reset();
	_vehicle_pool.CleanPool();
	PlanetManager::Reset();
	StellarNetwork::Reset();
	_local_company = old_company;
	_game_mode = old_mode;
	_switch_mode = old_switch;
}

TEST_CASE("Cab gate rejects stale unlinked ordinary and external heads", "[cab-view][gate]")
{
	const auto [entry, exit] = SetupCabLocalGate();
	CHECK(CabViewLocalGateVisible(entry, WorldID{0}));
	PortalRegistry::UnregisterPortalByTile(entry);
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{0}));
	REQUIRE(PortalRegistry::RegisterUnlinkedGate(entry, DiagDirection::SW, WorldID{0}));
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{0}));
	PortalRegistry::Reset();
	REQUIRE(PortalRegistry::RegisterPortalPair(entry, DiagDirection::SW, WorldID{0},
		exit, DiagDirection::NE, WorldID{1}) != INVALID_PORTAL);
	MakeClear(exit, ClearGround::Grass, 0);
	CHECK(PortalRegistry::ClassifyGate(entry).kind == PortalGateKind::LocalStale);
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{0}));
	PortalRegistry::Reset();
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{0})); // ordinary unmanaged tunnel head
	MakeRailTunnel(exit, Owner{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	REQUIRE(PortalRegistry::RegisterInterServerPortal(entry, DiagDirection::SW, WorldID{0}, WorldID{1}, 1) != INVALID_PORTAL);
	CHECK(PortalRegistry::ClassifyGate(entry).kind == PortalGateKind::ExternalLinked);
	CHECK_FALSE(CabViewLocalGateVisible(entry, WorldID{0}));
	PortalRegistry::Reset();
	PlanetManager::Reset();
	StellarNetwork::Reset();
}

TEST_CASE("Cab per-car mask checks both authoritative and coordinate worlds before sprites", "[cab-view][gate]")
{
	const auto [entry, exit] = SetupCabLocalGate();
	REQUIRE(Vehicle::CanAllocateItem());
	Train *car = Vehicle::Create<Train>();
	car->tile = TileXY(8, 10);
	car->x_pos = 8 * TILE_SIZE + 8;
	car->y_pos = 10 * TILE_SIZE + 8;
	car->track = Track::X;
	car->vehstatus.Reset(VehState::Hidden);
	CHECK(CabViewVehicleVisible(car, WorldID{0}));
	CHECK_FALSE(CabViewVehicleVisible(car, WorldID{1}));
	car->tile = exit;
	CHECK_FALSE(CabViewVehicleVisible(car, WorldID{0}));
	CHECK_FALSE(CabViewVehicleVisible(car, WorldID{1})); // coordinates still on world 0
	car->x_pos = TileX(exit) * TILE_SIZE + 8;
	car->y_pos = TileY(exit) * TILE_SIZE + 8;
	CHECK(CabViewVehicleVisible(car, WorldID{1}));
	car->vehstatus.Set(VehState::Hidden);
	CHECK_FALSE(CabViewVehicleVisible(car, WorldID{1}));
	car->vehstatus.Reset(VehState::Hidden);
	car->track = Track::Wormhole;
	car->tile = entry;
	CHECK_FALSE(CabViewVehicleVisible(car, WorldID{0}));
	StellarNetwork::RegisterWorld({WorldID{1}, "other", "Other", 10, 0, false});
	CHECK_FALSE(CabViewVehicleVisible(car, WorldID{1}));
	_vehicle_pool.CleanPool();
	PortalRegistry::Reset();
	PlanetManager::Reset();
	StellarNetwork::Reset();
}

TEST_CASE("Cab sprite collection preserves pending viewport state", "[cab-view]")
{
	SetupCabMap();
	CHECK(CabViewCollectorPreservesPendingStateForTest(TileXY(8, 8)));
	std::vector<GroundSpriteToDraw> ground;
	CollectTileGroundSprites(INVALID_TILE, ground);
	CHECK(ground.empty());
}

TEST_CASE("Cab waits on and under ordinary bridges instead of closing", "[cab-view]")
{
	SetupCabMap();
	AutoRestoreBackup mode(_game_mode, GameMode::Normal);
	AutoRestoreBackup switch_mode(_switch_mode, SwitchMode::None);
	AutoRestoreBackup company(_local_company, CompanyID{0});
	REQUIRE(Vehicle::CanAllocateItem());
	Train *train = Vehicle::Create<Train>();
	train->SetFrontEngine();
	train->owner = CompanyID{0};
	train->tile = TileXY(8, 8);
	train->x_pos = train->y_pos = 8 * TILE_SIZE + 8;
	train->direction = Direction::SW;
	train->track = Track::X;
	train->vehstatus.Reset(VehState::Hidden);
	const CabViewPresentation open_line = ResolveCabViewPresentation(train, CompanyID{0});
	REQUIRE(open_line.phase == CabViewPhase::Scene);

	/* An ordinary bridge along Y passes over the train's tile. */
	MakeRailBridgeRamp(TileXY(8, 6), Owner{0}, 0, DiagDirection::SE, RAILTYPE_BEGIN);
	MakeRailBridgeRamp(TileXY(8, 10), Owner{0}, 0, DiagDirection::NW, RAILTYPE_BEGIN);
	for (uint y = 7; y <= 9; ++y) SetBridgeMiddle(TileXY(8, y), Axis::Y);
	const CabViewPresentation under = ResolveCabViewPresentation(train, CompanyID{0});
	CHECK(under.phase == CabViewPhase::Bridge);
	CHECK(under.world == open_line.world);
	CHECK(CabViewTransitionRequiresReset(open_line, under));
	CHECK_FALSE(CabViewTransitionResetsLook(open_line, under));
	CHECK_FALSE(CabViewTransitionResetsLook(under, open_line));

	/* On the ramp, then on the deck inside the bridge's wormhole. */
	train->tile = TileXY(8, 6);
	train->x_pos = 8 * TILE_SIZE + 8;
	train->y_pos = 6 * TILE_SIZE + 8;
	train->direction = Direction::SE;
	train->track = Track::Y;
	CHECK(ResolveCabViewPresentation(train, CompanyID{0}).phase == CabViewPhase::Bridge);
	train->track = Track::Wormhole;
	train->y_pos = 7 * TILE_SIZE + 8;
	CHECK(ResolveCabViewPresentation(train, CompanyID{0}).phase == CabViewPhase::Bridge);

	/* A hidden train is not on a bridge: ordinary tunnels still close the view. */
	train->vehstatus.Set(VehState::Hidden);
	CHECK(ResolveCabViewPresentation(train, CompanyID{0}).phase == CabViewPhase::Unsupported);
	_vehicle_pool.CleanPool();
}

TEST_CASE("Cab look turns wraps clamps and zooms locally", "[cab-view]")
{
	CabViewLook look;
	CHECK(look.fov == CabViewLook::DEFAULT_FOV);

	/* Dragging the pointer right by one view width (scroll delta -width) turns left by the field of view. */
	look.Scroll(-400, 0, 400);
	CHECK(look.yaw == Approx(CabViewLook::DEFAULT_FOV * CabViewLook::PI / 180.0f));
	/* Dragging up (scroll delta +y) pulls the scene up, so the view tilts down. */
	look.Scroll(0, 40, 400);
	CHECK(look.pitch < 0);

	/* Yaw wraps into (-pi, pi]; pitch stops at the limit. */
	look.Turn(CabViewLook::PI, 0);
	CHECK(look.yaw > -CabViewLook::PI);
	CHECK(look.yaw <= CabViewLook::PI);
	CHECK(look.yaw == Approx(CabViewLook::DEFAULT_FOV * CabViewLook::PI / 180.0f - CabViewLook::PI));
	look.Turn(0, -10);
	CHECK(look.pitch == -CabViewLook::MAX_PITCH);
	look.Turn(0, 20);
	CHECK(look.pitch == CabViewLook::MAX_PITCH);

	/* Wheel notches zoom like the map and stop at the limits. */
	look.Zoom(-1);
	CHECK(look.fov == CabViewLook::DEFAULT_FOV - CabViewLook::FOV_STEP);
	look.Zoom(-100);
	CHECK(look.fov == CabViewLook::MIN_FOV);
	look.Zoom(100);
	CHECK(look.fov == CabViewLook::MAX_FOV);

	/* A wider view turns further per pixel, so the scene still follows the pointer. */
	CabViewLook wide;
	wide.Zoom(2);
	CabViewLook narrow;
	narrow.Zoom(-2);
	wide.Scroll(-10, 0, 400);
	narrow.Scroll(-10, 0, 400);
	CHECK(wide.yaw > narrow.yaw);

	/* Looking ahead again recentres the direction but keeps the zoom. */
	look.ResetDirection();
	CHECK(look.yaw == 0);
	CHECK(look.pitch == 0);
	CHECK(look.fov == CabViewLook::MAX_FOV);

	/* The renderer's frame cache must notice a zoom change. */
	CabViewCamera camera;
	CabViewCamera zoomed = camera;
	zoomed.fov = look.fov;
	CHECK_FALSE(camera == zoomed);
}

TEST_CASE("Cab effect clock handles paint-first tick-first and missed-transit arrival", "[cab-view]")
{
	CabViewPresentation source, hidden, destination;
	source.phase = destination.phase = CabViewPhase::Scene;
	source.world = hidden.world = WorldID{0};
	destination.world = WorldID{1};
	hidden.phase = CabViewPhase::LocalTransit;
	CabViewEffectTimeline paint_first;
	paint_first.Observe({}, source, false, false);
	paint_first.Advance(5000); // initial callback cannot age a newly observed phase
	paint_first.Advance(1000);
	paint_first.Observe(source, hidden, true, true);
	paint_first.Advance(1000);
	CHECK(paint_first.PhaseElapsed() == 0);
	paint_first.Advance(90);
	CHECK(paint_first.PhaseElapsed() == 90);
	paint_first.Observe(hidden, destination, true, true); // paint discovered arrival
	CHECK(paint_first.ArrivalCue());
	CHECK(paint_first.PhaseElapsed() == 0);
	paint_first.Advance(1000); // pending old-phase callback cannot skip settling
	CHECK(paint_first.ArrivalCue());
	paint_first.Advance(249);
	CHECK(paint_first.ArrivalCue());
	paint_first.Advance(1);
	CHECK_FALSE(paint_first.ArrivalCue());
	CabViewEffectTimeline tick_first;
	tick_first.Observe({}, source, false, false);
	tick_first.Advance(0);
	tick_first.Advance(3000);
	tick_first.Observe(source, destination, true, true); // transit fell between observations
	tick_first.Advance(900);
	CHECK(tick_first.ArrivalCue());
	CHECK(tick_first.PhaseElapsed() == 0);
	tick_first.Advance(80);
	CHECK(tick_first.ArrivalCue());
	tick_first.CancelCue(); // changing mode never replays an arrival
	CHECK_FALSE(tick_first.ArrivalCue());
}

TEST_CASE("Cab CPU renderer sky output resizes without simulation mutation", "[.][cab-view][gui]")
{
	SetupCabMap();
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) MakeVoid(tile);
	DrawPixelInfo *old_dpi = _cur_dpi;
	const auto old_random = _random;
	const auto old_date = TimerGameCalendar::date;
	Blitter *blitter = BlitterFactory::SelectBlitter("32bpp-simple");
	if (blitter != nullptr) {
		CabViewCamera camera;
		camera.x = camera.y = 8 * TILE_SIZE;
		camera.z = 8;
		camera.world = INVALID_WORLD;
		for (auto [w, h] : {std::pair{64, 40}, std::pair{72, 32}}) {
			std::vector<Colour> pixels(static_cast<size_t>(w) * h);
			DrawPixelInfo dpi{pixels.data(), 0, 0, w, h, w, ZoomLevel::Min};
			{
				AutoRestoreBackup screen_backup(_screen, dpi);
				AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
				AutoRestoreBackup anim_backup(_screen_disable_anim, true);
				DrawCabView3D({0, 0, w - 1, h - 1}, camera, static_cast<uint32_t>(w), 0);
			}
			CHECK(pixels.front().data != pixels.back().data);
			CHECK(pixels.front().data != 0);
		}
	}
	Blitter *palette_blitter = BlitterFactory::SelectBlitter("8bpp-simple");
	if (palette_blitter != nullptr) {
		constexpr int width = 64, height = 40;
		std::vector<uint8_t> pixels(width * height, 0xEE);
		DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
		CabViewCamera camera;
		camera.x = camera.y = 8 * TILE_SIZE;
		camera.z = 8;
		{
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			ResetCabView3D();
			DrawCabView3D({0, 0, width - 1, height - 1}, camera, 3, 0);
		}
		CHECK(std::ranges::any_of(pixels, [](uint8_t pixel) { return pixel != 0xEE; }));
		const auto off = pixels;
		camera.effect_mode = CabViewEffectMode::Full;
		camera.scene_effect = CabViewSceneEffect::Arrival;
		camera.effect_elapsed_ms = 0;
		{
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			DrawCabView3D({0, 0, width - 1, height - 1}, camera, 4, 0);
		}
		CHECK(pixels != off);
		camera.effect_elapsed_ms = 250;
		{
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			DrawCabView3D({0, 0, width - 1, height - 1}, camera, 5, 0);
		}
		CHECK(pixels == off);
	}
	_cur_dpi = old_dpi;
	ResetCabView3D();
	BlitterFactory::SelectBlitter("null");
	CHECK(blitter != nullptr);
	CHECK(palette_blitter != nullptr);
	CHECK(_random.state[0] == old_random.state[0]);
	CHECK(_random.state[1] == old_random.state[1]);
	CHECK(TimerGameCalendar::date == old_date);
	CHECK(IsTileType(TileXY(8, 8), TileType::Void));
}

TEST_CASE("Cab CPU renderer draws local terrain but masks foreign terrain and vehicles", "[.][cab-view][scene]")
{
	SetupCabMap();
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) MakeVoid(tile);
	_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
	_valid_searchpaths = {Searchpath::BinaryDir};
	InitializeLanguagePacks();
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
	SetupCargoForClimate(LandscapeType::Temperate);
	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	ResetHouses();
	InitializeBuildingCounts();
	ResetRoadTypes();
	/* The standard mock sprite cache has no backing SpriteFile. The CPU cab
	 * decoder intentionally reads raw graphics, so use the already installed
	 * base set in this isolated process instead of feeding it mock entries. */
	extern void DetermineBasePaths(std::string_view exe);
	const std::string test_binary = std::filesystem::absolute("openttd_test").string();
	DetermineBasePaths(test_binary);
	_valid_searchpaths.clear();
	for (Searchpath sp : EnumRange(Searchpath::End)) {
		if (!_searchpaths[sp].empty()) _valid_searchpaths.push_back(sp);
	}
	TarScanner::DoScan(TarScanner::Mode::Baseset);
	BaseGraphics::FindSets();
	REQUIRE(BaseGraphics::SetSet(nullptr));
	CabTestVideoDriverFactory video_factory;
	DriverFactoryBase::SelectDriver("cabtest", Driver::Type::Video);
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
	GfxLoadSprites();
	/* The simple blitter downsamples only its root sprite; its Normal image is
	 * not the Normal source decoded by the cab. The optimized native blitter
	 * encodes each zoom variant, so this is a like-for-like draw oracle. */
	Blitter *blitter = BlitterFactory::SelectBlitter("32bpp-optimized");
	REQUIRE(blitter != nullptr);
	GfxClearSpriteCache(); // retain base-file locations, re-encode under the selected blitter
	/* MockEnvironment installs a width-only font whose GetGlyph returns null.
	 * Native DrawString needs the sprite font backed by the loaded base set. */
	for (FontSize fs : EnumRange(FontSize::End)) Layouter::ResetFontCache(fs);
	FontCache::UninitializeFontCaches();
	FontCache::InitializeFontCaches();
	/* The installed base graphics give the native viewport and the cab's raw
	 * decoder the same actual source sprites. Compare the ordered compositor
	 * before mip generation against VehicleSpriteSeq::Draw at Normal zoom. */
	VehicleSpriteSeq two{};
	two.count = 2;
	two.seq[0] = {SPR_ARROW_LEFT, PAL_NONE};
	two.seq[1] = {SPR_ARROW_RIGHT, PALETTE_TO_RED};
	VehicleSpriteSeq eight{};
	eight.count = 8;
	const std::array<SpriteID, 4> images{SPR_ARROW_LEFT, SPR_ARROW_RIGHT, SPR_BOX_EMPTY, SPR_BOX_CHECKED};
	for (uint i = 0; i < eight.count; ++i) eight.seq[i] = {images[i % images.size()], i % 2 == 0 ? PAL_NONE : PALETTE_TO_BLUE};
	const char *sprite_output = std::getenv("OSTTD_CAB_SPRITE_OUTPUT");
	const std::filesystem::path sprite_dir = sprite_output != nullptr ? std::filesystem::path(sprite_output) : std::filesystem::path();
	if (sprite_output != nullptr) REQUIRE(std::filesystem::is_directory(sprite_dir));
	std::vector<std::string> sprite_metrics;
	auto write_ppm = [&](const std::string &name, const std::vector<Colour> &pixels, int w, int h) {
		if (sprite_output == nullptr) return;
		const std::filesystem::path file = sprite_dir / name;
		REQUIRE_FALSE(std::filesystem::exists(file));
		std::ofstream out(file, std::ios::binary);
		REQUIRE(out.good());
		out << "P6\n" << w << ' ' << h << "\n255\n";
		for (Colour c : pixels) {
			const char rgb[3]{static_cast<char>(c.r), static_cast<char>(c.g), static_cast<char>(c.b)};
			out.write(rgb, 3);
		}
		REQUIRE(out.good());
	};
	auto verify_layers = [&](const VehicleSpriteSeq &seq, std::string_view label, PaletteID default_pal, bool force_pal) {
		const auto probe = ProbeCabViewVehicleComposite(seq, default_pal, force_pal);
		REQUIRE(probe.width > 0);
		REQUIRE(probe.height > 0);
		REQUIRE(probe.pixels.size() == static_cast<size_t>(probe.width) * probe.height);
		constexpr int w = 256, h = 192, anchor_x = 128, anchor_y = 96;
		REQUIRE(anchor_x + probe.left >= 0);
		REQUIRE(anchor_y + probe.top >= 0);
		REQUIRE(anchor_x + probe.left + probe.width <= w);
		REQUIRE(anchor_y + probe.top + probe.height <= h);
		auto native = [&](Colour background) {
			std::vector<Colour> pixels(static_cast<size_t>(w) * h, background);
			DrawPixelInfo dpi{pixels.data(), 0, 0, w, h, w, ZoomLevel::Normal};
			{
				AutoRestoreBackup screen_backup(_screen, dpi);
				AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
				AutoRestoreBackup anim_backup(_screen_disable_anim, true);
				AutoRestoreBackup zoom_backup(_gui_zoom, ZoomLevel::Normal);
				seq.Draw(anchor_x, anchor_y, default_pal, force_pal);
			}
			return pixels;
		};
		const auto black = native(Colour(0, 0, 0));
		const auto white = native(Colour(255, 255, 255));
		std::vector<Colour> composition(static_cast<size_t>(w) * h, Colour(0, 0, 0));
		std::vector<Colour> expected_white(static_cast<size_t>(w) * h, Colour(255, 255, 255));
		int max_error = 0;
		uint covered_pixels = 0;
		for (int y = 0; y < probe.height; ++y) for (int x = 0; x < probe.width; ++x) {
			const auto &p = probe.pixels[static_cast<size_t>(y) * probe.width + x];
			const size_t dst = static_cast<size_t>(anchor_y + probe.top + y) * w + anchor_x + probe.left + x;
			auto channel = [&](float value) { return static_cast<int>(std::lround(std::clamp(value, 0.0f, 255.0f))); };
			composition[dst] = Colour(channel(p.r), channel(p.g), channel(p.b));
			expected_white[dst] = Colour(channel(p.r + (1 - p.a) * 255), channel(p.g + (1 - p.a) * 255), channel(p.b + (1 - p.a) * 255));
			if (p.a > 0) ++covered_pixels;
		}
		for (size_t i = 0; i < black.size(); ++i) {
			const std::array<int, 3> actual_black{black[i].r, black[i].g, black[i].b};
			const std::array<int, 3> actual_white{white[i].r, white[i].g, white[i].b};
			const std::array<int, 3> expect_black{composition[i].r, composition[i].g, composition[i].b};
			const std::array<int, 3> expect_white{expected_white[i].r, expected_white[i].g, expected_white[i].b};
			for (int k = 0; k < 3; ++k) max_error = std::max({max_error, std::abs(actual_black[k] - expect_black[k]), std::abs(actual_white[k] - expect_white[k])});
		}
		INFO(label << " native oracle maximum channel error " << max_error);
		CHECK(covered_pixels > 0);
		CHECK(max_error <= 1);
		const std::string stem(label);
		write_ppm(stem + "-native-black.ppm", black, w, h);
		write_ppm(stem + "-native-white.ppm", white, w, h);
		write_ppm(stem + "-cab-composite.ppm", composition, w, h);
		const auto timed = ProfileCabViewVehicleTexture(seq, default_pal, force_pal, true, 100);
		CHECK(timed.warm_lookups == 100);
		CHECK(timed.warm_texture_stable);
		CHECK(timed.cache.vehicle_textures == 1);
		CHECK(timed.cache.vehicle_texture_payload_bytes > 0);
		CHECK(timed.cache.vehicle_texture_payload_bytes <= timed.cache.texture_payload_bytes);
		CHECK(timed.cache_after_cold.vehicle_textures == timed.cache.vehicle_textures);
		CHECK(timed.cache_after_cold.vehicle_texture_payload_bytes == timed.cache.vehicle_texture_payload_bytes);
		std::string entries;
		for (uint i = 0; i < seq.count; ++i) {
			const PaletteID effective = force_pal || seq.seq[i].pal == 0 ? default_pal : seq.seq[i].pal;
			entries += fmt::format("{}{{\"sprite_id\":{},\"palette_id\":{},\"effective_palette_id\":{}}}",
				i == 0 ? "" : ",", seq.seq[i].sprite, seq.seq[i].pal, effective);
		}
		sprite_metrics.push_back(fmt::format("{{\"case\":\"{}\",\"layers\":{},\"entries\":[{}],\"zoom\":\"normal\",\"max_channel_error\":{},\"cold_build_ns\":{},\"warm_lookup_total_ns\":{},\"warm_lookups\":{},\"vehicle_payload_bytes\":{}}}",
			label, seq.count, entries, max_error, timed.cold_build_ns, timed.warm_lookup_total_ns, timed.warm_lookups, timed.cache.vehicle_texture_payload_bytes));
	};
	verify_layers(two, "two", PAL_NONE, false);
	verify_layers(eight, "eight", PAL_NONE, false);
	/* Find a recolourable sprite among the installed original train images;
	 * UI arrows alone need not contain any company-colour pixels. */
	SpriteID recolourable_train = 0;
	auto visibly_different = [&](const CabViewVehicleCompositeProbe &a, const CabViewVehicleCompositeProbe &b) {
		if (a.width != b.width || a.height != b.height || a.left != b.left || a.top != b.top) return true;
		for (size_t i = 0; i < a.pixels.size(); ++i) {
			if (std::abs(a.pixels[i].r - b.pixels[i].r) > 0.5f || std::abs(a.pixels[i].g - b.pixels[i].g) > 0.5f ||
				std::abs(a.pixels[i].b - b.pixels[i].b) > 0.5f) return true;
		}
		return false;
	};
	for (SpriteID id : {0x0B59U, 0x0B61U, 0x0B69U, 0x0BE1U, 0x0B71U}) {
		VehicleSpriteSeq one{};
		one.Set(id);
		if (visibly_different(ProbeCabViewVehicleComposite(one, PAL_NONE, false),
			ProbeCabViewVehicleComposite(one, PALETTE_TO_BLUE, false))) { recolourable_train = id; break; }
	}
	REQUIRE(recolourable_train != 0);
	VehicleSpriteSeq mixed_palette{};
	mixed_palette.count = 2;
	mixed_palette.seq[0] = {SPR_ARROW_LEFT, PALETTE_TO_RED}; // independent layer override
	mixed_palette.seq[1] = {recolourable_train, PAL_NONE}; // nonzero default must be used
	CHECK(visibly_different(ProbeCabViewVehicleComposite(mixed_palette, PAL_NONE, false),
		ProbeCabViewVehicleComposite(mixed_palette, PALETTE_TO_BLUE, false)));
	verify_layers(mixed_palette, "mixed-fallback", PALETTE_TO_BLUE, false);
	VehicleSpriteSeq red_over_blue{};
	red_over_blue.count = 2;
	red_over_blue.seq[0] = {recolourable_train, PALETTE_TO_BLUE};
	red_over_blue.seq[1] = {recolourable_train, PALETTE_TO_RED};
	VehicleSpriteSeq blue_over_red = red_over_blue;
	std::swap(blue_over_red.seq[0], blue_over_red.seq[1]);
	CHECK(visibly_different(ProbeCabViewVehicleComposite(red_over_blue, PAL_NONE, false),
		ProbeCabViewVehicleComposite(blue_over_red, PAL_NONE, false)));
	verify_layers(red_over_blue, "red-over-blue", PAL_NONE, false);
	verify_layers(blue_over_red, "blue-over-red", PAL_NONE, false);
	verify_layers(two, "crash", PALETTE_CRASH, true);
	verify_layers(two, "black", PALETTE_ALL_BLACK, true);
	VehicleSpriteSeq reversed = two;
	std::swap(reversed.seq[0], reversed.seq[1]);
	const auto ordered_key = ProbeCabViewVehicleCacheIdentity(two, reversed, PAL_NONE, false, true, true);
	CHECK(ordered_key.repeated_key_reused);
	CHECK(ordered_key.variant_key_distinct);
	CHECK(ordered_key.after_first.vehicle_textures == 1);
	CHECK(ordered_key.after_repeat.vehicle_textures == 1);
	CHECK(ordered_key.after_repeat.vehicle_texture_payload_bytes == ordered_key.after_first.vehicle_texture_payload_bytes);
	CHECK(ordered_key.after_variant.vehicle_textures == 2);
	const auto widened_key = ProbeCabViewVehicleCacheIdentity(two, two, PAL_NONE, false, true, false);
	CHECK(widened_key.variant_key_distinct);
	CHECK(widened_key.after_variant.vehicle_textures == 2);
	VehicleSpriteSeq recoloured = two;
	recoloured.seq[1].pal = PALETTE_TO_BLUE;
	const auto palette_key = ProbeCabViewVehicleCacheIdentity(two, recoloured, PAL_NONE, false, true, true);
	CHECK(palette_key.variant_key_distinct);
	CHECK(palette_key.after_variant.vehicle_textures == 2);
	CHECK(CabViewVehicleUnionSupported(-256, -128, 0, 128));
	CHECK_FALSE(CabViewVehicleUnionSupported(-257, -128, 256, 128));
	CHECK_FALSE(CabViewVehicleUnionSupported(0, 0, 257, 256));
	CHECK_FALSE(CabViewVehicleUnionSupported(INT64_MIN, 0, INT64_MAX, 1));
	CHECK_FALSE(CabViewVehicleUnionSupported(0, 0, 0, 1));
	VehicleSpriteSeq unsupported = two;
	unsupported.seq[1].sprite |= 1U << PALETTE_MODIFIER_TRANSPARENT;
	const auto partly_supported = ProbeCabViewVehicleComposite(unsupported, PAL_NONE, false);
	CHECK(partly_supported.skipped_destination_layers == 1);
	CHECK(partly_supported.width > 0);
	unsupported.seq[0].sprite |= 1U << PALETTE_MODIFIER_TRANSPARENT;
	const auto all_unsupported = ProbeCabViewVehicleComposite(unsupported, PAL_NONE, false);
	CHECK(all_unsupported.skipped_destination_layers == 2);
	CHECK(all_unsupported.pixels.empty());
	unsupported.count = 9;
	CHECK(ProbeCabViewVehicleComposite(unsupported, PAL_NONE, false).rejected_oversize);
	const auto source = ProbeCabViewVehiclePixel(91, 73, 42, 128, 0, PAL_NONE);
	CHECK(source.a == Approx(128.0f / 255.0f));
	CHECK(source.r == Approx(91.0f * 128.0f / 255.0f));
	const auto black_pixel = ProbeCabViewVehiclePixel(91, 73, 42, 128, 0, PALETTE_ALL_BLACK);
	CHECK(black_pixel.a == 1.0f);
	CHECK(black_pixel.r == 0.0f);
	const auto crash_pixel = ProbeCabViewVehiclePixel(91, 73, 42, 128, 0, PALETTE_CRASH);
	CHECK(crash_pixel.r == crash_pixel.g);
	CHECK(crash_pixel.g == crash_pixel.b);
	CHECK(crash_pixel.r < source.r);
	CHECK(crash_pixel.a == source.a);
	const auto clear_pixel = ProbeCabViewVehiclePixel(91, 73, 42, 0, 0, PALETTE_ALL_BLACK);
	CHECK(clear_pixel.a == 0.0f);
	uint8_t palette_index = 0;
	for (int i = 1; i < 256; ++i) {
		const Colour c = _cur_palette.palette[i];
		if (c.r > 100 && c.g > 10 && c.b > 10) { palette_index = static_cast<uint8_t>(i); break; }
	}
	REQUIRE(palette_index != 0);
	const auto indexed_pixel = ProbeCabViewVehiclePixel(0, 0, 0, 255, palette_index, PAL_NONE);
	CHECK(indexed_pixel.a == 1.0f);
	const Colour indexed_expected = AdjustBrightness(_cur_palette.palette[palette_index], GetColourBrightness(Colour(0, 0, 0)));
	CHECK(indexed_pixel.r == Approx(indexed_expected.r)); // indexed source without explicit recolour still uses palette brightness
	CHECK(indexed_pixel.g == Approx(indexed_expected.g));
	CHECK(indexed_pixel.b == Approx(indexed_expected.b));
	auto indexed_brightness = [&](uint8_t brightness) {
		const auto actual = ProbeCabViewVehiclePixel(brightness, brightness / 2, brightness / 4, 128, palette_index, PAL_NONE);
		const Colour expected = AdjustBrightness(_cur_palette.palette[palette_index], brightness);
		CHECK(actual.a == Approx(128.0f / 255.0f));
		CHECK(actual.r == Approx(expected.r * actual.a));
		CHECK(actual.g == Approx(expected.g * actual.a));
		CHECK(actual.b == Approx(expected.b * actual.a));
		return actual;
	};
	const auto indexed_dim = indexed_brightness(64);
	const auto indexed_bright = indexed_brightness(200);
	CHECK((indexed_dim.r != indexed_bright.r || indexed_dim.g != indexed_bright.g || indexed_dim.b != indexed_bright.b));
	const uint8_t *red_remap = GetNonSprite(PALETTE_TO_RED, SpriteType::Recolour) + 1;
	uint8_t nonzero_mapped = 0;
	for (int i = 1; i < 256; ++i) {
		if (red_remap[i] != 0 && red_remap[i] != i && nonzero_mapped == 0) nonzero_mapped = static_cast<uint8_t>(i);
	}
	REQUIRE(nonzero_mapped != 0);
	const auto remapped_pixel = ProbeCabViewVehiclePixel(0, 0, 0, 255, nonzero_mapped, PALETTE_TO_RED);
	CHECK(remapped_pixel.a == 1.0f);
	const Colour remapped_expected = AdjustBrightness(_cur_palette.palette[red_remap[nonzero_mapped]], GetColourBrightness(Colour(0, 0, 0)));
	CHECK(remapped_pixel.r == Approx(remapped_expected.r));
	CHECK(remapped_pixel.g == Approx(remapped_expected.g));
	CHECK(remapped_pixel.b == Approx(remapped_expected.b));
	auto remapped_brightness = [&](uint8_t brightness) {
		const auto actual = ProbeCabViewVehiclePixel(brightness, brightness / 2, brightness / 4, 128, nonzero_mapped, PALETTE_TO_RED);
		const Colour expected = AdjustBrightness(_cur_palette.palette[red_remap[nonzero_mapped]], brightness);
		CHECK(actual.a == Approx(128.0f / 255.0f));
		CHECK(actual.r == Approx(expected.r * actual.a));
		CHECK(actual.g == Approx(expected.g * actual.a));
		CHECK(actual.b == Approx(expected.b * actual.a));
		return actual;
	};
	const auto remapped_dim = remapped_brightness(64);
	const auto remapped_bright = remapped_brightness(200);
	CHECK((remapped_dim.r != remapped_bright.r || remapped_dim.g != remapped_bright.g || remapped_dim.b != remapped_bright.b));
	std::array<uint8_t, 256> zero_remap{};
	zero_remap[nonzero_mapped] = 0;
	CHECK(ProbeCabViewVehiclePixel(0, 0, 0, 255, nonzero_mapped, PALETTE_TO_RED, zero_remap.data()).a == 0.0f);
	if (sprite_output != nullptr) {
		const std::filesystem::path file = sprite_dir / "sprite-proof.json";
		REQUIRE_FALSE(std::filesystem::exists(file));
		std::ofstream out(file);
		REQUIRE(out.good());
		out << "{\"base_graphics\":\"" << BaseGraphics::GetUsedSet()->name << "\",\"blitter\":\"" << blitter->GetName()
			<< "\",\"palette_source\":\"_cur_palette\",\"max_channel_error_allowed\":1,\"order_positive_control\":true,\"cases\":[";
		for (size_t i = 0; i < sprite_metrics.size(); ++i) out << (i == 0 ? "" : ",") << sprite_metrics[i];
		out << "]}\n";
		REQUIRE(out.good());
	}

	/* The shared opaque transit path must never touch the pixels around a
	 * resized cab rectangle. Off is time-invariant; Reduced has no motion. */
	auto transit = [&](const Rect &rect, CabViewEffectMode mode, uint32_t elapsed) {
		std::vector<Colour> pixels(96 * 64, Colour{0xA5A5A5A5});
		DrawPixelInfo dpi{pixels.data(), 0, 0, 96, 64, 96, ZoomLevel::Min};
		{
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			DrawCabViewTransit(rect, mode, elapsed);
		}
		return pixels;
	};
	const Rect transit_rect{8, 6, 87, 57};
	auto check_clipped = [&](const std::vector<Colour> &pixels, const Rect &inside) {
		bool clipped = true;
		for (int y = 0; y < 64; ++y) for (int x = 0; x < 96; ++x) {
			if (x < inside.left || x > inside.right || y < inside.top || y > inside.bottom) {
				clipped &= pixels[static_cast<size_t>(y) * 96 + x].data == 0xA5A5A5A5;
			}
		}
		CHECK(clipped);
	};
	const auto transit_off = transit(transit_rect, CabViewEffectMode::Off, 0);
	CHECK(transit_off == transit(transit_rect, CabViewEffectMode::Off, 600));
	check_clipped(transit_off, transit_rect);
	const auto transit_reduced = transit(transit_rect, CabViewEffectMode::Reduced, 0);
	CHECK(transit_reduced != transit_off);
	CHECK(transit_reduced == transit(transit_rect, CabViewEffectMode::Reduced, 600));
	const auto transit_full = transit(transit_rect, CabViewEffectMode::Full, 0);
	check_clipped(transit_full, transit_rect);
	CHECK(transit_full != transit(transit_rect, CabViewEffectMode::Full, 600));
	const Rect smaller_transit{16, 12, 79, 51};
	check_clipped(transit(smaller_transit, CabViewEffectMode::Full, 600), smaller_transit);
	DrawPixelInfo *old_dpi = _cur_dpi;
	const DrawPixelInfo old_screen = _screen;
	const auto old_random = _random;
	const auto old_date = TimerGameCalendar::date;
	CabViewCamera camera;
	camera.x = camera.y = 8 * TILE_SIZE + 8;
	camera.z = 6;
	camera.yaw = 0;
	camera.world = INVALID_WORLD;
	constexpr int width = 96, height = 64;
	auto paint = [&](uint32_t frame, bool reset = true) {
		std::vector<Colour> pixels(width * height);
		DrawPixelInfo dpi{pixels.data(), 0, 0, width, height, width, ZoomLevel::Min};
		/* The 32bpp blitter addresses rows through _screen.pitch even when
		 * _cur_dpi points at a private test buffer. Scope both globals. */
		{
			AutoRestoreBackup screen_backup(_screen, dpi);
			AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
			AutoRestoreBackup anim_backup(_screen_disable_anim, true);
			if (reset) ResetCabView3D();
			DrawCabView3D({0, 0, width - 1, height - 1}, camera, frame, 0);
		}
		return pixels;
	};
	const auto sky = paint(1);
	PlanetRegion a;
	a.id = WorldID{0};
	a.min_x = a.min_y = 1;
	a.max_x = 12;
	a.max_y = 20;
	PlanetRegion b;
	b.id = WorldID{1};
	b.min_x = 13;
	b.max_x = 30;
	b.min_y = 1;
	b.max_y = 20;
	REQUIRE(PlanetManager::RegisterRegion(a));
	REQUIRE(PlanetManager::RegisterRegion(b));
	StellarNetwork::RegisterWorld({WorldID{0}, "home", "Home", 0, 0, true});
	StellarNetwork::RegisterWorld({WorldID{1}, "other", "Other", 0, 0, true});
	for (uint x = 1; x <= 12; ++x) for (uint y = 1; y <= 20; ++y) MakeClear(TileXY(x, y), ClearGround::Grass, 0);
	camera.world = WorldID{0};
	const auto local = paint(2);
	CHECK(local != sky);
	for (uint x = 13; x <= 30; ++x) for (uint y = 1; y <= 20; ++y) MakeClear(TileXY(x, y), ClearGround::Grass, 0);
	REQUIRE(Vehicle::CanAllocateItem());
	Train *foreign = Vehicle::Create<Train>();
	foreign->tile = TileXY(16, 8);
	foreign->x_pos = 16 * TILE_SIZE + 8;
	foreign->y_pos = 8 * TILE_SIZE + 8;
	foreign->vehstatus.Reset(VehState::Hidden);
	const auto masked = paint(3);
	CHECK(masked == local);
	/* The synthetic foreign vehicle has no engine image. Its mask has been
	 * exercised; remove it before looking from the destination world. */
	_vehicle_pool.CleanPool();
	/* This comparison goes through the real raw-base sprite decoder and CPU
	 * rasterizer. An unlinked physical head is suppressed; only an accessible
	 * LocalLinked head contributes the bounded opaque gate surface. */
	const TileIndex local_gate = TileXY(10, 8);
	const TileIndex remote_gate = TileXY(20, 8);
	MakeRailTunnel(local_gate, Owner{0}, DiagDirection::SW, RAILTYPE_BEGIN);
	MakeRailTunnel(remote_gate, Owner{0}, DiagDirection::NE, RAILTYPE_BEGIN);
	REQUIRE(IsTunnelTile(local_gate));
	const auto suppressed_head = paint(4);
	/* A cab already on each physical head must have local ground directly
	 * beneath its eye. Compare the lower central pixels at the exact same
	 * pose before and after linking, on both sides of the pair. */
	auto at_head = [&](TileIndex tile, WorldID world, int x_offset, float yaw, uint32_t frame) {
		camera.x = TileX(tile) * TILE_SIZE + x_offset;
		camera.y = TileY(tile) * TILE_SIZE + 8;
		camera.z = 4;
		camera.yaw = yaw;
		camera.world = world;
		return paint(frame);
	};
	/* The outbound NE pose matches the observed native x+4 mixed arrival.
	 * On the SW head use x+12 so twelve units of its tile lie ahead of the
	 * camera; the observed return at x+7 already sees the adjacent grass in
	 * this lower crop. Both views face AWAY from the gate frame, leaving the
	 * bounded local floor as the geometry under test. */
	const auto suppressed_arrival_a = at_head(local_gate, WorldID{0}, 12, static_cast<float>(M_PI), 10);
	const auto suppressed_arrival_b = at_head(remote_gate, WorldID{1}, 4, 0, 11);
	camera.x = camera.y = 8 * TILE_SIZE + 8;
	camera.z = 6;
	camera.yaw = 0;
	camera.world = WorldID{0};
	REQUIRE(PortalRegistry::RegisterPortalPair(local_gate, DiagDirection::SW, WorldID{0},
		remote_gate, DiagDirection::NE, WorldID{1}) != INVALID_PORTAL);
	CHECK(CabViewLocalGateVisible(local_gate, WorldID{0}));
	const auto local_gate_frame = paint(5);
	CHECK(local_gate_frame != suppressed_head);
	auto lower_changed = [&](const std::vector<Colour> &a, const std::vector<Colour> &b) {
		uint changed = 0;
		for (int y = height - 12; y < height; ++y) {
			for (int x = width / 4; x < 3 * width / 4; ++x) {
				if (a[static_cast<size_t>(y) * width + x] != b[static_cast<size_t>(y) * width + x]) ++changed;
			}
		}
		return changed;
	};
	const auto linked_arrival_a = at_head(local_gate, WorldID{0}, 12, static_cast<float>(M_PI), 12);
	const auto linked_arrival_b = at_head(remote_gate, WorldID{1}, 4, 0, 13);
	CHECK(lower_changed(linked_arrival_a, suppressed_arrival_a) >= 48);
	CHECK(lower_changed(linked_arrival_b, suppressed_arrival_b) >= 48);
	/* The validated local face owns the effect. Fixed-time repeated renders are
	 * identical, Full changes with GUI time, Reduced is static, and Off is the
	 * exact accepted Stage 2 picture even if an elapsed value is supplied. */
	camera.x = TileX(local_gate) * TILE_SIZE;
	camera.y = TileY(local_gate) * TILE_SIZE + 8;
	camera.z = 4;
	camera.yaw = 0;
	camera.world = WorldID{0};
	camera.effect_mode = CabViewEffectMode::Off;
	const auto effect_off = paint(20);
	camera.effect_mode = CabViewEffectMode::Full;
	camera.effect_elapsed_ms = 0;
	const auto effect_full_0 = paint(21);
	CHECK(effect_full_0 != effect_off);
	CHECK(paint(22) == effect_full_0);
	camera.effect_elapsed_ms = 350;
	const auto effect_full_350 = paint(23);
	CHECK(effect_full_350 != effect_full_0);
	/* Inspect a real contrasting local-frame edge, rather than accepting a
	 * changing glow on the otherwise uniform opaque centre as lensing proof. */
	unsigned changed_at_edge = 0;
	unsigned darkened_at_edge = 0;
	for (int y = 1; y < height - 1; ++y) for (int x = 1; x < width - 1; ++x) {
		const size_t index = static_cast<size_t>(y) * width + x;
		const int left_green = static_cast<int>((effect_off[index - 1].data >> 8) & 255);
		const int right_green = static_cast<int>((effect_off[index].data >> 8) & 255);
		if (std::abs(left_green - right_green) > 35 && effect_full_350[index] != effect_full_0[index]) {
			++changed_at_edge;
			if (static_cast<int>((effect_full_350[index].data >> 8) & 255) < right_green) ++darkened_at_edge;
		}
	}
	CHECK(changed_at_edge > 0);
	CHECK(darkened_at_edge > 0); // glow alone only increases these channels
	camera.effect_mode = CabViewEffectMode::Reduced;
	camera.effect_elapsed_ms = 0;
	const auto effect_reduced = paint(24);
	CHECK(effect_reduced != effect_off);
	camera.effect_elapsed_ms = 350;
	CHECK(paint(25) == effect_reduced);
	camera.effect_mode = CabViewEffectMode::Off;
	CHECK(paint(26) == effect_off);
	camera.effect_mode = CabViewEffectMode::Full;
	(void)paint(28);
	CHECK(GetCabViewCacheStats().effect_scratch_capacity_bytes > 0);
	camera.effect_mode = CabViewEffectMode::Off;
	CHECK(paint(29, false) == effect_off);
	CHECK(GetCabViewCacheStats().effect_scratch_capacity_bytes == 0);
	camera.effect_mode = CabViewEffectMode::Full;
	(void)paint(30, false);
	CHECK(GetCabViewCacheStats().effect_scratch_capacity_bytes > 0);
	{
		constexpr int narrow_width = 48, narrow_height = 32;
		std::vector<Colour> pixels(narrow_width * narrow_height);
		DrawPixelInfo dpi{pixels.data(), 0, 0, narrow_width, narrow_height, narrow_width, ZoomLevel::Min};
		AutoRestoreBackup screen_backup(_screen, dpi);
		AutoRestoreBackup dpi_backup(_cur_dpi, &dpi);
		AutoRestoreBackup anim_backup(_screen_disable_anim, true);
		DrawCabView3D({0, 0, narrow_width - 1, narrow_height - 1}, camera, 31, 0);
	}
	CHECK(GetCabViewCacheStats().effect_scratch_capacity_bytes <= 8 * 48 * 32 + 256 * 1024);
	camera.x = 8 * TILE_SIZE + 8;
	camera.z = 6;
	CHECK(paint(27) == local_gate_frame); // outside the two-tile entrance cue
	camera.effect_mode = CabViewEffectMode::Off;
	camera.x = camera.y = 8 * TILE_SIZE + 8;
	camera.z = 6;
	camera.yaw = 0;
	camera.world = WorldID{0};
	StellarNetwork::RegisterWorld({WorldID{1}, "other", "Other", 0, 0, false});
	CHECK_FALSE(CabViewLocalGateVisible(local_gate, WorldID{0}));
	const auto inaccessible_gate = paint(6);
	CHECK(inaccessible_gate == suppressed_head);
	CHECK(_random.state[0] == old_random.state[0]);
	CHECK(_random.state[1] == old_random.state[1]);
	CHECK(TimerGameCalendar::date == old_date);
	_vehicle_pool.CleanPool();
	PortalRegistry::Reset();
	PlanetManager::Reset();
	StellarNetwork::Reset();
	_cur_dpi = old_dpi;
	_screen = old_screen;
	ResetCabView3D();
	BlitterFactory::SelectBlitter("null");
	for (FontSize fs : EnumRange(FontSize::End)) Layouter::ResetFontCache(fs);
	FontCache::UninitializeFontCaches();
	MockFontCache::InitializeFontCaches();
	DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
}
