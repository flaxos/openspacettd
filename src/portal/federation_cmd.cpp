/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_cmd.cpp Federation inter-server transfer commands and coordinator implementation. */

#include "../stdafx.h"
#include "federation_cmd.h"
#include "authority_transport.h"
#include "consist_materializer.h"
#include "federation_identity.h"
#include "planet_manager.h"
#include "portal_registry.h"
#include "transfer_journal.h"
#include "universe_authority.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../settings_type.h"
#include "../train.h"
#include "../strings_func.h"
#include "../timer/timer_game_tick.h"
#include "../table/strings.h"
#include "../debug.h"

#include <set>
#include <cstdlib>

#include "../safeguards.h"

static std::string _authority_url;

void FederationTransferManager::SetAuthorityUrl(std::string url)
{
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

	std::string request_id = fmt::format("DEP-W{}-G{}-C{}-T{}",
		link->local_endpoint.world_id.base(), link->id.base(),
		cid.sequence, TimerGameTick::counter);

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
	cp.snapshot = snapshot_bytes.bytes;
	cp.state = TransferCheckpointState::Prepared;
	if (!TransferJournal::Prepare(cp)) return false;

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
	if (local_world == INVALID_WORLD) return 0;

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

			if (!ConsistMaterializer::CheckThroatClearance(dest_tile, enter_dir)) {
				continue;
			}

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
		std::vector<std::string> pending = UniverseAuthorityService::Instance().QueryPendingTransfers(local_world, current_tick);

		for (const std::string &tx_id : pending) {
			auto &authority = UniverseAuthorityService::Instance();
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

			if (!ConsistMaterializer::CheckThroatClearance(dest_tile, enter_dir)) {
				continue;
			}

			ConsistMaterializeResult mat_res = ConsistMaterializer::MaterializeFromTransfer(
				rec.snapshot, dest_tile, enter_dir
			);

			if (mat_res.success) {
				authority.ConfirmTransferArrival(tx_id, local_world, true);
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
	/* Poll incoming transfers every 10 ticks (~330ms) */
	if ((current_tick % 10) != 0) return;

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
		ProcessIncomingTransfers(wid, current_tick);
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
		if (!FederationTransferManager::InitiateConsistDeparture(v, portal_tile)) {
			return CommandCost(STR_ERROR_SITE_UNSUITABLE_FOR_TUNNEL);
		}
	}

	return CommandCost();
}
