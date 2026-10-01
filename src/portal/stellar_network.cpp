/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file stellar_network.cpp Authoritative gate projects, range, reservations and shared access. */
#include "../stdafx.h"
#include "integrated_economy.h"
#include "stellar_network.h"
#include "commonwealth_slice.h"
#include "remote_gate_projects.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "portal_cmd.h"
#include "tech_tree.h"
#include "company_stockpile.h"
#include "logistics_hub.h"
#include "../3rdparty/nlohmann/json.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../cargo_type.h"
#include "../cargotype.h"
#include "../economy_func.h"
#include "../station_base.h"
#include "../rail_map.h"
#include "../tunnelbridge_map.h"
#include "../map_func.h"
#include "../genworld.h"
#include "../vehicle_base.h"
#include "../gfx_func.h"
#include "../core/backup_type.hpp"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../table/strings.h"
#include "../safeguards.h"

namespace {
std::map<WorldID, StellarWorld> worlds;
std::map<uint32_t, StellarLandingZone> zones;
std::map<uint32_t, GateProject> projects;
std::map<TileIndex, StellarGatePolicy> policies;
std::map<VehicleID, TileIndex> admitted;
std::map<TileIndex, TileIndex> terminal_tiles;
void RebuildTerminalAccess()
{
	terminal_tiles.clear();
	for (const auto &[tile, policy] : policies) {
		terminal_tiles[tile] = tile;
		if (tile >= Map::Size() || !IsTileType(tile, TileType::TunnelBridge)) continue;
		if (auto layout = PortalTerminal::Plan(tile, GetTunnelBridgeDirection(tile), PlanetManager::GetTileWorld(tile))) {
			for (const auto &part : layout->tiles)
				terminal_tiles[part.tile] = tile;
		}
	}
}
uint32_t next_project = 1;
bool internal_construction = false;
struct ConstructionScope {
	bool previous = internal_construction;
	ConstructionScope() { internal_construction = true; }
	~ConstructionScope() { internal_construction = previous; }
};
bool Pending(const GateProject &p)
{
	return p.state == GateProjectState::Supplying || p.state == GateProjectState::Ready;
}
std::set<TileIndex> Footprint(const StellarLandingZone &z)
{
	std::set<TileIndex> result{z.tile};
	if (auto layout = PortalTerminal::Plan(z.tile, z.direction, z.world)) {
		for (const auto &part : layout->tiles)
			result.insert(part.tile);
	}
	return result;
}
CommandCost Failure()
{
	return CommandCost(STR_ERROR_STELLAR_PROJECT);
}
} // namespace

void StellarNetwork::Reset()
{
	terminal_tiles.clear();
	worlds.clear();
	zones.clear();
	projects.clear();
	policies.clear();
	admitted.clear();
	next_project = 1;
	internal_construction = false;
}
bool StellarNetwork::Enabled()
{
	return !worlds.empty();
}
void StellarNetwork::RegisterWorld(StellarWorld world)
{
	worlds[world.world] = std::move(world);
}
const std::map<WorldID, StellarWorld> &StellarNetwork::Worlds()
{
	return worlds;
}
const StellarWorld *StellarNetwork::GetWorld(WorldID world)
{
	auto it = worlds.find(world);
	return it == worlds.end() ? nullptr : &it->second;
}
void StellarNetwork::RegisterZone(StellarLandingZone zone)
{
	zones[zone.id] = zone;
}
const std::map<uint32_t, StellarLandingZone> &StellarNetwork::Zones()
{
	return zones;
}
const std::map<uint32_t, GateProject> &StellarNetwork::Projects()
{
	return projects;
}
bool StellarNetwork::InternalConstruction()
{
	return internal_construction;
}
uint32_t StellarNetwork::Range(CompanyID company)
{
	constexpr uint32_t ranges[]{10, 25, 50, 100};
	for (int i = 3; i >= 0; --i)
		if (TechTreeManager::IsTechUnlocked(company, TECH_PORTAL_1 + i)) return ranges[i];
	return 0;
}
uint64_t StellarNetwork::DistanceSquared(const StellarWorld &a, const StellarWorld &b)
{
	int64_t dx = int64_t(a.x) - b.x, dy = int64_t(a.y) - b.y;
	return dx * dx + dy * dy;
}
uint32_t StellarNetwork::DistanceBands(const StellarWorld &a, const StellarWorld &b)
{
	uint64_t distance = DistanceSquared(a, b);
	for (uint32_t band = 1; band <= 10; ++band)
		if (distance <= uint64_t(band * 10) * (band * 10)) return band;
	return 0;
}
bool StellarNetwork::CanReach(CompanyID company, WorldID source, WorldID destination)
{
	auto a = GetWorld(source), b = GetWorld(destination);
	uint64_t range = Range(company);
	return a && b && a != b && range != 0 && a->opened && DistanceSquared(*a, *b) <= range * range;
}
bool StellarNetwork::WorldAccessible(WorldID world)
{
	auto w = GetWorld(world);
	return w == nullptr || w->opened;
}
CommandCost StellarNetwork::CheckPlacement(TileIndex tile)
{
	if (!Enabled() || _generating_world || internal_construction || !Company::IsValidID(_current_company)) return CommandCost();
	if (RemoteGateProjects::Reserved(tile)) return CommandCost(STR_ERROR_STELLAR_RESERVED);
	if (!WorldAccessible(PlanetManager::GetTileWorld(tile))) return CommandCost(STR_ERROR_STELLAR_CLOSED);
	for (const auto &[id, p] : projects) {
		if (Pending(p) && (p.source == tile || (zones.contains(p.zone) && Footprint(zones.at(p.zone)).contains(tile))))
			return CommandCost(STR_ERROR_STELLAR_RESERVED);
	}
	return CommandCost();
}
CommandCost StellarNetwork::Start(DoCommandFlags flags, TileIndex source, uint32_t zone, StationID station)
{
	if (!Company::IsValidID(_current_company) || !Enabled() || !zones.contains(zone) || next_project == UINT32_MAX || !PortalRegistry::IsUnlinkedGate(source))
		return Failure();
	if (GetTileOwner(source) != _current_company) return CheckOwnership(GetTileOwner(source));
	const auto &z = zones.at(zone);
	WorldID from = PlanetManager::GetTileWorld(source);
	if (!CanReach(_current_company, from, z.world)) return CommandCost(STR_ERROR_STELLAR_RANGE);
	const Station *st = Station::GetIfValid(station);
	const auto *hub = LogisticsHubManager::GetHubForStation(station);
	if (!st || !hub || st->owner != _current_company || hub->world_id != from || DistanceManhattan(st->xy, source) > 32)
		return CommandCost(STR_ERROR_STELLAR_SUPPLY);
	if (GetCargoTypeByLabel(CT_STEEL) == INVALID_CARGO || GetCargoTypeByLabel(CargoLabel{"MACH"}) == INVALID_CARGO) return CommandCost(STR_ERROR_STELLAR_CARGO);
	if (RemoteGateProjects::SupplyReserved(station) || RemoteGateProjects::Reserved(source)) return CommandCost(STR_ERROR_STELLAR_RESERVED);
	auto footprint = Footprint(z);
	for (TileIndex tile : footprint)
		if (RemoteGateProjects::Reserved(tile)) return CommandCost(STR_ERROR_STELLAR_RESERVED);
	for (const auto &[id, p] : projects) {
		if (!Pending(p)) continue;
		if (p.source == source || p.supply_station == station) return CommandCost(STR_ERROR_STELLAR_RESERVED);
		for (TileIndex tile : Footprint(zones.at(p.zone)))
			if (footprint.contains(tile)) return CommandCost(STR_ERROR_STELLAR_RESERVED);
	}
	ConstructionScope scope;
	auto build = Command<Commands::BuildPortalGate>::Do({}, z.tile, z.direction, GetRailType(source));
	if (build.Failed()) return build;
	uint32_t bands = DistanceBands(*GetWorld(from), *GetWorld(z.world));
	CommandCost fee(ExpensesType::Construction, _price[Price::BuildTunnel] * 10 * bands);
	if (flags.Test(DoCommandFlag::Execute)) {
		projects.emplace(next_project, GateProject{next_project, _current_company, source, zone, station, 0, 0, bands});
		++next_project;
	}
	return fee;
}
uint32_t StellarNetwork::ReceiveDelivery(StationID station, CompanyID owner, CargoType cargo, uint32_t amount)
{
	for (auto &[id, p] : projects) {
		if (p.state != GateProjectState::Supplying || p.owner != owner || p.supply_station != station) continue;
		uint32_t *stock = nullptr, required = 0;
		if (cargo == GetCargoTypeByLabel(CT_STEEL)) {
			stock = &p.steel;
			required = 200 * p.bands;
		}
		if (cargo == GetCargoTypeByLabel(CargoLabel{"MACH"})) {
			stock = &p.machines;
			required = 40 * p.bands;
		}
		if (stock == nullptr) return 0;
		uint32_t captured = std::min(amount, required - *stock);
		*stock += captured;
		if (p.steel == 200 * p.bands && p.machines == 40 * p.bands) p.state = GateProjectState::Ready;
		return captured;
	}
	return RemoteGateProjects::ReceiveDelivery(station, owner, cargo, amount);
}
CommandCost StellarNetwork::Operate(DoCommandFlags flags, uint32_t project, bool cancel)
{
	auto it = projects.find(project);
	if (it == projects.end() || it->second.owner != _current_company || !Company::IsValidID(_current_company)) return Failure();
	auto &p = it->second;
	if (!Pending(p)) return Failure();
	if (cancel) {
		if (flags.Test(DoCommandFlag::Execute)) {
			WorldID source_world = PlanetManager::GetTileWorld(p.source);
			if (p.steel) StockpileManager::AddCargo(source_world, p.owner, GetCargoTypeByLabel(CT_STEEL), p.steel);
			if (p.machines) StockpileManager::AddCargo(source_world, p.owner, GetCargoTypeByLabel(CargoLabel{"MACH"}), p.machines);
			p.steel = p.machines = 0;
			p.state = GateProjectState::Cancelled;
		}
		return CommandCost();
	}
	if (p.state != GateProjectState::Ready || !zones.contains(p.zone) || !PortalRegistry::IsUnlinkedGate(p.source) || GetTileOwner(p.source) != p.owner)
		return Failure();
	const auto &z = zones.at(p.zone);
	ConstructionScope scope;
	auto build = Command<Commands::BuildPortalGate>::Do({}, z.tile, z.direction, GetRailType(p.source));
	if (build.Failed()) return build;
	if (flags.Test(DoCommandFlag::Execute)) {
		auto built = Command<Commands::BuildPortalGate>::Do(flags, z.tile, z.direction, GetRailType(p.source));
		assert(built.Succeeded());
		auto linked = Command<Commands::LinkPortalGates>::Do(flags, p.source, z.tile);
		assert(linked.Succeeded());
		worlds.at(z.world).opened = true;
		MarkWholeScreenDirty();
		policies[p.source] = policies[z.tile] = {p.owner, false, 0};
		RebuildTerminalAccess();
		IntegratedEconomy::Record(EconomyFlow::Consumed, GetCargoTypeByLabel(CT_STEEL), p.steel);
		IntegratedEconomy::Record(EconomyFlow::Consumed, GetCargoTypeByLabel(CargoLabel{"MACH"}), p.machines);
		p.steel = p.machines = 0;
		p.state = GateProjectState::Active;
	}
	/* The link fee was paid when the reservation was made. */
	return build;
}
CommandCost StellarNetwork::BuildRemoteLanding(DoCommandFlags flags, uint32_t zone, RailType rail, WorldID source_world, uint32_t source_gate)
{
	auto it = zones.find(zone);
	if (it == zones.end() || !Company::IsValidID(_current_company) || source_world == it->second.world || source_world == INVALID_WORLD || !source_gate)
		return Failure();
	const auto &z = it->second;
	ConstructionScope scope;
	auto cost = Command<Commands::BuildPortalGate>::Do({}, z.tile, z.direction, rail);
	if (cost.Failed() || !flags.Test(DoCommandFlag::Execute)) return cost;
	auto built = Command<Commands::BuildPortalGate>::Do(flags, z.tile, z.direction, rail);
	if (built.Failed()) return built;
	[[maybe_unused]] auto gate = PortalRegistry::RegisterInterServerPortal(z.tile, z.direction, z.world, source_world, source_gate, 1, z.tile.base());
	assert(gate != INVALID_PORTAL);
	/* The source paid the destination's persisted construction quote. Lock transit until it acknowledges the link. */
	ConfigureRemoteAccess(z.tile, CompanyID::Invalid(), false, 0);
	worlds.at(z.world).opened = true;
	MarkWholeScreenDirty();
	return cost;
}
void StellarNetwork::ConfigureRemoteAccess(TileIndex tile, CompanyID owner, bool public_access, Money toll)
{
	if (toll < 0 || toll > INT32_MAX) return;
	policies[tile] = {owner, public_access, toll};
	RebuildTerminalAccess();
}
const StellarGatePolicy *StellarNetwork::Policy(TileIndex gate)
{
	auto it = policies.find(gate);
	return it == policies.end() ? nullptr : &it->second;
}
void StellarNetwork::RegisterCSTGate(TileIndex gate)
{
	policies[gate] = {CompanyID::Invalid(), true, 100};
	RebuildTerminalAccess();
}
bool StellarNetwork::CanTraverseTile(TileIndex tile, CompanyID company, bool ordinary_access)
{
	auto it = terminal_tiles.find(tile);
	return it != terminal_tiles.end() ? CanUseGate(it->second, company) : ordinary_access;
}
bool StellarNetwork::CanUseGate(TileIndex gate, CompanyID company)
{
	auto p = Policy(gate);
	return p == nullptr || p->owner == company || p->public_access;
}
bool StellarNetwork::AdmitTrain(TileIndex gate, VehicleID train, CompanyID company)
{
	if (admitted.contains(train) && admitted.at(train) == gate) return true;
	auto p = Policy(gate);
	if (!p) return true;
	if (!CanUseGate(gate, company)) return false;
	Company *payer = Company::GetIfValid(company);
	Money toll = p->owner == company ? Money{0} : p->toll;
	if (!payer || payer->money < toll) return false;
	SubtractMoneyFromCompany(company, CommandCost(ExpensesType::Other, toll));
	if (Company::IsValidID(p->owner)) SubtractMoneyFromCompany(p->owner, CommandCost(ExpensesType::Other, -toll));
	if (_commonwealth_slice_audit != nullptr) {
		auto &observed = _commonwealth_slice_audit->gate_tolls[gate.base()];
		++observed.first; observed.second += int64_t(toll);
		_commonwealth_slice_audit->vehicle_tolls[train.base()] += int64_t(toll);
	}
	admitted[train] = gate;
	return true;
}
void StellarNetwork::ReleaseTrain(VehicleID train)
{
	admitted.erase(train);
}
CommandCost StellarNetwork::SetAccess(DoCommandFlags flags, TileIndex gate, bool public_access, Money toll)
{
	auto it = policies.find(gate);
	if (it == policies.end() || it->second.owner != _current_company || RemoteGateProjects::IsArrivalGate(gate) || !Company::IsValidID(_current_company) ||
		toll < 0 || toll > INT32_MAX)
		return Failure();
	if (flags.Test(DoCommandFlag::Execute)) {
		it->second.public_access = public_access;
		it->second.toll = toll;
		RemoteGateProjects::UpdateAccess(gate, public_access, toll);
		TileIndex other = PortalRegistry::GetOtherPortalEnd(gate);
		if (policies.contains(other)) policies[other] = it->second;
		YapfNotifyTrackLayoutChange(gate, Track::Invalid);
	}
	return CommandCost();
}
void StellarNetwork::RemoveGate(TileIndex gate)
{
	policies.erase(gate);
	RebuildTerminalAccess();
	std::erase_if(admitted, [gate](const auto &a) { return a.second == gate; });
}
void StellarNetwork::RemoveSupplyStation(StationID station)
{
	for (auto &[id, p] : projects)
		if (Pending(p) && p.supply_station == station && Company::IsValidID(p.owner)) {
			AutoRestoreBackup company(_current_company, p.owner);
			Operate(DoCommandFlag::Execute, id, true);
		}
	RemoteGateProjects::RemoveSupplyStation(station);
}
void StellarNetwork::ChangeCompany(CompanyID old_owner, CompanyID new_owner)
{
	for (auto &[id, p] : projects)
		if (p.owner == old_owner) {
			p.owner = new_owner;
			if (!Company::IsValidID(new_owner) && Pending(p)) {
				p.state = GateProjectState::Cancelled;
				p.steel = p.machines = 0;
			}
		}
	for (auto &[tile, p] : policies)
		if (p.owner == old_owner) {
			p.owner = new_owner;
			if (!Company::IsValidID(new_owner)) {
				p.public_access = true;
				p.toll = 0;
			}
		}
}
std::string StellarNetwork::Save()
{
	using nlohmann::json;
	json root{{"version", 1},
			  {"next", next_project},
			  {"worlds", json::array()},
			  {"zones", json::array()},
			  {"projects", json::array()},
			  {"policies", json::array()},
			  {"admitted", json::array()}};
	for (const auto &[id, w] : worlds)
		root["worlds"].push_back({id.base(), w.catalogue_id, w.name, w.x, w.y, w.opened});
	for (const auto &[id, z] : zones)
		root["zones"].push_back({id, z.world.base(), z.tile.base(), to_underlying(z.direction)});
	for (const auto &[id, p] : projects)
		root["projects"].push_back(
			{id, p.owner.base(), p.source.base(), p.zone, p.supply_station.base(), p.steel, p.machines, p.bands, to_underlying(p.state)});
	for (const auto &[tile, p] : policies)
		root["policies"].push_back({tile.base(), p.owner.base(), p.public_access, int64_t(p.toll)});
	for (const auto &[vehicle, tile] : admitted)
		root["admitted"].push_back({vehicle.base(), tile.base()});
	return root.dump();
}
bool StellarNetwork::Load(const std::string &data)
{
	Reset();
	try {
		auto root = nlohmann::json::parse(data);
		if (root.at("version") != 1) return false;
		next_project = root.at("next").get<uint32_t>();
		if (next_project == 0) return false;
		for (auto &r : root.at("worlds")) {
			StellarWorld w{WorldID{r.at(0).get<uint32_t>()}, r.at(1), r.at(2), r.at(3), r.at(4), r.at(5)};
			if (std::abs(int64_t(w.x)) > 100000 || std::abs(int64_t(w.y)) > 100000 || worlds.contains(w.world)) return false;
			RegisterWorld(w);
		}
		for (auto &r : root.at("zones")) {
			StellarLandingZone z{r.at(0), WorldID{r.at(1).get<uint32_t>()}, TileIndex{r.at(2).get<uint32_t>()},
								 static_cast<DiagDirection>(r.at(3).get<uint8_t>())};
			if (!worlds.contains(z.world) || !IsValidDiagDirection(z.direction) || zones.contains(z.id)) return false;
			RegisterZone(z);
		}
		for (auto &r : root.at("projects")) {
			GateProject p{r.at(0),
						  CompanyID{r.at(1).get<uint8_t>()},
						  TileIndex{r.at(2).get<uint32_t>()},
						  r.at(3),
						  StationID{r.at(4).get<uint16_t>()},
						  r.at(5),
						  r.at(6),
						  r.at(7),
						  static_cast<GateProjectState>(r.at(8).get<uint8_t>())};
			if (!zones.contains(p.zone) || p.id >= next_project || projects.contains(p.id) || p.bands == 0 || p.bands > 10 || p.steel > 200 * p.bands ||
				p.machines > 40 * p.bands || p.state > GateProjectState::Cancelled)
				return false;
			projects[p.id] = p;
		}
		for (auto &r : root.at("policies")) {
			StellarGatePolicy p{CompanyID{r.at(1).get<uint8_t>()}, r.at(2), r.at(3).get<int64_t>()};
			if (p.toll < 0 || p.toll > INT32_MAX) return false;
			policies[TileIndex{r.at(0).get<uint32_t>()}] = p;
		}
		for (auto &r : root.at("admitted"))
			admitted[VehicleID{r.at(0).get<uint32_t>()}] = TileIndex{r.at(1).get<uint32_t>()};
		return true;
	} catch (const std::exception &) {
		Reset();
		return false;
	}
}
void StellarNetwork::ValidateAfterLoad()
{
	std::erase_if(worlds, [](const auto &entry) { return PlanetManager::GetRegion(entry.first) == nullptr; });
	std::erase_if(
		zones, [](const auto &entry) { return PlanetManager::GetTileWorld(entry.second.tile) != entry.second.world || !worlds.contains(entry.second.world); });
	std::erase_if(projects, [](const auto &entry) { return !zones.contains(entry.second.zone) || !Company::IsValidID(entry.second.owner); });
	std::erase_if(policies, [](const auto &entry) { return !PortalRegistry::IsPortalTile(entry.first); });
	std::erase_if(admitted, [](const auto &entry) { return !Vehicle::IsValidID(entry.first); });
	RebuildTerminalAccess();
}
CommandCost CmdStartGateProject(DoCommandFlags f, TileIndex t, uint32_t z, StationID s)
{
	return StellarNetwork::Start(f, t, z, s);
}
CommandCost CmdOperateGateProject(DoCommandFlags f, uint32_t p, bool c)
{
	return StellarNetwork::Operate(f, p, c);
}
CommandCost CmdSetStellarGateAccess(DoCommandFlags f, TileIndex t, bool p, Money m)
{
	return StellarNetwork::SetAccess(f, t, p, m);
}
