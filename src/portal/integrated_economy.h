/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file integrated_economy.h Versioned economic roles and physical production. */
#ifndef INTEGRATED_ECONOMY_H
#define INTEGRATED_ECONOMY_H
#include "../3rdparty/nlohmann/json.hpp"
#include "../command_type.h"
#include "../industry_type.h"
#include "../town_type.h"
#include "planet_type.h"
#include "production_chain.h"
#include "tech_tree.h"

struct Industry;
struct Station;
enum class EconomicRole : uint8_t { Frontier, Industrial, Core };
enum class EconomyRulesVersion : uint8_t { Legacy = 0, V1 = 1, V2 = 2 };
enum class FactoryWorkType : uint8_t { None = 0, Capacity = 1, Reserve = 2 };
/** Cumulative physical events; transport/storage are movements, not consumption. */
enum class EconomyFlow : uint8_t { Produced, Transported, Stored, Consumed, Discarded };
using EconomyAccounting = std::array<std::array<uint64_t, NUM_CARGO>, 5>;
struct EconomyFactory {
	IndustryID industry = IndustryID::Invalid();
	CompanyID owner = CompanyID::Invalid();
	uint32_t capacity = 100;
	uint32_t last_batches = 0;
	std::map<CargoType, uint32_t> remainder;
	uint64_t total_batches = 0;
	FactoryWorkType work = FactoryWorkType::None;
	uint32_t input_capacity = 300;
	uint32_t output_capacity = 300;
};
struct EconomyCity {
	std::map<CargoType, uint32_t> reserves;
	std::map<CargoType, uint32_t> consumed;
};
struct TownFoodService {
	CompanyID owner = CompanyID::Invalid();
	uint32_t selected_rate = 0; // Off = 0, 10, 20, 40
	uint32_t market_ceiling = 40;
	uint32_t receiver_capacity = 60;
	std::array<uint32_t, 4> consumed_history{0, 0, 0, 0};
	std::array<uint32_t, 4> requested_history{0, 0, 0, 0};
	uint8_t history_cursor = 0;
	uint32_t fractional_passenger_remainder = 0;
	uint32_t delivered_this_period = 0;
	uint32_t delivered_last_period = 0;
};
struct TownResidentialDistrict {
	CompanyID owner = CompanyID::Invalid();
	bool commissioned = false;
	uint8_t building_opportunities = 0;
	uint8_t buildings_placed = 0;
	uint8_t buildings_completed = 0;
	bool site_blocked = false;
};
struct EconomyResearch {
	TechID kit_project = TECH_NONE;
	WorldID kit_world = INVALID_WORLD;
	std::map<CargoType, uint32_t> reserved;
	bool acceleration = false;
};
/** Transient read-only monthly basket evidence; never serialized or used by simulation. */
extern std::vector<nlohmann::json> *_integrated_city_month_audit;
class IntegratedEconomy
{
  public:
	static constexpr uint32_t VERSION = 1;
	static constexpr uint32_t FORMAT_VERSION = 2;
	static void Reset();
	static EconomyRulesVersion ActiveRules();
	static void SetRulesVersion(EconomyRulesVersion rules);
	static void Record(EconomyFlow flow, CargoType cargo, uint32_t units);
	static const EconomyAccounting &Accounting();
	static void RestoreAccounting(const EconomyAccounting &value);
	static bool Enabled();
	static bool ContentReady();
	static bool StartNewGame();
	/** Activate v2 only for a staged, populated, one-region Aster Vale profile. */
	static bool StartAuthoredWorld0(const std::array<TownID, 3> &towns);
	static void SetEnabled(bool enabled);
	static EconomicRole Role(WorldID world);
	static void RegisterRole(WorldID world, EconomicRole role);
	static const char *RoleName(WorldID world);
	static RecipeID IndustryRecipe(IndustryType type);
	static TechID RecipeTech(RecipeID recipe);
	static bool Managed(const Industry *industry);
	static void RegisterIndustry(Industry *industry);
	static void RemoveIndustry(IndustryID industry);
	static const std::map<IndustryID, EconomyFactory> &Factories();
	static void ChangeCompany(CompanyID old_owner, CompanyID new_owner);
	static CommandCost CheckIndustry(TileIndex tile, IndustryType type, CompanyID company);
	static void ConsumeIndustryMaterials(TileIndex tile, IndustryType type, CompanyID company);
	static uint32_t AcceptIndustry(Industry *industry, CargoType cargo, uint32_t amount);
	static uint32_t IndustrySpace(const Industry *industry, CargoType cargo);
	static uint32_t OutputBatchLimit(const Industry *industry);
	static void Produce();
	static void ConfigureRecipes();
	static std::map<CargoType, uint32_t> CityDemand(TownID town);
	static std::string CityStatus(TownID town);
	static const EconomyCity *City(TownID town);
	static bool IsManagedCityCargo(TownID town, CargoType cargo);
	static TownFoodService *GetTownFoodService(TownID town);
	static const std::map<TownID, TownFoodService> &TownFoodServices();
	static TownResidentialDistrict *GetTownResidentialDistrict(TownID town);
	static const std::map<TownID, TownResidentialDistrict> &TownResidentialDistricts();
	static bool TryGrowResidentialDistrict(TownID town);
	static void OnResidentialHouseCompleted(TownID town);
	static bool StartNewWorksPrototype();
	static uint32_t AcceptCity(TownID town, CargoType cargo, uint32_t amount, bool execute);
	static bool EvaluateCity(TownID town, float &growth, float &passengers);
	/**
	 * Finish an observed monthly evaluation after the native town growth hook runs.
	 * @param town Town whose native monthly growth state was just updated.
	 */
	static void ObserveMonthlyGrowth(TownID town);
	static std::map<CargoType, uint32_t> ResearchKit(TechID tech);
	static bool PrepareResearch(CompanyID company, TechID project);
	static void FinishResearch(CompanyID company);
	static void CancelResearch(CompanyID company);
	static bool CanConductResearch(CompanyID company);
	static bool SharedUnlock(CompanyID company, TechID tech);
	static nlohmann::json ResearchAdvertisements();
	static bool ApplyResearch(const nlohmann::json &record, bool execute);
	static std::string ResearchHome(CompanyID company);
	static bool Accelerates(CompanyID company);
	static void SetAcceleration(CompanyID company, bool enabled);
	static const EconomyResearch *Research(CompanyID company);
	static std::string IndustryDescription(IndustryType type);
	static std::string Save();
	static bool Load(const std::string &data);
	static bool ValidateAfterLoad();
};
#endif
