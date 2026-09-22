# Building reproducible UAT saves

Use this workflow when creating, repairing or publishing a playable scenario.
It captures the connected UAT lessons: the first save moved cargo successfully,
but human play exposed 742 invalid terrain tiles and missing quantity text for
all 13 custom cargos. Simulation success alone did not prove a playable save.

## Start from the existing tools

| Task | Reuse |
|---|---|
| Build the network through engine commands | [`connected_economy.cpp`](../src/portal/connected_economy.cpp) |
| Generate, warm up, verify and publish | [`generate_connected_economy.py`](../scripts/generate_connected_economy.py) |
| Launch an isolated process and exchange console commands | `Engine` in [`test_wp11_slice.py`](../scripts/test_wp11_slice.py) |
| Audit geometry, cargo text and save/reload | [`test_connected_uat_recovery.py`](../scripts/test_connected_uat_recovery.py) |
| Compile and check exact content | [`build_commonwealth_grf.py`](../scripts/build_commonwealth_grf.py), [`manifest`](../pkg/commonwealth_manifest.json) |
| Player launch and short instructions | [`launch-connected-economy.sh`](../demo/launch-connected-economy.sh), [player guide](../demo/CONNECTED-ECONOMY-UAT.md) |

These tools implement a **specific** four-world, 1024×1024 Arctic fixture with
37 trains, 28 stations and 24 active cargo types. Their markers, counts, route
names and acceptance thresholds are not a generic scenario API. For a different
scenario, adapt the builder, guards, audit and expected topology together.
Keep worlds as immutable logical regions of one global map.

Before editing, inspect the working tree, actual launch binary, save filename,
content dependencies and authoritative project docs. Reuse known answers from
the session. Keep unrelated user changes, personal profiles and crash artifacts
out of the delivery commit; an isolated review checkout does not require a
second engine build. Never infer the running binary from the branch name alone.

## Shortest reliable development loop

Run commands from the repository root. Reuse the existing configured build and
installed base set. The headless runners need OpenGFX and a loopback socket;
request the environment's required network permission instead of changing the test.

1. **Build content first, only if NML changed.** Install the pinned requirements
   from `requirements-commonwealth-grf.txt` into an isolated environment if needed.
   Set `NMLC` to its compiler. The first command writes current GRFs and manifest;
   `--verify` only checks reproducibility and does not update them.

   ```sh
   NMLC=/path/to/nmlc python3 scripts/build_commonwealth_grf.py
   NMLC=/path/to/nmlc python3 scripts/build_commonwealth_grf.py --verify
   ninja -C build -j2
   ```

   Configure with `cmake -B build -G Ninja` only when needed. Choose parallelism
   for available memory/disk. Batch English string edits before compiling: changing
   the generated string header can rebuild hundreds of translation units. Do not
   run the old executable against partially rebuilt language packs.

2. **Fail early before the long soak.** Generate only a few diagnostic steps:

   ```sh
   python3 scripts/generate_connected_economy.py --steps 2 --output build/connected-smoke
   ```

   The runner audits terrain and cargo text before advancing. Inspect its console
   log and `evidence.json`; diagnostic runs intentionally leave `passed` false.
   Fix construction/content/geometry failures before spending time on warm-up.

3. **Reuse checkpoints for diagnosis.** Keep output directories separate:

   ```sh
   python3 scripts/generate_connected_economy.py --steps 2 \
     --resume build/connected-smoke/checkpoint.sav --output build/connected-debug
   ```

   A resume does not exercise generation or prove an empty-stock startup. Regenerate
   from a fresh map after builder/content changes. Do not publish a resumed run as
   evidence that the full cold-start scenario passed.

4. **Freeze the binary and content, then run the full acceptance once:**

   ```sh
   python3 scripts/generate_connected_economy.py --steps 160 --verify \
     --publish demo/OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav
   python3 scripts/test_connected_uat_recovery.py --output build/connected-recovery-current
   python3 scripts/test_connected_uat_recovery.py \
     --save demo/regression/connected-legacy-terrain.sav --output build/connected-recovery-legacy
   ```

   `--steps` bounds warm-up; the soak and recovery checks are additional work.
   Publishing requires `--verify`. The generator chooses a grown, supplied checkpoint;
   later destructive tests run on a copy. Re-run affected checks after changes or
   failures, not merely because time has passed. Prose-only edits do not require
   rebuilding the executable or regenerating saves; still perform required repo checks.

## Terrain and content invariants

Terrain heights belong to **corners shared by four tiles**. Adjacent corners must
differ by at most one height level; diagonally opposite corners may differ by two
on a valid steep slope. Flattening a rectangle can leave invalid cliffs just outside
it. `MakeVoid` also resets height to zero: preserve or reconcile the corner height
when creating void regions. Scan the whole map, including construction boundaries,
void strips and outer edges, before constructing the network and before publication.

Use normal terraforming where practical. The connected fixture uses a deterministic
height-relaxation plan before infrastructure is built. Its legacy recovery preflights
all four tiles sharing every changed corner before writing anything, and refuses
corners touching infrastructure. Do not turn that fixture-specific repair into a
blanket repair of arbitrary saves, or suppress `GetPartialPixelZ` assertions.

For cargo content:

- Resolve loaded cargos by **label**, never assumed numeric slots. NML cargo item
  IDs select cargo slots; setting `number` alone does not prevent native-slot
  replacement. Current custom cargos occupy 16–28, preserving native farm/food cargo.
- Define all five text properties: `type_name`, parameter-free `unit_name`,
  `type_abbreviation`, `units_of_cargo`, and `items_of_cargo`. The last two supply
  short units and quantity descriptions; a correct type name alone is insufficient.
- Audit every active cargo, including native ones, in `english_US.lng` and
  `english.lng`. Check singular names, units, quantities (zero and large amounts)
  and abbreviations for empty, undefined or invalid strings. Assert the language
  actually loaded. Preserve unit conversion; the current NML uses native unit IDs.
- Set freight flags and check actual engine/wagon refits for every intended cargo,
  including mixed native grain/livestock trains. Inspect the loaded catalog.
- Preserve published GRF files and hashes. Introduce a versioned successor, update
  build/install lists and the manifest, and explicitly test admission/compatibility.
  A compatible replacement flag is not equivalent to the exact-content contract.

## Make the logistics demonstrate the claim

Design the route graph before constructing trains: producer → collection station
or warehouse → processor → consumer, plus construction and research destinations.
Use ordinary authoritative commands for track, portals, stations, facilities,
vehicles, refits, orders, timetables and research. Check every command result.
Give ownership and prerequisite research explicitly; guard mutation commands so
only an isolated fresh fixture can invoke them.

Prove each service carries and **delivers** its intended cargo repeatedly. A moving
train, empty platform or positive aggregate production is not delivery evidence.
Count per train and cargo, including both cargos on a mixed consist. Provide enough
platform/vehicle capacity and check signals, pathfinding, order destinations and
return trips. Dedicated corridors are a useful first demonstrator before adding
shared junctions and congestion.

Distinguish warehouse storage, industry consumption and town delivery. Company
stockpiles are shared within a world; a warehouse transfer does not prove a train
reached a city. Consumer stations must cover the actual town's houses. Check factory
output routing and platform capacity so automatic stockpile overflow does not
silently bypass the railway being demonstrated. Use normal timetables or fleet
spacing to spread supply across monthly quotas.

Warm from empty cargo stocks using real production. Declare startup cash/research;
do not hide recurring injections, forced deliveries or population edits. Count
native houses and population, allowing rebuilding dips. Choose a checkpoint with
repeated supply and growth visible within a few minutes at normal speed, then test
food interruption, stopped growth and recovery. Separate generous showcase setup
from claims about balance or indefinite growth.

## Reliable harness and evidence

Use a separate config and disposable outputs. In automated save/reload checks set
`threaded_saves = false` and disable autosave/exit saves: the console's save message
may arrive before an asynchronous file is complete. Prevent unintended background
ticks while comparing state. The recovery runner uses `min_active_clients = 1`;
the generator explicitly pauses its prepared fixture and advances bounded ticks.
Always close processes in `finally` blocks and bound waits.

Reload in a fresh process with exact GRFs. Compare tick/date, cash, cargo in all
stores, vehicle state, factory buffers/progress, research and actual town state.
Normalize only known serialization differences (zero-valued buffer entries are
omitted); do not broadly discard differences. Reconcile native production, station
loss, deliveries, recipe transformations and research consumption. Test quoted
versus executed construction costs and exact prefab materials.

Audit geometry through both pixel-height and inverse viewport calls. A successful
headless economic soak does **not** exercise every viewport path or prove readable
windows. The recovery runner covers the entire fixture and both languages; Linux
CI runs it on current and archived public saves. Also inspect the GUI at the player's
language, base set, font/UI scale and window size. If graphical access is unavailable,
record that limitation and leave human acceptance pending.

Publish the save, compact evidence and complete run log together, with matching
binary/content/save hashes. Check hashes after the final build. Keep a public
regression fixture when an old generated save exposes a defect. Preserve personal
crash saves unchanged; recover to a separate file and compare progress. Do not
publish private crash artifacts as the regression fixture.

The player guide should give the exact launcher, starting state, short numbered
checks and expected time to observe growth. Use signs and clear service names in
the save. Keep technical detail here; wrap in-game instructions and test resizing.
The launcher copies its separate profile only once—changing the template does not
update an existing player profile automatically.

## Triage without repeating the whole build

| Symptom | First check |
|---|---|
| `GetPartialPixelZ` / `NOT_REACHED` while panning | Corner-height differences at flattening/void boundaries; audit the original save before masking anything. |
| Cargo name works, station/industry shows undefined text | `units_of_cargo` and `items_of_cargo`, then actual language and exact GRF loaded. |
| Trains move but city does not grow | Actual consumer delivery, house catchment, monthly quotas, supply spacing and warehouse/industry diversion. |
| A factory works but its collection train stays empty | Output platform capacity, overflow policy, refit and station/order identity. |
| “Saved” but file missing | Synchronous save setting, final path and file completion. |
| Reload differs by ticks | Automatic unpause/background simulation before snapshots. |
| No available language packs during a rebuild | Finish building the executable and matching language packs together. |
| Prefab says materials missing | Company purchase mode, local-world stocks and required technology; ordinary company HQ is not the corporate management UI. |

Finish with the repository's required tests/linters and update status, ledger,
limitations and UAT results. Record human failures and pending retests separately
from automated passes. See [fixture details](CONNECTED_ECONOMY_UAT.md) and
[recovery evidence](../demo/CONNECTED-UAT-RECOVERY.evidence.json).
