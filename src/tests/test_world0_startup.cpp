/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_world0_startup.cpp Native Vale FIFO admission without generation or simulation. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/world0_scenario.h"
#include "../portal/world0_opening.h"
#include "../portal/first_permanent_line.h"
#include "../portal/integrated_economy.h"
#include "../portal/connected_economy.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_registry.h"
#include "../portal/stellar_network.h"
#include "../portal/commonwealth_pack.h"
#include "../portal/resource_sites.h"
#include "../portal/logistics_hub.h"
#include "../portal/corporate_hq.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../town.h"
#include "../station_base.h"
#include "../industry.h"
#include "../industry_cmd.h"
#include "../newgrf_industries.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../rail_map.h"
#include "../signal_func.h"
#include "../station_map.h"
#include "../void_map.h"
#include "../genworld.h"
#include "../progress.h"
#include "../console_func.h"
#include "../console_internal.h"
#include "../base_media_graphics.h"
#include "../fileio_func.h"
#include "../gfxinit.h"
#include "../window_gui.h"
#include "../window_func.h"
#include "../viewport_func.h"
#include "../saveload/saveload_internal.h"
#include "../story_base.h"
#include "../goal_base.h"
#include "../widgets/toolbar_widget.h"
#include "../table/strings.h"
#include "../timer/timer_game_tick.h"
#include "../core/random_func.hpp"
#include "../network/network.h"
#include "../openttd.h"
#include "../newgrf_config.h"
#include "../newgrf.h"
#include "../newgrf_commons.h"
#include "../cargotype.h"
#include "../driver.h"
#include "../video/null_v.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);
extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;
extern void DetermineBasePaths(std::string_view exe);

namespace {
class ValeQueueDriver : public VideoDriver_Null {
public:
	void Pump() { this->DrainCommandQueue(); }
	std::string_view GetName() const override { return "vale-queue-test"; }
};
class ValeQueueFactory : public DriverFactoryBase {
public:
	ValeQueueFactory() : DriverFactoryBase(Driver::Type::Video, 0, "vale-queue-test", "Vale FIFO queue test") {}
	std::unique_ptr<Driver> CreateInstance() const override { return std::make_unique<ValeQueueDriver>(); }
};
ValeQueueFactory vale_queue_factory;
int marker_calls = 0;
bool Marker(std::span<std::string_view>) { ++marker_calls; return true; }

struct ValeStartupFixture {
	GameSettings settings = _settings_game;
	GRFConfigList configs = std::move(_grfconfig);
	std::array<CargoSpec, NUM_CARGO> cargos;
	IndustrySpec quarry_spec = *GetIndustrySpec(IndustryType{0});
	IndustrySpec crusher_spec = *GetIndustrySpec(IndustryType{2});
	GRFFile industry;
	ValeQueueDriver *driver;
	void RestoreSyntheticContent()
	{
		/* Graphics loading resets NewGRF specs; this remains a fixed native-entity fixture. */
		_grfconfig.clear();
		_industry_mngr.ResetMapping(); _industry_mngr.ResetOverride();
		_industry_mngr.Add(0x10, COMMONWEALTH_INDUSTRY_GRFID, 0);
		_industry_mngr.Add(0x11, COMMONWEALTH_INDUSTRY_GRFID, 2);
		industry.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		for (auto id : {COMMONWEALTH_INDUSTRY_GRFID, COMMONWEALTH_RAIL_GRFID}) {
			auto config = std::make_unique<GRFConfig>();
			config->ident.grfid = id; config->version = id == COMMONWEALTH_INDUSTRY_GRFID ? 5 : 2;
			config->status = GRFStatus::Activated; _grfconfig.push_back(std::move(config));
		}
		for (size_t i = 0; i < NUM_CARGO; ++i) CargoSpec::Get(CargoType(i))->bitnum = INVALID_CARGO_BITNUM;
		for (uint8_t i = 0; i < static_cast<uint8_t>(CommonwealthCargoID::Count); ++i) {
			auto *cargo = CargoSpec::Get(CargoType(16 + i));
			cargo->label = CommonwealthPackManager::GetCargoLabel(static_cast<CommonwealthCargoID>(i));
			cargo->bitnum = i; cargo->grffile = &industry;
		}
		for (auto [i, label] : {std::pair{29, CargoLabel{"SIGE"}}, {30, CargoLabel{"MGLA"}}, {31, CargoLabel{"MACH"}},
				{32, CargoLabel{"OIL_"}}, {33, CargoLabel{"GRAI"}}, {34, CargoLabel{"FOOD"}}, {35, CargoLabel{"BALL"}}}) {
			auto *cargo = CargoSpec::Get(CargoType(i)); cargo->label = label; cargo->bitnum = i; cargo->grffile = &industry;
		}
		BuildCargoLabelMap();
	}
	ValeStartupFixture()
	{
		for (size_t i = 0; i < NUM_CARGO; ++i) cargos[i] = *CargoSpec::Get(CargoType(i));
		SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 0);
		_story_page_element_pool.CleanPool(); _story_page_pool.CleanPool(); _goal_pool.CleanPool();
		ResetIndustries();
		World0Opening::Reset(); FirstPermanentLine::Reset(); IntegratedEconomy::Reset();
		PlanetManager::Reset(); PortalRegistry::Reset(); StellarNetwork::Reset(); CorporateHQManager::Reset();
		ResourceSiteManager::Reset(); LogisticsHubManager::Reset();
		_vehicle_pool.CleanPool(); ResetVehicleHash(); _station_pool.CleanPool();
		_industry_pool.CleanPool(); _town_pool.CleanPool(); _company_pool.CleanPool();
		RebuildTownKdtree(); RebuildStationKdtree();
		UpdateSignalsInBuffer();
		Map::Allocate(256, 256);
		for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
			SetTileHeight(tile, 2);
			if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3); else MakeVoid(tile);
		}
		RestoreSyntheticContent();
		_game_mode = GameMode::Editor; _generating_world = false; SetModalProgress(false);
		_networking = _network_dedicated = false;
		_settings_game.game_creation.generation_seed = 11;
		_settings_game.game_creation.landscape = LandscapeType::Temperate;
		_settings_game.game_creation.cst_sector = false;
		_settings_game.difficulty.infinite_money = false;
		DriverFactoryBase::SelectDriver("vale-queue-test", Driver::Type::Video);
		driver = static_cast<ValeQueueDriver *>(VideoDriver::GetInstance());
		IConsoleInit(); IConsole::CmdRegister("vale_bridge_marker", Marker);
		marker_calls = 0;
	}
	~ValeStartupFixture()
	{
		SetModalProgress(false); _generating_world = false; _pause_mode = {};
		_game_mode = GameMode::Normal;
		UnInitWindowSystem();
		IConsoleFree();
		_story_page_element_pool.CleanPool(); _story_page_pool.CleanPool(); _goal_pool.CleanPool();
		World0Opening::Reset(); FirstPermanentLine::Reset();
		_vehicle_pool.CleanPool(); ResetVehicleHash(); _station_pool.CleanPool();
		ResourceSiteManager::Reset(); LogisticsHubManager::Reset();
		_industry_pool.CleanPool(); _town_pool.CleanPool(); _company_pool.CleanPool();
		IntegratedEconomy::Reset();
		PlanetManager::Reset(); PortalRegistry::Reset(); StellarNetwork::Reset();
		_grfconfig = std::move(configs);
		for (size_t i = 0; i < NUM_CARGO; ++i) *CargoSpec::Get(CargoType(i)) = cargos[i];
		BuildCargoLabelMap();
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0})) = quarry_spec;
		*const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{2})) = crusher_spec;
		_settings_game = settings;
	}
	void Bridge(const std::string &path)
	{
		std::array<std::string_view, 3> argv{"vale", "bridge", path};
		REQUIRE(ConWorld0Scenario(argv));
	}
	uint64_t Terrain() const
	{
		uint64_t hash = 1469598103934665603ULL;
		for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
			hash ^= uint64_t(to_underlying(GetTileType(tile))) | (uint64_t(TileHeight(tile)) << 8);
			hash *= 1099511628211ULL;
		}
		return hash;
	}
	void BindNativeVale()
	{
		REQUIRE(MapNewGRFIndustryType(0x90, COMMONWEALTH_INDUSTRY_GRFID) == IndustryType{0});
		REQUIRE(MapNewGRFIndustryType(0x91, COMMONWEALTH_INDUSTRY_GRFID) == IndustryType{2});
		REQUIRE(Company::CanAllocateItem());
		Company *company = Company::CreateAtIndex(CompanyID{0});
		REQUIRE(company != nullptr); company->money = 1000000;
		_current_company = _local_company = company->index;
		PlanetRegion region; region.id = WorldID{0}; region.name = "Aster Vale";
		region.phase = WorldPhase::Phase3_Frontier; region.biome = WorldBiome::Temperate;
		region.min_x = region.min_y = 1; region.max_x = region.max_y = 254;
		REQUIRE(PlanetManager::RegisterRegion(region));
		std::array<TownID, 3> towns;
		for (uint n = 0; n < towns.size(); ++n) {
			REQUIRE(Town::CanAllocateItem());
			Town *town = Town::Create(TileXY(20 + 20 * n, 20));
			town->cache.population = 200;
			towns[n] = town->index;
		}
		auto *quarry_type = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{0}));
		quarry_type->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		quarry_type->grf_prop.local_id = 0x10;
		quarry_type->produced_cargo.fill(INVALID_CARGO);
		quarry_type->produced_cargo[0] = GetCargoTypeByLabel(CargoLabel{"SILC"});
		auto *crusher_type = const_cast<IndustrySpec *>(GetIndustrySpec(IndustryType{2}));
		crusher_type->grf_prop.grfid = COMMONWEALTH_INDUSTRY_GRFID;
		crusher_type->grf_prop.local_id = 0x11;
		crusher_type->accepts_cargo.fill(INVALID_CARGO);
		crusher_type->accepts_cargo[0] = GetCargoTypeByLabel(CargoLabel{"SILC"});
		crusher_type->produced_cargo.fill(INVALID_CARGO);
		crusher_type->produced_cargo[0] = GetCargoTypeByLabel(CargoLabel{"BALL"});
		uint32_t site = ResourceSiteManager::AddSite(TileXY(38, 23), IndustryType{0}, 8, 8);
		REQUIRE(site != 0);
		World0OpeningProfile profile{company->index, towns, TileXY(30, 30), site};
		REQUIRE(World0Opening::BeginAuthoring(profile));
		REQUIRE(IntegratedEconomy::StartAuthoredWorld0(towns));
		IntegratedEconomy::ConfigureRecipes();
		REQUIRE(Industry::CanAllocateItem());
		Industry *quarry = Industry::Create(TileXY(40, 25));
		quarry->type = IndustryType{0}; quarry->founder = company->index;
		quarry->location = TileArea(TileXY(40, 25), 1, 1);
		quarry->produced.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"SILC"})});
		ResourceSiteManager::Occupy(quarry->location.tile, quarry->type, quarry_type->layouts[0], quarry->index);
		ResourceSiteManager::RestoreSurvey(company->index, TileXY(38, 23));
		REQUIRE(Industry::CanAllocateItem());
		Industry *crusher = Industry::Create(profile.crusher_plot);
		crusher->type = IndustryType{2}; crusher->founder = company->index;
		crusher->location = TileArea(profile.crusher_plot, 1, 1);
		crusher->accepted.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"SILC"})});
		crusher->produced.push_back({.cargo = GetCargoTypeByLabel(CargoLabel{"BALL"})});
		IntegratedEconomy::RegisterIndustry(crusher);
		REQUIRE(Station::CanAllocateItem());
		Station *yard = Station::Create(TileXY(12, 15));
		yard->owner = company->index; yard->town = Town::Get(towns[0]);
		yard->facilities.Set(StationFacility::Train);
		yard->train_station = TileArea(yard->xy, 1, 1); yard->spread = yard->train_station;
		MakeRailStation(yard->xy, company->index, yard->index, Axis::X, 0, RAILTYPE_RAIL);
		company->infrastructure.station++;
		company->infrastructure.rail[RAILTYPE_RAIL]++;
		RebuildStationKdtree(); yard->RecomputeCatchment();
		REQUIRE(LogisticsHubManager::RegisterHub(yard->xy, WorldID{0}, company->index, yard->index, "Greybank Yard") != 0);
		REQUIRE(World0Opening::Begin({quarry->index, crusher->index, yard->index}));
		const std::string saved = World0Opening::Save();
		World0Opening::Reset();
		REQUIRE(World0Opening::Load(saved));
		REQUIRE(World0Opening::ValidateAfterLoad());
		_game_mode = GameMode::Normal;
		_pause_mode.Set(PauseMode::Normal);
	}
};

#ifndef _WIN32
struct ValeFifo {
	std::filesystem::path dir, path;
	ValeFifo()
	{
		char name[] = "/tmp/vale-startup-XXXXXX";
		const char *created = mkdtemp(name); REQUIRE(created != nullptr);
		/* /tmp itself can be a symlink (for example, to /private/tmp on Darwin). */
		dir = std::filesystem::canonical(created);
		REQUIRE(dir.is_absolute());
		REQUIRE(dir == dir.lexically_normal());
		REQUIRE_FALSE(std::filesystem::is_symlink(std::filesystem::symlink_status(dir)));
		path = dir / "bridge.fifo";
		REQUIRE(mkfifo(path.c_str(), 0600) == 0);
		REQUIRE(path == std::filesystem::canonical(path));
		REQUIRE_FALSE(std::filesystem::is_symlink(std::filesystem::symlink_status(path)));
		REQUIRE(std::filesystem::is_fifo(std::filesystem::status(path)));
	}
	~ValeFifo() { std::error_code error; std::filesystem::remove_all(dir, error); }
};

using CompletionJson = nlohmann::json;

/* A fixed native opening, excluding only the Story Book created by completion. */
CompletionJson CompletionState()
{
	CompletionJson custody = GetCoreBasketObservation();
	/* Native UI commands leave only the transient CommandDuringPause bookkeeping bit. */
	custody["pause_mode"] = custody.at("pause_mode").get<uint>() & ~(1u << to_underlying(PauseMode::CommandDuringPause));
	return {{"mission", World0Opening::Save()}, {"economy", IntegratedEconomy::Save()},
		{"custody", custody}, {"money", int64_t(Company::Get(CompanyID{0})->money)},
		{"map", GetConnectedEconomyMapFingerprint()}, {"tick", uint64_t(TimerGameTick::counter)},
		{"rng", {_random.state[0], _random.state[1], _interactive_random.state[0], _interactive_random.state[1]}}};
}

struct ValeCompletionLog {
	std::filesystem::path dir, path;
	bool open = false;
	ValeCompletionLog()
	{
		char name[] = "/tmp/vale-completion-XXXXXX";
		const char *created = mkdtemp(name); REQUIRE(created != nullptr);
		dir = created; path = dir / "receipt.log";
	}
	~ValeCompletionLog()
	{
		if (open) IConsoleCmdExec("script");
		std::error_code error; std::filesystem::remove_all(dir, error);
	}
	void Start() { IConsoleCmdExec(fmt::format("script {}", path.string())); open = true; }
	CompletionJson Finish()
	{
		IConsoleCmdExec("script"); open = false;
		std::ifstream input(path); REQUIRE(input.good());
		std::string line;
		while (std::getline(input, line)) {
			constexpr std::string_view marker = "VALE prepared ";
			auto at = line.find(marker);
			if (at != std::string::npos) return CompletionJson::parse(line.substr(at + marker.size()));
		}
		FAIL("native VALE prepared receipt absent from console log");
		return {};
	}
};

void SelectValeNativeBaseGraphics()
{
	const char *test_binary = std::getenv("OSTTD_TEST_BINARY");
	REQUIRE(test_binary != nullptr);
	DetermineBasePaths(test_binary);
	_valid_searchpaths.clear();
	for (Searchpath path : EnumRange(Searchpath::End)) if (!_searchpaths[path].empty()) _valid_searchpaths.push_back(path);
	TarScanner::DoScan(TarScanner::Mode::Baseset);
	BaseGraphics::FindSets();
	INFO("The isolated viewport test requires real native base graphics");
	REQUIRE(BaseGraphics::SetSet(nullptr));
}

void PumpFromFifo(ValeStartupFixture &fixture, const ValeFifo &fifo)
{
	std::atomic<bool> wrote = false;
	std::thread writer([&] {
		const std::string path = fifo.path.string();
		int fd = -1;
		for (int n = 0; n < 200 && fd < 0; ++n) {
			fd = open(path.c_str(), O_WRONLY | O_NONBLOCK);
			if (fd < 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		if (fd >= 0) {
			constexpr char command[] = "vale_bridge_marker\n";
			wrote = write(fd, command, sizeof(command) - 1) == static_cast<ssize_t>(sizeof(command) - 1);
			close(fd);
		}
	});
	try {
		fixture.driver->Pump();
	} catch (...) {
		writer.join();
		throw;
	}
	writer.join();
	CHECK(wrote.load());
}
#endif
} // namespace

#ifndef _WIN32
TEST_CASE("Vale startup bridge defers a real FIFO exec beyond modal cleanup", "[world0-startup][.world0-startup]")
{
	ValeStartupFixture f; ValeFifo fifo;
	REQUIRE(IntegratedEconomy::ContentReady());
	REQUIRE(PlanetManager::Count() == 0);
	const auto before = f.Terrain();
	SetModalProgress(true); f.Bridge(fifo.path.string());
	f.Bridge(fifo.path.string()); // A duplicate cannot queue a second reader.
	CHECK(_pause_mode.Test(PauseMode::Normal));
	f.driver->Pump(); CHECK(marker_calls == 0);
	SetModalProgress(false);
	PumpFromFifo(f, fifo);
	CHECK(marker_calls == 1); f.driver->Pump(); CHECK(marker_calls == 1);
	_generating_world = true; f.Bridge(fifo.path.string());
	f.driver->Pump(); CHECK(marker_calls == 1);
	_generating_world = false; PumpFromFifo(f, fifo);
	CHECK(marker_calls == 2);
	CHECK(f.Terrain() == before); CHECK(PlanetManager::Count() == 0);
	CHECK_FALSE(IntegratedEconomy::Enabled()); CHECK_FALSE(World0Opening::Active());
	CHECK(_game_mode == GameMode::Editor);
}

TEST_CASE("Vale bridge rejects malformed paths and unrelated startup states without mutation", "[world0-startup][.world0-startup]")
{
	ValeStartupFixture f; ValeFifo fifo;
	const auto before = f.Terrain();
	CHECK_FALSE(_pause_mode.Test(PauseMode::Normal));
	for (std::string path : {std::string("relative.fifo"), std::string("/tmp/quote\".fifo"),
			std::string("/tmp/line\n.fifo"), std::string("/tmp/../tmp/bridge.fifo")}) f.Bridge(path);
	auto regular = fifo.dir / "plain"; { std::ofstream out(regular); out << "vale_bridge_marker\n"; }
	f.Bridge(regular.string());
	auto link = fifo.dir / "link"; std::filesystem::create_symlink(fifo.path, link); f.Bridge(link.string());
	auto ancestor = fifo.dir / "linked-parent";
	std::filesystem::create_directory_symlink(fifo.dir, ancestor);
	REQUIRE(std::filesystem::is_symlink(std::filesystem::symlink_status(ancestor)));
	f.Bridge((ancestor / fifo.path.filename()).string());
	const uint32_t seed = _settings_game.game_creation.generation_seed;
	_settings_game.game_creation.generation_seed = 12; f.Bridge(fifo.path.string()); _settings_game.game_creation.generation_seed = seed;
	_settings_game.game_creation.cst_sector = true; f.Bridge(fifo.path.string()); _settings_game.game_creation.cst_sector = false;
	_networking = true; f.Bridge(fifo.path.string()); _networking = false;
	_network_dedicated = true; f.Bridge(fifo.path.string()); _network_dedicated = false;
	_settings_game.difficulty.infinite_money = true; f.Bridge(fifo.path.string()); _settings_game.difficulty.infinite_money = false;
	MakeVoid(TileXY(10, 10)); f.Bridge(fifo.path.string()); MakeClear(TileXY(10, 10), ClearGround::Grass, 3);
	PlanetRegion world; world.id = WorldID{0}; world.name = "not empty"; world.phase = WorldPhase::Phase3_Frontier;
	world.min_x = world.min_y = 1; world.max_x = world.max_y = 254;
	REQUIRE(PlanetManager::RegisterRegion(world)); f.Bridge(fifo.path.string()); PlanetManager::Reset();
	_game_mode = GameMode::Normal; f.Bridge(fifo.path.string()); _game_mode = GameMode::Editor;
	Map::Allocate(64, 64); f.Bridge(fifo.path.string());
	Map::Allocate(256, 256);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		SetTileHeight(tile, 2);
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 3); else MakeVoid(tile);
	}
	f.driver->Pump(); CHECK(marker_calls == 0);
	CHECK(f.Terrain() == before); CHECK(PlanetManager::Count() == 0);
	CHECK_FALSE(IntegratedEconomy::Enabled()); CHECK_FALSE(World0Opening::Active());
	CHECK_FALSE(_pause_mode.Test(PauseMode::Normal));
}

TEST_CASE("Vale queued bridge rechecks native admission before opening its FIFO", "[world0-startup][.world0-startup]")
{
	ValeStartupFixture f; ValeFifo fifo;
	const auto before = f.Terrain();
	f.Bridge(fifo.path.string());
	_settings_game.game_creation.generation_seed = 12;
	f.driver->Pump();
	CHECK(marker_calls == 0); CHECK(f.Terrain() == before);
	_settings_game.game_creation.generation_seed = 11;
	/* A rejected queued request must release its one-at-a-time reservation. */
	f.Bridge(fifo.path.string());
	_settings_game.game_creation.cst_sector = true;
	f.driver->Pump(); CHECK(marker_calls == 0);
}

TEST_CASE("Vale bridge admits only a validated loaded World 0 opening", "[world0-startup][.world0-startup]")
{
	ValeStartupFixture f; ValeFifo fifo;
	f.BindNativeVale();
	REQUIRE(World0Opening::Active());
	const auto saved = World0Opening::Save();
	const auto before = f.Terrain();
	const Money money = Company::Get(CompanyID{0})->money;
	_pause_mode.Reset(PauseMode::Normal);
	f.Bridge(fifo.path.string()); f.driver->Pump();
	CHECK(marker_calls == 0); CHECK_FALSE(_pause_mode.Test(PauseMode::Normal));
	CHECK(World0Opening::Save() == saved); CHECK(Company::Get(CompanyID{0})->money == money);
	_pause_mode.Set(PauseMode::Normal);
	f.Bridge(fifo.path.string()); PumpFromFifo(f, fifo);
	CHECK(marker_calls == 1); CHECK(World0Opening::Save() == saved); CHECK(f.Terrain() == before);
	CHECK(_game_mode == GameMode::Normal); CHECK(IntegratedEconomy::Enabled());
	REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase4_Expansion));
	f.Bridge(fifo.path.string()); f.driver->Pump(); CHECK(marker_calls == 1);
	REQUIRE(PlanetManager::SetWorldPhase(WorldID{0}, WorldPhase::Phase3_Frontier));
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V1);
	f.Bridge(fifo.path.string()); f.driver->Pump(); CHECK(marker_calls == 1);
	IntegratedEconomy::SetRulesVersion(EconomyRulesVersion::V2);
	World0Opening::OnCompanyChanged(CompanyID{0}, CompanyID::Invalid());
	REQUIRE(World0Opening::State().unavailable);
	REQUIRE(World0Opening::ValidateAfterLoad());
	f.Bridge(fifo.path.string()); PumpFromFifo(f, fifo);
	CHECK(marker_calls == 2); CHECK(World0Opening::State().unavailable);
	World0Opening::Reset();
	f.Bridge(fifo.path.string()); f.driver->Pump(); CHECK(marker_calls == 2);
}

TEST_CASE("Vale completion emits its native briefing and receipt without a main viewport", "[world0-startup][.world0-startup]")
{
	ValeStartupFixture f;
	f.BindNativeVale();
	REQUIRE(FindWindowById(WindowClass::MainWindow, 0) == nullptr);
	REQUIRE(StoryPage::GetNumItems() == 0);
	REQUIRE(StoryPageElement::GetNumItems() == 0);
	REQUIRE(Goal::GetNumItems() == 0);
	const auto before = CompletionState();
	_game_mode = GameMode::Editor;
	CHECK_FALSE(FinishWorld0Authoring(1234, 5678));
	CHECK(CompletionState() == before);
	CHECK(StoryPage::GetNumItems() == 0);
	CHECK(StoryPageElement::GetNumItems() == 0);
	CHECK(Goal::GetNumItems() == 0);
	_game_mode = GameMode::Normal;
	ValeCompletionLog log; log.Start();
	REQUIRE(FinishWorld0Authoring(1234, 5678));
	const auto receipt = log.Finish();
	CHECK(receipt.at("stake") == 750000);
	CHECK(receipt.at("imported_stel") == 80);
	CHECK(receipt.at("route_quote") == 1234);
	CHECK(receipt.at("two_service_quote") == 5678);
	CHECK(receipt.at("baseline").at("mission") == CompletionJson::parse(World0Opening::Save()));
	CHECK(receipt.at("baseline").at("story_pages").size() == 1);
	CHECK(receipt.at("baseline").at("story_elements").size() == 13);
	CHECK(receipt.at("baseline").at("goals").size() == 3);
	CHECK(StoryPage::GetNumItems() == 1);
	CHECK(StoryPageElement::GetNumItems() == 13);
	CHECK(Goal::GetNumItems() == 3);
	CHECK(CompletionState() == before);
	CHECK(_pause_mode.Test(PauseMode::Normal));
	CHECK(World0Opening::State().economy_evaluations == 0);
	const auto completed = CompletionState();
	CHECK_FALSE(FinishWorld0Authoring(1234, 5678));
	CHECK(CompletionState() == completed);
	CHECK(StoryPage::GetNumItems() == 1);
	CHECK(StoryPageElement::GetNumItems() == 13);
	CHECK(Goal::GetNumItems() == 3);
	SaveViewportBeforeSaveGame();
	CHECK(_saved_scrollpos_x == std::numeric_limits<int>::max());
	CHECK(_saved_scrollpos_y == std::numeric_limits<int>::max());
}

TEST_CASE("Vale completion replaces editor chrome and scrolls a real main viewport", "[world0-startup][.world0-startup]")
{
	ValeStartupFixture f;
	SelectValeNativeBaseGraphics();
	_grfconfig.clear();
	GfxLoadSprites();
	ResetIndustries();
	f.RestoreSyntheticContent();
	SetupColoursAndInitialWindow();
	Window *main = FindWindowById(WindowClass::MainWindow, 0);
	REQUIRE(main != nullptr);
	REQUIRE(main->viewport != nullptr);
	Window *toolbar = FindWindowById(WindowClass::MainToolbar, 0);
	REQUIRE(toolbar != nullptr);
	REQUIRE(toolbar->GetWidget<NWidgetCore>(WID_TE_LAND_GENERATE) != nullptr);
	CHECK(toolbar->GetWidget<NWidgetCore>(WID_TE_LAND_GENERATE)->GetToolTip() == STR_SCENEDIT_TOOLBAR_LANDSCAPE_GENERATION_TOOLTIP);
	f.BindNativeVale();
	const int scroll_before_x = main->viewport->scrollpos_x;
	const int scroll_before_y = main->viewport->scrollpos_y;
	const auto before = CompletionState();
	ValeCompletionLog log; log.Start();
	REQUIRE(FinishWorld0Authoring(2345, 6789));
	const auto receipt = log.Finish();
	CHECK(receipt.at("baseline").at("story_pages").size() == 1);
	CHECK(receipt.at("baseline").at("story_elements").size() == 13);
	CHECK(receipt.at("baseline").at("goals").size() == 3);
	CHECK(receipt.at("route_quote") == 2345);
	CHECK(receipt.at("two_service_quote") == 6789);
	REQUIRE(FindWindowById(WindowClass::MainWindow, 0) == main);
	toolbar = FindWindowById(WindowClass::MainToolbar, 0);
	REQUIRE(toolbar != nullptr);
	REQUIRE(toolbar->GetWidget<NWidgetCore>(WID_TN_GOAL) != nullptr);
	CHECK(toolbar->GetWidget<NWidgetCore>(WID_TN_GOAL)->GetToolTip() == STR_TOOLBAR_TOOLTIP_PAUSE_GAME + WID_TN_GOAL);
	CHECK(toolbar->GetWidget<NWidgetCore>(WID_TN_GOAL)->GetToolTip() != STR_SCENEDIT_TOOLBAR_LANDSCAPE_GENERATION_TOOLTIP);
	CHECK(FindWindowById(WindowClass::Statusbar, 0) != nullptr);
	CHECK((main->viewport->scrollpos_x != scroll_before_x || main->viewport->scrollpos_y != scroll_before_y));
	CHECK(main->viewport->dest_scrollpos_x == main->viewport->scrollpos_x);
	CHECK(main->viewport->dest_scrollpos_y == main->viewport->scrollpos_y);
	SaveViewportBeforeSaveGame();
	CHECK(_saved_scrollpos_x == main->viewport->scrollpos_x);
	CHECK(_saved_scrollpos_y == main->viewport->scrollpos_y);
	CHECK(_saved_scrollpos_zoom == main->viewport->zoom);
	CHECK(CompletionState() == before);
	CHECK(_pause_mode.Test(PauseMode::Normal));
	CHECK(World0Opening::State().economy_evaluations == 0);
}
#endif
