# Sprint 30: Exotic Alien Biomes & Phase 4 Planetary Colonization Engine

**Status:** COMPLETE  
**Date:** 2026-09-14  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Test Suite:** `src/tests/test_sprint30_biomes_colonization.cpp`  

---

## 1. Executive Summary

Sprint 30 delivers the full 6-biome environmental ecosystem and the Phase 4 Planetary Colonization Engine for OpenSpaceTTD, completing the planetary simulation roadmap outlined in `docs/ALIEN_WORLD_ART_DIRECTION.md` and `docs/GAME_DESIGN_AND_TECHNICAL_PLAN.md`.

Prior to this sprint, the planetary engine supported the three baseline biomes (`Temperate`, `AridDesert`, `SubArctic`) and three development tiers (`Phase1_Core`, `Phase2_Developed`, `Phase3_Frontier`). Sprint 30 introduces:
1. **The Complete 6-Biome Environmental Palette:** Implementing procedural generation, terrain styling, flora transformations, and persistent tile loops for **SubTropic** (lush rainforests, jade soil, segmented fronds), **Volcanic** (scorched charcoal crust, hardy spires, rough/rocky persistence), and **Oceanic** (marine shelves, coastal reef rocks, giant mangrove-like cup corals).
2. **Phase 4 Expansion / Wilderness Worlds:** Rules and build constraints for virgin, unsettled planets (`WorldPhase::Phase4_Expansion`) where heavy industrial facilities and advanced depots cannot be placed prior to establishing human settlement.
3. **Colonial Outpost Founding (`Commands::ColonizeOutpost` / `CmdColonizeOutpost`):** Server-authoritative command allowing players and operators to found an initial colonial outpost town on a Phase 4 world, elevating it to `Phase3_Frontier` status, updating the world name, raising development score, broadcasting empire-wide news (`STR_NEWS_WORLD_COLONIZED`), and unlocking pioneer extraction industries and rail depots.
4. **Interactive Console Control (`colonize_world`):** In-game debug and dedicated server console command for operators to colonize and name wilderness worlds interactively.
5. **Dynamic Phase Promotion & Round-Trip Persistence:** Support in `PlanetManager` for multi-tier phase elevation (`Phase4_Expansion` $\to$ `Phase3_Frontier` $\to$ `Phase2_Developed` $\to$ `Phase1_Core`) with 100% round-trip binary persistence in the savegame `PLNT` chunk.

---

## 2. 6-Biome Environmental Specifications

Each world in OpenSpaceTTD is assigned both an environmental **Biome** (governing geology, foliage, and climate rendering) and a **World Phase** (governing settlement, infrastructure, and economic rules). Biome styling applies across all tiles belonging to the world's bounding box:

| Biome | Terrain & Atmosphere | Flora & Vegetation | Ground Persistence & Tile Loops |
|---|---|---|---|
| **Temperate** | Blue-green soil shadows, teal grass, mauve cuts | Broad fan canopies (`TREE_TEMPERATE`), fungal shelves | Standard grass growth up to density 3; normal tropic zone. |
| **Arid Desert** | Rust-red flats, violet shadow, salt-white ridges | Black spines, glassy succulents (`TREE_CACTUS`) | Clear tiles maintain desert/rocky ground; `TileLoopClearDesert` prevents grassing over. |
| **Sub-Arctic** | Blue-grey permafrost, lilac ice, turquoise melt channels | Low coral shrubs, conifer groves (`TREE_SUB_ARCTIC`) | High-altitude/sub-arctic snow line logic (`TileLoopClearAlps`); snow clearing/accumulation. |
| **Sub-Tropic** | Saturated jade soil, ochre wetlands, humid blue haze | Tall segmented fronds, dense spore crowns (`TREE_RAINFOREST`) | Rainforest tropic zone (`TropicZone::Rainforest`); lush grass (density 3) with wetland rough patches. |
| **Volcanic** | Charcoal crust, ember fissures, sulphur-green deposits | Sparse hardy spires (`TREE_TEMPERATE` on rough ground, growth 1) | Scorched crust persistence: `TileLoop_Clear` reverts bare/grass tiles back to `ClearGround::Rough` or `ClearGround::Rocks`. |
| **Oceanic** | Deep cobalt water, turquoise shelves, pale reef flats | Mangrove-like root towers, cup corals (`TREE_RAINFOREST`) | Coastal shelf reef rocks (`ClearGround::Rocks`) and lush grass clearings; clear land rendering with grass density. |

---

## 3. Planetary Colonization & Phase Progression Engine

### 3.1 Pre-Colonization Constraints (Phase 4 Expansion)
Uncolonized wilderness worlds (`Phase4_Expansion`) represent virgin planetary surfaces:
- Basic railway tracks and portal gates may be constructed to reach the world.
- Heavy industries (processing factories, refineries) are forbidden.
- Raw resource extraction (mines, farms) is locked until colonial infrastructure arrives.
- Depots cannot be built in uninhabited wilderness (`STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD`).

### 3.2 Colonization Command (`Commands::ColonizeOutpost`)
```cpp
CommandCost CmdColonizeOutpost(DoCommandFlags flags, TileIndex tile, const std::string &outpost_name);
```
- **Validation:** Ensures proposed tile is within a registered logical world and that the world is currently at `WorldPhase::Phase4_Expansion`.
- **Cost:** Charges standard colonial town expedition fee (`_price[Price::BuildTown] * 5`).
- **Execution:**
  1. Elevates world phase: `Phase4_Expansion` $\to$ `Phase3_Frontier`.
  2. Increments development score (+100).
  3. Updates world name to the player-specified outpost name (or generates default `"Outpost <World>"`).
  4. Broadcasts news alert `STR_NEWS_WORLD_COLONIZED`: *"{BIG_FONT}{BLACK}Outpost Founded!{}{STRING} has been colonized and elevated to Frontier status (Phase 3)!"*
  5. Unlocks pioneer resource extraction industries (mines, bio-farms) and standard rail depots.

### 3.3 Multi-Tier Phase Elevation Hierarchy
```
+------------------------+
|   Phase 4: Expansion   | (Virgin Wilderness: no heavy industries or depots)
+------------------------+
            |
            | CmdColonizeOutpost (+100 Dev Score)
            v
+------------------------+
|   Phase 3: Frontier    | (Colony Founded: raw extraction & pioneer rail depots unlocked)
+------------------------+
            |
            | PromoteWorldPhase (+250 Dev Score)
            v
+------------------------+
|  Phase 2: Developed    | (Industrial Hub: refineries & heavy processing unlocked)
+------------------------+
            |
            | PromoteWorldPhase (+500 Dev Score)
            v
+------------------------+
|     Phase 1: Core      | (Megacity Core: arcologies & high-demand consumer market)
+------------------------+
```

---

## 4. Console Management

Operators and script runners can query and colonize worlds via the interactive console:
```text
] colonize_world <world_id> [outpost_name]
```
Example:
```text
] colonize_world 3 "New Caldera"
Successfully colonized World 3: now 'New Caldera' (Phase 3 Frontier).
```

---

## 5. Verification & Test Coverage

All Sprint 30 features are verified via the dedicated Catch2 test suite `src/tests/test_sprint30_biomes_colonization.cpp`:
1. **All 6 Biomes Spatial Resolution:** Tests O(1) retrieval of biome and phase for all 6 environmental types.
2. **Procedural Biome Partitioning:** Generates multi-world layouts with 6+ worlds, verifying cyclic assignment of `Volcanic`, `SubTropic`, and `Oceanic` wilderness worlds.
3. **Procedural Biome Environmental Styling & Foliage:** Verifies tree transformations (`TREE_CACTUS`, `TREE_SUB_ARCTIC`, `TREE_RAINFOREST`), ground types (`Desert`, `Rough`, `Rocks`, `Grass`), and tropic zones (`Normal`, `Desert`, `Rainforest`).
4. **Phase 4 Expansion World Build Restrictions:** Verifies failure of industry and depot placement on uncolonized worlds.
5. **PlanetManager ColonizeWorld & Promotion Hierarchy:** Verifies promotion logic, development score increments, and denial of illegal promotions.
6. **CmdColonizeOutpost Command Execution & Rule Unlock:** Tests test-mode cost evaluation, live outpost establishment, and subsequent unlocking of pioneer industries and depots.
7. **Save/Load Persistence:** Full serialization round-trip test verifying that colonized world phases and custom outpost names survive save/load cycles without data loss.
