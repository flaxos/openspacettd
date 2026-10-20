# Agent-simulated walkthrough — 20 October 2026

A scripted pass over the player-facing windows, run in the real game in an SDL offscreen display
(1920×1200). Every image is a real screenshot, reviewed by the agent. **This is not human
acceptance**: it checks that things work, render and read clearly, not whether they are fun.
Human desktop UAT remains Pending.

- Tool: `scripts/run_agent_uat.py` → `Agent UAT walkthrough captures player-facing windows`
  (`src/tests/test_agent_uat_native.cpp`). Cab steps need a `WITH_CAB_VIEW=ON` build.
- Build: branch `codex/agent-uat-walkthrough`, stacked on PR #87 (`cabf37690b`).
- [`reserve-pilot/`](reserve-pilot/manifest.tsv): the Reserve run's final save
  (`docs/audit/2026-10-20/reserve-pilot/checkpoint-24.sav`), full walkthrough. It has no bridge on a
  train route (12,000 ticks searched).
- [`bridge/`](bridge/manifest.tsv): `demo/OpenSpaceTTD-Sustained-Growth-UAT.sav`, where train 24 crossed
  a bridge. `demo/OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav` had no bridge crossing, and
  `demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav` had no train that can show a cab.

## Worked as intended

| Step | Result |
|---|---|
| Retire | Asked first; the dialog names the plant and the loss; No kept it |
| Facilities at 2× | Rows, lines and buttons scale; nothing overlaps (PR #87) |
| Megacity FOOD | Rate button, reserve, history and passenger bonus shown |
| Cab look controls | A/A turned left; drag turned and tilted with the pointer; wheel zoomed in |
| Cab at a bridge | Holding screen while on the bridge; scene resumed after it (PR #87) |

## Findings

| # | Window | Finding | Severity |
|---|---|---|---|
| 1 | Facilities | At the default width, lines 2–3 are cut off: the recipe's outputs, the output buffer and last month's production are hidden | Medium |
| 2 | Facilities | `[Reserve Works: …]` badge and `Locate Stati…` are truncated in the action column | Low |
| 3 | Facilities, cab | Spacer gaps have no background: black at 1×, the map shows through at 2× (tab row, bottom row, cab button row) | Low |
| 4 | Facilities | Summary and Upgrade All say `t` / `t/mo` in Commonwealth mode, where the unit is batches | Low |
| 5 | Megacity | Tier 1 lines (Consumed, History) and the footer are cut off | Medium |
| 6 | Megacity | Fixed 190 px buttons are truncated at 2× (`Rate: 20/mo [Cli…`, `Commission Dis…`) | Medium |
| 7 | Megacity | Developer wording shown to players (`Sprint 2 Scope`); headings name cargos the pilot does not use (Water, Alloys, Goods, Valuables) | Low |
| 8 | Megacity | `Reserve: 60/60` beside `Consumed: 0 (last mo)` looks contradictory; the delivery arrived after the monthly check | UX |
| 9 | Cab | Caption is clipped at the default width (`Cab: Stone quarry > Ballast crus`) | Low |
| 10 | Cab | The bridge holding screen is plain black, so a short bridge reads as a blink | UX |
| 11 | Cab | Drag, wheel and keys are only explained in the hover tooltip | UX |

## After the fixes — 21 October 2026

All findings were addressed on branch `codex/walkthrough-fixes` and the walkthrough re-run
([`after-fixes/reserve-pilot/`](after-fixes/reserve-pilot/manifest.tsv),
[`after-fixes/bridge/`](after-fixes/bridge/manifest.tsv)):

| # | Outcome |
|---|---|
| 1 | Rows have seven short lines in a larger default window; nothing is cut off at 1× or 2× |
| 2 | Works badges read `Capacity Works` / `Reserve Works`; the action column fits `Locate Station` |
| 3 | Spacers have backgrounds in the Facilities tab and bottom rows and the cab button row |
| 4 | Summary and Upgrade All count batches in Commonwealth economies |
| 5 | Megacity text is split into short lines; panel heights follow the font; the footer wraps |
| 6 | Megacity buttons are sized to their labels, for drawing and clicks alike |
| 7 | Pilot tier titles; no developer wording |
| 8 | The panel explains the monthly check; the delivery counters are now saved (they reset to 0 on load before, which is why the original save showed `Delivered: 0`) |
| 9 | Not a window bug: the demo fixture cuts each station name to 12 characters to fit OpenTTD's 31-character train names. The cab window size is unchanged |
| 10 | On a bridge the cab shows its last view, dimmed to 60 %, with the message; it is black only if the cab was opened on the bridge |
| 11 | A Controls button shows the drag, wheel and key controls |

The Reserve-pilot save predates the saved counters, so its `Delivered` line still loads as 0.

## Agent opinion (not acceptance)

The Facilities and Megacity windows carry the right information but read like debug panels:
one long line per fact, several cut off at the default size. They would be easier to scan with
fewer words per line and the numbers that matter (buffer fill, batches last month, food met)
given their own columns. The cab view is the most immediately appealing feature; the holding
screen would feel smoother as a dimmed last frame than as black.
