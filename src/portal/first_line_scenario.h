/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_line_scenario.h Isolated authoring and bounded native mission proof. */
#ifndef FIRST_LINE_SCENARIO_H
#define FIRST_LINE_SCENARIO_H
#include <span>
#include <string_view>
/**
 * Offline console adapter for the declared authored baseline and native play commands.
 * @param argv Console tokens including the operation.
 * @return Whether the operation syntax was handled.
 */
bool ConFirstLineScenario(std::span<std::string_view> argv);
#endif
