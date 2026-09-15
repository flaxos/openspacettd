/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */

/** @file test_command_authority.cpp Command and real GUI regressions for WP-05. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../town.h"
#include "../newgrf_house.h"
#include "../road.h"
#include "../timer/timer_game_calendar.h"
#include "../town_kdtree.h"
#include "../station_base.h"
#include "../station_kdtree.h"
#include "../station_map.h"
#include "../station_cmd.h"
#include "../newgrf_station.h"
#include "../rail_map.h"
#include "../rail_cmd.h"
#include "../train.h"
#include "../train_cmd.h"
#include "../order_cmd.h"
#include "../signal_func.h"
#include "../settings_internal.h"
#include "../vehicle_cmd.h"
#include "../vehicle_func.h"
#include "../vehicle_base.h"
#include "../engine_func.h"
#include "../engine_base.h"
#include "../depot_base.h"
#include "../base_media_graphics.h"
#include "../cargotype.h"
#include "../industry.h"
#include "../economy_base.h"
#include "../economy_func.h"
#include "../gfx_func.h"
#include "../language.h"
#include "../genworld.h"
#include "../strings_func.h"
#include "../fileio_func.h"
#include "../saveload/saveload.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../textbuf_gui.h"
#include "../querystring_gui.h"
#include "../driver.h"
#include "../video/video_driver.hpp"
#include "../network/network.h"
#include "../network/network_internal.h"
#include "../network/network_func.h"
#include "../portal/corporate_hq.h"
#include "../portal/corporate_hq_gui.h"
#include "../portal/universe_authority.h"
#include "../portal/universe_directory_gui.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../portal/portal_registry.h"
#include "../portal/megacity_manager.h"
#include "../portal/logistics_hub.h"
#include "../portal/company_stockpile.h"
#include "../portal/production_chain.h"
#include "../portal/tech_tree.h"
#include "../portal/fabrication_manager.h"
#include "../widgets/corporate_hq_widget.h"
#include "../widgets/universe_directory_widget.h"
#include "../widgets/misc_widget.h"
#include "../table/strings.h"
#include "../table/sprites.h"
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <sstream>

#include "../safeguards.h"

/** Shared by isolated network workers; all persistent state is real engine state. */
void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score)
{
	(void)MockEnvironment::Instance();
	if (_current_language == nullptr) {
		extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
		auto paths = _valid_searchpaths;
		auto binary = _searchpaths[Searchpath::BinaryDir];
		_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
		_valid_searchpaths = {Searchpath::BinaryDir};
		InitializeLanguagePacks();
		_valid_searchpaths = std::move(paths);
		_searchpaths[Searchpath::BinaryDir] = std::move(binary);
	}
	SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
	UnInitWindowSystem();
	_networking = _network_server = false;
	NetworkFreeLocalCommandQueue();
	PlanetManager::Reset();
	CorporateHQManager::Reset();
	UniverseAuthorityService::Instance().Reset();
	MegacityManager::Reset();
	PortalRegistry::Reset();
	LogisticsHubManager::Reset();
	StockpileManager::Reset();
	ProductionChainManager::Reset();
	TechTreeManager::Reset();
	FabricationManager::Reset();
	_cargo_payment_pool.CleanPool();
	_vehicle_pool.CleanPool();
	ResetVehicleHash();
	_depot_pool.CleanPool();
	_station_pool.CleanPool();
	_industry_pool.CleanPool();
	_town_pool.CleanPool();
	_company_pool.CleanPool();
	_cargopacket_pool.CleanPool();
	Map::Allocate(64, 64);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3);
		else MakeVoid(tile);
	}
	SetupCargoForClimate(LandscapeType::Temperate);
	_settings_game.game_creation.landscape = LandscapeType::Temperate;
	TimerGameCalendar::SetDate(TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year{1950}, 0, 1), 0);
	ResetHouses();
	InitializeBuildingCounts();
	ResetRoadTypes();
	_engine_mngr.ResetToDefaultMapping();
	SetupEngines();
	for (uint8_t id = 0; id < 2; ++id) {
		Company *company = Company::CreateAtIndex(CompanyID{id});
		company->money = 10000000;
		company->name = id == 0 ? "Authority owner" : "Other company";
	}
	_current_company = _local_company = CompanyID{0};
	_game_mode = GameMode::Normal;
	_pause_mode = {};
	_generating_world = false;
	_settings_client.network.commands_per_frame = 16;
	_settings_client.network.commands_per_frame_server = 16;
	_price[Price::BuildTown] = 1000;
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "Authority world", .phase = phase,
		.biome = WorldBiome::Temperate, .min_x = 1, .min_y = 1, .max_x = 62, .max_y = 62, .development_score = score}));
	if (phase != WorldPhase::Phase4_Expansion) {
		REQUIRE(Town::CanAllocateItem());
		Town *town = Town::Create(TileXY(10, 10));
		/* Promotion tests retain their existing settlement; an Expansion World
		 * must let colonisation exercise native founding instead of a shell. */
		town->name = "Authority settlement";
		town->townnametype = SPECSTR_TOWNNAME_START;
	}
	RebuildTownKdtree();
	RebuildStationKdtree();
	REQUIRE(CorporateHQManager::RegisterHQ(CompanyID{0}, WorldID{0}, TileXY(20, 20), "Authority HQ"));
	_screen.width = _screen.pitch = 1024;
	_screen.height = 768;
	ScreenSizeChanged();
	InitWindowSystem();
	if (_valid_searchpaths.empty()) _valid_searchpaths.push_back(Searchpath::WorkingDir);
}

/** Compact canonical comparison, deliberately excluding transient window/connectivity data. */
std::string CommandAuthorityState()
{
	std::ostringstream out;
	for (const Company *company : Company::Iterate()) out << "company:" << static_cast<int>(company->index.base()) << ':' << company->money << ';';
	for (const auto &hq : CorporateHQManager::GetAllHQ()) out << "hq:" << static_cast<int>(hq.company_id.base()) << ':' << static_cast<int>(hq.tier) << ':' << hq.campus_name << ';';
	for (const auto &world : PlanetManager::GetAllRegions()) out << "world:" << world.id.base() << ':' << static_cast<int>(world.phase) << ':' << world.development_score << ':' << world.name << ':' << world.outpost_tile.base() << ';';
	for (const auto &world : UniverseAuthorityService::Instance().GetWorldDirectoryForGUI()) out << "directory:" << world.world_id.base() << ':' << static_cast<int>(world.phase) << ':' << world.name << ';';
	out << "towns:" << Town::GetNumItems();
	return out.str();
}

void SaveReloadCommandAuthority()
{
	UnInitWindowSystem();
	const auto dir = std::filesystem::temp_directory_path() / fmt::format("openspacettd-wp05-{}", std::chrono::steady_clock::now().time_since_epoch().count());
	REQUIRE(std::filesystem::create_directory(dir));
	const auto path = (dir / "authority.sav").string();
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	// Registry is transient: local display must derive from the restored canonical world.
	UniverseAuthorityService::Instance().Reset();
	REQUIRE(SaveOrLoad(path, SaveLoadOperation::Load, DetailedFileType::GameFile, Subdirectory::None, false) == SaveLoadResult::Ok);
	std::filesystem::remove(path);
	std::filesystem::remove(dir);
}

TEST_CASE("WP08 legacy empty outpost fixture v1 preserves saved identity and infrastructure", "[command-authority][wp08-legacy]")
{
	const bool already_promoted = GENERATE(false, true);
	SetupCommandAuthorityWorld(WorldPhase::Phase4_Expansion, 0);
	const TileIndex site = TileXY(30, 30);
	REQUIRE(Town::CanAllocateItem());
	Town *town = Town::Create(site);
	town->name = "Legacy empty outpost v1";
	town->townnametype = SPECSTR_TOWNNAME_START;
	const TownID town_id = town->index;
	RebuildTownKdtree();
	if (already_promoted) REQUIRE(PlanetManager::ColonizeWorld(WorldID{0}, town->name, site));
	const PlanetRegion world = *PlanetManager::GetRegion(WorldID{0});
	const TileIndex rail = TileXY(35, 35);
	MakeRailNormal(rail, CompanyID{0}, TrackBits{Track::X}, RAILTYPE_RAIL);
	Company::Get(CompanyID{0})->infrastructure.rail[RAILTYPE_RAIL] = 1;
	const Money balance = Company::Get(CompanyID{0})->money;
	const char *fixture_dir = std::getenv("OSTTD_WP08_FIXTURE_DIR");
	const auto dir = fixture_dir ? std::filesystem::path(fixture_dir) :
		std::filesystem::temp_directory_path() / fmt::format("ost-wp08-legacy-{}", std::chrono::steady_clock::now().time_since_epoch().count());
	std::filesystem::create_directories(dir);
	const auto path = dir / (already_promoted ? "empty-outpost-v1-frontier.sav" : "empty-outpost-v1-expansion.sav");
	REQUIRE_FALSE(std::filesystem::exists(path));
	UnInitWindowSystem();
	REQUIRE(SaveOrLoad(path.string(), SaveLoadOperation::Save, DetailedFileType::GameFile,
		Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(SaveOrLoad(path.string(), SaveLoadOperation::Load, DetailedFileType::GameFile,
		Subdirectory::None, false) == SaveLoadResult::Ok);
	town = PlanetManager::GetWorldPrimaryTown(WorldID{0});
	REQUIRE(town != nullptr);
	CHECK(town->index == town_id);
	CHECK(town->xy == site);
	CHECK(town->name == "Legacy empty outpost v1");
	CHECK(town->cache.population == 0);
	CHECK(town->cache.num_houses == 0);
	CHECK(CalcClosestTownFromTile(site) == town);
	CHECK(Town::GetNumItems() == 1);
	CHECK(IsPlainRailTile(rail));
	CHECK(GetTrackBits(rail) == TrackBits{Track::X});
	CHECK(GetTileOwner(rail) == CompanyID{0});
	CHECK(Company::Get(CompanyID{0})->money == balance);
	const PlanetRegion *loaded = PlanetManager::GetRegion(WorldID{0});
	REQUIRE(loaded != nullptr);
	CHECK(loaded->name == world.name);
	CHECK(loaded->phase == world.phase);
	CHECK(loaded->development_score == world.development_score);
	CHECK(loaded->outpost_tile == world.outpost_tile);
	const auto before_retry = CommandAuthorityState();
	_current_company = _local_company = CompanyID{0};
	const StringID error = already_promoted ? STR_ERROR_CANNOT_COLONIZE_NON_EXPANSION : STR_ERROR_CANNOT_COLONIZE_INCOMPLETE_OUTPOST;
	CHECK(Command<Commands::ColonizeOutpost>::Do({}, site, "Retry").GetErrorMessage() == error);
	CHECK_FALSE(Command<Commands::ColonizeOutpost>::Post(site, "Retry"));
	CHECK(CommandAuthorityState() == before_retry);
	if (fixture_dir == nullptr) {
		std::filesystem::remove(path);
		std::filesystem::remove(dir);
	}
}

TEST_CASE("Command authority - HQ rejects foreign invalid and stale tier requests", "[command-authority]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	CompanyID company{0};
	CorporateHQTier target = CorporateHQTier::PlanetaryHQ;
	SECTION("Foreign HQ") { _current_company = CompanyID{1}; }
	SECTION("Invalid company") { company = CompanyID::Invalid(); }
	SECTION("Spectator") { _current_company = COMPANY_SPECTATOR; }
	SECTION("Absent HQ") { CorporateHQManager::Reset(); }
	SECTION("Invalid target") { target = static_cast<CorporateHQTier>(255); }
	SECTION("Skip tier") { target = CorporateHQTier::CST_Arcology; }
	SECTION("Same tier") { target = CorporateHQTier::RegionalBranch; }
	SECTION("Malformed saved tier") {
		auto hq = *CorporateHQManager::GetHQ(company);
		hq.tier = static_cast<CorporateHQTier>(0);
		CorporateHQManager::RestoreHQ(hq);
	}
	SECTION("Already maximum") {
		auto hq = *CorporateHQManager::GetHQ(company);
		hq.tier = CorporateHQTier::CST_Arcology;
		CorporateHQManager::RestoreHQ(hq);
	}
	const auto before = CommandAuthorityState();
	CHECK(Command<Commands::UpgradeCorporateHQ>::Do({}, company, target).Failed());
	CHECK(CommandAuthorityState() == before);
	CHECK(Command<Commands::UpgradeCorporateHQ>::Do(DoCommandFlag::Execute, company, target).Failed());
	CHECK(CommandAuthorityState() == before);
}

TEST_CASE("Command authority - HQ advances once per requested tier and persists", "[command-authority]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	Company::Get(CompanyID{0})->money = 0; // Existing upgrades have no fee; WP-05 does not add one.
	for (auto tier : {CorporateHQTier::PlanetaryHQ, CorporateHQTier::Interstellar, CorporateHQTier::CST_Arcology}) {
		const auto before = CommandAuthorityState();
		auto query = Command<Commands::UpgradeCorporateHQ>::Do({}, CompanyID{0}, tier);
		REQUIRE(query.Succeeded());
		CHECK(query.GetCost() == 0);
		CHECK(CommandAuthorityState() == before);
		REQUIRE(Command<Commands::UpgradeCorporateHQ>::Post(CompanyID{0}, tier));
		CHECK(CorporateHQManager::GetHQ(CompanyID{0})->tier == tier);
		CHECK(Company::Get(CompanyID{0})->money == 0);
		const auto after = CommandAuthorityState();
		CHECK_FALSE(Command<Commands::UpgradeCorporateHQ>::Post(CompanyID{0}, tier));
		CHECK(CommandAuthorityState() == after);
	}
	const auto before = CommandAuthorityState();
	SaveReloadCommandAuthority();
	CHECK(CommandAuthorityState() == before);
}

TEST_CASE("Command authority - world commands preserve state on denial and charge once on success", "[command-authority]")
{
	const bool colony = GENERATE(false, true);
	SetupCommandAuthorityWorld(colony ? WorldPhase::Phase4_Expansion : WorldPhase::Phase3_Frontier, colony ? 0 : 2100);
	WorldID world{0};
	TileIndex tile = TileXY(20, 20);
	bool succeeds = false;
	SECTION("Unaffordable") { Company::Get(CompanyID{0})->money = 0; }
	SECTION("Invalid destination") { world = WorldID{100}; tile = INVALID_TILE; }
	SECTION("Wrong phase or insufficient development") {
		SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 0);
	}
	SECTION("Spectator") { _current_company = _local_company = COMPANY_SPECTATOR; }
	SECTION("Ready and affordable") { succeeds = true; }
	const auto before = CommandAuthorityState();
	const auto query = colony ? Command<Commands::ColonizeOutpost>::Do({}, tile, "Authority colony") : Command<Commands::PromoteWorld>::Do({}, world);
	CHECK(query.Succeeded() == succeeds);
	CHECK(CommandAuthorityState() == before);
	const bool posted = colony ? Command<Commands::ColonizeOutpost>::Post(tile, "Authority colony") : Command<Commands::PromoteWorld>::Post(world);
	CHECK(posted == succeeds);
	if (!succeeds) {
		CHECK(CommandAuthorityState() == before);
	} else {
		CHECK(Company::Get(CompanyID{0})->money == 10000000 - (colony ? 5000 : 8000));
		CHECK(PlanetManager::GetRegion(world)->phase == (colony ? WorldPhase::Phase3_Frontier : WorldPhase::Phase2_Developed));
		CHECK(PlanetManager::GetRegion(world)->development_score == (colony ? 100 : 2350));
		REQUIRE(UniverseAuthorityService::Instance().GetWorld(world) != nullptr);
		CHECK(UniverseAuthorityService::Instance().GetWorld(world)->phase == PlanetManager::GetRegion(world)->phase);
		const auto after = CommandAuthorityState();
		CHECK_FALSE((colony ? Command<Commands::ColonizeOutpost>::Post(tile, "Authority colony") : Command<Commands::PromoteWorld>::Post(world)));
		CHECK(CommandAuthorityState() == after);
		SaveReloadCommandAuthority();
		CHECK(CommandAuthorityState() == after);
	}
}

TEST_CASE("Corporate HQ GUI establishes an owned station hub and edits its reserve", "[.][command-authority][wp09-gui]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	if (VideoDriver::GetInstance() == nullptr) DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
	TileIndex station_tile = TileXY(30, 30);
	REQUIRE(Station::CanAllocateItem());
	Station *station = Station::Create(station_tile);
	station->name = "Authority rail station";
	station->owner = CompanyID{0};
	station->town = PlanetManager::GetWorldPrimaryTown(WorldID{0});
	station->facilities.Set(StationFacility::Train);
	station->train_station = TileArea(station_tile, 1, 1);
	station->spread = station->train_station;
	MakeRailStation(station_tile, CompanyID{0}, station->index, Axis::X, 0, RAILTYPE_RAIL);
	Company::Get(CompanyID{0})->infrastructure.station = 1;
	Company::Get(CompanyID{0})->infrastructure.rail[RAILTYPE_RAIL] = 1;
	RebuildStationKdtree();
	station->RecomputeCatchment();
	StationID station_id = station->index;
	ShowCorporateHQ(CompanyID{0});
	Window *window = FindWindowById(WindowClass::CorporateHQ, 0);
	REQUIRE(window != nullptr);
	window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
	window->OnPlaceObject({}, station_tile);
	const LogisticsHub *hub = LogisticsHubManager::GetHubForStation(station_id);
	REQUIRE(hub != nullptr);
	uint32_t hub_id = hub->hub_id;
	window->OnClick({}, WID_CHQ_SELECT_HUB, 1);
	window->OnClick({}, WID_CHQ_SELECT_CARGO, 1);
	REQUIRE(IsValidCargoType(CargoType{1}));
	window->OnClick({}, WID_CHQ_SET_RESERVE, 1);
	Window *query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(query != nullptr);
	REQUIRE(query->querystrings.count(WID_QS_TEXT) == 1);
	query->querystrings[WID_QS_TEXT]->text.Assign("40");
	query->OnClick({}, WID_QS_OK, 1);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, CargoType{1}) == 40);
}

/** Exercise real train construction and the normal station cargo tick after GUI establishment. */
static void ExerciseFreshHubCargo(Station *station, uint32_t hub_id)
{
	const CargoType coal = GetCargoTypeByLabel(CT_COAL);
	REQUIRE(coal == CargoType{1});
	_settings_game.vehicle.max_trains = 10;
	_settings_game.vehicle.max_train_length = 10;
	/* SetupEngines copies vanilla specifications; map their cargo labels as
	 * the graphics/NewGRF startup normally does before building vehicles. */
	for (Engine *engine : Engine::Iterate()) {
		if (const CargoLabel *label = std::get_if<CargoLabel>(&engine->info.cargo_label)) {
			const CargoType mapped = GetCargoTypeByLabel(*label);
			if (IsValidCargoType(mapped)) engine->info.cargo_type = mapped;
		}
	}
	StartupEngines();
	EngineID locomotive = EngineID::Invalid();
	EngineID coal_wagon = EngineID::Invalid();
	for (const Engine *engine : Engine::Iterate()) {
		if (engine->type != VehicleType::Train || !IsEngineBuildable(engine->index, VehicleType::Train, station->owner)) continue;
		const auto &info = engine->VehInfo<RailVehicleInfo>();
		if (!HasPowerOnRail(info.railtypes, RAILTYPE_RAIL)) continue;
		if (info.railveh_type == RailVehicleType::Singlehead && locomotive == EngineID::Invalid()) locomotive = engine->index;
		if (info.railveh_type == RailVehicleType::Wagon && engine->GetDefaultCargoType() == coal && info.capacity == 30) coal_wagon = engine->index;
	}
	REQUIRE(locomotive != EngineID::Invalid());
	REQUIRE(coal_wagon != EngineID::Invalid());
	const TileIndex depot = TileXY(25, 30);
	REQUIRE(Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, depot, RAILTYPE_RAIL, DiagDirection::SW).Succeeded());
	for (uint x = 26; x < TileX(station->xy); ++x) {
		REQUIRE(Command<Commands::BuildRail>::Do(DoCommandFlag::Execute,
			TileXY(x, 30), RAILTYPE_RAIL, Track::X, false).Succeeded());
	}
	auto [engine_cost, engine_id, capacity, mail, capacities] = Command<Commands::BuildVehicle>::Do(
		DoCommandFlag::Execute, depot, locomotive, false, INVALID_CARGO, ClientID::Invalid);
	INFO("locomotive=" << locomotive.base() << " build error=" << engine_cost.GetErrorMessage().base());
	REQUIRE(engine_cost.Succeeded());
	Train *train = Train::Get(engine_id);
	train->name = "WP09 freight";
	for (uint i = 0; i < 2; ++i) {
		auto [wagon_cost, wagon_id, wagon_capacity, wagon_mail, wagon_capacities] = Command<Commands::BuildVehicle>::Do(
			DoCommandFlag::Execute, depot, coal_wagon, false, INVALID_CARGO, ClientID::Invalid);
		REQUIRE(wagon_cost.Succeeded());
		REQUIRE(Command<Commands::MoveRailVehicle>::Do(DoCommandFlag::Execute, wagon_id, train->Last()->index, false).Succeeded());
	}
	/* The fixture starts with freight in its depot. From here, orders, vehicle
	 * movement, arrival and station ticks perform the entire exchange. */
	uint delivered = 0;
	for (Train *part = train; part != nullptr; part = part->Next()) {
		REQUIRE(part->track == Track::Depot);
		if (part->cargo_type != coal || part->cargo_cap == 0) continue;
		REQUIRE(CargoPacket::CanAllocateItem());
		CargoPacket *packet = CargoPacket::Create(part->cargo_cap, 1, StationID::Invalid(), TileXY(5, 5), 0);
		packet->UpdateLoadingTile(TileXY(5, 5));
		part->cargo.Append(packet, VehicleCargoList::MoveToAction::Keep);
		delivered += part->cargo_cap;
	}
	REQUIRE(delivered == 60);
	train->CargoChanged();
	for (const SettingVariant &setting : GetSaveLoadSettingTable()) {
		const SettingDesc *desc = GetSettingDesc(setting);
		if (desc->GetName().starts_with("pf.")) desc->ResetToDefault(&_settings_game);
	}
	Order order;
	order.MakeGoToStation(station->index);
	order.SetUnloadType(OrderUnloadType::Unload);
	order.SetLoadType(OrderLoadType::LoadIfPossible);
	REQUIRE(Command<Commands::InsertOrder>::Do(DoCommandFlag::Execute,
		train->index, VehicleOrderID{0}, order).Succeeded());
	REQUIRE(Command<Commands::StartStopVehicle>::Do(DoCommandFlag::Execute, train->index, false).Succeeded());
	AutoRestoreBackup gradual_loading(_settings_game.order.gradual_loading, false);
	bool observed_full_deposit = false;
	uint onboard = 0;
	uint ticks = 0;
	for (; ticks < 4096 && !train->vehicle_flags.Test(VehicleFlag::LoadingFinished); ++ticks) {
		REQUIRE(train->Tick());
		LoadUnloadStation(station);
		UpdateSignalsInBuffer();
		onboard = 0;
		for (const Train *part = train; part != nullptr; part = part->Next()) onboard += part->cargo.StoredCount();
		const uint stock = StockpileManager::GetStock(WorldID{1}, station->owner, coal);
		REQUIRE(onboard + stock + station->goods[coal].AvailableCount() == delivered);
		if (stock == delivered && onboard == 0) {
			observed_full_deposit = true;
			REQUIRE(train->cargo_payment != nullptr);
			CHECK(train->cargo_payment->route_profit == 0);
		}
	}
	INFO("cargo journey ticks=" << ticks << " front tile=" << train->tile.base());
	CHECK(ticks < 4096);
	CHECK(observed_full_deposit);
	CHECK(train->last_station_visited == station->index);
	CHECK(train->current_order.IsType(OT_LOADING));
	for (const Train *part = train; part != nullptr; part = part->Next()) {
		REQUIRE(IsRailStationTile(part->tile));
		CHECK(GetStationIndex(part->tile) == station->index);
		CHECK_FALSE(part->vehstatus.Test(VehState::Hidden));
	}
	CHECK(onboard == 20);
	CHECK(StockpileManager::GetStock(WorldID{1}, station->owner, coal) == 40);
	CHECK(station->goods[coal].AvailableCount() == 0);
	CHECK(onboard + StockpileManager::GetStock(WorldID{1}, station->owner, coal) == delivered);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_deposited == delivered);
	CHECK(LogisticsHubManager::GetHub(hub_id)->total_dispatched == 20);
}

TEST_CASE("Corporate HQ GUI lets an eligible player found the first HQ", "[.][command-authority][wp09-gui]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	if (VideoDriver::GetInstance() == nullptr) DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
	CorporateHQManager::Reset();
	ResetRailTypes();
	StationClass::Reset();
	Company::Get(CompanyID{0})->avail_railtypes.Set(RAILTYPE_RAIL);
	Company::Get(CompanyID{0})->clear_limit = 1000 << 16;
	_settings_game.station.station_spread = 12;
	PlanetManager::Reset();
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{0}, .name = "Core", .phase = WorldPhase::Phase1_Core,
		.biome = WorldBiome::Temperate, .min_x = 1, .min_y = 1, .max_x = 20, .max_y = 62}));
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{1}, .name = "Industry", .phase = WorldPhase::Phase2_Developed,
		.biome = WorldBiome::Temperate, .min_x = 21, .min_y = 1, .max_x = 40, .max_y = 62}));
	REQUIRE(PlanetManager::RegisterRegion({.id = WorldID{2}, .name = "Frontier", .phase = WorldPhase::Phase3_Frontier,
		.biome = WorldBiome::Temperate, .min_x = 41, .min_y = 1, .max_x = 62, .max_y = 62}));
	for (const auto &[tile, name] : {std::pair{TileXY(25, 10), "Developed town"}, std::pair{TileXY(45, 10), "Frontier town"}}) {
		REQUIRE(Town::CanAllocateItem());
		Town *town = Town::Create(tile);
		town->name = name;
		town->townnametype = SPECSTR_TOWNNAME_START;
	}
	RebuildTownKdtree();
	for (TileIndex station_tile : {TileXY(30, 30), TileXY(50, 30)}) {
		CommandCost built = Command<Commands::BuildRailStation>::Do(DoCommandFlag::Execute,
			station_tile, RAILTYPE_RAIL, Axis::X, 1, 3, STAT_CLASS_DFLT, 0,
			StationID::Invalid(), false);
		INFO("station site=" << TileX(station_tile) << ',' << TileY(station_tile) << " error=" << built.GetErrorMessage().base());
		REQUIRE(built.Succeeded());
		REQUIRE(IsRailStation(station_tile));
		REQUIRE(Station::Get(GetStationIndex(station_tile))->owner == CompanyID{0});
	}
	TileIndex site = TileXY(15, 15);
	Money before = Company::Get(CompanyID{0})->money;
	ShowCorporateHQ(CompanyID{0});
	Window *window = FindWindowById(WindowClass::CorporateHQ, 0);
	REQUIRE(window != nullptr);
	/* Changing the active facility tool must survive the native abort of
	 * the previous tool, including repeated clicks on the same control. */
	window->OnClick({}, WID_CHQ_BUILD_HQ, 1);
	window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
	window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
	window->OnPlaceObject({}, site);
	CHECK(window->GetWidgetString(WID_CHQ_STATUS_BAR, STR_NULL).find("Select one of your rail station platform tiles") == 0);
	CHECK_FALSE(CorporateHQManager::HasHQ(CompanyID{0}));
	CHECK(LogisticsHubManager::GetAllHubs().empty());
	/* The same controls expose costs and reject invalid or unaffordable sites. */
	for (const auto &[bad_site, balance] : {std::pair{TileXY(35, 20), before}, std::pair{site, Money{4999999}}}) {
		AutoRestoreBackup money(Company::Get(CompanyID{0})->money, balance);
		window->OnClick({}, WID_CHQ_BUILD_HQ, 1);
		CHECK(window->GetWidgetString(WID_CHQ_STATUS_BAR, STR_NULL).find("2,500,000 Cr") != std::string::npos);
		window->OnPlaceObject({}, bad_site);
		CHECK_FALSE(CorporateHQManager::HasHQ(CompanyID{0}));
		CHECK(Company::Get(CompanyID{0})->money == balance);
		CHECK(window->GetWidgetString(WID_CHQ_STATUS_BAR, STR_NULL).find("rejected") != std::string::npos);
	}
	window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
	window->OnClick({}, WID_CHQ_BUILD_HQ, 1);
	window->OnClick({}, WID_CHQ_BUILD_HQ, 1);
	window->OnPlaceObject({}, site);
	const CorporateHQProfile *hq = CorporateHQManager::GetHQ(CompanyID{0});
	REQUIRE(hq != nullptr);
	CHECK(hq->tile == site);
	CHECK(hq->world_id == WorldID{0});
	CHECK(Company::Get(CompanyID{0})->money == before - 2500000);
	window->OnClick({}, WID_CHQ_BUILD_HQ, 1);
	window->OnPlaceObject({}, TileXY(16, 15));
	CHECK(Company::Get(CompanyID{0})->money == before - 2500000);
	CHECK(CorporateHQManager::GetAllHQ().size() == 1);
	const TileIndex hub_tile = TileXY(30, 30);
	const StationID hub_station = GetStationIndex(hub_tile);
	window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
	CHECK(window->GetWidgetString(WID_CHQ_STATUS_BAR, STR_NULL).find("75,000 Cr") != std::string::npos);
	window->OnPlaceObject({}, TileXY(25, 25));
	CHECK(LogisticsHubManager::GetAllHubs().empty());
	{
		AutoRestoreBackup money(Company::Get(CompanyID{0})->money, Money{0});
		window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
		window->OnPlaceObject({}, hub_tile);
		CHECK(LogisticsHubManager::GetAllHubs().empty());
		CHECK(Company::Get(CompanyID{0})->money == 0);
	}
	window->OnClick({}, WID_CHQ_BUILD_HUB, 1);
	window->OnPlaceObject({}, hub_tile);
	const LogisticsHub *hub = LogisticsHubManager::GetHubForStation(hub_station);
	REQUIRE(hub != nullptr);
	const uint32_t hub_id = hub->hub_id;
	CHECK(hub->company_id == CompanyID{0});
	CHECK(hub->world_id == WorldID{1});
	CHECK(Company::Get(CompanyID{0})->money == before - 2500000 - 75000);
	window->OnClick({}, WID_CHQ_SELECT_HUB, 1);
	window->OnClick({}, WID_CHQ_SELECT_CARGO, 1);
	window->OnClick({}, WID_CHQ_SET_RESERVE, 1);
	Window *reserve_query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(reserve_query != nullptr);
	reserve_query->querystrings[WID_QS_TEXT]->text.Assign("40");
	reserve_query->OnClick({}, WID_QS_OK, 1);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, CargoType{1}) == 40);
	window->OnClick({}, WID_CHQ_SET_RESERVE, 1);
	reserve_query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(reserve_query != nullptr);
	reserve_query->querystrings[WID_QS_TEXT]->text.Assign("4294967296");
	reserve_query->OnClick({}, WID_QS_OK, 1);
	CHECK(LogisticsHubManager::GetReserveFloor(hub_id, CargoType{1}) == 40);
	CHECK(window->GetWidgetString(WID_CHQ_STATUS_BAR, STR_NULL).find("whole number") != std::string::npos);
	ExerciseFreshHubCargo(Station::Get(hub_station), hub_id);
	/* CTest reloads in a fresh process with real base graphics, so native
	 * NewGRF startup restores the saved train's engine specifications too. */
	if (const char *save_path = std::getenv("OSTTD_WP09_SAVE_PATH")) {
		UnInitWindowSystem();
		REQUIRE(SaveOrLoad(save_path, SaveLoadOperation::Save, DetailedFileType::GameFile,
			Subdirectory::None, false) == SaveLoadResult::Ok);
	}
}

TEST_CASE("WP09 fresh HQ hub and reserve survive process reload", "[.][wp09-reload]")
{
	const char *save_path = std::getenv("OSTTD_WP09_SAVE_PATH");
	REQUIRE(save_path != nullptr);
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 0);
	UnInitWindowSystem();
	REQUIRE(VideoDriver::GetInstance() == nullptr);
	extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
	/* Discover the same read-only base paths as game startup, including the
	 * user/shared folders used by CI, without creating personal directories. */
	extern void DetermineBasePaths(std::string_view exe);
	const char *test_binary = std::getenv("OSTTD_TEST_BINARY");
	REQUIRE(test_binary != nullptr);
	DetermineBasePaths(test_binary);
	_valid_searchpaths.clear();
	for (Searchpath sp : EnumRange(Searchpath::End)) {
		if (!_searchpaths[sp].empty()) _valid_searchpaths.push_back(sp);
	}
	TarScanner::DoScan(TarScanner::Mode::Baseset);
	BaseGraphics::FindSets();
	INFO("The process-reload integration test requires base graphics in a normal OpenTTD data folder or build/baseset");
	REQUIRE(BaseGraphics::SetSet(nullptr));
	DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
	REQUIRE(SaveOrLoad(save_path, SaveLoadOperation::Load, DetailedFileType::GameFile,
		Subdirectory::None, false) == SaveLoadResult::Ok);
	REQUIRE(CorporateHQManager::GetHQ(CompanyID{0}) != nullptr);
	CHECK(CorporateHQManager::GetHQ(CompanyID{0})->tile == TileXY(15, 15));
	CHECK(CorporateHQManager::GetHQ(CompanyID{0})->world_id == WorldID{0});
	REQUIRE(IsRailStation(TileXY(30, 30)));
	const StationID hub_station = GetStationIndex(TileXY(30, 30));
	REQUIRE(LogisticsHubManager::GetHubForStation(hub_station) != nullptr);
	const LogisticsHub *hub = LogisticsHubManager::GetHubForStation(hub_station);
	CHECK(hub->company_id == CompanyID{0});
	CHECK(hub->world_id == WorldID{1});
	CHECK(LogisticsHubManager::GetReserveFloor(hub->hub_id, CargoType{1}) == 40);
	CHECK(StockpileManager::GetStock(WorldID{1}, CompanyID{0}, CargoType{1}) == 40);
	CHECK(hub->total_deposited == 60);
	CHECK(hub->total_dispatched == 20);
	uint onboard = 0;
	for (const Train *train : Train::Iterate()) {
		if (train->owner == CompanyID{0} && train->cargo_type == CargoType{1}) onboard += train->cargo.StoredCount();
	}
	CHECK(onboard == 20);
	CHECK(Company::Get(CompanyID{0})->money == 10000000 - 2500000 - 75000);
}

static void ExecuteAuthorityQueue()
{
	_settings_client.network.commands_per_frame = 16;
	_settings_client.network.commands_per_frame_server = 16;
	NetworkDistributeCommands();
	_frame_counter = _frame_counter_max + 1;
	NetworkExecuteLocalCommandQueue();
}

TEST_CASE("Command authority - actual HQ GUI queues an upgrade without local mutation", "[command-authority][gui]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	AutoRestoreBackup networking(_networking, true);
	AutoRestoreBackup server(_network_server, true);
	ShowCorporateHQ(CompanyID{0});
	Window *window = FindWindowById(WindowClass::CorporateHQ, 0);
	REQUIRE(window != nullptr);
	const auto before = CommandAuthorityState();
	window->OnClick({}, WID_CHQ_UPGRADE, 1);
	window->OnClick({}, WID_CHQ_UPGRADE, 1); // Two pending clicks carry the same target tier.
	CHECK(CommandAuthorityState() == before);
	ExecuteAuthorityQueue();
	CHECK(CorporateHQManager::GetHQ(CompanyID{0})->tier == CorporateHQTier::PlanetaryHQ);
	NetworkFreeLocalCommandQueue();
	UnInitWindowSystem();
}

TEST_CASE("Command authority - foreign HQ window never upgrades either company", "[command-authority][gui]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase1_Core, 10000);
	REQUIRE(CorporateHQManager::RegisterHQ(CompanyID{1}, WorldID{0}, TileXY(21, 20), "Other HQ"));
	_current_company = _local_company = CompanyID{1};
	ShowCorporateHQ(CompanyID{0}); // Company zero must not be mistaken for 'use local company'.
	Window *window = FindWindowById(WindowClass::CorporateHQ, 0);
	REQUIRE(window != nullptr);
	const auto before = CommandAuthorityState();
	window->OnClick({}, WID_CHQ_UPGRADE, 1);
	CHECK(CommandAuthorityState() == before);
	window->OnClick({}, WID_CHQ_TECH_BUDGET_BTN, 1);
	window->OnClick({}, WID_CHQ_TECH_RESEARCH_BTN, 1);
	window->OnClick({}, WID_CHQ_FABRICATION_TOGGLE, 1);
	for (CompanyID company : {CompanyID{0}, CompanyID{1}}) {
		CHECK(TechTreeManager::GetMonthlyBudget(company) == 0);
		CHECK_FALSE(FabricationManager::IsFabricateFromStockpileEnabled(company));
	}
	UnInitWindowSystem();
}

TEST_CASE("Command authority - actual Directory GUI observes successful queued commands only", "[command-authority][gui]")
{
	const bool colony = GENERATE(false, true);
	SetupCommandAuthorityWorld(colony ? WorldPhase::Phase4_Expansion : WorldPhase::Phase3_Frontier, colony ? 0 : 2100);
	const bool affordable = GENERATE(false, true);
	if (!affordable) Company::Get(CompanyID{0})->money = 0;
	AutoRestoreBackup networking(_networking, true);
	AutoRestoreBackup server(_network_server, true);
	const auto before = CommandAuthorityState();
	ShowUniverseDirectory();
	Window *window = FindWindowById(WindowClass::UniverseDirectory, 0);
	REQUIRE(window != nullptr);
	CHECK(UniverseAuthorityService::Instance().GetWorldDirectory().empty());
	window->OnClick({}, WID_UD_REFRESH, 1);
	window->OnClick({}, colony ? WID_UD_COLONIZE_BTN : WID_UD_PROMOTE_BTN, 1);
	CHECK(CommandAuthorityState() == before);
	CHECK(UniverseAuthorityService::Instance().GetWorldDirectory().empty());
	ExecuteAuthorityQueue();
	if (affordable) {
		CHECK(PlanetManager::GetRegion(WorldID{0})->phase == (colony ? WorldPhase::Phase3_Frontier : WorldPhase::Phase2_Developed));
		CHECK(UniverseAuthorityService::Instance().GetWorld(WorldID{0})->phase == PlanetManager::GetRegion(WorldID{0})->phase);
	} else {
		CHECK(CommandAuthorityState() == before);
	}
	NetworkFreeLocalCommandQueue();
	UnInitWindowSystem();
}

TEST_CASE("Command authority - directory projection is read only and canonical despite stale metadata", "[command-authority]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 2100);
	auto &service = UniverseAuthorityService::Instance();
	REQUIRE(service.RegisterWorld({.world_id = WorldID{0}, .phase = WorldPhase::Phase1_Core,
		.name = "Stale name", .address = "Local connection", .description = "Local telemetry", .active_clients = 7}));
	REQUIRE(service.RegisterWorld({.world_id = WorldID{99}, .phase = WorldPhase::Phase4_Expansion,
		.name = "Remote only", .address = "Remote node", .description = "Remote telemetry"}));
	const auto worlds = service.GetWorldDirectoryForGUI();
	REQUIRE(worlds.size() == 2);
	CHECK(worlds[0].phase == WorldPhase::Phase3_Frontier);
	CHECK(worlds[0].name == "Authority world");
	CHECK(worlds[0].active_clients == 7);
	CHECK(worlds[1].name == "Remote only");
	CHECK(service.GetWorld(WorldID{0})->phase == WorldPhase::Phase1_Core);
	REQUIRE(Command<Commands::PromoteWorld>::Post(WorldID{0}));
	CHECK(service.GetWorld(WorldID{0})->phase == WorldPhase::Phase2_Developed);
	CHECK(service.GetWorld(WorldID{0})->active_clients == 7);
	CHECK(service.GetWorld(WorldID{99})->phase == WorldPhase::Phase4_Expansion);
}
