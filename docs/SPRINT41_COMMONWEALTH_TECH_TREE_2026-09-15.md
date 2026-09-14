# Sprint 41 — In-Lore Commonwealth Tech Tree & R&D Projects

**Date:** 2026-09-15  
**Status:** Completed and Verified  
**Milestone:** Commonwealth R&D Progression & Corporate HQ Technology Trees  
**Test Coverage:** 6 test cases, 77 assertions in `src/tests/test_sprint41_tech_tree.cpp`; 285/285 tests passing project-wide in CTest (100%).

---

## 1. Executive Summary

Sprint 41 replaces OpenTTD's legacy passive calendar-year vehicle introductions with an active, continuous, research-driven progression model grounded in Peter F. Hamilton's *Commonwealth Saga*. 

In the Commonwealth universe, railway expansion, wormhole engineering, and planetary colonization are driven by massive corporate R&D efforts spearheaded by CST (Commonwealth Stephenson Transport) and competitive megacorporations. Companies conduct active research programs from their planetary Corporate Headquarters, funding labs with both liquid capital and physical high-tech feedstocks—specifically **Enriched Quantum Data Crystals** containing complex mathematical singularity proofs from frontier observatories and **High-Tech Electronics**.

This sprint delivers:
1. `TechTreeManager` domain model with 3 canonical lore branches and 12 Tier 1–4 technologies.
2. Corporate HQ eligibility gates and prerequisite directed acyclic graph (DAG) enforcement.
3. Continuous monthly R&D simulation loop (`TechTreeManager::ProcessMonthlyResearch()`) integrated into `_economy_spaceports_conduits_monthly`.
4. Dual-input RP funding combining monthly cash budgets with physical feedstock consumption from the Corporate HQ world stockpile.
5. In-Kind Fabrication synergy: unlocking `TECH_MATERIALS_3` upgrades the material fabrication discount from 80% to 90% (reducing the cash labor fee to 10%).
6. Server-authoritative multiplayer commands: `Commands::SelectResearchProject` and `Commands::SetResearchBudget`.
7. Full save/load persistence via the `TECH` table chunk in `src/saveload/planet_sl.cpp`.
8. Corporate HQ GUI integration: 5th tab `WID_CHQ_TAB_TECH_TREE` with visual tree progress, feedstock status, budget controls, and project selection.

---

## 2. Tech Tree Architecture & Canonical Technologies

The Commonwealth Tech Tree is partitioned into three specialized branches reflecting the core pillars of the Commonwealth transport network:

```
                          [Corporate Headquarters]
                                     |
    +--------------------------------+-------------------------------+
    |                                |                               |
[Traction & Propulsion]   [Wormhole & Portal Physics]    [Materials & Fabrication]
    |                                |                               |
 Tier 1: CST Heavy Freight        Tier 1: Singularity Containment  Tier 1: Composite Aerobodies
         Dynamos (100 RP)                 Arrays (150 RP)                  (100 RP)
    |                                |                               |
 Tier 2: Cryo Superconducting     Tier 2: Dual-Track Throat        Tier 2: Superalloy Metallurgy
         Bogies (250 RP)                  Arrays (300 RP)                  (250 RP)
    |                                |                               |
 Tier 3: Maglev Guideway          Tier 3: Ozzie-Sheldon Resonance  Tier 3: Automated Nanofab
         Systems (600 RP)                 Stabilizers (750 RP)             Lines (600 RP)
    |                                |                               |
 Tier 4: Inertial Damping         Tier 4: Direct-Transit Wormhole  Tier 4: Void Monocrystalline
         Drives (1,500 RP)                Conduits (2,000 RP)              Infrastructure (1,500 RP)
```

### Technology Matrix

| ID | Name | Branch | Tier | Cost | Prerequisites | In-Lore Gameplay Effect |
|---|---|---|:---:|:---:|---|---|
| `TECH_TRACTION_1` | CST Heavy Freight Dynamos | Traction | 1 | 100 RP | None | High-torque electric freight locomotives |
| `TECH_TRACTION_2` | Cryo-Cooled Superconducting Bogies | Traction | 2 | 250 RP | `TECH_TRACTION_1` | High-speed electric and maglev traction bogies |
| `TECH_TRACTION_3` | Maglev Guideway Systems | Traction | 3 | 600 RP | `TECH_TRACTION_2` | High-speed maglev infrastructure & passenger expresses |
| `TECH_TRACTION_4` | Inertial Damping Drives | Traction | 4 | 1,500 RP | `TECH_TRACTION_3` | Ultra-heavy trans-planetary mega-freighters |
| `TECH_PORTAL_1` | Singularity Containment Arrays | Portal Physics | 1 | 150 RP | None | Unlocks basic inter-planetary wormhole gates |
| `TECH_PORTAL_2` | Dual-Track Throat Arrays | Portal Physics | 2 | 300 RP | `TECH_PORTAL_1` | Dual-track portal gates with higher transit capacity |
| `TECH_PORTAL_3` | Ozzie-Sheldon Resonance Stabilizers | Portal Physics | 3 | 750 RP | `TECH_PORTAL_2` | Maximum portal stability & cross-system linking |
| `TECH_PORTAL_4` | Direct-Transit Wormhole Conduits | Portal Physics | 4 | 2,000 RP | `TECH_PORTAL_3` | Bulk-cargo continuous edge conduit manifolds |
| `TECH_MATERIALS_1` | Composite Aerobodies | Materials | 1 | 100 RP | None | Lightweight rolling stock shells & reduced running costs |
| `TECH_MATERIALS_2` | Superalloy Metallurgy | Materials | 2 | 250 RP | `TECH_MATERIALS_1` | High-strength bridge girders & deep-bore tunnels |
| `TECH_MATERIALS_3` | Automated Nanofabrication Lines | Materials | 3 | 600 RP | `TECH_MATERIALS_2` | Upgrades in-kind fabrication discount from 80% to 90% |
| `TECH_MATERIALS_4` | Void-Hardened Monocrystalline Infra | Materials | 4 | 1,500 RP | `TECH_MATERIALS_3` | Vacuum and hazardous frontier trackbed stability |

---

## 3. R&D Simulation & Feedstock Mechanics

Research progression occurs deterministically on the monthly economy cycle inside `_economy_spaceports_conduits_monthly` in `src/economy.cpp`:

```cpp
void TechTreeManager::ProcessMonthlyResearch()
```

### Research Eligibility
1. **Corporate Headquarters Requirement:** A company must have placed an active Corporate HQ (`CorporateHQManager::HasHQ(company)`) on a Phase 1 Core world.
2. **Prerequisites:** All prerequisite technologies in the tree branch must be researched before a project can be activated.
3. **Active Project:** The company must have selected a valid project (`state.active_project != TECH_NONE`).

### Dual-Input RP Calculation
Monthly Research Points (RP) are derived from two distinct sources:
$$\text{Monthly RP} = \text{RP}_{\text{Cash}} + \text{RP}_{\text{Crystals}} + \text{RP}_{\text{Electronics}}$$

1. **Cash Budget ($\text{RP}_{\text{Cash}}$):**
   - Funded from company liquid cash up to `monthly_budget`.
   - Rate: $1\text{ RP}$ per $1,000\text{ Cr}$ deducted directly from `Company::money`.
   - Budget presets: $\$0$, $\$10\text{k}$ (10 RP), $\$25\text{k}$ (25 RP), $\$50\text{k}$ (50 RP), $\$100\text{k}$ (100 RP), $\$250\text{k}$ (250 RP).

2. **Enriched Quantum Data Crystals Feedstock ($\text{RP}_{\text{Crystals}}$):**
   - Imprinted quantum data crystals (`CT_MAIL` / `FabricationRole::EnrichedCrystals`) stored at the Corporate HQ world stockpile.
   - Rate: $10\text{ RP}$ per crystal unit consumed.
   - Cap: Max 5 units burned per month (up to $+50\text{ RP}/\text{mo}$).

3. **High-Tech Electronics Feedstock ($\text{RP}_{\text{Electronics}}$):**
   - Precision microchips and telemetry processors (`CT_VALUABLES` / `FabricationRole::Electronics`) stored at the Corporate HQ world stockpile.
   - Rate: $5\text{ RP}$ per electronics unit consumed.
   - Cap: Max 10 units burned per month (up to $+50\text{ RP}/\text{mo}$).

### Fabrication Synergy
When `TECH_MATERIALS_3` (Automated Nanofabrication Lines) is researched:
- `FabricationManager::GetBOMDiscountPercent(company)` dynamically changes from $80\%$ to $90\%$.
- Track, depot, signal, and train vehicle construction commands automatically adjust the required cash labor fee from $20\%$ down to $10\%$.

---

## 4. Server-Authoritative Commands

Deterministic multiplayer lockstep execution is ensured via two dedicated commands:

1. **`Commands::SelectResearchProject` (`CmdSelectResearchProject`):**
   - Validates HQ presence and prerequisite compliance.
   - Updates `active_project` for `_current_company`.
   - CommandType: `CommandType::CompanySetting`.

2. **`Commands::SetResearchBudget` (`CmdSetResearchBudget`):**
   - Updates `monthly_budget` for `_current_company`.
   - CommandType: `CommandType::CompanySetting`.

---

## 5. Save/Load Persistence (`TECH` Chunk)

Savegame compatibility is maintained with a table chunk handler registered in `src/saveload/planet_sl.cpp`:

```cpp
struct TECHChunkHandler : ChunkHandler {
    TECHChunkHandler() : ChunkHandler("TECH", ChunkType::Table) {}
    ...
};
```

- Tag: `"TECH"`.
- Supports records of type `kind=0` (Company R&D state: active project, accumulated RP, monthly budget) and `kind=1` (Unlocked technology node set).

---

## 6. GUI Integration

A dedicated tab has been added to the Corporate HQ window (`src/portal/corporate_hq_gui.cpp`):
- Tab 5: `WID_CHQ_TAB_TECH_TREE` ("Commonwealth Tech Tree").
- Header displays active research project, progress bar, accumulated RP vs target, and completion percentage.
- Stockpile burn indicator reports available and burning Enriched Crystals and Electronics at the HQ world.
- Interactive branch tree displays Tier 1–4 nodes with visual state tags: `[COMPLETED]`, `[IN PROGRESS]`, `[AVAILABLE]`, or `[LOCKED]`.
- Action buttons allow starting research and cycling the monthly R&D cash budget ($0 to $250k/mo).

---

## 7. Verification & Acceptance

Automated verification suite `src/tests/test_sprint41_tech_tree.cpp` validates all functionality:
1. `Sprint 41 Tech Tree - Catalog & Branches`: 12 nodes across 3 branches, cost scales, and prerequisite DAG.
2. `Sprint 41 Tech Tree - Corporate HQ Eligibility & Prerequisites`: HQ requirement and strict prerequisite gating.
3. `Sprint 41 Tech Tree - Server Commands & Budget Allocation`: Deterministic server commands and budget assignment.
4. `Sprint 41 Tech Tree - Monthly Progression & Feedstock Burning`: Monthly RP accumulation, cash deduction, crystal/electronics feedstock burning, and project completion.
5. `Sprint 41 Tech Tree - Fabrication Bonus Discount Integration`: Automated upgrade to 90% BOM discount upon unlocking `TECH_MATERIALS_3`.
6. `Sprint 41 Tech Tree - Save/Load State Restoration`: Serialization and exact restoration of research state and unlocked node sets.

**Test Run Results:**
```
ninja -C build openttd_test && ./build/openttd_test "Sprint 41*"
All tests passed (77 assertions in 6 test cases)

ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 285
Total Test time = 7.74 sec
```
