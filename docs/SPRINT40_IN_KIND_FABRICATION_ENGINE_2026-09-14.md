# Sprint 40: In-Kind Fabrication Engine & Bill of Materials (BOM)

**Status:** COMPLETE  
**Date:** 2026-09-14  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Test Suite:** `src/tests/test_sprint40_fabrication_engine.cpp`  

> **Evidence boundary:** In-kind fabrication mode toggle, Bill of Materials (BOM) recipe registry, command interception for rail, signal, depot, and train vehicle construction with 80% discount and physical stockpile deduction, GUI integration in Corporate HQ window, save/load serialization (`FABR` chunk tag), and automated test suites have native C++ unit test coverage and are verified (5/5 test cases, 76 assertions passing cleanly). Full CTest suite runs at 270/270 passing.

---

## 1. Executive Summary

Sprint 40 delivers the **In-Kind Fabrication Engine & Bill of Materials (BOM)** subsystem, unlocking a core gameplay loop envisioned in `docs/MID_TO_LATE_GAME_CORPORATE_TECH_SPIKE.md`: allowing logistics tycoons to construct infrastructure and rolling stock directly out of their accumulated planetary stockpiles rather than relying purely on commercial cash outlays.

Key accomplishments in Sprint 40:

1. **Bill of Materials (BOM) Domain Model (`src/portal/fabrication_manager.h`, `src/portal/fabrication_manager.cpp`):**
   - Implemented `BillOfMaterials` structure defining required quantities per `CargoType` or `FabricationRole` and standard discount percentages (default 80% labor discount).
   - Implemented `FabricationManager` providing authoritative recipe catalogs:
     - **Track Infrastructure:** Normal Rail (2 Ballast, 1 Metal), Electrified Rail (2 Ballast, 1 Metal, 1 Wiring), Monorail (4 Ballast, 2 Metal, 1 Wiring), Maglev (2 Superalloy, 2 Wiring, 1 Electronics).
     - **Signalling Equipment:** 1 Metal, 1 Wiring.
     - **Depot Facilities:** Standard Rail/Electric Depot (10 Metal, 5 Ballast), Monorail Depot (15 Metal, 8 Ballast, 2 Wiring), Maglev Depot (15 Superalloy, 8 Ballast, 4 Wiring, 2 Electronics).
     - **Rolling Stock (Trains & Wagons):**
       - Steam Engines: 30 Metal, 10 Ballast.
       - Diesel Locomotives: 40 Metal, 15 Wiring.
       - Electric Locomotives: 35 Superalloy, 25 Wiring, 10 Electronics.
       - Monorail Locomotives: 40 Superalloy, 25 Wiring, 15 Electronics.
       - Maglev Locomotives: 50 Superalloy, 30 Wiring, 20 Electronics.
       - Standard Freight/Passenger Wagons: 10 Metal, 2 Composites.
       - High-Speed/Maglev/Monorail Wagons: 15 Superalloy, 5 Composites.

2. **Dual-Mode Construction Toggle & Command (`src/portal/fabrication_manager.h`, `src/portal/portal_cmd.h`, `src/portal/portal_cmd.cpp`, `src/command_type.h`):**
   - Company-isolated setting: "Commercial Cash" (default) vs. "Fabricate from Stockpile".
   - Server-authoritative command `Commands::SetFabricationMode` with `CommandType::CompanySetting` trait to ensure multiplayer lockstep synchronization.

3. **Core Engine Construction Interception:**
   - **Track Laying (`src/rail_cmd.cpp` - `CmdBuildSingleRail`):**
     - Resolves the world hosting the construction tile.
     - If fabrication mode is active: verifies BOM availability in local planetary stockpile. Rejects placement with `STR_ERROR_INSUFFICIENT_STOCKPILE_MATERIALS` if insufficient.
     - On execution: deducts required materials from the company's world stockpile and reduces the rail build cost by 80% (20% nominal labor fee).
   - **Signalling (`src/rail_cmd.cpp` - `CmdBuildSingleSignal`):**
     - Validates and deducts signal hardware BOM, applying 80% discount.
   - **Train Depots (`src/rail_cmd.cpp` - `CmdBuildTrainDepot`):**
     - Validates and deducts depot structural BOM, applying 80% discount across both depot structure and track bed.
   - **Rolling Stock (`src/vehicle_cmd.cpp` - `CmdBuildVehicle`):**
     - For train locomotives and wagons: validates engine class and type against the local world stockpile where the depot resides.
     - Deducts vehicle BOM and reduces purchase cost by 80%.

4. **In-Game Corporate HQ GUI Integration (`src/portal/corporate_hq_gui.cpp`, `src/widgets/corporate_hq_widget.h`):**
   - Added new **"In-Kind Fabrication"** tab (`WID_CHQ_TAB_FABRICATION`) to `CorporateHQWindow`.
   - Interactive toggle button (`WID_CHQ_FABRICATION_TOGGLE`) to switch company fabrication mode.
   - Displays live status: active mode indicator, material stockpile requirements, and comprehensive BOM catalog.

5. **Save/Load Persistence (`src/saveload/planet_sl.cpp`):**
   - Implemented `FABRChunkHandler` under the `FABR` chunk tag to serialize and restore each company's fabrication mode flag across savegame cycles.

6. **Automated Verification (`src/tests/test_sprint40_fabrication_engine.cpp`):**
   - 5 comprehensive Catch2 test cases with 76 assertions covering:
     - BOM recipe registry and catalog calculations.
     - Mode toggling, company isolation, and command execution.
     - Track building interception (cash mode vs. fabrication failure vs. discounted success).
     - Signal and depot building interception and stockpile consumption.
     - Save/load serialization round-trip.
   - Full regression suite of 270 CTest cases passing 100%.
