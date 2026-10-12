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
/** Fixed ordinary rail corridor, including throats; no path search or mutation. */
std::vector<std::pair<TileIndex, Track>> GetWorld0RailRoute(uint route);
/** Offline adapter. Authoring is admitted only on a fresh empty 256-tile editor map. */
bool ConWorld0Scenario(std::span<std::string_view> argv);
/** Complete an already-bound Vale opening: native briefing, optional camera, and prepared receipt. */
bool FinishWorld0Authoring(int64_t route_quote, int64_t service_quote);
#endif
