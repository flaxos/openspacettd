/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_graph.cpp Implementation of Commonwealth 108-world universe graph topology manager. */

#include "../stdafx.h"
#include "universe_graph.h"
#include "../3rdparty/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>
#include <queue>
#include <sstream>

using json = nlohmann::json;

std::string EvidenceClassToString(EvidenceClass ev)
{
	switch (ev) {
		case EvidenceClass::Explicit:  return "Explicit (Novel text)";
		case EvidenceClass::Reference: return "Reference-Supported (Phase 1 structure)";
		case EvidenceClass::Inferred:  return "Inferred (Sector context)";
		case EvidenceClass::Assumed:   return "Assumed (Game completion topology)";
		case EvidenceClass::Unknown:
		default:                       return "Unknown / Unassigned";
	}
}

std::string EvidenceClassToBadge(EvidenceClass ev)
{
	switch (ev) {
		case EvidenceClass::Explicit:  return "E";
		case EvidenceClass::Reference: return "R";
		case EvidenceClass::Inferred:  return "I";
		case EvidenceClass::Assumed:   return "A";
		case EvidenceClass::Unknown:
		default:                       return "U";
	}
}

std::string ConnectionModeToString(ConnectionMode mode)
{
	switch (mode) {
		case ConnectionMode::Rail:        return "RAIL";
		case ConnectionMode::Gate:        return "GATE";
		case ConnectionMode::Scheduled:   return "SCHEDULED";
		case ConnectionMode::Private:     return "PRIVATE";
		case ConnectionMode::Exploration: return "EXPLORATION";
		case ConnectionMode::Data:        return "DATA";
		case ConnectionMode::SilfenRoute: return "SILFEN_ROUTE";
		case ConnectionMode::Orbital:     return "ORBITAL";
		case ConnectionMode::Shuttle:     return "SHUTTLE";
		case ConnectionMode::Aircraft:    return "AIRCRAFT";
		case ConnectionMode::None:
		default:                          return "NONE";
	}
}

std::string GraphFilterModeToString(GraphFilterMode mode)
{
	switch (mode) {
		case GraphFilterMode::SOURCE_ONLY:         return "SOURCE_ONLY";
		case GraphFilterMode::RECONSTRUCTED:       return "RECONSTRUCTED";
		case GraphFilterMode::PLAYABLE_COMPLETION: return "PLAYABLE_COMPLETION";
		default:                                   return "UNKNOWN";
	}
}

EvidenceClass ParseEvidenceClass(const std::string &str)
{
	if (str == "E" || str == "Explicit") return EvidenceClass::Explicit;
	if (str == "R" || str == "Reference") return EvidenceClass::Reference;
	if (str == "I" || str == "Inferred") return EvidenceClass::Inferred;
	if (str == "A" || str == "Assumed") return EvidenceClass::Assumed;
	return EvidenceClass::Unknown;
}

ConnectionMode ParseConnectionMode(const std::string &str)
{
	if (str == "RAIL") return ConnectionMode::Rail;
	if (str == "GATE") return ConnectionMode::Gate;
	if (str == "SCHEDULED") return ConnectionMode::Scheduled;
	if (str == "PRIVATE") return ConnectionMode::Private;
	if (str == "EXPLORATION") return ConnectionMode::Exploration;
	if (str == "DATA") return ConnectionMode::Data;
	if (str == "SILFEN_ROUTE") return ConnectionMode::SilfenRoute;
	if (str == "ORBITAL") return ConnectionMode::Orbital;
	if (str == "SHUTTLE") return ConnectionMode::Shuttle;
	if (str == "AIRCRAFT") return ConnectionMode::Aircraft;
	return ConnectionMode::None;
}

/**
 * Parse world phase from string.
 * @param str Input string identifier.
 * @return Parsed WorldPhase.
 */
static WorldPhase ParseWorldPhase(const std::string &str)
{
	if (str == "Phase1_Core" || str == "P1") return WorldPhase::Phase1_Core;
	if (str == "Phase2_Developed" || str == "P2") return WorldPhase::Phase2_Developed;
	if (str == "Phase3_Frontier" || str == "P3") return WorldPhase::Phase3_Frontier;
	if (str == "Phase4_Expansion" || str == "P4") return WorldPhase::Phase4_Expansion;
	return WorldPhase::Phase3_Frontier;
}

/**
 * Parse world biome from string.
 * @param str Input string identifier.
 * @return Parsed WorldBiome.
 */
static WorldBiome ParseWorldBiome(const std::string &str)
{
	if (str == "AridDesert") return WorldBiome::AridDesert;
	if (str == "BorealSnow" || str == "SubArctic") return WorldBiome::SubArctic;
	if (str == "VolcanicBasalt" || str == "Volcanic") return WorldBiome::Volcanic;
	if (str == "Oceanic") return WorldBiome::Oceanic;
	if (str == "SubTropic") return WorldBiome::SubTropic;
	return WorldBiome::Temperate;
}

static std::string ToLower(std::string_view str)
{
	std::string res(str);
	std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::tolower(c); });
	return res;
}

bool UniverseGraphManager::IsModePermittingTrains(ConnectionMode mode) noexcept
{
	return mode == ConnectionMode::Rail;
}

/**
 * Get singleton instance of UniverseGraphManager.
 * @return Reference to the UniverseGraphManager singleton.
 */
UniverseGraphManager &UniverseGraphManager::Instance()
{
	static UniverseGraphManager instance;
	return instance;
}

void UniverseGraphManager::Reset()
{
	this->_loaded = false;
	this->_nodes.clear();
	this->_special_bodies.clear();
	this->_edges.clear();
	this->_id_to_primary_index.clear();
	this->_id_to_special_index.clear();
	this->_name_to_id.clear();
}

bool UniverseGraphManager::EnsureLoaded()
{
	if (this->_loaded) return true;
	return this->LoadGraph();
}

bool UniverseGraphManager::LoadGraph(const std::string &filepath)
{
	std::vector<std::string> candidate_paths;
	if (!filepath.empty()) {
		candidate_paths.push_back(filepath);
	} else {
		candidate_paths.push_back("assets/data/commonwealth_universe.json");
		candidate_paths.push_back("bin/data/commonwealth_universe.json");
		candidate_paths.push_back("../assets/data/commonwealth_universe.json");
		candidate_paths.push_back("../bin/data/commonwealth_universe.json");
		candidate_paths.push_back("../../assets/data/commonwealth_universe.json");
		candidate_paths.push_back("../../bin/data/commonwealth_universe.json");
	}

	for (const auto &path : candidate_paths) {
		std::ifstream file(path);
		if (file.is_open()) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			if (this->LoadGraphFromJsonString(buffer.str())) {
				return true;
			}
		}
	}

	return false;
}

bool UniverseGraphManager::LoadGraphFromJsonString(const std::string &json_str)
{
	try {
		json root = json::parse(json_str);
		this->Reset();

		auto parse_node = [](const json &j, bool is_special) -> UniverseNode {
			UniverseNode node;
			node.world_id = j.value("world_id", "");
			node.canonical_name = j.value("canonical_name", "");
			node.display_name = j.value("display_name", node.canonical_name);
			node.phase = ParseWorldPhase(j.value("phase", "Phase3_Frontier"));
			node.phase_evidence = ParseEvidenceClass(j.value("phase_evidence", "R"));
			node.biome = ParseWorldBiome(j.value("biome", "Temperate"));
			node.location_type = j.value("location_type", "colony_world");
			node.system = j.value("system", "Unknown");
			node.era = j.value("era", "pre_invasion");
			node.connects_to = j.value("connects_to", "");
			node.connection_evidence = ParseEvidenceClass(j.value("connection_evidence", "A"));
			node.connection_mode = ParseConnectionMode(j.value("connection_mode", "RAIL"));
			node.source_rule = j.value("source_rule", "");
			if (j.contains("source_ids") && j["source_ids"].is_array()) {
				for (const auto &sid : j["source_ids"]) {
					node.source_ids.push_back(sid.get<std::string>());
				}
			}
			if (j.contains("economic_profile") && j["economic_profile"].is_object()) {
				const auto &ep = j["economic_profile"];
				node.economic_profile.role = ep.value("role", "");
				node.economic_profile.base_population = ep.value("base_population", 0ULL);
				node.economic_profile.megacity_eligible = ep.value("megacity_eligible", false);
				node.economic_profile.tariff_multiplier = ep.value("tariff_multiplier", 1.0f);
				if (ep.contains("primary_imports") && ep["primary_imports"].is_array()) {
					for (const auto &imp : ep["primary_imports"]) {
						node.economic_profile.primary_imports.push_back(imp.get<std::string>());
					}
				}
				if (ep.contains("primary_exports") && ep["primary_exports"].is_array()) {
					for (const auto &exp : ep["primary_exports"]) {
						node.economic_profile.primary_exports.push_back(exp.get<std::string>());
					}
				}
			}
			node.is_special_body = is_special;
			return node;
		};

		if (root.contains("nodes") && root["nodes"].is_array()) {
			for (const auto &jnode : root["nodes"]) {
				UniverseNode node = parse_node(jnode, false);
				size_t idx = this->_nodes.size();
				this->_nodes.push_back(node);
				this->_id_to_primary_index[node.world_id] = idx;
				this->_name_to_id[ToLower(node.canonical_name)] = node.world_id;
				this->_name_to_id[ToLower(node.display_name)] = node.world_id;
			}
		}

		if (root.contains("special_bodies") && root["special_bodies"].is_array()) {
			for (const auto &jnode : root["special_bodies"]) {
				UniverseNode node = parse_node(jnode, true);
				size_t idx = this->_special_bodies.size();
				this->_special_bodies.push_back(node);
				this->_id_to_special_index[node.world_id] = idx;
				this->_name_to_id[ToLower(node.canonical_name)] = node.world_id;
				this->_name_to_id[ToLower(node.display_name)] = node.world_id;
			}
		}

		if (root.contains("connections") && root["connections"].is_array()) {
			for (const auto &jconn : root["connections"]) {
				UniverseEdge edge;
				edge.connection_id = jconn.value("connection_id", "");
				edge.endpoint_a = jconn.value("endpoint_a", "");
				edge.endpoint_b = jconn.value("endpoint_b", "");
				edge.connection_type = ParseConnectionMode(jconn.value("connection_type", "RAIL"));
				edge.evidence_class = ParseEvidenceClass(jconn.value("evidence_class", "A"));
				edge.inference_or_assumption_id = jconn.value("inference_or_assumption_id", "NONE");
				edge.availability = jconn.value("availability", "PERMANENT");
				edge.access_policy = jconn.value("access_policy", "PUBLIC");
				edge.permits_through_running_train = jconn.value("permits_through_running_train", true);
				edge.virtual_length_tiles = jconn.value("virtual_length_tiles", 32U);
				edge.enabled_in_game = jconn.value("enabled_in_game", true);

				if (jconn.contains("source_ids") && jconn["source_ids"].is_array()) {
					for (const auto &sid : jconn["source_ids"]) {
						edge.source_ids.push_back(sid.get<std::string>());
					}
				}
				this->_edges.push_back(edge);
			}
		}

		this->_loaded = true;
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

size_t UniverseGraphManager::GetNodeCount() const
{
	return this->_nodes.size();
}

size_t UniverseGraphManager::GetSpecialBodyCount() const
{
	return this->_special_bodies.size();
}

size_t UniverseGraphManager::GetTotalLocationCount() const
{
	return this->_nodes.size() + this->_special_bodies.size();
}

size_t UniverseGraphManager::GetConnectionCount() const
{
	return this->_edges.size();
}

const UniverseNode *UniverseGraphManager::FindNode(const std::string &id_or_name) const
{
	auto it = this->_id_to_primary_index.find(id_or_name);
	if (it != this->_id_to_primary_index.end()) {
		return &this->_nodes[it->second];
	}

	auto it_spec = this->_id_to_special_index.find(id_or_name);
	if (it_spec != this->_id_to_special_index.end()) {
		return &this->_special_bodies[it_spec->second];
	}

	std::string lower = ToLower(id_or_name);
	auto it_name = this->_name_to_id.find(lower);
	if (it_name != this->_name_to_id.end()) {
		return this->FindNode(it_name->second);
	}

	return nullptr;
}

std::vector<const UniverseNode *> UniverseGraphManager::GetAllNodes() const
{
	std::vector<const UniverseNode *> res;
	res.reserve(this->GetTotalLocationCount());
	for (const auto &node : this->_nodes) res.push_back(&node);
	for (const auto &node : this->_special_bodies) res.push_back(&node);
	return res;
}

std::vector<const UniverseNode *> UniverseGraphManager::GetPrimaryNodes() const
{
	std::vector<const UniverseNode *> res;
	res.reserve(this->_nodes.size());
	for (const auto &node : this->_nodes) res.push_back(&node);
	return res;
}

std::vector<const UniverseNode *> UniverseGraphManager::GetSpecialBodies() const
{
	std::vector<const UniverseNode *> res;
	res.reserve(this->_special_bodies.size());
	for (const auto &node : this->_special_bodies) res.push_back(&node);
	return res;
}

/**
 * Check if an edge is admitted under the given evidentiary filter mode.
 * @param edge Universe connection edge to inspect.
 * @param mode Evidentiary filtering mode.
 * @return True if edge meets the filter criteria.
 */
static bool IsEdgeAdmitted(const UniverseEdge &edge, GraphFilterMode mode)
{
	switch (mode) {
		case GraphFilterMode::SOURCE_ONLY:
			return edge.evidence_class == EvidenceClass::Explicit ||
			       edge.evidence_class == EvidenceClass::Reference;
		case GraphFilterMode::RECONSTRUCTED:
			return edge.evidence_class == EvidenceClass::Explicit ||
			       edge.evidence_class == EvidenceClass::Reference ||
			       edge.evidence_class == EvidenceClass::Inferred;
		case GraphFilterMode::PLAYABLE_COMPLETION:
		default:
			return edge.evidence_class == EvidenceClass::Explicit ||
			       edge.evidence_class == EvidenceClass::Reference ||
			       edge.evidence_class == EvidenceClass::Inferred ||
			       edge.evidence_class == EvidenceClass::Assumed;
	}
}

std::vector<const UniverseEdge *> UniverseGraphManager::GetEdges(GraphFilterMode mode) const
{
	std::vector<const UniverseEdge *> res;
	for (const auto &edge : this->_edges) {
		if (IsEdgeAdmitted(edge, mode)) {
			res.push_back(&edge);
		}
	}
	return res;
}

const UniverseEdge *UniverseGraphManager::FindEdge(const std::string &node_a, const std::string &node_b, GraphFilterMode mode) const
{
	for (const auto &edge : this->_edges) {
		if (IsEdgeAdmitted(edge, mode)) {
			if ((edge.endpoint_a == node_a && edge.endpoint_b == node_b) ||
			    (edge.endpoint_a == node_b && edge.endpoint_b == node_a)) {
				return &edge;
			}
		}
	}
	return nullptr;
}

std::vector<const UniverseNode *> UniverseGraphManager::GetNeighbors(const std::string &node_id, GraphFilterMode mode) const
{
	std::set<std::string> neighbor_ids;
	for (const auto &edge : this->_edges) {
		if (IsEdgeAdmitted(edge, mode)) {
			if (edge.endpoint_a == node_id) neighbor_ids.insert(edge.endpoint_b);
			else if (edge.endpoint_b == node_id) neighbor_ids.insert(edge.endpoint_a);
		}
	}

	std::vector<const UniverseNode *> res;
	for (const auto &nid : neighbor_ids) {
		const UniverseNode *node = this->FindNode(nid);
		if (node != nullptr) res.push_back(node);
	}
	return res;
}

std::vector<const UniverseNode *> UniverseGraphManager::GetChildren(const std::string &parent_id, GraphFilterMode mode) const
{
	std::vector<const UniverseNode *> res;
	for (const auto &node : this->_nodes) {
		if (node.connects_to == parent_id) {
			const UniverseEdge *edge = this->FindEdge(node.world_id, parent_id, mode);
			if (edge != nullptr) {
				res.push_back(&node);
			}
		}
	}
	for (const auto &node : this->_special_bodies) {
		if (node.connects_to == parent_id) {
			const UniverseEdge *edge = this->FindEdge(node.world_id, parent_id, mode);
			if (edge != nullptr) {
				res.push_back(&node);
			}
		}
	}
	return res;
}

std::vector<const UniverseNode *> UniverseGraphManager::TraverseTreeFromRoot(const std::string &root_id, GraphFilterMode mode) const
{
	std::vector<const UniverseNode *> visited_order;
	const UniverseNode *root = this->FindNode(root_id);
	if (root == nullptr) return visited_order;

	std::set<std::string> visited;
	std::queue<std::string> q;

	visited.insert(root->world_id);
	q.push(root->world_id);
	visited_order.push_back(root);

	while (!q.empty()) {
		std::string curr = q.front();
		q.pop();

		for (const UniverseNode *nbr : this->GetNeighbors(curr, mode)) {
			if (!visited.contains(nbr->world_id)) {
				visited.insert(nbr->world_id);
				visited_order.push_back(nbr);
				q.push(nbr->world_id);
			}
		}
	}

	return visited_order;
}

bool UniverseGraphManager::HasPath(const std::string &from_id, const std::string &to_id, GraphFilterMode mode) const
{
	if (from_id == to_id) return true;
	if (this->FindNode(from_id) == nullptr || this->FindNode(to_id) == nullptr) return false;

	std::set<std::string> visited;
	std::queue<std::string> q;

	visited.insert(from_id);
	q.push(from_id);

	while (!q.empty()) {
		std::string curr = q.front();
		q.pop();

		if (curr == to_id) return true;

		for (const UniverseNode *nbr : this->GetNeighbors(curr, mode)) {
			if (!visited.contains(nbr->world_id)) {
				if (nbr->world_id == to_id) return true;
				visited.insert(nbr->world_id);
				q.push(nbr->world_id);
			}
		}
	}

	return false;
}

std::vector<std::string> UniverseGraphManager::GetPath(const std::string &from_id, const std::string &to_id, GraphFilterMode mode) const
{
	std::vector<std::string> path;
	if (from_id == to_id) {
		path.push_back(from_id);
		return path;
	}
	if (this->FindNode(from_id) == nullptr || this->FindNode(to_id) == nullptr) return path;

	std::set<std::string> visited;
	std::map<std::string, std::string> parent;
	std::queue<std::string> q;

	visited.insert(from_id);
	q.push(from_id);

	bool found = false;
	while (!q.empty()) {
		std::string curr = q.front();
		q.pop();

		if (curr == to_id) {
			found = true;
			break;
		}

		for (const UniverseNode *nbr : this->GetNeighbors(curr, mode)) {
			if (!visited.contains(nbr->world_id)) {
				visited.insert(nbr->world_id);
				parent[nbr->world_id] = curr;
				q.push(nbr->world_id);
				if (nbr->world_id == to_id) {
					found = true;
					break;
				}
			}
		}
		if (found) break;
	}

	if (!found) return path;

	std::string curr = to_id;
	while (curr != from_id) {
		path.push_back(curr);
		curr = parent[curr];
	}
	path.push_back(from_id);
	std::reverse(path.begin(), path.end());
	return path;
}

bool UniverseGraphManager::ValidateConnectivity(GraphFilterMode mode, std::string *error_msg) const
{
	const UniverseNode *earth = this->FindNode("world_earth");
	if (earth == nullptr) {
		if (error_msg != nullptr) *error_msg = "Root node world_earth not found";
		return false;
	}

	auto reachable = this->TraverseTreeFromRoot("world_earth", mode);

	static const std::vector<std::string> big15 = {
		"world_augusta", "world_bayovar", "world_buta", "world_democratic_republic_of_new_germany",
		"world_edenburg", "world_granada", "world_kerensk", "world_los_vada", "world_mito",
		"world_orleans", "world_piura", "world_shayoni", "world_stlincoln", "world_verona", "world_wessex"
	};

	for (const auto &b : big15) {
		bool is_reachable = std::any_of(reachable.begin(), reachable.end(), [&](const UniverseNode *n) {
			return n->world_id == b;
		});
		if (!is_reachable) {
			if (error_msg != nullptr) *error_msg = "Big15 hub not reachable: " + b;
			return false;
		}
	}

	if (mode == GraphFilterMode::PLAYABLE_COMPLETION && reachable.size() < 100) {
		if (error_msg != nullptr) *error_msg = "Expected >= 100 reachable nodes under PLAYABLE_COMPLETION";
		return false;
	}

	return true;
}

bool UniverseGraphManager::DetectCycles(GraphFilterMode mode) const
{
	std::set<std::string> visited;
	std::map<std::string, std::string> parent_map;

	auto dfs = [&](auto &self, const std::string &u, const std::string &p) -> bool {
		visited.insert(u);
		for (const UniverseNode *nbr : this->GetNeighbors(u, mode)) {
			std::string v = nbr->world_id;
			if (!visited.contains(v)) {
				if (self(self, v, u)) return true;
			} else if (v != p) {
				return true;
			}
		}
		return false;
	};

	for (const auto &node : this->_nodes) {
		if (!visited.contains(node.world_id)) {
			if (dfs(dfs, node.world_id, "")) return true;
		}
	}
	return false;
}
