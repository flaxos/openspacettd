# Portal rail pathfinding crash — 16 September 2026

The reported `yapf_destrail.hpp:194` assertion is fixed in source commit
`174e57178089be5cf5927b7915bd21ea1700a175`. Both binaries are rebuilt and
**366/366 CTests pass**. [Manifest and hashes](manifest.json).

## Cause and repair

The ordinary geographic rail estimate can greatly exceed the actual cost of a
short, distant portal jump. Crossing the jump then decreases the estimated total
cost, violating YAPF's assertion and its closed-node search assumptions.

The estimate now includes local directed portal jumps and their virtual lengths,
including chains and changes of heading. A small relaxed graph supplies a lower
bound; ordinary track and signal restrictions still belong to native YAPF. Portal
iteration is sorted, calculations are deterministic, and maps without local
links retain the original estimate. Actual route costs, search limits, save format
and the assertion are unchanged. Preparation is quadratic in local directed jumps;
each estimate is linear in that count. Large portal networks are not benchmarked.

## Reproduction and verification

- A native depot-to-station layout with junctions around a short off-axis portal
  [aborts at the reported assertion before the fix](logs/native-before.log).
  Afterward, route discovery and physical train journeys pass in both directions.
- The three focused cases [pass 10,258 assertions](logs/regressions-after.log):
  normal distances, chained and directed shortcuts, registration-order invariance,
  every head orientation, ordinary rail steps and native train movement.
- The original `crash20260915215730.sav` is preserved byte for byte. A native route
  request on its loaded train [aborts before](logs/crash-save-before.log) and
  [succeeds after](logs/crash-save-after.log).
- `TryPathReserve` had already set a tentative depot reservation when the game
  crashed. The crash save retains that bit, which explains why merely loading it
  does not trigger a fresh search. The replay verifies that the sole train is
  entirely inside its depot, then clears only that reservation in the loaded copy.
  The train moves during **4,096 native simulation ticks** without crashing.
- A recovered save, made before advancing those ticks, is available locally at
  `build/Testing/yapf-crash-replay/crash20260915215730-recovered.sav`. It is an
  ignored build artifact, not a replacement for the original save. The rebuilt
  game loads it and exits successfully after 20,000 null-video loops. Those loops
  are not a simulation-tick count or visual acceptance.
- [Full CTest output](logs/ctest.log) and [clean rebuild](logs/build.log). Tests ran
  on the source committed above; the final rebuild changed version metadata only.
- WP09 GUI establishment and fresh-process reload also pass with a copied binary
  in a temporary directory with **no adjacent baseset folder**, proving discovery
  of normal installed graphics: [output](logs/standard-baseset.log).

To replay the supplied crash save, use a copy and run the hidden case
`Saved portal train resumes native pathfinding`, setting `OSTTD_TEST_BINARY` to
the absolute test-binary path and `OSTTD_YAPF_CRASH_SAVE_PATH` to the copied save.
`OSTTD_YAPF_RECOVERED_SAVE_PATH` optionally writes a separate recovered save.
The fixture intentionally requires one train entirely in a reserved depot.

## Remaining checks

Source/documentation whitespace checks pass, excluding retained raw audit files.
The unused-string check reports six findings; an isolated snapshot of base main
`345e258867079cb6ee8a0c21245761d38d9c43ee` produces identical output:
[branch](logs/unused-strings.log), [main](logs/main-unused-strings.log).

The first PR checks also rejected formatting in retained audit logs/patches and
indentation in the original fault-injection helper commit. Raw logs and patches
remain intact; reconciling that evidence with the per-commit style checker is
still a PR blocker. Three missing file descriptions were added, and the current
file-description check passes. The documentation commit prefix was corrected.
These comment/format changes do not change the verified game behavior.

Visual UAT-04, 08, 10–12 and 09 save/reload remain pending. This verification does
not close the broader recovery acceptance goal or start WP10 and later packages.
