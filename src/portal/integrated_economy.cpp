/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file integrated_economy.cpp Integrated production, city baskets and research kits. */
#include "../stdafx.h"
#include "fabrication_manager.h"
#include "integrated_economy.h"
#include "../3rdparty/nlohmann/json.hpp"
#include "../cargotype.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../genworld.h"
#include "../industry.h"
#include "../industry_type.h"
#include "../network/network.h"
#include "../newgrf_config.h"
#include "../strings_func.h"
#include "../settings_type.h"
#include "../table/strings.h"
#include "../town.h"
#include "../timer/timer_game_calendar.h"
#include "../timer/timer_game_tick.h"
#include "commonwealth_pack.h"
#include "commonwealth_slice.h"
#include "corporate_hq.h"
#include "federation_cmd.h"
#include "federation_identity.h"
#include "planet_manager.h"
#include "portal_cmd.h"
#include "resource_sites.h"
#include "universe_network.h"
#include "../safeguards.h"
std::vector<nlohmann::json> *_integrated_city_month_audit = nullptr;
namespace
{
bool enabled = false;
EconomyAccounting accounting{};
std::map<WorldID, EconomicRole> roles;
std::map<IndustryID, EconomyFactory> factories;
std::map<TownID, EconomyCity> cities;
std::map<CompanyID, EconomyResearch> research;
std::map<std::string, nlohmann::json> shared_research;
std::string Identity(CompanyID company)
{
	auto id = FederationIdentityRegistry::FindCompany(company);
	return id ? UniverseNetwork::Namespace(id->name_space) + ":" + fmt::format("{}", id->sequence) : "";
}

CargoType Cargo(const char *label)
{
	CargoLabel value;
	std::copy_n(label, 4, value.begin());
	return GetCargoTypeByLabel(value);
}
std::map<CargoType, uint32_t> Materials(IndustryType type)
{
	if (IntegratedEconomy::RecipeTech(IntegratedEconomy::IndustryRecipe(type)) < TECH_MATERIALS_3) return {};
	return {{Cargo("STEL"), 50}, {Cargo("MACH"), 10}};
}
} // namespace
void IntegratedEconomy::Reset()
{
	enabled = false;
	accounting = {};
	roles.clear();
	factories.clear();
	cities.clear();
	research.clear();
	shared_research.clear();
}
void IntegratedEconomy::Record(EconomyFlow flow, CargoType cargo, uint32_t units)
{
	if (!enabled || cargo >= NUM_CARGO) return;
	auto &total = accounting[static_cast<size_t>(flow)][cargo];
	total += std::min<uint64_t>(units, UINT64_MAX - total);
}
const EconomyAccounting &IntegratedEconomy::Accounting()
{
	return accounting;
}
void IntegratedEconomy::RestoreAccounting(const EconomyAccounting &value)
{
	accounting = value;
}
bool IntegratedEconomy::Enabled()
{
	return enabled;
}
void IntegratedEconomy::SetEnabled(bool value)
{
	enabled = value;
	ProductionChainManager::InitDefaultRecipes();
}
bool IntegratedEconomy::ContentReady()
{
	auto config = GetGRFConfig(COMMONWEALTH_INDUSTRY_GRFID);
	if (config == nullptr || config->version != 5 || config->status != GRFStatus::Activated) return false;
	for (auto label : {"BALL", "SIGE", "MGLA", "MACH", "OIL_", "GRAI", "FOOD"})
		if (Cargo(label) == INVALID_CARGO) return false;
	return CommonwealthPackManager::GetContentStatus().mode == CommonwealthContentMode::Active;
}
bool IntegratedEconomy::StartNewGame()
{
	if (!ContentReady() || PlanetManager::Count() < 2) return false;
	SetEnabled(true);
	/* Surveyed, player-built expansion is part of this ruleset, not a legacy toggle. */
	_settings_game.game_creation.player_built_economy = true;
	ResourceSiteManager::SetEnabled(true);
	for (auto &world : PlanetManager::GetAllRegions()) {
		auto role = world.phase == WorldPhase::Phase1_Core		  ? EconomicRole::Core
					: world.phase == WorldPhase::Phase2_Developed ? EconomicRole::Industrial
																  : EconomicRole::Frontier;
		if (world.name == "Tandil") role = EconomicRole::Industrial;
		RegisterRole(world.id, role);
	}
	return true;
}
void IntegratedEconomy::RegisterRole(WorldID world, EconomicRole role)
{
	roles.try_emplace(world, role);
}
EconomicRole IntegratedEconomy::Role(WorldID world)
{
	auto it = roles.find(world);
	return it == roles.end() ? EconomicRole::Frontier : it->second;
}
const char *IntegratedEconomy::RoleName(WorldID world)
{
	switch (Role(world)) {
	case EconomicRole::Core:
		return "Core";
	case EconomicRole::Industrial:
		return "Industrial";
	default:
		return "Frontier";
	}
}
RecipeID IntegratedEconomy::IndustryRecipe(IndustryType type)
{
	auto spec = GetIndustrySpec(type);
	if (spec->grf_prop.grfid != COMMONWEALTH_INDUSTRY_GRFID) return RECIPE_NONE;
	switch (spec->grf_prop.local_id) {
	case 0x11:
		return RECIPE_BALLAST_CRUSHING;
	case 0x13:
		return RECIPE_STEEL_SMELTING;
	case 0x15:
		return RECIPE_SUPERALLOY_FOUNDRY;
	case 0x17:
		return RECIPE_COPPER_SMELTING;
	case 0x19:
		return RECIPE_SILICON_ARC;
	case 0x1a:
		return RECIPE_POLYMER_SYNTHESIS;
	case 0x1b:
		return RECIPE_MONOCRYSTAL_SYNTHESIS;
	case 0x1c:
		return RECIPE_QUANTUM_ENRICHMENT;
	case 0x1d:
		return RECIPE_CONSUMER_CRYSTAL_FORMAT;
	case 0x1e:
		return RECIPE_SIGNALLING_ASSEMBLY;
	case 0x1f:
		return RECIPE_MAGLEV_WORKS;
	case 0x20:
		return RECIPE_MACHINE_MODULES;
	case 0x21:
		return 404;
	default:
		return RECIPE_NONE;
	}
}
TechID IntegratedEconomy::RecipeTech(RecipeID r)
{
	switch (r) {
	case RECIPE_STEEL_SMELTING:
		return TECH_MATERIALS_1;
	case RECIPE_COPPER_SMELTING:
	case RECIPE_SILICON_ARC:
	case RECIPE_SIGNALLING_ASSEMBLY:
	case RECIPE_MACHINE_MODULES:
		return TECH_MATERIALS_2;
	case RECIPE_SUPERALLOY_FOUNDRY:
	case RECIPE_POLYMER_SYNTHESIS:
	case RECIPE_MONOCRYSTAL_SYNTHESIS:
	case RECIPE_CONSUMER_CRYSTAL_FORMAT:
		return TECH_MATERIALS_3;
	case RECIPE_MAGLEV_WORKS:
	case RECIPE_QUANTUM_ENRICHMENT:
		return TECH_MATERIALS_4;
	default:
		return TECH_NONE;
	}
}
bool IntegratedEconomy::Managed(const Industry *i)
{
	return enabled && i != nullptr && IndustryRecipe(i->type) != RECIPE_NONE;
}
void IntegratedEconomy::RegisterIndustry(Industry *i)
{
	if (Managed(i)) factories.try_emplace(i->index, EconomyFactory{i->index, i->founder, 100, 0, {}, 0});
}
void IntegratedEconomy::RemoveIndustry(IndustryID id)
{
	if (enabled && factories.contains(id)) {
		if (auto i = Industry::GetIfValid(id)) {
			auto discard = [](const auto &cargo) {
				if (!IsValidCargoType(cargo.cargo)) return;
				Record(EconomyFlow::Discarded, cargo.cargo, cargo.waiting);
				if (_commonwealth_slice_audit != nullptr) _commonwealth_slice_audit->discarded[cargo.cargo] += cargo.waiting;
			};
			for (auto &cargo : i->accepted)
				discard(cargo);
			for (auto &cargo : i->produced)
				discard(cargo);
		}
	}
	factories.erase(id);
}
const std::map<IndustryID, EconomyFactory> &IntegratedEconomy::Factories()
{
	return factories;
}
void IntegratedEconomy::ChangeCompany(CompanyID old_owner, CompanyID new_owner)
{
	if (!enabled || old_owner == new_owner) return;
	for (auto &[id, f] : factories)
		if (f.owner == old_owner) {
			f.owner = Company::IsValidID(new_owner) ? new_owner : CompanyID{OWNER_NONE};
			if (auto industry = Industry::GetIfValid(id)) industry->founder = f.owner;
		}
	CancelResearch(old_owner);
	research.erase(old_owner);
	StockpileManager::ChangeCompany(old_owner, new_owner);
}
CommandCost IntegratedEconomy::CheckIndustry(TileIndex tile, IndustryType type, CompanyID company)
{
	if (!enabled) return CommandCost();
	auto role = Role(PlanetManager::GetTileWorld(tile));
	auto recipe = IndustryRecipe(type);
	bool primary = ResourceSiteManager::IsPrimary(type);
	bool valid = primary ? role == EconomicRole::Frontier : recipe != RECIPE_NONE && role == EconomicRole::Industrial;
	if (recipe == RECIPE_QUANTUM_ENRICHMENT) valid = role == EconomicRole::Frontier;
	if (recipe == RECIPE_CONSUMER_CRYSTAL_FORMAT) valid = role == EconomicRole::Core;
	if (!valid) return CommandCost(STR_ERROR_PRODUCTION_WORLD);
	if (!_generating_world && Company::IsValidID(company)) {
		auto tech = primary ? ResourceSiteManager::RequiredTech(type) : RecipeTech(recipe);
		if (tech != TECH_NONE && !TechTreeManager::IsTechUnlocked(company, tech)) return CommandCost(STR_ERROR_COMMONWEALTH_RESEARCH);
		BillOfMaterials bill;
		bill.materials = Materials(type);
		if (!bill.IsEmpty()) {
			auto materials = FabricationManager::CheckMaterials(PlanetManager::GetTileWorld(tile), company, bill);
			if (materials.Failed()) return materials;
		}
	}
	return CommandCost();
}
void IntegratedEconomy::ConsumeIndustryMaterials(TileIndex tile, IndustryType type, CompanyID company)
{
	if (enabled && !_generating_world && Company::IsValidID(company))
		StockpileManager::ConsumeBOM(PlanetManager::GetTileWorld(tile), company, Materials(type));
}
void IntegratedEconomy::ConfigureRecipes()
{
	if (!enabled) return;
	auto replace = [](RecipeID id, std::vector<std::pair<CargoType, uint32_t>> inputs,
					  std::vector<std::pair<CargoType, uint32_t>> outputs) {
		auto p = ProductionChainManager::GetRecipe(id);
		if (p == nullptr) return;
		auto r = *p;
		r.inputs = std::move(inputs);
		r.outputs = std::move(outputs);
		ProductionChainManager::RegisterRecipe(r);
	};
	replace(RECIPE_BALLAST_CRUSHING, {{Cargo("SILC"), 2}}, {{Cargo("BALL"), 2}});
	replace(RECIPE_STEEL_SMELTING, {{Cargo("IRON"), 2}}, {{Cargo("STEL"), 1}});
	replace(RECIPE_COPPER_SMELTING, {{Cargo("COPR"), 2}}, {{Cargo("WIRE"), 2}});
	replace(RECIPE_MACHINE_MODULES, {{Cargo("STEL"), 2}, {Cargo("WIRE"), 1}, {Cargo("CHIP"), 1}}, {{Cargo("MACH"), 1}});
	replace(RECIPE_SUPERALLOY_FOUNDRY, {{Cargo("STEL"), 2}, {Cargo("RARE"), 1}}, {{Cargo("ALLO"), 2}});
	replace(RECIPE_MONOCRYSTAL_SYNTHESIS, {{Cargo("SAND"), 2}, {Cargo("RARE"), 1}}, {{Cargo("BCRY"), 2}});
	replace(RECIPE_QUANTUM_ENRICHMENT, {{Cargo("BCRY"), 2}}, {{Cargo("QCRY"), 2}});
	replace(RECIPE_CONSUMER_CRYSTAL_FORMAT, {{Cargo("BCRY"), 2}}, {{Cargo("CCRY"), 2}});
	replace(RECIPE_SILICON_ARC, {{Cargo("SAND"), 2}, {Cargo("WIRE"), 1}}, {{Cargo("CHIP"), 1}});
	replace(RECIPE_SIGNALLING_ASSEMBLY, {{Cargo("CHIP"), 1}, {Cargo("WIRE"), 1}}, {{Cargo("SIGE"), 2}});
	replace(RECIPE_POLYMER_SYNTHESIS, {{Cargo("OIL_"), 2}}, {{Cargo("POLY"), 2}});
	replace(RECIPE_MAGLEV_WORKS, {{Cargo("ALLO"), 2}, {Cargo("WIRE"), 2}}, {{Cargo("MGLA"), 2}});
	ProductionChainManager::RegisterRecipe({.id = 404,
											.pipeline = PipelineType::Structural,
											.name = "Food Processing",
											.description = "Grain to food",
											.inputs = {{Cargo("GRAI"), 2}},
											.outputs = {{Cargo("FOOD"), 2}},
											.allowed_phases = {WorldPhase::Phase2_Developed}});
}
uint32_t IntegratedEconomy::IndustrySpace(const Industry *i, CargoType cargo)
{
	if (!Managed(i)) return 0;
	auto f = factories.find(i->index);
	if (f == factories.end()) return 0;
	auto recipe = ProductionChainManager::GetRecipe(IndustryRecipe(i->type));
	if (!recipe) return 0;
	for (auto [c, n] : recipe->inputs)
		if (c == cargo) {
			auto it = i->GetCargoAccepted(cargo);
			if (it == i->accepted.end()) return 0;
			uint32_t cap = std::min(65535u, f->second.capacity * n * 3);
			return cap > it->waiting ? cap - it->waiting : 0;
		}
	return 0;
}
uint32_t IntegratedEconomy::AcceptIndustry(Industry *i, CargoType cargo, uint32_t amount)
{
	amount = std::min(amount, IndustrySpace(i, cargo));
	if (amount != 0) i->GetCargoAccepted(cargo)->waiting += amount;
	return amount;
}
uint32_t IntegratedEconomy::OutputBatchLimit(const Industry *industry)
{
	if (!Managed(industry)) return 0;
	auto it = factories.find(industry->index);
	if (it == factories.end()) return 0;
	const auto &factory = it->second;
	const auto recipe = ProductionChainManager::GetRecipe(IndustryRecipe(industry->type));
	if (recipe == nullptr) return 0;
	uint32_t batches = factory.capacity;
	uint32_t yield = TechTreeManager::IsTechUnlocked(factory.owner, TECH_MATERIALS_3) ? 115 : 100;
	for (auto [cargo, units] : recipe->outputs) {
		auto output = industry->GetCargoProduced(cargo);
		if (output == industry->produced.end()) return 0;
		uint32_t cap = std::min(65535u, (factory.capacity * units * 3 * yield + 99) / 100);
		uint32_t space = cap > output->waiting ? cap - output->waiting : 0;
		uint32_t remainder = factory.remainder.contains(cargo) ? factory.remainder.at(cargo) : 0;
		uint64_t numerator = uint64_t(space) * 100 + 99;
		uint32_t possible = numerator < remainder ? 0 : (numerator - remainder) / (units * yield);
		batches = std::min(batches, possible);
	}
	return batches;
}
void IntegratedEconomy::Produce()
{
	if (!enabled) return;
	for (auto &[id, f] : factories) {
		auto i = Industry::GetIfValid(id);
		if (i == nullptr) continue;
		auto r = ProductionChainManager::GetRecipe(IndustryRecipe(i->type));
		if (!r) continue;
		auto tech = RecipeTech(r->id);
		if (Company::IsValidID(f.owner) && tech != TECH_NONE && !TechTreeManager::IsTechUnlocked(f.owner, tech)) {
			f.last_batches = 0;
			continue;
		}
		uint32_t batches = OutputBatchLimit(i);
		uint32_t yield = TechTreeManager::IsTechUnlocked(f.owner, TECH_MATERIALS_3) ? 115 : 100;
		for (auto [c, n] : r->inputs) {
			auto a = i->GetCargoAccepted(c);
			batches = std::min(batches, a == i->accepted.end() ? 0u : a->waiting / n);
		}
		f.last_batches = batches;
		f.total_batches += batches;
		if (batches == 0) continue;
		for (auto [c, n] : r->inputs) {
			i->GetCargoAccepted(c)->waiting -= batches * n;
			Record(EconomyFlow::Consumed, c, batches * n);
			if (_commonwealth_slice_audit != nullptr) {
				_commonwealth_slice_audit->consumed[c] += batches * n;
				_commonwealth_slice_audit->processor_cargo.push_back({TimerGameCalendar::date.base(), id.base(), c, -int64_t(batches * n)});
			}
		}
		for (auto [c, n] : r->outputs) {
			uint32_t units = batches * n * yield + f.remainder[c];
			f.remainder[c] = units % 100;
			i->GetCargoProduced(c)->waiting += units / 100;
			Record(EconomyFlow::Produced, c, units / 100);
			i->GetCargoProduced(c)->history[THIS_MONTH].production += units / 100;
			if (_commonwealth_slice_audit != nullptr) {
				_commonwealth_slice_audit->produced[c] += units / 100;
				_commonwealth_slice_audit->processor_cargo.push_back({TimerGameCalendar::date.base(), id.base(), c, int64_t(units / 100)});
			}
		}
	}
}
std::map<CargoType, uint32_t> IntegratedEconomy::CityDemand(TownID town)
{
	auto t = Town::GetIfValid(town);
	if (!enabled || t == nullptr || Role(PlanetManager::GetTileWorld(t->xy)) != EconomicRole::Core) return {};
	auto pop = t->cache.population;
	uint32_t food = std::max(50u, (pop + 19) / 20), build = std::max(20u, (pop + 99) / 100), luxury = std::max(10u, (pop + 199) / 200);
	return {{Cargo("FOOD"), food}, {Cargo("STEL"), build}, {Cargo("BALL"), build}, {Cargo("CHIP"), luxury}, {Cargo("CCRY"), luxury}};
}
std::string IntegratedEconomy::CityStatus(TownID town)
{
	auto demand = CityDemand(town);
	if (demand.empty()) return {};
	auto city = City(town);
	for (const char *label : {"FOOD", "STEL", "BALL", "CHIP", "CCRY"}) {
		auto cargo = Cargo(label);
		uint32_t reserve = city && city->reserves.contains(cargo) ? city->reserves.at(cargo) : 0;
		if (reserve < demand.at(cargo))
			return fmt::format("Next month needs {} more {}.", demand.at(cargo) - reserve, GetString(CargoSpec::Get(cargo)->name));
	}
	return "All city baskets ready for next month.";
}
const EconomyCity *IntegratedEconomy::City(TownID town)
{
	auto it = cities.find(town);
	return it == cities.end() ? nullptr : &it->second;
}
uint32_t IntegratedEconomy::AcceptCity(TownID town, CargoType cargo, uint32_t amount, bool execute)
{
	auto demand = CityDemand(town);
	auto d = demand.find(cargo);
	if (d == demand.end()) return 0;
	uint32_t cap = d->second * (cargo == Cargo("FOOD") ? 3 : 1), stock = 0;
	if (auto city = City(town))
		if (auto it = city->reserves.find(cargo); it != city->reserves.end()) stock = it->second;
	auto accepted = std::min(amount, cap > stock ? cap - stock : 0);
	if (execute && accepted) cities[town].reserves[cargo] += accepted;
	return accepted;
}
bool IntegratedEconomy::EvaluateCity(TownID town, float &growth, float &passengers)
{
	auto demand = CityDemand(town);
	if (demand.empty()) return false;
	auto &city = cities[town];
	/* Observe at the real evaluation, rather than infer an atomic basket from later reserves. */
	auto before = _integrated_city_month_audit != nullptr ? city.reserves : std::map<CargoType, uint32_t>{};
	city.consumed.clear();
	growth = 0;
	passengers = 0.5f;
	auto consume = [&](std::initializer_list<const char *> labels) {
		for (auto label : labels) {
			auto c = Cargo(label);
			if (city.reserves[c] < demand[c]) return false;
		}
		for (auto label : labels) {
			auto c = Cargo(label);
			city.reserves[c] -= demand[c];
			city.consumed[c] = demand[c];
			Record(EconomyFlow::Consumed, c, demand[c]);
			if (_commonwealth_slice_audit != nullptr) {
				_commonwealth_slice_audit->consumed[c] += demand[c];
				if (c == Cargo("FOOD")) _commonwealth_slice_audit->city_months.push_back({TimerGameCalendar::date.base(), town.base(), demand[c], city.reserves[c]});
			}
		}
		return true;
	};
	bool food = consume({"FOOD"});
	bool construction = false;
	if (food) {
		passengers = 1;
		construction = consume({"STEL", "BALL"});
		if (construction) {
			growth = 1;
			if (consume({"CHIP", "CCRY"})) {
				growth = 2;
				passengers = 1.5f;
			}
		}
	}
	if (_integrated_city_month_audit != nullptr) {
		const Town *native = Town::Get(town);
		_integrated_city_month_audit->push_back({{"tick", TimerGameTick::counter}, {"date", TimerGameCalendar::date.base()},
			{"town", town.base()}, {"population", native->cache.population}, {"house_count", native->cache.num_houses},
			{"demand", demand}, {"before", before}, {"after", city.reserves}, {"consumed", city.consumed},
			{"food_sufficient", food}, {"construction_complete", construction}, {"growth", growth}, {"passengers", passengers},
			{"native_growth_observed", false}});
	}
	return true;
}
void IntegratedEconomy::ObserveMonthlyGrowth(TownID town)
{
	if (_integrated_city_month_audit == nullptr) return;
	const Town *native = Town::GetIfValid(town);
	if (native == nullptr) return;
	for (auto it = _integrated_city_month_audit->rbegin(); it != _integrated_city_month_audit->rend(); ++it) {
		if ((*it)["tick"] != TimerGameTick::counter) break;
		if ((*it)["town"] != town.base()) continue;
		(*it)["native_growth_observed"] = true;
		(*it)["native_growth_rate"] = native->growth_rate;
		(*it)["native_grow_counter"] = native->grow_counter;
		(*it)["native_growth_enabled"] = native->flags.Test(TownFlag::IsGrowing) && native->growth_rate != TOWN_GROWTH_RATE_NONE;
		(*it)["native_growth_hook_enabled"] = !native->flags.Test(TownFlag::CustomGrowth) && native->growth_rate != TOWN_GROWTH_RATE_NONE;
		(*it)["native_flags"] = native->flags.base();
		break;
	}
}
std::map<CargoType, uint32_t> IntegratedEconomy::ResearchKit(TechID tech)
{
	if (!enabled) return {};
	auto n = TechTreeManager::GetNode(tech);
	if (!n || n->tier < 3) return {};
	if (n->tier == 3) return {{Cargo("STEL"), 40}, {Cargo("CHIP"), 20}};
	return {{Cargo("CHIP"), 40}, {Cargo(tech == TECH_MATERIALS_4 ? "BCRY" : "QCRY"), 20}};
}
bool IntegratedEconomy::PrepareResearch(CompanyID company, TechID project)
{
	if (!enabled) return true;
	auto &r = research[company];
	if (r.kit_project == project) return true;
	if (r.kit_project != TECH_NONE) CancelResearch(company);
	auto hq = CorporateHQManager::GetHQ(company);
	if (!hq) return false;
	auto kit = ResearchKit(project);
	if (!kit.empty() && !StockpileManager::ConsumeBOM(hq->world_id, company, kit, false)) return false;
	r.kit_project = project;
	r.kit_world = hq->world_id;
	r.reserved = kit;
	return true;
}
void IntegratedEconomy::FinishResearch(CompanyID company)
{
	if (!enabled) return;
	auto &r = research[company];
	for (auto [cargo, units] : r.reserved)
		Record(EconomyFlow::Consumed, cargo, units);
	if (_commonwealth_slice_audit != nullptr)
		for (auto [cargo, units] : r.reserved) {
			_commonwealth_slice_audit->consumed[cargo] += units;
			_commonwealth_slice_audit->research_consumed[cargo] += units;
		}
	r.reserved.clear();
	r.kit_project = TECH_NONE;
	r.kit_world = INVALID_WORLD;
}
void IntegratedEconomy::CancelResearch(CompanyID company)
{
	auto it = research.find(company);
	if (it == research.end()) return;
	for (auto [c, n] : it->second.reserved)
		StockpileManager::AddCargo(it->second.kit_world, company, c, n);
	it->second.reserved.clear();
	it->second.kit_project = TECH_NONE;
	it->second.kit_world = INVALID_WORLD;
}
bool IntegratedEconomy::Accelerates(CompanyID company)
{
	auto r = Research(company);
	return !enabled || (r != nullptr && r->acceleration);
}
void IntegratedEconomy::SetAcceleration(CompanyID company, bool value)
{
	research[company].acceleration = value;
}
const EconomyResearch *IntegratedEconomy::Research(CompanyID company)
{
	auto it = research.find(company);
	return it == research.end() ? nullptr : &it->second;
}
std::string IntegratedEconomy::IndustryDescription(IndustryType type)
{
	if (!enabled || type >= NUM_INDUSTRYTYPES) return {};
	auto recipe = IndustryRecipe(type);
	auto r = ProductionChainManager::GetRecipe(recipe);
	auto role = ResourceSiteManager::IsPrimary(type) || recipe == RECIPE_QUANTUM_ENRICHMENT ? "Frontier"
				: recipe == RECIPE_CONSUMER_CRYSTAL_FORMAT									? "Core"
																							: "Industrial";
	std::string text = fmt::format("Economic role: {}. ", role);
	auto tech = ResourceSiteManager::IsPrimary(type) ? ResourceSiteManager::RequiredTech(type) : RecipeTech(recipe);
	if (auto node = TechTreeManager::GetNode(tech)) text += fmt::format("Research: {}. ", node->name);
	auto materials = Materials(type);
	if (!materials.empty()) {
		text += "Construction materials: ";
		for (auto [cargo, units] : materials)
			text += fmt::format("{} {} ", units, GetString(CargoSpec::Get(cargo)->name));
	}
	if (r != nullptr) {
		text += "\n" + r->name + ": ";
		for (auto [cargo, units] : r->inputs)
			text += fmt::format("{} {} ", units, GetString(CargoSpec::Get(cargo)->name));
		text += " -> ";
		for (auto [cargo, units] : r->outputs)
			text += fmt::format("{} {} ", units, GetString(CargoSpec::Get(cargo)->name));
	}
	return text;
}
std::string IntegratedEconomy::Save()
{
	using nlohmann::json;
	json j = {{"version", enabled ? VERSION : 0},
			  {"roles", json::array()},
			  {"factories", json::array()},
			  {"cities", json::array()},
			  {"research", json::array()}};
	for (auto [w, r] : roles)
		j["roles"].push_back({w.base(), uint8_t(r)});
	for (auto &[id, f] : factories) {
		json rem = json::array();
		for (auto [c, n] : f.remainder)
			rem.push_back({c, n});
		j["factories"].push_back({id.base(), f.owner.base(), f.capacity, f.last_batches, rem, f.total_batches});
	}
	for (auto &[id, c] : cities) {
		json stock = json::array(), used = json::array();
		for (auto [k, n] : c.reserves)
			stock.push_back({k, n});
		for (auto [k, n] : c.consumed)
			used.push_back({k, n});
		j["cities"].push_back({id.base(), stock, used});
	}
	for (auto &[id, r] : research) {
		json stock = json::array();
		for (auto [c, n] : r.reserved)
			stock.push_back({c, n});
		j["research"].push_back({id.base(), r.kit_project, r.kit_world.base(), r.acceleration, stock});
	}
	j["shared_research"] = shared_research;
	j["accounting"] = accounting;
	return j.dump();
}
bool IntegratedEconomy::Load(const std::string &data)
{
	try {
		const auto j = nlohmann::json::parse(data);
		auto number = [](const nlohmann::json &value, uint64_t maximum) -> uint64_t {
			if (!value.is_number_unsigned() || value.get<uint64_t>() > maximum) throw std::runtime_error("Invalid economy integer");
			return value.get<uint64_t>();
		};
		const auto version = number(j.at("version"), VERSION);
		EconomyAccounting loaded_accounting{};
		if (j.contains("accounting")) {
			const auto &rows = j.at("accounting");
			if (!rows.is_array() || rows.size() != loaded_accounting.size()) return false;
			for (size_t flow = 0; flow < rows.size(); ++flow) {
				if (!rows[flow].is_array() || rows[flow].size() != NUM_CARGO) return false;
				for (size_t cargo = 0; cargo < NUM_CARGO; ++cargo)
					loaded_accounting[flow][cargo] = number(rows[flow][cargo], UINT64_MAX);
			}
		}
		decltype(roles) loaded_roles;
		decltype(factories) loaded_factories;
		decltype(cities) loaded_cities;
		decltype(research) loaded_research;
		auto cargo_map = [&](const nlohmann::json &rows, uint32_t maximum) {
			std::map<CargoType, uint32_t> result;
			if (!rows.is_array() || rows.size() > NUM_CARGO) throw std::runtime_error("Invalid economy cargo map");
			for (const auto &row : rows) {
				if (!row.is_array() || row.size() != 2) throw std::runtime_error("Invalid economy cargo row");
				/* number() rejects values above the uint32_t maximum before conversion. */
				if (!result.emplace(CargoType(number(row.at(0), NUM_CARGO - 1)), static_cast<uint32_t>(number(row.at(1), maximum))).second)
					throw std::runtime_error("Duplicate economy cargo");
			}
			return result;
		};
		for (const auto &v : j.at("roles")) {
			if (v.size() != 2 ||
				!loaded_roles.emplace(WorldID{static_cast<uint32_t>(number(v.at(0), UINT32_MAX - 1))}, EconomicRole(number(v.at(1), 2)))
					 .second)
				return false;
		}
		for (const auto &v : j.at("factories")) {
			if (v.size() != 6) return false;
			EconomyFactory f{IndustryID{static_cast<uint16_t>(number(v.at(0), UINT16_MAX - 1))},
							 CompanyID{static_cast<uint8_t>(number(v.at(1), UINT8_MAX))},
							 static_cast<uint32_t>(number(v.at(2), 1000)),
							 static_cast<uint32_t>(number(v.at(3), 1000)),
							 {},
							 0};
			if (f.capacity < 100 || f.capacity % 50 != 0 || f.last_batches > f.capacity) return false;
			f.remainder = cargo_map(v.at(4), 99);
			f.total_batches = number(v.at(5), UINT64_MAX);
			if (!loaded_factories.emplace(f.industry, f).second) return false;
		}
		for (const auto &v : j.at("cities")) {
			if (v.size() != 3) return false;
			EconomyCity c{cargo_map(v.at(1), UINT32_MAX), cargo_map(v.at(2), UINT32_MAX)};
			if (!loaded_cities.emplace(TownID{static_cast<uint16_t>(number(v.at(0), UINT16_MAX - 1))}, c).second) return false;
		}
		for (const auto &v : j.at("research")) {
			if (v.size() != 5 || !v.at(3).is_boolean()) return false;
			EconomyResearch r{static_cast<TechID>(number(v.at(1), UINT16_MAX)),
							  WorldID{static_cast<uint32_t>(number(v.at(2), UINT32_MAX))},
							  {},
							  v.at(3).get<bool>()};
			if (r.kit_project != TECH_NONE && TechTreeManager::GetNode(r.kit_project) == nullptr) return false;
			r.reserved = cargo_map(v.at(4), 40);
			if (r.kit_project == TECH_NONE && !r.reserved.empty()) return false;
			if (!loaded_research.emplace(CompanyID{static_cast<uint8_t>(number(v.at(0), MAX_COMPANIES - 1))}, r).second) return false;
		}
		auto loaded_shared = j.value("shared_research", std::map<std::string, nlohmann::json>{});
		for (const auto &[key, row] : loaded_shared) {
			if (key != row.at("id").get<std::string>() || row.at("home") != row.at("namespace") ||
				!UniverseNetwork::ParseNamespace(row.at("home")).IsValid() || row.at("ruleset") != VERSION)
				return false;
			auto unlocks = row.at("unlocks").get<std::set<TechID>>();
			if (unlocks.size() != row.at("unlocks").size() || number(row.at("revision"), 12) != unlocks.size()) return false;
			for (auto tech : unlocks)
				if (!TechTreeManager::GetNode(tech)) return false;
		}
		if (version == 0 && (!loaded_roles.empty() || !loaded_factories.empty() || !loaded_cities.empty() || !loaded_research.empty() ||
							 !loaded_shared.empty()))
			return false;
		enabled = version == VERSION;
		accounting = loaded_accounting;
		roles = std::move(loaded_roles);
		factories = std::move(loaded_factories);
		cities = std::move(loaded_cities);
		research = std::move(loaded_research);
		shared_research = std::move(loaded_shared);
		ProductionChainManager::InitDefaultRecipes();
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
bool IntegratedEconomy::ValidateAfterLoad()
{
	if (!enabled) return true;
	if (!ContentReady()) return false;
	for (const auto &world : PlanetManager::GetAllRegions())
		if (!roles.contains(world.id)) return false;
	for (const auto &[world, role] : roles)
		if (PlanetManager::GetRegion(world) == nullptr) return false;
	std::erase_if(factories, [](auto &p) { return !Industry::IsValidID(p.first); });
	std::erase_if(cities, [](auto &p) { return !Town::IsValidID(p.first); });
	std::erase_if(research, [](auto &p) { return !Company::IsValidID(p.first); });
	for (const auto &[id, f] : factories) {
		auto industry = Industry::Get(id);
		if (!Managed(industry) || f.owner != industry->founder ||
			(f.owner != OWNER_NONE && !Company::IsValidID(f.owner))) return false;
		auto recipe = ProductionChainManager::GetRecipe(IndustryRecipe(industry->type));
		for (const auto &[cargo, remainder] : f.remainder)
			if (std::ranges::none_of(recipe->outputs, [cargo](const auto &output) { return output.first == cargo; })) return false;
	}
	for (const auto &[company, r] : research) {
		if (r.kit_project == TECH_NONE) continue;
		auto hq = CorporateHQManager::GetHQ(company);
		if (!hq || hq->world_id != r.kit_world || TechTreeManager::GetActiveProject(company) != r.kit_project ||
			r.reserved != ResearchKit(r.kit_project))
			return false;
	}
	for (auto i : Industry::Iterate())
		RegisterIndustry(i);
	ProductionChainManager::InitDefaultRecipes();
	return true;
}

std::string IntegratedEconomy::ResearchHome(CompanyID company)
{
	auto it = shared_research.find(Identity(company));
	return it == shared_research.end() ? "" : it->second.at("home").get<std::string>();
}
bool IntegratedEconomy::CanConductResearch(CompanyID company)
{
	if (!enabled) return true;
	auto home = ResearchHome(company);
	if (!home.empty()) return home == UniverseNetwork::Namespace(FederationIdentityRegistry::GetNamespace());
	if (FederationTransferManager::HasExternalAuthority()) return false;
	return std::ranges::none_of(UniverseNetwork::Directory(),
								[](const auto &entry) { return entry.second.value("ruleset", 0u) == VERSION; });
}
bool IntegratedEconomy::SharedUnlock(CompanyID company, TechID tech)
{
	if (!enabled) return false;
	auto it = shared_research.find(Identity(company));
	if (it == shared_research.end()) return false;
	const auto &unlocks = it->second.at("unlocks");
	return std::find(unlocks.begin(), unlocks.end(), tech) != unlocks.end();
}
nlohmann::json IntegratedEconomy::ResearchAdvertisements()
{
	using nlohmann::json;
	auto out = json::array();
	if (!enabled) return out;
	auto ns = UniverseNetwork::Namespace(FederationIdentityRegistry::GetNamespace());
	for (auto company : Company::Iterate()) {
		auto identity = Identity(company->index);
		auto home = ResearchHome(company->index);
		if (identity.empty() || (home.empty() && !CorporateHQManager::HasHQ(company->index)) || (!home.empty() && home != ns)) continue;
		std::set<TechID> unlocks;
		for (auto &state : TechTreeManager::GetAllCompanyTechStates())
			if (state.company_id == company->index) unlocks = state.unlocked_techs;
		out.push_back({{"id", identity}, {"home", ns}, {"revision", unlocks.size()}, {"unlocks", unlocks}});
	}
	return out;
}
bool IntegratedEconomy::ApplyResearch(const nlohmann::json &record, bool execute)
{
	try {
		if (!enabled || record.value("ruleset", 0u) != VERSION) return false;
		std::string identity = record.at("id"), home = record.at("home");
		if (!UniverseNetwork::ParseNamespace(home).IsValid() || home != record.at("namespace").get<std::string>()) return false;
		auto unlocks = record.at("unlocks").get<std::set<TechID>>();
		if (!record.at("unlocks").is_array() || unlocks.size() != record.at("unlocks").size() || unlocks.size() > 12 ||
			!record.at("revision").is_number_integer() || record.at("revision").get<int64_t>() != static_cast<int64_t>(unlocks.size())) return false;
		for (const auto &value : record.at("unlocks"))
			if (!value.is_number_integer() || value.get<int64_t>() < 0 || value.get<int64_t>() > UINT16_MAX) return false;
		for (auto tech : unlocks)
			if (!TechTreeManager::GetNode(tech)) return false;
		auto old = shared_research.find(identity);
		if (old != shared_research.end()) {
			if (old->second.at("home") != home) return false;
			auto previous = old->second.at("unlocks").get<std::set<TechID>>();
			if (!std::includes(unlocks.begin(), unlocks.end(), previous.begin(), previous.end())) return false;
		}
		if (execute) shared_research[identity] = record;
		return true;
	} catch (...) {
		return false;
	}
}

CommandCost CmdSetResearchAcceleration(DoCommandFlags flags, bool value)
{
	if (!IntegratedEconomy::Enabled() || !Company::IsValidID(_current_company) || !IntegratedEconomy::CanConductResearch(_current_company))
		return CMD_ERROR;
	if (flags.Test(DoCommandFlag::Execute)) IntegratedEconomy::SetAcceleration(_current_company, value);
	return CommandCost();
}
CommandCost CmdManageEconomyFactory(DoCommandFlags flags, IndustryID industry, bool retire)
{
	auto it = factories.find(industry);
	auto ind = Industry::GetIfValid(industry);
	if (!enabled || ind == nullptr || it == factories.end() || it->second.owner != _current_company ||
		!Company::IsValidID(_current_company))
		return CMD_ERROR;
	if (!retire && it->second.capacity >= 1000) return CommandCost(STR_ERROR_FACILITY_MAX_CAPACITY);
	Money price = retire ? GetIndustrySpec(ind->type)->GetRemovalCost() : Money{50000};
	if (flags.Test(DoCommandFlag::Execute)) {
		if (retire)
			delete ind;
		else
			it->second.capacity = std::min(1000u, it->second.capacity + 50);
	}
	return CommandCost(ExpensesType::Construction, price);
}
