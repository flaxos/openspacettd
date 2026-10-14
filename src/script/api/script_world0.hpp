/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file script_world0.hpp Pure saved facts for The Vale Railway. */
#ifndef SCRIPT_WORLD0_HPP
#define SCRIPT_WORLD0_HPP
#include "script_company.hpp"
#include "../../story_type.h"
#include "../../goal_type.h"
/**
 * Read-only native founding chapter facts.
 * @api game
 */
class ScriptWorld0 : public ScriptObject {
public:
	/** @return Whether the save contains the optional Aster Vale opening. */
	static bool IsActive();
	/** @return Bits: input and batches 1, yard storage 2, repeat 4, award 8, unavailable 16. */
	static int GetProgress();
	/** @return Native charter owner, or invalid company after loss. */
	static ScriptCompany::CompanyID GetCompany();
	/** @param index Quarry 0, crusher 1, yard 2, Greybank 3, Stonecross 4, Merefield 5.
	 * @return Native tile, or invalid tile. */
	static TileIndex GetLocation(int index);
	/** @param index Accepted SILC 0, qualifying native batches 1, stored BALL 2.
	 * @return Native observed count, or zero for an invalid index. */
	static int GetCounter(int index);
	/** @return Native explanation of lost bound assets, or an empty string. */
	static std::optional<std::string> GetReason();
	/** @param title Exact owned page title. @return Existing page, or invalid. */
	static StoryPageID FindPage(const std::string &title);
	/** @param text Exact owned goal text. @return Existing goal, or invalid. */
	static GoalID FindGoal(const std::string &text);
	/** @param page Owned page. @param type Native element type.
	 * @param reference Native location or goal identity. @param text Exact text, empty for goals.
	 * @return Existing native element, or invalid. */
	static StoryPageElementID FindElement(StoryPageID page, int type, int reference, const std::string &text);
};

#endif
