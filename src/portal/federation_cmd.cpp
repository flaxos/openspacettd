/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_cmd.cpp Federation inter-server transfer commands and coordinator implementation. */

#include "../stdafx.h"
#include "federation_cmd.h"
#include "authority_transport.h"
#include "authority_request_queue.h"
#include "consist_materializer.h"
#include "federation_identity.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "transfer_journal.h"
#include "universe_authority.h"
#include "federation_staging.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../settings_type.h"
#include "../train.h"
#include "../strings_func.h"
#include "../timer/timer_game_tick.h"
#include "../table/strings.h"
#include "../debug.h"
#include "../command_func.h"
#include "../network/network.h"
#include "../tunnelbridge_map.h"
#include "../tunnel_map.h"
#include "../pathfinder/yapf/yapf_cache.h"
#include "../engine_base.h"
#include "../window_func.h"
#include "../vehicle_func.h"


#include <set>
#include <cstdlib>

#include "../safeguards.h"

static std::string _authority_url;
static std::set<std::string> _network_departures_sent;
static AuthorityRequestQueue _network_authority_requests;
static std::map<WorldID, std::set<std::string>> _network_pending_transfers;
static void PollNetworkFederation(uint64_t current_tick);

void FederationTransferManager::SetTransportQuiescing(bool quiescing)
{
	_network_authority_requests.SetQuiescing(quiescing);
}

bool FederationTransferManager::IsTransportQuiescing()
{
	return _network_authority_requests.IsQuiescing();
}

size_t FederationTransferManager::PendingAuthorityRequests()
{
	return _network_authority_requests.PendingCount();
}

void FederationTransferManager::SetAuthorityUrl(std::string url)
{
	if (_authority_url != url) _network_authority_requests.Reset();
	_authority_url = std::move(url);
}

const std::string &FederationTransferManager::GetAuthorityUrl()
{
	if (_authority_url.empty()) {
		const char *env = std::getenv("OPENSPACETTD_AUTHORITY_URL");
		if (env != nullptr && *env != '\0') {
			_authority_url = env;
		} else if (!_settings_client.network.universe_authority_url.empty()) {
			_authority_url = _settings_client.network.universe_authority_url;
		}
	}
	return _authority_url;
}

bool FederationTransferManager::HasExternalAuthority()
{
	return !GetAuthorityUrl().empty();
}

void FederationTransferManager::Reset()
{
	UniverseAuthorityService::Instance().Reset();
	TransferJournal::Reset();
	_authority_url.clear();
	_network_departures_sent.clear();
	_network_pending_transfers.clear();
	_network_authority_requests.Reset();
}

bool FederationTransferManager::InitiateConsistDeparture(Train *consist, TileIndex portal_tile)
{
	if (consist == nullptr || portal_tile == INVALID_TILE) return false;

	const InterServerPortalLink *link = PortalRegistry::GetInterServerPortal(portal_tile);
	if (link == nullptr || !link->IsValid()) return false;

	Train *front = consist->First();
	if (front == nullptr) return false;

	/* Derive owner token */
	auto global_comp = FederationIdentityRegistry::FindCompany(front->owner);
	GlobalOwnerToken owner_token = global_comp.has_value() ? global_comp->ToOwnerToken() : GlobalOwnerToken{};

	uint32_t transit_ticks = std::max<uint32_t>(10, link->virtual_length * 20);

	/* Get or create GlobalConsistID */
	auto cid_opt = FederationIdentityRegistry::GetOrCreate(front);
	GlobalConsistID cid = cid_opt.has_value() ? *cid_opt : GlobalConsistID{};

	/* Every peer freezes the same train at natural gate entry and prepares the
	 * same snapshot. Only server-authored commands may subsequently remove it. */
	if (_networking) {
		for (const auto &[key, existing] : TransferJournal::GetAll()) {
			if (existing.state == TransferCheckpointState::Prepared && existing.request_id.starts_with("DEP-") &&
					existing.namespace_high == cid.name_space.high && existing.namespace_low == cid.name_space.low &&
					existing.consist_sequence == cid.sequence) return false;
		}
	}

	std::string request_id = fmt::format("DEP-W{}-G{}-N{:x}:{:x}-C{}-T{}",
		link->local_endpoint.world_id.base(), link->id.base(),
		cid.name_space.high, cid.name_space.low, cid.sequence, TimerGameTick::counter);

	ConsistDespawnResult capture_res = ConsistMaterializer::CaptureForTransfer(front, owner_token);
	if (!capture_res.success) return false;

	const ConsistSnapshot &snapshot = capture_res.snapshot;
	const ConsistSnapshotBytes &snapshot_bytes = capture_res.snapshot_bytes;

	/* 1. Record Prepared checkpoint while the physical consist still exists. */
	TransferCheckpoint cp;
	cp.request_id = request_id;
	cp.namespace_high = cid.name_space.high;
	cp.namespace_low = cid.name_space.low;
	cp.consist_sequence = cid.sequence;
	cp.source_world = link->local_endpoint.world_id.base();
	cp.destination_world = link->remote_world.base();
	cp.source_gate_id = link->id.base();
	cp.destination_gate_id = link->remote_gate_id;
	cp.snapshot = snapshot_bytes.bytes;
	cp.state = TransferCheckpointState::Prepared;
	if (!TransferJournal::Prepare(cp)) return false;
	if (_networking) return false; // Wait for the authority-admitted replicated departure.


	std::string tx_id;
	if (HasExternalAuthority()) {
		/* Build cargo breakdown JSON */
		uint32_t total_cargo = 0;
		nlohmann::json breakdown = nlohmann::json::object();
		for (const auto &unit : snapshot.units) {
			total_cargo += unit.cargo_count;
			if (unit.cargo_count > 0) {
				std::string c_key = fmt::format("{}", unit.cargo_type);
				breakdown[c_key] = breakdown.value(c_key, 0) + unit.cargo_count;
			}
		}

		/* Build orders JSON */
		nlohmann::json orders_json = nlohmann::json::array();
		for (const auto &dest : snapshot.orders) {
			orders_json.push_back({
				{"type", static_cast<uint8_t>(dest.type)},
				{"dest_seq", dest.destination_sequence},
				{"target_world", dest.target_world.base()},
			});
		}

		nlohmann::json payload = {
			{"request_id", request_id},
			{"source_world", link->local_endpoint.world_id.base()},
			{"dest_world", link->remote_world.base()},
			{"source_gate", link->id.base()},
			{"dest_gate", link->remote_gate_id},
			{"snapshot_base64", Base64Encode(snapshot_bytes.bytes)},
			{"total_cargo", total_cargo},
			{"cargo_breakdown", breakdown},
			{"orders", orders_json},
			{"current_order_index", snapshot.current_order_index},
			{"consist_id", fmt::format("{:x}:{:x}:{}", cid.name_space.high, cid.name_space.low, cid.sequence)},
			{"transit_delay_sec", static_cast<double>(link->virtual_length) * 0.2 + 0.5},
			{"priority", "STANDARD"}
		};

		AuthorityRequest init_req(GetAuthorityUrl(), AuthorityOperation::Initiate, payload, link->local_endpoint.world_id.base());
		if (!init_req.ExecuteSync()) return false;
		const auto &resp = init_req.GetResponse();
		if (!resp.contains("transfer_id")) return false;
		tx_id = resp["transfer_id"].get<std::string>();

		if (!TransferJournal::BindTransfer(link->local_endpoint.world_id.base(), request_id, tx_id)) return false;

		AuthorityRequest dep_req(GetAuthorityUrl(), AuthorityOperation::Depart, {{"transfer_id", tx_id}}, link->local_endpoint.world_id.base());
		if (!dep_req.ExecuteSync()) return false;
	} else {
		/* In-memory service fallback */
		tx_id = UniverseAuthorityService::Instance().InitiateTransfer(
			link->local_endpoint.world_id,
			link->remote_world,
			link->id.base(),
			link->remote_gate_id,
			snapshot_bytes,
			transit_ticks,
			FreightPriority::Standard,
			request_id
		);

		if (tx_id.empty()) return false;
		if (!TransferJournal::BindTransfer(link->local_endpoint.world_id.base(), request_id, tx_id)) return false;

		if (!UniverseAuthorityService::Instance().DepartTransfer(tx_id, TimerGameTick::counter)) return false;
	}

	if (!ConsistMaterializer::ReleaseCapturedConsist(front)) return false;
	if (!TransferJournal::MarkDeparted(link->local_endpoint.world_id.base(), request_id)) return false;
	Debug(net, 1, "[Federation] Consist departed: transfer_id={}, request_id={}, source_world={}, dest_world={}",
		tx_id, request_id, link->local_endpoint.world_id.base(), link->remote_world.base());
	return true;
}

size_t FederationTransferManager::ProcessIncomingTransfers(WorldID local_world, uint64_t current_tick)
{
	if (local_world == INVALID_WORLD || (_networking && !_network_server)) return 0;

	size_t materialized_count = 0;

	if (HasExternalAuthority()) {
		AuthorityRequest pend_req(GetAuthorityUrl(), AuthorityOperation::Pending, nlohmann::json::object(), local_world.base());
		if (!pend_req.ExecuteSync()) return 0;
		const auto &p_resp = pend_req.GetResponse();

		std::vector<std::string> pending;
		if (p_resp.contains("pending_transfers") && p_resp["pending_transfers"].is_array()) {
			for (const auto &item : p_resp["pending_transfers"]) pending.push_back(item.get<std::string>());
		} else if (p_resp.contains("pending") && p_resp["pending"].is_array()) {
			for (const auto &item : p_resp["pending"]) pending.push_back(item.get<std::string>());
		}

		for (const std::string &tx_id : pending) {
			/* Deduplication: prevent double materialization if already arrived */
			const auto *existing_cp = TransferJournal::FindByTransferId(tx_id);
			if (existing_cp != nullptr && (existing_cp->state == TransferCheckpointState::Materialized ||
					existing_cp->state == TransferCheckpointState::Confirmed)) {
				AuthorityRequest conf_dup(GetAuthorityUrl(), AuthorityOperation::Confirm, {
					{"transfer_id", tx_id},
					{"dest_world", local_world.base()},
					{"success", true},
					{"arrival_receipt", existing_cp->arrival_receipt}
				}, local_world.base());
				conf_dup.ExecuteSync();
				continue;
			}

			/* Claim transfer */
			AuthorityRequest claim_req(GetAuthorityUrl(), AuthorityOperation::Claim, {
				{"transfer_id", tx_id},
				{"dest_world", local_world.base()}
			}, local_world.base());
			if (!claim_req.ExecuteSync()) continue;
			const auto &claim_data = claim_req.GetResponse();

			uint32_t dest_gate = claim_data.value("dest_gate", claim_data.value("dest_gate_id", 0));
			TileIndex dest_tile = INVALID_TILE;
			DiagDirection enter_dir = DiagDirection::Begin;

			for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
				if (link.local_endpoint.world_id == local_world && link.id.base() == dest_gate) {
					dest_tile = tile;
					enter_dir = link.local_endpoint.enter_dir;
					break;
				}
			}

			if (dest_tile == INVALID_TILE) {
				continue;
			}

			TileIndex clear_dest = ConsistMaterializer::ResolveClearThroat(dest_tile, enter_dir);
			if (clear_dest == INVALID_TILE) {
				continue;
			}
			dest_tile = clear_dest;

			std::string snap_b64 = claim_data.value("snapshot_base64", "");
			auto snap_bytes = Base64Decode(snap_b64);
			auto dec_res = ConsistSnapshotCodec::DecodeForCurrentContent(snap_bytes);
			if (!dec_res.Succeeded()) {
				AuthorityRequest conf_fail(GetAuthorityUrl(), AuthorityOperation::Confirm, {
					{"transfer_id", tx_id},
					{"dest_world", local_world.base()},
					{"success", false},
					{"reason", "Content admission rejected: manifest mismatch"}
				}, local_world.base());
				conf_fail.ExecuteSync();
				continue;
			}

			ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(
				*dec_res.snapshot, dest_tile, enter_dir
			);

			if (mat_res.success) {
				std::string receipt_id = fmt::format("RCPT-W{}-{}-{}", local_world.base(), tx_id, current_tick);
				std::string req_id = fmt::format("ARR-W{}-{}", local_world.base(), tx_id);
				TransferCheckpoint rcpt;
				rcpt.request_id = req_id;
				rcpt.transfer_id = tx_id;
				rcpt.arrival_receipt = receipt_id;
				rcpt.namespace_high = dec_res.snapshot->consist_id.name_space.high;
				rcpt.namespace_low = dec_res.snapshot->consist_id.name_space.low;
				rcpt.consist_sequence = dec_res.snapshot->consist_id.sequence;
				rcpt.source_world = claim_data.value("source_world", 0);
				rcpt.destination_world = local_world.base();
				rcpt.source_gate_id = claim_data.value("source_gate", claim_data.value("source_gate_id", 0));
				rcpt.destination_gate_id = dest_gate;
				rcpt.state = TransferCheckpointState::Materialized;
				rcpt.snapshot = snap_bytes;
				TransferJournal::RecordArrival(rcpt);

				AuthorityRequest conf_ok(GetAuthorityUrl(), AuthorityOperation::Confirm, {
					{"transfer_id", tx_id},
					{"dest_world", local_world.base()},
					{"success", true},
					{"arrival_receipt", receipt_id},
					{"advance_order", true}
				}, local_world.base());
				if (conf_ok.ExecuteSync()) {
					TransferJournal::ConfirmArrival(rcpt.source_world, req_id, receipt_id);
				}
				Debug(net, 1, "[Federation] Consist materialized: transfer_id={}, receipt={}, dest_world={}, gate={}",
					tx_id, receipt_id, local_world.base(), dest_gate);
				materialized_count++;
			} else {
				AuthorityRequest conf_fail(GetAuthorityUrl(), AuthorityOperation::Confirm, {
					{"transfer_id", tx_id},
					{"dest_world", local_world.base()},
					{"success", false},
					{"reason", mat_res.error_message}
				}, local_world.base());
				conf_fail.ExecuteSync();
			}
		}
	} else {
		/* In-memory service logic */
		auto &authority = UniverseAuthorityService::Instance();
		authority.ReconcileFromJournal(current_tick);
		std::vector<std::string> pending = authority.QueryPendingTransfers(local_world, current_tick);

		for (const std::string &tx_id : pending) {
			const auto *existing_cp = TransferJournal::FindByTransferId(tx_id);
			if (existing_cp != nullptr && (existing_cp->state == TransferCheckpointState::Materialized ||
					existing_cp->state == TransferCheckpointState::Confirmed)) {
				if (authority.ConfirmTransferArrival(tx_id, local_world, true)) {
					TransferJournal::ConfirmArrival(existing_cp->source_world, existing_cp->request_id, existing_cp->arrival_receipt);
				}
				continue;
			}

			const UniverseTransferRecord *existing = authority.GetTransfer(tx_id);
			auto claim_opt = existing != nullptr && existing->dest_world == local_world &&
					existing->state == TransferState::ArrivalPending
					? std::optional<UniverseTransferRecord>(*existing)
					: authority.ClaimTransfer(tx_id, local_world);
			if (!claim_opt.has_value()) continue;

			const UniverseTransferRecord &rec = *claim_opt;

			TileIndex dest_tile = INVALID_TILE;
			DiagDirection enter_dir = DiagDirection::Begin;

			for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
				if (link.local_endpoint.world_id == local_world && link.id.base() == rec.dest_gate_id) {
					dest_tile = tile;
					enter_dir = link.local_endpoint.enter_dir;
					break;
				}
			}

			if (dest_tile == INVALID_TILE) {
				continue;
			}

			TileIndex clear_dest = ConsistMaterializer::ResolveClearThroat(dest_tile, enter_dir);
			if (clear_dest == INVALID_TILE) {
				continue;
			}
			dest_tile = clear_dest;

			ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(
				rec.snapshot, dest_tile, enter_dir
			);

			if (mat_res.success) {
				const std::string receipt_id = fmt::format("RCPT-W{}-{}", local_world.base(), tx_id);
				const std::string req_id = fmt::format("ARR-W{}-{}", local_world.base(), tx_id);
				TransferCheckpoint rcpt;
				rcpt.request_id = req_id;
				rcpt.transfer_id = tx_id;
				rcpt.arrival_receipt = receipt_id;
				rcpt.namespace_high = rec.snapshot.consist_id.name_space.high;
				rcpt.namespace_low = rec.snapshot.consist_id.name_space.low;
				rcpt.consist_sequence = rec.snapshot.consist_id.sequence;
				rcpt.source_world = rec.source_world.base();
				rcpt.destination_world = local_world.base();
				rcpt.source_gate_id = rec.source_gate_id;
				rcpt.destination_gate_id = rec.dest_gate_id;
				rcpt.state = TransferCheckpointState::Materialized;
				rcpt.snapshot = rec.snapshot_bytes.bytes;
				if (!TransferJournal::RecordArrival(rcpt)) continue;
				if (authority.ConfirmTransferArrival(tx_id, local_world, true)) {
					TransferJournal::ConfirmArrival(rcpt.source_world, req_id, receipt_id);
				}
				materialized_count++;
			} else {
				authority.ConfirmTransferArrival(
					tx_id, local_world, false, mat_res.error_message
				);
			}
		}
	}

	return materialized_count;
}

void FederationTransferManager::OnGameTick(uint64_t current_tick)
{
	if (_networking) {
		if (_network_server && (current_tick % 10) == 0) PollNetworkFederation(current_tick);
		return;
	}
	/* Staging manager runs on the faster cadence (every 10 ticks / ~330ms). */
	if ((current_tick % 10) == 0) {
		FederationStagingManager::OnGameTick(current_tick);
	}

	/* Authority HTTP polling runs on a slower cadence (every 150 ticks / ~5s)
	 * to avoid hammering the external authority with rapid synchronous requests.
	 * Each ExecuteSync call blocks the game loop until the HTTP roundtrip completes,
	 * so high frequency polling degrades simulation throughput and can cause
	 * concurrent HTTP state corruption in the curl thread. */
	if ((current_tick % 150) != 0) return;

	std::set<WorldID> local_worlds;
	for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
		if (link.local_endpoint.world_id != INVALID_WORLD) {
			local_worlds.insert(link.local_endpoint.world_id);
		}
	}
	for (const auto &reg : PlanetManager::GetAllRegions()) {
		if (reg.id != INVALID_WORLD) {
			local_worlds.insert(reg.id);
		}
	}
	if (local_worlds.empty() && HasExternalAuthority()) {
		local_worlds.insert(WorldID{1});
	}

	for (WorldID wid : local_worlds) {
		try {
			ProcessIncomingTransfers(wid, current_tick);
		} catch (const std::exception &e) {
			Debug(net, 0, "[Federation] Exception in ProcessIncomingTransfers for world {}: {}", wid.base(), e.what());
		} catch (...) {
			Debug(net, 0, "[Federation] Unknown exception in ProcessIncomingTransfers for world {}", wid.base());
		}
	}
}

CommandCost CmdDispatchInterServerTransfer(DoCommandFlags flags, VehicleID vehicle_id, TileIndex portal_tile)
{
	Train *v = Train::GetIfValid(vehicle_id);
	if (v == nullptr || !v->IsFrontEngine()) {
		return CommandCost(STR_ERROR_TRAIN_IN_THE_WAY);
	}

	if (!PortalRegistry::IsInterServerPortal(portal_tile)) {
		return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		if (FederationStagingManager::CheckAndDivertToStaging(v, portal_tile)) {
			return CommandCost();
		}
		if (!FederationTransferManager::InitiateConsistDeparture(v, portal_tile)) {
			return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
		}
	}

	return CommandCost();
}

/** Resolve a prepared source consist without allocating a new identity. */
static Train *FindPreparedConsist(const TransferCheckpoint &cp)
{
	for (Train *train : Train::Iterate()) {
		if (!train->IsFrontEngine()) continue;
		auto id = FederationIdentityRegistry::Find(train);
		if (id.has_value() && id->sequence == cp.consist_sequence && id->name_space.high == cp.namespace_high &&
				id->name_space.low == cp.namespace_low) return train;
	}
	return nullptr;
}

CommandCost CmdConfigureFederationGate(DoCommandFlags flags, TileIndex tile, uint32_t remote_world, uint32_t remote_gate,
	uint32_t local_gate, uint32_t length, uint32_t local_world)
{
	if (!IsValidTile(tile) || !IsTunnelTile(tile) || (!PortalRegistry::IsPortalTile(tile) && !PortalRegistry::IsUnlinkedGate(tile)) ||
			remote_world == INVALID_WORLD.base() || local_world == INVALID_WORLD.base() ||
			remote_gate == 0 || length == 0 || length > 100000 || local_gate == UINT32_MAX ||
			PortalRegistry::IsPortalInTransit(tile)) return CMD_ERROR;
	for (const auto &[other, link] : PortalRegistry::GetAllInterServerPortals()) {
		if (other != tile && local_gate != 0 && link.id.base() == local_gate) return CMD_ERROR;
	}
	for (const Vehicle *vehicle : VehiclesOnTile(tile)) {
		if (vehicle->type == VehicleType::Train) return CMD_ERROR;
	}
	if (flags.Test(DoCommandFlag::Execute)) {
		std::optional<PortalEndpoint> opposite;
		if (const auto *pair = PortalRegistry::GetPortalLink(tile); pair != nullptr) {
			opposite = pair->end_a.tile == tile ? pair->end_b : pair->end_a;
		}
		const DiagDirection dir = GetTunnelBridgeDirection(tile);
		PortalRegistry::UnregisterPortalByTile(tile);
		if (opposite.has_value()) {
			PortalRegistry::RegisterUnlinkedGate(opposite->tile, opposite->enter_dir, opposite->world_id);
			YapfNotifyTrackLayoutChange(opposite->tile, DiagDirToDiagTrack(opposite->enter_dir));
		}
		const PortalID id = PortalRegistry::RegisterInterServerPortal(tile, dir, WorldID{local_world}, WorldID{remote_world}, remote_gate, length, local_gate);
		assert(id != INVALID_PORTAL);
		YapfNotifyTrackLayoutChange(tile, DiagDirToDiagTrack(dir));
		Debug(net, 1, "Registered inter-server portal link (ID {}): Tile {} (World {}) -> Remote World {}, Gate {}",
			id.base(), tile.base(), local_world, remote_world, remote_gate);
	}
	return CommandCost();
}

CommandCost CmdCommitFederationDeparture(DoCommandFlags flags, uint32_t source_world, const std::string &request_id, const std::string &transfer_id)
{
	const auto *cp = TransferJournal::Find(source_world, request_id);
	if (cp == nullptr || !request_id.starts_with("DEP-") || transfer_id.empty() || transfer_id.size() > 128) return CMD_ERROR;
	if (cp->state == TransferCheckpointState::Departed) return cp->transfer_id == transfer_id ? CommandCost() : CMD_ERROR;
	Train *train = FindPreparedConsist(*cp);
	if (cp->state != TransferCheckpointState::Prepared || train == nullptr ||
			(!cp->transfer_id.empty() && cp->transfer_id != transfer_id)) return CMD_ERROR;
	if (flags.Test(DoCommandFlag::Execute)) {
		TransferJournal::BindTransfer(source_world, request_id, transfer_id);
		ConsistMaterializer::ReleaseCapturedConsist(train);
		TransferJournal::MarkDeparted(source_world, request_id);
		Debug(net, 1, "[Federation] Consist departed: transfer_id={}, source_world={}, dest_world={}", transfer_id, source_world, cp->destination_world);
	}
	return CommandCost();
}

/** Parse bounded server-authored metadata; snapshot data travels in separate MTU-sized fragments. */
static std::optional<TransferCheckpoint> ReadArrivalMetadata(const std::string &metadata)
{
	if (metadata.size() > 600) return std::nullopt;
	try {
		const auto json = nlohmann::json::parse(metadata);
		TransferCheckpoint cp;
		cp.transfer_id = json.at("tx").get<std::string>();
		cp.source_world = json.at("sw").get<uint32_t>();
		cp.destination_world = json.at("dw").get<uint32_t>();
		cp.source_gate_id = json.at("sg").get<uint32_t>();
		cp.destination_gate_id = json.at("dg").get<uint32_t>();
		cp.namespace_high = json.at("nh").get<uint64_t>();
		cp.namespace_low = json.at("nl").get<uint64_t>();
		cp.consist_sequence = json.at("seq").get<uint64_t>();
		cp.request_id = fmt::format("ARR-W{}-{}", cp.destination_world, cp.transfer_id);
		cp.snapshot = {0}; // IsValid requires nonempty bytes; replaced by the actual first fragment.
		if (cp.transfer_id.empty() || cp.transfer_id.size() > 80 || !cp.IsValid()) return std::nullopt;
		return cp;
	} catch (const nlohmann::json::exception &) {
		return std::nullopt;
	}
}

CommandCost CmdStageFederationArrival(DoCommandFlags flags, const std::string &metadata, uint32_t offset, const std::string &fragment)
{
	auto parsed = ReadArrivalMetadata(metadata);
	if (!parsed.has_value() || fragment.empty() || fragment.size() > 512 || offset > 1024 * 1024) return CMD_ERROR;
	auto bytes = Base64Decode(fragment);
	if (bytes.empty() || bytes.size() > 384 || offset > 1024 * 1024 - bytes.size() || Base64Encode(bytes) != fragment) return CMD_ERROR;
	auto cp = *parsed;
	const auto *existing = TransferJournal::Find(cp.source_world, cp.request_id);
	if (existing != nullptr) {
		if (existing->transfer_id != cp.transfer_id || existing->namespace_high != cp.namespace_high ||
				existing->namespace_low != cp.namespace_low || existing->consist_sequence != cp.consist_sequence ||
				existing->destination_world != cp.destination_world || existing->source_gate_id != cp.source_gate_id ||
				existing->destination_gate_id != cp.destination_gate_id) return CMD_ERROR;
		if (offset > existing->snapshot.size()) return CMD_ERROR;
		if (offset < existing->snapshot.size()) {
			return bytes.size() <= existing->snapshot.size() - offset &&
				std::equal(bytes.begin(), bytes.end(), existing->snapshot.begin() + offset) ? CommandCost() : CMD_ERROR;
		}
		if (existing->state != TransferCheckpointState::Prepared) return CMD_ERROR;
	} else if (offset != 0) {
		return CMD_ERROR;
	}
	if (flags.Test(DoCommandFlag::Execute)) {
		if (existing == nullptr) {
			const auto transfer_id = cp.transfer_id;
			cp.transfer_id.clear();
			cp.snapshot = bytes;
			TransferJournal::Prepare(cp);
			TransferJournal::BindTransfer(cp.source_world, cp.request_id, transfer_id);
		} else {
			TransferJournal::AppendArrivalFragment(cp.source_world, cp.request_id, offset, bytes);
		}
	}
	return CommandCost();
}

CommandCost CmdMaterializeFederationArrival(DoCommandFlags flags, const std::string &transfer_id)
{
	const auto *cp = TransferJournal::FindByTransferId(transfer_id);
	if (cp == nullptr || !cp->request_id.starts_with("ARR-")) return CMD_ERROR;
	if (TransferJournal::HasArrival(transfer_id)) return CommandCost();
	auto decoded = ConsistSnapshotCodec::DecodeForCurrentContent(cp->snapshot);
	if (!decoded.Succeeded()) return CMD_ERROR;
	const auto &snapshot = *decoded.snapshot;
	if (snapshot.consist_id.sequence != cp->consist_sequence || snapshot.consist_id.name_space.high != cp->namespace_high ||
			snapshot.consist_id.name_space.low != cp->namespace_low) return CMD_ERROR;
	const InterServerPortalLink *destination = nullptr;
	for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) {
		if (link.id.base() == cp->destination_gate_id && link.local_endpoint.world_id.base() == cp->destination_world) destination = &link;
	}
	if (destination == nullptr || !ConsistMaterializer::CheckThroatClearance(destination->local_endpoint.tile, destination->local_endpoint.enter_dir) ||
			!Vehicle::CanAllocateItem(snapshot.units.size()) || !CargoPacket::CanAllocateItem(snapshot.units.size())) return CMD_ERROR;
	for (const auto &unit : snapshot.units) {
		if (!Engine::IsValidID(EngineID{unit.engine_type})) return CMD_ERROR;
	}
	if (flags.Test(DoCommandFlag::Execute)) {
		auto result = ConsistMaterializer::MaterializeFromTransfer(snapshot, destination->local_endpoint.tile, destination->local_endpoint.enter_dir);
		if (!result.success) return CMD_ERROR;
		TransferJournal::MarkMaterialized(cp->source_world, cp->request_id, fmt::format("RCPT-W{}-{}", cp->destination_world, transfer_id));
		Debug(net, 1, "[Federation] Consist materialized: transfer_id={}, dest_world={}, gate={}, cargo={}", transfer_id, cp->destination_world, cp->destination_gate_id, result.total_cargo);
	}
	return CommandCost();
}

CommandCost CmdConfirmFederationArrival(DoCommandFlags flags, const std::string &transfer_id)
{
	const auto *cp = TransferJournal::FindByTransferId(transfer_id);
	if (cp == nullptr || !TransferJournal::HasArrival(transfer_id)) return CMD_ERROR;
	if (flags.Test(DoCommandFlag::Execute)) TransferJournal::ConfirmArrival(cp->source_world, cp->request_id, cp->arrival_receipt);
	return CommandCost();
}

/** Only the server reads wall-clock HTTP results. All shared mutations above use native commands. */
static void PollNetworkFederation([[maybe_unused]] uint64_t current_tick)
{
	if (!FederationTransferManager::HasExternalAuthority()) return;
	const auto &url = FederationTransferManager::GetAuthorityUrl();
	_network_authority_requests.Sweep();
	/* Copy: command execution can change the journal in a non-network test harness. */
	const auto records = TransferJournal::GetAll();
	for (const auto &[key, cp] : records) {
		if (cp.request_id.starts_with("DEP-")) {
			if (cp.state == TransferCheckpointState::Departed && !_network_departures_sent.contains(cp.transfer_id)) {
				auto depart = _network_authority_requests.Poll(url, AuthorityOperation::Depart, cp.transfer_id, {{"transfer_id", cp.transfer_id}}, cp.source_world);
				if (depart) _network_departures_sent.insert(cp.transfer_id);
			} else if (cp.state == TransferCheckpointState::Prepared) {
				auto decoded = ConsistSnapshotCodec::DecodeForCurrentContent(cp.snapshot);
				if (!decoded.Succeeded()) continue;
				uint32_t cargo = 0;
				nlohmann::json breakdown = nlohmann::json::object();
				for (const auto &unit : decoded.snapshot->units) {
					cargo += unit.cargo_count;
					if (unit.cargo_count != 0) {
						const auto type = fmt::format("{}", unit.cargo_type);
						breakdown[type] = breakdown.value(type, 0u) + unit.cargo_count;
					}
				}
				auto initiate = _network_authority_requests.Poll(url, AuthorityOperation::Initiate, cp.request_id, {
					{"request_id", cp.request_id}, {"source_world", cp.source_world}, {"dest_world", cp.destination_world},
					{"source_gate", cp.source_gate_id}, {"dest_gate", cp.destination_gate_id},
					{"snapshot_base64", Base64Encode(cp.snapshot)}, {"total_cargo", cargo}, {"cargo_breakdown", breakdown},
					{"consist_id", fmt::format("{:x}:{:x}:{}", cp.namespace_high, cp.namespace_low, cp.consist_sequence)},
					{"transit_delay_sec", 1.0}, {"priority", "STANDARD"}
				}, cp.source_world);
				if (initiate) {
					const auto tx = initiate->value("transfer_id", "");
					if (!tx.empty()) Command<Commands::CommitFederationDeparture>::Post(cp.source_world, cp.request_id, tx);
				}
			}
		} else if (cp.request_id.starts_with("ARR-")) {
			if (cp.state == TransferCheckpointState::Materialized) {
				auto confirm = _network_authority_requests.Poll(url, AuthorityOperation::Confirm, cp.transfer_id, {{"transfer_id", cp.transfer_id},
					{"dest_world", cp.destination_world}, {"success", true}, {"arrival_receipt", cp.arrival_receipt}, {"advance_order", true}}, cp.destination_world);
				if (confirm) Command<Commands::ConfirmFederationArrival>::Post(cp.transfer_id);
			} else if (!_network_authority_requests.IsQuiescing() && cp.state == TransferCheckpointState::Prepared && ConsistSnapshotCodec::DecodeForCurrentContent(cp.snapshot).Succeeded()) {
				Command<Commands::MaterializeFederationArrival>::Post(cp.transfer_id);
			}
		}
	}
	std::set<WorldID> worlds;
	for (const auto &[tile, link] : PortalRegistry::GetAllInterServerPortals()) worlds.insert(link.local_endpoint.world_id);
	for (WorldID world : worlds) {
		auto pending = _network_authority_requests.Poll(url, AuthorityOperation::Pending, "pending", nlohmann::json::object(), world.base());
		auto &known = _network_pending_transfers[world];
		if (pending) {
			const auto transfers = pending->value("pending_transfers", pending->value("pending", nlohmann::json::array()));
			if (transfers.is_array()) {
				known.clear();
				for (const auto &item : transfers) {
					if (item.is_string() && item.get_ref<const std::string &>().size() <= 128 && known.size() < 4096) known.insert(item.get<std::string>());
				}
			}
		}
		for (const auto &tx : known) {
			if (TransferJournal::HasArrival(tx)) continue;
			auto claim = _network_authority_requests.Poll(url, AuthorityOperation::Claim, tx, {{"transfer_id", tx}, {"dest_world", world.base()}}, world.base());
			if (!claim) continue;
			const auto &data = *claim;
			const auto bytes = Base64Decode(data.value("snapshot_base64", ""));
			auto decoded = ConsistSnapshotCodec::DecodeForCurrentContent(bytes);
			if (!decoded.Succeeded() || bytes.size() > 1024 * 1024) continue;
			const auto &cid = decoded.snapshot->consist_id;
			const auto metadata = nlohmann::json{{"tx", tx}, {"sw", data.value("source_world", 0u)}, {"dw", world.base()},
				{"sg", data.value("source_gate", data.value("source_gate_id", 0u))}, {"dg", data.value("dest_gate", data.value("dest_gate_id", 0u))},
				{"nh", cid.name_space.high}, {"nl", cid.name_space.low}, {"seq", cid.sequence}}.dump();
			if (!ReadArrivalMetadata(metadata).has_value()) continue;
			/* Queue one missing fragment per poll. A retry or joining client resumes
			 * from journal state without filling the command queue with duplicates. */
			const auto *staged = TransferJournal::FindByTransferId(tx);
			const size_t offset = staged == nullptr ? 0 : staged->snapshot.size();
			if (offset >= bytes.size()) continue;
			const std::vector<uint8_t> fragment(bytes.begin() + offset, bytes.begin() + std::min(bytes.size(), offset + 384));
			Command<Commands::StageFederationArrival>::Post(metadata, static_cast<uint32_t>(offset), Base64Encode(fragment));
		}
	}
}
