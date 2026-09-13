# Sprint 28: Guided Solo UAT & Persistent Story Book Specification

**Status:** COMPLETE  
**Date:** 2026-09-13  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Savegame Artifact:** `demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav`  

---

## 1. Executive Summary

Sprint 28 delivers the official **Guided Solo UAT (User Acceptance Testing)** milestone for OpenSpaceTTD. It supersedes the early historical v0.3 planetary slice by delivering a fresh, reproducible, all-feature savegame (`OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav`) powered by version 7 of the `OpenSpaceTTD-UAT-Demo` GameScript (`bin/game/openspacettd_uat/`).

This release provides:
1. **Persistent Story Book Goals:** 7 structured chapters containing 16 measurable, sequential acceptance goals with clickable viewport location pins spanning every feature implemented from Sprint 9 through Sprint 27.
2. **Dedicated Staging & Testing Fixtures:** Pre-leveled staging ground in World 2 for CST Prefab stamping, sample track layout for player blueprint map capture, and guidance markers for Megacity, Freight Corridors, Trade Ledger, and Federation governance.
3. **Megacity Saveload Persistence (`MEGA` Chunk):** Native RIFF table chunk handler in `src/saveload/planet_sl.cpp` ensuring registered megacities, multi-tier commodity quotas, deliveries, and growth states persist across save/reload cycles.
4. **Reproducible Deterministic Build Pipeline:** Headless multi-pass generation procedure from seed `9032026`, verified binary metadata, and a dedicated 5-suite Catch2 regression test suite (`src/tests/test_sprint28_guided_uat.cpp`).

---

## 2. Savegame Artifact Metadata

| Attribute | Specification / Recorded Value |
|---|---|
| **Relative Path** | `demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav` |
| **Savegame Version** | `367` (OpenTTD SaveLoad format with OpenSpace extensions) |
| **Random Seed** | `9032026` |
| **Map Geometry** | `1024 x 512` tiles, partitioned into 3 isolated worlds with void buffer bands |
| **Landscape Style** | Multi-World Biome Stylization (Temperate Core, Arid Industrial, Sub-Arctic Frontier) |
| **Active GameScript** | `OpenSpaceTTD-UAT-Demo` (OSUD) version `7`, API `16` |
| **Backwards Compatibility** | `MinVersionToLoad() = 1` |
| **File Size** | `533,968` bytes |
| **SHA-256 Checksum** | `6862158fcd1b3503832ee99bb49ec44eded1a240da577d9043bd159fb9ad0911` |

### Headless Reproduction Pipeline

The savegame is deterministically regenerated from clean repository state via:

```bash
# Pass 1: World generation, planetary partition, gateway placement, and global Story Book initialization
./build/openttd -v null:ticks=200 -s null -m null -b null \
  -c demo/uat_demo.cfg -x -G 9032026 -g
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav

# Pass 2: Human company creation, demonstrator train, spaceport, conduit, CST staging pad, and blueprint fixtures
./build/openttd -v null:ticks=1000 -s null -m null -b null \
  -c demo/uat_demo.cfg -x \
  -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav

# Validation: Verify clean load
./build/openttd -q demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
```

---

## 3. Persistent Story Book & Goals Catalogue

The in-game Story Book window (`Manage Company` > `Story Book`) contains 7 structured chapters with 16 persistent goals (`GSGoal`):

```
+-----------------------------------------------------------------------------------------------+
| OpenSpaceTTD Solo UAT Story Book (v7)                                                         |
+-----------------------------------------------------------------------------------------------+
| Chapter 1: Overview & Planetary Navigation       -> Goal 1: Navigate 3 Worlds & Biomes       |
| Chapter 2: Monumental Portal Gates               -> Goals 2-3: Gate Build/Link & Consist Run  |
| Chapter 3: Player Blueprints & CST Prefabs       -> Goals 4-5: CST Stamping & Map Capture     |
| Chapter 4: Planetary Operations                  -> Goals 6-7: Spaceports & Edge Conduits     |
| Chapter 5: Megacity Demands & Freight Corridors  -> Goals 8-9: Demand Tiers & Corridors       |
| Chapter 6: Supply Chain & Federation Governance  -> Goals 10-11: Trade Ledger & Auth/Charters|
| Chapter 7: Commonwealth Data Crystals            -> Goals 12-16: Rebranded Economy & Wagons   |
+-----------------------------------------------------------------------------------------------+
```

### Detailed Acceptance Procedures & Pass/Fail Criteria

#### Chapter 1: Overview & Planetary Navigation
- **Story Page:** `1. Overview & Planetary Navigation`
- **Location Pins:**
  - World 1 Anchor: `Oaktree Core` (TileXY `136, 172`)
  - World 2 Anchor: `Merredin Industrial` (TileXY `486, 172`)
  - World 3 Anchor: `Calyx Frontier` (TileXY `836, 172`)
- **Goal 1:** `1. Navigate all three worlds using Ctrl+Alt+1..3 or the Map menu world jump buttons. Confirm distinct environmental biomes.`
- **Procedure:**
  1. Press `Ctrl+Alt+1`, `Ctrl+Alt+2`, `Ctrl+Alt+3` in sequence, or open the Map dropdown menu and click `Jump to World 1`, `Jump to World 2`, `Jump to World 3`.
  2. Observe the terrain and tree styling across worlds.
- **Pass Criteria:** Viewport jumps immediately to the center of each world. World 1 exhibits temperate grass/trees, World 2 exhibits arid/desert industrial stylization, and World 3 exhibits sub-arctic snow-line/alpine stylization.
- **Fail Criteria:** Hotkeys fail, viewport lands in void space, or biomes are uniform.

#### Chapter 2: Monumental Portal Gates
- **Story Page:** `2. Monumental Portal Gates`
- **Location Pins:**
  - Gateway Alpha - Phase 1 Head (TileXY `298, 142`, NE-SW track axis)
  - Gateway Alpha - Phase 2 Head (TileXY `412, 284`, NW-SE track axis)
  - Gateway Beta - Phase 2 Head (TileXY `648, 284`, NW-SE track axis)
  - Gateway Beta - Phase 3 Head (TileXY `762, 142`, NE-SW track axis)
- **Goal 2:** `2. Build an unlinked portal gate with its automatic 18-tile two-lane terminal, then link it to a destination gate in another world.`
- **Goal 3:** `3. Run a portal consist through Gateway Alpha and verify it emerges smoothly at the non-aligned Phase 2 head on a perpendicular track axis.`
- **Procedure:**
  1. Click the location button for Gateway Alpha Phase 1 head. Observe the `UAT Wormhole Demonstrator` train entering the gate.
  2. Follow the train into Gateway Alpha. Click Gateway Alpha Phase 2 head. Confirm the train emerges and continues along the turnback track.
  3. Open the Railway Construction toolbar and select the Portal Gate tool (`WID_RAT_BUILD_PORTAL`). Build a gate on clear ground; observe the 18-tile terminal. Switch to Link mode and link to a second gate on World 2.
- **Pass Criteria:** Demonstrator train enters Gateway Alpha on NE-SW track and emerges at Phase 2 head on NW-SE track without derailment or teleportation artifacts. New portal construction builds gate and complete terminal atomically.
- **Fail Criteria:** Train blocks indefinitely in wormhole, flips facing direction erratically, or terminal footprint is truncated.

#### Chapter 3: Player Blueprints & CST Prefab Rail Blocks
- **Story Page:** `3. Player Blueprints & CST Prefabs`
- **Location Pins:**
  - CST Prefab Staging Area (World 2: Merredin Industrial, leveled 16x10 pad)
  - Custom Blueprint Capture Layout (World 2: Merredin Industrial, 10x4 sample tracks)
- **Goal 4:** `4. Open Blueprint Library ('B'), select a canonical CST Prefab (e.g. CST Dual-Track Passing Siding or Mainline Double Straight), rotate/flip, and stamp it on the staging area.`
- **Goal 5:** `5. Select 'Capture From Map' in the Blueprint Library, drag across the sample rail layout, save it to your local library, and place a replica.`
- **Procedure:**
  1. Click the CST Prefab Staging Area location button. Press `B` (or click blueprint icon on rail toolbar).
  2. Select `CST Dual-Track Passing Siding` (14x4). Press `R` to rotate 90°, `F` to mirror horizontally (observing RHD/LHD invariance). Click the staging pad to stamp.
  3. Click `Capture From Map` in the library window. Drag a bounding box over the sample track layout. Name it `My Test Crossover` and save. Select it from the library and stamp a replica nearby.
- **Pass Criteria:** CST prefab stamps cleanly with tracks and signals intact. Map capture records tile offsets, tracks, and signals. Both actions execute server-authoritatively via `Commands::PlaceBlueprint`.
- **Fail Criteria:** Window fails to open, CST prefabs are missing, hotkeys do not rotate/flip, or placement rejects valid ground.

#### Chapter 4: Planetary Operations: Spaceports & Conduits
- **Story Page:** `4. Planetary Operations: Spaceports & Conduits`
- **Location Pins:**
  - Phase 1 Spaceport candidate airport (World 1)
  - Phase 3 signed Edge Conduit construction tile (World 3)
- **Goal 6:** `6. Open station window for the Phase 1 Spaceport candidate and designate Spaceport. Upgrade through Tier 2 and Tier 3, observing supply status and projected off-world trade cargo.`
- **Goal 7:** `7. Select the Edge Conduit tool on the rail toolbar and build on the signed Phase 3 void boundary tile. Verify Land Area Information reports 100 units/mo Frontier extraction and mineral cargo.`
- **Procedure:**
  1. Click the Spaceport candidate location. Click the station sign `Phase 1 Spaceport Candidate`.
  2. Click `Designate Spaceport`. Verify Tier 1 status. Click `Upgrade Spaceport to Tier 2` and `Upgrade Spaceport to Tier 3`. Confirm button disables at Tier 3 max.
  3. Click the Edge Conduit location in World 3. Select the Edge Conduit tool on the rail toolbar. Click the signed empty tile directly beside the void buffer.
  4. Query the conduit with `Land Area Information`. Confirm Frontier world attribution, mineral cargo, and 100 units/month extraction rate.
- **Pass Criteria:** Spaceport designations and tier upgrades execute immediately with live telemetry. Conduit placement succeeds beside void and is rejected on interior tiles.
- **Fail Criteria:** Station window crashes or omits spaceport panel; conduit builds on invalid interior tiles.

#### Chapter 5: Megacity Demands & Freight Corridors
- **Story Page:** `5. Megacity Demands & Freight Corridors`
- **Location Pins:**
  - Oaktree Core (Metropolitan Megacity)
  - Gateway Alpha Freight Corridor (Phase 1)
- **Goal 8:** `8. Open Town window > 'Megacity' or Town menu > 'Megacity Overview'. Inspect Tier 1-3 demands and observe growth state transitions under monthly evaluation.`
- **Goal 9:** `9. Open Map dropdown > Freight Corridor Monitor. Inspect inter-world corridor transit volume, capacity utilization, and congestion bottleneck alerts.`
- **Procedure:**
  1. Click the Oaktree Core location pin. Click the town name to open the town window, then click `Megacity`, or select `Megacity Overview` from the Town toolbar menu.
  2. Inspect Tier 1 Sustenance, Tier 2 Expansion, and Tier 3 Prosperity progress bars. Click `Designate Megacity` if un-registered. Unpause and let month roll over to observe supply evaluation and growth state.
  3. Open the Map dropdown menu and choose `Freight Corridor Monitor`. Inspect inter-world route pairs (World 1 <-> World 2), active consist transit counts, capacity utilization bars, and transit health.
- **Pass Criteria:** Megacity window displays all 3 tiers with live quotas and delivery progress. Freight Corridor Monitor displays route utilization and congestion indicators.
- **Fail Criteria:** Windows fail to open, demand tiers are blank, or monthly evaluation crashes.

#### Chapter 6: Supply Chain Matrix & Federation Governance
- **Story Page:** `6. Supply Chain Matrix & Federation Governance`
- **Location Pins:**
  - Federation Administrative Core (`Oaktree Core`)
- **Goal 10:** `10. Open Map dropdown > Supply Chain & Trade Ledger. Inspect macro phase flows, spaceport/conduit infrastructure volume, and verify the CONSERVED commodity trade balance audit.`
- **Goal 11:** `11. Open Map dropdown > Federation Authentication & Charters. Authenticate player identity, charter a corporate entity, and expand world presence to World 1.`
- **Procedure:**
  1. Open Map dropdown menu > `Supply Chain & Trade Ledger` (`STR_MAP_MENU_TRADE_LEDGER`).
  2. Tab 1 (Supply Chain Matrix): Inspect Commonwealth macro flows, spaceport launch volume, conduit extraction volume, and tariffs.
  3. Tab 2 (Trade Balances & Conservation): Inspect bilateral balances and confirm the green `CONSERVED (Zero-Sum Invariant Satisfied)` badge.
  4. Open Map dropdown menu > `Federation Authentication & Charters` (`STR_MAP_MENU_FEDERATION_AUTH`).
  5. Inspect active player session, register or log into player handle, charter a corporate charter, and expand presence to current world.
- **Pass Criteria:** Both windows open from discoverable Map menu entries, display accurate live federation state, and enforce permissions cleanly.
- **Fail Criteria:** Menu entries missing, windows fail to initialize, or conservation badge displays false violation.

#### Chapter 7: Commonwealth Data Crystals Rebranding
- **Story Page:** `7. Commonwealth Data Crystals Rebranding`
- **Location Pins:**
  - Oaktree Core Station & Depot Area
- **Goal 12:** `12. Graphs > Cargo Payment Rates lists Data Crystals.`
- **Goal 13:** `13. Game Settings search shows Distribution mode for data crystals.`
- **Goal 14:** `14. Town station acceptance and waiting lists display Data Crystals.`
- **Goal 15:** `15. Train depot purchase list contains the Data Van wagon.`
- **Goal 16:** `16. Road depot purchase list contains the MPS Data Courier.`
- **Pass Criteria:** All 5 UI locations display `Data Crystals`, `Data Van`, and `MPS Data Courier`. The word `Mail` does not appear anywhere in user-facing strings.
- **Fail Criteria:** Legacy `Mail` appears in any payment graph, settings query, station window, or vehicle purchase list.

---

## 4. Technical Architecture: Megacity Saveload Persistence (`MEGA`)

To guarantee that player-designated megacities survive save/load cycles without data loss, Sprint 28 implemented the `MEGA` table chunk in `src/saveload/planet_sl.cpp`:

```cpp
struct SlMegacity {
    uint32_t town_id;
    uint32_t world_id;
    std::string town_name;
    uint32_t population;
    uint32_t quota_0;
    uint32_t quota_1;
    uint32_t quota_2;
    uint32_t deliv_curr_0;
    uint32_t deliv_curr_1;
    uint32_t deliv_curr_2;
    uint32_t deliv_last_0;
    uint32_t deliv_last_1;
    uint32_t deliv_last_2;
    uint8_t growth_state;
};
```

On load, `MEGAChunkHandler::Load()` calls `MegacityManager::RestoreMegacity(profile)` which reconstructs satisfaction indices and multipliers while maintaining backwards compatibility with older saves lacking the `MEGA` chunk.

---

## 5. Automated Verification Results

### Regression Test Suite (`test_sprint28_guided_uat.cpp`)

5 comprehensive test cases verified under Catch2:
1. `Sprint 28 UAT - Savegame File Artifact & Reproducibility Metadata`: Validates file existence, byte size bounds, and OTTD stream header.
2. `Sprint 28 UAT - GameScript OpenSpaceTTD-UAT-Demo v7 Registration`: Validates `info.nut` version 7, date, and `main.nut` 7-chapter / 16-goal presence and staging methods.
3. `Sprint 28 UAT - CST Prefab Rail Blocks Catalog & Invariance`: Validates all 8 canonical CST layouts, read-only status, and geometric mirror transposition.
4. `Sprint 28 UAT - Megacity Multi-Tier Demand & Growth States`: Validates cargo classification (Food/Goods/Valuables), quotas, deliveries, and Starvation -> Subsistence -> Boom -> HyperGrowth transitions.
5. `Sprint 28 UAT - Script Goal & Story Page Enums and Types`: Validates Squirrel Script API enum bindings.

### Full Test Suite Pass Rate

```bash
ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 232
Total Test time (real) = 6.85 sec
```
