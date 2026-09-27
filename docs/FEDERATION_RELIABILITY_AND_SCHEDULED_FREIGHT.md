# Federation reliability and scheduled freight

Status: **Scheduled freight and all nine recovery cases pass; human graphical UAT pending** — 2026-09-27.

## Acceptance boundary

Run one authority, two dedicated game servers and an ordinary joined client on
each server, on the same host. Each game retains the existing single global map
and logical world regions. This work does not introduce a multi-map engine.

A locomotive with two coal wagons must run a cyclic station schedule from a
native coal mine and collection station on A to a native power plant and delivery
station on B. Prove at least five deliveries and returns through natural gate
entry, without manual dispatch, reversal or cargo injection after setup. Record
cargo, revenue, ownership, full global identity, provenance and schedule state.

Recovery is limited to the **latest coordinated checkpoint**: pause both simulations, drain outstanding authority
requests and native commands while paused, synchronously save both
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

The cold-start scheduled-freight acceptance now passes five loaded deliveries
and empty returns (300 coal), with exact cargo and cash reconciliation. All nine
recovery cases pass with three further cycles each (32 round trips / 1,920 coal
in the retained sequence). The dated record below preserves the earlier pilot
result; it does not supersede current evidence.

## 2026-09-25 — Historical scheduled-freight pilot

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

### Historical pilot evidence and then-remaining gates

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

## Reproducing freight and recovery acceptance

Use the configured build and installed OpenGFX. The harness creates disposable
configs, two servers, two joined clients and one local authority. It does not use
player profiles or user saves. Run the suites sequentially: some unit and CTest
fixtures share temporary save filenames.

```sh
ninja -C build -j2
./build/openttd_test
ctest --test-dir build --output-on-failure
python3 scripts/test_federation_checkpoint.py
python3 scripts/test_federation_multiplayer.py --scheduled --deliveries 5 \
  --recovery --output build/federation-acceptance-new
```

Use a new output directory each time. Normal-speed native production needs a
startup period and several minutes per round trip; the timeout scales with the
requested cycle count. Console progress prints each completed return and each
recovery result. `scheduled-result.json` records the requested cycles and
accounting; its `passed` flag requires at least five cycles. A shorter
`--scheduled --deliveries 1` run is diagnostic only. `scheduled-recovery.json`
passes only after the full recovery matrix.
`recovery-progress.json` retains individually completed cases if a later case
fails. A directory or checkpoint by itself is not a passing result.

The recovery matrix covers destination restart, source restart, a 30-second
authority outage during prepared departure, both clients reconnecting, complete
reload, and reloads at prepared departure, authority custody and loaded arrival.
A separate obstruction case places an actual stopped locomotive in a depot on
the destination approach through native commands. It must prevent materialization
until the engine/depot are removed and track restored through native commands.
Every case then requires three additional loaded deliveries and empty returns.

Cargo reconciliation uses the existing read-only native audit hooks: initial
physical stock plus authority-held stock plus production allocated to stations
must equal final physical stock plus station losses plus consumption. Money
changes must equal all native cash debits, including income and infrastructure
costs. Monthly industry/company history is displayed as supporting diagnostics;
it is not substituted for exact counters over long recovery runs. Audit counters
are process-local observers, restarted at each paused case boundary. Persistent
train, packet, order, company, receipt and journal state is compared separately.

Only the latest coordinated two-save/authority/binary set is accepted for reload.
The checkpoint first pauses physical movement, then consumes outstanding HTTP
results through native replicated commands. It waits for HTTP and command queues
to drain before saving. This is not arbitrary-crash recovery or a write-ahead log.

For a harness-only correction after a verified cold-start baseline, preserve the
binary and continue from the previous output's newest checkpoint:

```sh
python3 scripts/test_federation_multiplayer.py --scheduled --recovery \
  --resume-checkpoint build/previous-run/latest-checkpoint-directory \
  --output build/federation-recovery-continuation
```

The runner verifies the binary/artifact hashes and rejects older checkpoints.
It carries forward completed case reports, reloads the matched pair and authority,
and runs the unfinished cases. Reports retain their original checkpoint paths.
Do not use this path to claim fresh generation after changing the binary/content.

## 2026-09-27 — Acceptance result

**All nine recovery cases pass:** physical blocked arrival, loaded-arrival
reload, authority-custody reload, prepared-departure reload, a 30-second authority
outage, destination restart, source restart, both clients reconnecting, and full
coordinated reload. Each case completes three further loaded deliveries and
empty returns with exact cargo and cash reconciliation. The retained sequence
contains 32 round trips and 1,920 coal delivered. During the authority outage,
both clients stayed connected and the maximum measured console response was
0.332 seconds; the original prepared request completed exactly once.

The loaded cargo/master-schedule save/load regression passes. Combined tests pass
439 cases / 66,636 assertions, isolated CTests pass 450/450, randomized federation
tests pass 59 cases / 1,172 assertions, and all three Python checkpoint/stdout
regressions pass. Current and legacy solo cargo/terrain recovery regressions also
pass. Graphical federation UAT remains separate and pending.

Machine evidence is recorded in [the compact machine evidence](../demo/FEDERATION-FREIGHT-RECOVERY.evidence.json),
including the frozen binary hash, checkpoint hashes, accounting and continuation
provenance. Recovery uses only the latest coordinated set; arbitrary crash
recovery and old/mixed-save rollback are outside this acceptance boundary.

The five-cycle baseline was generated cold with the frozen engine. Recovery was
continued from the latest matched checkpoints after harness corrections. The
first three recovery cases are retained from `build/federation-recovery-matrix-1`;
the remaining six passed in `build/federation-recovery-matrix-2`. A partial stdout
JSON record interrupted an earlier attempt at prepared-departure recovery. The
reader now waits for a complete line, has a regression test, and that case was
rerun from its latest coordinated checkpoint. This was a harness parsing failure,
not an engine crash; the continuation does not claim another cold baseline.

Full reports remain in the local build directories. The compact published
record includes their SHA-256 hashes and independently reconciled cargo/cash
values. The frozen binary SHA-256 is
`bafa1f88b35e5971d1c71047e9ebb07acfaeca51e81aecc134f676981e29f98b`.
