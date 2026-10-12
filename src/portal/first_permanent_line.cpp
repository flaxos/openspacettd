/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file first_permanent_line.cpp Deterministic observer of native freight and town events. */
#include "../stdafx.h"
#include "first_permanent_line.h"
#include "integrated_economy.h"
#include "logistics_hub.h"
#include "megacity_manager.h"
#include "planet_manager.h"
#include "company_stockpile.h"
#include "../3rdparty/nlohmann/json.hpp"
#include "../cargotype.h"
#include "../company_base.h"
#include "../depot_base.h"
#include "../industry.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../station_base.h"
#include "../tile_map.h"
#include "../town.h"
#include "../town_map.h"
#include "../vehicle_base.h"
#include "../waypoint_base.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
/** @brief JSON representation used for bounded observer save records. */
using Json = nlohmann::json;
/** @brief Current private First Permanent Line observer state. */
FirstPermanentLineState mission;
/** @var CORE
 * @brief World 0 containing Mito and its residential objective.
 */
/** @var INDUSTRIAL
 * @brief World 1 containing the bound steel and ballast factories.
 */
/** @var FRONTIER
 * @brief World 2 containing the bound iron and silica sources.
 */
constexpr WorldID CORE{0}, INDUSTRIAL{1}, FRONTIER{2};
/** @brief Maximum credited freight units per persisted objective counter. */
constexpr uint32_t MAX_EVIDENCE_UNITS = 1000000;
/** @brief Maximum vehicle journeys retained by the mission observer. */
constexpr size_t MAX_JOURNEYS = 4096;

/** @brief Resolve a four-character cargo label in the active cargo catalog.
 * @param label Four-character cargo label.
 * @return The native cargo type, or INVALID_CARGO when absent.
 */
CargoType Cargo(const char *label)
{
	CargoLabel value;
	std::copy_n(label, 4, value.begin());
	return GetCargoTypeByLabel(value);
}

/** @brief Check a protected Core house anchor against its native tile and completion state.
 * @param town Required Mito town.
 * @param tile House anchor to inspect.
 * @param complete Whether native house completion is required.
 * @return True when the bound protected house meets the requested state.
 */
bool HouseValid(TownID town, TileIndex tile, bool complete)
{
	if (tile == INVALID_TILE || tile >= Map::Size() || !IsTileType(tile, TileType::House)) return false;
	if (GetTownIndex(tile) != town || PlanetManager::GetTileWorld(tile) != CORE || !IsHousePlayerProtected(tile)) return false;
	return !complete || IsHouseCompleted(tile);
}

/** @brief Verify a player-founded Merredin factory and its bound recipe.
 * @param id Factory industry identifier.
 * @param owner Required founding company.
 * @param recipe Required integrated-economy recipe.
 * @return True when the live factory has the expected owner, world, and recipe.
 */
bool FactoryValid(IndustryID id, CompanyID owner, RecipeID recipe)
{
	const Industry *industry = Industry::GetIfValid(id);
	const auto &factories = IntegratedEconomy::Factories();
	const auto it = factories.find(id);
	return industry != nullptr && it != factories.end() && it->second.owner == owner && industry->founder == owner &&
		IntegratedEconomy::IndustryRecipe(industry->type) == recipe && PlanetManager::GetTileWorld(industry->location.tile) == INDUSTRIAL;
}

/** @brief Verify a live Clonclurry source produces the specified cargo.
 * @param id Source industry identifier.
 * @param cargo Required output cargo.
 * @return True for a live Frontier source with that output.
 */
bool SourceValid(IndustryID id, CargoType cargo)
{
	const Industry *industry = Industry::GetIfValid(id);
	return industry != nullptr && PlanetManager::GetTileWorld(industry->location.tile) == FRONTIER && industry->IsCargoProduced(cargo);
}

/** @brief Verify a player-owned Mito station has a valid native logistics hub.
 * @param id Station identifier.
 * @param company Required owning company.
 * @return True when the station and hub remain bound to the company and Core world.
 */
bool HubValid(StationID id, CompanyID company)
{
	const Station *station = Station::GetIfValid(id);
	const LogisticsHub *hub = LogisticsHubManager::GetHubForStation(id);
	return station != nullptr && station->owner == company && station->xy < Map::Size() &&
		PlanetManager::GetTileWorld(station->xy) == CORE && hub != nullptr && hub->company_id == company &&
		hub->world_id == CORE && LogisticsHubManager::ValidateForStation(*hub);
}

/** @brief Validate all saved mission identities against the current native economy.
 * @param b Company, town, factory, and source bindings.
 * @return True when every objective binding and role remains valid.
 */
bool BindingValid(const FirstPermanentLineBindings &b)
{
	const Town *town = Town::GetIfValid(b.mito);
	const MegacityProfile *megacity = MegacityManager::GetProfile(b.mito);
	return Company::IsValidID(b.company) && IntegratedEconomy::Enabled() &&
		IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2 &&
		IntegratedEconomy::Role(CORE) == EconomicRole::Core &&
		IntegratedEconomy::Role(INDUSTRIAL) == EconomicRole::Industrial &&
		IntegratedEconomy::Role(FRONTIER) == EconomicRole::Frontier && PlanetManager::GetRegion(CORE) != nullptr &&
		PlanetManager::GetRegion(INDUSTRIAL) != nullptr && PlanetManager::GetRegion(FRONTIER) != nullptr &&
		town != nullptr && PlanetManager::GetTileWorld(town->xy) == CORE &&
		megacity != nullptr && megacity->town_id == b.mito && megacity->world_id == CORE &&
		b.steel_works != b.ballast_works && b.iron_source != b.silica_source &&
		FactoryValid(b.steel_works, b.company, RECIPE_STEEL_SMELTING) &&
		FactoryValid(b.ballast_works, b.company, RECIPE_BALLAST_CRUSHING) &&
		SourceValid(b.iron_source, Cargo("IRON")) && SourceValid(b.silica_source, Cargo("SILC"));
}

/** @brief Check whether a station catchment intersects an industry footprint.
 * @param station Station to inspect, or null.
 * @param industry Industry to inspect, or null.
 * @return True when at least one industry tile lies in the catchment.
 */
bool Catches(const Station *station, const Industry *industry)
{
	if (station == nullptr || industry == nullptr) return false;
	for (TileIndex tile : industry->location) if (station->catchment_tiles.HasTile(tile)) return true;
	return false;
}

/** @brief Require the current arrival to match an explicit station order.
 * @param train Candidate front train.
 * @param station Station reported by the native arrival hook.
 * @return True when the active scheduled stop is this station.
 */
bool ScheduledAt(const Vehicle *train, StationID station)
{
	if (train == nullptr || train->GetNumOrders() < 2 || train->cur_real_order_index >= train->GetNumOrders()) return false;
	const Order *order = train->GetOrder(train->cur_real_order_index);
	return order != nullptr && order->IsType(OT_GOTO_STATION) && order->GetDestination().ToStationID() == station &&
		!order->GetNonStopType().Test(OrderNonStopFlag::GoVia) &&
		train->current_order.IsType(OT_GOTO_STATION) && train->current_order.GetDestination().ToStationID() == station &&
		!train->current_order.GetNonStopType().Test(OrderNonStopFlag::GoVia);
}

/** @brief Require a delivery to occur during an ordered station loading visit.
 * @param train Candidate front train.
 * @param station Station receiving the delivery.
 * @return True when its explicit order or native loading marker matches the station.
 */
bool ScheduledDeliveryAt(const Vehicle *train, StationID station)
{
	if (train == nullptr || train->cur_real_order_index >= train->GetNumOrders()) return false;
	const Order *order = train->GetOrder(train->cur_real_order_index);
	if (order == nullptr || !order->IsType(OT_GOTO_STATION) || order->GetDestination().ToStationID() != station ||
		order->GetNonStopType().Test(OrderNonStopFlag::GoVia)) return false;
	/* Native BeginLoading marks an ordered stop with GoVia on its temporary loading order. */
	return (train->current_order.IsType(OT_LOADING) && train->current_order.GetNonStopType().Test(OrderNonStopFlag::GoVia)) ||
		(train->current_order.IsType(OT_GOTO_STATION) && train->current_order.GetDestination().ToStationID() == station &&
		!train->current_order.GetNonStopType().Test(OrderNonStopFlag::GoVia));
}

/** @brief Check recurring explicit works pickup and unloadable Mito hub orders.
 * @param train Candidate front train.
 * @param hub Required hub, or invalid to accept any valid Mito hub.
 * @param owner Company that must own the train and route stops.
 * @return True when the native order list still supports both required legs.
 */
bool RouteValid(const Vehicle *train, StationID hub, CompanyID owner)
{
	if (train == nullptr || train->type != VehicleType::Train || !train->IsFrontEngine() || train->owner != owner ||
		train->GetNumOrders() < 2) return false;
	bool source = false, dest = false;
	for (VehicleOrderID i = 0; i < train->GetNumOrders(); ++i) {
		const Order *order = train->GetOrder(i);
		if (order == nullptr) return false;
		/* Native tracking orders may appear between player-authored stops; they never prove a leg. */
		if (order->IsType(OT_IMPLICIT)) continue;
		if (order->IsType(OT_GOTO_DEPOT)) {
			/* A halt ends recurring service; a nearest-depot order has no fixed destination ID. */
			if (order->GetDepotActionType().Test(OrderDepotActionFlag::Halt)) return false;
			if (order->GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) continue;
			const Depot *depot = Depot::GetIfValid(order->GetDestination().ToDepotID());
			if (depot == nullptr || depot->xy >= Map::Size() || !IsRailDepotTile(depot->xy) ||
				GetDepotIndex(depot->xy) != depot->index || GetTileOwner(depot->xy) != owner) return false;
			continue;
		}
		if (order->IsType(OT_CONDITIONAL)) {
			/* Conditional navigation is auxiliary; its target must be a live order slot. */
			if (order->GetConditionSkipToOrder() >= train->GetNumOrders() ||
				order->GetConditionVariable() >= OrderConditionVariable::End ||
				order->GetConditionComparator() >= OrderConditionComparator::End) return false;
			continue;
		}
		if (order->IsType(OT_GOTO_WAYPOINT)) {
			const Waypoint *waypoint = Waypoint::GetIfValid(order->GetDestination().ToStationID());
			if (waypoint == nullptr || waypoint->owner != owner || waypoint->xy >= Map::Size() ||
				!IsRailWaypointTile(waypoint->xy) || GetStationIndex(waypoint->xy) != waypoint->index) return false;
			continue;
		}
		if (!order->IsType(OT_GOTO_STATION)) return false;
		const Station *station = Station::GetIfValid(order->GetDestination().ToStationID());
		if (station == nullptr || station->owner != owner) return false;
		/* Go-via station orders are valid navigation but cannot witness pickup or unloading. */
		if (order->GetNonStopType().Test(OrderNonStopFlag::GoVia)) continue;
		if ((hub == StationID::Invalid() || station->index == hub) && HubValid(station->index, owner) &&
			order->GetUnloadType() != OrderUnloadType::NoUnload && order->GetUnloadType() != OrderUnloadType::Transfer) dest = true;
		if ((Catches(station, Industry::GetIfValid(mission.binding.steel_works)) ||
			Catches(station, Industry::GetIfValid(mission.binding.ballast_works))) &&
			order->GetLoadType() != OrderLoadType::NoLoad) source = true;
	}
	return source && dest;
}

/** @brief Find an explicit stopping order for a station.
 * @param train Candidate train.
 * @param station Station sought in its order list.
 * @return True when the train has a non-via stop at the station.
 */
bool RouteContains(const Vehicle *train, StationID station)
{
	if (train == nullptr) return false;
	for (VehicleOrderID i = 0; i < train->GetNumOrders(); ++i) {
		const Order *order = train->GetOrder(i);
		if (order != nullptr && order->IsType(OT_GOTO_STATION) &&
			!order->GetNonStopType().Test(OrderNonStopFlag::GoVia) && order->GetDestination().ToStationID() == station) return true;
	}
	return false;
}

/** @brief Require a live running train and a valid recurring mission route.
 * @param train Candidate train.
 * @param hub Required hub, or invalid for any valid Mito hub.
 * @param owner Required owner.
 * @return True when the train is operable and its route is valid.
 */
bool ServiceValid(const Vehicle *train, StationID hub, CompanyID owner)
{
	return train != nullptr && !train->vehstatus.Test(VehState::Crashed) &&
		!train->vehstatus.Test(VehState::Stopped) && RouteValid(train, hub, owner);
}

/** @brief Check a live, uncrashed front train belongs to the charter company.
 * @param id Native vehicle identifier.
 * @param company Required owner.
 * @return True for an eligible native train.
 */
bool TrainValid(VehicleID id, CompanyID company)
{
	const Vehicle *train = Vehicle::GetIfValid(id);
	return train != nullptr && train->type == VehicleType::Train && train->IsFrontEngine() && train->owner == company &&
		!train->vehstatus.Test(VehState::Crashed);
}

/** @brief Check both qualified input deliveries and subsequent factory production.
 * @return True once the first freight milestone is complete.
 */
bool MilestoneOne() { return mission.input_delivered[0] && mission.input_delivered[1] && mission.produced_after_input[0] && mission.produced_after_input[1]; }
/** @brief Latch an invalid mission reason and clear live journey evidence.
 * @param reason Stable reason recorded in the mission state.
 */
void Invalidate(const char *reason) { mission.invalid = true; mission.reason = reason; mission.journeys.clear(); }

/** @brief Parse a strictly unsigned bounded save integer.
 * @param value JSON value to decode.
 * @param max Largest permitted value.
 * @return The decoded integer; throws on type or range mismatch.
 */
uint64_t Number(const Json &value, uint64_t max)
{
	if (!value.is_number_unsigned() || value.get<uint64_t>() > max) throw std::runtime_error("mission integer");
	return value.get<uint64_t>();
}
/** @brief Parse a strictly typed save boolean.
 * @param value JSON value to decode.
 * @return The decoded boolean; throws for another JSON type.
 */
bool JsonBoolean(const Json &value)
{
	if (!value.is_boolean()) throw std::runtime_error("mission boolean");
	return value.get<bool>();
}
/** @brief Reject save objects with missing or extra fields.
 * @param value JSON object to inspect.
 * @param keys Exact required field names.
 */
void ExactObject(const Json &value, std::initializer_list<const char *> keys)
{
	if (!value.is_object() || value.size() != keys.size()) throw std::runtime_error("mission object");
	for (auto key : keys) if (!value.contains(key)) throw std::runtime_error("mission field");
}
} // namespace

/** @brief Clear all First Permanent Line bindings and evidence.
 */
void FirstPermanentLine::Reset() { mission = {}; }
/** @brief Report whether a complete mission binding is installed.
 * @return True while the observer is active, including after an award or invalidation.
 */
bool FirstPermanentLine::Active() { return mission.active; }
const FirstPermanentLineState &FirstPermanentLine::State() { return mission; }

bool FirstPermanentLine::Begin(const FirstPermanentLineBindings &b)
{
	if (mission.active || !BindingValid(b) || Cargo("IRON") == INVALID_CARGO || Cargo("SILC") == INVALID_CARGO ||
		Cargo("STEL") == INVALID_CARGO || Cargo("BALL") == INVALID_CARGO) return false;
	const auto &factories = IntegratedEconomy::Factories();
	if (factories.at(b.steel_works).total_batches != 0 || factories.at(b.ballast_works).total_batches != 0) return false;
	for (IndustryID id : {b.steel_works, b.ballast_works}) {
		for (const auto &[cargo, units] : factories.at(id).remainder) if (units != 0) return false;
		for (const auto &accepted : Industry::Get(id)->accepted) if (accepted.waiting != 0) return false;
	}
	for (const auto &[id, factory] : factories) if (factory.owner == b.company) {
		if (factory.total_batches != 0) return false;
		for (const auto &[cargo, units] : factory.remainder) if (units != 0) return false;
		const Industry *industry = Industry::GetIfValid(id);
		if (industry == nullptr) return false;
		for (const auto &accepted : industry->accepted) if (accepted.waiting != 0) return false;
	}
	for (const ProcessingFacility &facility : ProductionChainManager::GetAllFacilities()) if (facility.owner == b.company) {
		for (const auto &[cargo, units] : facility.input_buffers) if (units != 0) return false;
		for (const auto &[cargo, units] : facility.output_buffers) if (units != 0) return false;
	}
	for (const Vehicle *vehicle : Vehicle::Iterate())
		if (vehicle->owner == b.company && vehicle->cargo.TotalCount() != 0) return false;
	const EconomyCity *city = IntegratedEconomy::City(b.mito);
	if (city != nullptr) for (const auto &[cargo, units] : city->reserves) if (units != 0) return false;
	for (IndustryID id : {b.iron_source, b.silica_source})
		for (const auto &produced : Industry::Get(id)->produced) if (produced.waiting != 0) return false;
	for (const CompanyWorldStockpile &stock : StockpileManager::GetAllStockpiles())
		if (stock.company_id == b.company) for (const auto &[cargo, units] : stock.inventory) if (units != 0) return false;
	for (const Station *station : Station::Iterate()) if (station->owner == b.company)
		for (uint8_t cargo = 0; cargo < NUM_CARGO; ++cargo) if (station->goods[CargoType{cargo}].TotalCount() != 0) return false;
	const TownResidentialDistrict *district = IntegratedEconomy::GetTownResidentialDistrict(b.mito);
	if (district != nullptr && (district->commissioned || district->buildings_placed != 0 || district->buildings_completed != 0)) return false;
	FirstPermanentLineState next;
	next.active = true;
	next.binding = b;
	mission = std::move(next);
	return true;
}

/** @brief Record qualifying native train delivery from each bound source to its factory.
 * @param vehicle Delivering vehicle.
 * @param company Delivering company.
 * @param source Producing industry.
 * @param destination Accepting factory.
 * @param cargo Delivered cargo type.
 * @param units Positive native units delivered.
 */
void FirstPermanentLine::OnIndustryDelivery(VehicleID vehicle, CompanyID company, IndustryID source, IndustryID destination, CargoType cargo, uint32_t units)
{
	if (!mission.active || mission.invalid || mission.awarded || company != mission.binding.company || units == 0 || !TrainValid(vehicle, company)) return;
	for (size_t i = 0; i < 2; ++i) {
		if (source == (i == 0 ? mission.binding.iron_source : mission.binding.silica_source) &&
			destination == (i == 0 ? mission.binding.steel_works : mission.binding.ballast_works) &&
			cargo == Cargo(i == 0 ? "IRON" : "SILC") && BindingValid(mission.binding)) mission.input_delivered[i] = true;
	}
}

void FirstPermanentLine::OnEconomyEvaluation()
{
	if (!mission.active || mission.invalid) return;
	if (mission.economy_evaluations == UINT32_MAX) {
		if (!mission.awarded) Invalidate("evaluation-budget-exhausted");
		return;
	}
	++mission.economy_evaluations;
}

/** @brief Record positive native factory production after qualified input.
 * @param industry Factory that produced.
 * @param batches Native batches produced.
 */
void FirstPermanentLine::OnProduction(IndustryID industry, uint32_t batches)
{
	if (!mission.active || mission.invalid || mission.awarded || batches == 0) return;
	for (size_t i = 0; i < 2; ++i) {
		if (industry == (i == 0 ? mission.binding.steel_works : mission.binding.ballast_works) &&
			mission.input_delivered[i] && BindingValid(mission.binding)) mission.produced_after_input[i] = true;
	}
}

void FirstPermanentLine::OnCommission(CompanyID company, TownID town)
{
	if (!mission.active || mission.invalid || mission.awarded) return;
	if (town != mission.binding.mito) return;
	if (company != mission.binding.company) { Invalidate("foreign-commission"); return; }
	const TownResidentialDistrict *district = IntegratedEconomy::GetTownResidentialDistrict(town);
	if (!BindingValid(mission.binding) || !MilestoneOne() || mission.stored_units[0] < 160 || mission.stored_units[1] < 80 ||
		mission.commissioned || district == nullptr || district->owner != company || !district->commissioned) {
		Invalidate("premature-or-unsupported-commission");
		return;
	}
	mission.commissioned = true;
}

void FirstPermanentLine::OnHousePlaced(TownID town, TileIndex tile)
{
	if (!mission.active || mission.invalid || mission.awarded || !mission.commissioned || town != mission.binding.mito ||
		mission.house_count >= mission.houses.size() || !HouseValid(town, tile, false)) return;
	for (size_t i = 0; i < mission.house_count; ++i) if (mission.houses[i] == tile) return;
	mission.houses[mission.house_count++] = tile;
}

/** @brief Mark a registered protected house complete after native construction.
 * @param tile Completed house anchor.
 */
void FirstPermanentLine::OnHouseCompleted(TileIndex tile)
{
	if (!mission.active || mission.invalid || mission.awarded) return;
	for (size_t i = 0; i < mission.house_count; ++i)
		if (mission.houses[i] == tile && HouseValid(mission.binding.mito, tile, true)) mission.house_completed[i] = true;
}

void FirstPermanentLine::OnHouseRemoved(TileIndex tile)
{
	if (!mission.active || mission.invalid || mission.awarded) return;
	for (size_t i = 0; i < mission.house_count; ++i) {
		if (mission.houses[i] == tile) { Invalidate("objective-house-removed"); return; }
	}
}

/** @brief Record an explicit works or Mito hub arrival for journey ordering.
 * @param vehicle Arriving train.
 * @param station Native station visited.
 */
void FirstPermanentLine::OnArrival(VehicleID vehicle, StationID station)
{
	if (!mission.active || mission.invalid || mission.awarded || !TrainValid(vehicle, mission.binding.company)) return;
	const Vehicle *train = Vehicle::GetIfValid(vehicle);
	const Station *st = Station::GetIfValid(station);
	if (st == nullptr || st->owner != mission.binding.company || !ScheduledAt(train, station) || !RouteValid(train, StationID::Invalid(), mission.binding.company)) return;
	const bool source = Catches(st, Industry::GetIfValid(mission.binding.steel_works)) ||
		Catches(st, Industry::GetIfValid(mission.binding.ballast_works));
	const bool hub = HubValid(station, mission.binding.company);
	if (!source && !hub) return;
	auto it = mission.journeys.find(vehicle);
	if (it == mission.journeys.end()) {
		if (mission.journeys.size() >= MAX_JOURNEYS) return;
		it = mission.journeys.emplace(vehicle, FirstPermanentLineJourney{}).first;
	}
	auto &journey = it->second;
	if (journey.visit == std::numeric_limits<uint32_t>::max()) { journey = {}; return; }
	++journey.visit;
	journey.last_arrival = station;
	if (source && journey.phase == 1) journey.phase = 2;
}

/** @brief Record native rail storage and one positive deposit per ordered hub visit.
 * @param vehicle Delivering train.
 * @param company Delivering company.
 * @param source Bound producing works.
 * @param destination Receiving Mito hub station.
 * @param cargo Stored cargo type.
 * @param units Positive native units stored.
 */
void FirstPermanentLine::OnStoredDelivery(VehicleID vehicle, CompanyID company, IndustryID source, StationID destination, CargoType cargo, uint32_t units)
{
	if (!mission.active || mission.invalid || mission.awarded || company != mission.binding.company || units == 0 ||
		!TrainValid(vehicle, company) || !HubValid(destination, company) || !BindingValid(mission.binding)) return;
	int type = source == mission.binding.steel_works && cargo == Cargo("STEL") ? 0 :
		source == mission.binding.ballast_works && cargo == Cargo("BALL") ? 1 : -1;
	if (type < 0) return;
	mission.stored_units[type] = std::min(MAX_EVIDENCE_UNITS, mission.stored_units[type] + std::min(units, MAX_EVIDENCE_UNITS));

	auto it = mission.journeys.find(vehicle);
	if (it == mission.journeys.end() || it->second.last_arrival != destination || it->second.visit == 0 ||
		it->second.last_deposit_visit == it->second.visit ||
		!ScheduledDeliveryAt(Vehicle::GetIfValid(vehicle), destination) ||
		!ServiceValid(Vehicle::GetIfValid(vehicle), destination, company)) return;
	auto &journey = it->second;
	journey.last_deposit_visit = journey.visit;
	if (journey.phase == 0) journey.phase = 1;
	else if (journey.phase == 2 && mission.commissioned) journey.phase = 3;
}

/** @brief Discard journey progress before a native vehicle identifier can be reused.
 * @param vehicle Removed vehicle identifier.
 */
void FirstPermanentLine::OnVehicleRemoved(VehicleID vehicle) { mission.journeys.erase(vehicle); }

void FirstPermanentLine::Evaluate()
{
	if (!mission.active || mission.invalid || mission.awarded) return;
	if (!BindingValid(mission.binding)) { Invalidate("objective-binding-lost"); return; }
	for (auto it = mission.journeys.begin(); it != mission.journeys.end(); ) {
		if (!TrainValid(it->first, mission.binding.company) || !RouteValid(Vehicle::GetIfValid(it->first), StationID::Invalid(), mission.binding.company)) it = mission.journeys.erase(it);
		else ++it;
	}
	if (!MilestoneOne() || !mission.commissioned || mission.house_count != 4) return;
	for (size_t i = 0; i < 4; ++i) {
		if (!HouseValid(mission.binding.mito, mission.houses[i], true)) return;
		mission.house_completed[i] = true;
	}
	const TownResidentialDistrict *district = IntegratedEconomy::GetTownResidentialDistrict(mission.binding.mito);
	if (district == nullptr || !district->commissioned || district->owner != mission.binding.company ||
		district->buildings_placed != 4 || district->buildings_completed != 4) return;
	for (const auto &[id, journey] : mission.journeys) {
		if (journey.phase == 3 && ServiceValid(Vehicle::GetIfValid(id), StationID::Invalid(), mission.binding.company)) {
			mission.awarded = true;
			return;
		}
	}
}

/** @brief Serialize the bounded active observer state as schema-one JSON.
 * @return JSON evidence for an active mission, or an empty string when inactive.
 */
std::string FirstPermanentLine::Save()
{
	if (!mission.active) return "";
	const auto &b = mission.binding;
	Json j = {{"schema", 1}, {"active", true}, {"economy_evaluations", mission.economy_evaluations}, {"invalid", mission.invalid}, {"awarded", mission.awarded},
		{"reason", mission.reason}, {"binding", {b.company.base(), b.mito.base(), b.steel_works.base(), b.ballast_works.base(), b.iron_source.base(), b.silica_source.base()}},
		{"delivered", mission.input_delivered}, {"produced", mission.produced_after_input}, {"stored", mission.stored_units},
		{"commissioned", mission.commissioned}, {"house_count", mission.house_count}, {"houses", Json::array()}, {"journeys", Json::array()}};
	for (size_t i = 0; i < mission.house_count; ++i) j["houses"].push_back({mission.houses[i].base(), mission.house_completed[i]});
	for (const auto &[id, v] : mission.journeys) j["journeys"].push_back({id.base(), v.phase, v.last_arrival.base(), v.visit, v.last_deposit_visit});
	return j.dump();
}

bool FirstPermanentLine::Load(const std::string &data)
{
	if (data.empty()) { Reset(); return true; }
	try {
		if (data.size() > 300000) return false;
		const Json j = Json::parse(data);
		if (j.is_object() && j.contains("active") && !JsonBoolean(j.at("active"))) {
			ExactObject(j, {"schema", "active"});
			if (Number(j.at("schema"), 1) != 1) return false;
			Reset();
			return true;
		}
		ExactObject(j, {"schema", "active", "economy_evaluations", "invalid", "awarded", "reason", "binding", "delivered", "produced", "stored", "commissioned", "house_count", "houses", "journeys"});
		if (Number(j.at("schema"), 1) != 1 || !JsonBoolean(j.at("active"))) return false;
		FirstPermanentLineState next;
		next.active = true;
		next.economy_evaluations = uint32_t(Number(j.at("economy_evaluations"), UINT32_MAX));
		next.invalid = JsonBoolean(j.at("invalid")); next.awarded = JsonBoolean(j.at("awarded"));
		if (!j.at("reason").is_string()) return false;
		next.reason = j.at("reason").get<std::string>();
		if (next.reason.size() > 64 || (next.invalid != !next.reason.empty()) || (next.invalid && next.awarded)) return false;
		const auto &binding = j.at("binding");
		if (!binding.is_array() || binding.size() != 6) return false;
		next.binding.company = CompanyID{uint8_t(Number(binding[0], MAX_COMPANIES - 1))};
		next.binding.mito = TownID{uint16_t(Number(binding[1], TownID::End().base() - 1))};
		next.binding.steel_works = IndustryID{uint16_t(Number(binding[2], IndustryID::End().base() - 1))};
		next.binding.ballast_works = IndustryID{uint16_t(Number(binding[3], IndustryID::End().base() - 1))};
		next.binding.iron_source = IndustryID{uint16_t(Number(binding[4], IndustryID::End().base() - 1))};
		next.binding.silica_source = IndustryID{uint16_t(Number(binding[5], IndustryID::End().base() - 1))};
		if (next.binding.steel_works == next.binding.ballast_works || next.binding.iron_source == next.binding.silica_source) return false;
		for (const auto &[key, out] : {std::pair<const char *, std::array<bool, 2> *>{"delivered", &next.input_delivered}, {"produced", &next.produced_after_input}}) {
			const auto &a = j.at(key);
			if (!a.is_array() || a.size() != 2) return false;
			for (size_t i = 0; i < 2; ++i) (*out)[i] = JsonBoolean(a[i]);
		}
		for (size_t i = 0; i < 2; ++i) if (next.produced_after_input[i] && !next.input_delivered[i]) return false;
		const auto &stored = j.at("stored");
		if (!stored.is_array() || stored.size() != 2) return false;
		for (size_t i = 0; i < 2; ++i) next.stored_units[i] = uint32_t(Number(stored[i], MAX_EVIDENCE_UNITS));
		next.commissioned = JsonBoolean(j.at("commissioned"));
		next.house_count = uint8_t(Number(j.at("house_count"), 4));
		const auto &houses = j.at("houses");
		if (!houses.is_array() || houses.size() != next.house_count || (!next.commissioned && next.house_count != 0)) return false;
		for (size_t i = 0; i < next.house_count; ++i) {
			if (!houses[i].is_array() || houses[i].size() != 2) return false;
			next.houses[i] = TileIndex{uint32_t(Number(houses[i][0], UINT32_MAX - 1))};
			next.house_completed[i] = JsonBoolean(houses[i][1]);
			for (size_t k = 0; k < i; ++k) if (next.houses[k] == next.houses[i]) return false;
		}
		if (next.commissioned && (!next.input_delivered[0] || !next.input_delivered[1] || !next.produced_after_input[0] || !next.produced_after_input[1] || next.stored_units[0] < 160 || next.stored_units[1] < 80)) return false;
		const auto &journeys = j.at("journeys");
		if (!journeys.is_array() || journeys.size() > MAX_JOURNEYS || (next.invalid && !journeys.empty())) return false;
		for (const auto &row : journeys) {
			if (!row.is_array() || row.size() != 5) return false;
			VehicleID id{uint32_t(Number(row[0], VehicleID::End().base() - 1))};
			FirstPermanentLineJourney v;
			v.phase = uint8_t(Number(row[1], 3));
			v.last_arrival = StationID{uint16_t(Number(row[2], StationID::Invalid().base()))};
			v.visit = uint32_t(Number(row[3], UINT32_MAX));
			v.last_deposit_visit = uint32_t(Number(row[4], v.visit));
			if ((v.visit == 0) != (v.last_arrival == StationID::Invalid()) || (v.phase != 0 && v.last_deposit_visit == 0) ||
				(v.phase >= 2 && v.visit < 2) || (!next.commissioned && v.phase == 3) ||
				!next.journeys.emplace(id, v).second) return false;
		}
		if (next.awarded) {
			if (next.house_count != 4 || !next.commissioned ||
				!std::all_of(next.house_completed.begin(), next.house_completed.end(), [](bool done) { return done; })) return false;
		}
		mission = std::move(next);
		return true;
	} catch (const Json::exception &) { return false; } catch (const std::exception &) { return false; }
}

/** @brief Validate restored bindings and journey references against native objects.
 * @return True when the restored observer is coherent; objective loss may mark it invalid.
 */
bool FirstPermanentLine::ValidateAfterLoad()
{
	if (!mission.active || mission.awarded) return true;
	if (mission.invalid) return true;
	if (!BindingValid(mission.binding)) { Invalidate("objective-binding-lost"); return true; }
	for (size_t i = 0; i < mission.house_count; ++i) if (!HouseValid(mission.binding.mito, mission.houses[i], mission.house_completed[i])) {
		Invalidate("objective-house-lost"); return true;
	}
	for (auto it = mission.journeys.begin(); it != mission.journeys.end(); ) {
		if (!TrainValid(it->first, mission.binding.company) ||
			!RouteValid(Vehicle::GetIfValid(it->first), StationID::Invalid(), mission.binding.company) ||
			(it->second.last_arrival != StationID::Invalid() &&
				(Station::GetIfValid(it->second.last_arrival) == nullptr ||
				Station::Get(it->second.last_arrival)->owner != mission.binding.company ||
				!RouteContains(Vehicle::GetIfValid(it->first), it->second.last_arrival)))) it = mission.journeys.erase(it);
		else ++it;
	}
	return true;
}
