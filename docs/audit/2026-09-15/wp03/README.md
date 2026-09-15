# WP-03 — lossless hub loading

User-authorized goal,15 September2026. Base
`abbcd7e07737bcd83c3f830f539313a45bef5f01`, branch
`fix/portal-gate-lifecycle-crashes`, preserving WP-01/02 and existing audit work.

## Result

Before: a full-pool pickup removed60 from stock100 and recorded dispatch60, while
waiting/onboard remained0. The original failing regression is retained.
After: refusal preserves stock100, dispatch0 and empty waiting/onboard. A later
pickup loads60 and leaves reserve40.

`economy.cpp` now checks exclusive loading rights and packet capacity, then
allocates an owned packet **before** withdrawing stock. Zero surplus releases the
unused packet automatically; partial surplus reduces its count. Once published,
the native station cargo list owns the packet. A later split failure leaves the
units waiting rather than destroying them. No deposit-policy, reserve-manager,
binding-policy, UI or save-format change; no guessed historical inventory repair.

## Requirements and evidence

| Requirement | Authoritative check |
|---|---|
| Full pool cannot destroy cargo or inflate dispatch | Native `LoadUnloadStation` with zero free slots, with/without20 existing waiting units; refusal preserves stock100/dispatch0. Existing packets may still load. |
| Allocation recovery | Start another pickup attempt after capacity returns; stock+waiting+onboard stays100 or120 including existing cargo. |
| Zero/full capacity and NoLoad | Boundary case verifies no stock withdrawal or orphan packet. |
| Reserve floor | Empty, below40, exactly40,41 and100 stocks; no withdrawal below floor,1 or60 units above it. |
| Waiting cargo and exclusive rights | Existing waiting cargo fills some/all capacity; only the gap is withdrawn. Denied native exclusive rights leave stock untouched; holder loads normally. |
| Failed downstream split | One free slot permits the60-unit withdrawal packet; gradual loading needs a second slot and fails.40 stock+60 waiting survives. |
| Packet ownership / leaks | Zero withdrawal frees the allocation; after explicit cargo cleanup all four cases assert zero packets remain. |
| Actual save/reload | Both refused withdrawal and the40-stock/60-waiting split-failure state save/load through STCK/LHUB and cargo-packet data, then retry with unchanged reserve/dispatch semantics. |
| Regression scope | Four new loading CTests plus57 distinct related tests pass:61 total. Related selection includes six WP-02 cases, three production-gameplay cases and48 Blueprint/CST/corporate/fabrication/research/production/spaceport cases. |

The boundary CTest contains12 scenarios; the full-pool CTest covers0/20 initial
waiting cargo. Every case uses real cargo lists and station loading ticks. Fault
injection adds a scoped occupancy bias to the native pool's public `items` counter,
so its normal allocation check rejects without allocating16 million objects.
The bias preserves real allocation/deletion deltas and is removed before save/load.
This tests pool-capacity exhaustion, not process-wide out-of-memory failure.

## Build, failures and reproducibility

Both game and test binary were rebuilt in the existing Release/GCC13 build with
`ninja -C build -j 2 openttd openttd_test`. No second build directory or complete
Catch2 rerun. The final test-only fixture changes were rebuilt afterward.
`checks.json` records exact commands, exits and logs; `sha256.txt` identifies the
final binaries, sources and patches. `source.patch` is the WP-03-only delta against
the WP-02 working state, not a replacement for previous patches.

The original failing assertion traced actual cargo loss. Later fixture issues were
separate: a const engine accessor compile error; pool-wide vehicle cleanup retains
cargo for later global teardown; a raw train lacked matching engine capacity and
native consist length initialization. A temporary diagnostic signal handler
captured `VehicleLengthChanged → ConsistChanged → MarkDirty → LoadUnloadStation`
when gdb was unavailable. The final fixture initializes engine/consist state,
resets vehicle hashes, and explicitly releases cargo before checking for leaks.
The final retry explicitly begins another pickup attempt after native
LoadIfPossible finishes its earlier slice. No production assertion was disabled.

The existing test translation-unit assert-macro caveat remains tracked as
OST-TEST-001; Catch2 assertions and exercised engine assertions run. No new CI run,
graphical acceptance or multiplayer lockstep pass is claimed.

## Human handoff

On a copy with a verified owned hub, record stock, existing reserve floor, waiting
cargo and free train capacity. Use ordinary loading with No unloading. Only surplus
may leave stock; stock+waiting+onboard must remain constant. Repeat at the floor
and save/reload a new copy. No manual pool-exhaustion setup is needed. UAT-10/14
remain the human record; missing reserve-editing UI and hub binding are WP-09/04.

No commit, push, merge, branch switch, source reset or original-save overwrite.
Temporary save fixtures use newly created unique directories. Stop before WP-04.
