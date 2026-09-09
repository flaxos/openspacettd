# OpenSpaceTTD UAT Demo

`OpenSpaceTTD-Phase1-2-3-UAT-v0.1.sav` is a deterministic 1024 x 512
three-world acceptance-test save generated with seed `9032026`.

## World layout

- World 1: Phase 1 Core, anchored by **Oaktree Core**.
- World 2: Phase 2 Developed, anchored by **Merredin Industrial**.
- World 3: Phase 3 Frontier, anchored by **Calyx Frontier**.
- Gateway Alpha links Worlds 1 and 2.
- Gateway Beta links Worlds 2 and 3.

Use `Ctrl+Alt+1`, `Ctrl+Alt+2`, and `Ctrl+Alt+3` to jump between worlds.
The in-game Story Book contains the complete Data Crystals and Portal Gates
acceptance checklist, location buttons, and persistent goals.

The save contains a human company with starting funds, so it opens ready for
construction rather than in spectator mode.

## Launch

```bash
./build/openttd -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.1.sav
```

The save is a demo foundation rather than the finished vertical slice. Its
next iteration should add player-owned stations, depots, Data Crystal vehicles,
and an operating train service through both gateway corridors.

## Regenerate

Build OpenSpaceTTD, then run:

```bash
./build/openttd -v null:ticks=200 -s null -m null -b null \
  -c demo/uat_demo.cfg -x -G 9032026 -g
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.1.sav
./build/openttd -v null:ticks=50 -s null -m null -b null \
  -c demo/uat_demo.cfg -x \
  -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.1.sav
cmake -E copy demo/save/autosave/exit.sav \
  demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.1.sav
```

The first pass creates the deterministic world and the embedded UAT content.
The second non-networked load creates the initial human company. The null
video driver writes `exit.sav` to the autosave directory because
`autosave_on_exit` is enabled. Validate the final output with:

```bash
./build/openttd -q demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.1.sav
```
