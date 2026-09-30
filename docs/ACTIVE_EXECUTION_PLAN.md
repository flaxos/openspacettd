# First Sustainable Freight

**Outcome:** From a canonical integrated new game, a player establishes one
legitimate basic freight route using normal starting capital/resources and earns
enough to continue, without authored infrastructure, prebuilt trains, granted
research or injected cash.

This is the **only active implementation plan**. When delivered, retain evidence in
the project status, sprint/domain and UAT records; replace this file only with the
next approved slice. The [canonical roadmap](PROJECT_STATUS_AND_ROADMAP.md#active-planning-horizons--30-september-2026)
sets dependencies. This slice does not implement A2.

## Player-visible outcome

A player builds and runs a first freight service from an ordinary integrated new
game, sees repeated real deliveries and ordinary income, and can afford to keep
playing.

## Starting contract

Use the normal **New Game** UI and `src/genworld.cpp` generation, with at least
two logical worlds and integrated ruleset 1 active. Select integrated industry v1
(OST01 version 5), rail v3 and equipment v1; the Mito–Merredin seven-world preset
is a concrete supported configuration. Record exact game settings, seed, year,
content versions/hashes and resolved ruleset for each run. Do not load a scenario
or adapt an existing save. `IntegratedEconomy::StartNewGame` is conditional on
content readiness and two worlds; verify activation rather than assuming it.

Use the ordinary new-company initial loan/cash from `src/company_cmd.cpp`
(`INITIAL_LOAN` in `src/economy_type.h`) and only normal borrowing up to the
configured maximum. Do not pick a larger ceiling merely to make a fixture pass;
record the chosen existing difficulty/configuration and actual finances. Start
with no granted research, added stockpile materials or generated player assets.
Basic track, stations, depot, signals, steam/diesel and bootstrap factories are
cash-buildable. Generated towns, industries, resource sites, immutable world
regions and existing starting gates may exist; the player builds the railway,
vehicle, consist and orders. The starter opportunity may be raw freight to a
real accepting factory or another legitimate basic production/demand pair; prove
actual production, acceptance and payment. Exact cargo pair and minimum viable
budget are discovery/proof work, not arbitrary numbers in this plan. Showcase
saves and the connected economy harness with authored track, trains, cargo or
starting wealth do not prove this contract.

## Existing implementation

`src/genworld.cpp` and `src/portal/world_gen.*` generate the native map and worlds.
`src/portal/integrated_economy.*` owns ruleset activation, physical production,
roles and flow counters. `src/portal/planet_manager.*` owns placement/role policy;
`src/portal/resource_sites.*` owns surveyed sites. Native
`src/industry_cmd.cpp`, station/cargo, rail, train/order and `src/economy.cpp`
paths provide ordinary construction, movement, acceptance and revenue. The
integrated content trio supplies cargo/vehicles. `src/saveload/planet_sl.cpp`
loads custom economy state. Existing `scripts/test_integrated_economy.py` and
`demo/INTEGRATED-ECONOMY-UAT.md` cover authored progression, not this start.

## In scope

- Prove representative fresh generated worlds contain a legal reachable starter
  producer and accepting destination, with buildable station/track/depot sites.
- Prove the player can discover/select the pair with usable controls and can fund
  the minimum viable line, train and wagons using ordinary money/loan rules.
- Prove physical production, native loading/delivery/payment and repeat service
  yield a viable cash trajectory after running costs; make only evidenced minimal
  configuration/balance or interaction fixes.
- Adapt an existing deterministic native acceptance harness where necessary;
  retain generated-seed, command, cargo, finance and cold-reload evidence.
- Supply a short playable human mission and record its status separately.

## Out of scope

Federation, gate commissioning, advanced research/factories, full city progression,
competitors, narratives, global markets, independent maps, broad economy rewrites,
visual overhaul and unrelated historical UAT cleanup. Do not use authored fixture
infrastructure or artificial wealth to satisfy this slice.

## Architecture constraints

One global map per server; immutable logical world regions. Keep simulation and
commands deterministic, and use server-authoritative commands where required.
Build through ordinary OpenTTD construction, vehicles and orders. Preserve
save/load compatibility, cargo/money conservation and the current integrated
physical-custody path instead of duplicate accounting. Follow `AGENTS.md`,
`ost-dev`, `ost-uat` and `ost-deliver`; consult `docs/SAVEGAME_AUTHORING.md` before
building a UAT artifact.

## Player flow

Normal integrated New Game → locate a generated producer and real accepting
destination → use initial finances/normal loan to construct stations, basic rail
and depot → purchase steam/diesel train and suitable wagon(s) → set ordinary
orders → transport produced cargo → receive native delivery revenue → repeat
service → inspect cash, loan and costs to show continued play is viable.

## Failure paths

No legal producer/demand, unreachable terrain, unaffordable minimum service,
missing acceptance, invalid cargo-to-wagon mapping, incomplete route, structural
operating loss or save/reload discontinuity must surface as a failed acceptance
case and be diagnosed. Correct the bounded cause; do not grant the fixture cash,
stock, research, track or cargo to conceal it.

## Acceptance criteria

1. Generate a **fresh** canonical integrated game for a small representative set:
   seeds 11, 101 and 2026 using the same recorded settings/content; retain each
   result and failure if any. This is sample evidence, not all-seed assurance.
2. Confirm ruleset 1, no authored player rail, no pre-created train, no injected
   cargo and no starter research grant. Any generated neutral gates are recorded
   and are not needed for this route.
3. With only starting company cash and permitted normal loan, ordinary commands
   build the stations, track, depot, train/consist and orders. Record costs and
   remaining borrowing headroom; no cash injection after start.
4. A generated producer produces real cargo; the consist loads, moves and unloads
   it at a valid accepting destination. Record cargo label, quantity, destination
   and physical custody; do not count warehouse storage as final consumption.
5. Native company money changes on delivery. After repeated cycles, record
   revenue, running costs, loan/interest and cash trajectory. Demonstrate the
   player can continue or fund a modest next step without an immediate dead-end;
   a single paid delivery alone is insufficient.
6. Cold save/reload retains the route, orders, cargo/economy state and subsequent
   profitable continuation. Report seed-specific failures honestly.
7. Human UAT is recorded separately for the exact build, content and settings;
   headless success is not graphical acceptance.

## Automated proof

Use focused generation, placement, cargo, command and economic regressions while
implementing. Adapt `scripts/test_integrated_economy.py` or the existing native
builder/harness if suitable; do not establish a second test framework. Capture
seed/settings/content hashes, binary commit, command sequence, finance/cargo
snapshots, save hash and fresh-process reload result. At delivery, `ost-deliver`
requires `./build/openttd_test`, `ctest --test-dir build --output-on-failure`,
repo linters and `git diff --check`. Retain exact counts/logs in a durable evidence
record, and keep fixture proof distinct from human UAT.

## Human UAT

Aim for 5–10 minutes at a practical simulation speed. Launch the verified binary
with the recorded integrated content trio, select **New Game → Mito–Merredin
seven-world preset** and the recorded seed/settings; do not open a showcase save.
Objective: earn repeat freight revenue from a player-built basic line.

1. Locate the documented starter producer and accepting destination using the
   normal world/industry information controls.
2. Borrow only if needed; build two stations, connecting basic rail and a depot.
3. Buy the documented steam/diesel engine and suitable wagons; set load/delivery
   orders and release the train.
4. Watch at least two delivered loads and the company finance/cash display.
5. Save, exit, reload and confirm another delivery and continuing affordability.

Record pass/fail, seed/build/content, first unexpected action or message, cash and
loan before/after, and any unreadable or inaccessible control. If the route cannot
be completed in the target time, record actual wait and cause rather than claiming
acceptance.

## Replan triggers

**Stop implementation and return evidence to Codex planning** if completion
requires an economy redesign, independent maps, a new progression system, changes
to deterministic contracts, a changed development phase/economic role model,
federation dependency, material redefinition of the ordinary start, or a major
balance choice with no project rule. Evidence-backed minor balance tuning within
the intended loop is allowed and must be documented.

## Antigravity execution prompt

```text
Implement the current OpenSpaceTTD active execution slice in:

docs/ACTIVE_EXECUTION_PLAN.md

Outcome: First Sustainable Freight.

Load the relevant project skills and inspect the existing implementation before changing code. Treat the active plan as scope and acceptance authority. Implement the smallest architecture-consistent change using existing OpenTTD/OpenSpaceTTD systems. Run focused regressions during development. Use ost-uat for playable proof and ost-deliver for final verification, documentation, commit and PR. Do not expand into the next roadmap item. If a Replan Trigger requires a material architecture/product decision, stop, document the evidence and return it to Codex planning.
```
