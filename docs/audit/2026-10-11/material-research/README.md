# One Material-Funded Research Unlock (Card P1) Audit — 11 October 2026

Native simulation audit and proof for Card P1 ("One Material-Funded Research Unlock") of the Commonwealth design study on isolated branch `codex/capacity-or-reserve-pilot`.

---

## 1. Concrete Architecture & Pipeline B Verification

1. **Baseline Prerequisites & Feedstock Governance**:
   - **Corporate HQ**: Built on Core World 0 (`TileXY(70, 60)`), funded via server command `Commands::PlaceCorporateHQ` costing £2,500,000 cash.
   - **Research Feedstock Acceleration**: Explicitly disabled (`CmdSetResearchAcceleration(false)` / `IntegratedEconomy::SetAcceleration(false)`). Delivered `CHIP` cannot be burned for monthly RP; physical units must remain in the company stockpile for research kit reservation.
   - **Baseline Technologies**: Materials I (`TECH_MATERIALS_1`, 301, authored 100 RP) and Materials II (`TECH_MATERIALS_2`, 302, 250 RP funded with £250,000 cash budget) unlocked prior to Materials III.

2. **Physical 2-Input CHIP Conversion and Pipeline B Freight**:
   - **Copper Smelting** (`RECIPE_COPPER_SMELTING`, 0x17): 2 `COPR` -> 2 `WIRE` at Copper smelter (Node 7).
   - **Silicon Arcology** (`RECIPE_SILICON_ARC`, 0x19): 2 `SAND` + 1 `WIRE` -> 1 `CHIP` at Silicon works (Node 8).
   - **Structural Steel Smelting** (`RECIPE_STEEL_SMELTING`, 0x13): 2 `IRON` -> 1 `STEL` at Steel furnace (Node 5).
   - **Dedicated Freight Rail Consists**:
     * Copper mine (Node 15) -> Copper smelter (Node 7): `COPR` (4 wagons)
     * Copper smelter (Node 7) -> Silicon works (Node 8): `WIRE` (2 wagons)
     * Silica dunes (Node 16) -> Silicon works (Node 8): `SAND` (4 wagons)
     * Silicon works (Node 8) -> HQ warehouse (Node 0): `CHIP` (1 wagon)
     * Steel furnace (Node 5) -> HQ warehouse (Node 0): `STEL` (2 wagons)

3. **Atomic Escrow & Negative Paths**:
   - **Insufficient Kit Rejection**: Rejects research selection if company stockpile lacks 40 `STEL` or 20 `CHIP`.
   - **Foreign Company Rejection**: Company 1 without HQ/stockpile is rejected.
   - **Escrow Reservation**: Moving 40 `STEL` + 20 `CHIP` from World 0 stockpile to `research[c0].reserved` debits stockpile cleanly while preserving conservation equation (`Snapshot()["held"]` accounts for reserved units).
   - **Cancellation / Switch Refund**: Cancelling research returns 100% of reserved cargo back to the World 0 company stockpile without duplication or loss.

4. **Tier III Research Progression & Opportunity Revelation**:
   - **Research Project**: Materials III (`TECH_MATERIALS_3`, 303), 600 RP cost.
   - **Ordinary Research Commands**: Escrow kit reserved, project selected, £600,000 cash budget set (`Commands::SetResearchBudget`).
   - **Unlock & Consumption**: At monthly evaluation, 600 RP generated, project completed, escrowed kit consumed into `_commonwealth_slice_audit->research_consumed`.
   - **Rare Earth Opportunity Revealed**: 16x16 Rare Earth extraction site (0x14, producing `RARE`) surveyed on World 2 at Node 17 transitions from undiscovered (`Discovered() == false`) to discoverable (`Discovered() == true`).

---

## 2. Headless Native Simulation Evidence

Headless native runner (`scripts/verify_commonwealth_research.py`) executed on a fresh disposable map:

1. **Pre-flight & Commissioning (Period 0)**:
   - Scenario prepared under experimental-v2 rules.
   - Corporate HQ placed (£2.5M capital expenditure).
   - Materials II unlocked (£250k research budget).
   - Pipeline B extractive/processing industries and rail routes commissioned.
   - Rare Earth site registered and surveyed (Site ID 6, £60,000 survey cost).
   - Verified Rare Earth site: `surveyed == true`, `discovered == false`.
2. **Freight Hauling & Accumulation (Periods 1–8)**:
   - Real freight consists run across network.
   - Period 6: Steel furnace deliveries reach HQ warehouse (`stel_stock == 80 >= 40`).
   - Period 8: Silicon works deliveries reach HQ warehouse (`chip_stock == 35 >= 20`).
3. **Escrow Contract & Unlock (Periods 8–9)**:
   - Negative paths verified (missing kit rejected, foreign company rejected).
   - Atomic escrow debited (`stel_stock: 80 -> 40`, `chip_stock: 35 -> 15`).
   - Escrow refund verified (`stel_stock: 40 -> 80`, `chip_stock: 15 -> 35`).
   - Re-reserved kit and funded Materials III with £600,000 cash budget.
   - Period 9: Monthly research evaluated; `TECH_MATERIALS_3` (303) unlocked!
   - Rare Earth site 6 transitioned to `discovered == true`!
4. **Post-Unlock Stability (Periods 10–15)**:
   - 6 additional periods advanced under unlocked Materials III.
   - 100% cargo conservation verified in every step (`errors == []`).
5. **Fresh-Process Cold Reload**:
   - Checkpoint savegame (`checkpoint-materials3.sav`, SHA-256 `0ddfdb3c4423868f0c096b4aa3b100c4d2f2306fcd4dd1bacd2e66b029df7c02`) reloaded in a fresh engine process (`research_reload_probe`).
   - 100% state match verified across tick, date, money, held, stocks, facilities, economy, hq, research, and resource_sites.
   - 4 additional post-reload periods advanced with 100% cargo conservation.

---

## 3. Verification Artifacts & Hashes

- **Binary SHA-256**: `de864314237d4036536b9d966c586bf0d5a46a48023bbbadce2be81f7ef3ef18`
- **Checkpoint Save SHA-256**: `0ddfdb3c4423868f0c096b4aa3b100c4d2f2306fcd4dd1bacd2e66b029df7c02`
- **Audit Evidence JSON**: `docs/audit/2026-10-11/material-research/evidence.json`
- **Automated Tests**:
  * Unit Test Suite: 416 assertions in 14 test cases in `[commonwealth-works]` passed (`./build/openttd_test '[commonwealth-works]'`).
  * CTest Full Suite: 564 out of 564 tests passed (`ctest --test-dir build --output-on-failure`).
  * Repository Linters: `python3 .github/file-descriptions.py` (OK), `python3 .github/unused-strings.py` (OK), `git diff --check` (clean).
