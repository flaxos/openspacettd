# Sprint 28 UAT: Guided Solo Acceptance Test

Launch the prepared all-feature three-world save:

```sh
./build/openttd -g demo/OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav
```

The same file is visible in the normal load dialog under `OpenSpaceTTD-Demos`.

---

## Story Book Guided Walkthrough

Open the Story Book (`Manage Company` > `Story Book` or via the company toolbar button). Use the 7 chapters and 16 persistent goals to step through the vertical slice:

### 1. Planetary Navigation & Multi-World Biomes
1. In Chapter 1 (**1. Overview & Planetary Navigation**), click the location pins or press `Ctrl+Alt+1`, `Ctrl+Alt+2`, and `Ctrl+Alt+3`.
2. Verify World 1 shows temperate green landscape, World 2 shows arid industrial styling, and World 3 shows sub-arctic snow-line terrain.
3. Verify the Map dropdown menu contains direct jump entries: `Jump to World 1`, `Jump to World 2`, `Jump to World 3`.

### 2. Monumental Portal Gates & Wormhole Consist Run
1. In Chapter 2 (**2. Monumental Portal Gates**), click **Gateway Alpha - Phase 1 head**.
2. Follow the `UAT Wormhole Demonstrator` train. Confirm it enters the gate on the NE-SW track axis and emerges smoothly at the Phase 2 head on the NW-SE track axis without jumping or glitching.
3. Select the Portal Gate tool on the Railway Construction toolbar. Build a portal gate on clear land; observe the automatic 18-tile terminal with parallel holding lane and path signals. Switch to Link mode to pair it with a second gate.

### 3. Player Rail Blueprints & CST Prefabs
1. In Chapter 3 (**3. Player Blueprints & CST Prefabs**), click **CST Prefab Staging Area**.
2. Press `B` (or click the Blueprint toolbar icon).
3. Select a CST prefab (e.g. `CST Dual-Track Passing Siding`). Press `R` to rotate, `F` to mirror (RHD/LHD invariance), and stamp it onto the staging pad.
4. Click **Custom Blueprint Capture Track Layout**. Click `Capture From Map` in the Blueprint Library window, drag across the sample tracks, name it, and save it. Stamp a replica.

### 4. Planetary Operations (Spaceports & Edge Conduits)
1. In Chapter 4 (**4. Planetary Operations: Spaceports & Conduits**), click **Phase 1 Spaceport candidate airport**.
2. Open the station window for `Phase 1 Spaceport Candidate` and click `Designate Spaceport`.
3. Upgrade to Tier 2 and Tier 3, observing live telemetry and projected monthly output.
4. Click **Phase 3 signed Edge Conduit construction tile**. Select the Edge Conduit tool on the rail toolbar and build on the signed boundary tile.
5. Inspect the conduit with `Land Area Information` to confirm 100 units/mo Frontier mineral extraction.

### 5. Megacity Demands & Freight Corridors
1. In Chapter 5 (**5. Megacity Demands & Freight Corridors**), click **Oaktree Core**.
2. Open the Town window and click `Megacity`, or select `Megacity Overview` from the Town toolbar menu.
3. Observe Tier 1 Sustenance, Tier 2 Expansion, and Tier 3 Prosperity demand progress bars.
4. Click **Gateway Alpha Freight Corridor**. Open Map menu > `Freight Corridor Monitor` to view route pairs, active transit consist counts, capacity utilization, and congestion indicators.

### 6. Supply Chain Matrix & Federation Governance
1. In Chapter 6 (**6. Supply Chain Matrix & Federation Governance**), open Map menu > `Supply Chain & Trade Ledger`.
2. Inspect Tab 1 for Commonwealth macro phase flows, spaceport launches, conduit extraction, and tariffs.
3. Inspect Tab 2 for inter-world balances and confirm the green `CONSERVED` zero-sum balance status.
4. Open Map menu > `Federation Authentication & Charters`.
5. Review the active player session badge, test account registration/login, create a corporate charter, and expand presence to World 1.

### 7. Commonwealth Data Crystals Rebranding
1. In Chapter 7 (**7. Commonwealth Data Crystals Rebranding**), verify:
   - Graphs > Cargo Payment Rates lists `Data Crystals`.
   - Game Settings search shows `Distribution mode for data crystals`.
   - Station acceptance and waiting cargo displays `Data Crystals`.
   - Train depot purchase list contains the `Data Van` wagon.
   - Road depot purchase list contains the `MPS Data Courier`.
