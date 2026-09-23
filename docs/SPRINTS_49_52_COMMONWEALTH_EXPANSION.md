# OpenSpaceTTD: Sprints 49–52 Commonwealth Expansion Specification

**Status:** CANONICAL FUTURE SPRINT SPECIFICATION  
**Date:** 2026-09-18  
**Governing Architecture:** Peter F. Hamilton *Commonwealth Saga* + OpenTTD Engine Determinism  
**Prerequisites:** Completion of Sprints 43–48 (`docs/POST_RECOVERY_ROADMAP_SPRINTS_43_48.md`)  
**Companion Documents:**  
- [`docs/COMMONWEALTH_WORLD_GRAPH_AND_EXPANSION_DESIGN.md`](COMMONWEALTH_WORLD_GRAPH_AND_EXPANSION_DESIGN.md)  
- [`docs/COMMONWEALTH_UNIVERSE_CATALOGUE.md`](COMMONWEALTH_UNIVERSE_CATALOGUE.md)  
- [`docs/world_roadmap.txt`](world_roadmap.txt)  

---

## 1. Executive Roadmap Summary

Sprints 49 through 52 represent the **late-game apex** of OpenSpaceTTD. Once players have mastered closed-loop industrial ecology across their home systems, these sprints unlock expansion into the wider 108-world Commonwealth universe, enabling inter-world trade with prebuilt lore economies, multi-modal gateway transshipment, expeditionary exploration, and galactic economic hegemony.

```text
+---------------------------------------------------------------------------------------------------+
|                            SPRINTS 49-52 IMPLEMENTATION MATRIX                                    |
+--------+------------------------------------+-----------------------------------------------------+
| Sprint | Name                               | Key Deliverables & Gameplay Mechanics               |
+--------+------------------------------------+-----------------------------------------------------+
| **49** | **Commonwealth Graph Engine &**    | C++ UniverseGraphManager; JSON schema parser;       |
|        | **Prebuilt Lore Economies**        | Universe Directory Galaxy Map; off-map trade ports. |
+--------+------------------------------------+-----------------------------------------------------+
| **50** | **Multi-Modal Gateway Operations &**| Cyclic scheduled gates (Far Away power cycle);      |
|        | **Dynamic Gate Cycles**            | Vinmar Data Relays (RP beam); holding loop staging. |
+--------+------------------------------------+-----------------------------------------------------+
| **51** | **CST Mainline Heavy Freight Corridors &**| Quad-track trunk management; gate throat flying   |
|        | **Automated Marshalling Staging**  | junctions; automated hump/marshalling yards.        |
+--------+------------------------------------+-----------------------------------------------------+
| **52** | **Galactic Commonwealth Logistics &**| 108-world industrial clearinghouse; heavy axle-load |
|        | **Industrial Hegemony**            | unit train tariffs; network mastery challenges.     |
+--------+------------------------------------+-----------------------------------------------------+
```

---

## 2. Sprint 49: Commonwealth Graph Engine & Prebuilt Lore Economies

### Objective
Integrate the 108-world Commonwealth topology into the runtime engine, enabling the Universe Directory to browse the full galaxy tree and allowing player portal gates to link to simulated off-world trading partners.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-49.1** | `UniverseGraphManager` | Load and parse `assets/data/commonwealth_universe.json`. Validate graph connectivity, cycle detection, and parent-child hierarchy rooted at Earth/Sol. Support `SOURCE_ONLY`, `RECONSTRUCTED`, and `PLAYABLE_COMPLETION` filter modes. | Unit tests in `test_universe_graph.cpp` verifying 108 nodes, 16 special bodies, and correct tree traversal from Sol. |
| **WP-49.2** | Universe Directory Galaxy Map | Extend `UniverseDirectoryWindow` with a visual tree browser (`WID_UD_GALAXY_MAP`). Nodes are colored by Phase (P1 Gold, P2 Cyan, P3 Green, P4 Grey) with evidence badges ($E, R, I, A$). | Interactive GUI test verifying node expansion, panning, filtering by phase, and inspecting world detail cards. |
| **WP-49.3** | Prebuilt Trade Gateways | Allow local portal gates to link to off-world Commonwealth nodes without requiring an external server process. The Universe Authority acts as an economic proxy, consuming exported goods (Structural Steel, Silicon Chips) and queuing scheduled return consists. | Automated regression test: outbound train enters trade gate, despawns, credits freight tariff, and spawns inbound return train with ordered cargo after `virtual_length` transit delay. |
| **WP-49.4** | Economy Calibration | Implement phase-specific import/export profiles for Big15 hubs (high luxury demand, low raw demand) vs Phase 2 industrial worlds (high raw ore demand, high machine module output). | Ledger reconciliation test ensuring trade balance sheets match expected tariff formulas. |

---

## 3. Sprint 50: Multi-Modal Gateway Operations & Dynamic Gate Cycles

### Objective
Implement non-standard gateway operating physics, specifically cyclic scheduled gates (the Far Away stormrider power cycle), subspace data-only relays (Vinmar), and private/restricted charter gating.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-50.1** | Cyclic Scheduled Gateways | Add `GateOperatingCycle` to `PortalLink` and `PortalEndpoint`. Gateway cycles between `Active` (e.g. 5 game days) and `Recharging` (e.g. 15 game days). When recharging, gate throat signals automatically display red. | Test script verifies signals turn red upon gate deactivation; trains path into holding loops; signals turn green and trains resume on cycle activation. |
| **WP-50.2** | Automated Staging Loops | YAPF pathfinder integration: when a scheduled gate is in recharge, pathfinder routes trains into designated `StationFacility::HoldingSiding` blocks instead of halting on the mainline. | Simulation test verifying 0 mainline deadlock across 10 complete 20-day stormrider cycles with 6 active freight trains. |
| **WP-50.3** | Subspace Data Relays (Vinmar) | Enforce that `DATA` gates reject rolling stock (`STR_ERROR_DATA_GATE_NO_TRAINS`). A `SubspaceRelayStation` built on the gate tile generates continuous Tech Tree Research Points (RP) proportional to company network rating. | Catch2 test verifying trains cannot enter data gates; RP ledger increments monthly in `TechTreeManager`. |
| **WP-50.4** | Private & Diplomatic Charters | Implement gate access permissions (`CmdSetGateAccessPolicy`). Reaching private worlds (Cressat, Solidade, Ozzie's Asteroid, Hardrock) requires company reputation $\ge 80\%$, diplomatic charter, or payment of per-train toll. | Test asserting train without charter receives routing rejection; purchasing charter unlocks transit. |

---

## 4. Sprint 51: CST Mainline Heavy Freight Corridors & Automated Marshalling Staging

### Objective
Implement authentic Commonwealth Star Transit (CST) heavy industrial railway mechanics: high-capacity quad-track mainline trunks, directional throat signaling, automated classification/marshalling yards, and heavy axle-load unit trains connecting factories to planetary portal gates.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-51.1** | Quad-Track Trunk Management & Corridor Signaling | Extend YAPF pathfinder with directional speed-lane and cargo-tier track reservation. Inner high-speed tracks prioritize express inter-world freights; outer relief tracks handle local feeder consists. | Pathfinding regression test verifying express unit trains do not get trapped behind local shunting consists on 4-track trunk lines. |
| **WP-51.2** | High-Density Gate Throat Interlocking | Optimized turnout interlocking and flying junction prefabs for 18-tile portal arches. Prevents conflicting train paths from deadlock at portal entrance throats. | Catch2 test verifying continuous throughput of 12 consists/minute across a single quad-track portal arch throat with zero deadlocks. |
| **WP-51.3** | Automated Classification & Marshalling Yards | Dedicated classification sidings (`StationFacility::MarshallingYard`). Inbound mixed-manifest industrial trains are automatically sorted by destination world into homogeneous unit block trains. | Automated unit test: mixed cargo wagons decoupled at hump siding, reassembled into destination-specific unit trains, and dispatched through corresponding portal gates. |
| **WP-51.4** | Heavy Axle-Load Unit Freight Operations | Support for ultra-heavy unit trains (CST Titan D-100 twin-unit diesels and Vulcan heavy haulers). Dynamic track wear and axle-load constraints requiring reinforced heavy rail. | Unit test verifying locomotive haulage ratings, dynamic tractive effort on steep grades, and heavy rail maintenance economics. |

---

## 5. Sprint 52: Galactic Commonwealth Logistics & Industrial Hegemony

### Objective
Unify the 108-world Commonwealth into an interconnected industrial powerhouse with commodity arbitrage, inter-world supply contracts, corridor backpressure tariffs, and network mastery challenges.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-52.1** | Inter-World Industrial Clearinghouse | Dynamic multi-world commodity clearinghouse and price arbitrage ledger. Heavy manufacturing worlds demand raw minerals and energy; frontier worlds purchase machine modules and structural steel. | Headless simulation test verifying dynamic commodity pricing elasticity and tariff settlement across 12 simulated months. |
| **WP-52.2** | Corridor Backpressure & Bottleneck Tariffs | High-volume portal corridors exceeding capacity incur network congestion charges, incentivizing players to construct bypass corridors, grade-separated junctions, or upgrade to vacuum-tube maglev corridors. | Test verifying congestion delays and player revenue adjustments under heavy traffic loads. |
| **WP-52.3** | Commonwealth Logistics Master Contracts | Long-term multi-world supply contracts (e.g. delivering 10,000 tons of Heavy Machine Parts from Phase 2 Merredin to Phase 4 frontier worlds within 180 game days). | Savegame contract test verifying multi-consist milestone tracking and contract fulfillment payouts. |
| **WP-52.4** | Network Mastery Scenarios | High-density industrial network operational scenarios: managing peak-hour corridor surges, scheduled maintenance closures, and emergency freight re-routing. | Scenario runner test verifying corridor throughput and routing recovery under forced bottleneck events. |

---

## 6. Testing & Acceptance Matrix

```text
+---------------------------------------------------------------------------------------------------+
|                                  TEST & VERIFICATION MATRIX                                       |
+--------+----------------------------+-------------------------------------------------------------+
| Sprint | Test Target                | Verification Tool & Suite                                   |
+--------+----------------------------+-------------------------------------------------------------+
| **49** | Universe Graph & Prebuilt  | Catch2: `test_universe_graph.cpp`, `test_prebuilt_trade.cpp` |
|        | Economies                  | Python: `scripts/test_universe_graph.py`                    |
+--------+----------------------------+-------------------------------------------------------------+
| **50** | Scheduled Cycles & Data    | Catch2: `test_scheduled_gates.cpp`, `test_data_relays.cpp`  |
|        | Relays                     | Headless Run: 10 stormrider cycles zero-deadlock smoke      |
+--------+----------------------------+-------------------------------------------------------------+
| **51** | CST Mainline & Automated   | Catch2: `test_cst_mainline.cpp`, `test_marshalling_yard.cpp`|
|        | Marshalling Staging        | End-to-end: Quad-track portal throat throughput benchmark   |
+--------+----------------------------+-------------------------------------------------------------+
| **52** | Galactic Logistics &       | Catch2: `test_galactic_clearinghouse.cpp`                   |
|        | Industrial Hegemony        | Scenario Runner: Commonwealth Master Contract verification  |
+--------+----------------------------+-------------------------------------------------------------+
```
