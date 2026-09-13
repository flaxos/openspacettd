# OpenSpaceTTD Sprint 27 — Feature UI Completion

Status: **COMPLETE**  
Date: **2026-09-13**  
Reference Specifications: [GAME_DESIGN_AND_TECHNICAL_PLAN.md](GAME_DESIGN_AND_TECHNICAL_PLAN.md), [FEATURE_UI_UAT_COVERAGE.md](FEATURE_UI_UAT_COVERAGE.md), [SPRINT26_CST_PREFAB_RAIL_BLOCKS_2026-09-13.md](SPRINT26_CST_PREFAB_RAIL_BLOCKS_2026-09-13.md)

---

## 1. Executive Summary

Sprint 27 closes the remaining player- and operator-facing interface gaps identified in the release coverage audit. All federation mechanics—previously operated via command-line debug consoles or daemon endpoints—are now natively integrated into responsive, accessible in-game graphical windows accessible from the main toolbar's Map menu.

### Key Deliverables:
1. **Empire Supply Chain Matrix & Trade Ledger Window (`TradeLedgerWindow`, `WindowClass::TradeLedger`):**
   - **Supply Chain Matrix Tab:** Visualizes developmental phase resource dependencies across the Commonwealth (Phase 3 Frontier $\to$ Phase 2 Refinery, Phase 2 Refinery $\to$ Phase 1 Megacity Core, direct sustenance feeds, and Core exports), alongside Spaceport interplanetary launch throughput and Edge Conduit bulk extraction metrics.
   - **Trade Ledger & Conservation Tab:** Real-time bilateral trade balances (credits, exports, imports) across all registered worlds, real-time commodity conservation status badge (`CONSERVED` vs `VIOLATION`), and per-cargo detailed transfer audits.
2. **Federation Authentication & Corporate Charters Window (`FederationAuthWindow`, `WindowClass::FederationAuth`):**
   - **Player Identity & Session State:** Displays active username, 128-bit `GlobalPlayerID`, and authorization token validation status.
   - **Interactive User Registration & Login:** Direct in-game account creation and authentication via query string dialogues with immediate feedback.
   - **Corporate Charter Management:** Multi-world corporate registry showing company names, 128-bit `GlobalCompanyID`, global treasuries, authorized operator delegates, and active planetary presences.
   - **Actionable Permission Enforcement:** Owner-exclusive delegate authorization/revocation, multi-world presence registration for the current planetary region, and clear error explanations for unauthorized or invalid operations.
3. **Map Dropdown Menu Integration (`src/toolbar_gui.cpp`):**
   - Added native entries `STR_MAP_MENU_TRADE_LEDGER` ("Supply Chain & Trade Ledger") and `STR_MAP_MENU_FEDERATION_AUTH` ("Federation Authentication & Charters") alongside Freight Corridors and Universe Directory.
4. **Full Test & Regression Verification:**
   - 73 assertions in 5 Catch2 test cases in `src/tests/test_sprint27_feature_ui.cpp`.
   - 227/227 CTests pass with 100% success rate, verifying WindowDesc registration, NWidget tree compilation, and business logic.

---

## 2. Interface Specifications

### 2.1 Empire Supply Chain Matrix & Trade Ledger

```text
┌───────────────────────────────────────────────────────────────────────────────────────┐
│ [X]            Empire Supply Chain Matrix & Trade Ledger                      [_][^][*]│
├───────────────────────────────────────────────────────────────────────────────────────┤
│ [ Supply Chain Matrix ]  [ Trade Balances & Conservation ]                 [ Refresh ]│
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Empire Total Interplanetary Flow: 142,500 units  |  Cumulative Tariffs: Cr 1,425,000   │
│ Ledger Status: CONSERVED (Zero Duplication/Loss) - Init: 142,500, Comp: 140,000, Trans:2,500│
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Empire Developmental Phase Distribution & Infrastructure Feeder Flows:                │
│   • Frontier -> Refinery (Phase 3 -> 2): 48,200 units [Raw Mineral & Biomass Feeder]  │
│   • Refinery -> Core (Phase 2 -> 1):     35,100 units [Refined Superalloys & Chem]    │
│   • Frontier -> Core (Phase 3 -> 1):     24,000 units [Direct Sustenance & Aggregates]│
│   • Core Megacity Exports:               35,200 units [Consumer Goods & High-Tech]    │
│                                                                                       │
│ Dedicated Extraction & Interstellar Feeder Facilities:                                │
│   • Spaceport Launch Infrastructure:      45,000 units                                │
│   • Edge Conduit Bulk Pipelines:          97,500 units                                │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Commonwealth Interplanetary Economy Architecture:                                     │
│ Phase 3 frontier worlds harvest raw resources feeding Phase 2 refineries; manufactured│
│ components supply Phase 1 megacities. Tariffs generate revenue at 10 Cr/unit.         │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Federation Player Authentication & Corporate Charters

```text
┌───────────────────────────────────────────────────────────────────────────────────────┐
│ [X]          Federation Player Authentication & Corporate Charters            [_][^][*]│
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Active Session: commander_shepard  |  Global Player ID: aaaa:1001  |  Token: [VALID]   │
│ Authenticated across Federation Universe Authority cluster.                           │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ [ Log In / Register ] [ Charter Corporation ] [ Register Presence ] [ Authorize Del ] │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Corporate Entity         Company ID       Owner Player     Treasury (Cr)  Presences   │
│ Commonwealth Freight     aaaa:2001        aaaa:1001        1,000,000 Cr   2 worlds    │
│ Stellar Logistics        bbbb:2002        bbbb:1002        1,000,000 Cr   1 worlds    │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Charter: 'Commonwealth Freight' | Owner: aaaa:1001 | Global Treasury: Cr 1,000,000    │
│ Active World Presences: World 1, World 2                                              │
│ Authorized Operator Delegates (1): aaaa:1002                                          │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ Ready. Authenticated as 'commander_shepard'.                                          │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Verification & Testing Evidence

All unit, GUI widget tree compilation, and regression tests pass with zero failures:
```bash
$ ./build/openttd_test [sprint27_ui]
Filters: [sprint27_ui]
===============================================================================
All tests passed (73 assertions in 5 test cases)

$ ctest --test-dir build --output-on-failure
...
100% tests passed, 0 tests failed out of 227
Total Test time (real) = 6.68 sec
```
