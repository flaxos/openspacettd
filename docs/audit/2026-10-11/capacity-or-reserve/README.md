# Capacity or Reserve Pilot Prototype Audit — 11 October 2026

Native simulation audit and proof for Sprint 1 ("Capacity or Reserve") of the Commonwealth design study on isolated branch `codex/capacity-or-reserve-pilot`.

---

## 1. Core Mechanics & Architecture Proof

1. **Stable Settlements (Decoupled Baseline)**:
   - Mature native Core settlements maintain baseline 1.0 passenger generation under `EconomyRulesVersion::V2`.
   - UI status displays "Stable" rather than starvation decay or frozen growth states.
   - House expansion is entirely decoupled from commodity starvation; towns do not decay or freeze when food deliveries cease.

2. **Optional Commercial FOOD Service**:
   - Player/council-configurable service rate: `Off (0)`, `10`, `20`, or `40` units per monthly period.
   - Fixed receiver capacity ceiling of `60` units.
   - When configured `Off`, incoming deliveries are rejected and remain in train custody; existing reserves are preserved without evaporation.
   - Sliding 4-month civic benefit provides up to +25% passenger multiplier (`1.0 + total_consumed / 640.0`).
   - Exact integer remainder accumulator in town simulation ensures fractional passenger generation is never truncated or rounded away across ticks.

3. **Factory Works Choice (Capacity vs. Reserve)**:
   - One-time commissioning choice on player-owned bootstrap factories (e.g. Ballast Crusher: 2 SILC -> 2 BALL).
   - **Baseline**: 100 batches/month, 300 batch-equivalent input buffer (600 SILC), 300 output buffer.
   - **Capacity Works**: 150 batches/month throughput (+50%), 300 input buffer, 300 output buffer.
   - **Reserve Works**: 100 batches/month throughput, 600 batch-equivalent input buffer (1200 SILC, +100% buffer depth), 300 output buffer.
   - Real material costs: `160 STEL + 80 BALL` consumed atomically from the world company stockpile, plus £50,000 cash.
   - Duplicate commissioning and cross-switching are strictly rejected by server-authoritative command checks.

---

## 2. Headless Native Simulation Evidence (24 Periods)

Headless native 24-period simulations (`scripts/verify_commonwealth_works.py`) were executed for both works choices (`capacity` and `reserve`):

1. **Feeder Interruption & Buffer Behavior**:
   - Period 13: Silica (SILC) supply train stopped to simulate feeder disruption.
   - Periods 13–18: Ballast Crusher draws down local input buffer and stalls production cleanly once depleted (held at 405 batches), while independent steel and food facilities continue operating.
   - Period 19: SILC train restarted. Crusher replenishes input buffer and resumes processing (reaching 675 batches by period 20 and 810 by period 24).
2. **Food Delivery Interruption & Town Stability**:
   - Period 15: Food delivery train stopped to test starvation immunity.
   - Periods 15–18: Core metropolis consumes remaining local reserves and drops food service to 0; population remains stable (~890–920 citizens) with zero starvation collapse or negative decay.
   - Period 19: Food deliveries resumed; reserve replenishes and passenger bonus returns smoothly.
3. **Cargo Ledger Conservation**:
   - 100% cargo conservation verified at every single period: `held[c] + produced[c] - consumed[c] == after_held[c]`.
   - Zero phantom cargo generated; zero cargo dropped.
4. **Fresh-Process Cold Reload**:
   - Savegame at period 24 (`checkpoint-24.sav`) reloaded into a completely separate native process (`works_reload_probe`).
   - 100% equality verified across tick, calendar date, treasury, held goods, facility batches, and economy rules state.
   - 4 subsequent post-reload periods completed with 100% cargo conservation.

---

## 3. Verification Artifacts & Test Coverage

* **Automated Catch Suite**: `src/tests/test_commonwealth_works.cpp` passed all 150 assertions across 6 test cases (`[commonwealth-works]`).
* **Regression Protection**: `[integrated-economy]` passed all 490 assertions in 17 test cases with zero regressions on V1 / legacy behavior.
* **Evidence Data**:
  - `docs/audit/2026-10-11/capacity-or-reserve/evidence-capacity.json` (Capacity Works 24-period ledger & reload proof)
  - `docs/audit/2026-10-11/capacity-or-reserve/evidence-reserve.json` (Reserve Works 24-period ledger & reload proof)
