# OpenSpaceTTD Sprint 15 — Federation Prototype (Technical Spike F2)

Status: **COMPLETE**

Sprint 15 implements and validates **Phase F2: Federation Prototype (Technical Spike)**. It establishes an authoritative transaction coordinator (Universe Authority daemon & in-engine client), implements inter-server portal gate registrations, provides full train consist despawn on departure and emergence/materialization on arrival with track reservation acquisition and throat clearance checks, guarantees strict commodity conservation with zero vehicle/cargo duplication, and verifies the end-to-end multi-process handoff protocol.

---

## Player & Architecture Outcomes

- **Single-Map Continuity:** Local intra-map portal wormholes and single-map gameplay continue to operate with 100% backward compatibility.
- **Authoritative Consist Handoff:** Trains entering an inter-server portal gate are safely despawned, their reservations lifted, and their state transferred through transactional ledger states (`LOCKED -> DEPARTED -> IN_TRANSIT -> ARRIVAL_PENDING -> COMPLETED`).
- **Emergence & Materialization:** Receiving server instances validate content manifests, verify portal throat clearance, allocate pool vehicles, recreate train topology and dynamics, restore cargo packets with origin provenance, acquire PBS tunnel/track reservations, and anchor `GlobalConsistID`.
- **Zero Duplication & Commodity Conservation:** In-transit and completed ledger accounting enforces the invariant:
  $$\sum \text{Cargo}_{\text{Initiated}} = \sum \text{Cargo}_{\text{Completed}} + \sum \text{Cargo}_{\text{InTransit}}$$
  with zero commodity loss and zero duplication.

---

## Technical Components Delivered

### 1. Universe Authority Service & Daemon
- **In-Engine Service (`src/portal/universe_authority.h`, `src/portal/universe_authority.cpp`):**
  - World registry (`RegisterWorld`, `UnregisterWorld`, `GetWorlds`).
  - Inter-server route topology (`RegisterRoute`, `FindRoute`).
  - Transfer state machine (`InitiateTransfer`, `DepartTransfer`, `QueryPendingTransfers`, `ClaimTransfer`, `ConfirmTransferArrival`).
  - Commodity conservation ledger and discrepancy auditor (`GetCommodityAudit()`).
- **REST Daemon (`scripts/universe_authority.py`):**
  - Standalone Python 3 HTTP daemon (`http.server`, zero dependencies) providing REST endpoints for world discovery, portal routes, transfer lifecycle handoffs, and commodity audits.

### 2. Inter-Server Portal Gate Extension
- **`InterServerPortalLink` (`src/portal/portal_type.h`):**
  - Connects local portal gate tiles to remote worlds and remote portal gate IDs with virtual route length.
- **Registry Integration (`src/portal/portal_registry.h`, `src/portal/portal_registry.cpp`):**
  - `RegisterInterServerPortal`, `IsInterServerPortal`, `GetInterServerPortal`, `UnregisterInterServerPortal`, `GetAllInterServerPortals`.
  - Pathfinder boundary exit integration in `src/pathfinder/follow_track.hpp`.
  - Automated departure trigger in `TrainController` (`src/train_cmd.cpp`).

### 3. Consist Despawn & Materialization
- **Consist Despawn (`ConsistMaterializer::DespawnForTransfer` in `src/portal/consist_materializer.cpp`):**
  - Captures v2 snapshot with `GlobalCompanyID`, `GlobalConsistID`, and cargo provenance.
  - Releases portal transit progress.
  - Frees PBS train track reservations via `FreeTrainTrackReservation` and lifts tunnel gate reservations.
  - Releases vehicle anchor in `FederationIdentityRegistry`.
  - Cleanly deletes consist chain with zero memory leaks.
- **Throat Clearance & Emergence (`ConsistMaterializer::MaterializeFromTransfer`):**
  - Verifies content manifest token match against receiving server.
  - Tests throat clearance (`CheckThroatClearance`): checks tunnel reservations and occupying vehicles.
  - Allocates pool items and instantiates engine and wagon chain.
  - Restores vehicle dynamics (speed, subspeed, acceleration, flags).
  - Restores cargo packets, station/origin provenance, and feeder share.
  - Restores persistent `GlobalConsistID` anchor.
  - Acquires emergence PBS tunnel/track reservations.

### 4. Game Commands & CLI Inspection
- **`CmdDispatchInterServerTransfer` (`src/portal/federation_cmd.h`, `src/portal/federation_cmd.cpp`):**
  - Validated game command dispatching consist departure across server boundaries.
- **Console Command `federation_status` (`src/console_cmds.cpp`):**
  - Displays registered worlds, active routes, pending transfers, and commodity ledger audit in the in-game console.

---

## Automated Acceptance & Verification

- [x] **Catch2 Transfer Suite (`src/tests/test_federation_transfer.cpp`):**
  - Inter-server portal registration, lookup, virtual length, and unregistration.
  - Complete Universe Authority transfer state lifecycle (`Locked -> InTransit -> ArrivalPending -> Completed`).
  - Strict commodity conservation ledger audit invariant ($\sum \text{Cargo}_{\text{Init}} = \sum \text{Cargo}_{\text{Done}} + \sum \text{Cargo}_{\text{Transit}}$, discrepancy $= 0$).
  - Consist despawn: snapshot capture, reservation release, clean vehicle destruction.
  - Consist materialization: vehicle pool allocation, dynamics restoration, cargo packet reconstruction, tunnel reservation acquisition.
  - Failure handling: throat obstruction wait, content manifest mismatch rejection.
- [x] **Multi-Process Spike Test (`scripts/test_two_server_federation.py`):**
  - Spawns Universe Authority daemon, executes HTTP REST registration, routes, transfers, claims, confirmations, and audit verification against live daemon.
  - Validates headless `openttd -h` invocation.
- [x] **Full Regression & Unit Test Suites:**
  - `openttd_test`: 174 test cases, 14,445 assertions pass (0 failures).
  - `ctest`: 178/178 tests pass (100% pass rate).
- [x] **Zero Engine Compilation Warnings or Errors:**
  - Both `openttd` and `openttd_test` build and link cleanly with GCC 13/Ninja.
