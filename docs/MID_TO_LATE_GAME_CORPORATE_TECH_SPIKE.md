# Architecture Spike: Mid-to-Late Game Objectives — Corporate Headquarters, In-Lore R&D Tech Tree, and Self-Sustaining In-Kind Fabrication

**Status:** DESIGN & ARCHITECTURE SPIKE  
**Date:** 2026-09-14  
**Inspiration:** *OpenTTD* macro-rail network simulation + *Factorio* / *Captain of Industry* material self-sufficiency + Peter F. Hamilton's *Commonwealth Saga* (CST corporate hegemony & Sheldon/Ozzie wormhole science).

---

## 1. Executive Summary & Gameplay Vision

In vanilla OpenTTD, late-game economics suffer from the "infinite cash problem": once a few high-volume rail lines are established, bank balances quickly soar into hundreds of millions of credits, rendering financial constraints meaningless and reducing infrastructure expansion to arbitrary mouse-clicking.

This spike defines the **Mid-to-Late Game Progression Pivot**:
Once the player has established stable, profitable railway lines across 3 to 4 world phases (Phase 4 Expansion outposts, Phase 3 Frontier extraction, Phase 2 Developed refineries, and Phase 1 Core Megacities), the game transitions from a simple commercial freight carrier into a **self-sustaining, vertically integrated planetary industrial conglomerate** (akin to *Compression Space Transport (CST)* in the Commonwealth Saga).

### The Three Core Pillars of the Mid-to-Late Game:
1. **Corporate Headquarters Nexus (Phase 1 Core World):**
   - Players construct an active **Corporate Headquarters Campus** on a Phase 1 Core world.
   - The HQ acts as the administrative, financial, and scientific command center for the empire.
2. **In-Lore Commonwealth Tech Tree (R&D):**
   - Rather than vehicles simply appearing based on an arbitrary calendar year, advanced technologies must be researched through dedicated R&D projects.
   - R&D is sustained by delivering high-tech research inputs (Data Crystals, Silicon Wafers, Neural Processors, and R&D Consignments) to the Corporate HQ or affiliated Planetary Research Labs.
   - Research unlocks high-tier traction (Catenary electrics, CST Vacuum-Tube Maglevs), twin-array wormhole stabilization, advanced CST station prefabs, and deep-spaceport bridges.
3. **In-Kind Material Fabrication (Factorio / Captain of Industry Play Style):**
   - Rather than spending pure cash to build tracks, bridges, tunnels, signals, depots, and locomotives, players can manufacture and fabricate them using **company-owned planetary stockpiles**.
   - **No Micro-Conveyor Hassle:** Resources do not need to be physically hauled by trucks or belts to every individual track tile. A train unloads materials at a company depot or planetary logistics warehouse; once in the **Planetary Company Stockpile**, any construction or purchase on that world consumes from that stockpile.

---

## 2. The Macro vs. Micro Logistics Model

A vital design requirement is preserving OpenTTD’s macro-railway strengths:

> *"As a concept you don't need to move the actual resources around; they just need to be on the same phase world in a depot and not at a railway yard bound for onward logistics."*

### 2.1 Public Freight in Transit vs. Company Planetary Stockpile
OpenTTD already has a rigid concept of cargo ownership: cargo on station platforms is public freight waiting for transport, and delivering it to a town/industry yields transit revenue. We introduce a clean separation:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        Cargo Flow: Public Trade vs. Company Stockpile                  │
└────────────────────────────────────────────────────────────────────────────────────────┘

 [Mines / Smelters / Chemical Fabs]
                │
                ▼ (Train Consist)
       ┌───────────────────────────────┐
       │ Destination Routing Decision  │
       └──────────────┬────────────────┘
                      │
        ┌─────────────┴─────────────────────────┐
        ▼                                       ▼
  [Public Commercial Station]             [Company Depot / Logistics Hub]
  • Freight delivered to town/industry    • Train order: "Unload & Stockpile"
  • Passenger/goods commercial delivery   • Cargo is absorbed into Company Inventory
  • Generates liquid credits ($/Cr)       • Stored as Planetary Stockpile for WorldID
  • Contributes to World Development Pts  • Used for In-Kind Fabrication & R&D Projects
```

### 2.2 Planetary Stockpile Data Structure
Each company maintains a material ledger per world:
```cpp
struct CompanyWorldStockpile {
    WorldID world_id;
    CompanyID company_id;

    /* Raw & Structural Bulk */
    uint32_t stone_ballast;          ///< Ballast for trackbeds (Stone/Gravel/Slag).
    uint32_t structural_steel;      ///< Steel rails, bridge girders, depot frames.
    uint32_t structural_alloys;     ///< High-performance superalloys for Maglev guideways.

    /* Electrical & High-Tech Components */
    uint32_t copper_wire;           ///< Catenary wiring, inductive coils, dynamos.
    uint32_t silicon_wafers;        ///< Microprocessors, signalling circuits, computers.
    uint32_t advanced_composites;   ///< Lightweight train bodies, vacuum tube shields.
    uint32_t mechanical_assemblies; ///< Bogies, wheelsets, diesel blocks, steam boilers.

    /* Scientific & R&D Data */
    uint32_t data_crystals;         ///< Experimental telemetry & quantum research data.
    uint32_t research_credits;      ///< Accumulated R&D project points.
};
```

When an engineer places a track, bridge, or buys a locomotive in a depot on `WorldID X`, the engine evaluates the world's stockpile.

---

## 3. In-Kind Fabrication Engine (Bill of Materials)

### 3.1 Bill of Materials (BOM) Examples

#### A. Track Infrastructure (per tile)
| Track Type | Cash-Only Cost | In-Kind Bill of Materials (BOM) | Minimum World Phase |
|---|---|---|:---:|
| **Pioneer Standard Rail** | 800 Cr | 2 units Stone (Ballast) + 1 unit Steel | Phase 4+ |
| **Catenary Electric Rail** | 2,400 Cr | 2 units Stone + 1 unit Steel + 1 unit Copper Wiring | Phase 3+ |
| **High-Speed Monorail** | 4,800 Cr | 4 units Concrete/Stone + 2 units Steel + 1 unit Copper | Phase 2+ |
| **CST Vacuum Maglev Guideway** | 12,000 Cr | 2 units Superalloys + 2 units Superconducting Coils + 1 Silicon Chip | Phase 1 Core |

#### B. Rolling Stock (per vehicle unit)
| Vehicle Class | Cash-Only Cost | In-Kind Bill of Materials (BOM) | Manufacturing Location |
|---|---|---|---|
| **Pioneer Steam Locomotive** | 50,000 Cr | 30 units Steel + 10 units Mechanical Assemblies | Local World Depot |
| **Heavy Planetary Diesel** | 120,000 Cr | 40 units Steel + 20 units Engine Blocks + 10 units Copper | Local World Depot |
| **High-Voltage Electric Hauler**| 250,000 Cr | 35 units Superalloys + 25 units Electric Motors + 15 units Copper | Phase 2+ World Depot |
| **CST Mark IV Hyper-Maglev** | 850,000 Cr | 50 units Superalloys + 25 units Superconducting Magnets + 15 Silicon/Neural Chips | Phase 1 Core World Depot |
| **Standard Freight Wagon** | 12,000 Cr | 10 units Steel + 4 units Wheelsets | Local World Depot |

### 3.2 Dual Construction Modes
Players can choose how they fund construction via a toolbar toggle:
1. **Commercial Purchase (Default / Cash Only):** Uses bank balance ($). Standard vanilla behavior.
2. **Fabricate from Stockpile (Self-Sufficiency Mode):**
   - Automatically deducts the BOM from the world's company stockpile.
   - Financial cost is waived (or reduced to a nominal labor fee, e.g. 5–10% of base cost).
   - If materials are insufficient, player is prompted with missing components or can opt to pay a steep spot-market import markup.

---

## 4. Corporate Headquarters & In-Lore Commonwealth Tech Tree

### 4.1 Corporate HQ Placement Requirements
- **Location:** Must be constructed on a **Phase 1 Core World** (e.g. Earth Prime or an elevated Phase 1 Core Metropolis).
- **Prerequisites:**
  - Company must operate active rail infrastructure across at least 3 distinct world phases.
  - Company net worth $\ge 5,000,000\text{ Cr}$.
  - Primary Megacity satisfaction $\ge 80\%$.
- **Functionality:**
  - Upgradable from Regional Branch Office $\to$ Planetary HQ $\to$ Interstellar Corporate Campus $\to$ CST Arcology Tower.
  - Houses the **Research & Development Department**.

### 4.2 In-Lore Tech Tree Branches
The Tech Tree draws directly from Peter F. Hamilton's Commonwealth lore:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        Commonwealth Tech Tree Progression                              │
└────────────────────────────────────────────────────────────────────────────────────────┘

  [TRACTION & PROPULSION]            [WORMHOLE & PORTAL PHYSICS]       [MATERIALS & FABRICATION]
            │                                     │                                │
            ▼                                     ▼                                ▼
  Tier 1: High-Adhesion Steam          Tier 1: Stable Gateway Links       Tier 1: Blast Furnace Steel
  (Vulcan 2-8-0 Hauler)                (Single-Track Portal Gates)        (Structural Steel Rails)
            │                                     │                                │
            ▼                                     ▼                                ▼
  Tier 2: Multi-Unit Diesel            Tier 2: Dual-Track Throat Arrays   Tier 2: Copper & Silicon Fabs
  (Titan D-100 Twin-Engine)            (2x Bandwidth Portals)             (Catenary & Signal Electronics)
            │                                     │                                │
            ▼                                     ▼                                ▼
  Tier 3: High-Voltage Electrics       Tier 3: Freight Corridor Bridges   Tier 3: Superalloys & Composites
  (CST E-40 Inter-World)               (Inter-Server Bulk Priority)       (Lightweight High-Speed Cars)
            │                                     │                                │
            ▼                                     ▼                                ▼
  Tier 4: CST Vacuum Maglev            Tier 4: Twin-Array Wormholes       Tier 4: Quantum Neural Fabs
  (1,000 km/h Vactrains)               (High-Speed Continuous Transit)    (Autonomous Train AI / Telemetry)
```

### 4.3 Research Funding Mechanism
To advance a technology project:
1. **Assign Research Focus:** Player selects an active project in the Corporate HQ Window.
2. **Deliver Scientific Feedstock:**
   - Transporting **Data Crystals** from research extraction worlds to the HQ.
   - Delivering **Silicon Chips / Neural Processors** manufactured on high-tech worlds.
   - Allocating a portion of monthly company operating profits to the R&D budget.
3. **Project Completion:** Unlocks new locomotives, wagons, track types, CST blueprints, and reduces fabrication material costs.

---

## 5. Architectural Review: Baked-In vs. Modular Add-On Set

A critical question raised in the spike:
> *Should this be baked into core C++ simulation code, or delivered as a modular NewGRF / NML add-on package?*

### Comparison Matrix

| Aspect | Option A: Pure Hardcoded C++ | Option B: Pure NewGRF Add-On | Option C: Hybrid Architecture (Recommended) |
|---|---|---|---|
| **Industry & Cargo Set** | Hardcoded in `cargotype.h` & `industry_cmd.cpp`. High maintenance; rigid. | Implemented via standard NML (`openspacettd_industries.grf`). Cleanly extensible. | **Modular NewGRF for Cargos/Industries;** core engine defines abstract fabrication roles. |
| **In-Kind Build BOM** | Direct hook into `CmdBuildSingleRail` and `CmdBuildRailVehicle`. | **Impossible:** NewGRF cannot intercept core C++ tile build commands or deduct materials. | **Engine C++ Core:** `FabricationManager` intercepts build commands and deducts from stockpile. |
| **Planetary Stockpiles** | Saved in C++ pool/chunk (`STCK` or `CORP`). Fast, deterministic. | Impossible in NewGRF (NewGRF cannot store persistent global inventory per world). | **Engine C++ Core:** `CompanyWorldStockpile` integrated into map/world savegame structure. |
| **Tech Tree & R&D UI** | Native C++ GUI window (`CorporateHQWindow`), responsive and customizable. | Extremely awkward (relies on story book or fake news popups). | **Engine C++ Core:** Native GUI window linked to Universe Authority and company data. |
| **Vanilla Compatibility** | Breaks vanilla scenarios if cargo IDs change. | Preserves vanilla fallback when mod is disabled. | **Seamless Fallback:** In vanilla games, BOM maps to vanilla Steel/Goods; with add-on, maps to rich chains. |

### The Hybrid Recommendation:
- **Core C++ Engine:** Implements the *mechanics* (Planetary Stockpiles, In-Kind BOM evaluation in build commands, Tech Tree manager, Corporate HQ window).
- **Abstract Cargo Roles:** The C++ engine references logical roles (`FabricationRole::Ballast`, `FabricationRole::StructuralMetal`, `FabricationRole::Wiring`, `FabricationRole::Electronics`).
  - Under vanilla/base games: Maps to `Gravel/Stone`, `Steel`, `Goods`, `Valuables`.
  - Under the Commonwealth Industry Pack (`openspacettd_industries.grf`): Maps to `Silicates`, `Superalloys`, `Copper Wire`, `Silicon Chips`.
- This ensures the gameplay loop works immediately with existing assets and scenarios, while providing the exact Factorio/CoI-scale depth when the dedicated Commonwealth content pack is loaded!

---

## 6. Required Core Engine Changes & Technical Impact

### 6.1 Data Structures & Managers
1. **`CompanyWorldStockpile` (`src/portal/company_stockpile.h`):**
   - Tracks quantities of materials owned by each company on each `WorldID`.
   - Methods: `AddMaterial(WorldID, CompanyID, CargoRole, uint32_t)`, `ConsumeBOM(WorldID, CompanyID, const BillOfMaterials &)`.
2. **`FabricationManager` (`src/portal/fabrication_manager.h`):**
   - Registry of BOM recipes for rail types, signals, bridges, stations, and vehicle engines.
   - Evaluates whether a build command should use credits or stockpile materials.
3. **`TechTreeManager` (`src/portal/tech_tree.h`):**
   - Manages research project nodes, prerequisites, research progress, and unlocks.
4. **Savegame Serializer:**
   - Add `STCK` (Stockpile) and `TECH` (Tech Tree) chunks to the savegame serializer (`src/saveload/`) to ensure full multiplayer determinism.

### 6.2 Command Interception Points
1. **`CmdBuildSingleRail` / `CmdBuildRailroadTrack` ([`src/rail_cmd.cpp`](file:///home/flax/games/openspacettd/src/rail_cmd.cpp)):**
   - If player has "Fabricate from Stockpile" enabled, query `FabricationManager::GetTrackBOM(railtype)`.
   - Deduct stone and steel from the world's stockpile; reduce or eliminate money cost.
2. **`CmdBuildRailVehicle` ([`src/train_cmd.cpp`](file:///home/flax/games/openspacettd/src/train_cmd.cpp)):**
   - Inspect vehicle BOM. Deduct materials from depot world stockpile.
3. **`CmdUnloadToStockpile` / Depot Cargo Transfer ([`src/economy.cpp`](file:///home/flax/games/openspacettd/src/economy.cpp)):**
   - When a train unloads at a company depot or warehouse, deposit cargo into company stockpile rather than selling it to the local town/industry.

### 6.3 UI & Windows
1. **`CorporateHQWindow` (`src/portal/corporate_hq_gui.cpp`):**
   - Tab 1: Headquarters Overview & Expansion Level.
   - Tab 2: Planetary Stockpiles (table of materials stored across all worlds).
   - Tab 3: Commonwealth Tech Tree & Active R&D Project.
2. **Rail Construction Toolbar Indicator:**
   - Small toggle button on rail toolbar: `[Fabricate from Stockpile: ON/OFF]`.
   - Tooltips show material cost alongside credit cost (e.g. `Cost: 2 Stone, 1 Steel (Stockpile: 4,200 Stone, 1,800 Steel)`).

---

## 7. Proposed Sprints & Prioritization Roadmap

To deliver this vision safely without destabilizing existing multiplayer, portal gate, or UAT baselines, we propose sequencing this work into four focused sprints:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        Mid-to-Late Game Sprint Sequence                                │
└────────────────────────────────────────────────────────────────────────────────────────┘

  [Sprint 35: Cross-Process Federation Transport]  (Already Planned: Real multi-process network)
                         │
                         ▼
  [Sprint 36: Consolidated Playable Solo/Fed UAT]  (Already Planned: All-feature UAT save)
                         │
                         ▼
  [Sprint 39: Corporate HQ & Planetary Stockpiles] ◄── CORE REFACTOR STARTS HERE
  • CompanyWorldStockpile data model & save/load serialization
  • Corporate HQ placement on Phase 1 Core worlds
  • Unload-to-Stockpile depot mechanic & UI inventory viewer
                         │
                         ▼
  [Sprint 40: In-Kind Fabrication Engine & BOM]
  • Bill of Materials (BOM) registry for track, signals, and trains
  • Rail build command interception & material deduction
  • Dual-mode construction toggle on rail toolbar
                         │
                         ▼
  [Sprint 41: In-Lore Commonwealth Tech Tree & R&D]
  • Research project nodes (CST Maglev, Twin Gates, Advanced Metallurgy)
  • Data Crystal & high-tech research feedstock deliveries
  • Tech Tree UI in Corporate HQ Window
                         │
                         ▼
  [Sprint 42: Factorio-Scale 12-Cargo Production Chains] (Harmonized with Sprint 37)
  • Silicon, Copper, Superalloys, Machinery Modules, Neural Processors
  • Multi-world industrial loops feeding both Megacity quotas and R&D
```

---

## 8. Approved Architectural Design Decisions (Alignment: 1A, 2B, 3B, 4A)

Following interactive review, the architecture has converged on the following approved design choices:

### Decision 1A: Soft / Economic Incentive (Dual Construction Modes)
- **Mechanic:** Cash purchase is always retained as a fallback so networks never suffer catastrophic deadlock if an upstream supply line is severed.
- **Economic Incentive:** Constructing infrastructure and vehicles using **In-Kind Planetary Stockpile Fabrication** waives the heavy commercial markup, offering a **70% to 85% cost reduction** (charging only a nominal 10–15% local assembly/labor fee).
- **Late-Game Scaling:** Advanced Tier infrastructure (CST Maglevs, vacuum tubes, vactrains) carries steep exponential cash import surcharges. While technically purchasable with cash, doing so at scale rapidly depletes even late-game corporate reserves, making planetary in-kind fabrication the overwhelmingly dominant and practically essential strategy.
- **Missing Material Spot Market:** When in Fabrication Mode, if the planetary stockpile lacks sufficient materials for a build command, the player is alerted with the exact missing bill of materials and given the choice to either abort or auto-purchase the missing components at a **150% spot-market import penalty**.

### Decision 2B: Dedicated Company Logistics Hubs & Warehouses (Bi-Directional Inventory Buffer)
- **Infrastructure:** Rather than overloading standard train maintenance depots with freight handling, companies construct dedicated **Company Logistics Hubs** (Corporate Warehouses) physically integrated with rail siding platforms.
- **Physical Footprint:** These facilities represent active planetary distribution yards, featuring dedicated siding tracks, gantry cranes, and climate-controlled storage silos.
- **Bi-Directional Buffer (Not a One-Way Cargo Sink):**
  - A warehouse does not simply "eat" cargo. It absorbs it into the company's planetary inventory (`CompanyWorldStockpile`) where it is immediately available for local fabrication, Megacity supply quotas, or scientific R&D.
  - **Surplus Release & Pickup Orders (`Load from Stockpile`):** If a surplus accumulates on-world or another planet in the Commonwealth network faces an urgent material deficit (e.g. steel for a major Maglev project or silicon for Megacity expansion), trains can dock at the Logistics Hub station with the order **`Load from Stockpile`**.
  - **Reserve Floor / Threshold Rules:** In the Logistics Hub GUI, companies can configure a **Reserve Floor** (e.g. *"Retain 1,000 units of Structural Steel on-world for local track maintenance; release all surplus for outgoing export trains"*).
- **Depot Specialization:** Standard train depots remain focused on rolling stock assembly, maintenance, and consist modifications, drawing materials from the world's stockpile when building new engines or wagons.

### Decision 3B: Continuous Research Duration & Enriched Data Crystal Lifecycle
- **Mechanic:** R&D progression is a continuous simulation process requiring both research duration and specialized scientific feedstocks.
- **The Two-Tier Data Crystal Lifecycle (Consumer vs. Enriched Mathematical Proofs):**
  1. **Blank Data Crystals (Raw Monocrystalline Substrate):** Precision manufactured at a *Crystal Synthesis Facility* from refined *Silica Sand / Quartz* and *Rare Earth Minerals*. These crystals start completely unformatted.
  2. **Consumer Data Crystals (Encrypted Correspondence):** Blank crystals delivered to urban Megacities and towns are formatted into *Consumer Data Crystals*, carrying encrypted civilian correspondence, banking tokens, and neural memory backups. This completely replaces standard mail on Phase 1 and 2 worlds, commanding high transit revenue and driving Megacity Prosperity.
  3. **Enriched Quantum Data Crystals (Advanced Mathematical Proofs & Telemetry):**
     - Blank data crystals **cannot** be consumed directly for corporate R&D. To be useful to Commonwealth science, they must hold deep physics models and advanced mathematical proofs (e.g. Ozzie-Sheldon wormhole field tensors, quantum singularity dynamics, superconductor flux proofs).
     - Non-consumer blank crystals must be hauled to dedicated research installations: **Planetary Observatories**, **Deep Space Telemetry Arrays**, or **Quantum Computing Complexes** on Phase 3/4 frontier worlds.
     - These science installations run quantum simulations, probe exotic physical phenomena, and **imprint/enrich** the blank crystals with mathematical proofs, generating **Enriched Quantum Data Crystals**.
  4. **Corporate HQ Consumption:** Trains haul these Enriched Quantum Crystals to the Corporate HQ on the Phase 1 Core world, where R&D laboratories burn them over simulated in-game months to unlock Tier 3 and Tier 4 Commonwealth technologies.
- **Live GUI Feedback:** The Corporate HQ window displays an active laboratory progress bar, projected completion date, current RP burn rate, and required Enriched Crystal quotas.

### Decision 4A: Phased Cargo Rollout (Abstract Engine Roles $\to$ Full 12-Cargo Suite)
- **Phase 1 (Sprints 39–41):** The C++ engine core implements abstract fabrication and research roles (`FAB_BALLAST`, `FAB_STRUCTURAL_METAL`, `FAB_WIRING`, `FAB_ELECTRONICS`, `FAB_RESEARCH_DATA`). In the base engine, these map to vanilla cargos (`Stone/Gravel`, `Steel`, `Goods`, `Valuables`).
  - *Benefit:* Sprints 39 (Stockpiles & HQ), 40 (Fabrication Engine & BOM), and 41 (Tech Tree R&D) are immediately playable, testable, and verifiable in vanilla scenarios without waiting for external NewGRF assets.
- **Phase 2 (Sprint 42, harmonized with Sprint 37):** The Commonwealth Industry Pack (`openspacettd_industries.grf`) introduces the bespoke 12-cargo Commonwealth chains (Silicates, Quartz, Refined Copper, Superalloys, Synthetic Polymers, Neural Processors), which seamlessly bind directly to the engine's abstract fabrication roles.

---

## 9. Comprehensive Resource Model & Production Chains

### 9.1 Phase 1: Abstract Role to Vanilla Cargo Mapping (Sprints 39–41)

During Phase 1, the core engine links fabrication and R&D requirements to standard cargos:

| Engine Abstract Role | Vanilla Cargo Mapping | Planetary Fabrication Purpose |
|---|---|---|
| `FAB_BALLAST` | **Stone / Gravel** | Trackbed foundation, concrete bridge piers, depot pads. |
| `FAB_STRUCTURAL_METAL` | **Steel** | Rails, structural trusses, wagon frames, locomotive boilers. |
| `FAB_ELECTRICAL_WIRING` | **Goods** (Proxy) | Catenary overhead wiring, dynamos, traction motors. |
| `FAB_ELECTRONICS` | **Valuables / Goods** (Proxy) | Signal relays, PBS processors, train telemetry computers. |
| `FAB_RESEARCH_DATA` | **Valuables / Gold** (Proxy) | R&D feedstocks delivered to Phase 1 Corporate HQ. |

### 9.2 Phase 2: Bespoke 12-Cargo Commonwealth Production Suite (Sprint 42)

When the Commonwealth Industry Pack is loaded, the abstract roles bind to dedicated multi-world supply chains:

| # | Cargo Name | Category | Primary Origin Biome / World | Commonwealth Role |
|---|---|---|---|---|
| 1 | **Stone & Slag** | Raw Bulk | Frontier (Phase 3) / Volcanic | Crushed into Ballast & High-Strength Concrete (`FAB_BALLAST`). |
| 2 | **Iron Ore** | Raw Bulk | Frontier (Phase 3) / Sub-Arctic | Smelted into Structural Steel. |
| 3 | **Structural Steel** | Refined Metal | Industrial Refineries (Phase 2) | Steel rails, bridge girders, depot frames (`FAB_STRUCTURAL_METAL`). |
| 4 | **Copper Ore** | Raw Mineral | Arid Desert / Frontier (Phase 3) | Smelted into conductive Copper Wire and Inductive Coils. |
| 5 | **Conductive Wire & Coils** | Intermediate Electrical | Industrial Smelters (Phase 2) | Electrified catenary overhead lines, electric motors (`FAB_ELECTRICAL_WIRING`). |
| 6 | **Silica Sand & Quartz** | Raw Mineral | Oceanic / Arid (Phase 2/3) | Refined into Silicon Wafers and Blank Crystals. |
| 7 | **Silicon Wafers & Chips** | High-Tech Component | Phase 2 High-Tech Fabs | Advanced PBS signalling circuits, telemetry (`FAB_ELECTRONICS`). |
| 8 | **Rare Earth Minerals** | Exotic Raw | Volcanic / Sub-Arctic (Phase 3/4) | Alloyed with steel for Superalloys & crystal lattice doping. |
| 9 | **Superalloys & Superconductors** | Advanced Material | Phase 2/1 Metallurgy Arcologies | CST Maglev guideways, vacuum tube seals, cryo-bogies (`FAB_SUPERALLOY`). |
| 10| **Synthetic Polymers & Composites**| Advanced Material | Chemical Refineries (Phase 2) | Aerodynamic train shells, high-speed passenger interiors (`FAB_COMPOSITES`). |
| 11| **Blank Data Crystals** | Precision Intermediate | Monocrystal Synthesis Fabs | Unformatted monocrystalline storage substrate; fed into civil and scientific pipelines. |
| 12| **Enriched Quantum Crystals** | Scientific Feedstock | Quantum Observatories / Telemetry Arrays | Imprinted with advanced mathematical proofs, wormhole tensors & Ozzie equations (`FAB_RESEARCH_DATA`). |
| * | **Encrypted Consumer Crystals**| Civil Express Freight | Formatted in Towns & Megacities | High-security human mail/data replacement; generated when towns receive Blank Crystals. |

### 9.3 The Four Interlocking Production Pipelines

```
══════════════════════════════════════════════════════════════════════════════════════════
PIPELINE A: STRUCTURAL & TRACK INFRASTRUCTURE (From Raw Ore to CST Guideway)
══════════════════════════════════════════════════════════════════════════════════════════
[Quarries / Slag Heaps]  ──► Stone / Slag     ──► [Crusher Plant]     ──► Ballast & Concrete
[Deep Iron Mines]        ──► Iron Ore         ──► [Foundry & Mill]    ──► Structural Steel
[Rare Earth Mines]       ──► Rare Minerals    ──► [Alloy Arcology]    ──► Superalloys

══════════════════════════════════════════════════════════════════════════════════════════
PIPELINE B: ELECTRONICS, SIGNALLING & CATENARY (From Sand & Ore to Microchips)
══════════════════════════════════════════════════════════════════════════════════════════
[Copper Mines]           ──► Copper Ore       ──► [Copper Smelter]    ──► Conductive Wiring
[Quartz / Silica Dunes]  ──► Silica Sand      ──► [Silicon Arcology]  ──► Silicon Chips
[Silicon Chips + Copper] ─────────────────────► [Electronics Plant]   ──► Signalling Logic
[High-Purity Silicon]    ──► Cleanroom Fab    ──► [Quantum Fab]       ──► Neural Processors

══════════════════════════════════════════════════════════════════════════════════════════
PIPELINE C: ADVANCED TRAIN PROPULSION (From Polymers & Alloys to CST Maglev)
══════════════════════════════════════════════════════════════════════════════════════════
[Bio-Farms / Petroleum]  ──► Hydrocarbons     ──► [Polymer Complex]   ──► Synthetic Composites
[Superalloys + Wiring]   ──► Cryo-Assembly    ──► [Maglev Works]      ──► CST Guideway & Motors

══════════════════════════════════════════════════════════════════════════════════════════
PIPELINE D: DATA CRYSTAL ENRICHMENT & MATHEMATICAL PROOFS (Consumer vs R&D)
══════════════════════════════════════════════════════════════════════════════════════════
[Silica Sand + Rare Minerals] ──► [Monocrystal Synthesis Fab] ──► Blank Data Crystals
                                                                         │
                     ┌───────────────────────────────────────────────────┴───────────────────────────────────────────────────┐
                     ▼                                                                                                       ▼
           [Towns / Megacities]                                                                                 [Quantum Observatories / Telemetry Arrays]
        • Blank crystals formatted                                                                           • Frontier anomaly telemetry (Phase 3/4)
        • Consumed for high-security comms                                                                   • Mathematical proofs & wormhole tensors
        • Outputs: Encrypted Consumer Crystals (Mail Replacement)                                            • Outputs: Enriched Quantum Crystals
        • Generates high transit revenue & Megacity Prosperity                                               • Shipped to Phase 1 Corporate HQ for Tier 3/4 R&D
```

---

## 10. Bill of Materials (BOM) & Economic Pricing Matrix

### 10.1 Track Infrastructure (per tile placed)

| Track Type | Commercial Cash Cost | In-Kind Stockpile Fabrication Cost | Tech Tree Prerequisite |
|---|---|---|---|
| **Pioneer Standard Rail** | 800 Cr | 120 Cr (Labor) + 2 units Ballast + 1 unit Steel | Pioneer Engineering (Unlocked) |
| **Catenary Electrified Rail** | 2,400 Cr | 360 Cr (Labor) + 2 units Ballast + 1 unit Steel + 1 unit Wiring | Tier 2: Overhead Electrification |
| **Heavy Industrial Monorail** | 4,800 Cr | 720 Cr (Labor) + 4 units Concrete + 2 units Steel + 1 unit Wiring | Tier 3: Industrial Monorails |
| **CST Maglev Guideway** | 14,000 Cr | 2,100 Cr (Labor) + 2 units Superalloys + 2 units Coils + 1 Silicon Chip | Tier 4: Maglev Propulsion |
| **CST Vacuum Hyper-Tube** | 28,000 Cr | 4,200 Cr (Labor) + 4 units Superalloys + 2 Composites + 2 Coils | Tier 4: Vacuum Guideway Enclosures |

### 10.2 Signalling & Network Hardware (per tile placed)

| Signal Type | Commercial Cash Cost | In-Kind Stockpile Fabrication Cost | Operating Advantage |
|---|---|---|---|
| **Mechanical Semaphores** | 100 Cr | 15 Cr + 1 unit Steel | Low-cost manual block signalling for Phase 3/4. |
| **High-Voltage Light Signals** | 300 Cr | 45 Cr + 1 unit Steel + 1 unit Wiring | Standard block signals for electrified lines. |
| **Advanced PBS (Path Signals)** | 600 Cr | 90 Cr + 1 unit Steel + 1 unit Wiring + 1 Silicon Chip | Dynamic multi-train junction reservations. |
| **CST Automated Block Sensors**| 1,500 Cr | 225 Cr + 1 unit Composites + 1 Silicon Chip + 1 unit Wiring | High-speed headway sensors for 600+ km/h Maglevs. |

### 10.3 Rolling Stock (Locomotives & Consists)

| Vehicle Class | Operating World Phase | Commercial Cash Cost | In-Kind Stockpile Fabrication Cost | Tech Unlock |
|---|---|---|---|---|
| **Vulcan 2-8-0 Pioneer Steamer** | Phase 4 / Phase 3 | 50,000 Cr | 7,500 Cr + 25 Steel + 10 Assemblies | Unlocked |
| **Titan D-100 Planetary Diesel**| Phase 3 / Phase 2 | 120,000 Cr | 18,000 Cr + 40 Steel + 15 Engine Blocks + 10 Wiring | Tier 1: Combustion |
| **CST E-40 Inter-World Electric**| Phase 2 / Phase 1 | 250,000 Cr | 37,500 Cr + 35 Superalloys + 25 Motors + 20 Wiring | Tier 2: Electrification |
| **CST Mark IV Hyper-Maglev** | Phase 1 Core Only | 850,000 Cr | 127,500 Cr + 50 Superalloys + 30 Coils + 15 Neural Processors | Tier 4: CST Maglev |
| **Heavy Bulk Ore Hopper** | Any World | 12,000 Cr | 1,800 Cr + 12 Structural Steel + 4 Wheelsets | Unlocked |
| **Cryogenic Intermodal Container Car** | Phase 2 / Phase 1 | 22,000 Cr | 3,300 Cr + 15 Steel + 8 Composites + 4 Cryo-Valves | Tier 3: Intermodal |

---

## 11. Core C++ Engine Architecture & Implementation Touchpoints

### 11.1 New Data Structures & Managers
1. **`CompanyWorldStockpile` ([`src/portal/company_stockpile.h`](file:///home/flax/games/openspacettd/src/portal/company_stockpile.h)):**
   - Ledger storing owned material quantities for each `(WorldID, CompanyID)`.
   - API: `AddMaterial()`, `HasSufficientMaterials()`, `ConsumeMaterials()`, `CalculateMissingCost()`.
2. **`FabricationManager` ([`src/portal/fabrication_manager.h`](file:///home/flax/games/openspacettd/src/portal/fabrication_manager.h)):**
   - Central registry storing BOM recipes for all track, signal, bridge, station, and locomotive types.
   - Computes commercial cash cost vs stockpile fabrication cost dynamically based on world and company state.
3. **`TechTreeManager` ([`src/portal/tech_tree.h`](file:///home/flax/games/openspacettd/src/portal/tech_tree.h)):**
   - Tracks research nodes, dependencies, accumulated Research Points (RP), active projects, and completion timers.
4. **`CompanyLogisticsHub` (`src/portal/logistics_hub.h`):**
   - Tile and station logic for the company warehouse siding.
   - Manages bi-directional transfer: `DepositToStockpile()` and `WithdrawFromStockpile()`.
   - Stores per-cargo reserve threshold floors (`min_reserve[cargo]`) to guarantee local fabrication supplies are not drained by export trains.

### 11.2 OpenTTD Engine Command Hooks
1. **`CmdBuildSingleRail` / `CmdBuildRailroadTrack` ([`src/rail_cmd.cpp`](file:///home/flax/games/openspacettd/src/rail_cmd.cpp)):**
   - Inspects active company's build mode (`Commercial` vs `Fabricate`).
   - In `Fabricate` mode, validates stockpile inventory via `FabricationManager`.
   - On execution (`DC_EXEC`), deducts stone/steel from `CompanyWorldStockpile` and charges the discounted labor fee.
2. **`CmdBuildRailVehicle` ([`src/train_cmd.cpp`](file:///home/flax/games/openspacettd/src/train_cmd.cpp)):**
   - Identifies the depot's `WorldID`.
   - Evaluates locomotive/wagon BOM against the local world stockpile, waiving standard purchase cost in favor of parts + labor.
3. **Bi-Directional Logistics Orders in Engine Transfer Loops ([`src/economy.cpp`](file:///home/flax/games/openspacettd/src/economy.cpp)):**
   - **`Unload to Stockpile` (`DeliverGoods`):** When a consist unloads at a `CompanyLogisticsHub`, bypasses public municipal revenue and deposits cargo straight into `CompanyWorldStockpile`.
   - **`Load from Stockpile` (`LoadUnloadVehicle`):** When an outgoing consist docks at a Logistics Hub with `Load from Stockpile`, queries `CompanyLogisticsHub::GetAvailableSurplus()`. If inventory exceeds the world's reserve floor, draws cargo from stockpile and loads it onto wagons as standard commercial freight eligible for inter-world transit.
4. **Savegame Serializer ([`src/saveload/`](file:///home/flax/games/openspacettd/src/saveload/)):**
   - Adds `STCK` (Planetary Stockpiles and Hub reserves) and `TECH` (Tech Tree) chunks to guarantee deterministic multiplayer save/load.

### 11.3 GUI & User Interface Windows
1. **`CorporateHQWindow` (`src/portal/corporate_hq_gui.cpp`):**
   - **Tab 1 — Campus Overview:** Corporate Net Worth, active world presence count, Megacity satisfaction.
   - **Tab 2 — Planetary Stockpiles:** Multi-world matrix showing material inventories on each colonized planet.
   - **Tab 3 — Commonwealth Tech Tree:** Interactive node graph displaying researched technologies, active project progress bar, monthly RP burn, and projected completion date.
2. **Rail Construction Toolbar Indicator:**
   - Small toggle button: `[Mode: Stockpile Fabrication (85% Discount) / Commercial Cash]`.
   - Tooltip details: `Cost: 360 Cr + 2 Ballast, 1 Steel, 1 Wire (Stockpile: 4,500 Ballast, 1,200 Steel, 800 Wire)`.

---

## 12. Final Prioritized Sprint Roadmap

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        Mid-to-Late Game Sprint Sequence                                │
└────────────────────────────────────────────────────────────────────────────────────────┘

  [Sprint 35: Cross-Process Federation Transport]  (Pending: live multi-process network)
                         │
                         ▼
  [Sprint 36: Consolidated Playable Solo/Fed UAT]  (Pending: all-feature UAT save)
                         │
                         ▼
  [Sprint 39: Corporate HQ & Dedicated Logistics Hubs] ◄── CORPORATE REFACTOR STARTS
  • CompanyWorldStockpile data model & STCK chunk serialization
  • Corporate HQ placement on Phase 1 Core worlds
  • Dedicated Company Logistics Hub station tile & "Unload to Stockpile" order action
  • CorporateHQWindow (Campus Overview & Planetary Stockpile Matrix)
                         │
                         ▼
  [Sprint 40: In-Kind Fabrication Engine & Dual-Mode BOM]
  • Bill of Materials (BOM) registry for rails, signals, bridges, and rolling stock
  • Soft economic incentive model (70–85% discount for fabrication; spot-market fallback)
  • Command hooks in CmdBuildSingleRail and CmdBuildRailVehicle
  • Rail toolbar dual-mode toggle with real-time stockpile tooltips
                         │
                         ▼
  [Sprint 41: In-Lore Commonwealth Tech Tree & Continuous R&D]
  • TechTreeManager & TECH chunk serialization
  • Research project nodes (CST Maglev, Twin Gates, Advanced Metallurgy)
  • Continuous research duration (RP point burn over in-game months)
  • Corporate HQ Tech Tree GUI tab with active research progress bar
                         │
                         ▼
  [Sprint 42: Factorio-Scale 12-Cargo Production Chains] (Harmonized with Sprint 37)
  • Full bespoke Commonwealth Industry Pack (openspacettd_industries.grf)
  • Multi-world industrial loops feeding both Megacity consumption and corporate R&D
```

