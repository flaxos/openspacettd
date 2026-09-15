# OpenSpaceTTD recovery plan

**Canonical delivery plan · 15 September 2026 · audit baseline and WP-01–09 follow-ups**

OpenSpaceTTD has substantial working engine code and useful automated coverage.
It does **not yet have a demonstrated, dependable player vertical slice**. The
reported Blueprint placement crash has preserved evidence; source review also
finds cargo-conservation, command-authority, workflow and federation gaps that
current passing tests do not cover. Pause feature expansion until the gates below
pass. Preserve the rail-heavy, multi-world industrial vision and existing work.

This is the single recovery plan. [Project status](PROJECT_STATUS_AND_ROADMAP.md)
retains the sprint register; [defects](CRITICAL_BUG_REVIEW_2026-09-15.md) own stable
issue IDs; [feature evidence](FEATURE_UI_UAT_COVERAGE.md) separates implementation
from verification; [player checklist](../demo/ALL-FEATURES-UAT.md) and
[results](../demo/UAT-RESULTS.md) own acceptance. [DEVBOX_HANDOFF](../DEVBOX_HANDOFF.md)
is a short checkpoint, not another backlog. The audit began no implementation packages;
the subsequent user-authorized WP-01–09 execution is recorded below.

## Latest user acceptance report

After being asked to run UAT-00,04a–c and09, the user reports that OpenTTD no longer
crashes and UAT seems to pass. This is a positive user smoke result, not an
assistant-observed per-subcase record. It supersedes the earlier assistant GUI
access blocker as the latest gameplay evidence. Preserve unreported subcase
boundaries in `demo/UAT-RESULTS.md`. WP-04 has since been implemented below.

## WP-07–09 implementation follow-up — 16 September

**Implemented locally; visual acceptance remains pending.** Both binaries are
rebuilt and all **363 CTests pass**. [Evidence, fixture hashes and limitations](audit/2026-09-16/wp07-09/README.md).

- **WP-07:** native capture drag/naming/cancel and empty/foreign rejection work.
  Switching Capture/Place or re-clicking a tool preserves the new selection mode.
  All eight complete layouts are covered by placement/transform checks and 312
  native train runs: 39 movements × eight rotations/mirrors, signals intact,
  90-degree turns forbidden. Wye turns now follow directional lanes; corridor
  turnbacks fit within the existing footprint. RoRo/balloon platforms and both
  depot bays are exercised. Built-in layout revision 2 is separate from JSON
  format version 1; older imported geometry remains unchanged.
- **WP-08:** native founding creates houses/population/spatial state before world
  promotion. Pool/site/caller/funds rejection and real save/reload are covered.
  **Legacy decision:** preserve incomplete towns; no automatic repair or blanket
  regeneration. Retries reject without mutation. Two versioned generated empty-
  outpost fixtures cover both Expansion and already-promoted Frontier states,
  preserving identity, infrastructure, money and world metadata through reload.
- **WP-09:** Establish HQ, owned-station Build Hub, hub/cargo selection and reserve
  editing are available. Local owned stations on Developed/Frontier Worlds plus
  the Core HQ site satisfy presence; charter presence remains supported. Costs
  and errors are shown. Ordinary load/unload exchanges stock; Transfer/NoUnload
  retain native semantics. The fresh-company GUI/cargo test drives a depot-built
  train to the hub using native orders, deposits 60, reloads 20 onto its wagons
  and retains reserve 40, then persists all state across a process restart.
  Cargo is seeded only before departure; conservation holds on every tick.
  HQ/Hub tool switching and repeated selection are covered. Visual UAT remains pending.

**Next acceptance:** UAT-04, 08, 10–12 and save/reload using copies with the current
binary. `orca-ide` remains unavailable, so no new visual pass is claimed. The
previous user crash-smoke report predates these changes. These packages are not
closed for human acceptance, and WP-10 or later work has not started.

## WP-06 implementation follow-up

**Implemented locally; graphical UAT-04f/g acceptance pending (16 September).**
Blueprint admission now checks format/version1, integer types before narrowing,
enum and bit ranges, duplicate cells/signals, signal/rail compatibility, metadata
and resource bounds before transformation or placement. Invalid programmatic
models cannot enter rotation/mirroring. Existing valid v1 exports remain supported.
Limits:2 MiB JSON,16 nested containers,64x64/4096 cells,256-byte names and4096-byte
author/description fields; text must be valid UTF-8 without C0/DEL controls.

Library operations track each row's real backing path. Saves write an exclusive
sibling temporary file, check write/flush/close, then publish atomically. Rename
updates the name inside the same backing file. Name/filename collisions, symlinks,
invalid files and I/O failures no longer overwrite an unrelated file or report a
false success. A duplicate player name on import is rejected with an error.
Existing files are neither converted nor deleted by scanning; skipped files are
reported when the library opens. Export defaults outside the scanned directory.

Import/Export now use native path queries and real files; Export writes the
current rotated/mirrored selection. Cancellation and errors preserve selection
and stored data. Capture's save error is now reported, while drag/naming remains
WP-07. Built-ins remain read only; an imported built-in becomes an editable copy.

**Verification:**35 distinct CTests pass:30 Blueprint/CST cases (including three
actual window/query/OK callback tests) and five related Sprint28 cases. Existing
128 prefab rotation/mirror/payment variants and infrastructure persistence pass.
Six injected real file failures (open/write/fsync/close/rename/link) preserve old
bytes and memory, survive reopen, and allow retry. Both binaries are rebuilt;
351 CTests registered. The GUI cases use a null video driver in isolated processes;
no graphical screenshot or Windows execution is claimed. Evidence, failures,
separate parser/storage reviews and package-only patch:
[WP-06 evidence](audit/2026-09-15/wp06/README.md).

**Next: WP-07 functional capture and prefab routes.** Repair native drag/cancel/
naming, then prove and fix advertised port-to-port routes. No capture or routing
acceptance is inferred from WP-06 import/storage tests.

## WP-05 implementation follow-up

**Implemented locally; UAT-08/10 visual acceptance pending.** HQ Upgrade Tier now
posts a normal authoritative command with an explicit next tier. It validates
company ownership, the HQ and the exact next tier; repeated pending clicks cannot
skip tiers. Existing upgrades remain free. Foreign-company controls are blocked,
company zero is displayed correctly, and the HQ paint handler draws its widgets.

Opening or refreshing the Universe Directory now reads canonical local worlds
without registering or pruning model data. Colonize/Promote only post commands.
Successful execution synchronizes directory phase/name/biome to the canonical
world and refreshes the window; denial leaves world and directory unchanged.
Existing construction fees, development thresholds and progression are preserved.

The real persistence test also exposed a missing PLNT outpost location field.
The named save table now stores it; older rows retain INVALID_TILE without guessing
an old location. Old crash/M1 copies still load in the rebuilt game. Native town
creation and atomic founding remain WP-08; tests use an existing settlement.

**Verification:** seven new local command/actual-click/save tests and88 related
CTests pass (95 distinct;338 registered). A TCP loopback run verifies native
command serialization/sanitization and server/two independent client execution
queues, six denied requests, exactly three accepted changes, and equal state after
actual save/reload. This does not exercise a full multiplayer join/map handshake
or graphical rendering. Exact commands, failures, hashes and the package-only
patch: [WP-05 evidence](audit/2026-09-15/wp05/README.md).

WP-06 was subsequently authorized and implemented as recorded above.

## WP-04 implementation follow-up

**Implemented locally; player lifecycle acceptance pending.** The baseline real
command regression failed 39 assertions: invalid attachments could create a hub
and charge75,000. Query and execution now resolve the same eligible station before
payment. A hub requires a live company-owned rail station in its world, with a
real owned platform within four Manhattan tiles. Automatic selection uses nearest
platform then station ID, skipping bound stations. Station and anchor are unique.
Runtime cargo/production lookups enforce the binding, and arbitrary-tile deposits
no longer fall back to a world's stockpile.

Native last-platform removal and station destruction remove attachments; partial
removal retains one only while remaining rail stays in range. Native company
acquisition transfers hub ownership after tile/station ownership changes;
bankruptcy removes the binding. A real save/load round-trip verifies ownership,
reserve floors, counters and stock quantities. No save schema changes were made.

**Legacy policy:** after the engine restores map, stations and worlds, discard
invalid hub metadata and duplicate associations, retaining the lowest valid hub
ID. Never attach a discarded hub to another station or rewrite stock quantities.
The remaining planetary inventory can be reached from a valid replacement hub.
Maximum saved IDs wrap allocation safely and cannot overwrite another record.

Seven new cases cover command rejection/payment purity, auto/explicit parity,
nearest/tie selection, partial/full removal, deleted station ID reuse, native
acquisition/bankruptcy, legacy repair and actual persistence. Older synthetic hub
fixtures now create real owned platforms. The old exclusive-rights fixture now
requires unowned hubs to remain inactive while ordinary waiting cargo can load.
Exact commands, failures, final counts/hashes and package delta:
[WP-04 evidence](audit/2026-09-15/wp04/README.md).

**Boundary:** hub attachment transfer does not merge company-wide stockpile, HQ
or research ledgers. That pre-existing company lifecycle gap needs a separately
scoped inventory migration, including overflow policy; do not claim it fixed here.
UAT-10's existing-hub lifecycle retest remains human work. WP-09 still owns fresh
hub/reserve controls. WP-05 was subsequently repaired as recorded above.

## WP-03 implementation follow-up

**Implemented locally; human pickup retest pending.** A forced-full-pool test
reproduced stock100→40 and dispatch60 while both waiting and onboard stayed0.
The repaired loading block allocates a real packet before withdrawing inventory,
checks native exclusive transport rights first, and uses automatic packet cleanup
for zero withdrawal. Partial surplus reduces the packet before publication.

Four new tests cover native station ticks with full-pool refusal and later retry,
existing waiting cargo, zero/full capacity, empty/below/at/above reserve stock,
NoLoad and exclusive rights. They also exhaust the last free slot after withdrawal:
when gradual loading cannot split the packet,60 stays waiting and40 in stock,
survives actual save/reload, then loads without another withdrawal. Failed pickup
itself also survives reload and retries. Packet counts are checked after explicit
cargo cleanup. The scoped test occupancy bias uses the native allocation check
without constructing millions of objects and is removed before persistence.

The shared fixture resets engine data and vehicle hashes, initializes native
consist capacity/length, and explicitly frees owned cargo before pool teardown.
No production assertion or allocation check was weakened. Existing build reused;
commands, failing→passing evidence, final counts and hashes are in
[WP-03 evidence](audit/2026-09-15/wp03/README.md).

WP-02 deposit semantics and existing save data remain unchanged. UAT-10 now covers
normal pickup and existing reserve boundaries; allocation exhaustion remains a
developer fault-injection test. Graphical acceptance is separate and not claimed.
WP-04 binding/lifecycle was subsequently repaired above; WP-09 UI remains open.

## WP-02 implementation follow-up

**Implemented locally; human delivery retest pending.** The user authorized the
next critical repair after WP-01. Review confirmed the 60-unit industry+hub
delivery created 120 physical units, paid sale income and awarded development.
The regression now preserves 60 units across partial and complete unloading.

The reviewed destination rule follows the corporate spike §6.2: station processing
consumes matching recipe inputs first; remaining freight at the delivering
company's registered hub goes to stockpile instead of nearby industry/town.
Stored units receive no sale income, delivered-cargo count, subsidy, world,
Megacity or spaceport supply credit. Normal non-hub/non-freight consumption and
explicit Transfer/NoUnload orders retain their behavior. Freight follows the
loaded cargo specification's `is_freight` flag; no new order or GUI was invented.
Hub-only stations use the same acceptance rule at arrival and during unloading.
Invalid destination sentinels cannot qualify as storage destinations.

Focused regressions cover native-industry/town controls, non-freight and foreign
hub controls, order behavior, actual station-tick unloading, existing balances and
real save/reload. Existing station-production and broader Blueprint/CST,
stockpile/hub, fabrication, research, production and spaceport checks pass.
Commands, failures and final counts/hashes: [WP-02 evidence](audit/2026-09-15/wp02/README.md).
The reused production fixture now preserves mandatory void borders; assertion-
active CI confirmation of OST-TEST-001 remains pending.

Existing saves use this destination rule for future unloading; historical balances
are preserved, with no migration or guessed removal of duplicated inventory.
Human UAT-10/14 must check one delivery and save/reload on a copy. WP-03 loading
was subsequently repaired above; WP-04 binding/ownership was subsequently repaired above; WP-09 GUI remains open.
WP-01 graphical acceptance also remains pending.

## WP-01 implementation follow-up

**Implemented locally; graphical acceptance pending.** The later “Make it so”
instruction authorized the first package. The branch/base and audit documents are
preserved. `blueprint_cmd.cpp` now builds one deterministic preflight plan using
canonical depot/station quotes and shared rail/signal pricing, prospective track
geometry, and aggregate per-world material checks. Execution checks funds before
mutation and propagates child errors. Copied signal directions now consume their
BOM. Command assertions remain enabled and the command has no `NoTest` exemption.

The depot regression failed **500 quoted versus 600 executed** before the fix and
now passes at 600 in both phases. Fixture repairs initialize rail prices, valid
map edges, towns, station classes and language resources. **78 distinct selected
CTests passed:** 17 Blueprint/CST cases, one later persistence case, and 60 broader
construction/fabrication/portal/conduit/production cases. The CST case checks all
128 layout/rotation/mirror/payment combinations, complete footprints, stockpile
exhaustion and free exact overlap. The separate persistence test saves/reloads
mixed rail, signals, depot and station plus money/materials. Its no-video harness
restores the vanilla engine catalogue through normal initialization after loading;
it does not verify graphical NewGRF/engine loading. Logs, failed experiments,
configuration and hashes are in [WP-01 evidence](audit/2026-09-15/wp01/README.md).
The existing OST-TEST-001 production-fixture caveat still applies.

Supported placements cover clear land/ordinary trees for tracks and depots, exact
owned rail overlap, and one new default rectangular station block on clear land
(or complete matching existing station tiles). Irregular/multiple new/custom
station blocks, partial station overbuilding, trees under stations, rail/depot
conversion and destructive building/road/object clearing reject before mutation.
Sparse unused footprint cells stay untouched. These limits are intentional and
regression-tested where applicable; route topology and capture/import/export
remain separate packages.

**Next acceptance action:** use rebuilt `build/openttd` for UAT-00 and 04a–c on a
copy of the original crash save and a fresh clear pad, then save/reload. Record
Mainline placement, each transform, exact cash/material deltas and a late-tile
rejection. Native GUI automation remains unavailable, so no graphical pass or
exact original crash-tile reproduction is claimed. The later WP-02 authorization
and implementation are recorded above.

## A. Baseline, scope and evidence

### Repository and executable are distinct evidence

| Item | Observed baseline |
|---|---|
| Working directory / branch | `/home/flax/games/openspacettd`; `fix/portal-gate-lifecycle-crashes` |
| HEAD | `abbcd7e07737bcd83c3f830f539313a45bef5f01` |
| Initial local work | No tracked changes; one untracked `scripts/fix_codex_alias.sh`, read as a shell-alias helper and left untouched/unexecuted. It is not gameplay code. |
| Remotes | `openspace=https://github.com/flaxos/openspacettd.git`; `upstream=https://github.com/OpenTTD/OpenTTD.git` |
| Local main equivalent | No local `main`; `openspacettd/main=1d60095c1f`, ancestor of HEAD by 24 commits; `master=de306de893` is upstream, not this fork's product baseline. |
| Cached remote main | `openspace/main=776e236b31`, stale; symmetric difference 3 remote-only merge commits / 5 local-only commits; not identical tree. Cached feature branch matches HEAD. |
| Live remote main | GitHub API reports `345e258867079cb6ee8a0c21245761d38d9c43ee`; comparison says 14 commits ahead, 0 behind HEAD. Includes PR5 gameplay merge `7e60a6cb9c` and PR4 CI merge `345e258867`. No fetch/reset/switch/merge performed. |
| Live-main tree differences | Nine files: four CI workflows; English/US/AU strings; `portal/universe_authority.h`; `tests/test_sprint24_alien_biomes.cpp`. Remote CI fixes must be reviewed/reused in subsequent work, not recreated or assumed present locally. Blueprint command source is unchanged in that comparison. |
| Current executable | `OpenTTD 20260915-portal-gate-lifecycle-crashes-gabbcd7e077`; SHA256 `ace3d2ce48f7620723a953e9821fc56d3eddf5996e9a115ccc899391ad7d23aa` |
| Test executable | SHA256 `cb77a38bb3c704c009c7957b377147afcbef0743768f51a73b57528dfddb472a` |
| Configuration | Existing CMake/Ninja build, Release, GNU C++13, SDL2 available, dedicated-only OFF, asserts ON, dependency packaging OFF. No new build, configure or compilation. Dry-run schedules only version generation/object and relinks, not gameplay compilation. |
| Storage / processes | 3.7 GiB available on 115 GiB filesystem (97% used); build about 216–217 MiB; existing logs 132 KiB. No running engine/build/compiler observed before checks. One engine/check at a time. |
| Crash executable | Modified `a6b79add...`, older than current binary. Crash metadata cannot identify all its uncommitted inputs; do not call it exact committed a6b79add or remote main. |

Relevant commits: `30bee5fdce` corporate/fabrication/UAT; `387dd5d99d` transport;
`50df597962` research; `a14c6538ea` production; `a6b79add03` content;
`abbcd7e077` UAT/production/identity/callback fixes. The [PR/CI snapshot](audit/2026-09-15/github-pr-ci-snapshot.txt)
and [live comparison](audit/2026-09-15/live-main-compare.json) record exact results.
PRs [3](https://github.com/flaxos/openspacettd/pull/3),
[4](https://github.com/flaxos/openspacettd/pull/4) and
[5](https://github.com/flaxos/openspacettd/pull/5) are merged; this audit did not merge them.

### Checks actually executed

[checks.json](audit/2026-09-15/checks.json) records commands, configuration/ref,
results and log locations. Reuse these results while their inputs remain unchanged.

| Check | Result / limit |
|---|---|
| Version/help, binary hashes, Ninja dry-run/staleness | Pass/read-only; binary matches named ref, only generated version work pending. Exact reproducible build provenance is still a release gate. |
| CTest inventory | **306 cases**, individually registered Catch2 cases plus four upstream script regressions. CTest already includes unit coverage: do not rerun the whole binary after CTest. |
| Selected existing CTest regression | **67/67 pass**, serial, 30s per-case timeout, 1.37s actual. Blueprint/CST, local portal/conduit, HQ/stockpile/fabrication and production. This is not a new full 306-case pass. |
| Original crash save metadata | Version367; no active NewGRFs. Read-only query. |
| v1.1 metadata/hash | Version367; no active NewGRFs; SHA256 `20f9057fccf0262f7b17a5a1aef4bb6999354ac0ee31f3f9a940b64f4d884f0b`. |
| Copied crash and v1.1 load smokes | Each exits0 after null-video 1,000 GameLoop iterations. Copies/config in `/tmp/openspacettd-audit-20260915`; autosave and exit-save off. No placement command or graphical observation. |
| Supported native GUI availability | `orca-ide skills get computer-use --json` fails127: command not found; native CUA disabled. GUI reproduction/acceptance not performed. [computer-use skill](/home/flax/.agents/skills/computer-use/SKILL.md) says “If the selected executable cannot run, report its exact error and stop.” No alternate Orca executable was guessed; only graphical verification remains unavailable. |
| GitHub access | Default shell DNS failed. Approved read-only GH API access succeeded; no source/user save upload. Cached refs were not updated. |
| Documentation/link/whitespace checks | See final recorded audit logs; missing historical targets are listed, not invented. |

The latest observed HEAD CI build run [34956070026](https://github.com/flaxos/openspacettd/actions/runs/34956070026)
was still in progress, with failed Linux variants and Mac Debug, successful
Emscripten/Mac Release, Windows pending. HEAD file descriptions, unused strings,
Doxygen and commit checks failed; script-mode check passed. Doxygen failure log
contains new undocumented APIs. Live-main run [34956427967](https://github.com/flaxos/openspacettd/actions/runs/34956427967)
subsequently completed **failure**: Linux, Windows and Mac Debug test jobs failed;
Emscripten and Mac Release passed. Linux reports 303/306 passed; the three production
gameplay fixtures abort on `SetTileType`'s required void-edge assertion. Fixture
`test_sprint42_production_chains.cpp:454` calls MakeClear on every tile, including
mandatory outer void tiles. The [failure context](audit/2026-09-15/ci-main-linux-failure-context.log)
and [final snapshot](audit/2026-09-15/ci-main-final-snapshot.json) preserve evidence.
**Neither local passes nor remote main establish release readiness.**

Not executed: full suite, fresh compilation, sanitizers, malformed-input crash,
normal-dispatch new regression, graphical new-game/placement UAT, active-pack game,
full moving production route, sockets/federation/recovery, AI training/inference,
long-horizon simulation or release build. No saves/crash artifacts were overwritten.

### Assertion coverage qualification

A preprocess-only check using the exact existing production-test compiler flags
shows `NDEBUG=1`, `WITH_ASSERT=1`, but final `assert(expr)` expands to a no-op.
Thus Release/asserts-ON configuration does not guarantee assertions remain active
inside this test translation unit after includes. Catch2 CHECK/REQUIRE still run;
the 67 passes are real but do not establish this map invariant. See
[macro evidence](audit/2026-09-15/production-test-assert-macros.txt) and OST-TEST-001.
No assertion behavior for every other translation unit is inferred.

### Reading coverage and contradictions

All discovered project-authored planning/gameplay/architecture/sprint/UAT/spike/
content Markdown was read in full across bounded read-only reviews. The
[reading inventory](audit/2026-09-15/READING_INVENTORY.md) records file hashes/reviewers.
Upstream material was selective. Exact referenced local crash paths were inspected.
No exact Antigravity planning-file reference was found; an example author name is
not a path. No unseen private plan is presumed read.

| Existing claim | Evidence conflict | Resolution |
|---|---|---|
| AGENTS/design stop at Sprint33/258 tests | Source implements later work; 306 registered now | Correct current pointers, retain old totals as dated evidence. |
| “Sprints24–33 fully playable/verified” | Human results absent; placement crash exists | Historical implementation description only; use matrix/UAT gate. |
| Sprint25 capture/export/deterministic placement complete | Missing drag/I/O and query mismatch | OST-BP-001–005 stay open; no root-cause guess from button labels. |
| Sprint35 complete natural transfer/recovery | Runner dispatches manually; native entry rejects external gate; custody/order risks | Runtime exists; acceptance incomplete and concrete repair needed. |
| “Closed 12-cargo economy” | 13 conceptual entries; runtime aliases + unproven generated industries | Preserve desired material loops; explicitly bind/test 13 entries before full content claim. |
| Every research description is an unlock / replaces dates | Some effects only text; calendar availability remains | Decide rules per effect; completion of a tech node is not its behavioral test. |
| Connecting a conduit station guarantees output | Catchment/service/rating/allocation can yield zero | Rewrite UAT and expose actual delivery quantities. |
| v1.1 World2 is an unhubbed production fixture | Seeded World2 hub redirects all company output in that world | Separate hubbed and unhubbed fixtures; do not assume a marker proves setup. |
| Pack builder verifies NML output | Generator hardcodes its own bytes; --verify also writes | Treat actual generator as source; separate compile/verification and runtime checks. |

Existing numbering remains 1–42: Sprint36 is represented by UAT artifacts, Sprint38
art remains planned; no new sprint numbers assigned. Recovery packages below are
small work units, not a renamed sprint sequence.

## B. Feature and evidence matrix

The expanded [feature matrix](FEATURE_UI_UAT_COVERAGE.md) maps design document →
implementation → actual player entry → automated evidence → human evidence →
blocker → next action, including every gameplay track and the three AI objectives.
“Implemented”, “test exists”, “test passed at this ref”, “human accepted” and
“proposed” are deliberately independent states.

## C. Priorities and defect handling

The five highest-priority blocker groups are:

1. **Blueprint placement crash** (OST-BP-001), confirmed by saved stack/screenshot
   and user; manually estimated query versus real execution violates command contract.
2. **Hub cargo duplication/loss** (OST-HUB-001), source-confirmed industry+stockpile
   double destination and withdrawal before packet allocation.
3. **Authority/ownership bypasses** (OST-HUB-002, OST-AUTH-001), invalid station
   binding and GUI mutations outside replicated commands.
4. **Core progression/workflow gaps** (OST-COL-001, OST-UI-001, OST-CONT-001),
   incomplete native town creation, missing HQ/hub/reserve entry points and distinct
   material mapping. These block a player-established full supply loop.
5. **Federation natural entry and durable custody** (OST-FED-001/003), closed
   external gate plus unproven loss/retry/order/identity recovery. Keep isolated
   from the single-map delivery gate.

Imported-data crashes and dedicated external-gate demolition are additional P1
source risks. Conduit feedback, prefab topology and truthful I/O follow safety/
conservation work. Existing identity-aware CST gating, delayed callback relay and
station-facility cleanup fixes remain recognized; no contrary reproduction was
found and they are not reopened merely because other systems have defects.

Every defect record provides action/configuration, expected/actual, evidence,
confidence, dependency cases, smallest proposed fix and regression/human retest.
A missing control is a workflow/design blocker, not a falsely completed bug fix.

### Edge Conduit: complete local delivery trace

`economy.cpp` monthly processing → `edge_conduit.cpp` nominal production (50,
Frontier/Expansion100, Core25) → `StationFinder` near real station tiles and
`TileIsInCatchment` (`station_base.h:653`) → `MoveGoodsToStation`
(`station_cmd.cpp:4597`) filters exclusivity, zero rating, pickup service when
`order.selectgoods=true`, and facility suitability → rating/company allocation →
`UpdateStationWaiting`/CargoPacket allocation → actual returned delivered count.
A loading-enabled mineral vehicle must attempt loading to set `last_speed`
(`economy.cpp:1801`); No loading skips it. Single-station allocation scales by
`(rating+1)/256` with fractional carry; competition shares it. Pool exhaustion can
return zero. Connecting track does not establish any of these conditions.

The conduit ignores that returned count (`edge_conduit.cpp:240`) and adds nominal
production whenever a catchment station exists. Land Information shows “Total
minerals extracted”; it neither proves station waiting cargo nor reports retained
output (no local retry buffer exists). Decide whether unallocated extraction is
lost or buffered, and separately display produced/delivered quantities. UAT must
record counter, waiting, onboard and deliveries across an actual monthly event,
not equate these values or require 100 waiting from 100 nominal output.

## D. Dependency-ordered work packages

### Common delivery contract and resource limits

Each package follows **reproduce → failing regression → smallest fix → targeted
verification → relevant broader regression → human retest → closure evidence**.
Keep its own review; do not remove assertions, skip cases, weaken ownership or
conservation, add `NoTest` to conceal a mismatch, or silently change gameplay rules.

For every package: reuse one existing build; measure free space before compilation;
start with serial or at most `-j2` incremental compilation and serial targeted
CTest. No second full build. Stop before projected work leaves <2GiB free; request
user-directed cleanup of known artifacts instead of deleting saves/builds. A future
sanitizer configuration needs an explicit resource plan. Build/test evidence must
include exact commit and dirty diff, binary hash, configuration, content and fixture
hashes, command, result and log. New full-suite runs replace stale evidence only
after relevant code changes; never also rerun the identical Catch2 suite directly.

Default rollback is the individual code change before distribution, using a new
working save; no save-format change unless specified. If a save-format change
becomes necessary, split a migration package with compatibility tests. Each card
below names additional risks, human acceptance and evidence to close.

### First three bounded packages, in order

**WP-01 — Blueprint quote/execution correctness (OST-BP-001).**
Outcome: placing the recorded straight block and supported layouts succeeds or
rejects atomically. Scope: command plan/validation/cost/BOM and focused tests;
non-goals: catalogue redesign, UI capture/export, imported-schema expansion,
new art/content. Dependencies: existing baseline only. Likely files:
`src/blueprint/blueprint_cmd.cpp`, `src/tests/test_blueprint.cpp`,
`src/tests/test_cst_prefabs.cpp`, minimal canonical rail helpers if unavoidable.
First demonstrate one-tile depot mismatch, then normal dispatcher. Plan prospective
track/signals, full footprint, total resources, ownership/phase/terrain and
compatibility using engine semantics. Preflight every operation before mutation;
never keep successful earlier pieces after a rejected later piece. Tests: parity,
all eight complete stamps, overlap/slopes/signals, insufficient aggregate cash/BOM,
late obstruction and no state delta on rejection. Human: copied crash save Mainline
placement and fresh pad, rotated/mirrored/obstructed cases. Close with failing→passing
regression, dispatcher evidence, targeted+broader logs and GUI capture/crash absence.
Risk: correct sequential preflight is harder than summing command quotes; narrow
unsupported combinations explicitly if a canonical safe path cannot support them.
Rollback: code only, retain original saves; no new build directory.

**WP-02 — Exactly one destination for hub unloading (OST-HUB-001, deposit half).**
Outcome: delivered cargo cannot be both native-industry/town input and reusable
stockpile. Scope: `src/economy.cpp`, `src/portal/logistics_hub.*`, acceptance helpers
and focused integration tests. Non-goals: hub GUI, automatic production redesign,
new cargo packs. **Reviewed policy:** facility inputs first, then owned-hub
freight storage without consumer revenue/progression; otherwise native consumption.
Existing Transfer/NoUnload orders remain available; no new UI orders. Compare the 60-unit industry
+hub case with normal industry, station facility and town controls. Implement one
allocation decision before consumers/ledger/payment/progression callbacks. Include
hub-only freight acceptance in this decision; split UI order work if necessary.
Tests: per-cargo input=consumer+stockpile+vehicle+waiting, no duplicate payment or
progression, partial/full unload, save/reload. Human UAT-10/14: one real delivery,
stockpile withdrawal and consumer production before/after. Close with physical
balances and expected economic effect. Risk: changing where legitimate cargo goes;
record intended behavior and old-save semantics. Rollback: code/new fixture only;
existing duplicated balances cannot be silently rewritten.

**WP-03 — Lossless hub loading under allocation failure (OST-HUB-001, load half).**
Implemented locally in the follow-up above; retain these acceptance criteria and
the recorded human retest boundary.
Outcome: a full CargoPacket pool never destroys inventory. Scope: withdrawal/
allocation commit sequence at `economy.cpp:1816–1820` and focused tests. Non-goals:
deposit policy, hub binding, reserve UI. Depends on conserved test fixture from
WP-02; can be reviewed independently. Allocate/reserve storage first or guarantee
rollback before inventory decrement; preserve reserve-floor and exclusive-right
rules. Test full pool, zero capacity, reserve boundary, allocation recovery and
save/reload with exact per-cargo sums. Human: normal hub pickup and reserve limit;
pool exhaustion acceptance is deterministic fault-injection, not a manual task to
fill millions of packets. Close with failure-injection trace and unchanged inventory
on failure. Risk: abandoned allocated packet/leak; test failed downstream branches.
Rollback: local change; negligible fixture storage, one process.

### Subsequent packages (ordered by dependencies; independent reviews)

| Package / outcome | Scope, dependencies and approach | Verification / closure | Risks, rollback and resources |
|---|---|---|---|
| **WP-04 Hub station authority** | OST-HUB-002; `portal_cmd`, `logistics_hub`, station lifecycle. Validate supplied/auto station identically before Execute; real owner/world/range/rail facility/unique binding. No GUI expansion. Depends WP-02 fixture. | Invalid/foreign/deleted/distant/duplicate station and takeover/save tests; eventual player rejection in UAT10. Close with no mutation on all negatives. | Old invalid records need explicit safe rejection/migration policy. Single build, tiny maps; separate migration if needed. |
| **WP-05 Command-only GUI state** | OST-AUTH-001; separate reviews for HQ tier and Directory mutations; command traits, managers, GUI. Replicate valid tier changes; UI observes command result only. No progression redesign. | Query purity, affordability, denial preserves phase/tier; success once; two-client state equality. UAT08/10 captures. | Multiplayer desync high severity; use loopback two clients only after local tests; timeout/process cleanup; revert each subchange independently. |
| **WP-06 Safe parser and persistence** | OST-BP-002/004; separate parser and storage reviews. `blueprint.cpp/manager/gui`; reject invalid schema/enums/counts then atomic storage/collision/I/O reporting. Depends WP01 for valid placement. | Malformed JSON/version/track/signal; bounded data; failed write/rename preserves old library; actual export/import; UAT04. | Untrusted file handling; no mass conversion/deletion of existing library. Small temp directories only; retain old JSON. |
| **WP-07 Functional capture and prefab routes** | OST-BP-003/005, two reviews: native drag/cancel/naming; then topology fixes supported by port-route tests. No bridges/grade separation/new catalogue. Depends WP01/06. | Capture empty/foreign/cancel; eight full layouts, sequential transforms, YAPF routes; trains each promised movement. GUI UAT04. | Geometry repair can alter saved templates; version built-ins and retain imported layouts. Bounded maps/one engine. |
| **WP-08 Correct colonisation** | OST-COL-001; native `town_cmd` initialization and portal/world command preflight. Depends WP05. No new town economy model. | Pool/terrain/owner/funds failure atomicity; real houses/population/spatial lookup; directory matches world; save/reload; UAT08. | Existing incomplete outposts need explicit repair decision and versioned fixture; do not blanket regenerate towns. |
| **WP-09 Establishable HQ/hubs/reserves** | OST-UI-001, corporate GUI/commands/widgets/lang. Depends WP02–05. Native placement, owned-station selection, reserve editing; decide explicit transfer workflow and company presence. No decorative redesign. | Fresh player builds HQ/hub, moves cargo, edits reserve, sees cost/errors and persists; UAT10/11/12. | Workflow scope can grow; one control/path per review. Small maps and same content; rollback preserves backend data. |
| **WP-10 Honest conduit delivery** | OST-EC-001; edge manager/station allocation/UI. Define gross, allocated and buffered/lost semantics; do not force rating/service bypass. Independent after WP01. | Complete trace cases from §C; monthly serviced/unsupplied control, actual waiting+onboard; UAT05. | Buffering changes economy/persistence; if chosen split buffer schema from counter/UI fix. No unbounded output logs. |
| **WP-11 Content and economic vertical slice** | OST-CONT-001/DES-001; label mapping, pack generator/NML, research bindings, scripts/fixtures. Separate runtime pack validity, role binding, then each chain. Depends WP02–10 where route uses them. No full art pack. | Fresh active-content labels/IDs/refits/industry behavior; conserved moving ore→steel→useful output; dates/research actual effects; UAT13/14. | Save cargo remapping is high risk: never retrofit replacement GRFs; retained old fixture; each output hashed; no large content downloads. |
| **WP-F1 External gate classification/demolition** | OST-FED-001/PORT-001; distinguish endpoint kinds, native enter/status and direct removal. Independent local tests; release use remains gated on F2. | All directions, local/unlinked/stale/conduit negatives, direct external demolish; then physical natural entry capture. | Opening gate exposes unsafe transaction path; keep federation acceptance isolated until F2; 2 servers+authority bounded. |
| **WP-F2 Durable federation custody** | OST-FED-003 epic split into transaction-ID/reconciliation, identity/order restoration, and fault matrix. Existing transport retained. No federation expansion. | Exactly one physical owner/consist; per-cargo conservation through request loss/duplicates/obstruction/restarts/older saves; natural ordered return with IDs/flags. Remove permissive missing-evidence defaults, never assertions. | Distributed recovery cannot be a single small “sprint”. Dedicated fixtures and <=3 processes, bounded logs/ports/deadlines; rollback disables unsupported external workflow, preserves journal evidence. |
| **WP-Q Repeatable acceptance/CI** | Current workflows + this checklist + fixture builder; review live-main repairs first. Depends fixes per case. Add focused dispatcher/conservation tests, scenario preflight and failure artifacts. No wholesale CI replacement. | One clean smoke before human UAT; moving vertical slice + persistence + content; green required final-ref CI; result records per suite. | CI compute/storage controlled as §E; fixture changes hashed/versioned; old fixtures retained. |
| **WP-L Provenance/identity preparation** | Register in §G; package notices/full licences/source links, original-content decision and release manifests. Independent read/docs work may proceed alongside fixes. No deletion/relicensing/release now. | Inspect actual distributable file list and exact source correspondence; check credits/rights per asset. | Unknown rights block only affected public distribution; no blanket assurances. Small text artifacts; retain original notices. |
| **WP-A / WP-T AI and title milestones** | Three research tracks §F; title §H. AI execution starts after core gates; small branding does not depend on AI/training. | Explicit hypothesis/evaluation and title budgets below. | No paid service/model download without separate authorization; locally reproducible saves and fallback. |

## E. Acceptance and release control

### Minimum demonstrated vertical slice

Use a versioned **single-map** fixture: one company, Phase3 iron extraction → train
through local portal → Phase2 station furnace → steel train through second portal
→ Phase1 useful stockpile/fabrication or other explicitly accepted final consumer.
Player constructs/operates the essential route using real controls and available
engines, with normal orders and no manual cargo injections after fixture startup.
Observe extraction, pickup, transit, facility input, monthly conversion, waiting/
stockpile output, final acceptance and cash/material/progression consequence.

Record initial+produced cargo = final+consumed+legitimately-disposed cargo, with
per-cargo transformations justified by recipe. Cash = initial + income − actual
construction/running/research costs. Materials deducted once, ownership remains
correct, consist counts stay stable. Save/reload mid-route and resume three
successful deliveries. Failure/obstruction must leave recoverable state. The
vanilla-alias slice is labelled as such; it does not accept distinct Commonwealth
content or all food/electronics/data loops in the design.

### Test layers and fixture separation

- **Unit:** schema/transforms/recipes/prerequisites. Cannot prove GUI or routes.
- **Command integration:** dispatcher Test/Execute parity, atomicity, ownership,
  aggregate resources; real station unload/conversion/load and persistence.
- **Full engine headless:** generated/versioned scenario preflight and bounded
  train/delivery counters, save/reload. A passive load smoke is a smaller result.
- **GUI automation:** native supported adapter with screenshots/action trace;
  currently unavailable here. Do not fabricate graphical passes from data tests.
- **Human:** short UAT-00 then literal cases, real visible errors and supply changes.
- **Network:** lockstep client equality; federation adds independent engines,
  external authority, custody/order/restart evidence. Manual dispatch is separate.

Suites are migrated-save regression, fresh active-content, single-map gameplay,
real multiprocess federation, and UI/visual. Their exact fixture/company/world/
content/resource preconditions and status fields are in the existing checklist.
Use fixture IDs and SHA256; a location marker is not fixture proof. A required
fixture/control not yet delivered is **Blocked** with a creation package, not
instructions to click an imaginary control.

### CI improvements grounded in existing workflows

CI already covers Emscripten, Linux Clang/GCC/SDL/dedicated, macOS debug/release,
Windows; nightly adds further variants; Doxygen, strings, commit/file checks exist.
Local `ci-build.yml` only pushes `master`, while PRs target main. Live-main repair
changes this and testing behavior: review the nine-file comparison before proposing
a duplicate patch. Linux currently runs CTest with timeout120 and unbounded nproc
parallelism; add a measured memory/disk ceiling, job timeout, cancel superseded
runs, and explicit resource groups for engine/socket fixtures.

Run fast command/schema/conservation checks on each PR; one representative engine
scenario after relevant changes; expensive fault/long-run matrices on scheduled or
manual jobs. Bound each process and total wall time, reserve ports, kill children
on exit, cap logs and artifact retention (e.g. 7 days, <=100MiB/case, 1GiB/job;
proposed limits to measure). On failure retain command trace, seed, binary/ref,
CMake config, fixture/content hashes, small normalized state summary and consented
synthetic save/screenshot. Never upload private user crash saves automatically.
Before relying on production regression evidence, fix only the invalid fixture
border initialization (use native void-edge rules) and establish assertion coverage
in the test configuration. Reuse the existing build or CI; no second full build.
Preserve the engine invariant and all production assertions. Compare Debug/assert-
active and Release results after the fixture fix; do not green CI by disabling
assertions. Treat this as the narrow OST-TEST-001 verification prerequisite within
WP-Q, independent of gameplay changes.
Inspect actual required jobs at final ref. Doxygen failure needs documentation of
new API parameters; don't disable the checker. No CI changes executed in this run.

### Gates

**Resume feature development only when:** Blueprint dispatcher and malformed-input
crash gates pass; no open demonstrated corruption/conservation/authority blocker
on the supported slice; UAT-00 and conserved moving vertical slice pass graphically
and after reload; fixtures/preconditions reproducible; targeted and required
broader checks green on recorded ref; remaining gaps have honest scope labels.
Federation may remain experimental and isolated if the accepted product slice is
single-map. No new sprint merely because a PR exists.

**Publish a playable demo only when:** above gate plus supported fresh/migrated-save
smoke, complete scoped player checklist, clear controls/error recovery, exact
build+save+content manifest, clean target-platform package smoke, public-source/
notice/provenance obligations satisfied and no unresolved rights for shipped
content. Clearly label excluded federation/unfinished art/aliased economy. Obtain
user's explicit release authorization at that time; this audit publishes nothing.

## F. Practical AI research plan and entry gate

Preserve the [existing AI spike](LLM_WORLD_BUILDING_AND_AI_UAT_SPIKE_2026-09-15.md),
including its three ambitions. Its appended API/example audit marks each example
existing, partial, proposed or unsupported; examples are not delivered tools.

| Track | First engineering baseline | Model role and evaluation |
|---|---|---|
| **A: world/logistics scenario generation** | Versioned high-level manifest → validator → deterministic compiler → acknowledged engine commands → ordinary save → reload/delivery check | Model optionally proposes a manifest outside simulation. Measure valid construction, delivery, conservation, reproducibility and editing effort. |
| **B: in-game opponents** | Deterministic heuristic operates one accepted corridor with cash/material reserves, bounded cycle work, persisted AI state and real Script API permissions | Offline model assistance or later constrained strategy choices; compare success, sustainable supply and deadlocks against same seeded baseline. No external inference inside lockstep command execution. |
| **C: long-horizon UAT/telemetry/balance** | Bounded engine runner + actual event counters + numerical assertions first; separate local/federation observability | Model summarizes evidence with run/event IDs and clearly labelled causal hypotheses. Validate proposed interventions experimentally; no automatic gameplay tuning. |

### First spike: one functioning route, no training

**Hypothesis:** a hand-authored validated manifest and deterministic executor can
create, save, reload and operate a small two-world logistics route without manual
construction. The deterministic baseline must work before testing LLM output.

**Entry:** WP01/parser/relevant prefab routing fixed; native portal player route
accepted; conservation/ownership paths used by the scenario fixed; no unresolved
fixture/content mismatch. A one-company native mine→accepting industry route can
avoid unneeded HQ/research/federation dependencies. This smaller research fixture
does not replace the three-world processing vertical slice required for the game.

**Scope/input:** 256x256, two actual engine-reported world regions, one local portal
pair, two stations, a depot, one available conventional engine and short compatible
consist (<=5 tiles), one verified base cargo, known receiving industry. Versioned
schema/build/content/config/seed/company; stable IDs for cargo, engine, recipe,
stations and connectors; explicit coordinates/directions/rotations, cash/inventory,
orders, expected deliveries and deadline. Use seed-controlled source generation
or validated native construction, never edit binary save chunks.

**Validation:** reject unknown fields/IDs; check real map/world/void bounds,
terrain, complete terminal clearance, non-overlap, owner/phase/rail compatibility,
connector reachability, actual consist/platform lengths, cargo acceptance/pickup
service, date/research availability and aggregate cash/BOM. Reserve floors are not
initial inventory. No fallback to guessed vehicle/cargo or silently skipped command.
Scenario-only injection/unlock APIs, if needed later, are separate privileged setup
capabilities, unavailable to ordinary opponents.

**Prerequisite APIs:** existing ordinary rail/station/vehicle/order Script APIs;
minimal command-backed portal and Blueprint bindings plus read-only world metadata
where needed. No `GSSaveLoad`: use acknowledged console `save` through the existing
local harness pattern and normal serializer. Build a real schema/compiler/telemetry
exporter; none of the example LLM files currently exists. JSON admin events/RCON
are an existing bridge, not unrestricted access to all C++ methods.

**Outputs/diagnostics:** canonical manifest and compiler version, ordered operations
with IDs/arguments/result/cost, seed and content manifest, initial/final checkpoint,
observed vehicle/cargo/cash/material/delivery events, bounded JSON summary and
failure reason. Stop on command error, retain last safe checkpoint and replay from
start; do not continue a partly executed invalid plan. Publish only synthetic
fixtures with cleared provenance, never private user saves by default.

**Evaluation:** 10 development +10 sealed held-out valid scenarios, separated by
topology/template as well as seeds; additional six invalid cases (void overlap,
blocked exit, missing cargo/engine, short platform, foreign owner, insufficient
resources). Every valid baseline case must build and complete >=3 real deliveries
within a calibrated deadline, without crash/deadlock/duplication/unexplained loss.
Every invalid manifest must fail validation or atomic engine command without
unauthorized partial state. Repeat selected cases twice and compare normalized
event/counter results; compressed save bytes need not be identical. Mid-route
save/reload must preserve ownership/orders/inventories and continue deliveries.
Graphical captures show actual trains moving; appearance cannot pass the test.

**Proposed caps, not measurements:** one engine; 300s wall-time or 180 economy days
per case, whichever first; RSS target<=2GiB; <=1GiB retained artifacts; initially
run just one baseline then scale. Twenty valid runs at cap use 1.67 process-hours,
two passes 3.34; budget six negatives separately (<=0.5h worst case). Assuming
20MiB/save +10MiB/log, twenty retained cases use 600MiB; retain replay summaries,
not unlimited duplicate saves. Stop before violating current 3.7GiB free-space
constraint; no new complete build or model download.

**Optional model pilot:** 20 prompts, <=3 attempts each, assumed 8k input+2k output
per attempt =480k input/120k output tokens. Hypothetical rates $5/$20 per million
would cost $4.80; set a separately approved $10 cap and verify actual provider
pricing then. These are planning assumptions, not a price quotation or audit spend.
Training compute/storage/cost is intentionally unallocated until justified.

### Model-method decision gates

1. Deterministic scripts/templates and heuristics are the reproducible baseline.
2. Prompt an existing model with schema + validated tools; measure correctness,
   retries, reviewer time and cost against baseline.
3. Add retrieval of versioned project rules, real prefab metadata and successful
   licensed examples when failures reflect missing context.
4. Consider fine-tuning if held-out failures persist after prompting/retrieval,
   enough rights-cleared demonstrations exist, and measured quality/cost improvement
   justifies training. Split by topology/template to avoid leakage; reserve unseen
   evaluation cases before collecting/training.
5. Consider reinforcement learning only for a bounded measurable task where a
   stable simulator/reward correlates with conserved deliveries and sustainable
   operation, and cheaper methods plateau. Profit via duplication is a failed run.

Record dataset source/author/licence/consent, model/provider/version/terms, prompts,
outputs, tool versions, corrections and validation IDs. Model weights, datasets
and generated deliverables need separate provenance. Do not ingest novel text,
book art, private saves or credentials without rights and authorization.

**Stop/go:** stop on any conservation/desync/save failure, invalid command escape,
repeated unrecoverable construction or cap breach. Expand scale only after baseline
and negatives pass, held-out correctness is maintained and model assistance shows
measured benefit. Do not launch all three tracks as one “train an AI” project.

## G. Licensing, provenance and fork identity

This is a compliance/provenance review, not legal sign-off. Scope is actual source,
headers/packages/scripts and reviewed distribution configuration; no release bundle
was built or cleared in this audit. A GitHub fork relation, retained Git history,
licence compliance and independent public identity are four different matters.
GitHub's [repository metadata](audit/2026-09-15/github-repository.json) reports
`fork:false`, default branch `main`, no parent/source and licence classifier
`NOASSERTION`. It is not currently represented as a GitHub network fork. This
metadata does not erase retained OpenTTD history or override actual GPL headers/
COPYING; NOASSERTION is not a finding that the repository has no licence.

OpenTTD-derived code is GPL **version 2** according to the repository's full
`COPYING.md` and headers; do not silently relabel it “or later”. Distribution must
preserve applicable notices, mark modifications/dates, include the licence and
provide corresponding source, including required build/install scripts, by an
applicable GPL route. Tie every released binary to its exact source/ref and
patches. A repository link to a different revision is insufficient evidence of
corresponding source. [Official GPL text](https://raw.githubusercontent.com/OpenTTD/OpenTTD/master/COPYING.md),
[OpenTTD project description](https://www.openttd.org/about).

### Concise provenance register

| Material / source | Licence and attribution evidence | Modification / distribution status and action |
|---|---|---|
| Derived engine / upstream history | `COPYING.md`, original headers, CREDITS; GPLv2 | OpenSpace modifications present. Retain upstream notices and identify modified files/dates; exact-source release manifest required. |
| Custom C++ / NML / GRF generator | GPLv2 headers in `src/portal`, `src/blueprint`, NML and `scripts/build_commonwealth_grf.py` | Actual generator does not consume NML/nmlc: hardcoded bytes are build source. Document both sources accurately and verify which behavior ships. |
| Two custom pack bundles | `pkg/commonwealth_*/license.txt` only 14-line GPL preamble fragments | **Standalone distribution blocker:** include full terms/credits/source instructions. Root COPYING exists for repository, but fragments alone are not complete pack licences. Do not relicense/delete content. |
| Pack artifacts/manifest | 605-byte industry +759-byte rail GRF, IDs/checksums in `pkg/commonwealth_manifest.json` | Hashes identify bytes, not valid industries/art or rights. Add source/tool versions and redistribution metadata. `--verify` currently writes outputs; not run as a read-only check. |
| Squirrel and MD5 | Zlib notices in `squirrel/COPYRIGHT` and MD5 source | Preserve notices and mark altered sources; no origin misrepresentation. |
| JSON / OpenGL / Social Integration API | MIT files/headers | Include required copyright/permission text for shipped copies; inspect actual packaging. |
| fmt | MIT plus compiled-object exception in `LICENSE.rst` | Preserve source licence; account for exception rather than overstate binary notice requirement. |
| Catch2 | Boost1.0, object-code exception | Test dependency; distinguish whether a test executable ships. |
| ICU scriptrun | Unicode licence at `src/3rdparty/icu/LICENSE` | Preserve notice in distributed copies or associated documentation as terms require. |
| Monocypher4.0.2 | Header offers BSD-2-Clause **OR** CC0-1.0 | Record chosen distribution basis. README points to missing LICENSE.md; headers retain terms. Broken link housekeeping. |
| LLVM CMake helper | Apache2 with LLVM exceptions | Build-source role; retain actual licence/exception rather than assume runtime dependency. |
| System/runtime libraries | Cache references SDL2, curl, compression/font/audio libraries; packaging dependencies OFF | Unresolved per-platform shipped-linkage inventory. Inspect actual binaries/package contents and collect licences/notices before release. Cache alone is not an SBOM. |
| OpenGFX/OpenGFX2 graphics | GPLv2; upstream sources/credits | Installed audit base OpenGFX7.1 differs from CI0.6 and release OpenGFX2 Classic0.8. Pin exact redistributable archive/hash/source. [Graphics](https://raw.githubusercontent.com/OpenTTD/OpenGFX/master/README.md), [OpenGFX2](https://github.com/OpenTTD/OpenGFX2). |
| OpenSFX sounds | Collection CC BY-SA3.0; per-sound credits; other scripts/text GPLv2-or-later OR CDDL1.1 | Preserve exact package/per-asset terms, attribution and modifications. Release workflow references1.0.3. [Licence/credits](https://github.com/OpenTTD/OpenSFX/blob/master/README.md). |
| OpenMSX music | GPLv2 plus composer credits | Workflow references0.4.2; include exact terms/credits/source. [OpenMSX README](https://raw.githubusercontent.com/OpenTTD/OpenMSX/master/README.md). |
| Original TTD data descriptors | Compatibility descriptors in tree do not grant original artwork/music rights | Do not bundle original proprietary game data without separate rights. Distinguish descriptors from payload. [OpenTTD content guidance](https://www.openttd.org/content-creators). |
| Fonts | Four upstream OpenTTD TTFs; CREDITS names Richard Wheeler; root licensing baseline | Verify exact font metadata/source/exception and modified font naming before custom branding distribution. System-font use does not authorize bundling it. |
| Logos and title save | Upstream `media/openttd.svg`, `media/baseset/opntitle.dat` | No new OpenSpace logo/art source package found. Retain attribution and provenance for modifications. |
| Commonwealth-inspired names/text/art | Specific CST/Sheldon/Ozzie/world names in docs/strings/catalogue; no permission evidence | Rights/branding unresolved. Broad multi-world rail logistics can use original names/lore/art; permission or scoped replacement decision before affected public distribution. |
| AI datasets/models/outputs | Proposed only; no model/dataset deliverable discovered | No blanket rights clearance; per-source/model/provider terms and human authorship/provenance record required (§F). |
| Demo saves and scripts | Versioned UAT saves and GS/refresh sources | Record build/save version, content dependencies, authors and permitted assets. Engine licence alone does not establish rights to every saved asset or narrative. |

`cmake/InstallAndPackage.cmake` installs baseset/game and top-level notices; it does
not automatically install `pkg` packs or every third-party licence file. Existing
release workflows target upstream channels and fetch base packs. Do not blindly
reuse their publication destinations/secrets for OpenSpace. A later staging-package
inspection must prove contents, notice payload and source correspondence.

Non-commercial intent does not replace licence conditions or permission. Broad
ideas differ from protected literary/artistic expression; names can also raise
branding/trademark questions. The cited US guidance is not clearance across
jurisdictions. Record uncertainty, and obtain relevant advice/permission where
needed before distributing affected material. [Ideas/expression guidance](https://www.copyright.gov/help/faq/faq-protect.html),
[Permission/derivative works guidance](https://www.copyright.gov/help/faq/faq-fairuse.html).

**Immediate public-release gates:** full pack licence payloads; exact corresponding
source and third-party notices; valid redistributable content manifest; decision
on fiction-derived specific material/unknown assets; tested actual package.
**Housekeeping:** Monocypher link, machine-local links, upstream download/reporting
links, accurate modification notices and one explicit meaning for CST (documents
currently use several expansions). No material removed or relicensed in this run.

**Practical identity:** use “OpenSpaceTTD — an independent transport simulation
based on OpenTTD” in project introduction/About, retain upstream copyright/credits
and licence access, direct OpenSpace bug reports/releases to this project. Keep
OpenTTD names in attribution, APIs, compatibility and history. Develop original
world names, lore, architecture silhouettes and commissioned/rights-cleared art
where specific fictional material remains unresolved. This does not require
rewriting engine symbols or replacing every upstream name.

## H. Opening screen and megacity showcase

### Existing title mechanism

`src/openttd.cpp:322–347` `LoadIntroGame` sets Menu mode, resets GRF/window state,
loads Baseset **opntitle.dat**, falls back to an empty64x64 map, chooses company/
spectator, resets zoom and clears pause. `media/baseset/CMakeLists.txt` packages
that save. `intro_gui.cpp:333` uses `STR_INTRO_CAPTION` (currently OpenTTD in English);
About/version strings retain upstream information. Vehicle/landscape/timer loops
can run behind the menu. Custom manager/monthly/GameScript behavior in Menu mode
must be measured, not assumed identical to ordinary play.

**T1 — small identity change:** after crash stabilization, change caption and
independent-project About/version descriptor, using a rights-cleared original logo
if available; retain upstream notices and the existing stable title save. No AI
or model-training dependency. Verify startup, UI scales/translations, keyboard/menu
focus, missing-title fallback and start/load transitions. Scope excludes city
construction and general menu redesign. Roll back strings/assets individually.

**T2 — validated functioning showcase:** after §F baseline, build a versioned
256x256/512x512 local scenario with populated districts, freight yards, moving
trains and measured deliveries. Save a prepared checkpoint; menu must not wait for
city growth. Optional LLM world planning is offline and validated. No remote
federation, live API or training dependency at startup.

Manifest includes build/save/config/seed/compiler/content hashes, provenance,
train/station/route IDs, capacity/supply targets and deliberately staged concessions
(e.g. subsidies or simplified demand). It is a showcase, not full economic-game
acceptance. Check actual cargo/ownership/cash balances, not only attractive motion.

Proposed budgets to benchmark on a recorded reference machine: extra load<=2s,
RSS delta<=256MiB, background tick p95<=5ms, menu input<=100ms. Initial30-minute idle
soak, then longer acceptance soak; bound population, fleet, queues and generated
files. Evaluate minimized/throttled behavior, readable contrast behind controls,
UI scaling, reduced-motion/static-background option and focus/keyboard accessibility.

Use immutable checkpoint reload/loop (for example10 minutes or scene failure)
with complete cleanup of custom managers, timers, queues and script state. A
reload must not accumulate errors or write autosaves. Validate menu→new/load game
and return to menu without state leakage. Missing/incompatible showcase falls back
to the known-good title, then existing empty-map fallback. Player saves, NewGRF
presets and ordinary game state stay isolated. Close with startup/resource/idle
traces and graphical evidence. No showcase implementation in this audit.

## I. Deferred scope, decisions and execution handoff

Deferred: broad new feature sprints; original six-biome/CST art production; all
13-cargo pipelines beyond the accepted small route; public federation deployment;
model training/RL or model downloads; mega-map scale; dynamic climate/combat/
spacecraft/complex power grid. Existing federation code remains available for
bounded repair/testing, with its acceptance status qualified.

Unresolved decisions: explicit stockpile transfer semantics and payments; whether
conduit unallocated output is lost or buffered; exact calendar/research/company-
presence rules; native versus station-based extraction/processing content;
original-content naming/licensing route; final supported demo scope/platforms.
Resolve each using the scoped work package and design evidence. Non-critical
unknowns did not stop this audit.

### Ready-to-paste first implementation prompt

Historical audit handoff: WP-01 has now been implemented as recorded above. Use
the current DEVBOX_HANDOFF for the remaining human retest instead of repeating it.

```text
Implement WP-01 from docs/RECOVERY_PLAN_2026-09-15.md: repair OST-BP-001 only.
Read AGENTS.md, /home/flax/RTK.md and DEVBOX_HANDOFF.md first. Preserve all user
work, audit docs, saves and crash evidence; do not reset/stash/switch a dirty tree,
push/merge/release or begin other packages. Audit baseline is abbcd7e077; live
main was 345e258867 (14 commits ahead, nine-file CI/string/header/test difference).
Inspect current refs/status and review those differences before choosing a base;
do not silently substitute remote main for the user's binary/source.

Recorded user-confirmed action: place CST Mainline Double Straight (8x2).
Crash /home/flax/.local/share/openttd/crash20260915082450.json.log (+sav/png)
asserts src/command.cpp:365 query/execute cost or success parity, stack
BlueprintLibraryWindow::OnPlaceObject. Crash build is modified a6b79add; exact
terrain/child-command trigger remains unisolated. Do not assume trees cause it.

First add a failing bounded regression: one-tile depot blueprint on flat clear
terrain with fabrication off, using existing test fixture prices. Query currently
omits rail build cost; demonstrate exact query/execute disagreement (source predicts
500 vs600 plus equal clearing). Then exercise the normal command dispatcher.
Existing tests only assert positive independent costs and often first-tile output.

Primary files: src/blueprint/blueprint_cmd.cpp, src/tests/test_blueprint.cpp,
src/tests/test_cst_prefabs.cpp; use minimal shared canonical rail/station/signal
helpers only when needed. Implement one deterministic, fully validated placement
plan with canonical terrain/ownership/rail/phase/station/signal rules and exact
aggregate money/BOM cost before mutation. Model prospective track state for
signals/overlap; isolated dry-runs against an unchanged map are insufficient.
Return child failures; no partial layout or resource delta on rejected placement.
Preserve TileIndex/map design, lockstep, existing command assertions and cargo/
material conservation. Do not add NoTest, suppress assertions or skip tests.

Cover all eight full supported prefab footprints, slope/clearing, existing track,
signal conversion, rail mismatch/phase/foreign owner, late obstruction,
insufficient aggregate cash/materials and unchanged state on failure. Keep capture,
export/storage, parser-hardening, topology redesign and all other gameplay separate.
If a layout cannot be safely preflighted, identify an explicit narrow rejection
with a regression instead of pretending the feature works.

Reuse build only, check disk/processes first (audit had3.7GiB free). No second full
build or parallel full builds. Run incremental with bounded jobs, targeted CTest,
then relevant broader regression once; CTest already includes Catch2, so do not
repeat the same full suite directly. Log command/ref/diff/config/result/artifact.
Current audit evidence:67/67 selected tests pass; both copied saves load under
abbcd7e077; these do not test placement.

Use copies of original crash save and a fresh small flat fixture for graphical
Mainline placement/rotate/mirror/obstructed rejection. Supported native GUI was
unavailable (orca-ide absent); if still unavailable, state exactly what graphical
retest remains and never claim it passed. Capture first failing command/result,
then before/after map, money and materials. No overwrite of original saves.
Update existing defect/UAT evidence, not historical claims. Finish with minimal
diff, cause/confidence, tests, limitations and next human retest; stop before WP-02.
```

**Recommended next action:** run UAT-00 and 04a–c with the rebuilt executable on
copied fixtures. The command repair and automated checks are complete; graphical
Mainline placement/transform/rejection and reload evidence is still required.
Do not spend another broad human UAT session before the short smoke gate passes.
