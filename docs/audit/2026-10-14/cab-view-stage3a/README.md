# Stage 3A — native gate-crossing effects

**Automated engineering complete; human UAT and other platforms Pending.**
This is an uncommitted, unpublished checkpoint on `codex/cab-view-stage1`, based
on `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`. The inherited cab renderer is pinned
to upstream [`15.3-cabview-alpha1`](https://github.com/vaclavloubek/OpenTTD/releases/tag/15.3-cabview-alpha1),
commit `b7bd17b04dde3379b2be33be40ba4d6fc9f5d391`. Stage 1 and Stage 2 patches and
audit files remain exact. The [active plan](../../../ACTIVE_EXECUTION_PLAN.md)
records the bounded authority and frozen budget.

## Player-visible change

A validated local gate has a mild animated edge lens as the cab approaches.
Genuine hidden transit draws an opaque dark surface with restrained blue rings
and streaks; the status text remains readable. Arrival briefly settles the real
destination picture for at most 250 ms. No source-world image is retained and
no destination picture is shown early. Missed transit does not get replayed.

The cab's Effects button cycles **Full → Reduced → Off**. Full is the fresh-window
default. Reduced is static, with no distortion or travelling streaks; Off restores
the accepted Stage 2 pixels. This choice is window-local and unsaved. The build
feature `WITH_CAB_VIEW` still defaults OFF. Simulation, movement, economy, save
serialization and network implementation did not change in Stage 3A.

Native examples: [approach](gate-approach-full.png), [opaque transit](gate-transit-full.png),
[arrival](gate-arrival-full.png), [Reduced](gate-approach-reduced.png), [Off](gate-approach-off.png).
These are unedited native screenshots. Animation is evidenced by multiple GUI
captures and pixel checks in the archive, not inferred from a single still.

## Verification

- Final OFF and ON engine/test targets pass. OFF compiles no cab translation
  units or definitions; ON compiles five cab units and eleven defined units.
  Final ON hashes exactly match the binaries used for the accepted native runs.
- 37 focused tests pass with 11,198 assertions. Coverage includes timeline
  ordering, missed transit, 249/250 ms expiry, deterministic paint, static Reduced,
  exact Off, real edge displacement, clipped output, 8bpp output, cache release,
  resize, ownership/access/session boundaries and per-car masking.
- Three fresh native runs, each with forward/reverse journey and cold reload,
  pass: 50 assertions, 77 PNGs and 72 profiling cells per run. Explicit simulation
  calls are 2,495 / 2,499 / 2,495 against the 24,576 combined cap.
- All 27 Off-to-baseline widget comparisons and all six ordinary viewport
  comparisons have zero changed pixels. Frozen simulation snapshots and masked
  geometry are unchanged across modes and paused UI/profiling actions.
- Independent Astra review recomputed all 24 comparison groups from 216 cells
  (21,600 measured frames), checked rotations and poses, verified all 231 PNG
  hashes, and independently repeated the exact pixel comparisons.
- File-description, unused-string and diff whitespace checks pass.

See [verification.json](verification.json), [independent review](independent-review.json),
[performance summary](performance-summary.json), and the
[complete budget calculation](performance-budget.json).

## Measured budget

Linux 6.8, i7-7700K at 4.2 GHz, eight logical CPUs, g++ 13.3 Release with
`WITH_ASSERT`, SDL offscreen/software, `32bpp-anim`, installed OpenGFX and the
fixture's Commonwealth industry/rail GRFs. Exact content hashes and loaded IDs
are in the native manifests. This is one disposable four-car SW/NE local pair.

Each cell uses a private fixed-scale renderer: one cold frame, 20 warmups,
100 forced fresh frames and a separate cached sample. Sizes are 480×272 and
640×372. The actual screenshot widget is 480×270 at inclusive screen rectangle
`[0,38,479,307]`. GUI captures inject paused 80/160 ms callbacks; native callbacks
may also occur. Profiling uses immutable representative effect times and does
not advance the live effect clock or simulation.

The table gives the maximum **median-of-three matched-repeat result** across
the comparison groups. Individual runs vary; they are not each claimed to meet
the budget. Mode order rotates Off/Reduced/Full across fresh repeats.

| Metric | Full | Reduced | Frozen limit |
| --- | ---: | ---: | --- |
| Added median frame time | 628.5 µs | 158 µs | 1,000 / 250 µs |
| Added p95 frame time | 808 µs | 373 µs | 2,000 / 500 µs |
| Direct effect median | 532.5 µs | 117 µs | same absolute limits |
| Direct effect p95 | 663 µs | 145 µs | same absolute limits |
| Scene total median ratio to Off | 1.3398 | 1.1078 | Full ≤1.35 |
| Retained effect scratch | 952,320 B | 0 B | ≤8×width×height +256 KiB |

Off has zero effect time/storage and no flagged regression against the matched
three-run baseline. The relative headroom is modest. Cold maxima remain visible:
Off 14.276 ms, Reduced 15.326 ms, Full 13.953 ms. These independent cold values
do not imply Full is faster. Sampled process RSS peaks at 240,672 KiB; sampling
is every 100 ms and can miss final allocations. Cache accounting is allocated
payload capacity, excluding container/node/allocator overhead and noted fixed
or temporary data; it is not exact process memory.

The cost fix stays inside the effect pass: copy only its local rectangle, reject
zero-contribution pixels before expensive math, hoist repeated calculations,
and cap the Full radius at 45% of image height. The 2–3 pixel displacement still
crosses contrasting gate edges, as verified by darkening that positive glow
alone cannot produce. No geometry/detail/worker-policy optimization was used.

## Evidence and retained failures

`verification-evidence.tar.gz` contains baseline and candidate raw manifests,
PNGs, fresh checkpoints, logs, source snapshots, and reproduction helpers.
`archive-manifest.json` hashes every member; identical content uses archive
hardlinks so all logical files are retained without duplicate payloads.
Installed base assets and executable
binaries are not duplicated; their hashes are recorded. Final runs are under
`candidate/final-repeat-01`, `-02`, and `-03`; the passing calculation is
`candidate/budget-02.json`.

Earlier evidence remains intact: a missing include; two mock-font test crashes
repaired by real sprite fonts and layout-cache reset; a crop that included a
button row; a moving newspaper that affected the viewport pair; an initially
invisible interior warp; and performance failures before the bounded pass/radius
refinements. `candidate/budget-01.json` records the failed three-run matrix
(Full reverse 640-wide ratio about 1.355 against 1.35). It was not overwritten or
relabeled as passing. News is closed once before both final viewport captures.

Reproduce an accepted native run with the exact source/content/build recorded
here, using a fresh output path:

```sh
python3 scripts/test_cab_view_gate_native.py \
  --save build/cab-gate-native/fixture-01/cab-gate-depot-fixture.sav \
  --output build/cab-stage3-check/repeat-01 --profile --profile-rotation 0
```

Repeat with rotations 1 and 2, then use the archived
`helpers/compare-cab-stage3-effects.py` against the preserved extended baseline.
Helpers assume their original sibling-clone layout; archived paths identify it.

## Remaining boundaries

Human motion comfort, natural-speed interaction, interactive FPS and other
platforms remain unverified. Wider content/orientation coverage, layered vehicle
sprites (`seq[0]` remains), complete Commonwealth gate/station/industry artwork,
bridges and external federation are separate work. No owner repository, save or
running application was changed. No commit, push, PR, merge or release occurred.
