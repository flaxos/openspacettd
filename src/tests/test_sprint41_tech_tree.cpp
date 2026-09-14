/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenSpaceTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenSpaceTTD.
 */

/** @file test_sprint41_tech_tree.cpp Unit tests for Sprint 41 Commonwealth Tech Tree & R&D Projects. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"

#include "../portal/tech_tree.h"
#include "../portal/corporate_hq.h"
#include "../portal/company_stockpile.h"
#include "../portal/fabrication_manager.h"
#include "../portal/planet_manager.h"
#include "../portal/portal_cmd.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "mock_environment.h"

#include "../safeguards.h"

TEST_CASE("Sprint 41 Tech Tree - Catalog & Branches")
{
	TechTreeManager::Reset();

	const auto &nodes = TechTreeManager::GetAllNodes();
	CHECK(nodes.size() == 12);

	auto traction_nodes = TechTreeManager::GetNodesByBranch(TechBranch::Traction);
	CHECK(traction_nodes.size() == 4);
	CHECK(traction_nodes[0].id == TECH_TRACTION_1);
	CHECK(traction_nodes[0].tier == 1);
	CHECK(traction_nodes[0].cost_rp == 100);
	CHECK(traction_nodes[0].prerequisites.empty());

	CHECK(traction_nodes[1].id == TECH_TRACTION_2);
	CHECK(traction_nodes[1].tier == 2);
	CHECK(traction_nodes[1].cost_rp == 250);
	REQUIRE(traction_nodes[1].prerequisites.size() == 1);
	CHECK(traction_nodes[1].prerequisites[0] == TECH_TRACTION_1);

	CHECK(traction_nodes[2].id == TECH_TRACTION_3);
	CHECK(traction_nodes[2].tier == 3);
	CHECK(traction_nodes[2].cost_rp == 600);
	REQUIRE(traction_nodes[2].prerequisites.size() == 1);
	CHECK(traction_nodes[2].prerequisites[0] == TECH_TRACTION_2);

	CHECK(traction_nodes[3].id == TECH_TRACTION_4);
	CHECK(traction_nodes[3].tier == 4);
	CHECK(traction_nodes[3].cost_rp == 1500);
	REQUIRE(traction_nodes[3].prerequisites.size() == 1);
	CHECK(traction_nodes[3].prerequisites[0] == TECH_TRACTION_3);

	auto portal_nodes = TechTreeManager::GetNodesByBranch(TechBranch::PortalPhysics);
	CHECK(portal_nodes.size() == 4);
	CHECK(portal_nodes[0].id == TECH_PORTAL_1);
	CHECK(portal_nodes[1].id == TECH_PORTAL_2);
	CHECK(portal_nodes[2].id == TECH_PORTAL_3);
	CHECK(portal_nodes[3].id == TECH_PORTAL_4);
	CHECK(portal_nodes[3].cost_rp == 2000);

	auto material_nodes = TechTreeManager::GetNodesByBranch(TechBranch::Materials);
	CHECK(material_nodes.size() == 4);
	CHECK(material_nodes[0].id == TECH_MATERIALS_1);
	CHECK(material_nodes[1].id == TECH_MATERIALS_2);
	CHECK(material_nodes[2].id == TECH_MATERIALS_3);
	CHECK(material_nodes[3].id == TECH_MATERIALS_4);
}

TEST_CASE("Sprint 41 Tech Tree - Corporate HQ Eligibility & Prerequisites")
{
	(void)MockEnvironment::Instance();
	TechTreeManager::Reset();
	CorporateHQManager::Reset();

	CompanyID c0{0};
	std::string err;

	/* Cannot research without Corporate HQ */
	CHECK_FALSE(CorporateHQManager::HasHQ(c0));
	CHECK_FALSE(TechTreeManager::CanResearch(c0, TECH_TRACTION_1, err));
	CHECK(err.find("Corporate Headquarters") != std::string::npos);

	/* Establish Corporate HQ on World 1 */
	WorldID w1{1};
	TileIndex hq_tile{1000};
	CorporateHQManager::RegisterHQ(c0, w1, hq_tile, "CST Central Spire");
	CHECK(CorporateHQManager::HasHQ(c0));

	/* Now Tier 1 can be researched */
	CHECK(TechTreeManager::CanResearch(c0, TECH_TRACTION_1, err));

	/* Tier 2 cannot be researched until Tier 1 prerequisite is unlocked */
	CHECK_FALSE(TechTreeManager::CanResearch(c0, TECH_TRACTION_2, err));
	CHECK(err.find("Prerequisite") != std::string::npos);

	/* Unlock Tier 1 and re-check */
	TechTreeManager::SetActiveProject(c0, TECH_TRACTION_1);
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_TRACTION_1);

	TechTreeManager::AddResearchPoints(c0, 100);
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_TRACTION_1));
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_NONE);

	/* Now Tier 2 is eligible */
	CHECK(TechTreeManager::CanResearch(c0, TECH_TRACTION_2, err));

	/* Tier 1 cannot be researched again */
	CHECK_FALSE(TechTreeManager::CanResearch(c0, TECH_TRACTION_1, err));
	CHECK(err.find("already researched") != std::string::npos);
}

TEST_CASE("Sprint 41 Tech Tree - Server Commands & Budget Allocation")
{
	(void)MockEnvironment::Instance();
	TechTreeManager::Reset();
	CorporateHQManager::Reset();

	CompanyID c0{0};
	_current_company = c0;
	CorporateHQManager::RegisterHQ(c0, WorldID{1}, TileIndex{1000}, "CST HQ");

	/* Set research budget */
	auto res_budget = Command<Commands::SetResearchBudget>::Do(DoCommandFlag::Execute, 50000);
	CHECK(res_budget.Succeeded());
	CHECK(TechTreeManager::GetMonthlyBudget(c0) == 50000);

	/* Select research project */
	auto res_select = Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_MATERIALS_1);
	CHECK(res_select.Succeeded());
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_MATERIALS_1);

	/* Cancel active research project */
	res_select = Command<Commands::SelectResearchProject>::Do(DoCommandFlag::Execute, TECH_NONE);
	CHECK(res_select.Succeeded());
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_NONE);
}

TEST_CASE("Sprint 41 Tech Tree - Monthly Progression & Feedstock Burning")
{
	(void)MockEnvironment::Instance();
	TechTreeManager::Reset();
	CorporateHQManager::Reset();
	StockpileManager::Reset();

	_company_pool.CleanPool();
	Company::CreateAtIndex(CompanyID{0});
	CompanyID c0{0};
	_current_company = c0;
	Company *comp = Company::Get(c0);
	REQUIRE(comp != nullptr);
	comp->money = 5000000;
	WorldID hq_world{1};
	CorporateHQManager::RegisterHQ(c0, hq_world, TileIndex{1000}, "Sol Prime HQ");

	/* Select project: Dual-Track Throat Arrays (cost: 300 RP) */
	TechTreeManager::RestoreCompanyTech(c0, TECH_NONE, 0, 0, {TECH_PORTAL_1});
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_PORTAL_1));

	TechTreeManager::SetActiveProject(c0, TECH_PORTAL_2);
	TechTreeManager::SetMonthlyBudget(c0, 50000); // 50k Cr -> 50 RP / mo

	/* Stockpile feedstocks: 5 Enriched Crystals (50 RP) + 10 Electronics (50 RP) */
	CargoType cr_cargo = StockpileManager::RoleToDefaultCargo(FabricationRole::EnrichedCrystals);
	CargoType el_cargo = StockpileManager::RoleToDefaultCargo(FabricationRole::Electronics);
	StockpileManager::AddCargo(hq_world, c0, cr_cargo, 10);
	StockpileManager::AddCargo(hq_world, c0, el_cargo, 20);

	/* Month 1: 50 (budget) + 50 (5 crystals) + 50 (10 electronics) = 150 RP */
	TechTreeManager::ProcessMonthlyResearch();
	CHECK(TechTreeManager::GetAccumulatedRP(c0) == 150);
	CHECK(StockpileManager::GetStock(hq_world, c0, cr_cargo) == 5);  // 10 - 5 = 5
	CHECK(StockpileManager::GetStock(hq_world, c0, el_cargo) == 10); // 20 - 10 = 10
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(c0, TECH_PORTAL_2));

	/* Month 2: Another 150 RP -> total 300 RP -> Completed! */
	TechTreeManager::ProcessMonthlyResearch();
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_PORTAL_2));
	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_NONE);
	CHECK(TechTreeManager::GetAccumulatedRP(c0) == 0);
	CHECK(StockpileManager::GetStock(hq_world, c0, cr_cargo) == 0);  // 5 - 5 = 0
	CHECK(StockpileManager::GetStock(hq_world, c0, el_cargo) == 0);  // 10 - 10 = 0
}

TEST_CASE("Sprint 41 Tech Tree - Fabrication Bonus Discount Integration")
{
	(void)MockEnvironment::Instance();
	TechTreeManager::Reset();
	FabricationManager::Reset();

	CompanyID c0{0};
	CompanyID c1{1};

	/* Standard base discount is 80% */
	CHECK(FabricationManager::GetBOMDiscountPercent(c0) == 80);
	CHECK(FabricationManager::GetBOMDiscountPercent(c1) == 80);

	/* Unlock TECH_MATERIALS_3 for Company 0 */
	TechTreeManager::RestoreCompanyTech(c0, TECH_NONE, 0, 0, {TECH_MATERIALS_1, TECH_MATERIALS_2, TECH_MATERIALS_3});

	/* Company 0 receives upgraded 90% discount (leaving 10% labor fee) */
	CHECK(FabricationManager::GetBOMDiscountPercent(c0) == 90);

	/* Company 1 remains at base 80% discount */
	CHECK(FabricationManager::GetBOMDiscountPercent(c1) == 80);
}

TEST_CASE("Sprint 41 Tech Tree - Save/Load State Restoration")
{
	TechTreeManager::Reset();

	CompanyID c0{0};
	CompanyID c1{1};

	TechTreeManager::RestoreCompanyTech(c0, TECH_TRACTION_3, 420, 100000, {TECH_TRACTION_1, TECH_TRACTION_2});
	TechTreeManager::RestoreCompanyTech(c1, TECH_PORTAL_1, 50, 25000, {});

	auto states = TechTreeManager::GetAllCompanyTechStates();
	CHECK(states.size() == 2);

	CHECK(TechTreeManager::GetActiveProject(c0) == TECH_TRACTION_3);
	CHECK(TechTreeManager::GetAccumulatedRP(c0) == 420);
	CHECK(TechTreeManager::GetMonthlyBudget(c0) == 100000);
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_TRACTION_1));
	CHECK(TechTreeManager::IsTechUnlocked(c0, TECH_TRACTION_2));
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(c0, TECH_TRACTION_3));

	CHECK(TechTreeManager::GetActiveProject(c1) == TECH_PORTAL_1);
	CHECK(TechTreeManager::GetAccumulatedRP(c1) == 50);
	CHECK(TechTreeManager::GetMonthlyBudget(c1) == 25000);
	CHECK_FALSE(TechTreeManager::IsTechUnlocked(c1, TECH_PORTAL_1));
}
