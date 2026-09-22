# AGENTS.md - OpenSpaceTTD Developer Guide

## Project Premise
- **Project Name:** OpenSpaceTTD
- **Goal:** Railway-heavy industrial and logistics simulation game.
- **Inspiration:** OpenTTD + Factorio-scale production chains + Peter F. Hamilton-style Commonwealth saga (planetary rail networks interlinked via fixed wormholes).
- **Core Future Feature:** Multiple logical worlds connected by railway portal/wormhole gates.
- **Current Status:** Main branch (`e4baa35623`) includes Sprints 1–48, WP-01–11 recovery repairs, WP-F1/F2 federation custody, and Horizon A cluster testbed (PRs #4–#25). Sprints 49–50 are on feature branches not yet merged to main. Sprints 51–52 are planned. Automated tests pass (435+ CTests); human UAT for Sprints 43–50 remains outstanding. Read `docs/PROJECT_STATUS_AND_ROADMAP.md` for canonical status, `docs/CURRENT_ARCHITECTURE.md` for source-verified architecture, `docs/KNOWN_LIMITATIONS.md` for gaps and unproven claims, and `docs/RECOVERY_PLAN_2026-09-15.md` for the recovery plan. Passing domain tests are not human acceptance.

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

---

## Autonomous Agent Delivery & Completion Protocol

After every successful implementation and verification pass, the agent MUST automatically:
1. **Verify Clean Linters & Tests**:
   - Run unit tests and CTest suite (`./build/openttd_test`, `ctest --test-dir build --output-on-failure`).
   - Run repo linters: `python3 .github/file-descriptions.py`, `python3 .github/unused-strings.py`, `git diff --check`.
2. **Commit and Open Pull Request**:
   - Create a clean git commit on a descriptive feature branch with a conventional commit message.
   - Push the branch to `openspace` remote and create a GitHub Pull Request using `gh pr create` with summary, verification evidence, and issue references.
3. **Report Next Steps, Options, and Roadmap**:
   - Inspect roadmap and recovery plan registers (`docs/POST_RECOVERY_ROADMAP_SPRINTS_43_48.md`, `docs/RECOVERY_PLAN_2026-09-15.md`).
   - Present a quick roadmap summary indicating current progress.
   - Present 2–4 prioritized next-step options with the recommended choice highlighted, so the user can immediately choose how to proceed.
