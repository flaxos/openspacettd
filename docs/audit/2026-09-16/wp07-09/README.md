# WP-07–09 local implementation evidence — 16 September 2026

Both `build/openttd` and `build/openttd_test` are rebuilt. The full CTest suite
passes **363/363**, including native regression scripts. [Manifest](manifest.json)
records source/binary/fixture hashes; the working tree includes earlier recovery
packages and is not a clean release commit.

## WP-07: capture and usable routes

- Native capture callbacks cover drag, naming, empty/foreign rejection, naming
  cancellation and selection abort. This hidden GUI case is now registered in CTest.
- Switching Capture/Place or clicking either tool twice now keeps the selected
  action active. The previous tool's native abort completes before the new mode is
  set. The regression captures a real selection and stamps it through the dispatcher.
- All eight layouts are checked after sequential rotations and mirrors. Native
  locomotives, built in depots and given real orders, complete 39 movements across
  eight transformations: **312 runs**, with all signals intact and 90-degree turns
  forbidden. Coverage includes six Wye and twelve roundabout movements, both
  corridor turnbacks, the passing loop, all RoRo/balloon platforms and both depots.
  Alternate platform reservations force each platform; a native waypoint selects
  the passing loop. Trains enter the destination station/depot, not just its approach.
- The stronger test initially found 88 failures in the Wye/corridor checks. Wye
  curves now connect to correctly directed lanes. Corridor turnbacks use the outer
  rows of the existing 10×4 footprint, giving the curves enough space.
- Built-ins carry `layout_revision: 2`, separately from JSON format version 1.
  Missing revisions default to 1. Imports retain their geometry even with the same
  name as a revised built-in; scans do not rewrite the original files.

## WP-08: native founding and preservation of older state

Native town founding initializes roads, houses, population and spatial lookup
before world promotion. Command tests cover rejected sites/callers, lack of funds,
pool exhaustion, directory agreement and save/reload of a populated town.

**Legacy repair decision:** preserve older incomplete towns and surrounding state;
do not generate houses automatically or silently replace their identities.
Colonisation retries fail without charging or promoting again. Automatic repair
is unsupported; a separate salvage operation would need its own scope.

The two retained `fixtures/empty-outpost-v1-*.sav` files reproduce an empty town
before/after promotion, with one owned rail tile. They are generated fixtures in
the current native schema, not copies of historical user saves. Both reload with
the same town ID/name/location, phase/score/outpost location, rail and money.
The 53-assertion fixture test then verifies a rejected retry leaves state unchanged.

## WP-09: establishment, cargo and reserves

The Corporate HQ window supplies Establish HQ, owned-platform Build Hub, hub/cargo
selection and Set Reserve. Displayed costs are 2,500,000 Cr for HQ and 75,000 Cr for
a hub. HQ requires 5,000,000 Cr cash before construction. The Core site plus owned
rail stations on Developed/Frontier Worlds establish local presence; registered
charter presence can also satisfy the existing three-phase rule.

Ordinary unloading/loading exchanges hub stock; Transfer and No unloading retain
native order semantics. Station production inputs keep priority. A fresh-company
test creates native stations, establishes HQ/hub through real GUI callbacks, edits
reserve, and rejects invalid/unaffordable sites and an out-of-range reserve.
It builds a locomotive and two coal wagons in a depot and starts them with a native
station order. The train physically reaches the hub, deposits 60, loads 20 and
retains 40 in stock. Conservation is checked on every movement/station tick.
The initial 60 units are fixture cargo; no cargo is added or vehicle repositioned
after departure. Switching HQ/Hub tools or re-clicking them also remains functional.
A separate process reloads HQ, hub, reserve, stock and onboard cargo. That reload
uses installed base graphics for normal engine/NewGRF startup, searching both
`build/baseset` and the normal user/shared game data folders used by CI, including
packed tar sets. The CTest is labelled `integration;requires-baseset`.

## Checks and remaining acceptance

- [Final build](logs/build-final.log), [full CTest](logs/ctest-final.log).
- [Failing signalled routes](logs/signalled-routes-before.log),
  [passing CST tests](logs/cst-after.log), [GUI checks](logs/gui-after.log).
- [Retained fixture creation/reload](logs/legacy-fixtures.log).
- [Failing tool-switch regressions](logs/tool-switch-before.log) and
  [passing capture, establishment, cargo journey and reload](logs/tool-switch-after.log).
- `git diff --check` passes. Original user saves and libraries were preserved.

Visual UAT is **pending**: `orca-ide: command not found`; native app control is
unavailable. Run the updated [UAT-04, 08 and 10–12](../../../../demo/ALL-FEATURES-UAT.md)
and save/reload on copies using the rebuilt binary. Automated callbacks and the
native train journey do not certify a human-operated delivery or complete gameplay slice.
The earlier positive user crash-smoke report predates these changes.
