/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file consist_materializer.cpp Engine functions for consist despawn and materialization. */

#include "../stdafx.h"
#include "consist_materializer.h"
#include "federation_cargo.h"
#include "consist_snapshot.h"
#include "content_manifest.h"
#include "federation_identity.h"
#include "portal_registry.h"
#include "prebuilt_trade.h"

#include "../company_base.h"
#include "../company_func.h"
#include "../direction_func.h"
#include "../engine_base.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../order_base.h"
#include "../rail_map.h"
#include "../station_base.h"
#include "../train.h"
#include "../tunnelbridge_map.h"
#include "../timer/timer_game_calendar.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "planet_manager.h"

#include "../safeguards.h"

ConsistDespawnResult ConsistMaterializer::DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token,
	const std::function<bool(const ConsistSnapshotBytes &)> &admit)
{
	return DespawnForTransfer(consist, owner_token,
		[admit](const ConsistSnapshot &, const ConsistSnapshotBytes &bytes) {
			return !admit || admit(bytes);
		});
}

ConsistDespawnResult ConsistMaterializer::CaptureForTransfer(Train *consist, const GlobalOwnerToken &owner_token)
{
	ConsistDespawnResult result;
	if (consist == nullptr) {
		result.error_message = "Null consist pointer";
		return result;
	}

	Train *front = consist->First();
	if (front == nullptr) {
		result.error_message = "No front engine in consist";
		return result;
	}

	/* 1. Capture snapshot for active content */
	ConsistSnapshotResult capture_res = ConsistSnapshotCodec::CaptureForCurrentContent(front, owner_token);
	if (!capture_res.Succeeded()) {
		result.error_message = "Failed to capture consist snapshot";
		return result;
	}
	result.snapshot = *capture_res.snapshot;

	/* 2. Encode to canonical v2 wire format */
	result.snapshot_bytes = ConsistSnapshotCodec::Encode(result.snapshot);
	if (!result.snapshot_bytes.Succeeded()) {
		result.error_message = "Failed to encode consist snapshot";
		return result;
	}

	/* 3. Compute cargo units */
	for (const auto &unit : result.snapshot.units) {
		result.total_cargo += unit.cargo_count;
	}

	result.success = true;
	return result;
}

bool ConsistMaterializer::ReleaseCapturedConsist(Train *consist)
{
	if (consist == nullptr) return false;

	Train *front = consist->First();
	if (front == nullptr) return false;

	/* 4. Release vehicle transit progress in portal registry */
	PortalRegistry::ClearPortalTransit(front->index);

	/* 5. Free track reservations */
	FreeTrainTrackReservation(front);

	/* Unreserve tile track if still reserved */
	TileIndex tile = front->tile;
	if (IsTunnelTile(tile)) {
		SetTunnelBridgeReservation(tile, false);
	} else if (IsPlainRailTile(tile)) {
		Track track = TrackBitsToTrack(front->track);
		if (IsValidTrack(track) && HasTrack(tile, track) && GetRailReservationTrackBits(tile).Test(track)) {
			UnreserveTrack(tile, track);
		}
	}

	/* 6. Release local vehicle anchor mapping */
	FederationIdentityRegistry::ReleaseVehicle(front->index);

	/* 7. Cleanly delete vehicle chain */
	delete front;

	return true;
}

ConsistDespawnResult ConsistMaterializer::DespawnForTransfer(Train *consist, const GlobalOwnerToken &owner_token,
	const std::function<bool(const ConsistSnapshot &, const ConsistSnapshotBytes &)> &admit)
{
	ConsistDespawnResult result = CaptureForTransfer(consist, owner_token);
	if (!result.success) return result;

	/* Admission must succeed before relinquishing the physical consist. */
	if (admit && !admit(result.snapshot, result.snapshot_bytes)) {
		result.success = false;
		result.error_message = "Consist transfer admission rejected";
		return result;
	}

	if (!ReleaseCapturedConsist(consist)) {
		result.success = false;
		result.error_message = "Failed to release captured consist";
		return result;
	}

	result.success = true;
	return result;
}

bool ConsistMaterializer::CheckThroatClearance(TileIndex exit_tile, DiagDirection /*enter_dir*/)
{
	if (!IsValidTile(exit_tile)) {
		return false;
	}

	/* Check for conflicting reservations if tile is a rail tunnel/bridge */
	if (IsTileType(exit_tile, TileType::TunnelBridge) &&
	    GetTunnelBridgeTransportType(exit_tile) == TransportType::Rail &&
	    HasTunnelBridgeReservation(exit_tile)) {
		return false;
	}

	/* Check for vehicles currently on the exit tile */
	for (const Vehicle *v : VehiclesOnTile(exit_tile)) {
		if (v->type == VehicleType::Train) {
			return false;
		}
	}

	return true;
}

TileIndex ConsistMaterializer::ResolveClearThroat(TileIndex primary_exit_tile, DiagDirection enter_dir)
{
	if (CheckThroatClearance(primary_exit_tile, enter_dir)) {
		return primary_exit_tile;
	}

	/* Check for configured parallel throat track on inter-server, local, or trade portal */
	TileIndex parallel = PortalRegistry::GetParallelThroat(primary_exit_tile);
	if (parallel == INVALID_TILE) {
		const auto *gw = PrebuiltTradeManager::Instance().GetTradeGateway(primary_exit_tile);
		if (gw != nullptr) parallel = gw->parallel_throat_tile;
	}

	if (parallel != INVALID_TILE && CheckThroatClearance(parallel, enter_dir)) {
		return parallel;
	}

	return INVALID_TILE;
}

ConsistMaterializeResult ConsistMaterializer::MaterializeFromTransfer(
	const ConsistSnapshot &snapshot,
	TileIndex exit_tile,
	DiagDirection enter_dir)
{
	ConsistMaterializeResult result;
	if (snapshot.units.empty()) {
		result.error_message = "Snapshot contains no units";
		return result;
	}

	/* 1. Validate content manifest compatibility */
	ContentManifestResult current_manifest = ContentManifestCodec::CaptureCurrent();
	if (current_manifest.Succeeded()) {
		ContentManifestTokenResult token_res = ContentManifestCodec::Digest(*current_manifest.manifest);
		if (token_res.Succeeded() && token_res.token != snapshot.content_manifest) {
			result.error_message = "Universe content manifest mismatch between servers";
			return result;
		}
	}

	/* 2. Check exit throat clearance (supporting parallel throat fallback) */
	TileIndex active_exit = ResolveClearThroat(exit_tile, enter_dir);
	if (active_exit == INVALID_TILE) {
		result.error_message = "Portal throat is obstructed";
		return result;
	}
	exit_tile = active_exit;

	/* 3. Check pool allocations and engine types */
	if (!Vehicle::CanAllocateItem(snapshot.units.size())) {
		result.error_message = "Vehicle pool exhausted";
		return result;
	}

	size_t required_cargo_packets = 0;
	for (const auto &u_snap : snapshot.units) {
		if (!Engine::IsValidID(EngineID{u_snap.engine_type})) {
			result.error_message = "Invalid engine type";
			return result;
		}
		if (u_snap.cargo_count > 0) required_cargo_packets += u_snap.packets.empty() ? 1 : u_snap.packets.size();
		uint32_t packet_count = 0;
		for (const auto &packet : u_snap.packets) {
			if (packet.count == 0 || packet.travelled_x < INT16_MIN || packet.travelled_x > INT16_MAX ||
					packet.travelled_y < INT16_MIN || packet.travelled_y > INT16_MAX ||
					((packet.source_x != UINT32_MAX || packet.source_y != UINT32_MAX) &&
					(packet.source_x >= Map::SizeX() || packet.source_y >= Map::SizeY()))) {
				result.error_message = "Invalid native cargo packet state";
				return result;
			}
			packet_count += packet.count;
		}
		if (!u_snap.packets.empty() && packet_count != u_snap.cargo_count) {
			result.error_message = "Cargo packet quantity mismatch";
			return result;
		}
	}

	if (!CargoPacket::CanAllocateItem(required_cargo_packets)) {
		result.error_message = "Cargo packet pool exhausted";
		return result;
	}

	/* 4. Resolve local company */
	CompanyID local_company = CompanyID::Invalid();
	for (const auto &[cid, seq] : FederationIdentityRegistry::GetCompanyMappings()) {
		if (FederationIdentityRegistry::FindCompany(CompanyID{cid}) == snapshot.company_id) {
			local_company = CompanyID{cid};
			break;
		}
	}
	if (local_company == CompanyID::Invalid() && snapshot.company_id.IsValid() && !FederationIdentityRegistry::GetCompanyMappings().empty()) {
		result.error_message = "No explicit mapping for the global company";
		return result;
	}
	if (local_company == CompanyID::Invalid() || !Company::IsValidID(local_company)) {
		local_company = (_current_company != COMPANY_SPECTATOR && Company::IsValidID(_current_company))
			? _current_company : CompanyID{0};
	}

	/* Resolve every order before allocating vehicles; dropping one would shift the active index. */
	WorldID exit_world = PlanetManager::GetTileWorld(exit_tile);
	if (exit_world == INVALID_WORLD) {
		const auto *link = PortalRegistry::GetInterServerPortal(exit_tile);
		if (link != nullptr) exit_world = link->local_endpoint.world_id;
	}
	std::vector<Order> restored_orders;
	if (!snapshot.orders.empty() && !OrderList::CanAllocateItem()) {
		result.error_message = "Order list pool exhausted";
		return result;
	}
	for (const auto &global_order : snapshot.orders) {
		auto destination = FederationIdentityRegistry::ResolveOrderDestination(global_order, exit_world);
		if (!destination) {
			result.error_message = "No explicit local mapping for a scheduled destination";
			return result;
		}
		Order order;
		if (global_order.type == OrderDestinationType::Waypoint) order.MakeGoToWaypoint(destination->ToStationID());
		else if (global_order.type == OrderDestinationType::Depot) order.MakeGoToDepot(*destination, OrderDepotTypeFlags{});
		else order.MakeGoToStation(destination->ToStationID());
		if (global_order.type == OrderDestinationType::Station && snapshot.station_order_flags.size() == snapshot.orders.size()) {
			const auto flags = snapshot.station_order_flags[restored_orders.size()];
			order.SetLoadType(static_cast<OrderLoadType>(flags & 7));
			order.SetUnloadType(static_cast<OrderUnloadType>((flags >> 3) & 7));
			OrderNonStopFlags non_stop;
			if (flags & (1 << 6)) non_stop.Set(OrderNonStopFlag::NonStop);
			if (flags & (1 << 7)) non_stop.Set(OrderNonStopFlag::GoVia);
			order.SetNonStopType(non_stop);
			order.SetStopLocation(static_cast<OrderStopLocation>(flags >> 8));
		}
		restored_orders.push_back(std::move(order));
	}

	/* 5. Calculate vehicle emergence position */
	DiagDirection exit_vdir = ReverseDiagDir(enter_dir);
	static constexpr DiagDirectionIndexArray<uint8_t> tunnel_vis_frame{12, 8, 8, 12};
	uint8_t frame = TILE_SIZE - tunnel_vis_frame[enter_dir];

	int offset_x = 8;
	int offset_y = 8;
	switch (exit_vdir) {
		case DiagDirection::NE: offset_x = TILE_SIZE - 1 - frame; break;
		case DiagDirection::SE: offset_y = frame; break;
		case DiagDirection::SW: offset_x = frame; break;
		case DiagDirection::NW: offset_y = TILE_SIZE - 1 - frame; break;
		default: break;
	}

	int x = TileX(exit_tile) * TILE_SIZE + offset_x;
	int y = TileY(exit_tile) * TILE_SIZE + offset_y;
	int z = GetSlopePixelZ(x, y, true);
	Direction dir = DiagDirToDir(exit_vdir);
	Track track = DiagDirToDiagTrack(exit_vdir);

	/* 6. Construct train consist chain */
	Train *front = nullptr;
	Train *prev = nullptr;
	uint32_t total_cargo = 0;
	auto rollback = [&]() {
		if (front != nullptr) {
			delete front;
			front = nullptr;
			prev = nullptr;
		}
		if (IsTunnelTile(exit_tile)) {
			SetTunnelBridgeReservation(exit_tile, false);
		} else if (IsPlainRailTile(exit_tile) && HasTrack(exit_tile, track) && GetRailReservationTrackBits(exit_tile).Test(track)) {
			UnreserveTrack(exit_tile, track);
		}
	};

	for (size_t i = 0; i < snapshot.units.size(); ++i) {
		const auto &u_snap = snapshot.units[i];
		Train *t = Vehicle::Create<Train>();
		if (t == nullptr) {
			rollback();
			result.error_message = "Vehicle allocation failed";
			return result;
		}
		t->subtype = u_snap.subtype;
		if (i == 0) {
			front = t;
			t->SetFrontEngine();
			t->SetEngine();
		} else {
			t->ClearFrontEngine();
			if (u_snap.subtype & 1) {
				t->SetEngine();
			} else {
				t->SetWagon();
			}
		}

		t->owner = local_company;
		t->tile = exit_tile;
		t->x_pos = x;
		t->y_pos = y;
		t->z_pos = z;
		t->direction = dir;
		t->track = track;
		t->vehstatus.Reset(VehState::Hidden);

		t->engine_type = EngineID{u_snap.engine_type};
		if (u_snap.cargo_type < to_underlying(NUM_CARGO)) {
			t->cargo_type = CargoType{u_snap.cargo_type};
		} else {
			t->cargo_type = CargoType{0};
		}
		t->cargo_subtype = u_snap.cargo_subtype;
		t->cargo_cap = u_snap.cargo_capacity;
		t->refit_cap = u_snap.refit_capacity;
		t->build_year = TimerGameCalendar::Year{u_snap.build_year};
		t->age = TimerGameCalendar::Date{u_snap.age};
		t->max_age = TimerGameCalendar::Date{u_snap.max_age};
		t->value = u_snap.value;
		t->reliability = u_snap.reliability;
		t->reliability_spd_dec = u_snap.reliability_speed_decrease;
		t->breakdown_ctr = u_snap.breakdown_counter;
		t->breakdown_delay = u_snap.breakdown_delay;
		t->breakdowns_since_last_service = u_snap.breakdowns_since_service;
		t->breakdown_chance = u_snap.breakdown_chance;
		t->random_bits = u_snap.random_bits;

		if (prev != nullptr) {
			prev->SetNext(t);
		}
		prev = t;

		if (u_snap.cargo_count > 0) {
			auto packets = u_snap.packets;
			if (packets.empty()) {
				/* Legacy V1/V2 snapshots only carried a wagon aggregate. */
				packets.push_back({static_cast<uint16_t>(u_snap.cargo_count), 0, 0,
					static_cast<int32_t>(TileX(exit_tile)), static_cast<int32_t>(TileY(exit_tile)),
					TileX(exit_tile), TileY(exit_tile), u_snap.cargo_source});
			}
			for (const auto &packet : packets) {
				StationID station = StationID::Invalid();
				if (auto resolved = FederationIdentityRegistry::ResolveStation(packet.source.origin_station)) station = *resolved;
				TileIndex source_xy = packet.source_x == UINT32_MAX ? INVALID_TILE : TileXY(packet.source_x, packet.source_y);
				CargoPacket *cargo = CargoPacket::Create(packet.count, packet.periods_in_transit, station, source_xy, Money{packet.feeder_share});
				if (cargo == nullptr) {
					rollback();
					result.error_message = "Cargo packet allocation failed";
					return result;
				}
				cargo->travelled = {static_cast<int16_t>(packet.travelled_x), static_cast<int16_t>(packet.travelled_y)};
#ifdef WITH_ASSERT
				cargo->in_vehicle = true;
#endif
				FederationCargoRegistry::Set(cargo->index.base(), packet.source);
				t->cargo.Append(cargo);
				total_cargo += packet.count;
			}
		}
	}

	if (front == nullptr) {
		result.error_message = "Failed to create front engine";
		return result;
	}

	/* 7. Dynamics & consist properties */
	front->cur_speed = snapshot.speed;
	front->subspeed = snapshot.subspeed;
	front->acceleration = snapshot.acceleration;
	if (snapshot.stopped) front->vehstatus.Set(VehState::Stopped);
	if (snapshot.driving_backwards) front->vehicle_flags.Set(VehicleFlag::DrivingBackwards);

	front->ConsistChanged(CCF_ARRANGE);
	/* Keep trailing cars inside the receiving wormhole until their actual
	 * spacing behind the moving front has cleared the portal visibility frame. */
	if (PortalRegistry::IsInterServerPortal(exit_tile)) {
		int32_t distance = 0;
		Train *previous = nullptr;
		for (Train *unit = front->GetMovingFront(); unit != nullptr; unit = unit->GetMovingNext()) {
			unit->SetMovingDirection(dir);
			if (previous != nullptr) {
				distance += (previous->gcache.cached_veh_length + unit->gcache.cached_veh_length) / 2;
				unit->track = Track::Wormhole;
				unit->vehstatus.Set(VehState::Hidden);
				PortalRegistry::SetVehicleTransitProgress(unit->index, static_cast<uint32_t>(static_cast<int32_t>(PORTAL_TRANSIT_DISTANCE) - distance));
			}
			previous = unit;
		}
	}

	for (Train *u = front; u != nullptr; u = u->Next()) {
		u->UpdatePositionAndViewport();
	}

	/* 8. Acquire exit track reservation */
	if (IsTunnelTile(exit_tile)) {
		SetTunnelBridgeReservation(exit_tile, true);
	} else if (IsPlainRailTile(exit_tile) && HasTrack(exit_tile, track)) {
		TryReserveTrack(exit_tile, track);
	}

	/* 9. Restore persistent GlobalConsistID */
	if (snapshot.consist_id.IsValid()) {
		FederationIdentityRegistry::RestoreMapping(front->index, snapshot.consist_id.sequence, snapshot.consist_id.name_space);
	}

	/* 10. Restore and resolve order schedule */
	if (!snapshot.orders.empty() && OrderList::CanAllocateItem()) {
		if (snapshot.consist_id.IsValid()) FederationIdentityRegistry::SetConsistSchedule(snapshot.consist_id, snapshot.orders);

		if (!restored_orders.empty()) {
			front->orders = OrderList::Create(std::move(restored_orders), front);

			/* Advance order index if the previous order was targeting the origin world */
			uint16_t active_idx = 0;
			if (snapshot.current_order_index < snapshot.orders.size()) {
				const auto &orig_order = snapshot.orders[snapshot.current_order_index];
				if (orig_order.target_world == exit_world) {
					active_idx = snapshot.current_order_index;
				} else {
					active_idx = static_cast<uint16_t>((snapshot.current_order_index + 1) % snapshot.orders.size());
				}
			}

			if (active_idx >= front->GetNumOrders()) active_idx = 0;
			front->cur_real_order_index = active_idx;
			front->cur_implicit_order_index = active_idx;
			front->current_order = *front->GetOrder(active_idx);
		}
	}

	result.success = true;
	result.consist = front;
	result.total_cargo = total_cargo;
	return result;
}

bool ConsistMaterializer::AssignRoundTripOrders(Train *consist, StationID origin_st, WorldID origin_world, StationID dest_st, WorldID dest_world)
{
	if (consist == nullptr || !BaseStation::IsValidID(origin_st) || !BaseStation::IsValidID(dest_st)) return false;

	Train *front = consist->First();
	if (front == nullptr) return false;

	/* Register stations with FederationIdentityRegistry */
	auto orig_global = FederationIdentityRegistry::GetOrCreateStation(origin_st);
	auto dest_global = FederationIdentityRegistry::GetOrCreateStation(dest_st);
	if (!orig_global.has_value() || !dest_global.has_value()) return false;

	orig_global->world_id = origin_world;
	dest_global->world_id = dest_world;

	/* Construct master schedule */
	std::vector<GlobalOrderDestinationID> master_schedule;
	master_schedule.push_back(GlobalOrderDestinationID::ForStation(*orig_global, false));
	master_schedule.push_back(GlobalOrderDestinationID::ForStation(*dest_global, false));

	std::optional<GlobalConsistID> cid = FederationIdentityRegistry::GetOrCreate(front);
	if (cid.has_value()) {
		FederationIdentityRegistry::SetConsistSchedule(*cid, master_schedule);
	}

	/* Build local projection */
	std::vector<Order> local_orders;
	Order o1;
	o1.MakeGoToStation(origin_st);
	local_orders.push_back(std::move(o1));

	Order o2;
	auto resolved_dest = FederationIdentityRegistry::ResolveOrderDestination(master_schedule[1], origin_world);
	if (resolved_dest.has_value()) {
		o2.MakeGoToStation(resolved_dest->ToStationID());
	} else {
		o2.MakeGoToStation(dest_st);
	}
	local_orders.push_back(std::move(o2));

	if (front->orders != nullptr) {
		front->orders->FreeChain();
		front->orders = nullptr;
	}

	if (!OrderList::CanAllocateItem()) return false;
	front->orders = OrderList::Create(std::move(local_orders), front);
	front->cur_real_order_index = 0;
	front->cur_implicit_order_index = 0;
	front->current_order = *front->GetOrder(0);
	return true;
}
