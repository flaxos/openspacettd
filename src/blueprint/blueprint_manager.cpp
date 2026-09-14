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

static void CreateBuiltinCSTPrefabs()
{
	if (!_builtins.empty()) return;

	/* 1. CST Mainline Double Straight (8x2) */
	{
		Blueprint bp;
		bp.name = "CST Mainline Double Straight";
		bp.description = "Commonwealth Standard Transit dual-track high-speed trunk (8 tiles, X-axis). Directional one-way path signals at mid-span. Default RHD; use Flip ('F') for LHD.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 8;
		bp.height = 2;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 8; ++x) {
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
			bp.tiles.push_back(t1);

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
			bp.tiles.push_back(t2);
		}
		_builtins.push_back(bp);
	}

	/* 2. CST Dual-Track Passing Siding (14x4) */
	{
		Blueprint bp;
		bp.name = "CST Dual-Track Passing Siding";
		bp.description = "High-capacity mainline overtake siding (14x4). Parallel mainline tracks with dedicated offline loop for slower mineral/freight consists. Entry & exit path signals prevent mainline blocking.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 14;
		bp.height = 4;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 14; ++x) {
			BlueprintTile m1;
			m1.dx = x;
			m1.dy = 1;
			m1.type = BlueprintTileType::Track;
			m1.railtype = RAILTYPE_BEGIN;
			if (x == 1) {
				m1.trackbits = TrackBits{Track::X, Track::Upper};
			} else if (x == 12) {
				m1.trackbits = TrackBits{Track::X, Track::Right};
			} else {
				m1.trackbits = TrackBits{Track::X};
			}
			if (x == 0 || x == 7) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				m1.signals.push_back(s);
			}
			bp.tiles.push_back(m1);

			BlueprintTile m2;
			m2.dx = x;
			m2.dy = 2;
			m2.type = BlueprintTileType::Track;
			m2.railtype = RAILTYPE_BEGIN;
			m2.trackbits = TrackBits{Track::X};
			if (x == 7) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
				m2.signals.push_back(s);
			}
			bp.tiles.push_back(m2);
		}

		BlueprintTile s_in;
		s_in.dx = 2; s_in.dy = 0; s_in.type = BlueprintTileType::Track; s_in.railtype = RAILTYPE_BEGIN;
		s_in.trackbits = TrackBits{Track::Lower};
		bp.tiles.push_back(s_in);

		for (int16_t x = 3; x <= 10; ++x) {
			BlueprintTile st;
			st.dx = x; st.dy = 0; st.type = BlueprintTileType::Track; st.railtype = RAILTYPE_BEGIN;
			st.trackbits = TrackBits{Track::X};
			if (x == 10) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				st.signals.push_back(s);
			}
			bp.tiles.push_back(st);
		}

		BlueprintTile s_out;
		s_out.dx = 11; s_out.dy = 0; s_out.type = BlueprintTileType::Track; s_out.railtype = RAILTYPE_BEGIN;
		s_out.trackbits = TrackBits{Track::Left};
		bp.tiles.push_back(s_out);

		_builtins.push_back(bp);
	}

	/* 3. CST Portal Gate Approach Corridor (10x4) */
	{
		Blueprint bp;
		bp.name = "CST Portal Gate Approach Corridor";
		bp.description = "Commonwealth wormhole gatehead approach corridor (10x4). Features dual deceleration buffer blocks and emergency crossover loop to prevent traffic spillover when gate transits queue.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 10;
		bp.height = 4;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 10; ++x) {
			BlueprintTile in_t;
			in_t.dx = x; in_t.dy = 1; in_t.type = BlueprintTileType::Track; in_t.railtype = RAILTYPE_BEGIN;
			if (x == 4) {
				in_t.trackbits = TrackBits{Track::X, Track::Lower};
			} else {
				in_t.trackbits = TrackBits{Track::X};
			}
			if (x == 2 || x == 6) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				in_t.signals.push_back(s);
			}
			bp.tiles.push_back(in_t);

			BlueprintTile out_t;
			out_t.dx = x; out_t.dy = 2; out_t.type = BlueprintTileType::Track; out_t.railtype = RAILTYPE_BEGIN;
			if (x == 5) {
				out_t.trackbits = TrackBits{Track::X, Track::Upper};
			} else {
				out_t.trackbits = TrackBits{Track::X};
			}
			if (x == 3 || x == 7) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
				out_t.signals.push_back(s);
			}
			bp.tiles.push_back(out_t);
		}
		_builtins.push_back(bp);
	}

	/* 4. CST High-Speed 3-Way Wye Junction (12x12) */
	{
		Blueprint bp;
		bp.name = "CST High-Speed 3-Way Wye Junction";
		bp.description = "Grade-separated high-speed triangular junction (12x12). Connects three dual-track corridors (West, East, North) with complete directional path signaling and zero diamond crossing conflicts.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 12;
		bp.height = 12;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 12; ++x) {
			BlueprintTile t1;
			t1.dx = x; t1.dy = 5; t1.type = BlueprintTileType::Track; t1.railtype = RAILTYPE_BEGIN;
			if (x == 2) {
				t1.trackbits = TrackBits{Track::X, Track::Upper};
			} else if (x == 9) {
				t1.trackbits = TrackBits{Track::X, Track::Left};
			} else {
				t1.trackbits = TrackBits{Track::X};
			}
			if (x == 0 || x == 6 || x == 10) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				t1.signals.push_back(s);
			}
			bp.tiles.push_back(t1);

			BlueprintTile t2;
			t2.dx = x; t2.dy = 6; t2.type = BlueprintTileType::Track; t2.railtype = RAILTYPE_BEGIN;
			if (x == 3) {
				t2.trackbits = TrackBits{Track::X, Track::Lower};
			} else if (x == 8) {
				t2.trackbits = TrackBits{Track::X, Track::Right};
			} else {
				t2.trackbits = TrackBits{Track::X};
			}
			if (x == 1 || x == 5 || x == 11) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
				t2.signals.push_back(s);
			}
			bp.tiles.push_back(t2);
		}

		for (int16_t y = 0; y < 5; ++y) {
			BlueprintTile n1;
			n1.dx = 5; n1.dy = y; n1.type = BlueprintTileType::Track; n1.railtype = RAILTYPE_BEGIN;
			n1.trackbits = TrackBits{Track::Y};
			if (y == 2) {
				BlueprintSignal s;
				s.track = Track::Y;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::Y_SE);
				n1.signals.push_back(s);
			}
			bp.tiles.push_back(n1);

			BlueprintTile n2;
			n2.dx = 6; n2.dy = y; n2.type = BlueprintTileType::Track; n2.railtype = RAILTYPE_BEGIN;
			n2.trackbits = TrackBits{Track::Y};
			if (y == 2) {
				BlueprintSignal s;
				s.track = Track::Y;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::Y_NW);
				n2.signals.push_back(s);
			}
			bp.tiles.push_back(n2);
		}
		_builtins.push_back(bp);
	}

	/* 5. CST 4-Way Compact Roundabout Junction (10x10) */
	{
		Blueprint bp;
		bp.name = "CST 4-Way Compact Roundabout Junction";
		bp.description = "Symmetric 4-way circular distribution junction (10x10). Provides full turning and interchange capability across four cardinal directions. Recommended for trains up to length 5.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 10;
		bp.height = 10;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 10; ++x) {
			if (x < 3 || x > 6) {
				BlueprintTile w1;
				w1.dx = x; w1.dy = 4; w1.type = BlueprintTileType::Track; w1.railtype = RAILTYPE_BEGIN;
				w1.trackbits = TrackBits{Track::X};
				if (x == 1 || x == 8) {
					BlueprintSignal s;
					s.track = Track::X;
					s.sigtype = SignalType::PathOneWay;
					s.sigvar = SignalVariant::Electric;
					s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
					w1.signals.push_back(s);
				}
				bp.tiles.push_back(w1);

				BlueprintTile w2;
				w2.dx = x; w2.dy = 5; w2.type = BlueprintTileType::Track; w2.railtype = RAILTYPE_BEGIN;
				w2.trackbits = TrackBits{Track::X};
				if (x == 1 || x == 8) {
					BlueprintSignal s;
					s.track = Track::X;
					s.sigtype = SignalType::PathOneWay;
					s.sigvar = SignalVariant::Electric;
					s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
					w2.signals.push_back(s);
				}
				bp.tiles.push_back(w2);
			}
		}

		for (int16_t y = 0; y < 10; ++y) {
			if (y < 3 || y > 6) {
				BlueprintTile n1;
				n1.dx = 4; n1.dy = y; n1.type = BlueprintTileType::Track; n1.railtype = RAILTYPE_BEGIN;
				n1.trackbits = TrackBits{Track::Y};
				if (y == 1 || y == 8) {
					BlueprintSignal s;
					s.track = Track::Y;
					s.sigtype = SignalType::PathOneWay;
					s.sigvar = SignalVariant::Electric;
					s.signals_copy = SignalAlongTrackdir(Trackdir::Y_SE);
					n1.signals.push_back(s);
				}
				bp.tiles.push_back(n1);

				BlueprintTile n2;
				n2.dx = 5; n2.dy = y; n2.type = BlueprintTileType::Track; n2.railtype = RAILTYPE_BEGIN;
				n2.trackbits = TrackBits{Track::Y};
				if (y == 1 || y == 8) {
					BlueprintSignal s;
					s.track = Track::Y;
					s.sigtype = SignalType::PathOneWay;
					s.sigvar = SignalVariant::Electric;
					s.signals_copy = SignalAlongTrackdir(Trackdir::Y_NW);
					n2.signals.push_back(s);
				}
				bp.tiles.push_back(n2);
			}
		}

		for (int16_t y = 3; y <= 6; ++y) {
			for (int16_t x = 3; x <= 6; ++x) {
				BlueprintTile r;
				r.dx = x; r.dy = y; r.type = BlueprintTileType::Track; r.railtype = RAILTYPE_BEGIN;
				if (y == 3 || y == 6) {
					r.trackbits = TrackBits{Track::X};
				} else {
					r.trackbits = TrackBits{Track::Y};
				}
				bp.tiles.push_back(r);
			}
		}
		_builtins.push_back(bp);
	}

	/* 6. CST Ro-Ro 4-Platform Terminal Station Block (12x8) */
	{
		Blueprint bp;
		bp.name = "CST Ro-Ro 4-Platform Terminal Station Block";
		bp.description = "High-throughput Roll-On/Roll-Off passenger & freight terminal (12x8). Features 4 parallel platforms (length 6) with ladder throat distribution. Eliminates reversal delay and terminal deadlocks.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 12;
		bp.height = 8;
		bp.is_builtin = true;

		BlueprintTile in_lead;
		in_lead.dx = 0; in_lead.dy = 2; in_lead.type = BlueprintTileType::Track; in_lead.railtype = RAILTYPE_BEGIN;
		in_lead.trackbits = TrackBits{Track::X};
		{
			BlueprintSignal s;
			s.track = Track::X;
			s.sigtype = SignalType::PathOneWay;
			s.sigvar = SignalVariant::Electric;
			s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
			in_lead.signals.push_back(s);
		}
		bp.tiles.push_back(in_lead);

		for (int16_t y = 2; y <= 5; ++y) {
			for (int16_t x = 1; x <= 2; ++x) {
				BlueprintTile th;
				th.dx = x; th.dy = y; th.type = BlueprintTileType::Track; th.railtype = RAILTYPE_BEGIN;
				th.trackbits = TrackBits{Track::X};
				if (x == 2) {
					BlueprintSignal s;
					s.track = Track::X;
					s.sigtype = SignalType::PathOneWay;
					s.sigvar = SignalVariant::Electric;
					s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
					th.signals.push_back(s);
				}
				bp.tiles.push_back(th);
			}
		}

		for (int16_t y = 2; y <= 5; ++y) {
			for (int16_t x = 3; x <= 8; ++x) {
				BlueprintTile st;
				st.dx = x; st.dy = y; st.type = BlueprintTileType::Station; st.railtype = RAILTYPE_BEGIN;
				st.axis = Axis::X;
				st.spec_class = STAT_CLASS_DFLT;
				st.spec_index = 0;
				bp.tiles.push_back(st);
			}
		}

		for (int16_t y = 2; y <= 5; ++y) {
			for (int16_t x = 9; x <= 10; ++x) {
				BlueprintTile th;
				th.dx = x; th.dy = y; th.type = BlueprintTileType::Track; th.railtype = RAILTYPE_BEGIN;
				th.trackbits = TrackBits{Track::X};
				if (x == 9) {
					BlueprintSignal s;
					s.track = Track::X;
					s.sigtype = SignalType::PathOneWay;
					s.sigvar = SignalVariant::Electric;
					s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
					th.signals.push_back(s);
				}
				bp.tiles.push_back(th);
			}
		}

		BlueprintTile out_lead;
		out_lead.dx = 11; out_lead.dy = 2; out_lead.type = BlueprintTileType::Track; out_lead.railtype = RAILTYPE_BEGIN;
		out_lead.trackbits = TrackBits{Track::X};
		bp.tiles.push_back(out_lead);

		_builtins.push_back(bp);
	}

	/* 7. CST Industrial Bulk Balloon Loop (14x10) */
	{
		Blueprint bp;
		bp.name = "CST Industrial Bulk Balloon Loop";
		bp.description = "Continuous-flow unidirectional balloon turnaround loop (14x10) with integrated 2-platform bulk loading siding. Designed for continuous-motion ore, mineral, and grain loading.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 14;
		bp.height = 10;
		bp.is_builtin = true;

		for (int16_t x = 0; x <= 4; ++x) {
			BlueprintTile in_t;
			in_t.dx = x; in_t.dy = 3; in_t.type = BlueprintTileType::Track; in_t.railtype = RAILTYPE_BEGIN;
			in_t.trackbits = TrackBits{Track::X};
			if (x == 2) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				in_t.signals.push_back(s);
			}
			bp.tiles.push_back(in_t);
		}

		for (int16_t y = 3; y <= 4; ++y) {
			for (int16_t x = 5; x <= 9; ++x) {
				BlueprintTile st;
				st.dx = x; st.dy = y; st.type = BlueprintTileType::Station; st.railtype = RAILTYPE_BEGIN;
				st.axis = Axis::X;
				st.spec_class = STAT_CLASS_DFLT;
				st.spec_index = 0;
				bp.tiles.push_back(st);
			}
		}

		for (int16_t y = 1; y <= 8; ++y) {
			BlueprintTile lp;
			lp.dx = 13; lp.dy = y; lp.type = BlueprintTileType::Track; lp.railtype = RAILTYPE_BEGIN;
			lp.trackbits = TrackBits{Track::Y};
			if (y == 4) {
				BlueprintSignal s;
				s.track = Track::Y;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::Y_SE);
				lp.signals.push_back(s);
			}
			bp.tiles.push_back(lp);
		}

		for (int16_t x = 0; x <= 12; ++x) {
			BlueprintTile out_t;
			out_t.dx = x; out_t.dy = 6; out_t.type = BlueprintTileType::Track; out_t.railtype = RAILTYPE_BEGIN;
			out_t.trackbits = TrackBits{Track::X};
			if (x == 6) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
				out_t.signals.push_back(s);
			}
			bp.tiles.push_back(out_t);
		}
		_builtins.push_back(bp);
	}

	/* 8. CST Depot Maintenance Staging Yard (10x6) */
	{
		Blueprint bp;
		bp.name = "CST Depot Maintenance Staging Yard";
		bp.description = "Offline dual-depot service facility (10x6). Mainline double-track bypass with dedicated depot branch, two service bays, and an acceleration merge track preventing mainline disruptions.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 10;
		bp.height = 6;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 10; ++x) {
			BlueprintTile m1;
			m1.dx = x; m1.dy = 1; m1.type = BlueprintTileType::Track; m1.railtype = RAILTYPE_BEGIN;
			m1.trackbits = TrackBits{Track::X};
			if (x == 5) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				m1.signals.push_back(s);
			}
			bp.tiles.push_back(m1);

			BlueprintTile m2;
			m2.dx = x; m2.dy = 2; m2.type = BlueprintTileType::Track; m2.railtype = RAILTYPE_BEGIN;
			m2.trackbits = TrackBits{Track::X};
			if (x == 5) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
				m2.signals.push_back(s);
			}
			bp.tiles.push_back(m2);
		}

		for (int16_t x = 1; x <= 3; ++x) {
			BlueprintTile b1;
			b1.dx = x; b1.dy = 4; b1.type = BlueprintTileType::Track; b1.railtype = RAILTYPE_BEGIN;
			b1.trackbits = TrackBits{Track::X};
			bp.tiles.push_back(b1);
		}

		BlueprintTile d1;
		d1.dx = 4; d1.dy = 4; d1.type = BlueprintTileType::Depot; d1.railtype = RAILTYPE_BEGIN;
		d1.dir = DiagDirection::NE;
		bp.tiles.push_back(d1);

		BlueprintTile d2;
		d2.dx = 4; d2.dy = 5; d2.type = BlueprintTileType::Depot; d2.railtype = RAILTYPE_BEGIN;
		d2.dir = DiagDirection::NE;
		bp.tiles.push_back(d2);

		for (int16_t x = 5; x <= 8; ++x) {
			BlueprintTile esc;
			esc.dx = x; esc.dy = 4; esc.type = BlueprintTileType::Track; esc.railtype = RAILTYPE_BEGIN;
			esc.trackbits = TrackBits{Track::X};
			if (x == 8) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_SW);
				esc.signals.push_back(s);
			}
			bp.tiles.push_back(esc);
		}
		_builtins.push_back(bp);
	}
}

void BlueprintManager::Initialize()
{
	if (_initialized) return;
	_initialized = true;

	CreateBuiltinCSTPrefabs();
	RescanLibrary();
}

size_t BlueprintManager::GetBuiltinCount()
{
	if (!_initialized) Initialize();
	return _builtins.size();
}

const Blueprint *BlueprintManager::FindBuiltin(const std::string &name)
{
	if (!_initialized) Initialize();
	for (const auto &b : _builtins) {
		if (b.name == name) return &b;
	}
	return nullptr;
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
