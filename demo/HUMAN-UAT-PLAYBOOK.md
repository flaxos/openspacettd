# OpenSpaceTTD Commonwealth UAT Playbook

This playbook provides human verification steps for the OpenSpaceTTD Commonwealth features introduced and stabilized in Sprint 50. Use the generated `OpenSpaceTTD-Commonwealth-UAT-v1.0.sav` fixture to execute these tests.

## Preparation
1. Load `demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav` in the game.
2. You will start as **Company 0 (Commonwealth Interplanetary Transport)**.
3. Pause the game to review the map and company status before starting.

## Test Cases

### UAT-01: Blueprint Stamping & BOM Consumption
**Objective:** Verify that CST blueprints can be placed and that they correctly consume resources from the planetary stockpile.
1. Navigate to World 0 (Sol Earth Core) using the world-switcher (Ctrl+Alt+1).
2. Open the Corporate Headquarters dashboard. Note the stockpiled amounts of Structural Steel and Enriched Quantum Crystals.
3. Select the Blueprint construction tool from the rail toolbar.
4. Attempt to place a `CST_Logistics_Hub` blueprint on an empty flat area near the city.
5. **Expected:** The blueprint stamps successfully, laying out the prefab structure (platforms, tracks).
6. **Expected:** The stockpile correctly deducts the corresponding BOM (Bill of Materials) for the placed blueprint.

### UAT-02: Resource Extraction Loop (Merredin -> Augusta)
**Objective:** Verify that the automated mining loop between Merredin and Augusta is operational and moves cargo.
1. Navigate to World 2 (Merredin Mining Colony) using the world-switcher (Ctrl+Alt+3).
2. Locate the Iron Ore Mine and the adjacent station. 
3. Unpause the game.
4. **Expected:** A train should arrive, load Iron Ore, and depart.
5. Follow the train as it enters the portal gateway to World 1 (Augusta CST Hub).
6. **Expected:** The train successfully traverses the gateway, arrives at Augusta, and unloads the Iron Ore to the smelting facility.

### UAT-03: Gateway Staging & Holding Queues
**Objective:** Verify that trains properly queue in staging sidings when the primary gateway throat is congested.
1. Navigate to the main Portal Gateway connecting World 1 (Augusta) and World 3 (Prometheus).
2. Observe the holding sidings adjacent to the portal arch.
3. Using the vehicle list, dispatch multiple trains through the portal simultaneously to create congestion.
4. **Expected:** Trains that cannot immediately enter the portal should automatically route into the holding loops/sidings.
5. **Expected:** Once the portal clears, the holding train should depart the siding and enter the portal without throwing a "Path not found" error.

### UAT-04: Corporate Charters & Gate Access
**Objective:** Verify that competitor companies must adhere to gate access charters and pay tolls.
1. Switch to Company 1 (Consortium Heavy Industries).
2. Locate a train owned by Company 1 that attempts to use a gateway owned by Company 0.
3. **Expected:** The train can traverse the gateway if a neutral or allied charter is in place.
4. Open the financial ledger for Company 1.
5. **Expected:** A toll deduction is recorded under "Infrastructure Maintenance" or "Toll Fees" for using Company 0's gateway. 
6. Switch back to Company 0 and check the income statement to verify the toll was collected.

