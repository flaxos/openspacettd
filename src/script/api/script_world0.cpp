/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file script_world0.cpp Pure facts; no progress evaluation or reward command. */
#include "../../stdafx.h"
#include "script_world0.hpp"
#include "../../portal/world0_opening.h"
#include "../../industry.h"
#include "../../station_base.h"
#include "../../town.h"
#include "../../story_base.h"
#include "../../goal_base.h"
#include "../../safeguards.h"
bool ScriptWorld0::IsActive() { return World0Opening::Active(); }
int ScriptWorld0::GetProgress()
{
	const auto &s = World0Opening::State();
	if (!s.active) return 0;
	int bits = s.input_goal && s.production_goal ? 1 : 0;
	if (s.storage_goal) bits |= 2;
	if (s.awarded) bits |= 12;
	if (s.unavailable) bits |= 16;
	return bits;
}
ScriptCompany::CompanyID ScriptWorld0::GetCompany() { return ScriptCompany::ToScriptCompanyID(World0Opening::State().profile.company); }
TileIndex ScriptWorld0::GetLocation(int index)
{
	const auto &s = World0Opening::State();
	if (!s.active) return INVALID_TILE;
	if (index == 1) return s.profile.crusher_plot;
	if (index >= 3 && index < 6) { const Town *town = Town::GetIfValid(s.profile.towns[index - 3]); return town == nullptr ? INVALID_TILE : town->xy; }
	if (index == 2) { const Station *station = Station::GetIfValid(s.binding.yard); return station == nullptr ? INVALID_TILE : station->xy; }
	IndustryID id = index == 0 ? s.binding.quarry : index == 1 ? s.binding.crusher : IndustryID::Invalid();
	const Industry *industry = Industry::GetIfValid(id);
	return industry == nullptr ? INVALID_TILE : industry->location.tile;
}
int ScriptWorld0::GetCounter(int index)
{
	const auto &s = World0Opening::State();
	return !s.active ? 0 : index == 0 ? s.accepted_silc : index == 1 ? s.qualified_batches : index == 2 ? s.stored_ball : 0;
}
std::optional<std::string> ScriptWorld0::GetReason() { return World0Opening::State().reason; }
StoryPageID ScriptWorld0::FindPage(const std::string &title)
{
	if (IsActive()) for (const StoryPage *page : StoryPage::Iterate())
		if (page->company == World0Opening::State().profile.company && page->title.GetDecodedString() == title) return page->index;
	return StoryPageID::Invalid();
}
GoalID ScriptWorld0::FindGoal(const std::string &text)
{
	if (IsActive()) for (const Goal *goal : Goal::Iterate())
		if (goal->company == World0Opening::State().profile.company && goal->text.GetDecodedString() == text) return goal->index;
	return GoalID::Invalid();
}
StoryPageElementID ScriptWorld0::FindElement(StoryPageID page, int type, int reference, const std::string &text)
{
	const StoryPage *p = StoryPage::GetIfValid(page);
	if (!IsActive() || p == nullptr || p->company != World0Opening::State().profile.company) return StoryPageElementID::Invalid();
	for (const StoryPageElement *element : StoryPageElement::Iterate())
		if (element->page == page && int(element->type) == type && int64_t(element->referenced_id) == reference && element->text.GetDecodedString() == text) return element->index;
	return StoryPageElementID::Invalid();
}
