# First Core Construction Basket: first native attempt

**ASSISTED FUNCTIONAL FAIL at the first advance before any simulation tick.**
Human UAT Pending; ordinary-start economics and independent fresh repeatability NOT RUN.
This is the owner-authorized new basket campaign, separate from the closed
1 October food/HQ/Materials I campaign. No new virtual or real-money assistance,
loan, cargo, technology, population or balance change was used.

The [raw proof](attempt-1-proof.tar.gz) and [hash manifest](attempt-1-manifest.json)
retain all 43 regular run artifacts, including the exact copied source save,
profile, frozen state, native console and command logs, every paid transaction,
the plan, retained failed result, and the final runner report. Nine disposable
profile GRF symlinks are listed in the manifest and omitted from the archive;
their target content hashes are pinned in `campaign/preparation.json` and already
published in Git. Archive SHA256:
`44e5478562e9da75125c3320a2289b20c56fbb1b017615af1296aa050434d961`.

| Boundary | Retained result |
| --- | --- |
| Source | Save `206acdd9485bed64927197af89e07a70926e0b1e386ed51cb93744df2c78e2f7`, tick167168, cash£3,705,864/debt£100,000, Materials I retained |
| Executed build | Source head `dcd0328d98c9bb9cf2e94cb56de7d70f861d23f1`, binary SHA256 `5ed099724b59f9ad363d323003389636d22f0ddb07265a845b89edc42666e33b` |
| Paused checks | Full source equality, terrain/cargo text and adapter admission PASS |
| Planning | Stable layout0 PASS: all8 endpoint searches within16 candidates/30,000 states, construction quote£293,395 + vehicles£70,886 = £364,281; frozen plan hash `5c8a8a651388809d605440497a4292128210ae257d9cdda86194f8e813707b2a` |
| Paid construction | Four normal IRON/STEL/SILC/BALL services and receiver identities PASS. 1,811 native debit rows total exactly£364,281; no receipts. Post-build terrain/cargo text PASS. |
| First advance | One 2,048-tick request counted; native pre-tick guard rejected `unbounded-fleet-running-or-speed` before `StateGameLoop`: **0 actual ticks**. No cargo arrivals, monthly basket, missing-BALL control, cold load or final save was run. |
| Final cash | £3,705,864−£364,281=£3,341,583; debt£100,000, new assistance£0, cash/cargo conservation PASS. Gross aggregate with historical debits£3,131,053, leaving a numerical£385,719 under this new cap, without permission to retry. |
| Time | First-advance clock started `2026-10-03T00:58:49.320471Z`; immutable stop `02:13:49.320471Z`. Process ended at `00:58:50.502831Z`; stopped time remains in the clock. |

All six front locomotives in the retained `vehicle_guard` have cached/current
speeds at or below72, so the combined guard's speed clauses did not cause this
failure. Its `GetRunningCost() != 0` clause rejected the ordinary positive annual
running cost of the pinned trains. OpenTTD converts that annual amount and running
ticks to a fractional daily charge, then books any whole-pound carry; a nonzero
annual cost is expected. The guard had incorrectly required zero because the old
campaign's *booked* train running charges happened to be£0. This is a narrow
source/guard defect, not a material-chain design stop or a grant requirement.

Commit `1d6781761be6e6657cb8eb54d1361e9d31585a00` replaces that false
assumption with a conservative per-tick bound using the native annual cost,
possible next running tick, and fractional cash carry. It retains the six-front
speed and all other authority, search, spending and integrity checks. The
corrected source has **not** been run in a game. The original failed result and
raw logs remain unchanged.

The attempted paid state existed only in the closed native process; no
post-construction save was made before the hard stop. Any confirmation would
therefore need a **new explicit handoff** that names the original source save,
the one additional load/attempt, and how the failed attempt's £364,281 gross
debits count within the existing £750,000 bound and immutable wall clock. Do not
silently reset that ledger, grant cash, retry automatically, or label the focused
tests as the requested material-basket proof. The owner may instead review this
as partial source delivery and defer the functional outcome.
