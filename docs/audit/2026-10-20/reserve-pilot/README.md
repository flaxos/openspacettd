# Reserve Works pilot — native run, 20 October 2026

The owner approved the frozen Reserve envelope on 20 October 2026: one run of exactly
24 monthly evaluations (30 minute limit), then one fresh-process reload probe of exactly
four evaluations (5 minute limit), stopping at the first failed invariant. This is that
single run. It is headless engineering evidence, not human desktop acceptance.

## Inputs

- Source: `codex/capacity-or-reserve-pilot` at `45d9a2fd7f` plus the runner rework in the
  same delivery commit (`scripts/verify_commonwealth_works.py`).
- Binary `build/openttd` (Linux, Release) SHA-256
  `e57db3415c3a55ad717ad444788ceca71f0aa955cbee2f24e609052378a40a03`.
- Fixture: `connected_economy prepare-works`, seed 11, arctic, 1950 start, food rate 20.
  The works kit is no longer seeded; trains haul STEL and BALL to the World 1 warehouse.
- Command: `python3 scripts/verify_commonwealth_works.py --works-choice reserve --output build/audit-reserve-pilot-20261020`

## Result: PASSED (integrity)

| Check | Result |
|---|---|
| Monthly evaluations, main run | exactly 24, dates strictly increasing (27 advances) |
| Cargo conservation | held at every advance, main run and probe |
| Kit haulage | 160 STEL + 320 BALL in World 1 after 8 evaluations, none seeded |
| Commission debit | exactly £50,000, 160 STEL and 80 BALL from World 1; nothing else |
| Reserve cap | SILC input cap 600 → 1,200 after commissioning; never exceeded |
| Interruptions | SILC stopped at evaluation 12, FOOD at 14, both resumed at 18 |
| Cold reload | tick, date, money, held, stocks, facilities and economy equal |
| Post-reload | exactly 4 evaluations, conserved |

## What the run shows

- **The Reserve buffer was never used.** SILC waiting peaked at 270 after commissioning,
  below even the 600 baseline cap. Each 210–270 SILC delivery is consumed within the month
  by the crusher's 100 batches (200 SILC) per month, so nothing accumulates.
- **Reserve did not carry production through the feeder cut.** The crusher made 20 batches
  in the interruption window and then stopped; it restarted two evaluations after resumption,
  when the first train returned.
- **FOOD service** consumed 240 units at rate 20. Deliveries of 60 cover three of every four
  months. Population moved between 807 and 929 (904 at start, 876 at the end).
- **BALL piles up in the warehouse** (1,360 at the end), because nothing but works
  commissioning consumes it.

In this fixture the Reserve choice gives the player nothing visible. It only matters when
deliveries arrive faster or in larger bursts than the plant can process, for example with
longer consists or a second source such as the Sprint 3 World 3 link.

## Files

- `evidence.json` — full per-advance ledger, commission record, probe ledger and summary.
- `checkpoint-24.sav` — state after evaluation 24 (SHA-256 `c43318f38d109c4d129e678c429c078e7a47a3e5b244579e7977ebe294ceceef`).
- `run-logs.tar.gz` — engine and probe console logs and the generated `works.cfg`.

The commissioning checkpoint (`commissioned.sav`, SHA-256
`77b2951f782af9eaa22efe8e2e4d85227c6c096de385c12c319d8749c0dca748`) is not committed.

Human desktop UAT of the works buttons and FOOD controls remains **Pending**.
