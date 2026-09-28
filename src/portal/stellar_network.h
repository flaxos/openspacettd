/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file stellar_network.h Persistent stellar geography, landing zones and gate commissioning. */
#ifndef STELLAR_NETWORK_H
#define STELLAR_NETWORK_H

#include "../command_type.h"
#include "../station_type.h"
#include "../cargo_type.h"
#include "../vehicle_type.h"
#include "../rail_type.h"
#include "planet_type.h"
#include <map>
#include <set>
#include <string>
#include <vector>

/** Authored coordinates are game geography, independent of terrain coordinates. */
struct StellarWorld {
	WorldID world = INVALID_WORLD;
	std::string catalogue_id;
	std::string name;
	int32_t x = 0, y = 0;
	bool opened = false;
};
struct StellarLandingZone {
	uint32_t id = 0;
	WorldID world = INVALID_WORLD;
	TileIndex tile = INVALID_TILE;
	DiagDirection direction = DiagDirection::NE;
};
enum class GateProjectState : uint8_t { Supplying, Ready, Active, Cancelled };
struct GateProject {
	uint32_t id = 0;
	CompanyID owner = CompanyID::Invalid();
	TileIndex source = INVALID_TILE;
	uint32_t zone = 0;
	StationID supply_station = StationID::Invalid();
	uint32_t steel = 0, machines = 0, bands = 1;
	GateProjectState state = GateProjectState::Supplying;
};
struct StellarGatePolicy {
	CompanyID owner = CompanyID::Invalid();
	bool public_access = false;
	Money toll = 0;
};

class StellarNetwork {
public:
	static void Reset();
	static bool Enabled();
	static void RegisterWorld(StellarWorld world);
	static const std::map<WorldID, StellarWorld> &Worlds();
	static const StellarWorld *GetWorld(WorldID world);
	static void RegisterZone(StellarLandingZone zone);
	static const std::map<uint32_t, StellarLandingZone> &Zones();
	static const std::map<uint32_t, GateProject> &Projects();
	static uint32_t Range(CompanyID company);
	static uint64_t DistanceSquared(const StellarWorld &a, const StellarWorld &b);
	static uint32_t DistanceBands(const StellarWorld &a, const StellarWorld &b);
	static bool CanReach(CompanyID company, WorldID source, WorldID destination);
	static bool WorldAccessible(WorldID world);
	static bool InternalConstruction();
	static CommandCost CheckPlacement(TileIndex tile);
	static CommandCost Start(DoCommandFlags flags, TileIndex source, uint32_t zone, StationID station);
	static CommandCost Operate(DoCommandFlags flags, uint32_t project, bool cancel);
	static uint32_t ReceiveDelivery(StationID station, CompanyID owner, CargoType cargo, uint32_t amount);
	static CommandCost BuildRemoteLanding(DoCommandFlags flags, uint32_t zone, RailType rail, WorldID source_world, uint32_t source_gate);
	static void ConfigureRemoteAccess(TileIndex tile, CompanyID owner, bool public_access, Money toll);
	static CommandCost SetAccess(DoCommandFlags flags, TileIndex gate, bool public_access, Money toll);
	static const StellarGatePolicy *Policy(TileIndex gate);
	static void RegisterCSTGate(TileIndex gate);
	static bool CanTraverseTile(TileIndex tile, CompanyID company, bool ordinary_access = false);
	static bool CanUseGate(TileIndex gate, CompanyID company);
	static bool AdmitTrain(TileIndex gate, VehicleID train, CompanyID company);
	static void ReleaseTrain(VehicleID train);
	static void RemoveGate(TileIndex gate);
	static void RemoveSupplyStation(StationID station);
	static void ChangeCompany(CompanyID old_owner, CompanyID new_owner);
	static std::string Save();
	static bool Load(const std::string &data);
	static void ValidateAfterLoad();
};
CommandCost CmdStartGateProject(DoCommandFlags flags, TileIndex source, uint32_t zone, StationID station);
CommandCost CmdOperateGateProject(DoCommandFlags flags, uint32_t project, bool cancel);
CommandCost CmdSetStellarGateAccess(DoCommandFlags flags, TileIndex gate, bool public_access, Money toll);
DEF_CMD_TRAIT(Commands::StartGateProject, CmdStartGateProject, {}, CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::OperateGateProject, CmdOperateGateProject, {}, CommandType::LandscapeConstruction)
DEF_CMD_TRAIT(Commands::SetStellarGateAccess, CmdSetStellarGateAccess, {}, CommandType::OtherManagement)
void ShowStellarNetwork();
#endif
