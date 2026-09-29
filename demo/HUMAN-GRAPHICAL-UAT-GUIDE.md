# OpenSpaceTTD Human Graphical UAT Playbook & How-To Guide

> **Authority Notice:** Passing automated unit tests (473 cases / 67,508 assertions) and green CTests (485/485) prove deterministic internal state machines, but **do not constitute human graphical acceptance**.
> Human visual playthrough is mandatory to verify GUI layout, font scaling, multi-line text wrapping, player feedback clarity, and actual gameplay ergonomics.

---

## 1. Quick Start & Execution Setup

### 1.1 Prerequisites
1. OpenSpaceTTD built with configured dependencies:
   ```bash
   ninja -C build
   ```
2. Verify all NewGRF packs are compiled in `bin/newgrf/`:
   - `openspacettd_integrated_v1.grf`
   - `openspacettd_rail_v3.grf`
   - `openspacettd_equipment_v1.grf`

### 1.2 Test Saves Provided
The repository contains two verified test fixtures in `demo/`:

| Savegame File | Purpose | Use When Testing |
|---|---|---|
| `demo/OpenSpaceTTD-Integrated-Economy-UAT.sav` | **Running, fully-connected Commonwealth economy.** Contains 40 operating trains, 13 physical factories, Megacity delivery networks, and an active commissioned gate. | Testing steady-state factory buffers, starvation/recovery, research escrow, warehouse priority, and gate transit. |
| `demo/OpenSpaceTTD-Integrated-CST-Sector.sav` | **Clean-slate 7-world Mito–Merredin sector.** Initial capital, zero trains, empty city reserves, and unprospected frontier sites. | Testing cold-start world roles, paid prospecting/surveying, initial Fund Industry placement, and baseline startup ergonomics. |

### 1.3 Launching the Game
Launch the graphical client with the integrated configuration:

```bash
# Launch the running economy scenario (Primary Playthrough):
./build/openttd -c demo/integrated_economy.cfg -g demo/OpenSpaceTTD-Integrated-Economy-UAT.sav

# Or launch the clean-slate sector:
./build/openttd -c demo/integrated_economy.cfg -g demo/OpenSpaceTTD-Integrated-CST-Sector.sav
```

---

## 2. Multi-Scale UI Auditing Guidelines

Before executing the gameplay tracks, configure and audit the UI scaling. Custom OpenSpaceTTD panels must render legibly without clipping, overlapping, or overflowing text.

### How to Change UI Scaling
1. Open **Game Options** (`Alt+O` or Main Menu gear icon) → **Interface**.
2. Adjust **GUI Scale** (or edit `gui_scale` under `[gui]` in `demo/integrated_economy.cfg`):
   - **100% (1.0x)**: Standard baseline resolution.
   - **125% (1.25x)**: High-DPI compact laptops.
   - **150% (1.5x)**: Standard 1440p/4K displays.
   - **200% (2.0x)**: Ultra High-DPI / 4K TV scaling.

### Visual Audit Criteria for All Custom Windows
Inspect every window during the playthrough against these four rules:
* [ ] **No Text Truncation:** Strings must not be clipped by widget boundaries or replaced by empty boxes.
* [ ] **Clean Multi-line Wrapping:** Long descriptions (such as recipe inputs/outputs and storage priority warnings) must wrap to subsequent lines rather than overflowing horizontally.
* [ ] **Button Alignment:** Text labels must fit comfortably inside push buttons without overflowing the button bevel.
* [ ] **Window Resizability:** Dragging the resize handle in the bottom-right corner must cleanly scale list matrices and keep headers anchored.

---

## 3. Step-by-Step Graphical Playthrough Tracks

Execute the following 8 tracks in sequence. Record your observations in [HUMAN-UAT-CHECKLIST.md](HUMAN-UAT-CHECKLIST.md) or [UAT-RESULTS.md](UAT-RESULTS.md).

---

### Track 1: Universe Directory & CST Star Map

**Objective:** Verify multi-world geography, regional economic roles, CST public backbone links, and access policies.

1. **Open the Star Map:**
   * Open the Map dropdown menu on the main toolbar → Select **Universe Directory**.
   * In the Universe Directory window, click the **Galaxy Map / Star Map** button (`UD_MAP`).
2. **Inspect the 7-World Mito–Merredin Sector:**
   * Locate all seven worlds arranged on the star grid:
     1. **Mito** (World 0) — Phase 1 Core (Gold badge).
     2. **Merredin** (World 1) — Phase 2 Industrial (Cyan badge).
     3. **Clonclurry** (World 2) — Phase 3 Frontier (Green badge).
     4. **Valvida** (World 3) — Phase 3 Frontier (Green badge).
     5. **Chelva** (World 4) — Closed Frontier (Grey badge).
     6. **Tandil** (World 5) — Closed Industrial (Grey badge).
     7. **Pioneer Reach** (World 6) — Closed Wilderness (Grey badge).
3. **Verify CST Backbone & Access Controls:**
   * Click on the line connecting Mito and Merredin. Confirm it is marked as a **Public CST Backbone Link**.
   * Note the toll rate display.
   * Click on an owned gate and verify the **Access Mode** toggle alternates between **Public** (tolls charged to other companies) and **Private** (exclusive company use).
4. **Promotion Role Invariance:**
   * In the clean save, inspect a Frontier colony world.
   * Promote the colony to the next phase.
   * **Expected:** The world's economic role remains **Frontier**; raw resource extraction remains legal and is not invalidated by colony development.

---

### Track 2: Paid Resource Surveys & Gated Extraction

**Objective:** Verify that primary extraction requires paid prospecting, that company knowledge persists, and that unsurveyed placement is rejected.

1. **Open Fund Industry Window:**
   * Click the Industry icon on the toolbar → Select **Fund New Industry**.
2. **Inspect Gated Industry Information:**
   * Select **Iron Ore Mine** or **Copper Ore Mine**.
   * Look at the bottom information panel.
   * **Expected:** The panel clearly shows:
     * `Economic role: Frontier`
     * `Research: Materials I` (or `Materials II`)
     * `Required Site: Survey required`
3. **Open Resource Surveys Browser:**
   * Click the **Survey Sites** button on the Fund Industry window (`WID_DPI_SURVEY_WIDGET`).
   * The **Resource Surveys** browser opens (`WindowClass::ResourceSurvey`).
4. **Perform an Area Survey:**
   * Click **Survey Area** (`RW_SURVEY`). The cursor turns into a 16×16 tile highlight box.
   * Click on unprospected terrain on **Clonclurry** or **Valvida**.
   * **Expected:**
     * The survey fee is deducted from company funds.
     * Hidden resource sites within the 16×16 box are revealed and added to the company's discovered sites list.
     * The overlay displays resource icons on the surveyed tiles.
5. **Attempt Unsurveyed Placement:**
   * Attempt to manually fund a mine on an unprospected tile.
   * **Expected:** The engine rejects construction with `STR_RESOURCE_SITE_REQUIRED` or `STR_ERROR_CAN_T_CONSTRUCT_THIS_INDUSTRY`.
6. **Construct on Discovered Site:**
   * In the Resource Surveys window, select a discovered site and click **Build Industry** (`RW_BUILD`).
   * **Expected:** The mine constructs successfully on the verified anchor tile.

---

### Track 3: Physical Industry Factory Operations & Buffer Saturation (`Ctrl+I`)

**Objective:** Verify that physical factories use bounded input/output buffers (3-month storage limit), that batch processing strictly requires all inputs, and that backpressure halts production.

1. **Open Facilities Dashboard:**
   * Press **`Ctrl+I`** (or Map dropdown → **Industrial Facilities**).
   * Verify the 5 tabs: **All**, **Pipeline A (Structural)**, **Pipeline B (Electronics)**, **Pipeline C (Propulsion)**, and **Pipeline D (Data Crystals)**.
2. **Inspect an Active Factory:**
   * In `OpenSpaceTTD-Integrated-Economy-UAT.sav`, select the **Steel Mill** on Merredin.
   * Check the display:
     * Monthly Batch Capacity: (e.g., 100/mo base, up to 1000 with upgrades).
     * Input Buffer: Iron Ore units currently stored.
     * Output Buffer: Steel units awaiting pickup.
3. **Test Feeder Starvation (Missing Inputs):**
   * Locate the freight train delivering Iron Ore to the Steel Mill station.
   * Stop or redirect the train so no Iron Ore arrives.
   * Fast-forward simulation for 1–2 months.
   * **Expected:**
     * Input buffer exhausts to 0.
     * Production halts completely (0 batches processed).
     * Monthly output remains 0; no steel is fabricated out of thin air.
4. **Test Buffer Backpressure (Full Outputs):**
   * Restart Iron Ore deliveries, but **stop all collection trains** that pick up Steel.
   * Fast-forward simulation until the output buffer reaches capacity (3 months of rated production, e.g. 300 units).
   * **Expected:**
     * When the output buffer fills, factory conversion stops.
     * Input cargo remains in the input buffer without being wasted.
     * Production resumes immediately once collection trains clear output stock.
5. **Verify Warehouse Isolation:**
   * Check an owned warehouse on a different world.
   * **Expected:** Zero factory cargo appears in the warehouse unless physically delivered there by train.

---

### Track 4: Owned Logistics Warehouses & Storage Priority

**Objective:** Verify warehouse station priority, zero-fare storage delivery, and reserve floor configuration.

1. **Inspect Warehouse Station Window:**
   * Click on an owned station platform designated as a Logistics Hub (e.g. Augusta Hub or Mito Central Hub).
   * **Expected:** A clear banner reads:
     > *"Warehouse: your unloaded freight enters company stock on this world. Storage earns no final delivery payment and supplies no city growth, even when consumers share this station's catchment."*
2. **Verify Priority Catchment:**
   * Observe a train unloading cargo at a station where both a factory/city and an owned warehouse share the catchment area.
   * **Expected:** Unloaded cargo enters warehouse stock first according to project priority; excess remains on the consist if warehouse capacity is exceeded.
3. **Inspect Delivery Finances:**
   * Open the income/loss tooltip when a train unloads cargo into a warehouse hub.
   * **Expected:** Storage deposit earns **£0 delivery payment** (storage is company transfer, not commercial sale).
4. **Edit Reserve Floors:**
   * Open Corporate HQ (`Map Menu` → `Corporate HQ`) → Select **Logistics Hubs** tab.
   * Select a Hub and choose a cargo (e.g. Structural Steel).
   * Adjust the reserve floor slider.
   * **Expected:** Outbound trains loading at this hub will only load cargo *above* the specified reserve floor, preserving necessary local stock.

---

### Track 5: Corporate HQ, Tech Tree & Atomic Research Kit Escrow

**Objective:** Verify Corporate HQ campus tiers, dual-mode BOM preview, Tech Tree R&D, and atomic research kit escrow.

1. **Open Corporate HQ:**
   * Map menu → **Corporate HQ**.
   * Verify all tabs: **Overview**, **Stockpiles**, **Logistics Hubs**, **Fabrication**, **Tech Tree**, and **Alliances**.
2. **Review Planetary Stockpiles:**
   * Click **Stockpiles** tab. Confirm inventories of Steel, Copper Wiring, Silicon Chips, and Machine Modules are accurately tracked per-world.
3. **Inspect Tech Tree & Research Escrow:**
   * Click **Tech Tree** tab.
   * Select a **Tier III Research Project** (e.g. *High-Speed Transit III* or *Advanced Materials III*).
   * Note the requirement: requires cash budget PLUS **40 Steel + 20 Silicon Chips**.
   * **Expected:**
     * Monthly research budget advances project progress.
     * Progress pauses at 100% waiting for the material kit.
     * When 40 Steel and 20 Chips are present in the planetary stockpile, the entire kit is **reserved atomically in escrow**.
4. **Test Escrow Refund on Project Switch:**
   * While the material kit is reserved in escrow, switch research to a different project.
   * **Expected:** The reserved 40 Steel and 20 Chips are immediately refunded back to the planetary stockpile without loss or duplication.
5. **Feedstock Acceleration Check:**
   * Inspect the **Acceleration** toggle (`WID_CHQ_ACCELERATION`).
   * **Expected:** Defaults to **OFF**. When toggled, it clearly indicates that optional feedstock consumption accelerates research speed but never consumes kits reserved for project completion.

---

### Track 6: Material-Gated Construction, Autoreplace & CST Prefabs

**Objective:** Verify that advanced infrastructure and vehicles strictly enforce physical material bills even in Cash Mode, and that CST prefabs place cleanly.

1. **Test Material-Gated Rail Construction:**
   * Switch the railway construction toolbar to **Electric Rail** or **Maglev**.
   * In a world with zero stockpiled materials:
     * Attempt to lay electric track or place an electric signal.
     * **Expected:** Construction fails or previews missing materials (e.g. *Requires 1 Copper Wiring, 1 Signalling Equipment*). Cash alone cannot bypass this gate.
   * In a world with sufficient stockpiled materials:
     * Place the track/signal.
     * **Expected:** Construction succeeds; the exact BOM materials are deducted from the planetary stockpile.
2. **Blueprint Library Purchase Mode Toggle:**
   * Press **`B`** (or Rail toolbar → Blueprint Library icon).
   * Locate the **Purchase Mode** button (`WID_BPL_FABRICATION_TOGGLE`) above the status bar.
   * Click to toggle between **Cash Mode** and **Stockpile / Fabrication Mode**.
   * **Expected:** Button text and tooltip update cleanly without text clipping.
3. **Stamp a CST Prefab:**
   * In the Blueprint Library, select **`CST_Logistics_Hub`** or **`CST_Passing_Siding`**.
   * Click **Place** (`WID_BPL_PLACE`) and hover over a clear, flat terrain area.
   * Rotate (`R`) and Flip (`F`) the layout. Confirm preview updates correctly.
   * Click to place the prefab.
   * **Expected:** The layout stamps onto the map, and corresponding materials are deducted from the stockpile.

---

### Track 7: Core Megacity Food Basket, Starvation & Recovery

**Objective:** Verify that Core worlds consume balanced 3-tier cargo baskets, that growth halts during food starvation, and that passenger production suffers a 50% penalty until recovered.

1. **Open Megacity Overview:**
   * Town menu → **Megacity Overview** (or click on the city name **Mito**).
   * Inspect the three demand tiers:
     * **Tier 1 (Staples):** Food (Grain/Processed Food).
     * **Tier 2 (Industrial):** Structural Steel and Ballast.
     * **Tier 3 (High-Tech):** Silicon Chips and Consumer Crystals.
2. **Check Growth State:**
   * When all tiers are supplied: Confirm growth status displays **Active Growth** with a visible countdown timer.
3. **Induce Food Starvation:**
   * Stop the trains delivering Food to Mito Central Station.
   * Let simulation run for over **3 game months** until food reserves drop to 0.
   * **Expected Visual Outcomes:**
     * Megacity status changes to **Starvation / Stalled**.
     * Growth countdown timer freezes; the town stops building new houses.
     * Passenger and mail production in Mito drops by **50%**.
     * Megacity status bar displays a clear starvation warning.
4. **Restore Food Service & Observe Recovery:**
   * Resume food freight deliveries.
   * Observe the Megacity window as food arrives.
   * **Expected:**
     * Food reserve bar replenishes.
     * Starvation warning clears.
     * Passenger generation returns to 100% capacity.
     * Growth timer resumes countdown.

---

### Track 8: Multi-World Portal Transit & Federation Gate Commissioning

**Objective:** Verify train transit through wormhole gates, physical gate commissioning via freight delivery, and remote schedule orders.

1. **Observe Consist Gateway Traversal:**
   * Find a train en route between Mito and Merredin in `OpenSpaceTTD-Integrated-Economy-UAT.sav`.
   * Follow the train as it approaches the Gateway portal head.
   * **Expected:**
     * Train enters portal head cleanly.
     * Tail wagons traverse without decoupling or graphical snapping.
     * Train emerges at the destination world's portal head on the correct track.
2. **Commission a New Gate on the Star Map:**
   * Open the Star Map (`Map Menu` → `Universe Directory` → `Star Map`).
   * Select a reachable closed frontier world (e.g. **Chelva**).
   * Select departure gate and arrival zone.
   * Click **Start Project** (`SW_START`).
   * Deliver the quoted Bill of Materials (Structural Steel and Machine Modules) via train to the portal terminal.
   * **Expected:**
     * Delivered cargo counters increment in the Star Map project list.
     * Partial deliveries update the progress bar.
     * Once 100% delivered, click **Activate** (`SW_ACTIVATE`).
     * The gate opens, the destination world becomes accessible, and the link turns green.
3. **Inspect Remote Station Orders:**
   * Open train orders window → Click **Remote stop**.
   * Verify the picker displays reachable stations across the portal network.
   * Add a remote station to the timetable and confirm the train executes the schedule.

---

## 4. Troubleshooting & Savegame Recovery

* **Accidental save corruption:** Do not overwrite the baseline fixtures. Keep `demo/OpenSpaceTTD-Integrated-Economy-UAT.sav` read-only or copy it to `demo/save/` before playing.
* **Console Commands for Inspection:**
  * Press **`` ` ``** (tilde) to open the in-game developer console:
    * `resource_sites`: prints active survey mode and discovered site counts.
    * `connected_economy status`: outputs active economy counters, facilities, and city quotas.
    * `connected_economy audit`: audits terrain slopes and cargo string validity.

---

## 5. Result Submission

Upon completing the playthrough, transfer your findings to **[demo/UAT-RESULTS.md](UAT-RESULTS.md)**. Note any text overlaps, UI scale quirks, or unexpected behaviour.
