# OpenSpaceTTD: Commonwealth Saga Lore, In-Game Asset Alignment & Roadmap

Status: **CANONICAL SPECIFICATION & AUDIT**  
Date: **2026-09-13**  
Inspiration: **Peter F. Hamilton's *Commonwealth Saga* (*Pandora's Star*, *Judas Unchained*) + OpenTTD Engine**

---

## 1. Executive Summary & Core Lore Premise

In Peter F. Hamilton’s *Commonwealth Saga*, human civilization expanded across hundreds of star systems not through conventional rocket spacecraft, but through standard-gauge **planetary railways running directly through fixed, ground-based wormholes**. 

The transport monopoly **Compression Space Transport (CST)**, founded by Nigel Sheldon and Ozzie Isaacs, created a cohesive interstellar empire where a commuter can board a passenger train in an arcology on Earth and arrive at a frontier colony world twenty minutes later, or a 100-car heavy freight consist can haul raw iron ore from an untamed pioneer planet directly into an industrial refining world.

The overarching design of OpenSpaceTTD faithfully adapts this vision into OpenTTD's simulation engine. This document establishes:
1. **Nomenclature & Codebase Alignment:** Resolving discrepancies between documentation, C++ engine data types, and Python services.
2. **In-World Commonwealth Lore Mapping:** How traction technologies (steam, diesel, overhead catenary electric, vacuum-tube maglev) and industry chains map across planetary development phases.
3. **Immediate Name/String Updates vs. Scoped Asset Additions:** A clear division between low-effort textual rebrands and medium/high-effort NewGRF/code extensions.
4. **Future Sprints Plan (Sprints 18–21):** Detailed roadmap for GUI integration, server cluster orchestration, and planetary infrastructure.

---

## 2. Nomenclature & Codebase Alignment Audit

To keep the codebase clean and prevent terminology drift, the following naming conventions are established as canonical across all documentation, source code, and scripts:

| Concept | Canonical Term | Code Type / Symbol | Deprecated / Disallowed Terms | Rationale / Context |
|---|---|---|---|---|
| **Logical World Entity** | **World** / **WorldID** | `WorldID` (`src/portal/portal_type.h`) | `PlanetID`, `world_index` | A distinct world server or simulated planet realm. |
| **Spatial Map Partition** | **Planet Region** | `PlanetRegion`, `PlanetManager` | `WorldTileMap`, `SubMap` | The contiguous bounding box of a world within the single-map 4096×4096 coordinate grid. |
| **Developmental Tier** | **World Phase** | `WorldPhase` (`Phase1_Core` .. `Phase4_Expansion`) | `PlanetPhase`, `DevelopmentTier` | Reflects economic role, consumption profile, and infrastructure permissions. |
| **Ecological Climate** | **World Biome** | `WorldBiome` (`Temperate` .. `Oceanic`) | `PlanetClimate`, `LandscapeType` | Defines terrain, foliage, and environmental aesthetics independently of phase. |
| **Wormhole Physical Tile** | **Portal Gate** | `CmdBuildPortalGate`, `PortalEndpoint` | `WormholeStation`, `PortalHead` | The physical rail structure on the ground where trains enter or exit a wormhole. |
| **Intra-Map Portal Pair** | **Portal Link** | `PortalLink`, `PortalRegistry` | `LocalWormhole`, `GatewayLink` | Bidirectional connection between two portal gates on the same physical map grid. |
| **Inter-Server Route** | **Freight Corridor** | `InterServerRoute`, `FreightCorridor` | `InterWorldLink`, `ServerWormhole` | High-throughput trans-world route coordinated by the Universe Authority. |
| **Interstellar Authority** | **Universe Authority** | `UniverseAuthorityService` (C++), `UniverseAuthority` (Python) | `MasterServer`, `TransactionServer` | Authoritative coordinator of player accounts, corporate charters, directories, and trade ledgers. |
| **Core World Metropolis** | **Megacity** | `MegacityManager`, `MegacityProfile` | `SuperCity`, `MetropolisTown` | Large Phase 1 urban centers tracking 3-tier sustained commodity quotas and growth states. |
| **Consist Data Transfer** | **Consist Snapshot** | `ConsistSnapshot`, `ConsistSnapshotUnit` | `TrainStateBytes`, `ConsistPayload` | Serialized wire representation of an in-transit train consist traversing a wormhole. |

### Cleanup Targets Identified
1. **`PlanetManager::GetTileWorld(tile)` vs `PlanetManager::GetRegionByTile(tile)`:** Code is already internally consistent. Ensure documentation consistently uses **"World"** for logical identities and **"Planet Region"** for coordinate bounding boxes.
2. **`InterServerRoute` vs `FreightCorridor`:** In C++, `InterServerRoute` is the underlying struct, while `GetFreightCorridors()` is the API method. This is acceptable; documentation should refer to the gameplay feature as **"Freight Corridors"**.
3. **Consist Spacing vs Wormhole Distance:** In documentation, avoid calling wormhole transit "teleportation" — in both engine physics and Commonwealth lore, trains continuously advance along a 1D virtual wormhole track length (`PORTAL_TRANSIT_DISTANCE` / `virtual_length`).

---

## 3. In-World Commonwealth Lore Alignment

In the Commonwealth Saga, planetary infrastructure follows an evolutionary gradient dictated by distance from Earth (the original Core world), population age, and capital investment.

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                   Commonwealth Planetary Traction & Industry Gradient                 │
└────────────────────────────────────────────────────────────────────────────────────────┘

 [PHASE 3: FRONTIER WORLDS]        [PHASE 2: DEVELOPED / REFINERY]     [PHASE 1: CORE MEGACITIES]
 (e.g. Far Away, Silverglide)      (e.g. Merredin, Clonclurry)         (e.g. Earth, Oaktree, Augusta)
 ────────────────────────────      ───────────────────────────────     ──────────────────────────────
 PRIMARY TRACTION:                 PRIMARY TRACTION:                   PRIMARY TRACTION:
 • Rugged Pioneer Steamers (Coal)  • Heavy Overhead Catenary Electric  • CST Vacuum-Tube Maglev (Vactrain)
 • Heavy Planetary Diesels         • Multi-Unit Freight Electrics      • High-Speed Linear Induction EMUs
 • Rough-terrain bulk haulers      • Regional Intermodal Freighters    • Multi-track Subterranean Terminals
                                                                       
 INDUSTRIAL ROLE:                  INDUSTRIAL ROLE:                    INDUSTRIAL ROLE:
 • Biomass & Grain Bio-Farms       • Alloy Smelters & Foundries        • High-Tech Nanotech & Optical Fabs
 • Open-cast Heavy Ore Mines       • Petrochemical & Polymer Plants    • Rejuvenation & Genetic Clinics
 • Timber logging camps            • Packaged Nutrient Synthesizers    • Arcology Consumer Megastores
 • Raw mineral siphons             • Heavy Machinery Assemblers        • Massive 3-Tier Sustained Demand
                                                                       
 EXPORT FLOWS:                     EXPORT FLOWS:                       EXPORT FLOWS:
 • Raw Grain, Biomass, Ore, Coal   • Structural Alloys, Synthetics,    • Neural Processors, Rejuvenation
   via CST Gateway Corridors ────►   Machine Modules, Nutrients ─────►   Therapies, Capital Machinery ───►
```

### 3.1 Traction & Train Technology Tiers

#### Tier 1: Pioneer Steam & Coal Traction (Phase 3 Frontier)
- **Commonwealth Context:** In the early colonial expansion or on low-infrastructure backwaters (e.g. initial settlement of Far Away or rural outposts on Silverglide), electrical catenary infrastructure and high-tech maintenance depots do not exist. Pioneer settlements rely on rugged, mechanical, coal- and biomass-burning steam locomotives to establish initial trackheads, haul timber, and clear land.
- **Role in OpenSpaceTTD:** Starting traction on Phase 3 worlds. Low initial purchase cost, tolerates rugged track, high tractive effort at low speeds, high fuel running cost.

#### Tier 2: Heavy Planetary Industrial Diesel (Phase 3 & Phase 2 Outer Corridors)
- **Commonwealth Context:** The heavy freight workhorse of the outer Commonwealth. Massive 4,000+ HP multi-axle diesel locomotives hauling 100-car consists of iron ore, bauxite, raw silicates, and grain across thousands of kilometers of un-electrified planetary plains directly to gateway staging terminals.
- **Role in OpenSpaceTTD:** Long-distance bulk hauler. Immune to catenary infrastructure requirements; ideal for connecting remote extraction sites to portal throat terminals.

#### Tier 3: High-Voltage Overhead Catenary Electric (Phase 2 Industrial Backbone)
- **Commonwealth Context:** The industrial standard of the Commonwealth. Phase 2 manufacturing worlds operate vast electrified networks. High-acceleration multi-unit freight locomotives haul containers, tank cars of polymers, and structural steel between smelters, assemblers, and inter-world freight corridors.
- **Role in OpenSpaceTTD:** High-throughput regional corridors. Rapid acceleration, high top speed (160–200 km/h), requires electrified catenary rails, low maintenance overhead.

#### Tier 4: CST Vacuum-Tube Maglev / Vactrains (Phase 1 Core Worlds)
- **Commonwealth Context:** The signature technology of CST and the Commonwealth core worlds. Running in evacuated subterranean tubes and aerodynamically enclosed elevated viaducts, CST vactrains and superconducting maglevs achieve speeds between 400 km/h and 1,000+ km/h. They connect planetary arcologies with inter-world wormhole portals, carrying hundreds of thousands of commuters and containerized prosperity goods daily with zero aerodynamic drag.
- **Role in OpenSpaceTTD:** Ultra-high-speed transit exclusive to Phase 1 worlds. Massive capacity, extreme speed, premium ticket revenue, high construction cost, requires dedicated Maglev/Vacuum depots and tracks.

---

## 4. Immediate String Changes vs. Scoped Asset Additions

To achieve lore alignment efficiently, work is divided into two distinct tracks: **Track A** (pure string/name updates, zero C++ code changes, zero artist dependencies) and **Track B** (scoped gameplay extensions and custom NewGRF assets).

### 4.1 Track A: Pure Name & String Changes (Immediate / Low Effort)

These changes update `src/lang/english.txt` and string tables directly without requiring NewGRF compilation or breaking savegame compatibility.

#### 1. Vehicle Rebrands (`src/lang/english.txt`)
| Current Engine Name | Lore-Aligned Commonwealth Name | Traction Class | Target Phase |
|---|---|---|:---:|
| **Kirby Paul Tank (Steam)** | **CST Pioneer 0-6-0 'Surveyor' (Steam)** | Steam | Phase 3 |
| **Wills 2-8-0 (Steam)** | **Vulcan 2-8-0 'Frontier Hauler' (Steam)** | Steam | Phase 3 |
| **Chaney 'Jubilee' (Steam)** | **Commonwealth 'Jubilee' 4-6-0 (Steam)** | Steam | Phase 3 |
| **Ginzu 'A4' (Steam)** | **CST 'A4' Express Streamliner (Steam)** | Steam | Phase 3/2 |
| **SH '8P' (Steam)** | **Sheldon-Isaacs '8P' Heavy Freight (Steam)** | Steam | Phase 3 |
| **CS 4000 (Diesel)** | **CST D-4000 Heavy Planetary Diesel** | Diesel | Phase 3/2 |
| **CS 2400 (Diesel)** | **Overland 2400 General Utility Diesel** | Diesel | Phase 3 |
| **Centennial (Diesel)** | **Titan D-100 Twin-Engine Hauler (Diesel)** | Diesel | Phase 3 |
| **Kelling 3100 (Diesel)** | **Frontier Freight 3100 Diesel** | Diesel | Phase 3 |
| **SH '125' (Diesel)** | **CST High-Speed Inter-World Diesel** | Diesel | Phase 2 |
| **SH '30' (Electric)** | **CST E-30 Heavy Catenary Freight** | Electric | Phase 2 |
| **SH '40' (Electric)** | **CST E-40 Inter-World Catenary Hauler** | Electric | Phase 2 |
| **'T.I.M.' (Electric)** | **Metropolitan Rapid Commuter EMU** | Electric | Phase 2/1 |
| **'AsiaStar' (Electric)** | **Commonwealth Star Passenger Express** | Electric | Phase 2/1 |
| **'X2001' (Electric)** | **CST Trans-World High-Speed Electric** | Electric | Phase 1 |
| **Lev1 'Leviathan'** | **CST Mark I 'Leviathan' Vacuum Maglev** | Maglev | Phase 1 |
| **Lev2 'Cyclops'** | **CST Mark II 'Cyclops' Heavy Maglev Hauler** | Maglev | Phase 1 |
| **Lev3 'Pegasus'** | **CST Mark III 'Pegasus' High-Speed Maglev** | Maglev | Phase 1 |
| **Lev4 'Chimaera'** | **CST Mark IV 'Chimaera' Hyper-Maglev Express** | Maglev | Phase 1 |

#### 2. Wagon & Carriage Rebrands (`src/lang/english.txt`)
| Current Wagon Name | Lore-Aligned Commonwealth Name | Primary Cargo Role |
|---|---|---|
| **Armoured Van** | **Secure Data Crystal & Valuables Van** | Data Crystals, Valuables, High-Tech |
| **Goods Van** | **Manufactured Goods & Colony Supplies Van** | Intermediate manufactured goods |
| **Grain Hopper** | **Bio-Grain & Biomass Hopper** | Raw agricultural feedstocks |
| **Iron Ore Hopper** | **Mineral & Heavy Ore Hopper** | Metallic ores, bauxite, silicates |
| **Food Van** | **Synthesized Food & Provisions Van** | Packaged foodstuffs, rations |
| **Passenger Carriage** | **Commonwealth Passenger Coach** | Inter-world travelers, colonists |

#### 3. Rail Infrastructure Labels (`src/lang/english.txt`)
| Current Label | Lore-Aligned Commonwealth Label | In-Game Description |
|---|---|---|
| **Railroad** | **Conventional Pioneer Track** | Standard-gauge un-electrified rails for steam & diesel. |
| **Electrified Railroad** | **Catenary Heavy Rail** | High-voltage overhead electrified rail for industrial lines. |
| **Monorail** | **High-Speed Guided Rail** | Grade-separated intermediate high-speed network. |
| **Maglev** | **CST Vacuum / Maglev Guideway** | Superconducting magnetic levitation for Phase 1 Core Worlds. |
| **Train Depot** | **Rail Operations Depot** | Maintenance & consist assembly facility. |

#### 4. Industry & Station Rebrands (`src/lang/english.txt`)
| Current Industry Name | Lore-Aligned Commonwealth Name | Economic Role |
|---|---|---|
| **Coal Mine** | **Carbon & Mineral Extraction Pit** | Fuel for steam traction and carbon feedstock. |
| **Power Station** | **Planetary Fusion Substation** | Power grid distribution node. |
| **Iron Ore Mine** | **Heavy Metallurgical Ore Mine** | Raw ore extraction for smelting. |
| **Steel Mill** | **Alloy Smelting & Foundry Complex** | Structural steel and alloy production. |
| **Factory** | **Heavy Manufacturing & Assembly Fab** | Assembles finished manufactured goods. |
| **Farm** | **Bio-Agricultural Complex / Agri-Plains** | Large-scale biomass and grain harvesting. |
| **Food Processing Plant**| **Nutrient Synthesis & Packaging Facility**| Converts biomass into packaged rations. |
| **Oil Refinery** | **Petrochemical & Polymer Cracker** | Produces plastics, chemicals, and fuels. |

---

### 4.2 Track B: Scoped Gameplay & Asset Additions (Medium / High Effort)

These items require custom NewGRF development (in NML), custom pixel artwork, or engine feature expansion.

#### 1. Advanced Commonwealth Industry Chains (`openspacettd_industries.grf`)
A dedicated in-tree industry set replacing default industries with a cohesive 3-tier production chain:

```
[PHASE 3: FRONTIER EXTRACTION]
  ├─ Deep Carbon Pit ──────────► Coal / Carbon ────────┐
  ├─ Metallurgical Mine ───────► Raw Heavy Ore ────────┼─► [PHASE 2: REFINERY & INDUSTRIAL]
  ├─ Silicate Quarry ──────────► Raw Silicates ────────┼─►   ├─ Alloy Smelter ──────► Structural Superalloys
  ├─ Bio-Agricultural Plains ──► Raw Biomass & Grain ──┼─►   ├─ Chemical Cracker ───► Synthetic Polymers
  └─ Hydrocarbon Well ─────────► Crude Hydrocarbons ───┘     ├─ Nutrient Synthesizer► Packaged Nutrients
                                                             └─ Component Fab ──────► Machinery Modules
                                                                                          │
                                                                   ┌──────────────────────┘
                                                                   ▼
                                                       [PHASE 1: CORE MEGACITIES]
                                                         ├─ Optical & Nanotech Fab ──► Neural Processors
                                                         ├─ Rejuvenation Clinic ─────► Anti-Senescence Therapies
                                                         └─ Megacity Consumer Hub ───► Immense 3-Tier Consumption
```

- **Cargo Definitions:**
  - `BIOM` (Biomass), `GRAI` (Bio-Grain), `ORE_` (Heavy Ore), `SILC` (Silicates), `COAL` (Carbon).
  - `ALLO` (Superalloys), `POLY` (Synthetic Polymers), `FOOD` (Packaged Nutrients), `MACH` (Machinery Modules).
  - `PROC` (Neural Processors), `REJV` (Anti-Senescence Therapies), `DATA` (Data Crystals), `LUXU` (Luxury Goods).
- **Megacity Consumption Mapping:**
  - **Tier 1 (Sustenance):** `FOOD` (Packaged Nutrients), `WATR` (Purified Water).
  - **Tier 2 (Expansion):** `ALLO` (Superalloys), `MACH` (Machinery Modules), `POLY` (Polymers).
  - **Tier 3 (Prosperity):** `PROC` (Neural Processors), `REJV` (Anti-Senescence), `DATA` (Data Crystals).

#### 2. Custom Rolling Stock Asset Pack (`openspacettd_rail.grf`)
- **CST Vacuum-Tube Passenger & Freight Consists:** Sleek, aerodynamic, capsule-like trains with glowing blue/cyan energy strips and transparent passenger observation viewports.
- **Frontier Pioneer Steamers:** Heavy, rugged, double-chimney locomotives with massive cowcatchers, oversized headlamps, and high-capacity coal tenders.
- **Planetary Multi-Unit Diesels:** Angular, industrial 6-axle and 8-axle diesel locomotives built for harsh alien atmospheric conditions.
- **Articulated Mineral Hoppers & Tankers:** Specialized wagons matching the Commonwealth industrial aesthetic.

#### 3. Custom CST Wormhole Portal Gate Tile Graphics
- Replace standard stone/brick tunnel portals with a monumental CST Gateway archway:
  - Reinforced titanium-carbide pylon towers flanking the tracks.
  - An active, shimmering blue/violet wormhole force-field horizon inside the portal frame.
  - Integrated overhead catenary conduits, warning beacons, and PBS signal gantries mounted on the portal lintel.

#### 4. Megacity Arcology Tile Graphics
- Custom urban building sprites for Phase 1 Core Worlds:
  - Towering, multi-tile modular arcologies (50–100 stories).
  - Elevated skybridge walkways and monorail guideways between structures.
  - Holographic projection signage and illuminated transit concourses.

---

## 5. Future Sprints Roadmap (Sprints 18–21)

```text
Sprint 17 (DONE)             Sprint 18                   Sprint 19                   Sprint 20                   Sprint 21
┌──────────────────────┐    ┌─────────────────────┐    ┌──────────────────────┐    ┌─────────────────────┐    ┌──────────────────────┐
│ Phase F4 Complete    │    │ In-Game GUI         │    │ Server Cluster       │    │ Planetary Conduit & │    │ Commonwealth Content │
│ Megacity Quotas      │───►│ Integration         │───►│ Orchestration        │───►│ Spaceport Bridge    │───►│ Pack (NewGRF & Art)  │
│ Corridor Congestion  │    │ Megacity & Corridor │    │ Supervisor Daemons   │    │ Spaceports & Conduits│   │ NML Industries       │
│ Supply Chain Matrix  │    │ Windows on Toolbar  │    │ Multi-Instance Clust │    │ feed Freight Queues │    │ Custom Sprites       │
└──────────────────────┘    └─────────────────────┘    └──────────────────────┘    └─────────────────────┘    └──────────────────────┘
```

### Sprint 18: In-Game GUI Integration for Federation & Megacities
- **Goal:** Replace console-only administrative access with native OpenTTD GUI windows accessible from the top toolbar and town windows.
- **Deliverables:**
  1. **Megacity Overview Window:**
     - Displays 3-tier delivery progress bars (Sustenance, Expansion, Prosperity).
     - Live satisfaction percentages, current monthly quota progress, and active growth multiplier badge (Starvation $0.0\times$ / Subsistence $1.0\times$ / Boom $1.5\times$ / HyperGrowth $2.0\times$).
     - Direct button in the Town View window: *"View Megacity Status"*.
  2. **Freight Corridor Monitor Window:**
     - Toolbar button opening the list of active inter-server freight corridors.
     - Displays route endpoints (Source World/Gate $\to$ Dest World/Gate), active in-transit train count, utilization gauge ($U$), congestion status badge (`Clear`, `Moderate`, `Congested`, `Saturated`), and effective transit delay multiplier.
  3. **Universe Directory Browser Window:**
     - In-game server browser displaying registered federation world servers with ping, client load, train counts, and phase badges (`Phase 1: Core` through `Phase 4: Expansion`).
- **Tests:** GUI window creation and widget event handler unit tests (`test_window_desc.cpp`, `test_megacity_gui.cpp`).

### Sprint 19: Dedicated Server Cluster Orchestration & Daemons
- **Goal:** Provide production-grade automation to launch, monitor, and manage multi-instance OpenSpaceTTD server clusters.
- **Deliverables:**
  1. **Cluster Supervisor Script (`scripts/run_cluster.py`):**
     - Launches the `UniverseAuthority` daemon on port 8080.
     - Spawns multiple headless OpenSpaceTTD dedicated servers (`./build/openttd -D`) on distinct ports (e.g. Server 1 on 3979, Server 2 on 3980, Server 3 on 3981).
     - Automated heartbeat sender maintaining live server directory registration.
  2. **Automated Topology Bootstrapping:**
     - Config file (`cluster.json`) defining world IDs, phases, server ports, and initial portal route links.
     - Automatically provisions inter-server portal links upon server startup.
  3. **Health Monitoring & Failover:**
     - Supervisor detects crashed or hanging servers, triggers recovery transfers into quarantine bays, and restarts instances automatically.
- **Tests:** End-to-end 3-server cluster test script validating automated launch, cross-server train transfer, and clean shutdown.

### Sprint 20: Planetary Infrastructure Integration (Spaceports & Edge Conduits)
- **Goal:** Unify single-map planetary infrastructure (Spaceports, Edge Conduits) with the multi-server federation transport system.
- **Deliverables:**
  1. **Spaceport Interplanetary Routing Bridge:**
     - Allow Spaceport off-world trade deliveries to be dispatched into Universe Authority freight corridors, transferring cargo to destination world spaceports or megacity distribution terminals.
  2. **Edge Conduit Inter-Server Feeders:**
     - Allow mineral extraction from planetary edge conduits to directly pipe raw bulk cargo into inter-world freight queues without intermediate rail handling if desired.
  3. **Unified Supply Chain Attribution:**
     - Spaceport and conduit throughput recorded in the `EmpireSupplyChainMatrix`.
- **Tests:** Catch2 tests verifying spaceport cargo conversion into consist transfer records.

### Sprint 21: Commonwealth Saga Content & Asset Alignment Pack
- **Goal:** Implement the Commonwealth Saga industry chains and specialized rolling stock in a curated, in-tree NewGRF.
- **Deliverables:**
  1. **Execute Track A String Rebrands:** Update `src/lang/english.txt` with all approved engine, wagon, cargo, and industry names.
  2. **`assets/grf/openspacettd_industries.nml`:** Compile the 12-cargo Commonwealth industry economy.
  3. **`assets/grf/openspacettd_rail.nml`:** Custom sprites for CST Vacuum Maglevs, heavy planetary diesels, and pioneer steamers.
  4. **Custom Wormhole Portal Archway Sprites:** Replace default tunnel mouth graphics.
- **Tests:** NewGRF regression compilation tests and cargo chain delivery validation.
