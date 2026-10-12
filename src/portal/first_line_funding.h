/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_line_funding.h Native, bounded First Permanent Line asset authoring. */
#ifndef FIRST_LINE_FUNDING_H
#define FIRST_LINE_FUNDING_H

#include "../industry_type.h"
#include "../tile_type.h"
#include "planet_type.h"
#include "../3rdparty/nlohmann/json.hpp"

struct Industry;

/**
 * Found one scenario industry through ordinary survey and industry commands.
 * The diagnostic report is populated on both success and failure; this helper never prints.
 */
Industry *FirstLineFundIndustry(IndustryType type, WorldID world, TileIndex near, bool primary, nlohmann::json &report);

#endif
