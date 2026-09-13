# Sprint 31: Planetary Colonization GUI, Universe Authority Federation Expansion, and Settlement Lifecycle

**Status:** COMPLETE  
**Date:** 2026-09-14  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Test Suite:** `src/tests/test_sprint31_colonization_gui.cpp`  
**Acceptance Kit:** `scripts/test_sprint31_colonization_kit.py`  

---

## 1. Executive Summary

Sprint 31 bridges the gap between server-authoritative backend colonization logic introduced in Sprint 30 and the player-facing operational interface, universe authority federation protocol, and settlement lifecycle. It provides seamless in-game and cross-server federation workflows for discovering, inspecting, colonizing, and developing virgin planetary worlds across all 6 biomes.

Key accomplishments in Sprint 31:
1. **Universe Directory Colonization GUI (`UniverseDirectoryWindow`):**
   - Added interactive `Colonize Outpost` action button (`WID_UD_COLONIZE_BTN`) with dynamic contextual enablement.
   - Distinct visual badges and details displaying both **World Phase** (`Phase 1: Core`, `Phase 2: Developed`, `Phase 3: Frontier`, `Phase 4: Expansion`) and **Biome Taxonomy** (`Temperate`, `Arid Desert`, `Sub-Arctic`, `Sub-Tropic`, `Volcanic`, `Oceanic`).
   - Direct execution of `Commands::ColonizeOutpost` when selecting an uncolonized Phase 4 world, prompting immediate settlement founding, phase elevation, news broadcast, and automatic directory list refresh.
   - Synchronizes local single-player / solo UAT planet regions into directory view if authority is running standalone.
2. **Universe Authority Biome Tracking & Dynamic Federation API:**
   - Expanded `RegisteredWorld` in both native C++ `UniverseAuthorityService` and Python `universe_authority.py` with `WorldBiome biome` tracking and persistence.
   - Implemented remote outpost colonization REST API endpoints:
     - `POST /worlds/<id>/colonize`
     - `POST /worlds/colonize`
     - Validates world state, rejects colonization of non-expansion worlds (Core/Developed/Frontier), prevents duplicate colonization, promotes target world to `Phase 3: Frontier`, and renames the world to the specified outpost name.
3. **Colonial Outpost Origins & Viewport Targeting:**
   - Added `outpost_tile` tracking to `PlanetRegion`, recording the exact origin coordinate where the outpost was established.
   - Enhanced `PlanetManager::JumpToPlanet` to focus the player viewport directly on the colonial outpost tile when present, rather than defaulting to the geometric center of the planetary bounding box.
4. **Daemon Crash Checkpoint Persistence & Settlement Recovery:**
   - State checkpoint file serialization (`--state-file`) preserves colonized world names, phases, and biome taxonomies across daemon crashes and restarts.
   - Validated seamless resumption of freight corridors and strict zero-leak commodity conservation (`audit/commodity`) immediately following colony elevation.
5. **Comprehensive Verification:**
   - 4 Catch2 unit test cases in `src/tests/test_sprint31_colonization_gui.cpp`.
   - 5 automated end-to-end acceptance scenarios in `scripts/test_sprint31_colonization_kit.py` passing 100%.

---

## 2. In-Game Colonization UI Architecture

### 2.1 Widget Hierarchy & Layout
The `UniverseDirectoryWindow` has been updated with an operational action bar housing both viewport navigation and outpost colonization:

```
+-------------------------------------------------------------------------+
| Universe Directory & Federation Registry                             [X]|
+-------------------------------------------------------------------------+
| [Header: Select a world to inspect environmental biomes and status]     |
|                                                                         |
|  [0] Oaktree Core     [Phase 1: Core | Temperate]                       |
|  [1] Merredin Basin   [Phase 2: Developed | Arid Desert]                |
|  [2] Calyx Frontier   [Phase 3: Frontier | Sub-Arctic]                  |
| >[3] Ignis Caldera    [Phase 4: Expansion | Volcanic]                   |
|  [4] Pelagios Shelf   [Phase 4: Expansion | Oceanic]                    |
+-------------------------------------------------------------------------+
| World Details:                                                          |
|  World #3: Ignis Caldera                                                |
|  Biome: Volcanic | Phase: Phase 4: Expansion                            |
|  Status: Unsettled wilderness. Ready for colonial outpost founding.     |
+-------------------------------------------------------------------------+
| [ Refresh List ]        [ Jump to World View ]     [ Colonize Outpost ] |
+-------------------------------------------------------------------------+
```

### 2.2 Dynamic Enablement & Tooltip Feedback
- **Phase 4 Expansion Worlds:** `WID_UD_COLONIZE_BTN` is active and enabled. Clicking triggers `Commands::ColonizeOutpost` on the selected world, founding the outpost at the center or selected location, elevating the world to `Phase 3 Frontier`, updating the directory, and switching the button to disabled.
- **Core / Developed / Frontier Worlds:** `WID_UD_COLONIZE_BTN` is disabled; status displays `Settled colonial territory (Active colony: <Outpost Name>)`.
- **String Localization:**
  - `STR_UNIVERSE_DIRECTORY_COLONIZE`: `"Colonize Outpost"`
  - `STR_UNIVERSE_DIRECTORY_COLONIZE_TOOLTIP`: `"Found a new colonial settlement on this wilderness world, unlocking basic rail and mining operations."`
  - `STR_UNIVERSE_DIRECTORY_ALREADY_COLONIZED`: `"World is already colonized."`

---

## 3. Universe Authority Federation Expansion

### 3.1 REST API Endpoint: Outpost Colonization
- **Method:** `POST /worlds/<id>/colonize` or `POST /worlds/colonize`
- **Request Body:**
  ```json
  {
    "world_id": 4,
    "outpost_name": "Caldera Outpost Alpha"
  }
  ```
- **Response (Success 200):**
  ```json
  {
    "world_id": 4,
    "name": "Caldera Outpost Alpha",
    "phase": 3,
    "biome": "Volcanic",
    "status": "Colonized",
    "development_score": 100
  }
  ```
- **Response (Error 400):**
  - Target world not found: `{"error": "World <id> not found"}`
  - Target world already colonized: `{"error": "World <id> is not an Expansion world (Phase 4)"}`

### 3.2 State Persistence & Checkpointing
When configured with `--state-file <path>`, every registration and colonization mutation persists to disk. In the event of process termination (`SIGKILL`) or server migration:
1. The daemon restores all registered worlds, their modified names, and promoted `Phase 3 Frontier` status.
2. Inter-world freight corridors originating from the new colony immediately connect to existing hubs with zero transfer corruption or cargo leaks.

---

## 4. Acceptance Test Scenarios

`scripts/test_sprint31_colonization_kit.py` validates the entire settlement lifecycle end-to-end:

| Scenario | Scope & Validation | Result |
|---|---|---|
| **Scenario 1** | **6-Biome Registration & Directory Taxonomy:** Registers worlds across Temperate, Arid Desert, Sub-Arctic, Volcanic, Sub-Tropic, Oceanic. Validates accurate biome tags in `/worlds`. | **PASS (100%)** |
| **Scenario 2** | **Remote Colonization via REST API:** Colonizes wilderness world via `POST /worlds/4/colonize`. Validates promotion to Phase 3 and name update. | **PASS (100%)** |
| **Scenario 3** | **Colonization Validation & State Guards:** Validates rejection of duplicate colonization, rejection of Core world colonization, and rejection of non-existent worlds. | **PASS (100%)** |
| **Scenario 4** | **Daemon Crash Persistence of Colonized Worlds:** Injects `SIGKILL` on active daemon, restarts on new port from persisted state file, and verifies restored colony state. | **PASS (100%)** |
| **Scenario 5** | **Post-Colonization Inter-World Trade Flow:** Registers corridor from newly colonized world to industrial hub, initiates consist transfer with 100 units raw ore, claims, arrives, and verifies commodity conservation audit. | **PASS (100%)** |

---

## 5. Verification Commands

```bash
# Compile and run Catch2 unit tests
ninja -C build openttd openttd_test
./build/openttd_test "Sprint 31*"

# Run full CTest suite
ctest --test-dir build --output-on-failure

# Run multi-server federation acceptance suite
bash scripts/run_acceptance_kit.sh
```
