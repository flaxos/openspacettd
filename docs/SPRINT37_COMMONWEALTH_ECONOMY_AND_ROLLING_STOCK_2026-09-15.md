# Sprint 37 Milestone Report: Commonwealth Economy and Rolling-Stock Pack

Status: **COMPLETED & AUTOMATED VERIFIED**  
Date: **2026-09-15**  
Branch: `fix/portal-gate-lifecycle-crashes`  
CTest Coverage: **299 CTest cases passing cleanly (100% pass rate, 0 failures)**

---

## 1. Executive Summary

Sprint 37 closes the core in-tree content gap for OpenSpaceTTD by delivering the canonical **Commonwealth Economy and Rolling-Stock Pack**. It translates the abstract fabrication and multi-world production concepts developed in Sprints 39–42 into concrete, reproducible in-tree packages, linking Commonwealth Space Transportation (CST) train families to the Commonwealth Tech Tree (`TechTreeManager`), enforcing planetary world phase operational eligibility, and certifying closed delivery loops for all 12 Commonwealth cargos across Pipelines A–D.

---

## 2. Key Deliverables & Architecture

### 2.1 In-Tree NML Source Packages
1. **Industry & Cargo Pack (`pkg/commonwealth_industry/`):**
   - Source: `commonwealth_industry.nml` (GPL-2.0).
   - GRF ID: `OST\x01` (`0x0154534F`).
   - 12 Commonwealth cargos defined (`SILC`, `IRON`, `STEL`, `COPR`, `WIRE`, `SAND`, `CHIP`, `RARE`, `ALLO`, `POLY`, `BCRY`, `QCRY`, plus `CCRY` encrypted consumer crystals).
   - Extraction and processing facilities across Pipelines A–D: Quarries, Ballast Crushers, Iron Mines, Steel Mills, Copper Mines, Wire Smelters, Quartz Dunes, Silicon Arcologies, Rare Earth Mines, Superalloy Foundries, Polymer Complexes, Monocrystal Fabs, Quantum Observatories, and Megacity Formatting Centers.
   - Localized English string table: `lang/english.lng`.
   - License and documentation: `license.txt`, `README.md`.

2. **Rolling-Stock Pack (`pkg/commonwealth_rail/`):**
   - Source: `commonwealth_rail.nml` (GPL-2.0).
   - GRF ID: `OST\x02` (`0x0254534F`).
   - CST train families and dedicated rolling stock for all 12 cargos:
     - `cst_pioneer_steam`: CST Pioneer 0-6-0 'Surveyor' (Steam, Baseline)
     - `cst_vulcan_steam`: Vulcan 2-8-0 'Frontier Hauler' (Steam, `TECH_TRACTION_1`)
     - `cst_titan_diesel`: Titan D-100 Twin-Engine Hauler (Diesel, `TECH_TRACTION_2`)
     - `cst_e40_electric`: CST E-40 Inter-World Catenary Hauler (Electric, `TECH_TRACTION_3`)
     - `cst_mark4_maglev`: CST Mark IV 'Chimaera' Hyper-Maglev (Maglev, `TECH_TRACTION_4`)
     - Specialized wagons: Passenger Coach, Vault Van, Heavy Ore Hopper, Steel Flatcar, Cryogenic Container Car, Pressurized Chemical Tanker, CST Maglev Cargo Pod.
   - Localized English string table: `lang/english.lng`.
   - License and documentation: `license.txt`, `README.md`.

### 2.2 Reproducible Build & Bytecode Compiler Pipeline
- **Generator Script:** `scripts/build_commonwealth_grf.py`.
- Generates bit-for-bit reproducible OpenTTD Container version 1 binary GRFs:
  - `pkg/commonwealth_industry/openspacettd_industry.grf`
  - `pkg/commonwealth_rail/openspacettd_rail.grf`
  - `bin/data/openspacettd_industry.grf`
  - `bin/data/openspacettd_rail.grf`
- Implements Action 8 metadata, Action 14 compatibility dictionaries, Action 4 localized strings, and Action 0 property definitions.
- Generates JSON manifest (`pkg/commonwealth_manifest.json`) recording byte lengths, MD5, and SHA256 checksums.

### 2.3 Core C++ Engine Integration (`CommonwealthPackManager`)
- Implemented in `src/portal/commonwealth_pack.h` and `src/portal/commonwealth_pack.cpp`.
- **CST Locomotive Specifications:** Full stat registry (speed, horsepower, weight, tractive effort, engine class).
- **Tech Tree Research Gating:**
  - Vulcan 2-8-0 requires `TECH_TRACTION_1` (High-Adhesion Steam).
  - Titan D-100 requires `TECH_TRACTION_2` (Multi-Unit Heavy Diesel).
  - CST E-40 requires `TECH_TRACTION_3` (High-Voltage Electrics).
  - CST Mark IV requires `TECH_TRACTION_4` (CST Vacuum Maglev & Vactrains).
- **World Phase Operational Restrictions:**
  - CST Mark IV Hyper-Maglev: Phase 1 Core worlds only.
  - CST E-40 Electric: Phase 1 Core and Phase 2 Developed worlds.
  - Titan D-100 Diesel: Phase 2 Developed and Phase 3 Frontier worlds.
  - Vulcan 2-8-0 Steam: Phase 3 Frontier and Phase 4 Expansion worlds.
  - Pioneer 0-6-0 Steam: All worlds.
- **Engine Build Hooks:**
  - `IsEngineBuildable()` in `src/engine.cpp` checks `CommonwealthPackManager::IsVehicleBuildableForCompany()`.
  - `CmdBuildVehicle()` in `src/vehicle_cmd.cpp` evaluates world-phase eligibility for the target depot tile.
- **Universe Content Manifest Admission:**
  - Integrated into `UniverseContentManifest` with `RegisterPacksInContentManifest()`.
  - Encodes and validates tokens for strict multiplayer compatibility.

### 2.4 Complete 12-Cargo Economy Closed Delivery Loops
Every cargo in the 12-cargo Commonwealth suite is verified with a complete production $\to$ haulage $\to$ consumption cycle:
1. **Silicates & Ballast Slag (`SILC`):** Quarries $\to$ Ore Hoppers $\to$ Ballast Crusher / In-Kind Trackbed.
2. **Iron Ore (`IRON`):** Deep Iron Mines $\to$ Ore Hoppers $\to$ Steel Smelter & Mill.
3. **Structural Steel (`STEL`):** Steel Mill $\to$ Steel Flatcars $\to$ Stockpiles & Megacity Tier 2.
4. **Copper Ore (`COPR`):** Copper Mines $\to$ Ore Hoppers $\to$ Conductive Wire Smelter.
5. **Conductive Wiring (`WIRE`):** Smelter $\to$ Flatcars/Cryo Cars $\to$ Electrification & Electronics.
6. **Silica Sand (`SAND`):** Silica Dunes $\to$ Ore Hoppers $\to$ Silicon Arcology & Monocrystal Fabs.
7. **Silicon Chips (`CHIP`):** Silicon Arcology $\to$ Cryo Containers $\to$ PBS Signalling & Megacity Tier 3.
8. **Rare Earth Minerals (`RARE`):** Extraction Mines $\to$ Ore Hoppers $\to$ Superalloy Foundries & Monocrystal Fabs.
9. **Superalloys (`ALLO`):** Superalloy Foundries $\to$ Cryo/Maglev Pods $\to$ Maglev Guideways & Megacity Tier 2.
10. **Synthetic Polymers (`POLY`):** Polymer Complexes $\to$ Tankers/Cryo Cars $\to$ Train Shells & Megacity Tier 2.
11. **Blank Data Crystals (`BCRY`):** Monocrystal Fabs $\to$ Vault Vans $\to$ Observatories & Megacity Formatting.
12. **Enriched Quantum Crystals (`QCRY`):** Quantum Observatories $\to$ Vault Vans $\to$ Phase 1 Corporate HQ R&D.
13. **Encrypted Consumer Crystals (`CCRY`):** Megacity Formatting $\to$ Vault Vans $\to$ Commonwealth Towns (Prosperity).

---

## 3. Verification & Acceptance Record

### 3.1 Unit & Regression Test Suite
Implemented in `src/tests/test_sprint37_commonwealth_pack.cpp`:
- `Sprint 37: In-Tree NML Package Source Integrity`: PASSED
- `Sprint 37: Reproducible Binary GRF Container Synthesis`: PASSED
- `Sprint 37: Content Admission Boundary and Manifest Integration`: PASSED
- `Sprint 37: CST Rolling Stock Specifications and Tech Tree Gating`: PASSED
- `Sprint 37: World Phase Operational Restrictions`: PASSED
- `Sprint 37: CST Vehicle In-Kind Fabrication BOM Linkage`: PASSED
- `Sprint 37: Complete 12-Cargo Economy Closed Delivery Loops`: PASSED

### 3.2 Full CTest Execution
```text
100% tests passed, 0 tests failed out of 299
Total Test time (real) = 8.19 sec
```

---

## 4. Acceptance Boundary & Next Roadmap Step

- **Sprint 37 is hereby COMPLETED and AUTOMATED VERIFIED.**
- In-tree NML source packages, reproducible GRF builder, CST rolling stock, Tech Tree integration, and closed 12-cargo loops are verified.
- **Next Planned Work:** Sprint 38 — Bespoke World and CST Art (Original terrain, flora, portal, arcology, station, and infrastructure source art).
