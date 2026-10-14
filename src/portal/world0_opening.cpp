/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file world0_opening.cpp Native evidence for the Aster Vale founding chapter. */
#include "../stdafx.h"
#include "world0_opening.h"
#include "first_permanent_line.h"
#include "integrated_economy.h"
#include "logistics_hub.h"
#include "company_stockpile.h"
#include "planet_manager.h"
#include "resource_sites.h"
#include "../3rdparty/nlohmann/json.hpp"
#include "../cargotype.h"
#include "../company_base.h"
#include "../depot_base.h"
#include "../industry.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../town.h"
#include "../vehicle_base.h"
#include "../waypoint_base.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>
#include "../safeguards.h"

namespace {
/** @brief JSON representation used for bounded Vale opening saves. */
using Json = nlohmann::json;
/** @brief The single authored Aster Vale world. */
constexpr WorldID WORLD{0};
/** @brief Maximum freight units credited by persisted Vale counters. */
constexpr uint32_t MAX_UNITS = 1000000;
/** @brief Maximum live train journeys tracked by the opening. */
constexpr size_t MAX_JOURNEYS = 4096;
/** @var LOST_QUARRY
 * @brief Tombstone bit for removal of the bound silica quarry.
 */
/** @var LOST_CRUSHER
 * @brief Tombstone bit for removal of the bound ballast crusher.
 */
/** @var LOST_YARD
 * @brief Tombstone bit for removal of the bound logistics hub.
 */
/** @var LOST_COMPANY
 * @brief Tombstone bit for revocation of the founding charter.
 */
constexpr uint8_t LOST_QUARRY = 1, LOST_CRUSHER = 2, LOST_YARD = 4, LOST_COMPANY = 8;
/** @brief Current private Vale opening state. */
World0OpeningState opening;

/** @brief Resolve a four-character Vale cargo label.
 * @param label Four-character cargo label.
 * @return The native cargo type, or INVALID_CARGO when absent.
 */
CargoType Cargo(const char *label)
{
	CargoLabel value;
	std::copy_n(label, 4, value.begin());
	return GetCargoTypeByLabel(value);
}

/** @brief Find the authored quarry resource record by stable site identifier.
 * @param id Resource-site identifier.
 * @return The matching native site, or null when absent.
 */
const ResourceSite *QuarrySite(uint32_t id)
{
	for (const ResourceSite &site : ResourceSiteManager::Sites()) if (site.id == id) return &site;
	return nullptr;
}

/** @brief Validate the one-world Frontier profile, towns, plot, and silica site.
 * @param p Authored opening profile.
 * @return True when its native geography and source catalog remain valid.
 */
bool WorldProfileValid(const World0OpeningProfile &p)
{
	const PlanetRegion *region = PlanetManager::GetRegion(WORLD);
	if (PlanetManager::Count() != 1 ||
		region == nullptr || region->phase != WorldPhase::Phase3_Frontier ||
		p.crusher_plot == INVALID_TILE || p.crusher_plot >= Map::Size() ||
		PlanetManager::GetTileWorld(p.crusher_plot) != WORLD) return false;
	for (size_t i = 0; i < p.towns.size(); ++i) {
		const Town *town = Town::GetIfValid(p.towns[i]);
		if (town == nullptr || PlanetManager::GetTileWorld(town->xy) != WORLD) return false;
		for (size_t j = 0; j < i; ++j) if (p.towns[i] == p.towns[j]) return false;
	}
	const ResourceSite *site = QuarrySite(p.quarry_site);
	if (p.quarry_site == 0 || site == nullptr || site->world != WORLD || site->anchor >= Map::Size() ||
		site->width == 0 || site->height == 0 ||
		TileX(site->anchor) + site->width > Map::SizeX() || TileY(site->anchor) + site->height > Map::SizeY() ||
		PlanetManager::GetTileWorld(site->anchor) != WORLD) return false;
	IndustryType type = ResourceSiteManager::ResolveType(*site);
	if (type == IT_INVALID || !ResourceSiteManager::IsPrimary(type)) return false;
	const auto &outputs = GetIndustrySpec(type)->produced_cargo;
	return Cargo("SILC") != INVALID_CARGO && Cargo("BALL") != INVALID_CARGO &&
		std::ranges::find(outputs, Cargo("SILC")) != outputs.end();
}

/** @brief Validate the charter company and active v2 Frontier role.
 * @param p Authored opening profile.
 * @return True when the saved profile matches the live economy.
 */
bool IdentityValid(const World0OpeningProfile &p)
{
	return Company::IsValidID(p.company) && WorldProfileValid(p) &&
		IntegratedEconomy::Enabled() && IntegratedEconomy::ActiveRules() == EconomyRulesVersion::V2 &&
		IntegratedEconomy::Role(WORLD) == EconomicRole::Frontier;
}

/** @brief Verify a player-founded World 0 ballast crusher.
 * @param id Crusher industry identifier.
 * @param owner Required founding company.
 * @return True when the live factory has the required owner, world, and recipe.
 */
bool FactoryValid(IndustryID id, CompanyID owner)
{
	const Industry *industry = Industry::GetIfValid(id);
	const auto &factories = IntegratedEconomy::Factories();
	auto it = factories.find(id);
	return industry != nullptr && it != factories.end() && it->second.owner == owner &&
		industry->founder == owner && IntegratedEconomy::IndustryRecipe(industry->type) == RECIPE_BALLAST_CRUSHING &&
		PlanetManager::GetTileWorld(industry->location.tile) == WORLD;
}

/** @brief Match the bound silica quarry to its surveyed resource site.
 * @param s Opening state carrying the quarry binding and site identity.
 * @return True when the live quarry occupies the correct site and produces SILC.
 */
bool QuarryValid(const World0OpeningState &s)
{
	const Industry *industry = Industry::GetIfValid(s.binding.quarry);
	const ResourceSite *site = QuarrySite(s.profile.quarry_site);
	if (industry == nullptr || site == nullptr || industry->founder != s.profile.company ||
		site->occupant != industry->index ||
		site->world != WORLD || industry->type != ResourceSiteManager::ResolveType(*site) ||
		PlanetManager::GetTileWorld(industry->location.tile) != WORLD || !industry->IsCargoProduced(Cargo("SILC"))) return false;
	return TileX(industry->location.tile) >= TileX(site->anchor) &&
		TileY(industry->location.tile) >= TileY(site->anchor) &&
		TileX(industry->location.tile) < TileX(site->anchor) + site->width &&
		TileY(industry->location.tile) < TileY(site->anchor) + site->height;
}

bool Catches(const Station *station, const Industry *industry);

/** @brief Check the bound Greybank station and native logistics hub.
 * @param s Opening state carrying the yard and charter.
 * @return True when the owned yard is live, separate from the crusher, and valid.
 */
bool YardValid(const World0OpeningState &s)
{
	const Station *station = Station::GetIfValid(s.binding.yard);
	const LogisticsHub *hub = LogisticsHubManager::GetHubForStation(s.binding.yard);
	return station != nullptr && station->owner == s.profile.company && station->town != nullptr &&
		station->town->index == s.profile.towns[0] &&
		!Catches(station, Industry::GetIfValid(s.binding.crusher)) && station->xy < Map::Size() &&
		PlanetManager::GetTileWorld(station->xy) == WORLD && hub != nullptr &&
		hub->company_id == s.profile.company && hub->world_id == WORLD &&
		LogisticsHubManager::ValidateForStation(*hub);
}

/** @brief Validate all live Vale quarry, crusher, and yard bindings.
 * @param s Bound opening state.
 * @return True when the three native assets and opening identity agree.
 */
bool BindingValid(const World0OpeningState &s)
{
	const Industry *crusher = Industry::GetIfValid(s.binding.crusher);
	return IdentityValid(s.profile) && QuarryValid(s) && FactoryValid(s.binding.crusher, s.profile.company) &&
		crusher != nullptr && crusher->location.tile == s.profile.crusher_plot &&
		crusher->GetCargoAccepted(Cargo("SILC")) != crusher->accepted.end() &&
		crusher->IsCargoProduced(Cargo("BALL")) && YardValid(s);
}

/** @brief Reconcile tracked crusher input and batches with native physical buffers.
 * @param s Opening evidence to check.
 * @return True when pending SILC and credited batches match native custody limits.
 */
bool PhysicalEvidenceValid(const World0OpeningState &s)
{
	const Industry *crusher = Industry::GetIfValid(s.binding.crusher);
	const auto factory = IntegratedEconomy::Factories().find(s.binding.crusher);
	if (crusher == nullptr || factory == IntegratedEconomy::Factories().end()) return false;
	const auto input = crusher->GetCargoAccepted(Cargo("SILC"));
	return input != crusher->accepted.end() &&
		uint64_t(s.qualified_input_remaining) + s.unqualified_input_remaining == input->waiting &&
		s.qualified_batches <= factory->second.total_batches;
}

const char *LossReason(uint8_t losses);

/** @brief Validate explicit removal tombstones and all surviving native bindings.
 * @param s Unavailable opening state.
 * @return True when losses explain cleared bindings and surviving assets remain valid.
 */
bool SurvivingBindingsValid(const World0OpeningState &s)
{
	if (!WorldProfileValid(s.profile) || !IntegratedEconomy::Enabled() ||
		IntegratedEconomy::ActiveRules() != EconomyRulesVersion::V2 ||
		IntegratedEconomy::Role(WORLD) != EconomicRole::Frontier || s.losses == 0 || s.losses > 15 ||
		s.unavailable != (s.losses != 0) || s.reason != LossReason(s.losses) || !s.journeys.empty()) return false;
	if (s.losses & LOST_YARD) {
		if (s.removed_hub_id == 0 || s.removed_yard_station == StationID::Invalid() ||
			s.removed_yard_tile == INVALID_TILE || s.removed_yard_tile >= Map::Size() ||
			PlanetManager::GetTileWorld(s.removed_yard_tile) != WORLD) return false;
	} else if (s.removed_hub_id != 0 || s.removed_yard_station != StationID::Invalid() ||
		s.removed_yard_tile != INVALID_TILE) return false;
	if (s.losses & LOST_COMPANY) {
		return s.profile.company == CompanyID::Invalid() && s.binding.quarry == IndustryID::Invalid() &&
			s.binding.crusher == IndustryID::Invalid() && s.binding.yard == StationID::Invalid();
	}
	if (!Company::IsValidID(s.profile.company)) return false;
	if (s.losses & LOST_QUARRY) {
		if (s.binding.quarry != IndustryID::Invalid()) return false;
	} else if (!QuarryValid(s)) return false;
	if (s.losses & LOST_CRUSHER) {
		if (s.binding.crusher != IndustryID::Invalid()) return false;
	} else {
		const Industry *crusher = Industry::GetIfValid(s.binding.crusher);
		if (!FactoryValid(s.binding.crusher, s.profile.company) || crusher == nullptr ||
			crusher->location.tile != s.profile.crusher_plot ||
			crusher->GetCargoAccepted(Cargo("SILC")) == crusher->accepted.end() ||
			!crusher->IsCargoProduced(Cargo("BALL"))) return false;
	}
	if (s.losses & LOST_YARD) {
		if (s.binding.yard != StationID::Invalid()) return false;
	} else if (!YardValid(s)) return false;
	return true;
}

/** @brief Select the stable reason for explicit native asset or charter loss.
 * @param losses Bitmask of recorded native removals.
 * @return The highest-priority saved loss reason.
 */
const char *LossReason(uint8_t losses)
{
	if (losses & LOST_COMPANY) return "founding-charter-revoked";
	if (losses & LOST_QUARRY) return "quarry-removed";
	if (losses & LOST_CRUSHER) return "crusher-removed";
	if (losses & LOST_YARD) return "yard-removed";
	return "";
}

/** @brief Check whether station catchment includes any industry tile.
 * @param station Station to inspect, or null.
 * @param industry Industry to inspect, or null.
 * @return True when the station can serve the industry.
 */
bool Catches(const Station *station, const Industry *industry)
{
	if (station == nullptr || industry == nullptr) return false;
	for (TileIndex tile : industry->location) if (station->catchment_tiles.HasTile(tile)) return true;
	return false;
}

/** @brief Check a live, uncrashed front train belongs to the Vale charter.
 * @param id Native vehicle identifier.
 * @param owner Required charter company.
 * @return True for an eligible native train.
 */
bool TrainValid(VehicleID id, CompanyID owner)
{
	const Vehicle *train = Vehicle::GetIfValid(id);
	return train != nullptr && train->type == VehicleType::Train && train->IsFrontEngine() &&
		train->owner == owner && !train->vehstatus.Test(VehState::Crashed);
}

/** @brief Require an arrival at the current explicit station order.
 * @param train Candidate front train.
 * @param station Native arrival station.
 * @return True when the active order proves this stop.
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

/** @brief Require ordered loading at the delivery station.
 * @param train Delivering front train.
 * @param station Destination yard station.
 * @return True when the explicit order or native loading marker proves this visit.
 */
bool ScheduledDeliveryAt(const Vehicle *train, StationID station)
{
	if (train == nullptr || train->cur_real_order_index >= train->GetNumOrders()) return false;
	const Order *order = train->GetOrder(train->cur_real_order_index);
	if (order == nullptr || !order->IsType(OT_GOTO_STATION) || order->GetDestination().ToStationID() != station ||
		order->GetNonStopType().Test(OrderNonStopFlag::GoVia)) return false;
	return (train->current_order.IsType(OT_LOADING) && train->current_order.GetNonStopType().Test(OrderNonStopFlag::GoVia)) ||
		(train->current_order.IsType(OT_GOTO_STATION) && train->current_order.GetDestination().ToStationID() == station &&
		!train->current_order.GetNonStopType().Test(OrderNonStopFlag::GoVia));
}

/** @brief Check recurring crusher-source and unloadable yard orders.
 * @param train Candidate front train.
 * @param s Opening bindings and charter identity.
 * @return True when the native order list supports both required legs.
 */
bool RouteValid(const Vehicle *train, const World0OpeningState &s)
{
	if (train == nullptr || train->type != VehicleType::Train || !train->IsFrontEngine() ||
		train->owner != s.profile.company || train->GetNumOrders() < 2) return false;
	bool source = false, yard = false;
	for (VehicleOrderID i = 0; i < train->GetNumOrders(); ++i) {
		const Order *order = train->GetOrder(i);
		if (order == nullptr) return false;
		if (order->IsType(OT_IMPLICIT)) continue;
		if (order->IsType(OT_GOTO_DEPOT)) {
			if (order->GetDepotActionType().Test(OrderDepotActionFlag::Halt)) return false;
			if (order->GetDepotActionType().Test(OrderDepotActionFlag::NearestDepot)) continue;
			const Depot *depot = Depot::GetIfValid(order->GetDestination().ToDepotID());
			if (depot == nullptr || depot->xy >= Map::Size() || !IsRailDepotTile(depot->xy) ||
				GetDepotIndex(depot->xy) != depot->index || GetTileOwner(depot->xy) != s.profile.company) return false;
			continue;
		}
		if (order->IsType(OT_CONDITIONAL)) {
			if (order->GetConditionSkipToOrder() >= train->GetNumOrders() ||
				order->GetConditionVariable() >= OrderConditionVariable::End ||
				order->GetConditionComparator() >= OrderConditionComparator::End) return false;
			continue;
		}
		if (order->IsType(OT_GOTO_WAYPOINT)) {
			const Waypoint *waypoint = Waypoint::GetIfValid(order->GetDestination().ToStationID());
			if (waypoint == nullptr || waypoint->owner != s.profile.company || waypoint->xy >= Map::Size() ||
				!IsRailWaypointTile(waypoint->xy) || GetStationIndex(waypoint->xy) != waypoint->index) return false;
			continue;
		}
		if (!order->IsType(OT_GOTO_STATION)) return false;
		const Station *station = Station::GetIfValid(order->GetDestination().ToStationID());
		if (station == nullptr || station->owner != s.profile.company) return false;
		if (order->GetNonStopType().Test(OrderNonStopFlag::GoVia)) continue;
		if (station->index == s.binding.yard && order->GetUnloadType() != OrderUnloadType::NoUnload &&
			order->GetUnloadType() != OrderUnloadType::Transfer) yard = true;
		if (Catches(station, Industry::GetIfValid(s.binding.crusher)) &&
			order->GetLoadType() != OrderLoadType::NoLoad) source = true;
	}
	return source && yard;
}

/** @brief Require an operable Vale train with a valid recurring route.
 * @param train Candidate front train.
 * @param s Opening bindings and charter identity.
 * @return True when the train is running and the route remains valid.
 */
bool ServiceValid(const Vehicle *train, const World0OpeningState &s)
{
	return train != nullptr && !train->vehstatus.Test(VehState::Crashed) &&
		!train->vehstatus.Test(VehState::Stopped) && RouteValid(train, s);
}

/** @brief Latch a native loss, clear journey evidence, and preserve prior milestones.
 * @param kind Bit corresponding to the removed binding or charter.
 */
void Lose(uint8_t kind)
{
	opening.losses |= kind;
	opening.unavailable = true;
	opening.reason = LossReason(opening.losses);
	opening.journeys.clear();
}

/** @brief Parse a strictly unsigned bounded save integer.
 * @param value JSON value to decode.
 * @param max Largest permitted value.
 * @return The decoded integer; throws on type or range mismatch.
 */
uint64_t Number(const Json &value, uint64_t max)
{
	if (!value.is_number_unsigned() || value.get<uint64_t>() > max) throw std::runtime_error("world0 integer");
	return value.get<uint64_t>();
}

/** @brief Parse a strictly typed save boolean.
 * @param value JSON value to decode.
 * @return The decoded boolean; throws for another JSON type.
 */
bool JsonBoolean(const Json &value)
{
	if (!value.is_boolean()) throw std::runtime_error("world0 boolean");
	return value.get<bool>();
}

/** @brief Reject save objects with missing or extra fields.
 * @param value JSON object to inspect.
 * @param keys Exact required field names.
 */
void ExactObject(const Json &value, std::initializer_list<const char *> keys)
{
	if (!value.is_object() || value.size() != keys.size()) throw std::runtime_error("world0 object");
	for (const char *key : keys) if (!value.contains(key)) throw std::runtime_error("world0 field");
}
} // namespace

/** @brief Clear the authored profile, live bindings, and observer evidence.
 */
void World0Opening::Reset() { opening = {}; }
/** @brief Report whether authoring is ready or an opening is already active.
 * @return True for a validated pre-activation profile or an already active opening.
 * This is not the crusher-placement predicate.
 */
bool World0Opening::Ready()
{
	if (!opening.ready || FirstPermanentLine::Active()) return false;
	if (opening.active) return true;
	const ResourceSite *site = QuarrySite(opening.profile.quarry_site);
	return !IntegratedEconomy::Enabled() && Company::IsValidID(opening.profile.company) &&
		WorldProfileValid(opening.profile) && site != nullptr && !Industry::IsValidID(site->occupant);
}
/** @brief Report whether a complete Vale observer binding is installed.
 * @return True for an active bound opening, including unavailable free play.
 */
bool World0Opening::Active() { return opening.active; }
const World0OpeningState &World0Opening::State() { return opening; }

bool World0Opening::BeginAuthoring(const World0OpeningProfile &profile)
{
	if (opening.ready || opening.active || FirstPermanentLine::Active() || IntegratedEconomy::Enabled() ||
		!Company::IsValidID(profile.company) || !WorldProfileValid(profile)) return false;
	for (TownID id : profile.towns) if (Town::Get(id)->cache.population == 0) return false;
	const ResourceSite *site = QuarrySite(profile.quarry_site);
	if (site == nullptr || Industry::IsValidID(site->occupant)) return false;
	for (const Industry *industry : Industry::Iterate())
		if (PlanetManager::GetTileWorld(industry->location.tile) == WORLD &&
			IntegratedEconomy::IndustryRecipe(industry->type) == RECIPE_BALLAST_CRUSHING) return false;
	World0OpeningState next;
	next.ready = true;
	next.profile = profile;
	opening = std::move(next);
	return true;
}

bool World0Opening::PermitsCrusher(TileIndex tile, IndustryType type, CompanyID company)
{
	if (!opening.ready || FirstPermanentLine::Active() || !IdentityValid(opening.profile) || company != opening.profile.company ||
		tile != opening.profile.crusher_plot || IntegratedEconomy::IndustryRecipe(type) != RECIPE_BALLAST_CRUSHING) return false;
	for (const Industry *industry : Industry::Iterate())
		if (PlanetManager::GetTileWorld(industry->location.tile) == WORLD &&
			IntegratedEconomy::IndustryRecipe(industry->type) == RECIPE_BALLAST_CRUSHING) return false;
	return true;
}

bool World0Opening::Begin(const World0OpeningBindings &binding)
{
	if (!opening.ready || opening.active || FirstPermanentLine::Active()) return false;
	World0OpeningState next = opening;
	next.binding = binding;
	if (!BindingValid(next)) return false;
	const ResourceSite *site = QuarrySite(next.profile.quarry_site);
	if (site == nullptr || !ResourceSiteManager::Discovered(next.profile.company, *site)) return false;
	const Industry *quarry = Industry::Get(binding.quarry);
	const Industry *crusher = Industry::Get(binding.crusher);
	if (quarry->GetCargoProduced(Cargo("SILC"))->waiting != 0 ||
		crusher->GetCargoAccepted(Cargo("SILC"))->waiting != 0 ||
		crusher->GetCargoProduced(Cargo("BALL"))->waiting != 0) return false;
	const auto &factory = IntegratedEconomy::Factories().at(binding.crusher);
	if (factory.total_batches != 0 || factory.last_batches != 0) return false;
	for (const auto &[cargo, units] : factory.remainder) if (units != 0) return false;
	for (const Industry *industry : Industry::Iterate()) {
		for (CargoType cargo : {Cargo("SILC"), Cargo("BALL")}) {
			auto produced = industry->GetCargoProduced(cargo);
			auto accepted = industry->GetCargoAccepted(cargo);
			if ((produced != industry->produced.end() && produced->waiting != 0) ||
				(accepted != industry->accepted.end() && accepted->waiting != 0)) return false;
		}
	}
	for (const Station *station : Station::Iterate()) if (station->owner == next.profile.company &&
		(station->goods[Cargo("SILC")].TotalCount() != 0 || station->goods[Cargo("BALL")].TotalCount() != 0)) return false;
	for (const Vehicle *vehicle : Vehicle::Iterate())
		if (vehicle->owner == next.profile.company &&
			(vehicle->cargo_type == Cargo("SILC") || vehicle->cargo_type == Cargo("BALL")) &&
			vehicle->cargo.TotalCount() != 0) return false;
	for (const CompanyWorldStockpile &stock : StockpileManager::GetAllStockpiles())
		if (stock.company_id == next.profile.company && stock.world_id == WORLD &&
			((stock.inventory.contains(Cargo("SILC")) && stock.inventory.at(Cargo("SILC")) != 0) ||
			 (stock.inventory.contains(Cargo("BALL")) && stock.inventory.at(Cargo("BALL")) != 0))) return false;
	next.active = true;
	opening = std::move(next);
	return true;
}

/** @brief Classify native SILC input as quarry-qualified or unqualified custody.
 * @param vehicle Delivering vehicle.
 * @param company Delivering company.
 * @param source Source industry.
 * @param destination Accepting industry.
 * @param cargo Delivered cargo type.
 * @param units Native units accepted by the crusher.
 */
void World0Opening::OnIndustryDelivery(VehicleID vehicle, CompanyID company, IndustryID source, IndustryID destination, CargoType cargo, uint32_t units)
{
	if (!opening.active || opening.unavailable || destination != opening.binding.crusher ||
		cargo != Cargo("SILC") || units == 0) return;
	bool qualified = company == opening.profile.company && source == opening.binding.quarry &&
		TrainValid(vehicle, company);
	auto add_unqualified = [](uint32_t amount) {
		if (amount > MAX_UNITS - opening.unqualified_input_remaining) {
			opening.qualified_input_remaining = 0; // Saturation forfeits ambiguous qualifying custody.
			opening.unqualified_input_remaining = MAX_UNITS;
		} else {
			opening.unqualified_input_remaining += amount;
		}
	};
	if (qualified) {
		uint32_t credited = std::min(units, MAX_UNITS - opening.accepted_silc);
		opening.accepted_silc += credited;
		uint32_t tracked = std::min(credited, MAX_UNITS - opening.qualified_input_remaining);
		opening.qualified_input_remaining += tracked;
		add_unqualified(units - tracked);
		opening.input_goal = opening.accepted_silc >= 80;
	} else {
		add_unqualified(units);
	}
}

/** @brief Debit pending crusher input and credit conservatively qualified batches.
 * @param industry Producing industry.
 * @param batches Native ballast batches produced.
 */
void World0Opening::OnProduction(IndustryID industry, uint32_t batches)
{
	if (!opening.active || opening.unavailable || industry != opening.binding.crusher || batches == 0) return;
	uint64_t consumed = uint64_t(batches) * 2;
	uint32_t unqualified = uint32_t(std::min<uint64_t>(consumed, opening.unqualified_input_remaining));
	opening.unqualified_input_remaining -= unqualified;
	consumed -= unqualified;
	uint32_t qualified = uint32_t(std::min<uint64_t>(consumed, opening.qualified_input_remaining));
	opening.qualified_input_remaining -= qualified;
	opening.qualified_batches = std::min(MAX_UNITS, opening.qualified_batches + qualified / 2);
	opening.production_goal = opening.qualified_batches >= 40;
}

/** @brief Record a scheduled crusher-source or yard visit for the return journey.
 * @param vehicle Arriving train.
 * @param station Native station visited.
 */
void World0Opening::OnArrival(VehicleID vehicle, StationID station)
{
	if (!opening.active || opening.unavailable || opening.awarded || !TrainValid(vehicle, opening.profile.company)) return;
	const Vehicle *train = Vehicle::GetIfValid(vehicle);
	const Station *st = Station::GetIfValid(station);
	if (st == nullptr || st->owner != opening.profile.company || !ScheduledAt(train, station) || !ServiceValid(train, opening)) return;
	bool source = Catches(st, Industry::GetIfValid(opening.binding.crusher));
	bool yard = station == opening.binding.yard && YardValid(opening);
	if (!source && !yard) return;
	auto it = opening.journeys.find(vehicle);
	if (it == opening.journeys.end()) {
		if (opening.journeys.size() >= MAX_JOURNEYS) return;
		it = opening.journeys.emplace(vehicle, World0OpeningJourney{}).first;
	}
	auto &journey = it->second;
	if (journey.visit == UINT32_MAX) { journey = {}; return; }
	++journey.visit;
	journey.last_arrival = station;
	if (source && journey.phase == 1) journey.phase = 2;
}

/** @brief Record native BALL storage and one positive deposit per yard visit.
 * @param vehicle Delivering train.
 * @param company Delivering company.
 * @param source Bound crusher.
 * @param destination Bound yard station.
 * @param cargo Stored cargo type.
 * @param units Positive native units stored.
 */
void World0Opening::OnStoredDelivery(VehicleID vehicle, CompanyID company, IndustryID source, StationID destination, CargoType cargo, uint32_t units)
{
	if (!opening.active || opening.unavailable || opening.awarded || company != opening.profile.company ||
		source != opening.binding.crusher || destination != opening.binding.yard || cargo != Cargo("BALL") ||
		units == 0 || !TrainValid(vehicle, company) || !YardValid(opening)) return;
	opening.stored_ball = std::min(MAX_UNITS, opening.stored_ball + std::min(units, MAX_UNITS));
	opening.storage_goal = opening.stored_ball >= 40;
	auto it = opening.journeys.find(vehicle);
	if (it == opening.journeys.end() || it->second.last_arrival != destination || it->second.visit == 0 ||
		it->second.last_deposit_visit == it->second.visit ||
		!ScheduledDeliveryAt(Vehicle::GetIfValid(vehicle), destination) ||
		!ServiceValid(Vehicle::GetIfValid(vehicle), opening)) return;
	auto &journey = it->second;
	journey.last_deposit_visit = journey.visit;
	if (journey.phase == 0) journey.phase = 1;
	else if (journey.phase == 2) journey.phase = 3;
}

/** @brief Discard journey evidence before native vehicle identifier reuse.
 * @param vehicle Removed vehicle identifier.
 */
void World0Opening::OnVehicleRemoved(VehicleID vehicle) { opening.journeys.erase(vehicle); }

void World0Opening::OnIndustryRemoved(IndustryID industry)
{
	if (!opening.active) return;
	if (industry == opening.binding.quarry) {
		opening.binding.quarry = IndustryID::Invalid(); Lose(LOST_QUARRY);
	}
	if (industry == opening.binding.crusher) {
		opening.binding.crusher = IndustryID::Invalid(); Lose(LOST_CRUSHER);
	}
}

/** @brief Tombstone removal of the bound hub without treating future ID reuse as evidence.
 * @param station Removed hub station.
 * @param hub_id Historical native hub identifier.
 * @param tile Historical hub tile.
 */
void World0Opening::OnYardRemoved(StationID station, uint32_t hub_id, TileIndex tile)
{
	if (!opening.active || station != opening.binding.yard || hub_id == 0 || tile == INVALID_TILE) return;
	opening.removed_yard_station = station;
	opening.removed_yard_tile = tile;
	opening.removed_hub_id = hub_id;
	opening.binding.yard = StationID::Invalid();
	Lose(LOST_YARD);
}

/** @brief Revoke the original charter on native company transfer or deletion.
 * @param old_owner Former owner identifier.
 * @param new_owner Successor owner, or invalid after deletion.
 */
void World0Opening::OnCompanyChanged(CompanyID old_owner, CompanyID new_owner)
{
	if (!opening.active || old_owner != opening.profile.company || old_owner == new_owner) return;
	opening.profile.company = CompanyID::Invalid();
	opening.binding = {};
	Lose(LOST_COMPANY);
}

/** @brief Count one native integrated-economy production invocation.
 */
void World0Opening::OnEconomyEvaluation()
{
	if (!opening.active || opening.economy_evaluations == UINT32_MAX) return;
	++opening.economy_evaluations;
}

void World0Opening::Evaluate()
{
	if (!opening.active || opening.unavailable || opening.awarded) return;
	/* Explicit deletion callbacks write tombstones. A merely missing reference is
	 * save corruption at load and cannot silently turn into a completed mission. */
	if (!BindingValid(opening)) return;
	for (auto it = opening.journeys.begin(); it != opening.journeys.end();) {
		const Vehicle *train = Vehicle::GetIfValid(it->first);
		if (train == nullptr || train->type != VehicleType::Train || !train->IsFrontEngine() ||
			train->owner != opening.profile.company) it = opening.journeys.erase(it);
		else ++it;
	}
	if (!PhysicalEvidenceValid(opening)) return;
	if (!opening.input_goal || !opening.production_goal || !opening.storage_goal) return;
	for (const auto &[id, journey] : opening.journeys) {
		if (journey.phase == 3 && ServiceValid(Vehicle::GetIfValid(id), opening)) { opening.awarded = true; return; }
	}
}

/** @brief Serialize the complete active Vale opening as schema-one JSON.
 * @return JSON evidence for a bound opening, or an empty string when inactive.
 */
std::string World0Opening::Save()
{
	if (!opening.active) return "";
	const auto &p = opening.profile;
	const auto &b = opening.binding;
	Json j = {{"schema", 1}, {"active", true}, {"unavailable", opening.unavailable}, {"awarded", opening.awarded},
		{"losses", opening.losses},
		{"reason", opening.reason}, {"profile", {p.company.base(), p.towns[0].base(), p.towns[1].base(), p.towns[2].base(), p.crusher_plot.base(), p.quarry_site}},
		{"binding", {b.quarry.base(), b.crusher.base(), b.yard.base()}},
		{"yard_tombstone", {opening.removed_yard_station.base(), opening.removed_yard_tile.base(), opening.removed_hub_id}},
		{"economy_evaluations", opening.economy_evaluations},
		{"accepted_silc", opening.accepted_silc}, {"qualified_input_remaining", opening.qualified_input_remaining},
		{"unqualified_input_remaining", opening.unqualified_input_remaining}, {"qualified_batches", opening.qualified_batches},
		{"stored_ball", opening.stored_ball}, {"input_goal", opening.input_goal}, {"production_goal", opening.production_goal},
		{"storage_goal", opening.storage_goal}, {"journeys", Json::array()}};
	for (const auto &[id, v] : opening.journeys)
		j["journeys"].push_back({id.base(), v.phase, v.last_arrival.base(), v.visit, v.last_deposit_visit});
	return j.dump();
}

bool World0Opening::Load(const std::string &data)
{
	if (data.empty()) { Reset(); return true; }
	try {
		if (data.size() > 300000) return false;
		std::vector<std::set<std::string>> object_keys;
		Json j = Json::parse(data, [&object_keys](int, Json::parse_event_t event, Json &parsed) {
			if (event == Json::parse_event_t::object_start) object_keys.emplace_back();
			if (event == Json::parse_event_t::key && !object_keys.back().insert(parsed.get<std::string>()).second)
				throw std::runtime_error("duplicate world0 key");
			if (event == Json::parse_event_t::object_end) object_keys.pop_back();
			return true;
		});
		if (j.is_object() && j.contains("active") && !JsonBoolean(j.at("active"))) {
			ExactObject(j, {"schema", "active"});
			if (Number(j.at("schema"), 1) != 1) return false;
			Reset(); return true;
		}
		ExactObject(j, {"schema", "active", "unavailable", "awarded", "losses", "reason", "profile", "binding", "yard_tombstone", "economy_evaluations",
			"accepted_silc", "qualified_input_remaining", "unqualified_input_remaining", "qualified_batches", "stored_ball",
			"input_goal", "production_goal", "storage_goal", "journeys"});
		if (Number(j.at("schema"), 1) != 1 || !JsonBoolean(j.at("active"))) return false;
		World0OpeningState next;
		next.ready = next.active = true;
		next.unavailable = JsonBoolean(j.at("unavailable")); next.awarded = JsonBoolean(j.at("awarded"));
		next.losses = uint8_t(Number(j.at("losses"), 15));
		if (!j.at("reason").is_string()) return false;
		next.reason = j.at("reason").get<std::string>();
		if (next.reason.size() > 64 || next.unavailable != (next.losses != 0) ||
			next.reason != LossReason(next.losses)) return false;
		const Json &p = j.at("profile"), &b = j.at("binding"), &tombstone = j.at("yard_tombstone");
		if (!p.is_array() || p.size() != 6 || !b.is_array() || b.size() != 3 ||
			!tombstone.is_array() || tombstone.size() != 3) return false;
		next.profile.company = CompanyID{uint8_t(Number(p[0], CompanyID::Invalid().base()))};
		for (size_t i = 0; i < 3; ++i) next.profile.towns[i] = TownID{uint16_t(Number(p[i + 1], TownID::End().base() - 1))};
		next.profile.crusher_plot = TileIndex{uint32_t(Number(p[4], UINT32_MAX - 1))};
		next.profile.quarry_site = uint32_t(Number(p[5], UINT32_MAX));
		if (next.profile.quarry_site == 0 || next.profile.towns[0] == next.profile.towns[1] ||
			next.profile.towns[0] == next.profile.towns[2] || next.profile.towns[1] == next.profile.towns[2]) return false;
		next.binding.quarry = IndustryID{uint16_t(Number(b[0], IndustryID::Invalid().base()))};
		next.binding.crusher = IndustryID{uint16_t(Number(b[1], IndustryID::Invalid().base()))};
		next.binding.yard = StationID{uint16_t(Number(b[2], StationID::Invalid().base()))};
		next.removed_yard_station = StationID{uint16_t(Number(tombstone[0], StationID::Invalid().base()))};
		next.removed_yard_tile = TileIndex{uint32_t(Number(tombstone[1], UINT32_MAX))};
		next.removed_hub_id = uint32_t(Number(tombstone[2], UINT32_MAX));
		if ((next.losses & LOST_YARD) != 0) {
			if (next.removed_yard_station == StationID::Invalid() || next.removed_yard_tile == INVALID_TILE ||
				next.removed_hub_id == 0 || next.binding.yard != StationID::Invalid()) return false;
		} else if (next.removed_yard_station != StationID::Invalid() || next.removed_yard_tile != INVALID_TILE ||
			next.removed_hub_id != 0) return false;
		if ((next.losses & LOST_COMPANY) != 0) {
			if (next.profile.company != CompanyID::Invalid() || next.binding.quarry != IndustryID::Invalid() ||
				next.binding.crusher != IndustryID::Invalid() || next.binding.yard != StationID::Invalid()) return false;
		} else if (next.profile.company == CompanyID::Invalid() ||
			(next.binding.quarry == IndustryID::Invalid()) != bool(next.losses & LOST_QUARRY) ||
			(next.binding.crusher == IndustryID::Invalid()) != bool(next.losses & LOST_CRUSHER) ||
			(next.binding.yard == StationID::Invalid()) != bool(next.losses & LOST_YARD) ||
			(next.binding.quarry == next.binding.crusher && next.binding.quarry != IndustryID::Invalid())) return false;
		next.economy_evaluations = uint32_t(Number(j.at("economy_evaluations"), UINT32_MAX));
		next.accepted_silc = uint32_t(Number(j.at("accepted_silc"), MAX_UNITS));
		next.qualified_input_remaining = uint32_t(Number(j.at("qualified_input_remaining"), MAX_UNITS));
		next.unqualified_input_remaining = uint32_t(Number(j.at("unqualified_input_remaining"), MAX_UNITS));
		next.qualified_batches = uint32_t(Number(j.at("qualified_batches"), MAX_UNITS));
		next.stored_ball = uint32_t(Number(j.at("stored_ball"), MAX_UNITS));
		next.input_goal = JsonBoolean(j.at("input_goal"));
		next.production_goal = JsonBoolean(j.at("production_goal"));
		next.storage_goal = JsonBoolean(j.at("storage_goal"));
		if (uint64_t(next.qualified_input_remaining) + uint64_t(next.qualified_batches) * 2 > next.accepted_silc ||
			next.input_goal != (next.accepted_silc >= 80) || next.production_goal != (next.qualified_batches >= 40) ||
			next.storage_goal != (next.stored_ball >= 40) ||
			(next.awarded && (!next.input_goal || !next.production_goal || !next.storage_goal))) return false;
		const Json &journeys = j.at("journeys");
		if (!journeys.is_array() || journeys.size() > MAX_JOURNEYS || (next.unavailable && !journeys.empty())) return false;
		for (const Json &row : journeys) {
			if (!row.is_array() || row.size() != 5) return false;
			VehicleID id{uint32_t(Number(row[0], VehicleID::End().base() - 1))};
			World0OpeningJourney v;
			v.phase = uint8_t(Number(row[1], 3));
			v.last_arrival = StationID{uint16_t(Number(row[2], StationID::Invalid().base()))};
			v.visit = uint32_t(Number(row[3], UINT32_MAX));
			v.last_deposit_visit = uint32_t(Number(row[4], v.visit));
			if ((v.visit == 0) != (v.last_arrival == StationID::Invalid()) ||
				(v.phase == 0 && v.last_deposit_visit != 0) ||
				(v.phase != 0 && (v.last_deposit_visit == 0 || next.stored_ball == 0)) ||
				(v.phase >= 2 && v.visit < 2) ||
				(v.phase == 3 && (v.visit < 3 || v.last_deposit_visit < 3 || next.stored_ball < 2)) ||
				!next.journeys.emplace(id, v).second) return false;
		}
		opening = std::move(next);
		return true;
	} catch (const std::exception &) { return false; }
}

bool World0Opening::ValidateAfterLoad()
{
	if (!opening.active) return true;
	if (FirstPermanentLine::Active()) return false;
	if (opening.losses != 0) return SurvivingBindingsValid(opening);
	if (!BindingValid(opening)) return false;
	for (const auto &entry : opening.journeys) {
		const Vehicle *train = Vehicle::GetIfValid(entry.first);
		if (train == nullptr || train->type != VehicleType::Train || !train->IsFrontEngine() ||
			train->owner != opening.profile.company) return false;
	}
	return PhysicalEvidenceValid(opening);
}
