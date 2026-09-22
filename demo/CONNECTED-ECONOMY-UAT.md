# Connected Economy UAT v2.0

Open **OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav** with the current build, or run:

```sh
./demo/launch-connected-economy.sh
```

The launcher uses a separate settings profile. Your v1.0 save is preserved.

## Quick checks

1. **Unpause.** Open **Towns → Megacity Overview** and select Commonwealth Metropolis.
   The Signs list also has **START: City growth**.
2. **Watch growth for 5–10 minutes at normal speed.** The starting city has
   **872 people and 32 houses**. In the verified run it reaches **905 people and
   35 houses after about 5½ minutes**. Population can dip during rebuilding.
3. **Follow deliveries.** The train list includes **Food to megacity**,
   **Chips to megacity**, and **Composites to megacity**. Open
   **Map → Industrial Facilities & Supply Chain** to inspect all eleven factories.
4. **Try a CST prefab.** Open the rail toolbar's **Blueprint Library**. Select
   **CST Mainline Double Straight** and place it in the signed **CST prefab test area**
   near `(100, 200)`. Cash mode works immediately. In-kind mode uses earned stock;
   this prefab needs **32 ballast, 18 steel, and 2 wiring**.
5. **Try research.** Open **Map → Corporate Headquarters & Stockpiles**, then
   **Commonwealth Tech Tree**. Start **Dual-Track Throat Arrays**. Delivered chips
   and quantum crystals support research alongside the existing cash budget.

Optional, on a copy: stop **Food to megacity** for three months. Food supply falls
and growth enters starvation. Restart it to restore supply and growth.

## What is connected

Four logical worlds share one map. **37 trains** serve **28 stations**, all four
Commonwealth production chains, native wheat/livestock → food, and passengers.
Short collection services feed warehouses; long services cross portal gates.
Warehouses share their company's stockpile within each world.

The save starts with an operating economy warmed from empty cargo stocks. Startup
cash and three prerequisite technologies are scenario setup. No recurring cargo
injections or forced city growth are used.

Automated acceptance passed; **human visual acceptance is still pending**.
See the [evidence](OpenSpaceTTD-Connected-Economy-UAT-v2.0.evidence.json) and
[reproduction details](../docs/CONNECTED_ECONOMY_UAT.md).
