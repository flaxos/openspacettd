# OpenSpaceTTD player UAT — Sprints 1–42

Current checklist: 2026-09-15, reviewed source baseline `a6b79add03`.
Human acceptance starts **Not run** for every case below. A sprint commit, a
unit test or an in-game goal is not evidence that a player completed it.

## Start here

Build with `ninja -C build openttd openttd_test`, then launch:

```sh
./build/openttd -g demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav
```

Play the first human company (internal Company 0). Open the Story Book and
start with **13. START HERE: Player Ownership & UAT Results**. Chapters 1–12
are historical orientation; this checklist supersedes their old acceptance
claims and console shortcuts. Chapters 14–15 explain new coverage and blockers.
World 1 in player instructions means internal world ID 0; World 6 means ID 5.

Save a personal working copy before testing. Reload the supplied v1.1 for each
independent case unless a prerequisite below explicitly carries progress forward.
Never overwrite the supplied save. Use [UAT-RESULTS.md](UAT-RESULTS.md) to record
build hash, case, screenshot, expected/actual result and Pass/Fail/Blocked.
Use **Fail** for a playable action behaving incorrectly; **Blocked** for missing
content, fixtures, controls or prerequisites. Leave unattempted cases **Not run**.

The migrated demo retains its existing content configuration: do not add NewGRFs
to this running save to make a content test pass. Its ten generated gate heads
and complete terminals are assigned to the player, not neutral infrastructure.
The legacy short-approach train builder is disabled on migration because it
overlaps modern gate terminals. If no ready train is present, build your own in
UAT-01; a missing automatic demonstrator is not proof of transit.

## Player cases

For every case: capture the visible starting state, perform the steps, capture
the result, then record it. Reset by reloading your clean v1.1 copy. For month-based
tests record elapsed game months and delivered quantities; do not infer supply
or research success just from an open window.

### UAT-01 — Ownership and usable prebuilt gate tracks

Location: Story Book Gateway Alpha pins; repeat ownership inspection at all five
gate pairs across six worlds. Prerequisite: first human company selected.

1. Use Land Area Information on both heads, both holding lanes, switches and
   connection tiles. Confirm every rail fixture belongs to your company.
2. On an unoccupied outer approach, remove and rebuild one rail piece. Extend
   the approach onto clear land, connect a rail depot, and modify a path signal.
   Do not edit occupied/reserved track or demolish the gate for this exercise.
3. Buy a locomotive and wagons available in this save. Connect stations beyond
   the two heads and give the train station orders in both worlds. Start it.
4. Follow it through the gate and back. Verify all wagons emerge aligned and
   orders remain intact. Save/reload and repeat the ownership inspection.

Pass: no foreign-owner error, usable depot/connection, signals remain functional,
and a complete player train traverses the local portal. A stopped train or a
blocked reservation requires diagnosis, not disabling ownership rules.

### UAT-02 — Worlds, biomes and navigation

Use Map world-jump controls or Ctrl+Alt+1 through 6. Inspect viewport world labels,
boundaries and Universe Directory badges. Capture all six worlds at the same zoom.
Pass: six correct destinations without landing in void; terrain behaviour and
phase labels differ appropriately. Bespoke art acceptance is UAT-15, not this case.

### UAT-03 — Build, link and protect portal terminals

On a fresh copy, use the railway toolbar Portal Gate tool on clear level terrain
in two developed worlds. Follow its placement/link controls. Inspect the full
two-lane terminal and path signals. Run a train through the pair. Try demolition
while transit is active, then after the route is clear.
Pass: correct linking, complete terminal or clean rejection with no partial build,
and safe handling of an occupied portal. Record the displayed rejection text.

### UAT-04 — CST prefabs and player blueprints

Open the rail toolbar Blueprint Library at the World 2 staging pad. Place each of
the eight built-in CST blocks on separate clear areas; rotate/mirror using the
visible controls. Capture your own rail layout, name it, export/import it and
stamp a replica. Test an obstructed footprint on a separate copy.
Pass: correct rails/signals/orientation; imported layout matches; obstruction
produces an actionable result without an unexplained partial layout. If the
sample layout is absent, construct a small player-owned sample and record that.

### UAT-05 — Spaceports and Edge Conduits

Open a player station's Spaceport controls (use the Phase 1 candidate if present).
Designate it, inspect requirements, supply the stated cargo, then attempt upgrades.
At the World 3 boundary marker, build an Edge Conduit using the rail toolbar and
connect a receiving station. Inspect Land Area Information and wait a game month.
Pass: valid placement, visible upgrade consequences and actual cargo output.
Missing station fixtures must be built normally or recorded Blocked; a marker
alone is not a station. External routing is separately covered by UAT-16.

### UAT-06 — Megacity supply, growth and corridor telemetry

Open a town's Megacity view and the Map freight-corridor monitor. Record the actual
cargo labels/quotas shown. Deliver required cargo, observe monthly satisfaction
and growth, then compare an unsupplied period on a separate copy. Run trains
through a portal and compare corridor counts before/after.
Pass: delivered supply and real traffic change the corresponding state; idle UI
inspection alone does not pass economic or congestion behaviour.

### UAT-07 — Ledger and governance UI

Open Map > Supply Chain & Trade Ledger and Federation Authentication & Charters.
Inspect tabs, create a test identity/charter using the offered controls and inspect
world presence. Record whether data is local or external. Run a delivery and
compare ledger values. Pass the UI portion only if actions provide visible state,
errors and consequences. External credentials and conserved remote transactions
need UAT-16; a static CONSERVED label is insufficient.

### UAT-08 — Colonisation, promotion and restrictions

In the Universe Directory inspect Worlds 4–6 before colonisation. Attempt ordinary
construction on an uncolonised world and record the restriction. Use the directory
colonisation control with its stated requirements, then test newly allowed builds.
Deliver cargo, inspect development progress and attempt promotion below/above its
threshold. Test a phase-restricted rail type and biome-restricted industry.
Pass: prerequisites/rejections are visible and permitted actions change phase,
construction access and progression. Do not use console promotion as proof of the
player workflow. Missing controls or cargo supply are explicit blockers.

### UAT-09 — Save/load and crash regression

After UAT-01 and one progression action, save your working copy. Close/relaunch,
open Company details, Story Book, train orders, directory and gate views.
Pass: no String 0xFFFF crash; ownership, links, orders and progress persist;
tutorial pages/goals are not duplicated. Record the loading build hash.

### UAT-10 — Corporate HQ, stockpiles and logistics hubs

Open Map > Corporate Headquarters; inspect Overview, Stockpiles and Logistics Hubs.
Record HQ world/tier and inventory, then use available upgrade controls. Inspect
the hub's actual station, owner and world. Deliver cargo to that station and
compare inventory. Configure reserve floors and attempt loading above/below them.
Pass: valid same-company/same-world attachment, cargo really enters inventory and
floors prevent over-withdrawal. A registry entry without a usable station is Blocked.

### UAT-11 — In-kind fabrication

Use the Corporate HQ Fabrication tab. Record cash and local inventory; build a
fixed rail sample with fabrication off. Reload and build the same sample with
fabrication on. Repeat with inadequate material supply.
Pass: displayed BOM, cash reduction and inventory deductions agree; inadequate
supply has a clear outcome. Baseline discount is 80%; test 90% only after completing
the relevant Materials research. Shared fallback cargo pools are not independent
inventories: do not add the six role displays together.

### UAT-12 — Commonwealth research (Sprint 41)

Prerequisite: eligible HQ in a Core world. Open Corporate HQ > Tech Tree, select
an available project and use the budget control. Record RP, cash and HQ-world
feedstock; observe at least one monthly update. Try a locked prerequisite project,
then complete a project and save/reload. Continue through Materials tier 3 to
repeat UAT-11's discount comparison if resources permit.
Pass: prerequisite rejection, funding/progress and completion persistence are
observed. Missing feedstock or controls blocks the dependent step, not a pass.

### UAT-13 — Commonwealth cargos and CST rolling stock (Sprint 37)

Inspect active NewGRFs, Cargo Payment Rates and depot purchase/refit lists. Record
which of the 12 intended cargos and CST vehicles actually exist. Test research and
world-phase purchase restrictions only where the corresponding content is active.
**Blocked for full-pack acceptance in this migrated save:** its content config
does not activate the new packs. Generated package metadata/domain tests do not
prove visible functional vehicles or industries. A separately validated fresh
content-enabled scenario is required; do not alter GRFs in an existing save.

### UAT-14 — Closed production chains (Sprint 42)

**Player path implemented; human result: Not run.** Facilities are upgrades to
owned rail stations. They cost 100,000 Cr and process up to 100 batches per month.

1. Build a rail station in a Phase 2 Developed world. Open its station window,
   choose **Build production facility**, then **Blast Furnace Structural Steel**.
   Wrong-phase recipes are disabled. A station supports one facility at a time.
2. Run an ore train from an existing mine to that station. Use normal delivery
   (not Transfer) and **No loading** at the refinery for the input train. Confirm
   Iron Ore appears in the facility's input buffer, even without a nearby mill.
3. Deliver 60 units: at the next monthly update expect 30 batches and 30 Steel,
   with no ore left. Without a company hub on that world, Steel appears as normal
   waiting station cargo. Load it onto a separate train and deliver it onward.
4. Repeat with a company logistics hub on the same world. Expect produced Steel
   in the planetary stockpile and no duplicate credit for the incoming ore.
   Hub surplus remains available through the existing hub loading path.
5. Stop ore deliveries: production must stop. Save with residual input and
   waiting output, reload, and verify inventories and recipe persist. Research
   Automated Nanofabrication Lines and check the existing 15% yield bonus.
6. Try another company's station and a duplicate facility: both must fail.
   Remove the facility, or demolish its final rail platform: it must retire and
   salvage remaining internal buffers to the owner's planetary stockpile.
   Already waiting cargo follows ordinary station behaviour.
7. Repeat appropriate recipes across Pipelines A–D and world phases; include a
   cross-world train route before recording full chain acceptance.

The window displays the actual cargo inputs and outputs. The migrated demo uses
vanilla cargo aliases (several conceptual materials share a cargo); this is not
acceptance of distinct Commonwealth cargos or new raw-extraction industries.
UAT-13's active-content blocker remains. Do not mark all chains passed based only
on the steel example.

### UAT-15 — Commonwealth art direction (Sprint 38)

Capture comparable normal-zoom views of six biomes, active/inactive gates, busy
rail layouts, stations, industries and settlements. Record legibility and visual
identity. **Blocked for bespoke-art completion:** original terrain/flora/CST and
arcology asset work remains planned. Palette styling is not proof of those assets.

### UAT-16 — Independent-server federation

Use [FEDERATION-UAT.md](FEDERATION-UAT.md) with an operator and two game servers
plus external authority. This solo save cannot pass the case. Record each subcase
separately: physical gate-entry departure, exact remote arrival, visible full
consist/cargo, return orders, content mismatch rejection, obstructed arrival,
duplicate prevention and restart recovery. The existing Sprint 35 runner uses
manual `federation_dispatch`; record that as operator-path evidence only.

## Complete sprint and spike mapping

Every sprint is represented; documentation sprints are reviewed rather than
turned into invented gameplay. Individual player outcomes are recorded above.

| Sprints | Outcome / UAT cases |
|---|---|
| 1, 2 | Worlds and phase restrictions: 02, 08 |
| 3, 4 | Local traversal and generated gates: 01–03 |
| 5 | Cargo attribution, revenue, industry rules: 05–08 |
| 6 | World-aware UI: 02 |
| 7 | Persistence: 09 |
| 8 | Portal construction and demolition: 03 |
| 9, 10 | Planetary operations and crash stabilisation: 05, 09 |
| 11 | Terminal topology/PBS: 01, 03 |
| 12, 13, 14, 15 | Snapshots, admission, identities, transfer: 16 |
| 16 | Accounts, charters, directory, ledger: 07, 16 |
| 17, 18 | Megacities, freight economy and UI: 06 |
| 19, 20 | Cluster recovery and infrastructure routing: 05, 16 |
| 21 | Telemetry/navigation and Commonwealth strings: 02, 06, 13 |
| 22 | Round-trip orders: 09, 16 |
| 23 | Scope/art/UAT audit: this mapping and 15; documentation review |
| 24 | Procedural biomes: 02, 15 |
| 25, 26 | Player blueprints and eight CST blocks: 04 |
| 27 | UI completion: 04–08; record missing controls as gaps |
| 28, 29 | Solo guide and federation acceptance kit: 01–09, 16 |
| 30, 31, 32, 33 | Six biomes, colonisation, development, town/industry lifecycle: 02, 06, 08 |
| 34 | Documentation consolidation: reconcile this guide, roadmap and coverage matrix |
| 35 | Cross-process transport: 16; operator-dispatch evidence is not natural entry |
| 36 | All-feature demo: 01–12; artifact availability is not human acceptance |
| 37, 38 | Content pack and bespoke art: 13, 15; blockers explicit |
| 39, 40 | HQ/logistics and fabrication: 10, 11 |
| 41, 42 | Research and production chains: 12, 14 |

Portal/YAPF and multi-wagon spikes map to 01/03/09; F2 transfer to 16;
F3 persistent universe to 07/16; F4 economy to 06/07. The corporate-tech design
spike maps to 10–14. Their historical automated evidence remains separate from
these unproven human acceptance outcomes.

## Reproduce the artifact

`python3 scripts/refresh_uat_demo.py` migrates the preserved v1.0 using the current
GameScript and acknowledged ownership/save commands. It refuses to overwrite an
existing output; use `--output /tmp/uat-review.sav` for a repeat run. `-X` isolates
script discovery from installed older copies; ensure OpenGFX is available under
`build/baseset/` when using that mode. This is a local artifact operation, not
federation acceptance. Never run fixture setup on a live shared server.

Run `./build/openttd_test 'Portal Construction - UAT*,Sprint 36 UAT*'` for ownership
regressions, then the full CTest suite. Verify the real saved fixture with
`python3 scripts/refresh_uat_demo.py --verify demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav`.
Graphical and live-server results must be
recorded independently in the results sheet.
