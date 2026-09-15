# Sprint 42 — Factorio-Scale Multi-World Production Chains

**Date:** 2026-09-15  
**Status:** Completed and Verified  
**Milestone:** Commonwealth 12-Cargo Production Suite & Planetary Industrial Chains  
**Test Coverage:** 7 test cases, 79 assertions in `src/tests/test_sprint42_production_chains.cpp`; 292/292 tests passing project-wide in CTest (100%).

---

## 1. Executive Summary

Sprint 42 establishes **Factorio-Scale Multi-World Production Chains** (harmonized with the Sprint 37 Commonwealth industry framework). It bridges the Commonwealth Tech Tree (Sprint 41), In-Kind Fabrication (Sprint 40), Planetary Stockpiles & Dedicated Logistics Hubs (Sprint 39), and Megacity Multi-Tier Demand into a closed, multi-world industrial ecology inspired by Peter F. Hamilton's *Commonwealth Saga*.

Raw mineral and bulk extraction on Phase 3 Frontier worlds is transported through inter-planetary wormholes to heavy processing arcologies on Phase 2 Developed worlds, producing high-tech components, structural materials, and blank crystal matrices. These in turn feed both Phase 1 Core Megacities (civil express mail formatting and prosperity quotas) and Phase 3 Deep-Space Quantum Observatories, which imprint mathematical proofs into **Enriched Quantum Data Crystals** shipped to Corporate HQ stockpiles to power advanced R&D.

---

## 2. Canonical 12-Cargo Commonwealth Suite

The full 12-cargo Commonwealth suite maps raw extractions to precision intermediates and scientific feedstocks:

| Cargo ID | Category | Primary Origin World | Role & Description |
|---|---|---|---|
| `StoneSlag` | Raw Bulk | Phase 3 Frontier / Volcanic | Quarried rock and furnace slag, crushed into aggregate ballast & concrete. |
| `IronOre` | Raw Bulk | Phase 3 Frontier / Sub-Arctic | Deep-mined iron ore smelted into structural steel rails and girders. |
| `StructuralSteel` | Refined Metal | Phase 2 Developed | Heavy structural steel used for track, depots, and vehicle chassis. |
| `CopperOre` | Raw Mineral | Phase 3 Frontier / Arid | Mined copper ore refined into conductive wiring and inductive coils. |
| `ConductiveWiring` | Intermediate Electrical | Phase 2 Developed | Drawn copper wire and catenary coils for electrification and dynamos. |
| `SilicaSand` | Raw Mineral | Phase 3 Frontier / Arid | Quartz and silica dunes refined into silicon wafers and blank crystals. |
| `SiliconChips` | High-Tech Component | Phase 2 Developed | Monocrystalline silicon wafers, microchips, and PBS signalling logic. |
| `RareEarthMinerals` | Exotic Raw | Phase 3 Frontier / Volcanic | Lanthanide and neodymium minerals for superalloys and crystal doping. |
| `Superalloys` | Advanced Material | Phase 2 Developed | High-temperature superconducting alloys for cryo-bogies & maglev. |
| `SyntheticComposites`| Advanced Material | Phase 2 Developed | Aerodynamic polymer body shells and rolling stock coach interiors. |
| `BlankCrystals` | Precision Intermediate | Phase 2 Developed | Unformatted monocrystalline substrate matrix for data storage. |
| `EnrichedQuantumCrystals`| Scientific Feedstock | Phase 3 Frontier / Phase 4 | Imprinted with cosmological telemetry proofs for Corporate HQ R&D. |
| `EncryptedConsumerCrystals`| Civil Express Freight | Phase 1 Core (Megacity) | High-security encrypted human communications (replaces mail). |

---

## 3. Four Interlocking Production Pipelines

```
══════════════════════════════════════════════════════════════════════════════════════════════
PIPELINE A: STRUCTURAL & TRACK INFRASTRUCTURE
══════════════════════════════════════════════════════════════════════════════════════════════
[Frontier Quarries]    ──► Stone / Slag     ──► [Crusher Plant]     ──► Ballast & Concrete
[Frontier Iron Mines]  ──► Iron Ore         ──► [Blast Furnace]     ──► Structural Steel
[Steel + Rare Earths]  ──► Metallurgy       ──► [Superalloy Arc]    ──► Superalloys & Superconductors

══════════════════════════════════════════════════════════════════════════════════════════════
PIPELINE B: ELECTRONICS, SIGNALLING & CATENARY
══════════════════════════════════════════════════════════════════════════════════════════════
[Frontier Copper Mines]──► Copper Ore       ──► [Copper Smelter]    ──► Conductive Wiring & Coils
[Silica Dunes]         ──► Silica Sand      ──► [Silicon Arcology]  ──► Silicon Wafers & Chips
[Chips + Wiring]       ──► Electronics Fab  ──► [Signalling Works]  ──► PBS Relays & Telemetry Logic

══════════════════════════════════════════════════════════════════════════════════════════════
PIPELINE C: ADVANCED TRAIN PROPULSION
══════════════════════════════════════════════════════════════════════════════════════════════
[Chemical Refineries]  ──► Hydrocarbons     ──► [Polymer Complex]   ──► Synthetic Composites
[Superalloys + Wiring] ──► Cryo-Assembly    ──► [Maglev Works]      ──► CST Guideways & Cryo-Bogies

══════════════════════════════════════════════════════════════════════════════════════════════
PIPELINE D: DATA CRYSTAL ENRICHMENT & SCIENTIFIC R&D
══════════════════════════════════════════════════════════════════════════════════════════════
[Silica Sand + Rare Earths] ──► [Monocrystal Synthesis Fab] ──► Blank Data Crystals
                                                                        │
                   ┌────────────────────────────────────────────────────┴────────────────────────────────────────┐
                   ▼                                                                                             ▼
       [Phase 1 Megacity Formatters]                                                  [Phase 3 Deep-Space Quantum Observatories]
       • Formats blank crystal matrices into civil comms                              • Imprints frontier cosmological tensors & math proofs
       • Outputs: Encrypted Consumer Crystals (Mail)                                  • Outputs: Enriched Quantum Data Crystals
       • Delivers high transit revenue & prosperity                                   • Shipped to Corporate HQ stockpile for Tech Tree R&D
```

---

## 4. World Phase & Biome Enforcement

To preserve OpenSpaceTTD's planetary specialisation:
- **Phase 3 Frontier:** Raw bulk and mineral extraction (Quarries, Mines, Silica Dunes) + Deep-Space Quantum Telemetry Arrays. Heavy secondary processing is rejected.
- **Phase 2 Developed:** High-throughput secondary refining and fabrication (Smelters, Foundries, Silicon Arcologies, Polymer Works, Monocrystal Fabs). Raw extraction is rejected.
- **Phase 1 Core:** Metropolitan consumer formatting, advanced assembly, Megacity consumption tiers, and Corporate HQ campuses.

---

## 5. Monthly Production Simulation & Logistics Buffering

Inside the monthly economy timer `_economy_spaceports_conduits_monthly`:
```cpp
ProductionChainManager::ProcessMonthlyProduction();
```

1. **Input Verification:** Calculates available batches based on input buffers and recipe stoichiometry.
2. **Batch Execution:** Deducts inputs and increments `last_month_production` and `total_produced`.
3. **Nanofabrication Yield Synergy:** When the facility owner has researched `TECH_MATERIALS_3` (Automated Nanofabrication Lines), output yield is boosted by **+15%**.
4. **Logistics Hub Buffering:** If the facility owner has established a `LogisticsHub` on the world, manufactured goods are automatically deposited directly into the company's planetary stockpile (`StockpileManager::AddCargo`).

---

## 6. Save/Load Serialization (`PROD` Chunk)

Savegame compatibility is maintained with table chunk handler `PROD` registered in `src/saveload/planet_sl.cpp`:
- `kind = 0`: Facility definition (facility_id, tile, world_id, recipe_id, owner, station_id, capacity, last_production, total_produced).
- `kind = 1`: Input buffer entries.
- `kind = 2`: Output buffer entries.

---

## 7. Verification & Acceptance

Automated test suite `src/tests/test_sprint42_production_chains.cpp`:
1. `Sprint 42 Production Chains - Canonical Cargoes and Recipe Catalog`: All 13 cargo types and 10 canonical recipes across Pipelines A–D.
2. `Sprint 42 Production Chains - World Phase Placement Constraints`: WorldPhase verification (Core vs Developed vs Frontier).
3. `Sprint 42 Production Chains - Facility Lifecycle & Input-to-Output Conversion`: Input deduction, output generation, and withdrawal.
4. `Sprint 42 Production Chains - Multi-Input Superalloy Foundry & Nanofab Perk`: Multi-input stoichiometry and +15% yield bonus from `TECH_MATERIALS_3`.
5. `Sprint 42 Production Chains - Quantum Observatory Telemetry & R&D Loop`: Complete end-to-end loop: Blank Crystals $\to$ Observatory Telemetry $\to$ Enriched Crystals $\to$ Corporate HQ Stockpile $\to$ Tech Tree R&D burning.
6. `Sprint 42 Production Chains - Logistics Hub Stockpile Auto-Buffering`: Automatic buffering into planetary stockpile when a Logistics Hub is present.
7. `Sprint 42 Production Chains - Save/Load Serialization (PROD Chunk)`: Serialization and round-trip state restoration.

**Test Run Results:**
```
ninja -C build openttd_test && ./build/openttd_test "Sprint 42*"
All tests passed (79 assertions in 7 test cases)

ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 292
Total Test time = 7.87 sec
```
