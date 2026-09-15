/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file blueprint.cpp Implementation of blueprint data model, JSON serialization, and transformations. */

#include "../stdafx.h"
#include "blueprint.h"
#include "../signal_func.h"
#include "../track_func.h"
#include "../direction_func.h"
#include "../3rdparty/nlohmann/json.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <type_traits>

using json = nlohmann::json;

namespace {

/** Reject oversized, deeply nested, or ambiguous input before building a JSON DOM. */
struct BlueprintJsonPreflight : nlohmann::json_sax<json> {
	std::vector<std::optional<std::set<std::string>>> scopes;
	bool null() override { return true; }
	bool boolean(bool) override { return true; }
	bool number_integer(number_integer_t) override { return true; }
	bool number_unsigned(number_unsigned_t) override { return true; }
	bool number_float(number_float_t, const string_t &) override { return true; }
	bool string(string_t &) override { return true; }
	bool binary(binary_t &) override { return false; }
	bool start_object(std::size_t) override
	{
		if (this->scopes.size() >= Blueprint::MAX_JSON_DEPTH) return false;
		this->scopes.emplace_back(std::set<std::string>{});
		return true;
	}
	bool key(string_t &value) override
	{
		return !this->scopes.empty() && this->scopes.back().has_value() && this->scopes.back()->insert(value).second;
	}
	bool end_object() override { this->scopes.pop_back(); return true; }
	bool start_array(std::size_t) override
	{
		if (this->scopes.size() >= Blueprint::MAX_JSON_DEPTH) return false;
		this->scopes.emplace_back(std::nullopt);
		return true;
	}
	bool end_array() override { this->scopes.pop_back(); return true; }
	bool parse_error(std::size_t, const std::string &, const nlohmann::detail::exception &) override { return false; }
};

template <typename T> bool ReadInteger(const json &object, const char *key, T &result, uint64_t max_value, bool required = true)
{
	if (!object.is_object()) return false;
	auto it = object.find(key);
	if (it == object.end()) return !required;
	if (!it->is_number_integer()) return false;
	uint64_t value;
	if (it->is_number_unsigned()) {
		value = it->get<uint64_t>();
	} else {
		int64_t signed_value = it->get<int64_t>();
		if (signed_value < 0) return false;
		value = static_cast<uint64_t>(signed_value);
	}
	if (value > max_value) return false;
	result = static_cast<T>(value);
	return true;
}

bool ReadString(const json &object, const char *key, std::string &result, size_t max_bytes, bool required = false)
{
	if (!object.is_object()) return false;
	auto it = object.find(key);
	if (it == object.end()) return !required;
	if (!it->is_string()) return false;
	result = it->get<std::string>();
	return result.size() <= max_bytes;
}

bool ReadBool(const json &object, const char *key, bool &result)
{
	auto it = object.find(key);
	if (it == object.end()) return true;
	if (!it->is_boolean()) return false;
	result = it->get<bool>();
	return true;
}

bool ReadTime(const json &object, int64_t &result)
{
	auto it = object.find("created_time");
	if (it == object.end()) return true;
	if (!it->is_number_integer()) return false;
	if (it->is_number_unsigned()) {
		uint64_t value = it->get<uint64_t>();
		if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) return false;
		result = static_cast<int64_t>(value);
	} else {
		result = it->get<int64_t>();
	}
	return true;
}

bool ValidMetadata(const std::string &s, size_t max_bytes)
{
	if (s.size() > max_bytes) return false;
	for (size_t i = 0; i < s.size();) {
		unsigned char lead = static_cast<unsigned char>(s[i]);
		if (lead < 0x80) {
			if (lead < 0x20 || lead == 0x7F) return false;
			++i;
			continue;
		}
		size_t trailing = lead >= 0xC2 && lead <= 0xDF ? 1 : lead >= 0xE0 && lead <= 0xEF ? 2 : lead >= 0xF0 && lead <= 0xF4 ? 3 : 0;
		if (trailing == 0 || trailing >= s.size() - i) return false;
		unsigned char first = static_cast<unsigned char>(s[i + 1]);
		if ((first & 0xC0) != 0x80 || (lead == 0xE0 && first < 0xA0) || (lead == 0xED && first >= 0xA0) ||
			(lead == 0xF0 && first < 0x90) || (lead == 0xF4 && first >= 0x90)) return false;
		for (size_t j = 2; j <= trailing; ++j) {
			if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) return false;
		}
		i += trailing + 1;
	}
	return true;
}

} // namespace

Track RotateTrack90CW(Track track)
{
	switch (track) {
		case Track::X:     return Track::Y;
		case Track::Y:     return Track::X;
		case Track::Upper: return Track::Left;
		case Track::Left:  return Track::Lower;
		case Track::Lower: return Track::Right;
		case Track::Right: return Track::Upper;
		default:           return track;
	}
}

Trackdir RotateTrackdir90CW(Trackdir td)
{
	switch (td) {
		case Trackdir::X_NE:    return Trackdir::Y_NW;
		case Trackdir::Y_NW:    return Trackdir::X_SW;
		case Trackdir::X_SW:    return Trackdir::Y_SE;
		case Trackdir::Y_SE:    return Trackdir::X_NE;
		case Trackdir::Upper_E: return Trackdir::Left_N;
		case Trackdir::Left_N:  return Trackdir::Lower_W;
		case Trackdir::Lower_W: return Trackdir::Right_S;
		case Trackdir::Right_S: return Trackdir::Upper_E;
		case Trackdir::Upper_W: return Trackdir::Left_S;
		case Trackdir::Left_S:  return Trackdir::Lower_E;
		case Trackdir::Lower_E: return Trackdir::Right_N;
		case Trackdir::Right_N: return Trackdir::Upper_W;
		default:                return td;
	}
}

Track MirrorTrack(Track track)
{
	switch (track) {
		case Track::X:     return Track::Y;
		case Track::Y:     return Track::X;
		case Track::Upper: return Track::Upper;
		case Track::Lower: return Track::Lower;
		case Track::Left:  return Track::Right;
		case Track::Right: return Track::Left;
		default:           return track;
	}
}

Trackdir MirrorTrackdir(Trackdir td)
{
	switch (td) {
		case Trackdir::X_NE:    return Trackdir::Y_NW;
		case Trackdir::Y_NW:    return Trackdir::X_NE;
		case Trackdir::X_SW:    return Trackdir::Y_SE;
		case Trackdir::Y_SE:    return Trackdir::X_SW;
		case Trackdir::Upper_E: return Trackdir::Upper_W;
		case Trackdir::Upper_W: return Trackdir::Upper_E;
		case Trackdir::Lower_E: return Trackdir::Lower_W;
		case Trackdir::Lower_W: return Trackdir::Lower_E;
		case Trackdir::Left_S:  return Trackdir::Right_S;
		case Trackdir::Right_S: return Trackdir::Left_S;
		case Trackdir::Left_N:  return Trackdir::Right_N;
		case Trackdir::Right_N: return Trackdir::Left_N;
		default:                return td;
	}
}

bool Blueprint::IsValid() const
{
	if (this->version != 1 || this->layout_revision == 0 || this->width == 0 || this->height == 0 || this->width > MAX_DIMENSION || this->height > MAX_DIMENSION) return false;
	if (this->tiles.empty() || this->tiles.size() > MAX_TILES) return false;
	if (this->name.empty() || !ValidMetadata(this->name, MAX_NAME_BYTES) || !ValidMetadata(this->description, MAX_METADATA_BYTES) || !ValidMetadata(this->author, MAX_METADATA_BYTES)) return false;
	std::set<std::pair<int16_t, int16_t>> positions;
	for (const auto &tile : this->tiles) {
		if (tile.dx < 0 || tile.dx >= this->width) return false;
		if (tile.dy < 0 || tile.dy >= this->height) return false;
		if (!positions.emplace(tile.dx, tile.dy).second) return false;
		if (to_underlying(tile.railtype) >= RAILTYPE_END) return false;
		switch (tile.type) {
			case BlueprintTileType::Track: {
				if (tile.trackbits.base() == 0 || (tile.trackbits.base() & ~TRACK_BIT_ALL.base()) != 0 || tile.signals.size() > static_cast<size_t>(Track::End)) return false;
				std::set<Track> tracks_with_signals;
				for (const auto &signal : tile.signals) {
					if (!IsValidTrack(signal.track) || !tile.trackbits.Test(signal.track) || !tracks_with_signals.insert(signal.track).second) return false;
					if (signal.sigtype >= SignalType::End || signal.sigvar >= SignalVariant::End) return false;
					if (signal.signals_copy == 0 || (signal.signals_copy & ~SignalOnTrack(signal.track)) != 0) return false;
				}
				if (!tile.signals.empty() && TracksOverlap(tile.trackbits)) return false;
				break;
			}
			case BlueprintTileType::Depot:
				if (tile.dir >= DiagDirection::End || tile.trackbits.base() != 0 || !tile.signals.empty()) return false;
				break;
			case BlueprintTileType::Station:
				if (tile.axis >= Axis::End || tile.spec_class.base() == UINT16_MAX || tile.trackbits.base() != 0 || !tile.signals.empty()) return false;
				break;
			default: return false;
		}
	}
	return true;
}

size_t Blueprint::GetTrackPieceCount() const
{
	size_t count = 0;
	for (const auto &tile : this->tiles) {
		if (tile.type == BlueprintTileType::Track) {
			count += tile.trackbits.Count();
		}
	}
	return count;
}

size_t Blueprint::GetSignalCount() const
{
	size_t count = 0;
	for (const auto &tile : this->tiles) {
		if (tile.type == BlueprintTileType::Track) {
			count += tile.signals.size();
		}
	}
	return count;
}

size_t Blueprint::GetStationCount() const
{
	size_t count = 0;
	for (const auto &tile : this->tiles) {
		if (tile.type == BlueprintTileType::Station) count++;
	}
	return count;
}

size_t Blueprint::GetDepotCount() const
{
	size_t count = 0;
	for (const auto &tile : this->tiles) {
		if (tile.type == BlueprintTileType::Depot) count++;
	}
	return count;
}

std::string Blueprint::ToJson() const
{
	json j;
	j["format"] = "OpenSpaceTTD_Blueprint";
	j["version"] = this->version;
	j["layout_revision"] = this->layout_revision;
	j["name"] = this->name;
	j["description"] = this->description;
	j["author"] = this->author;
	j["width"] = this->width;
	j["height"] = this->height;
	j["is_builtin"] = this->is_builtin;
	j["created_time"] = this->created_time;

	json tiles_arr = json::array();
	for (const auto &tile : this->tiles) {
		json t;
		t["dx"] = tile.dx;
		t["dy"] = tile.dy;
		t["type"] = to_underlying(tile.type);
		t["railtype"] = to_underlying(tile.railtype);

		if (tile.type == BlueprintTileType::Track) {
			t["trackbits"] = tile.trackbits.base();
			if (!tile.signals.empty()) {
				json sigs_arr = json::array();
				for (const auto &sig : tile.signals) {
					json s;
					s["track"] = to_underlying(sig.track);
					s["sigtype"] = to_underlying(sig.sigtype);
					s["sigvar"] = to_underlying(sig.sigvar);
					s["signals_copy"] = sig.signals_copy;
					sigs_arr.push_back(s);
				}
				t["signals"] = sigs_arr;
			}
		} else if (tile.type == BlueprintTileType::Depot) {
			t["dir"] = to_underlying(tile.dir);
		} else if (tile.type == BlueprintTileType::Station) {
			t["axis"] = to_underlying(tile.axis);
			t["spec_class"] = tile.spec_class.base();
			t["spec_index"] = tile.spec_index;
		}
		tiles_arr.push_back(t);
	}
	j["tiles"] = tiles_arr;

	return j.dump(2);
}

std::optional<Blueprint> Blueprint::FromJson(const std::string &json_str)
{
	try {
		if (json_str.empty() || json_str.size() > MAX_JSON_BYTES) return std::nullopt;
		BlueprintJsonPreflight preflight;
		if (!json::sax_parse(json_str, &preflight)) return std::nullopt;
		json j = json::parse(json_str);
		if (!j.is_object() || !j.contains("format") || !j["format"].is_string() || j["format"] != "OpenSpaceTTD_Blueprint") return std::nullopt;

		Blueprint bp;
		if (!ReadInteger(j, "version", bp.version, 1) || !ReadString(j, "name", bp.name, MAX_NAME_BYTES, true) ||
			!ReadInteger(j, "layout_revision", bp.layout_revision, UINT32_MAX, false) ||
			!ReadString(j, "description", bp.description, MAX_METADATA_BYTES) || !ReadString(j, "author", bp.author, MAX_METADATA_BYTES) ||
			!ReadInteger(j, "width", bp.width, MAX_DIMENSION) || !ReadInteger(j, "height", bp.height, MAX_DIMENSION) ||
			!ReadBool(j, "is_builtin", bp.is_builtin) || !ReadTime(j, bp.created_time)) return std::nullopt;
		if (!j.contains("tiles") || !j["tiles"].is_array() || j["tiles"].empty() || j["tiles"].size() > MAX_TILES) return std::nullopt;

		for (const auto &t : j["tiles"]) {
			if (!t.is_object()) return std::nullopt;
			BlueprintTile tile;
			uint64_t value = 0;
			if (!ReadInteger(t, "dx", tile.dx, MAX_DIMENSION - 1) || !ReadInteger(t, "dy", tile.dy, MAX_DIMENSION - 1) ||
				!ReadInteger(t, "type", value, 2)) return std::nullopt;
			tile.type = static_cast<BlueprintTileType>(value);
			if (!ReadInteger(t, "railtype", value, RAILTYPE_END - 1)) return std::nullopt;
			tile.railtype = static_cast<RailType>(value);

			if (tile.type == BlueprintTileType::Track) {
				if (!ReadInteger(t, "trackbits", value, TRACK_BIT_ALL.base())) return std::nullopt;
				tile.trackbits = TrackBits{static_cast<uint8_t>(value)};
				if (t.contains("signals")) {
					if (!t["signals"].is_array() || t["signals"].size() > static_cast<size_t>(Track::End)) return std::nullopt;
					for (const auto &s : t["signals"]) {
						if (!s.is_object()) return std::nullopt;
						BlueprintSignal sig;
						if (!ReadInteger(s, "track", value, to_underlying(Track::End) - 1)) return std::nullopt;
						sig.track = static_cast<Track>(value);
						if (!ReadInteger(s, "sigtype", value, to_underlying(SignalType::End) - 1)) return std::nullopt;
						sig.sigtype = static_cast<SignalType>(value);
						if (!ReadInteger(s, "sigvar", value, to_underlying(SignalVariant::End) - 1)) return std::nullopt;
						sig.sigvar = static_cast<SignalVariant>(value);
						if (!ReadInteger(s, "signals_copy", sig.signals_copy, UINT8_MAX)) return std::nullopt;
						tile.signals.push_back(sig);
					}
				}
			} else if (tile.type == BlueprintTileType::Depot) {
				if (!ReadInteger(t, "dir", value, to_underlying(DiagDirection::End) - 1)) return std::nullopt;
				tile.dir = static_cast<DiagDirection>(value);
			} else if (tile.type == BlueprintTileType::Station) {
				if (!ReadInteger(t, "axis", value, to_underlying(Axis::End) - 1)) return std::nullopt;
				tile.axis = static_cast<Axis>(value);
				if (!ReadInteger(t, "spec_class", value, UINT16_MAX - 1)) return std::nullopt;
				tile.spec_class = StationClassID{static_cast<uint16_t>(value)};
				if (!ReadInteger(t, "spec_index", tile.spec_index, UINT16_MAX)) return std::nullopt;
			}
			/* Fields for the other tile shapes must not be silently ignored. */
			for (const char *field : {"trackbits", "signals", "dir", "axis", "spec_class", "spec_index"}) {
				bool relevant = (tile.type == BlueprintTileType::Track && (std::string(field) == "trackbits" || std::string(field) == "signals")) ||
					(tile.type == BlueprintTileType::Depot && std::string(field) == "dir") ||
					(tile.type == BlueprintTileType::Station && (std::string(field) == "axis" || std::string(field) == "spec_class" || std::string(field) == "spec_index"));
				if (!relevant && t.contains(field)) return std::nullopt;
			}

			bp.tiles.push_back(tile);
		}

		if (!bp.IsValid()) return std::nullopt;
		return bp;
	} catch (...) {
		return std::nullopt;
	}
}

Blueprint Blueprint::Rotate(int steps_90_cw) const
{
	if (!this->IsValid()) return Blueprint{};
	int steps = ((steps_90_cw % 4) + 4) % 4;
	if (steps == 0) return *this;

	Blueprint current = *this;

	for (int s = 0; s < steps; ++s) {
		Blueprint next;
		next.name = current.name;
		next.description = current.description;
		next.author = current.author;
		next.version = current.version;
		next.layout_revision = current.layout_revision;
		next.is_builtin = current.is_builtin;
		next.created_time = current.created_time;
		next.width = current.height;
		next.height = current.width;

		for (const auto &tile : current.tiles) {
			BlueprintTile rtile;
			rtile.dx = static_cast<int16_t>((current.height - 1) - tile.dy);
			rtile.dy = tile.dx;
			rtile.type = tile.type;
			rtile.railtype = tile.railtype;

			if (tile.type == BlueprintTileType::Track) {
				TrackBits r_bits{};
				for (Track t : tile.trackbits) {
					r_bits.Set(RotateTrack90CW(t));
				}
				rtile.trackbits = r_bits;

				for (const auto &sig : tile.signals) {
					BlueprintSignal rsig;
					rsig.track = RotateTrack90CW(sig.track);
					rsig.sigtype = sig.sigtype;
					rsig.sigvar = sig.sigvar;

					Trackdir td1 = TrackToTrackdir(sig.track);
					Trackdir td2 = ReverseTrackdir(td1);

					uint8_t r_copy = 0;
					if ((sig.signals_copy & SignalAlongTrackdir(td1)) != 0) {
						Trackdir r_td = RotateTrackdir90CW(td1);
						r_copy |= SignalAlongTrackdir(r_td);
					}
					if ((sig.signals_copy & SignalAlongTrackdir(td2)) != 0) {
						Trackdir r_td = RotateTrackdir90CW(td2);
						r_copy |= SignalAlongTrackdir(r_td);
					}
					rsig.signals_copy = r_copy;
					rtile.signals.push_back(rsig);
				}
			} else if (tile.type == BlueprintTileType::Depot) {
				rtile.dir = ChangeDiagDir(tile.dir, DiagDirDiff::Left90);
			} else if (tile.type == BlueprintTileType::Station) {
				rtile.axis = OtherAxis(tile.axis);
				rtile.spec_class = tile.spec_class;
				rtile.spec_index = tile.spec_index;
			}

			next.tiles.push_back(rtile);
		}
		current = next;
	}

	return current.IsValid() ? current : Blueprint{};
}

Blueprint Blueprint::Mirror() const
{
	if (!this->IsValid()) return Blueprint{};
	Blueprint mirrored;
	mirrored.name = this->name;
	mirrored.description = this->description;
	mirrored.author = this->author;
	mirrored.version = this->version;
	mirrored.layout_revision = this->layout_revision;
	mirrored.is_builtin = this->is_builtin;
	mirrored.created_time = this->created_time;
	mirrored.width = this->height;
	mirrored.height = this->width;

	for (const auto &tile : this->tiles) {
		BlueprintTile mtile;
		mtile.dx = tile.dy;
		mtile.dy = tile.dx;
		mtile.type = tile.type;
		mtile.railtype = tile.railtype;

		if (tile.type == BlueprintTileType::Track) {
			TrackBits m_bits{};
			for (Track t : tile.trackbits) {
				m_bits.Set(MirrorTrack(t));
			}
			mtile.trackbits = m_bits;

			for (const auto &sig : tile.signals) {
				BlueprintSignal msig;
				msig.track = MirrorTrack(sig.track);
				msig.sigtype = sig.sigtype;
				msig.sigvar = sig.sigvar;

				Trackdir td1 = TrackToTrackdir(sig.track);
				Trackdir td2 = ReverseTrackdir(td1);

				uint8_t m_copy = 0;
				if ((sig.signals_copy & SignalAlongTrackdir(td1)) != 0) {
					Trackdir m_td = MirrorTrackdir(td1);
					m_copy |= SignalAlongTrackdir(m_td);
				}
				if ((sig.signals_copy & SignalAlongTrackdir(td2)) != 0) {
					Trackdir m_td = MirrorTrackdir(td2);
					m_copy |= SignalAlongTrackdir(m_td);
				}
				msig.signals_copy = m_copy;
				mtile.signals.push_back(msig);
			}
		} else if (tile.type == BlueprintTileType::Depot) {
			switch (tile.dir) {
				case DiagDirection::NE: mtile.dir = DiagDirection::NW; break;
				case DiagDirection::NW: mtile.dir = DiagDirection::NE; break;
				case DiagDirection::SE: mtile.dir = DiagDirection::SW; break;
				case DiagDirection::SW: mtile.dir = DiagDirection::SE; break;
				default: mtile.dir = tile.dir; break;
			}
		} else if (tile.type == BlueprintTileType::Station) {
			mtile.axis = OtherAxis(tile.axis);
			mtile.spec_class = tile.spec_class;
			mtile.spec_index = tile.spec_index;
		}

		mirrored.tiles.push_back(mtile);
	}

	return mirrored.IsValid() ? mirrored : Blueprint{};
}
