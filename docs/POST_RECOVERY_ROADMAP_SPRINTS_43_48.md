# OpenSpaceTTD: Post-Recovery Strategic Roadmap (Sprints 43–48)

**Status:** ALL STRATEGIC SPRINTS 43–48 IMPLEMENTED & MERGED INTO MAIN
**Date:** 2026-09-21
**Governing Architecture:** Peter F. Hamilton *Commonwealth Saga* + Factorio-Scale Production + OpenTTD Determinism
**Next priority:** Live federation with two multiplayer clients; human UAT and current graphics accepted by the user on 23 September 2026.

---

## 1. Executive Summary & Design Alignment

Through the `/grill-me` architectural interview, the long-term vision for OpenSpaceTTD was codified into an actionable, multi-phase roadmap spanning Sprints 43–48. As of September 2026, **all six strategic sprints (43–48) have been fully implemented, verified, and merged into `main`** alongside the Horizon A multi-node cluster testbed:

```text
+---------------------------------------------------------------------------------------------------------------+
|                                      SIX-SPRINT ROADMAP MATRIX                                                |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| Sprint | Name                               | Key Deliverables & Gameplay Mechanics               | Status    |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| **43** | **Empire Facility Operations &**   | Empire-Wide Industrial Dashboard; Station Platform  | **MERGED**|
|        | **Closed Production Loops**        | output with Hub overflow; 4-pipeline factory loop.  | (PR #22)  |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| **44** | **Lore AI Competitors:**           | Scripted home networks (CST on Merredin, Grand      | **MERGED**|
|        | **CST vs Grand Central**           | Central on Augusta); dynamic gateway corridor races.| (PR #23)  |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| **45** | **Unified Commonwealth**           | Synchronized NewGRF: Monumental portals, animated   | **MERGED**|
|        | **Visual Overhaul Pack**           | wormholes, 4 biomes, Arcologies, 12-cargo fleet.    | (PR #24)  |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| **46** | **Seamless Multi-Server**          | Live trans-server train transit; authority custody; | **MERGED**|
|        | **Federation Universe**            | automatic holding loops; in-game Galaxy directory.  | (PR #21)  |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| **47** | **Colonial Megaprojects &**        | 3-tier supply delivery colonisation; Arcology       | **MERGED**|
|        | **Arcology Metropolises**          | urban evolution; planetary phase promotion events.  | (PR #18)  |
+--------+------------------------------------+-----------------------------------------------------+-----------+
| **48** | **LLM Narrative Synthesis &**      | Prompt-to-savegame generator; 50-year headless fast-| **MERGED**|
|        | **Autonomous Balancing Critic**    | forward economic balancing; automated BOM tuning.   | (PR 19/20)|
+--------+------------------------------------+-----------------------------------------------------+-----------+
```

---

## 2. Sprint-by-Sprint Implementation Specifications

### Sprint 43: Empire Facility Operations & Closed Production Loops

* **Objective:** Give players complete industrial visibility and control over all station-attached processing facilities across every planet, fully closing the four 12-cargo production pipelines.
* **Core Systems:**
  1. **Empire-Wide Facility Dashboard Window:**
     - Accessible via `Map > Industrial Facilities & Supply Chain` or hotkey `Ctrl+I`.
     - Displays all company facilities grouped by Planet / World.
     - Metrics per facility: active recipe, monthly input/output rates, capacity utilization %, feedstock reserve status, and deficit warnings (e.g. *Input Starved: Missing Silica Sand*).
     - Global batch actions: Upgrade Capacity (+50 tons/mo), Retire Facility, Change Recipe.
  2. **Station Platform First with Overflow Routing:**
     - Finished goods from processing facilities are automatically published as waiting cargo on the station platform for outbound trains.
     - When station platform storage hits capacity or waiting cargo exceeds rating thresholds, surplus production automatically diverts into the local planetary Logistics Hub / Stockpile, preventing waste and feeding In-Kind Fabrication.
  3. **Visual In-Game Overlays:**
     - Overhead status pins over industrial stations indicating active processing, feedstock starvation, or buffer overflow.
  4. **The Four Complete Closed Loops:**
     - *Pipeline A (Structural):* Iron Ore + Stone/Slag $\rightarrow$ Structural Steel $\rightarrow$ Superalloys.
     - *Pipeline B (Electronics):* Copper Ore + Silica Sand $\rightarrow$ Wiring + Silicon Chips $\rightarrow$ Signalling Logic.
     - *Pipeline C (Propulsion):* Hydrocarbons $\rightarrow$ Synthetic Composites + Superalloys $\rightarrow$ Maglev Cryo-Bogies.
     - *Pipeline D (Data Crystals):* Silica Sand + Rare Earths $\rightarrow$ Blank Crystals $\rightarrow$ Quantum / Consumer Crystals.

---

### Sprint 44: Autonomous Lore-Driven AI Competitors (CST vs Grand Central)

* **Objective:** Bring the Commonwealth universe to life by introducing rival corporate empires that play by authentic lore strategies.
* **Core Systems:**
  1. **Scripted Home Networks:**
     - *Commonwealth Synergy Transport (CST):* Starts with established heavy freight infrastructure on Phase 2 Merredin (balloon loops, ore hoppers, smelters, steel yards).
     - *Grand Central Trans-Portal:* Starts with high-frequency commuter and express networks on Phase 1 Earth/Augusta (Ro-Ro terminals, express passenger EMUs, Data Crystal couriers).
     - *InterWorld Logistics:* Starts as a rugged extraction operator on Phase 3 Calyx (mining loops, Edge Conduits).
  2. **Dynamic Expansion Engine:**
     - AIs monitor milestone triggers (world population tiers, gateway activation, tech unlocks).
     - When triggered, AIs expand toward contested gateway corridors using the 8 canonical CST prefabs (`Commands::PlaceBlueprint`), stamping robust, non-deadlocking double mainline track.
  3. **Gateway Transit Bidding:**
     - AIs schedule consists across portal wormholes, competing with the player for track capacity, platform time, and Megacity commodity supply contracts.

---

### Sprint 45: Unified Commonwealth Visual Overhaul Pack

* **Objective:** Replace vanilla OpenTTD sprite recolors with a cohesive, professional, high-definition sci-fi aesthetic.
* **Core Systems:**
  1. **Monumental Portal Architecture:**
     - Monolithic stone and composite portal arch structures (18-tile footprint).
     - Animated shimmering wormhole horizon and active particle effects when trains transit.
     - Integrated approach catenary and signaling gantries.
  2. **Distinct World Biome Surfaces:**
     - *Arid / Rust Frontier:* Red sand dunes, cracked salt flats, industrial dust haze.
     - *Boreal / Glacial Tundra:* Crystalline ice sheets, permafrost, geothermal vents.
     - *Volcanic / Barren:* Basalt fields, obsidian ridges, sulfur vents.
  3. **Megacity Arcology Buildings:**
     - Tier 1–3 urban evolution: standard city buildings evolve into towering multi-tier arcologies, skybridges, and illuminated transit corridors.
  4. **Custom 12-Cargo Rolling Stock Fleet:**
     - CST 10,000-hp Heavy Diesel Haulers, High-Power Electric Freight, 400 km/h Vacuum Maglevs, Armored Data Vans, and MPS Express Couriers.

---

### Sprint 46: Seamless Multi-Server Federation Universe

* **Objective:** Transform OpenSpaceTTD into a true distributed universe where each major world runs on its own dedicated server process, structured around the canonical 108-world Commonwealth topology ([`docs/COMMONWEALTH_WORLD_GRAPH_AND_EXPANSION_DESIGN.md`](COMMONWEALTH_WORLD_GRAPH_AND_EXPANSION_DESIGN.md)).
* **Core Systems:**
  1. **Seamless Physical Train Transit:**
     - A train on Server A enters Gateway Alpha; it despawns into Central Universe Authority custody.
     - Moments later, it materializes with identical consist composition, wagon cargo counts, and persistent orders on Server B's matching portal.
  2. **Automatic Staging & Holding Loops:**
     - If Server B is experiencing network congestion or a scheduled restart, inbound trains on Server A automatically hold in designated staging sidings without blocking mainline traffic.
  3. **In-Game Galaxy Directory & Cross-Server Telemetry:**
     - Universe Directory shows live ping, server load, and active in-transit consists.
     - Multi-server economic clearing house: delivery revenue automatically credits the owning player's corporate treasury across servers.

---

### Sprint 47: Colonial Megaprojects & Arcology Metropolises

* **Objective:** Connect logistics directly to world transformation, expanding from settled Core worlds into the outer Phase 4 Wilderness nodes established in [`docs/COMMONWEALTH_UNIVERSE_CATALOGUE.md`](COMMONWEALTH_UNIVERSE_CATALOGUE.md).
* **Core Systems:**
  1. **Multi-Stage Colonial Megaprojects:**
     - Transforming a barren Phase 4 Wilderness requires delivering structured supply tiers through the gateway:
       - *Tier 1 (Outpost Founding):* Bio-Domes & Life Support $\rightarrow$ Spawns native frontier settlement.
       - *Tier 2 (Industrial Unlocking):* Heavy Steel & Mining Machinery $\rightarrow$ Generates deep ore mines and extraction sites.
       - *Tier 3 (Planetary Promotion):* Nanotech & Quantum Crystals $\rightarrow$ World officially promotes to Phase 3 Frontier with full market economy.
  2. **Arcology Urban Evolution:**
     - Phase 1 Megacities monitor 3-tier demand satisfaction (Food/Water, Goods/Steel, Luxuries/Data Crystals). Sustained 90%+ satisfaction triggers structural evolution into gleaming Arcologies with exponential population growth.

---

### Sprint 48: LLM Narrative Scenario Synthesis & Autonomous Balancing Critic

* **Objective:** Harness AI for infinite procedural lore scenarios and automated game balance.
* **Core Systems:**
  1. **Prompt-to-Savegame Generator:**
     - CLI and in-game tool converting narrative descriptions (*"Generate a 3-world system where an arid mining colony is striking over water shortages while a greedy core world demands superalloys"*) into fully initialized, working `.sav` scenario files with pre-built corridors and active fleets.
  2. **Autonomous 50-Year Headless Balancing Critic:**
     - Runs fast-forward headless simulations (50 game years in ~3 minutes).
     - Detects inflation drift, portal choke points, and stockpile starvation.
     - Emits tuning recommendations for Bill of Materials (BOM) formulas and freight tariffs.

---

## 3. Current next steps — 23 September 2026

Human UAT and the current graphics are accepted by the user. Graphics refinement
can follow later. The controlled two-server/two-client loaded natural-entry round
trip now passes after the desync repair. Next prove scheduled loading/unloading,
orders, ownership and money, then blocking, restarts and reconnects.
[Live evidence](audit/2026-09-23/federation-multiplayer/README.md). See the
[current acceptance target](PROJECT_STATUS_AND_ROADMAP.md#next-priority-live-federation-with-two-clients).

Sprints 49–50 remain candidates for graph/trading and gateway operations after
that proof. Sprints 51–52 are removed from active scope, including exploration,
Silfen paths, orbital docks, galactic market/tariff expansion and crisis scenarios.
Keep narrative and story-driven scenarios out of future work. Existing implemented
features above remain historical records, not new commitments.
