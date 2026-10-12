/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_world0_script.cpp Compile the optional story scripts without running a game. */
#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../3rdparty/squirrel/include/squirrel.h"
#include "../game/game_instance.hpp"
#include "../script/api/script_object.hpp"
#include "../script/squirrel.hpp"
#include <fstream>
#include <filesystem>
#include "../safeguards.h"
struct ValeScriptScope : ScriptObject {
	GameInstance game{};
	ActiveInstance active{game};
	Squirrel engine{"vale-script-parser"};
	ScriptAllocatorScope scope{&engine};
};
TEST_CASE("Vale GameScript files compile in the native Squirrel parser", "[world0-script]")
{
	ValeScriptScope scope;
	for (const char *name : {"main.nut", "info.nut"}) {
		std::filesystem::path path = std::filesystem::path("bin/game/openspacettd_vale") / name;
		if (!std::filesystem::exists(path)) path = std::filesystem::path("../bin/game/openspacettd_vale") / name;
		std::ifstream file(path);
		REQUIRE(file.good());
		const std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
		HSQUIRRELVM vm = scope.engine.GetVM();
		REQUIRE(vm != nullptr);
		const auto result = sq_compilebuffer(vm, source, name, SQFalse);
		CHECK(SQ_SUCCEEDED(result));
	}
}
