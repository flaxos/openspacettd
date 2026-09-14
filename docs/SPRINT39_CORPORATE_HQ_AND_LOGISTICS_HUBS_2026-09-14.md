# Sprint 39: Corporate Headquarters, Planetary Stockpiles & Logistics Hubs

**Status:** COMPLETE  
**Date:** 2026-09-14  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Test Suite:** `src/tests/test_sprint39_corporate_hq_and_stockpile.cpp`  

> **Evidence boundary:** Corporate Headquarters placement rules, multi-world company stockpile ledgers, dedicated logistics hubs with reserve floors, and bi-directional cargo buffering have native C++ unit test coverage and are automated verified (7/7 tests, 108 assertions passing cleanly). Full CTest suite runs at 265/265 passing.

---

## 1. Executive Summary

Sprint 39 introduces the **Corporate Headquarters & Dedicated Logistics Hubs** subsystem, realizing the mid-to-late game vision bridging OpenTTD network transport with Factorio / Captain of Industry macro-logistics and Peter F. Hamilton's Commonwealth saga (`docs/MID_TO_LATE_GAME_CORPORATE_TECH_SPIKE.md`).

Key accomplishments in Sprint 39:

1. **Corporate Headquarters Campus (`src/portal/corporate_hq.h`, `src/portal/corporate_hq.cpp`):**
   - Implemented `CorporateHQManager` managing company-level headquarters profiles across worlds.
   - Enforced founding restrictions:
     - Must be constructed within an active Phase 1 Core world (`STR_ERROR_CANNOT_BUILD_HQ_NOT_CORE_WORLD`).
     - Requires minimum company capital of 5,000,000 Cr (`STR_ERROR_CANNOT_BUILD_HQ_INSUFFICIENT_FUNDS`).
     - Requires established company operational presence spanning at least 3 distinct world phases (`STR_ERROR_CANNOT_BUILD_HQ_INSUFFICIENT_PRESENCE`).
     - Restricts each company to a single authoritative Corporate HQ (`STR_ERROR_ALREADY_HAS_CORPORATE_HQ`).
   - Supports 4 evolutionary campus tiers: `RegionalBranch` $\to$ `PlanetaryHQ` $\to$ `Interstellar` $\to$ `CST_Arcology`.

2. **Company-Owned Planetary Stockpile Ledger (`src/portal/company_stockpile.h`, `src/portal/company_stockpile.cpp`):**
   - Implemented `StockpileManager` tracking company inventories per world (`CompanyWorldStockpile`).
   - Defined `FabricationRole` enum for infrastructure manufacturing roles (Ballast, StructuralMetal, Wiring, Electronics, Superalloy, Composites, BlankCrystals, EnrichedCrystals) mapped to default game cargos.
   - Implemented Bill of Materials (BOM) validation (`HasSufficient`) and atomic consumption (`ConsumeBOM`).

3. **Dedicated Logistics Hubs & Bi-Directional Warehouses (`src/portal/logistics_hub.h`, `src/portal/logistics_hub.cpp`):**
   - Implemented `LogisticsHubManager` tracking dedicated company warehouses adjacent to stations.
   - **Ingest to Stockpile:** Train consists delivering freight to an attached station deposit goods into the company's regional stockpile via `DeliverGoods` hook in `src/economy.cpp`.
   - **Surplus Extraction with Reserve Floors:** Train consists loading at an attached station can withdraw surplus cargo above a configurable per-cargo `reserve_floor` via `LoadUnloadVehicle` hook in `src/economy.cpp`. Cargo below the reserve threshold remains strictly locked in storage for planetary fabrication, R&D, and local consumption.

4. **In-Game Corporate HQ GUI (`src/portal/corporate_hq_gui.h`, `src/portal/corporate_hq_gui.cpp`, `src/widgets/corporate_hq_widget.h`):**
   - Integrated `CorporateHQWindow` with multi-tab layout:
     - **Overview Tab:** Campus name, current tier badge, founding world, and operating capital.
     - **Planetary Stockpiles Tab:** Multi-world inventory ledger matrix displaying stockpiled tons across all worlds.
     - **Logistics Hubs Tab:** Registered hub stations, configured reserve floors, and cumulative deposit/dispatch throughput metrics.
   - Integrated into the main map toolbar menu under "Corporate HQ & Stockpiles".

5. **Save/Load Persistence (`src/saveload/planet_sl.cpp`):**
   - Implemented `STCKChunkHandler` for `CompanyWorldStockpile` inventories (`STCK` chunk tag).
   - Implemented `LHUBChunkHandler` for `LogisticsHub` records and configurable reserve floors (`LHUB` chunk tag).
   - Implemented `CHQSChunkHandler` for `CorporateHQProfile` instances (`CHQS` chunk tag).

6. **Commands & Net-Safe Authoritative Actions (`src/portal/portal_cmd.h`, `src/portal/portal_cmd.cpp`, `src/command_type.h`):**
   - `Commands::PlaceCorporateHQ`: Validates prerequisites, constructs campus, broadcasts news notification.
   - `Commands::BuildLogisticsHub`: Auto-associates with adjacent stations, establishes dedicated warehouse buffer.

7. **Automated Verification:**
   - 7 Catch2 unit tests in `src/tests/test_sprint39_corporate_hq_and_stockpile.cpp` with 108 assertions passing 100%.
   - Full regression suite of 265 CTest cases passing cleanly.
