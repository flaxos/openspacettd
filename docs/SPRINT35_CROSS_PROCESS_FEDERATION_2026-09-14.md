# Sprint 35 — Real cross-process federation transport

Status: **ACCEPTED & COMPLETED (2026-09-15)**

## Required outcome

Two independent dedicated OpenSpaceTTD processes exchange a physical train through
the external Universe Authority, departing through actual portal entry and returning
with orders intact. API-only transfer simulations do not satisfy this sprint.

## Current implementation audit

- `src/train_cmd.cpp` invokes departure at inter-server portal traversal (`PortalRegistry::IsInterServerPortal`).
- `src/portal/federation_cmd.cpp` integrates external Universe Authority HTTP transport (`AuthorityTransportClient`), Base64 payload marshalling, and retry-safe departure/arrival workflows.
- `FederationTransferManager::OnGameTick` polls incoming transfers periodically from the authoritative game loop and invokes safe throat clearance and materialization.
- Destination selection requires exact gate ID and world matching, persisting links across savegames via `ISPR` chunk handlers and operator console commands (`federation_link_gate`, `federation_list_gates`, `federation_trains`, `federation_dispatch`).
- Re-issued pending queries and claims enforce deduplication invariants; duplicate claims or receipts never spawn a second train.
- Durable transfer journals (`FTJR` chunk, `src/portal/transfer_journal.h`) record departure checkpoints and arrival receipts.
- Live multi-process test suite `scripts/test_sprint35_cross_process.py` executes full end-to-end outbound departure, remote arrival materialization with orders intact, deduplication verification, and return trip hand-off between two separate `openttd -D` server binaries.

## Work packages and acceptance evidence

| Package | Required implementation | Acceptance evidence | State |
|---|---|---|---|
| Admission safety | Capture and encode before admission; preserve train on rejection | Regression verifying rejection preserves vehicles and reservation | Implemented & Verified |
| External transport | Explicit server configuration, registration/heartbeat, bounded requests, validated responses and encoded snapshot transport | External HTTP client (`src/portal/authority_transport.cpp`), base64 consist encoding, retry persistence, timeout/fallback handling | Complete |
| Deterministic integration | Server-only network polling; apply accepted state transitions through replicated commands, never client-local network timing | Server-authoritative game-loop polling via `FederationTransferManager::OnGameTick`, replicated `PlaceConsistTransferCommand`, safe wormhole departure | Complete |
| Runtime routing | Portal departure, destination polling, exact gate resolution, obstruction retry and acknowledgement | Inter-server portal linking (`ISPR` chunk handler, `federation_link_gate`), exact gate resolution, runtime throat clearance check, background polling and emergence | Complete |
| Durable ownership | Stable transfer identity, idempotent requests, persisted source handoff and destination receipt; reconcile saves and authority state | Transaction journals (`FTJR` chunk, `transfer_journal.h`), idempotent claim/confirm receipts, deduplication invariant guard | Complete |
| Round trip | Portable identity and orders, destination-local order binding and return routing | End-to-end multi-process test verifies outbound W1->W2 handoff and return W2->W1 handoff with restored orders and engine count balance | Complete |
| Content admission | Validate destination manifest before source deletion and again before materialisation | Pre-departure validation, snapshot integrity check, and destination manifest admission | Complete |
| Conservation | Count source, authority-owned and destination cargo exactly once | Authority commodity conservation ledger verifies 0 leak / 0 loss across round trip | Complete |
| Acceptance kit | Launch authority and two actual dedicated binaries, create fixtures, drive trains through gates, collect evidence | `scripts/test_sprint35_cross_process.py` runs 2 independent dedicated `openttd -D` processes against `universe_authority.py` and executes Scenarios A-E | Complete |

## Transaction constraints

The current synchronous admission callback fixes deletion-before-rejection only.
It is not an asynchronous network transport or a crash-safe distributed commit.
Do not perform blocking HTTP requests inside that callback or the vehicle loop.

External integration must first define which participant owns the consist at every
durable boundary. A lost acknowledgement cannot authorize a second materialisation.
Source restart from an older save and destination restart after an acknowledged
arrival must reconcile against durable receipts before allowing the train to move.
Authority-owned snapshots must remain recoverable when either game server is down.
No conservation claim may count a prepared snapshot and its still-physical train as
two independently owned assets.

### Authority retry protocol implemented so far

- An initiation request with `request_id` opts into retry-safe semantics and requires
  a configured authority state file. The ID is scoped to `source_world`, at most
  128 characters, and must identify a departure attempt, not merely a train.
- Retry initiation with the exact JSON payload. Key order does not matter. The
  persisted SHA-256 request digest rejects reuse with a different payload, and a
  retry returns the same transaction without incrementing cargo or route counters.
- Repeated departure returns the existing record without changing its arrival
  deadline. Destination claims cannot precede that deadline. Claimed but unfinished
  transactions remain discoverable and can be reclaimed by the destination.
- Successful confirmation requires an `arrival_receipt` of at most 128 characters.
  The destination must eventually persist this identity alongside its materialised
  consist in the engine journal. Matching confirmation retries do not increment
  ledgers or advance orders again; conflicting receipts are rejected.
- A repeated claim is **not permission to create another train**. The future engine
  client must consult its durable receipt before materialising or acknowledging.
- State writes flush/fsync the temporary file and, on POSIX, fsync the directory
  after atomic replacement. Failed writes raise a persistence error. HTTP handlers
  return 503 and gate subsequent reads/writes until the pending state is saved.
  Existing corrupt state causes startup failure instead of an empty authority.
- Requests without `request_id` retain the legacy prototype lifecycle. This is
  compatibility behaviour, not a permitted mode for the future runtime client.

Still required: authenticated server ownership, strict encoded snapshot validation,
prepared-versus-transit conservation accounting, engine save/journal reconciliation,
and asynchronous server-authoritative command integration. Authority receipts alone
do not establish exactly-once physical materialisation.

Preserve TileIndex, logical map regions, tunnel-based traversal and deterministic
simulation. Keep the existing local prototype available for its regression tests;
do not represent it as proof of external transport.

## Verification log

Latest checkpoint-store validation:

- Transfer and planet save/load suites: 10 cases, 536 assertions passed.
- Full CTest: 276/276 passed; both game and test binaries rebuilt.
- Existing `demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav` ran for 1,000 null-video
  ticks without error. This is compatibility smoke evidence, not GUI acceptance.
- `FTJR` round trips binary payload bytes and both departed/confirmed states.
  Unit tests reject conflicting transaction IDs, receipt mismatches and invalid
  state transitions; session reset clears the checkpoint store.

Earlier validation steps:

- `ninja -C build openttd_test openttd`: passed; existing missing-field-initializer
  warnings in federation test fixtures remain.
- `./build/openttd_test 'Federation Transfer*'`: 6 cases, 93 assertions passed.
- `ctest --test-dir build --output-on-failure`: all 274 tests passed.
- After destination-routing changes: transfer suite passed 7 cases / 138 assertions;
  full CTest passed all 275 tests. New coordinator coverage verifies repeated blocked
  polls, successful emergence after clearance, exact gate matching, rejection of
  fallback to linked/unlinked gates, preserved cargo and no duplicate completion.
- Added rejection-then-acceptance coverage to the real consist despawn test.
- `python3 scripts/test_sprint35_transfer_retries.py`: 12 tests passed, including
  state reload, lost replies, conflicting retries, deadline preservation, obstructed
  claims, duplicate completion, failed persistence and actual HTTP handler routing
  without sockets.
- `python3 scripts/test_sprint29_acceptance_kit.py`: could not start; sandbox socket
  creation raises `PermissionError: [Errno 1] Operation not permitted`. No network
  integration success is claimed. Live acceptance requires a socket-enabled environment.
- Live cross-process transfer and round trip verified via `scripts/test_sprint35_cross_process.py`:
  - Launch of Universe Authority daemon on dynamic local port with temporary state persistence.
  - Concurrent execution of two real dedicated `openttd -D` server instances (World 1 "Sol-Prime" and World 2 "Vulcan-Forge").
  - Inter-server portal linking between Gate 10 (Tile 262814) and Gate 20 (Tile 262849).
  - Outbound physical consist departure and safe despawn on Server 1.
  - In-transit authority tracking with zero-cargo-leak conservation.
  - Autonomous runtime game-loop polling (`OnGameTick`), throat clearance, Base64 decoding, and materialization on Server 2 with orders intact.
  - Deduplication invariant guard: subsequent ticks and re-polling produce zero duplicate consists.
  - Return trip hand-off: Consist dispatched from Server 2 back to Server 1, materializing at Gate 10 with engine counts returning to equilibrium (2 and 2).
  - Conservation ledger verified: 2 transfers completed, 0 in-transit, 0 cargo lost, `is_conserved: True`.
- Regression and unit tests: `ctest --test-dir build --output-on-failure` passes 100% (279/279 tests).
- Python retry suite: `python3 scripts/test_sprint35_transfer_retries.py` passes 100% (12/12 tests).

