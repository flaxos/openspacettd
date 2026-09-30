# CI maintenance: prefab vehicle lifetime and script mode guards

Base: `a683e0b08b9830490daea415749e2df7912829ff` (main after merged PR53).
This repair preserves the approved Option A plan and changes no gameplay,
world roles, economy, balance, or A1 proof/runner files.

## Prefab generation crash

Inherited Windows x86/x64 failures:

- [Main run 36696429203](https://github.com/flaxos/openspacettd/actions/runs/36696429203),
  `cc66cf17d7`, jobs `109825407147` / `109825407202`.
- [Docs PR53 run 36701649658](https://github.com/flaxos/openspacettd/actions/runs/36701649658),
  `1fcd292c65`, jobs `109842243761` / `109842243964`.

The first section passes 25 assertions. Line 128 names the second Catch section;
it does not identify the failing operation. A Linux diagnostic binary with
`vehicle.cpp` instrumented by AddressSanitizer reproduces a **heap-use-after-free**
at `UpdateVehicleTileHash` (`vehicle.cpp:657`) during the second generation's
`SpawnActiveFleets`, before its first REQUIRE. The allocation belongs to the first
generation, and the free stack leads to the second generation's
`PoolBase::Clean(PoolType::Normal)`.

Vehicle pool cleanup deliberately skips per-vehicle hash removal. The scenario
generator omitted the corresponding global reset, leaving freed train pointers
in the tile and viewport hash tables. New vehicle insertion writes through a
stale hash entry. This is an observed lifetime bug, not a conjecture about map
allocation or pool ordering. Reset both hashes after pool cleanup, using the
existing `ResetVehicleHash()` helper.

- [Baseline full prefab sanitizer report](baseline-prefab-asan.log): fails on
  the second generation with the allocation and deallocation stacks.
- [Baseline isolated roundtrip](baseline-roundtrip-asan.log): 7 assertions pass.
  A first generation alone does not exercise the stale hash.

The existing complete prefab test and roundtrip remain enabled. A new section
generates a fleet, generates no fleet and checks that old tile lookups are empty,
then generates 3/6/4-world fleets and checks bounded, live vehicle lookups. The
fixed focused sanitizer run passed 172 assertions; the script regression passed
34 assertions. Full build/unit/CTest and platform CI results belong to the draft
PR's exact-head verification record.

## Script mode enforcement

The unchanged `.github/script-missing-mode-enforcement.py` reports two missing
guards on main. `CanBuildIndustry` now uses `EnforceDeityOrCompanyModeValid(false)`
so privileged GameScript construction remains available. The company-private
`GetDiscoveredResourceSites` uses `EnforceCompanyModeValid(nullptr)` before
allocating a list. Invalid company/deity queries return null and set
`ERR_PRECONDITION_INVALID_COMPANY`; valid-company queries still return a list,
including empty lists for invalid types, disabled resources and no discoveries.
No checker suppression is added.

Native script API regressions cover deity construction, invalid/spectator modes,
separate companies' knowledge, occupied/free values, and empty-list/error behavior.
The mode checker passes after the guards.

## Reproduction and limits

Use the existing configured Debug build and complete its normal build first.
The helper creates a separate diagnostic binary; it does not replace normal
objects or executables. Run from the repository root:

```sh
python3 docs/audit/2026-09-30/ci-maintenance/prefab_asan.py --generator-ref a683e0b08b9830490daea415749e2df7912829ff
ASAN_OPTIONS=detect_leaks=0 ./build/agent-logs/openttd_test-prefab-asan '[prefab_world]'
ASAN_OPTIONS=detect_leaks=0 ./build/agent-logs/openttd_test-prefab-asan '[prefab_world]' -c 'Save/Load Round-Trip Persistence and TRAD Chunk Restoration'
python3 docs/audit/2026-09-30/ci-maintenance/prefab_asan.py
ASAN_OPTIONS=detect_leaks=0 ./build/agent-logs/openttd_test-prefab-asan '[prefab_world]'
./build/openttd_test
ctest --test-dir build --output-on-failure
python3 .github/script-missing-mode-enforcement.py
python3 .github/file-descriptions.py
python3 .github/unused-strings.py
git diff --check
```

This is targeted Linux ASan instrumentation of vehicle code, with ASan allocation
interception across the linked executable; it is not a claim of a fully sanitized
engine. Leak detection is disabled because process-global test fixtures survive
exit. Windows MSVC executables cannot run locally in this Linux environment;
the draft PR's Windows x86/x64 CI is the required platform verification. No test
has been disabled, and no human gameplay acceptance is claimed.
