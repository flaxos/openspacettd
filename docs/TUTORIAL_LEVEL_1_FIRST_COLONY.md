# OpenSpaceTTD Level 1 Tutorial: "First Colony"

*An interactive playthrough guide for new players — and a structured acceptance test for developers.*

---

## Before You Begin

**What is OpenSpaceTTD?**

OpenSpaceTTD extends OpenTTD from a single-map transport simulation into a **multi-world industrial empire builder** inspired by Peter F. Hamilton's Commonwealth Saga. The year is 2050. Nigel Sheldon and Ozzie Isaacs have perfected stable wormhole technology, and **Commonwealth Star Transit (CST)** is building railway connections between worlds. You are a corporate charter holder developing transport infrastructure across multiple planets connected by portal gates.

**What's different from standard OpenTTD?**

| Feature | Standard OpenTTD | OpenSpaceTTD |
|---------|-----------------|--------------|
| Map layout | One continuous landscape | Multiple **planetary regions** separated by void space |
| Time period | 1950–2050 (vehicles appear by year) | **2050 start** — vehicles unlocked via Tech Tree |
| Industries | Fixed types per climate | **12 new Commonwealth cargo types** in 4 production pipelines |
| Construction | Build anywhere | **Planetary Phase restrictions** — extraction on Frontier, processing on Core |
| Progression | Calendar-driven | **Tech Tree research** with 3 branches × 4 tiers |
| Inter-world transit | N/A | **Portal gates** — wormhole connections between worlds |
| Company management | Basic finances | **Corporate HQ**, Logistics Hubs, Stockpiles, Fabrication |
| City growth | Passenger/mail/goods | **Megacity tier system** — 3 cargo tiers drive growth multipliers |
| Rail construction | Piece-by-piece | **Blueprint Library** — stamp pre-designed CST Prefab layouts |

---

## Tutorial Scenario Setup

Start a new game with the tutorial scenario, or generate a new game with these settings:

- **Map size:** 1024 × 512 (map_x=10, map_y=9)
- **Starting year:** 2050
- **Climate:** Temperate
- **Currency:** Credits (suffix " credits")
- **Competitors:** 0 (solo for learning)
- **Vehicles never expire:** Yes
- **Max loan:** £300,000

The tutorial scenario contains three pre-configured planetary regions:

| World | Phase | Biome | Role |
|-------|-------|-------|------|
| **Merredin** | Phase 3 — Frontier | Temperate | Raw resource extraction (ore, minerals, stone) |
| **Augusta** | Phase 2 — Developed | Temperate | Industrial processing (smelters, fabs, assembly) |
| **New Brisbane** | Phase 1 — Core | Temperate | HQ site, megacity, advanced manufacturing, consumption |

---

## Prologue: Welcome to the Commonwealth

*"The year is 2050. You've just been granted a corporate charter to develop transport infrastructure on the frontier world of Merredin — a Phase 3 colony rich in raw minerals but lacking industrial processing. Your ultimate goal: build a multi-world production empire connecting Merredin's raw resources through portal gates to the developed inner worlds."*

### Objectives

- [ ] Start a new game using the tutorial scenario
- [ ] Identify the three world regions on the minimap
- [ ] Open the **Universe Directory** and identify each world's Phase and Biome
- [ ] Note the starting year (2050) and available locomotive

### Steps

1. **Start the game** and load the tutorial scenario (or generate a new map).
2. Open the **minimap** — observe three clusters of land separated by dark void. These are your three planetary regions. The void buffer space between them is impassable — you cannot build on it.
3. Open the **Universe Directory**: click the **Map** menu (globe icon on the main toolbar) → **"Universe Directory"**.
4. The Universe Directory lists all registered worlds with their **Phase** (1–4) and **Biome**. Locate:
   - **Merredin** — Phase 3 Frontier
   - **Augusta** — Phase 2 Developed
   - **New Brisbane** — Phase 1 Core
5. Click **"Locate World"** for each entry to scroll the main viewport to that planet.
6. Open a **train depot** (you'll build one shortly) — note the available locomotive: the **CST Pioneer 0-6-0 Surveyor** steam engine. This is baseline Commonwealth technology, available without any research.

### Key Concept: Planetary Phases

Phases determine what you can build on each world:

| Phase | Name | Can Build | Cannot Build |
|-------|------|-----------|-------------|
| **Phase 1** | Core World | Advanced processing, CST Maglev, Megacities, Corporate HQ | Raw extraction, bio-farms |
| **Phase 2** | Developed | Intermediate processing, manufacturing | CST vacuum-tube maglev |
| **Phase 3** | Frontier | Raw extraction, mining, bio-farms, small colonies | Advanced processing, vac-trains |
| **Phase 4** | Expansion | Nothing (wilderness) — must be colonised first | Everything until colonised |

> **Remember:** Extraction happens on the frontier. Processing happens in the core. Your job is building the transport links between them.

### 🔍 Verification Checkpoint

- [ ] Universe Directory opens and shows all 3 worlds
- [ ] World phases are correctly displayed (Phase 3, 2, 1)
- [ ] Starting year is 2050
- [ ] You can scroll to all three worlds via "Locate World"
- [ ] Clicking on void space correctly rejects construction attempts

---

## Chapter 1: First Rails on Merredin

*In which you build your first railway on an alien world and discover that Frontier worlds have rules.*

### Objectives

- [ ] Build a basic coal transport route on Merredin
- [ ] Buy a CST Pioneer Steam locomotive
- [ ] Earn your first revenue
- [ ] Discover the planetary construction restriction system

### Steps

1. **Navigate to Merredin** — use Universe Directory → Locate World, or scroll the minimap.

2. **Find a coal mine** — open the Industry List (factory icon on toolbar → "Industry Directory"), filter by coal. Find one on Merredin.

3. **Build a train depot** near the coal mine:
   - Open the Rail Construction toolbar (rail track icon).
   - Click the depot button (small building icon).
   - Place a depot on flat ground near the mine.

4. **Build Station A** (coal loading):
   - Click the station button on the Rail Construction toolbar.
   - Place a 1-track, 5-platform station near the coal mine (within catchment area).

5. **Find a power station** on Merredin and build **Station B** nearby.

6. **Lay track** connecting the depot → Station A → Station B. Use standard OpenTTD track-laying — drag single rail, autorail, or use the drag tool.

7. **Buy your first train**:
   - Click your depot to open it.
   - Click "New Vehicles".
   - Find the **"CST Pioneer 0-6-0 Surveyor"** steam locomotive — select it and click "Buy Vehicle".
   - Add 3 coal wagons.

8. **Set orders**:
   - Open the train's orders window.
   - Click Station A → set to **Full Load**.
   - Click Station B → set to **Unload All**.

9. **Start the train** — click the red "Stopped" flag to start it.

10. **Wait for the first delivery** — watch the train load coal, travel to the power station, and unload. Check your Finances window — you're earning credits!

11. **Test the Phase restriction** — this is important:
    - At Station A, open the station window.
    - Look for the **"Build production facility (100,000 Cr)"** button.
    - Click it and try to select **"Steel Smelting"** (Recipe 102).
    - You should see an error: ***"cannot construct advanced processing or vac-train facilities on a Frontier World"***
    - This is working as intended — steel smelting requires a Phase 2 or Phase 1 world.

### What Just Happened?

You've run a standard OpenTTD coal route — but on an alien planet in 2050. The CST Pioneer Steam is available without research because it's baseline technology. The key lesson: **Frontier worlds are for extraction, not processing.** To smelt iron ore into steel, you'll need to ship it to Augusta or New Brisbane through a portal gate.

### 🔍 Verification Checkpoint

- [ ] CST Pioneer Steam appears in the depot purchase list
- [ ] Train runs correctly between two stations
- [ ] Revenue is generated from coal deliveries
- [ ] "Build production facility" button appears on station windows
- [ ] Phase restriction error displays correctly when attempting steel smelting on Merredin
- [ ] Train speed and acceleration feel reasonable

---

## Chapter 2: Commonwealth Industry

*In which you learn about the 12-cargo production system and the four industrial pipelines.*

### Objectives

- [ ] Open the Empire Facilities dashboard
- [ ] Explore the 4 pipeline tabs
- [ ] Build an Iron Ore extraction route on Merredin
- [ ] Understand the renamed Commonwealth cargo types

### Steps

1. **Open the Empire Facilities dashboard**: press **Ctrl+I**, or go to **Map menu → "Industrial Facilities & Supply Chain"**.
   - Note: it's empty because you haven't built any production facilities yet.
   - Browse the 4 pipeline tabs:
     - **A: Structural** — Stone/Slag, Iron Ore, Steel, Superalloys
     - **B: Electronics** — Copper Ore, Silica Sand, Silicon Chips, Signalling
     - **C: Propulsion** — Hydrocarbons, Synthetic Composites, Cryo-Bogies
     - **D: Data Crystals** — Blank Crystals, Quantum Crystals, Consumer Crystals

2. **Open the Cargo Payment Rates chart** (from the Graph menu) and note the renamed cargos:

   | Original OpenTTD Name | Commonwealth Name |
   |----------------------|-------------------|
   | Mail | Data Crystals |
   | Goods | Manufactured Goods & Colony Supplies |
   | Iron Ore | Heavy Iron Ore |
   | Steel | Structural Alloys |
   | Grain | Bio-Grain |
   | Valuables | Valuables & Rare Crystals |
   | Food | Packaged Nutrients & Rations |
   | Water | Purified Hydration Feedstock |
   | Diamonds | Rare Silicates & Optical Crystals |

3. **Find an Iron Ore mine** on Merredin (Industry Directory → filter by Iron Ore / Heavy Iron Ore).

4. **Build a station** at the Iron Ore mine, lay track from your existing depot, buy another Pioneer locomotive with Iron Ore wagons.

5. **Set orders** to load Iron Ore at the mine — but **don't set a destination yet**. You'll need a portal gate to send this ore somewhere useful.

6. **Study the production chain** for Pipeline A (Structural):
   ```
   Iron Ore (Frontier) ──→ Steel Smelting (Phase 2+) ──→ Structural Steel
                                                              │
   Rare Earth Minerals (Frontier) ──┐                        │
                                    ├──→ Superalloy Foundry ──→ Superalloys
   Structural Steel ────────────────┘    (Phase 2+)
   ```
   Your Iron Ore needs to reach a **Steel Smelting** facility on Augusta (Phase 2).

### What Just Happened?

You've discovered the raw resources on your Frontier world and learned that they feed into **production pipelines** — but the processing facilities only exist on more developed worlds. This is the core economic tension of OpenSpaceTTD: **extraction on the frontier, processing in the core, consumption in the megacities.** Your job is to build the transport links.

### 🔍 Verification Checkpoint

- [ ] Empire Facilities dashboard opens (`Ctrl+I`)
- [ ] All 4 pipeline tabs are visible and correctly labelled
- [ ] Renamed cargos display correctly in: cargo list, station windows, vehicle cargo info, payment rate chart
- [ ] Iron Ore mine exists on Merredin and accepts station catchment
- [ ] Wagons correctly accept Heavy Iron Ore cargo type

---

## Chapter 3: The Portal Gate

*In which you build a wormhole between worlds and send your first inter-planetary train.*

Portal gates are OpenSpaceTTD's central mechanic — artificial Einstein-Rosen bridges that trains pass through like tunnels, connecting non-contiguous tiles across arbitrary distances.

### Objectives

- [ ] Build a portal gate on Merredin
- [ ] Build a matching portal gate on Augusta
- [ ] Link the two gates into a wormhole pair
- [ ] Route the Iron Ore train through the portal to Augusta
- [ ] Confirm the train arrives intact with its cargo

### Steps

1. **Open the Rail Construction toolbar** (the rail track icon).

2. **Find the Portal Gate button** — it uses a tunnel-like icon and its tooltip reads: *"Build a Portal Gate with an automatic two-lane, 14-tile holding terminal, or explicitly link two existing gates"*. It's next to the standard tunnel button.

3. **Place the Merredin gate**:
   - Click the Portal Gate button.
   - Click on a suitable flat area on Merredin, near your Iron Ore station.
   - The gate constructs with an automatic 14-tile holding terminal.
   - Connect the gate to your Iron Ore station via standard rail track.

4. **Navigate to Augusta** (Universe Directory → Locate World for "Augusta").

5. **Place the Augusta gate**:
   - With the Rail Construction toolbar still open, click the Portal Gate button again.
   - Place it on Augusta, on a flat area.
   - Build a station nearby (this will be your Iron Ore receiving station on Augusta).
   - Connect the Augusta gate to the new station via track.

6. **Link the gates**: The construction process should offer to link two existing unlinked gates. If using the console, the command is:
   ```
   portal_link <gate_id_1> <gate_id_2>
   ```
   Check the portal gate's info window for its ID.

7. **Update your Iron Ore train's orders**:
   - Open the train's orders window.
   - Set: **Load Full at Merredin Iron Ore station → Unload All at Augusta receiving station**.
   - The YAPF pathfinder will route the train through the portal gates automatically.

8. **Watch the transit**:
   - Start (or unpause) the Iron Ore train.
   - Watch it travel to the Merredin gate, enter the wormhole, and emerge from the Augusta gate.
   - It then continues to the Augusta receiving station and unloads its Iron Ore.

### What Just Happened?

Your train just passed through a wormhole connecting two planets separated by void. Under the hood, OpenSpaceTTD uses the same YAPF wormhole logic that handles tunnels and bridges (`Track::Wormhole`, `VehicleEnterTileState::EnteredWormhole`), but portals connect **non-contiguous tiles** across arbitrary distances. The pathfinder treats the gate pair as a valid route segment — just like a very long tunnel.

This is the fundamental OpenSpaceTTD mechanic: **planetary specialisation connected by portal logistics.**

### 🔍 Verification Checkpoint

- [ ] Portal Gate construction button appears in the Rail Construction toolbar
- [ ] Gate placement works on flat terrain
- [ ] The 14-tile holding terminal auto-constructs correctly
- [ ] Gate linking works (via GUI interaction or console command)
- [ ] YAPF pathfinder correctly routes trains through linked gates
- [ ] Train maintains its identity, orders, and cargo through portal transit
- [ ] No desync, crash, or state corruption during transit
- [ ] Train speed through the portal feels reasonable (not instant, not glacial)

---

## Chapter 4: Steel Smelting — Your First Production Facility

*In which you complete the first stage of Pipeline A and see the industrial system in action.*

### Objectives

- [ ] Build a Steel Smelting facility at your Augusta receiving station
- [ ] Deliver Iron Ore and watch it convert to Structural Steel
- [ ] Monitor the facility in the Empire Facilities dashboard
- [ ] Collect and transport the Structural Steel output

### Steps

1. **Open the Augusta receiving station window** (the station where Iron Ore arrives from the portal).

2. **Click "Build production facility (100,000 Cr)"**.

3. **Select "Steel Smelting" (Recipe 102)** from the recipe list. This is Pipeline A: Structural.
   - Recipe: **2 Iron Ore → 1 Structural Steel**
   - Allowed on: Phase 2 (Developed) and Phase 1 (Core) worlds ✓

4. **Confirm placement** — the facility is now attached to this station. Cost: 100,000 credits.

5. **Wait for delivery** — your Iron Ore train should arrive shortly. Watch the facility status change:
   - **Starved** → (Iron Ore arrives) → **Active** → (producing) → Structural Steel appears as waiting cargo

6. **Open the Empire Facilities dashboard** (`Ctrl+I`):
   - Click the **"A: Structural"** tab.
   - Your Steel Smelter should appear, showing:
     - Monthly capacity: **100 t/mo** (default)
     - Input buffer: Iron Ore (amount waiting)
     - Output buffer: Structural Steel (amount produced)
     - Status: Active

7. **Collect the output** — build a second train on Augusta to collect Structural Steel:
   - Buy a locomotive + Structural Steel wagons at a depot on Augusta.
   - Set orders to load at the smelter station.
   - You can deliver the steel to another station, export it through a portal to New Brisbane, or stockpile it at a Logistics Hub (next chapter).

8. **Check the facility stats** — after a few months, revisit the Empire Facilities dashboard:
   - `last_month_production` shows how much was produced.
   - If production exceeds collection, the output buffer grows.
   - If the buffer overflows and you have a Logistics Hub (Chapter 6), excess goes to your company stockpile.

### What Just Happened?

You've completed Pipeline A Stage 1:

```
Iron Ore (Merredin, Frontier) ──Portal──→ Steel Smelting (Augusta, Developed) ──→ Structural Steel
```

Production facilities are **station attachments** — they consume cargo delivered by trains and produce new cargo types. Each facility has:
- A **recipe** defining inputs → outputs
- A **monthly capacity** (upgradeable) limiting throughput
- **Input/output buffers** that hold cargo between processing cycles
- A **status** (Starved, Active, Overflow, Idle) visible in the Empire Facilities dashboard

### 🔍 Verification Checkpoint

- [ ] "Build production facility" button works at Augusta station
- [ ] Recipe 102 (Steel Smelting) appears in the recipe list
- [ ] Phase restriction correctly allows it on Phase 2 Augusta
- [ ] Iron Ore is consumed and Structural Steel is produced
- [ ] Empire Facilities dashboard updates with facility status
- [ ] Facility status transitions correctly: Starved → Active
- [ ] Structural Steel appears as waiting cargo at the station
- [ ] A train can load and transport the Structural Steel
- [ ] Facility survives save/load with correct state

---

## Chapter 5: Corporate Headquarters & The Tech Tree

*In which you establish your empire's nerve centre and unlock your first advanced locomotive.*

### Objectives

- [ ] Build your Corporate HQ on New Brisbane (Phase 1 Core World)
- [ ] Explore the 6 HQ tabs
- [ ] Start researching TECH_TRACTION_1 (High-Adhesion Steam)
- [ ] Understand the Tech Tree progression system

### Steps

1. **Navigate to New Brisbane** — Universe Directory → Locate World for "New Brisbane".

2. **Open the Corporate HQ window**: go to **Map menu → "Corporate Headquarters & Stockpiles"**.
   - You'll see the message: *"No active Corporate Headquarters campus established. Must be founded on a Phase 1 Core World."*

3. **Establish your HQ**:
   - Click **"Establish HQ"**.
   - Click on a flat tile on New Brisbane — the HQ campus is built. Cost: **2,500,000 credits** (you may need to take a loan).

4. **Try to build the HQ on Merredin** (optional, to test the restriction):
   - You should get the error: *"Corporate Headquarters must be founded on a Phase 1 Core World"*

5. **Explore the 6 HQ tabs**:

   | Tab | Purpose |
   |-----|---------|
   | **Overview** | Company statistics, world presence summary |
   | **Planetary Stockpiles** | Cargo reserves across all your worlds (empty at first) |
   | **Logistics Hubs** | List of your hub stations with buffer levels |
   | **In-Kind Fabrication** | Build infrastructure using stockpiled materials instead of cash |
   | **Commonwealth Tech Tree** | Research progression (3 branches × 4 tiers = 12 projects) |
   | **Alliances** | Corporate alliances and track-sharing agreements (advanced) |

6. **Open the Tech Tree tab** and study the three research branches:

   | Tier | 🚂 Traction | 🌀 Portal Physics | ⚙️ Materials |
   |------|------------|-------------------|-------------|
   | **1** | High-Adhesion Steam (100 RP) | Stable Gateway Links (100 RP) | Blast Furnace Steel (100 RP) |
   | **2** | Multi-Unit Heavy Diesel (250 RP) | Dual-Track Throats (300 RP) | Electronics Fabs (250 RP) |
   | **3** | High-Voltage Electrics (600 RP) | Freight Corridors (750 RP) | Superalloys (600 RP) |
   | **4** | CST Vacuum Maglev (1500 RP) | Twin-Array Wormholes (2000 RP) | Quantum Neural Fabs (1500 RP) |

7. **Start your first research project**:
   - Select **TECH_TRACTION_1** — "High-Adhesion Steam".
   - Click **"Start Research"**.
   - Click **"Budget"** to set your monthly R&D allocation.
   - Research Points accumulate monthly based on your budget. When you reach 100 RP, the project completes and you unlock the **Vulcan Heavy-Adhesion Steam** locomotive — a freight powerhouse.

### What Just Happened?

In standard OpenTTD, better trains appear automatically as the calendar advances past their introduction date. In OpenSpaceTTD, **all vehicles are unlocked through the Tech Tree**, and `never_expire_vehicles` is always on — no locomotive ever becomes obsolete.

The Tech Tree has three branches reflecting the Commonwealth's engineering priorities:
- **Traction**: Better locomotives (Steam → Diesel → Electric → Maglev)
- **Portal Physics**: Better portal gates (single-track → dual-track → freight corridors → twin-array)
- **Materials**: Better industrial capacity (basic steel → electronics → superalloys → quantum)

Each project requires **Research Points** (funded by your monthly cash budget) and may also consume **Data Crystals** and **Electronics** from your stockpile at higher tiers.

### 🔍 Verification Checkpoint

- [ ] Corporate HQ window opens from Map menu
- [ ] "Establish HQ" restricts placement to Phase 1 worlds only
- [ ] HQ campus appears on the map after placement
- [ ] All 6 tabs are present and navigable without crashes
- [ ] Tech Tree shows all 12 nodes across 3 branches
- [ ] "Start Research" initiates the selected project
- [ ] Budget allocation UI works (clicking cycles through budget levels)
- [ ] Research Points accumulate over time
- [ ] HQ survives save/load

---

## Chapter 6: Logistics Hubs & Stockpile Management

*In which you tame production overflow and learn to build with materials instead of cash.*

### Objectives

- [ ] Attach a Logistics Hub to your Augusta smelter station
- [ ] Observe cargo overflow flowing to the hub
- [ ] Set a minimum cargo reserve
- [ ] Learn about the Fabrication system

### Steps

1. **Open the Corporate HQ window** → click the **"Logistics Hubs"** tab.
   - It shows no hubs yet.

2. **Click "Build Hub"**.

3. **Click on your Augusta Steel Smelter station platform** — a Hub attachment is created.
   - The hub is now linked to this station: any production overflow will be stored here.

4. **Let the smelter run for a few game months** — if your collecting trains can't keep up with production, excess Structural Steel flows into the Hub's buffer.

5. **View hub cargo levels**:
   - In the Logistics Hubs tab, your hub appears with its current cargo levels.
   - Click **"Next Cargo"** to browse stored cargo types.
   - Click **"Next Hub"** if you have multiple hubs.

6. **Set a minimum reserve**:
   - Click **"Set Reserve"** to set a threshold (e.g., 50 units of Structural Steel).
   - The hub will retain at least this much — trains won't drain it below the reserve floor.
   - This is useful for ensuring you always have materials available for the Fabrication system.

7. **Explore Fabrication** (HQ → "In-Kind Fabrication" tab):
   - Click **"Toggle Mode"** to switch between standard cash purchasing and in-kind fabrication.
   - When fabrication mode is active, building track, depots, and stations consumes **materials from your planetary stockpile** instead of (or at a discount to) cash.
   - This becomes powerful later when you have abundant Structural Steel and want to build infrastructure cheaply.

### What Just Happened?

Logistics Hubs solve a real logistics problem: production facilities run continuously at their monthly capacity, but trains arrive in batches. Without a Hub, excess output just piles up as waiting cargo at the station (and eventually congests). With a Hub, overflow goes into your company's **planetary stockpile** where it can be:

- **Reserved** for future train pickups
- **Consumed** by the Fabrication system (build infrastructure with materials instead of cash)
- **Redistributed** to other hubs on the same world

This is Factorio-style resource management integrated into OpenTTD's station system.

### 🔍 Verification Checkpoint

- [ ] "Build Hub" correctly attaches to existing stations
- [ ] Cargo overflow from production facility routes to the hub
- [ ] Logistics Hubs tab shows the hub with correct cargo types and levels
- [ ] "Set Reserve" dialog works and persists the threshold
- [ ] "Next Cargo" and "Next Hub" navigation works
- [ ] Fabrication toggle switches modes
- [ ] Hub and reserve settings survive save/load

---

## Chapter 7: Blueprints & CST Prefabs

*In which you learn to build complex rail infrastructure in a single click.*

### Objectives

- [ ] Open the Blueprint Library
- [ ] Browse and place a CST Prefab
- [ ] Capture your own custom blueprint
- [ ] Rotate and flip blueprints before placement

### Steps

1. **Open the Blueprint Library**: click the **Blueprint button** on the Rail Construction toolbar (it uses a landscaping-like icon). Its tooltip reads: *"Open Rail Blueprint Library. Capture, manage, and stamp rail layouts, depots, and stations"*.

2. **Browse the CST Prefabs** — the library should contain pre-built Commonwealth standard designs:
   - Passing sidings
   - 4-way junctions
   - Station throat layouts
   - Mainline double-track sections

3. **Select a prefab** — choose a simple one (e.g., "CST Mainline Passing Siding").

4. **Click "Place"** — your cursor enters **stamp placement mode**. A ghost outline of the blueprint follows your cursor.

5. **Rotate and flip** before placing:
   - Press **R** to rotate the blueprint 90° clockwise.
   - Press **F** to mirror (flip) the blueprint horizontally.
   - Find the right orientation for your terrain.

6. **Click on the map** to stamp the blueprint down — track, signals, and station elements appear instantly. Cost is deducted based on the components placed.

7. **Capture your own blueprint**:
   - Click **"Capture"** in the Blueprint Library.
   - Drag a rectangle over a section of your existing track on the map.
   - A dialog asks you to name it — enter something like "My Coal Loading Loop".
   - Click OK — your custom blueprint is saved and appears in the library.

8. **Manage your blueprints**:
   - **Rename**: Select a blueprint and click "Rename".
   - **Delete**: Select and click "Delete" to remove a player blueprint.
   - **Export**: Save a blueprint as a portable JSON file.
   - **Import**: Load a blueprint from a JSON file.

### What Just Happened?

Blueprints use a deterministic JSON schema and the server-authoritative `PlaceBlueprint` command to ensure multiplayer determinism. Every blueprint placement is fully replicated across all clients — there's no desync risk. CST Prefabs represent canonical Commonwealth rail engineering standards, just as real-world railways use standardised junction and station designs.

> ⚠️ **Known risk**: Blueprint placement has historically experienced crashes (tracked as OST-BP-001). If you encounter a crash during placement, note the blueprint name, map location, and rotation state for the bug report.

### 🔍 Verification Checkpoint

- [ ] Blueprint Library window opens from Rail Construction toolbar
- [ ] CST Prefabs are listed in the library
- [ ] Stamp placement mode activates with ghost preview
- [ ] Rotate (R) and Flip (F) work correctly
- [ ] Placed blueprint creates correct track, signals, and station elements
- [ ] Capture correctly selects a map rectangle and saves the blueprint
- [ ] Saved blueprint appears in the library and can be re-placed
- [ ] Export produces a valid JSON file
- [ ] Import loads a JSON file correctly
- [ ] No crashes during placement (report any as high priority)

---

## Chapter 8: Building Your Empire

*In which you supply a Megacity, promote worlds, and see the full industrial loop.*

### Objectives

- [ ] Designate a Megacity on New Brisbane
- [ ] Supply it with Tier 1 (Sustenance) cargo
- [ ] Observe the growth state change
- [ ] Check Merredin's development score and understand world promotion
- [ ] See the complete multi-world production loop

### Steps

1. **Navigate to New Brisbane** and find the largest town.

2. **Open the Megacity Overview**: use **Map menu (on the main toolbar) → Town submenu → "Megacity Overview"**, or from the Universe Directory click **"View Megacity"** for New Brisbane.

3. **Click "Designate Megacity"** to register the town as a metropolitan core.

4. **Study the 3 cargo tiers** required for growth:

   | Tier | Category | Required Cargo |
   |------|----------|---------------|
   | **T1** | Sustenance | Packaged Nutrients & Rations (Food), Purified Hydration Feedstock (Water) |
   | **T2** | Expansion | Structural Alloys (Steel), Manufactured Goods & Colony Supplies |
   | **T3** | Prosperity | Data Crystals, Valuables & Rare Crystals |

5. **Supply T1 cargo** — find food/water sources on New Brisbane (or import via portal):
   - Build stations near food processing plants and water towers/pumps.
   - Build trains to deliver Packaged Nutrients and Purified Hydration to the Megacity station.

6. **Watch the growth meter change**:

   | State | Multiplier | Triggered By |
   |-------|-----------|-------------|
   | **Starvation** | 0.0× growth | No T1 cargo supplied |
   | **Subsistence** | 1.0× growth | T1 quota partially met |
   | **Metropolitan Boom** | 1.5× growth | T1 quota fully met |
   | **HyperGrowth** | 2.0× growth (+50% traffic) | All 3 tiers met |

7. **Check development scores** — open the Universe Directory and look at Merredin's development score:
   - It should have increased from your rail construction and industrial activity.
   - Note the promotion thresholds:
     - **2,000 points** → Promote to Phase 2 (Developed) — unlocks local processing on Merredin
     - **5,000 points** → Promote to Phase 1 (Core) — unlocks maglev, megacities, HQ placement

8. **Understand world promotion**: when a world's development score meets the threshold, the **"Promote World"** button becomes active in the Universe Directory. Promoting Merredin from Phase 3 → Phase 2 would let you build smelters *locally*, removing the need for inter-world portal transport of raw materials.

### The Complete Empire Loop

Congratulations — you've now touched every major system in OpenSpaceTTD:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    THE COMMONWEALTH EMPIRE LOOP                         │
│                                                                         │
│  FRONTIER (Phase 3)          DEVELOPED (Phase 2)      CORE (Phase 1)   │
│  ┌─────────────┐             ┌─────────────────┐      ┌──────────────┐ │
│  │ Iron Ore    │──Portal──→  │ Steel Smelting  │──→   │ Megacity     │ │
│  │ Copper Ore  │──Portal──→  │ Copper Smelting │──→   │ Supply       │ │
│  │ Silica Sand │──Portal──→  │ Silicon Arc Fab │──→   │              │ │
│  │ Stone/Slag  │             │                 │      │ Corporate HQ │ │
│  └─────────────┘             └─────────────────┘      │ Tech Tree    │ │
│       ↑                            ↑                  │ Logistics    │ │
│  Development                 Development              │ Fabrication  │ │
│  Score grows                 Score grows               └──────────────┘ │
│       ↓                            ↓                        ↑          │
│  Phase 3 → 2                 Phase 2 → 1              Megacity tiers   │
│  (Promote!)                  (Promote!)               drive growth     │
└──────────────────────────────────────────────────────────────────────────┘
```

The game's ultimate loop is:
1. **Extract** raw resources on Frontier worlds
2. **Transport** them through portal gates to processing worlds
3. **Process** them through production chain recipes
4. **Supply** Megacities on Core worlds to trigger growth
5. **Research** the Tech Tree to unlock better locomotives and portal capacity
6. **Promote** Frontier worlds as their development score rises
7. **Expand** to new worlds through Phase 4 colonisation and new portal gates

### 🔍 Verification Checkpoint

- [ ] Megacity designation works (button functions, town is registered)
- [ ] All 3 cargo tiers display correctly in the Megacity Overview
- [ ] Delivering T1 cargo changes the growth state
- [ ] Growth state text updates correctly (Starvation → Subsistence → Boom)
- [ ] Development score displays and updates in the Universe Directory
- [ ] "Promote World" button appears when threshold is met
- [ ] World promotion actually changes the Phase and unlocks new construction types
- [ ] The entire system survives a save → load → verify cycle

---

## Appendix A: Complete Recipe Reference

### Pipeline A: Structural & Track Infrastructure
| Recipe ID | Name | Inputs | Outputs | Min Phase |
|-----------|------|--------|---------|-----------|
| 101 | Ballast Crushing | 2 Stone/Slag | 2 Ballast & Concrete | Phase 2 |
| 102 | Steel Smelting | 2 Iron Ore | 1 Structural Steel | Phase 2 |
| 103 | Superalloy Foundry | 2 Structural Steel + 1 Rare Earth Minerals | 2 Superalloys | Phase 2 |

### Pipeline B: Electronics, Signalling & Catenary
| Recipe ID | Name | Inputs | Outputs | Min Phase |
|-----------|------|--------|---------|-----------|
| 201 | Copper Smelting | 2 Copper Ore | 2 Conductive Wiring | Phase 2 |
| 202 | Silicon Arc Fabrication | 2 Silica Sand | 1 Silicon Chips | Phase 2 |
| 203 | Signalling Assembly | 1 Silicon Chips + 1 Conductive Wiring | 2 Telemetry & Signalling Logic | Phase 2 |

### Pipeline C: Advanced Train Propulsion
| Recipe ID | Name | Inputs | Outputs | Min Phase |
|-----------|------|--------|---------|-----------|
| 301 | Polymer Synthesis | 2 Hydrocarbons | 2 Synthetic Composites | Phase 2 |
| 302 | Maglev Works | 2 Superalloys + 2 Conductive Wiring | 2 Cryo-Bogies & Guideways | Phase 1 |

### Pipeline D: Data Crystals & Scientific R&D
| Recipe ID | Name | Inputs | Outputs | Min Phase |
|-----------|------|--------|---------|-----------|
| 401 | Monocrystal Synthesis | 2 Silica Sand + 1 Rare Earth Minerals | 2 Blank Data Crystals | Phase 2 |
| 402 | Quantum Enrichment | 2 Blank Data Crystals | 2 Enriched Quantum Crystals | Phase 1 |
| 403 | Consumer Crystal Format | 2 Blank Data Crystals | 2 Encrypted Consumer Crystals | Phase 2 |

---

## Appendix B: Tech Tree Quick Reference

### 🚂 Branch 0: Traction & Propulsion
| ID | Tier | Name | RP Cost | Prerequisite | Unlocks |
|----|------|------|---------|-------------|---------|
| TRACTION_1 | 1 | High-Adhesion Steam | 100 | None | Vulcan heavy-adhesion steam locomotive |
| TRACTION_2 | 2 | Multi-Unit Heavy Diesel | 250 | TRACTION_1 | Titan heavy diesel sets, high-torque freight |
| TRACTION_3 | 3 | High-Voltage Electrics | 600 | TRACTION_2 | CST E-40 Inter-World high-speed electric |
| TRACTION_4 | 4 | CST Vacuum Maglev | 1500 | TRACTION_3 | 1,000 km/h vacuum-tube maglev trainsets |

### 🌀 Branch 1: Portal Physics
| ID | Tier | Name | RP Cost | Prerequisite | Unlocks |
|----|------|------|---------|-------------|---------|
| PORTAL_1 | 1 | Stable Gateway Links | 100 | None | Single-track portal gate links |
| PORTAL_2 | 2 | Dual-Track Throats | 300 | PORTAL_1 | Parallel dual-track gateway arrays |
| PORTAL_3 | 3 | Freight Corridors | 750 | PORTAL_2 | Inter-server priority bulk transit |
| PORTAL_4 | 4 | Twin-Array Wormholes | 2000 | PORTAL_3 | Continuous multi-track trans-galactic corridors |

### ⚙️ Branch 2: Materials & Fabrication
| ID | Tier | Name | RP Cost | Prerequisite | Unlocks |
|----|------|------|---------|-------------|---------|
| MATERIALS_1 | 1 | Blast Furnace Steel | 100 | None | Steel furnaces, in-kind fabrication of rails |
| MATERIALS_2 | 2 | Electronics Fabs | 250 | MATERIALS_1 | Catenary wiring, signalling microchips |
| MATERIALS_3 | 3 | Superalloys & Composites | 600 | MATERIALS_2 | 90% fabrication discount, +15% facility output |
| MATERIALS_4 | 4 | Quantum Neural Fabs | 1500 | MATERIALS_3 | Autonomous telemetry, AI routing, quantum crystals |

---

## Appendix C: Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+I` | Open Industrial Facilities & Supply Chain dashboard |
| `R` | Rotate blueprint 90° clockwise (in stamp mode) |
| `F` | Flip/mirror blueprint horizontally (in stamp mode) |

---

## What's Next?

After completing this tutorial, you've mastered the foundations. Here's what lies ahead:

- **Multi-chain logistics**: Build all 4 pipelines simultaneously, managing cargo flow across 3+ worlds
- **Tech Tree depth**: Research through all 4 tiers to unlock electric and maglev trains
- **Colonisation**: Found outposts on Phase 4 Expansion worlds and develop them from wilderness
- **Federation**: Connect your game instance to other servers for inter-server train transport (experimental)
- **Lore AI competitors**: Enable CST, Grand Central, and InterWorld AI factions to compete for territory
- **Advanced Megacities**: Achieve HyperGrowth by supplying all 3 cargo tiers simultaneously

*Good luck, Director. The Commonwealth is watching.*
