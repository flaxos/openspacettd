# Sprint 33: Planetary Town Growth, Megacity Supply Loops, and Biome-Specific Industry Lifecycle

**Status:** COMPLETE  
**Date:** 2026-09-14  
**Branch:** `fix/portal-gate-lifecycle-crashes`  
**Test Suite:** `src/tests/test_sprint33_planetary_economy_and_megacity.cpp`  
**Acceptance Kit:** `scripts/test_sprint33_megacity_kit.py`  

---

## 1. Executive Summary

Sprint 33 delivers the **Planetary Town Growth, Megacity Supply Loops, and Biome-Specific Industry Lifecycle** for OpenSpaceTTD. This sprint closes the crucial gameplay and economic feedback loop between planetary colonization, 6-biome taxonomy, railway logistics, and demographic growth. Stations delivering food, consumer goods, and high-tech luxuries now directly fuel Megacity development and passenger generation, while hostile biomes (such as Volcanic worlds) restrict incompatible bio-industries like farms.

Key accomplishments in Sprint 33:

1. **Town Founding Protection (`PlanetManager::CheckTownPlacement` in `src/town_cmd.cpp`):**
   - Inter-planetary void buffer space is strictly protected against town founding (`STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE`).
   - Uncolonized Phase 4 Expansion wilderness worlds prohibit manual town placement until an expeditionary colonial outpost is founded (`STR_ERROR_CANNOT_BUILD_ON_EXPANSION_WORLD`).
   - Unified enforcement within `TownCanBePlacedHere` guarantees protection across player commands, AI scripts, and world generation.

2. **Megacity Supply Loop & Monthly Demand Evaluation (`src/economy.cpp`):**
   - Connected station cargo deliveries in `DeliverGoods` to `MegacityManager::RecordDeliveryByCargo(town_id, cargo_type, accepted_total)`.
   - Mapped cargo types to Megacity demand tiers (Tier 1 Sustenance: Food/Water; Tier 2 Expansion: Goods; Tier 3 Prosperity: Valuables/Diamonds).
   - Hooked `MegacityManager::EvaluateMonthlySupply()` into `_economy_spaceports_conduits_monthly` to synchronously evaluate satisfaction ratios and reset monthly delivery quotas.

3. **Town Growth & Passenger Generation Multipliers (`src/town_cmd.cpp`):**
   - In `UpdateTownGrowthRate`, applied integer-deterministic scaling from `MegacityManager::GetGrowthMultiplier(t->index)`:
     - `Starvation` ($0.0\times$): Sets `t->growth_rate = TOWN_GROWTH_RATE_NONE`.
     - `Subsistence` ($1.0\times$): Normal baseline growth.
     - `MetropolitanBoom` ($1.5\times$): Accelerates growth rate by dividing tick interval by $1.5$.
     - `HyperGrowth` ($2.0\times$): Doubled growth rate (tick interval halved).
   - In `UpdateTownGrowth`, halts growth when `Starvation` is active, preserving town size until supply is restored.
   - In `TownGenerateCargo`, scaled passenger and mail generation by `MegacityManager::GetPassengerMultiplier(t->index)` ($0.5\times$ in Starvation up to $1.5\times$ in HyperGrowth).

4. **Colonial Outpost Town & Industry Inception (`src/portal/portal_cmd.cpp`):**
   - In `CmdColonizeOutpost`, automatically spawns a frontier outpost settlement on the newly colonized world if no town exists.
   - In `CmdPromoteWorld`, automatically registers the world's primary settlement as a Megacity with `MegacityManager` upon promotion to Phase 1 Core.

5. **Biome-Specific Industry Lifecycle Protection (`src/industry_cmd.cpp`, `src/portal/planet_manager.cpp`):**
   - Enforced environmental restrictions preventing organic farms and plantations from being founded on hostile Volcanic worlds (`STR_ERROR_CANNOT_BUILD_FARM_ON_VOLCANIC_WORLD`).
   - Preserved mineral extraction and heavy geothermal processing facilities on Volcanic worlds.

6. **Universe Directory GUI Enhancements (`src/portal/universe_directory_gui.cpp`, `src/widgets/universe_directory_widget.h`):**
   - Added `View Megacity` button (`WID_UD_MEGACITY_BTN`) to inspect Megacity demand curves and satisfaction tiers via `ShowMegacityOverview(town_id)`.
   - Rendered real-time population metrics and growth status badges (`[Starvation]`, `[Subsistence]`, `[Boom 1.5x]`, `[HyperGrowth 2.0x]`).

7. **Universe Authority Federation REST API & Checkpoint Persistence:**
   - Added demographics tracking (`population`, `is_megacity`, `megacity_growth_state`, `satisfaction_pct`) to `RegisteredWorld` in C++ and Python.
   - Implemented `GET /worlds/<id>/megacity` and `POST /worlds/<id>/megacity` REST endpoints.
   - Guaranteed full checkpoint crash recovery across `--state-file` saves and loads.

8. **Comprehensive Verification:**
   - 5 Catch2 unit tests in `src/tests/test_sprint33_planetary_economy_and_megacity.cpp`.
   - 5 multi-server acceptance scenarios in `scripts/test_sprint33_megacity_kit.py` passing 100%.

---

## 2. Megacity Demand Tiers & Multiplier Matrix

| Growth State | Demand Condition | Growth Multiplier | Passenger Multiplier | Town Growth Effect |
|---|---|---|---|---|
| **Starvation** | Tier 1 (Food) $< 50\%$ | $0.0\times$ | $0.5\times$ | Completely halted (`TOWN_GROWTH_RATE_NONE`) |
| **Subsistence** | Baseline or partial Tier 2 | $1.0\times$ | $1.0\times$ | Normal standard growth |
| **Metropolitan Boom** | Tier 1 & Tier 2 $\ge 100\%$ | $1.5\times$ | $1.25\times$ | $1.5\times$ faster growth rate |
| **Hyper Growth** | Tiers 1, 2, & 3 $\ge 100\%$ | $2.0\times$ | $1.5\times$ | $2.0\times$ accelerated growth |

---

## 3. Biome Industry Construction Rules

| Biome | Raw Extraction | Processing Facilities | Bio-Farms & Plantations | Restrictions |
|---|---|---|---|---|
| **Temperate** | Allowed (Phase $\le 3$) | Allowed (Phase $\le 2$) | Allowed | Standard Commonwealth rules |
| **Arid Desert** | Allowed (Phase $\le 3$) | Allowed (Phase $\le 2$) | Allowed (requires water) | Standard Commonwealth rules |
| **Sub-Arctic** | Allowed (Phase $\le 3$) | Allowed (Phase $\le 2$) | Allowed (in green regions)| Standard Commonwealth rules |
| **Sub-Tropic** | Allowed (Phase $\le 3$) | Allowed (Phase $\le 2$) | Allowed | Standard Commonwealth rules |
| **Volcanic** | Allowed (Phase $\le 3$) | Allowed (Phase $\le 2$) | **Prohibited** | `STR_ERROR_CANNOT_BUILD_FARM_ON_VOLCANIC_WORLD` |
| **Oceanic** | Offshore Only | Coastal / Platforms | Aquaculture only | Marine zoning rules |

---

## 4. Verification Commands

```bash
# Compile OpenSpaceTTD and test runner
ninja -C build openttd openttd_test

# Run Sprint 33 unit tests
./build/openttd_test "Sprint 33*"

# Run full Catch2 test suite & CTest
ctest --test-dir build --output-on-failure

# Run multi-server acceptance kit
bash scripts/run_acceptance_kit.sh
```
