/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_error_gui.cpp Regression tests for ownership error manager faces. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../company_base.h"
#include "../company_func.h"
#include "../error.h"
#include "../strings_func.h"
#include "../table/strings.h"

#include "../safeguards.h"

static EncodedString MakeErrorSummary()
{
	return GetEncodedString(STR_JUST_RAW_STRING, std::string_view{"Test ownership error"});
}

TEST_CASE("Ownership errors only show manager faces for real companies")
{
	_company_pool.CleanPool();

	REQUIRE(Company::CanAllocateItem(2));
	Company *local = Company::Create();
	Company *competitor = Company::Create();
	REQUIRE(local != nullptr);
	REQUIRE(competitor != nullptr);

	_current_company = local->index;
	_local_company = local->index;

	SECTION("A competing company remains attached to the ownership error")
	{
		CommandCost cost = CheckOwnership(competitor->index);
		REQUIRE(cost.Failed());
		CHECK(cost.GetErrorOwner() == competitor->index);

		ErrorMessageData data(MakeErrorSummary(), {}, false, 0, 0, {}, cost.GetErrorOwner());
		CHECK(data.HasFace());
	}

	SECTION("Neutral ownership does not request a manager face")
	{
		CommandCost cost = CheckOwnership(OWNER_NONE);
		REQUIRE(cost.Failed());
		CHECK(cost.GetErrorOwner() == CompanyID::Invalid());

		ErrorMessageData data(MakeErrorSummary(), {}, false, 0, 0, {}, cost.GetErrorOwner());
		CHECK_FALSE(data.HasFace());
	}

	SECTION("The error window normalises pseudo owners from any caller")
	{
		ErrorMessageData neutral(MakeErrorSummary(), {}, false, 0, 0, {}, OWNER_NONE);
		ErrorMessageData water(MakeErrorSummary(), {}, false, 0, 0, {}, OWNER_WATER);
		ErrorMessageData deity(MakeErrorSummary(), {}, false, 0, 0, {}, OWNER_DEITY);

		CHECK_FALSE(neutral.HasFace());
		CHECK_FALSE(water.HasFace());
		CHECK_FALSE(deity.HasFace());
	}
}
