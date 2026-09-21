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
| **51** | **Expeditionary Survey Logistics &**| Expeditionary Survey Trains; Silfen Path Intermodal |
|        | **Silfen Intermodal Paths**        | Depots; High Angel / Kerensk orbital shuttle docks. |
+--------+------------------------------------+-----------------------------------------------------+
| **52** | **Galactic Commonwealth Hegemony &**| 108-world economic clearinghouse; tariff arbitrage; |
|        | **Narrative Lore Scenarios**       | historical event scenarios (Dyson Alpha, Elan evac).|
+--------+------------------------------------+-----------------------------------------------------+
```

---

## 2. Sprint 49: Commonwealth Graph Engine & Prebuilt Lore Economies

**Status:** COMPLETE (Implemented in Sprint 49, verified with 430 passing CTests, Catch2 suites `test_universe_graph.cpp` and `test_prebuilt_trade.cpp`, and Python validator `scripts/test_universe_graph.py`)

### Objective
Integrate the 108-world Commonwealth topology into the runtime engine, enabling the Universe Directory to browse the full galaxy tree and allowing player portal gates to link to simulated off-world trading partners.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-49.1** | `UniverseGraphManager` | Load and parse `assets/data/commonwealth_universe.json`. Validate graph connectivity, cycle detection, and parent-child hierarchy rooted at Earth/Sol. Support `SOURCE_ONLY`, `RECONSTRUCTED`, and `PLAYABLE_COMPLETION` filter modes. | Unit tests in `test_universe_graph.cpp` verifying 108 nodes, 16 special bodies, and correct tree traversal from Sol. (Passed: 100%) |
| **WP-49.2** | Universe Directory Galaxy Map | Extend `UniverseDirectoryWindow` with a visual tree browser (`WID_UD_GALAXY_MAP`). Nodes are colored by Phase (P1 Gold, P2 Cyan, P3 Green, P4 Grey) with evidence badges ($E, R, I, A$). | Interactive GUI test verifying node expansion, panning, filtering by phase, and inspecting world detail cards. (Passed: 100%) |
| **WP-49.3** | Prebuilt Trade Gateways | Allow local portal gates to link to off-world Commonwealth nodes without requiring an external server process. The Universe Authority acts as an economic proxy, consuming exported goods (Structural Steel, Silicon Chips) and queuing scheduled return consists. | Automated regression test: outbound train enters trade gate, despawns, credits freight tariff, and spawns inbound return train with ordered cargo after `virtual_length` transit delay. (Passed: 100%) |
| **WP-49.4** | Economy Calibration | Implement phase-specific import/export profiles for Big15 hubs (high luxury demand, low raw demand) vs Phase 2 industrial worlds (high raw ore demand, high machine module output). | Ledger reconciliation test ensuring trade balance sheets match expected tariff formulas. (Passed: 100%) |

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

## 4. Sprint 51: Expeditionary Survey Logistics & Silfen Intermodal Paths

### Objective
Implement exploration and intermodal transshipment mechanics for hazardous frontier worlds, Silfen alien paths, and orbital starship habitats.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-51.1** | Expeditionary Survey Consists | New specialized rolling stock: *Mobile Survey Laboratory Car*, *Environmental Hazard Shield Wagon*, and *Tracklayer Caboose*. Unexplored worlds (Chelva, Tandil, Gaczyna) cannot receive standard commercial trains until surveyed. | Test asserting un-surveyed gate rejects standard freight; dispatching an Expeditionary Consist surveys world, discovers resource clusters, and unlocks commercial gate. |
| **WP-51.2** | Silfen Path Intermodal Depots | Implement `StationFacility::SilfenIntermodalDepot` on Silfen worlds (Silvergalde, Jaruva, Ice Citadel). Rail consists terminate and unload; cargo is containerized into alien path pods and transported across the Silfen network without rails. | End-to-end cargo test: raw freight unloaded at Silvergalde depot; authority ledger tracks path transit; materializes at Jaruva depot. |
| **WP-51.3** | High Angel Orbital Interface | Kerensk orbital wormhole terminal connects to `StationFacility::OrbitalLighterDock`. Rail freight converts to orbital cargo lighters feeding High Angel orbital facilities. Unlocks alien technology trade (Quantum Crystals $\rightarrow$ Exotic Arcology Blueprints). | Verification test for Kerensk-to-High Angel orbital transit and exotic blueprint purchase. |

---

## 5. Sprint 52: Galactic Commonwealth Hegemony & Narrative Lore Scenarios

### Objective
Unify the 108-world Commonwealth into a living galactic economy with dynamic tariffs, supply-chain congestion surcharges, and playable historical lore scenarios.

### Work Packages

| Package | Component | Required Implementation | Acceptance Evidence |
|---|---|---|---|
| **WP-52.1** | Galactic Clearinghouse | Real-time commodity arbitrage ledger across all active and simulated worlds. Prices float dynamically based on local supply deficits (e.g. food shortages on mining worlds increase grain prices by 300%). | Headless simulation test verifying market price elasticity and automated tariff transfers across 12 game months. |
| **WP-52.2** | Corridor Backpressure & Tariffs | Freight corridors exceeding 80% utilization incur congestion delays and transit surcharge taxes, incentivizing players to build bypass corridors or upgrade to vacuum-tube maglev. | Test verifying congestion delays and player revenue adjustments under heavy traffic loads. |
| **WP-52.3** | Scenario: The Dyson Alpha Crisis | Standalone narrative scenario: astronomical observation detects the sudden disappearance of the Dyson Alpha stars. The player must rush heavy supplies to construct defensive orbital gates and evacuation sidings. | Savegame scenario test: scripted event triggers supply quotas and emergency passenger evacuation orders. |
| **WP-52.4** | Scenario: The Evacuation of Elan | Wartime scenario modeling the emergency evacuation of Elan via Wessex infrastructure. Player must organize high-capacity evacuation EMUs under strict tick countdowns. | Timed scenario test verifying passenger survival metrics and evacuation clearing receipts. |

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
| **51** | Expeditionary Survey &     | Catch2: `test_expeditionary_consist.cpp`                    |
|        | Silfen Intermodal Depots   | End-to-end: Silvergalde -> Jaruva container transshipment   |
+--------+----------------------------+-------------------------------------------------------------+
| **52** | Galactic Hegemony &        | Catch2: `test_galactic_clearinghouse.cpp`                   |
|        | Historical Scenarios       | Scenario Runner: Dyson Alpha Crisis automated playthrough   |
+--------+----------------------------+-------------------------------------------------------------+
```
