---
name: ost-dev
description: Implement or debug OpenSpaceTTD C++ gameplay, UI wiring, rail routing, economy, and deterministic commands. Use for a bounded bug or feature, not prose-only work or scenario authoring.
---

# OpenSpaceTTD development

Use the root AGENTS.md contracts. Run the context helper if not already read in
this task; follow relevant canonical sections, not every historical sprint.

For a bug, capture the shortest repro and expected player outcome. Identify the
actual executable, save/content and UI scale when relevant; never assume branch
name identifies the running binary. Trace UI → command → manager → persistence
or native movement. Compare integrated versus legacy rules before restoring an
old toggle or bypassing a prerequisite. Report uncertainty instead of guessing.

For a feature, define one observable loop (player action → simulation change →
feedback), its failure path and acceptance boundary. Preserve transport-first scope;
do not revive withdrawn narrative/crisis sprints from old documents. Implement
through the existing engine path, then prove that path.

| Concern | Start here; inspect adjacent tests |
|---|---|
| World/placement | `src/portal/planet_manager.*`, `src/portal/world_gen.*` |
| Gates/pathfinding | `src/portal/portal_*`, `src/pathfinder/yapf/`, `src/train_cmd.cpp` |
| Economy | `src/portal/integrated_economy.*`, `docs/UNIFIED_COMMONWEALTH_ECONOMY.md` |
| Blueprints | `src/blueprint/`, `src/tests/test_blueprint*`, `src/tests/test_cst_prefabs.cpp` |
| UI/strings | Owning `*_gui.cpp`, `src/lang/english.txt`; native window tests |
| Federation | `docs/FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md`, `src/portal/` |
| Persistence | `src/saveload/planet_sl.cpp`, relevant save/load tests |

Search test names/tags before choosing a Catch filter or CTest regex. Confirm at
least one relevant test runs. Use focused regressions while editing; keep GUI tests
requiring isolation in CTest. Do not weaken assertions to hide terrain, cargo,
command-authority or save corruption.

Simulation changes need failure-path tests proportional to risk: no partial payment
or mutation on rejection, stable ordering and conservation. Persistent changes also
need round-trip and old-save coverage. Player integration requires a native command
or black-box proof, not only a manager mock.

Reuse the configured build and batch string edits. Log lengthy output and report
exit status plus decisive lines. Use `ost-deliver` at completion; use `ost-uat` for
a runnable player check and retain its separate human acceptance status.
