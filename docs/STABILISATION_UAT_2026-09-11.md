# OpenSpaceTTD UAT stabilisation record — 2026-09-11

Status: **COMPLETE — Sprint 11 remains paused**

Scope is limited to the Sprint 10 Edge Conduit crash, Portal Gate placement audit,
and the deterministic UAT GameScript fixture. No Sprint 11 or roadmap work is part
of this gate.

## Preserved evidence

The original crash artefacts remain in place and were inspected read-only:

| Artefact | UTC time | SHA-256 |
|---|---:|---|
| `/home/flax/.local/share/openttd/crash20260911021215.json.log` | 2026-09-11 02:12:15 | `a3a833133618e7b5e3781d82ee1ed20bf459d513c432e6a625654563f893f62d` |
| `/home/flax/.local/share/openttd/crash20260911021215.sav` | 2026-09-11 02:12:15 | `26807cee392cb4798d0712731e9685d642c816c59a43d049d7aa26dc0e8adb47` |
| `/home/flax/.local/share/openttd/crash20260911021215.png` | 2026-09-11 02:12:15 | `f53f567259454aab027908fd06d730704476c68e16dad67dc13dea5c8c1033f3` |

The closest suitable pre-crash autosave is
`/home/flax/.local/share/openttd/save/autosave/autosave1.sav`, written at
2026-09-10 23:23:57 UTC, size 620,660 bytes, SHA-256
`982e6e9c13802ddb76f3cdcc102a4fc0fe288bd9c8f76c273d96d606097fcbb3`.
Both the crash save and autosave are readable Savegame version 367 files.

Repository state captured before stabilisation edits:

- HEAD: `3f3424e751bf54e02739601ac0505cc088ef1df0`
- version: `20260909-main-m3f3424e751`
- worktree: 31 tracked files modified (1,256 insertions, 129 deletions), plus
  untracked UAT saves/logs and documentation. These are the user's existing
  Sprint 9/10 and UAT changes and must be preserved.

## GameScript compatibility evidence

Only one installed OpenSpace GameScript is present:
`bin/game/openspacettd_uat`, currently advertising
`OpenSpaceTTD-UAT-Demo` version 6 with `MinVersionToLoad()` equal to 5.
The repository HEAD advertised version 1. The UAT warning states that the
loaded save expects version 1; the crash report shows version 6 was substituted.
The current `Load(version, data)` reads the later `service_built` and
`sprint10_built` keys optionally. The warning was therefore caused by an
incorrect compatibility floor rather than an incompatible state schema.
`MinVersionToLoad()` is now 1, and `Load()` preserves the version-1
`initialized` value when present. No migration framework or fixture replacement
is required.

## Command 151 and placement route

Command 151 is conclusively `Commands::BuildEdgeConduit` in this fork. Counting
the current `Commands` enumeration gives Portal Gate 146 and Edge Conduit 151;
the crash template signature also exactly matches
`CmdBuildEdgeConduit(DoCommandFlags, TileIndex, DiagDirection, CargoType,
RailType)`.

The complete route is:

1. `WID_RAT_BUILD_CONDUIT` — `src/widgets/rail_widget.h`.
2. `BuildRailToolbarWindow::OnClick()` selects the special-placement cursor —
   `src/rail_gui.cpp`.
3. `BuildRailToolbarWindow::OnPlaceObject()` posts
   `Command<Commands::BuildEdgeConduit>` — `src/rail_gui.cpp`.
4. `DEF_CMD_TRAIT(Commands::BuildEdgeConduit, CmdBuildEdgeConduit, ...)` —
   `src/portal/portal_cmd.h`.
5. `CmdBuildEdgeConduit()` performs validation, clears the tile, calls
   `MakeRailTunnel()`, queues signal work with `AddSideToSignalBuffer()`,
   invalidates YAPF, and registers the conduit — `src/portal/portal_cmd.cpp`.
6. `CommandHelperBase::InternalExecuteProcessResult()` flushes the queued work
   through `UpdateSignalsInBuffer()` — `src/command.cpp` and `src/signal.cpp`.

`EdgeConduitManager::IsVoidAdjacent()` and the persisted conduit registry are in
`src/portal/edge_conduit.cpp`; the structure is declared in
`src/portal/edge_conduit.h`.

## Root cause and corrections

- A conduit is stored as a one-ended `TileType::TunnelBridge` rail head. The old
  `UpdateSignalsInBuffer()` assumed every such tile had a second end and queued
  `GetOtherTunnelBridgeEnd()` without validating it. The conduit returned
  `INVALID_TILE`, which was inserted into `_tbdset` and later dereferenced by
  `GetTileType()`. `UpdateSignalsInBuffer()` and `ExploreSegment()` now stop at a
  one-ended head and never enqueue or dereference an invalid other end. The core
  tile assertion remains unchanged.
- The old `IsVoidAdjacent()` treated proximity to the physical map perimeter as
  equivalent to a real adjacent `TileType::Void`, and raw
  `TileAddByDiagDir()` arithmetic could wrap between rows. Placement now uses
  `EdgeConduitManager::ResolvePlacement()`, which derives neighbours by checked
  coordinates and requires a valid in-map void-facing tile plus a valid rail
  approach tile. An outside-map coordinate, OpenTTD void tile, and OpenSpace
  logical boundary are now handled as distinct concepts. Invalid placement
  returns a command error before any tile, registry, YAPF, or signal state changes.
- The crash save restores the placed conduit at TileIndex `521399`
  (`0x7f4b7`), coordinate `(183,509)`, facing SE on a 1024×512 map
  (`Map::Size() == 524288`, `MaxX() == 1023`, `MaxY() == 511`). Its derived
  void neighbour is valid tile `522423` at `(183,510)` and its rail approach is
  valid tile `520375` at `(183,508)`. The first invalid value is
  `GetOtherTunnelBridgeEnd(521399) == INVALID_TILE` (`0xffffffff`), which the
  old signal updater inserted into `_tbdset`; the following `GetTileType()`
  dereference asserted.
- The command path now calls the shared, small
  `PlanetManager::CheckConstructionPlacement()` validator. It proves the base
  tile is in range, non-void, an inner map tile, and belongs to a registered
  logical world. Edge Conduit and Portal Gate commands then validate their own
  complete footprints, orientation, rail type, terrain/clearing rules, company,
  and asset-specific requirements before execution. Query and execute use the
  same authoritative validation.
- Portal Gates are intentionally legal at arbitrary locations and orientations
  inside registered Phase 1, 2, and 3 worlds. The UAT observation was therefore
  classification **A: legal according to current requirements**; no undocumented
  Phase restriction was invented. Illegal void/unregistered-world, invalid
  footprint/approach, and same-world pair placements are rejected by the command,
  including direct invocations. Pair construction validates both ends before
  modifying either one, so failure is atomic. The GUI uses the same posted command
  and displays its error.

## Regression and UAT verification

- Incremental build: `ninja -C build openttd openttd_test` — passed.
- Full repository test binary: 151 test cases and 13,516 assertions — passed.
- CTest suite: 155/155 tests — passed with zero failures.
- Independent clean CMake/Ninja build in `/tmp` — both `openttd` and
  `openttd_test` built successfully; its full 151-case/13,516-assertion suite
  passed. The temporary build was removed after verification.
- Edge Conduit regression coverage includes all four physical sides and corners,
  valid OpenTTD void borders, out-of-range tiles, logical-world boundaries,
  correct and incorrect orientation, valid and invalid terrain, existing adjacent
  rail, query versus execute, Phases 1–4, invalid Phase metadata, signal refresh,
  construction/removal, and save/reload. Invalid cases leave no partial
  construction or signal-buffer state.
- Portal regression coverage includes permitted Phases 1–3, forbidden
  void/unregistered placements, invalid approaches, direct command calls, atomic
  pair failure, and one-ended signal processing.
- The current UAT fixture, original crash save, and closest suitable autosave each
  ran for 3,000 ticks under the repaired clean executable and exited normally.
- The version-1 UAT autosave loaded under GameScript version 6 without the
  substitution/state-discard warning.
- Automated construction-save-reload-removal-save-reload coverage confirms the
  corrected Edge Conduit state remains deterministic across persistence.
- Original crash JSON, save, and screenshot were not modified or overwritten.
- `git diff --check` reports no whitespace errors.

## Acceptance status

- [x] Root cause of Edge Extractor crash identified.
- [x] Exact invalid TileIndex path documented.
- [x] No invalid tile reaches `GetTileType()`.
- [x] Invalid boundary placement fails cleanly and valid placement works.
- [x] No signal-buffer pollution or partial construction occurs.
- [x] Wormhole Gate rules identified and enforced authoritatively.
- [x] Legal Wormhole Gate construction remains available.
- [x] GUI and direct invocation share command validation.
- [x] GameScript version warning resolved without discarding saved state.
- [x] UAT fixture behavior remains deterministic.
- [x] Boundary, Phase, terrain, rail, signal, and persistence regressions added.
- [x] Fresh and incremental builds succeed.
- [x] All existing and OpenSpace-specific automated tests pass.
- [x] Crash/UAT saves load and run without recurrence.

This stabilisation gate is complete. `CURRENT_SPRINT` was not advanced, no
Sprint 11 functionality was implemented, and Sprint 11 remains paused pending an
explicit decision to resume it.
