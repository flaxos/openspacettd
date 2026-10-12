/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file test_train_purchase_compatibility.cpp Train purchase railtype admission versus native build quotes. */

#include "../stdafx.h"
#include "../3rdparty/catch2/catch.hpp"
#include "../company_base.h"
#include "../depot_base.h"
#include "../engine_base.h"
#include "../engine_func.h"
#include "../engine_gui.h"
#include "../cargotype.h"
#include "../command_func.h"
#include "../map_func.h"
#include "../rail.h"
#include "../rail_cmd.h"
#include "../signal_func.h"
#include "../vehicle_cmd.h"
#include "../portal/planet_manager.h"
#include "../core/backup_type.hpp"

#include "../safeguards.h"

extern void SetupCommandAuthorityWorld(WorldPhase phase, uint32_t score);

TEST_CASE("Train purchase depot filter matches native wagon and locomotive rail admission", "[train-purchase][railtype]")
{
	SetupCommandAuthorityWorld(WorldPhase::Phase3_Frontier, 2000);
	struct DepotCleanup {
		~DepotCleanup() { UpdateSignalsInBuffer(); _depot_pool.CleanPool(); }
	} depot_cleanup;
	ResetRailTypes();
	/* The shared authority fixture clears map tiles but does not set their
	 * corner heights. Give the native depots flat, deterministic ground. */
	for (TileIndex tile{0}; tile < Map::Size(); ++tile) SetTileHeight(tile, 2);
	Company *company = Company::Get(CompanyID{0});
	company->money = 10000000;
	company->clear_limit = 1000 << 16;
	for (Engine *engine : Engine::Iterate()) {
		if (const CargoLabel *label = std::get_if<CargoLabel>(&engine->info.cargo_label)) {
			CargoType cargo = GetCargoTypeByLabel(*label);
			if (IsValidCargoType(cargo)) engine->info.cargo_type = cargo;
		}
	}
	StartupEngines();
	/* Startup recalculates available railtypes for the 1950 date. This isolated
	 * compatibility fixture needs both RAIL and ELRL native depot commands. */
	for (RailType railtype : {RAILTYPE_RAIL, RAILTYPE_ELECTRIC, RAILTYPE_MONO, RAILTYPE_MAGLEV}) company->avail_railtypes.Set(railtype);

	Engine *wagon = nullptr;
	Engine *steam = nullptr;
	for (Engine *engine : Engine::IterateType(VehicleType::Train)) {
		if (!IsEngineBuildable(engine->index, VehicleType::Train, CompanyID{0}) || !IsValidCargoType(engine->GetDefaultCargoType())) continue;
		const RailVehicleInfo &rvi = engine->VehInfo<RailVehicleInfo>();
		if (!rvi.railtypes.Test(RAILTYPE_RAIL)) continue;
		if (rvi.railveh_type == RailVehicleType::Wagon && wagon == nullptr) wagon = engine;
		if (rvi.railveh_type == RailVehicleType::Singlehead && rvi.engclass == EngineClass::Steam && steam == nullptr) steam = engine;
	}
	REQUIRE(wagon != nullptr);
	REQUIRE(steam != nullptr);

	const TileIndex rail_depot = TileXY(25, 30);
	const TileIndex electric_depot = TileXY(29, 30);
	const auto rail_quote = Command<Commands::BuildRailDepot>::Do({}, rail_depot, RAILTYPE_RAIL, DiagDirection::SW);
	INFO("RAIL depot quote error=" << rail_quote.GetErrorMessage().base() << " slope=" << uint(GetTileSlope(rail_depot)));
	REQUIRE(rail_quote.Succeeded());
	const auto rail_build = Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, rail_depot, RAILTYPE_RAIL, DiagDirection::SW);
	INFO("RAIL depot build error=" << rail_build.GetErrorMessage().base());
	REQUIRE(rail_build.Succeeded());
	const auto electric_quote = Command<Commands::BuildRailDepot>::Do({}, electric_depot, RAILTYPE_ELECTRIC, DiagDirection::SW);
	INFO("ELRL depot quote error=" << electric_quote.GetErrorMessage().base() << " slope=" << uint(GetTileSlope(electric_depot)));
	REQUIRE(electric_quote.Succeeded());
	const auto electric_build = Command<Commands::BuildRailDepot>::Do(DoCommandFlag::Execute, electric_depot, RAILTYPE_ELECTRIC, DiagDirection::SW);
	INFO("ELRL depot build error=" << electric_build.GetErrorMessage().base());
	REQUIRE(electric_build.Succeeded());

	auto check = [](Engine *engine, RailTypes railtypes, RailType depot_type, TileIndex depot, bool expected) {
		AutoRestoreBackup restore(engine->VehInfo<RailVehicleInfo>().railtypes, railtypes);
		CHECK(IsRailVehicleCompatibleWithDepot(engine->index, depot_type) == expected);
		auto [cost, vehicle, capacity, mail, cargo_capacities] = Command<Commands::BuildVehicle>::Do(
			DoCommandFlag::QueryCost, depot, engine->index, false, INVALID_CARGO, ClientID::Invalid);
		INFO("engine=" << engine->index.base() << " railtype=" << depot_type << " error=" << cost.GetErrorMessage().base());
		CHECK(cost.Succeeded() == expected);
	};

	/* Modular-like ELRL wagons can run on RAIL even though ELRL has no power there. */
	CHECK_FALSE(HasPowerOnRail(RailTypes{RAILTYPE_ELECTRIC}, RAILTYPE_RAIL));
	check(wagon, RailTypes{RAILTYPE_ELECTRIC}, RAILTYPE_RAIL, rail_depot, true);
	check(steam, RailTypes{RAILTYPE_ELECTRIC}, RAILTYPE_RAIL, rail_depot, false);
	check(steam, RailTypes{RAILTYPE_RAIL}, RAILTYPE_RAIL, rail_depot, true);
	check(steam, RailTypes{RAILTYPE_RAIL}, RAILTYPE_ELECTRIC, electric_depot, true);
	check(wagon, RailTypes{RAILTYPE_MONO}, RAILTYPE_RAIL, rail_depot, false);
	check(wagon, RailTypes{RAILTYPE_MAGLEV}, RAILTYPE_RAIL, rail_depot, false);
}
