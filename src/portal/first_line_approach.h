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

/** Shared bounds for native-command approach searches across all route legs. */
struct FirstLineApproachBudget {
	static constexpr uint32_t MAX_STATES = 150000; ///< Maximum explored route states across legs.
	static constexpr uint32_t MAX_QUERIES = 600000; ///< Maximum native command previews across legs.
	uint32_t states = 0; ///< Route states already explored.
	uint32_t queries = 0; ///< Native construction previews already issued.
	bool exhausted = false; ///< A state or preview request was refused at its cap.
	/** Count one explored route state. @return Whether the shared state cap still permits it. */
	bool State() { if (this->states >= MAX_STATES) return this->exhausted = true, false; ++this->states; return true; }
	/** Count one native command preview. @return Whether the shared query cap still permits it. */
	bool Query() { if (this->queries >= MAX_QUERIES) return this->exhausted = true, false; ++this->queries; return true; }
};

/** One ordered ordinary rail build or existing bridge in an approach plan. */
struct FirstLineApproachPiece {
	/** How this piece uses the native map. */
	enum class Kind : uint8_t {
		Rail, ///< Build or reuse rail track on one tile.
		Bridge, ///< Build a new rail bridge between two endpoints.
		ReuseBridge ///< Travel through an existing owned rail bridge.
	} kind = Kind::Rail; ///< Selected piece kind.
	TileIndex tile = INVALID_TILE; ///< Rail tile or bridge near endpoint.
	Track track = Track::Invalid; ///< Track orientation for a rail-tile piece.
	TileIndex end = INVALID_TILE; ///< Bridge far endpoint.
};

/** Quoted ordered route from a station platform toward a portal terminal. */
struct FirstLineApproachPlan {
	TileIndex platform = INVALID_TILE; ///< First tile of the station platform.
	TileIndex start = INVALID_TILE; ///< Approach tile adjacent to the platform.
	TileIndex target = INVALID_TILE; ///< Tile next to the portal terminal.
	Axis axis = Axis::X; ///< Orientation of the station platform.
	std::vector<FirstLineApproachPiece> pieces; ///< Ordered native route pieces.
	int64_t cost = 0; ///< Sum of quoted native construction costs.
};

/** Every reserved span tile maps to the same two native bridge endpoints. */
using FirstLineBridgeReservations = std::map<TileIndex, std::pair<TileIndex, TileIndex>>;

/** Preview one fixed platform against the unchanged native map. Output reservations commit only on success.
 * @param platform First tile of the proposed or existing station platform.
 * @param axis Platform's rail axis.
 * @param length Number of platform tiles.
 * @param gate Portal endpoint to connect.
 * @param blocked Tiles excluded from the approach search.
 * @param planned_rails Existing and newly reserved track bits; updated only on success.
 * @param reserved_bridges Existing and newly reserved bridge spans; updated only on success.
 * @param budget Shared state and command-query budget.
 * @param plan Receives the ordered approach pieces on success.
 * @param report Receives bounded search and native-refusal diagnostics.
 * @return Whether a complete native-command-quoted approach was found.
 */
bool FirstLineQuoteApproach(TileIndex platform, Axis axis, uint length, const PortalEndpoint &gate,
	const std::set<TileIndex> &blocked, std::map<TileIndex, TrackBits> &planned_rails,
	FirstLineBridgeReservations &reserved_bridges, FirstLineApproachBudget &budget,
	FirstLineApproachPlan &plan, nlohmann::json &report);

/** Requote and execute the ordered ordinary native pieces; stops on the first refusal.
 * @param plan Previously quoted approach to build.
 * @param report Receives native costs and first-failure diagnostics.
 * @return Whether every piece was built at its quoted cost.
 */
bool FirstLineExecuteApproach(const FirstLineApproachPlan &plan, nlohmann::json &report);

#endif /* FIRST_LINE_APPROACH_H */
