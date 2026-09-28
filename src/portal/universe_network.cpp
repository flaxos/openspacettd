/* This file is part of OpenSpaceTTD, licensed under GNU GPL version 2. */
/** @file universe_network.cpp Asynchronous advertisements, stable station orders and automatic host switching. */
#include "../stdafx.h"
#include "../console_func.h"
#include "universe_network.h"
#include "authority_transport.h"
#include "federation_cmd.h"
#include "transfer_journal.h"
#include "stellar_network.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "content_manifest.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../command_func.h"
#include "../order_base.h"
#include "../order_cmd.h"
#include "../window_gui.h"
#include "../vehicle_gui.h"
#include "../gui.h"
#include "../order_func.h"
#include "../train.h"
#include "../station_base.h"
#include "../town.h"
#include "../settings_type.h"
#include "../strings_func.h"
#include "../network/network.h"
#include "../network/network_func.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../map_func.h"
#include "../rail_map.h"
#include "../table/strings.h"
#include <queue>
#include <charconv>
#include <cstdlib>
#include "../safeguards.h"
using nlohmann::json;
namespace {
std::map<std::string, json> directory;
std::map<StationID, GlobalOrderDestinationID> gate_orders;
std::unique_ptr<AuthorityRequest> publishing, fetching;
std::string visit_host, visit_status;
WorldID visit_world = INVALID_WORLD;
GlobalCompanyID visit_company{};
std::optional<GlobalConsistID> visit_train;
struct Camera {
	int32_t x, y;
};
std::map<std::pair<std::string, WorldID>, Camera> cameras;
std::string CompanyIdentity(CompanyID company)
{
	auto id = FederationIdentityRegistry::FindCompany(company);
	return id ? UniverseNetwork::Namespace(id->name_space) + ":" + fmt::format("{}", id->sequence) : "";
}
GlobalStationID StationIdentity(const json &r)
{
	return {UniverseNetwork::ParseNamespace(r.at("namespace")), r.at("sequence").get<uint64_t>(), WorldID{r.at("world").get<uint32_t>()}};
}
bool Compatible(const json &r)
{
	return r.value("online", false) && r.value("manifest", "") == UniverseNetwork::Manifest();
}
} // namespace
void UniverseNetwork::Reset()
{
	directory.clear();
	gate_orders.clear();
	RemoteGateProjects::Reset();
	publishing.reset();
	fetching.reset();
}
std::string UniverseNetwork::Namespace(FederationNamespace ns)
{
	return fmt::format("{:x}:{:x}", ns.high, ns.low);
}
FederationNamespace UniverseNetwork::ParseNamespace(const std::string &text)
{
	auto split = text.find(':');
	if (split == std::string::npos) return {};
	FederationNamespace ns{};
	auto high = std::from_chars(text.data(), text.data() + split, ns.high, 16);
	auto low = std::from_chars(text.data() + split + 1, text.data() + text.size(), ns.low, 16);
	return high.ec == std::errc{} && low.ec == std::errc{} && high.ptr == text.data() + split && low.ptr == text.data() + text.size() ? ns
																																	  : FederationNamespace{};
}
std::string UniverseNetwork::Manifest()
{
	auto manifest = ContentManifestCodec::CaptureCurrent();
	if (!manifest.Succeeded()) return {};
	auto token = ContentManifestCodec::Digest(*manifest.manifest);
	if (!token.Succeeded()) return {};
	std::string text;
	for (auto b : token.token)
		text += fmt::format("{:02x}", b);
	return text;
}
const std::map<std::string, json> &UniverseNetwork::Directory()
{
	return directory;
}
json UniverseNetwork::Advertisement()
{
	auto ns = Namespace(FederationIdentityRegistry::GetNamespace());
	const char *address = std::getenv("OPENTTD_UNIVERSE_GAME_ADDRESS");
	json host{{"namespace", ns},
			  {"address", address ? address : fmt::format("127.0.0.1:{}", _settings_client.network.server_port)},
			  {"manifest", Manifest()},
			  {"worlds", json::array()},
			  {"stations", json::array()},
			  {"gates", json::array()},
			  {"companies", json::array()},
			  {"zones", json::array()},
			  {"gateprojects", RemoteGateProjects::Advertisements(false)},
			  {"gatereplys", RemoteGateProjects::Advertisements(true)},
			  {"trains", json::array()}};
	for (const auto &world : PlanetManager::GetAllRegions()) {
		auto stellar = StellarNetwork::GetWorld(world.id);
		host["worlds"].push_back({{"id", world.id.base()},
								  {"name", world.name},
								  {"catalogue", stellar ? stellar->catalogue_id : ""},
								  {"x", stellar ? stellar->x : 0},
								  {"y", stellar ? stellar->y : 0},
								  {"opened", !stellar || stellar->opened}});
	}
	for (const auto &[id, z] : StellarNetwork::Zones()) {
		auto w = StellarNetwork::GetWorld(z.world);
		if (!w) continue;
		host["zones"].push_back({{"id", id}, {"world", z.world.base()}, {"name", fmt::format("{} / landing {}", w->name, id)}, {"x", w->x}, {"y", w->y}});
	}
	for (const Company *c : Company::Iterate()) {
		if (!CompanyIdentity(c->index).empty()) host["companies"].push_back({{"local", c->index.base()}, {"identity", CompanyIdentity(c->index)}});
	}
	for (const Station *st : Station::Iterate()) {
		if (!st->facilities.Test(StationFacility::Train) || GateOrder(st->index)) continue;
		auto id = FederationIdentityRegistry::FindStation(st->index);
		if (!id || id->name_space != FederationIdentityRegistry::GetNamespace()) continue;
		host["stations"].push_back({{"namespace", ns},
									{"sequence", id->sequence},
									{"world", id->world_id.base()},
									{"name", GetString(STR_STATION_NAME, st->index)},
									{"owner", CompanyIdentity(st->owner)}});
	}
	for (const Train *t : Train::Iterate())
		if (t->IsFrontEngine())
			if (auto id = FederationIdentityRegistry::Find(t)) {
				host["trains"].push_back({{"id", fmt::format("{}:{}", Namespace(id->name_space), id->sequence)},
										  {"consist_namespace", Namespace(id->name_space)},
										  {"sequence", id->sequence},
										  {"world", PlanetManager::GetTileWorld(t->tile).base()},
										  {"owner", CompanyIdentity(t->owner)},
										  {"name", fmt::format("Train {}", t->unitnumber)},
										  {"revision", OrderRevision(t)}});
			}
	auto gate = [&](TileIndex tile, WorldID local, WorldID remote, uint32_t gate_id, uint32_t dest_gate) {
		const auto *policy = StellarNetwork::Policy(tile);
		auto owner = GetTileOwner(tile);
		host["gates"].push_back({{"id", gate_id},
								 {"tile", tile.base()},
								 {"world", local.base()},
								 {"destination", remote.base()},
								 {"dest_gate", dest_gate},
								 {"owner", CompanyIdentity(owner)},
								 {"public", policy ? policy->public_access : owner == OWNER_NONE},
								 {"toll", policy ? int64_t(policy->toll) : 0},
								 {"rail", to_underlying(GetRailType(tile))}});
	};
	for (const auto &[id, p] : PortalRegistry::GetAllPortals()) {
		gate(p.end_a.tile, p.end_a.world_id, p.end_b.world_id, p.end_a.tile.base(), p.end_b.tile.base());
		gate(p.end_b.tile, p.end_b.world_id, p.end_a.world_id, p.end_b.tile.base(), p.end_a.tile.base());
	}
	for (const auto &[tile, p] : PortalRegistry::GetAllInterServerPortals())
		gate(tile, p.local_endpoint.world_id, p.remote_world, p.id.base(), p.remote_gate_id);
	return host;
}
bool UniverseNetwork::Apply(const std::string &key, const json &record, bool execute)
{
	if (key.empty() || key.size() > 128 || record.dump().size() > 1200) return false;
	if (record.is_null()) {
		if (execute) directory.erase(key);
		return true;
	}
	try {
		std::string kind = record.at("kind"), ns = record.at("namespace");
		if (!ParseNamespace(ns).IsValid() || (kind != "host" && kind != "world" && kind != "station" && kind != "gate" && kind != "zone" &&
											  kind != "gateproject" && kind != "gatereply" && kind != "train"))
			return false;
		if (!record.at("manifest").is_string() || !record.at("online").is_boolean()) return false;
		if (kind == "world" && (!record.at("id").is_number_unsigned() && !record.at("id").is_number_integer())) return false;
		if (kind == "gate") {
			(void)record.at("world").get<uint32_t>();
			(void)record.at("destination").get<uint32_t>();
			(void)record.at("owner").get<std::string>();
			(void)record.at("public").get<bool>();
		}
		if (kind == "host" && !record.at("companies").is_array()) return false;
		if (kind == "station" && !StationIdentity(record).IsValid()) return false;
		if (kind == "gate" &&
			(record.at("toll").get<int64_t>() < 0 || record.at("toll").get<int64_t>() > INT32_MAX || record.at("rail").get<uint32_t>() >= RAILTYPE_END))
			return false;
		if (execute) directory[key] = record;
		return true;
	} catch (...) {
		return false;
	}
}
size_t UniverseNetwork::PendingRequests()
{
	return (publishing ? 1 : 0) + (fetching ? 1 : 0);
}
void UniverseNetwork::Tick(uint64_t tick)
{
	if (!_networking || !_network_server || !FederationTransferManager::HasExternalAuthority()) return;
	const auto &url = FederationTransferManager::GetAuthorityUrl();
	if (publishing && publishing->IsFinished()) publishing.reset();
	if (fetching && fetching->IsFinished()) {
		if (fetching->Succeeded()) {
			try {
				std::map<std::string, json> records;
				for (const auto &host : fetching->GetResponse().at("hosts")) {
					std::string ns = host.at("namespace");
					json common{{"namespace", ns}, {"manifest", host.at("manifest")}, {"online", host.at("online")}};
					json h = common;
					h["kind"] = "host";
					h["address"] = host.at("address");
					h["companies"] = host.at("companies");
					records[ns] = h;
					for (const auto &kind : {"world", "station", "gate", "zone", "gateproject", "gatereply", "train"}) {
						for (const auto &entry : host.value(std::string(kind) + "s", json::array())) {
							json r = common;
							r.update(entry);
							r["kind"] = kind;
							std::string key = ns + "/" + kind + "/" +
											  (std::string(kind) == "gateproject" || std::string(kind) == "gatereply" || std::string(kind) == "train"
												   ? entry.at("id").get<std::string>()
												   : fmt::format("{}", entry.at(std::string(kind) == "station" ? "sequence" : "id").get<uint64_t>()));
							records[key] = r;
						}
					}
				}
				for (const auto &[key, r] : records)
					if (r.dump().size() <= 1200 &&
						(!directory.contains(key) || directory.at(key) != r || r.at("kind") == "gateproject" || r.at("kind") == "gatereply"))
						Command<Commands::UpdateUniverseRecord>::Post(key, r.dump());
				for (const auto &[key, r] : directory)
					if (!records.contains(key)) Command<Commands::UpdateUniverseRecord>::Post(key, "null");
			} catch (...) { /* Keep last directory; the next heartbeat retries. */
			}
		}
		if (!fetching->Succeeded())
			for (const auto &[key, r] : directory)
				if (r.value("online", false)) {
					auto offline = r;
					offline["online"] = false;
					Command<Commands::UpdateUniverseRecord>::Post(key, offline.dump());
				}
		fetching.reset();
	}
	if (FederationTransferManager::IsTransportQuiescing() || tick % 150 != 0) return;
	Command<Commands::EnsureUniverseIdentities>::Post(0);
	if (!publishing) {
		const char *token = std::getenv("OPENTTD_UNIVERSE_HOST_TOKEN");
		if (token && *token) {
			publishing = std::make_unique<AuthorityRequest>(url, AuthorityOperation::PublishUniverse, json{{"token", token}, {"host", Advertisement()}});
			publishing->Start();
		}
	}
	if (!fetching) {
		fetching = std::make_unique<AuthorityRequest>(url, AuthorityOperation::UniverseDirectory, json::object());
		fetching->Start();
	}
}
std::vector<std::string> UniverseNetwork::FindRoute(WorldID from, WorldID to, const std::string &owner, RailTypes railtypes)
{
	const auto manifest = Manifest();
	auto compatible = [&](const json &r) { return r.value("online", false) && r.value("manifest", "") == manifest; };
	struct Visit {
		uint32_t world;
		std::vector<std::string> path;
		int64_t toll;
	};
	std::vector<Visit> pending{{from.base(), {}, 0}};
	std::set<uint32_t> visited;
	auto rank = [](const Visit &v) { return std::tuple{v.path.size(), v.toll, v.path}; };
	while (!pending.empty()) {
		auto best = std::min_element(pending.begin(), pending.end(), [&](const Visit &a, const Visit &b) { return rank(a) < rank(b); });
		Visit current = *best;
		pending.erase(best);
		if (!visited.insert(current.world).second) continue;
		if (current.world == to.base()) return current.path;
		for (const auto &[key, g] : directory) {
			if (g.value("kind", "") != "gate" || !compatible(g) || g.at("world").get<uint32_t>() != current.world ||
				!railtypes.Test(static_cast<RailType>(g.at("rail").get<uint8_t>())) || (!g.value("public", false) && g.value("owner", "") != owner))
				continue;
			uint32_t destination = g.at("destination");
			bool online = false;
			for (const auto &[wk, w] : directory)
				if (w.value("kind", "") == "world" && w.at("id").get<uint32_t>() == destination && compatible(w)) online = true;
			if (!online || visited.contains(destination)) continue;
			auto next = current;
			next.world = destination;
			next.path.push_back(key);
			next.toll += g.value("owner", "") == owner ? 0 : g.at("toll").get<int64_t>();
			pending.push_back(std::move(next));
		}
	}
	return {};
}
TileIndex UniverseNetwork::Route(WorldID from, WorldID to, CompanyID company, RailTypes railtypes)
{
	auto route = FindRoute(from, to, CompanyIdentity(company), railtypes);
	if (route.empty()) return INVALID_TILE;
	const auto &first = directory.at(route.front());
	if (ParseNamespace(first.at("namespace")) != FederationIdentityRegistry::GetNamespace()) return INVALID_TILE;
	TileIndex tile{first.at("tile").get<uint32_t>()};
	return tile < Map::Size() && PortalRegistry::IsPortalTile(tile) ? tile : INVALID_TILE;
}
std::optional<StationID> UniverseNetwork::EnsureStation(const GlobalStationID &station)
{
	if (auto local = FederationIdentityRegistry::ResolveStation(station)) return local;
	if (station.name_space == FederationIdentityRegistry::GetNamespace() || !Station::CanAllocateItem()) return std::nullopt;
	const json *entry = nullptr;
	for (const auto &[key, r] : directory)
		if (r.value("kind", "") == "station" && StationIdentity(r) == station) {
			entry = &r;
			break;
		}
	if (!entry || !Compatible(*entry) || PlanetManager::GetAllRegions().empty()) return std::nullopt;
	const auto &world = PlanetManager::GetAllRegions().front();
	Station *proxy = Station::Create(TileXY(world.min_x, world.min_y));
	proxy->owner = OWNER_NONE;
	proxy->name = entry->at("name").get<std::string>() + " (remote)";
	proxy->town = ClosestTownFromTile(proxy->xy, UINT_MAX);
	proxy->facilities.Set(StationFacility::Train);
	FederationIdentityRegistry::RestoreStationMapping(proxy->index, station.sequence, station.name_space, station.world_id);
	return proxy->index;
}
bool UniverseNetwork::IsRemoteStation(StationID station)
{
	auto id = FederationIdentityRegistry::FindStation(station);
	return GateOrder(station).has_value() || (id && id->name_space != FederationIdentityRegistry::GetNamespace());
}
bool UniverseNetwork::FollowTrain(const std::string &key)
{
	auto it = directory.find(key);
	if (it == directory.end() || it->second.value("kind", "") != "train") return false;
	const auto &r = it->second;
	GlobalConsistID id{ParseNamespace(r.at("consist_namespace")), r.at("sequence").get<uint64_t>()};
	if (ParseNamespace(r.at("namespace")) == FederationIdentityRegistry::GetNamespace()) {
		for (const Train *t : Train::Iterate())
			if (t->IsFrontEngine() && FederationIdentityRegistry::Find(t) == id) {
				ShowVehicleViewWindow(t);
				ShowOrdersWindow(t);
				return true;
			}
		visit_status = "Train has departed; waiting for the directory to locate it.";
		return false;
	}
	visit_train = id;
	if (Visit(r.at("namespace"), WorldID{r.at("world").get<uint32_t>()})) return true;
	visit_train.reset();
	return false;
}
bool UniverseNetwork::Visit(const std::string &host, WorldID world)
{
	auto it = directory.find(host);
	if (it == directory.end() || !Compatible(it->second)) {
		visit_status = "Destination is offline or has incompatible content.";
		return false;
	}
	auto owner = FederationIdentityRegistry::FindCompany(_local_company);
	if (!owner) {
		visit_status = "Your company has no universe identity.";
		return false;
	}
	CompanyID target = CompanyID::Invalid();
	for (const auto &c : it->second.at("companies"))
		if (c.at("identity") == CompanyIdentity(_local_company)) target = CompanyID{c.at("local").get<uint8_t>()};
	if (target == CompanyID::Invalid()) {
		visit_status = "Your company has no construction rights on this host.";
		return false;
	}
	if (auto w = GetMainWindow(); w && w->viewport) {
		auto &vp = *w->viewport;
		if (const auto *region = PlanetManager::GetViewportCurrentPlanet(w))
			cameras[{Namespace(FederationIdentityRegistry::GetNamespace()), region->id}] = {vp.scrollpos_x, vp.scrollpos_y};
	}
	if (ParseNamespace(host) == FederationIdentityRegistry::GetNamespace()) {
		PlanetManager::JumpToPlanet(world);
		return true;
	}
	if (!_networking || _network_server) {
		visit_status = "Visit from a multiplayer client; the server must keep running.";
		return false;
	}
	visit_host = host;
	visit_world = world;
	visit_company = *owner;
	visit_status = "Connecting to destination world…";
	return NetworkClientConnectGame(it->second.at("address").get<std::string>(), target);
}
void UniverseNetwork::FinishVisit()
{
	if (visit_host.empty()) return;
	if (Namespace(FederationIdentityRegistry::GetNamespace()) != visit_host || FederationIdentityRegistry::FindCompany(_local_company) != visit_company) {
		visit_status = "Destination company or world identity did not match.";
		return;
	}
	PlanetManager::JumpToPlanet(visit_world);
	if (auto saved = cameras.find({visit_host, visit_world}); saved != cameras.end())
		if (auto w = GetMainWindow(); w && w->viewport) {
			auto &vp = *w->viewport;
			vp.follow_vehicle = VehicleID::Invalid();
			vp.scrollpos_x = vp.dest_scrollpos_x = saved->second.x;
			vp.scrollpos_y = vp.dest_scrollpos_y = saved->second.y;
		}
	visit_status.clear();
	visit_host.clear();
	ShowStellarNetwork();
	if (visit_train) {
		for (const Train *t : Train::Iterate())
			if (t->IsFrontEngine() && FederationIdentityRegistry::Find(t) == visit_train) {
				ShowVehicleViewWindow(t);
				ShowOrdersWindow(t);
				ShowUniverseDestinations(t->index);
				break;
			}
		visit_train.reset();
	}
}
const std::string &UniverseNetwork::VisitStatus()
{
	return visit_status;
}
bool UniverseNetwork::ScheduleEditable(const Vehicle *vehicle)
{
	if (!vehicle || vehicle->type != VehicleType::Train) return true;
	for (const Vehicle *v = vehicle->FirstShared(); v; v = v->NextShared())
		if (auto id = FederationIdentityRegistry::Find(Train::From(v))) {
			for (const auto &[key, rec] : TransferJournal::GetAll())
				if (rec.state == TransferCheckpointState::Prepared && rec.namespace_high == id->name_space.high && rec.namespace_low == id->name_space.low &&
					rec.consist_sequence == id->sequence && rec.request_id.starts_with("DEP-"))
					return false;
		}
	return true;
}
bool UniverseNetwork::HasUniverseOrders(const Vehicle *vehicle)
{
	if (!vehicle) return false;
	for (const auto &o : vehicle->Orders())
		if (o.IsType(OT_GOTO_STATION) && IsRemoteStation(o.GetDestination().ToStationID())) return true;
	return false;
}
void UniverseNetwork::ValidateAfterLoad()
{
	std::erase_if(gate_orders, [](const auto &entry) { return !Station::IsValidID(entry.first); });
}
uint64_t UniverseNetwork::OrderRevision(const Train *train)
{
	uint64_t hash = 14695981039346656037ULL;
	std::vector<uint8_t> bytes;
	EndianBufferWriter writer{bytes};
	if (auto id = FederationIdentityRegistry::Find(train)) writer << id->name_space.high << id->name_space.low << id->sequence;
	for (const auto &o : train->Orders())
		writer << o;
	for (uint8_t b : bytes) {
		hash ^= b;
		hash *= 1099511628211ULL;
	}
	return hash;
}
std::optional<GlobalOrderDestinationID> UniverseNetwork::GateOrder(StationID station)
{
	auto it = gate_orders.find(station);
	return it == gate_orders.end() ? std::nullopt : std::optional{it->second};
}
void UniverseNetwork::ReleaseStation(StationID station)
{
	gate_orders.erase(station);
}
std::optional<StationID> UniverseNetwork::EnsureGate(const GlobalOrderDestinationID &gate)
{
	for (const auto &[id, value] : gate_orders)
		if (value == gate && Station::IsValidID(id)) return id;
	if (!gate.IsValid() || gate.type != OrderDestinationType::PortalGate || !Station::CanAllocateItem() || PlanetManager::GetAllRegions().empty())
		return std::nullopt;
	const auto &world = PlanetManager::GetAllRegions().front();
	Station *proxy = Station::Create(TileXY(world.min_x, world.min_y));
	proxy->owner = OWNER_NONE;
	proxy->facilities.Set(StationFacility::Train);
	proxy->town = ClosestTownFromTile(proxy->xy, UINT_MAX);
	proxy->name = fmt::format("Via gate {} / world {}", gate.destination_sequence, gate.target_world.base());
	gate_orders[proxy->index] = gate;
	return proxy->index;
}
bool UniverseNetwork::IsPinnedGate(const GlobalOrderDestinationID &order, TileIndex tile)
{
	if (order.type != OrderDestinationType::PortalGate || order.name_space != FederationIdentityRegistry::GetNamespace() ||
		order.target_world != PlanetManager::GetTileWorld(tile))
		return false;
	auto link = PortalRegistry::GetInterServerPortal(tile);
	return order.destination_sequence == (link ? link->id.base() : tile.base());
}
void UniverseNetwork::AdvanceGateOrder(Train *train, TileIndex tile)
{
	if (!train->GetNumOrders()) return;
	auto order = train->GetOrder(train->cur_real_order_index);
	auto pin = order && order->IsType(OT_GOTO_STATION) ? GateOrder(order->GetDestination().ToStationID()) : std::nullopt;
	if (pin && IsPinnedGate(*pin, tile)) {
		train->IncrementRealOrderIndex();
		train->current_order.Free();
	}
}
std::string UniverseNetwork::Save()
{
	json pins = json::array();
	for (const auto &[id, g] : gate_orders)
		pins.push_back({id.base(), Namespace(g.name_space), g.destination_sequence, g.target_world.base()});
	return json{{"version", 1}, {"records", directory}, {"pins", pins}, {"gate_projects", RemoteGateProjects::Save()}}.dump();
}
bool UniverseNetwork::Load(const std::string &state)
{
	try {
		auto root = json::parse(state);
		if (root.at("version") != 1) return false;
		auto records = root.at("records");
		directory.clear();
		gate_orders.clear();
		for (auto it = records.begin(); it != records.end(); ++it)
			if (!Apply(it.key(), it.value())) return false;
		for (auto &row : root.at("pins")) {
			auto gate = GlobalOrderDestinationID::ForPortalGate(ParseNamespace(row.at(1)), row.at(2).get<uint64_t>(), WorldID{row.at(3).get<uint32_t>()});
			if (!gate.IsValid()) return false;
			gate_orders[StationID{row.at(0).get<uint16_t>()}] = gate;
		}
		return RemoteGateProjects::Load(root.at("gate_projects"));
	} catch (...) {
		return false;
	}
}
CommandCost CmdEnsureUniverseIdentities(DoCommandFlags flags, [[maybe_unused]] uint8_t unused)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		for (const Company *c : Company::Iterate())
			FederationIdentityRegistry::GetOrCreateCompany(c->index);
		for (const Train *t : Train::Iterate())
			if (t->IsFrontEngine()) FederationIdentityRegistry::GetOrCreate(t);
		for (const Station *st : Station::Iterate())
			if (!UniverseNetwork::IsRemoteStation(st->index)) FederationIdentityRegistry::GetOrCreateStation(st->index);
	}
	return CommandCost();
}
CommandCost CmdUpdateUniverseRecord(DoCommandFlags flags, const std::string &key, const std::string &record)
{
	if (record.size() > 1200 || key.size() > 128) return CMD_ERROR;
	try {
		auto value = json::parse(record);
		if (!UniverseNetwork::Apply(key, value, flags.Test(DoCommandFlag::Execute))) return CMD_ERROR;
		if (flags.Test(DoCommandFlag::Execute) && !value.is_null() && (value.value("kind", "") == "gateproject" || value.value("kind", "") == "gatereply"))
			RemoteGateProjects::Reconcile(value);
		return CommandCost();
	} catch (...) {
		return CMD_ERROR;
	}
}
CommandCost CmdAddUniverseOrder(DoCommandFlags flags, VehicleID vehicle, uint64_t expected_revision, const std::string &station)
{
	Train *train = Train::GetIfValid(vehicle);
	auto it = UniverseNetwork::Directory().find(station);
	if (!train || train->type != VehicleType::Train || !train->IsFrontEngine() || train->owner != _current_company ||
		UniverseNetwork::OrderRevision(train) != expected_revision || !FederationIdentityRegistry::Find(train) || train->GetNumOrders() >= 255 ||
		train->IsOrderListShared() || it == UniverseNetwork::Directory().end() ||
		(it->second.value("kind", "") != "station" && it->second.value("kind", "") != "gate") || !Compatible(it->second))
		return CMD_ERROR;
	const bool pin = it->second.value("kind", "") == "gate";
	if (it->second.value("owner", "") != CompanyIdentity(_current_company) && !(pin && it->second.value("public", false))) return CMD_ERROR;
	if (!UniverseNetwork::ScheduleEditable(train)) return CMD_ERROR;
	if (!Station::CanAllocateItem() || (!train->orders && !OrderList::CanAllocateItem())) return CMD_ERROR;
	if (flags.Test(DoCommandFlag::Execute)) {
		auto destination = pin ? UniverseNetwork::EnsureGate(GlobalOrderDestinationID::ForPortalGate(
									 UniverseNetwork::ParseNamespace(it->second.at("namespace")), it->second.at("id").get<uint64_t>(),
									 WorldID{it->second.at("world").get<uint32_t>()}))
							   : UniverseNetwork::EnsureStation(StationIdentity(it->second));
		if (!destination) return CMD_ERROR;
		Order order;
		order.MakeGoToStation(*destination);
		InsertOrder(train, std::move(order), train->GetNumOrders());
		auto id = FederationIdentityRegistry::GetOrCreate(train);
		std::vector<GlobalOrderDestinationID> schedule;
		for (const auto &o : train->Orders()) {
			auto global = FederationIdentityRegistry::GetOrCreateOrderDestination(o.GetDestination(), o.GetType());
			if (global) schedule.push_back(*global);
		}
		if (id) FederationIdentityRegistry::SetConsistSchedule(*id, std::move(schedule));
		InvalidateWindowData(WindowClass::VehicleOrders, vehicle);
	}
	return CommandCost();
}

/** Mapping is an administrator provisioning action, before trains or gate projects exist. */
CommandCost CmdMapUniverseCompany(DoCommandFlags flags, CompanyID company, const std::string &name_space, uint64_t sequence)
{
	auto ns = UniverseNetwork::ParseNamespace(name_space);
	if (!Company::IsValidID(company) || !ns.IsValid() || sequence == 0 || sequence == UINT64_MAX) return CMD_ERROR;
	GlobalCompanyID identity{ns, sequence};
	if (FederationIdentityRegistry::FindCompany(company) == identity) return CommandCost();
	for (const Company *c : Company::Iterate())
		if (c->index != company && FederationIdentityRegistry::FindCompany(c->index) == identity) return CMD_ERROR;
	for (const Train *t : Train::Iterate())
		if (t->owner == company) return CMD_ERROR;
	if (!TransferJournal::GetAll().empty() || !RemoteGateProjects::Advertisements(false).empty() || !RemoteGateProjects::Advertisements(true).empty())
		return CMD_ERROR;
	for (const auto &[id, p] : StellarNetwork::Projects())
		if (p.owner == company) return CMD_ERROR;
	if (flags.Test(DoCommandFlag::Execute)) FederationIdentityRegistry::RestoreCompanyMapping(company, sequence, ns);
	return CommandCost();
}
bool ConUniverseCompanyMap(std::span<std::string_view> argv)
{
	if (argv.size() != 4) {
		IConsolePrint(CC_HELP,
					  "universe_company_map <company number, 1-based> <namespace hex:hex> <sequence>: map a fresh local company; requires server console");
		for (const Company *c : Company::Iterate())
			IConsolePrint(CC_DEFAULT, "Company {}: {}", c->index.base() + 1, CompanyIdentity(c->index));
		return true;
	}
	uint32_t company = 0;
	uint64_t sequence = 0;
	auto cp = std::from_chars(argv[1].data(), argv[1].data() + argv[1].size(), company);
	auto sp = std::from_chars(argv[3].data(), argv[3].data() + argv[3].size(), sequence);
	if (cp.ec != std::errc{} || cp.ptr != argv[1].data() + argv[1].size() || sp.ec != std::errc{} || sp.ptr != argv[3].data() + argv[3].size() || company < 1 ||
		company > MAX_COMPANIES)
		return false;
	bool result = Command<Commands::MapUniverseCompany>::Post(CompanyID{static_cast<uint8_t>(company - 1)}, std::string(argv[2]), sequence);
	IConsolePrint(result ? CC_DEFAULT : CC_ERROR, result ? "Universe company mapping command posted" : "Universe company mapping rejected");
	return result;
}
