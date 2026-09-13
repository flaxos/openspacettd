/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_identity.cpp Stable identities for future inter-server consist transfer. */

#include "../stdafx.h"
#include "federation_identity.h"

#include "../map_func.h"
#include "../openttd.h"
#include "../settings_type.h"
#include "../train.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

namespace {

FederationNamespace _federation_namespace{};
uint64_t _next_consist_sequence = 1;
std::map<uint32_t, uint64_t> _consist_mappings;

static uint64_t Mix64(uint64_t value)
{
	value += 0x9E3779B97F4A7C15ULL;
	value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
	value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
	return value ^ (value >> 31);
}

static const Train *CanonicalFront(const Train *train)
{
	if (train == nullptr) return nullptr;
	const Train *front = train->First();
	return front != nullptr && front->IsFrontEngine() ? front : nullptr;
}

static bool AnchorBelongsTo(const Train *anchor, const Train *front)
{
	return anchor != nullptr && front != nullptr && anchor->First() == front;
}

} // namespace

FederationNamespace FederationIdentityRegistry::DeriveNamespace(uint32_t generation_seed, uint32_t map_x, uint32_t map_y, int32_t starting_year)
{
	uint64_t geometry = (static_cast<uint64_t>(map_x) << 32) | map_y;
	uint64_t calendar = static_cast<uint32_t>(starting_year);
	FederationNamespace result{
		Mix64((static_cast<uint64_t>(generation_seed) << 32) ^ geometry ^ 0x4F53545444464944ULL),
		Mix64(geometry ^ (calendar << 32) ^ generation_seed ^ 0x534156454E414D45ULL),
	};
	if (!result.IsValid()) result.low = 1;
	return result;
}

void FederationIdentityRegistry::EnsureNamespace()
{
	if (_federation_namespace.IsValid()) return;
	if (!_game_session_stats.savegame_id.empty()) {
		uint64_t high = 0xCBF29CE484222325ULL;
		uint64_t low = 0x84222325CBF29CE4ULL;
		for (uint8_t byte : _game_session_stats.savegame_id) {
			high = (high ^ byte) * 0x100000001B3ULL;
			low = (low ^ (byte + 0x9D)) * 0x100000001B3ULL;
		}
		_federation_namespace = {Mix64(high), Mix64(low)};
		if (_federation_namespace.IsValid()) return;
	}
	_federation_namespace = DeriveNamespace(_settings_game.game_creation.generation_seed, Map::SizeX(), Map::SizeY(),
			_settings_game.game_creation.starting_year.base());
}

FederationNamespace FederationIdentityRegistry::GetNamespace()
{
	EnsureNamespace();
	return _federation_namespace;
}

uint64_t FederationIdentityRegistry::GetNextSequence()
{
	return _next_consist_sequence;
}

std::optional<GlobalConsistID> FederationIdentityRegistry::Find(const Train *train)
{
	const Train *front = CanonicalFront(train);
	if (front == nullptr) return std::nullopt;

	std::optional<uint64_t> best_sequence;
	for (const auto &[anchor_id, sequence] : _consist_mappings) {
		const Train *anchor = Train::GetIfValid(VehicleID{anchor_id});
		if (AnchorBelongsTo(anchor, front) && (!best_sequence.has_value() || sequence < *best_sequence)) best_sequence = sequence;
	}
	return best_sequence.has_value() ? std::optional<GlobalConsistID>{GlobalConsistID{GetNamespace(), *best_sequence}} : std::nullopt;
}

std::optional<GlobalConsistID> FederationIdentityRegistry::GetOrCreate(const Train *train)
{
	const Train *front = CanonicalFront(train);
	if (front == nullptr) return std::nullopt;
	if (auto existing = Find(front); existing.has_value()) return existing;

	EnsureNamespace();
	if (_next_consist_sequence == 0) return std::nullopt;
	uint64_t sequence = _next_consist_sequence++;
	_consist_mappings[front->index.base()] = sequence;
	return GlobalConsistID{_federation_namespace, sequence};
}

void FederationIdentityRegistry::ReconcileConsistChange(const Train *source, const Train *destination,
		std::optional<GlobalConsistID> preferred_destination)
{
	const Train *source_front = CanonicalFront(source);
	const Train *destination_front = CanonicalFront(destination);

	/* A split needs no rewrite: the stable anchor follows its resulting chain.
	 * A merge can bring two mappings into one chain; retain the explicitly
	 * preferred destination identity, or the oldest sequence as a fallback. */
	for (const Train *front : {source_front, destination_front}) {
		if (front == nullptr) continue;
		std::vector<uint32_t> anchors;
		for (const auto &[anchor_id, sequence] : _consist_mappings) {
			const Train *anchor = Train::GetIfValid(VehicleID{anchor_id});
			if (AnchorBelongsTo(anchor, front)) anchors.push_back(anchor_id);
		}
		if (anchors.size() < 2) continue;

		uint32_t keep = anchors.front();
		for (uint32_t anchor_id : anchors) {
			uint64_t sequence = _consist_mappings.at(anchor_id);
			if (preferred_destination.has_value() && sequence == preferred_destination->sequence &&
					preferred_destination->name_space == GetNamespace()) {
				keep = anchor_id;
				break;
			}
			if (sequence < _consist_mappings.at(keep)) keep = anchor_id;
		}
		for (uint32_t anchor_id : anchors) {
			if (anchor_id != keep) _consist_mappings.erase(anchor_id);
		}
	}
}

void FederationIdentityRegistry::ReleaseVehicle(VehicleID vehicle)
{
	_consist_mappings.erase(vehicle.base());
}

const std::map<uint32_t, uint64_t> &FederationIdentityRegistry::GetMappings()
{
	return _consist_mappings;
}

void FederationIdentityRegistry::RestoreState(FederationNamespace name_space, uint64_t next_sequence)
{
	_federation_namespace = name_space;
	_next_consist_sequence = next_sequence;
}

bool FederationIdentityRegistry::RestoreMapping(VehicleID anchor, uint64_t sequence)
{
	if (anchor == VehicleID::Invalid() || sequence == 0) return false;
	_consist_mappings[anchor.base()] = sequence;
	if (sequence == UINT64_MAX) {
		_next_consist_sequence = 0;
	} else if (_next_consist_sequence != 0) {
		_next_consist_sequence = std::max(_next_consist_sequence, sequence + 1);
	}
	return true;
}

void FederationIdentityRegistry::PruneStaleMappings()
{
	for (auto it = _consist_mappings.begin(); it != _consist_mappings.end();) {
		const Train *anchor = Train::GetIfValid(VehicleID{it->first});
		if (CanonicalFront(anchor) == nullptr) {
			it = _consist_mappings.erase(it);
		} else {
			++it;
		}
	}

	/* Broken or legacy state may contain two anchors that now belong to one
	 * consist. Keep the oldest identity as the deterministic fallback. */
	std::map<uint32_t, uint32_t> front_to_anchor;
	for (auto it = _consist_mappings.begin(); it != _consist_mappings.end();) {
		const Train *anchor = Train::GetIfValid(VehicleID{it->first});
		uint32_t front_id = anchor->First()->index.base();
		auto [known, inserted] = front_to_anchor.emplace(front_id, it->first);
		if (inserted) {
			++it;
			continue;
		}

		uint32_t old_anchor = known->second;
		if (it->second < _consist_mappings.at(old_anchor)) {
			_consist_mappings.erase(old_anchor);
			known->second = it->first;
			++it;
		} else {
			it = _consist_mappings.erase(it);
		}
	}
}

void FederationIdentityRegistry::Reset()
{
	_federation_namespace = {};
	_next_consist_sequence = 1;
	_consist_mappings.clear();
}
