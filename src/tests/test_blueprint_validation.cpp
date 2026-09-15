/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_blueprint_validation.cpp Untrusted blueprint admission and transform regressions. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../blueprint/blueprint.h"
#include "../signal_func.h"
#include "../track_func.h"

#include <string>
#include <vector>

static Blueprint ValidBlueprint()
{
	Blueprint bp;
	bp.name = "Valid layout";
	bp.width = 1;
	bp.height = 1;
	BlueprintTile tile;
	tile.trackbits = TrackBits{Track::X};
	bp.tiles.push_back(tile);
	return bp;
}

static std::string ValidJson()
{
	return ValidBlueprint().ToJson();
}

TEST_CASE("Blueprint strict JSON schema rejects malformed values", "[blueprint][blueprint-validation]")
{
	const std::string base = ValidJson();
	const std::vector<std::pair<std::string, std::string>> substitutions = {
		{"\"version\": 1", "\"version\": 2"},
		{"\"version\": 1", "\"version\": true"},
		{"\"layout_revision\": 1", "\"layout_revision\": 0"},
		{"\"layout_revision\": 1", "\"layout_revision\": -1"},
		{"\"layout_revision\": 1", "\"layout_revision\": true"},
		{"\"layout_revision\": 1", "\"layout_revision\": 4294967296"},
		{"\"width\": 1", "\"width\": 1.5"},
		{"\"width\": 1", "\"width\": 65537"},
		{"\"height\": 1", "\"height\": -1"},
		{"\"dx\": 0", "\"dx\": 4294967296"},
		{"\"type\": 0", "\"type\": 256"},
		{"\"railtype\": 0", "\"railtype\": 64"},
		{"\"trackbits\": 1", "\"trackbits\": 128"},
		{"\"trackbits\": 1", "\"trackbits\": false"},
		{"\"is_builtin\": false", "\"is_builtin\": 0"},
		{"\"description\": \"\"", "\"description\": 42"},
		{"\"created_time\": 0", "\"created_time\": 18446744073709551615"},
	};
	for (const auto &[before, after] : substitutions) {
		std::string changed = base;
		auto pos = changed.find(before);
		REQUIRE(pos != std::string::npos);
		changed.replace(pos, before.size(), after);
		CHECK_FALSE(Blueprint::FromJson(changed).has_value());
	}

	const std::vector<std::string> malformed = {
		R"({"format":"OpenSpaceTTD_Blueprint","version":1,"name":"a","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":0,"railtype":0}]})",
		R"({"format":"OpenSpaceTTD_Blueprint","version":1,"name":"a","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":1,"railtype":0}]})",
		R"({"format":"OpenSpaceTTD_Blueprint","version":1,"name":"a","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":2,"railtype":0,"axis":0,"spec_class":0}]})",
		R"({"format":"OpenSpaceTTD_Blueprint","version":1,"name":"a","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":0,"railtype":0,"trackbits":1,"signals":{}}]})",
		R"({"format":"OpenSpaceTTD_Blueprint","version":1,"name":"a","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":0,"railtype":0,"trackbits":1,"dir":0}]})",
		R"({"format":"OpenSpaceTTD_Blueprint","version":1,"version":1,"name":"a","width":1,"height":1,"tiles":[{"dx":0,"dy":0,"type":0,"railtype":0,"trackbits":1}]})",
	};
	for (const auto &input : malformed) CHECK_FALSE(Blueprint::FromJson(input).has_value());
	Blueprint signaled = ValidBlueprint();
	signaled.tiles.front().signals.push_back(BlueprintSignal{Track::X, SignalType::Block, SignalVariant::Electric, SignalAlongTrackdir(Trackdir::X_NE)});
	REQUIRE(signaled.IsValid());
	const std::string signal_json = signaled.ToJson();
	const std::vector<std::pair<std::string, std::string>> signal_substitutions = {
		{"\"track\": 0", "\"track\": 6"},
		{"\"sigtype\": 0", "\"sigtype\": 6"},
		{"\"sigvar\": 0", "\"sigvar\": 2"},
		{"\"signals_copy\": " + std::to_string(signaled.tiles.front().signals.front().signals_copy), "\"signals_copy\": 256"},
	};
	for (const auto &[before, after] : signal_substitutions) {
		std::string changed = signal_json;
		auto pos = changed.find(before);
		REQUIRE(pos != std::string::npos);
		changed.replace(pos, before.size(), after);
		CHECK_FALSE(Blueprint::FromJson(changed).has_value());
	}
	CHECK_FALSE(Blueprint::FromJson(std::string(Blueprint::MAX_JSON_BYTES + 1, ' ')).has_value());
	CHECK_FALSE(Blueprint::FromJson(std::string(Blueprint::MAX_JSON_DEPTH + 1, '[') + "0" + std::string(Blueprint::MAX_JSON_DEPTH + 1, ']')).has_value());
}

TEST_CASE("Blueprint central validity protects transforms", "[blueprint][blueprint-validation]")
{
	Blueprint valid = ValidBlueprint();
	REQUIRE(valid.IsValid());
	CHECK(Blueprint::FromJson(valid.ToJson()).has_value());
	CHECK(valid.Rotate(1).IsValid());
	CHECK(valid.Mirror().IsValid());

	std::vector<Blueprint> invalid;
	Blueprint changed = valid;
	changed.version = 2; invalid.push_back(changed);
	changed = valid; changed.width = 65; invalid.push_back(changed);
	changed = valid; changed.name.clear(); invalid.push_back(changed);
	changed = valid; changed.tiles.push_back(changed.tiles.front()); invalid.push_back(changed);
	changed = valid; changed.tiles.front().trackbits = TrackBits{0x80}; invalid.push_back(changed);
	changed = valid; changed.tiles.front().type = static_cast<BlueprintTileType>(255); invalid.push_back(changed);
	changed = valid; changed.tiles.front().railtype = INVALID_RAILTYPE; invalid.push_back(changed);
	changed = valid; changed.tiles.front().signals.push_back(BlueprintSignal{Track::Invalid, SignalType::Block, SignalVariant::Electric, 1}); invalid.push_back(changed);
	changed = valid; changed.tiles.front().signals.push_back(BlueprintSignal{Track::Y, SignalType::Block, SignalVariant::Electric, 1}); invalid.push_back(changed);
	changed = valid; changed.tiles.front().signals.push_back(BlueprintSignal{Track::X, SignalType::Block, SignalVariant::Electric, 0}); invalid.push_back(changed);
	changed = valid; changed.tiles.front().trackbits = TRACK_BIT_CROSS; changed.tiles.front().signals.push_back(BlueprintSignal{Track::X, SignalType::Block, SignalVariant::Electric, SignalAlongTrackdir(Trackdir::X_NE)}); invalid.push_back(changed);
	for (const auto &bp : invalid) {
		CHECK_FALSE(bp.IsValid());
		CHECK_FALSE(bp.Rotate(1).IsValid());
		CHECK_FALSE(bp.Rotate(0).IsValid());
		CHECK_FALSE(bp.Mirror().IsValid());
	}
}

TEST_CASE("Blueprint v1 bounds and metadata round trip", "[blueprint][blueprint-validation]")
{
	Blueprint bp = ValidBlueprint();
	bp.width = Blueprint::MAX_DIMENSION;
	bp.height = Blueprint::MAX_DIMENSION;
	bp.name = std::string(Blueprint::MAX_NAME_BYTES, 'n');
	bp.description = std::string(Blueprint::MAX_METADATA_BYTES, 'd');
	bp.author = std::string(Blueprint::MAX_METADATA_BYTES, 'a');
	bp.tiles.clear();
	for (int dy = 0; dy < Blueprint::MAX_DIMENSION; ++dy) {
		for (int dx = 0; dx < Blueprint::MAX_DIMENSION; ++dx) {
			BlueprintTile tile;
			tile.dx = dx;
			tile.dy = dy;
			tile.trackbits = TrackBits{Track::X};
			bp.tiles.push_back(tile);
		}
	}
	REQUIRE(bp.tiles.size() == Blueprint::MAX_TILES);
	REQUIRE(bp.IsValid());
	std::string output = bp.ToJson();
	REQUIRE(output.size() <= Blueprint::MAX_JSON_BYTES);
	auto loaded = Blueprint::FromJson(output);
	REQUIRE(loaded.has_value());
	CHECK(loaded->IsValid());
	CHECK(loaded->tiles.size() == bp.tiles.size());
	CHECK(loaded->name == bp.name);
	CHECK(loaded->description == bp.description);
	CHECK(loaded->author == bp.author);
}
