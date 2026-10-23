/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file script_planet.cpp Read-only world and stockpile facts; no command or state change. */
#include "../../stdafx.h"
#include "script_planet.hpp"
#include "script_cargo.hpp"
#include "../../portal/planet_manager.h"
#include "../../portal/company_stockpile.h"
#include "../../map_func.h"
#include "../../safeguards.h"

/** @return The registered region for a script world ID, or null. */
static const PlanetRegion *ScriptRegion(SQInteger world)
{
	if (world < 0 || world >= SQInteger(UINT32_MAX)) return nullptr;
	return PlanetManager::GetRegion(WorldID{uint32_t(world)});
}

/* static */ SQInteger ScriptPlanet::GetWorldCount()
{
	return PlanetManager::Count();
}

/* static */ SQInteger ScriptPlanet::GetWorldID(SQInteger index)
{
	const auto &regions = PlanetManager::GetAllRegions();
	if (index < 0 || index >= SQInteger(regions.size())) return -1;
	return regions[index].id.base();
}

/* static */ SQInteger ScriptPlanet::GetTileWorld(TileIndex tile)
{
	if (!::IsValidTile(tile)) return -1;
	WorldID world = PlanetManager::GetTileWorld(tile);
	return world == INVALID_WORLD ? -1 : SQInteger(world.base());
}

/* static */ std::optional<std::string> ScriptPlanet::GetName(SQInteger world)
{
	const PlanetRegion *region = ScriptRegion(world);
	if (region == nullptr) return std::nullopt;
	return region->name;
}

/* static */ SQInteger ScriptPlanet::GetBiome(SQInteger world)
{
	const PlanetRegion *region = ScriptRegion(world);
	return region == nullptr ? -1 : SQInteger(region->biome);
}

/* static */ SQInteger ScriptPlanet::GetPhase(SQInteger world)
{
	const PlanetRegion *region = ScriptRegion(world);
	return region == nullptr ? -1 : SQInteger(region->phase);
}

/* static */ SQInteger ScriptPlanet::GetStock(SQInteger world, ScriptCompany::CompanyID company, CargoType cargo)
{
	const PlanetRegion *region = ScriptRegion(world);
	::CompanyID cid = ScriptCompany::FromScriptCompanyID(ScriptCompany::ResolveCompanyID(company));
	if (region == nullptr || cid >= MAX_COMPANIES || !ScriptCargo::IsValidCargo(cargo)) return -1;
	return StockpileManager::GetStock(region->id, cid, cargo);
}
