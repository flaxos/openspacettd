# Active execution slice — Reduced timing attribution, Stage 3D (15 October 2026)

This is the **sole current implementation handoff**. The owner authorized a bounded
diagnosis and evidenced repair of the Reduced-mode timing regression within the
performance stage. Retain the original failed results and unchanged 500-us added
p95 allowance. Stop if the measurement cannot isolate implementation cost or a
material design change is needed. Stage 3C is sealed by
`../cab-view-stage3d-start.json` and `build/cab-stage3d-baseline/`.

**Current gate: bounded diagnosis complete; stop without a production repair.**
The one declared 400-frame experiment preserved exact state, images and cache.
Experimental Off/Reduced p95 was 6,202/6,627 us: +425 us, within the unchanged
500-us allowance for this diagnostic only. Paired R-O medians were -61 us in OR
order and +64 us in RO order; physical Off/Off controls also varied. Slow frames
accumulated time in shared preparation/raster paths, with zero measured effect
time. Independent source and raw-data review found no attributable Reduced-specific
cost that justifies a narrow repair. The earlier 1,114-us failed matrix and 671-us
diagnostic remain unresolved; this is not broader performance acceptance.

Stage 3C's separate renderer/framebuffer comparison confounded mode with context
identity. The same-context run reduces that confound; it does not prove allocator,
thread startup, scheduling or CPU-frequency causation. Those remain hypotheses.
No further timing runs or production optimization are authorized by this bounded
slice. See the [Stage 3D audit](audit/2026-10-15/cab-view-stage3d/README.md).

## One declared paired experiment

- Use the existing naturally reached cold `back_arrival_mixed` scene at 640 x 372,
  canonical private palette, Arrival elapsed 0, fixed full scale, one persistent
  private renderer and one framebuffer. Preserve camera/world, full FrozenState,
  palette/DPI/animation state and the ordinary native journey/recovery guards.
- Warm the same renderer for 20 Off frames followed by 20 Reduced frames. Run
  exactly 50 cycles of four adjacent pairs: even cycles use OR, AB, RO, BA; odd
  cycles reverse that list to BA, RO, AB, OR. O/R are experimental Off/Reduced;
  A/B are recording labels that both physically execute Off. This yields exactly
  100 experimental pairs (50 OR/50 RO) and 100 Off/Off pairs (50 AB/50 BA), or
  400 measured fresh frames. Unique increasing frame IDs prevent cached output.
- Preallocate result and image storage. Between pair members, only capture the
  first member's output in preallocated storage and record its already-computed
  statistics; no file writes, deep cache scans, FrozenState serialization or
  allocations are permitted there. Record raw chronological starts/wall/stage
  times and actual mode/label/order. Apply identical capture/validation overhead
  in both arms. Validate both images after each pair; check full state/cache at
  cycle boundaries. Retain representative raw images and exact equality flags.
- Report original-style p95(Reduced)-p95(Off) using only the experimental 100+100
  samples, not all 300 physically Off frames. Also report signed paired R-O values
  split by OR/RO, Off/Off A/B contrasts and first/second position effects. Use
  nearest-rank p95 consistently. No alternative statistic can rescue a failed
  original metric; this one diagnostic does not replace a three-repeat acceptance
  matrix. If its original-style difference exceeds 500 us, state that clearly;
  otherwise it is only this diagnostic being within that numerical allowance.
- Run this declared experiment once. A state/image/work/cache failure invalidates
  interpretation. A mode-linked cost is only a candidate if it survives both
  pair orders on the same context, exceeds comparable control/position effects,
  and paired stage timings plus source inspection identify differential code work.
  If attribution remains inconclusive, stop and report it. Do not add a broad
  profiler, optimize shared rendering blindly, change thread policy, or cycle runs
  until a threshold passes. Any narrow evidenced repair requires independent
  review and a separately declared baseline/candidate paired validation.

## Boundaries and delivery

Sol owns narrow C++ diagnostic changes after source review; Root owns controller,
execution, analysis and records; Astra independently reviews method, source and
results. Existing process/call/disk limits remain. Code changes require the focused
and final repository checks. Keep the original 1,114-us Stage 3B failure, later
passing matrix, and 671-us Stage 3C diagnostic separate and immutable.

Real multilayer/animated-palette vehicle qualification stays explicitly blocked
for lack of a qualified installed asset; do not download replacements or repeat
that scan. Preserve owner checkouts/saves/apps, earlier checkpoints, default OFF,
and the uncommitted/unpublished state. No scenery, renderer architecture, simulation,
save-format or network changes are part of this slice.

---

## Historical Stage 3C and earlier records (preserved below)

# Active execution slice — Existing graphics qualification, Stage 3C (14 October 2026)

This is the **sole current implementation handoff**. The owner authorized the
smallest evidence-closing slice before scenery: identify installed assets, check
real multilayer vehicles and animated palettes where existing content permits,
and investigate the retained Stage 3B timing failure without changing thresholds.
Stage 3B is sealed by `../cab-view-stage3c-start.json`; its patch, delivery, audit,
source and binaries remain preserved. Work stays isolated, default OFF and
unpublished. Owner repositories, saves and applications remain untouched.

**Current gate: bounded diagnostic complete; graphics qualification remains blocked
and the timing cause unresolved.** One independently reviewed run retained 200
frames and eight identical images, with exact historical scene/state/cache identity.
Off/Reduced median was 4,177/4,224.5 us and p95 5,650/6,321 us. The 671-us diagnostic
p95 difference does not replace the original Stage 3B failed 1,114-us result or its
500-us threshold. Four cars queried in eight directions yielded 32 single-layer
sequences and zero animated-index contributions. This bounds the current fixture;
it does not establish absence across every installed engine, livery or cargo state.
No production rendering behavior was repaired or changed. Feature OFF/ON builds,
focused/default tests, all 564 CTest cases (one sandbox loopback rerun), and
independent evidence review have passing records. See the
[Stage 3C audit](audit/2026-10-14/cab-view-stage3c/README.md).

Installed OpenGFX and the project vehicle GRFs provide no demonstrated real
SpriteStack vehicle. Those GRFs declare properties only. Real-stack and animated
vehicle qualification require a suitable existing asset/state; dependent two-epoch
comparison stopped. Do not create art, compile a GRF or download content to turn
diagnostic stacks into acceptance evidence. Further causal timing instrumentation
or a rendering design change requires a new bounded slice, not repeated runs until
a threshold passes. The contract below records this completed diagnostic.

## Bounded comparison and stop rules

- Extend only the existing native fixture/controller and private renderer probes.
  At the naturally reached cold return safe endpoint, inspect each visible car's
  actual OnMap GetImage sequence in eight directions. Record engine/car/world,
  complete layer IDs, effective palettes and referenced Normal sprite pixels.
  Count visible indexed pixels whose effective colour lies in animated slots
  227–254, honoring source alpha, zero-remap transparency and all-black handling.
  Do not query hidden or other-world vehicles. A used animated index is a content
  candidate, not proof of an observed animated colour or cab/native fidelity.
- If that bounded scan finds no animated vehicle pixels, stop the dependent
  two-epoch vehicle comparison and state the asset/coverage limitation. Water or
  lighthouse movement must not substitute for vehicle proof. If it does find a
  candidate, define a source-backed, bounded two-epoch comparison before changing
  rendering; require actual colour change and native output, with all temporary
  visual state restored. No production repair without a concrete reproduction
  and independent review; stop at a material design choice.
- Investigate only cold `back_arrival_mixed`, 640 x 372, canonical private palette,
  Arrival at elapsed 0, fixed full scale. Use two persistent private renderers,
  one Off and one Reduced, with 20 warmups each. Run exactly eight 25-frame blocks
  in order Off, Reduced, Reduced, Off, Reduced, Off, Off, Reduced: 100 measured
  fresh frames per mode. Unique frame IDs force new scenes. Preserve ordered raw
  wall/stage times and relative start times; record block CPU/context switches
  outside frame timing where supported. Require exact image, geometry, cache and
  full FrozenState equality and restore DPI/palette/animation state.
- This single counterbalanced run is diagnostic, not a replacement acceptance
  matrix. Retain the failed Stage 3B matrix and unchanged 500-us Reduced p95 limit;
  do not extend runs until green, change worker policy/priority/affinity or infer
  causes by subtracting independently ranked stage percentiles. Scheduling,
  frequency, thread setup and allocation remain hypotheses until isolated.
- Keep existing process/controller/call/disk limits. Sol owns narrow C++ probes;
  Root owns controller, execution and records; Astra reviews plan, source and
  measured evidence independently. No new renderer, asset/dependency download,
  asset build, scenery, simulation/save/network change, commit, PR or merge.

## Delivery boundary

Record installed content hashes, scan coverage/blockers, timing distributions and
correlated raw tails. Rebuild changed diagnostic code, run focused and required
final checks once after the last relevant source change, independently review the
result, and retain reproducible source/binary hashes. Human comfort, untested
content/platforms and any unresolved timing cause remain explicitly Pending.

---

## Historical Stage 3B and earlier records (preserved below)

# Active execution slice — Vehicle layers and palettes, Stage 3B (14 October 2026)

This is the **sole current implementation handoff**. Following delivery of the
Stage 3A diagnostic preview, the owner directed the next bounded Stage 3 slice:
correct layered vehicle sprites and palettes using existing assets, with an
independent review and measured native comparisons. The feature remains default
OFF. Stage 3A is sealed in `../cab-view-stage3b-start.json`, with its exact binaries
and source preserved under `build/cab-stage3b-baseline/`; Stage 1/2/3A patches,
audits and failed attempts remain immutable. Historical contracts below are
retained as evidence and are superseded only for this bounded slice.

**Current gate: Stage 3B bounded engineering acceptance complete.** Seven native
sprite-oracle cases are pixel-exact; OFF/ON builds, 37 focused tests, default
Catch and all 564 CTest cases (including five targeted setup reruns) have passing
evidence. Independent source/image/data review is CLEAR. The final 270-cell,
27,000-frame matrix passes unchanged budgets, with 72 exact fixed Off image
comparisons. The earlier candidate matrix remains failed for Reduced added p95
1,114 us versus 500 us; its cause is unproven and final acceptance does not imply
stable tail latency. See the [Stage 3B audit](audit/2026-10-14/cab-view-stage3b/README.md).
Real SpriteStack GRFs, animated palettes, human comfort and other platforms remain
Pending. No further graphics slice or publication is implied by this checkpoint.

Three matched baseline
repeats in `build/cab-stage3b-baseline/matrix-02/` passed 270 profile cells
(27,000 measured frames), 54 visible-vehicle cells, exact fixed-scale repeat
images and normal viewport recovery before production rendering changed. The
preserved engine binary is exactly Stage 3A. Native visible sequences have one
layer; two/eight-layer proof remains an explicit diagnostic. No owner game/save/app, simulation, serialization, network protocol,
vehicle movement, gate registry or economic code is to change. No new renderer,
asset purchase/download/build, dependency install, publication, merge or history
rewrite is authorized or needed.

## Smallest acceptance target

Replace the three first-layer-only consumers in the cab renderer with one shared
vehicle-specific ordered compositor. Trains/road vehicles keep their existing
south-west OnMap box projection; ships keep their existing direction/InDetails
cutout; aircraft keep their existing direction/OnMap cutout. Preserve each path's
widening, sizing and placement policy. Honor up to eight sequence entries at a
shared origin using each sprite's intrinsic offsets, ordered alpha composition,
union bounds, default/explicit palette precedence, and native crash palette
selection. Correct remapped index-zero transparency and 32bpp remap brightness
within this vehicle path, without changing the shared terrain/scenery painter.

Cache identity includes the complete ordered sprite IDs/modifiers, effective
palettes and widening policy. Reject unions over 512 pixels on either axis or
65,536 pixels in area using checked signed arithmetic before allocation. Retain existing texture
cache limits, include payload/key storage in diagnostics, and exercise reset on
close/reload/content change. Keep world/hidden/car rejection before GetImage and
raw-sprite lookup; sprite work remains on the main thread and workers consume
prepared immutable data.

Native destination-dependent transparency/remapping and shadows are outside this
source-texture compositor's claim. Do not add a framebuffer pass or silently claim
those modes are native-exact. Existing aircraft shadow/rotor handling remains as
before. A real SpriteStack NewGRF acceptance test remains Pending: checked-in
Commonwealth NML and installed content have no demonstrated stacked vehicle.
Existing OpenGFX sprites assembled into explicit diagnostic sequences, checked
against native VehicleSpriteSeq::Draw, establish compositor behavior only.
No new GRF compilation or art is part of this slice.

## Ownership, evidence and limits

- Sol owns focused C++ fixture instrumentation first, then the renderer/helper and
  focused tests after the baseline gate. Root owns runner extensions, build/run
  execution, measurements, canonical docs and packaging. Astra independently
  reviews the source and evidence. No additional task tracker is needed.
- Extend the existing native controller with optional visible-vehicle profiles at
  a genuinely reached paused mixed arrival. Offset only a private diagnostic
  camera beside the arrived vehicle; never write train poses. Record real
  GetImage sequence counts/IDs/palettes and the camera, world, content and binary
  identity. Retain the original Stage 3A gate/effect matrix and viewport recovery.
- At 480 x 272 and 640 x 372, run cold + 20 warmup + 100 fresh frames + cached
  output, for Off/Reduced/Full in three fresh-process rotated repeats. All cells
  must retain exact FrozenState equality and full-size fixed-scale rendering.
  Report cold/warm median/p95, repeat spread, cache payload and sampled RSS.
- Private profile images use the existing canonical DOS palette in an explicitly
  scoped test-only override, restoring every palette entry and dirty flag before
  returning from the main-thread callback. Record the actual incoming palette
  separately. The callback holds the native game-state/video locks; worker draws
  finish before restoration. Live GUI rendering and its adaptive scale/palette
  remain unchanged. Require exact fixed-scale Off baseline/candidate images,
  exact within-run Off/Reduced stability and normal viewport recovery. Retain
  actual GUI captures/scale metadata and every failed historical comparison.
  Static-palette evidence does not establish animated-palette cache fidelity.
- Investigate matched Off median regressions above max(250 us, 10%); retain the
  Stage 3A Full/Reduced effect and storage limits. The two/eight-layer diagnostic
  has no old multilayer baseline: report cold-composite time and aggregate 100
  same-key cache-hit nanoseconds separately, with asset-cache warmth stated. Do
  not invent a relative budget or p95 from one aggregate. Require no new entries
  or payload growth on hits and enforce the union allocation bounds above. Preserve
  the controller/process timeouts, simulation-call and disk-reserve stops.
- Focused tests cover order, offsets/union, alpha, fallback/override/crash,
  zero-remap, brightness, empty entries, eight-entry limit, key differentiation,
  allocation bounds/reset and 8bpp output where appropriate. A native draw oracle
  must use real installed sprite assets; report exact versus tolerance-based
  comparisons and any intentionally synthetic raw-pixel cases separately.
- Native evidence includes an actually visible vehicle, selected-world masking
  positive/negative controls, gate round trip/cold reload, paused purity, normal
  viewport recovery, and comparison of preserved Stage 3A scenes. Final OFF/ON
  builds, focused regressions and independent review are required. Human comfort,
  real stacked-GRF content and cross-platform UAT remain Pending.

---

## Historical Stage 3A and earlier records (preserved below)

# Active execution slice — Gate crossing effects and measured baseline, Stage 3A (14 October 2026)

This is the **sole current implementation handoff**. The owner directed continued
work after Stage 2 acceptance and specifically requested animated event-horizon
crossing distortion and visual effects. Stage 3A is bounded to transient cab
presentation and reproducible profiling. It supersedes the historical statements
below that Stage 3 is not authorized, only for this slice. Stage 1/2 patches,
audits, failed attempts and Pending human acceptance remain intact. The feature
stays default OFF. Publication, merging, owner-save changes, external federation,
new dependencies and a general graphics/backend rewrite are outside this scope.

**Current execution gate: Stage 3A automated engineering complete.** Final OFF/ON
builds, 37 focused tests (11,198 assertions), three native repeats and independent
source/image/data review pass. The final binary matches the measured binary.
See [the Stage 3A audit](audit/2026-10-14/cab-view-stage3a/README.md). Human comfort
and cross-platform acceptance remain Pending. The contract below records the
authorized slice and frozen measurement rules; it does not authorize broader work.
The accepted Stage 2 source/image/evidence review is CLEAR: both build modes,
36 focused tests, native forward/reverse/cold passage and normal viewport recovery
passed. Its local gate floor repair and outward-pose regression are prerequisites,
not work to replace. See [the Stage 2 audit](audit/2026-10-14/cab-view-stage2/README.md).

## Player outcome and state contract

Show a restrained lensing ripple/glow as a validated local gate fills the cab,
an opaque procedural crossing effect during genuine hidden transit, and a short
settle into the actual arrival scene. Add a window-local Full / Reduced / Off
control; no persistent setting or save change. Full may animate; Reduced uses
subdued static presentation without distortion or travelling streaks; Off must
retain accepted Stage 2 scene/transit pixels. Never flash the entire view or
strobe. Keep the transit status text readable in every mode. Reopening starts
with a documented window default; the final default is part of the effects review.

| Observed native state | Allowed presentation |
| --- | --- |
| Ordinary supported scene | Existing masked scene; no gate effect without a valid nearby gate. |
| Approaching a physically valid accessible local gate | Mild bounded ripple/glow within two tiles, facing the entrance. Read only the current validated endpoint and already-masked current frame. No destination scenery or screen-buffer sampling. |
| `LocalTransit` | Opaque dark background with restrained blue/cyan rings or stretched light. This path must not collect world geometry or vehicle sprites. |
| Actual accessible arrival | Reset world/camera caches first, then settle the newly validated scene for at most 250 ms. Never retain source imagery or anticipate the destination. |
| Scene-to-scene discontinuity with transit missed by painting | Reset immediately and optionally settle the actual arrival. Do not invent or replay hidden transit. |
| Invalid identity, owner, access, session or unsupported gate | Existing safe closure; discard all effect state. |

Animation time belongs to the cab window and advances only through its real-time
tick callback. Paused animation must leave simulation snapshots unchanged. Use
no game RNG, game-time advancement, train pose writes or portal-progress writes.
Observe transitions in a common presentation-update path: `CurrentPresentation`
also runs during painting, so it must not consume a phase/world change before
the effect state sees it. Repeated painting at the same state/time is identical.
Reverse passage follows the same rules. Look, resize, effects-mode changes,
close/reopen and cold loading must not preserve an obsolete effect phase.

Effect sampling is bounded to the private, already-masked current frame. Build
each effect result from that immutable frame, never from the previous distorted
result. Reuse bounded scratch storage and clip reads/writes to the cab rectangle.
Handle cached-frame output as well as full rasterization; Off bypasses the effect
work. Raster workers still consume prepared immutable data only. Native UI
framebuffer and animation-buffer access remain on the main thread.

## Baseline-first Sol handoff and ownership

1. **Instrument, without changing pixels or policy.** Sol owns
   `src/cab_view_3d.h`, `src/cab_view_3d.cpp` and a compatible profiling extension
   to `src/tests/test_cab_view_gate_native.cpp`. Provide last-frame timings and
   counts through read-only diagnostics: full render versus cached output,
   view/internal dimensions and scale, selected world/camera, tiles, triangles,
   worker count, prepare/raster/output/total microseconds and effect time (zero
   before effects). Keep cold frames rather than dropping them from the report.
   The existing smoothed `GetCabView3DFrameTime` is not a benchmark sample.
2. Expose memory accounting separately from the timed draw: raw sprite, ground,
   box and future effect-cache counts; allocated payload capacity in bytes; frame,
   depth and scratch buffers. State which container/node/allocator overhead is
   excluded. Do not sum every cache entry inside each timed frame or call the
   result exact process memory. Root records process RSS/high-water separately.
3. **Root owns** the optional `--profile` extension to
   `scripts/test_cab_view_gate_native.py`, all build/run execution, manifests and
   evidence. Astra owns this plan only during planning and independently reviews
   source/evidence. Root owns other canonical documents and final packaging.
   CMake/test registrations may change only if required for this focused proof;
   do not add a second task tracker or a general benchmark framework.
4. Stop after baseline collection for the measured budget decision below. Only
   then may Sol implement the approved effect presentation in `src/cab_view.h`,
   `src/cab_view_gui.cpp`, `src/cab_view_3d.{h,cpp}`,
   `src/widgets/cab_view_widget.h`, `src/lang/english.txt`,
   `src/tests/test_cab_view.cpp` and the existing gate-native controller. A small
   shared transit drawing helper is permitted if it preserves baseline pixels
   and allows production and benchmark code to exercise the same opaque path.
   No changes to train movement, portal registry, world ownership, economy,
   network protocol or save serialization are needed or authorized by this slice.

## Reproducible measurement matrix

Use the existing disposable four-car fixture and installed OpenGFX plus the exact
loaded Commonwealth industry/rail content. Record file hashes and actual loaded
content identifiers, source and binary hashes, compiler/build/assert settings,
CPU model/logical concurrency, OS, SDL driver, blitter, display and view sizes,
view distance, selected world, camera pose and simulation snapshot for each case.
No downloads or regenerated art. Retain baseline and candidate evidence separately.

Profile naturally observed, paused approach, opaque transit and mixed arrival in
the native forward route; include corresponding reverse captures and measurements.
Do not fabricate a train position or hidden state. Use the current native camera
pose in private draw buffers with `_screen`, `_cur_dpi` and `_screen_disable_anim`
scoped/restored exactly as in the accepted mixed-car probe. Transit profiling
exercises opaque transit drawing only, with scene geometry/raster fields marked
zero or not applicable. A diagnostic renderer measurement is labelled separately
from the actual GUI screenshot and its observed crossing timing.

For each measured phase use view sizes **480 x 272** and **640 x 372**, one cold
sample, **20 warmup frames and 100 measured frames**, across **three fresh-process
repeats**. Force new frame IDs for full scene-frame samples; report cached output
separately. Use zero adaptive budget in a fresh fixed-scale profiling context and
record actual internal size so adaptation cannot silently change the comparison.
Compare matched camera/world states; disclose differing naturally observed poses.
Do not consume or modify the GUI's effect clock to run a benchmark. Later effect
profiles use immutable descriptors at documented representative elapsed times;
they do not purport to be natural gameplay timing.

Compare Off, Reduced and Full with the same scene, dimensions, sample policy and
content. Report cold time, warm median/p95 and repeat spread for prepare, raster,
effect, output and total, plus memory/cache counts and process RSS. Evaluate
statistics outside timed drawing. Preserve the existing 180-second controller,
240-second process and 100 MiB disk-reserve stops. Profiling adds no simulation
calls: retain 8192 per directional pass, 24576 combined, including depot warmup.
Assert the full tracked FrozenState before/after every paused profiling cell.

**Budget frozen after baseline (14 October 2026).** Initial three repeats cover
far approach, transit and arrival; three extended repeats add genuine near
approach at 16 units from the rendered gate face, with zero lateral offset.
All 24 extended comparison groups have identical poses across repeats. Evidence:
`build/cab-stage3-baseline/summary-01.json` and `near-summary-01.json`.
On this i7-7700K (8 logical CPUs), 640 x 372 warm scene medians span roughly
1.4–4.6 ms; cold frames reach 14.373 ms. Repeat noise is retained, including a
1.239 ms spread in the return near-approach 480-pixel case. These are private
fixed-scale renderer measurements, not interactive FPS or comfort claims.

- Full incremental effect cost: median <=1.0 ms and p95 <=2.0 ms. Scene total
  median must also be <=1.35 times the matched candidate Off median.
- Reduced incremental effect cost: median <=0.25 ms and p95 <=0.50 ms.
- Evaluate the median of three matched-repeat timing deltas and ratios, rotating
  Off / Reduced / Full order across repeats. Retain each repeat and direct
  `effect_us` measurements. Transit uses the absolute allowance because its
  baseline is only about 0.04–0.13 ms.
- Off preserves Stage 2 pixels with zero effect work. Investigate median total
  regression above max(0.25 ms, 10% of matched baseline); do not dismiss it based
  on a single noisy run.
- Additional retained effect storage <=8 * width * height + 256 KiB; repeated
  same-size phase/mode cycles cannot accumulate storage. Geometry counts and
  world masks stay identical between modes. Report cold timings separately.

The independent review cleared implementation with these limits. It did not
grant final engineering or human acceptance. No culling, cache, worker-count or
detail change is authorized without a measured bottleneck and bounded review.

## Acceptance and next review

- Baseline instrumentation leaves Off pixels and tracked simulation state exact;
  timing and memory data are reproducible, complete and honestly scoped.
- Focused tests cover mode switching, deterministic repeated paints, bounded
  sampling/resize, phase resets, skipped short transit, reverse, access/session
  loss, and Off equivalence. Keep selected-world rejection before sprite lookup.
- Native screenshots demonstrate visible approach animation, opaque crossing and
  actual arrival in both directions. Capture more than one observed real-time
  frame while naturally paused in a phase to show animation, while proving
  FrozenState equality. Never extend the simulated transit to obtain a picture.
- Fresh-process reload, mixed-world consist suppression, exact normal viewport
  recovery, default-OFF/explicit-ON builds and the existing focused regression
  set continue to pass. Check 8bpp bounded output as well as native 32bpp.
- Compare candidate measurements against the recorded budget and preserve any
  failures. Independent source/image/evidence review is required before Stage3A
  engineering acceptance. Human comfort/gameplay and cross-platform acceptance
  stay Pending; no broader Stage 3 graphics completion or publication follows.

Known deferred issue: `AddVehicles` currently uses only `VehicleSpriteSeq::seq[0]`,
although native sequence drawing honors all layers and their palettes. The
checked-in Commonwealth rail/equipment NML defines properties using base sprites,
not a demonstrated sprite-stack fixture. Do not claim layered Commonwealth train
fidelity from the current WP11 assets. Composite vehicles, complete custom gate
front/rear artwork, stations, industries, catenary and bridges remain later work.

**Accepted Stage 3A result:** Full adds bounded local edge lensing, opaque animated
transit and a <=250 ms settle into the actual destination. Reduced is static and
Off retains Stage 2 pixels. A newly opened cab defaults to Full; the preference
is window-local and unsaved. The feature itself still defaults OFF. The final
median-of-three matrix passes: maximum Full added median 628.5 us, added p95
808 us and scene ratio 1.3398; Reduced 158 us / 373 us. The ratio has modest
headroom and individual runs vary. All 27 Off image comparisons and six ordinary
viewport comparisons are exact. No simulation, save or network implementation
changed.

**Next action:** hand off the isolated checkpoint and perform human motion/comfort
UAT on a disposable fixture. Broader Commonwealth sprite layers, other platforms
and publication remain separate work. Historical Stage 1/2 text below is retained.

---

## Historical Stage 2 and earlier records (preserved verbatim below)

The following original handoffs retain their historical authorization/status
wording. The Stage 3A section above is the current authority for its bounded scope.

# Owner decision, 2026-10-03: unlimited offline development money

## Active execution slice — Local gate cab, Stage 2 (14 October 2026)

The owner accepted Stage 1 as engineering evidence and authorized this bounded
Stage 2 continuation on the same isolated `codex/cab-view-stage1` lineage. This
section is the **sole current implementation handoff**. Stage 1 and all older
sections below are historical snapshots; their Pending human acceptance and
consumed budgets remain intact. Stage 3, publication, merging and external-server
continuation are not authorized. The feature remains default OFF.

**Engineering checkpoint — bounded Stage 2 complete.** Direct owner approval
resolved the automatic approval blocker. Final OFF/ON builds pass both native
targets; all 36 focused regressions (11,160 assertions), both repository linters
and whitespace checks pass. Native attempt 02 passes the A→B→A journey and
fresh-process cold reload (47 assertions, 2,519 explicit simulation calls),
three mixed-consist render probes and both exact normal-viewport comparisons.
Independent Astra source/image/evidence review is CLEAR. Attempt 01's missing
local gate floor was repaired with bounded current-tile geometry and outward
arrival regressions; failed attempts remain preserved. See the
[Stage 2 audit](audit/2026-10-14/cab-view-stage2/README.md).
Human UAT, cross-platform behavior and performance qualification remain Pending.
The feature remains default OFF; nothing is committed or published.

Preserve `/home/flax/Documents/Codex/2026-10-14/task/cab-view-stage1.patch` exactly;
its verified SHA256 is
`42754c64b80c8d2b7d892af4f3177180be81e33ba7fcfddca665145d531748fe`.
Do not overwrite it with Stage 2. Preserve the Stage 1 audit and failed native
attempts. No other checkout, owner save or running desktop session is writable.

### Player outcome and source contract

Keep one owned train's cab open across a local two-world gate journey, show a
recognizable local gate and an opaque transit state, then snap to the train's
actual accessible arrival world. Repeat in reverse, without showing void,
destination scenery early, or wagons remaining in the other world. Native train
commands, movement, gate admission, economy, network protocol and custom save
formats remain unchanged. This is presentation of existing authoritative state.

Source checked for this handoff: `VehicleEnter_TunnelBridge` sets each entering
car's tile, `Track::Wormhole` and `VehState::Hidden`; `TrainController` advances
existing `PortalRegistry` progress and, after `PORTAL_TRANSIT_DISTANCE` (16
movement units), assigns exit tile/track/position/height and moving direction,
clears progress and unhides that car. Consist cars may occupy different worlds.
`PortalRegistry::ClassifyGate` distinguishes local linked, unlinked/stale,
external and conduit heads; `IsPortalTile` alone does not. `GetMovingDirection`
accounts for backing independently of sprite-facing `direction`. These are
read-only inputs to the cab; the cab must never call transit/admission mutators.

### Presentation state and failure contract

| Observed state | Required cab behavior |
| --- | --- |
| Visible owned primary train on supported ground | Render only its actual accessible world and position; use its moving direction for camera heading. Keep the same primary vehicle identity when backing. |
| Visible train on a physically valid `LocalLinked` head | Permit the cab, distinguish the head from an ordinary tunnel, and render only the local endpoint. Classification and endpoint world metadata must agree with actual map ownership. |
| Hidden / `Track::Wormhole` on that valid local head | Keep the window with an opaque localized “Local gate transit” presentation; collect no scene or vehicle sprites. Do not project a tunnel, preview the remote scene, or compute camera travel from progress. Zero progress is possible at initial entry. |
| Visible arrival or discontinuous actual pose/world change | Recheck actual tile and coordinate world/access, clear frame/depth/scene caches and transient camera history before any output, then adopt actual pose/world. Never interpolate over the gap. |
| Access lost, invalid identity/session/company, stale/unlinked/external/conduit head, ordinary tunnel, bridge/underbridge or hidden state without a valid local pair | Close safely and invalidate renderer state before further sampling. Existing deletion/replacement/ID-reuse and new/load/menu safeguards remain active. |

Sample on the main thread and validate again before drawing; no stored live
vehicle or gate pointer may outlive that operation. A valid local hidden state
must also validate the destination's world access, but must not reveal hidden
destination scenery or metadata in the status message. Never call generic
`GetOtherTunnelEnd` for a portal, invalid head or conduit.

The 16-unit transit can occur between GUI samples. Direct visible arrival in an
accessible world must reset and use the authoritative train pose even when no
hidden frame was observed; do not require a synthetic entry event or fabricate
missed frames. Conversely, no stale previous-world framebuffer may be reused.
Reset on world changes, position jumps greater than two tiles, moving-direction
changes, height changes and entry/exit from transit; reset local look offset on
transit/world/teleport discontinuities. Ordinary local look controls remain
read-only and retain the corrected left/right convention.

Per-car visibility remains independent: both its authoritative tile and actual
coordinate tile must belong to the selected accessible world, and hidden cars
must be omitted before image lookup. Never apply the primary train's world to
the whole consist. Preserve shared native terrain-corner sampling and complete
viewport-state restoration; this remains one global map with logical regions.

### Bounded Sol implementation handoff

1. Implement a small inspectable presentation snapshot/state resolver in
   `src/cab_view.h` / `src/cab_view_gui.cpp`, separating visible scene, local transit
   and unsupported state. Reuse existing session/identity guards. Keep opening,
   painting, local controls and closure free of simulation commands/mutations.
2. Add an explicit local-gate renderer path in `src/cab_view_3d.cpp` before the
   generic tunnel/bridge suppression. Use a bounded local frame/event-surface
   representation from the current endpoint and existing rendering primitives;
   do not reconstruct an ordinary tunnel interior or sample the remote endpoint
   for geometry. Full Commonwealth front/rear sprite fidelity remains Stage 3.
   Retain conservative suppression of unsupported tunnel/bridge types.
3. Reset the existing renderer and transient pose/look state on the boundaries
   above. Any new state is window-local and unsaved. Preserve sprite reload
   invalidation, synchronous worker completion and native main-thread collection.
4. Extend cab fixtures with real local gate classification, transit/arrival,
   missed-transit sampling, backing, access loss, stale/external rejection,
   same-position direction/height changes and per-car mixed-world masking. Prove
   actual rendered suppression separately from a policy-helper assertion.
5. Extend the opt-in native proof for a disposable two-world pair and consist:
   approach, opaque hidden state, arrival, reverse return, independent wagon
   masks, normal viewport recovery and a fresh-process cold reload. Reuse existing
   native command/save harnesses. The parent owns fixture preparation, build/run
   execution, budgets and final evidence; coordinate before changing the runner.
6. Stop for independent source/evidence review at Stage 2. Do not begin Stage 3,
   publish or modify movement to make the presentation test pass. A genuine
   simulation defect is a replan trigger, not permission to expand this patch.

Code ownership: `src/cab_view.h`, `src/cab_view_gui.cpp`,
`src/cab_view_3d.cpp`, `src/cab_view_3d.h`, `src/lang/english.txt`,
`src/tests/test_cab_view.cpp`, `src/tests/test_cab_view_native.cpp`,
`src/tests/CMakeLists.txt`, and the agreed extension to
`scripts/test_cab_view_native.py` (or one focused Stage 2 runner).
CMake registration changes are allowed only if required by a new test source;
keep source-specific feature definitions consistent in both targets. No changes
to `train_cmd.cpp`, portal simulation/registry, save handlers, networking,
economy, map storage or owner state are planned or authorized by this handoff.

### Acceptance, bounded proof and next action

Record source/binary/content/save hashes and actual command/tick budgets. Reuse
the existing build and installed dependencies; only the parent runs builds.
Retain failed attempts and keep Stage 1's immutable patch as the baseline.

| Gate | Required evidence |
| --- | --- |
| Local classification and access | Linked physical pair accepted; ordinary tunnel, conduit, external, stale and unlinked cases fail safely. Closed destination cannot expose geometry, vehicles or stale pixels. |
| Camera state | Arrival changes world/pose without interpolation; position, direction, height and phase discontinuities invalidate prior image. Test a skipped hidden sample as well as an observed one. |
| Native round trip | Real native commands/ticks traverse A→B and B→A with the same primary identity; screenshots show approach, transit and both arrivals. No forced production movement or coordinate edits. A test-only pause may capture a naturally reached transit state. |
| Mixed-world consist | Source-world, hidden and arrived units coexist in the fixture; actual rendered results include only visible units in the camera world. Confirm mask before image lookup, not only through final overdraw. |
| Purity and 2D isolation | Paused camera actions preserve deterministic RNG, date, money, positions and full relevant orders/consist cargo snapshot; intentional native movement commands are recorded separately. Normal viewport restoration is compared on an explicit unaffected region. |
| Cold reload | Save the disposable gate/consist state through the existing engine path, load in a fresh process, verify serialized gate/world/vehicle/order/cargo state and absence of persistent cab state, reopen and traverse again. Do not compare naturally advanced RNG/ticks as if load-time rendering changed them. |
| Regressions | Stage 1 cab tests plus selected portal classification/wormhole, consist traversal, gate sprites, access and persistence cases; final OFF/ON build availability, both repository linters and diff check. Choose exact matching test names and retain isolated CTest cases. |

Fixture refinement: the original disposable
`build/cab-native/fixture-02/cab-native-fixture.sav` has only two cars, so it cannot
simultaneously demonstrate source-world, hidden and arrived units. Retain that
path as the historical fallback for simpler checks, not the mixed-consist proof.
The parent prepares a fresh WP11 prepare/build save before its first advance at
`build/cab-gate-native/fixture-01/cab-gate-depot-fixture.sav`. In this disposable
profile the native test uses ordinary `StartStopVehicle`, `BuildVehicle` and
`MoveRailVehicle` commands in the depot to grow train 0 to four cars (the same
primary plus three wagons), preserving the primary VehicleID. Setup commands are recorded separately; no
coordinates, movement state or production fixture harness were edited.
Setup/warm-up advances count within the first 8,192-call pass cap. This fixture
passed native attempt 02; the earlier attempt is retained as a visual failure.

Use train 0, world 0 gate `(70,40)` facing SW paired with world 1 gate `(112,40)`
facing NE. Other fixture worlds/pairs are outside the proof. Observe after each
explicit `StateGameLoop` call so the short hidden state and mixed-world consist
can be captured; do not skip inspection across an eight-tick batch. Use existing
native `ReverseVehicle` / `SkipToOrder` commands for fixture steering and record
them separately from camera actions. Do not set train coordinates or movement
state directly. Pause a naturally observed phase only for capture/comparison.

Recommended run caps: at most 8,192 explicit game-loop calls per directional
pass, two initial passes (out and back), one saved endpoint and one fresh-process
post-load pass; at most 24,576 explicit calls total. Give each process a
180-second controller deadline and 240-second hard subprocess stop. Count native
automatic ticks separately where available; these caps are explicit calls, not
an assertion that no normal timer ticks occur. The parent may tighten these
bounds after inspecting route distance and controls; increasing them requires a
recorded bounded rationale, not an open-ended retry. Compare frozen full consist,
orders, money, RNG and date immediately after cold load, before explicit advance.

No economy campaign, network federation test or new progression
slice follows implicitly. If native movement cannot meet the local round-trip
contract within that budget, retain failure evidence and investigate without
changing simulation. Human mission: observe the local round trip, resize/look,
confirm travel orientation and safe handling of access/unsupported states.
Human UAT, cross-platform behavior and performance qualification remain Pending.

Next action: review the completed Stage 2 evidence and use the roadmap to scope
the smallest demonstrated Stage 3 graphics defect and profiling route. Do not
expand this completed local-gate slice implicitly. The [roadmap](PROJECT_STATUS_AND_ROADMAP.md#stage-2--world-and-gate-transition-authorized)
keeps Stage 3 behind its separate decision gate.

---

Historical Stage 1 handoff follows; its “current”/authorization wording describes
the earlier checkpoint and is superseded only by the Stage 2 section above.

## Active execution slice — Experimental train cab, Stage 1 (14 October 2026)

This is the sole current implementation handoff for `codex/cab-view-stage1`.
The owner authorized planning all three cab stages and executing **Stage 1 only**.
Older sections below retain their historical authorizations, consumed budgets,
failures and Pending acceptance; none is reopened by this slice. The staged
sequence and later prompts live in the [canonical roadmap](PROJECT_STATUS_AND_ROADMAP.md#cab-view-integration-sequence--14-october-2026).

Implementation checkpoint: the local Stage 1 prototype is present. The
[audit packet](audit/2026-10-14/cab-view/README.md) records final build/test results,
failed attempts and independent review. The native SDL offscreen proof now passes
paint, both resize axes, local look controls, four-tile movement and cleanup.
Human acceptance remains Pending; Stage 2 still requires separate authorization. No commit, publication or integration into main is implied.

### Native proof continuation (14 October 2026)

The owner explicitly authorized completing Stage 1 native paint, resize, local
controls, movement, adjacent viewport, masking and cleanup proof on a disposable
profile before expanding scope. Use the already-installed SDL offscreen driver;
no owner display or save is needed. Reuse the existing WP11 native fixture builder
and a hidden test-only controller, with bounded commands/ticks, screenshot hashes
and timings. Fix ordinary scoped defects, retain failed evidence and obtain a
new independent review. This is automated visual evidence; human UAT stays Pending.
No Stage 2, install, system change or publication is authorized.

### Outcome and starting contract

With an explicitly enabled development build, the player opens one native cab
window from an owned primary train's ordinary vehicle window, watches the train
move through an accessible world, adjusts local camera controls, and closes it.
The normal isometric window remains correct. The cab dispatches no simulation
commands and changes no orders, economy, construction, RNG, networking or saves.

Implement in the isolated checkout `openspacettd-cabview`, branch
`codex/cab-view-stage1`, based on live main
`8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`. The Antigravity primary checkout and the
older `ost-first-permanent-line` worktree are inspect-only. PR #83 remains open;
#85 was merged into that branch, not main. Neither is a cab dependency and neither
should be incorporated implicitly. This is a dated baseline, not a permanent
claim about remote branch state.

Port source from `vaclavloubek/OpenTTD`, tag `15.3-cabview-alpha1`, commit
`b7bd17b04dde3379b2be33be40ba4d6fc9f5d391`; preserve GPL notices and attribution.
Use the source implementation, not its release binary. Upstream native 15.3
interfaces differ from this repository's current scoped enums and GUI APIs.
The [audit packet](audit/2026-10-14/cab-view/README.md) records provenance, findings
and actual verification separately from this acceptance contract.

### Bounded Sol implementation handoff

1. Add CMake `WITH_CAB_VIEW`, **OFF by default**. Only ON compiles/links the cab
   renderer, GUI and focused tests and exposes its UI entry. Use the native CPU
   renderer in `src/cab_view_3d.cpp` / `.h`; do not import the full upstream fork.
   Keep a single cab window and train support only. Use a desktop video driver
   with supported output; reject headless/unsupported output safely.
2. Add a small native vehicle-window entry and read-only local camera controls.
   Do not port cab start/stop/reverse controls, road/ship/aircraft selection,
   map editing, global settings or persistent camera state. Enable only for a
   valid owned primary train in a supported visible state. Camera state belongs
   to the window and dies with it.
3. Isolate `CollectTileGroundSprites` with a fresh temporary `ViewportDrawer`.
   Restore the entire previous drawer, including pending lists, sort pointers,
   foundation indices/offsets and child links, on every return/exception. Also
   restore `_cur_ti`, `_cur_dpi`, transparency and invisibility state. The upstream
   empty-list assertion plus list clearing is insufficient. Never leave pointers
   into moved or destroyed scratch buffers. Native sprite collection and vehicle
   inspection stay on the GUI/main thread; workers receive complete immutable
   render inputs and finish before frame/window resources can disappear.
4. Mask every candidate terrain/object/bridge/sprite contribution and each vehicle
   before collecting its image or geometry. Resolve current APIs `PlanetManager::GetTileWorld`,
   `GetRegionByTile`, and `StellarNetwork::WorldAccessible`; the older guide's
   `GetWorldAtTile` wording is not an existing API. With registered worlds, draw
   only the followed train's accessible world and suppress void/other worlds.
   On ordinary maps without registered worlds, allow native map tiles. Recheck
   access each frame, including cache invalidation: access masking must cover
   geometry and vehicles, not merely black overdraw of already collected data.
   Validate secondary bridge/tunnel geometry endpoints too. Native tile drawing
   and height queries may read adjacent shared terrain corners on the global map;
   preserve that native topology while preventing hidden-world geometry or sprites
   from contributing to the picture. This is visual masking, not independent maps.
5. Close or disable before rendering on invalid/deleted/replaced train, company
   switch, world/access change, new/load game, return to menu, network disconnect
   or external-server transfer. Integrate native destruction/replacement hooks
   so a recycled `VehicleID` cannot attach to another train. Portal tiles and
   tunnel/wormhole/bridge transit, including travel beneath bridges, are unsupported
   in Stage 1; reject before sampling,
   clear transient state, and require deliberate reopening on supported ground.
   Do not smooth through world gaps or follow across servers. The upstream
   greater-than-two-tile camera reset alone does not satisfy these conditions.
6. Add meaningful boundary tests, then give the parent exact changed files,
   compilation concerns and next command. Parent owns configuration/builds and
   resource monitoring; do not launch a second build. No commit, push, PR, merge,
   released binary or Stage 2 execution is included in this implementation handoff.

Expected code ownership: `cmake/Options.cmake`, root `CMakeLists.txt` if needed;
`src/CMakeLists.txt`;
`src/cab_view_3d.cpp`, `src/cab_view_3d.h`, `src/cab_view_gui.cpp`;
`src/widgets/cab_view_widget.h`; `src/viewport.cpp`, `src/viewport_func.h`;
`src/vehicle_gui.cpp`, `src/vehicle_gui.h`, `src/vehicle.cpp`;
`src/window_type.h`, necessary native window lifecycle declarations/hooks;
`src/widgets/vehicle_widget.h`; `src/lang/english.txt`;
`src/tests/test_cab_view.cpp`, `src/tests/CMakeLists.txt`.
A narrow shared cab declaration/policy header is allowed if required. Do not
modify simulation commands, portal routing, map storage, custom save chunks or
economy. Escalate an actual need for such a change to Astra before broadening.

### Acceptance and verification

| Proof | Required observation |
| --- | --- |
| Feature OFF | Default configuration omits cab compilation/UI; normal native build and existing regression paths remain valid. |
| Feature ON | Native engine and test binary compile with supported desktop output; entry opens one cab on an owned train. |
| Renderer isolation | A fixture with pre-existing nonempty draw/sort queues survives collection unchanged, including referenced parent storage, DPI, tile context, foundation/combine fields, transparency and invisibility; normal viewport drawing still works. |
| Access boundaries | Allowed tile/vehicle renders; inaccessible, other-world and void samples produce no collected geometry/sprites; access loss invalidates caches; ordinary no-world maps work. |
| Lifecycle | Sale/deletion/autoreplace, reused ID, depot/unsupported transit, world change, company switch, load/new/menu and disconnect close or disable without stale references. |
| Purity | Open, paint, resize, local camera changes and close leave command dispatch, deterministic RNG, vehicle/orders, treasury/cargo and custom state unchanged. |
| Native pixels | Real GUI draw/resize/close and ordinary viewport redraw do not crash or corrupt pixels. A null-video test is not graphical proof. |
| Delivery checks | Run matched cab fixtures and adjacent viewport/vehicle/gate regressions with recorded cases, OFF/ON native builds, both repo linters and `git diff --check`; report every blocked check with logs. The user authorized scoped automated fixtures; unrelated full save/network campaigns are not run. |

Use a fresh disposable native fixture or a copied compatible test save with an
owned train, simple straight/curved/sloped track and an ordinary station, plus a
separate multi-world boundary fixture. Preserve originals. This does not renew
historical economy campaign clocks or authorize a new progression campaign.
Use existing project harnesses/dependencies and retain failed attempts.
The parent chooses one build directory after measuring free space; there is no
permission to remove other worktrees, caches, running games or unique evidence.
If resource limits prevent build/GUI proof, retain the patch and mark that gate
blocked; never infer success from source inspection or old test totals.

Human mission: enable the feature, open the train's cab, observe a short local
journey, resize/look around, close/reopen, then approach an unsupported portal and
confirm safe loss of the cab before transit. Confirm the ordinary viewport still
looks correct. **Human acceptance remains Pending until an explicit result.**
Independent Astra review is required after Stage 1; Stage 2 and Stage 3 require
separate authorization and a new bounded active handoff. Replan on unsafe native
render side effects, undiscoverable lifecycle invalidation, platform failure,
unbounded memory/CPU usage or a need to change map/simulation/save contracts.

## Active execution handoff — A World Worth Connecting: Sprint 3 (11 October 2026)

Owner approved the third candidate slice "A World Worth Connecting" from `ANTIGRAVITY-PLAN-AND-PROMPTS.md` and sections 4, 5, and 7 of `OPENSPACETTD-DESIGN-STUDY.md` on branch `codex/capacity-or-reserve-pilot`:
- **Principle**: Purposeful inter-world expansion providing supply diversification, topological bottleneck independence, and demonstrable industrial output resilience.
- **Ruleset**: Opt-in experimental v2 rules for fresh disposable fixtures only. V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope & Implementation**:
  - **Existing Consumer**: Player-owned bootstrap Ballast Crusher at Node 4 (`TileXY(300, 80)`, World 1 / Industrial Commonwealth) consuming raw SILC to produce BALL.
  - **Original Source & Corridor**: Silica Dunes at Node 13 (`TileXY(650, 80)`, World 2 / Extraction Frontier) delivered via Train 1 across the World 2 ↔ World 1 portal throat on row 82 (`TileXY(534, 82)` ↔ `TileXY(476, 82)`). Track length: 294 tiles.
  - **Alternative Raw Resource Origin**: A new Silica Dunes extraction facility on World 3 (`Research frontier`, `WorldID{3}`, Phase 3 Frontier), discovered via paid company area surveying (`Commands::SurveyResources`), funded via native `Commands::BuildIndustry`.
  - **Independent Wormhole Link**: A direct portal pair constructed via `Commands::BuildPortalPair` directly between World 3 (`TileXY(790, 96)`) and World 1 (`TileXY(476, 96)`), connecting to a new platform at Ballast Crusher (Stop 4) on row 96. Track length: 248 tiles (~16% shorter route).
  - **Bottleneck Independence**: The World 3 connection completely bypasses World 2 and does **NOT** share the World 2 ↔ World 1 portal throat on row 82. When the primary corridor is deliberately closed, World 3 maintains deliveries without interruption.
- **Verification & Bounds**:
  - Measure route timing, transit distance, and round-trip cycles between World 2 and World 3 origins.
  - Deliver native before/after operating sample: baseline operation (periods 1–6), expansion connected (period 7), dual-source operation (periods 8–12), deliberate closure of original corridor (periods 13–18), reopening/recovery (periods 19–24).
  - Demonstrate sustained Ballast Crusher production during closure of the original approach.
  - Fresh-process cold reload at period 24 and 4 post-reload periods with 100% cargo conservation.
  - Real costs and treasury accounting: surveying, industry funding, portal pair construction, track laying, depot, and train purchase.
- **Delivery Boundary**: Local review-ready delivery on `codex/capacity-or-reserve-pilot`. No push, no PR creation, no merge, no Sprint 4 execution. Hand off to Codex/Astra for review.

## Active execution handoff — A District That Stays Built: Sprint 2 (11 October 2026)

Owner accepted Sprint 1 "Capacity or Reserve" pilot evidence and approved the residential slice "A District That Stays Built" from `ANTIGRAVITY-PLAN-AND-PROMPTS.md` on branch `codex/capacity-or-reserve-pilot`:
- **Principle**: Finite native residential works, material-funded capital additions, and durable development that stays built through supply interruption.
- **Ruleset**: Opt-in experimental v2 rules for fresh disposable fixtures only. V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope & Implementation**:
  - One non-repeatable pilot residential work for one pilot Core town (`TownID{0}`, Commonwealth Metropolis).
  - Material kit: `160 STEL + 80 BALL` and £50,000 cash debited atomically from company world stockpile via replicated command `Commands::CommissionTownResidentialDistrict`.
  - Grants a finite budget of 4 native building opportunities.
  - Native building opportunities locate legal road-adjacent building sites (`CanBuildHouseHere` and `RoadTypesAllowHouseHere`) and construct native house shells with `is_protected = true`.
  - House shells start at construction stage 0 and progress through native tile ticks to completion; native population is added by `AdvanceSingleHouseConstruction` upon reaching `TOWN_HOUSE_COMPLETED`. Zero phantom or clamped population.
  - Native protection bit (`IsHousePlayerProtected`) permanently shields completed buildings from OpenTTD's native random redevelopment demolition loop (`CanDeleteHouse`).
  - Blocked physical access (lack of valid road frontage or terrain obstruction) is clearly reported in UI/status ("Awaiting site access") without burning materials or fabricating houses.
  - Optional Commercial FOOD service is decoupled: neither unlocks residential construction nor revokes committed or completed development during service interruption.
- **Verification & Evidence**:
  - Status: **PASSED (Execution Completed)**
  - Automated Unit Tests: Catch2 suite `[commonwealth-works]` (202 assertions passed across 8 test cases); full suite 307,537 assertions across 538 cases passed.
  - CTest Suite: 553/553 tests passed (100%).
  - Repo Linters: `file-descriptions.py` OK, `unused-strings.py` OK, `git diff --check` clean.
  - Headless Simulation Artifacts (`docs/audit/2026-10-11/residential-district/`):
    - Binary SHA256: `5cc4d2a5fe897887941c807e9f7b32ac7a171e06c916f38f8994c48faf2d5365`
    - Cold Start Save SHA256: `cc2a94b4aa65132bf279ff6f4ac02bc1a79787302e3d2c9a4f33c353af684e20`
    - Checkpoint-24 Save SHA256: `8987cb9c9c746071cedc485d4023010c0189e44fe70d6881a7fe4408d329fb26`
    - Cold Reload Equality: Verified 100% identical state across tick, date, money, held, stocks, facilities, economy.
    - 4 post-reload periods passed with zero cargo conservation errors.
  - Mechanics Verified in Headless Run:
    - Atomically debited £50,000 cash and 160 STEL + 80 BALL materials.
    - Initial period 1: 4 opportunities, 0 placed, 0 completed.
    - Periods 2..5: 4 native house shells placed adjacent to road frontage; opportunities decremented 4 -> 0; shells started at stage 0 with population unaltered.
    - Period 8: All 4 houses reached native completion (`TOWN_HOUSE_COMPLETED`); completed reached 4; town population rose from 904 to 1136 (+232 native population).
    - Periods 14..18: Commercial food service completely shut off; completed houses remained strictly at 4 and were never demolished.
    - Period 18: Food service restored.
- **Delivery Boundary**: Local review-ready delivery on `codex/capacity-or-reserve-pilot`. No push, no PR creation, no merge, no Sprint 3 execution. Stopped at agreed boundary for review.


## Active execution handoff — Capacity or Reserve Prototype: Sprint 1 (11 October 2026)

Owner authorized the experimental "Capacity or Reserve" prototype from `ANTIGRAVITY-PLAN-AND-PROMPTS.md` on branch `codex/capacity-or-reserve-pilot`:
- **Principle**: Stable settlements, optional FOOD civic service, and a factory capacity-versus-reserve choice.
- **Ruleset**: Opt-in experimental v2 rules for fresh disposable fixtures only (`connected_economy prepare-works` / `--works-prototype`). V1 and legacy rules remain default; existing saves and normal new games are 100% preserved.
- **Scope**:
  - One mature Core town with Stable civic baseline (no supply-driven growth, no starvation lockout).
  - One optional FOOD service slot per town: rate Off / 10 / 20 / 40, receiver cap 60, market ceiling 40, four-period sliding useful consumption history yielding up to +25% passenger generation bonus.
  - One owned bootstrap ballast plant (`CompanyID{0}`) with one-time commissioning choice between Capacity Works (speed 100 -> 150 batches/period, storage 300) and Reserve Works (speed 100, storage 300 -> 600 batch-equiv), costing `160 STEL + 80 BALL` and £50,000 cash debit.
  - V1 generic +50 upgrade bypass blocked under v2.
  - Managed town cargo routing in `economy.cpp` separating managed destination status from active demand, preventing rate-Off FOOD and non-demanded commodities from generic absorption.
- **Budget & Bounds**: One fresh fixture generation, one bounded run up to 24 actual economy periods or 30 execution minutes, one fresh-process cold reload and up to 4 further periods. Stop at first integrity/command defect. Real transported physical cargo; no calculated growth claims.
- **Delivery Boundary**: Local review-ready delivery only. No push, no PR creation, no merge, no Sprint 2 execution. Hand off to Codex for review.

## Active execution handoff — Core Basket Repeat and Cold Proof (10 October 2026)

This replaces the expired campaign permissions with an owner-authorized finite continuation:
- **Objective**: Execute the Core Town Station Adapter delivered in PR #75 (`dddd49f18d`)
  to retarget Train 4 FOOD delivery to the nearer Core station (tiles 795,62–796,62 via connector 794,62).
  Prove second atomic monthly basket consumption (60 FOOD + 20 STEL + 20 BALL), observe native town growth hook
  and evaluate actual house/population response, and prove cold-reload persistence without state corruption.
- **Starting Point**: Copied paused checkpoint save at tick 304,384, SHA256
  `89e94d49dc95b5df0277679dcaa7c52f95b39c2bc0b90b840f5d4484e346663f`.
- **Bounds**: Exactly 1 simulation advance (up to next monthly tick boundary ~307,200).
  Immediate stop at first command or integrity failure.
- **Economic Mode**: Preserves offline `infinite_money` mode for functional validation;
  financial profitability and ordinary-start balancing remain deferred.

## Synchronization status — 10 October 2026

This file retains the historical First Core Construction Basket plan and consumed
attempts below. Its October 3 clocks and load permissions are closed. The owner's
current request is checkpoint synchronization; it supplies no new simulation
clock, load budget or source-save campaign. No historical clock is being reset.

Later October 5 evidence proves one full consuming basket, superseding older
unproved-outcome prose below for that specific result. See
[the checkpoint alignment](audit/2026-10-10/checkpoint-alignment/README.md).
The existing [station-adapter source handoff](handoffs/CORE_TOWN_STATION_ADAPTER.md)
remains the next bounded gameplay review. A subsequent repeat/cold campaign needs
an agreed replacement here with exact checkpoint, delivered helper and finite
load/tick/wall bounds. This status update does not create that next plan.

For the current First Core Construction Basket development proof, the owner
explicitly deferred financial balance and authorized unlimited virtual money.
The former £600,000 quote, £750,000 aggregate gross-spend and cash-reserve
ceilings below are superseded as acceptance gates. Preserve their historical
failed evidence and count all actual native receipts/debits separately from the
earlier £6,000,000 assistance. Enable the game's explicit `infinite_money`
setting only in this offline proof after loading the pinned save; record the
setting in evidence. It changes affordability, not physical cargo or receipts.
Do not change normal new-game or server defaults, inject cargo or research, or
claim ordinary-start economic proof.

The one bounded recovery starts from the pinned final source save and retains
the failed first attempt's one requested advance, 0 actual ticks, search and
spending history. The immutable outer stop is **2026-10-03 02:13:49.320471 UTC**;
the original initial-phase stop is 01:28:49.320471 UTC. Initial/cold advance
ceilings, search/topology constraints, first-hard-defect stop, resource
conservation, recipes, logistics and cold equality remain binding. No clock or
count reset, automatic second replay, merge or next slice is authorized.

That single recovery load stopped before planning or a new tick: the
money-setting response omitted the monthly audit field required by the runner.
The response shape has a narrow source correction, but that correction is not
authority for another game load. Preserve both stopped attempts. The requested
functional material-basket outcome is still unproven.

The subsequent explicit handoff authorizes **one** further source-save load
after focused full response-contract checks. It assigns the initial phase the
remaining time through the unchanged outer stop
**2026-10-03 02:13:49.320471 UTC**; this supersedes the earlier initial-phase
stop only. Retain the original requested advance and all earlier attempts.
No fresh grant, extra tick, silent replay after gameplay starts, or change to
the resource and authority rules follows from this timing allocation.

That one further replay ran two full advances and stopped at its first wrapper
integrity assertion. A neutral unbuilt candidate tile naturally changed from
clear to trees; native command, cash and all-cargo conservation remained sound.
The observation guard now accepts only that scoped vegetation change and still
rejects altered rail, signals, ownership or terrain. The captured observations
pass the repaired guard offline, but gameplay after the stop and cold proof are
**NOT RUN**. No additional replay follows from this repair.

The next explicit handoff authorizes one more replay from the pinned source save
with the unused-candidate vegetation guard tightened and proven against both
retained snapshots. Retain all **3 requested advances / 4,096 actual ticks**
and both earlier paid attempts in aggregate evidence. The initial phase now
shares the original 02:13:49.320471 UTC outer stop; the total advance cap is
unchanged. Save a normal paused checkpoint after construction and at each safe
advance boundary. On a wrapper assertion, attempt a diagnostic save only after
independent native cash/cargo and full route-state verification; never save an
unsafe state or treat a diagnostic file as continuation authority. Stop at the
first real gameplay defect or the original outer deadline.

That checkpointed replay stopped after multiple native advances at a
wrapper-only world equality check: Industrial world development increased after
a paid IRON delivery and normal steel conversion, while role, phase, geometry
and ownership remained fixed. The revised observer compares immutable world
fields and records development as a live resource outcome; the retained final
native snapshot passes the focused check offline. The last safe normal save is
the advance-014 checkpoint, one advance before the stopped result. No cold
continuation or further replay is authorized by this repair.

The latest explicit handoff authorizes one guarded continuation from the last
verified advance-014 paused checkpoint, **not** another source-save build.
Restore the transient adapter session only after full paused state, physical
cargo, paid infrastructure, research, services, immutable world identity and
provenance agree. Retain **15 cumulative requested advances / 28,672 executed
ticks** across attempts; the checkpoint predates the failed last advance, whose
command and result remain separate evidence. The unchanged 02:13:49.320471 UTC
outer stop and advance ceilings remain binding. Financial affordability is
deferred under the explicit offline development-money mode.

The authorized checkpoint load emitted its startup marker but could not open
the native FIFO: the older functional adapter rejected the checkpoint's saved
development-money setting before its bridge branch. No resumed-state comparison
or gameplay command ran. A narrow bridge-only guard correction is committed but
the failed load remains consumed; it is not automatic retry authority.

The one final bridge-corrected load reached paused equality and stopped before
arming: the persisted state matched, but eight wagon cached maximum-speed
observer values were zero after reload rather than their prior derived values.
All six front-engine speed values matched. The observer now reports the stable
front speed and zero for inert wagon cache fields; the original observation is
retained separately. This correction is unrun in game and supplies no further
load authority before the original deadline.

# Active execution plan: First Core Construction Basket

Owner-authorized execution handoff, 2 October 2026. This is the single active
implementation slice. Use the final already-funded checkpoint; new assistance is
£0. The prior campaign, grant, deadlines, failures and evidence remain closed and
unchanged. The owner approved the reviewed recommendation and delegated routine
bounded implementation choices. No further routine confirmation is required.

The latest timing instruction supersedes the proposal's first-launch clock:
the NEW 75-minute simulation clock starts at the FIRST native simulation advance,
after plan/source/preflight preparation. Record actual UTC start and immutable
start+75-minute stop immediately before that advance. Planning/compiler builds
and paused preflight preparation do not consume that clock. Never reset it.

## Live delivery and what is established

Read-only GitHub/ref checks in this reassessment show:

- Main: `85c423b4a2dd57e5a2c2cf1f6d29bc2e4667845d`.
- PR66 is open/draft, targeting main, exact head
  `7bc29a60cc1bc60b4d555d7500c2ded79c175eaa`; all seven exact-head workflows passed.
- That dependency contains main/PR67, original Money compiler fix `d1bb6176`,
  reviewed PR69 `3630d490` and preserved owner merge `eead18f1`. Main is an ancestor.
- The isolated implementation branch is `codex/first-core-construction-basket`,
  based on that refreshed PR66 head and targeting `codex/generation-money-comparison`
  while it remains unmerged. No merge or public-history rewrite is authorized.
- Preserve PR68, original worktrees, ordinary-money failures and every old archive.

| Acceptance dimension | Evidence and limit |
| --- | --- |
| Existing systems | Generation, commands, native processing/demand, research and save handling exist. PR69 repairs replicated designation, query mutation, startup/snapshot support and integrated post-load growth caches. Its exact-head CI and independent review passed. |
| Assisted functional outcome | PASS across preserved authorized handoffs: paid grain/FOOD services, three initial consuming months, interruption/recovery, normal paid HQ/I, II eligibility, exact cold retention and further paid FOOD plus consumption. The last consuming month used carried stock before the new arrival. |
| Ordinary economics | Unproven for this company. The original revised A1 loss remains failed evidence. The HQ/research were cash-assisted; FOOD did not unlock research or prove it funded the HQ. |
| Fresh repeatability | NOT RUN. Cold equality and source/unit checks do not substitute for an independent fresh replay. |
| Human acceptance / balance | Human UAT Pending. Pacing/profitability and uninterrupted supply are deferred. This proposal neither waits for profitable operations nor labels them passed. |

Source of the retained outcome: [continuation manifest](https://github.com/flaxos/openspacettd/blob/3630d490304916eb16c812b69e0aed92bfdd0f96/docs/audit/2026-10-01/functional-core/continuation-manifest.json)
and [raw archive](https://github.com/flaxos/openspacettd/blob/3630d490304916eb16c812b69e0aed92bfdd0f96/docs/audit/2026-10-01/functional-core/continuation-proof.tar.gz).

## Dependency decision

Recommend one smaller extension through existing material systems: **deliver and
consume the Core construction basket, with retained growth state after reload**.
This follows the roadmap's A3 functional-continuity direction before earned A2.

Materials II is an eligible, cash-only 250 RP project (£250,000 at current rules)
and would expose copper/silica and electronics. It is not a dependency for steel
or ballast. The retained world has no copper/silica producer and no wiring,
chip, signal or machinery factory. Making II into a physical freight opportunity
would add surveying, resource extraction, factory funding and destination work
whose legal sites and prices are not established here. Research-only completion
would prove another paid unlock but leave physical material use untested.

Steel-only delivery would stop at inventory. The city consumes FOOD first, then
STEL and BALL atomically; steel alone cannot provide expansion. A further FOOD-only
run would add continuity evidence but is not a prerequisite for this material
extension. Do not reopen the completed FOOD finish line or require profitable
FOOD before continuing functional work.

The proposed material chain uses neutral generated factories. Their production
does not enforce a company research gate (`Produce` checks that gate only for a
valid company owner). Consequently this proves physical use of existing steel
and ballast systems, not that Materials I newly enabled these neutral factories.
Company-funded factories and Materials II remain separate future opportunities.

## Starting state and changed assumptions

Use a new working copy of `campaign/owner-observation.sav` from the archive:

- Save SHA256: `206acdd9485bed64927197af89e07a70926e0b1e386ed51cb93744df2c78e2f7`.
- Tick 167168; cash £3,705,864; debt £100,000; Materials I (301) retained;
  active project/RP/budget zero. Existing grain and FOOD trains/orders retained.
- Core town 0, population 1,191 in the snapshot, FOOD 120, STEL 0/BALL 0, growth 0,
  passenger multiplier 0.5. Current demand is FOOD 60/STEL 20/BALL 20; recompute
  demand from live population at each observed month, never hard-code the result.
- Existing industries: iron source 6 at (600,335), world 2; steel processor 7 at
  (597,222), world 1; stone source 2 at (816,518), world 3; ballast processor 4 at
  (819,237), world 1. Processors have zero completed material batches in the
  snapshot. Public gates 2 (world 2↔1),3 (world 3↔1),1 (world 1↔0) exist.
- Station 3 catches town 0's own house at (794,66), has no warehouse and is the
  existing Core receiver. Its actual acceptance/capacity and any shared junction
  must be revalidated before paying for a material route. Existence is not a
  legal route quote or capacity proof.
- Keep canonical profile, exact published GRFs, optional art disabled, existing
  world roles/development and ownership. No save conversion or new generation.

The available £3.7m is inherited assisted cash. Old unused ticks and the numerical
£1,233,228 gross-spend headroom are historical bookkeeping, not current authority.
Retain the old counters: 82 initial advances (including25 control advances) and
24 cold advances. Report new advances separately and as an aggregate
with those106; no historical counter is reset or relabelled.
No cash, cargo, technology, RP, population or route may be authored. No new loan.

## Authorized implementation scope

One town, the four named industries and at most four new cargo services:
IRON6→steel7; STEL7→Core0; SILC2→ballast4; BALL4→Core0. Use the existing public
backbone and legal private branches/signals. Retain the existing FOOD service;
normal orders/refits/start-stop are permitted, with no added FOOD fleet or
automatic strategy/fleet tuning. No new factory, warehouse, survey, resource
funding, gate, research project, colony or material policy.

Current proof builders cannot simply be rerun: `functional-plan/build` is fixed
to fresh GRAI/FOOD construction; `first-freight-build` also rejects an existing
fleet and its funding helper may borrow. Implement only a separately guarded
loaded-checkpoint paid-command adapter, reusing narrow planner/quote primitives.
Verify the source save hash and complete frozen starting state before arming it.
Keep the original pristine/grant/ordinary-start guards and command flags intact;
the adapter must have no grant, loan or research operation. Native previews and
commands still decide every construction, ownership and payment result.

Explicitly handle existing owned track, shared/private junction footprints and
duplicate quotes without replacing neutral infrastructure or hiding failed
commands. Freeze one complete affordable layout/consist/order plan before spending.
If legal reuse cannot be established within the binding caps, stop with the
failed quote rather than expand into network/pathfinder redesign.

The existing `PlanFreightLeg` predecessor map resets for each station candidate.
The new adapter must carry one shared 30,000-state counter across all candidates
within an endpoint-planner invocation, stop before exceeding it, and count failed
searches too. Reusing the old per-candidate counter unchanged would violate this
plan. Keep the old helper's caller defaults intact; test the new budget's
exhaustion behavior without changing simulation rules.

Add only necessary read-only evidence for SILC/BALL labels, all-cargo custody,
per-month town consumption and native growth eligibility. Existing monthly audit
events name FOOD only; record the full basket at the actual monthly evaluation
so coarse tick sampling cannot invent simultaneous consumption. Record the town's
native growth rate/state as well as the integrated multiplier. Preserve native
processing, demand, prices, capacities, RNG, dates, save schema and permissions.

## Required finish line and verification

1. Cold-open the exact observation copy, paused, and compare retained state before
   any mutation. Pin source/binary/content/config/save hashes; record zero new
   assistance and unchanged debt. Added observation fields must not normalize away
   discrepancies in the frozen fields.
2. Query and build the bounded plan through normal paid commands. Prove real IRON
   and SILC arrivals, exact 2IRON→1STEL and 2SILC→2BALL processing, and positive
   native STEL/BALL delivery/payment to the actual town receiver. Separate industry,
   train/station, city and company-stock custody; no warehouse diversion.
3. In the same instance, hold BALL delivery with normal vehicle controls until one
   month has sufficient FOOD and STEL but insufficient BALL. Require FOOD use,
   unchanged STEL reserve, no BALL consumption and zero expansion. If FOOD is also
   missing, that month does not count as the isolated missing-BALL control.
4. Release BALL. Observe one month consuming the complete live FOOD+STEL+BALL basket
   and enabling normal native growth permission: growth/passenger multipliers
   exactly 1/1, with the normal native growth-rate hook enabled. Record native house
   and population changes, but do not turn a short observation into a sustained
   population-growth or pacing claim. Save immediately while the proven state is
   visible; research remains off and Materials II unstarted.
5. Cold-load in one fresh process, require full paused equality, then observe
   another positive STEL and BALL delivery/payment before a second distinct
   complete basket is consumed. Initial STEL/BALL are zero, so both baskets must
   reconcile to material produced and transported in this extension. End with a
   hash-verified paused save and terrain/cargo-text audit.

Reconcile every native receipt and debit, including existing services, interest,
fees and tolls. Keep original assistance £6m and new assistance £0 separately:
`ending cash = £3,705,864 + new native receipts − new native debits`; debt remains
£100,000. Continue whole-history figures from primary debits £2,761,963 and
disposable-control debits £4,809. Track all physical production, conversion,
holdings, consumption and explicit discard; reserve increases alone do not pass.
STEL/BALL city storage is only one live basket each (currently 20 each). Retain and
account for excess aboard trains or in ordinary buffers; never force-clear it to
make an order or payment appear complete.

Meaningful focused checks cover resumed-adapter admission/denial, shared owned
track/native quote parity, all-cargo observer non-mutation and complete-basket
atomicity/cold retention. Reuse existing economy/save tests and add only uncovered
cases. After final source changes run repository full gates and exact-head CI,
then independent source/evidence review. A fresh campaign replay stays NOT RUN;
do not represent a resumed checkpoint as pristine-start repeatability or human UAT.

## Binding new bounds

| Item | Authorized ceiling |
| --- | --- |
| Campaign | One copied existing checkpoint, one initial load and one planned cold load; no fresh game, funded replay, seed change or automatic retry. |
| Topology | Fixed four industries/one Core town; at most four new trains with up to three wagons each; one material-basket outcome. |
| Search | At most two complete layout candidates in stable order, each with at most eight endpoint-planner invocations. Each invocation has at most 16 station candidates, 30,000 total predecessor states and 16-tile bridge span. Count all search attempts; no hidden cap reset. |
| Spending | At most £600,000 quoted construction/vehicles/refits; at most £750,000 added gross debits overall, including elapsed charges from all six services and all native diagnostic/control spending. New assistance £0; debt fixed. Historical plus new gross ≤£3,516,772. |
| Time | Paused native preflight/paid construction has its own 30-minute wall cap; planning and compiler-build preparation precede it. The new 75-minute simulation campaign starts exactly once immediately before the FIRST native advance, after preflight preparation. Persist actual UTC start and immutable start+75-minute stop. Initial operation has a 30-minute wall cap; cold continuation has a 15-minute wall cap; both also obey the new outer stop. Include all stopped/diagnostic time after first advance; never reset or extend the clock. Startup failure consumes the one initial load and requires a new handoff even if the clock never started. Old deadlines remain closed. |
| Ticks | Initial operation at most 240×2048; cold at most 120×2048; total 360 advances/737,280 requested native ticks. All controls and diagnosis share those caps; unused phase ticks do not transfer. Paused build has no free background ticking. |
| Stops | First integrity/command/accounting/save defect, absent receiver, wrong payment/custody, crash/lost train, unaffordable quote or exhausted search/spend/time/tick bound stops with partial evidence. No rerun after a repair without another explicit handoff. |

These approved limits authorize this single slice. A legal but slow supply
pattern is measured as pacing; it does not authorize tuning, time extension or
inventory injection. If these bounds cannot support the four services, return
the concrete blocker and a smaller follow-up proposal, without silently dropping
ballast/consumption or claiming the full outcome.

## Short owner mission after delivery

Allow 15 minutes for feedback, with completion time unpromised:

1. Load a copy of the delivered paused save on the identified build/content;
   inspect assistance provenance, cash/debt, research off and the Core basket.
2. Follow one iron/steel and one stone/ballast consignment to the Core station;
   inspect actual payments and distinct reserves.
3. Observe missing-BALL feedback/retained steel, then a complete consumed basket
   and enabled growth state. Report confusing controls or long waits.
4. Save under a new name, quit/reload and check cash, stock, orders, growth state
   and the retained unlock. Report Pass/Fail/Partial and the first issue.

Observation of a supplied network is separate from manual construction acceptance.

## Approved handoff and independent review

The owner explicitly approved this one existing-material basket outcome, a new
ledger from the final verified checkpoint, zero additional assistance, the new
spending/search/tick limits and the corrected first-advance clock. The original
proposal received independent Astra maximum CLEAR; implementation and resulting
native evidence receive a new independent Astra maximum review. No merge, next
slice, balance-policy change, research/cargo grant, extra loan or campaign retry
is authorized. Stop at the first hard defect/exhausted bound; preserve partial
logs and return a concrete blocker plus a bounded next action.

Before implementation verify the copied source save and frozen state, read the
source/AGENTS/ost-plan/dev/uat/deliver, and preserve complete old evidence. Update
functional, economic and human statuses separately. An inherited funded checkpoint
is ASSISTED FUNCTIONAL; ordinary-start economics/fresh replay remain unproven and
human UAT Pending until the owner reports an identified playtest.

## Source anchors

- Roadmap: `docs/PROJECT_STATUS_AND_ROADMAP.md:68–135` (functional-first/A3 ordering).
- Research: `src/portal/tech_tree.cpp:127–165`; `integrated_economy.cpp:452–459`
  (I/II requirements and empty tier II kit).
- Recipes/neutral ownership: `src/portal/integrated_economy.cpp:272–278,332–378`.
- City demand/capacity/atomic basket: `src/portal/integrated_economy.cpp:379–448`;
  native hook `src/town_cmd.cpp:4179–4201`.
- Builder constraints: `src/portal/connected_economy.cpp:1529,1835–1842,1889`;
  scoped planner `1254–1364`; observer `1649–1664` and FOOD-only monthly event
  `src/portal/integrated_economy.cpp:437`.
- Existing focused coverage: `src/tests/test_integrated_economy.cpp:90–145,520–545`
  and integrated Core native round-trip cases in `src/tests/test_megacity_gui.cpp:175`.
- Frozen `final_state` in the continuation archive, inspected without loading a game,
  provides the industry/world/gate/state observations above.
