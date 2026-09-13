# OpenSpaceTTD Sprint 16 — Persistent Universe & Corporate Ledger (Technical Spike F3)

Status: **COMPLETE**

Sprint 16 implements and validates **Phase F3: Persistent Universe & Corporate Ledger**. It establishes persistent player authentication and multi-world corporate ownership across world servers, dynamic world discovery and heartbeat directories, per-cargo-type detailed commodity conservation tracking, inter-world trade balance accounting, and in-engine console administration commands.

---

## Player & Architecture Outcomes

- **Persistent Player Accounts:** Global 128-bit player identities (`GlobalPlayerID`) with secure token generation, login verification, and multi-world session management (`PlayerAccount`, `FederationPlayerRegistry`).
- **Corporate Charters & Multi-World Ownership:** Players can charter multi-world transport corporations (`CorporateCharter`, `GlobalCompanyID`) with initial treasury credit capitalization, active world presence tracking (`active_world_presences`), and owner-delegated administrative permissions (`AuthorizeDelegate`, `RevokeDelegate`, `IsAuthorized`).
- **Dynamic World Server Directory:** Real-time world server discovery (`GetWorldDirectory`), live heartbeat tracking (`UpdateWorldHeartbeat`), status reporting (`WorldOnlineStatus::Online`, `Unreachable`, `Maintenance`), developmental phase filtering (`FindWorldsByPhase`), and stale world timeout detection (`PruneStaleWorlds`).
- **Detailed Commodity Conservation & Trade Balances:** Per-cargo-type commodity ledger tracking verifies strict conservation across all individual goods:
  $$\forall c \in \text{Commodities},\quad \text{Cargo}_{\text{Init}}(c) = \text{Cargo}_{\text{Done}}(c) + \text{Cargo}_{\text{InTransit}}(c)$$
  Inter-world trade balance accounting tracks cargo exports/imports and credit valuations between world servers.
- **In-Game Administration Console Commands:** Integrated console commands (`universe_auth`, `universe_worlds`, `universe_company`, `universe_trade`) provide direct administrative control and inspection.

---

## Technical Components Delivered

### 1. Player Accounts & Corporate Registry (`src/portal/federation_player.h`, `src/portal/federation_player.cpp`)
- **`GlobalPlayerID`:** 128-bit namespaced persistent player identifier.
- **`PlayerAccount`:** Account record containing player ID, unique username, deterministic auth token, and activity timestamps.
- **`CorporateCharter`:** Multi-world enterprise tracking company ID, owner player, company name, authorized delegates, active world presences, and global treasury credits.
- **`FederationPlayerRegistry`:**
  - `RegisterPlayer`, `Authenticate`, `ValidateToken`, `GetPlayer`, `GetAllPlayers`.
  - `CharterCompany`, `AuthorizeDelegate`, `RevokeDelegate`, `IsAuthorized`, `GetCompanyCharter`, `GetPlayerCompanies`, `RegisterWorldPresence`, `GetAllCharters`.

### 2. Universe Authority Service & Daemon Extensions (`src/portal/universe_authority.h`, `src/portal/universe_authority.cpp`, `scripts/universe_authority.py`)
- **Dynamic World Directory:**
  - Extended `RegisteredWorld` with network address, description, active/max client caps, active train counts, and `WorldOnlineStatus`.
  - `UpdateWorldHeartbeat`, `PruneStaleWorlds`, `FindWorldsByPhase`, `GetWorldDirectory`.
- **Per-Cargo-Type Commodity Ledger:**
  - Extended `UniverseTransferRecord` to capture multi-cargo breakdown (`std::map<uint8_t, uint32_t>`).
  - `DetailedCommodityAudit` tracks per-cargo-type `initiated`, `in_transit`, `completed` and verifies individual conservation.
- **Inter-World Trade Balance Accounting:**
  - `TradeBalanceSummary` tracks exported cargo counts, imported cargo counts, and net trade balance credits.
  - Automatically updates trade balances upon departure and arrival confirmation.
- **REST Daemon Endpoints (`scripts/universe_authority.py`):**
  - `/auth/register`, `/auth/login`
  - `/companies/register`, `/companies/presence`, `/companies/list`
  - `/directory/register`, `/directory/heartbeat`, `/directory/worlds`
  - `/ledger/audit_detailed`, `/ledger/trade_balance`

### 3. In-Game Console Commands (`src/console_cmds.cpp`)
- **`universe_auth <register|login|list>`:** In-game account registration and authentication.
- **`universe_worlds [min_phase]`:** Displays dynamic world directory with server addresses, active clients, trains, and online statuses.
- **`universe_company <charter|list|presence>`:** Charters new corporations, lists corporate assets and presences, and expands presence across worlds.
- **`universe_trade`:** Displays detailed per-cargo commodity conservation audit and inter-world trade balances with net credits.

---

## Automated Acceptance & Verification

- [x] **Catch2 Persistent Universe Suite (`src/tests/test_federation_persistent_universe.cpp`):**
  - Player account creation, duplicate username rejection, deterministic token generation, and authentication.
  - Corporate chartering, owner permissions, delegate authorization/revocation, and multi-world presences.
  - Dynamic world directory, heartbeat updates, phase filtering, and stale world pruning.
  - Multi-commodity consist transfer handoff, per-cargo conservation audit, and trade balance accounting.
- [x] **Full Catch2 Unit Suite (`./build/openttd_test "Federation*"`):**
  - 14 test cases, 271 assertions passing with 0 failures.
- [x] **Full Regression & Unit Suite (`ctest --test-dir build`):**
  - 182/182 tests passing with 0 failures.
- [x] **Backward Compatibility Protocol Spike (`scripts/test_two_server_federation.py`):**
  - Verified F2 transfer handoff and binary execution continue to pass cleanly.
- [x] **Federation F3 Integration Spike (`scripts/test_f3_persistent_universe.py`):**
  - End-to-end multi-process test of live daemon REST endpoints: auth, companies, directory heartbeats, multi-cargo transfer handoffs, and commodity conservation audits.
