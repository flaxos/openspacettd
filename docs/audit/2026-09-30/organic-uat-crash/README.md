# Organic UAT crash recovery

The supplied report contains two independent crashes:

- `crash20260929060130`: zero-width tooltip layout (`gfx.cpp:718`), fixed by
  merged PR #47. This branch brings that fix into the organic-layout branch.
- `crash20260929233517`: invalid rail foundation (`landscape.cpp:222`). Both the
  published UAT and crash save fail at Augusta tile **(63, 103)**: track bits 18,
  slope 6, corners 0/0/1/1. The organic generator protected only +/-2 tiles around
  gate heads, leaving the long terminal's far switches and shared corners exposed.

## Repair

Anchor every corner under pre-existing portal infrastructure, and relax the whole
height field (including void corners) with two deterministic lowering passes.
Audit all corner differences and foundation callbacks before spawning trains or
saving; then query foundation sprite blocks and pixel heights across the map.
This preserves the native renderer's assertions instead of suppressing a crash.

The load-time recovery applies only to the recognized 256-square, four-world UAT.
It checks original world/company identities and unmodified neutral terminal track
layouts, computes candidate heights off-map, and preflights every affected shared
corner and vehicle before committing. Recovery is idempotent. Ownership, tracks,
money and world rectangles do not change. Edits that cannot be safely preserved
produce a descriptive load error; unrelated saves are not reshaped.

## Verification

- Full unit suite: **473 cases / 67,547 assertions**.
- CTests: **486/486**, including the existing isolated tooltip regression.
- Prefab cases: **120 assertions**, including the exact switch regression,
  idempotence, unrelated-company exclusion and atomic refusals for edited rails,
  shared-corner infrastructure and live vehicle support.
- Native original/crash save recovery, simulation progress and cold reload;
  fresh native 3/4/6-world generation and reload; actual SDL/dummy-video processes
  at 2560×1389 remain live without either reported assertion.
- Independent connected legacy save: two-language cargo/terrain audit, full-map
  pixel/viewport checks, save/reload and four simulation intervals.
- File-description/unused-string linters and whitespace checks pass.

See [native evidence](native.json), [legacy evidence](legacy.json),
[unit log](unit.log), [CTest log](ctest.log), [prefab log](prefab.log).
Original source hashes and the exact tested binary hash are in the native evidence.
The published v1.0 save remains the public regression fixture, unchanged.

```sh
ninja -C build
./build/openttd_test
ctest --test-dir build --output-on-failure
python3 scripts/test_organic_uat_recovery.py --output /tmp/organic-recovery --gui
python3 scripts/test_connected_uat_recovery.py --output /tmp/legacy-check
```

Add `--save /path/to/crash.sav` (repeatable) to check additional copies. The harness
uses isolated configuration and output directories, pauses before comparisons and
never overwrites input saves. No new GRF content or existing-save economy conversion
is involved. Human UI-scale/language/visual acceptance remains **Pending**; SDL
smoke and geometry queries do not establish NVIDIA/OpenGL graphical acceptance,
route connectivity, freight profitability or the full UAT playthrough.
