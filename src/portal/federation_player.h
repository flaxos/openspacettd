/*
 * This file is part of OpenSpaceTTD.
 * OpenSpaceTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 */

/** @file federation_player.h Persistent player accounts, authentication, and corporate charters for Federation F3. */

#ifndef FEDERATION_PLAYER_H
#define FEDERATION_PLAYER_H

#include "federation_identity.h"
#include "portal_type.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

/** Globally portable identity of one registered player account across world servers. */
struct GlobalPlayerID {
	FederationNamespace name_space{};
	uint64_t sequence = 0;

	bool IsValid() const { return this->name_space.IsValid() && this->sequence != 0; }
	auto operator<=>(const GlobalPlayerID &) const = default;
};

/** Registered player account record. */
struct PlayerAccount {
	GlobalPlayerID player_id{};
	std::string username;
	std::string auth_token;
	uint64_t created_tick = 0;
	uint64_t last_seen_tick = 0;

	bool IsValid() const { return this->player_id.IsValid() && !this->username.empty(); }
};

/** Multi-world corporate charter tracking ownership and active world presences. */
struct CorporateCharter {
	GlobalCompanyID company_id{};
	GlobalPlayerID owner_player_id{};
	std::string company_name;
	std::vector<GlobalPlayerID> authorized_delegates;
	std::vector<WorldID> active_world_presences;
	int64_t global_treasury_credits = 0;

	bool IsValid() const { return this->company_id.IsValid() && this->owner_player_id.IsValid(); }
};

/** Registry and authentication service for player accounts and corporate charters. */
class FederationPlayerRegistry {
public:
	/* Player Account Management */
	static GlobalPlayerID RegisterPlayer(const std::string &username, const std::string &auth_token = "");
	static std::optional<PlayerAccount> Authenticate(const std::string &username, const std::string &auth_token);
	static std::optional<PlayerAccount> ValidateToken(const std::string &auth_token);
	static const PlayerAccount *GetPlayer(GlobalPlayerID player_id);
	static std::vector<PlayerAccount> GetAllPlayers();

	/* Corporate Charter Management */
	static GlobalCompanyID CharterCompany(GlobalPlayerID owner_id, const std::string &company_name);
	static bool AuthorizeDelegate(GlobalCompanyID company_id, GlobalPlayerID delegate_id, GlobalPlayerID requester_id);
	static bool RevokeDelegate(GlobalCompanyID company_id, GlobalPlayerID delegate_id, GlobalPlayerID requester_id);
	static bool IsAuthorized(GlobalCompanyID company_id, GlobalPlayerID player_id);
	static const CorporateCharter *GetCompanyCharter(GlobalCompanyID company_id);
	static std::vector<CorporateCharter> GetPlayerCompanies(GlobalPlayerID player_id);
	static void RegisterWorldPresence(GlobalCompanyID company_id, WorldID world_id);
	static std::vector<CorporateCharter> GetAllCharters();

	/* Active Local Player Session */
	static std::optional<PlayerAccount> GetActiveSession();
	static bool SetActiveSession(const PlayerAccount &account);
	static void ClearActiveSession();

	/* Reset */
	static void Reset();

private:
	static std::string GenerateDeterministicToken(GlobalPlayerID player_id, const std::string &username);
};

#endif /* FEDERATION_PLAYER_H */
