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

using json = nlohmann::json;

Track RotateTrack90CW(Track track)
{
	switch (track) {
		case Track::X:     return Track::Y;
		case Track::Y:     return Track::X;
		case Track::Upper: return Track::Right;
		case Track::Right: return Track::Lower;
		case Track::Lower: return Track::Left;
		case Track::Left:  return Track::Upper;
		default:           return track;
	}
}

Trackdir RotateTrackdir90CW(Trackdir td)
{
	switch (td) {
		case Trackdir::X_NE:    return Trackdir::Y_SE;
		case Trackdir::Y_SE:    return Trackdir::X_SW;
		case Trackdir::X_SW:    return Trackdir::Y_NW;
		case Trackdir::Y_NW:    return Trackdir::X_NE;
		case Trackdir::Upper_E: return Trackdir::Right_S;
		case Trackdir::Right_S: return Trackdir::Lower_W;
		case Trackdir::Lower_W: return Trackdir::Left_N;
		case Trackdir::Left_N:  return Trackdir::Upper_E;
		case Trackdir::Upper_W: return Trackdir::Right_N;
		case Trackdir::Right_N: return Trackdir::Lower_E;
		case Trackdir::Lower_E: return Trackdir::Left_S;
		case Trackdir::Left_S:  return Trackdir::Upper_W;
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
	if (this->width == 0 || this->height == 0) return false;
	if (this->tiles.empty()) return false;

	for (const auto &tile : this->tiles) {
		if (tile.dx < 0 || tile.dx >= this->width) return false;
		if (tile.dy < 0 || tile.dy >= this->height) return false;
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
		json j = json::parse(json_str);
		if (!j.contains("format") || j["format"] != "OpenSpaceTTD_Blueprint") {
			return std::nullopt;
		}

		Blueprint bp;
		bp.version = j.value("version", 1u);
		bp.name = j.value("name", "Imported Blueprint");
		bp.description = j.value("description", "");
		bp.author = j.value("author", "");
		bp.width = j.value("width", static_cast<uint16_t>(0));
		bp.height = j.value("height", static_cast<uint16_t>(0));
		bp.is_builtin = j.value("is_builtin", false);
		bp.created_time = j.value("created_time", static_cast<int64_t>(0));

		if (!j.contains("tiles") || !j["tiles"].is_array()) {
			return std::nullopt;
		}

		for (const auto &t : j["tiles"]) {
			BlueprintTile tile;
			tile.dx = t.value("dx", static_cast<int16_t>(0));
			tile.dy = t.value("dy", static_cast<int16_t>(0));
			tile.type = static_cast<BlueprintTileType>(t.value("type", static_cast<uint8_t>(0)));
			tile.railtype = static_cast<RailType>(t.value("railtype", static_cast<uint8_t>(RAILTYPE_BEGIN)));

			if (tile.type == BlueprintTileType::Track) {
				tile.trackbits = TrackBits{static_cast<uint8_t>(t.value("trackbits", static_cast<uint8_t>(0)))};
				if (t.contains("signals") && t["signals"].is_array()) {
					for (const auto &s : t["signals"]) {
						BlueprintSignal sig;
						sig.track = static_cast<Track>(s.value("track", static_cast<uint8_t>(0)));
						sig.sigtype = static_cast<SignalType>(s.value("sigtype", static_cast<uint8_t>(0)));
						sig.sigvar = static_cast<SignalVariant>(s.value("sigvar", static_cast<uint8_t>(0)));
						sig.signals_copy = s.value("signals_copy", static_cast<uint8_t>(0));
						tile.signals.push_back(sig);
					}
				}
			} else if (tile.type == BlueprintTileType::Depot) {
				tile.dir = static_cast<DiagDirection>(t.value("dir", static_cast<uint8_t>(0)));
			} else if (tile.type == BlueprintTileType::Station) {
				tile.axis = static_cast<Axis>(t.value("axis", static_cast<uint8_t>(0)));
				tile.spec_class = StationClassID{t.value("spec_class", STAT_CLASS_DFLT.base())};
				tile.spec_index = t.value("spec_index", static_cast<uint16_t>(0));
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
	int steps = ((steps_90_cw % 4) + 4) % 4;
	if (steps == 0) return *this;

	Blueprint current = *this;

	for (int s = 0; s < steps; ++s) {
		Blueprint next;
		next.name = current.name;
		next.description = current.description;
		next.author = current.author;
		next.version = current.version;
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
				rtile.dir = ChangeDiagDir(tile.dir, DiagDirDiff::Right90);
			} else if (tile.type == BlueprintTileType::Station) {
				rtile.axis = OtherAxis(tile.axis);
				rtile.spec_class = tile.spec_class;
				rtile.spec_index = tile.spec_index;
			}

			next.tiles.push_back(rtile);
		}
		current = next;
	}

	return current;
}

Blueprint Blueprint::Mirror() const
{
	Blueprint mirrored;
	mirrored.name = this->name;
	mirrored.description = this->description;
	mirrored.author = this->author;
	mirrored.version = this->version;
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

	return mirrored;
}
