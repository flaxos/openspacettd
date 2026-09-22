/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file prompt_scenario_generator.h Prompt-to-Savegame Generator for narrative scenario synthesis. */

#ifndef PROMPT_SCENARIO_GENERATOR_H
#define PROMPT_SCENARIO_GENERATOR_H

#include "planet_type.h"
#include "production_chain.h"
#include "corporate_alliance.h"
#include "../cargo_type.h"
#include "../tile_type.h"

#include <string>
#include <vector>
#include <map>
#include <optional>

/** Specifications for a single planetary world in a synthesized scenario. */
struct PromptWorldSpec {
	WorldID id = INVALID_WORLD;
	std::string name;
	WorldPhase phase = WorldPhase::Phase3_Frontier;
	WorldBiome biome = WorldBiome::Temperate;
	std::string role_description;

	std::vector<CommonwealthCargoID> demanded_cargos;
	std::vector<CommonwealthCargoID> supplied_cargos;

	bool is_striking = false;
	std::string strike_cause;

	uint32_t population = 5000;
	bool has_megacity = false;
	bool has_corporate_hq = false;
	bool has_industrial_facility = false;
	RecipeID industrial_recipe = RECIPE_NONE;
};

/** High-level specification for a synthesized scenario. */
struct PromptScenarioSpec {
	std::string original_prompt;
	std::string title;
	std::string narrative_summary;

	uint32_t world_count = 3;
	std::vector<PromptWorldSpec> worlds;

	CorporateRelation rival_relation = CorporateRelation::Neutral;
	std::string rival_corporation = "CST";

	bool create_active_fleets = true;
	bool create_prebuilt_corridors = true;
};

/** Synthesis summary details returned after scenario generation. */
struct ScenarioSynthesisResult {
	bool success = false;
	std::string output_file;
	std::string error_message;
	uint32_t worlds_created = 0;
	uint32_t corridors_built = 0;
	uint32_t trains_spawned = 0;
	uint32_t stations_placed = 0;
	uint32_t facilities_placed = 0;
	uint32_t industries_placed = 0;
	uint32_t depots_placed = 0;
};

/**
 * Procedural scenario generator converting natural language narrative descriptions
 * into fully initialized, playable OpenSpaceTTD .sav scenarios.
 */
class PromptScenarioGenerator {
public:
	/**
	 * Parse a natural language prompt (or structured JSON string) into a scenario specification.
	 * @param prompt_text Natural language prompt or JSON document.
	 * @return Parsed PromptScenarioSpec.
	 */
	static PromptScenarioSpec ParsePrompt(const std::string &prompt_text);

	/**
	 * Synthesize a scenario on the currently allocated map from a specification and export to .sav file.
	 * @param spec Scenario specification.
	 * @param output_path Output savegame path (.sav).
	 * @return ScenarioSynthesisResult containing execution metrics.
	 */
	static ScenarioSynthesisResult SynthesizeAndSave(const PromptScenarioSpec &spec, const std::string &output_path);

	/**
	 * Convenience method: parse prompt and synthesize directly to file.
	 * @param prompt_text Natural language narrative prompt.
	 * @param output_path Destination savegame path.
	 * @return ScenarioSynthesisResult.
	 */
	static ScenarioSynthesisResult GenerateFromPrompt(const std::string &prompt_text, const std::string &output_path);

	/**
	 * Build pre-built railway corridors, portal throats, stations, depots, and waypoints between worlds.
	 * @param spec Scenario specification.
	 * @param result Result tracking structure to record corridor counts.
	 * @return True on success.
	 */
	static bool BuildCorridorsAndStations(const PromptScenarioSpec &spec, ScenarioSynthesisResult &result);

	/**
	 * Spawn canonical resource industries within station catchment to establish active supply chains.
	 * @param spec Scenario specification.
	 * @param result Result tracking structure to record industry counts.
	 * @return True on success.
	 */
	static bool PlaceCanonicalIndustries(const PromptScenarioSpec &spec, ScenarioSynthesisResult &result);

	/**
	 * Spawn and dispatch operational trains with assigned inter-world round-trip orders.
	 * @param spec Scenario specification.
	 * @param result Result tracking structure to record fleet counts.
	 * @return True on success.
	 */
	static bool SpawnActiveFleets(const PromptScenarioSpec &spec, ScenarioSynthesisResult &result);

	/**
	 * Synthesize a canonical multi-world Commonwealth Prefab World for automated UAT.
	 * @param output_path Destination savegame path (.sav).
	 * @param world_count Number of worlds to partition (default 4).
	 * @return ScenarioSynthesisResult containing generated stats.
	 */
	static ScenarioSynthesisResult GenerateCommonwealthPrefabWorld(const std::string &output_path, uint32_t world_count = 4);

	/**
	 * Verify that the currently loaded game world is a healthy, playable Commonwealth UAT scenario.
	 * @param error_msg Optional pointer to string receiving detailed diagnostics.
	 * @return True if all UAT invariants pass.
	 */
	static bool VerifyCommonwealthUAT(std::string *error_msg = nullptr);
};

#endif /* PROMPT_SCENARIO_GENERATOR_H */
