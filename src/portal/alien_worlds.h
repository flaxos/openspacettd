/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file alien_worlds.h Authored Alien Frontier maps and their deterministic terrain toolkit. */
#ifndef ALIEN_WORLDS_H
#define ALIEN_WORLDS_H

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/** @brief Deterministic corner-height shapes for authored alien terrain. */
namespace AlienTerrain {

/** @brief Impact crater: steep inner wall, raised rim, outer apron and optional central lake. */
struct Crater {
	int x; ///< Centre X corner coordinate.
	int y; ///< Centre Y corner coordinate.
	int radius; ///< Rim crest distance from the centre.
	int rim; ///< Rim crest height.
	int floor; ///< Crater floor height.
	int lake; ///< Radius of the central basin falling to sea level, or 0 for none.
};

/** @brief Flat-topped mesa with cliff sides. */
struct Mesa {
	int x; ///< Centre X corner coordinate.
	int y; ///< Centre Y corner coordinate.
	int radius; ///< Radius of the flat top.
	int top; ///< Height of the flat top.
};

/** @brief Straight ridge whose flanks fall away from its crest line. */
struct Ridge {
	int x1; ///< First crest end X.
	int y1; ///< First crest end Y.
	int x2; ///< Second crest end X.
	int y2; ///< Second crest end Y.
	int crest; ///< Crest height.
};

/** @brief Level construction pad: every corner of the inclusive rectangle is exactly at its level. */
struct Pad {
	int x0; ///< First corner X.
	int y0; ///< First corner Y.
	int x1; ///< Last corner X.
	int y1; ///< Last corner Y.
	int level; ///< Pad height.
};

/** @brief Map-wide corner heights; corner (x, y) is the north corner of tile (x, y). */
class Field {
public:
	/**
	 * Create a field.
	 * @param width Map width in tiles.
	 * @param height Map height in tiles.
	 */
	Field(uint32_t width, uint32_t height);
	/** @return Map width. */
	uint32_t Width() const { return this->width; }
	/** @return Map height. */
	uint32_t Height() const { return this->height; }
	/**
	 * @param x Corner X.
	 * @param y Corner Y.
	 * @return Mutable unquantized height.
	 */
	float &At(uint32_t x, uint32_t y) { return this->heights[size_t(y) * this->width + x]; }
	/**
	 * @param x Corner X.
	 * @param y Corner Y.
	 * @return Unquantized height.
	 */
	float At(uint32_t x, uint32_t y) const { return this->heights[size_t(y) * this->width + x]; }
	/** @param crater Crater to carve over the existing terrain; it replaces anything inside its rim. */
	void AddCrater(const Crater &crater);
	/** @param mesa Mesa to raise above the existing terrain. */
	void AddMesa(const Mesa &mesa);
	/** @param ridge Ridge to raise above the existing terrain. */
	void AddRidge(const Ridge &ridge);
	/** @param pad Pad to level exactly, blending the surroundings at a half-level-per-tile grade. */
	void Level(const Pad &pad);
	/** Lower the terrain so that it reaches sea level at the outer map border. */
	void TaperEdges();
	/**
	 * Round to whole levels and lower any corner standing more than one level above a neighbour.
	 * @param max_height Highest allowed level.
	 * @return Corner levels whose edge-adjacent corners differ by at most one level.
	 */
	std::vector<uint8_t> Quantize(int max_height) const;

private:
	uint32_t width; ///< Map width.
	uint32_t height; ///< Map height.
	std::vector<float> heights; ///< Row-major unquantized corner heights.
};

/**
 * Count tiles that would form an invalid OpenTTD slope.
 * @param heights Row-major corner levels.
 * @param width Map width.
 * @param height Map height.
 * @return Tiles with an edge-adjacent corner pair more than one level apart.
 */
uint32_t CountInvalidTiles(const std::vector<uint8_t> &heights, uint32_t width, uint32_t height);

} // namespace AlienTerrain

/**
 * Dispatch the offline Alien Frontier adapter: prepare, status, audit, build or speed.
 * @param argv Console tokens.
 * @return True when the console command was handled; failures print an ALIEN FAIL receipt.
 */
bool ConAlienWorld(std::span<std::string_view> argv);

#endif /* ALIEN_WORLDS_H */
