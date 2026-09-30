/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */
/** @file test_tooltips.cpp Regression coverage for empty decoded tooltip layout. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "mock_environment.h"
#include "../driver.h"
#include "../fileio_func.h"
#include "../gfx_func.h"
#include "../language.h"
#include "../strings_func.h"
#include "../video/video_driver.hpp"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_type.h"
#include "../table/strings.h"
#include <filesystem>

extern EnumIndexArray<std::string, Searchpath, Searchpath::End> _searchpaths;

TEST_CASE("Tooltips reject empty rendered text and preserve visible help", "[.][gui][tooltip]")
{
	(void)MockEnvironment::Instance();
	_searchpaths[Searchpath::BinaryDir] = std::filesystem::exists("build/lang/english.lng") ? "build/" : "./";
	_valid_searchpaths = {Searchpath::BinaryDir};
	InitializeLanguagePacks();
	DriverFactoryBase::SelectDriver("null", Driver::Type::Video);
	_screen.width = _screen.pitch = 2560;
	_screen.height = 1389;
	ScreenSizeChanged();
	InitWindowSystem();
	_cursor.in_window = true;
	_cursor.pos = {400, 200};

	for (int scale : {100, 150, 200}) {
		_gui_scale_cfg = scale;
		UpdateGUIZoom();
		SetupWidgetDimensions();
		CAPTURE(scale);
		for (auto condition : {TooltipCloseCondition::Hover, TooltipCloseCondition::RightClick}) {
			GuiShowTooltips(nullptr, GetEncodedString(STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), condition);
			REQUIRE(FindWindowById(WindowClass::ToolTips, 0) != nullptr);
			/* STR_EMPTY has nonempty encoded bytes, but decodes to no visible text.
			 * Replacing an existing tooltip must also close it. */
			auto empty = GetEncodedString(STR_EMPTY);
			REQUIRE_FALSE(empty.empty());
			REQUIRE(empty.GetDecodedString().empty());
			GuiShowTooltips(nullptr, std::move(empty), condition);
			CHECK(FindWindowById(WindowClass::ToolTips, 0) == nullptr);
			for (const std::string &text : {std::string{}, std::string{"\n"}}) {
				GuiShowTooltips(nullptr, GetEncodedString(STR_JUST_RAW_STRING, text), condition);
				CHECK(FindWindowById(WindowClass::ToolTips, 0) == nullptr);
			}
			GuiShowTooltips(nullptr, GetEncodedString(STR_JUST_RAW_STRING, std::string{"Normal help\nSecond line"}), condition);
			auto *tooltip = FindWindowById(WindowClass::ToolTips, 0);
			REQUIRE(tooltip != nullptr);
			CHECK(tooltip->width > 0);
			CHECK(tooltip->height > 0);
			GuiShowTooltips(nullptr, {}, condition);
			CHECK(FindWindowById(WindowClass::ToolTips, 0) == nullptr);
		}
	}
	UnInitWindowSystem();
}
