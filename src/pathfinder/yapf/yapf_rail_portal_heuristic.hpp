/* This file is part of OpenSpaceTTD, licensed under the GNU GPL version 2. */
/** @file yapf_rail_portal_heuristic.hpp Rail distance lower bounds including local portal shortcuts. */

#ifndef YAPF_RAIL_PORTAL_HEURISTIC_HPP
#define YAPF_RAIL_PORTAL_HEURISTIC_HPP

#include "../../map_func.h"
#include "../../track_func.h"
#include "../../portal/portal_registry.h"
#include "../pathfinder_type.h"
#include <algorithm>
#include <tuple>
#include <vector>

/**
 * Shortest distances in a relaxed rail network: unrestricted octile movement
 * plus the real directed portal jumps. Removing track and signal restrictions
 * can only reduce the remaining cost, while retaining each portal's virtual
 * length makes the estimate consistent across distant, reorienting jumps.
 *
 * The small portal graph is solved once per search in O(P^2), then each node's
 * estimate costs O(P). No train route costs or node limits are changed.
 */
class YapfRailPortalHeuristic {
	/** Tile-centre/track-exit coordinates in half-tile units. */
	struct Coordinate { int x; int y; };
	/** A directed jump and its cheapest relaxed continuation to the target. */
	struct Jump {
		Coordinate entrance;
		Coordinate exit;
		int64_t cost;
		int64_t distance = 0;
		bool settled = false;
	};

	Coordinate destination{}; ///< Target tile centre.
	std::vector<Jump> jumps; ///< Local directed links, in deterministic coordinate order.

	/** Return the end of a track leaving a tile in the given direction. */
	static Coordinate ExitPoint(TileIndex tile, DiagDirection direction)
	{
		static constexpr int x_offset[] = {-1, 0, 1, 0};
		static constexpr int y_offset[] = {0, 1, 0, -1};
		return {2 * static_cast<int>(TileX(tile)) + x_offset[to_underlying(direction)],
			2 * static_cast<int>(TileY(tile)) + y_offset[to_underlying(direction)]};
	}

	/** Native octile metric between two points, before target half-tile adjustment. */
	static int Distance(Coordinate a, Coordinate b)
	{
		const int dx = abs(a.x - b.x);
		const int dy = abs(a.y - b.y);
		return std::min(dx, dy) * YAPF_TILE_CORNER_LENGTH + abs(dx - dy) * (YAPF_TILE_LENGTH / 2);
	}

	/** Add travel from the entry-facing head to the exit-facing remote head. */
	void AddJump(const PortalEndpoint &from, const PortalEndpoint &to, uint32_t virtual_length)
	{
		/* YAPF charges the virtual middle tiles plus the remote head tile. */
		this->jumps.push_back({ExitPoint(from.tile, from.enter_dir),
			ExitPoint(to.tile, ReverseDiagDir(to.enter_dir)), (int64_t{virtual_length} + 1) * YAPF_TILE_LENGTH});
	}

public:
	/**
	 * Prepare the relaxed portal graph for one destination.
	 * @param tile Destination tile selected by the native rail destination provider.
	 */
	void SetDestination(TileIndex tile)
	{
		this->destination = {2 * static_cast<int>(TileX(tile)), 2 * static_cast<int>(TileY(tile))};
		this->jumps.clear();
		for (const auto &[id, link] : PortalRegistry::GetAllPortals()) {
			if (link.end_a.tile >= Map::Size() || link.end_b.tile >= Map::Size()) continue;
			if (!IsValidDiagDirection(link.end_a.enter_dir) || !IsValidDiagDirection(link.end_b.enter_dir)) continue;
			this->AddJump(link.end_a, link.end_b, link.virtual_length);
			if (link.bidirectional) this->AddJump(link.end_b, link.end_a, link.virtual_length);
		}
		std::sort(this->jumps.begin(), this->jumps.end(), [](const Jump &a, const Jump &b) {
			return std::tie(a.entrance.x, a.entrance.y, a.exit.x, a.exit.y, a.cost) <
				std::tie(b.entrance.x, b.entrance.y, b.exit.x, b.exit.y, b.cost);
		});
		for (Jump &jump : this->jumps) jump.distance = jump.cost + Distance(jump.exit, this->destination);

		/* Reverse Dijkstra over jumps. After taking jump i, walk freely to
		 * the target or to the entry of another jump with a known distance. */
		for (size_t i = 0; i < this->jumps.size(); ++i) {
			Jump *next = nullptr;
			for (Jump &jump : this->jumps) {
				if (!jump.settled && (next == nullptr || jump.distance < next->distance)) next = &jump;
			}
			assert(next != nullptr);
			next->settled = true;
			for (Jump &jump : this->jumps) {
				if (jump.settled) continue;
				jump.distance = std::min(jump.distance,
					jump.cost + Distance(jump.exit, next->entrance) + next->distance);
			}
		}
	}

	/** Whether there are no local jumps and the original fast estimate can be used. */
	bool Empty() const { return this->jumps.empty(); }

	/**
	 * Lower-bound the remaining travel cost from a track exit to the target.
	 * @param tile Current node's last tile.
	 * @param trackdir Current node's last track direction.
	 * @return Minimum relaxed cost, using the native destination half-tile adjustment.
	 */
	int Estimate(TileIndex tile, Trackdir trackdir) const
	{
		const Coordinate point = ExitPoint(tile, TrackdirToExitdir(trackdir));
		int64_t distance = Distance(point, this->destination);
		for (const Jump &jump : this->jumps) {
			distance = std::min(distance, Distance(point, jump.entrance) + jump.distance);
		}
		return static_cast<int>(std::max<int64_t>(0, distance - YAPF_TILE_LENGTH / 2));
	}
};

#endif /* YAPF_RAIL_PORTAL_HEURISTIC_HPP */
