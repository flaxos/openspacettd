/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file remote_gate_projects.h Durable source commitments and destination reservations for remote gate construction. */
#ifndef REMOTE_GATE_PROJECTS_H
#define REMOTE_GATE_PROJECTS_H
#include "stellar_network.h"
#include "../3rdparty/nlohmann/json.hpp"
class RemoteGateProjects {
public:
	static void Reset();
	static void RemoveSupplyStation(StationID station);
	static nlohmann::json Save();
	static bool Load(const nlohmann::json &state);
	static bool ValidateAfterLoad();
	static nlohmann::json Advertisements(bool replies);
	static void Reconcile(const nlohmann::json &record);
	static uint32_t ReceiveDelivery(StationID station, CompanyID owner, CargoType cargo, uint32_t amount);
	static bool Reserved(TileIndex tile);
	static bool SupplyReserved(StationID station);
	static bool OwnsGate(TileIndex tile);
	static bool IsArrivalGate(TileIndex tile);
	static CommandCost Operate(DoCommandFlags flags, TileIndex source, const std::string &key, uint8_t operation);
	static void UpdateAccess(TileIndex tile, bool public_access, Money toll);
};
CommandCost CmdRemoteGateProject(DoCommandFlags flags, TileIndex source, const std::string &key, uint8_t operation);
DEF_CMD_TRAIT(Commands::RemoteGateProject, CmdRemoteGateProject, {}, CommandType::LandscapeConstruction)
#endif
