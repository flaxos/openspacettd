# Human UAT results — recovery / v1.1

Suite/fixture ID: ______  Save SHA256/version: ______  Base graphics/version: ______
Language/UI scale/resolution: ______  Simulation date: ______  Reset copy: ______

Tester: ______  Date: ______  Build/hash: ______  Save copy: ______
Company name: ______  Active NewGRFs: ______  Environment: ______

Record observations, not assumed passes. Attach screenshots or logs and the
failing working save where appropriate. Never put account credentials in evidence.

**WP-11 automated update (16 September):** the separate version-1 active-content
structural fixture passes moving ore→steel→depot, two local portals, blocked-route
recovery, fresh-process reload and three complete deliveries in 1800 and 2300.
377 CTests pass. No new human pass is claimed. [Quick check and saves](WP11-UAT.md)
· [evidence](../docs/audit/2026-09-16/wp11/README.md).

**WP-02 automated update:** exclusive hub unloading is implemented locally;
57 distinct selected CTests pass, including real cargo/station ticks and save/reload.
This is not a human UAT pass. [Evidence](../docs/audit/2026-09-15/wp02/README.md).

**WP-03 automated update:** allocation-safe loading is now implemented. Four new
cases cover full pool/retry,12 reserve/capacity/order/rights scenarios, packet
cleanup and actual save/reload after failed pickup or failed splitting.
Human delivery/pickup acceptance remains open; WP-04 subsequently repairs binding
while the establishment/reserve UI remains WP-09.
[Evidence](../docs/audit/2026-09-15/wp03/README.md).

**WP-04 automated update:** supplied/automatic station authority, demolition,
company acquisition/bankruptcy and persistence are repaired locally. Seven new
regressions exercise real commands and engine lifecycle/save paths. This does not
mark the UAT-10 graphical lifecycle checks as passed.
[Evidence](../docs/audit/2026-09-15/wp04/README.md).

**WP-05 automated update:** HQ upgrades and Directory actions now use authoritative
commands. Seven new local tests and88 related CTests pass; a native-command TCP
server/two-client replay confirms denied actions preserve state, three successful
changes apply once and full compared state survives save/reload. Outpost locations
now persist. HQ rendering was repaired, but no graphical result is claimed.
UAT-08/10 authority captures and a full multiplayer join remain pending. Existing
settlement fixtures isolate the separate WP-08 town initialization defect.
[Evidence](../docs/audit/2026-09-15/wp05/README.md).

**WP-06 automated update (16 September):** strict parser/transform admission and
atomic file storage are repaired locally.35 selected CTests pass, including actual
Import/Export query/OK callbacks and existing128 prefab variants. Six real I/O
failure points preserve old bytes/memory, survive reopen and permit retry.
UAT-04f/g graphical retest remains pending; capture and routes remain WP-07.
[Evidence](../docs/audit/2026-09-15/wp06/README.md).

**WP-07–09 automated update (16 September):** capture/naming/cancel, 312 native
signalled train routes over the revised catalogue, native populated colony
founding, HQ/hub/reserve controls and cargo persistence are implemented locally.
The cargo train now drives from its depot to the hub using native orders; all 60
units remain accounted for through unload, reserve-limited pickup and process
reload. Switching/re-clicking Blueprint and HQ/Hub tools is also repaired and tested.
All 363 CTests pass. Older empty outposts are preserved; automatic repair is
unsupported. UAT-04, 08, 10–12 and reload still need visual acceptance on the newer
build. [Evidence](../docs/audit/2026-09-16/wp07-09/README.md).

## Subsequent user gameplay report

The user reports: “OpenTTD no longer crashes. UAT seems to pass.” This follows the
requested UAT-00,04a–c and09 run and is recorded as a positive user-reported smoke
result. The assistant did not observe it; individual eight-prefab, transform,
obstruction and save/reload results were not separately supplied. Preserve that
scope rather than treating this as acceptance of unrelated UAT cases.

The environment block below describes the earlier assistant attempt; it does not
invalidate the user's later gameplay observation. WP-04 is subsequently repaired
locally as recorded above. Existing user-created save files must be preserved.

## Earlier assistant graphical attempt — blocked by environment

UAT-00,04a–c and09 could not be operated or visually checked: configured
`orca-ide` is missing, native GUI control is disabled, and the X display probe
cannot open `:0.0`. The SDL process stayed alive for10 seconds and was stopped;
that is not a graphical pass. Crash/M1 copies are prepared and hashes unchanged;
a fresh clear pad has not been prepared. [Attempt evidence](../docs/audit/2026-09-15/openspacettd-blueprint-gui-uat-s63nc900/README.md).

| Case | Result | Actual result / evidence / issue |
|---|---|---|
| 00 Main-window and Blueprint smoke | User-reported no crash / apparent pass | Later user gameplay report above; per-window detail not supplied |
| 01 Ownership and local train run | Not run | |
| 02 World navigation/biomes | Not run | |
| 03 Gate construction/linking/safeguards | Not run | |
| 04 Eight prefabs and player blueprints | Historical crash; current smoke reported passing | User confirmed Mainline Double Straight placement crash; OST-BP-001. Separate04a–g below; capture/export/routes now repaired locally, visual retest pending |
| 05 Spaceports/conduits | Not run | |
| 06 Megacities/corridor traffic | Not run | |
| 07 Ledger/governance UI | Not run | |
| 08 Colonisation/promotion/restrictions | Not run | |
| 09 Save/reload/crash regression | Included in general UAT report; not individually confirmed | User reports UAT seems to pass; no separate persistence observation supplied |
| 10 HQ/stockpiles/hubs | Not run visually on current build | WP-09 controls and cargo/reload checks pass automatically; player delivery and acceptance remain pending |
| 11 Fabrication | Not run | |
| 12 Research | Not run | |
| 13 Full Commonwealth content | Human check pending; structural branch automated | Separate WP-11 v1 fixture loads 13 cargoes/14 industries/12 trains; other pipeline behavior is not accepted |
| 14 Player production pipelines | P0 Blocked / P1 Not run | Need unhubbed Developed P0 fixture; M1 World2 is hubbed. Staged integration tests pass, moving player chain not observed; content depends13 |
| 15 Bespoke art | Blocked | Sprint 38 assets pending |
| 16 Independent-server federation | Blocked for natural route/recovery | Source-confirmed external entry rejection; WP-F1/F2 and exact F1 fixture needed. Manual dispatch is separate |

For each failure/blocker: case/substep, prerequisites, expected result, actual
result/error, reproducibility, evidence path and follow-up issue. For federation,
record natural departure, remote arrival, visible return/orders, mismatch,
obstruction, deduplication and restart independently. Do not aggregate a partial
run into one Pass.


## Preserved reported failure (not a new graphical audit run)

15 September2026 user confirmed the Mainline Double Straight placement crash:
`crash20260915082450` (local OpenTTD data directory), modified a6b79add build,
GS9/OpenGFX/no active NewGRFs, save367. Assertion command.cpp:365 and placement
stack; exact terrain contribution unknown. Original artifacts/hashes are in
[OST-BP-001](../docs/CRITICAL_BUG_REVIEW_2026-09-15.md). User's prior ownership
complaint is retained for01 retest; historical repair is not a current human pass.

## Per-subcase worksheet (copy for every attempted subcase)

- Test/subcase ID and purpose:
- Suite/fixture SHA256, build/ref+dirty diff/binary hash, save version:
- Content/base assets, company/world/station/train IDs and coordinates:
- Cash/materials/research, loading orders/service, initial date/counters:
- Exact clicks and simulation events/months elapsed:
- Expected result / actual result / exact error:
- Negative case and resulting state delta:
- Screenshot/video/log/save paths (local; no credentials):
- Result: Pass / Fail / Blocked / Not run:
- Defect ID, smallest missing prerequisite/check, reset completed:

Blueprint: record04a opening/selection/preview,04b transforms,04c placement/rejection,
04d routing,04e capture/naming,04f export/import,04g persistence separately.
Conduit: record nominal counter, waiting, onboard, accepted downstream, service,
rating where available, and monthly event. Production: distinguish input consumed,
output produced, waiting, stockpile and final useful delivery. Federation: separate
natural departure, arrival, full consist/cargo, orders/return, mismatch, obstruction,
duplicate/lost response, restart and older-save recovery; never aggregate partial pass.

## Automated audit evidence, kept separate

Current abbcd7e077 binary:67/67 selected CTests pass; copied M1 and Blueprint crash
save load in null-video1000-loop smokes. No GUI action was executed, so **no human
or graphical case was marked Pass**. See [check records](../docs/audit/2026-09-15/checks.json).

Production verification caveat: live-main CI fails three invalid-border fixtures;
local Release preprocessing disables assert in that test translation unit despite
WITH_ASSERT. Catch2 checks pass but do not establish that invariant. See
OST-TEST-001 before relying on this as production acceptance.

## WP-01 implementation verification — later follow-up

The rebuilt local game includes the Blueprint command repair. **78 distinct
selected automated tests pass**, including all 128 prefab transform/payment
combinations and a real infrastructure/money/material save/reload test. See
[WP-01 evidence](../docs/audit/2026-09-15/wp01/README.md) for exact commands,
fixture limitations and failed-before/fixed-after evidence.

Human cases 00/04/09 remain **Not run** on this build. The prior Mainline placement
failure stays historical. Retest on copied BC/M1 fixtures and a fresh pad; retain
screenshots and exact money/material changes. Stockpile-mode signal placement now
consumes materials, so record and supply the entire stamp's materials. No graphical pass
is inferred from the automated results.
