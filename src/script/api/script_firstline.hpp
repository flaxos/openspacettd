/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file script_firstline.hpp Pure facts for the First Permanent Line Story Book. */
#ifndef SCRIPT_FIRSTLINE_HPP
#define SCRIPT_FIRSTLINE_HPP
#include "script_company.hpp"
#include "../../town_type.h"
#include "../../story_type.h"
#include "../../goal_type.h"
/**
 * Read-only facts from the optional native mission observer.
 * @api game
 */
class ScriptFirstLine : public ScriptObject {
public:
	/** @return Whether this save contains the authored mission. */
	static bool IsActive();
	/** @return Bits: works 1, commission 2, houses 4, repeat 8, victory 16, invalid 32. */
	static int GetProgress();
	/** @return Company receiving the commission, or invalid company. */
	static ScriptCompany::CompanyID GetCompany();
	/** @return Native target town identity, or invalid town. */
	static TownID GetTown();
	/**
	 * @param index Iron 0, silica 1, steel 2, ballast 3, town 4.
	 * @return Native location, or invalid tile.
	 */
	static TileIndex GetLocation(int index);
	/**
	 * @param index House index 0..3.
	 * @return Actual new protected house, or invalid tile.
	 */
	static TileIndex GetHouse(int index);
	/**
	 * @param title Exact mission page title.
	 * @return Existing owned page, or invalid.
	 */
	static StoryPageID FindPage(const std::string &title);
	/**
	 * @param text Exact mission goal text.
	 * @return Existing owned goal, or invalid.
	 */
	static GoalID FindGoal(const std::string &text);
	/**
	 * Find a native mission UI element after a save interrupted a script command.
	 * @param page Existing mission page identity.
	 * @param type Native story element type.
	 * @param reference Location tile or goal identity, or zero for text.
	 * @param text Exact element text, or an empty string for a goal.
	 * @return Existing matching element, or invalid.
	 */
	static StoryPageElementID FindElement(StoryPageID page, int type, int reference, const std::string &text);
};

#endif
