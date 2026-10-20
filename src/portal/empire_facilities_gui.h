/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file empire_facilities_gui.h Empire Industrial Facilities and Supply Chain dashboard window header. */

#ifndef EMPIRE_FACILITIES_GUI_H
#define EMPIRE_FACILITIES_GUI_H

#include "../company_type.h"
#include "../cargo_type.h"
#include "../economy_type.h"
#include "../gfx_type.h"
#include "../industry_type.h"
#include "../core/geometry_type.hpp"
#include <array>
#include <optional>
#include <string>
#include <cstdint>

struct ProcessingFacility;
struct ProductionRecipe;

/** First limiting constraint evaluation for an industrial processing facility. */
struct FacilityBottleneck {
	std::string badge;
	TextColour colour = TextColour::Green;
	uint32_t batches_permitted = 0;
};

/**
 * Geometry of one facility row, shared by drawing and click handling.
 * Sizes follow the interface scale and font height, so text and buttons stay inside the row.
 */
struct FacilityRowLayout {
	static constexpr int LINES = 7; ///< Text lines per row.

	Rect row; ///< Row background.
	Rect text; ///< Text area left of the action column.
	std::array<Rect, LINES> lines; ///< The text lines.
	Rect works_first; ///< Capacity works button.
	Rect works_second; ///< Reserve works button.
	Rect works_span; ///< Works status badge or Upgrade (+50), covering both works slots.
	Rect retire; ///< Retire button.
	Rect plant; ///< Locate Plant button.
	Rect station; ///< Locate Station / Hub button.
	Rect platform; ///< Legacy platform holding target button, at the end of the last line.

	FacilityRowLayout(int left, int right, int top, int line_height);
	static int LineStep(int line_height);
	static int Height(int line_height);
};

FacilityBottleneck DiagnoseFacilityBottleneck(const ProcessingFacility &f, const ProductionRecipe *rec, uint32_t throughput, uint32_t in_cap, uint32_t out_cap);
uint32_t GetFacilityBufferedUnits(const ProcessingFacility &f, CargoType cargo, bool input);
std::optional<Money> QueryWorksCommissionPrice(IndustryID industry, CompanyID company);

void ShowEmpireFacilitiesWindow(CompanyID company = CompanyID::Invalid());

#endif /* EMPIRE_FACILITIES_GUI_H */
