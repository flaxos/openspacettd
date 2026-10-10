# Core town station adapter — bounded implementation handoff

Prepared 6 October 2026. **Planning only; not dispatched or implemented.**
This helper supports the existing Core proof, not a new sprint or replacement
for [ACTIVE_EXECUTION_PLAN.md](../ACTIVE_EXECUTION_PLAN.md). Preserve that plan,
all campaign authorizations/counters, failures, and independent art work.

## Outcome and evidence boundary

Player-visible objective: through normal paid native commands, add one small
owned rail station near the existing Core town, connect it to existing owned
track and retarget the existing FOOD train's delivery stop so ordinary service
can count in native town activity. Do not change the town's growth rules to make
the proof pass. This handoff delivers tooling and focused proof; an actual campaign
continuation is a separate explicitly bounded owner handoff.

Planning checkout: `codex/plan-gameplay-scifi-tracks-recovered`, planning revision
`4b79537ef751596c828ebf5a175ba60a7e55c10f`, source base
`1c95f075c0bdd4d6ada912b58cc2993302520a94`. A read-only GitHub check on 6 October
still reports PR71 open at that source head, based on PR66
`7bc29a60cc1bc60b4d555d7500c2ded79c175eaa`. The recovered repository is shallow;
do not treat its ancestry or older prose as the current cloud worktree state.
Implementation must pin the gameplay owner's actual current revision/diff first.

**Reported by parent, not replayed in this planning task:** the full Core resource
basket was consumed, but the existing station anchor lies outside native town
growth radius. A candidate station spans (795,62)–(796,62), with one link tile
(794,62), and existing FOOD front train4 is the proposed affected vehicle.
No current save/map, native quote or command result establishing that candidate
was supplied to this local task. Its legality, actual track directions, exact
company/town IDs, train/order state and native activity eligibility remain runtime
preconditions. Do not copy those numbers into an unconditional production action.
The previously verified advance32/tick255232 cloud master is historical; it is
not automatically the latest post-consumption continuation source.

## Source facts verified in this task

- `src/town_cmd.cpp:ForAllStationsNearTown` compares
  `DistanceSquare(st->xy, t->xy)` to the live squared TownEdge radius.
  `CountActiveStations` then uses `time_since_load <= 20 || time_since_unload <= 20`.
  Catchment/FOOD acceptance alone does not prove an active nearby station.
- `UpdateTownGrowth` preserves native setting/goals and the unserviced-town
  stochastic check. Eligible station service is not a guarantee of immediate
  population or house growth. No Chance16/RNG/flag/rate edits are required.
- `src/portal/connected_economy.cpp:ConConnectedEconomy` routes basket/functional
  proof subcommands; `CoreBasket` rejects networking/dedicated mode. Existing
  station and order builders live inside complete fixture/service workflows,
  rather than a narrow existing-save quote/build/retarget operation.
- `CmdBuildRailStation` delegates normal authority, terrain, footprint, joining,
  construction and material checks and charges through the native path. Its
  arguments include explicit station-joining policy; `NEW_STATION` is distinct
  from unspecified joining. Use a new independent station so a distant existing
  station anchor is not silently retained through joining.
- `CmdModifyOrder` permits station load/unload/nonstop/stop-location changes,
  **not a station-destination replacement**. `InsertOrder` checks live destination,
  ownership/station usability and shared vehicles. Use native Insert/Delete/other
  existing native order commands as appropriate, never direct order field writes.
- Existing admission freezes train4's original orders and the basket session's
  expected paused state. A legitimate retarget invalidates those old comparisons.
  Do not weaken or overwrite the old campaign envelope to hide the change.

## Exact implementation scope and ownership

Work on one gameplay-owner-approved isolated branch based on its verified current
source. Reuse its configured build; no new heavy local build while disk remains
constrained. No contacting, resuming or sharing files with the Antigravity art task.

Allowed owning edits:

- `src/portal/connected_economy.cpp`: narrow separately guarded offline adapter,
  non-mutating quote/observation and normal command execution.
- `src/portal/connected_economy.h`: small testable admission/plan declarations only
  where needed; transient state must reset via existing lifecycle reset.
- `src/tests/test_core_construction_basket.cpp`: meaningful adapter regressions
  using existing native fixtures. `src/tests/CMakeLists.txt` only if an existing
  isolated/native case requires registration.
- `scripts/test_core_construction_basket.py`: bounded request/response decoding,
  explicit new operation allowlist and test cases. Preserve existing snapshots,
  archives and the original campaign runner; do not automatically execute its
  old advance loop. Prefer functions/unit checks over a new standalone runner.

No economy, megacity/town-growth implementation, station/order native engine
semantics, world/phase/role model, routing, save schema, NewGRFs, assets, CMake
build configuration, funding/research authoring or public console generalization.
`src/console_cmds.cpp` already registers connected_economy; no new general cheat
or remote command is needed. A genuinely missing native primitive is a replan
trigger, not permission to implement one here.

## Admission and proposed interface

Implement explicit read-only quote and apply operations under the existing
connected_economy proof console (names below are proposed, not current APIs):
`town-station-quote` and `town-station-apply` with one bounded plan envelope/token.
Avoid a generic arbitrary command/JSON execution endpoint. Validate exact argument
count/types/ranges and reject out-of-map coordinates before tile conversion.

Admission: normal game, loaded canonical integrated profile/content, offline
(no networking, clients or dedicated server), single expected company, paused
native state, identified owned front train with editable **unshared** ordinary
station orders, no crash/loading/depot transition that would make retarget unsafe.
Verify actual ownership before setting command context; native ownership guards
remain binding. Do not allow the old armed basket session to unknowingly accept
this mutation: require an explicitly ended/unarmed session, and a new owner-signed
continuation envelope after the helper mutation. Reject active proof ownership
rather than resetting its ledger inside this adapter.

Freeze save/binary/revision/content/config hashes and current train consist,
orders/current indices/status, city/world identity, RNG/date/tick, finances/debt,
all-cargo custody, infrastructure and live station cache observations. The source
save hash is verified by the existing wrapper; a client-supplied string alone
is not proof of native state. Reject stale/mismatched native fingerprints and
invalid/reused tokens. No grant, borrow, research, devmoney toggle, pause toggle,
simulation advance or save operation is included in these two helper commands.
Preserve the already approved saved offline devmoney mode and record all debits;
that mode does not bypass materials, land or command legality.

## Paused quote and construction protocol

1. Inspect the reported candidate only. Default maximum footprint is one native
   default station, one track, two platform tiles, Axis::X, on (795,62)/(796,62),
   with at most one native owned rail connector on (794,62). Coordinates remain
   envelope parameters pinned by the owner, not a new map search. No land clearing
   outside ordinary native station/rail commands, terraforming, bridges, signals,
   neutral-track adoption or demolition. Stop if another tile/topology is needed.
2. Read actual tiles/ownership/heights/railtype/trackbits/reservations/occupancy and
   immutable world membership for **every** footprint/link tile. Identify the
   proper connector track from native topology; do not assume Track::X from
   coordinate naming. Require no unsafe occupied/reserved construction and valid
   ordinary route access under the current 90-degree rule.
3. Native-query `BuildRailStation` and any required `BuildRail` with exactly the
   frozen execution parameters, normal class/spec, `NEW_STATION` and explicit
   adjacent policy. Reused owned link is a zero-cost no-op, not a duplicate charge.
   Report component costs, materials, errors, company cash/debt and predicted
   footprint. Snapshot before/after repeated query: zero cash/material/cargo,
   map/station/order/RNG/tick/counter mutation is mandatory.
4. Record target town center, squared radius, candidate anchor squared distance,
   expected FOOD catchment/acceptance and recent-activity criterion. Prediction
   is explicitly provisional: native creation may choose/maintain a different
   anchor. Do not write town association/catchment/radius by hand to force it.
5. Freeze the permitted positive total quote and exact command sequence. Re-query
   each actual command immediately before execution at the same paused state.
   Native command changes are authoritative. Distinct station/link tiles avoid
   pretending hypothetical clearing costs are actual quotes. A mismatch or native
   denial stops before that command; no retry/requote expansion silently occurs.
6. Execute station/link through normal `Command<Commands::...>::Do(Execute,...)`.
   Identify the returned station via the native resulting footprint/ID, not a
   hard-coded next pool index. Record actual anchor, owner/world/catchment and
   radius inclusion. Verify native cash/material changes equal the quoted values,
   with unchanged debt and no added assistance; devmoney exceptions must be
   measured as existing native behavior, not hand-edited accounting.

A sequence of native commands is **not an atomic transaction**. There is no valid
native InsertOrder quote to a station ID that does not yet exist. Therefore split
construction and retarget into explicit stages. If construction succeeds but
anchor/catchment/order validation fails, preserve the paid infrastructure and
return `partial-construction`, with exact completed commands/costs and untouched
orders. Do not claim rollback, undo cash, demolish the station, erase failed
attempts, or save/advance as if complete. A tiny station costing successfully is
not acceptance if its actual anchor still misses the growth radius.

## Native order retarget

After the live station exists and is usable by train4, copy only its frozen
FOOD delivery order semantics: type, load/unload, nonstop, stop location and any
supported timing/refit constraints. Preserve pickup and other order destinations.
Reject shared orders, conditional/depot/implicit/timetabled special cases that
cannot be reproduced faithfully by this bounded native command sequence. Reject
unbunching/refit/wait/max-speed differences before spending where observable;
normal InsertOrder explicitly rejects non-default timing/refit/max-speed fields.

Use native insertion of the new station order and deletion of the old order
(or another genuinely existing supported primitive proved by source). Never use
`MOF_COND_DESTINATION` for a station destination. Quote each actual order command
with its real live station ID immediately before execution. Stay paused with zero
simulation ticks; record intermediate list and current-order/index changes.
Check native handling of a currently active delivery order; if an existing normal
SkipToOrder/start-stop command is required, freeze it and its exact effects
before this retarget stage. No direct current_order/current-index writes.

An unexpected second-command failure preserves a truthful partial-order result
and halts with that train left safely controlled via an explicitly frozen native
stop command if needed. No automatic rollback/replacement/restart strategy.
On complete success require the same order count, pickup, unaffected flags,
consist, cargo and all other services, with only the selected FOOD destination
and its resulting native current-order reference changed. Duplicate apply is
rejected without more cost or list entries.

## Acceptance tests and finish line

| Test | Required observation |
| --- | --- |
| Admission/parser controls | Reject network server/client/dedicated, fresh/unpaused/foreign company, stale envelope, active competing proof, invalid coordinates/IDs, foreign/wagon/crashed/loading train and shared orders; no mutation. |
| Repeated native quote | Success and terrain/ownership/placement/material/occupancy denials match native commands; complete snapshots/RNG/tick/cargo/cash/orders unchanged, including failed queries. |
| Footprint and anchor | Candidate Axis::X covers exactly two intended tiles; new station cannot join/reuse distant anchor; actual station.xy/DistanceSquare/radius and FOOD acceptance recorded. Outside-radius/invalid receiver returns partial paid construction rather than a fabricated pass. |
| Paid construction parity | Each quote equals native execution debit/material effect; one connector maximum, owned compatible no-op reuse, no duplicate cost, no adjacent/neutral/unrelated infrastructure touched. Stale state/second command failure yields exact partial ledger. |
| Order legality and preservation | Native Insert/Delete against the live destination, preserved pickup/unload/load/nonstop/stop semantics, correct current indices, same final order count, other trains byte-equivalent in relevant state; shared/special order rejection. No direct order mutation. |
| Reapply/fault controls | Consumed token and duplicate apply denied; induced order failure leaves honest staged result with all previous native costs/actions preserved and zero hidden ticks. |
| Lifecycle/cold retention | On a disposable fixture, normal saved station/track/orders/finances/custody survive fresh-process reload; transient token/authority clears. No new saveload chunk. Cold restoration is not another operation allowance. |
| Native activity evidence | A later separately authorized normal FOOD visit sets native recent activity inside the actual radius. Record basket/growth hook/flags/house/population independently; no guarantee of instant growth or human acceptance. |

Implementer finish: small adapter and focused meaningful native regressions;
code/content delivery gates per ost-deliver (unit binary, isolated CTest, two
linters, diff check), exact counts/failures, scoped patch/commit for reviewer.
No code gate was run by this prose task. No campaign, native quote, station build,
order change, fresh replay, growth or human UAT is claimed here. If local disk
cannot support verification, use the existing gameplay owner's usable cloud
executor/build rather than create a new heavy local build; report missing gates.

Human mission after a separately authorized proof: load a copied identified
checkpoint paused, inspect nearer station/FOOD delivery order, observe one normal
FOOD unloading, inspect town growth status, save to a new name and cold reload.
Report first unexpected behavior and wait time; observation of a supplied network
is not human construction or ordinary-start financial balance acceptance.

## Copyable implementation prompt (not sent)

```text
Implement only the offline Core town station adapter in
 docs/handoffs/CORE_TOWN_STATION_ADAPTER.md, in the gameplay owner's separately
 assigned isolated branch from its actual verified current source. Read AGENTS,
 ost-dev/uat/deliver and the existing active plan; preserve that approved plan,
 original campaign guards, counters, saves and all art ownership. First identify
 exact post-basket checkpoint/content/build and any cloud-only diff. Validate
 candidate795–796,62/link794,62/train4 against native state; these are reported
 hypotheses, not permission to author state. Add non-mutating quote and bounded
 native station/link plus FOOD order retarget stages, no generic cheat endpoint.
 Use normal native paid commands and actual station IDs, quote/execute parity,
 offline/server-authority rejection and zero ticks. No station joining to the
 distant anchor, economy/town growth rule change, direct order mutation, grants,
 new fleet or map search. Handle partial construction/order failure honestly.
 Preserve other services and RNG; test native admission, quotes, failures,
 ownership, anchor radius, order semantics, reapply and cold state retention.
 Return source/files, exact focused/full gates, staged outcome schema and a
 five-minute mission. Do not run a campaign, advance/load an owner game, push,
 merge or contact the gate-art agent. A new campaign handoff comes from its owner.
 Stop for an absent native primitive, unsafe order transition, invalid candidate
 or a product/architecture change; provide the smallest concrete next decision.
```

## Remaining inputs before implementation/proof dispatch

- Actual current gameplay revision and any uncommitted cloud adapter changes;
  PR71 live head is not a substitute for its running binary/diff.
- Post-consumption paused save/manifest/hash, exact orders/current indices and
  the candidate's native tile/anchor/radius/FOOD acceptance observations.
- A fixed positive cost/material ceiling chosen from the actual paused quote,
  exact native command sequence and safe partial-failure train policy.
- New owner-approved continuation ledger before any campaign run. This document
  creates no new load, tick, retry, money, research or save authority.

## Retained evidence verification — 6 October 2026

Live PR71 still has source head `1c95f075c0bdd4d6ada912b58cc2993302520a94`.
The recovered planning branch descends from that source; this handoff does not
assert the unavailable cloud executor's current binary or private working diff.

Library archive `libfile_ee24f3542b6c8191a89a8ff6fed5ab7c`
(`core-basket-20261005-assisted-continuation.zip`) was materialized read-only.
Its 8,575,259 bytes have SHA256
`0d12e707bfe3d9f43578a97d5ad1efa779ea7d38941267f8ace86531557a98d4`.
ZIP CRC checks and all 27 operation / 24 checkpoint SHA256 comparisons passed.
Local evidence lives outside this repository at
`/home/flax/Documents/Codex/2026-10-05/task/core-helper-evidence`; this local
materialization path is not a cloud-executor path. Preserve the original archive/ledger.

**The ledger's final “complete basket NOT PROVEN” conclusion is stale.**
`operations/018-initial-advance-15.json` records tick 284,995: complete atomic
consumption of 20 STEL, 20 BALL and 60 FOOD, growth 1.0, passengers 1.0,
`native_growth_hook_enabled=true`, `native_growth_observed=true`, but
`native_growth_enabled=false`; population 1,191 and house count 29 did not grow.
This proves one basket consumption/hook event, not house growth, sustained basket
supply, cold replay equality or human acceptance. Source's monthly-audit test
explicitly distinguishes these outcomes. Operation 003 successfully restarted
BALL train 28 at zero ticks; the older restart failure is historical.

Final retained checkpoint `checkpoints/initial-advance-24.sav`:
SHA256 `89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f`,
tick 304,384, cash 3,520,926. This is after the proven basket event, but the final
month is incomplete again. New continuation totals were 24 requested advances,
49,152 ticks, one cold load, zero new grants/loans, native receipts 59,385 and
debits 5,672. Historical assistance and expired bounds remain recorded.

Train 4 is company 0's non-crashed/non-lost front engine, with consist
`4 -> 7 -> 6 -> 5`; the three FOOD wagons each have capacity 35, all empty in
the final snapshot. Ordinary orders are exactly
`[[1,2,2,4,1,0],[1,3,4,0,1,0]]`. Source serializes the fields as
`[type,destination,load,unload,nonstop,stop_location]`. Pickup is station 2;
delivery is station 3. Current destination 2, real order index 0,
current-order flags `[3,2,4,3,0]`, speed 0, stopped false, tile 204111.
These current transient flags differ from its ordinary order and must be handled
through native commands; speed zero alone does not mean the train is stopped.

Both retained configs specify map_x/map_y 10 (1,024 square). Station 3 anchor
is tile 64278 = (790,62); town 0 center is tile 71454 = (798,69). Their squared
distance is 113. The proposed new anchor (795,62) would have squared distance 58,
with connector (794,62) and second station tile (796,62). This arithmetic does
not prove the live TownEdge radius, placement legality, acceptance or route
continuity. The final snapshot omits the cached town radius, native recent
station activity, shared-order relation and full candidate tile command checks.
Those remain explicit native preflight inputs; no simulation was run to fill them.

## Exact implementation test scaffolding

Extend `src/tests/test_core_construction_basket.cpp`, using its existing
`BasketFixture`, `LoadedContext`, `Core()` label map and native command-authority
world. Resolve cargo by label, never archive numeric IDs: the fixture label
layout differs from the save. Use small fixture coordinates, not the 1,024-map
candidate. Keep existing `[core-construction-basket]` regressions unchanged.

1. Add a dedicated adapter envelope fixture with company/town/train/station IDs,
   full immutable snapshot, expiry/consumed token, maximum material/debit ceiling,
   explicit station rectangle and optional single connector. Table-driven
   admission rejects each prohibited context and stale/shared/special-order
   condition before any query or mutation. Compare snapshot/tick/RNG on rejection.
2. Add `[core-town-station][quote]` tests comparing repeated adapter query with
   native `BuildRailStation`/`BuildSingleRail` query and actual sequential commands
   on independent equivalent fixtures. Assert cash, all cargo custody, orders,
   RNG, terrain and ownership unchanged by query, including denial branches.
   Explicitly test `NEW_STATION`, two-tile Axis::X footprint, compatible connector
   reuse and insufficient materials despite offline infinite money.
3. Add `[core-town-station][anchor]` fixtures where old station has catchment but
   lies outside TownEdge; new anchor is inside, exactly on, and outside radius.
   Assert native station.xy and squared-radius relation, no automatic join,
   native FOOD acceptance, exact paid debit/material ledger. An outside-radius
   result after native paid construction is reported as partial, not rolled back.
4. Add `[core-town-station][orders]` independent order lists and shared-list
   rejection. Insert the real new station at delivery index 1, then delete old
   delivery now at index 2, quoting each native operation immediately before
   execution. Freeze the actual sequence only after tests cover current pickup,
   current delivery, loading state and native index/current-order adjustment.
   Never prequote an invented station ID. Preserve six ordinary-order fields,
   count, pickup, consist/custody and every other service. If native safety is
   unproven for a state, reject it rather than inventing current-order writes.
5. Add `[core-town-station][partial]` controlled stale-state/second-command failure
   cases: durable paid work remains, exact completed commands/debits are returned,
   consumed-token reapply is denied, and order failure stops under the frozen
   native safe-train policy with zero simulation ticks. Use existing test seams
   or a narrow private fault seam; do not expose a generic command bypass.
6. Extend `scripts/test_core_construction_basket.py` only with an explicitly
   selected adapter regression mode and disposable fixture/save inputs. Reuse its
   existing bounded process/config discipline; do not reset campaign defaults,
   counters or archive files. A native save/reload test verifies station/track/
   orders/cash/custody persistence and clears transient authorization. Register
   a new isolated CTest only if the existing entry cannot select these cases.

Implementation gates: focused `[core-town-station]` and inherited
`[core-construction-basket]`, then the unit binary, isolated CTest and both
linters once after final code changes, per ost-deliver. Record exact results and
missing runtime gates. No build/test/campaign run is claimed by this planning
artifact. The existing monthly-consumption test is retained as a regression,
not rewritten to claim house growth.

## Updated copyable prompt — prepared, not dispatched

```text
Implement only docs/handoffs/CORE_TOWN_STATION_ADAPTER.md, including its
6 October retained-evidence and test-scaffolding sections, on the gameplay
owner's separately assigned isolated branch. Read AGENTS and ost-dev/uat/deliver;
preserve ACTIVE_EXECUTION_PLAN and the original expired campaign evidence.
PR71 source is 1c95f075c0bdd4d6ada912b58cc2993302520a94. Obtain the owner's
actual source/binary/private-diff identity before editing; unavailable cloud
execution does not authorize a replacement campaign or heavy local build.
The verified archive proves basket consumption at284995, despite its stale
NOT PROVEN summary. Final checkpoint304384 has hash89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f;
train4 ordinary orders target pickup2/delivery3, consist4->7->6->5.
Use the explicit native preflight gaps and six test groups in the handoff.
Implement the offline nonmutating quote and staged paid native station/link/order
adapter only. Candidate795–796,62/link794,62 requires actual native validation.
Native NEW_STATION prevents joining the far anchor. Use actual station IDs and
Insert/Delete commands; preserve current-order semantics through native paths.
No direct state edits, economy/growth rules, grants, new fleet, map search,
graphics ownership, campaign advancement, owner-save load, push or merge.
Return focused/full gate results, honest partial-outcome schema and human mission.
Stop if native safety/primitive or candidate validity fails; report the concrete
missing fact. This is implementation preparation, not new continuation authority.
```
