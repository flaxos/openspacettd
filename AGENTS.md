# OpenSpaceTTD agent guide

Railway-heavy industrial logistics: OpenTTD transport, Factorio-scale production,
Commonwealth-inspired worlds. These rules serve Codex and Antigravity.

## Start small

- Inspect Git state; preserve unrelated edits, running games and original saves.
- For game code or architecture, run `python3 scripts/agent_context.py` once per
  task, then read relevant sections of the four canonical documents it lists.
  Read further when scope grows. For guidance/prose, inspect affected files/links;
  do not reload the whole project history.
- Current source and tests outrank prose. Newer dated canonical updates supersede
  older snapshots. Read historical sprint/recovery records only when relevant.
  Do not copy test totals or branch names into this file.
- Use one agent by default; delegate only when requested and independently useful.
  Search narrow paths with `rg`; if RTK rejects syntax, use `rtk proxy rg ...`.
  Keep full logs in `build/agent-logs/`; show summaries and relevant failures.

## Game contracts

- One global contiguous map; worlds are immutable logical regions. Retain
  `TileIndex`; validate ownership with `PlanetManager::GetWorldAtTile`.
  Independent maps/unloading require an explicit owner-approved architecture change.
- Reuse OpenTTD and narrow adapters in `src/portal/` / `src/blueprint/`.
  Gates reuse tunnel/bridge wormholes and YAPF. Keep physical `PortalRegistry`
  records separate from `UniverseGraphManager` routing metadata.
- Centralize placement in `PlanetManager::CheckConstructionPlacement`.
  Preserve deterministic ticks, RNG, iteration, commands and vehicle movement;
  GUI/network workers must not bypass replicated simulation commands.
- Persistent custom state needs handlers in `src/saveload/planet_sl.cpp`, round-trip
  tests, backward compatibility and post-load reference validation.
- Blueprints/prefabs use portable JSON and server-authoritative
  `Commands::PlaceBlueprint`. Preserve cargo and money across ownership moves.
- Before save/scenario/content work, read `docs/SAVEGAME_AUTHORING.md`. Reuse its
  harnesses; preserve published content hashes and audit shared terrain corners.
- Unit tests, headless runs and rendering smoke are distinct from human acceptance.
  Never promote Pending UAT to accepted without a human result for that scope.

## Skills on demand

Read the matching `.agents/skills/<name>/SKILL.md` only when needed:

| Task | Skill |
|---|---|
| Roadmap assessment or one active execution slice | `ost-plan` |
| Gameplay bug, UI wiring, deterministic feature | `ost-dev` |
| Playtest, save authoring/repair, native acceptance | `ost-uat` |
| Final checks, commit, PR, delivery records | `ost-deliver` |

## Verification and delivery

- Reuse `build/`; configure only when needed: `cmake -B build -G Ninja`.
  Compile changed code with `ninja -C build`. Build changed NewGRFs first.
- During implementation run focused regressions. At code/content delivery run
  `./build/openttd_test`, `ctest --test-dir build --output-on-failure`, both repo
  linters and `git diff --check` once after the last relevant change. Keep CTest:
  it includes isolated cases absent from the default Catch run.
- Guidance/prose-only work: validate links, skills/config and changed helpers;
  run linters and diff checks. No engine rebuild, full suite or regenerated saves
  unless an affected contract or observed failure warrants them.
- Complete requested implementation through a conventional commit on `codex/...`,
  push to `openspace` and open/update a scoped PR. Do not merge automatically.
  Read-only questions/planning do not create delivery work. Honor explicit scope.
- Feature/sprint delivery updates status, ledger, limitations and UAT results;
  architecture docs change only with architecture. Small tooling/prose edits need
  one proportionate record, not repeated historical summaries.

## Keep it playable

State the player-visible objective and next proof briefly. Resolve routine choices;
ask only at a material design fork. For broad requests propose one playable slice
before expanding scope. Finish a fix with a short mission and visible success/failure;
distinguish automated proof from pending human play. Offer at most 2–3 useful next
choices, recommend one, and stop at the agreed outcome. Prompt examples and setup:
`docs/AGENT_WORKFLOW.md` (on demand).
