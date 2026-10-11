# A World Worth Connecting (Sprint 3) Audit — 11 October 2026

Native simulation audit and proof for Sprint 3 ("A World Worth Connecting") of the Commonwealth design study on isolated branch `codex/capacity-or-reserve-pilot`.

---

## 1. Concrete Source / Link / Consumer Objective & Architecture Proof

1. **Concrete Industrial Flow & Geometry**:
   - **Consumer**: Ballast Crusher (`Node 4`, `TileXY(300, 80)`, World 1 / Industrial Commonwealth) consuming raw Silica (`SILC`) to produce Ballast (`BALL`) for HQ stockpiles.
   - **Primary Source (World 2)**: Silica extraction at `Node 13` (`TileXY(650, 80)`, World 2 / Extraction Frontier) delivering across the shared World 2 ↔ World 1 throat on row 82 (`TileXY(534, 82)` ↔ `TileXY(476, 82)`). Track length: 294 tiles.
   - **Alternative Source (World 3)**: Independent Silica extraction on World 3 (`Research Frontier`, `WorldID{3}`, Phase 3 Frontier, `TileXY(860, 96)`), discovered via paid company surveying (`Commands::SurveyResources`), funded via native `Commands::BuildIndustry`.
   - **Independent Wormhole Link**: Direct portal pair constructed via server-authoritative `Commands::BuildPortalPair` directly linking World 3 (`TileXY(790, 96)`) and World 1 (`TileXY(476, 96)`), running into a dedicated platform at Ballast Crusher (Stop 4) on row 96. Track length: 248 tiles (~15.6% shorter route than World 2).
   - **Bottleneck Independence**: The World 3 direct portal pair completely bypasses World 2 and does NOT share the World 2 ↔ World 1 throat on row 82.

2. **Genuine Capital Costs & Treasury Debits**:
   - The connection required real company capital debits across all construction phases (£5,095,387 total spent):
     * Area Surveying: £60,000 (`Commands::SurveyResources`)
     * Industry Founding: £4,921,875 (`Commands::BuildIndustry`)
     * Extraction Station (World 3): £1,365 (`Commands::BuildRailStation`)
     * Consumer Redundant Platform (World 1): £1,093 (`Commands::BuildRailStation`)
     * Direct Portal Pair (W1 ↔ W3): £13,532 (`Commands::BuildPortalPair`)
     * Rail Depot: £540 (`Commands::BuildRailDepot`)
     * Freight Consist (Loco + 6 SILC wagons): £76,657 (`Commands::BuildVehicle` & `Commands::MoveRailVehicle`)
   - Company treasury debited from £98,396,121 to £93,300,734 during commissioning, verified in simulation logs.

3. **Selective Corridor Control & Resilience**:
   - Selective control of the primary throat corridor via `connected_economy close-primary-corridor` and `connected_economy reopen-primary-corridor`.
   - When the primary corridor is closed, Train 1 is stopped, halting deliveries from World 2.
   - Redundant World 3 train (`Train 9` / Consist 46) continues operating unaffected, keeping Ballast Crusher fed and maintaining production without starvation.

---

## 2. Headless Native Simulation Evidence (24 Periods)

Headless native 24-period simulation runner (`scripts/verify_commonwealth_expansion.py`) was executed on a fresh scenario map:

1. **Periods 1–6 (Baseline Operation)**:
   - Primary corridor (Train 1) delivers SILC from World 2 to Ballast Crusher.
   - Ballast Crusher batches advance, feeding BALL to HQ warehouse.
   - Treasury stabilizes at £98,324,397 by Period 6.
2. **Period 7 (Alternative World Connection)**:
   - Executed `connected_economy connect-world`.
   - World 3 resource surveyed, industry founded, direct portal pair built, rails laid on row 96, platform added at Stop 4, and Train 9 dispatched.
   - Real capital expenditure of £5,095,387 debited from Company 0 treasury.
3. **Periods 8–12 (Dual-Source Operation)**:
   - Both World 2 (Train 1) and World 3 (Train 9) deliver SILC to Ballast Crusher.
   - Route cycle times verified: World 3 route is 248 tiles vs World 2 route of 294 tiles (15.6% shorter).
4. **Periods 13–18 (Deliberate Primary Throat Interruption)**:
   - Executed `connected_economy close-primary-corridor` (Train 1 stopped).
   - Primary World 2 delivery halts.
   - Ballast Crusher production rate is sustained via World 3 deliveries, maintaining continuous production and town growth. Zero starvation collapse.
5. **Periods 19–24 (Corridor Recovery & Restored Combined Flow)**:
   - Executed `connected_economy reopen-primary-corridor` (Train 1 restarted).
   - Dual-source flow resumes; Ballast Crusher accepts both corridors.
   - 100% material cargo conservation verified across all 24 periods.
6. **Fresh-Process Cold Reload & Continuity Probe**:
   - Savegame at period 24 (`checkpoint-24.sav`) reloaded in a fresh engine process (`expansion_reload_probe`).
   - 100% exact equality verified across tick, date, treasury, cargo inventory, facility states, and world expansion status (`connected: true, primary_corridor_closed: false`).
   - 4 additional post-reload periods advanced with 100% cargo conservation.

---

## 3. Verification Artifacts & Test Coverage

* **Automated Catch Suite**:
  - `src/tests/test_commonwealth_works.cpp` passed all 229 assertions across 9 test cases (`[commonwealth-works]`).
  - Full suite passed all 307,564 assertions across 539 test cases (`./build/openttd_test`).
* **Full CTest Suite**: 554/554 tests passed (100%).
* **Linters & Quality Checks**:
  - `python3 .github/file-descriptions.py` — PASSED.
  - `python3 .github/unused-strings.py` — PASSED.
  - `git diff --check` — PASSED (clean).
* **Evidence Data**:
  - `docs/audit/2026-10-11/world-connection/evidence.json` (Full 24-period simulation ledger & reload proof)
  - `docs/audit/2026-10-11/world-connection/cold-start.sav` (Savegame at scenario start, SHA256: `0b501b19c45ee8c83c02c204f155320592e5b6d8083baac2ad50832b4b46717c`)
  - `docs/audit/2026-10-11/world-connection/checkpoint-24.sav` (Savegame at period 24, SHA256: `2536cdbb51d9130a8c8d89ddf23908888c9ded746e0b1b604284d37668264829`)
