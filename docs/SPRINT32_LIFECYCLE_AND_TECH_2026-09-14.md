# Sprint 32: Planetary Settlement Lifecycle, Economy-Driven Phase Promotion, and Technology Progression Engine

**Status:** COMPLETE  
**Date:** 2026-09-14  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Test Suite:** `src/tests/test_sprint32_lifecycle_and_tech.cpp`  
**Acceptance Kit:** `scripts/test_sprint32_lifecycle_kit.py`  

---

## 1. Executive Summary

Sprint 32 delivers the complete **Planetary Settlement Lifecycle, Economy-Driven Phase Promotion, and Technology Progression Engine** for OpenSpaceTTD. This bridges the physical railway infrastructure with planetary socio-economic development, creating an organic feedback loop where logistics investment directly elevates worlds through the canonical Commonwealth development hierarchy.

Key accomplishments in Sprint 32:
1. **Commonwealth Traction & Track Progression Engine (`PlanetManager::CheckTrackPlacement`):**
   - Implemented strict technology tier enforcement on track construction (`CmdBuildSingleRail`, `CmdBuildRailroadTrack`, `CmdConvertRail`).
   - Inter-planetary void buffer space is strictly protected against track placement (`STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE`).
   - Wilderness worlds (`Phase 4 Expansion`): Only basic pioneer standard track (`RAILTYPE_RAIL`) may be laid. High-tech electrified, monorail, and maglev tracks are restricted until civilization arrives (`STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD`).
   - Frontier worlds (`Phase 3 Frontier`): Standard un-electrified rail and high-voltage catenary electric rail (`RAILTYPE_ELECTRIC`) are permitted. High-speed Maglev and Monorail guideways are prohibited (`STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD`).
   - Developed worlds (`Phase 2 Developed`): Standard, electrified, and monorail networks are permitted. CST Vacuum-Tube Maglev is restricted to Phase 1 Core worlds (`STR_ERROR_CANNOT_BUILD_ON_DEVELOPED_WORLD`).
   - Core worlds (`Phase 1 Core`): All rail technologies, including 1,000+ km/h CST Vacuum Maglev, are fully unlocked.
2. **Economy-Driven Planetary Development Points Accumulation (`PlanetManager::RecordCargoDelivery`):**
   - Connected cargo deliveries at stations (`DeliverGoods` in `src/economy.cpp`) directly to destination world growth.
   - Local intra-world deliveries grant standard development score increments (1 pt / 10 units).
   - Interplanetary imports arriving from distant worlds grant a $5\times$ growth premium (5 pts / 10 units), rewarding players for establishing multi-world supply chains.
3. **Server-Authoritative Multi-Tier Phase Promotion (`Commands::PromoteWorld` / `CmdPromoteWorld`):**
   - Governs the elevation of colonized worlds along the developmental hierarchy:
     - `Phase 4 Expansion` $\to$ `Phase 3 Frontier` (via Outpost Founding, 100 threshold).
     - `Phase 3 Frontier` $\to$ `Phase 2 Developed` (requires $\ge 2,000$ development points).
     - `Phase 2 Developed` $\to$ `Phase 1 Core` (requires $\ge 5,000$ development points).
   - Validates threshold criteria, assesses civic elevation fees, updates development scores, and broadcasts empire-wide news alerts (`STR_NEWS_WORLD_DEVELOPED` and `STR_NEWS_WORLD_CORE_METROPOLIS`).
4. **In-Game Universe Directory GUI Enhancement (`UniverseDirectoryWindow`):**
   - Added `Promote World` button (`WID_UD_PROMOTE_BTN`) with contextual enablement based on real-time eligibility (`PlanetManager::CanPromoteWorld`).
   - Dynamic progress metrics rendering in details panel (`Development: X / Y pts`).
   - Operators and players can inspect requirements and trigger elevation directly from the in-game UI.
5. **Interactive Console Control (`promote_world`):**
   - Added `promote_world <world_id>` console command for dedicated server operators.
6. **Universe Authority Federation REST API & Persistence:**
   - Expanded both C++ `UniverseAuthorityService` and Python `universe_authority.py` with `POST /worlds/<id>/promote`.
   - Verified `--state-file` checkpoint crash persistence across daemon `SIGKILL` terminations and port re-bindings.
7. **Comprehensive Verification:**
   - 4 Catch2 unit test cases in `src/tests/test_sprint32_lifecycle_and_tech.cpp`.
   - 5 automated end-to-end acceptance scenarios in `scripts/test_sprint32_lifecycle_kit.py` passing 100%.

---

## 2. Commonwealth Technology & Track Progression Matrix

| World Phase | Economic & Settlement Status | Permitted Track Types | Prohibited Track Types | Error String |
|---|---|---|---|---|
| **Phase 4 (Expansion)** | Virgin wilderness; uncolonized. | Standard Rail (`RAILTYPE_RAIL`) | Electric, Monorail, Maglev | `STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD` |
| **Phase 3 (Frontier)** | Pioneer colonies, mining outposts. | Standard Rail, Electric (`RAILTYPE_ELECTRIC`) | Monorail, Maglev | `STR_ERROR_CANNOT_BUILD_ON_FRONTIER_WORLD` |
| **Phase 2 (Developed)** | Industrialized processing centers. | Standard, Electric, Monorail (`RAILTYPE_MONORAIL`) | Vacuum Maglev | `STR_ERROR_CANNOT_BUILD_ON_DEVELOPED_WORLD` |
| **Phase 1 (Core)** | Urban metropolises & arcologies. | All types including Maglev (`RAILTYPE_MAGLEV`) | None (Fully Unlocked) | *N/A* |
| **Void Buffer** | Inter-planetary buffer space. | None | All Track Types | `STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE` |

---

## 3. Development Points & Phase Promotion Hierarchy

### 3.1 Cargo Delivery Inflow
Every accepted cargo consignment at a station invokes:
```cpp
PlanetManager::RecordCargoDelivery(st->xy, cargo_type, accepted_total, src_tile);
```
- **Local Shipments:** $\text{Points} = \max(1, \text{units} / 10)$
- **Inter-World Imports:** $\text{Points} = \max(3, \text{units} / 2)$

### 3.2 Promotion Thresholds
```cpp
static constexpr uint32_t DEVELOPMENT_THRESHOLD_FRONTIER = 100;   // Outpost founding
static constexpr uint32_t DEVELOPMENT_THRESHOLD_DEVELOPED = 2000; // Frontier -> Developed
static constexpr uint32_t DEVELOPMENT_THRESHOLD_CORE = 5000;      // Developed -> Core
```

---

## 4. Verification Commands

```bash
# Compile and run Catch2 unit tests
ninja -C build openttd openttd_test
./build/openttd_test "Sprint 32*"

# Run full CTest regression suite
ctest --test-dir build --output-on-failure

# Run multi-server federation acceptance suite
bash scripts/run_acceptance_kit.sh
```
