# OpenSpaceTTD UAT Demo

> **Historical fixture:** v0.3 validates the early single-map planetary slice. It predates automatic portal terminals and the later federation, telemetry, round-trip order, blueprint and alien-art work. It is not the all-feature release UAT save. Sprint 28 will generate the successor guided save; Sprint 29 will add the matching three-server federation kit. See [FEATURE_UI_UAT_COVERAGE.md](../docs/FEATURE_UI_UAT_COVERAGE.md).

`OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav` is a deterministic 1024 x 512
three-world acceptance-test save generated with seed `9032026`.

## World layout

- World 1: Phase 1 Core, anchored by **Oaktree Core**.
- World 2: Phase 2 Developed, anchored by **Merredin Industrial**.
- World 3: Phase 3 Frontier, anchored by **Calyx Frontier**.
- Gateway Alpha links Worlds 1 and 2 using spatially offset, perpendicular
  portal heads. Its automatic demonstrator locomotive repeatedly traverses
  the wormhole in both directions.
- Gateway Beta links Worlds 2 and 3 with another non-aligned, rotated pair.

Use `Ctrl+Alt+1`, `Ctrl+Alt+2`, and `Ctrl+Alt+3` to jump between worlds.
The in-game Story Book contains the complete Data Crystals, Portal Gates and
Sprint 10 Planetary Operations acceptance checklists, location buttons, and
persistent goals. Sprint 11 adds automatic high-capacity rail terminals to
newly constructed and newly generated Portal Gates.

Sprint 10 adds two ready-to-use sites:

- **Phase 1 Spaceport Candidate**, an owned small airport whose ordinary
  station window exposes Spaceport designation, Tier 1-3 upgrades, supplies
  and off-world trade telemetry.
- **Frontier Edge Minerals**, an owned rail platform beside a signed empty
  Phase 3 boundary tile. The final tunnel-style railway tool builds or removes
  the Edge Conduit there; Land Area Information shows its live status.

The save contains a human company with starting funds, so it opens ready for
construction rather than in spectator mode.

## Launch

```bash
./build/openttd -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav
```

Follow the Story Book location links or the signs for Gateway Alpha. The
`UAT Wormhole Demonstrator` starts on an east-west line in World 1 and emerges
on a north-south line at a different X and Y coordinate in World 2. This makes
the arbitrary endpoint transition visibly distinct from an ordinary tunnel.

Follow [SPRINT10-UAT.md](SPRINT10-UAT.md) for Planetary Operations and
[SPRINT11-UAT.md](SPRINT11-UAT.md) for Portal Terminal acceptance. The existing
save-menu folder link `OpenSpaceTTD-Demos` points at this directory. The v0.3
save predates automatic terminal construction; use a newly generated game for
the Sprint 11 terminal checks.

## Regenerate

Build OpenSpaceTTD, then run:

```bash
./build/openttd -v null:ticks=200 -s null -m null -b null \
  -c demo/uat_demo.cfg -x -G 9032026 -g
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav
./build/openttd -v null:ticks=1000 -s null -m null -b null \
  -c demo/uat_demo.cfg -x \
  -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav
```

The first pass creates the deterministic world and the embedded UAT content.
The second non-networked load creates the initial human company. The null
video driver writes `exit.sav` to the autosave directory because
`autosave_on_exit` is enabled. Validate the final output with:

```bash
./build/openttd -q demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.3.sav
```
