/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file test_universe_graph.cpp Unit tests for Sprint 49 Commonwealth Universe Graph Engine (WP-49.1 & WP-49.2). */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../portal/universe_graph.h"
#include <algorithm>

TEST_CASE("Sprint 49: UniverseGraphManager loads and parses canonical 108-world universe", "[sprint49][universe_graph]")
{
	auto &mgr = UniverseGraphManager::Instance();
	REQUIRE(mgr.EnsureLoaded());

	SECTION("Node and special body counts")
	{
		CHECK(mgr.GetNodeCount() == 108);
		CHECK(mgr.GetSpecialBodyCount() == 16);
		CHECK(mgr.GetTotalLocationCount() == 124);
		CHECK(mgr.GetConnectionCount() >= 115);
	}

	SECTION("Sol / Earth root identification and metadata")
	{
		const UniverseNode *earth = mgr.FindNode("world_earth");
		REQUIRE(earth != nullptr);
		CHECK(earth->canonical_name == "Earth");
		CHECK(earth->location_type == "homeworld");
		CHECK(earth->system == "Sol");
		CHECK(earth->economic_profile.megacity_eligible == true);
		CHECK(earth->economic_profile.base_population > 1000000000ULL);
	}

	SECTION("All Big15 core worlds are present and connected to Earth under Rule R0")
	{
		static const std::vector<std::string> big15_names = {
			"Augusta", "Bayovar", "Buta", "Democratic Republic of New Germany",
			"EdenBurg", "Granada", "Kerensk", "Los Vada", "Mito", "Orleans",
			"Piura", "Shayoni", "StLincoln", "Verona", "Wessex"
		};

		for (const auto &name : big15_names) {
			const UniverseNode *hub = mgr.FindNode(name);
			INFO("Checking Big15 Hub: " << name);
			REQUIRE(hub != nullptr);
			CHECK(hub->phase == WorldPhase::Phase1_Core);
			CHECK(hub->connects_to == "world_earth");
			CHECK(hub->connection_evidence == EvidenceClass::Reference);
			CHECK(hub->connection_mode == ConnectionMode::Rail);
			CHECK(hub->economic_profile.megacity_eligible == true);

			// Direct edge must exist in graph
			const UniverseEdge *edge = mgr.FindEdge(hub->world_id, "world_earth", GraphFilterMode::SOURCE_ONLY);
			REQUIRE(edge != nullptr);
			CHECK(edge->permits_through_running_train == true);
		}
	}

	SECTION("Tree traversal from Sol/Earth visits all reachable worlds")
	{
		auto traversal = mgr.TraverseTreeFromRoot("world_earth", GraphFilterMode::PLAYABLE_COMPLETION);
		// In PLAYABLE_COMPLETION, all connected nodes (except unassigned/enemy worlds) are reachable
		CHECK(traversal.size() >= 100);

		// First node traversed must be Earth
		REQUIRE(!traversal.empty());
		CHECK(traversal.front()->world_id == "world_earth");
	}

	SECTION("Graph filter modes enforce evidentiary admission boundaries")
	{
		// 1. PLAYABLE_COMPLETION includes E, R, I, A
		auto playable_edges = mgr.GetEdges(GraphFilterMode::PLAYABLE_COMPLETION);
		auto playable_reachable = mgr.TraverseTreeFromRoot("world_earth", GraphFilterMode::PLAYABLE_COMPLETION);

		// 2. RECONSTRUCTED includes E, R, I
		auto recon_edges = mgr.GetEdges(GraphFilterMode::RECONSTRUCTED);
		auto recon_reachable = mgr.TraverseTreeFromRoot("world_earth", GraphFilterMode::RECONSTRUCTED);

		// 3. SOURCE_ONLY includes E, R only
		auto source_edges = mgr.GetEdges(GraphFilterMode::SOURCE_ONLY);
		auto source_reachable = mgr.TraverseTreeFromRoot("world_earth", GraphFilterMode::SOURCE_ONLY);

		CHECK(source_edges.size() < recon_edges.size());
		CHECK(recon_edges.size() < playable_edges.size());

		CHECK(source_reachable.size() < recon_reachable.size());
		CHECK(recon_reachable.size() < playable_reachable.size());

		CHECK(source_reachable.size() >= 20);
		CHECK(recon_reachable.size() >= 30);
		CHECK(playable_reachable.size() >= 100);
	}

	SECTION("Modal transport restrictions are strictly maintained")
	{
		// Vinmar is DATA only - trains must not be permitted
		const UniverseNode *vinmar = mgr.FindNode("world_vinmar");
		REQUIRE(vinmar != nullptr);
		CHECK(vinmar->connection_mode == ConnectionMode::Data);
		CHECK(UniverseGraphManager::IsModePermittingTrains(vinmar->connection_mode) == false);

		const UniverseEdge *vinmar_edge = mgr.FindEdge("world_vinmar", "world_augusta", GraphFilterMode::SOURCE_ONLY);
		REQUIRE(vinmar_edge != nullptr);
		CHECK(vinmar_edge->permits_through_running_train == false);

		// Far Away is SCHEDULED
		const UniverseNode *far_away = mgr.FindNode("world_far_away");
		REQUIRE(far_away != nullptr);
		CHECK(far_away->connection_mode == ConnectionMode::Scheduled);

		// Merredin is RAIL
		const UniverseNode *merredin = mgr.FindNode("world_merredin");
		REQUIRE(merredin != nullptr);
		CHECK(merredin->connection_mode == ConnectionMode::Rail);
		CHECK(UniverseGraphManager::IsModePermittingTrains(merredin->connection_mode) == true);
	}

	SECTION("Economic profile calibration across phases")
	{
		// Big15 Hub
		const UniverseNode *augusta = mgr.FindNode("world_augusta");
		REQUIRE(augusta != nullptr);
		CHECK(augusta->economic_profile.role == "core_metropolitan_hub");
		CHECK(augusta->economic_profile.tariff_multiplier >= 1.2f);
		CHECK(augusta->economic_profile.megacity_eligible == true);

		// Phase 2 Industrial (Merredin)
		const UniverseNode *merredin = mgr.FindNode("world_merredin");
		REQUIRE(merredin != nullptr);
		CHECK(merredin->phase == WorldPhase::Phase2_Developed);
		CHECK(merredin->economic_profile.role == "heavy_refining_and_fabrication");
		const auto &exports = merredin->economic_profile.primary_exports;
		bool has_steel = std::find(exports.begin(), exports.end(), "STRUCTURAL_STEEL") != exports.end();
		CHECK(has_steel);

		// Phase 3 Frontier (Clonclurry)
		const UniverseNode *clonclurry = mgr.FindNode("world_clonclurry");
		REQUIRE(clonclurry != nullptr);
		CHECK(clonclurry->phase == WorldPhase::Phase3_Frontier);
		CHECK(clonclurry->economic_profile.role == "primary_resource_extraction");
		const auto &clon_exports = clonclurry->economic_profile.primary_exports;
		bool has_ore = std::find(clon_exports.begin(), clon_exports.end(), "IRON_ORE") != clon_exports.end();
		CHECK(has_ore);
	}

	SECTION("Connectivity validation and cycle detection")
	{
		std::string err;
		CHECK(mgr.ValidateConnectivity(GraphFilterMode::PLAYABLE_COMPLETION, &err) == true);
		CHECK(err.empty());

		// In PLAYABLE_COMPLETION, check path finding from Earth to Merredin
		CHECK(mgr.HasPath("world_earth", "world_merredin", GraphFilterMode::PLAYABLE_COMPLETION));
		auto path = mgr.GetPath("world_earth", "world_merredin", GraphFilterMode::PLAYABLE_COMPLETION);
		// Path should be Earth -> Mito -> Merredin
		REQUIRE(path.size() >= 3);
		CHECK(path.front() == "world_earth");
		CHECK(path.back() == "world_merredin");
	}

	SECTION("WP-49.2: Galaxy Map tree traversal, evidence badges, and phase filtering")
	{
		// Test Evidence Badges (E, R, I, A)
		CHECK(EvidenceClassToBadge(EvidenceClass::Explicit) == "E");
		CHECK(EvidenceClassToBadge(EvidenceClass::Reference) == "R");
		CHECK(EvidenceClassToBadge(EvidenceClass::Inferred) == "I");
		CHECK(EvidenceClassToBadge(EvidenceClass::Assumed) == "A");

		// Test Connection Mode string formatting
		CHECK(ConnectionModeToString(ConnectionMode::Rail) == "RAIL");
		CHECK(ConnectionModeToString(ConnectionMode::Scheduled) == "SCHEDULED");
		CHECK(ConnectionModeToString(ConnectionMode::Data) == "DATA");

		// Test tree traversal from Earth root
		auto tree_nodes = mgr.TraverseTreeFromRoot("world_earth", GraphFilterMode::PLAYABLE_COMPLETION);
		REQUIRE_FALSE(tree_nodes.empty());
		CHECK(tree_nodes.front()->world_id == "world_earth");

		// Earth children must include Big15 hubs
		auto earth_children = mgr.GetChildren("world_earth", GraphFilterMode::PLAYABLE_COMPLETION);
		CHECK(earth_children.size() >= 15);

		// Phase filtering counts across canonical nodes
		auto all_nodes = mgr.GetPrimaryNodes();
		size_t p1_count = 0, p2_count = 0, p3_count = 0;
		for (const auto *n : all_nodes) {
			if (n->phase == WorldPhase::Phase1_Core) p1_count++;
			else if (n->phase == WorldPhase::Phase2_Developed) p2_count++;
			else if (n->phase == WorldPhase::Phase3_Frontier) p3_count++;
		}
		CHECK(p1_count == 22);
		CHECK(p2_count == 9);
		CHECK(p3_count == 77);

		// World detail card verification for Far Away
		const UniverseNode *far_away = mgr.FindNode("world_far_away");
		REQUIRE(far_away != nullptr);
		CHECK(far_away->canonical_name == "Far Away");
		CHECK(far_away->phase == WorldPhase::Phase3_Frontier);
		CHECK(far_away->connection_mode == ConnectionMode::Scheduled);
		CHECK(far_away->connects_to == "world_half_way");
		CHECK(far_away->economic_profile.role == "primary_resource_extraction");
		CHECK_FALSE(far_away->economic_profile.primary_imports.empty());
		CHECK_FALSE(far_away->economic_profile.primary_exports.empty());
	}
}

