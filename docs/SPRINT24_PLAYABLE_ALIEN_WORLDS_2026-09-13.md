# OpenSpaceTTD Sprint 24 — Playable Alien Worlds & Visual Biome Foundation

Status: **COMPLETE**  
Date: **2026-09-13**  
Reference Specifications: [ALIEN_WORLD_ART_DIRECTION.md](ALIEN_WORLD_ART_DIRECTION.md), [SPRINT23_SCOPE_ART_AND_UAT_AUDIT_2026-09-13.md](SPRINT23_SCOPE_ART_AND_UAT_AUDIT_2026-09-13.md)

---

## 1. Executive Summary

Sprint 24 delivers the visual transformation of OpenSpaceTTD from a standard OpenTTD map into distinct, readable Commonwealth planetary worlds. 

Prior to Sprint 24, all logical world regions shared identical green temperate grass tiles and stone tunnel portals regardless of their configured `WorldBiome`. Sprint 24 implements:
1. **Procedural Multi-World Environmental Stylization:** Seamless procedural generation of distinct surface geology and foliage for the three UAT showcase worlds during map creation.
2. **$O(1)$ Spatial Biome Pipeline:** Direct tile-to-biome resolution in `PlanetManager` driving runtime tile drawing and environmental simulation loops.
3. **Monumental CST Portal Gate Visual Distinction:** Automatic recolouring of portal gate structures and tunnel mouths using canonical CST infrastructure palettes (active cyan-blue conduit vs. unlinked amber standby).
4. **Comprehensive Automated Verification:** 100% pass rate across 211 CTest suites including dedicated Catch2 test coverage.

---

## 2. Key Deliverables & Architecture

### 2.1 Procedural Multi-World Biome Stylization (`src/portal/world_gen.cpp`)

During `MultiWorldGen::GenerateMultiWorldLayout()`, after spatial partitioning and void buffer carving, `MultiWorldGen::ApplyBiomeStyling(const PlanetRegion &region)` executes across each world region's bounding box:

* **Phase 1 Core — Oaktree Core (`WorldBiome::Temperate`):**
  - Surface: Lush alien temperate grass (`ClearGround::Grass`, `TropicZone::Normal`).
  - Flora: Specimen alien trees (`TREE_TEMPERATE`).
  - Aesthetic: Clean, civic arcology setting with ordered plazas and guideways.

* **Phase 2 Developed — Merredin Industrial (`WorldBiome::AridDesert`):**
  - Surface: Rust-red arid desert flats (`ClearGround::Desert`, `TropicZone::Desert`) with natural sand density variation and procedural rocky outcrops (`ClearGround::Rocks`).
  - Flora: Glassy succulents and desert spines (`TREE_CACTUS` with `TreeGround::SnowOrDesert`).
  - Aesthetic: Stark, open industrial terrain with high-contrast railway corridors.

* **Phase 3 Frontier — Calyx Frontier (`WorldBiome::SubArctic`):**
  - Surface: Frozen permafrost and snow cover (`MakeSnow(tile, density)`).
  - Flora: Sub-arctic conifer groves (`TREE_SUB_ARCTIC` with `TreeGround::SnowOrDesert`).
  - Aesthetic: Cold frontier landscape with bright permafrost and isolated modular extraction sites.

* **Phase 4 Expansion / Wilderness (`WorldBiome::Volcanic`):**
  - Surface: Rough terrain (`ClearGround::Rough`) and volcanic basalt outcrops (`ClearGround::Rocks`).

### 2.2 Biome-Aware Rendering & Simulation (`src/portal/planet_manager.*`, `src/clear_cmd.cpp`)

* **$O(1)$ Biome Lookup:** Added `PlanetManager::GetTileBiome(TileIndex tile)` utilizing the spatial acceleration grid to resolve tile biomes with zero runtime overhead.
* **Biome-Aware Drawing:** Enhanced `DrawTile_Clear()` so clear tiles render with appropriate desert or permafrost sprites corresponding to the world's `WorldBiome`.
* **Environmental Tile Loop:** Updated `TileLoop_Clear()` to execute `TileLoopClearDesert()` and `TileLoopClearAlps()` based on the tile's `WorldBiome`, ensuring desert and permafrost dynamics persist throughout gameplay.

### 2.3 CST Monumental Portal Gate Visuals (`src/tunnelbridge_cmd.cpp`)

To clearly distinguish Commonwealth fixed wormhole gates from standard rustic stone/brick railway tunnels:
* Portal tiles (`PortalRegistry::IsPortalTile` or `PortalRegistry::IsUnlinkedGate`) are intercepted during `DrawTile_TunnelBridge()`.
* **Active Linked Gate:** Rendered with `PALETTE_TO_STRUCT_BLUE` (CST cyan-blue conduit excitation across the portal mouth and roof arch).
* **Standby Unlinked Gate:** Rendered with `PALETTE_TO_STRUCT_YELLOW` (amber standby excitation indicating an unlinked gate head awaiting pairing).

---

## 3. Test & Verification Evidence

### 3.1 Unit Test Suite (`src/tests/test_sprint24_alien_biomes.cpp`)

Added 4 Catch2 test cases covering:
1. `Alien Biomes - PlanetManager GetTileBiome O(1) query`: Verifies $O(1)$ biome and phase resolution across worlds and void buffer fallbacks.
2. `Alien Biomes - Procedural MultiWorldGen Biome Environmental Styling`: Verifies that `GenerateMultiWorldLayout` transforms core tiles to grass, industrial tiles to desert/rocks, and frontier tiles to permafrost/snow.
3. `Alien Biomes - Tree Foliage Transformation per Biome`: Verifies tree conversion to `TREE_CACTUS` in desert and `TREE_SUB_ARCTIC` in sub-arctic regions.
4. `Alien Biomes - CST Portal Gate Classification and Visual Styling`: Verifies portal gate classification and palette mapping (`PALETTE_TO_STRUCT_BLUE` vs `PALETTE_TO_STRUCT_YELLOW`).

### 3.2 Verification Results

* **Catch2 Suite:** 41/41 assertions passed.
* **CTest Suite:** 211/211 test suites passed (100%).
* **Integration Tests:** `scripts/test_sprint22_roundtrip_orders.py` passed with full commodity conservation verified.
