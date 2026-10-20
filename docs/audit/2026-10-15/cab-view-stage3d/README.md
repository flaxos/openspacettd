# Cab view Stage 3D: bounded Reduced-mode timing diagnosis

The declared diagnosis is complete. **No production repair is justified by the
measured evidence, and the investigation stops here.** The earlier timing failures
remain unresolved. This slice adds opt-in diagnostic instrumentation and guards;
production drawing, caching, palette behavior and thread policy are unchanged.

Work remains isolated, uncommitted, unpublished and default OFF on
`codex/cab-view-stage1`, base `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`.
Owner checkouts, saves and running applications were untouched. Earlier checkpoints
and failed results are preserved by [preservation verification](preservation-verification.json).

## What the single run establishes

Stage 3C used separate persistent renderers and framebuffers for each mode. Its
roughly 40-us output median gap confounded mode with context identity: output has
no mode branch. Stage 3D used one persistent renderer/framebuffer at the same
naturally reached cold `back_arrival_mixed` pose, 640 x 372, full scale, canonical
private palette, Arrival elapsed 0. After 20 Off and 20 Reduced warmups, 50 cycles
ran four adjacent pairs each: OR/AB/RO/BA on even cycles, reversed on odd cycles.
O/R mean experimental Off/Reduced; A/B are labels that both physically run Off.
There are exactly 100 experimental pairs and 100 negative-control pairs.

| Label / actual mode | Frames | Median | Nearest-rank p95 | Maximum |
|---|---:|---:|---:|---:|
| O / Off | 100 | 4,395.5 us | 6,202 us | 6,895 us |
| R / Reduced | 100 | 4,413 us | 6,627 us | 8,206 us |
| A / Off control | 100 | 4,361.5 us | 6,316 us | 7,722 us |
| B / Off control | 100 | 4,512.5 us | 6,076 us | 7,120 us |

The original-style metric uses experimental O/R only: p95(R)-p95(O) = **425 us**,
within the unchanged 500-us allowance **for this diagnostic only**. It does not
replace the three-repeat acceptance matrix. The retained original Stage 3B result
is still a failed 1,114-us median added p95; the later matrix recorded -313 us,
and Stage 3C's one diagnostic recorded 671 us. None is discarded or relabeled.

Paired R-O median changes sign with order: -61 us for OR and +64 us for RO
(pooled +1 us). Off/Off B-A paired medians are +27.5 us for AB and +112.5 us for BA
(pooled +87 us); their marginal p95 difference is -240 us. Paired differences and
marginal quantile differences are distinct statistics and are kept separate in
[the summary and all raw pairs](diagnostic-summary.json).

The largest Reduced frame, 8,206 us, contains 3,086 us preparation, 4,291 us
rasterization, 827 us output and zero measured effect time. Its adjacent Off
frame is 5,291 us. Physical-Off controls also have tails: a 7,120-us B frame has
4,983 us rasterization, versus 2,186 us in its adjacent A frame. These correlated
stages locate variability in shared work; they do not identify allocation,
thread startup, scheduling or CPU frequency as its cause. The same-context
output medians are now 470/472.5 us for O/R, but this does not retroactively prove
what caused Stage 3C's gap.

All 400 fresh frames have identical 6,256 triangles, 2,693 visible tiles and eight
raster stripes. Every image guard passes. Four retained PPMs exactly match each
other and Stage 3C; full camera/FrozenState/palette/cache identity also matches.
All 50 cycle checks preserve a 91-entry cache with 1,824,532 bytes of estimated
capacity payload, no vehicle textures and no effect scratch. The gate cue faces
away from this camera, so Reduced does no observed effect pixel work here.

Result and image storage is preallocated/pre-touched. The first member's capture
still perturbs the second member: actual intermember gaps are 37–113 us, with
43–45-us medians by order. They are retained, never subtracted. No allocations,
JSON, image validation, deep cache scan or state serialization occur in probe
bookkeeping between pair members. Renderer-internal allocations remain shared
unmeasured work. No additional timing run or speculative optimization followed.

## Verification and remaining limits

- OFF/ON builds pass; final ON hashes equal the measured binaries and OFF hashes
  equal Stage 3C. Default remains OFF.
- All 37 focused regressions and default Catch's 545 cases / 307,645 assertions pass.
- Full CTest passed 563/564 initially; its sole failure was sandbox denial of a
  private loopback socket. That test passed with local-socket permission, giving
  passing evidence for all 564 cases across those runs. Both repo linters pass.
- Native round trip/cold reload pass with 2,513 explicit simulation calls, an
  unchanged source save, 77 screenshot hashes and two exact normal-viewport pairs.
  Palette, screen/DPI and animation state are restored. Peak sampled process
  high-water was 237,184 KiB; sampling is every 100 ms, not an exact allocation peak.
- The controller accepts both real phase reports and rejects 14 malformed copies;
  a nonzero rounded Reduced effect timer is correctly allowed. See
  [controller validation](controller-validation.json).
- [Independent review](independent-review.json) verified source, binary, raw timing,
  images and historical identity, and confirmed the stop rule. See
  [verification](verification.json) for hashes, logs and commands.

Real multilayer and animated-palette vehicle qualification remains blocked by the
existing content gap recorded in the [Stage 3C inventory](../../2026-10-14/cab-view-stage3c/asset-inventory.json).
No assets were downloaded, built or rescanned. Human UAT and other platforms remain
unqualified. Resolving shared-path timing causation would need a separately bounded
measurement design; this result cannot establish stable broader performance acceptance.

[Source delta](source-delta.patch) applies to the preserved Stage 3C working state,
not directly to HEAD. The [evidence archive](verification-evidence.tar.gz),
[member manifest](archive-manifest.json) and [read-back check](archive-verification.json)
retain raw reports/images, exact source snapshots, helper scripts and verification
logs, including the initial sandbox failure. No binaries are distributed in the archive.
