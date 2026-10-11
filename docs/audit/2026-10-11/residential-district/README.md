# A District That Stays Built (Sprint 2) Audit — 11 October 2026

Native simulation audit and proof for Sprint 2 ("A District That Stays Built") of the Commonwealth design study on isolated branch `codex/capacity-or-reserve-pilot`.

---

## 1. Core Mechanics & Architecture Proof

1. **Finite Native Residential District Commissioning**:
   - Authorized via replicated command `Commands::CommissionTownResidentialDistrict` for Core settlements under `EconomyRulesVersion::V2`.
   - Exact bill of materials: `160 STEL + 80 BALL` consumed atomically from the world company stockpile, plus £50,000 cash debited from the company account.
   - Duplicate commissioning is rejected by server-authoritative command checks.
   - Grants a finite budget of 4 native building opportunities (`building_opportunities = 4`).

2. **Legal Road-Adjacent Frontage Placement**:
   - `TryGrowResidentialDistrict` searches outward concentric rings around town center for legal road frontage (`CanBuildHouseHere` and `RoadTypesAllowHouseHere`).
   - Constructs native house shells at stage 0 with player protection enabled (`SetHousePlayerProtected(tile, true)`).
   - Blocked physical access (lack of valid road frontage or terrain obstruction) is reported in the UI as `"Awaiting site access"` without burning materials or fabricating houses.

3. **Stage 0 Shells & Stage 3 Population Credit**:
   - Newly placed shells start at construction stage 0 and contribute 0 population.
   - Builders advance shells natively through stages 1, 2, and 3 (`AdvanceSingleHouseConstruction`).
   - Population is credited strictly upon reaching `TOWN_HOUSE_COMPLETED` (stage 3), notifying `IntegratedEconomy::OnResidentialHouseCompleted`. Zero calculated or phantom population.

4. **Permanent Demolition Immunity**:
   - Protected houses (`IsHousePlayerProtected`) are permanently immune to OpenTTD's native town redevelopment demolition loop (`CanDeleteHouse` returns `false` during `TileLoop_Town` / `ClearTownHouse`).
   - Historic town population collapse is eliminated for player-commissioned housing.

5. **Commercial FOOD Decoupling**:
   - Town food service remains purely commercial, providing a passenger generation multiplier.
   - Interrupting or turning off food service does not unlock construction, revoke unplaced building opportunities, or demolish completed houses.

---

## 2. Headless Native Simulation Evidence (24 Periods)

Headless native 24-period simulation (`scripts/verify_commonwealth_residential.py`) was executed on a fresh scenario map:

1. **Atomic Commissioning & Material Debit**:
   - Period 1: Command `connected_economy commission-residential` executed.
   - World 0 stockpile debited atomically: STEL dropped from 160 to 0, BALL dropped from 80 to 0; company cash debited £50,000.
2. **Sequential Stage 0 Shell Placement**:
   - Periods 1–4: One house shell placed per period on legal road frontage plots. By period 4, all 4 opportunities were placed (`buildings_placed = 4`, `building_opportunities = 0`).
3. **Native Construction Advance & Population Credit**:
   - Construction advanced across periods 2–11.
   - All 4 houses reached stage 3 completion by period 11 (`buildings_completed = 4`).
   - Population increased by exactly +232 citizens (matching native house specs), climbing from baseline 898 to 1,130.
4. **Demolition Immunity & Food Decoupling**:
   - Periods 12–15: Food service completely shut off (`rate = Off`).
   - Food buffer drew down to 0, passenger bonus dropped to 1.0x baseline.
   - Zero house demolitions occurred; all 4 protected houses remained intact; population remained stable at 1,130 with zero decay.
5. **Cargo Conservation**:
   - 100% material cargo conservation verified across all 24 periods.
6. **Fresh-Process Cold Reload**:
   - Savegame at period 24 (`checkpoint-24.sav`) reloaded into an independent native process (`residential_reload_probe`).
   - 100% equality verified across tick, date, treasury, residential district state (`commissioned: true, placed: 4, completed: 4`), town population (1,130), and cargo ledger.

---

## 3. Verification Artifacts & Test Coverage

* **Automated Catch Suite**:
  - `src/tests/test_commonwealth_works.cpp` passed all 202 assertions across 8 test cases (`[commonwealth-works]`).
  - Full suite passed all 307,537 assertions across 538 test cases.
* **Full CTest Suite**: 553/553 tests passed (100%).
* **Linters & Quality Checks**: file-descriptions, unused-strings, and git diff checks clean.
* **Evidence Data**:
  - `docs/audit/2026-10-11/residential-district/evidence.json` (Full 24-period simulation ledger & reload proof)
  - `docs/audit/2026-10-11/residential-district/cold-start.sav` (Savegame at commissioning, SHA256: `ecae70878e02d3bb1bc75d06b6338b97500b65e12ec5845582f6e5200ecf83e8`)
  - `docs/audit/2026-10-11/residential-district/checkpoint-24.sav` (Savegame at period 24, SHA256: `0ff161ae33e9b25aa6ca3b97b0a88b5e28a50c8e030efcf7b312dbd4705307b2`)
