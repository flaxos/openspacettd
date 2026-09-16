# WP-11: ore → steel → construction

**Fixture version 1; exact Commonwealth v2 pack hashes required.** The older UAT
save is unchanged. These are new, separate single-map games with three logical
worlds and local portals. Automated checks passed; the visual checks below have
not been performed by the assistant.

## Start

Use rebuilt `build/openttd` and a copy of one of these saves:

- [Construction ready](wp11-v1-construction-ready.sav): mine, worlds, startup cash,
  Materials I, 5 ballast; build the route yourself.
- [Operational](wp11-v1-operational.sav): two ordered trains, two portal pairs,
  furnace and hub, paused before the first cargo is produced. **Start here for
  the quick check.**
- [Completed](wp11-v1-completed.sav): three loads delivered after reload/obstruction
  recovery, connected Core depot fabricated, 110 steel and 0 ballast remaining.

Do not replace NewGRFs in another save. If this fixture reports missing or
substituted content, restore the exact shipped packs before loading.
[Hashes and automated evidence](../docs/audit/2026-09-16/wp11/evidence.json).

## Quick visual check — UAT-13/14 and save/reload

1. Open the operational copy and inspect NewGRF Settings, cargo names and the
   two trains. The ore wagon carries **Iron Ore**; the flatcar carries **Structural
   Steel**. Both are ordinary ordered trains with Full load at pickup and No
   loading at delivery. Use the vehicle list to center on each train.
2. Unpause. Follow ore from the mine at `(20,35)` through the first portal to the
   furnace station at `(145,40)`. The furnace converts **2 ore → 1 steel** in monthly
   batches. Its second platform at `(145,45)` serves the steel train.
3. Follow the steel train through the second portal to the hub at `(280,45)`.
   Confirm actual unloading increases the Core stockpile; arrival alone is not
   delivery. Do not inject cargo or grant further research during this check.
4. Save while a loaded train is moving, quit, reload that copy and observe three
   complete further deliveries. Check both trains remain intact and resume orders.
5. Once the hub holds at least 10 steel, enable Fabricate from Stockpile and build
   a basic rail depot at `(283,45)`, facing the adjacent station. Expect **10 steel
   and 5 ballast consumed once**, plus the reduced construction cost. Disable the
   fabrication toggle afterwards. Terrain clearing is charged separately.
6. Inspect the research and purchase UI: Vulcan needs Traction I and is restricted
   to Frontier/Expansion. A late calendar date alone must not bypass that lock.
   Materials III's stated effects are 90% material-cost discount and +15% yield.

Record visual errors, any stuck train, unexpected cargo/refit name, incorrect
cost or changed reload state in [UAT results](UAT-RESULTS.md). Other pipelines,
multiplayer/federation, bespoke art and the wider UAT suite remain separate work.

## Construction-ready route

All coordinates are tile coordinates. Ore route: depot `(10,40)` facing SW,
three-tile station `(20,40)`, portal heads `(70,40)` SW and `(112,40)` NE,
three-tile furnace station `(145,40)`. Steel route: depot `(135,45)` facing SW,
three-tile platform `(145,45)` joined to the furnace, portal heads `(190,45)` SW
and `(232,45)` NE, three-tile hub station `(280,45)`. Connect each stretch of
ordinary rail to the automatically built portal terminals. Attach the furnace
to the joined station and the logistics hub to the Core station. Buy a Pioneer
plus ore hopper, and a Pioneer plus steel flatcar, then give the two-stop orders
above. Cash construction is used until steel reaches the final hub.
