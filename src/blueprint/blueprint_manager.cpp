/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint_manager.cpp Implementation of blueprint repository, disk storage, and area capture. */

#include "../stdafx.h"
#include "blueprint_manager.h"
#include "../rail_map.h"
#include "../station_map.h"
#include "../fileio_func.h"
#include "../portal/portal_registry.h"
#include "../core/format.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>

static std::vector<Blueprint> _blueprints;
static std::vector<Blueprint> _builtins;
static bool _initialized = false;

static std::string CleanBlueprintFilename(const std::string &name)
{
	std::string clean;
	for (char c : name) {
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ') {
			clean += c;
		} else {
			clean += '_';
		}
	}
	if (clean.empty()) clean = "blueprint";
	return clean;
}

static void CreateSampleBuiltin()
{
	if (!_builtins.empty()) return;

	/* Create a sample CST double-track straight module (8 tiles long along X axis) */
	Blueprint cst_double;
	cst_double.name = "CST Mainline Double Straight";
	cst_double.description = "Commonwealth Standard Transit parallel double track (8 tiles, X-axis, with block signals).";
	cst_double.author = "Commonwealth Transit Authority";
	cst_double.version = 1;
	cst_double.width = 8;
	cst_double.height = 2;
	cst_double.is_builtin = true;

	for (int16_t x = 0; x < 8; ++x) {
		/* Track 1: Bound SE */
		BlueprintTile t1;
		t1.dx = x;
		t1.dy = 0;
		t1.type = BlueprintTileType::Track;
		t1.railtype = RAILTYPE_BEGIN;
		t1.trackbits = TrackBits{Track::X};
		if (x == 4) {
			BlueprintSignal s;
			s.track = Track::X;
			s.sigtype = SignalType::PathOneWay;
			s.sigvar = SignalVariant::Electric;
			s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
			t1.signals.push_back(s);
		}
		cst_double.tiles.push_back(t1);

		/* Track 2: Bound NW */
		BlueprintTile t2;
		t2.dx = x;
		t2.dy = 1;
		t2.type = BlueprintTileType::Track;
		t2.railtype = RAILTYPE_BEGIN;
		t2.trackbits = TrackBits{Track::X};
		if (x == 4) {
			BlueprintSignal s;
			s.track = Track::X;
			s.sigtype = SignalType::PathOneWay;
			s.sigvar = SignalVariant::Electric;
			s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
			t2.signals.push_back(s);
		}
		cst_double.tiles.push_back(t2);
	}

	_builtins.push_back(cst_double);
}

void BlueprintManager::Initialize()
{
	if (_initialized) return;
	_initialized = true;

	CreateSampleBuiltin();
	RescanLibrary();
}

void BlueprintManager::Reset()
{
	_blueprints.clear();
	_builtins.clear();
	_initialized = false;
}

void BlueprintManager::RegisterBuiltin(const Blueprint &bp)
{
	Blueprint copy = bp;
	copy.is_builtin = true;
	_builtins.push_back(copy);

	/* Add to active list if not already present */
	auto it = std::find_if(_blueprints.begin(), _blueprints.end(), [&](const Blueprint &b) {
		return b.is_builtin && b.name == copy.name;
	});
	if (it == _blueprints.end()) {
		_blueprints.push_back(copy);
	}
}

void BlueprintManager::RescanLibrary()
{
	_blueprints.clear();

	/* Add all registered built-in prefabs first */
	for (const auto &b : _builtins) {
		_blueprints.push_back(b);
	}

	/* Locate user blueprint directory */
	std::string dir = FioFindDirectory(Subdirectory::Blueprint);
	if (dir.empty()) {
		dir = FioGetDirectory(Searchpath::PersonalDir, Subdirectory::Blueprint);
	}
	if (dir.empty()) return;

	std::error_code ec;
	std::filesystem::path dir_path = OTTD2FS(dir);
	if (!std::filesystem::exists(dir_path, ec)) {
		std::filesystem::create_directories(dir_path, ec);
		return;
	}

	for (const auto &entry : std::filesystem::directory_iterator(dir_path, ec)) {
		if (ec) break;
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".json") continue;

		std::ifstream file(entry.path());
		if (!file.is_open()) continue;

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		auto bp = Blueprint::FromJson(content);
		if (bp.has_value() && bp->IsValid()) {
			bp->is_builtin = false;
			_blueprints.push_back(*bp);
		}
	}
}

const std::vector<Blueprint> &BlueprintManager::GetBlueprints()
{
	if (!_initialized) Initialize();
	return _blueprints;
}

const Blueprint *BlueprintManager::GetBlueprint(size_t index)
{
	if (!_initialized) Initialize();
	if (index >= _blueprints.size()) return nullptr;
	return &_blueprints[index];
}

bool BlueprintManager::SaveBlueprint(const Blueprint &bp)
{
	if (!bp.IsValid()) return false;

	std::string dir = FioFindDirectory(Subdirectory::Blueprint);
	if (dir.empty()) {
		dir = FioGetDirectory(Searchpath::PersonalDir, Subdirectory::Blueprint);
	}
	if (dir.empty()) return false;

	std::error_code ec;
	std::filesystem::path dir_path = OTTD2FS(dir);
	if (!std::filesystem::exists(dir_path, ec)) {
		std::filesystem::create_directories(dir_path, ec);
	}

	std::string filename = CleanBlueprintFilename(bp.name) + ".json";
	std::filesystem::path file_path = dir_path / OTTD2FS(filename);

	std::ofstream out(file_path);
	if (!out.is_open()) return false;

	out << bp.ToJson();
	out.close();

	/* Update or append in memory list */
	auto it = std::find_if(_blueprints.begin(), _blueprints.end(), [&](const Blueprint &b) {
		return !b.is_builtin && b.name == bp.name;
	});
	if (it != _blueprints.end()) {
		*it = bp;
		it->is_builtin = false;
	} else {
		Blueprint copy = bp;
		copy.is_builtin = false;
		_blueprints.push_back(copy);
	}

	return true;
}

bool BlueprintManager::DeleteBlueprint(size_t index)
{
	if (!_initialized) Initialize();
	if (index >= _blueprints.size()) return false;
	if (_blueprints[index].is_builtin) return false;

	std::string dir = FioFindDirectory(Subdirectory::Blueprint);
	if (dir.empty()) {
		dir = FioGetDirectory(Searchpath::PersonalDir, Subdirectory::Blueprint);
	}

	if (!dir.empty()) {
		std::string filename = CleanBlueprintFilename(_blueprints[index].name) + ".json";
		std::filesystem::path file_path = std::filesystem::path(OTTD2FS(dir)) / OTTD2FS(filename);
		std::error_code ec;
		std::filesystem::remove(file_path, ec);
	}

	_blueprints.erase(_blueprints.begin() + index);
	return true;
}

bool BlueprintManager::RenameBlueprint(size_t index, const std::string &new_name)
{
	if (!_initialized) Initialize();
	if (index >= _blueprints.size()) return false;
	if (_blueprints[index].is_builtin) return false;
	if (new_name.empty()) return false;

	std::string old_name = _blueprints[index].name;

	/* Delete old file */
	std::string dir = FioFindDirectory(Subdirectory::Blueprint);
	if (dir.empty()) {
		dir = FioGetDirectory(Searchpath::PersonalDir, Subdirectory::Blueprint);
	}
	if (!dir.empty()) {
		std::string old_filename = CleanBlueprintFilename(old_name) + ".json";
		std::filesystem::path old_path = std::filesystem::path(OTTD2FS(dir)) / OTTD2FS(old_filename);
		std::error_code ec;
		std::filesystem::remove(old_path, ec);
	}

	_blueprints[index].name = new_name;
	return SaveBlueprint(_blueprints[index]);
}

std::optional<Blueprint> BlueprintManager::CaptureArea(TileIndex start_tile, TileIndex end_tile, const std::string &name)
{
	if (!IsValidTile(start_tile) || !IsValidTile(end_tile)) return std::nullopt;

	uint x1 = TileX(start_tile), y1 = TileY(start_tile);
	uint x2 = TileX(end_tile), y2 = TileY(end_tile);
	uint min_x = std::min(x1, x2);
	uint max_x = std::max(x1, x2);
	uint min_y = std::min(y1, y2);
	uint max_y = std::max(y1, y2);
	uint width = max_x - min_x + 1;
	uint height = max_y - min_y + 1;

	if (width == 0 || height == 0 || width > 64 || height > 64) return std::nullopt;

	Blueprint bp;
	bp.width = static_cast<uint16_t>(width);
	bp.height = static_cast<uint16_t>(height);
	bp.version = 1;
	bp.is_builtin = false;
	bp.created_time = 0;

	for (uint y = min_y; y <= max_y; ++y) {
		for (uint x = min_x; x <= max_x; ++x) {
			TileIndex tile = TileXY(x, y);
			if (!IsValidTile(tile)) continue;

			if (IsTileType(tile, TileType::Railway)) {
				/* Portal wormhole gate heads are connection targets, not copied objects */
				if (PortalRegistry::IsPortalTile(tile) || PortalRegistry::IsUnlinkedGate(tile)) {
					continue;
				}

				if (IsRailDepot(tile)) {
					BlueprintTile bt;
					bt.dx = static_cast<int16_t>(x - min_x);
					bt.dy = static_cast<int16_t>(y - min_y);
					bt.type = BlueprintTileType::Depot;
					bt.railtype = GetRailType(tile);
					bt.dir = GetRailDepotDirection(tile);
					bp.tiles.push_back(bt);
				} else if (IsPlainRail(tile)) {
					BlueprintTile bt;
					bt.dx = static_cast<int16_t>(x - min_x);
					bt.dy = static_cast<int16_t>(y - min_y);
					bt.type = BlueprintTileType::Track;
					bt.railtype = GetRailType(tile);
					bt.trackbits = GetTrackBits(tile);

					if (HasSignals(tile)) {
						for (Track track : bt.trackbits) {
							if (HasSignalOnTrack(tile, track)) {
								BlueprintSignal sig;
								sig.track = track;
								sig.sigtype = GetSignalType(tile, track);
								sig.sigvar = GetSignalVariant(tile, track);
								sig.signals_copy = static_cast<uint8_t>(GetPresentSignals(tile) & SignalOnTrack(track));
								bt.signals.push_back(sig);
							}
						}
					}
					bp.tiles.push_back(bt);
				}
			} else if (IsTileType(tile, TileType::Station) && IsRailStation(tile)) {
				BlueprintTile bt;
				bt.dx = static_cast<int16_t>(x - min_x);
				bt.dy = static_cast<int16_t>(y - min_y);
				bt.type = BlueprintTileType::Station;
				bt.railtype = GetRailType(tile);
				bt.axis = GetRailStationAxis(tile);
				bt.spec_class = STAT_CLASS_DFLT;
				bt.spec_index = 0;
				bp.tiles.push_back(bt);
			}
		}
	}

	if (bp.tiles.empty()) return std::nullopt;

	if (name.empty()) {
		bp.name = fmt::format("Blueprint {}", _blueprints.size() + 1);
	} else {
		bp.name = name;
	}

	return bp;
}

std::string BlueprintManager::ExportToString(size_t index)
{
	const Blueprint *bp = GetBlueprint(index);
	if (bp == nullptr) return "";
	return bp->ToJson();
}

bool BlueprintManager::ImportFromString(const std::string &json_data, std::string *error_msg)
{
	auto bp = Blueprint::FromJson(json_data);
	if (!bp.has_value()) {
		if (error_msg != nullptr) *error_msg = "Invalid JSON or unsupported blueprint format.";
		return false;
	}

	if (!bp->IsValid()) {
		if (error_msg != nullptr) *error_msg = "Blueprint dimensions or tile coordinates are invalid.";
		return false;
	}

	bp->is_builtin = false;
	return SaveBlueprint(*bp);
}
