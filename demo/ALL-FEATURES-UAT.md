# OpenSpaceTTD All-Features Guided Solo UAT (Sprints 1–40)

## Executive Summary

This document provides a guided solo User Acceptance Testing (UAT) manual for **OpenSpaceTTD**, verifying all functional features delivered from Sprint 1 through Sprint 40. 

Acceptance is anchored on the canonical savegame artifact:
```bash
demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav
```
Powered by **GameScript v8** (`OpenSpaceTTD-UAT-Demo` / `OSUD`), this scenario features a 6-world procedural universe ($1024 \times 512$ tiles) interconnected by 5 monumental gateway pairs, multi-tier megacity economies, planetary company stockpiles, corporate headquarters, logistics hubs, and in-kind fabrication.

---

## Launch Instructions

To launch the all-feature solo UAT scenario directly:

```bash
./build/openttd -g demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav
```

The scenario loads with an active company (Company 0) provided with starting capital (25M Cr) and pre-seeded industrial fixtures across all worlds.

---

## The In-Game Story Book

Open the **Story Book** window via `Manage Company` > `Story Book` or the company toolbar button. The book contains **12 structured chapters** with **25 persistent, measurable acceptance goals** and interactive viewport location pins.

```
+---------------------------------------------------------------------------------------------------+
| OpenSpaceTTD All-Features Solo UAT Story Book (v8)                                                |
+---------------------------------------------------------------------------------------------------+
| Chapter 1: Overview & Planetary Navigation       -> Goal 1: Navigate 6 Worlds & 6 Alien Biomes    |
| Chapter 2: Monumental Portal Gates               -> Goals 2-3: Gate Build/Link & Consist Run      |
| Chapter 3: Player Blueprints & CST Prefabs       -> Goals 4-5: CST Stamping & Map Capture         |
| Chapter 4: Planetary Operations                  -> Goals 6-7: Spaceports & Edge Conduits         |
| Chapter 5: Megacity Demands & Freight Corridors  -> Goals 8-9: Demand Tiers & Corridor Monitor    |
| Chapter 6: Supply Chain & Federation Governance  -> Goals 10-11: Trade Ledger & Auth/Charters    |
| Chapter 7: Commonwealth Data Crystals            -> Goals 12-16: Rebranded Economy & Rolling Stock|
| Chapter 8: Phase 4 Colonisation & Outposts       -> Goals 17-18: Expansion Worlds & Survey Sites  |
| Chapter 9: Planetary Development & Promotion     -> Goals 19-20: Dev Scores & World Promotion     |
| Chapter 10: Corporate Headquarters Campus        -> Goals 21-22: Central HQ & Campus Progression  |
| Chapter 11: Planetary Stockpiles & Logistics     -> Goals 23-24: 6 Fabrication Roles & Hub Floors |
| Chapter 12: In-Kind Fabrication & BOM Engine     -> Goal 25: 80% Discount & Stockpile Construction|
+---------------------------------------------------------------------------------------------------+
```

---

## Detailed Chapter Walkthrough & Acceptance Tests

### Chapter 1: Planetary Navigation & Multi-World Biomes
- **Goals:**
  - `Goal 1`: Navigate all worlds using `Ctrl+Alt+1..6` or the Map menu world jump buttons. Confirm distinct environmental biomes.
- **Procedure:**
  1. Press `Ctrl+Alt+1` through `Ctrl+Alt+6` in sequence, or open the **Map** dropdown menu on the top toolbar and select `Jump to World 1` through `Jump to World 6`.
  2. Confirm each world renders its distinctive environmental biome:
     - **World 1 (Core):** Lush Temperate green fields and deciduous forestry.
     - **World 2 (Developed):** Arid Desert red sands, savannah, and cacti.
     - **World 3 (Frontier):** Sub-Arctic alpine snowline and conifers.
     - **World 4 (Expansion):** Volcanic basalt, ash plains, and lava rifts.
     - **World 5 (Expansion):** Sub-Tropic rainforest canopy with dense jungle greenery.
     - **World 6 (Expansion):** Oceanic archipelago with extensive coastal water basins.
- **Pass Criteria:** Viewport centers reliably over the designated world region without landing in void buffer bands.

---

### Chapter 2: Monumental Portal Gates & Wormhole Consist Run
- **Goals:**
  - `Goal 2`: Build an unlinked portal gate with its automatic 18-tile two-lane terminal, then link it to a destination gate in another world.
  - `Goal 3`: Run a portal consist through Gateway Alpha and verify it emerges smoothly at the non-aligned Phase 2 head on a perpendicular track axis.
- **Procedure:**
  1. In Chapter 2, click the **Gateway Alpha - Phase 1 head** location pin.
  2. Follow the demonstrator train `UAT Wormhole Demonstrator`. Confirm it enters the tunnel entrance in World 1 and teleports seamlessly into World 2 at the Phase 2 head without derailment or visual glitching.
  3. Open the **Railway Construction** toolbar, click the **Portal Gate** icon, and click an open grass tile in World 1.
  4. Verify the automatic 18-tile two-lane terminal layout with holding track and path signals is stamped. Click a destination tile in World 2 to link them.
- **Pass Criteria:** Consists enter wormhole tracks at line speed and emerge correctly aligned with destination track direction.

---

### Chapter 3: Player Rail Blueprints & CST Prefabs
- **Goals:**
  - `Goal 4`: Open Blueprint Library (`B`), select a canonical CST Prefab (e.g. `CST Dual-Track Passing Siding` or `Mainline Double Straight`), rotate/flip, and stamp it on the staging area.
  - `Goal 5`: Select `Capture From Map` in the Blueprint Library, drag across the sample rail layout, save it to your local library, and place a replica.
- **Procedure:**
  1. Click the **CST Prefab Staging Area** location pin in World 2.
  2. Press `B` to open the Blueprint Library.
  3. Select `CST Dual-Track Passing Siding`. Press `R` to rotate, `F` to mirror, and click the designated leveling staging pad to stamp the layout.
  4. Click the **Custom Blueprint Capture Track Layout** pin.
  5. In the Blueprint Library, click `Capture From Map`, drag a selection box across the sample rails, enter a name (e.g., `My Layout`), and click `Save`. Stamp a replica nearby.
- **Pass Criteria:** Prefabs and captured blueprints place deterministically with all signals and track combinations preserved.

---

### Chapter 4: Planetary Operations: Spaceports & Edge Conduits
- **Goals:**
  - `Goal 6`: Open station window for the Phase 1 Spaceport candidate and designate Spaceport. Upgrade through Tier 2 and Tier 3, observing supply status and projected off-world trade cargo.
  - `Goal 7`: Select the Edge Conduit tool on the rail toolbar and build on the signed Phase 3 void boundary tile. Verify Land Area Information reports 100 units/mo Frontier extraction and mineral cargo.
- **Procedure:**
  1. Click the **Phase 1 Spaceport Candidate** pin in World 1.
  2. Open the station window and click `Designate Spaceport`.
  3. Upgrade to Tier 2 and Tier 3. Observe live telemetry metrics for monthly cargo projections.
  4. Click the **Frontier Edge Minerals** pin in World 3.
  5. Select the **Edge Conduit** tool from the Railway Construction toolbar and click the signed void boundary tile.
  6. Query the built conduit using `Land Area Information` (`?`) to verify primary mineral extraction and Frontier phase bonuses.
- **Pass Criteria:** Spaceport upgrades update capacity and throughput; Edge Conduits generate commodity output into connected rail stations.

---

### Chapter 5: Megacity Demands & Freight Corridors
- **Goals:**
  - `Goal 8`: Open Town window > `Megacity` or Town menu > `Megacity Overview`. Inspect Tier 1-3 demands and observe growth state transitions under monthly evaluation.
  - `Goal 9`: Open Map dropdown > Freight Corridor Monitor. Inspect inter-world corridor transit volume, capacity utilization, and congestion bottleneck alerts.
- **Procedure:**
  1. Click the **Oaktree Core Megacity** pin in World 1.
  2. Open the Town window and click the `Megacity` button (or select `Megacity Overview` from the Town menu).
  3. Inspect the three commodity demand tiers:
     - **Tier 1 (Sustenance):** Food, Water.
     - **Tier 2 (Expansion):** Goods, Building Materials.
     - **Tier 3 (Prosperity):** Valuables, Luxuries.
  4. Observe growth state indicators: *Starvation*, *Subsistence*, *Metropolitan Boom*, or *HyperGrowth*.
  5. Open **Map** dropdown > **Freight Corridor Monitor** to inspect gateway route congestion, active consists in transit, and bottleneck alerts.
- **Pass Criteria:** Megacity demand progress bars reflect live cargo deliveries and correct growth stage multipliers.

---

### Chapter 6: Supply Chain Matrix & Federation Governance
- **Goals:**
  - `Goal 10`: Open Map dropdown > Supply Chain & Trade Ledger. Inspect macro phase flows, spaceport/conduit infrastructure volume, and verify the CONSERVED commodity trade balance audit.
  - `Goal 11`: Open Map dropdown > Federation Authentication & Charters. Authenticate player identity, charter a corporate entity, and expand world presence to World 1.
- **Procedure:**
  1. Open **Map** dropdown > **Supply Chain & Trade Ledger**.
  2. In Tab 1 (Macro Commodity Flows), verify balanced inter-world imports, exports, and extraction.
  3. In Tab 2 (Inter-World Ledger), verify zero-sum commodity balance showing the green **CONSERVED** status badge.
  4. Open **Map** dropdown > **Federation Authentication & Charters**.
  5. Review player session credentials, corporation chartering options, and multi-world operating privileges.
- **Pass Criteria:** Trade ledger maintains strict commodity conservation; Federation interface reports correct corporate profile status.

---

### Chapter 7: Commonwealth Data Crystals Rebranding
- **Goals:**
  - `Goal 12`: Graphs > Cargo Payment Rates lists Data Crystals.
  - `Goal 13`: Game Settings search shows Distribution mode for data crystals.
  - `Goal 14`: Town station acceptance and waiting lists display Data Crystals.
  - `Goal 15`: Train depot purchase list contains the Data Van wagon.
  - `Goal 16`: Road depot purchase list contains the MPS Data Courier.
- **Procedure:**
  1. Open **Graphs** > **Cargo Payment Rates** and confirm `Data Crystals` replaces Mail/Valuables.
  2. Open **Settings** and verify `Distribution mode for data crystals`.
  3. Open a town station window accepting high-value cargo and verify `Data Crystals` display.
  4. Open a train depot purchase list to confirm the `Data Van` wagon is available.
  5. Open a road vehicle depot purchase list to confirm the `MPS Data Courier` is available.
- **Pass Criteria:** All UI strings, cargo tables, and rolling stock catalogs consistently reference Commonwealth Data Crystals.

---

### Chapter 8: Phase 4 Colonisation & Frontier Outposts
- **Goals:**
  - `Goal 17`: Inspect uncolonised Expansion Worlds (Worlds 4, 5, 6) and verify environmental styling and pre-colonisation placement restrictions.
  - `Goal 18`: Found a colonial outpost on an Expansion World to elevate it to Phase 3 Frontier status, unlocking primary extraction and settlement expansion.
- **Procedure:**
  1. Jump to World 4 (`Ctrl+Alt+4`), World 5 (`Ctrl+Alt+5`), or World 6 (`Ctrl+Alt+6`).
  2. Inspect the pre-colonisation survey markers.
  3. Note that standard commercial infrastructure construction is restricted prior to outpost establishment.
  4. Use console command `colonize_world 3` (or the Outpost tool) to establish the first colonial outpost on World 4 (Ignis Caldera).
  5. Verify the world status upgrades to **Phase 3 Frontier**, enabling resource extraction and settlement expansion.
- **Pass Criteria:** World Phase advances to Frontier; news broadcast announces planetary colonisation.

---

### Chapter 9: Planetary Development Scoring & Phase Promotion
- **Goals:**
  - `Goal 19`: Deliver inter-world cargo across gateway pairs to accumulate planetary development score points.
  - `Goal 20`: Promote a Frontier or Developed world to its next development tier when the score threshold is satisfied, unlocking higher technology tiers.
- **Procedure:**
  1. Open the Story Book Chapter 9.
  2. Inspect development scores across worlds (pre-seeded: World 0 = 25,000, World 1 = 8,000, World 2 = 2,500).
  3. Execute `promote_world 1` in the console to promote World 1 (Arid Desert) to **Phase 1 Core Metropolis**.
  4. Confirm the news banner announces promotion and technology tier unlock.
- **Pass Criteria:** Promotion succeeds when development thresholds are satisfied; news broadcast announces the promotion.

---

### Chapter 10: Corporate Headquarters Campus
- **Goals:**
  - `Goal 21`: Open Map menu > 'Corporate Headquarters & Stockpiles' to inspect the established Commonwealth Central HQ campus on World 1.
  - `Goal 22`: Advance headquarters tier through Planetary HQ and Commonwealth HQ to unlock corporate-wide bonuses.
- **Procedure:**
  1. In Chapter 10, click the **Commonwealth Central HQ Campus** pin in World 1.
  2. Open **Map** dropdown > **Corporate Headquarters & Stockpiles**.
  3. Verify the campus profile:
     - Name: `Commonwealth Central HQ`
     - Tier: `PlanetaryHQ` (Tier 2)
     - Location: World 1 Core
  4. Click `Upgrade Campus Tier` to advance to Tier 3 (`Interstellar`) or Tier 4 (`CST Arcology Tower`).
- **Pass Criteria:** Headquarters tier advances and updates production bonuses across all active company facilities.

---

### Chapter 11: Planetary Stockpiles & Logistics Hubs
- **Goals:**
  - `Goal 23`: Open Corporate Headquarters > 'Planetary Stockpiles' tab and verify multi-world inventory levels across all 6 fabrication roles.
  - `Goal 24`: Inspect the Merredin Planetary Logistics Hub on World 2, configure minimum reserve floors, and verify train stockpile ingestion.
- **Procedure:**
  1. In the Corporate Headquarters window, switch to the **Planetary Stockpiles** tab.
  2. Confirm physical material balances across all 6 fabrication roles:
     - **Ballast:** 1,000 units
     - **Structural Metal:** 700 units (shared steel pool)
     - **Wiring:** 400 units (shared goods pool)
     - **Electronics:** 150 units
     - **Superalloy:** 700 units
     - **Composites:** 400 units
  3. Click the **Merredin Planetary Logistics Hub** pin in World 2.
  4. Inspect the logistics hub reserve floor configuration (pre-set to 200 Ballast and 100 Structural Metal).
- **Pass Criteria:** Stockpile matrix displays distinct multi-world inventories; logistics hub preserves minimum inventory floors.

---

### Chapter 12: In-Kind Fabrication & BOM Construction
- **Goals:**
  - `Goal 25`: Toggle In-Kind Fabrication mode in the Corporate HQ window, construct rail infrastructure using local stockpile materials, and verify the 80% cash discount.
- **Procedure:**
  1. Open the Corporate Headquarters window and locate the **In-Kind Fabrication** toggle.
  2. With mode **Enabled**, build rail tracks, signals, or depots in World 1 or World 2.
  3. Observe construction cash costs: an immediate **80% cash discount** is applied.
  4. Re-check the Planetary Stockpile tab: note corresponding deductions in Ballast and Structural Metal matching the Bill of Materials (BOM) recipe.
- **Pass Criteria:** Construction deducts materials from local physical stockpile and grants 80% cash discount.

---

## Headless Reproduction Pipeline

To deterministically re-generate the canonical savegame artifact from clean repository state:

```bash
python3 -c '
import subprocess, time

p = subprocess.Popen(
    ["./build/openttd", "-D", "-c", "demo/uat_demo.cfg", "-G", "9032026"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    env=dict(**{"OPENSPACETTD_WORLD_COUNT": "6"}, **dict(subprocess.os.environ))
)

while True:
    line = p.stdout.readline()
    if not line: break
    if "Map generated, starting game" in line: break

time.sleep(1)
p.stdin.write("setup_uat_fixtures\n")
p.stdin.flush()
time.sleep(3)

p.stdin.write("save demo/OpenSpaceTTD-All-Features-UAT-v1.0\n")
p.stdin.flush()
time.sleep(1)

p.stdin.write("quit\n")
p.stdin.flush()
p.wait()
'
```

Verify savegame integrity:
```bash
./build/openttd -q demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav
```
