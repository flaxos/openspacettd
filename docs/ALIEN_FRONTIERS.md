# Alien Frontiers

Authored alien maps with a mission each, built from the same native pieces as
ordinary games: one global map split into immutable logical worlds, public CST
gates, Works industries, ordinary rail and a GameScript story. The first map is
**The Hollowfrost Survey**. [Player guide](../demo/alien_worlds/PLAY.md).

## The Hollowfrost Survey

A 512×256 map holding two worlds and a public CST gate between them.

| World | Role | Look | What is there |
|---|---|---|---|
| **Hollowfrost** (x 1–245) | Phase 3 Frontier, sub-arctic | Snowfields pocked by 17 impact craters, frozen meltwater lakes with icy gravel shores, snowy rock walls, two ice ridges, crystalline groves | Rare-earth mine on the floor of The Hollow, silica on the Glass Flats, Rimehold Observatory under the rim of Rimehold Crater, towns Rimehold and Hollow Camp |
| **Kestrel Forge** (x 266–509) | Phase 2 Industrial, volcanic | Charcoal basalt with eight cliff-walled mesas, the Ash Flats and obsidian spines | Monocrystal fab, Kestrel Survey Institute warehouse, towns Kestrel and Ochre Point |

**Mission.** Rail rare earths (RARE) and silica (SAND) through the gate to the
fab, which grows blank data crystals (BCRY) at two silica to one rare earth. Bring
blanks back through the gate to the observatory, which enriches them into quantum
crystals (QCRY), and store those at the Institute.

1. Deliver 180 SAND and 90 RARE to Kestrel Fab.
2. Deliver 60 BCRY to Rimehold Observatory.
3. Store 40 QCRY at the Kestrel Survey Institute (standard contract), or 100
   after choosing the priority contract offered once goal 1 is met. Bonus:
   £250,000 or £1,000,000. Free play continues afterwards.

**The puzzle.** The Institute warehouse stores everything a train unloads there,
and orders cannot pick cargo by type. One mixed survey train therefore has to
reach the Institute carrying only crystals: ore off at the fab with no loading,
then the Institute, then blanks on at the fab on the way home. Two trains on the
one surveyed line need passing loops and signals.

**Charter.** £2,000,000, Materials I–IV and Traction I licences, the four works
(their imported 50 STEL / 10 MACH construction kits are spent), five platforms on a
graded line at row 128, a depot at each end, an empty Institute warehouse, and one
empty survey train: a Vulcan with two silica hoppers, one rare-earth hopper, two
blank-crystal vaults and one quantum-crystal vault. All stores start empty; no rail
is laid. The map is authored paused, on the Works rules.

## How it is built

- `src/portal/alien_worlds.cpp`. `AlienTerrain::Field` composes corner heights
  from ridges, craters (inner wall, rim, apron, optional lake), mesas and level pads,
  tapers to sea level at the map border, then `Quantize` lowers any corner more than
  one level above a neighbour, so every tile is a valid slope. The authorer is the
  console command `alienworld prepare hollowfrost`. It accepts only a fresh, isolated,
  unowned 512×256 temperate game with integrated content, and removes the
  generator's towns (and with the last town its objects) through the native
  `DeleteTown` in a temporary editor scope. It then registers the two worlds, writes
  terrain, makes flat sea-level basins still river water, applies biome styling and
  exposed rock, and plants groves. After that it builds the gate pair, founds towns and
  signs as deity, and funds industries, platforms, warehouse, depots and the train as
  the company through ordinary commands.
- Gates reuse the generator's construction through the new
  `MultiWorldGen::BuildPublicGatePair`, factored out of world generation without
  changing it.
- `GSPlanet` (`src/script/api/script_planet.*`) gives game scripts read-only world
  facts and company world stock. The mission script
  (`bin/game/openspacettd_hollowfrost`) reads progress only from native cargo
  monitors and the Kestrel stockpile; it never moves cargo or builds. Milestone bits
  are set before their commands, so a reload cannot repeat a reward.
- Volcanic worlds now draw bare, rough and rocky ground with the charcoal palette
  that `VisualOverhaulManager` already defined for them. This changes every volcanic
  world's look, including generated ones; other biomes are unchanged.

## Proof and review tools

```sh
python3 scripts/alien_worlds.py prepare --output build/hf-prepare --publish demo/alien_worlds/Hollowfrost-Survey.sav
python3 scripts/alien_worlds.py prove --opening demo/alien_worlds/Hollowfrost-Survey.sav --output build/hf-prove
python3 scripts/alien_worlds.py shots --save build/hf-prove/hollowfrost-complete.sav --output build/hf-shots
python3 scripts/alien_worlds.py play --profile /path/to/new/profile
```

`prepare` checks terrain validity, lakes, snow, basalt, rock and groves, the
mission cargo text, the briefing, empty stores, the gate and charter, and an
identical cold reload. `prove` works on a copy: it lays the reference rail, orders
the starter train as above, and runs the real dedicated-server loop at full speed
(`alienworld speed 0`) until all three goals complete, with a four-year bound. Then
it checks the completion pages and cold-reloads the result. `shots` renders a
minimap and eight named views on a private SDL offscreen display.

## Adding the next map

Each map needs a terrain recipe, a layout of pads and works, an authorer entry and
a mission script. Keep works on the platform side within catchment (`Prepare`
fails otherwise), match cargo ratios to recipes, and remember the base set's
limits. In a temperate game, rock and rough ground draw green and desert draws as
snow; snow, water, bare earth and the volcanic charcoal palette are the strongest
alien surfaces available without new sprites.
