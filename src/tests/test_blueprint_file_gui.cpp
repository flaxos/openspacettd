/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */
/** @file test_blueprint_file_gui.cpp Real file-dialog callback coverage using a private library. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"
#include "../blueprint/blueprint_manager.h"
#include "../blueprint/blueprint_gui.h"
#include "../map_func.h"
#include "../clear_map.h"
#include "../void_map.h"
#include "../rail_map.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../economy_func.h"
#include "../openttd.h"
#include "../rail.h"
#include "../fileio_func.h"
#include "../strings_func.h"
#include "../language.h"
#include "../gfx_func.h"
#include "../driver.h"
#include "../video/video_driver.hpp"
#include "../window_gui.h"
#include "../window_func.h"
#include "../textbuf_gui.h"
#include "../querystring_gui.h"
#include "../widgets/blueprint_widget.h"
#include "../widgets/misc_widget.h"
#include "../table/strings.h"
#include "../table/sprites.h"
#include <filesystem>
#include <fstream>
#include <chrono>

extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;

namespace {
class BlueprintFileGUIFixture {
	decltype(_searchpaths) paths = _searchpaths;
	std::vector<Searchpath> valid = _valid_searchpaths;
public:
	std::filesystem::path root;
	BlueprintFileGUIFixture()
	{
		(void)MockEnvironment::Instance();
		if (_current_language == nullptr) {
			_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
			_valid_searchpaths = {Searchpath::BinaryDir};
			InitializeLanguagePacks();
		}
		UnInitWindowSystem();
		/* Real editbox focus requires a video driver. These hidden Catch cases are
		 * run as separate CTest processes, avoiding driver leakage into save tests. */
		if (VideoDriver::GetInstance() == nullptr) DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
		_screen.width = _screen.pitch = 1024;
		_screen.height = 768;
		ScreenSizeChanged();
		SetMouseCursor(SPR_CURSOR_MOUSE, PAL_NONE);
		InitWindowSystem();
		root = std::filesystem::temp_directory_path() / ("ost-bp-gui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		REQUIRE(std::filesystem::create_directory(root));
		_searchpaths[Searchpath::PersonalDir] = root.string() + "/";
		_valid_searchpaths = {Searchpath::PersonalDir};
		BlueprintManager::Reset();
	}
	~BlueprintFileGUIFixture()
	{
		UnInitWindowSystem();
		BlueprintManager::Reset();
		_searchpaths = std::move(paths);
		_valid_searchpaths = std::move(valid);
		std::error_code ec;
		std::filesystem::remove_all(root, ec);
	}
};

Window *LibraryWindow()
{
	ShowBlueprintLibrary();
	Window *window = FindWindowById(WindowClass::BlueprintLibrary, 0);
	REQUIRE(window != nullptr);
	return window;
}

void SubmitFileQuery(Window *library, WidgetID action, const std::string &value)
{
	library->OnClick({}, action, 1);
	Window *query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(query != nullptr);
	REQUIRE(query->querystrings.count(WID_QS_TEXT) == 1);
	query->querystrings[WID_QS_TEXT]->text.Assign(value);
	query->OnClick({}, WID_QS_OK, 1); // Native query acceptance calls the real library callback.
}

std::string Status(Window *window) { return window->GetWidgetString(WID_BPL_STATUS_BAR, STR_NULL); }
std::string Bytes(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

TEST_CASE("Blueprint GUI captures a selected map rectangle and saves the chosen name", "[.][blueprint-file-gui][wp07]")
{
	BlueprintFileGUIFixture fixture;
	Map::Allocate(64, 64);
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) {
		if (IsInnerTile(tile)) MakeClear(tile, ClearGround::Grass, 0);
		else MakeVoid(tile);
	}
	_company_pool.CleanPool();
	Company *company = Company::CreateAtIndex(CompanyID{0});
	REQUIRE(company != nullptr);
	_current_company = _local_company = company->index;
	_game_mode = GameMode::Normal;
	ResetRailTypes();
	company->avail_railtypes.Set(RAILTYPE_RAIL);
	company->money = 1000000;
	company->clear_limit = 1000 << 16;
	_price[Price::BuildRail] = 100;
	TileIndex first = TileXY(20, 20);
	TileIndex last = TileXY(21, 20);
	MakeRailNormal(first, company->index, TrackBits{Track::X}, RAILTYPE_RAIL);
	MakeRailNormal(last, company->index, TrackBits{Track::X}, RAILTYPE_RAIL);
	Window *window = LibraryWindow();
	const size_t before = BlueprintManager::GetBlueprints().size();
	/* Switching away from placement, or clicking Capture twice, must leave
	 * capture active after the previous tool's native abort callback. */
	window->OnClick({}, WID_BPL_PLACE, 1);
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	window->OnPlaceObject({}, first);
	window->OnPlaceDrag(VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT,
		{static_cast<int>(TileX(last) * TILE_SIZE), static_cast<int>(TileY(last) * TILE_SIZE)});
	window->OnPlaceMouseUp(VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT, {1, 1}, first, last);
	Window *query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(query != nullptr);
	query->querystrings[WID_QS_TEXT]->text.Assign("GUI Captured Rail");
	query->OnClick({}, WID_QS_OK, 1);
	REQUIRE(BlueprintManager::GetBlueprints().size() == before + 1);
	CHECK(BlueprintManager::GetBlueprints().back().name == "GUI Captured Rail");
	CHECK(BlueprintManager::GetBlueprints().back().GetTrackPieceCount() == 2);
	CHECK(Status(window).find("Captured and saved") == 0);
	const size_t saved_count = BlueprintManager::GetBlueprints().size();
	/* The reverse switch and a repeated Place click must still build the
	 * selected two-tile capture through the real command dispatcher. */
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	window->OnClick({}, WID_BPL_PLACE, 1);
	window->OnClick({}, WID_BPL_PLACE, 1);
	window->OnPlaceObject({}, TileXY(30, 20));
	for (TileIndex tile : {TileXY(30, 20), TileXY(31, 20)}) {
		REQUIRE(IsPlainRailTile(tile));
		CHECK(GetTrackBits(tile) == TrackBits{Track::X});
		CHECK(GetTileOwner(tile) == company->index);
	}
	CHECK(company->money == 1000000 - 200);
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	TileIndex empty = TileXY(30, 30);
	window->OnPlaceObject({}, empty);
	window->OnPlaceMouseUp(VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT, {1, 1}, empty, TileXY(31, 30));
	CHECK(BlueprintManager::GetBlueprints().size() == saved_count);
	CHECK(Status(window).find("Capture failed") == 0);
	MakeRailNormal(last, CompanyID{1}, TrackBits{Track::X}, RAILTYPE_RAIL);
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	window->OnPlaceObject({}, first);
	window->OnPlaceMouseUp(VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT, {1, 1}, first, last);
	CHECK(BlueprintManager::GetBlueprints().size() == saved_count);
	CHECK(Status(window).find("Capture failed") == 0);
	MakeRailNormal(last, company->index, TrackBits{Track::X}, RAILTYPE_RAIL);
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	window->OnPlaceObject({}, first);
	window->OnPlaceMouseUp(VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT, {1, 1}, first, last);
	query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(query != nullptr);
	query->OnClick({}, WID_QS_CANCEL, 1);
	CHECK(BlueprintManager::GetBlueprints().size() == saved_count);
	window->OnClick({}, WID_BPL_CAPTURE, 1);
	window->OnPlaceObject({}, first);
	window->OnPlaceObjectAbort();
	window->OnPlaceMouseUp(VPM_X_AND_Y, DDSP_CAPTURE_BLUEPRINT, {1, 1}, first, last);
	CHECK(FindWindowByClass(WindowClass::QueryString) == nullptr);
	CHECK(BlueprintManager::GetBlueprints().size() == saved_count);
	CHECK(Status(window).find("Map selection cancelled") == 0);
}

TEST_CASE("Blueprint GUI exports the transformed selection and imports a real file", "[.][blueprint-file-gui]")
{
	BlueprintFileGUIFixture fixture;
	Window *window = LibraryWindow();
	const Blueprint original = *BlueprintManager::GetBlueprint(0);
	window->OnClick({}, WID_BPL_ROTATE, 1);
	window->OnClick({}, WID_BPL_FLIP, 1);
	const Blueprint expected = original.Rotate().Mirror();
	const auto exported = fixture.root / "shared.json";
	SubmitFileQuery(window, WID_BPL_EXPORT, exported.string());
	REQUIRE(std::filesystem::is_regular_file(exported));
	CHECK(Bytes(exported) == expected.ToJson());
	CHECK(Status(window).find(exported.string()) != std::string::npos);
	CHECK(Status(window).find("Exported") == 0);
	const size_t before = BlueprintManager::GetBlueprints().size();
	SubmitFileQuery(window, WID_BPL_IMPORT, exported.string());
	REQUIRE(BlueprintManager::GetBlueprints().size() == before + 1);
	auto imported = BlueprintManager::GetBlueprints().back();
	CHECK_FALSE(imported.is_builtin);
	imported.is_builtin = expected.is_builtin;
	CHECK(imported.ToJson() == expected.ToJson());
	CHECK(Status(window).find("Imported") == 0);
	SubmitFileQuery(window, WID_BPL_RENAME, "Imported module");
	CHECK(BlueprintManager::GetBlueprints().back().name == "Imported module");
	CHECK(Status(window).find("Renamed") == 0);
	CloseWindowByClass(WindowClass::BlueprintLibrary);
	BlueprintManager::Reset();
	window = LibraryWindow();
	CHECK(BlueprintManager::GetBlueprints().back().name == "Imported module");
}

TEST_CASE("Blueprint GUI reports invalid imports collisions and export failures truthfully", "[.][blueprint-file-gui]")
{
	BlueprintFileGUIFixture fixture;
	Window *window = LibraryWindow();
	const size_t before = BlueprintManager::GetBlueprints().size();
	const auto malformed = fixture.root / "malformed.json";
	{ std::ofstream out(malformed); out << "{broken"; }
	SubmitFileQuery(window, WID_BPL_IMPORT, malformed.string());
	CHECK(BlueprintManager::GetBlueprints().size() == before);
	CHECK(Status(window).find("Import failed:") == 0);
	CHECK(Bytes(malformed) == "{broken");
	const auto invalid_enum = fixture.root / "invalid-signal.json";
	{ std::ofstream out(invalid_enum); out << R"({"format":"OpenSpaceTTD_Blueprint","version":1,"name":"Invalid signal","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":0,"railtype":0,"trackbits":1,"signals":[{"track":255,"sigtype":0,"sigvar":0,"signals_copy":1}]}]})"; }
	SubmitFileQuery(window, WID_BPL_IMPORT, invalid_enum.string());
	CHECK(BlueprintManager::GetBlueprints().size() == before);
	CHECK(Status(window).find("Import failed:") == 0);
	SubmitFileQuery(window, WID_BPL_EXPORT, malformed.string());
	CHECK(Status(window).find("Export failed:") == 0);
	CHECK(Bytes(malformed) == "{broken");
	SubmitFileQuery(window, WID_BPL_EXPORT, (fixture.root / "missing" / "new.json").string());
	CHECK(Status(window).find("Export failed:") == 0);
	CHECK_FALSE(std::filesystem::exists(fixture.root / "missing"));
	const auto shared = fixture.root / "valid.json";
	SubmitFileQuery(window, WID_BPL_EXPORT, shared.string());
	SubmitFileQuery(window, WID_BPL_IMPORT, shared.string());
	REQUIRE(BlueprintManager::GetBlueprints().size() == before + 1);
	const std::string state = BlueprintManager::GetBlueprints().back().ToJson();
	SubmitFileQuery(window, WID_BPL_IMPORT, shared.string());
	CHECK(BlueprintManager::GetBlueprints().size() == before + 1);
	CHECK(BlueprintManager::GetBlueprints().back().ToJson() == state);
	CHECK(Status(window).find("Import failed:") == 0);
	window->OnClick({}, WID_BPL_EXPORT, 1);
	Window *query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(query != nullptr);
	query->OnClick({}, WID_QS_CANCEL, 1);
	CHECK(Status(window) == "Operation cancelled.");
	CHECK(BlueprintManager::GetBlueprints().size() == before + 1);
}

TEST_CASE("Blueprint GUI default export remains separate from the library scan", "[.][blueprint-file-gui]")
{
	BlueprintFileGUIFixture fixture;
	Window *window = LibraryWindow();
	const size_t before = BlueprintManager::GetBlueprints().size();
	window->OnClick({}, WID_BPL_EXPORT, 1);
	Window *query = FindWindowByClass(WindowClass::QueryString);
	REQUIRE(query != nullptr);
	const std::filesystem::path path(query->querystrings[WID_QS_TEXT]->text.GetText());
	CHECK(path.parent_path() == fixture.root);
	query->OnClick({}, WID_QS_OK, 1);
	REQUIRE(std::filesystem::is_regular_file(path));
	CHECK(Status(window).find("Exported") == 0);
	CloseWindowByClass(WindowClass::BlueprintLibrary);
	BlueprintManager::Reset();
	window = LibraryWindow();
	CHECK(BlueprintManager::GetBlueprints().size() == before);
	SubmitFileQuery(window, WID_BPL_IMPORT, path.string());
	CHECK(BlueprintManager::GetBlueprints().size() == before + 1);
	const auto broken = fixture.root / "blueprint" / "broken.json";
	{ std::ofstream out(broken); out << "malformed"; }
	CloseWindowByClass(WindowClass::BlueprintLibrary);
	window = LibraryWindow();
	CHECK(Status(window).find("Library scan:") == 0);
	CHECK(Status(window).find("broken.json") != std::string::npos);
	CHECK(BlueprintManager::GetBlueprints().size() == before + 1);
}
