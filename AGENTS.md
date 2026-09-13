# AGENTS.md - OpenSpaceTTD Developer Guide

## Project Premise
- **Project Name:** OpenSpaceTTD
- **Goal:** Railway-heavy industrial and logistics simulation game.
- **Inspiration:** OpenTTD + Factorio-scale production chains + Peter F. Hamilton-style Commonwealth saga (planetary rail networks interlinked via fixed wormholes).
- **Core Future Feature:** Multiple logical worlds connected by railway portal/wormhole gates.
- **Current Status:** Federation foundations, playable alien world biomes, player rail blueprints, and 8 canonical CST prefabs are implemented through Sprint 26. Sprints 27–29 deliver UI completion, guided solo UAT, and the federation acceptance kit.

---

## Build, Test, and Run Commands

- **Configure (CMake + Ninja):**
  ```bash
  cmake -B build -G Ninja
  ```
- **Compile:**
  ```bash
  ninja -C build
  ```
- **Run Unit & Regression Tests:**
  ```bash
  ctest --test-dir build --output-on-failure
  # Or run Catch2 unit tests directly:
  ./build/openttd_test
  ```
- **Launch Executable:**
  ```bash
  ./build/openttd
  # Or check version/help:
  ./build/openttd -h
  # Dedicated/headless mode:
  ./build/openttd -D
  ```

---

## Architectural Rules & Guidelines

1. **Preserve OpenTTD Systems Where Possible:**
   Reuse existing engine constructs, subsystems, data structures, and algorithms rather than rewriting them from scratch.

2. **Do NOT Redesign `TileIndex`:**
   `TileIndex` is deeply integrated across all map arrays, pool IDs, rendering routines, network synchronization, and save/load serializers. Retain the existing `TileIndex` typedef and contiguous tile array design.

3. **Logical Map Regions for Future Worlds:**
   Instead of refactoring the entire engine into a complex multi-map hierarchy, partition the single coordinate map space (up to 4096×4096) into discrete logical world regions (e.g. separated by void or buffer space).

4. **Wormhole-Based Portal Gates:**
   Portal gates between worlds must investigate and adapt OpenTTD's existing tunnel/bridge wormhole architecture (`Track::Wormhole`, `VehicleEnterTileState::EnteredWormhole`, `GetOtherTunnelBridgeEnd`) and YAPF rail pathfinder integration (`src/pathfinder/yapf/`). Wormholes naturally allow non-contiguous transitions between distant tiles.

5. **Preserve Deterministic Simulation:**
   All simulation logic, command handling, random number generation, and vehicle movement must remain strictly deterministic to ensure savegame integrity and lockstep multiplayer synchronization.

6. **Player Rail Blueprints:**
   Player blueprints and CST prefabs must use the portable JSON schema and server-authoritative placement command (`Commands::PlaceBlueprint`) to ensure lockstep simulation and multiplayer determinism.
