# OpenSpaceTTD: Commonwealth World Graph & Late-Game Multi-World Expansion Specification

**Status:** CANONICAL DESIGN & ARCHITECTURE  
**Date:** 2026-09-18  
**Governing Lore:** Peter F. Hamilton *Commonwealth Saga* (*Pandora's Star*, *Judas Unchained*)  
**Source of Truth:** [`docs/world_roadmap.txt`](world_roadmap.txt)  
**Prerequisites:** Sprints 1–42 Core Engine Baseline, [`AGENTS.md`](../AGENTS.md) Engine Invariants  

---

## 1. Executive Overview & Design Philosophy

In Peter F. Hamilton's *Commonwealth Saga*, humanity did not conquer the stars through interstellar spacecraft or hyperdrives. Instead, human expansion was forged by **Compression Space Transport (CST)**, founded by Nigel Sheldon and Ozzie Isaacs, who established a contiguous, standard-gauge planetary railway network running across interstellar distances through **fixed, ground-based wormholes**.

A citizen on Earth can board a high-speed commuter train at St. Pancras or L.A. Galactic and disembark twenty minutes later on an alien frontier world forty light-years away. Heavy freight trains carrying thousands of tons of raw iron ore or bio-crops cross directly from pioneer worlds into massive industrial refinery planets.

[`docs/world_roadmap.txt`](world_roadmap.txt) establishes the canonical reconstruction of this universe:
- **108 Primary Worlds & Locations:** Categorized by developmental phases (Phase 1 Core, Phase 2 Developed/Industrial, Phase 3 Frontier, and Phase 4 Expansion/Wilderness).
- **Strict Evidence Classifications:** Distinguishing Explicit canonical routes ($E$), Reference-supported Phase 1 Earth-hub routes ($R$), Inferred sector routes ($I$), and Assumed routes ($A$) necessary for complete gameplay topology.
- **Rich Multi-Modal Transport Topologies:** Moving beyond simple railway tracks to embrace scheduled power cycles, data-only relays, orbital lighters, and mystical Silfen alien paths.

This document defines the **late-game expansion architecture** for OpenSpaceTTD, allowing a player who has mastered their starting planetary systems to expand into the wider Commonwealth, trade with prebuilt lore economies, survey untamed frontier worlds, and build colonies across Phase 3 and Phase 4 worlds.

---

## 2. Engine Scale & 3-Tier Hybrid Universe Architecture

### 2.1 The Architectural Challenge
OpenSpaceTTD operates under strict engine rules codified in [`AGENTS.md`](../AGENTS.md):
1. **Preserve `TileIndex`:** The engine's map grid is capped at $4096 \times 4096$ contiguous tiles ($16,777,216$ tiles).
2. **Deterministic Simulation:** All vehicles, signals, commands, and economy cycles must execute deterministically across ticks.
3. **Wormhole Traversal:** Trains navigate between distant tiles via `Track::Wormhole` and YAPF `CFollowTrackRail` leap traversal.

If all 108 Commonwealth worlds were crammed simultaneously onto a single $4096 \times 4096$ map, each world would be restricted to approximately $200 \times 200$ tiles—far too small for Factorio-scale railway networks, balloon loops, and sprawling arcologies.

### 2.2 The 3-Tier Solution

To resolve this, OpenSpaceTTD employs a **3-Tier Hybrid Universe Architecture**:

```
+---------------------------------------------------------------------------------------------------+
|                                 3-TIER HYBRID UNIVERSE TOPOLOGY                                   |
+---------------------------------------------------------------------------------------------------+
|                                                                                                   |
|  [TIER 1: ACTIVE LOCAL SECTOR MAP] (4096 x 4096 In-Memory Coordinate Space)                       |
|  +------------------------+      +------------------------+      +------------------------+       |
|  |  Planet Region 1       |      |  Planet Region 2       |      |  Planet Region 3       |       |
|  |  (e.g. Earth / Sol)    |<====>|  (e.g. Merredin)       |<====>|  (e.g. Calyx Frontier) |       |
|  |  Local Mega-Terminals  | Gate |  Industrial Foundries  | Gate |  Raw Ore Mines         |       |
|  +------------------------+      +------------------------+      +------------------------+       |
|                                                                                                   |
|                                      ▲ Portal Gate Traversal                                      |
|                                      ▼ (Consist Snapshot / Snapshot v2)                           |
|                                                                                                   |
|  [TIER 2: UNIVERSE AUTHORITY HUB] (Centralized State Coordinator & Commodity Conservation Ledger) |
|  • Tracks Universe Graph (108 Nodes + Edges)       • Enforces Conservation of Consists & Cargo    |
|  • Dynamic World Directory & Galaxy Browser        • Economic Clearinghouse & Tariff Balances     |
|                                                                                                   |
|                                      ▲ Inter-Server Transport & Clearinghouse                     |
|                                      ▼                                                            |
|                                                                                                   |
|  [TIER 3: OFF-WORLD FEDERATION & SIMULATED LORE ECONOMIES]                                        |
|  +-------------------------------+  +-------------------------------+  +-----------------------+  |
|  | Dedicated Federation Servers  |  | Prebuilt Simulated Lore Worlds|  | Unexplored Wilderness |  |
|  | (Live 'openttd -D' processes) |  | (Simulated Demand Endpoints)  |  | (Dormant World Seeds) |  |
|  | • Multi-server multiplayer    |  | • Big15 Core Hubs (Augusta)   |  | • Phase 4 Expansion   |  |
|  | • Live remote consists        |  | • Buys Steel/Chips; Sells Meds|  | • Survey & Colonise   |  |
|  +-------------------------------+  +-------------------------------+  +-----------------------+  |
|                                                                                                   |
+---------------------------------------------------------------------------------------------------+
```

1. **Tier 1: Active Local System (In-Memory Map):**
   - The player's active game session runs 3 to 6 active `PlanetRegion`s on the $4096 \times 4096$ map, separated by void tiles.
   - Within this local system, the player has complete microscopic construction control (tracks, depots, signals, stations, factory facilities).
2. **Tier 2: Universe Authority Hub (`scripts/universe_authority.py` / `src/portal/universe_authority.h`):**
   - Coordinates global player charters, directory discovery, commodity conservation ledgers, and trade balances.
   - Ingests the authoritative Commonwealth graph schema and enforces gate access policies.
3. **Tier 3: Distributed Federation & Simulated Lore Trading Partners:**
   - **Simulated Prebuilt Lore Worlds:** Established Commonwealth worlds (e.g. Big15 Core hubs like Augusta, Wessex, DRNG, or industrial hubs like Gralmond) can be linked as external trade gateways without needing dedicated server processes. The Universe Authority simulates their consumption, credits trade revenue, and returns scheduled inbound consists.
   - **Live Federation Servers:** In multiplayer clusters, external worlds run as separate `openttd -D` server instances, exchanging physical consists via the Sprint 35 cross-process federation protocol.
   - **Unexplored Phase 4 Wilderness:** Dormant procedural seeds that can be staged into local map space or spun up as new server instances when the player initiates a colonial megaproject.

---

## 3. Commonwealth Graph Data Schema

To maintain complete fidelity with `docs/world_roadmap.txt`, the universe graph is serialized into a machine-readable JSON structure (`assets/data/commonwealth_universe.json`).

### 3.1 World Node Schema
Each world record preserves its canonical identity, phase, biome, and evidentiary source:

```json
{
  "world_id": "world_merredin",
  "canonical_name": "Merredin",
  "display_name": "Merredin",
  "phase": "Phase2_Developed",
  "phase_evidence": "R",
  "biome": "AridDesert",
  "location_type": "colony_world",
  "system": "Merredin",
  "era": "pre_invasion",
  "source_ids": ["S01", "S02", "S05"],
  "economic_profile": {
    "role": "heavy_refining_and_fabrication",
    "primary_imports": ["IRON_ORE", "SILICA_SAND", "FOOD"],
    "primary_exports": ["STRUCTURAL_STEEL", "SUPERALLOYS", "MACHINE_MODULES"],
    "base_population": 4500000,
    "megacity_eligible": false
  }
}
```

### 3.2 Connection Edge Schema
Connections preserve their mode, evidence class, availability, and physical constraints:

```json
{
  "connection_id": "conn_wessex_boongate",
  "endpoint_a": "world_wessex",
  "endpoint_b": "world_boongate",
  "connection_type": "RAIL",
  "evidence_class": "E",
  "source_ids": ["S06"],
  "inference_or_assumption_id": "NONE",
  "availability": "PERMANENT",
  "access_policy": "PUBLIC",
  "permits_through_running_train": true,
  "virtual_length_tiles": 32,
  "enabled_in_game": true
}
```

### 3.3 Graph Topology Modes
The engine supports three operational graph modes:
1. **`SOURCE_ONLY`:** Includes only Explicit ($E$) and Reference-supported ($R$) connections. Disconnected worlds without documented routes remain isolated until player discovery.
2. **`RECONSTRUCTED`:** Includes $E, R,$ and Inferred ($I$) connections (such as Verona $\leftrightarrow$ Saville and Wessex $\leftrightarrow$ Elan).
3. **`PLAYABLE_COMPLETION` (Default Gameplay Mode):** Includes $E, R, I,$ and Assumed ($A$) connections, creating a coherent, fully traversable branching tree rooted at Earth/Sol and the Big15 hubs. Every assumed link is visibly badged in the UI to maintain clear separation from Hamilton canon.

---

## 4. Multi-Modal Gateway Mechanics

In the *Commonwealth Saga*, wormholes are not all identical standard-gauge railway tracks. The engine implements specialized mechanics for each mode defined in Section 0 and Section 7 of `docs/world_roadmap.txt`:

```
+---------------------------------------------------------------------------------------------------+
|                                 MULTI-MODAL GATEWAY TYPOLOGY                                      |
+-------------------+-----------------------------+-------------------------------------------------+
| Mode              | Novel Precedent             | OpenSpaceTTD Engine Implementation              |
+-------------------+-----------------------------+-------------------------------------------------+
| **RAIL**          | Earth <-> Verona / Wessex   | Standard CST high-capacity portal gate. Direct  |
|                   |                             | consist traversal, PBS signalling, 400 km/h.    |
+-------------------+-----------------------------+-------------------------------------------------+
| **GATE**          | Boongate <-> Half Way       | Terminal break-of-gauge. Consists terminate at  |
|                   |                             | a transfer terminal; cargo is cross-docked.     |
+-------------------+-----------------------------+-------------------------------------------------+
| **PRIVATE**       | Cressat, Solidade,          | Locked behind company reputation, tech tree     |
|                   | Ozzie's Asteroid, Hardrock  | research ("Consortium Charter"), or gate tolls. |
+-------------------+-----------------------------+-------------------------------------------------+
| **EXPLORATION**   | Chelva, Tandil, Gaczyna     | Uninhabited / hazardous. Requires Expeditionary |
|                   |                             | Survey Consists to scan and anchor gate heads.  |
+-------------------+-----------------------------+-------------------------------------------------+
| **SILFEN_ROUTE**  | Silvergalde, Jaruva,        | Alien mystical paths. Trains cannot transit.    |
|                   | Ice Citadel World, Jandk    | Requires Intermodal Depots & container pods.    |
+-------------------+-----------------------------+-------------------------------------------------+
| **ORBITAL /       | Kerensk <-> High Angel      | Orbital wormhole to High Angel gateway docks;   |
|   SHUTTLE**       | (Orbital Starship Habitat)  | local aerospace shuttles transfer passengers.   |
+-------------------+-----------------------------+-------------------------------------------------+
```

### 4.1 Staging Loops & Congestion Sidings
- **Freight Operation Context:** High-traffic inter-world corridors (such as the route to Far Away or Augusta) require automated staging loops and overflow sidings to manage peak throughput without mainline gridlock.
- **Gameplay Implementation:**
  - Automated staging loop integration with YAPF pathfinding.
  - Trains divert into designated holding sidings when downstream portals or terminals are occupied.

### 4.2 Universal Rail Freight Connectivity
- All standard Commonwealth trade nodes (including frontier and remote worlds) support physical rail freight and trade gateway interchange, ensuring robust logistics operations without artificial gate power shutdowns or non-train data gimmicks.

### 4.3 Silfen Path Intermodal Depots (Silvergalde & Jaruva)
- **Lore Context:** The Silfen paths are mystical, extra-dimensional trails through alien forests that connect distant worlds without human technology. Standard trains cannot run along them.
- **Gameplay Implementation:**
  - Trains terminate at a specialized **Silfen Path Intermodal Depot**.
  - Cargo (Biomass, Rare Earths, Data Crystals) is containerized into low-impact pods and transferred across the Silfen network, re-emerging at remote Silfen path stations on Jaruva or the Ice Citadel world.

### 4.4 Multi-Leg Terminal Routes (Do Not Flatten)
Section 7A of `docs/world_roadmap.txt` specifies the exact chain of custody for reaching Far Away:
```
Wessex (CST Rail Gate)
  └──> Boongate (CST Interchange)
         └──> Half Way / Shackleton (Wormhole Gate)
                └──> Half Way / Port Evergreen (Local Aircraft Transfer)
                       └──> Far Away / Armstrong City (Scheduled Stormrider Gate)
```
OpenSpaceTTD models this as an **intermodal logistics chain**: trains cannot run uninterrupted from Wessex to Far Away. The player must coordinate railway lines on Wessex, an interchange on Boongate, an atmospheric air shuttle across Half Way, and scheduled staging yards at Port Evergreen.

---

## 5. Late-Game Player Expansion Progression Loop

```
+---------------------------------------------------------------------------------------------------+
|                             LATE-GAME EMPIRE EXPANSION LIFECYCLE                                  |
+---------------------------------------------------------------------------------------------------+
|                                                                                                   |
|  [STAGE 1: STARTING EMPIRE HEGEMONY]                                                              |
|  • Build self-sufficient railway networks across 3–4 local starting worlds.                       |
|  • Close the four 12-cargo industrial pipelines (Structural, Electronics, Propulsion, Crystals).  |
|  • Establish Logistics Hubs and In-Kind Fabrication depots; research Tech Tree Tiers 1–3.        |
|                                                                                                   |
|                                                ▼                                                  |
|                                                                                                   |
|  [STAGE 2: COMMONWEALTH TRADE INTEGRATION]                                                        |
|  • Open the Universe Directory; discover prebuilt Big15 Core Hubs (Augusta, Wessex, Verona).      |
|  • Construct High-Capacity Portal Terminals using CST Double-Track Prefabs.                       |
|  • Export Superalloys and Silicon Wafers to Core Arcologies; earn massive tariff revenues.        |
|  • Import high-margin Commuter Passengers, Rejuvenation Therapies, and Machine Modules.           |
|                                                                                                   |
|                                                ▼                                                  |
|                                                                                                   |
|  [STAGE 3: FRONTIER EXPLORATION & MULTI-MODAL LOGISTICS]                                          |
|  • Outfit Expeditionary Survey Consists (Mobile Lab, Pioneer Tracklayer, Shielded Bogies).        |
|  • Survey rugged Phase 3 nodes (Chelva, Tandil) to discover deep-vein deposits.                   |
|  • Solve complex terminal bottlenecks: Half Way aircraft link & Far Away stormrider staging.      |
|  • Negotiate Private Gate Charters to reach Cressat, Solidade, and Ozzie's Asteroid.              |
|                                                                                                   |
|                                                ▼                                                  |
|                                                                                                   |
|  [STAGE 4: COLONIAL MEGAPROJECTS ON PHASE 4 WILDERNESS]                                           |
|  • Claim uncolonized Phase 4 Expansion sectors branching off outer Phase 3 worlds.                |
|  • Multi-Tier Delivery Megaproject:                                                               |
|    - Tier 1 (Outpost Founding): Deliver Bio-Domes, Life Support, Water -> Spawns Settlement.      |
|    - Tier 2 (Industrial Genesis): Deliver Heavy Machinery, Steel -> Spawns Mines & Smelters.      |
|    - Tier 3 (Phase Promotion): Deliver Nanotech & Electronics -> Promotes to Phase 2/1 Arcology.  |
|                                                                                                   |
|                                                ▼                                                  |
|                                                                                                   |
|  [STAGE 5: GALACTIC HEGEMONY & ANOMALY MASTERY]                                                   |
|  • Link High Angel Orbital Docks; trade with exotic alien starships for prototype technologies.   |
|  • Establish Silfen path transshipment depots across Silvergalde, Jaruva, and the Ice Citadel.    |
|  • Coordinate a 108-world trade empire balancing freight corridor congestion and global tariffs.  |
|                                                                                                   |
+---------------------------------------------------------------------------------------------------+
```

---

## 6. Future Sprints Scoping & Alignment

This design integrates cleanly into the post-recovery roadmap by establishing dedicated future sprints:

### 6.1 Alignment with Sprints 43–48
- **Sprint 46 (Seamless Multi-Server Federation Universe):** Supplies the low-level distributed process protocol for exchanging physical trains between live servers.
- **Sprint 47 (Colonial Megaprojects & Arcology Metropolises):** Supplies the settlement promotion mechanics and arcology evolution rules.

### 6.2 Dedicated Commonwealth Expansion Sprints (Sprints 49–52)

#### Sprint 49: Commonwealth Graph Engine & Prebuilt Lore Economies
- **Scope:**
  1. JSON parser for `assets/data/commonwealth_universe.json` into `UniverseGraphManager`.
  2. Universe Directory Galaxy Browser GUI (`WID_UD_GALAXY_MAP`) rendering the full branching tree from Earth/Sol through the Big15 hubs.
  3. Prebuilt simulated off-world trading endpoints: local gates can link to unsimulated Commonwealth worlds, generating realistic export consumption and scheduled return consists.

#### Sprint 50: Multi-Modal Gateway Operations & Dynamic Gate Cycles
- **Scope:**
  1. Implementation of cyclic scheduled gates (Far Away 5h/15h stormrider cycle) with automated signal gating and holding loop staging.
  2. Subspace data-only wormholes (Vinmar) converting station throughput into Tech Tree Research Points.
  3. Private and diplomatic gate licensing (Cressat, Solidade, Ozzie's Asteroid, Hardrock).

#### Sprint 51: Expeditionary Survey Logistics & Silfen Intermodal Paths
- **Scope:**
  1. Expeditionary Survey Trains: specialized rolling stock (survey lab, hazard shielding, pioneer tracklayer) required to commission gates on unexplored worlds (Chelva, Tandil).
  2. Silfen Path Intermodal Depots: automated containerization facilities on Silvergalde and Jaruva.
  3. Kerensk / High Angel orbital interface and spacecraft shuttle transshipment.

#### Sprint 52: Galactic Commonwealth Hegemony & Narrative Lore Scenarios
- **Scope:**
  1. Global 108-world economic clearinghouse: dynamic freight tariffs, corridor congestion surcharges, and inter-world trade balance sheets.
  2. Narrative Commonwealth scenarios: *The Dyson Alpha Quarantine*, *Anshun Generator Retargeting*, *The Evacuation of Elan*.

---

## 7. Engine Invariants & Verification Boundaries

1. **`TileIndex` Preservation:** All local map construction occurs strictly within the existing $4096 \times 4096$ coordinate space. Off-map worlds are handled via Universe Authority proxy endpoints or distributed server processes.
2. **Deterministic Traversal:** Wormhole crossing duration ($\text{virtual\_length}$) and cached consist exit endpoints preserve deterministic simulation across all game ticks.
3. **Conservation of Cargo & Consists:** Consists in transit are tracked with durable transaction IDs (`FTJR` chunk) and commodity conservation ledgers, guaranteeing zero duplication and zero cargo loss across all transitions.
