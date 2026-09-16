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
#include "../company_func.h"
#include "../company_base.h"
#include "../portal/portal_registry.h"
#include "../core/format.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <limits>
#include <climits>

static std::vector<Blueprint> _blueprints;
static std::vector<Blueprint> _builtins;
static std::vector<std::filesystem::path> _source_paths;
static unsigned long _temporary_counter = 0;

#if defined(_WIN32)
using ssize_t = std::ptrdiff_t;
/** Status of an open Blueprint backing file. */
using StorageStat = struct _stat64;
static int StorageOpen(const std::filesystem::path &path, int flags, int mode = 0) { return _wopen(path.c_str(), flags | _O_BINARY, mode); }
static int StorageClose(int fd) { return _close(fd); }
/** Query the status of an already-open Blueprint file. */
static int StorageFstat(int fd, StorageStat *st) { return _fstat64(fd, st); }
/** Check that the opened Blueprint file is still regular. */
static bool StorageIsRegular(const StorageStat &st) { return (st.st_mode & _S_IFMT) == _S_IFREG; }
static ssize_t StorageRead(int fd, void *buf, size_t size) { return _read(fd, buf, static_cast<unsigned>(std::min(size, static_cast<size_t>(INT_MAX)))); }
static ssize_t StorageWrite(int fd, const void *buf, size_t size) { return _write(fd, buf, static_cast<unsigned>(std::min(size, static_cast<size_t>(INT_MAX)))); }
static int StorageFlush(int fd) { return _commit(fd); }
static bool StoragePublish(const std::filesystem::path &tmp, const std::filesystem::path &target, bool replace) { return replace ? MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0 : MoveFileExW(tmp.c_str(), target.c_str(), 0) != 0; }
static std::string StoragePublishFailure() { return std::error_code(static_cast<int>(GetLastError()), std::system_category()).message(); }
static unsigned long StorageProcessId() { return GetCurrentProcessId(); }
#else
/** Status of an open Blueprint backing file. */
using StorageStat = struct stat;
static int StorageOpen(const std::filesystem::path &path, int flags, int mode = 0) { return open(path.c_str(), flags | O_NOFOLLOW, mode); }
static int StorageClose(int fd) { return close(fd); }
/** Query the status of an already-open Blueprint file. */
static int StorageFstat(int fd, StorageStat *st) { return fstat(fd, st); }
/** Check that the opened Blueprint file is still regular. */
static bool StorageIsRegular(const StorageStat &st) { return S_ISREG(st.st_mode); }
static ssize_t StorageRead(int fd, void *buf, size_t size) { return read(fd, buf, size); }
static ssize_t StorageWrite(int fd, const void *buf, size_t size) { return write(fd, buf, size); }
static int StorageFlush(int fd) { return fsync(fd); }
static bool StoragePublish(const std::filesystem::path &tmp, const std::filesystem::path &target, bool replace) { if (replace) return rename(tmp.c_str(), target.c_str()) == 0; if (link(tmp.c_str(), target.c_str()) != 0) return false; unlink(tmp.c_str()); return true; }
static std::string StoragePublishFailure() { return std::strerror(errno); }
static unsigned long StorageProcessId() { return getpid(); }
#endif
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
	if (clean.size() > 100) clean.resize(100);
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
			} else if (x == 11) {
				m1.trackbits = TrackBits{Track::X, Track::Left};
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
		s_in.dx = 1; s_in.dy = 0; s_in.type = BlueprintTileType::Track; s_in.railtype = RAILTYPE_BEGIN;
		s_in.trackbits = TrackBits{Track::Lower};
		bp.tiles.push_back(s_in);

		for (int16_t x = 2; x <= 10; ++x) {
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
		s_out.trackbits = TrackBits{Track::Right};
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
				in_t.trackbits = TrackBits{Track::X, Track::Right};
			} else if (x == 5) {
				in_t.trackbits = TrackBits{Track::X, Track::Y};
			} else if (x == 7) {
				in_t.trackbits = TrackBits{Track::X, Track::Left};
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
			if (x == 2) {
				out_t.trackbits = TrackBits{Track::X, Track::Right};
			} else if (x == 4) {
				out_t.trackbits = TrackBits{Track::X, Track::Y};
			} else if (x == 5) {
				out_t.trackbits = TrackBits{Track::X, Track::Left};
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
		/* The outer rows give both turnbacks a straight segment between opposing
		 * curves, so the loops work with the native ban on 90-degree turns. */
		struct LoopPiece { int16_t x; int16_t y; Track track; };
		for (const LoopPiece piece : {LoopPiece{5, 0, Track::Lower}, {6, 0, Track::X}, {7, 0, Track::Right},
				{2, 3, Track::Left}, {3, 3, Track::X}, {4, 3, Track::Upper}}) {
			BlueprintTile loop;
			loop.dx = piece.x;
			loop.dy = piece.y;
			loop.trackbits = TrackBits{piece.track};
			bp.tiles.push_back(loop);
		}
		_builtins.push_back(bp);
	}

	/* 4. CST High-Speed 3-Way Wye Junction (12x12) */
	{
		Blueprint bp;
		bp.name = "CST High-Speed 3-Way Wye Junction";
		bp.description = "At-grade directional 3-way Wye (12x12). Connects west, east and north dual-track corridors with path signals; keep approach speed and spacing appropriate for an at-grade junction.";
		bp.author = "Commonwealth Synergy Transport (CST)";
		bp.version = 1;
		bp.width = 12;
		bp.height = 12;
		bp.is_builtin = true;

		for (int16_t x = 0; x < 12; ++x) {
			BlueprintTile t1;
			t1.dx = x; t1.dy = 5; t1.type = BlueprintTileType::Track; t1.railtype = RAILTYPE_BEGIN;
			if (x == 5) {
				/* North arrivals continue south to the westbound lane or turn east. */
				t1.trackbits = TrackBits{Track::X, Track::Y, Track::Left};
			} else if (x == 6) {
				/* West arrivals turn north; east arrivals cross on the northbound line. */
				t1.trackbits = TrackBits{Track::X, Track::Y, Track::Upper};
			} else {
				t1.trackbits = TrackBits{Track::X};
			}
			if (x == 0 || x == 7 || x == 10) {
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
			if (x == 5) {
				t2.trackbits = TrackBits{Track::X, Track::Upper};
			} else if (x == 6) {
				t2.trackbits = TrackBits{Track::X, Track::Left};
			} else {
				t2.trackbits = TrackBits{Track::X};
			}
			if (x == 1 || x == 7 || x == 11) {
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
		bp.description = "Compact at-grade four-way distribution junction (10x10). Two parallel tracks on each approach provide straight routes and directional turns between all four cardinal corridors. Recommended for trains up to length 5.";
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
					s.signals_copy = SignalAlongTrackdir(Trackdir::Y_NW);
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
					s.signals_copy = SignalAlongTrackdir(Trackdir::Y_SE);
					n2.signals.push_back(s);
				}
				bp.tiles.push_back(n2);
			}
		}

		for (int16_t y = 3; y <= 6; ++y) {
			for (int16_t x = 3; x <= 6; ++x) {
				BlueprintTile r;
				r.dx = x; r.dy = y; r.type = BlueprintTileType::Track; r.railtype = RAILTYPE_BEGIN;
				/* Four central intersections carry turns in both entry directions.
				 * Continuous X/Y approaches keep each straight movement. */
				if ((x == 4 || x == 5) && (y == 4 || y == 5)) {
					r.trackbits = (x == 4 && y == 4) || (x == 5 && y == 5) ?
						TrackBits{Track::X, Track::Y, Track::Upper, Track::Lower} :
						TrackBits{Track::X, Track::Y, Track::Left, Track::Right};
				} else if (x == 4 || x == 5) {
					r.trackbits = TrackBits{Track::X, Track::Y};
				} else {
					r.trackbits = TrackBits{Track::X};
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
				/* The left ladder branches from the single incoming lead to all platforms. */
				th.trackbits = x == 1 && y == 2 ? TrackBits{Track::X, Track::Right} :
					x == 1 && y < 5 ? TrackBits{Track::Y, Track::Left} :
					x == 1 ? TrackBits{Track::Left} : TrackBits{Track::X};
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
				/* The right ladder gathers all platforms into the single outgoing lead. */
				th.trackbits = x == 10 && y == 2 ? TrackBits{Track::X, Track::Lower} :
					x == 10 && y < 5 ? TrackBits{Track::Y, Track::Upper} :
					x == 10 ? TrackBits{Track::Upper} : TrackBits{Track::X};
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
			in_t.trackbits = x == 4 ? TrackBits{Track::X, Track::Right} : TrackBits{Track::X};
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
		BlueprintTile second_platform_entry;
		second_platform_entry.dx = 4; second_platform_entry.dy = 4;
		second_platform_entry.type = BlueprintTileType::Track;
		second_platform_entry.railtype = RAILTYPE_BEGIN;
		second_platform_entry.trackbits = TrackBits{Track::Left};
		bp.tiles.push_back(second_platform_entry);

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

		for (int16_t y = 3; y <= 6; ++y) {
			BlueprintTile lp;
			lp.dx = 13; lp.dy = y; lp.type = BlueprintTileType::Track; lp.railtype = RAILTYPE_BEGIN;
			lp.trackbits = y == 3 ? TrackBits{Track::Right} :
				y == 4 ? TrackBits{Track::Y, Track::Right} :
				y == 6 ? TrackBits{Track::Upper} : TrackBits{Track::Y};
			if (y == 5) {
				BlueprintSignal s;
				s.track = Track::Y;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::Y_SE);
				lp.signals.push_back(s);
			}
			bp.tiles.push_back(lp);
		}
		for (int16_t y = 3; y <= 4; ++y) {
			for (int16_t x = 10; x <= 12; ++x) {
				BlueprintTile lead;
				lead.dx = x; lead.dy = y;
				lead.type = BlueprintTileType::Track;
				lead.railtype = RAILTYPE_BEGIN;
				lead.trackbits = TrackBits{Track::X};
				bp.tiles.push_back(lead);
			}
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
		bp.description = "Offline dual-depot service facility (10x6). The double-track mainline bypass has a bidirectional service lead, two reachable depot bays, and a return merge onto the lower mainline.";
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
			m2.trackbits = x == 1 ? TrackBits{Track::X, Track::Lower} :
				x == 2 ? TrackBits{Track::X, Track::Right} : TrackBits{Track::X};
			if (x == 0 || x == 5) {
				BlueprintSignal s;
				s.track = Track::X;
				s.sigtype = SignalType::PathOneWay;
				s.sigvar = SignalVariant::Electric;
				s.signals_copy = SignalAlongTrackdir(Trackdir::X_NE);
				m2.signals.push_back(s);
			}
			bp.tiles.push_back(m2);
		}

		for (int16_t y = 3; y <= 5; ++y) {
			for (int16_t x = 1; x <= 3; ++x) {
				if (y == 3 && x == 3) continue;
				BlueprintTile lead;
				lead.dx = x; lead.dy = y;
				lead.type = BlueprintTileType::Track;
				lead.railtype = RAILTYPE_BEGIN;
				lead.trackbits = y == 3 ? TrackBits{Track::Y} :
					y == 4 && x < 3 ? TrackBits{Track::X, Track::Y, Track::Left} :
					y == 5 && x == 1 ? TrackBits{Track::Left} :
					y == 5 && x == 2 ? TrackBits{Track::X, Track::Y, Track::Left} : TrackBits{Track::X};
				bp.tiles.push_back(lead);
			}
		}

		BlueprintTile d1;
		d1.dx = 4; d1.dy = 4; d1.type = BlueprintTileType::Depot; d1.railtype = RAILTYPE_BEGIN;
		d1.dir = DiagDirection::NE;
		bp.tiles.push_back(d1);

		BlueprintTile d2;
		d2.dx = 4; d2.dy = 5; d2.type = BlueprintTileType::Depot; d2.railtype = RAILTYPE_BEGIN;
		d2.dir = DiagDirection::NE;
		bp.tiles.push_back(d2);

		_builtins.push_back(bp);
	}
	/* Revision 2 identifies the repaired CST catalogue. Imported exports stay
	 * independent player layouts, including revision 1 and unversioned files. */
	for (Blueprint &bp : _builtins) bp.layout_revision = 2;
}

void BlueprintManager::Initialize()
{
	if (!_initialized) RescanLibrary();
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
	_source_paths.clear();
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
		_source_paths.emplace_back();
	}
}

static bool StorageError(std::string *error, const std::string &message)
{
	if (error != nullptr) *error = message;
	return false;
}

static std::filesystem::path LibraryDirectory()
{
	try {
		std::string dir = FioFindDirectory(Subdirectory::Blueprint);
		if (dir.empty()) dir = FioGetDirectory(Searchpath::PersonalDir, Subdirectory::Blueprint);
		return OTTD2FS(dir);
	} catch (const std::exception &) {
		return {};
	}
}

static bool EnsureDirectory(const std::filesystem::path &dir, std::string *error)
{
	std::error_code ec;
	if (dir.empty()) return StorageError(error, "Personal blueprint directory is unavailable.");
	std::filesystem::create_directories(dir, ec);
	if (ec) return StorageError(error, "Cannot create blueprint directory: " + ec.message());
	if (!std::filesystem::is_directory(dir, ec) || ec) return StorageError(error, "Blueprint path is not a directory.");
	return true;
}

static bool SafeRegularFile(const std::filesystem::path &path, std::string *error)
{
	std::error_code ec;
	auto status = std::filesystem::symlink_status(path, ec);
	if (ec || !std::filesystem::is_regular_file(status)) return StorageError(error, "Blueprint backing file is missing, unreadable or not a regular file: " + FS2OTTD(path.native()));
	return true;
}

static bool ReadBounded(const std::filesystem::path &path, std::string &content, std::string *error)
{
	if (path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos) return StorageError(error, "Blueprint path contains a NUL byte.");
	if (!SafeRegularFile(path, error)) return false;
	int fd = StorageOpen(path, O_RDONLY);
	if (fd < 0) return StorageError(error, "Cannot open blueprint file: " + std::string(std::strerror(errno)));
	StorageStat st{};
	if (StorageFstat(fd, &st) != 0 || !StorageIsRegular(st) || st.st_size < 0 || static_cast<uint64_t>(st.st_size) > Blueprint::MAX_JSON_BYTES) {
		StorageClose(fd);
		return StorageError(error, "Blueprint file is invalid or exceeds 2 MiB.");
	}
	std::string result(static_cast<size_t>(st.st_size), '\0');
	size_t pos = 0;
	while (pos < result.size()) {
		ssize_t n = StorageRead(fd, result.data() + pos, result.size() - pos);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) { int saved = errno; StorageClose(fd); return StorageError(error, n == 0 ? "Blueprint file ended during reading." : "Cannot read blueprint file: " + std::string(std::strerror(saved))); }
		pos += static_cast<size_t>(n);
	}
	char trailing_byte;
	ssize_t trailing = StorageRead(fd, &trailing_byte, 1);
	if (trailing != 0) { int saved = errno; StorageClose(fd); return StorageError(error, trailing > 0 ? "Blueprint file grew while reading." : "Cannot finish reading blueprint file: " + std::string(std::strerror(saved))); }
	if (StorageClose(fd) != 0) return StorageError(error, "Cannot close blueprint file: " + std::string(std::strerror(errno)));
	content = std::move(result);
	return true;
}

/* A temporary file is exclusive and resides beside its destination. Failed writes
 * leave the previous file and in-memory row untouched. */
static bool AtomicWrite(const std::filesystem::path &target, const std::string &bytes, bool replace, std::string *error)
{
	if (target.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos) return StorageError(error, "Blueprint path contains a NUL byte.");
	std::error_code ec;
	if (replace) {
		if (!SafeRegularFile(target, error)) return false;
	} else {
		auto status = std::filesystem::symlink_status(target, ec);
		if (ec && ec != std::errc::no_such_file_or_directory) return StorageError(error, "Cannot inspect destination: " + ec.message());
		if (!ec && std::filesystem::exists(status)) return StorageError(error, "Blueprint destination already exists: " + FS2OTTD(target.native()));
	}
	std::filesystem::path tmp;
	int fd = -1;
	for (int attempt = 0; attempt < 32; ++attempt) {
		tmp = std::filesystem::path(target.native() + OTTD2FS(".tmp-" + std::to_string(StorageProcessId()) + "-" + std::to_string(++_temporary_counter)));
		fd = StorageOpen(tmp, O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) break;
		if (errno != EEXIST) return StorageError(error, "Cannot create temporary blueprint file: " + std::string(std::strerror(errno)));
	}
	if (fd < 0) return StorageError(error, "Cannot reserve a temporary blueprint file.");
	auto fail = [&](const std::string &message) {
		StorageClose(fd);
		std::filesystem::remove(tmp, ec);
		return StorageError(error, message);
	};
	size_t pos = 0;
	while (pos < bytes.size()) {
		ssize_t n = StorageWrite(fd, bytes.data() + pos, bytes.size() - pos);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) return fail("Cannot write blueprint file: " + std::string(std::strerror(errno)));
		pos += static_cast<size_t>(n);
	}
	if (StorageFlush(fd) != 0) return fail("Cannot flush blueprint file: " + std::string(std::strerror(errno)));
	if (StorageClose(fd) != 0) {
		int saved = errno;
		fd = -1;
		std::filesystem::remove(tmp, ec);
		return StorageError(error, "Cannot close blueprint file: " + std::string(std::strerror(saved)));
	}
	fd = -1;
	if (replace && !SafeRegularFile(target, error)) { std::filesystem::remove(tmp, ec); return false; }
	if (!StoragePublish(tmp, target, replace)) {
		std::string reason = StoragePublishFailure();
		std::filesystem::remove(tmp, ec);
		return StorageError(error, "Cannot commit blueprint file: " + reason);
	}
	return true;
}

bool BlueprintManager::RescanLibrary(std::string *error_msg)
{
	if (!_initialized) {
		_initialized = true;
		CreateBuiltinCSTPrefabs();
		_blueprints = _builtins;
		_source_paths.assign(_blueprints.size(), {});
	}
	if (error_msg != nullptr) error_msg->clear();
	std::vector<Blueprint> next = _builtins;
	std::vector<std::filesystem::path> paths(next.size());
	const auto dir = LibraryDirectory();
	if (!EnsureDirectory(dir, error_msg)) return false;
	std::error_code ec;
	std::filesystem::directory_iterator it(dir, ec), end;
	if (ec) return StorageError(error_msg, "Cannot scan blueprint directory: " + ec.message());
	std::vector<std::filesystem::path> files;
	for (; it != end; it.increment(ec)) {
		if (ec) return StorageError(error_msg, "Cannot scan blueprint directory: " + ec.message());
		if (it->path().extension() == ".json") files.push_back(it->path());
	}
	if (ec) return StorageError(error_msg, "Cannot scan blueprint directory: " + ec.message());
	std::sort(files.begin(), files.end());
	std::string skipped;
	for (const auto &path : files) {
		std::string content, reason;
		if (!ReadBounded(path, content, &reason)) { skipped += path.filename().string() + ": " + reason + "\n"; continue; }
		auto bp = Blueprint::FromJson(content);
		if (!bp.has_value() || !bp->IsValid()) { skipped += path.filename().string() + ": invalid blueprint JSON\n"; continue; }
		bp->is_builtin = false;
		next.push_back(std::move(*bp));
		paths.push_back(path);
	}
	_blueprints = std::move(next);
	_source_paths = std::move(paths);
	bool clean = skipped.empty();
	if (error_msg != nullptr) *error_msg = std::move(skipped);
	return clean;
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

static size_t FindPlayerName(const std::string &name)
{
	for (size_t i = 0; i < _blueprints.size(); ++i) if (!_blueprints[i].is_builtin && _blueprints[i].name == name) return i;
	return _blueprints.size();
}

static bool UniquePlayerName(const std::string &name, std::string *error)
{
	size_t matches = 0;
	for (const auto &bp : _blueprints) if (!bp.is_builtin && bp.name == name) ++matches;
	if (matches > 1) return StorageError(error, "Multiple player blueprints have this name; choose a row and rename it first.");
	return true;
}

bool BlueprintManager::SaveBlueprint(const Blueprint &bp, std::string *error_msg)
{
	if (!_initialized) Initialize();
	if (error_msg != nullptr) error_msg->clear();
	if (!bp.IsValid()) return StorageError(error_msg, "Blueprint is invalid.");
	if (!UniquePlayerName(bp.name, error_msg)) return false;
	Blueprint copy = bp;
	copy.is_builtin = false;
	std::string bytes = copy.ToJson();
	if (bytes.size() > Blueprint::MAX_JSON_BYTES) return StorageError(error_msg, "Blueprint JSON exceeds 2 MiB.");
	auto dir = LibraryDirectory();
	if (!EnsureDirectory(dir, error_msg)) return false;
	size_t index = FindPlayerName(copy.name);
	std::filesystem::path path;
	if (index < _blueprints.size()) {
		path = _source_paths[index];
		if (path.empty()) return StorageError(error_msg, "Blueprint source path is unknown.");
		if (!AtomicWrite(path, bytes, true, error_msg)) return false;
		_blueprints[index] = std::move(copy);
	} else {
		std::string base = CleanBlueprintFilename(copy.name);
		if (base.size() > 100) base.resize(100);
		for (size_t suffix = 0; suffix < 10000; ++suffix) {
			path = dir / OTTD2FS(base + (suffix == 0 ? "" : "-" + std::to_string(suffix)) + ".json");
			std::error_code ec;
			auto status = std::filesystem::symlink_status(path, ec);
			if (ec && ec != std::errc::no_such_file_or_directory) return StorageError(error_msg, "Cannot inspect blueprint destination: " + ec.message());
			if (ec == std::errc::no_such_file_or_directory || !std::filesystem::exists(status)) break;
			if (suffix == 9999) return StorageError(error_msg, "No free blueprint filename is available.");
		}
		if (!AtomicWrite(path, bytes, false, error_msg)) return false;
		_blueprints.push_back(std::move(copy));
		_source_paths.push_back(path);
	}
	return true;
}

bool BlueprintManager::DeleteBlueprint(size_t index, std::string *error_msg)
{
	if (error_msg != nullptr) error_msg->clear();
	if (!_initialized) Initialize();
	if (index >= _blueprints.size() || _blueprints[index].is_builtin) return StorageError(error_msg, "Cannot delete a built-in or missing blueprint.");
	const auto &path = _source_paths[index];
	if (!SafeRegularFile(path, error_msg)) return false;
	std::error_code ec;
	bool removed = std::filesystem::remove(path, ec);
	if (ec || !removed) return StorageError(error_msg, "Cannot delete blueprint file: " + ec.message());
	_blueprints.erase(_blueprints.begin() + index);
	_source_paths.erase(_source_paths.begin() + index);
	return true;
}

bool BlueprintManager::RenameBlueprint(size_t index, const std::string &new_name, std::string *error_msg)
{
	if (error_msg != nullptr) error_msg->clear();
	if (!_initialized) Initialize();
	if (index >= _blueprints.size() || _blueprints[index].is_builtin) return StorageError(error_msg, "Cannot rename a built-in or missing blueprint.");
	if (new_name.empty()) return StorageError(error_msg, "Blueprint name cannot be empty.");
	for (size_t i = 0; i < _blueprints.size(); ++i) if (i != index && !_blueprints[i].is_builtin && _blueprints[i].name == new_name) return StorageError(error_msg, "A player blueprint already has this name.");
	Blueprint copy = _blueprints[index];
	copy.name = new_name;
	if (!copy.IsValid()) return StorageError(error_msg, "Renamed blueprint is invalid.");
	std::string bytes = copy.ToJson();
	if (bytes.size() > Blueprint::MAX_JSON_BYTES) return StorageError(error_msg, "Blueprint JSON exceeds 2 MiB.");
	if (!AtomicWrite(_source_paths[index], bytes, true, error_msg)) return false;
	_blueprints[index] = std::move(copy);
	return true;
}

bool BlueprintManager::ExportToFile(const Blueprint &bp, const std::string &path, std::string *error_msg)
{
	if (error_msg != nullptr) error_msg->clear();
	if (!bp.IsValid() || path.empty()) return StorageError(error_msg, "Invalid blueprint or export path.");
	if (path.find('\0') != std::string::npos) return StorageError(error_msg, "Export path contains a NUL byte.");
	std::string bytes = bp.ToJson();
	if (bytes.size() > Blueprint::MAX_JSON_BYTES) return StorageError(error_msg, "Blueprint JSON exceeds 2 MiB.");
	try {
		std::filesystem::path target = OTTD2FS(path);
		std::error_code ec;
		if (!std::filesystem::is_directory(target.has_parent_path() ? target.parent_path() : std::filesystem::path{"."}, ec) || ec) return StorageError(error_msg, "Export parent directory does not exist.");
		return AtomicWrite(target, bytes, false, error_msg);
	} catch (const std::exception &e) {
		return StorageError(error_msg, "Cannot use export path: " + std::string(e.what()));
	}
}

std::string BlueprintManager::GetDefaultExportPath(const Blueprint &bp)
{
	try {
		auto dir = LibraryDirectory();
		if (dir.empty()) return {};
		/* Keep exports outside the scanned library so reopening does not import them implicitly. */
		auto path = (dir / ".." / OTTD2FS(CleanBlueprintFilename(bp.name) + "-export.json")).lexically_normal();
		return FS2OTTD(path.native());
	} catch (const std::exception &) {
		return {};
	}
}

bool BlueprintManager::ImportFromFile(const std::string &path, std::string *error_msg)
{
	if (error_msg != nullptr) error_msg->clear();
	if (path.empty() || path.find('\0') != std::string::npos) return StorageError(error_msg, "Import path is empty or contains a NUL byte.");
	std::string bytes;
	try {
		if (!ReadBounded(OTTD2FS(path), bytes, error_msg)) return false;
	} catch (const std::exception &e) {
		return StorageError(error_msg, "Cannot use import path: " + std::string(e.what()));
	}
	return ImportFromString(bytes, error_msg);
}

std::optional<Blueprint> BlueprintManager::CaptureArea(TileIndex start_tile, TileIndex end_tile, const std::string &name)
{
	if (!Company::IsValidID(_current_company)) return std::nullopt;
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
				if (GetTileOwner(tile) != _current_company) return std::nullopt;

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
				if (GetTileOwner(tile) != _current_company) return std::nullopt;
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
	if (!_initialized) Initialize();
	if (FindPlayerName(bp->name) < _blueprints.size()) return StorageError(error_msg, "A player blueprint already has this name.");
	return SaveBlueprint(*bp, error_msg);
}
