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

## Project Continuity & Documentation Authority Rules

Every coding or architecture agent working on OpenSpaceTTD MUST follow these rules:
1. **Read Authoritative Docs First:** Always read the canonical roadmap (`docs/PROJECT_STATUS_AND_ROADMAP.md`), source-verified architecture (`docs/CURRENT_ARCHITECTURE.md`), known limitations (`docs/KNOWN_LIMITATIONS.md`), and sprint ledger (`docs/SPRINT_LEDGER.md`) before planning or proposing changes.
2. **Source and Tests Are Truth:** Treat CURRENT source code, test files, and passing CTests as truth over historical sprint documents, audit notes, or outdated handoffs. Historical documents are evidence of intent at a point in time, not current status.
3. **No Implicit World Architecture Drift:** Do NOT silently change or drift into a true multi-map engine. Current worlds are logical regions on one global map. If independent maps, world unloading, or world-local simulation are ever needed, document them as an explicit architecture decision with owner approval.
4. **Mandatory Sprint Completion Updates:** Every completed sprint or feature branch MUST update the sprint ledger (`docs/SPRINT_LEDGER.md`), project status (`docs/PROJECT_STATUS_AND_ROADMAP.md`), known limitations (`docs/KNOWN_LIMITATIONS.md`), and UAT results. A feature is NOT done until implementation, automated test status, UAT status, and limitations are recorded.

---

## Architectural Rules & Guidelines

1. **Preserve OpenTTD Systems Where Possible:**
   Reuse existing engine constructs, subsystems, data structures, and algorithms rather than rewriting them from scratch. Minimise changes to upstream OpenTTD core; prefer narrow adapters into project-owned modules (`src/portal/`, `src/blueprint/`).

2. **Do NOT Redesign `TileIndex`:**
   `TileIndex` is deeply integrated across all map arrays, pool IDs, rendering routines, network synchronization, and save/load serializers. Retain the existing `TileIndex` typedef and contiguous tile array design. However, **bare `TileIndex` is insufficient when world identity matters**: always validate world ownership via `PlanetManager::GetWorldAtTile(tile)`.

3. **Logical Map Regions for Worlds:**
   Instead of refactoring the entire engine into a complex multi-map hierarchy, partition the single coordinate map space (up to 4096×4096) into discrete logical world regions (separated by void or buffer space). **Treat world rectangles as immutable after generation** unless a migration design is deliberately introduced.

4. **Wormhole-Based Portal Gates:**
   Portal gates between worlds must adapt OpenTTD's existing tunnel/bridge wormhole architecture (`Track::Wormhole`, `VehicleEnterTileState::EnteredWormhole`, `GetOtherTunnelBridgeEnd`) and YAPF rail pathfinder integration (`src/pathfinder/yapf/`). Keep physical portal records (`PortalRegistry`) separate from universe-level routing concepts (`UniverseGraphManager`).

5. **Centralise Construction & World Policy:**
   Centralise world/placement policy checks inside `PlanetManager::CheckConstructionPlacement` rather than scattering policy checks across upstream OpenTTD commands.

6. **Persistent Custom Systems Must Have Save/Load Tests:**
   Every new persistent custom system requires custom chunk handlers in `src/saveload/planet_sl.cpp`, complete round-trip save/load tests, and post-load reference validation.

7. **Prove Core Behaviour with Black-Box & UAT Tests:**
   Manager-level unit tests are necessary but insufficient; prove core player behaviour with end-to-end black-box tests and human-playable UAT scenarios.

8. **Preserve Deterministic Simulation:**
   All simulation logic, command handling, random number generation, and vehicle movement must remain strictly deterministic to ensure savegame integrity and lockstep multiplayer synchronization.

9. **Player Rail Blueprints:**
   Player blueprints and CST prefabs must use the portable JSON schema and server-authoritative placement command (`Commands::PlaceBlueprint`) to ensure lockstep simulation and multiplayer determinism.

---

## Building or Repairing UAT Savegames

Before authoring a scenario, read [SAVEGAME_AUTHORING.md](docs/SAVEGAME_AUTHORING.md)
and reuse the existing builder/harness rather than scripting a new construction path.

- Confirm the actual binary, save and exact content dependencies; preserve user saves
  and unrelated work. Keep worlds as immutable regions of the existing global map.
- Build changed NewGRFs before the engine. Batch string changes, reuse the configured
  build, and run a short generation/terrain/text audit before a long economic soak.
- Validate the whole height field, including rectangle boundaries and void edges.
  Tile corners are shared; direct height writes and `MakeVoid` can invalidate neighbors.
- Resolve cargo by label, preserve native slots, and supply all five cargo text fields.
  Audit all active cargo text in the player's language and English, not just names.
- Build via normal commands and prove repeated delivery per train/cargo, genuine town
  growth, conservation, fresh-process reload and interruption/recovery. Distinguish
  warehouse storage and industry input from actual city consumption.
- Use checkpoints for diagnosis, not as a substitute for cold-start generation evidence.
  Freeze binary/content before final verification; publish matching save and hash evidence.
- Preserve old GRF hashes and a public regression fixture for compatibility. Scope any
  recovery narrowly and preflight shared corners before changing terrain under a save.
- Keep player instructions short and wrapped. Headless tests do not establish graphical
  acceptance; record the visual retest separately at the player's UI scale and language.

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
