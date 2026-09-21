# Recovery defect register — 2026-09-15

Current source: `abbcd7e07737bcd83c3f830f539313a45bef5f01`. This register feeds the
[master recovery plan](RECOVERY_PLAN_2026-09-15.md). **No fixes were implemented in
this audit.** The later WP-01 implementation is recorded separately below. P1 means crash, conservation/authority risk or blocked core loop;
P2 means a workflow/design/content gap. “Source-confirmed” means a concrete code
path, not a new runtime reproduction. Historical fixes below retain their own
verification boundaries.

## OST-BP-001 — P1: Blueprint placement aborts on query/execute disagreement

- **State:** repaired in the local WP-01 diff; automated command acceptance passes.
  Original crash evidence remains preserved; graphical closure is pending.
- **Configuration:** crash at 2026-09-15 08:24:50 UTC, modified build
  `a6b79add0362b847ce01457c7875ae21a5523157`, built 07:13:24; SDL/X11,
  OpenGFX, US English, UAT GameScript v9, no NewGRFs, save version 367,
  6 December 1950, tick 26394. That modified build is not assumed identical to
  committed a6b79add or current abbcd7e077.
- **Action:** open rail toolbar → Blueprint Library → select **CST Mainline
  Double Straight (8x2)** → Place → click terrain. Screenshot shows Placing over
  grass/trees; stack contains `BlueprintLibraryWindow::OnPlaceObject`.
  User explicitly confirmed this is the reported crash. Exact target tile and
  individual child-command failure/cost contribution are not known.
- **Expected/actual:** one fully quoted layout or a clean rejection; actual abort
  in `src/command.cpp:365` on test/execute cost or success disagreement.
- **Original evidence:** `/home/flax/.local/share/openttd/crash20260915082450`
  plus `.json.log`, `.sav`, `.png`, inspected read-only. SHA-256 respectively:
  `4557e03cbd24684de4551425130665baa45cbc5391ee3dcae30a39734f714de4`,
  `503cc9797197b5d2699c52a6cbc4808be6594b4d7a6f1b6a95b6ada113e88b03`,
  `2ae0312f1825234e660f397339e293a0e31092b6722d08c9b125a321a4d20aa7`.
  Evidence stays local; no crash bundle uploaded or copied into release content.
- **Audit-base trace/confidence:** `src/blueprint/blueprint_cmd.cpp:55–106` hand-prices
  construction; `:109–145` invokes real commands and ignores failures. High
  confidence in the broken contract; exact trigger for this mainline remains
  unisolated. Trees alone are not a proven cause because clearing is estimated.
  Slopes, rail phase/compatibility, signals and fabrication are distinct candidates.
- **Small deterministic regression:** 1x1 depot on flat clear terrain with
  fabrication off: query omits `RailBuildCost`, actual `rail_cmd.cpp:1083–1094`
  adds it. Existing test prices imply 500 versus 600 plus matching clearing.
  The audit prediction was subsequently executed: 500 versus 600 failed before
  the fix; both now return 600. See `audit/2026-09-15/wp01/depot-before-initialized.log`.
- **Impact:** UAT-00/04 and blueprint-assisted construction blocked. Execution
  can mutate tiles before the assertion and before money subtraction. With
  assertions off it may silently leave partial layouts while charging successful
  child costs; do not label all placement free or the crash save corrupt.
- **Smallest repair:** one deterministic validated placement plan using canonical
  construction rules, with complete preflight, exact cost/resource aggregation
  and no ignored child failures. Preflight must model prospective track for
  signals/overlaps; independent dry-runs on an unchanged map are insufficient.
- **Regression/retest:** depot cost parity first; normal dispatcher; all eight
  full footprints; signal conversion; terrain; rail/phase/owner restrictions;
  late obstruction; insufficient aggregate money/BOM; save/reload. Preserve
  command assertions and conservation. Graphical replay on copied crash save
  and fresh empty pad, recording each sub-action, is required to close.
- **Audit checks:** 67 selected existing CTests pass; copied crash save and v1.1
  each complete a null-video 1,000-loop load smoke. Neither places a blueprint.
  Supported native automation unavailable (`orca-ide: command not found`;
  native CUA disabled). These are historical audit checks, not placement evidence.
- **WP-01 evidence:** [run record](audit/2026-09-15/wp01/README.md): full preflight,
  exact canonical costs and aggregate BOM; copied signals consume material.
  78 distinct selected CTests passed, including all 128 prefab variants and a
  mixed-infrastructure money/material save/reload test. Normal dispatcher checks
  retain query/execute assertions. Complex station overbuilding and destructive
  clearing reject safely; see the recovery plan for exact support boundaries.
  Next: graphical UAT-00/04c and reload on copies of the crash save and fresh pad.
  The exact historical target tile/trigger remains unisolated.

## Additional defects and design blockers

For source-only entries, configuration is current source abbcd7e077; the existing
binary reports that ref, Release/asserts ON, but the stated reproduction has
**not** been executed. Use copied version-367/no-GRF v1.1 only where its fixture
satisfies the stated preconditions; otherwise build the specified isolated test.
WP-02/03 later implemented hub deposit/loading repairs as recorded below; other proposed
checks remain open unless explicitly updated. Reset by reloading the untouched copy;
never save a failed partial operation over the fixture.

| ID / priority / confidence | Action, expected → actual and evidence | Smallest repair; regression and human retest |
|---|---|---|
| **OST-BP-002 P1, repaired locally; graphical retest pending** | WP-06 validates version/types before narrowing, dimensions/count/byte/depth limits, duplicates, UTF-8 metadata and every rail/signal/depot/station enum. Signals require present compatible tracks and valid direction bits. Invalid models cannot transform. | Table-driven malformed data, maximum valid footprint, central IsValid/transform rejection and real GUI invalid-signal import tests pass. All built-ins and128 placement variants still pass. [WP-06 evidence](audit/2026-09-15/wp06/README.md). UAT-04f pending; existing files retained. |
| **OST-HUB-001 P1, deposit/loading repaired locally; human retest pending** | WP-02 reproduced60 input→industry60+stockpile60 and repaired exclusive freight storage. WP-03 reproduced100 stock→40 with0 waiting/onboard under full-pool refusal; loading now allocates before withdrawal and preserves100/dispatch0 on refusal. | WP-03 adds four native-tick regressions: full-pool/retry,12 capacity/reserve/order/rights boundaries, packet cleanup and actual save/reload after refusal or downstream split failure.61 distinct selected CTests pass. [WP-02 evidence](audit/2026-09-15/wp02/README.md), [WP-03 evidence](audit/2026-09-15/wp03/README.md). UAT-10/14 pending; binding/authority OST-HUB-002 remains separate. No old-balance rewrite. |
| **OST-HUB-002 P1, repaired locally; human lifecycle retest pending** | WP-04 reproduced invalid attachments succeeding and charging75,000. Shared validation now requires live owned rail, same world, actual platform within four Manhattan tiles, unique station/anchor; query and execute agree. | Seven new cases cover atomic rejection, automatic selection, partial/full demolition, station ID reuse, native takeover/bankruptcy and real save/load. Invalid/duplicate legacy bindings retire without stockpile writes. [WP-04 evidence](audit/2026-09-15/wp04/README.md). UAT-10 lifecycle pending; fresh binding UI stays WP-09. Company-wide inventory takeover/overflow policy remains separate. |
| **OST-AUTH-001 P1, repaired locally; visual acceptance pending** | WP-05 gives HQ upgrades an owner-checked exact-next-tier command. Directory open/refresh is read only; colonize/promote update canonical state only on successful execution. Free HQ upgrades and existing world fees/thresholds are preserved. | Seven command/actual-GUI-click/persistence cases and88 related CTests pass. TCP native server/two-client command queues agree before execution, after six denials/three successes and after save/reload. Outpost location now persists in PLNT; older missing fields default invalid. [WP-05 evidence](audit/2026-09-15/wp05/README.md). Full join/map handshake and graphical UAT-08/10 remain separate. |
| **OST-COL-001 P1, high code confidence** | Colonize a valid Phase4 world with no town; expect functioning settlement → `portal_cmd.cpp:617–627` changes world before raw `Town::Create`, omitting native town initialization (`town_cmd.cpp:2013–2087`). Full pool can still yield founding success without town. | Reuse native town initialization after deterministic terrain/footprint/pool preflight, commit town/world atomically. Test population/buildings/spatial lookup/ratings and failed founding rollback. Graphical settlement + reload retest UAT-08. |
| **OST-FED-001 P1, high** | Naturally drive train into registered external gate; expect dispatch → `IsClosedOpenSpaceRailHead` (`tunnelbridge_cmd.cpp:1878,1893,2013`) rejects it because local other-end lookup is INVALID. Pathfinder has different external exception. | Distinguish valid external terminal from stale local link; keep unlinked/conduit/stale rejection. Four-direction real vehicle-entry tests then two-process natural departure. UAT-16. Do not enable unsafe distributed transfer before FED-003 acceptance. |
| **OST-PORT-001 P1, high source risk** | Dedicated DestroyPortalGate command on owned unoccupied external gate; expect unlink/removal → `portal_cmd.cpp:406–407` dereferences null local link. | Branch on local/external/unlinked kind and validate endpoint; command tests, ownership/reservations/persistence. No native UI caller found: ordinary bulldozing is a separate path, not a reproduced user crash. |
| **OST-FED-003 P1, review risks** | Drop departure response, retry/restart with old save or restore a remote consist; expect unique durable custody/identity/orders → tick-based request IDs (`federation_cmd.cpp:84`), synchronous departure before destruction (`:140–155`), namespace/owner fallbacks (`consist_materializer.cpp:172,303`), reconstructed default orders (`:322`) and no demonstrated reconcile-before-move leave serious gaps. | Separate transaction, identity/order and recovery subpackages. Stable transfer ID, durable reconciliation, exact global namespaces and order flags, explicit unresolved destination errors. Fault injection at every transition with physical per-cargo and vehicle counts; natural ordered round trip and old-save recovery. UAT-16; no public federation acceptance yet. |
| **OST-EC-001 P2, fixed & verified (PR #8)** | WP-10 tracks actual whole units returned by OpenTTD station allocator; preserves rating, service, competition, and fractional carry. LandInfo displays honest potential, allocation, and outcome status. | 6 test cases (97 assertions in `[wp10]`) pass; honest waiting+onboard and LandInfo display verified. [WP-10 follow-up in recovery plan](RECOVERY_PLAN_2026-09-15.md). |
| **OST-BP-003 P2, high** | Capture and drag selection; expect name dialog → capture mode set but no `VpStartPlaceSizing`/OnPlaceDrag; OnPlaceObject only places (`blueprint_gui.cpp:238,374`). | Wire standard selection lifecycle and cancellation; propagate SaveBlueprint errors (`:358`). GUI capture/naming/reopen and invalid/empty/foreign capture cases. UAT-04. |
| **OST-BP-004 P2, repaired locally; graphical retest pending** | WP-06 tracks actual backing filenames; atomic checked writes preserve old bytes/memory on failure. Rename changes metadata in the same file. Imports reject duplicate player names, sanitized filename collisions are disambiguated, and exports never overwrite an existing destination. GUI path queries perform real file I/O and report errors. |35 selected CTests pass including real query/OK callbacks, roundtrip files, reopen, collisions, symlinks and bounds. Six real I/O fault points preserve old data and permit retry. [WP-06 evidence](audit/2026-09-15/wp06/README.md). UAT-04f/g graphical acceptance pending. |
| **OST-BP-005 P2, high code confidence** | Route through every advertised Wye/RoRo port; expect connected routes → Wye TrackY at (5,4)/(6,4) lacks curve into row5; RoRo parallel rows have leads only on y2 (`blueprint_manager.cpp:228–294,408–455`). | Native track-follower/YAPF movement requirements for every port, then local topology repairs. Four sequential rotations (not Rotate(4) shortcut), mirrors, RHD/LHD and running trains. Full first-tile/count tests are inadequate. UAT-04. |
| **OST-UI-001 P2, confirmed missing workflow** | Fresh player establishes HQ/hub or edits reserve; docs promise actions → commands/managers exist, no production UI callers for placement/hub/reserve editing. | Scope native build/select-station and reserve controls only after HUB-002/AUTH-001; clear requirements/error display. Human fresh-game establishment and reserve boundary loading. UAT-10. Design decision for transfer-order semantics remains separate. |
| **OST-CONT-001 P2, high** | Enable packs in a fresh game and use distinct material chains; expect WIRE/CHIP/ALLO/BCRY/QCRY/CCRY → production/stockpile mappings use vanilla aliases (`production_chain.cpp:37–78`, `company_stockpile.cpp:121`). GRF generator primarily emits cargo/vehicle properties, not the NML industry's promised layouts. | Runtime label binding with explicit vanilla fallback, full generated-content inspection and one fresh content fixture before all chains. Validate 13 conceptual cargo entries, actual industries and vehicle refits; keep migrated saves unchanged. UAT-13/14. |
| **OST-DES-001 P2, design discrepancy** | Research/phase progression expected to replace calendar and deliver every described unlock → calendar still gates `engine.cpp:1298–1310`; several tech effects are only catalogue descriptions. HQ presence counts global developed worlds, not company operations (`corporate_hq.cpp:117`). | Decide exact calendar/phase/company-presence and unlock rules; implement individually with behavioral tests. Do not silently redefine design to match code. UAT-08/10/12/13. |

Lower-confidence follow-up: portal takeover/bankruptcy infrastructure accounting
uses virtual length in `ChangeTileOwner_TunnelBridge` while portal build/removal
counts physical heads. Establish a focused accounting test before promotion to a
confirmed defect. Documented Oceanic zoning and blanket Phase2 extraction rules
also exceed current `CheckIndustryPlacement` checks; treat as design gaps.

## OST-TEST-001 — P1 verification blocker: invalid production fixture hidden locally

**WP-02 follow-up:** the reused production fixture now calls `MakeVoid` on
mandatory edge tiles and `MakeClear` only for `IsInnerTile` tiles. Three existing
production gameplay tests pass locally. This repairs the fixture source; an
assertion-active CI run is still required to close the verification blocker.

**Observed remote CI failure**, not a new player crash: live main345e258867,
Linux GCC SDL2 job104339490455, run34956427967, tests286–288 all abort at
`tile_map.h:136` (`IsInnerTile(tile) || type == TileType::Void`). Source
`test_sprint42_production_chains.cpp:454` makes every map tile grass, including
required outer void tiles. Expected valid fixture; actual invalid map setup before
production assertions. High confidence from source plus CI assertion.

The local existing Release build passes those cases. Preprocessing this exact
translation unit shows WITH_ASSERT and NDEBUG defined but final assert is a no-op;
configuration alone is insufficient coverage evidence. Catch2 assertions still
execute. Do not generalize this macro finding to all engine files.

Small repair package within WP-Q: initialize mandatory void boundaries through
native map rules, retain interior/production assertions and establish assertion-
active test coverage. Verify three cases under CI Debug/assert-active configuration
and the existing Release configuration, then relevant broader tests. No second
full build. No reason to disable SetTileType's assertion or rewrite production.
Closure needs original failing CI, valid fixture diff, configuration/macro evidence
and passing CI. Human production UAT remains separate. Logs:
`audit/2026-09-15/ci-main-linux-failure-context.log` and
`production-test-assert-macros.txt` in the same evidence directory.

## Historical review and previously verified fixes

The following original record is retained. Its “fixed” entries apply only to the
named behaviors; they do not close the new Blueprint/hub/federation findings.

### Original critical bug review — 2026-09-15

Scope: recent Sprint 37/42 implementation, v1.1 UAT review, current roadmap,
and the LLM world-building/AI UAT spike. Existing uncommitted UAT ownership,
fixture, save and tutorial changes were preserved. This is a focused review,
not a complete engine audit or human acceptance run.

## Fixed: P1 — research gates identify the wrong locomotives

Both `IsEngineBuildable` (purchase availability) and `CmdBuildVehicle` (depot
construction) call `CommonwealthPackManager::IsVehicleBuildableForCompany`.
That function previously treated global engine pool IDs 0, 7, 18, 24 and 87 as
CST families, regardless of which content defined the engine. Consequently,
ordinary engines could be research/phase locked without the Commonwealth pack,
while real CST locomotives could escape those restrictions. The NML and binary
generator define the five CST locomotive local IDs as 0x20–0x24.

Runtime gating now checks the loaded engine's Commonwealth rail GRF identity
and maps its local ID to the catalog family. Vanilla and unrelated NewGRF
vehicles retain their existing availability rules; CST wagons do not inherit
locomotive gates. Catalog-only policy tests use an explicitly named helper.

Regression coverage exercises a vanilla engine at the old Titan pool ID, an
unrelated GRF at the CST local ID, a CST Titan at pool ID 200 before/after
research, allowed/forbidden world phases, a coach and an invalid engine ID.

Validation:

- `ninja -C build openttd openttd_test`: passed.
- `./build/openttd_test '[sprint37]'`: 129 assertions, 8 cases passed.
- `ctest --test-dir build --output-on-failure`: 301/301 passed.
- Full suite log: `/tmp/openspacettd-critical-review-ctest.log`.
- No graphical UAT or active-pack save acceptance was performed.

## Remaining priorities

### Follow-up fixes: urgent UAT and gameplay blockers

**P1 — HTTP timeout leaves a dangling backend callback (fixed).** Federation
operations use stack-local `AuthorityRequest` objects. `ExecuteSync` cancels and
returns after five seconds, but HTTP cancellation is asynchronous. The backend
previously retained the destroyed request's address, so a later cancellation
check/data/completion callback could access freed memory and crash the game.
A separately retained callback relay now holds only a weak target, cancels when
the request is gone, discards late data and releases itself on terminal response.
Request copy/move is prohibited to keep the callback target address stable.
No change to transfer ownership, simulation commands or protocol acceptance is
claimed. Synchronous polling stalls and natural departure/recovery acceptance
remain separate follow-up work.

**P1 — deleting rail platforms leaves ghost processing facilities (fixed).**
The production cleanup hook was in `Station::AfterStationTileSetChange`, which
the rail removal path does not call. Removing the last platform could therefore
leave a live processing attachment and stranded buffers. Cleanup now runs in
the shared rail-removal path for both individual tiles and bulk demolition:
last-platform removal salvages buffered cargo and clears production acceptance;
partial removal preserves the facility and updates its anchor to the remaining
platform. Waypoints do not execute station-production cleanup.

Validation of this follow-up:

- Federation transport/transfer checks: **198 assertions, 13 cases passed**.
- Production gameplay plus transport checks: **181 assertions, 7 cases passed**.
- Full CTest: **306/306 passed**; `/tmp/uat-blocker-fix-ctest.log`.
- UAT v1.1 full-engine null-video load: **1,000 ticks passed**.
- Timeout tests inject delayed callbacks without sockets, including late failure,
  late success, queued data and abandoned asynchronous requests.
- Production regression fixtures now initialise language data and a valid town
  name generator, and load/stage cargo through valid engine states. These fixture
  corrections exposed the real platform-cleanup bug rather than suppressing it.
- Existing locomotive-gating, UAT and production integration changes were
  preserved. Graphical player UAT and live network recovery remain unverified.

### Still open

1. **Player production facilities — implementation gap resolved in the follow-up.**
   [Station production integration](PRODUCTION_GAMEPLAY_INTEGRATION_2026-09-15.md)
   adds player construction, delivery, output, retirement and persistence.
   Human UAT-14 remains Not run; active-pack cargo acceptance is separate.
2. **High-priority acceptance gap — natural federation departure/recovery.**
   The runner explicitly dispatches trains. Natural entry, remote arrival,
   return orders, obstruction, deduplication and restart need separate evidence;
   the existing runner is not proof that all of them work or fail.
3. **Content acceptance blocker — UAT-13.** The migrated save does not activate
   the new packs. Validate a separate new-game content fixture; this fix does
   not activate or retrofit content in existing saves.

The ownership repair already present in the worktree remains subject to human
UAT-01. The LLM spike is a proposal: its manifest APIs, AI modules and telemetry
examples are not implementation or acceptance evidence. Production integration
should precede automation built on those assumptions.

The roadmap's final evidence-gap list was corrected: it still claimed no pack
sources/GRFs and no process-level federation runner existed, contradicting its
own current acceptance section. Remaining limitations are retained explicitly.
