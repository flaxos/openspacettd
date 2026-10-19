# Cab view Stage 3C: existing-content qualification and timing diagnosis

The bounded diagnostic is complete. It does **not** qualify real multilayer or
animated-palette vehicles, and the retained timing failure remains unresolved.
No production drawing, texture caching, painter, simulation or save behavior changed.
The added code is opt-in private-renderer and native-fixture instrumentation.

Work remains uncommitted, unpublished and default OFF on `codex/cab-view-stage1`,
base `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`. The prior Stage 3B patch, audit,
source and binaries are preserved. Both owner checkouts remain at their prior clean
heads; owner saves and running applications were untouched.

## Existing assets and the blocked checks

[Inventory and hashes](asset-inventory.json) records the installed OpenGFX files
and checked-in project GRFs/NML. The project's vehicle definitions are property-only;
no suitable installed real SpriteStack asset was identified. Diagnostic two/eight-layer
sequences from Stage 3B remain compositor tests, not real NewGRF acceptance.

At the naturally reached cold return safe endpoint, the native probe queried all
four visible cars in eight OnMap directions, without changing vehicle poses. All
32 real `GetImage` sequences had one layer. The complete scan found **zero raw or
effective animated-index contributions** in slots 227–254. It accounted for native
palette precedence, alpha, remap-zero transparency, all-black handling and later
layer coverage. It kept actual train heading, queried direction and the renderer's
south-west sprite direction separate.

This negative result covers this consist's engines, current livery/cargo state and
32 queries. Other installed engines or states were not exhaustively scanned.
A suitable existing vehicle/GRF state is needed before the dependent two-epoch
animated-vehicle comparison can run. No download, new art, GRF build or scenery
substitute was used. Palette-cache animation fidelity remains unqualified.

## Timing evidence

[Measured summary](diagnostic-summary.json) and
[retained-frame analysis](retained-timing-analysis.json) preserve the data.
Exactly one diagnostic ran: cold `back_arrival_mixed`, 640 x 372, full scale,
canonical private palette, Arrival elapsed 0; two persistent private renderers,
20 warmups each, then eight 25-frame blocks in this declared order:
Off, Reduced, Reduced, Off, Reduced, Off, Off, Reduced.

| Measured mode | Frames | Median | p95 | Maximum |
|---|---:|---:|---:|---:|
| Off | 100 | 4,177 us | 5,650 us | 6,582 us |
| Reduced | 100 | 4,224.5 us | 6,321 us | 7,046 us |
| Reduced minus Off | | 47.5 us | 671 us | |

The p95 difference is descriptive, **not a replacement acceptance result**.
The original failed Stage 3B matrix remains failed: median-of-three added p95
1,114 us against the unchanged 500-us limit. Its repeat differences were
1,569 / 866 / 1,114 us; the later Stage 3B matrix recorded -313 / -125 / -1,063 us.

The new run's eight block-end images are byte-identical to each other and to the
retained failed scene's Off image. Full historical FrozenState, camera, world,
palette and cache snapshots also match. All 200 frames rendered fresh at full
scale, with 6,256 triangles, 2,693 tiles and eight raster stripes. The scene cache
remained 91 entries and 1,824,532 bytes of estimated capacity payload; it contained
no vehicle textures. The highest frame, 7,046 us, comprised 2,368 us preparation,
4,059 us rasterization, 617 us output and 2 us overhead, with measured effect time 0.

The prior failure's highest frame was 9,697 us, spanning preparation and
rasterization, with zero measured effect time. The new result retains the tail
concern under the declared alternating order; it does not isolate the cause.
CPU and context-switch deltas are available per block, but include image comparisons,
copies and cache inspection outside frame timing. They are not render-only CPU
utilization or evidence of a particular frame's cause. There is no CPU-frequency,
scheduler, allocator or per-worker trace. No thread policy, priority, affinity or
acceptance threshold was changed, and no extra runtime repeats were added.

## Verification and retained evidence

- Feature OFF and ON engine/test builds pass. Final ON hashes exactly match the
  measured binaries; OFF hashes match Stage 3B. Default remains OFF.
- 37 focused regressions pass. Default Catch passes 545 cases / 307,645 assertions.
- Full CTest passes 563/564 initially. The sole failure was a sandbox-denied private
  loopback socket; that one test passed with local-socket permission. All 564 cases
  have passing evidence across those runs.
- Native round trip and cold reload pass, with 2,517 explicit simulation calls,
  unchanged source save, 77 recorded screenshot hashes and two exact normal-viewport
  comparison pairs. The diagnostic restored palette, screen/DPI and animation state.
- The controller accepts the real report and rejects 12 deliberately malformed
  copies, including missing samples/images, altered state/cache, incomplete scan
  claims and false palette histograms. Original evidence was not modified.
- Astra independently recomputed quantiles, raw correlated tails and counter deltas;
  checked source/binary/content/image hashes and historical identity; and cleared
  diagnostic completion with the limits stated here. See
  [independent review](independent-review.json) and [verification](verification.json).
- Initial diagnostic compilation failed on an unavailable header constant and the
  repository's forbidden `memcmp` macro. Source/logs are retained; the corrected
  instrumentation uses a zero palette default and existing Colour equality.
  No failed native attempt or timing run was discarded.

[Evidence archive](verification-evidence.tar.gz), [archive manifest](archive-manifest.json)
and [read-back verification](archive-verification.json) retain raw reports, images,
source snapshots, build/test logs and reproduction helpers. The source delta is
against the preserved Stage 3B working state, not directly against Git HEAD.

Reproduce with the existing installed graphics and a cab-enabled build:

```sh
python3 scripts/test_cab_view_gate_native.py \
  --save build/cab-gate-native/fixture-01/cab-gate-depot-fixture.sav \
  --output build/cab-stage3c-candidate/new-diagnostic \
  --stage3c-diagnostic
```

This runner uses disposable SDL offscreen processes and private profiles. It never
launches the owner's game or edits its source save. Human comfort, real SpriteStack
content, observed animated vehicle colours and other platforms remain Pending.
