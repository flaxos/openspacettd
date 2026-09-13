# OpenSpaceTTD Sprint 26 — CST Prefab Rail Blocks

Status: **COMPLETE**  
Date: **2026-09-13**  
Reference Specifications: [GAME_DESIGN_AND_TECHNICAL_PLAN.md](GAME_DESIGN_AND_TECHNICAL_PLAN.md), [FEATURE_UI_UAT_COVERAGE.md](FEATURE_UI_UAT_COVERAGE.md), [SPRINT25_PLAYER_RAIL_BLUEPRINTS_2026-09-13.md](SPRINT25_PLAYER_RAIL_BLUEPRINTS_2026-09-13.md)

---

## 1. Executive Summary

Sprint 26 ships the **8 Canonical CST Prefab Rail Blocks** embedded into OpenSpaceTTD's Player Blueprint Library (`BlueprintManager`). Standardized by Commonwealth Synergy Transport (CST), these modular infrastructure blocks provide ready-to-stamp, high-capacity, crash-resilient railway elements engineered specifically for high-speed planetary corridors and inter-world wormhole gateheads.

### Key Highlights
1. **8 Standardized CST Layouts:** From double-track trunks and grade-friendly wye junctions to roll-on/roll-off (Ro-Ro) terminals and wormhole gatehead approach corridors.
2. **Deterministic Stamping & Preservation:** Seamlessly constructed via `Commands::PlaceBlueprint` with multi-world void-space safety checks, track bit isolation, and PBS path signal attachment.
3. **Traffic-Side (RHD / LHD) Adaptation:** Built symmetrically or directionally paired. Players toggle between Right-Hand Drive (RHD) and Left-Hand Drive (LHD) via isometric horizontal reflection (`Flip`, hotkey `F`).
4. **Read-Only Builtin Protection:** Built-in blueprints cannot be deleted or renamed in the Blueprint Library window. Players can stamp them, capture modified sections, and save custom derivatives.
5. **Comprehensive Verification:** 342 assertions across 5 dedicated Catch2 test cases in `src/tests/test_cst_prefabs.cpp` (468 assertions across all blueprint tests; 222/222 CTests pass).

---

## 2. Canonical CST Prefab Catalog

Each prefab is authored by *"Commonwealth Synergy Transport (CST)"* with embedded operating guidance describing signal clearance rules, speed profiles, and traffic orientations.

```text
┌───────────────────────────────────────────────────────────────────────────────────────┐
│                              CST PREFAB RAIL BLOCKS                                   │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ 1. CST Mainline Double Straight (8x2)     - Standard 2-track high-speed trunk         │
│ 2. CST Dual-Track Passing Siding (14x4)   - Offline express overtaking bypass         │
│ 3. CST Portal Gate Approach (10x4)        - Gatehead buffer blocks & crossover loop   │
│ 4. CST High-Speed 3-Way Wye (12x12)       - Grade-friendly triangle junction          │
│ 5. CST 4-Way Roundabout Junction (10x10)  - Circular distribution interchange         │
│ 6. CST Ro-Ro 4-Platform Terminal (12x8)   - 4-platform drive-through terminal         │
│ 7. CST Industrial Bulk Balloon Loop(14x10)- Continuous turnaround loading loop        │
│ 8. CST Depot Maintenance Yard (10x6)      - Dual-bay depot with acceleration merge    │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 CST Mainline Double Straight (8x2)
* **Footprint:** $8 \times 2$ tiles.
* **Composition:** 16 track tiles (8 Track X per lane).
* **Signaling:** 2 one-way electric path signals (one SW-bound on lane $y=0$, one NE-bound on lane $y=1$).
* **Guidance:** Dual-track high-speed mainline corridor with directional path signaling. Standard trunk module for linking planetary regions. Default configuration is Right-Hand Drive (RHD); use Mirror (F) for Left-Hand Drive (LHD).

### 2.2 CST Dual-Track Passing Siding (14x4)
* **Footprint:** $14 \times 4$ tiles.
* **Composition:** Mainline dual tracks ($y=1, 2$) plus an offline bypass siding ($y=3$) connected via turnout switches.
* **Signaling:** 5 one-way path signals (mainline through-lanes and siding entrance/exit blocks).
* **Guidance:** High-throughput dual mainline with an offline passing siding. Allows fast express passenger trains to overtake heavy freight consists without blocking through-corridor traffic.

### 2.3 CST Portal Gate Approach Corridor (10x4)
* **Footprint:** $10 \times 4$ tiles.
* **Composition:** 20 tiles (22 track pieces including crossover turnout bits).
* **Signaling:** 4 one-way path signals forming entry and exit control blocks.
* **Guidance:** Gatehead approach corridor engineered for CST wormhole portals. Features dual signal buffer blocks preventing portal choke points and an emergency crossover loop for train turnaround during gate recalibration.

### 2.4 CST High-Speed 3-Way Wye Junction (12x12)
* **Footprint:** $12 \times 12$ tiles.
* **Composition:** 44 tiles (46 track pieces including junction switches).
* **Signaling:** 6 one-way path signals protecting entry branches and junction throats.
* **Guidance:** Grade-friendly triangular junction connecting 3 dual-track corridors without diamond crossing conflicts. Smooth curves maintain express train velocity. Symmetrical design works for both RHD and LHD operations.

### 2.5 CST 4-Way Compact Roundabout Junction (10x10)
* **Footprint:** $10 \times 10$ tiles.
* **Composition:** 32 tiles (36 track pieces).
* **Signaling:** 8 one-way path signals (4 entry throat signals, 4 circulating signals).
* **Guidance:** Symmetric 4-way circular distribution junction for regional networks. Distributes traffic between north, south, east, and west corridors with integrated path signal arbitration.

### 2.6 CST Ro-Ro 4-Platform Terminal Station Block (12x8)
* **Footprint:** $12 \times 8$ tiles.
* **Composition:** 24 station platform tiles (4 tracks of length 6) flanked by entry ladder throat tracks and exit collection tracks.
* **Signaling:** 8 one-way path signals (4 entry ladder block signals, 4 platform exit starter signals).
* **Guidance:** Roll-on/roll-off 4-platform terminal station (length 6 per platform). Dual ladder throat feeds all platforms simultaneously while dedicated exit throat maintains forward momentum without reversing.

### 2.7 CST Industrial Bulk Balloon Loop (14x10)
* **Footprint:** $14 \times 10$ tiles.
* **Composition:** Inbound lead, 10 station platform tiles (2 parallel tracks of length 5), balloon turnaround curve, and outbound bypass.
* **Signaling:** 3 one-way path signals.
* **Guidance:** Continuous-flow unidirectional balloon turnaround loop with integrated 2-platform bulk loading siding. Designed for continuous-motion ore, mineral, and grain loading without locomotives needing to uncouple or reverse.

### 2.8 CST Depot Maintenance Staging Yard (10x6)
* **Footprint:** $10 \times 6$ tiles.
* **Composition:** 20 mainline track tiles, 2 train depots facing NE (`DiagDirection::NE`), branch throat track, and acceleration escape track.
* **Signaling:** 3 one-way path signals (2 mainline through-signals, 1 depot acceleration exit signal).
* **Guidance:** Offline dual-depot service facility. Mainline double-track bypass with dedicated depot branch, two service bays, and an acceleration merge track preventing mainline disruptions during fleet staging.

---

## 3. Geometric Transformations & Traffic Orientation

The 8 CST prefabs are designed to support both global traffic rule standards:

1. **Right-Hand Drive (RHD) vs. Left-Hand Drive (LHD):**
   - By default, all prefabs are constructed with Right-Hand Drive traffic flows.
   - Calling `Blueprint::Mirror()` (or pressing hotkey `F` in the Blueprint Library GUI) transposes coordinates $(x', y') = (y, x)$ and swaps diagonal track axes ($X \leftrightarrow Y$), accurately transforming RHD layouts into LHD layouts while maintaining signal orientation and platform alignments.
2. **Orthogonal Rotations ($90^\circ, 180^\circ, 270^\circ$):**
   - Pressing `Rotate` (hotkey `R`) performs mathematical coordinate rotation $(x', y') = (H - 1 - y, x)$, cycling through all 4 cardinal railway headings.
   - Permutation tables ensure signals, depot orientations, and station axes cleanly update.
3. **Four-Quadrant Invariance:**
   - Rotating any CST prefab 4 times ($360^\circ$) restores the exact original geometry and piece count.
   - Mirroring any CST prefab twice ($F \circ F$) restores the exact original geometry and piece count.

---

## 4. Test & Verification Evidence

Sprint 26 adds a dedicated Catch2 test suite in `src/tests/test_cst_prefabs.cpp` covering:
* **`CST Prefab Builtin Registration and Metadata`:** Verifies all 8 prefabs are registered with non-empty names, descriptions, CST author attribution, version 1, and `is_builtin == true`.
* **`CST Prefab Structural Integrity and Element Counts`:** Validates exact width, height, track counts, signal counts, station platforms, and depot counts matching engineering specifications.
* **`CST Prefab Geometric Transforms and Traffic Invariance`:** Stresses 4-cycle $360^\circ$ rotation invariance and 2-cycle reflection involution across all 8 prefabs.
* **`CST Prefab In-Game Map Placement`:** Executes `CmdPlaceBlueprint` with `CST Dual-Track Passing Siding`, validating that all 14x4 tiles, track bits, and one-way path signals are placed deterministically on the map.
* **`CST Prefab Immutability and Custom Derivative Stamping`:** Proves that built-in prefabs cannot be deleted or renamed in `BlueprintManager`, but can be cloned into custom blueprints and modified.

### Test Results
```bash
$ ./build/openttd_test [cst_prefab]
Filters: [cst_prefab]
===============================================================================
All tests passed (342 assertions in 5 test cases)

$ ./build/openttd_test [blueprint],[cst_prefab]
Filters: [blueprint],[cst_prefab]
===============================================================================
All tests passed (468 assertions in 11 test cases)

$ ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 222
```
