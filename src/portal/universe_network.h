/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file universe_network.h Replicated universe directory, remote orders and client world navigation. */
#ifndef UNIVERSE_NETWORK_H
#define UNIVERSE_NETWORK_H
#include "federation_identity.h"
#include "remote_gate_projects.h"
#include "../command_type.h"
#include "../rail_type.h"
#include "../3rdparty/nlohmann/json.hpp"
#include <map>
#include <string>

struct Train;
struct Vehicle;
class UniverseNetwork {
public:
	static void Reset();
	static void Tick(uint64_t tick);
	static size_t PendingRequests();
	static std::string Namespace(FederationNamespace ns);
	static FederationNamespace ParseNamespace(const std::string &text);
	static std::string Manifest();
	static nlohmann::json Advertisement();
	static const std::map<std::string, nlohmann::json> &Directory();
	static bool Apply(const std::string &key, const nlohmann::json &record, bool execute = true);
	static TileIndex Route(WorldID from, WorldID to, CompanyID company, RailTypes railtypes);
	static std::vector<std::string> FindRoute(WorldID from, WorldID to, const std::string &owner, RailTypes railtypes);
	static std::optional<StationID> EnsureStation(const GlobalStationID &station);
	static bool ScheduleEditable(const Vehicle *vehicle);
	static bool HasUniverseOrders(const Vehicle *vehicle);
	static void ValidateAfterLoad();
	static uint64_t OrderRevision(const Train *train);
	static std::optional<GlobalOrderDestinationID> GateOrder(StationID station);
	static std::optional<StationID> EnsureGate(const GlobalOrderDestinationID &gate);
	static bool IsPinnedGate(const GlobalOrderDestinationID &order, TileIndex tile);
	static void AdvanceGateOrder(Train *train, TileIndex tile);
	static void ReleaseStation(StationID station);
	static bool IsRemoteStation(StationID station);
	static bool FollowTrain(const std::string &key);
	static bool Visit(const std::string &host, WorldID world);
	static void FinishVisit();
	static const std::string &VisitStatus();
	static std::string Save();
	static bool Load(const std::string &state);
};
bool ConUniverseCompanyMap(std::span<std::string_view> argv);
CommandCost CmdMapUniverseCompany(DoCommandFlags flags, CompanyID company, const std::string &name_space, uint64_t sequence);
DEF_CMD_TRAIT(Commands::MapUniverseCompany, CmdMapUniverseCompany, CommandFlag::Server, CommandType::ServerSetting)
CommandCost CmdEnsureUniverseIdentities(DoCommandFlags flags, [[maybe_unused]] uint8_t unused);
CommandCost CmdUpdateUniverseRecord(DoCommandFlags flags, const std::string &key, const std::string &record);
CommandCost CmdAddUniverseOrder(DoCommandFlags flags, VehicleID train, uint64_t expected_revision, const std::string &station);
DEF_CMD_TRAIT(Commands::EnsureUniverseIdentities, CmdEnsureUniverseIdentities, CommandFlag::Server, CommandType::ServerSetting)
DEF_CMD_TRAIT(Commands::UpdateUniverseRecord, CmdUpdateUniverseRecord, CommandFlag::Server, CommandType::ServerSetting)
DEF_CMD_TRAIT(Commands::AddUniverseOrder, CmdAddUniverseOrder, {}, CommandType::RouteManagement)
void ShowUniverseDestinations(VehicleID train);
#endif
