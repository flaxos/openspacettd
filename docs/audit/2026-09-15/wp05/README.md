# WP-05 — command-only HQ and Universe Directory state

**Implemented and verified locally, 15 September 2026.** User goal `WO-05` was
interpreted as WP-05 from the preceding recovery recommendation. Both binaries
were rebuilt in the existing `build` directory. No commits, publishing or user-save
changes were made. [Commands, results and hashes](checks.json),
[package-only source delta](source.patch), [separate source reviews](source-review.json).

## Changes

- HQ Upgrade Tier posts a normal command carrying the exact requested next tier.
  The command rejects foreign/invalid companies, missing HQs, malformed tiers,
  skipped tiers and duplicate/stale requests. Successful execution advances once.
  Existing upgrades remain free; no new progression requirements were introduced.
- HQ company-zero viewing and foreign action guards are corrected. Its paint
  handler now calls `DrawWidgets` after updating state and counts.
- Directory opening, refresh and selection read a canonical local-world projection
  without registering or pruning shared model data. Colonize/Promote only post
  commands. Successful execution synchronizes phase/name/biome and dirties the
  window; rejection makes no speculative change. Existing fees and thresholds remain.
- The persistence test found that PLNT omitted `outpost_tile`: before repair a
  colony's location changed from1300 to INVALID_TILE after reload. The named table
  now stores it. Older rows retain an invalid default; no old location is guessed.

## Verification

| Check | Result |
|---|---|
| New command, actual GUI-click and save/reload CTests | 7/7 pass |
| Related Blueprint/CST, hubs, production, lifecycle, HQ, tech and spaceport CTests | 88/88 pass |
| Native command TCP relay: server + two independent client engine workers | Pass: six denied requests, three successful changes, equal compared state before/after execution and actual save/reload |
| Old crash and M1 save copies, rebuilt game with null video for1000 loops each | Both exit0; copy hashes unchanged |
| Built executable help/version | Exit0 |
| Registered CTests | 338;95 distinct cases run above |

The TCP runner uses native packet serialization, sanitizing receive and server/
client command execution queues. Python controls the relay. It does **not** run the
full multiplayer join/authentication/map-transfer handshake. The worker's default
CTest invocation returns without starting the protocol and is not counted above.
The runner used scoped approved loopback sandbox escalation and cleaned up all workers.

The first local run passed2/7: the fixture had set screen dimensions without
allocating the dirty-screen buffer. A temporary signal backtrace located
`AddDirtyBlock`; normal `ScreenSizeChanged` initialization fixed all five crashes.
The next run passed6/7 and exposed the real PLNT omission, which was repaired
without weakening the full-state comparison. Failure logs are retained. Earlier
syntax-only checks also caught a Catch macro/ternary parenthesis issue; final syntax
checks and builds passed. No production assertion was weakened. The pre-existing
Release/NDEBUG evidence limitation remains OST-TEST-001.

## Remaining acceptance and next package

Actual window creation and click handlers run with mock fonts/sprites. Rendering
and live-player multiplayer joining remain unobserved. Check UAT-10 on an existing
owned HQ: one tier per accepted upgrade, foreign controls blocked, then save/reload.
Check UAT-08 denial leaves phase/name unchanged; a successful eligible action must
agree with the Directory and retain its location after reload.

Tests use an existing settlement. Native town initialization and atomic founding
remain **WP-08**; this package does not establish functioning new-town acceptance.
Missing HQ/hub establishment controls remain WP-09.

**Next: WP-06, safe Blueprint parsing and library persistence.** Reject malformed
imports and preserve the old library on write/rename failure. Do not begin that
package without subsequent authorization. Preserve all existing dirty work and saves.
