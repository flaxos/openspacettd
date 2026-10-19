# Local gate cab — Stage 2 engineering evidence

The isolated default-OFF cab now follows one owned train through a local gate,
shows opaque localized transit, and adopts the actual arrival world and pose.
Per-car visibility hides wagons still in the source world or wormhole. No
production train movement, economy, save format or network protocol changed.
This is automated engineering evidence; human acceptance remains Pending.

Baseline: `8182b3903f9a231d81b1823e82e6ec77e8a6a6ce`, local branch
`codex/cab-view-stage1`. Upstream source provenance remains
[`15.3-cabview-alpha1`](https://github.com/vaclavloubek/OpenTTD/tree/b7bd17b04dde3379b2be33be40ba4d6fc9f5d391).
The [active plan](../../../ACTIVE_EXECUTION_PLAN.md) is the sole implementation
handoff. Changes remain uncommitted and unpublished.

## Verified result

| Check | Result |
| --- | --- |
| Native engine and test builds | OFF and ON pass; OFF contains zero cab translation units/definitions. |
| Focused regressions | 36/36 CTest cases, 11,160 assertions. |
| Repository checks | File descriptions, unused strings and whitespace checks pass. |
| Native round trip | A→B→A, same primary train ID 0, four cars; 25 assertions. |
| Fresh-process reload | Existing save path, frozen state equality before advance, no persisted cab, reopened return journey; 22 assertions. |
| Mixed-world rendering | Three observed samples each have one arrived, two hidden, one source-world car; real CPU-render exclusion checks plus a visible-engine positive control. |
| Normal viewport | Both paused before/after regions `[0,22,1280,800]` have exactly zero changed pixels. |
| Runtime budget | 2,183 round-trip plus 336 cold explicit game-loop calls = 2,519 / 24,576. |
| Independent review | Astra source/image/evidence review CLEAR. |

The native proof uses isolated SDL offscreen/software processes, a new disposable
WP11 save, installed OpenGFX and pinned NewGRF hashes. It does not use the owner's
display or save. Preparation requests zero simulation advances; ordinary native
commands grow train 0 to four cars in its depot. Native route/reverse commands and
intentional movement are separate from paused camera-purity comparisons.

The round trip takes 5.652 seconds and the cold process 1.062 seconds on this
machine. These are stepped harness elapsed times, **not frame-rate measurements**.
Explicit calls exclude automatic native timer ticks: initial game tick 1,280 to
3,641 includes 2,183 explicit calls; cold tick 3,295 to 3,641 includes 336.
Both original input saves remain unchanged.

Paused comparisons cover RNG, calendar/game tick, cash, full relevant consist and
cargo packets, exact serialized order bytes, and gate/world metadata. Cold reload
compares that frozen state before deliberate movement. The first cold event's
pose fields are `-1` because the event recorder is not attached yet; the equality
check itself resolves train 0 and passes. The diagnostic 160×100 mixed-car render
is separate from the actual cab-window screenshots.

## Captures

[Gate approach](gate-approach.png) · [Opaque local transit](gate-transit.png) ·
[Arrival with bounded floor](gate-arrival.png).

All 20 passing screenshots and the 20 previous-attempt screenshots are inside
[the evidence archive](verification-evidence.tar.gz). Screenshot, save, source,
binary and content hashes are recorded in [verification.json](verification.json).
The archive has 129 files and SHA256
`6b1612aef1798221760158a79e6ae0680dd329cddfba96f41b2d026da6cd5a2e`.
Profile caches and duplicate screenshot-directory copies are omitted.

## Failures retained and fixed

Earlier source attempts, controller review findings, rejected launches and their
limits remain in the preserved source-ready checkpoint and bundled source logs.
Direct owner approval resolved the launch blocker; the unchanged isolated
fixture command was then accepted by automatic approval review.

Native attempt 01 passed runtime assertions but failed visual acceptance: the
local gate branch returned after drawing its frame without covering its own
floor. This exposed a cyan haze band under the arriving cab. The fix adds opaque
ballast and two rail strips strictly inside the already-validated current gate
tile; it never collects ordinary tunnel or remote-endpoint geometry.

The first new fixture faced into the gate and exposed an uninitialized synthetic
vehicle after switching worlds (35/36 regressions passed). That synthetic car is
now removed after its masking check. Correct outward poses put each frame behind
the camera. The actual reverse offset of seven units projects its gate floor
below this small test image, so its positive check was invalid; the symmetric
twelve-unit case and exact failing outward pose now test visible floor geometry.
Both failed test attempts are retained. Final native attempt 02 shows the repaired
surface, passes all assertions and preserves normal viewport pixels.

## Limits and next handoff

Coverage is one flat SW/NE local gate pair, one four-car OpenGFX train and Linux
SDL offscreen/software. The frame/floor is intentionally simple; Commonwealth
front/rear gate artwork and full composite rolling-stock fidelity are Stage 3.
Ordinary tunnels, bridges, external/conduit/stale gates and server continuity
remain unsupported. Human comfort, interactive performance and other platforms
are unqualified. The feature remains OFF by default.

Preserve the Stage 1 patch unchanged, SHA256
`42754c64b80c8d2b7d892af4f3177180be81e33ba7fcfddca665145d531748fe`,
and the source-ready Stage 2 patch, SHA256
`ef0a93a41c6235da72b9afbda820714826647b9f3c6a87e216126ea5409f62a8`.
Stage 3 starts with the roadmap's bounded graphics/performance review and a
concrete content/profiling matrix; this evidence does not accept a wider release,
new assets, GPU redesign, persistent settings or federation continuation.
