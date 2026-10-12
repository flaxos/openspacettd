/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_line_approach.h Bounded native-command previews for authored gate approaches. */
#ifndef FIRST_LINE_APPROACH_H
#define FIRST_LINE_APPROACH_H

#include "portal_type.h"
#include "../tile_type.h"
#include "../track_type.h"
#include "../3rdparty/nlohmann/json.hpp"
#include <map>
#include <set>
#include <vector>

struct PortalEndpoint;

struct FirstLineApproachBudget {
	static constexpr uint32_t MAX_STATES = 150000;
	static constexpr uint32_t MAX_QUERIES = 600000;
	uint32_t states = 0;
	uint32_t queries = 0;
	bool exhausted = false;
	bool State() { if (this->states >= MAX_STATES) return this->exhausted = true, false; ++this->states; return true; }
	bool Query() { if (this->queries >= MAX_QUERIES) return this->exhausted = true, false; ++this->queries; return true; }
};

struct FirstLineApproachPiece {
	enum class Kind : uint8_t { Rail, Bridge, ReuseBridge } kind = Kind::Rail;
	TileIndex tile = INVALID_TILE; ///< Rail tile or bridge near endpoint.
	Track track = Track::Invalid;
	TileIndex end = INVALID_TILE; ///< Bridge far endpoint.
};

struct FirstLineApproachPlan {
	TileIndex platform = INVALID_TILE;
	TileIndex start = INVALID_TILE;
	TileIndex target = INVALID_TILE;
	Axis axis = Axis::X;
	std::vector<FirstLineApproachPiece> pieces;
	int64_t cost = 0;
};

/** Every reserved span tile maps to the same two native bridge endpoints. */
using FirstLineBridgeReservations = std::map<TileIndex, std::pair<TileIndex, TileIndex>>;

/** Preview one fixed platform against the unchanged native map. Output reservations commit only on success. */
bool FirstLineQuoteApproach(TileIndex platform, Axis axis, uint length, const PortalEndpoint &gate,
	const std::set<TileIndex> &blocked, std::map<TileIndex, TrackBits> &planned_rails,
	FirstLineBridgeReservations &reserved_bridges, FirstLineApproachBudget &budget,
	FirstLineApproachPlan &plan, nlohmann::json &report);

/** Requote and execute the ordered ordinary native pieces; stops on the first refusal. */
bool FirstLineExecuteApproach(const FirstLineApproachPlan &plan, nlohmann::json &report);

#endif /* FIRST_LINE_APPROACH_H */
