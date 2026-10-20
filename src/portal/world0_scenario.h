/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file world0_scenario.h Isolated Aster Vale authoring and native diagnostics. */
#ifndef WORLD0_SCENARIO_H
#define WORLD0_SCENARIO_H
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>
#include "../tile_type.h"
#include "../track_type.h"
/** @brief Get a fixed ordinary rail corridor including its station throats.
 * @param route Zero for the silica feeder or one for the ballast export leg.
 * @return Ordered tile and track pieces without modifying the map.
 */
std::vector<std::pair<TileIndex, Track>> GetWorld0RailRoute(uint route);
/** @brief Dispatch the offline Vale adapter under its native admission guards.
 * @param argv Console tokens selecting bridge, prepare, build, audit, status, or advance.
 * @return True when the console command was handled; failures have Vale receipts.
 */
bool ConWorld0Scenario(std::span<std::string_view> argv);
/** @brief Finish an already-bound opening with briefing, optional camera, and receipt.
 * @param route_quote Native quoted cost of the two unbuilt corridors.
 * @param service_quote Native quote including the second freight service.
 * @return True when completion and its native briefing succeed.
 */
bool FinishWorld0Authoring(int64_t route_quote, int64_t service_quote);
#endif
