# Player-Funded Electronics Loop — Preflight Specification

## 1. Roadmap Context & Placement

This preflight establishes the concrete architectural, economic, and logistical blueprint
for the **Player-Funded Electronics Loop** (Horizon A, milestone A3 functional continuity),
which queues immediately after Core Construction Basket closure (PR #71 / PR #75).

- **Objective**: Establish the first advanced physical manufacturing chain in OpenSpaceTTD,
  transforming raw surveyed minerals (Copper Ore, Silica Sand) into intermediate components
  (Conductive Wiring, Silicon Chips) and advanced capital goods (PBS Signalling, Machine Modules).
- **Scope Isolation**: Disjoint from parallel visual art (Wormhole Gates, Phase-1 Fleets) and
  Horizon B multi-node federation. Fully deterministic and solo-playable on the contiguous multi-world map.

---

## 2. Research Unlocking Contract

- **Node**: `TECH_MATERIALS_2` (`TechID` 302, Materials Branch Level 2).
  - **Prerequisite**: `TECH_MATERIALS_1` (Blast Furnace Structural Steel).
  - **Cost**: 250 Research Points (RP). At canonical £1,000/RP, requires £250,000 player research spend.
  - **Authority**: Selected via `Commands::SelectResearchProject` (`src/portal/tech_tree.cpp`).
  - **Capabilities Unlocked**:
    1. Unlocks discovery and extraction of Copper Ore (`COPR`) and Silica Sand (`SAND`) resource sites.
    2. Enables Pipeline B manufacturing facilities (`RECIPE_COPPER_SMELTING`, `RECIPE_SILICON_ARC`).
    3. Enables electrified rail trackbed construction (`RAILTYPE_ELECTRIC`).

---

## 3. Upstream Surveying & Extraction

- **Resource Site Discovery**:
  - Primary extraction industries for `COPR` and `SAND` remain hidden until surveyed.
  - Survey command checks: `ResourceSiteManager::CanDiscover` requires `TechTreeManager::IsTechUnlocked(company, TECH_MATERIALS_2)`.
  - Survey grid: Player commissions 16×16 tile surveys (`ResourceSiteManager::SURVEY_SIZE = 16`).
- **World Placement Rules**:
  - `WorldPhase::Phase1_Core`: Extraction prohibited (protected metropolitan core).
  - `WorldPhase::Phase2_Developed`: Primary extraction allowed for copper and silica deposits.
  - `WorldPhase::Phase3_Frontier`: High-yield remote deposits.

---

## 4. Pipeline B Manufacturing Catalog

| Recipe ID | Facility Name | Inputs | Outputs | Allowed World Phases |
|---|---|---|---|---|
| `RECIPE_COPPER_SMELTING` | Electrolytic Copper Smelting | 2 Copper Ore (`COPR`) | 2 Conductive Wiring (`WIRE`) | `Phase2_Developed` |
| `RECIPE_SILICON_ARC` | High-Purity Silicon Arcology | 2 Silica Sand (`SAND`) | 1 Silicon Chips (`CHIP`) | `Phase2_Developed` |
| `RECIPE_SIGNALLING_ASSEMBLY` | Telemetry & PBS Signalling Works | 1 Silicon Chips (`CHIP`) + 1 Conductive Wiring (`WIRE`) | 2 Signalling Relays (`CHIP`) | `Phase2_Developed`, `Phase1_Core` |
| `RECIPE_MACHINE_MODULES` | Machine Module Assembly | 2 Structural Steel (`STEL`) + 1 Conductive Wiring (`WIRE`) + 1 Silicon Chips (`CHIP`) | 1 Machine Modules (`MACH`) | `Phase2_Developed` |

---

## 5. Downstream Demand & Infrastructure Impact

1. **Megacity Prosperity Demand**:
   - The Core Megacity panel tracks secondary prosperity goods alongside the base construction basket.
   - Delivery of Conductive Wiring and Silicon Chips unlocks Tier 2 prosperity bonuses and passenger generation multipliers.
2. **Electrified Rail Corridors**:
   - Availability of wiring allows constructing `RAILTYPE_ELECTRIC` track across planetary gates.
3. **Gateway Staging**:
   - Machine Modules (`MACH`) provide the physical capital input required for Gateway commissioning and multi-server freight staging.

---

## 6. Verification & Boundary Conditions

- **Resource Conservation**: Input cargo units must strictly equal output cargo plus documented slag loss; zero phantom cargo injection.
- **Server Authority**: All facility placements, surveys, and research selections must use replicated server-authoritative commands.
- **Deterministic Simulation**: All batch transformations occur on monthly ticks without floating-point arithmetic or desync-prone RNG calls.

---

## 7. Execution & Test Verification

The test harness and integration suite were implemented and verified in `src/tests/test_electronics_loop.cpp`:
- **`TECH_MATERIALS_2` Unlock Lifecycle**: Corporate HQ establishment, prerequisite gating, £250k / 250 RP incremental funding, and unlocked tech protection.
- **Resource Site Discovery & Mapping**: Primary extraction validation for `COPR` and `SAND`.
- **Pipeline B Batch Conservation**: Exact material balance across `RECIPE_COPPER_SMELTING` (2 Copper -> 2 Wire), `RECIPE_SILICON_ARC` (2 Sand -> 1 Chips), and `RECIPE_SIGNALLING_ASSEMBLY` (1 Wire + 1 Chips -> 2 Relays).
- **Core Megacity Acceptance**: Classification of `WIRE` (Tier 2 Expansion) and `CHIP` (Tier 3 Prosperity) and verification of multi-tier delivery satisfaction.

**Results**:
- Catch2 test suite: 4/4 test cases passed (61 assertions).
- CTest suite: 528/528 passed (100%).
- Repository linters: `.github/file-descriptions.py` and `.github/unused-strings.py` passed (OK).
- `git diff --check`: 0 errors.
