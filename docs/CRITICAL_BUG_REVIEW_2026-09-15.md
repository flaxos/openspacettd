# Critical bug review — 2026-09-15

Scope: recent Sprint 37/42 implementation, v1.1 UAT review, current roadmap,
and the LLM world-building/AI UAT spike. Existing uncommitted UAT ownership,
fixture, save and tutorial changes were preserved. This is a focused review,
not a complete engine audit or human acceptance run.

## Fixed: P1 — research gates identify the wrong locomotives

Both `IsEngineBuildable` (purchase availability) and `CmdBuildVehicle` (depot
construction) call `CommonwealthPackManager::IsVehicleBuildableForCompany`.
That function previously treated global engine pool IDs 0, 7, 18, 24 and 87 as
CST families, regardless of which content defined the engine. Consequently,
ordinary engines could be research/phase locked without the Commonwealth pack,
while real CST locomotives could escape those restrictions. The NML and binary
generator define the five CST locomotive local IDs as 0x20–0x24.

Runtime gating now checks the loaded engine's Commonwealth rail GRF identity
and maps its local ID to the catalog family. Vanilla and unrelated NewGRF
vehicles retain their existing availability rules; CST wagons do not inherit
locomotive gates. Catalog-only policy tests use an explicitly named helper.

Regression coverage exercises a vanilla engine at the old Titan pool ID, an
unrelated GRF at the CST local ID, a CST Titan at pool ID 200 before/after
research, allowed/forbidden world phases, a coach and an invalid engine ID.

Validation:

- `ninja -C build openttd openttd_test`: passed.
- `./build/openttd_test '[sprint37]'`: 129 assertions, 8 cases passed.
- `ctest --test-dir build --output-on-failure`: 301/301 passed.
- Full suite log: `/tmp/openspacettd-critical-review-ctest.log`.
- No graphical UAT or active-pack save acceptance was performed.

## Remaining priorities

### Follow-up fixes: urgent UAT and gameplay blockers

**P1 — HTTP timeout leaves a dangling backend callback (fixed).** Federation
operations use stack-local `AuthorityRequest` objects. `ExecuteSync` cancels and
returns after five seconds, but HTTP cancellation is asynchronous. The backend
previously retained the destroyed request's address, so a later cancellation
check/data/completion callback could access freed memory and crash the game.
A separately retained callback relay now holds only a weak target, cancels when
the request is gone, discards late data and releases itself on terminal response.
Request copy/move is prohibited to keep the callback target address stable.
No change to transfer ownership, simulation commands or protocol acceptance is
claimed. Synchronous polling stalls and natural departure/recovery acceptance
remain separate follow-up work.

**P1 — deleting rail platforms leaves ghost processing facilities (fixed).**
The production cleanup hook was in `Station::AfterStationTileSetChange`, which
the rail removal path does not call. Removing the last platform could therefore
leave a live processing attachment and stranded buffers. Cleanup now runs in
the shared rail-removal path for both individual tiles and bulk demolition:
last-platform removal salvages buffered cargo and clears production acceptance;
partial removal preserves the facility and updates its anchor to the remaining
platform. Waypoints do not execute station-production cleanup.

Validation of this follow-up:

- Federation transport/transfer checks: **198 assertions, 13 cases passed**.
- Production gameplay plus transport checks: **181 assertions, 7 cases passed**.
- Full CTest: **306/306 passed**; `/tmp/uat-blocker-fix-ctest.log`.
- UAT v1.1 full-engine null-video load: **1,000 ticks passed**.
- Timeout tests inject delayed callbacks without sockets, including late failure,
  late success, queued data and abandoned asynchronous requests.
- Production regression fixtures now initialise language data and a valid town
  name generator, and load/stage cargo through valid engine states. These fixture
  corrections exposed the real platform-cleanup bug rather than suppressing it.
- Existing locomotive-gating, UAT and production integration changes were
  preserved. Graphical player UAT and live network recovery remain unverified.

### Still open

1. **Player production facilities — implementation gap resolved in the follow-up.**
   [Station production integration](PRODUCTION_GAMEPLAY_INTEGRATION_2026-09-15.md)
   adds player construction, delivery, output, retirement and persistence.
   Human UAT-14 remains Not run; active-pack cargo acceptance is separate.
2. **High-priority acceptance gap — natural federation departure/recovery.**
   The runner explicitly dispatches trains. Natural entry, remote arrival,
   return orders, obstruction, deduplication and restart need separate evidence;
   the existing runner is not proof that all of them work or fail.
3. **Content acceptance blocker — UAT-13.** The migrated save does not activate
   the new packs. Validate a separate new-game content fixture; this fix does
   not activate or retrofit content in existing saves.

The ownership repair already present in the worktree remains subject to human
UAT-01. The LLM spike is a proposal: its manifest APIs, AI modules and telemetry
examples are not implementation or acceptance evidence. Production integration
should precede automation built on those assumptions.

The roadmap's final evidence-gap list was corrected: it still claimed no pack
sources/GRFs and no process-level federation runner existed, contradicting its
own current acceptance section. Remaining limitations are retained explicitly.
