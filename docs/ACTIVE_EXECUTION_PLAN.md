# Owner decision, 2026-10-03: unlimited offline development money

## Vale Railway publication — source and automated proof

The optional Aster Vale founding scenario is ready for draft review, stacked on
PR83. It adds a native persistent objective observer, pure GameScript queries,
empty-stock authored assistance and isolated bounded preparation/proof tools.
The freight purchase list now accepts physically compatible unpowered wagons;
locomotive power checks remain. Native LORE and purchase-test fixtures repair
base-graphics and stale signal-buffer prerequisites without weakening assertions.

Prior local proof passed preparation/service/fresh reload at 0/9/4 evaluations and
actual SDL purchase of one empty 40 BALL wagon for93GBP with zero added evaluations.
Publication integration and exact-head CI are separately recorded. Human desktop
comprehension, enjoyment and pacing remain pending; inherited modular car artwork
is cosmetic and unchanged. The broader Reserve/FOOD pilot is not accepted.
[Scenario guide](VALE_RAILWAY.md) and [sanitised engineering evidence](audit/2026-10-12/vale-railway-publication/README.md).


## Active execution slice — One Material-Funded Research Unlock: P1 (11 October 2026)

Owner authorized the "One Material-Funded Research Unlock" (Card P1) execution slice from `/home/flax/Documents/Codex/2026-10-11/task/feature-delivery/OPENSPACETTD-FEATURE-DELIVERY-PLAN.md` on branch `codex/capacity-or-reserve-pilot` following C0/C1 delivery and preflight approval:
- **Principle**: The railway physically manufactures a capability the player can point to: real delivered materials complete Materials III and expose an existing advanced resource opportunity.
- **Ruleset**: Opt-in experimental V2 rules (`EconomyRulesVersion::V2`) on the connected 4-world fixture. V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope & Implementation**:
  - **Corporate HQ & Baseline Prerequisites**: Corporate HQ founded on World 0 (`TileXY(70, 60)`, £2.5M capital expenditure). Baseline Materials I (100 RP) authored; baseline Materials II (250 RP, £250,000 cash budget) unlocked via server research commands, unlocking legal extraction of Copper (`COPR`) and Silica (`SAND`), plus recipes for Wire (`RECIPE_COPPER_SMELTING`) and Chips (`RECIPE_SILICON_ARC`).
  - **Feedstock Acceleration**: Explicitly turned OFF (`CmdSetResearchAcceleration(false)`) so delivered `CHIP` cannot be burned for monthly RP.
  - **Physical Kit Freight Delivery**:
    * Iron mine (14) -> Steel furnace (5) -> HQ warehouse (0): 40 `STEL`.
    * Copper mine (15) -> Copper smelter (7): `COPR -> WIRE`.
    * Silica dunes (16) -> Silicon works (8): `SAND`.
    * Copper smelter (7) -> Silicon works (8): `WIRE`.
    * Silicon works (8) -> HQ warehouse (0): `2 SAND + 1 WIRE -> 1 CHIP` (20 `CHIP`).
    * Both cargos physically delivered and unloaded at Stop 0 into the World 0 company stockpile.
  - **Negative Paths & Atomic Escrow**:
    * Calling `PrepareResearch(CompanyID{0}, TECH_MATERIALS_3)` when stock < 40 STEL or < 20 CHIP fails/rejected.
    * With kit present, selecting `TECH_MATERIALS_3` reserves 40 STEL + 20 CHIP into escrow (`research[company].reserved`).
    * Cancelling/switching project restores 40 STEL + 20 CHIP back into World 0 stockpile without loss or duplication.
  - **Unlock & Site Visibility**:
    * Funding 600 RP (£600,000 cash budget) consumes the escrowed kit via `FinishResearch()`, unlocks `TECH_MATERIALS_3`.
    * Surveyed 16x16 Rare Earth site on World 2 at Node 17 changes from undiscovered (`Discovered() == false`) to discoverable (`Discovered() == true`).
  - **Cold Reload Equality**: Fresh-process reload at completion checkpoint, 4 subsequent monthly evaluations, verifying identical state, persistent unlock, persistent site discovery, and 100% cargo conservation.
- **Execution Budget & Non-Goals**:
  - Run capped at 36 actual economy evaluations or 45 execution minutes; reload probe capped at 4 evaluations/5 minutes.
  - No new recipe, city HyperGrowth basket, Materials IV, maglev, full A–D campaign, normal-new-game rollout, price tuning or federation.
  - Delivery boundary: Local review-ready delivery on `codex/capacity-or-reserve-pilot`. No remote push, no PR creation, no merge.

## Active execution slice — Freight You Can Read: C1 (11 October 2026)

Owner authorized the "Freight You Can Read" (Card C1) execution slice from `/home/flax/Documents/Codex/2026-10-11/task/feature-delivery/OPENSPACETTD-FEATURE-DELIVERY-PLAN.md` on branch `codex/capacity-or-reserve-pilot` following C0 pilot correction and acceptance:
- **Principle**: Clear, honest, and actionable understanding of owned industrial facilities and physical logistics directly within the existing game UI.
- **Ruleset**: Applied to the existing Facilities window (`empire_facilities_gui.cpp`) across all modes, with enhanced V2 works integration and strict preservation of legacy/V1 fallbacks.
- **Scope & Implementation**:
  - **Active Recipe & Units**: Displays recipe name, exact input/output cargo ratios (e.g. `2 Silica Sand -> 2 Trackbed Ballast`), rated throughput (`Rated: X batches/mo`), and observed completed-period production (`Last mo: Y batches` or `[Unavailable]`). Eliminates misleading `t/mo` units in integrated modes.
  - **First Limiting Bottleneck**: Diagnoses and displays the first limiting constraint on the plant using authoritative rules: technology locked (`[Locked: Requires ...]`), blocked output (`[Blocked: Output <Cargo> Full (<amt>/<cap>)]`), missing input (`[Starved: Missing <Cargo> (0/<req> per batch)]`), partial input (`[Starved: Insufficient <Cargo> (<amt>/<req> for batch)]`), operational production (`[Active: <last>/<cap> batches/mo]`), or buffered ready (`[Ready: <batches> batches buffered]`).
  - **Factory Buffers vs World Stockpile**: Explicitly distinguishes machine input/output buffers from company planetary construction stockpiles. Shows exact buffer quantities, capacities, and complete batches permitted (`permits X batches`) without speculative forecasting.
  - **Works Effects & Queried Cost**: In V2 rules, displays works bill (`160 STEL + 80 BALL`), queried command cost via `DoCommandFlag::Query` (£50,000), active Capacity (+50% throughput, 150 batches/mo) or Reserve (2x input buffer, 600 batch storage) effects, and local planetary warehouse stock.
  - **Role vs Phase Separation**: Queries and displays world economic role (`Core`, `Industrial`, `Frontier`) distinctly from development phase (`Phase 1 Core`, `Phase 2 Developed`, `Phase 3 Frontier`, `Phase 4 Expansion`).
  - **Asset Navigation & Controls**: Interactive `[Locate Plant]` (centers on factory tile `f.tile`) and `[Locate Station]` / `[Locate Hub]` (centers on station `st->xy` or planetary logistics hub `hub.tile`). Preserves V2 commissioning buttons (`[Capacity]`, `[Reserve]`, `[Retire]`) with foreign/spectator disablement.
  - **Query Purity Contract**: Opening, painting, resizing, tooltips, and navigation clicks strictly execute pure reads; zero simulation commands dispatched and zero game state mutated.
- **Verification & Non-Goals**:
  - No new telemetry database, notification system, dispatch AI, timetable optimizer, or economic balance modifications.
  - Unit & GUI regressions in `src/tests/test_sprint43_empire_facilities.cpp`.
  - Delivery boundary: Local review-ready patch on `codex/capacity-or-reserve-pilot`. No remote push, no PR creation, no merge, and no starting P1.

## Commonwealth Pilot Correction and Acceptance Supplement (11 October 2026)

Owner authorized the "Commonwealth Pilot Correction and Acceptance" task on the PR #82 stack (`codex/capacity-or-reserve-pilot`, reviewed checkpoint `20ede820d0b9ac1670dd1ed3606df68d50073d1b`), resolving the five correction packages from `/home/flax/Documents/Codex/2026-10-10/task-4/OpenSpaceTTD-Sprint1-five-issue-takeover-plan-20261011.md` without reverting Sprint 2 or 3 features:

### 1. Corrected Packages on Current Source
- **Package 1 (Food Service Retirement & Lifecycle Resets)**:
  - Distinct company deletion vs acquisition vs owner Off handling in `IntegratedEconomy::ChangeCompany`. Deletion resets `TownFoodService` state (wiping history, cursor, and fractional remainder) while preserving town-level food reserves; acquisition transfers ownership cleanly.
  - Recommissioning uncommissioned or retired slots (`CmdSetTownFoodServiceRate`) resets to baseline (four zero periods).
  - Foreign company commands fail with `STR_ERROR_OWNED_BY_ANOTHER_COMPANY`; spectators fail with `CMD_ERROR`.
- **Package 2 (Strict V2 Save Invariants)**:
  - Strict validation in `IntegratedEconomy::Load`: enforces exact `(work, throughput, input_cap, output_cap)` tuples for None `(0, 100, 300, 300)`, Capacity `(1, 150, 300, 300)`, and Reserve `(2, 100, 600, 300)`.
  - Rates strictly restricted to `{0, 10, 20, 40}`; market ceiling 40; receiver capacity 60; history bounds `0 <= consumed[i] <= requested[i] <= 40`; history cursor 0–3; remainder 0–639.
  - Invalid owner inactive defaults supported; neutral factory ownership preserved.
  - Transactional rollback on parse or invariant failure.
  - Cross-reference and Core world role validation in `IntegratedEconomy::ValidateAfterLoad`. Bounded passenger bonus (+25% max); unaffected mail generation.
- **Package 3 (Reconciled GUI Controls & Feedback)**:
  - `megacity_gui.cpp`: Button-only hit testing for rate cycling; disabled styling for spectator and foreign company contexts; chronological history display with fraction met; explicit received vs consumed displays; evaluation boundary (`Evaluates 1st of month`) noted.
  - `empire_facilities_gui.cpp`: `Upgrade All` button visibly disabled in V2; displays explicit works bill (`£50k + 160 STEL/80 BALL`), throughput and capacities (`Cap: .../mo | Buffers: ... in, ... out`), and local stockpile stock.
- **Package 4 (Truthful Runner & Offline Guards)**:
  - `scripts/verify_commonwealth_works.py`: Reconciled advance count with monthly evaluation events using a monotonic identifier; records V2 service history (`food_rate`, `food_consumed_history`, `food_requested_history`, `food_reserve`, `food_delivered_last`, `evaluations_observed`); validates £50k commissioning cash debit; retains partial/error reports on failure in cleanup path.
  - `scripts/test_verify_runner_guards.py`: 7/7 offline unit tests passing in 0.001s with zero engine processes launched.
- **Package 5 (Physical Pilot Preparation)**:
  - In `src/portal/connected_economy.cpp`, removed direct `StockpileManager::AddCargo` seed of 160 STEL / 80 BALL; routed TrainRoute 2 (BALL) and 7 (STEL) to Industrial warehouse Stop(30) in World 1.
  - Exposed V2 service fields in `Snapshot().cities` and `advance` audit output (`city_months`).

### 2. Truthful Acceptance Checks for Sprints 2 & 3
- Added Catch2 automated test cases in `src/tests/test_commonwealth_works.cpp` verifying:
  - **Sprint 2**: Material debit, legal road frontage requirement, house shell construction at stage 0, tile progression through native ticks to `TOWN_HOUSE_COMPLETED`, population credit (+232 citizens), and permanent demolition immunity (`IsHousePlayerProtected`) under food service shutoff.
  - **Sprint 3**: Commissioning of alternate Silica extraction on World 3, direct portal delivery to Ballast Crusher in World 1, BALL production from alternate supply, and continuous BALL output during primary corridor feeder interruption.

### 3. Historical Headless Evidence & Consumed Budget
- The two historical headless runs in `docs/audit/2026-10-11/capacity-or-reserve/` are raw 22-calendar-month advance/reload/conservation traces with incomplete pilot acceptance (they did not record V2 service history or count evaluation boundaries). Their initial simulation run budget was consumed and is not silently renewed.

### 4. Proposed Reserve Execution Envelope (Awaiting Owner Approval)
- **Recommendation**: Reserve Works pilot execution (validating 600-batch buffer, transported commissioning kit, real food consumption/passengers, and cold equality).
- **Proposed Run Envelope**:
  - Run 1 (Main Simulation): 1 process, exactly 24 monthly evaluations (max 30 execution minutes).
  - Run 2 (Reload Probe): 1 reload process, exactly 4 monthly evaluations (max 5 execution minutes).
- **Stop Condition**: Immediate halt and artifact retention on first defect or invariant violation.
- **Approval Gate**: Owner approved on 20 October 2026; the single run was executed and is
  closed. Integrity PASSED (24 + 4 evaluations, conservation, exact kit debit, cold reload).
  The Reserve buffer was never used in this fixture. See the
  [run record](audit/2026-10-20/reserve-pilot/README.md); no further runs are authorized.

## Active execution handoff — A World Worth Connecting: Sprint 3 (11 October 2026)

Owner approved the third candidate slice "A World Worth Connecting" from `ANTIGRAVITY-PLAN-AND-PROMPTS.md` and sections 4, 5, and 7 of `OPENSPACETTD-DESIGN-STUDY.md` on branch `codex/capacity-or-reserve-pilot`:
- **Principle**: Purposeful inter-world expansion providing supply diversification, topological bottleneck independence, and demonstrable industrial output resilience.
- **Ruleset**: Opt-in experimental v2 rules for fresh disposable fixtures only. V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope & Implementation**:
  - **Existing Consumer**: Player-owned bootstrap Ballast Crusher at Node 4 (`TileXY(300, 80)`, World 1 / Industrial Commonwealth) consuming raw SILC to produce BALL.
  - **Original Source & Corridor**: Silica Dunes at Node 13 (`TileXY(650, 80)`, World 2 / Extraction Frontier) delivered via Train 1 across the World 2 ↔ World 1 portal throat on row 82 (`TileXY(534, 82)` ↔ `TileXY(476, 82)`). Track length: 294 tiles.
  - **Alternative Raw Resource Origin**: A new Silica Dunes extraction facility on World 3 (`Research frontier`, `WorldID{3}`, Phase 3 Frontier), discovered via paid company area surveying (`Commands::SurveyResources`), funded via native `Commands::BuildIndustry`.
  - **Independent Wormhole Link**: A direct portal pair constructed via `Commands::BuildPortalPair` directly between World 3 (`TileXY(790, 96)`) and World 1 (`TileXY(476, 96)`), connecting to a new platform at Ballast Crusher (Stop 4) on row 96. Track length: 248 tiles (~16% shorter route).
  - **Bottleneck Independence**: The World 3 connection completely bypasses World 2 and does **NOT** share the World 2 ↔ World 1 portal throat on row 82. When the primary corridor is deliberately closed, World 3 maintains deliveries without interruption.
- **Verification & Bounds**:
  - Measure route timing, transit distance, and round-trip cycles between World 2 and World 3 origins.
  - Deliver native before/after operating sample: baseline operation (periods 1–6), expansion connected (period 7), dual-source operation (periods 8–12), deliberate closure of original corridor (periods 13–18), reopening/recovery (periods 19–24).
  - Demonstrate sustained Ballast Crusher production during closure of the original approach.
  - Fresh-process cold reload at period 24 and 4 post-reload periods with 100% cargo conservation.
  - Real costs and treasury accounting: surveying, industry funding, portal pair construction, track laying, depot, and train purchase.
- **Delivery Boundary**: Local review-ready delivery on `codex/capacity-or-reserve-pilot`. No push, no PR creation, no merge, no Sprint 4 execution. Hand off to Codex/Astra for review.

## Active execution handoff — A District That Stays Built: Sprint 2 (11 October 2026)

Owner accepted Sprint 1 "Capacity or Reserve" pilot evidence and approved the residential slice "A District That Stays Built" from `ANTIGRAVITY-PLAN-AND-PROMPTS.md` on branch `codex/capacity-or-reserve-pilot`:
- **Principle**: Finite native residential works, material-funded capital additions, and durable development that stays built through supply interruption.
- **Ruleset**: Opt-in experimental v2 rules for fresh disposable fixtures only. V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope & Implementation**:
  - One non-repeatable pilot residential work for one pilot Core town (`TownID{0}`, Commonwealth Metropolis).
  - Material kit: `160 STEL + 80 BALL` and £50,000 cash debited atomically from company world stockpile via replicated command `Commands::CommissionTownResidentialDistrict`.
  - Grants a finite budget of 4 native building opportunities.
  - Native building opportunities locate legal road-adjacent building sites (`CanBuildHouseHere` and `RoadTypesAllowHouseHere`) and construct native house shells with `is_protected = true`.
  - House shells start at construction stage 0 and progress through native tile ticks to completion; native population is added by `AdvanceSingleHouseConstruction` upon reaching `TOWN_HOUSE_COMPLETED`. Zero phantom or clamped population.
  - Native protection bit (`IsHousePlayerProtected`) permanently shields completed buildings from OpenTTD's native random redevelopment demolition loop (`CanDeleteHouse`).
  - Blocked physical access (lack of valid road frontage or terrain obstruction) is clearly reported in UI/status ("Awaiting site access") without burning materials or fabricating houses.
  - Optional Commercial FOOD service is decoupled: neither unlocks residential construction nor revokes committed or completed development during service interruption.
- **Verification & Evidence**:
  - Status: **PASSED (Execution Completed)**
  - Automated Unit Tests: Catch2 suite `[commonwealth-works]` (202 assertions passed across 8 test cases); full suite 307,537 assertions across 538 cases passed.
  - CTest Suite: 553/553 tests passed (100%).
  - Repo Linters: `file-descriptions.py` OK, `unused-strings.py` OK, `git diff --check` clean.
  - Headless Simulation Artifacts (`docs/audit/2026-10-11/residential-district/`):
    - Binary SHA256: `5cc4d2a5fe897887941c807e9f7b32ac7a171e06c916f38f8994c48faf2d5365`
    - Cold Start Save SHA256: `cc2a94b4aa65132bf279ff6f4ac02bc1a79787302e3d2c9a4f33c353af684e20`
    - Checkpoint-24 Save SHA256: `8987cb9c9c746071cedc485d4023010c0189e44fe70d6881a7fe4408d329fb26`
    - Cold Reload Equality: Verified 100% identical state across tick, date, money, held, stocks, facilities, economy.
    - 4 post-reload periods passed with zero cargo conservation errors.
  - Mechanics Verified in Headless Run:
    - Atomically debited £50,000 cash and 160 STEL + 80 BALL materials.
    - Initial period 1: 4 opportunities, 0 placed, 0 completed.
    - Periods 2..5: 4 native house shells placed adjacent to road frontage; opportunities decremented 4 -> 0; shells started at stage 0 with population unaltered.
    - Period 8: All 4 houses reached native completion (`TOWN_HOUSE_COMPLETED`); completed reached 4; town population rose from 904 to 1136 (+232 native population).
    - Periods 14..18: Commercial food service completely shut off; completed houses remained strictly at 4 and were never demolished.
    - Period 18: Food service restored.
- **Delivery Boundary**: Local review-ready delivery on `codex/capacity-or-reserve-pilot`. No push, no PR creation, no merge, no Sprint 3 execution. Stopped at agreed boundary for review.


## Active execution handoff — Capacity or Reserve Prototype: Sprint 1 (11 October 2026)

Owner authorized the experimental "Capacity or Reserve" prototype from `ANTIGRAVITY-PLAN-AND-PROMPTS.md` on branch `codex/capacity-or-reserve-pilot`:
- **Principle**: Stable settlements, optional FOOD civic service, and a factory capacity-versus-reserve choice.
- **Ruleset**: Opt-in experimental v2 rules for fresh disposable fixtures only (`connected_economy prepare-works` / `--works-prototype`). V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope**:
  - One mature Core town with Stable civic baseline (no supply-driven growth, no starvation lockout).
  - One optional FOOD service slot per town: rate Off / 10 / 20 / 40, receiver cap 60, market ceiling 40, four-period sliding useful consumption history yielding up to +25% passenger generation bonus.
  - One owned bootstrap ballast plant (`CompanyID{0}`) with one-time commissioning choice between Capacity Works (speed 100 -> 150 batches/period, storage 300) and Reserve Works (speed 100, storage 300 -> 600 batch-equiv), costing `160 STEL + 80 BALL` and £50,000 cash debit.
  - V1 generic +50 upgrade bypass blocked under v2.
  - Managed town cargo routing in `economy.cpp` separating managed destination status from active demand, preventing rate-Off FOOD and non-demanded commodities from generic absorption.
- **Budget & Bounds**: One fresh fixture generation, one bounded run up to 24 actual economy periods or 30 execution minutes, one fresh-process cold reload and up to 4 further periods. Stop at first integrity/command defect. Real transported physical cargo; no calculated growth claims.
- **Delivery Boundary**: Local review-ready delivery only. No push, no PR creation, no merge, no Sprint 2 execution. Hand off to Codex for review.

## Active execution handoff — Core Basket Repeat and Cold Proof (10 October 2026)

This replaces the expired campaign permissions with an owner-authorized finite continuation:
- **Objective**: Execute the Core Town Station Adapter delivered in PR #75 (`dddd49f18d`)
  to retarget Train 4 FOOD delivery to the nearer Core station (tiles 795,62–796,62 via connector 794,62).
  Prove second atomic monthly basket consumption (60 FOOD + 20 STEL + 20 BALL), observe native town growth hook
  and evaluate actual house/population response, and prove cold-reload persistence without state corruption.
- **Starting Point**: Copied paused checkpoint save at tick 304,384, SHA256
  `89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f`.
- **Bounds**: Exactly 1 simulation advance (up to next monthly tick boundary ~307,200).
  Immediate stop at first command or integrity failure.
- **Economic Mode**: Preserves offline `infinite_money` mode for functional validation;
  financial profitability and ordinary-start balancing remain deferred.

## Synchronization status — 10 October 2026

This file retains the historical First Core Construction Basket plan and consumed
attempts below. Its October 3 clocks and load permissions are closed. The owner's
current request is checkpoint synchronization; it supplies no new simulation
clock, load budget or source-save campaign. No historical clock is being reset.

Later October 5 evidence proves one full consuming basket, superseding older
unproved-outcome prose below for that specific result. See
[the checkpoint alignment](audit/2026-10-10/checkpoint-alignment/README.md).
The existing [station-adapter source handoff](handoffs/CORE_TOWN_STATION_ADAPTER.md)
remains the next bounded gameplay review. A subsequent repeat/cold campaign needs
an agreed replacement here with exact checkpoint, delivered helper and finite
load/tick/wall bounds. This status update does not create that next plan.

For the current First Core Construction Basket development proof, the owner
explicitly deferred financial balance and authorized unlimited virtual money.
The former £600,000 quote, £750,000 aggregate gross-spend and cash-reserve
ceilings below are superseded as acceptance gates. Preserve their historical
failed evidence and count all actual native receipts/debits separately from the
earlier £6,000,000 assistance. Enable the game's explicit `infinite_money`
setting only in this offline proof after loading the pinned save; record the
setting in evidence. It changes affordability, not physical cargo or receipts.
Do not change normal new-game or server defaults, inject cargo or research, or
claim ordinary-start economic proof.

The one bounded recovery starts from the pinned final source save and retains
the failed first attempt's one requested advance, 0 actual ticks, search and
spending history. The immutable outer stop is **2026-10-03 02:13:49.320471 UTC**;
the original initial-phase stop is 01:28:49.320471 UTC. Initial/cold advance
ceilings, search/topology constraints, first-hard-defect stop, resource
conservation, recipes, logistics and cold equality remain binding. No clock or
count reset, automatic second replay, merge or next slice is authorized.

That single recovery load stopped before planning or a new tick: the
money-setting response omitted the monthly audit field required by the runner.
The response shape has a narrow source correction, but that correction is not
authority for another game load. Preserve both stopped attempts. The requested
functional material-basket outcome is still unproven.

The subsequent explicit handoff authorizes **one** further source-save load
after focused full response-contract checks. It assigns the initial phase the
remaining time through the unchanged outer stop
**2026-10-03 02:13:49.320471 UTC**; this supersedes the earlier initial-phase
stop only. Retain the original requested advance and all earlier attempts.
No fresh grant, extra tick, silent replay after gameplay starts, or change to
the resource and authority rules follows from this timing allocation.

That one further replay ran two full advances and stopped at its first wrapper
integrity assertion. A neutral unbuilt candidate tile naturally changed from
clear to trees; native command, cash and all-cargo conservation remained sound.
The observation guard now accepts only that scoped vegetation change and still
rejects altered rail, signals, ownership or terrain. The captured observations
pass the repaired guard offline, but gameplay after the stop and cold proof are
**NOT RUN**. No additional replay follows from this repair.

The next explicit handoff authorizes one more replay from the pinned source save
with the unused-candidate vegetation guard tightened and proven against both
retained snapshots. Retain all **3 requested advances / 4,096 actual ticks**
and both earlier paid attempts in aggregate evidence. The initial phase now
shares the original 02:13:49.320471 UTC outer stop; the total advance cap is
unchanged. Save a normal paused checkpoint after construction and at each safe
advance boundary. On a wrapper assertion, attempt a diagnostic save only after
independent native cash/cargo and full route-state verification; never save an
unsafe state or treat a diagnostic file as continuation authority. Stop at the
first real gameplay defect or the original outer deadline.

That checkpointed replay stopped after multiple native advances at a
wrapper-only world equality check: Industrial world development increased after
a paid IRON delivery and normal steel conversion, while role, phase, geometry
and ownership remained fixed. The revised observer compares immutable world
fields and records development as a live resource outcome; the retained final
native snapshot passes the focused check offline. The last safe normal save is
the advance-014 checkpoint, one advance before the stopped result. No cold
continuation or further replay is authorized by this repair.

The latest explicit handoff authorizes one guarded continuation from the last
verified advance-014 paused checkpoint, **not** another source-save build.
Restore the transient adapter session only after full paused state, physical
cargo, paid infrastructure, research, services, immutable world identity and
provenance agree. Retain **15 cumulative requested advances / 28,672 executed
ticks** across attempts; the checkpoint predates the failed last advance, whose
command and result remain separate evidence. The unchanged 02:13:49.320471 UTC
outer stop and advance ceilings remain binding. Financial affordability is
deferred under the explicit offline development-money mode.

The authorized checkpoint load emitted its startup marker but could not open
the native FIFO: the older functional adapter rejected the checkpoint's saved
development-money setting before its bridge branch. No resumed-state comparison
or gameplay command ran. A narrow bridge-only guard correction is committed but
the failed load remains consumed; it is not automatic retry authority.

The one final bridge-corrected load reached paused equality and stopped before
arming: the persisted state matched, but eight wagon cached maximum-speed
observer values were zero after reload rather than their prior derived values.
All six front-engine speed values matched. The observer now reports the stable
front speed and zero for inert wagon cache fields; the original observation is
retained separately. This correction is unrun in game and supplies no further
load authority before the original deadline.

# Active execution plan: First Core Construction Basket

Owner-authorized execution handoff, 2 October 2026. This is the single active
implementation slice. Use the final already-funded checkpoint; new assistance is
£0. The prior campaign, grant, deadlines, failures and evidence remain closed and
unchanged. The owner approved the reviewed recommendation and delegated routine
bounded implementation choices. No further routine confirmation is required.

The latest timing instruction supersedes the proposal's first-launch clock:
the NEW 75-minute simulation clock starts at the FIRST native simulation advance,
after plan/source/preflight preparation. Record actual UTC start and immutable
start+75-minute stop immediately before that advance. Planning/compiler builds
and paused preflight preparation do not consume that clock. Never reset it.

## Live delivery and what is established

Read-only GitHub/ref checks in this reassessment show:

- Main: `85c423b4a2dd57e5a2c2cf1f6d29bc2e4667845d`.
- PR66 is open/draft, targeting main, exact head
  `7bc29a60cc1bc60b4d555d7500c2ded79c175eaa`; all seven exact-head workflows passed.
- That dependency contains main/PR67, original Money compiler fix `d1bb6176`,
  reviewed PR69 `3630d490` and preserved owner merge `eead18f1`. Main is an ancestor.
- The isolated implementation branch is `codex/first-core-construction-basket`,
  based on that refreshed PR66 head and targeting `codex/generation-money-comparison`
  while it remains unmerged. No merge or public-history rewrite is authorized.
- Preserve PR68, original worktrees, ordinary-money failures and every old archive.

| Acceptance dimension | Evidence and limit |
| --- | --- |
| Existing systems | Generation, commands, native processing/demand, research and save handling exist. PR69 repairs replicated designation, query mutation, startup/snapshot support and integrated post-load growth caches. Its exact-head CI and independent review passed. |
| Assisted functional outcome | PASS across preserved authorized handoffs: paid grain/FOOD services, three initial consuming months, interruption/recovery, normal paid HQ/I, II eligibility, exact cold retention and further paid FOOD plus consumption. The last consuming month used carried stock before the new arrival. |
| Ordinary economics | Unproven for this company. The original revised A1 loss remains failed evidence. The HQ/research were cash-assisted; FOOD did not unlock research or prove it funded the HQ. |
| Fresh repeatability | NOT RUN. Cold equality and source/unit checks do not substitute for an independent fresh replay. |
| Human acceptance / balance | Human UAT Pending. Pacing/profitability and uninterrupted supply are deferred. This proposal neither waits for profitable operations nor labels them passed. |

Source of the retained outcome: [continuation manifest](https://github.com/flaxos/openspacettd/blob/3630d490304916eb16c812b69e0aed92bfdd0f96/docs/audit/2026-10-01/functional-core/continuation-manifest.json)
and [raw archive](https://github.com/flaxos/openspacettd/blob/3630d490304916eb16c812b69e0aed92bfdd0f96/docs/audit/2026-10-01/functional-core/continuation-proof.tar.gz).

## Dependency decision

Recommend one smaller extension through existing material systems: **deliver and
consume the Core construction basket, with retained growth state after reload**.
This follows the roadmap's A3 functional-continuity direction before earned A2.

Materials II is an eligible, cash-only 250 RP project (£250,000 at current rules)
and would expose copper/silica and electronics. It is not a dependency for steel
or ballast. The retained world has no copper/silica producer and no wiring,
chip, signal or machinery factory. Making II into a physical freight opportunity
would add surveying, resource extraction, factory funding and destination work
whose legal sites and prices are not established here. Research-only completion
would prove another paid unlock but leave physical material use untested.

Steel-only delivery would stop at inventory. The city consumes FOOD first, then
STEL and BALL atomically; steel alone cannot provide expansion. A further FOOD-only
run would add continuity evidence but is not a prerequisite for this material
extension. Do not reopen the completed FOOD finish line or require profitable
FOOD before continuing functional work.

The proposed material chain uses neutral generated factories. Their production
does not enforce a company research gate (`Produce` checks that gate only for a
valid company owner). Consequently this proves physical use of existing steel
and ballast systems, not that Materials I newly enabled these neutral factories.
Company-funded factories and Materials II remain separate future opportunities.

## Starting state and changed assumptions

Use a new working copy of `campaign/owner-observation.sav` from the archive:

- Save SHA256: `206acdd9485bed64927197af89e07a70926e0b1e386ed51cb93744df2c78e2f7`.
- Tick 167168; cash £3,705,864; debt £100,000; Materials I (301) retained;
  active project/RP/budget zero. Existing grain and FOOD trains/orders retained.
- Core town 0, population 1,191 in the snapshot, FOOD 120, STEL 0/BALL 0, growth 0,
  passenger multiplier 0.5. Current demand is FOOD 60/STEL 20/BALL 20; recompute
  demand from live population at each observed month, never hard-code the result.
- Existing industries: iron source 6 at (600,335), world 2; steel processor 7 at
  (597,222), world 1; stone source 2 at (816,518), world 3; ballast processor 4 at
  (819,237), world 1. Processors have zero completed material batches in the
  snapshot. Public gates 2 (world 2↔1),3 (world 3↔1),1 (world 1↔0) exist.
- Station 3 catches town 0's own house at (794,66), has no warehouse and is the
  existing Core receiver. Its actual acceptance/capacity and any shared junction
  must be revalidated before paying for a material route. Existence is not a
  legal route quote or capacity proof.
- Keep canonical profile, exact published GRFs, optional art disabled, existing
  world roles/development and ownership. No save conversion or new generation.

The available £3.7m is inherited assisted cash. Old unused ticks and the numerical
£1,233,228 gross-spend headroom are historical bookkeeping, not current authority.
Retain the old counters: 82 initial advances (including25 control advances) and
24 cold advances. Report new advances separately and as an aggregate
with those106; no historical counter is reset or relabelled.
No cash, cargo, technology, RP, population or route may be authored. No new loan.

## Authorized implementation scope

One town, the four named industries and at most four new cargo services:
IRON6→steel7; STEL7→Core0; SILC2→ballast4; BALL4→Core0. Use the existing public
backbone and legal private branches/signals. Retain the existing FOOD service;
normal orders/refits/start-stop are permitted, with no added FOOD fleet or
automatic strategy/fleet tuning. No new factory, warehouse, survey, resource
funding, gate, research project, colony or material policy.

Current proof builders cannot simply be rerun: `functional-plan/build` is fixed
to fresh GRAI/FOOD construction; `first-freight-build` also rejects an existing
fleet and its funding helper may borrow. Implement only a separately guarded
loaded-checkpoint paid-command adapter, reusing narrow planner/quote primitives.
Verify the source save hash and complete frozen starting state before arming it.
Keep the original pristine/grant/ordinary-start guards and command flags intact;
the adapter must have no grant, loan or research operation. Native previews and
commands still decide every construction, ownership and payment result.

Explicitly handle existing owned track, shared/private junction footprints and
duplicate quotes without replacing neutral infrastructure or hiding failed
commands. Freeze one complete affordable layout/consist/order plan before spending.
If legal reuse cannot be established within the binding caps, stop with the
failed quote rather than expand into network/pathfinder redesign.

The existing `PlanFreightLeg` predecessor map resets for each station candidate.
The new adapter must carry one shared 30,000-state counter across all candidates
within an endpoint-planner invocation, stop before exceeding it, and count failed
searches too. Reusing the old per-candidate counter unchanged would violate this
plan. Keep the old helper's caller defaults intact; test the new budget's
exhaustion behavior without changing simulation rules.

Add only necessary read-only evidence for SILC/BALL labels, all-cargo custody,
per-month town consumption and native growth eligibility. Existing monthly audit
events name FOOD only; record the full basket at the actual monthly evaluation
so coarse tick sampling cannot invent simultaneous consumption. Record the town's
native growth rate/state as well as the integrated multiplier. Preserve native
processing, demand, prices, capacities, RNG, dates, save schema and permissions.

## Required finish line and verification

1. Cold-open the exact observation copy, paused, and compare retained state before
   any mutation. Pin source/binary/content/config/save hashes; record zero new
   assistance and unchanged debt. Added observation fields must not normalize away
   discrepancies in the frozen fields.
2. Query and build the bounded plan through normal paid commands. Prove real IRON
   and SILC arrivals, exact 2IRON→1STEL and 2SILC→2BALL processing, and positive
   native STEL/BALL delivery/payment to the actual town receiver. Separate industry,
   train/station, city and company-stock custody; no warehouse diversion.
3. In the same instance, hold BALL delivery with normal vehicle controls until one
   month has sufficient FOOD and STEL but insufficient BALL. Require FOOD use,
   unchanged STEL reserve, no BALL consumption and zero expansion. If FOOD is also
   missing, that month does not count as the isolated missing-BALL control.
4. Release BALL. Observe one month consuming the complete live FOOD+STEL+BALL basket
   and enabling normal native growth permission: growth/passenger multipliers
   exactly 1/1, with the normal native growth-rate hook enabled. Record native house
   and population changes, but do not turn a short observation into a sustained
   population-growth or pacing claim. Save immediately while the proven state is
   visible; research remains off and Materials II unstarted.
5. Cold-load in one fresh process, require full paused equality, then observe
   another positive STEL and BALL delivery/payment before a second distinct
   complete basket is consumed. Initial STEL/BALL are zero, so both baskets must
   reconcile to material produced and transported in this extension. End with a
   hash-verified paused save and terrain/cargo-text audit.

Reconcile every native receipt and debit, including existing services, interest,
fees and tolls. Keep original assistance £6m and new assistance £0 separately:
`ending cash = £3,705,864 + new native receipts − new native debits`; debt remains
£100,000. Continue whole-history figures from primary debits £2,761,963 and
disposable-control debits £4,809. Track all physical production, conversion,
holdings, consumption and explicit discard; reserve increases alone do not pass.
STEL/BALL city storage is only one live basket each (currently 20 each). Retain and
account for excess aboard trains or in ordinary buffers; never force-clear it to
make an order or payment appear complete.

Meaningful focused checks cover resumed-adapter admission/denial, shared owned
track/native quote parity, all-cargo observer non-mutation and complete-basket
atomicity/cold retention. Reuse existing economy/save tests and add only uncovered
cases. After final source changes run repository full gates and exact-head CI,
then independent source/evidence review. A fresh campaign replay stays NOT RUN;
do not represent a resumed checkpoint as pristine-start repeatability or human UAT.

## Binding new bounds

| Item | Authorized ceiling |
| --- | --- |
| Campaign | One copied existing checkpoint, one initial load and one planned cold load; no fresh game, funded replay, seed change or automatic retry. |
| Topology | Fixed four industries/one Core town; at most four new trains with up to three wagons each; one material-basket outcome. |
| Search | At most two complete layout candidates in stable order, each with at most eight endpoint-planner invocations. Each invocation has at most 16 station candidates, 30,000 total predecessor states and 16-tile bridge span. Count all search attempts; no hidden cap reset. |
| Spending | At most £600,000 quoted construction/vehicles/refits; at most £750,000 added gross debits overall, including elapsed charges from all six services and all native diagnostic/control spending. New assistance £0; debt fixed. Historical plus new gross ≤£3,516,772. |
| Time | Paused native preflight/paid construction has its own 30-minute wall cap; planning and compiler-build preparation precede it. The new 75-minute simulation campaign starts exactly once immediately before the FIRST native advance, after preflight preparation. Persist actual UTC start and immutable start+75-minute stop. Initial operation has a 30-minute wall cap; cold continuation has a 15-minute wall cap; both also obey the new outer stop. Include all stopped/diagnostic time after first advance; never reset or extend the clock. Startup failure consumes the one initial load and requires a new handoff even if the clock never started. Old deadlines remain closed. |
| Ticks | Initial operation at most 240×2048; cold at most 120×2048; total 360 advances/737,280 requested native ticks. All controls and diagnosis share those caps; unused phase ticks do not transfer. Paused build has no free background ticking. |
| Stops | First integrity/command/accounting/save defect, absent receiver, wrong payment/custody, crash/lost train, unaffordable quote or exhausted search/spend/time/tick bound stops with partial evidence. No rerun after a repair without another explicit handoff. |

These approved limits authorize this single slice. A legal but slow supply
pattern is measured as pacing; it does not authorize tuning, time extension or
inventory injection. If these bounds cannot support the four services, return
the concrete blocker and a smaller follow-up proposal, without silently dropping
ballast/consumption or claiming the full outcome.

## Short owner mission after delivery

Allow 15 minutes for feedback, with completion time unpromised:

1. Load a copy of the delivered paused save on the identified build/content;
   inspect assistance provenance, cash/debt, research off and the Core basket.
2. Follow one iron/steel and one stone/ballast consignment to the Core station;
   inspect actual payments and distinct reserves.
3. Observe missing-BALL feedback/retained steel, then a complete consumed basket
   and enabled growth state. Report confusing controls or long waits.
4. Save under a new name, quit/reload and check cash, stock, orders, growth state
   and the retained unlock. Report Pass/Fail/Partial and the first issue.

Observation of a supplied network is separate from manual construction acceptance.

## Approved handoff and independent review

The owner explicitly approved this one existing-material basket outcome, a new
ledger from the final verified checkpoint, zero additional assistance, the new
spending/search/tick limits and the corrected first-advance clock. The original
proposal received independent Astra maximum CLEAR; implementation and resulting
native evidence receive a new independent Astra maximum review. No merge, next
slice, balance-policy change, research/cargo grant, extra loan or campaign retry
is authorized. Stop at the first hard defect/exhausted bound; preserve partial
logs and return a concrete blocker plus a bounded next action.

Before implementation verify the copied source save and frozen state, read the
source/AGENTS/ost-plan/dev/uat/deliver, and preserve complete old evidence. Update
functional, economic and human statuses separately. An inherited funded checkpoint
is ASSISTED FUNCTIONAL; ordinary-start economics/fresh replay remain unproven and
human UAT Pending until the owner reports an identified playtest.

## Source anchors

- Roadmap: `docs/PROJECT_STATUS_AND_ROADMAP.md:68–135` (functional-first/A3 ordering).
- Research: `src/portal/tech_tree.cpp:127–165`; `integrated_economy.cpp:452–459`
  (I/II requirements and empty tier II kit).
- Recipes/neutral ownership: `src/portal/integrated_economy.cpp:272–278,332–378`.
- City demand/capacity/atomic basket: `src/portal/integrated_economy.cpp:379–448`;
  native hook `src/town_cmd.cpp:4179–4201`.
- Builder constraints: `src/portal/connected_economy.cpp:1529,1835–1842,1889`;
  scoped planner `1254–1364`; observer `1649–1664` and FOOD-only monthly event
  `src/portal/integrated_economy.cpp:437`.
- Existing focused coverage: `src/tests/test_integrated_economy.cpp:90–145,520–545`
  and integrated Core native round-trip cases in `src/tests/test_megacity_gui.cpp:175`.
- Frozen `final_state` in the continuation archive, inspected without loading a game,
  provides the industry/world/gate/state observations above.
