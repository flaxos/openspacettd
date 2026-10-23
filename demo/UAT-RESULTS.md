# Human UAT results — recovery / v1.1

## The Hollowfrost Survey — automated proof passed; human UAT pending (23 October 2026)

**Human desktop status: Pending.** [Play guide](alien_worlds/PLAY.md).

- **Desktop mission (about 30 minutes)**: run
  `python3 scripts/alien_worlds.py play --profile /path/to/new/folder`. Read the Story
  Book, lay rail along the graded line and through each gate terminal, order the survey
  train as the guide says and unpause. Success: the three goals complete and the
  closing page appears. Report whether the craters, lakes, snowy rock walls and
  basalt read as alien worlds, whether the warehouse rule was clear, and how long it took.

## Reserve Works pilot — native run passed; human UAT pending (20 October 2026)

**Human desktop status: Pending.** One approved headless run passed integrity: exactly 24 + 4
monthly evaluations, conserved cargo, a train-hauled kit with an exact £50,000 + 160 STEL /
80 BALL debit, and an equal fresh-process reload. The Reserve buffer was never used: SILC
peaked at 270 of 1,200. [Run record](../docs/audit/2026-10-20/reserve-pilot/README.md).

- **Desktop mission (5 minutes)**: load `docs/audit/2026-10-20/reserve-pilot/checkpoint-24.sav`.
  Open the Facilities window and find the Ballast Crusher row. Success: it shows
  `Works: Active - Reserve`, an input buffer below 1,200 SILC, and World 1 stock of
  640 STEL / 1,360 BALL. Then open the Megacity window: the food rate reads 20/mo.
  Report anything that disagrees, and whether Reserve felt worth choosing.

## Vale Railway — automated proof passed; human UAT pending

The authored empty opening, actual rail input and subsequent production, repeat
warehouse service and fresh-process reload passed local bounded proof. The real
SDL purchase screen selected a forty-unit BALL car and its ordinary Buy handler
charged 93 GBP for one empty wagon, with zero simulation advancement. Existing
fleet/materials/objectives remained intact. These are automated observations;
human desktop appearance, comprehension, fun and pacing remain pending.
[Sanitised result](../docs/audit/2026-10-12/vale-railway-publication/README.md).


## One Material-Funded Research Unlock (Card P1) — 11 October 2026

**Human desktop status: Pending.** Automated Catch2 test suite (416 assertions across 14 cases in `src/tests/test_commonwealth_works.cpp`), full CTest suite (564/564 tests passing 100%), and native headless simulation runner (`scripts/verify_commonwealth_research.py`, 15 periods + 4 reload periods) executed on branch `codex/capacity-or-reserve-pilot`.
- **Pipeline B Freight & 2-Input CHIP Logistics**: Verified dedicated freight rail network hauling physical cargo: Iron mine -> Steel furnace -> HQ warehouse (40 `STEL`); Copper mine -> Copper smelter (`COPR -> WIRE`); Silica dunes -> Silicon works (`SAND`); Copper smelter -> Silicon works (`WIRE`); Silicon works -> HQ warehouse (20 `CHIP` via `2 SAND + 1 WIRE -> 1 CHIP`). Unloaded into company stockpile at World 0 warehouse.
- **Corporate HQ & Feedstock Acceleration Off**: Corporate HQ placed at World 0 (`TileXY(70, 60)`, £2.5M cash). Feedstock acceleration confirmed OFF (`CmdSetResearchAcceleration(false)`). Baseline Materials I (100 RP) and Materials II (250 RP, £250k budget) prerequisites verified.
- **Atomic Kit Escrow & Negative Paths**: Verified missing kit rejection (< 40 STEL or < 20 CHIP) and foreign company rejection. Verified atomic debit of 40 STEL + 20 CHIP into escrow (`research[company].reserved`), and complete restoration on cancellation/project change without cargo leakage or duplication.
- **Materials III Unlock & Site Revelation**: Verified £600k cash budget completing 600 RP, consuming escrowed kit into `research_consumed`, unlocking `TECH_MATERIALS_3` (303), and revealing surveyed 16x16 Rare Earth extraction site (0x14, producing `RARE`) on World 2 at Node 17 (`Discovered() == true`).
- **Conservation & Cold Reload Probe**: Verified 100% cargo conservation (`stockpile + held + in_transit + consumed`) across 15 native evaluation periods. Verified byte-for-byte cold reload state match in a fresh engine process (`checkpoint-materials3.sav`), followed by 4 post-reload evaluation periods with continuing conservation.
- **Desktop Playtest Mission (5 Minutes)**:
  * **Objective**: Confirm that physically hauling 40 STEL and 20 CHIP into the Core World warehouse funds Materials III, debits kit escrow, and discovers the Rare Earth site on World 2.
  * **Step 1**: Launch OpenSpaceTTD desktop GUI and load `docs/audit/2026-10-11/material-research/checkpoint-materials3.sav`.
  * **Step 2**: Open Corporate HQ window (or company stockpile): verify 40 STEL and 20 CHIP were consumed, and company owns HQ at (70, 60) on World 0.
  * **Step 3**: Open Research window: confirm Materials III is unlocked (`TECH_MATERIALS_3` completed).
  * **Step 4**: Open World 2 viewport at Node 17 (`TileXY(540, 80)`): verify the surveyed 16x16 Rare Earth extraction site is now visible and discoverable (`Discovered() == true`), allowing mine construction.
  * **Step 5**: Check company finances and freight trains: confirm trains 10, 11, 12, 13, 14 are running smoothly hauling freight between mines, smelters, and warehouses.
  * **Success Criteria**: Research is completed with kit consumed, Rare Earth site on World 2 is revealed, and freight logistics continue operating without errors.



## Freight You Can Read (Card C1) — 11 October 2026

**Human desktop status: Pending.** Automated Catch2 test suite (86 assertions in 5 cases in `test_sprint43_empire_facilities.cpp`), `[sprint43]` suite (274 assertions in 12 cases), and full CTest suite (563/563 tests passing 100%) executed on branch `codex/capacity-or-reserve-pilot`.
- **4-Line Facility Presentation**: Verified complete row layout in `empire_facilities_gui.cpp`:
  * Line 1: World Name, Role, Phase distinct, Station name/Tile.
  * Line 2: Status badge in highlight colour + Recipe ratio + Rated throughput (`batches/mo`) + Last completed-period production (`batches` or `[Unavailable]`).
  * Line 3: Factory Buffers (inputs with complete batches permitted, outputs with remaining clearance).
  * Line 4: Works Bill (`£50k + 160 STEL / 80 BALL`), queried price via `DoCommandFlag::Query`, and World Warehouse Stock.
- **Authoritative Bottleneck Diagnosis**: Verified constraint evaluation order: technology locked (`[Locked: Requires ...]`), blocked output (`[Blocked: Output <Cargo> Full]`), missing input (`[Starved: Missing ...]`), partial input (`[Starved: Insufficient ...]`), active operation (`[Active: ...]`), buffered ready (`[Ready: ...]`).
- **Query Purity & Navigation**: Verified that opening, resizing, painting, tab cycling, refresh clicks, and navigation clicks (`[Locate Plant]`, `[Locate Station]`) execute pure reads without modifying company money, game ticks, calendar dates, or RNG state.
- **Desktop Playtest Mission (5 Minutes)**:
  * **Objective**: Verify that an owned processing plant (Ballast Crusher at Node 4, World 1) is immediately understandable in the Facilities window without external guides.
  * **Step 1**: Launch OpenSpaceTTD desktop GUI and open the Facilities window from the map menu (or hotkey).
  * **Step 2**: Inspect Ballast Crusher row: confirm Line 1 shows World 1 ("Industrial Commonwealth"), Role ("Industrial"), Phase ("Phase 2 Developed"), and linked station Stop 4.
  * **Step 3**: Confirm Line 2 shows prominent status badge, recipe name (`2 Silica Sand -> 2 Trackbed Ballast`), rated throughput (`Rated: 100 batches/mo`), and completed-period output.
  * **Step 4**: Confirm Line 3 shows input buffers with complete batches permitted (`permits X batches`) and output buffer space.
  * **Step 5**: Confirm Line 4 shows Works bill (`£50k + 160 STEL / 80 BALL`), queried price, and warehouse construction stock.
  * **Step 6**: Click `[Locate Plant]` and `[Locate Station]`: verify viewport instantly centers on the factory and platform without mutating funds or state.
  * **Success Criteria**: All four lines are visible, truth-in-advertising metrics reflect physical reality, and navigation centers correctly.

## Commonwealth Pilot Correction and Truthful Acceptance — 11 October 2026

**Human desktop status: Pending.** Automated Catch2 test suite (374 assertions across 13 test cases in `src/tests/test_commonwealth_works.cpp`), `[integrated-economy]` suite (490 assertions across 17 test cases), full Catch2 suite (307,709 assertions in 543 test cases), CTest suite (558/558 tests passing), and offline runner guards (`scripts/test_verify_runner_guards.py`, 7/7 passing) executed on branch `codex/capacity-or-reserve-pilot`.
- **Sprint 1 Engine & GUI Corrections**: Verified retired food service reset on company deletion vs acquisition, strict V2 save invariant rejections (factory tuples, rates, ceilings, history bounds, fractional remainder, transactional rollback), button-only rate cycling in `megacity_gui.cpp`, spectator/foreign disabling, chronological history with fraction met, and visibly disabled V2 Upgrade All in `empire_facilities_gui.cpp`.
- **Sprint 2 Truthful Acceptance**: Verified material debit (£50k, 160 STEL, 80 BALL), legal road frontage requirement, native house shell placement at stage 0, tile progression through native ticks to `TOWN_HOUSE_COMPLETED`, population credit (+232 citizens), and permanent demolition immunity (`IsHousePlayerProtected`) under commercial food shutoff.
- **Sprint 3 Truthful Acceptance**: Verified commissioning of alternate Silica extraction on World 3, direct portal delivery to Ballast Crusher in World 1, BALL production from alternate supply, and continuous BALL output during primary corridor feeder interruption.
- **Physical Pilot Simulation**: Corrected runner (`scripts/verify_commonwealth_works.py`) prepared with monotonic evaluation events, V2 service history recording, and partial/error report retention. Executed once on 20 October 2026 after owner approval; integrity passed and the Reserve buffer was never used ([run record](../docs/audit/2026-10-20/reserve-pilot/README.md)).
- **Human Desktop Acceptance**: Interactive GUI inspection (Megacity food rate cycling, facilities works choice buttons, works bill display) and multi-world route playtesting remain **Pending**. Playable owner mission defined below.

## Vehicle layers and palettes, Stage 3B — 14 October 2026

**Human desktop and real SpriteStack content status: Pending.** The
[Stage 3B audit](../docs/audit/2026-10-14/cab-view-stage3b/README.md) records bounded
automated acceptance: seven explicit installed-asset sequences match native
pixels, including visibly reversed red/blue train layers. Three final native
journeys preserve the Stage 3A images, gate/world boundaries, simulation and
normal viewport. The measured journey assets have one layer; no genuine stacked
GRF journey is claimed. Final budgets pass, while an earlier Reduced p95 failure
remains recorded with cause unproven.

Next content mission: use an existing genuinely stacked vehicle GRF on a
disposable save, record source/binary/content and every layer/palette, then check
livery, offsets, order and visibility through approach, mixed arrival and return.
Record actual display/blitter, natural-speed comfort and Pass/Fail/Partial;
repeat relevant timing and ordinary-viewport checks. Animated palette behaviour,
arbitrary translucent 32bpp assets and other platforms need separate evidence.
Destination-dependent transparent/text-colour layers and aircraft shadows/rotors
remain outside the supported claim. No owner save or running game was used.

## Gate crossing effects, Stage 3A — 14 October 2026

**Human desktop status: Pending.** The
[Stage 3A audit](../docs/audit/2026-10-14/cab-view-stage3a/README.md) records accepted
automated evidence: native forward/reverse/cold crossing, animated edge lensing,
opaque transit, actual-arrival settling, static Reduced and exact Off. Three
fresh native repeats provide 231 screenshots, 216 profiling cells and six exact
normal-viewport comparisons. OFF/ON builds and 37 focused tests pass.

Remaining mission: on a disposable local-gate train, watch approach/transit/arrival
in both directions at natural speed; cycle Full / Reduced / Off, resize/look,
close/reopen and check comfort/readability. Full is the fresh-window default;
Reduced removes motion and distortion; Off restores baseline cab pixels. Confirm
no stale source image or invented transit after a missed phase. Record exact
source/binary/content, display/blitter and Pass/Fail/Partial. Injected paused GUI
callbacks and private fixed-scale profiling do not establish interactive FPS or
human comfort. Other platforms and broader graphics remain unaccepted.

## Local gate cab, Stage 2 — 14 October 2026

**Human desktop status: Pending.** Automated engineering acceptance is complete
for the bounded local-gate slice; see the
[Stage 2 audit](../docs/audit/2026-10-14/cab-view-stage2/README.md).
Twenty final screenshots, 47 native assertions and independent Astra review
cover an actual four-car A→B→A journey, opaque hidden transit, mixed-world
visibility and a fresh-process reload/return. Both ordinary viewport comparisons
are pixel-identical below the toolbar. A visible arrival-floor gap was repaired.
This proof used a new private fixture and SDL offscreen; no owner save or running
game was used. Human appearance, comfort, performance and other platforms remain
unaccepted.

Remaining mission: use a disposable owned-train local pair, observe approach,
opaque transit and arrival in both directions, look and resize, close/reopen,
then check access loss and unsupported gate classes fail safely. Record exact
source/binary/content, display/blitter and Pass/Fail/Partial. The simple gate
frame/floor is intentional at this stage; Commonwealth art fidelity belongs to
the separately scoped Stage 3. The Stage 1 mission below is historical.


## Experimental native train cab, Stage 1 — 14 October 2026

**Human desktop status: Pending.** A default-OFF local source port exists on
`codex/cab-view-stage1`, based on main `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`.
Automated builds, pixel-buffer/mask/lifecycle checks and a native SDL offscreen
proof are recorded in the [cab audit packet](../docs/audit/2026-10-14/cab-view/README.md).
Nine screenshots and independent review verify paint, correct look directions,
480×300 to 640×400 resize, a four-tile journey, reopening and owner-loss closure.
This used a new disposable WP11 fixture; no owner save was loaded, advanced or
converted, and no running owner game was used.

Remaining desktop mission: use a fresh disposable owned-train fixture, open the
cab, watch a short accessible-world journey, look left/ahead/right, resize and
close/reopen, and inspect the ordinary 2D viewport. Approach an unsupported gate
and verify safe closure before transit. Record the exact source/binary/content,
display/blitter and Pass/Fail/Partial result. Automated native paint has passed;
human appearance/controls, train artwork, comfort and performance remain unaccepted.
Stage 2/3 and publication remain separate decisions.

## A World Worth Connecting (Sprint 3) — 11 October 2026

**Human desktop status: Pending.** Automated Catch2 test suite (229 assertions across 9 test cases in `src/tests/test_commonwealth_works.cpp`), full Catch2 suite (307,564 assertions in 539 test cases), CTest suite (554/554 tests passing), and headless verification runner (`scripts/verify_commonwealth_expansion.py`) executed on branch `codex/capacity-or-reserve-pilot`.
- **Concrete Source / Link / Consumer**: Ballast Crusher (`Node 4`, `TileXY(300, 80)`, World 1) consuming raw SILC to produce BALL. Primary source from World 2 Silica Dunes (`Node 13`, x=650) via Train 1 through the World 2 ↔ World 1 throat on row 82 (distance: 294 tiles). Alternative source on World 3 (`Research Frontier`, `WorldID{3}`, Stop 32 at `TileXY(860, 96)`), surveyed via `Commands::SurveyResources` and funded via `Commands::BuildIndustry`.
- **Independent Direct Portal Pair & Bottleneck Bypass**: Direct portal pair linking World 3 (`TileXY(790, 96)`) directly to World 1 (`TileXY(476, 96)`) via server-authoritative `Commands::BuildPortalPair`, feeding a new platform at Ballast Crusher (Stop 4) on row 96. Shorter route of 248 tiles (~15.6% reduction). Bypasses World 2 entirely and does NOT share the throat on row 82.
- **Genuine Capital Costs**: Full capital cost breakdown (£5,095,387 total spent from real company balance): surveying £60,000, industry £4,921,875, station £1,365, platform £1,093, portal pair £13,532, depot £540, freight consist (Train 9 / Consist 46) £76,657.
- **Selective Interruption & Production Resilience**: Primary corridor closure via `connected_economy close-primary-corridor` halts Train 1; Train 9 sustains SILC deliveries from World 3, keeping Ballast Crusher production rate steady without starvation collapse. Reopening via `connected_economy reopen-primary-corridor` restores dual-source flow.
- **Simulation Proof**: 24-period native simulation runs proved connection, dual-source operation, throat closure resilience, corridor reopening, 100% cargo conservation, and exact cold reload match.
- **Desktop Acceptance**: Human desktop verification of Track 13 in `demo/HUMAN-GRAPHICAL-UAT-GUIDE.md` remains **Pending**. Audit evidence staged at `docs/audit/2026-10-11/world-connection/README.md`.

## A District That Stays Built (Sprint 2) — 11 October 2026

**Human desktop status: Pending.** Automated Catch2 test suite (202 assertions across 8 test cases in `src/tests/test_commonwealth_works.cpp`), full Catch2 suite (307,537 assertions in 538 test cases), CTest suite (553/553 tests passing), and headless simulation runner (`scripts/verify_commonwealth_residential.py`) executed on branch `codex/capacity-or-reserve-pilot`.
- **Material-Funded Residential Expansion**: Commissioned one-time residential expansion for pilot Core town (`TownID{0}`, Commonwealth Metropolis) under `EconomyRulesVersion::V2`. Replicated command `Commands::CommissionTownResidentialDistrict` atomically debits `160 STEL + 80 BALL` and £50,000 cash; grants a finite budget of 4 native building opportunities.
- **Native House Shells & Staged Growth**: Candidate sites search outwards from town center for legal road frontage (`RoadTypesAllowHouseHere`, `TownLayoutAllowsHouseHere`, `CanBuildHouseHere`). Blocked frontage reports `"Residential Works: Awaiting site access (N remaining, frontage blocked)"` without burning budget. Available frontage constructs native house shells at stage 0 with zero premature population. Shells progress natively through stages 0 -> 1 -> 2 -> 3; upon `TOWN_HOUSE_COMPLETED`, town population is credited (+232 citizens).
- **Permanent Demolition Immunity**: Sets native `IsHousePlayerProtected` bit on completed residential buildings. Permanently blocks OpenTTD's random town redevelopment demolition loop in `TileLoop_Town` and `CanDeleteHouse`.
- **Food Service Decoupling**: Residential district construction and preservation operate independently of commercial food service. Completely shutting off food delivery does not revoke completed housing or trigger population decay.
- **Desktop Acceptance**: Human desktop verification of Track 12 in `demo/HUMAN-GRAPHICAL-UAT-GUIDE.md` remains **Pending**. Audit evidence staged at `docs/audit/2026-10-11/residential-district/evidence.json`.

## Capacity or Reserve Pilot Prototype (Sprint 1) — 11 October 2026

**Human desktop status: Pending.** Automated Catch2 test suite (150 assertions in 6 test cases in `src/tests/test_commonwealth_works.cpp`), full Catch2 suite (307,485 assertions in 536 test cases), CTest suite (551/551 tests passing), and headless simulation runner (`scripts/verify_commonwealth_works.py`) executed on isolated branch `codex/capacity-or-reserve-pilot`.
- **Stable Core Settlements**: Core towns under `EconomyRulesVersion::V2` maintain 1.0 passenger baseline ("Stable") without starvation freezes or population collapse when commodity delivery stops; house growth decoupled from supply counters.
- **Optional Commercial FOOD Service**: Rate-selectable (`Off / 10 / 20 / 40`) service with fixed receiver capacity 60 and sliding 4-month civic passenger bonus (up to +25%). Rate Off rejects deliveries, keeping cargo safely in consist custody, with fractional generation preserved via integer remainder accumulator.
- **Factory Works Choice**: One-time commissioning choice on player-owned bootstrap factory (Ballast Crusher: 2 SILC -> 2 BALL). Capacity Works boosts processing throughput from 100 to 150 batches/month; Reserve Works expands input buffer from 300 to 600 batch equivalents (1200 SILC). Real materials `160 STEL + 80 BALL` and £50,000 cash consumed atomically with duplicate protection.
- **Simulation Proof**: 24-period native simulation runs proved SILC feeder interruption, food interruption, 100% cargo conservation, and fresh-process cold reload.
- **Desktop Acceptance**: Human desktop verification of interactive GUI elements (food service cycling button, commissioning buttons, status badges) remains **Pending**. Track 11 documented in `demo/HUMAN-GRAPHICAL-UAT-GUIDE.md`. Audit evidence staged at `docs/audit/2026-10-11/capacity-or-reserve/README.md`.

## Horizon A Multi-Consist Shuttle Fleet & Sustained Megacity Expansion — 11 October 2026

**Human desktop status: Pending.** Automated integration test suite (184 assertions across 5 test cases in `src/tests/test_fleet_shuttle_growth.cpp`), full Catch2 suite (307,335 assertions in 530 test cases), headless verification runner (`scripts/verify_fleet_shuttle_growth.py`), and cold-reload proof on staged scenario `demo/OpenSpaceTTD-Sustained-Growth-UAT.sav` (SHA256: `957f169d2583530a6f9be0ed3874dbb18e75042ea96f38f901eddc0c4ab3416f`).
- **Logistics & Starvation Elimination**: Spacing 10 consists along the 17.1-month transit loop provides deliveries at headway $\le 1.7\text{ months}$, completely eliminating the 15.2-month starvation gap (0 starvation months over 36 simulated months, 100% food sufficiency).
- **Sustained Growth & House Additions**: Town food reserves stay continuously above 50 units across all evaluation cycles, sustaining continuous countdown, house expansion (29 -> 33 houses), and population gain (1,096 -> 1,276 citizens).
- **HyperGrowth Evaluated**: Multi-tier delivery of Sustenance (Food), Expansion (Steel, Ballast, Wiring), and Prosperity (Chips, Crystals) evaluates `MegacityGrowthState::HyperGrowth` (2.0x growth speed, 1.5x passenger/mail multiplier).
- **Desktop Acceptance**: Human desktop verification of live graphical consist spacing, Megacity status panel HyperGrowth badges, and continuous house appearance remains **Pending**. See [audit evidence](../docs/audit/2026-10-11/fleet-shuttle-growth/README.md).

## Commonwealth Wormhole Gate Art Runtime Integration — 11 October 2026

**Human visual status: Pending.** Automated Catch2 test suite (65 assertions in `test_gate_sprites.cpp`),
CTest suite (503/503 passing), native headless engine game initialization (resolving base sprite 6283),
and asset tests (`test_commonwealth_gate_art.py`) verify runtime loading and state/orientation mapping.
Human visual acceptance (verifying train transit clearance, sprite sorting, catenary wire overlay,
and visual aesthetics across zoom levels in desktop GUI) remains **Pending**.

## Core basket repeat and town station delivery — 10 October 2026

**Human UAT Passed (Transport & Station Unloading) with Operational Starvation Finding.**
Human desktop playtest executed using staged scenario `demo/OpenSpaceTTD-Core-Basket-Repeat-UAT.sav`
and advanced over 24 in-game years (tick 378,112 to tick 668,053, saved to
`demo/save/ASSISTED FUNCTIONAL Core Supply, 1974-09-02.sav`).
Confirmed: Train 4 reliably departs Station 2, transits to Station 10, unloads FOOD into
Core Megacity (Town 0) reserves, and generates valid transport revenue (£18,484,224 over 13 trips).
Operational finding: Because the city storage buffer is hard-capped at 3 months (150–165 units)
by `IntegratedEconomy::AcceptCity`, Train 4's single-consist 105-unit delivery sustains the city for
only ~1.9 months against a 17-month round-trip journey. The city spends >90% of simulation time in
starvation, resulting in net population decline (1,096 -> 870) and near-zero house growth (29 -> 30).
Automated proof: `./build/openttd_test` (522 cases), `ctest` (537/537 passed).
See [audit evidence](../docs/audit/2026-10-10/sustained-food-growth/README.md).

## Recovered full-basket evidence — 10 October 2026

**Human UAT Pending.** Historical automated evidence from October 5 records one
atomic FOOD/STEL/BALL consuming month and native growth-hook observation.
Population 1,191 and 29 houses did not increase; a hook observation is not house
growth acceptance. Sustained supply, a second basket and safe post-helper cold
continuation remain open. The October 10 Antigravity primary-checkout tests do
not establish gameplay acceptance or verify the unpublished station adapter.

[Exact checkpoint alignment and proof boundaries](../docs/audit/2026-10-10/checkpoint-alignment/README.md).
No owner save was loaded or advanced for this synchronization delivery.

## Core Megacity panel repair — 3 October 2026

**Human UAT Pending.** Antigravity's Core display patch passes focused and
aggregate automated checks, including construction-shortage and growth cases.
No player has checked the 92px header or tier/status text at game UI scales;
the fit assessment was static. A light visual check should open the Core panel
at minimum width, inspect the FOOD/STEL/BALL demand and reserve rows, observe
BALL as the sole missing construction item when FOOD and STEL are supplied, and
confirm the growth label matches the displayed numeric outcome. No material
delivery, monthly basket completion, cold continuation or economic acceptance
is claimed from these tests. The active UAT game was not changed.

## Core construction basket first attempt — 3 October 2026

**Human UAT Pending; ASSISTED FUNCTIONAL basket proof FAIL/incomplete.** The
automated attempt loaded the funded Core save, quoted and paid four normal
material trains/routes, then stopped at the first pre-tick running-cost guard.
There was no material delivery, monthly basket, growth change or cold proof.
The owner has not played this build; the old supplied observation save remains
the only delivered Core save. Cash, failure and corrected-but-unrun source are
[recorded](../docs/audit/2026-10-03/core-construction-basket/README.md).
Do not mark construction, economics or the owner mission accepted from the
passing planner and local gates.

## First Core Construction Basket intake — 2 October 2026

**Human UAT Pending; new ASSISTED FUNCTIONAL proof NOT RUN.** Owner-authorized
material extension starts from the supplied final funded observation state,
with zero added assistance. Normal-command iron/steel and stone/ballast transport,
actual complete monthly basket and cold continuation are the planned automated
claims. Supplied network observation does not establish manual construction or
ordinary-start economic acceptance. [Scope and forthcoming owner mission](../docs/audit/2026-10-02/core-construction-basket/README.md).

## First Functional Core Supply repaired observation — 1 October 2026

**Human status Pending. ASSISTED FUNCTIONAL requested outcome PASS across recorded
authorized handoffs; complete reviewed acceptance PARTIAL.** Repaired second load
matched the entire existing completed state; another105FOOD paid visit and60FOOD
consuming month passed, followed by final audit/save. The month used carried
inventory before that delivery; continuous supply and later-load consumption remain
unproven. Final cash£3,705,864/debt£100k; Materials I retained/budgetoff/II eligibility
already proved. Sole£6m allowance fully used, no additional grant or loan.

[Five-step owner playtest and safe checkout](../docs/audit/2026-10-01/functional-core/README.md#delivery-and-practical-owner-playtest)
uses a copy of the retained owner observation save. Preserve originals; apply no
grant and do not start II. Record exact build/save/content, manual versus supplied
actions, time, cash/debt and first unexpected control/message. Observation cannot
establish manual construction UAT. Independent replay/ordinary economics NOTRUN,
human acceptancePending; exact-head commit-format CI FAIL remains a delivery gate.
No merge or next slice. Earlier entries preserve their historical failed attempts.

## First Functional Core Supply third handoff — 1 October 2026

**Human status: Pending. ASSISTED FUNCTIONAL campaign: FAIL at second cold equality.**
Initial paid grain/FOOD/Core consumption, stopped/restarted supply control, paidHQ,
first cold equality, paidMaterialsI and II eligibility pass. The sole second-load
mismatch is growth0→1; the narrow existing-state repair has focused/full fixture
gates and independent source review, not a canonical continuation pass.
[Verified checkpoint, five-step owner observation and exact bounds](../docs/audit/2026-10-01/functional-core/README.md#safe-checkout-and-practical-owner-observation).

Assistance£6m is fully used, no additional grant; cash£3,620,714/debt£100k reconciles.
The supplied completed checkpoint is partial observation evidence only. A new
explicit automated continuation handoff must keep the original cold/absolute
clocks and aggregate counts; none was executed. Record build/save/content, manual
versus supplied actions, time, assistance and first unexpected event. No human,
ordinary economic or full functional acceptance, funded replay or next slice follows.
Earlier entries remain historical failed attempts.

## First Functional Core Supply recovery — 1 October 2026

**Human status: Pending. ASSISTED FUNCTIONAL single recovery: FAIL before first
state snapshot.** START/READY reached; the map fingerprint asserted on an
ownerless tile. Original and recovery failures are retained. Focused corrected
map/teardown and full native gates pass, but no flow, paid HQ/I/II or cold mission
was run. Total assistance applied is £0; ending cash/debt are unobserved. The
crash save is diagnostic and must not be offered as an observation checkpoint.

The [safe checkout and six-step manual feedback mission](../docs/audit/2026-10-01/functional-core/README.md#safe-fresh-checkout-and-manual-owner-feedback)
remains unverified. Record exact build/content, manual actions, total assistance,
cash/debt, elapsed time and first failure. Any further automated campaign needs
a new explicit handoff carrying the consumed recovery and existing bounds; funded
replay would exceed the one-total allowance. No human/economic acceptance or
next slice follows from automated gates. The prior entry below is historical.

## First Functional Core Supply implementation — 1 October 2026

**Human status: Pending. ASSISTED FUNCTIONAL automated campaign: FAIL at startup.**
The first bounded run stopped before any assistance command or checkpoint; all
food/control/research/cold/replay phases are NOT RUN. £0 was granted; actual ending
cash/debt were not observed. Focused designation/query/panel/save/relay tests and
the diagnosed console transport repair do not establish the playable outcome.

Use the [safe fresh checkout and six-step manual feedback mission](../docs/audit/2026-10-01/functional-core/README.md#safe-fresh-checkout-and-manual-owner-feedback).
There is no verified observation save or route quote. Record exact build/content,
manual steps, assistance/cash/debt, elapsed time and first unexpected message;
15 minutes is a feedback window, not promised completion. The reviewed plan
requires a new handoff for an automated full-campaign retry. Historical economic
failures remain failed; no human acceptance, merge or next slice is inferred.

## Functional progression replan — 1 October 2026

**Human status: Pending. No new game or human test was run for this docs change.**
Generation, A1 and optional art retain their scope-specific pending results;
the historical seed11 financial failure below remains failed economic evidence,
not a current veto on all functional testing. PR65 is now merged; PR66 owns its
separate portability follow-up. Check the actual build before testing.

The proposed [six-step mission](../docs/ACTIVE_EXECUTION_PLAN.md#six-step-human-mission)
covers manual grain/FOOD construction, real Core consumption, paid HQ/Materials I,
and save/reload. It uses one disclosed £6m cash grant and makes no earned-profit
claim. The mission is proposed, not a published verified playable checkpoint.
Record manual versus prebuilt steps, exact build/content/save, assistance/cash/debt,
time and the first unexpected control/message. A supplied observation checkpoint
cannot pass manual-build UAT, and timebox expiry is Partial rather than Pass.

## New integrated-game generation — 1 October 2026

> Historical delivery report. The functional-first replan above supersedes the
> blanket active-plan block; no human or financial result is rewritten.

**Human graphical status: Pending; active execution plan BLOCKED at revised A1
finances.** Generation/legal paid join/Core house catchment/repeat/cold proof passes
actual seeds 11/101/2026, with unchanged original saves. SDL/dummy drawing smoke
passes six focused views; it is not desktop/OpenGL or human acceptance. Revised
seed 11 A1 service reaches cash −£170 with fixed £190k debt before its third further
paid load; seeds 101/2026 operation are NOT RUN. No FOOD consumption is claimed.

Use the [generation-only human mission and safe separate-checkout instructions](../docs/audit/2026-10-01/generation/README.md).
Record exact build/seed/content, visible Core town/own houses, normal paid join/
station preview, cash/debt, reload, elapsed time and first unexpected control or
message. Do not use the failed financial checkpoint as accepted economic proof.

## Commonwealth platform art proof — 1 October 2026

**Human visual status: Pending.** Review the optional four-platform family using
the [isolated art mission](../docs/COMMONWEALTH_PLATFORM_ART_PROOF.md). Compare
normal/2x zoom and both orientations beside native shelters, with company colours,
transparency and a moving train; save, quit and reload. Record exact build,
base graphics, art hash, UI/display scale, seams/occlusion and observed frame/RSS
impact. Paired native freight/reload and SDL automation are separate evidence.
The authored WP11 art scene does not replace A1's ordinary-start human mission.

## A1 First Sustainable Freight — 30 September 2026

**Human graphical status: Pending.** The owner-approved Codex takeover implements
only the approved Option A slice. Use the exact build/profile/content and
[5–10 minute timeboxed fresh New Game mission](../docs/audit/2026-09-30/first-freight/README.md).
Fast-forward is necessary; manual construction time and normal UI usability have
not been measured. Native automation and saved command-built checkpoints do not
substitute for the fresh graphical mission.

The runner seed correction supersedes the earlier 28 September three-seed generation
claim: all those labelled runs actually used seed 11. Historical files remain intact.
All three actual native seeds pass the bounded command/cargo/cost/output-buffer/
cold-reload proof; complete datasets and retained failures are linked in the audit.
Record actual build/seed/content, elapsed time, cash/loan before/after, first unexpected
action/message and inaccessible controls. No human pass or A2 completion is inferred.

## Organic UAT crashes — 30 September 2026

**Human graphical retest: Pending.** Both reported failures are addressed on
`codex/fix-organic-uat-crashes`: merged empty-tooltip fix plus complete terminal
terrain protection and scoped recovery of the original UAT save. Automated native
rendering-query and SDL smoke checks pass; see
[the audit](../docs/audit/2026-09-30/organic-uat-crash/README.md).

Restart the rebuilt game with the original command:

```sh
./build/openttd -g demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav
```

Inspect and scroll around Augusta's portal terminal, then hover/right-click the
prompt-generator, HQ and facility buttons at 150% scale. Use a new save filename
for your retest; the original scenario and crash saves have not been overwritten.

## Empty-tooltip crash repair — 29 September 2026

**Human graphical retest: Pending.** Automated tooltip windows exercise empty,
newline-only and normal text at 100%, 150% and 200% scale. The reported crash save
is preserved; native load, continued simulation and a separate save/reload were
checked. See [repair evidence](../docs/audit/2026-09-29/tooltip-crash/README.md).

To retest: load the crash save using the fixed build at 150% UI scale, open the
prompt generator and hover/right-click its presets and Generate button. Repeat
for HQ tabs, facilities filters and trade tabs. Buttons without help should not
open a tooltip; title-bar help should still appear. Do not overwrite the original
crash save while testing.

## Unified Commonwealth economy — 28 September 2026

**Human graphical status: Pending.** No human pass is inferred from automation.
Follow the comprehensive **[HUMAN-GRAPHICAL-UAT-GUIDE.md](HUMAN-GRAPHICAL-UAT-GUIDE.md)** and use **[HUMAN-UAT-CHECKLIST.md](HUMAN-UAT-CHECKLIST.md)** to record visual observations and test results.
The [UAT checklist and saves](INTEGRATED-ECONOMY-UAT.md) cover Fund Industry,
physical factories, warehouse priority, research kits, construction materials,
city baskets and shared research. The earlier user acceptance below applies to
its earlier build.

Automated evidence: full 473-case unit suite / 67,508 assertions, 485 CTests,
all-chain native progression, a profitable 24-month operating soak, food
interruption/recovery, electric construction, save/reload, sector generation and
two legacy-save regressions. Three hosts with three joined clients pass shared
research/outage and five round trips plus a sixth after coordinated cold restart,
with cargo and cash reconciled. Exact hashes and federation results are retained in
[the audit](../docs/audit/2026-09-28/integrated-economy/README.md).

## Current user acceptance and direction — 23 September 2026

The user confirms human UAT is OK and accepts the new graphics as good enough
for now. Further graphics improvements are optional later work. This supersedes
older blanket statements that human UAT and graphics acceptance are pending.
This is a user-reported overall acceptance, without new per-subcase captures or
a pinned build/save manifest; historical observations remain unchanged.

The subsequent live test reproduced a local portal hop and client desync. The
repair now passes a loaded, natural-entry round trip between two dedicated servers
with one joined client each, including a run with two graphical clients. Ten cargo
units and the global train identity return without a detected desync or duplicate.
This is a controlled two-car fixture with no station orders; scheduled routes,
restart recovery and human federation acceptance remain open. See
[the current roadmap](../docs/PROJECT_STATUS_AND_ROADMAP.md#historical-milestone-live-federation-with-scheduled-freight--completed-27-september-2026).


Suite/fixture ID: ______  Save SHA256/version: ______  Base graphics/version: ______
Language/UI scale/resolution: ______  Simulation date: ______  Reset copy: ______

Tester: ______  Date: ______  Build/hash: ______  Save copy: ______
Company name: ______  Active NewGRFs: ______  Environment: ______

Record observations, not assumed passes. Attach screenshots or logs and the
failing working save where appropriate. Never put account credentials in evidence.

**WP-11 automated update (16 September):** the separate version-1 active-content
structural fixture passes moving ore→steel→depot, two local portals, blocked-route
recovery, fresh-process reload and three complete deliveries in 1800 and 2300.
377 CTests pass. No new human pass is claimed. [Quick check and saves](WP11-UAT.md)
· [evidence](../docs/audit/2026-09-16/wp11/README.md).

**WP-02 automated update:** exclusive hub unloading is implemented locally;
57 distinct selected CTests pass, including real cargo/station ticks and save/reload.
This is not a human UAT pass. [Evidence](../docs/audit/2026-09-15/wp02/README.md).

**WP-03 automated update:** allocation-safe loading is now implemented. Four new
cases cover full pool/retry,12 reserve/capacity/order/rights scenarios, packet
cleanup and actual save/reload after failed pickup or failed splitting.
Human delivery/pickup acceptance remains open; WP-04 subsequently repairs binding
while the establishment/reserve UI remains WP-09.
[Evidence](../docs/audit/2026-09-15/wp03/README.md).

**WP-04 automated update:** supplied/automatic station authority, demolition,
company acquisition/bankruptcy and persistence are repaired locally. Seven new
regressions exercise real commands and engine lifecycle/save paths. This does not
mark the UAT-10 graphical lifecycle checks as passed.
[Evidence](../docs/audit/2026-09-15/wp04/README.md).

**WP-05 automated update:** HQ upgrades and Directory actions now use authoritative
commands. Seven new local tests and88 related CTests pass; a native-command TCP
server/two-client replay confirms denied actions preserve state, three successful
changes apply once and full compared state survives save/reload. Outpost locations
now persist. HQ rendering was repaired, but no graphical result is claimed.
UAT-08/10 authority captures and a full multiplayer join remain pending. Existing
settlement fixtures isolate the separate WP-08 town initialization defect.
[Evidence](../docs/audit/2026-09-15/wp05/README.md).

**WP-06 automated update (16 September):** strict parser/transform admission and
atomic file storage are repaired locally.35 selected CTests pass, including actual
Import/Export query/OK callbacks and existing128 prefab variants. Six real I/O
failure points preserve old bytes/memory, survive reopen and permit retry.
UAT-04f/g graphical retest remains pending; capture and routes remain WP-07.
[Evidence](../docs/audit/2026-09-15/wp06/README.md).

**WP-07–09 automated update (16 September):** capture/naming/cancel, 312 native
signalled train routes over the revised catalogue, native populated colony
founding, HQ/hub/reserve controls and cargo persistence are implemented locally.
The cargo train now drives from its depot to the hub using native orders; all 60
units remain accounted for through unload, reserve-limited pickup and process
reload. Switching/re-clicking Blueprint and HQ/Hub tools is also repaired and tested.
All 363 CTests pass. Older empty outposts are preserved; automatic repair is
unsupported. UAT-04, 08, 10–12 and reload still need visual acceptance on the newer
build. [Evidence](../docs/audit/2026-09-16/wp07-09/README.md).

**WP-10 automated update (17 September):** honest conduit delivery and LandInfo allocation
telemetry are implemented and merged via PR #8. All 6 dedicated Catch2 unit/regression
tests pass (97 assertions in `src/tests/test_spaceports_and_conduits.cpp` under `[wp10]`),
verifying unserved extraction produces 0 allocated waiting cargo, served stations receive
honest allocation, multiple stations share extraction proportionally, and LandInfo UI
accurately reflects extraction status (`OST-EC-001` resolved). Visual human acceptance remains pending.

## Subsequent user gameplay report

The user reports: “OpenTTD no longer crashes. UAT seems to pass.” This follows the
requested UAT-00,04a–c and09 run and is recorded as a positive user-reported smoke
result. The assistant did not observe it; individual eight-prefab, transform,
obstruction and save/reload results were not separately supplied. Preserve that
scope rather than treating this as acceptance of unrelated UAT cases.

The environment block below describes the earlier assistant attempt; it does not
invalidate the user's later gameplay observation. WP-04 is subsequently repaired
locally as recorded above. Existing user-created save files must be preserved.

## Earlier assistant graphical attempt — blocked by environment

UAT-00,04a–c and09 could not be operated or visually checked: configured
`orca-ide` is missing, native GUI control is disabled, and the X display probe
cannot open `:0.0`. The SDL process stayed alive for10 seconds and was stopped;
that is not a graphical pass. Crash/M1 copies are prepared and hashes unchanged;
a fresh clear pad has not been prepared. [Attempt evidence](../docs/audit/2026-09-15/openspacettd-blueprint-gui-uat-s63nc900/README.md).

| Case | Result | Actual result / evidence / issue |
|---|---|---|
| 00 Main-window and Blueprint smoke | User-reported no crash / apparent pass | Later user gameplay report above; per-window detail not supplied |
| 01 Ownership and local train run | Not run | |
| 02 World navigation/biomes | Not run | |
| 03 Gate construction/linking/safeguards | Not run | |
| 04 Eight prefabs and player blueprints | Historical crash; current smoke reported passing | User confirmed Mainline Double Straight placement crash; OST-BP-001. Separate04a–g below; capture/export/routes now repaired locally, visual retest pending |
| 05 Spaceports/conduits | Automated pass ([wp10]); visual pending | WP-10 honest conduit delivery implemented (PR #8); 6 unit tests pass (97 assertions); LandInfo honest allocation UI operational; visual human UAT pending |
| 06 Megacities/corridor traffic | Not run | |
| 07 Ledger/governance UI | Not run | |
| 08 Colonisation/promotion/restrictions | Not run | |
| 09 Save/reload/crash regression | Included in general UAT report; not individually confirmed | User reports UAT seems to pass; no separate persistence observation supplied |
| 10 HQ/stockpiles/hubs | Not run visually on current build | WP-09 controls and cargo/reload checks pass automatically; player delivery and acceptance remain pending |
| 11 Fabrication | Not run | |
| 12 Research | Not run | |
| 13 Full Commonwealth content | Human check pending; structural branch automated | Separate WP-11 v1 fixture loads 13 cargoes/14 industries/12 trains; other pipeline behavior is not accepted |
| 14 Player production pipelines | P0 Blocked / P1 Not run | Need unhubbed Developed P0 fixture; M1 World2 is hubbed. Staged integration tests pass, moving player chain not observed; content depends13 |
| 15 Bespoke art | Blocked | Sprint 38 assets pending |
| 16 Independent-server federation | Blocked for natural route/recovery | Source-confirmed external entry rejection; WP-F1/F2 and exact F1 fixture needed. Manual dispatch is separate |

For each failure/blocker: case/substep, prerequisites, expected result, actual
result/error, reproducibility, evidence path and follow-up issue. For federation,
record natural departure, remote arrival, visible return/orders, mismatch,
obstruction, deduplication and restart independently. Do not aggregate a partial
run into one Pass.


## Preserved reported failure (not a new graphical audit run)

15 September2026 user confirmed the Mainline Double Straight placement crash:
`crash20260915082450` (local OpenTTD data directory), modified a6b79add build,
GS9/OpenGFX/no active NewGRFs, save367. Assertion command.cpp:365 and placement
stack; exact terrain contribution unknown. Original artifacts/hashes are in
[OST-BP-001](../docs/CRITICAL_BUG_REVIEW_2026-09-15.md). User's prior ownership
complaint is retained for01 retest; historical repair is not a current human pass.

## Per-subcase worksheet (copy for every attempted subcase)

- Test/subcase ID and purpose:
- Suite/fixture SHA256, build/ref+dirty diff/binary hash, save version:
- Content/base assets, company/world/station/train IDs and coordinates:
- Cash/materials/research, loading orders/service, initial date/counters:
- Exact clicks and simulation events/months elapsed:
- Expected result / actual result / exact error:
- Negative case and resulting state delta:
- Screenshot/video/log/save paths (local; no credentials):
- Result: Pass / Fail / Blocked / Not run:
- Defect ID, smallest missing prerequisite/check, reset completed:

Blueprint: record04a opening/selection/preview,04b transforms,04c placement/rejection,
04d routing,04e capture/naming,04f export/import,04g persistence separately.
Conduit: record nominal counter, waiting, onboard, accepted downstream, service,
rating where available, and monthly event. Production: distinguish input consumed,
output produced, waiting, stockpile and final useful delivery. Federation: separate
natural departure, arrival, full consist/cargo, orders/return, mismatch, obstruction,
duplicate/lost response, restart and older-save recovery; never aggregate partial pass.

## Automated audit evidence, kept separate

Current abbcd7e077 binary:67/67 selected CTests pass; copied M1 and Blueprint crash
save load in null-video1000-loop smokes. No GUI action was executed, so **no human
or graphical case was marked Pass**. See [check records](../docs/audit/2026-09-15/checks.json).

Production verification caveat: live-main CI fails three invalid-border fixtures;
local Release preprocessing disables assert in that test translation unit despite
WITH_ASSERT. Catch2 checks pass but do not establish that invariant. See
OST-TEST-001 before relying on this as production acceptance.

## WP-01 implementation verification — later follow-up

The rebuilt local game includes the Blueprint command repair. **78 distinct
selected automated tests pass**, including all 128 prefab transform/payment
combinations and a real infrastructure/money/material save/reload test. See
[WP-01 evidence](../docs/audit/2026-09-15/wp01/README.md) for exact commands,
fixture limitations and failed-before/fixed-after evidence.

Human cases 00/04/09 remain **Not run** on this build. The prior Mainline placement
failure stays historical. Retest on copied BC/M1 fixtures and a fresh pad; retain
screenshots and exact money/material changes. Stockpile-mode signal placement now
consumes materials, so record and supply the entire stamp's materials. No graphical pass
is inferred from the automated results.


## 2026-09-22 — CST prefab purchase-mode UAT blocker

The player identified `OpenSpaceTTD-All-Features-UAT-v1.0.sav` as the input.
The player reported that CST placement rejected missing materials and that the
suggested fabrication switch could not be found after building an HQ. Source
inspection confirms the switch already posts `SetFabricationMode`, but belongs
to Map → Corporate Headquarters & Stockpiles, separate from the ordinary company
HQ. The corporate window also put six tabs and six actions in one oversized row.
The UAT setup enables fabrication; a shortage in the placement world's stockpile
then correctly rejects construction. This is a control-discovery/layout and UAT
preflight defect, not a requirement to establish an HQ before cash construction.

The Blueprint Library now exposes the company purchase mode directly. The HQ
window separates tabs/actions and gives the same mode switch its own row. Both
show the current mode and use the existing authoritative command; changing it
redraws both windows. The shortage message gives the two exact control locations.
No saved mode is silently changed and no materials are granted or bypassed.

Automated verification: incremental build passed; 425 unit cases / 66,224
assertions and 436/436 CTests passed, including the new 56-assertion GUI
placement regression. File-description and unused-string linters and
`git diff --check` passed. Human retest pending;
the player-reported failure is recorded, not converted into a human pass.
Retest on a working save copy: open Blueprint Library, set **Purchase mode: Cash**,
place **CST Mainline Double Straight** on clear valid terrain, verify the whole
layout and cash charge, then save/reload. No HQ establishment is needed. For
fabrication UAT, enable in-kind mode and supply the **placement world's** stockpile.

The existing v1.0 working save can use the new control without migration. For a
fresh broad UAT run, use the documented All-Features v1.1 fixture, which repairs
v1.0 terminal ownership. This resource-control fix does not migrate ownership or
claim acceptance of the wider UAT suite.

Verified executable: `build/openttd`, SHA256
`61b340ba34fbef2290b13fad65cc0d3fc5a45c87a4ba54dc5c1a079c36ed2105`.
The new isolated GUI regression uses the null video driver and tests real widget
callbacks/commands, full track footprint, both one-way signals, exact cash charge,
unchanged empty stockpiles, spectator protection, shared HQ/library mode labels,
library width <=640 and HQ width <=1024 at the test's default UI scale. It does
not constitute a human screenshot or a full replay of the user's save. Save
metadata inspection with the rebuilt executable reads the supplied v1.0 fixture
as version 367 with no NewGRFs; the save itself was not modified.


## 2026-09-22 — UAT text readability

Blueprint details and status messages now wrap; related counts share a compact
line, with more room for descriptions. Corporate HQ Overview and Fabrication
use short, wrapped instructions and a compact materials list. Repeated discount
claims and nonessential explanatory text were removed. HQ status messages wrap.
This is a presentation-only change; construction and resource rules are unchanged.

Verification: incremental build, 425 unit cases / 66,224 assertions, 436/436
CTests, both repository linters and `git diff --check` passed. Human visual retest
pending: reopen Blueprint Library and HQ Overview/Fabrication, check readability
at the user's font/UI scale, and resize the windows. Very long imported text still
requires sufficient panel height; the wider HQ dashboard is not redesigned here.

## Connected economy UAT v2.0 — 2026-09-23

**Automated: PASS. Human visual acceptance: NOT RUN.**

- Artifact: [OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav](OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav).
- Start with the [short player checklist](CONNECTED-ECONOMY-UAT.md).
- 37 trains carry all required cargoes repeatedly; all eleven factories operate.
- Checkpoint: 32 houses / 872 people. After approximately 332 normal-speed seconds:
  35 houses / 905 people. After the 24-month soak: 42 houses / 906 people.
- Reload conserves observed state. Stopping food causes starvation and halts new
  houses; restarting restores growth. The negative test uses a separate copy.
- CST prefab: exactly 32 ballast, 18 steel and 2 wiring consumed. Research uses
  delivered feedstock and completes the selected project.
- 427 unit cases / 66,233 assertions; 438/438 CTests; both linters; content rebuild
  verification; legacy v2 WP-11 regression; whitespace and script syntax checks pass.
- [Machine evidence](OpenSpaceTTD-Connected-Economy-UAT-v2.0.evidence.json) and
  [full compressed run](OpenSpaceTTD-Connected-Economy-UAT-v2.0.run.json.gz).

The visual check could not run because `orca-ide` was unavailable. Earlier human
UAT gaps remain open; these automated results do not mark them accepted.

## 2026-09-23 — Connected UAT crash and undefined cargo text

Human UAT **failed** on the initial connected save: `GetPartialPixelZ` reached
`NOT_REACHED` at landscape.cpp:298 while moving the viewport. The preserved
crash save contained 742 tiles with adjacent corners differing by more than one
height level. Direct flattening had left discontinuities at the construction
band boundary. All 13 Commonwealth cargos also lacked their quantity/unit strings.

The generator now smooths exposed terrain; a deterministic, preflighted load
repair handles the marked legacy fixture without touching infrastructure or
changing cargo, trains, cash or towns. Industry v4 supplies complete text; the
unchanged v2/v3 binaries receive a pack-scoped text compatibility repair.

Verification: 429 unit cases / 66,359 assertions; 440/440 CTests; repository
linters, diff whitespace and reproducible GRF checks passed. The actual crash
save passed 3,139,587 pixel-height queries and 1,046,529 inverse viewport queries,
with zero invalid slopes, complete text for all 24 active cargos in English-US
and English, exact state across reload and continued cargo-conserving simulation.
The recovered copy retains 37 trains and the city with 1,107 people / 45 houses.
The original crash save is unchanged. Fresh demo acceptance and legacy v2 WP-11
acceptance passed. Human graphical retest remains **pending**.

[Recovery evidence](CONNECTED-UAT-RECOVERY.evidence.json) ·
[Reproduction](../docs/CONNECTED_ECONOMY_UAT.md#terrain-and-cargo-text-recovery-2026-09-23).

## 2026-09-23 — Savegame authoring workflow

Documented the terrain, cargo-text, logistics and recovery lessons in
[Savegame authoring](../docs/SAVEGAME_AUTHORING.md), with a required `AGENTS.md`
checklist. This documentation change adds no human acceptance claim or new save.

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

See [the implementation record](../docs/FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

## Federation freight acceptance follow-up — 27 September 2026

PR #40 is merged on main (`d886260607`). Follow-up work is on
`fix/federation-freight-recovery-acceptance`.

**Scheduled freight passes:** five automatic loaded outbound trips and empty
returns delivered 300 coal. Native production, unallocated cargo, station losses,
remaining stock and consumption reconcile exactly; all native cash changes also
reconcile. The two clients remain connected with no detected desync or duplicate.
The previous 600-second timeout was shorter than the native route's five-cycle
runtime. The harness now budgets startup plus time per requested cycle.

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

Machine evidence is recorded in `demo/FEDERATION-FREIGHT-RECOVERY.evidence.json`,
including the frozen binary hash, checkpoint hashes, accounting and continuation
provenance. Recovery uses only the latest coordinated set; arbitrary crash
recovery and old/mixed-save rollback are outside this acceptance boundary.

See [acceptance method and evidence](../docs/FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

## Player-built resource economy — 28 September 2026

Implementation: new-game default, basic starter industries, hidden primary sites,
paid company surveys, Materials II/III unlocks, industry placement, script API and
save/load are integrated. Existing saves/demo configs retain Classic rules.

Human UAT: **Pending**. Use a new Player-built game at the player's language/UI
scale. Open Industries → Fund new industry → Survey / discovered sites; check
quoted price and 16×16 preview, survey an empty and a productive area, inspect the
site list/blue and red outlines, build a basic primary and processor, research
Materials II/III, and build newly revealed resources. Confirm ordinary production,
transport, a saved/reloaded discovery, and another company's independent survey.
Check long industry/world names, resized windows and placement errors.

Automated verification: incremental build; **447 unit cases / 66,829 assertions**;
**458/458 CTests**; file-description and unused-string linters; whitespace checks.
Native fresh generation, two client map joins, cold reload, private command
replication, competing construction and the Classic WP-11 economy regression pass.
The resource fixture passes every service's repeated delivery, a 24-month soak,
conservation, reload, starvation/recovery, fabrication and research consumption.
Its genuine growth check starts from the cold map; it does not claim the original
showcase's separate three-houses-in-ten-minutes presentation target.
[Retained machine evidence](../docs/audit/2026-09-28/resource-economy/README.md). [Design and reproduction](../docs/PLAYER_BUILT_RESOURCE_ECONOMY.md).

## CST star map and gate expansion — 28 September 2026

Implementation: Mito–Merredin preset, existing public CST backbone, research
range checks, delivered-equipment gate projects, cancellation/refunds, native
arrival construction, access/tolls, connected-host directory, remote station
orders/gate pins, automatic multi-hop routing and client host visits.

Automated results: see the exact counts, hashes and test boundaries in
[the retained audit](../docs/audit/2026-09-28/stellar-network/README.md). The
generation runner uses a new map and cold reload. The multi-hop runner uses
three actual servers with joined SDL dummy clients and native freight. Native
window tests invoke the star-map project buttons through ordinary commands.

Native suite: **457 unit cases / 67,032 assertions; 469/469 CTests**.
Three-server freight and cold restart pass: six total round trips / 24 native
gate crossings, 360 coal consumed, conserved cargo and reconciled cash on all
hosts, preserved global train identity and no detected client desync.

Human graphical UAT: **Pending**, including seamless Visit, camera restoration,
remote train follow, UI scaling, equipment loading/refits and commissioning during
an authority interruption. Use [the player checklist](CST-STELLAR-UAT.md). Earlier
overall user acceptance is preserved but does not imply acceptance of this new UI.

## Agent workflow tooling — 30 September 2026

No game source/content/save changes in this tooling delivery; no new human game
acceptance is claimed. Codex CLI skill discovery is verified; Antigravity discovery and interactive use
remain Pending.
[Setup and verification](../docs/AGENT_WORKFLOW.md).
