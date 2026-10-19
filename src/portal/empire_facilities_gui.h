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

FacilityBottleneck DiagnoseFacilityBottleneck(const ProcessingFacility &f, const ProductionRecipe *rec, uint32_t throughput, uint32_t in_cap, uint32_t out_cap);
uint32_t GetFacilityBufferedUnits(const ProcessingFacility &f, CargoType cargo, bool input);
std::optional<Money> QueryWorksCommissionPrice(IndustryID industry, CompanyID company);

void ShowEmpireFacilitiesWindow(CompanyID company = CompanyID::Invalid());

#endif /* EMPIRE_FACILITIES_GUI_H */
