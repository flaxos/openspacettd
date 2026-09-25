# Federation reliability and scheduled freight

Status: **Implementation in progress** — 2026-09-25.

## Acceptance boundary

Run one authority, two dedicated game servers and an ordinary joined client on
each server, on the same host. Each game retains the existing single global map
and logical world regions. This work does not introduce a multi-map engine.

A locomotive with two coal wagons must run a cyclic station schedule from a
native coal mine and collection station on A to a native power plant and delivery
station on B. Prove at least five deliveries and returns through natural gate
entry, without manual dispatch, reversal or cargo injection after setup. Record
cargo, revenue, ownership, full global identity, provenance and schedule state.

Recovery is limited to the **latest coordinated checkpoint**: drain authority
requests and native commands, pause both simulations, synchronously save both
games, and preserve the matching persisted authority state. Test blocked arrival,
destination restart, authority restart, client reconnect and complete reload.
Each independent recovery case must complete at least three more deliveries.
Cover pre-departure hold, authority custody and confirmed arrival. During a
30-second authority outage, servers must remain responsive and clients connected;
retries retain the original request and transfer identities.

Unexpected process crashes, arbitrary old or mismatched saves, write-ahead crash
recovery, conditional/shared orders, timetables and distinct-company economies
are outside this acceptance boundary. Ordinary station schedules use one mapped
company identity. Human federation signoff remains separate from automation.

## Implementation and verification record

- Integrate current main, the cargo/terrain recovery and savegame runbook, and
  relevant federation fixes from PRs #36 and #37. Exclude tutorial, artwork and
  year-2050 changes from PRs #38 and #39.
- Establish the integrated baseline before attributing combined-suite failures
  to the federation changes.
- Remove synchronous authority waits from multiplayer ticks. HTTP completion
  supplies data; deterministic native commands perform shared state changes.
- Preserve full consist identities and portable schedules through transfer and
  save/load. Never resolve a foreign station by a coincident local pool ID.
- Extend the existing multiplayer harness and session controller with native
  production, repeated scheduled delivery and coordinated recovery evidence.
- Run combined unit tests, isolated CTests, shuffled affected tests, repository
  linters, and the existing solo cargo/terrain and legacy-content regressions.
- Record exact binary/content hashes and machine evidence before publishing a
  human-playable session and concise UAT instructions.

No passing scheduled-freight or recovery result is recorded yet. Update the
canonical roadmap, architecture, limitations, sprint ledger and UAT results with
actual evidence when these checks complete.

## 2026-09-25 — Federation scheduled freight (draft)

Branch `fix/federation-scheduled-recovery` integrates main with PRs #36/#37
and the existing UAT cargo/terrain recovery and authoring guide. New work adds
nonblocking multiplayer authority requests, explicit company/station mappings,
portable station schedules, packet provenance and coordinated-checkpoint tools.

This is **in progress, not sprint completion**. Combined unit tests previously
passed 438 cases / 66,610 assertions. The transport-only fixture passed a
30-second authority outage and destination restart with joined clients. The
scheduled native coal fixture completed three loaded deliveries and empty
returns (180 coal accepted) before its 600-second timeout; the required five
cycles and full recovery matrix have **not passed**. Conservation/payment checks
after five cycles, nonempty custom-state reload, blocked arrival and checkpoint
phase coverage remain unverified. Human federation UAT remains pending.

See [the implementation record](FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

### Evidence locations and remaining gates

- Combined unit log: `/tmp/federation-schedules-unit4.log`.
- Transport-only pass: `build/federation-transport-recovery-2/transport-recovery.json`.
- Timed-out scheduled pilot: `build/federation-scheduled-smoke-2/`.
- Checkpoint manifest tests: `python3 scripts/test_federation_checkpoint.py` (2 pass).

These local build artifacts are not published human acceptance evidence.
The draft retains the timeout and accounting assertions so the incomplete run
cannot be mistaken for a successful acceptance result. The latest client-join
harness adjustment also still needs a live rerun. Complete the recovery cases,
final solo/legacy-content regressions, shuffled tests and CI before promotion.

Pre-publication verification: combined unit suite passes 438 cases / 66,610
assertions; isolated CTests pass 449/449. Both repository linters, checkpoint
manifest tests and `git diff --check` pass. This does not supersede the failed
scheduled pilot or the remaining acceptance gates above.
