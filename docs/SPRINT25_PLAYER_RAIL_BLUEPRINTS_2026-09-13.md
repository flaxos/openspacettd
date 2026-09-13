# OpenSpaceTTD Sprint 25 — Player Rail Blueprints

Status: **COMPLETE**  
Date: **2026-09-13**  
Reference Specifications: [GAME_DESIGN_AND_TECHNICAL_PLAN.md](GAME_DESIGN_AND_TECHNICAL_PLAN.md), [FEATURE_UI_UAT_COVERAGE.md](FEATURE_UI_UAT_COVERAGE.md), [SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md](SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md)

---

## 1. Executive Summary

Sprint 25 implements a comprehensive **Player Rail Blueprint & Library System** for OpenSpaceTTD. Players can now capture arbitrary rectangular rail layouts from the game map, save them into a persistent library, rotate and flip them geometrically, export/import them via human-readable JSON files, and deterministically stamp them anywhere across the Commonwealth railway network.

The blueprint system preserves all core OpenSpaceTTD architectural rules:
1. **Deterministic Lockstep Simulation:** Stamping is executed strictly through the server-authoritative command pipeline via `Commands::PlaceBlueprint`.
2. **TileIndex & Map Preservation:** Preserves standard `TileIndex`, coordinates, and void buffer safeguards.
3. **Multi-Element Infrastructure Coverage:** Seamlessly captures and constructs plain tracks, 6 signal types (Block, Entry, Exit, Combo, Path, One-Way Path) in electric/semaphore variants, train depots with 4-way facing directions, and rail stations.
4. **Wormhole Portal Boundary Safety:** Approach tracks leading to CST portal gates can be captured and stamped; portal gate heads and unlinked gateway structures are protected from duplication and overwrite.
5. **Multi-Region Localization:** Synchronized across `english.txt`, `english_US.txt`, and `english_AU.txt`.

---

## 2. Key Architecture & Deliverables

### 2.1 Blueprint Data Model & JSON Serialization (`src/blueprint/blueprint.*`)

* **Sparse Blueprint Representation:** Blueprints store bounding box dimensions (`width`, `height`), metadata (`name`, `description`, `author`, `version`, `is_builtin`, `created_time`), and an array of relative sparse tile entries (`dx`, `dy`, `BlueprintTileType`, `railtype`).
* **Tile Elements Supported:**
  - `BlueprintTileType::Track`: Supports arbitrary `TrackBits` combinations and per-track `BlueprintSignal` definitions (`track`, `sigtype`, `sigvar`, `signals_copy`).
  - `BlueprintTileType::Depot`: Captures depot entrance direction (`DiagDirection`).
  - `BlueprintTileType::Station`: Captures platform orientation (`Axis`), station class ID (`StationClassID`), and specification index.
* **Portable JSON Interchange:** High-performance, schema-validated JSON serialization using `3rdparty/nlohmann/json.hpp`. Saved blueprints are portable across installations and platforms.

### 2.2 Geometric Transformations: Rotation & Horizontal Reflection

* **Orthogonal 90° CW Rotations:**
  - Mathematical tile transformation: $(x', y') = (H - 1 - y, x)$, swapping bounding dimensions $W' = H, H' = W$.
  - 12-way `Trackdir` and track permutation tables preserving connectivity across Track X, Y, Upper/Lower, and Left/Right curves.
  - Complete 8-way signal direction rotation, 4-way depot direction rotation, and station axis toggle ($X \leftrightarrow Y$).
  - Full cyclic invariance verified: 4 successive 90° rotations restore the exact original layout and track states.
* **Isometric Horizontal Reflection (Mirroring):**
  - Mathematical tile transformation: $(x', y') = (y, x)$, swapping width and height.
  - Track permutation swaps diagonal axes ($X \leftrightarrow Y$) and mirrors junction turnouts.
  - Invariant involution verified: Applying mirror twice is an exact identity transform.

### 2.3 Blueprint Manager & Filesystem Storage (`src/blueprint/blueprint_manager.*`)

* **Persistent Filesystem Storage:** Managed in OpenTTD's `Subdirectory::Blueprint` (`blueprint/` folder within user data directory).
* **Sanitized Filenames:** Clean filesystem naming protecting against filesystem path traversals and invalid filename characters.
* **Built-in CST Prefabs:** Seeded default templates including:
  - *CST Dual-Track Passing Loop (12x4)*
  - *CST Portal Gate Approach Y-Junction (8x6)*
  - *CST 4-Way Trumpet Interchange (16x16)*
* **Read-Only Builtin Protection:** Built-in prefabs cannot be deleted or overwritten in-game; players can clone, modify, and save them under custom names.

### 2.4 Deterministic Placement Command (`src/blueprint/blueprint_cmd.*`)

* **Server-Authoritative Command:** Registered as `Commands::PlaceBlueprint` (`DEF_CMD_TRAIT(Commands::PlaceBlueprint, CmdPlaceBlueprint, CommandFlag::LandscapeConstruction)`).
* **Two-Pass Placement Algorithm:**
  - **Pass 1 (Footprint & Base Assets):** Validates bounding box bounds (`STR_ERROR_TOO_CLOSE_TO_EDGE_OF_MAP`), void buffers (`STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE` via `PlanetManager::CheckConstructionPlacement`), water exclusion (`STR_ERROR_CAN_T_BUILD_ON_WATER`), and active portal gate protection. Lays down tracks, depots, and station platforms idempotently without double-charging existing track.
  - **Pass 2 (Signaling):** Attaches signals onto existing tracks with matching types and variants.
  - **Pass 3 (Cache & YAPF Invalidation):** Dirties affected viewports and notifies YAPF rail pathfinding caches (`YapfNotifyTrackLayoutChange`).
* **Dry-Run Cost Estimation:** Accurately queries landscape clearing, track, depot, station, and signal construction costs without executing modifications when `DoCommandFlag::Execute` is absent.

### 2.5 Blueprint Library GUI & Toolbar Integration (`src/blueprint/blueprint_gui.*`, `src/rail_gui.cpp`)

* **Rail Construction Toolbar Integration:** Dedicated Blueprint icon (`WID_RAT_BLUEPRINT`, `SPR_IMG_LANDSCAPING`) added directly to the rail toolbar next to portal gate and conduit tools.
* **Blueprint Library Window (`BlueprintLibraryWindow`):**
  - Left panel: Scrollable list of player blueprints and built-in CST prefabs.
  - Right panel: Blueprint inspection metadata (name, author, dimensions, track piece count, signal count, station platforms, description).
  - Control action buttons:
    - `Capture`: Activates viewport drag-drop selection box (up to 64×64 tiles) to capture map layouts directly.
    - `Place`: Enters stamp placement mode with interactive tile highlighting matching blueprint footprint dimensions.
    - `Rotate`: Rotates active blueprint 90° clockwise in real time (hotkey `R`).
    - `Flip`: Horizontally mirrors active blueprint in real time (hotkey `F`).
    - `Rename`: In-place query string rename for player blueprints.
    - `Delete`: Removes custom blueprint from disk and memory.
    - `Export / Import`: Exporting to disk and importing custom JSON configurations.

---

## 3. Test & Verification Evidence

Sprint 25 includes a dedicated Catch2 unit and integration test suite (`src/tests/test_blueprint.cpp`):
1. **`Blueprint Data Model and JSON Round-Trip`:** Verifies serialization and deserialization of composite tracks, signals, stations, and depots with exact value preservation.
2. **`Blueprint Rotation 90, 180, 270 and Invariance`:** Verifies geometric coordinate transformation and track permutation math through all 4 quadrants and 360° identity.
3. **`Blueprint Horizontal Flip/Mirror`:** Verifies coordinate transposition and axis swapping with double-reflection identity.
4. **`Blueprint Capture from Game Map`:** Builds a test rail layout on the map, captures the bounding area via `BlueprintManager::CaptureArea`, and validates all captured tile data.
5. **`Blueprint Manager Storage and Builtin Protection`:** Validates manager initialization, builtin prefab registration, saving, deleting, and deletion prevention on built-ins.
6. **`Deterministic Blueprint Placement Command`:** Executes `CmdPlaceBlueprint` in dry-run mode (verifying zero map alteration), verifies boundary/void rejections, and executes placement (verifying correct map tiles, track bits, and signal types).

### Test Suite Execution Output
```bash
$ ./build/openttd_test [blueprint]
Filters: [blueprint]
===============================================================================
All tests passed (126 assertions in 6 test cases)

$ ctest --test-dir build --output-on-failure
...
100% tests passed, 0 tests failed out of 217
Total Test time (real) = 6.47 sec
```

---

## 4. Documentation & Traceability

* Added `docs/SPRINT25_PLAYER_RAIL_BLUEPRINTS_2026-09-13.md`.
* Updated `AGENTS.md` with blueprint architecture notes.
* Updated `docs/GAME_DESIGN_AND_TECHNICAL_PLAN.md` with Sprint 25 delivery.
* Updated `docs/FEATURE_UI_UAT_COVERAGE.md` with Blueprint Library UI coverage.
