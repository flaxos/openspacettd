# Stage 3B — ordered vehicle sprites and palettes

**Bounded engineering acceptance complete; independent review CLEAR.** This
uncommitted, unpublished checkpoint remains default OFF on
`codex/cab-view-stage1`, based on `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`.
The inherited renderer remains pinned to upstream
[`15.3-cabview-alpha1`](https://github.com/vaclavloubek/OpenTTD/releases/tag/15.3-cabview-alpha1),
commit `b7bd17b04dde3379b2be33be40ba4d6fc9f5d391`. Stage 1/2/3A patches and
audits remain exact. See the [active contract](../../../ACTIVE_EXECUTION_PLAN.md).

## Change and boundary

The three vehicle consumers now share an ordered compositor instead of taking
only `seq[0]`. It combines up to eight supported layers at their intrinsic
offsets, with native default/explicit/crash palette precedence, indexed colour
brightness, remap-zero transparency, black remapping and alpha composition.
Train/road boxes, ship and aircraft cutouts retain their direction, image type,
widening and placement policies. World/hidden/car checks still precede sprite
lookup; workers consume prepared immutable geometry and textures.

The union is rejected before allocation if either axis exceeds 512 pixels or
area exceeds 65,536 pixels. Cache identity includes ordered sprite IDs/modifiers,
effective palettes and widening. Existing frame-start eviction remains intact.
The 4,096-entry policy is not a hard RSS bound: one maximum-size float/alpha/mip
texture is approximately 1.4 MiB, and raw assets and other caches also use memory.
Recorded vehicle payload is a subset of existing texture accounting, not an
additional amount to add twice. Container/node/allocator overhead is excluded.

Destination-dependent transparent image modifiers and text-colour layers are
skipped with probe diagnostics. Ordinary supported layers remain ordered; an
all-unsupported sequence is empty. Aircraft shadow/rotor limitations remain.
No new renderer, art, GRF build, dependency, simulation, economy, save-format or
network change was made. Owner checkouts, saves and running apps were untouched.

The [review delta](renderer-delta.patch) compares the five affected source/test
and controller files with the sealed Stage 3A snapshot. Complete source snapshots
and exact build/content identities are included in the evidence archive.

## Pixel and regression proof

- Seven actual installed-OpenGFX diagnostic sequences match native
  `VehicleSpriteSeq::Draw` with **zero channel error** over complete black and
  white reference frames. The frozen allowance was one channel level. Cases
  cover two/eight layers, mixed fallback/override, explicit overlapping red/blue
  train layers in both orders, crash and black remapping. Reversal must visibly
  change pixels. [Raw metrics](sprite-proof.json),
  [red over blue](native-red-over-blue.png), [blue over red](native-blue-over-red.png).
- The oracle uses native `32bpp-optimized` at Normal zoom, matching the cab's
  actual Normal source. UI arrows/checkboxes provide the two/eight-layer cases;
  installed original-train sprite 2905 supplies the recolourable overlap cases.
  These are explicit diagnostic sequences, not a demonstrated SpriteStack GRF.
- Separate synthetic raw-pixel checks exercise half alpha, two nonzero RGB
  brightness values for indexed/remapped pixels, remap-zero and black/crash
  semantics through the production painter. They do not establish universal
  native integer-blend equivalence for arbitrary translucent 32bpp GRFs.
- 37 focused tests pass with 11,438 assertions. Default Catch passes 545 cases
  and 307,645 assertions. Initial full CTest passed 559/564: four fixtures needed
  the existing `regression_files` copy target and one private loopback test
  needed socket access outside the sandbox. All five targeted reruns passed;
  no production change was needed. The initial failures remain recorded.
- Final default-OFF and explicit-ON engine/test builds pass. OFF compiles no cab
  units or feature definitions. Final ON hashes match the final native matrix.
- Three final native repeats cover forward/reverse gate travel and cold reload:
  270 cells, 27,000 measured frames, 54 visible-vehicle cells and 231 GUI PNGs.
  All 72 fixed-scale Off image comparisons and six normal-viewport comparisons
  are exact. Frozen simulation and palette restoration checks pass throughout.
  Loaded native vehicle sequences all have one layer. Omission controls change
  1,419–2,594 pixels, proving the vehicle contributes to the diagnostic image.

[Visible-vehicle image](vehicle-diagnostic.png) uses a private camera beside a
naturally reached, paused arrival. It is a lossless PNG conversion of the raw
PPM, not a live player screenshot. Native GUI captures remain in the archive.

## Timing and memory

Same Linux i7-7700K, g++ 13.3 Release with assertions, SDL offscreen/software,
`32bpp-anim`, installed OpenGFX and fixture Commonwealth GRFs as the baseline.
Each cell uses a cold frame, 20 warmups, 100 fresh frames and a cached output at
480×272 and 640×372, with mode order rotated across fresh processes. Private
images use a scoped canonical static palette and restore all actual palette
entries/dirty flags afterward; live GUI palette and adaptive scale are unchanged.

| Final median-of-three maximum across groups | Full | Reduced | Limit |
| --- | ---: | ---: | --- |
| Added median time | 535 µs | 157 µs | 1,000 / 250 µs |
| Added p95 time | 971 µs | 317 µs | 2,000 / 500 µs |
| Direct effect median | 524 µs | 117 µs | same absolute limits |
| Direct effect p95 | 555 µs | 129 µs | same absolute limits |
| Scene median ratio to Off | 1.2964 | 1.1036 | Full ≤1.35 |

No matched Off regression exceeded max(250 µs, 10%); maximum matched delta was
128 µs. Visible-vehicle Off warm medians span 1.933–3.444 ms, with a 9.778 ms cold
maximum. Observed vehicle texture payload peaks at 6,816 B with a 320-pixel union;
total cache payload peaks at 1,824,532 B. Sampled process RSS peaks at 246,404 KiB.
These are diagnostic measurements on one machine, not interactive FPS guarantees.
See [the summary](performance-summary.json) and [all groups](performance-budget.json).

The explicit two/eight-layer microprobes took 22.030/39.171 µs for a fresh private
composite, then 3,565/4,781 ns total for 100 same-key hits after 20 warmups.
Texture addresses, entry counts and payload stayed stable. Raw/composite caches
start empty in each private renderer, while installed assets have already been
read by the oracle and native sprite/file caches may be warm. No p95, speedup or
comparison with an old multilayer implementation is inferred from these totals.

**An earlier matrix failed the unchanged tail budget.** Candidate `matrix-01`
cold reverse-arrival 640×372 Reduced had a median added p95 of 1,114 µs against
500 µs (individual deltas 1,569 / 866 / 1,114 µs). Direct effect samples were zero,
and Off checks passed, but the end-to-end failure remains valid; its cause is
unproven. Final `matrix-02` is separate, predeclared three-repeat evidence after
the last test-only ordering control and final OFF/ON builds. Production renderer
code is identical between the matrices. The final pass does not erase the earlier
failure or establish reliable tail latency across workloads.

## Retained evidence and remaining acceptance

[The evidence archive](verification-evidence.tar.gz) retains the complete baseline and both candidate
matrices, all failed attempts, native PNGs/PPMs, manifests, source snapshots,
build/test logs and reproduction helpers. Identical contents use archive
hardlinks; the manifest and full readback verification cover every logical member.
Executable/base-asset payloads are not duplicated; their exact hashes and local
baseline copies are preserved. [Verification](verification.json) and the
[independent review](independent-review.json) are the entry points.

Other retained failures include a Catch assertion compile error, an invalid
root-zoom `32bpp-simple` oracle, and historical baseline image mismatches. The
baseline static-palette control did not retroactively prove the causes of those
older mismatches. No failed file has been relabeled as passing.

Human appearance/comfort, actual multilayer NewGRF content, arbitrary translucent
32bpp assets, animated-palette cache fidelity, other vehicle content/platforms,
broader Commonwealth scenery and federation remain Pending. Palette contents
are baked into cached cab textures; no palette-animation epoch was added.
The next content acceptance should use an existing genuinely stacked vehicle
GRF on a disposable save, record its full sequence/palette identity, compare
layers/livery through real gate arrivals and repeat the relevant timing checks.
That later content test is not claimed by this checkpoint.

To reproduce the native matrix, use the recorded source/content/build and fresh
output paths; repeat with rotations 0, 1 and 2 and matching baseline repeat IDs:

```sh
python3 scripts/test_cab_view_gate_native.py \
  --save build/cab-gate-native/fixture-01/cab-gate-depot-fixture.sav \
  --output build/cab-stage3b-check/repeat-01 --profile --vehicle-profile \
  --profile-rotation 0 \
  --fixed-image-baseline build/cab-stage3b-baseline/matrix-02/repeat-01
```

For the focused native sprite proof, create a fresh output directory and set
`OSTTD_CAB_SPRITE_OUTPUT` to its absolute path before running
`ctest --test-dir build -R '^Cab_view_local_scene_mask$' --output-on-failure`.
