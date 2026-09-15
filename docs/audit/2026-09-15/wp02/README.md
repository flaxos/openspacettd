# WP-02 — exclusive hub unloading

User-authorized follow-up, 15 September 2026. Base commit
`abbcd7e07737bcd83c3f830f539313a45bef5f01` on
`fix/portal-gate-lifecycle-crashes`, preserving the existing WP-01 and audit diff.

## Result and decision

The original 60-unit iron delivery produced **industry60 + stockpile60**, sale
income218 and development5 in the controlled fixture. The repaired route produces
**stockpile60 + industry0**, with no sale income or development. Partial17/43-unit
unloads preserve the physical sum at each step. Baseline failures are retained.

The corporate spike §6.2 says hub storage replaces sale to industry/town.
Implementation therefore preserves station-recipe priority, then stores remaining
freight at the delivering company's registered hub. Stored units do not also count
as consumer deliveries, subsidies, world development, Megacity or spaceport supply.
Freight uses the cargo specification's `is_freight` flag. Explicit Transfer and
NoUnload remain native orders; there is no new player control.

The same acceptance rule runs during `PrepareUnload` and the unloading tick,
including isolated hubs without native acceptance. Invalid tile/world sentinels
cannot qualify as storage destinations. Existing balances are unchanged; future
unloading follows the new rule. Save/reload preserves an existing11 plus delivered60.
Feeder-share netting retains native accounting: storage has zero company revenue;
a final vehicle may offset earlier virtual transfer profit.

## Validation

**57 distinct selected CTests passed during this package:**

- Six new hub cases: partial/full industry conservation; actual isolated-hub
  arrival and station tick; nine town/cargo/ownership combinations; explicit
  Transfer/NoUnload; actual save/reload; invalid tile/world destination controls.
- Three existing production-gameplay cases, including facility-before-hub input
  consumption, onward loading, lifecycle and actual save/reload.
- 48 broader Blueprint/CST, corporate stockpile/hub, fabrication, research,
  production-chain and spaceport cases.

The existing build was reused, Release/GCC13, `OPTION_USE_ASSERTS=ON`, incremental
`ninja -C build -j 2 openttd openttd_test`. Both game and tests were rebuilt.
`checks.json` records commands, failures and successful retries. No separate full
Catch2 run or second build directory. The final hub run covers the final runtime
guard; a subsequent test-only initializer cleanup removes compiler warnings.

Fixture corrections: mandatory void edges now follow `IsInnerTile`; train-tick
tests initialize the default engine mappings/catalogue. Initial compile and
uninitialized-mapping failures are recorded, not counted as product regressions.
The existing local test translation-unit assert-macro caveat remains; Catch2
checks and the exercised engine assertions run. Assertion-active CI confirmation
for OST-TEST-001 remains pending.

## Boundaries and human check

On a copy with a verified owned hub, unload known freight with **No loading** and
no matching station-processing recipe. Expect the quantity once in stockpile,
zero nearby consumer input and no sale/development credit; save/reload and verify
the balance. Running costs can still change cash. Follow UAT-10/14.

No graphical run or multiplayer lockstep run was performed. WP-03 allocation-safe
stockpile loading, WP-04 hub binding/lifecycle, and WP-09 establishment/reserve UI
remain open. No historical inventory was rewritten. No commit, push, merge, branch
switch, original-save overwrite or deletion was performed. Temporary save/reload
test files used a uniquely created directory.

`source.patch` contains only the WP-02 runtime/test changes; WP-01 evidence remains
in its separate directory. `sha256.txt` identifies final sources, binaries and
patches; `checks.json` and `logs/` retain execution evidence.
