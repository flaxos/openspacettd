# New integrated games: connectable terminals and a Core town

**Owner-review proposal — 30 September 2026. Planning only.** The owner approved
preparing both repairs in parallel, with no gameplay implementation until this
concrete plan is approved. No implementation, merge, save migration or food
operation is authorized by this document. The [food execution plan](ACTIVE_EXECUTION_PLAN.md)
remains blocked; this proposal does not become a second active execution plan.

The proposed bounded outcome is an ordinary fresh integrated game with public
terminal joins the player can legally connect to, and one living Core town that
can supply an existing house receiver. It establishes generation prerequisites,
not an affordable or profitable food service, city growth, research or complete A2.

## Current ancestry and evidence

Verified main is `82e7b1ac75bbd198bcd5b2e61996d2fa7a75a826`: it contains
[PR58 maintenance](https://github.com/flaxos/openspacettd/pull/58),
[A1 PR60](https://github.com/flaxos/openspacettd/pull/60) and the reviewed food
plan from [PR61](https://github.com/flaxos/openspacettd/pull/61). PR61 merged into
A1 at `5183d2e47904549820b91d227d10f91e857aa33c`; its plan tree equals reviewed
`6b89358863941e6c902816f3940e9e7d3b001ccc`. A1 subsequently merged into main.

[PR62](https://github.com/flaxos/openspacettd/pull/62) merged into the old A1
branch at `10856e709e492168f5c3fccb89ae4a27daf16643`, **not main**. Its tested
partial designation/harness code and failed food preflights are references, not
part of this main-based docs-only proposal. Parent independently verified exact
PR62 head `78a30686c3467306b24dc1f0a6cf215f66f484dd` at 18:49 UTC: all eight
platform jobs plus content pass in
[run36756654376](https://github.com/flaxos/openspacettd/actions/runs/36756654376);
docs, file-description, unused-string and script-mode pass. That build is red only
at the same 18 inherited A1 compiler annotations; no new source/test failure.
Its preserved non-ASCII commit-history gate still fails, separately from the
owner merge-subject failure. These are PR62 results, not this proposal's CI, and
are not rerun here. Main keeps PR58's maintenance. Do not import its full tree, revive
its obsolete clean-history candidate, rewrite either merge or remove PR58.
Preserve [PR59](https://github.com/flaxos/openspacettd/pull/59) and all A1 evidence.

The pinned [food blocker audit](https://github.com/flaxos/openspacettd/tree/78a30686c3467306b24dc1f0a6cf215f66f484dd/docs/audit/2026-09-30/core-food-preflight)
records zero spending/progression on retained states:

- Seed11's authorized operation save has one Industrial town, no Core town or
  Core house receiver. Seed101 also has only an Industrial town.
- Seed2026 has a genuine Core Town0, population 2,366, 55 houses, but its complete
  grain route fails at the Industrial link2 terminal's only external ground-rail
  join. Its 34 internal rail tiles are intact and traversable. The neighboring
  tile is another neutral terminal's incompatible horizontal track.
- Native track-follow rejects that edge with `NoWay`; native vertical rail
  construction rejects it with `STR_ERROR_OWNED_BY`. Public compatible rail is
  already traversable. A planner accepting compatible neutral rail cannot repair
  this geometry under unchanged construction rights.
- Complete-chain quotes, affordability, paid FOOD operation and consumed baskets
  are **unproven**. Neither the retained seed nor its acceptance start may be
  silently switched. Human A1 and food acceptance remain Pending.

## What the source establishes

[FindGeneratedGatewaySite](../src/portal/world_gen.cpp) checks the head and
`PortalTerminal::Plan(...).tiles` for clear/tree ground, flatness, common height
and existing claims. [PortalTerminal::Plan](../src/portal/portal_terminal.cpp)
ends its main lane at distance 18, `connection_tile`. The external join at
`TileAddByDiagDir(connection_tile, outward_dir)` (distance 19) is absent from those
checks. Current pair generation carries no claims for previous clear joins;
later terminals can occupy one without overlapping any built rail footprint.
Arrival-zone claims likewise contain only heads and terminal rail tiles.

This source omission supports a narrow deterministic geometry repair. The
retained seed2026 coordinates show the resulting interference; the proposal does
not claim a replay of every RNG/placement step or damage to internal terminal rails.
Current tests in [test_world_gen.cpp](../src/tests/test_world_gen.cpp) check built
rail bits/signals, not the external join's native traversability/connectability.

[GenerateTowns](../src/town_cmd.cpp) runs once globally after layout and
[IntegratedEconomy::StartNewGame](../src/portal/integrated_economy.cpp), before
industries and final arrival zones. The [canonical config](../demo/integrated_economy.cfg)
sets `number_towns=4` (custom selector), `custom_town_number=1`: **one town globally**,
not four or one per world. It accepts the first successful random legal placement.
[CheckTownPlacement](../src/portal/planet_manager.cpp) permits Core, Developed and
Frontier phases, and bars Phase4; it promises no Core town. This is a false
saved-start assumption, not a proven town-count defect or corrupted save. The
new guarantee below is an explicit product contract change.

## Proposed invariants and scope

| Proposed invariant | Exact boundary |
|---|---|
| T — Every successfully generated public terminal has a legal external join | Its head, full rail/signal footprint and one outward joining tile are valid inner tiles in the same world; terminal rail geometry is intact; the joining tile remains clear/tree, flat at terminal height and ordinarily buildable as compatible rail. No other generated terminal/advertised arrival footprint may consume that join. |
| C — Every successfully started ordinary integrated new game has at least one valid Core town | A native Town has positive population, its center has saved economic role Core, and it has at least one real house belonging to that town within the same Core world as its center. Use one slot of the existing configured global target, never add a bonus town. |
| D — Same inputs reproduce the new result | Seed, settings, content, world IDs and algorithm revision produce the same ordered placements, roles, rails, town/house state and RNG state, or the same bounded failure. No wall-clock-dependent choices, unordered iteration or GUI/network-worker bypass of native generation/command authority; preserve existing native generation ownership. |
| S — Loading existing saves preserves their existing contract | No generation repair runs on load; no new town/track, ownership change, reserve migration or new saved requirement is applied. Existing seed11/101 absence and seed2026 obstruction remain recorded facts. |

T is a shared generated-gateway bug fix: it may change future non-integrated
multi-world gateway positions through the existing shared helper. C applies only
to ordinary **new games with integrated rules actually enabled**, not Classic,
scenario editor, empty maps, loaded saves or a content configuration that failed
to activate integrated rules. Do not conflate development phase with economic
role: filter with the initialized Core role, then retain native phase/placement
checks. No role reassignment, world resize, climate/terrain relaxation, new town
founding permission or changed configured count.

Generation need not automatically designate the town, construct a player station,
grant cargo or ensure an affordable whole-chain route. Main still has the direct
GUI designation path; PR62's tested command wiring is absent. Importing/reviewing
that partial code is separate food-follow-up delivery, not generation work.
“Receiver” below means a potential legal station serving a real own-town Core
house. A station query alone does not allocate a station or establish its town/
catchment. Prove that assignment by paid station execution on a disposable fresh
proof game; actual FOOD acceptance requires separate native authoritative
designation proof. Do not call the registration manager directly for this proof. The native station/receiver
and route checks below are representative acceptance tests, not an all-seed food
profitability guarantee. Permanent protection against later player actions or
ordinary town growth is outside this initial-generation invariant.

## Smallest proposed terminal repair

1. In the generated-site path, calculate a pure geometry footprint containing
   head, all planned rails and the one outward joining tile. Validate bounds,
   world, flat/common height and clear/tree ground for the joining tile as well
   as the existing footprint. Reject an incompatible occupied tile; never rebuild
   neutral rails, use `AdoptForUAT`, change traversal or relax ownership checks.
2. Carry generation-local claims for those complete footprints across public
   pairs. Validate both candidate footprints and their mutual intersections before
   clearing/materializing either endpoint; reserve both together. Later endpoints
   must avoid earlier rails **and** clear joins. Keep existing direction/radius/
   coordinate candidate order and its finite world-extent bound, with no added RNG.
3. Extend arrival-zone planning claims to complete footprints, including public
   terminal joins already selected. A zone is advertised space, not a new built
   terminal or additional free track. Keep existing zone count/access rules.
4. Audit all built terminal joins after towns, industries, objects/trees and zones,
   then after the native initialization tile/GameScript loops, before
   `GenWorldInfo::proc` and game publication in `genworld.cpp`. Later
   generation can obstruct initially clear joins; initial pair checks alone are
   insufficient. Reject that start with a precise failure rather than clear an
   already generated town/industry or silently alter rights. A generation-local
   collision filter is permissible only if it reuses the same footprint and
   native placement rules; a broad town/industry placement redesign is a replan.
5. After company startup, prove an ordinary company can query/build the required
   joining track on a disposable **fresh proof game** and native track-follow can
   traverse into/out of the public throat. Compare preview/execute cost and actual
   owner/track bits. Free native generation of the public backbone stays as before;
   company connecting track is paid. Geometry checks alone are not command proof.

Use no new persistent reservation or load adapter. Do not widen the existing
freight planner's 16 station candidates, 30,000 predecessor-state cutoff or
16-tile bridge span. Compatible-neutral-rail planner improvements, new construction
methods and player-commissioned gate policy are separate work.

## Smallest proposed Core placement

1. At the ordinary integrated `GenerateTowns` entry, compute the same native global
   target, density scaling and pool clamp as today. Preserve custom count 1 as one
   actual town; native density/count is a requested target, not a promise that all
   remaining slots will succeed on unsuitable terrain. Preserve that existing
   behavior and do not make every town Core.
2. Reserve the first requested slot for a Core town. Enumerate initialized Core
   regions in stable WorldID order, and aligned candidate tiles in a deterministic
   seed-derived order using native synchronized RNG. Apply current native edge
   distance, clear/flat ground, surrounding room, grid, town spacing, names, size,
   city-frequency and phase checks. Coastal relocation must remain in an eligible
   Core region; never accept its neighboring Industrial/Frontier landing instead.
3. Proposed hard budget: at most 10,000 candidate probes across all Core regions,
   and at most 20 native town-creation attempts. One probe is one distinct aligned
   final center evaluated for placement: charge it before role/terrain/native
   validation, including every aligned coastal landing considered by relocation.
   Those landings share the same visited set and budget; do not hide repeated or
   uncounted center validations inside the coastal helper. Intermediate coast/
   water-distance scans retain native finite spiral bounds (40 then 10), not extra
   town attempts. One creation attempt means one `DoCreateTown` call. Freeze
   iteration, RNG consumption and counters in tests. Reuse
   native creation and authoritative `DeleteTown` cleanup for any candidate that
   fails the positive-population/own-Core-house predicate, including a populated
   town with no qualifying Core house. A failed candidate consumes an attempt,
   retains no counted slot/name/town infrastructure and continues only within
   the same budget; verify native cleanup rather than add a new deletion policy.
   Keep native RNG consumed by failed creation; do not roll it back. Require the
   qualifying house to be in the same Core world as the town center.
   No authored houses/population or repeatedly regenerated landscapes.
4. Success requires native positive population and an own-town house in Core,
   then decrements the remaining global target by one. Generate remaining slots
   through the existing ordinary path, preserving name uniqueness and the same
   city-frequency offset/slot accounting inside the same `GenerateTowns` call;
   do not call it again and recompute the offset for remaining slots. The Core
   town is not automatically a large city, megacity, HQ or research unlock. Check the Core town/house predicate
   again before starting the game.
5. No qualifying Core region, exhausted probes, exhausted native attempts or no
   surviving Core house are explicit failure reasons. Exhausting this finite
   search means **no valid town found within the budget**, not a mathematical
   proof that no legal site exists. Do not run the global fallback to accept only
   a non-Core town under the proposed strict contract.

This bounded approach may reject some terrain seeds with a legal but unfound
Core site. More extensive scanning, retries or a terrain fallback would need a
revised approved bound. Exact seeded town/industry/resource layouts may change
because placement and RNG consumption change; reproducibility means repeated
runs at the new revision agree, not that old revision save hashes/layouts recur.

## Decisions requiring owner approval before implementation

Approval to prepare this proposal is **not** approval of these behavior choices:

1. **Recommended: strict successful-start contract for enabled integrated games.**
   Approve T+C and fail generation clearly if the required join or Core town is
   not found within the fixed bounds. This changes which future seeds/settings
   successfully start and usually moves the sole custom count 1 town to Core.
   The alternative is an explicit permissive mode that starts without those
   prerequisites and reports the missing capability; that does not deliver a
   Core-receiver guarantee and requires a separate settings/UI contract.
2. **Approve future fresh-start validation as replacement prospective evidence.**
   New seed 11/101/2026 games exercise the revised generator and ordinary finances;
   they do not repair or retroactively validate retained operation saves. Existing
   saves remain loadable with their old limitations. An opted-in save repair would
   require a separately reviewed migration, ownership/custody policy and new UAT;
   it is excluded here.
3. **Approve the narrow new-game generation implementation scope and bounds.**
   Approve the budgets above, shared terminal geometry effect, native terrain/
   count/role rules and failure-before-play behavior. If no legal Core site or
   terminal footprint is found, the owner may choose another seed/settings later;
   agents may not silently increase town count, terraform a guaranteed enclave,
   change roles, relax ownership, increase search budgets or retry alternate seeds.

Once those choices are approved, promote this proposal into the **single**
`ACTIVE_EXECUTION_PLAN.md`, preserving the food plan as dated historical authority
and linking its blocked evidence. That later approval must explicitly authorize
implementation and fresh validation. Do not launch a second gameplay outcome
from a generic “continue” or treat this draft PR's merge as implicit approval.

## Implementation proof and negative cases

Reuse the native [generation tests](../src/tests/test_world_gen.cpp),
[terminal construction tests](../src/tests/test_portal_construction.cpp),
ordinary [A1 runner](../scripts/test_integrated_economy.py) and its shared
[Engine](../scripts/test_wp11_slice.py). Extend only the observations/proof path
needed for these invariants; never use `prepare-integrated` or scenario authoring.
Read [SAVEGAME_AUTHORING.md](SAVEGAME_AUTHORING.md) for future save custody.

- Deterministic terminal regression: recreate the observed distance 19 interference
  with intact distance 1–18 rails, require rejection/alternative in the existing
  candidate order. Cover all four directions, both map split axes, adjacent public
  pairs, zone/public-join collisions, bounds/void, unequal corner heights, water,
  existing foreign/neutral rail and a world with no legal footprint. Verify no
  map/registry/RNG mutation during failed pure terminal candidate validation and no playable
  partial start on generator failure. Include native foundation/rendering checks.
- Town regressions: custom count 1 produces exactly one qualifying Core town;
  larger custom targets reserve one slot and preserve the remaining native target
  semantics. Cover density target/clamp, multiple Core regions/nonzero WorldIDs,
  zero population, no own Core house, insufficient flat room, grid layouts,
  spacing/name/city-frequency, coastal relocation leaving Core, Phase4 rejection,
  no Core role, probe/creation exhaustion and ordinary Classic/editor paths.
  Failure reasons/counters and cleanup must be deterministic, with no fallback
  to a non-Core-only successful integrated start.
- Fresh representative proof: actual native seeds 11/101/2026, unchanged canonical
  1024²/seven-world temperate 1950 integrated_v1(OST01v5)/rail_v3/equipment_v1 profile,
  initial £100k cash/£100k loan, £300k ceiling and ordinary costs/tolls. Record all
  public terminal joins, the Core town/house identity and native full-cost station
  query. On disposable fresh-game copies, pay/build the station and confirm its
  assigned TownID and own-house catchment, no warehouse diversion and no automatic
  designation. Keep the pristine fresh checkpoint for A1 proof. Prove all required
  native joins and this potential house receiver; a query alone, merely allocated
  Town or valid internal rail is insufficient. No active FOOD consumer is claimed.
- Repeat each seed's fresh generation twice in separate processes. Compare semantic
  world/role/terminal/zone/town/house/RNG projections and counts, rather than raw
  save bytes that may include metadata. Save a new checkpoint, cold-load in a fresh
  process and compare those projections and native legal join/station queries.
  Also cold-reload the disposable paid station/join proof and verify its actual
  town/catchment/owner/rails and unchanged neutral terminal state.
- Repeat bounded ordinary A1 iron startup and paid/cold-continuation proof on the
  new layouts: generation/RNG changes can move industries and affect capital or
  service. Retain old evidence separately. For each seed, use existing paid native
  construction/loan/orders and public tolls; preserve costs, cargo/cash identities,
  fixed operating debt, paid output relief and at least three deliveries before
  and three after cold reload. Each generation/query phase is capped at 30 minutes
  wall time; each operating phase at 240 advances of 2048 native ticks and 30 minutes.
  Keep the planner caps above. No cheaper authored fixture or dropped failed seed.
- Existing-save regression: on copies, hash-verify A1 seed 11/101/2026 checkpoints,
  cold-load without advances/spending, compare original captured state and the
  recorded missing-town/blocked-join facts. Original saves and archives are never
  rewritten. Preserve existing MEGA/ECON/portal formats and role initialization
  on load; no retroactive contract enforcement.

A failure in ordinary route, station catchment, revised A1 affordability/profit,
geometry, native command authority, content activation, cleanup or proof budget
is a stop/replan with retained evidence. Do not widen search, tune economy or
change the acceptance criterion to make a selected seed pass. A full FOOD chain
quote/operation remains a **separate later approved preflight**, not a finish
criterion here; this generation proof cannot establish its affordability.

After final code: focused native tests, affected build, default unit suite,
isolated sequential CTest, both linters, script-mode and diff checks, independent
code/evidence review, exact-head CI and relevant SDL drawing smoke. No inherited
failure waiver; keep owner-history/PR62 commit-checker failures distinct from new
code regressions. Human graphical acceptance remains Pending until recorded.

## Finish line and practical human mission

Implementation would finish at one independently reviewed draft PR and retained
fresh/cold evidence for T+C+D+S plus the revised representative A1 regression.
No merge, existing-save migration, food operating proof, HQ/research pacing,
positive starter-running-cost policy, role evolution or A2 delivery follows.
Return exact PR/base/head, passed/failed/not-run checks and any blocked seed.

Later human mission, five-to-ten-minute timebox: launch the approved fresh build
with canonical settings and seed 11; pause, inspect the one Core town and its real
houses, confirm ordinary starting cash/loan and neutral terminal ownership. Pay
for the outward joining rail through normal controls, see a legal house-catching
station preview, and observe the separately proved A1 train use the public gate
with normal tolls. Save under a new filename, exit/reload and inspect the same
joins/town/service. Record pass/fail/partial, build/content/seed, first unexpected
message or inaccessible control, cash/debt and elapsed time. Use fast-forward for
train observation; the timebox is feedback, not a full paid food completion promise.

**Current delivery verification:** this PR changes prose only. Source inspection,
independent proposal review, link/anchor checks, both linters, script-mode and diff
checks govern this planning delivery. All gameplay tests and fresh-generation
claims above are proposed work, **not run or passed by this planning task**.

Independent read-only planning review passes in two scopes: product/evidence and
native placement/receiver feasibility. A separate terminal source audit confirms
the missing exterior-join checks and final-audit timing. Corrections made the
coastal probe budget, failed native cleanup/RNG, same-world house predicate and
paid station assignment explicit. No implementation or owner acceptance follows.

Planning checks pass: both repository linters, script-mode and diff check. All
introduced local links resolve. The full 85-link scan retains one unchanged
historical roadmap anchor failure (`next-priority-live-federation-with-two-clients`);
this is an inherited documentation issue, not claimed fixed or silently dropped.
Engine builds/tests, generation runs and save writes were not performed for prose.
Exact-head remote checks are reported in the draft PR handoff.
