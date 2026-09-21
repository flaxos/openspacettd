/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file universe_graph.h Commonwealth 108-world universe graph topology manager. */

#ifndef UNIVERSE_GRAPH_H
#define UNIVERSE_GRAPH_H

#include "planet_type.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

/** Filtering modes for universe graph connectivity based on evidentiary rigor. */
enum class GraphFilterMode : uint8_t {
	SOURCE_ONLY         = 0, ///< Only E (Explicit) and R (Reference-supported) connections.
	RECONSTRUCTED       = 1, ///< E, R, and I (Inferred) connections.
	PLAYABLE_COMPLETION = 2, ///< E, R, I, and A (Assumed for game completion) connections.
};

/** Inter-world connection transport mode. */
enum class ConnectionMode : uint8_t {
	Rail,        ///< Standard through-running railway wormhole.
	Gate,        ///< Wormhole break-of-gauge / transfer terminal.
	Scheduled,   ///< Wormhole with cyclic scheduled operating intervals (e.g. Far Away).
	Private,     ///< Private or restricted charter wormhole (e.g. Cressat, Solidade).
	Exploration, ///< Exploratory or survey gate; requires expeditionary consist.
	Data,        ///< Communications / data beam only; strictly blocks rolling stock.
	SilfenRoute, ///< Alien Silfen mystical forest path network; requires intermodal container pods.
	Orbital,     ///< Orbital wormhole leading to orbital lighter stations.
	Shuttle,     ///< Local spacecraft transfer.
	Aircraft,    ///< Local atmospheric flight.
	None,        ///< No active transport edge.
};

/** Evidentiary classification of world existence or connection link. */
enum class EvidenceClass : uint8_t {
	Explicit,  ///< E: Directly described in novel passages.
	Reference, ///< R: Supported by secondary reference or Earth-to-Phase-One structure.
	Inferred,  ///< I: Specific geographical, journey, or sector inference.
	Assumed,   ///< A: Game topology allocation; never novel canon.
	Unknown,   ///< U: Unassigned or unknown.
};

/**
 * Convert evidence class to string description.
 * @param ev Evidence class enum.
 * @return String description.
 */
std::string EvidenceClassToString(EvidenceClass ev);

/**
 * Convert evidence class to single-character badge (E, R, I, A, U).
 * @param ev Evidence class enum.
 * @return Single-character badge string.
 */
std::string EvidenceClassToBadge(EvidenceClass ev);

/**
 * Convert connection mode to uppercase string.
 * @param mode Transport connection mode.
 * @return String representation of connection mode.
 */
std::string ConnectionModeToString(ConnectionMode mode);

/**
 * Convert filter mode to string.
 * @param mode Graph filter mode.
 * @return String representation of filter mode.
 */
std::string GraphFilterModeToString(GraphFilterMode mode);

/**
 * Parse evidence string ("E", "R", "I", "A", "U").
 * @param str Input string to parse.
 * @return Parsed EvidenceClass enum.
 */
EvidenceClass ParseEvidenceClass(const std::string &str);

/**
 * Parse connection mode string ("RAIL", "GATE", "SCHEDULED", etc.).
 * @param str Input string to parse.
 * @return Parsed ConnectionMode enum.
 */
ConnectionMode ParseConnectionMode(const std::string &str);

/** Economic profile for a world node in the Commonwealth universe. */
struct UniverseEconomicProfile {
	std::string role{};
	std::vector<std::string> primary_imports{};
	std::vector<std::string> primary_exports{};
	uint64_t base_population = 0;
	bool megacity_eligible = false;
	float tariff_multiplier = 1.0f;
};

/** A world or location node in the Commonwealth universe graph. */
struct UniverseNode {
	std::string world_id{};
	std::string canonical_name{};
	std::string display_name{};
	WorldPhase phase = WorldPhase::Phase3_Frontier;
	EvidenceClass phase_evidence = EvidenceClass::Reference;
	WorldBiome biome = WorldBiome::Temperate;
	std::string location_type{};
	std::string system{};
	std::string era = "pre_invasion";
	std::string connects_to{};
	EvidenceClass connection_evidence = EvidenceClass::Assumed;
	ConnectionMode connection_mode = ConnectionMode::Rail;
	std::string source_rule{};
	std::vector<std::string> source_ids{};
	UniverseEconomicProfile economic_profile{};
	bool is_special_body = false;
};

/** A transport connection edge between two nodes in the Commonwealth universe graph. */
struct UniverseEdge {
	std::string connection_id{};
	std::string endpoint_a{};
	std::string endpoint_b{};
	ConnectionMode connection_type = ConnectionMode::Rail;
	EvidenceClass evidence_class = EvidenceClass::Assumed;
	std::vector<std::string> source_ids{};
	std::string inference_or_assumption_id = "NONE";
	std::string availability = "PERMANENT";
	std::string access_policy = "PUBLIC";
	bool permits_through_running_train = true;
	uint32_t virtual_length_tiles = 32;
	bool enabled_in_game = true;
};

/**
 * Singleton manager responsible for loading, traversing, querying, and
 * validating the 108-world Commonwealth Universe Graph.
 */
class UniverseGraphManager {
public:
	/**
	 * Get singleton instance of UniverseGraphManager.
	 * @return Reference to the singleton instance.
	 */
	static UniverseGraphManager &Instance();

	/**
	 * Load graph from disk. If empty, checks standard paths (assets/data, bin/data).
	 * @param filepath Optional path to the universe graph JSON file.
	 * @return True if loaded successfully, false otherwise.
	 */
	bool LoadGraph(const std::string &filepath = "");

	/**
	 * Load graph directly from a JSON string.
	 * @param json_str JSON string containing universe nodes and edges.
	 * @return True if parsed and loaded successfully, false otherwise.
	 */
	bool LoadGraphFromJsonString(const std::string &json_str);

	/**
	 * Ensure the graph is loaded, using default search paths if not yet loaded.
	 * @return True if already loaded or successfully loaded.
	 */
	bool EnsureLoaded();

	/**
	 * Get count of primary 108 reference nodes.
	 * @return Number of primary reference world nodes.
	 */
	size_t GetNodeCount() const;

	/**
	 * Get count of 16 special bodies and non-planet nodes.
	 * @return Number of special bodies.
	 */
	size_t GetSpecialBodyCount() const;

	/**
	 * Get total count of all locations (108 nodes + 16 special bodies = 124).
	 * @return Total number of known location nodes.
	 */
	size_t GetTotalLocationCount() const;

	/**
	 * Get count of connection edges.
	 * @return Total number of registered edges.
	 */
	size_t GetConnectionCount() const;

	/**
	 * Find a node by its world_id or canonical/display name. Returns nullptr if not found.
	 * @param id_or_name World ID or name string.
	 * @return Pointer to UniverseNode, or nullptr if not found.
	 */
	const UniverseNode *FindNode(const std::string &id_or_name) const;

	/**
	 * Get all locations (primary nodes + special bodies).
	 * @return Vector of pointers to all known universe nodes.
	 */
	std::vector<const UniverseNode *> GetAllNodes() const;

	/**
	 * Get only the primary 108 reference nodes.
	 * @return Vector of pointers to primary world nodes.
	 */
	std::vector<const UniverseNode *> GetPrimaryNodes() const;

	/**
	 * Get only the 16 special bodies and locations.
	 * @return Vector of pointers to special bodies.
	 */
	std::vector<const UniverseNode *> GetSpecialBodies() const;

	/**
	 * Get all edges admitted by the given evidentiary filter mode.
	 * @param mode Evidentiary graph filtering mode.
	 * @return Vector of pointers to admitted edges.
	 */
	std::vector<const UniverseEdge *> GetEdges(GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Find an edge connecting node_a and node_b under the specified filter mode.
	 * @param node_a Source node identifier.
	 * @param node_b Destination node identifier.
	 * @param mode Evidentiary graph filtering mode.
	 * @return Pointer to matching edge, or nullptr if no connection exists.
	 */
	const UniverseEdge *FindEdge(const std::string &node_a, const std::string &node_b, GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Get direct neighbor nodes for a given node under the filter mode.
	 * @param node_id World node identifier.
	 * @param mode Evidentiary graph filtering mode.
	 * @return Vector of pointers to neighbor nodes.
	 */
	std::vector<const UniverseNode *> GetNeighbors(const std::string &node_id, GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Get child nodes where parent_id is their connects_to hub.
	 * @param parent_id Parent world node identifier.
	 * @param mode Evidentiary graph filtering mode.
	 * @return Vector of pointers to child nodes.
	 */
	std::vector<const UniverseNode *> GetChildren(const std::string &parent_id, GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Perform breadth-first search tree traversal rooted at Sol/Earth.
	 * @param root_id Root node identifier to begin traversal.
	 * @param mode Evidentiary graph filtering mode.
	 * @return Traversed node list in breadth-first order.
	 */
	std::vector<const UniverseNode *> TraverseTreeFromRoot(const std::string &root_id = "world_earth", GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Check if a valid path exists between from_id and to_id under the filter mode.
	 * @param from_id Origin world identifier.
	 * @param to_id Destination world identifier.
	 * @param mode Evidentiary graph filtering mode.
	 * @return True if a reachable path exists.
	 */
	bool HasPath(const std::string &from_id, const std::string &to_id, GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Get shortest hop path of node IDs from from_id to to_id. Empty if no path.
	 * @param from_id Origin world identifier.
	 * @param to_id Destination world identifier.
	 * @param mode Evidentiary graph filtering mode.
	 * @return Ordered list of world IDs along shortest path.
	 */
	std::vector<std::string> GetPath(const std::string &from_id, const std::string &to_id, GraphFilterMode mode = GraphFilterMode::PLAYABLE_COMPLETION) const;

	/**
	 * Validate graph connectivity and return true if expected core connectivity holds.
	 * @param mode Evidentiary graph filtering mode.
	 * @param error_msg Optional pointer to string receiving validation error details.
	 * @return True if connectivity invariants hold.
	 */
	bool ValidateConnectivity(GraphFilterMode mode, std::string *error_msg = nullptr) const;

	/**
	 * Check if graph has any cycles under the filter mode.
	 * @param mode Evidentiary graph filtering mode.
	 * @return True if one or more topological cycles exist.
	 */
	bool DetectCycles(GraphFilterMode mode) const;

	/**
	 * Check whether the connection mode allows through-running trains.
	 * @param mode Connection transport mode.
	 * @return True if rolling stock trains can transit this mode.
	 */
	static bool IsModePermittingTrains(ConnectionMode mode) noexcept;

	/**
	 * Set active filter mode.
	 * @param mode Graph filter mode to activate.
	 */
	void SetFilterMode(GraphFilterMode mode) { this->_current_mode = mode; }

	/**
	 * Get active filter mode.
	 * @return Active graph filter mode.
	 */
	GraphFilterMode GetFilterMode() const { return this->_current_mode; }

	/** Clear all loaded graph data. */
	void Reset();

private:
	UniverseGraphManager() = default;

	GraphFilterMode _current_mode = GraphFilterMode::PLAYABLE_COMPLETION;
	bool _loaded = false;

	std::vector<UniverseNode> _nodes{};
	std::vector<UniverseNode> _special_bodies{};
	std::vector<UniverseEdge> _edges{};

	std::map<std::string, size_t> _id_to_primary_index{};
	std::map<std::string, size_t> _id_to_special_index{};
	std::map<std::string, std::string> _name_to_id{};
};

#endif /* UNIVERSE_GRAPH_H */
