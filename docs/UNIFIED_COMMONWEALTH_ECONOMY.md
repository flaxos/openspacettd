# Unified Commonwealth economy

This feature is delivered on `codex/unified-commonwealth-economy`, based on
`959fe33f18` (CST star map, PR #43). It uses the existing single global map and
immutable logical world rectangles. Human graphical acceptance is Pending.

## Activation and compatibility

New Commonwealth games with two or more worlds and the integrated content pack
enable saved economy ruleset 1 and surveyed, player-built expansion, even when
an older configuration has the random-industry preference selected. Use `openspacettd_integrated_v1.grf` together with
rail v3 and equipment v1. Integrated content has OST01 version 5 / minimum
compatible version 5; it replaces the industry pack in a **new game's** content
selection. Do not add it to an existing save. The published industry v4, rail v3
and equipment v1 files and hashes are preserved in the manifest.

`ECON` stores its version, immutable world roles, factory records, integer yield
remainders, city reserves, research escrow, confirmed research homes/unlocks and
cumulative physical-flow counters. Native industry chunks store physical factory
input/output cargo. Post-load validation checks content, worlds, ownership,
recipes and research escrow. Saves without `ECON` retain legacy rules, recipes,
station facilities, research and fabrication behavior; no conversion is performed.

## Roles and production

| Role | Activities |
|---|---|
| Frontier | Surveyed raw extraction, agriculture, forestry, fishing, quantum enrichment |
| Industrial | Food processing, refining, manufacturing, machinery and maglev assembly |
| Core | Consumer crystal formatting, city demand and corporate research |

The seven-world preset assigns Mito Core, Merredin and Tandil Industrial, and
all other worlds Frontier. Colony development never changes this role. Access,
colonisation, terrain and surveying are still separate placement requirements.
`PlanetManager` applies the same economic placement policy to native commands,
Fund Industry, AI/scripts and the authoring harness.

Fund physical factories through **Fund Industry**. The facilities dashboard
locates, upgrades and retires these same native industries. Each starts at 100
batches/month, retains the existing +50 capacity upgrade up to 1,000, and holds
at most three months of rated inputs and outputs. A full output buffer stops
production; missing one input stops the entire batch. Native/NewGRF tick and
delivery production paths are disabled for managed processors. Only the
project-owned monthly manager converts their cargo.

| Process | Batch | Unlock |
|---|---|---|
| Ballast | 2 stone → 2 ballast | Bootstrap |
| Steel | 2 iron ore → 1 steel | Materials I |
| Wiring | 2 copper ore → 2 wiring | Materials II |
| Chips | 2 silica + 1 wiring → 1 chip | Materials II |
| Signals | 1 chip + 1 wiring → 2 signalling equipment | Materials II |
| Machinery | 2 steel + 1 wiring + 1 chip → 1 machine module | Materials II |
| Superalloys | 2 steel + 1 rare minerals → 2 superalloys | Materials III |
| Polymers | 2 oil → 2 composites | Materials III |
| Maglev | 2 superalloys + 2 wiring → 2 maglev assemblies | Materials IV |
| Blank crystals | 2 silica + 1 rare minerals → 2 blank crystals | Materials III |
| Quantum enrichment | 2 blank crystals → 2 enriched crystals | Materials IV |
| Consumer crystals | 2 blank crystals → 2 consumer crystals | Materials III |
| Food | 2 grain → 2 food | Bootstrap |

The catalogue in `IntegratedEconomy::ConfigureRecipes` supplies production,
funding descriptions and dashboard quantities. Materials III applies the existing
15% yield bonus with persisted integer remainders, independent of batch splitting.
BALL, SIGE and MGLA are distinct finished products. Existing labels are preserved;
FOOD, GRAI and OIL_ are supplied only when absent. A universal freight wagon supports
all 20 required freight labels. Cargo text and native refit masks are audited.

## Delivery and materials

Factory inputs arrive through ordinary station catchment. Output waits in the
factory or a collecting station; an idle warehouse elsewhere cannot receive it.
Planetary stock is company- and world-local and can fund construction and HQ
research anywhere on that world. Factories never withdraw that stock automatically.

Owned warehouses take priority over nearby factories/cities and show that role in
the station window. Each unloaded unit goes to exactly one recipient; project
reservations and warehouse export floors remain in force. Warehouse deposits earn
no final-delivery payment or city credit. Accepted factory/city deliveries retain
native freight payment and transfer provenance. Capacity checks leave excess on
the train. City growth depends on consumption, not arrival. Cumulative counters
separate production, transport, storage, consumption and explicit discard.

Conventional track, basic signals, steam/diesel and bootstrap factories remain
cash-buildable. Electric/monorail/maglev infrastructure and powered vehicles use
material bills even in Cash mode, alongside the existing discounted cash price.
BALL replaces raw stone, SIGE replaces signal placeholders, and MGLA supplies
maglev components. Advanced Materials III/IV factories require 50 steel and
10 machine modules plus their cash price. Gates retain delivered equipment bills.
Shared material queries cover native construction, conversions, blueprints,
cloning and autoreplacement; previews and failures preserve inventory.

## Research and cities

Tier I–II research needs cash only. Every Tier III requires 40 steel + 20 chips;
Materials IV needs 40 chips + 20 blank crystals; Traction IV and Portal IV require
Materials IV and 40 chips + 20 enriched crystals. Cash progress stops at the
project's target while waiting for its complete kit. Reservation is atomic;
switching projects refunds unconsumed escrow, and completion consumes it once.
The HQ's optional chip/crystal acceleration is off by default and cannot spend
reserved goods.

Core city monthly demand rounds up:

| Basket | Demand | Storage |
|---|---|---|
| Food | max(50, ceil(population/20)) FOOD | Three months |
| Expansion | max(20, ceil(population/100)) each STEL and BALL | One month |
| Prosperity | max(10, ceil(population/200)) each CHIP and CCRY | One month |

Food is consumed monthly. Expansion requires food; prosperity requires food and
expansion. Unused baskets stay stored, and falling capacity never deletes reserves.
No food means no growth and half passenger generation without population loss.
Food alone maintains the city. Food + expansion permits normal growth; all baskets
permit twice normal growth and 1.5× passenger generation. The city panel explains
reserves, last consumption and the exact missing basket.

## Federated technology

An explicitly mapped global company has one confirmed research home. The first
registered HQ home claim is committed by the existing authority; competing home
claims and incompatible economy rulesets are rejected. Only the home spends local
cash/materials and conducts research. Confirmed monotonically versioned unlocks
are replicated through server-authored native commands. Remote hosts use those
unlocks for construction, factories, surveying and gates and retain them during
outages. An unconfirmed disconnected host cannot establish competing research.

Home identity, revision and unlocks are saved. Recovery uses coordinated cluster
checkpoints; research-home relocation and independent rollback are outside this
feature. Money and inventory never become a global shared pool.

## Verification and limits

See [recorded evidence](audit/2026-09-28/integrated-economy/README.md) and
[the playable UAT checklist](../demo/INTEGRATED-ECONOMY-UAT.md). The automated
connected fixture authors infrastructure and starting capital, then uses physical
freight and earned revenue with no recurring injected cargo or cash. It tests
production/progression, not minimum-loan early-game balance. The generation test
separately exercises the seven-world preset and empty city reserves.

Dynamic commodity markets, narrative systems, individual warehouse inventories,
automatic host provisioning and existing-save conversion are excluded. Exact
human readability, economic pacing across arbitrary seeds, and large-cluster
performance require acceptance beyond these bounded automated fixtures.
