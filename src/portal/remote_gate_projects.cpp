/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file remote_gate_projects.cpp Idempotent remote reservations, equipment escrow and native far-end construction. */
#include "../stdafx.h"
#include "remote_gate_projects.h"
#include "universe_network.h"
#include "federation_identity.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "portal_terminal.h"
#include "company_stockpile.h"
#include "logistics_hub.h"
#include "../company_base.h"
#include "../core/backup_type.hpp"
#include "../company_func.h"
#include "../station_base.h"
#include "../cargotype.h"
#include "../rail_map.h"
#include "../tunnelbridge_map.h"
#include "../map_func.h"
#include "../table/strings.h"
#include "../safeguards.h"
using nlohmann::json;
namespace {
std::map<std::string, json> outbound, inbound;
uint64_t sequence = 1;
std::string Namespace()
{
	return UniverseNetwork::Namespace(FederationIdentityRegistry::GetNamespace());
}
std::string Identity(CompanyID company)
{
	auto id = FederationIdentityRegistry::FindCompany(company);
	return id ? fmt::format("{}:{}", UniverseNetwork::Namespace(id->name_space), id->sequence) : "";
}
CompanyID LocalCompany(const std::string &identity)
{
	for (const auto *c : Company::Iterate())
		if (Identity(c->index) == identity) return c->index;
	return CompanyID::Invalid();
}
bool Pending(const json &p)
{
	return p.at("state") != "active" && p.at("state") != "cancelled";
}
const json *Reply(const json &p)
{
	for (const auto &[key, r] : UniverseNetwork::Directory())
		if (r.value("kind", "") == "gatereply" && r.at("id") == p.at("id") && r.at("namespace") == p.at("target_host") && r.value("online", false)) return &r;
	return nullptr;
}
CommandCost Failure()
{
	return CommandCost(STR_ERROR_STELLAR_PROJECT);
}
} // namespace
void RemoteGateProjects::Reset()
{
	outbound.clear();
	inbound.clear();
	sequence = 1;
}
void RemoteGateProjects::RemoveSupplyStation(StationID station)
{
	for (auto &[id, p] : outbound)
		if ((p.at("state") == "supplying" || p.at("state") == "ready") && p.at("station").get<uint16_t>() == station.base()) {
			auto owner = LocalCompany(p.at("owner"));
			if (!Company::IsValidID(owner)) continue;
			AutoRestoreBackup company(_current_company, owner);
			Operate(DoCommandFlag::Execute, INVALID_TILE, id, 2);
		}
}
json RemoteGateProjects::Save()
{
	return {{"next", sequence}, {"outbound", outbound}, {"inbound", inbound}};
}
bool RemoteGateProjects::Load(const json &state)
{
	try {
		auto out = state.at("outbound").get<std::map<std::string, json>>();
		auto in = state.at("inbound").get<std::map<std::string, json>>();
		auto next = state.at("next").get<uint64_t>();
		if (!next || out.size() > 4096 || in.size() > 4096) return false;
		for (const auto &[id, p] : out)
			if (id != p.at("id").get<std::string>() || p.at("bands").get<uint32_t>() < 1 || p.at("bands").get<uint32_t>() > 10 ||
				p.at("steel").get<uint32_t>() > 200 * p.at("bands").get<uint32_t>() || p.at("machines").get<uint32_t>() > 40 * p.at("bands").get<uint32_t>())
				return false;
		for (const auto &[id, p] : out) {
			const std::string status = p.at("state");
			if (status != "supplying" && status != "ready" && status != "committed" && status != "active" && status != "cancelled") return false;
			if (!UniverseNetwork::ParseNamespace(p.at("namespace")).IsValid() || !UniverseNetwork::ParseNamespace(p.at("target_host")).IsValid()) return false;
			(void)p.at("source").get<uint32_t>();
			(void)p.at("source_world").get<uint32_t>();
			(void)p.at("target_world").get<uint32_t>();
			(void)p.at("zone").get<uint32_t>();
			(void)p.at("station").get<uint16_t>();
			(void)p.at("owner").get<std::string>();
			if (p.at("rail").get<uint32_t>() >= RAILTYPE_END || p.at("toll").get<int64_t>() < 0 || p.at("toll").get<int64_t>() > INT32_MAX) return false;
			(void)p.at("public").get<bool>();
			if (status == "committed" && p.at("price").get<int64_t>() < 0) return false;
		}
		for (const auto &[id, p] : in) {
			const std::string status = p.at("state");
			if (id != p.at("id").get<std::string>() || (status != "reserved" && status != "built" && status != "active" && status != "cancelled")) return false;
			if (!UniverseNetwork::ParseNamespace(p.at("source_host")).IsValid() || p.at("price").get<int64_t>() < 0 ||
				p.at("rail").get<uint32_t>() >= RAILTYPE_END)
				return false;
			(void)p.at("zone").get<uint32_t>();
			(void)p.at("tile").get<uint32_t>();
			(void)p.at("source").get<uint32_t>();
			(void)p.at("source_world").get<uint32_t>();
			(void)p.at("owner").get<std::string>();
		}
		outbound = std::move(out);
		inbound = std::move(in);
		sequence = next;
		return true;
	} catch (...) {
		return false;
	}
}
bool RemoteGateProjects::ValidateAfterLoad()
{
	for (const auto &[id, p] : outbound) {
		if (p.at("state") == "cancelled") continue;
		TileIndex source{p.at("source").get<uint32_t>()};
		if (source >= Map::Size() || PlanetManager::GetTileWorld(source).base() != p.at("source_world").get<uint32_t>()) return false;
		if (p.at("state") == "active" ? !PortalRegistry::IsInterServerPortal(source) : !PortalRegistry::IsUnlinkedGate(source)) return false;
	}
	for (const auto &[id, p] : inbound) {
		if (p.at("state") == "cancelled") continue;
		auto zone = StellarNetwork::Zones().find(p.at("zone").get<uint32_t>());
		if (zone == StellarNetwork::Zones().end() || zone->second.tile.base() != p.at("tile").get<uint32_t>()) return false;
		if (p.at("state") != "reserved" && !PortalRegistry::IsInterServerPortal(zone->second.tile)) return false;
	}
	return true;
}
json RemoteGateProjects::Advertisements(bool replies)
{
	json result = json::array();
	for (const auto &[id, p] : replies ? inbound : outbound)
		result.push_back(p);
	return result;
}
bool RemoteGateProjects::SupplyReserved(StationID station)
{
	for (const auto &[id, p] : outbound)
		if (Pending(p) && p.at("station").get<uint16_t>() == station.base()) return true;
	return false;
}
bool RemoteGateProjects::IsArrivalGate(TileIndex tile)
{
	for (const auto &[id, p] : inbound)
		if (p.at("state") != "cancelled" && p.at("tile").get<uint32_t>() == tile.base()) return true;
	return false;
}
bool RemoteGateProjects::OwnsGate(TileIndex tile)
{
	for (const auto &[id, p] : outbound)
		if (p.at("state") != "cancelled" && p.at("source").get<uint32_t>() == tile.base()) return true;
	for (const auto &[id, p] : inbound)
		if (p.at("state") != "cancelled" && p.at("tile").get<uint32_t>() == tile.base()) return true;
	return false;
}
bool RemoteGateProjects::Reserved(TileIndex tile)
{
	for (const auto &[id, p] : outbound)
		if (Pending(p) && p.at("source").get<uint32_t>() == tile.base()) return true;
	for (const auto &[id, p] : inbound) {
		if (p.at("state") == "cancelled" || p.at("state") == "active") continue;
		auto z = StellarNetwork::Zones().find(p.at("zone").get<uint32_t>());
		if (z == StellarNetwork::Zones().end()) continue;
		if (z->second.tile == tile) return true;
		if (auto layout = PortalTerminal::Plan(z->second.tile, z->second.direction, z->second.world))
			for (const auto &part : layout->tiles)
				if (part.tile == tile) return true;
	}
	return false;
}
uint32_t RemoteGateProjects::ReceiveDelivery(StationID station, CompanyID owner, CargoType cargo, uint32_t amount)
{
	for (auto &[id, p] : outbound) {
		if (p.at("state") != "supplying" || p.at("station").get<uint16_t>() != station.base() || p.at("owner") != Identity(owner)) continue;
		const char *field = nullptr;
		uint32_t required = 0;
		if (cargo == GetCargoTypeByLabel(CT_STEEL)) {
			field = "steel";
			required = 200 * p.at("bands").get<uint32_t>();
		}
		if (cargo == GetCargoTypeByLabel(CargoLabel{"MACH"})) {
			field = "machines";
			required = 40 * p.at("bands").get<uint32_t>();
		}
		if (!field) return 0;
		uint32_t captured = std::min(amount, required - p.at(field).get<uint32_t>());
		p[field] = p.at(field).get<uint32_t>() + captured;
		if (p.at("steel").get<uint32_t>() == 200 * p.at("bands").get<uint32_t>() && p.at("machines").get<uint32_t>() == 40 * p.at("bands").get<uint32_t>())
			p["state"] = "ready";
		return captured;
	}
	return 0;
}
CommandCost RemoteGateProjects::Operate(DoCommandFlags flags, TileIndex source, const std::string &key, uint8_t operation)
{
	if (!Company::IsValidID(_current_company) || operation > 2) return Failure();
	if (operation != 0) {
		auto it = outbound.find(key);
		if (it == outbound.end()) return Failure();
		auto &p = it->second;
		if (p.at("owner") != Identity(_current_company)) return Failure();
		if (p.at("state") != "supplying" && p.at("state") != "ready") return Failure();
		if (operation == 2) {
			if (flags.Test(DoCommandFlag::Execute)) {
				auto world = WorldID{p.at("source_world").get<uint32_t>()};
				StockpileManager::AddCargo(world, _current_company, GetCargoTypeByLabel(CT_STEEL), p.at("steel").get<uint32_t>());
				StockpileManager::AddCargo(world, _current_company, GetCargoTypeByLabel(CargoLabel{"MACH"}), p.at("machines").get<uint32_t>());
				p["steel"] = 0;
				p["machines"] = 0;
				p["state"] = "cancelled";
			}
			return CommandCost();
		}
		auto reply = Reply(p);
		source = TileIndex{p.at("source").get<uint32_t>()};
		if (p.at("state") != "ready" || !reply || reply->at("state") != "reserved" || !PortalRegistry::IsUnlinkedGate(source) ||
			GetTileOwner(source) != _current_company)
			return Failure();
		Money price = reply->at("price").get<int64_t>();
		if (price < 0) return Failure();
		if (flags.Test(DoCommandFlag::Execute)) {
			p["state"] = "committed";
			p["price"] = int64_t(price);
		}
		return CommandCost(ExpensesType::Construction, price);
	}
	auto found = UniverseNetwork::Directory().find(key);
	if (found == UniverseNetwork::Directory().end() || found->second.value("kind", "") != "zone") return Failure();
	const auto &z = found->second;
	if (!z.value("online", false) || z.at("manifest") != UniverseNetwork::Manifest() || z.at("namespace") == Namespace() ||
		!PortalRegistry::IsUnlinkedGate(source) || GetTileOwner(source) != _current_company || sequence == UINT64_MAX)
		return Failure();
	WorldID from = PlanetManager::GetTileWorld(source), to{z.at("world").get<uint32_t>()};
	auto a = StellarNetwork::GetWorld(from);
	if (!a || !a->opened) return Failure();
	StellarWorld b{to, "", "", z.at("x").get<int32_t>(), z.at("y").get<int32_t>(), false};
	uint64_t range = StellarNetwork::Range(_current_company);
	if (!range || StellarNetwork::DistanceSquared(*a, b) > range * range) return CommandCost(STR_ERROR_STELLAR_RANGE);
	auto owner = Identity(_current_company);
	if (owner.empty()) return Failure();
	auto host = UniverseNetwork::Directory().find(z.at("namespace").get<std::string>());
	bool company_mapped = false;
	if (host != UniverseNetwork::Directory().end())
		for (const auto &c : host->second.at("companies"))
			if (c.at("identity") == owner) company_mapped = true;
	if (!company_mapped) return Failure();
	StationID station = StationID::Invalid();
	for (const auto &hub : LogisticsHubManager::GetAllHubs()) {
		auto st = Station::GetIfValid(hub.station_id);
		if (st && hub.company_id == _current_company && hub.world_id == from && DistanceManhattan(st->xy, source) <= 32 &&
			LogisticsHubManager::ValidateForStation(hub) && (station == StationID::Invalid() || hub.station_id < station))
			station = hub.station_id;
	}
	if (station == StationID::Invalid()) return CommandCost(STR_ERROR_STELLAR_SUPPLY);
	if (GetCargoTypeByLabel(CT_STEEL) == INVALID_CARGO || GetCargoTypeByLabel(CargoLabel{"MACH"}) == INVALID_CARGO) return CommandCost(STR_ERROR_STELLAR_CARGO);
	for (const auto &[id, p] : outbound)
		if (Pending(p) && (p.at("source").get<uint32_t>() == source.base() || p.at("station").get<uint16_t>() == station.base())) return Failure();
	for (const auto &[id, p] : StellarNetwork::Projects())
		if (p.state <= GateProjectState::Ready && (p.source == source || p.supply_station == station)) return Failure();
	uint32_t bands = StellarNetwork::DistanceBands(*a, b);
	if (flags.Test(DoCommandFlag::Execute)) {
		auto id = fmt::format("{}:{}", Namespace(), sequence++);
		outbound[id] = {{"id", id},
						{"namespace", Namespace()},
						{"source", source.base()},
						{"source_world", from.base()},
						{"target_host", z.at("namespace")},
						{"target_world", to.base()},
						{"zone", z.at("id")},
						{"owner", owner},
						{"station", station.base()},
						{"rail", to_underlying(GetRailType(source))},
						{"bands", bands},
						{"steel", 0},
						{"machines", 0},
						{"state", "supplying"},
						{"public", false},
						{"toll", 0}};
	}
	return CommandCost(ExpensesType::Construction, _price[Price::BuildTunnel] * 10 * bands);
}
void RemoteGateProjects::Reconcile(const json &r)
{
	try {
		std::string kind = r.value("kind", "");
		if (!r.value("online", false) || r.at("manifest") != UniverseNetwork::Manifest()) return;
		if (kind == "gateproject" && r.at("target_host") == Namespace()) {
			std::string id = r.at("id"), state = r.at("state");
			auto owner = LocalCompany(r.at("owner"));
			if (!Company::IsValidID(owner)) return;
			uint32_t zone = r.at("zone");
			auto z = StellarNetwork::Zones().find(zone);
			if (z == StellarNetwork::Zones().end() || z->second.world.base() != r.at("target_world").get<uint32_t>()) return;
			AutoRestoreBackup company(_current_company, owner);
			auto rail = static_cast<RailType>(r.at("rail").get<uint8_t>());
			if (!ValParamRailType(rail)) return;
			if (!inbound.contains(id)) {
				if (state == "cancelled") return;
				if (Reserved(z->second.tile)) return;
				if (auto layout = PortalTerminal::Plan(z->second.tile, z->second.direction, z->second.world)) {
					for (const auto &part : layout->tiles)
						if (Reserved(part.tile)) return;
				}
				for (const auto &[local_id, project] : StellarNetwork::Projects())
					if (project.state <= GateProjectState::Ready) {
						auto local_zone = StellarNetwork::Zones().find(project.zone);
						if (local_zone == StellarNetwork::Zones().end()) return;
						auto layout = PortalTerminal::Plan(local_zone->second.tile, local_zone->second.direction, local_zone->second.world);
						auto target = PortalTerminal::Plan(z->second.tile, z->second.direction, z->second.world);
						if (!layout || !target) return;
						std::set<TileIndex> occupied{local_zone->second.tile};
						for (const auto &part : layout->tiles)
							occupied.insert(part.tile);
						if (occupied.contains(z->second.tile)) return;
						for (const auto &part : target->tiles)
							if (occupied.contains(part.tile)) return;
					}
				auto price = StellarNetwork::BuildRemoteLanding({}, zone, rail, WorldID{r.at("source_world").get<uint32_t>()}, r.at("source").get<uint32_t>());
				if (price.Failed()) return;
				inbound[id] = {{"id", id},
							   {"source_host", r.at("namespace")},
							   {"zone", zone},
							   {"owner", r.at("owner")},
							   {"source", r.at("source")},
							   {"source_world", r.at("source_world")},
							   {"rail", r.at("rail")},
							   {"state", "reserved"},
							   {"tile", z->second.tile.base()},
							   {"price", int64_t(price.GetCost())}};
			}
			auto &p = inbound.at(id);
			if (p.at("owner") != r.at("owner") || p.at("source_host") != r.at("namespace") || p.at("source") != r.at("source") ||
				p.at("zone") != r.at("zone") || p.at("rail") != r.at("rail"))
				return;
			if (state == "cancelled" && p.at("state") == "reserved") {
				p["state"] = "cancelled";
				return;
			}
			if (state == "committed" && p.at("state") == "reserved" && r.at("price") == p.at("price")) {
				auto result = StellarNetwork::BuildRemoteLanding(DoCommandFlag::Execute, zone, rail, WorldID{r.at("source_world").get<uint32_t>()},
																 r.at("source").get<uint32_t>());
				if (result.Succeeded()) p["state"] = "built";
			}
			if (state == "active" && (p.at("state") == "built" || p.at("state") == "active")) {
				p["state"] = "active";
				StellarNetwork::ConfigureRemoteAccess(z->second.tile, owner, r.at("public").get<bool>(), Money{r.at("toll").get<int64_t>()});
			}
		} else if (kind == "gatereply") {
			auto it = outbound.find(r.at("id").get<std::string>());
			if (it == outbound.end()) return;
			auto &p = it->second;
			if (p.at("target_host") != r.at("namespace") || p.at("state") != "committed" || (r.at("state") != "built" && r.at("state") != "active")) return;
			TileIndex source{p.at("source").get<uint32_t>()};
			if (!PortalRegistry::IsUnlinkedGate(source)) return;
			auto owner = LocalCompany(p.at("owner"));
			if (!Company::IsValidID(owner)) return;
			auto gate =
				PortalRegistry::RegisterInterServerPortal(source, GetTunnelBridgeDirection(source), WorldID{p.at("source_world").get<uint32_t>()},
														  WorldID{p.at("target_world").get<uint32_t>()}, r.at("tile").get<uint32_t>(), 1, source.base());
			if (gate == INVALID_PORTAL) return;
			StellarNetwork::ConfigureRemoteAccess(source, owner, false, 0);
			p["steel"] = 0;
			p["machines"] = 0;
			p["state"] = "active";
		}
	} catch (...) { /* A malformed advertisement never authorises construction. */
	}
}
void RemoteGateProjects::UpdateAccess(TileIndex tile, bool public_access, Money toll)
{
	for (auto &[id, p] : outbound)
		if (p.at("source").get<uint32_t>() == tile.base() && p.at("state") == "active") {
			p["public"] = public_access;
			p["toll"] = int64_t(toll);
		}
}
CommandCost CmdRemoteGateProject(DoCommandFlags flags, TileIndex source, const std::string &key, uint8_t operation)
{
	return RemoteGateProjects::Operate(flags, source, key, operation);
}
