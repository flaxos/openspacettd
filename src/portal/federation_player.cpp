/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_player.cpp Persistent player accounts, corporate charters, and authentication logic. */

#include "../stdafx.h"
#include "federation_player.h"
#include "federation_identity.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "../safeguards.h"

static uint64_t _next_player_seq = 1;
static std::map<uint64_t, PlayerAccount> _players;
static std::map<std::string, uint64_t> _username_to_player;
static std::map<std::string, uint64_t> _token_to_player;
static std::map<uint64_t, CorporateCharter> _charters;

std::string FederationPlayerRegistry::GenerateDeterministicToken(GlobalPlayerID player_id, [[maybe_unused]] const std::string &username)
{
	std::ostringstream ss;
	ss << "AUTH-TK-"
	   << std::hex << std::setw(16) << std::setfill('0') << player_id.name_space.high
	   << std::setw(16) << std::setfill('0') << player_id.name_space.low
	   << "-" << std::setw(8) << std::setfill('0') << player_id.sequence;
	return ss.str();
}

GlobalPlayerID FederationPlayerRegistry::RegisterPlayer(const std::string &username, const std::string &auth_token)
{
	if (username.empty()) return GlobalPlayerID{};

	auto it = _username_to_player.find(username);
	if (it != _username_to_player.end()) {
		return GlobalPlayerID{};
	}

	FederationNamespace ns = FederationIdentityRegistry::GetNamespace();
	uint64_t seq = _next_player_seq++;
	GlobalPlayerID pid{ns, seq};

	std::string token = auth_token.empty() ? GenerateDeterministicToken(pid, username) : auth_token;

	PlayerAccount acc{
		.player_id = pid,
		.username = username,
		.auth_token = token,
		.created_tick = 0,
		.last_seen_tick = 0,
	};

	_players[seq] = acc;
	_username_to_player[username] = seq;
	_token_to_player[token] = seq;

	return pid;
}

std::optional<PlayerAccount> FederationPlayerRegistry::Authenticate(const std::string &username, const std::string &auth_token)
{
	auto it = _username_to_player.find(username);
	if (it == _username_to_player.end()) return std::nullopt;

	PlayerAccount &acc = _players[it->second];
	if (acc.auth_token != auth_token) return std::nullopt;

	return acc;
}

std::optional<PlayerAccount> FederationPlayerRegistry::ValidateToken(const std::string &auth_token)
{
	if (auth_token.empty()) return std::nullopt;
	auto it = _token_to_player.find(auth_token);
	if (it == _token_to_player.end()) return std::nullopt;

	return _players[it->second];
}

const PlayerAccount *FederationPlayerRegistry::GetPlayer(GlobalPlayerID player_id)
{
	auto it = _players.find(player_id.sequence);
	if (it == _players.end()) return nullptr;
	if (it->second.player_id != player_id) return nullptr;
	return &it->second;
}

std::vector<PlayerAccount> FederationPlayerRegistry::GetAllPlayers()
{
	std::vector<PlayerAccount> result;
	result.reserve(_players.size());
	for (const auto &[seq, acc] : _players) {
		result.push_back(acc);
	}
	return result;
}

GlobalCompanyID FederationPlayerRegistry::CharterCompany(GlobalPlayerID owner_id, const std::string &company_name)
{
	if (!owner_id.IsValid() || company_name.empty()) return GlobalCompanyID{};

	FederationNamespace ns = FederationIdentityRegistry::GetNamespace();
	uint64_t seq = FederationIdentityRegistry::GetNextCompanySequence();
	GlobalCompanyID cid{ns, seq};

	CorporateCharter charter{
		.company_id = cid,
		.owner_player_id = owner_id,
		.company_name = company_name,
		.authorized_delegates = {},
		.active_world_presences = {DEFAULT_WORLD},
		.global_treasury_credits = 1000000,
	};

	_charters[seq] = std::move(charter);
	return cid;
}

bool FederationPlayerRegistry::AuthorizeDelegate(GlobalCompanyID company_id, GlobalPlayerID delegate_id, GlobalPlayerID requester_id)
{
	auto it = _charters.find(company_id.sequence);
	if (it == _charters.end()) return false;
	if (it->second.owner_player_id != requester_id) return false;
	if (!delegate_id.IsValid()) return false;

	auto &delegates = it->second.authorized_delegates;
	if (std::find(delegates.begin(), delegates.end(), delegate_id) == delegates.end()) {
		delegates.push_back(delegate_id);
	}
	return true;
}

bool FederationPlayerRegistry::RevokeDelegate(GlobalCompanyID company_id, GlobalPlayerID delegate_id, GlobalPlayerID requester_id)
{
	auto it = _charters.find(company_id.sequence);
	if (it == _charters.end()) return false;
	if (it->second.owner_player_id != requester_id) return false;

	auto &delegates = it->second.authorized_delegates;
	auto pos = std::find(delegates.begin(), delegates.end(), delegate_id);
	if (pos != delegates.end()) {
		delegates.erase(pos);
		return true;
	}
	return false;
}

bool FederationPlayerRegistry::IsAuthorized(GlobalCompanyID company_id, GlobalPlayerID player_id)
{
	if (!company_id.IsValid() || !player_id.IsValid()) return false;
	auto it = _charters.find(company_id.sequence);
	if (it == _charters.end()) return false;

	if (it->second.owner_player_id == player_id) return true;
	const auto &delegates = it->second.authorized_delegates;
	return std::find(delegates.begin(), delegates.end(), player_id) != delegates.end();
}

const CorporateCharter *FederationPlayerRegistry::GetCompanyCharter(GlobalCompanyID company_id)
{
	auto it = _charters.find(company_id.sequence);
	if (it == _charters.end()) return nullptr;
	if (it->second.company_id != company_id) return nullptr;
	return &it->second;
}

std::vector<CorporateCharter> FederationPlayerRegistry::GetPlayerCompanies(GlobalPlayerID player_id)
{
	std::vector<CorporateCharter> result;
	if (!player_id.IsValid()) return result;

	for (const auto &[seq, charter] : _charters) {
		if (charter.owner_player_id == player_id ||
		    std::find(charter.authorized_delegates.begin(), charter.authorized_delegates.end(), player_id) != charter.authorized_delegates.end()) {
			result.push_back(charter);
		}
	}
	return result;
}

void FederationPlayerRegistry::RegisterWorldPresence(GlobalCompanyID company_id, WorldID world_id)
{
	if (!company_id.IsValid() || world_id == INVALID_WORLD) return;
	auto it = _charters.find(company_id.sequence);
	if (it == _charters.end()) return;

	auto &worlds = it->second.active_world_presences;
	if (std::find(worlds.begin(), worlds.end(), world_id) == worlds.end()) {
		worlds.push_back(world_id);
	}
}

std::vector<CorporateCharter> FederationPlayerRegistry::GetAllCharters()
{
	std::vector<CorporateCharter> result;
	result.reserve(_charters.size());
	for (const auto &[seq, charter] : _charters) {
		result.push_back(charter);
	}
	return result;
}

void FederationPlayerRegistry::Reset()
{
	_next_player_seq = 1;
	_players.clear();
	_username_to_player.clear();
	_token_to_player.clear();
	_charters.clear();
}
