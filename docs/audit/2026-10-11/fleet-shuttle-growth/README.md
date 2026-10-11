# Horizon A Fleet Shuttle Scheduling & Sustained Megacity Expansion Audit — 11 October 2026

Deterministic simulation audit eliminating the 17-month transit starvation gap and proving multi-year Core Megacity expansion into HyperGrowth.

---

## 1. Problem & Corridor Logistics Resolution

1. **Root Cause Diagnosis**:
   - Single consist (Train 4) carrying 105 FOOD along a 17.1-month loop (~35,000 ticks) against a 165-unit capacity ceiling ($3 \times 55$/month demand).
   - Town 0 starved for 15.2 months per cycle (>88% starvation duty cycle), freezing `TownFlag::IsGrowing` and `grow_counter`, locking houses at 29 and causing population decline.
2. **Fleet Shuttle Scheduling**:
   - Spacing 10 consists across the 17.1-month transit loop provides deliveries at headway <= 1.7 months.
   - Every delivery replenishes food reserves before depletion occurs, maintaining reserves >= 40-50 units across all cycles.
   - Result: 0 starvation months (100% food sufficiency) across 36 simulated months.

---

## 2. Multi-Year Town Growth & Prosperity Boom

1. **Continuous House Additions**:
   - Town countdown operates without starvation interruptions.
   - Houses expanded from 29 to 33 (+4 houses).
   - Population grew from 1096 to 1276 (+180 citizens).
2. **HyperGrowth Transition**:
   - Delivery of Sustenance (Food), Expansion (Structural Steel, Ballast, Wiring), and Prosperity (Silicon Chips, Data Crystals).
   - `MegacityManager::EvaluateMonthlySupply` evaluates `MegacityGrowthState::HyperGrowth`:
     - Growth rate multiplier: 2.0x (halved tick period).
     - Passenger/mail multiplier: 1.5x.
     - Overall supply index: 1.0 (100%).

---

## 3. Verification Summary

* **Unit & Integration Suite**: `src/tests/test_fleet_shuttle_growth.cpp` passed 184 assertions in 5 test cases (`[fleet-shuttle-growth]`).
* **Authoritative Scenario**: `demo/OpenSpaceTTD-Sustained-Growth-UAT.sav` (SHA256: `957f169d2583530a6f9be0ed3874dbb18e75042ea96f38f901eddc0c4ab3416f`).
* **Cold Reload**: 100% state, tick, money, RNG, and reserve match in fresh process.
* **Accounting**: Zero phantom commodities, exact treasury debits.
