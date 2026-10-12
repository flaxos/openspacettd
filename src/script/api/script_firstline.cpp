/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file script_firstline.cpp Pure native mission queries without simulation mutations. */
#include "../../stdafx.h"
#include "script_firstline.hpp"
#include "../../portal/first_permanent_line.h"
#include "../../industry.h"
#include "../../town.h"
#include "../../story_base.h"
#include "../../goal_base.h"
#include "../../safeguards.h"
bool ScriptFirstLine::IsActive() { return FirstPermanentLine::Active(); }
int ScriptFirstLine::GetProgress()
{
	const auto &s = FirstPermanentLine::State();
	if (!s.active) return 0;
	if (s.awarded) return 31;
	int bits = s.input_delivered[0] && s.input_delivered[1] && s.produced_after_input[0] && s.produced_after_input[1] ? 1 : 0;
	if (s.commissioned) bits |= 2;
	if (s.house_count == 4 && std::ranges::all_of(s.house_completed, [](bool complete) { return complete; })) bits |= 4;
	for (const auto &[id, j] : s.journeys) if (j.phase == 3) bits |= 8;
	if (s.awarded) bits |= 16;
	if (s.invalid) bits |= 32;
	return bits;
}
ScriptCompany::CompanyID ScriptFirstLine::GetCompany() { return ScriptCompany::ToScriptCompanyID(FirstPermanentLine::State().binding.company); }
TownID ScriptFirstLine::GetTown() { return FirstPermanentLine::State().binding.mito; }
TileIndex ScriptFirstLine::GetLocation(int index)
{
	const auto &b = FirstPermanentLine::State().binding;
	if (!FirstPermanentLine::Active()) return INVALID_TILE;
	if (index == 4) { const Town *town = Town::GetIfValid(b.mito); return town == nullptr ? INVALID_TILE : town->xy; }
	const std::array<IndustryID, 4> ids{b.iron_source, b.silica_source, b.steel_works, b.ballast_works};
	const Industry *industry = index >= 0 && index < 4 ? Industry::GetIfValid(ids[index]) : nullptr;
	return industry == nullptr ? INVALID_TILE : industry->location.tile;
}
TileIndex ScriptFirstLine::GetHouse(int index)
{
	const auto &s = FirstPermanentLine::State();
	return s.active && index >= 0 && index < s.house_count ? s.houses[index] : INVALID_TILE;
}

StoryPageID ScriptFirstLine::FindPage(const std::string &title)
{
	if (FirstPermanentLine::Active()) for (const StoryPage *page : StoryPage::Iterate())
		if (page->company == FirstPermanentLine::State().binding.company && page->title.GetDecodedString() == title) return page->index;
	return StoryPageID::Invalid();
}
GoalID ScriptFirstLine::FindGoal(const std::string &text)
{
	if (FirstPermanentLine::Active()) for (const Goal *goal : Goal::Iterate())
		if (goal->company == FirstPermanentLine::State().binding.company && goal->text.GetDecodedString() == text) return goal->index;
	return GoalID::Invalid();
}
StoryPageElementID ScriptFirstLine::FindElement(StoryPageID page, int type, int reference, const std::string &text)
{
	const StoryPage *native_page = StoryPage::GetIfValid(page);
	if (!FirstPermanentLine::Active() || native_page == nullptr || native_page->company != FirstPermanentLine::State().binding.company) return StoryPageElementID::Invalid();
	for (const StoryPageElement *element : StoryPageElement::Iterate())
		if (element->page == page && int(element->type) == type && int64_t(element->referenced_id) == reference &&
			element->text.GetDecodedString() == text) return element->index;
	return StoryPageElementID::Invalid();
}
