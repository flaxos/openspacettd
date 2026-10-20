# Experimental native train cab — 14 October 2026

This packet records provenance and actual evidence for the authorized Stage 1
cab port. The [active plan](../../../ACTIVE_EXECUTION_PLAN.md#active-execution-slice--experimental-train-cab-stage-1-14-october-2026)
is the implementation authority; the [canonical roadmap](../../../PROJECT_STATUS_AND_ROADMAP.md#cab-view-integration-sequence--14-october-2026)
holds all three stages. This packet is not a competing execution plan.

**Current local outcome:** Stage 1 code and automated native SDL offscreen proof
are independently reviewed. OFF/ON build evidence, 8 focused Catch cases, all 7
selected CTest entries, and the 14-assertion native proof pass. The final nine
screenshots show correct controls, resize, movement and cleanup. Human acceptance
remains Pending. The [native continuation below](#native-desktop-continuation)
supersedes earlier status snapshots without erasing failed attempts.

## Baseline and custody

- Target: isolated checkout `openspacettd-cabview`, branch `codex/cab-view-stage1`,
  initial clean HEAD `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce` (live main).
- Source: <https://github.com/vaclavloubek/OpenTTD/tree/15.3-cabview-alpha1>, pinned
  commit `b7bd17b04dde3379b2be33be40ba4d6fc9f5d391`; based on native OpenTTD 15.3.
  Parent investigation counted 61 commits / 54 changed files from 15.3, with
  4,898 renderer and 1,048 GUI lines. These describe the fork, not the final port.
- Port source only. Preserve copyright/GPL notices; no upstream release binary
  was selected as an OpenSpaceTTD runtime or save compatibility proof.
- Parent verified PR #83 open at `6355873d4bba5c84dfdf29f26d78fccfdf9965a6`;
  PR #85 merged into that branch, not main. This is a dated preflight observation.
- Primary `/home/flax/games/openspacettd` is Antigravity-owned and inspect-only;
  prior `ost-first-permanent-line` remains inspect-only. Do not conflate their
  newer experimental branch records with main's merged feature set.
- Root owns build configuration/resource monitoring; Sol owns the bounded code
  port; Astra owns canonical planning documentation and independent review.

## Source findings behind the safety contract

Upstream `CollectTileGroundSprites` asserts empty shared draw lists, overwrites
`_vd.dpi` and foundation/combine state, invokes native tile drawing, and clears
several lists. It restores DPI pointer, tile context and transparency settings,
but does not preserve the complete shared `ViewportDrawer`. Existing pending
sort pointers and child/foundation relationships require whole-state isolation.

The current tree uses scoped `FoundationPart`, `SpriteCombineMode`, `TileType`
and vehicle/direction enums; a blind 15.3 source transplant will not compile.
`PlanetManager::GetTileWorld` and `GetRegionByTile` are the current world queries.
`StellarNetwork::WorldAccessible` returns true for worlds without stellar metadata,
so it must be combined with selected-world/registered-void constraints.
The normal viewport masks closed worlds at draw dispatch; the cab renderer also
collects geometry and iterates vehicles independently, so it needs both masks.

Upstream camera code resets for a large positional jump, but native tunnel tests
also match portal tiles. Stage 1 must reject unsupported transitions before
sampling. Upstream raster/output jobs join their worker threads; native map,
vehicle, sprite-cache and GUI collection must stay on the main thread.
Vehicle images use sprite sequences; custom gate layers and full composite train
rendering require later native comparison proof, not a claim based on first-sprite
support. These limitations do not justify changing simulation or persistent data.

## Evidence status at planning handoff

| Item | Status |
| --- | --- |
| Isolated checkout and initial Git state | Confirmed clean on the recorded branch/base. |
| Repository guidance and canonical scope | Read; existing historical records preserved. |
| Three-stage roadmap / Stage 1 handoff | Written in existing canonical documents. |
| Stage 1 implementation | Delegated; no pass inferred here. |
| OFF / ON compilation | Pending parent execution. |
| Scoped cab / adjacent regressions / linters | Pending final code and parent execution. |
| Unrelated full suites / save and network campaigns | Not run under the scoped fixture authorization. |
| Native graphical smoke | Pending supported driver and actual run. |
| Human journey/comfort/appearance | Pending; automation cannot mark accepted. |
| Independent implementation review | Pending completed patch and evidence. |
| Stage 2 / Stage 3 | Planned only, not authorized. |

Cross-platform compiler/runtime checks remain unexecuted unless separately recorded.
A Linux native smoke does not establish Windows/macOS behavior.

Append actual commands, configuration, matched test counts, log paths, failures,
source/binary/content hashes and outcome below as work completes. Keep failed
attempts and distinguish automated render smoke from human gameplay acceptance.
Build storage was approximately 514 MiB free after checkout; parent found RAM
build storage unavailable and will monitor one build directory. A resource block
is an explicit incomplete gate, not a passing check. No original save or running
primary session is required by this plan.

## Preliminary independent review

The first viewport adapter review found the move-based whole-drawer restoration
sound with standard vector allocators: original parent storage and sort pointers
survive, and the DPI pointer is restored before the drawer's contents. A focused
nonempty-queue/alias regression is still required; this is source review only.

A renderer review identified a separate gate hazard: looking along an entrance
can call `AddTunnelPortal` -> `AddTunnelInside` before the followed train enters
it. Native `GetOtherTunnelEnd` returns `INVALID_TILE` for unlinked gates and edge
conduits, or a different world's tile for linked portals. Unchecked distance and
interior reconstruction must not run for these states. Camera closure alone
cannot prevent this path. Bridge-middle rendering also queries its distant head,
so secondary geometry references need access/world validation. These findings
were sent to implementation; their resolution must be verified on the final
patch rather than presumed here.

## Initial execution record — before native desktop proof

The final patch remains uncommitted in the isolated checkout. No push, PR, merge,
release, dependency installation, owner-save operation or running-game interaction
was performed. The primary and prior isolated repositories remained clean at
their original heads. GitHub was rechecked: main remains `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`,
PR #83 is open at `6355873d4bba5c84dfdf29f26d78fccfdf9965a6`, and #85 is merged into
that branch. No cab work depends on or integrates either PR.

The port adds a default-OFF, native-only `WITH_CAB_VIEW`, one owned-primary-train
window and local look controls. Collection uses a separate complete drawer and
restores the previous one, including pending referenced storage. Candidate tiles
and vehicle tile/position are filtered to the current accessible world.
Unsupported tunnel/bridge/gate cells and under-bridge cells are skipped before
geometry reconstruction. Invalid train/company/world/session closes the window;
native replacement/deletion hooks prevent ID reuse. Window closure and native
graphics reload invalidate cab caches. Sprite and deterministic detail-table
preparation happen before worker rasterization, and workers join before return.
No simulation commands, save chunks, map storage or network protocol were added.

| Final check | Actual result and evidence |
| --- | --- |
| Linux Release with assertions, OFF | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPTION_USE_ASSERTS=ON -DWITH_CAB_VIEW=OFF`, then `ninja -C build -j2 openttd openttd_test`: exit 0. Final compile database has zero cab sources and zero cab definitions. `native-off-configure-final.log`, `native-off-build-final.log`. |
| Linux Release with assertions, ON | Same commands with `-DWITH_CAB_VIEW=ON`: exit 0, both targets linked. Exactly 9 intended translation units receive the macro. `native-on-configure-final.log`, `native-on-build-final.log`. |
| OFF adjacent fixtures | `./build/openttd_test '[gate-sprites],[yapf-regression]'`: 6 cases / 10,323 assertions, all passed. `native-off-adjacent.log`. |
| ON scoped Catch | `./build/openttd_test '[cab-view]~[.],[gate-sprites],[yapf-regression]'`: 8 cases / 10,334 assertions, all passed. `native-on-catch-repaired.log`. |
| Final ON isolated CTest | `ctest --test-dir build --output-on-failure -R '^(Cab[ _]|Tooltips_reject)' -V`: 7/7 passed, 110 assertions total; six cab fixtures contribute 50 and the existing tooltip fixture 60. `native-on-ctest-final.log`. |
| Repository linters | `python3 .github/file-descriptions.py`, `python3 .github/unused-strings.py`: both exit 0 / OK. `git diff --check`: exit 0. `final-linters.json`. |
| Independent Astra review | CLEAR after final production changes, CMake scope repair and fixture initialization repair. Verified drawer restoration, masks, unsupported geometry exclusion, identity/cache lifecycle, per-source macro/enum consistency and immutable worker inputs. |

The six cab fixtures establish: tile access policy; real collection while keeping
pre-existing viewport queues and references; synthetic sky output at two 32bpp
sizes plus 8bpp output; entry policy; real window-object singleton/session/deletion
and explicitly reused ID handling; and CPU terrain pixels loaded from an installed
real baseset, unchanged when foreign-world terrain and a train are added. The
pixel fixtures check deterministic RNG/date stability. GUI fixtures use a test
video driver: no actual desktop surface or human visual acceptance is inferred.

### Failed attempts retained

- First syntax checks exposed 15.3-to-current enum/array/API differences, a private
  driver-factory access in the test and a missing language declaration. Those were
  repaired. The first GUI syntax check also preceded regenerated string headers.
- First ON build failed because child-directory CMake source properties did not
  reach targets created at project root. Root-scope absolute source properties
  fixed the generated commands without raising the CMake 3.17 minimum or enabling
  the feature globally. Final OFF binaries match their earlier OFF hashes exactly.
- First scoped Catch/CTest run failed on a fixture's assumed void edge and missing
  vehicle-pool allocation prechecks. The scene also lacked native engine mappings.
  Assertions remained enabled; fixtures were repaired rather than production guards.
- The real scene then segfaulted during incomplete native graphics initialization.
  Adding the existing language/cargo/house/road/engine startup sequence resolved it.
  No debugger was installed, so the exact faulting instruction was not captured.
- Source inspection caught empty mock sprites lacking raw backing files before
  relying on the scene test; the fixture now reads the existing installed baseset.
- Full native builds emitted GCC 13 warnings in unchanged NewGRF byte-reader code;
  the full logs retain them. Cab compilation produced no new diagnostic in the
  successful ON build. Optional Doxygen/Breakpad/Allegro discovery was unavailable;
  configuration and native linking still succeeded with existing dependencies.
- The final diff check including newly added files found two extra EOF blank
  lines in imported files. They were removed, the check passed, and the affected
  native targets were rebuilt; the failed formatting check is retained.

Raw logs, including failures and final manifests, are retained in
`build/agent-logs/` and bundled in `verification-logs.tar.gz` beside this document.
`verification.json` pins code, binaries and generated English strings. No save was
used. The scene discovered installed base graphics through normal read-only paths;
its exact chosen content files were not logged, so a reproducible graphical mission
must separately pin baseset/NewGRF identities and hashes.

### Remaining acceptance and next action

Native desktop full-window paint/resize, ordinary 2D redraw, an actual moving-train
journey and real train artwork remain **NOT RUN**. The fixtures do not exhaustively
snapshot commands/orders/treasury/cargo, exercise every replacement/disconnect/load
path, or test a live graphics reload after populated caches. Broader purity and
transition claims remain source-reviewed rather than complete native acceptance.
Windows/macOS, dedicated/Emscripten builds, performance/comfort and human UAT are
unexecuted. Unrelated aggregate save/network/economy suites were not run under this
scoped fixture authorization. Stage 2 and Stage 3 have not started.

Next bounded handoff:

> Continue Stage 1 on this isolated branch using the active plan and this packet.
> Pin the current source, binary, baseset/NewGRFs and display/blitter. Prepare one
> fresh disposable owned-train desktop fixture with simple track and one visible
> world boundary. Prove cab open/look/resize/close/reopen and ordinary viewport
> redraw, then safe closure before unsupported gate transit. Capture actual pixels
> and unchanged simulation state around camera-only actions. Preserve owner saves,
> existing worktrees and the running game. Repair only demonstrated Stage 1 defects
> with focused regressions and independent review; do not expand gate travel,
> federation, graphics scope or persistence. Record human acceptance separately.
> No push, PR, merge, release or Stage 2/3 execution is included.

## Native desktop continuation

The owner authorized completing Stage 1 native proof before expansion. The
already-installed SDL2 `offscreen` driver created a private native window and
surface; no DISPLAY/Wayland connection, installation or owner-game interaction
was needed. The hidden Catch controller calls the real `openttd_main` with SDL,
32bpp animation, null sound/music, a private config/profile and a copied save.
The normal screenshot path draws the actual windows into 1280×800 PNGs. This is
automated native graphical evidence, not human UAT or a visible desktop playtest.

Final source/binary/content/save hashes and commands are in
[native-desktop-verification.json](native-desktop-verification.json).
[native-desktop-proof.tar.gz](native-desktop-proof.tar.gz) retains the fixture,
all three graphical attempts, screenshots, manifests, raw build/test logs and
independent review. The earlier verification packet is preserved unchanged.
Representative final screenshots: [resized cab](native-resized.png),
[moving train](native-moving.png).

| Final evidence | Result |
| --- | --- |
| Native proof `desktop-03` | PASS, 14 assertions, 9 PNGs, 30 callbacks, about 1.016 seconds total process time. |
| Real Cab entry and local controls | Opened from the train vehicle window; ahead/left/right scene orientation independently checked. |
| Resize | 480×300 → 640×400, both native dimensions checked. |
| Movement | Train 0 moved (332,648) → (396,648), four tiles, after a separately recorded native SkipToOrder fixture command and 128 explicit StateGameLoop calls. Ordinary timer ticks can also run. |
| Cleanup | Close/reopen and transient local-owner loss close the cab; final native process exits cleanly. |
| Adjacent normal viewport | Every pixel below toolbar y=22 matches before/after closure. The only full-image difference is the 202-pixel pause-button state. Owner-loss closure equals the closed frame exactly. |
| Masks | Real loaded WP11 foreign tile rejected; separate real-baseset CPU scene confirms adding foreign-world terrain/train changes no cab pixels. |
| Camera-only purity | Paused RNG/date/cash/position/order-count/front-engine-cargo snapshots unchanged. Full order/consist/custom-state serialization was not compared. |
| Focused final checks | 8 Catch cases / 10,334 assertions; 7 CTest entries / 110 assertions; repository file-description/string linters and diff check pass. |
| Default-OFF guard | Fresh OFF compile database has zero cab sources/macros; restored ON has four cab sources and ten intended macro-bearing translation units. Earlier full OFF build remains retained; final ON targets rebuilt. |
| Independent review | Astra source and final image/evidence review CLEAR for Stage 1 automated acceptance, after finding the control-direction defect below. |

Capture queue-to-file timings were 30–51 ms, including rendering and PNG IO.
They are not a frame-rate, input-latency, memory-stability or comfort benchmark.
The fixture is a short straight station departure, not slope/curve, portal,
bridge, network-transfer or long-session graphical qualification.

### Retained native proof failures and corrections

1. Fixture attempt 01 hit sandbox `PermissionError` creating a loopback socket
   before any game launched. A bounded approved escalation ran the existing
   ProfileEngine builder in attempt 02; it built and saved a new three-world
   WP11 fixture after one 1,024-tick native advance. Its £100m starting fixture
   money is assistance, not ordinary-economy evidence. The desktop proof itself
   ran within the normal sandbox. Original and copied save hashes remain equal.
2. Native-controller compilation first exposed a Money/int conditional and then
   the location-argument requirement of SkipToOrder. Both test-only API mistakes
   were corrected; logs are retained.
3. `desktop-01` passed its initial assertions, but image review found horizontal
   resize fixed at its initial width. A resizable native panel/spacer fixed the
   production layout; the test now requires both dimensions to grow. The early
   two-unit movement check was strengthened to a four-tile displacement.
4. `desktop-02` passed stronger assertions, but independent image review found
   left/right directions reversed against the renderer basis. Swapping the local
   yaw signs fixed the production controls. `desktop-03` repeats and passes all
   checks, with scene orientation independently verified. Earlier images remain
   failed visual evidence even though their assertion processes exited zero.

Reproduce the final graphical proof on a disposable compatible fixture:

```sh
python3 scripts/test_cab_view_native.py --save /path/to/disposable-fixture.sav --output /path/to/new-proof-directory
```

The runner requires the existing ON test binary, installed OpenGFX and SDL
offscreen support; it does not build or install anything. It refuses an existing
output directory and copies the input save. Raw final native command, config,
profile isolation and content hashes are preserved in its runner manifest.

Human UAT, Windows/macOS and interactive performance remain Pending. Complete
composite sprites and unsupported gate/tunnel/bridge rendering remain deferred.
No Stage 2/3 implementation, commit, push, PR, merge, release or publication was
performed. Primary and prior worktrees remain inspect-only.
