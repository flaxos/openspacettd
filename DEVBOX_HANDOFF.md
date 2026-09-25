# OpenSpaceTTD recovery checkpoint — 2026-09-15

> [!WARNING]
> **HISTORICAL DOCUMENT — DO NOT USE FOR CURRENT REPO STATUS**
> This file records a point-in-time audit checkpoint from 15–16 September 2026. All work packages described herein as uncommitted (WP-01 through WP-11) have subsequently been committed, verified, and merged into `main` (PRs #4–#25).
> For authoritative project status, see [docs/PROJECT_STATUS_AND_ROADMAP.md](docs/PROJECT_STATUS_AND_ROADMAP.md). For source-verified architecture, see [docs/CURRENT_ARCHITECTURE.md](docs/CURRENT_ARCHITECTURE.md). For the full sprint ledger, see [docs/SPRINT_LEDGER.md](docs/SPRINT_LEDGER.md).

## Current checkpoint — 23 September, multiplayer federation repair

Human UAT and graphics are accepted by the user. The later federation check
reproduced a local portal hop and a client desync. A controlled loaded round trip
now passes between two independent dedicated servers with one client each, in
both graphical and unattended runs. Gate configuration and physical transfers
use replicated commands; only the server contacts the authority. Full train
identity, wagon spacing and all ten cargo units are preserved.

The two graphical clients are paused in `/tmp/federation-native-final`; ports are
54701 and 37965. Use `scripts/federation_session.py --session
/tmp/federation-native-final resume` from the repo root to replay the outward trip.
The helper's `status`, `pause`, `reverse` and `stop` actions operate only on that
session. [Evidence and limitations](docs/audit/2026-09-23/federation-multiplayer/README.md).

All 430 isolated CTests and repository linters pass. The combined unit binary
still fails in four legacy cases and aborts; this also reproduces with the new
federation tests excluded. Do not report all tests green. Scheduled station routes,
restarts/reconnects and human federation acceptance remain open. Sprints 51–52 and
future narrative scope remain withdrawn.

## Earlier checkpoint — 16 September, WP-11 structural slice

The remaining WP-11 structural economic slice is implemented locally on
`fix/wp11-content-economic-slice` (base `3081af218d`). Both binaries are rebuilt;
**377/377 CTests pass**, the unused-string check is clean, and pinned NML reproduces
both packs and their manifest. Full active-content runs at years 1800 and 2300
validate loaded catalog/refits, research/phase purchase checks, mined ore → steel,
physical cargo and cash ledgers, fresh-process reload, blocked-route recovery,
three complete steel deliveries, and a connected depot consuming 10 steel +
5 ballast exactly once. Linux CI now runs this proof; GitHub has not run it yet.

Start the human check with a copy of `demo/wp11-v1-operational.sav`.
[Short UAT steps](demo/WP11-UAT.md) · [evidence and hashes](docs/audit/2026-09-16/wp11/README.md).
Graphical UAT-13/14 and save/reload remain pending. Other production pipelines,
federation and art remain outside this slice. No existing save was retrofitted.
This implementation is uncommitted; no new push or PR was made in this turn.

## Earlier checkpoint — 16 September, portal routing and PR

All recovery work is committed on `fix/recovery-wp01-wp09`, based on current main
`345e258867`. The subsequent user-reported YAPF assertion is fixed in `174e571780`:
rail distance estimates account for local portal shortcuts and retain the engine
assertion. Both binaries are rebuilt and **366/366 CTests pass**. The crash-save
route request succeeds and a copied train resumes for 4,096 native ticks after
clearing the tentative depot reservation left by the abort. Original saves are
unchanged; the recovered copy is in `build/Testing/yapf-crash-replay/`.
[Current verification](docs/audit/2026-09-16/portal-yapf/README.md).

The unused-string check has six findings identical to base main. The first remote
commit-style check also rejects retained raw audit formatting and indentation in
the original fault-injection helper commit. File-description failures were fixed.
CI cleanup and visual acceptance remain pending; keep PR #6 draft and the earlier
goal open for acceptance.

## Earlier checkpoint — 16 September, WP-07–09

User goal: resolve WP-07/08/09. Local changes are implemented and verified;
**363/363 CTests pass** and both binaries are rebuilt. Capture drag/naming/cancel,
eight revised CST layouts with 312 signalled native train runs, native populated
colonies, and fresh HQ/hub/reserve controls are covered. A fresh-company cargo
journey from depot to hub and process reload preserve 60 total units (20 onboard,
40 reserved). Cargo is seeded only before departure, with conservation checked
every tick. Tool-switch regressions exposed and repaired cancellation of newly
selected Blueprint Capture/Place and HQ/Hub tools, including repeated clicks.

Built-ins use layout revision 2 while retaining old imports. The default legacy
outpost decision is preservation: no automatic town regeneration; retries reject
without mutation. Two generated version-1 empty-town fixtures and hashes are kept.
The user was offered preservation versus explicit repair; no reply had arrived
when preservation was recorded as the default.

[Current evidence](docs/audit/2026-09-16/wp07-09/README.md) and
[updated player steps](demo/ALL-FEATURES-UAT.md). Visual UAT-04, 08, 10–12 and
save/reload remain pending; `orca-ide: command not found`, native control unavailable.
Do not repeat environment discovery or broad passing tests without a new change.
Keep the goal open for acceptance and preserve all existing user work/saves.
WP-10 and later packages remain outside the current goal.

## Earlier package checkpoints

The user now reports **OpenTTD no longer crashes and UAT seems to pass**, following
the requested00/04a–c/09 checks. Record this positive user gameplay report without
claiming separately observed results for every subcase; see `demo/UAT-RESULTS.md`.

**WP-04 hub station authority is implemented locally.** Supplied and automatic
attachments validate the same live owned rail station, world and four-tile
Manhattan platform range before payment. One hub per station and anchor.
Demolition removes invalid bindings; native acquisition transfers the hub to the
buyer; bankruptcy removes the attachment. Load keeps valid records and removes
invalid/duplicate records deterministically, preserving world stockpile quantities.

Seven new command/lifecycle/persistence cases supplement WP-02/03 cargo tests.
All68 distinct selected CTests pass;330 registered. Both binaries rebuilt.
Final build/test results and package-only patch: `docs/audit/2026-09-15/wp04/README.md`.
Preserve the user's new save and all existing working-tree changes.

**WP-05 (user goal WO-05) is implemented and verified locally.** HQ upgrades use an
owner-checked exact-next-tier command; Directory opening/refresh is read only and
colonize/promote synchronize only after successful commands. HQ upgrades remain
free, company-zero viewing is corrected, foreign controls are blocked and HQ
OnPaint draws widgets. A persistence regression additionally repaired missing
PLNT outpost_tile storage; legacy missing columns default INVALID_TILE.

Both binaries rebuilt. Seven new local command/GUI-click/persistence CTests plus
88 related CTests pass (95 distinct;338 registered). The real TCP native-command
server/two-client queue replay passes six denials, three successes, and actual
save/reload equality. Old crash/M1 temporary copies load for1000 null-video loops
with unchanged hashes. Evidence, failures and package-only source patch:
`docs/audit/2026-09-15/wp05/README.md`. No user save was changed.

The first local failures were an uninitialized dirty-screen buffer in the fixture
and then the real missing outpost persistence field; neither was hidden by weaker
assertions. Loopback ran with approved scoped sandbox escalation and cleaned up.
Full multiplayer join/map handshake and graphical UAT-08/10 remain separate.
Native new-town initialization is still WP-08; tests use an existing settlement.
**WP-06 (user goal sp-06) is implemented and verified locally, 16 September.**
Strict typed/bounded JSON admission and guarded transforms precede placement.
Actual Import/Export path queries use validated files; rotated selections export.
Backed rows retain their real filenames. Checked temporary writes publish
atomically; rename keeps the backing file, import rejects duplicate player names,
new sanitized-name collisions get a suffix, and all I/O failures are reported.
Default exports stay outside the scanned library; opening reports skipped files.

35 distinct CTests pass (30 Blueprint/CST +5 related Sprint28); six real injected
open/write/fsync/close/rename/link failures preserve old bytes and memory, reopen
and retry. All128 previous prefab placement variants still pass.351 CTests are
registered. The three hidden Catch GUI cases are explicitly registered as isolated
CTest processes using a null video driver, avoiding driver state in other tests.
Both binaries rebuilt. Failures, command logs, hashes and package-only source patch:
`docs/audit/2026-09-15/wp06/README.md`. No real player library/save was changed;
the old storage test now redirects to a private temporary library too.

Graphical UAT-04f/g remains human acceptance; Windows wrappers were reviewed but
not executed here. No capture/route acceptance is claimed. Next package is WP-07
native capture workflow and prefab routes; do not start without authorization. Preserve all current uncommitted work and saves.

 Human UAT-10 hub delivery/pickup and lifecycle acceptance
remain separate; fresh hub construction/reserve UI is still WP-09.
Company-wide stockpile/HQ/research ownership migration on acquisition is outside
WP-04: transferring a hub does not merge the former company's inventory ledger.

The earlier assistant GUI attempt was blocked by missing `orca-ide` and disabled
native control. Its copied saves/launcher/evidence remain at
`docs/audit/2026-09-15/openspacettd-blueprint-gui-uat-s63nc900/README.md`.
Do not repeat that environment investigation or the full audit.

## WP-03 implementation state

- `src/economy.cpp` allocates a real cargo packet before hub withdrawal, checks
  exclusive loading rights first, reduces the packet to actual surplus, and frees
  unused allocation when there is no withdrawable stock. WP-02 is preserved.
- Baseline forced-full-pool station tick lost60 of100 stock with no waiting/onboard
  cargo. Fixed failure leaves100 and dispatch0; retry loads60 and leaves reserve40.
- Four new loading cases cover full pool with/without waiting cargo,12 capacity/
  reserve/order/rights boundaries, allocation recovery, actual save/reload and a
  downstream split failure retaining60 waiting+40 stock. Explicit packet cleanup
  checks find no orphaned allocations.
- Evidence/counts/hashes: `docs/audit/2026-09-15/wp03/README.md`. Both binaries rebuilt
  in the existing directory with -j2. Source patch contains only WP-03 delta.
- Test fixture now resets vehicle hashes/engine data and initializes a native
  consist matching the controlled60-unit capacity; no assertions disabled.
- Human acceptance remains pending. WP-04 binding/lifecycle was subsequently
  repaired above; WP-09 UI remains open. No new save format or migration; old lost/duplicated quantities not guessed.

## WP-02 implementation state

- `src/economy.cpp`: facility recipe inputs retain priority; remaining owned-hub
  freight goes only to stockpile, without native consumption, sale income or
  delivery/progression credit. Both arrival and unloading recheck accept hub-only
  freight. Invalid destination sentinels reject storage routing.
- `src/tests/test_sprint42_production_chains.cpp`: real packet/payment and station
  tick regressions, partial/full unload, native/town/non-freight/foreign controls,
  explicit orders, invalid destination, existing balances and actual save/reload.
  Reused fixture now preserves required void map edges. No assertion bypass.
- Before:60 input became industry60+stockpile60, with income218/development5 in
  the controlled test. After:stockpile60, industry0, revenue/progression0.
- Evidence and exact final test counts/hashes: `docs/audit/2026-09-15/wp02/README.md`.
  Reused existing build with -j2. GUI acceptance and assertion-active CI pending.
- New behavior applies to future unloading; existing balances are unchanged.
  WP-03 subsequently repairs packet-allocation cargo loss as recorded above;
  WP-04 binding/lifecycle was subsequently repaired above.

## WP-01 implementation state

- Authorized by the user's later “Make it so”. Source files changed:
  `src/blueprint/blueprint_cmd.cpp`, `src/rail_cmd.cpp`, `src/rail_cmd.h`;
  tests in `src/tests/test_blueprint.cpp` and `src/tests/test_cst_prefabs.cpp`.
- One deterministic placement preflight; canonical costs, prospective track/signal
  validation, aggregate per-world BOM, funds checked before execution, no ignored
  child failures. Copied signals consume their BOM. No assertion/NoTest bypass.
- Depot regression captured 500 quoted vs600 executed before fix; now600/600.
- **78 distinct selected CTests pass**:17 Blueprint/CST +1 later persistence +60
  broader. All128 prefab rotation/mirror/payment combinations cover full layouts,
  costs, materials and exact-overlap no-op. Save/reload preserves mixed
  infrastructure/money/materials; no-video harness restores the vanilla engine
  catalogue through normal initialization, so graphical engine loading is untested.
- Deliberate support limits: one new default rectangular station block on clear
  land or complete matching existing station footprint; no partial/custom/irregular
  station overbuild, multiple new station blocks or trees under station cells.
  Tracks/depots accept clear land/ordinary trees and compatible exact owned overlap;
  conversion and clearing roads/buildings/objects reject before mutation.
- Tests now initialize real rail prices (formerly zero multipliers), valid map
  borders, towns/station classes/language/cursor and reset material/research state.
- Evidence, commands, failed attempts, source/binary hashes and limits:
  `docs/audit/2026-09-15/wp01/README.md` and `checks.json`. Keep original audit
  evidence separate. Original crash target/terrain contribution remains unisolated.
- Rebuilt game also loads both untouched crash/M1 temporary copies with a real
  null video driver for1000 loops; these smokes perform no placement.
- Build reused with -j2; no second build, push, merge, branch switch or save overwrite.
  Native GUI remains unavailable; no graphical pass claimed. This historical WP-01
  checkpoint precedes the separately authorized WP-02 implementation above.

## Audit baseline to preserve

- Branch fix/portal-gate-lifecycle-crashes, HEAD abbcd7e07737bcd83c3f830f539313a45bef5f01.
- Initial tracked tree clean; untracked scripts/fix_codex_alias.sh preserved/unexecuted.
- The original audit added/updated documentation/evidence only. No commit, push, merge, reset,
  stash, branch switch, release, save overwrite or deletion.
- Local openspacettd/main1d60095c1f is24 commits behind HEAD; cached remote
  main776e236b31 stale. Live main345e258867 includes HEAD and is14 commits ahead;
  nine-file difference is recorded in docs/audit/2026-09-15/live-main-compare.json.
- Audit build was about216MiB with3.7GiB free. WP-01 reused this build;
  current size/free space are recorded in its evidence.
  Reuse build, no second full build, measure disk and running processes first.

## Evidence already obtained — do not repeat without changed inputs

- User confirms reported crash is placement of CST Mainline Double Straight8x2.
  Local crash20260915082450 log/save/PNG preserved. Stack Blueprint OnPlaceObject,
  command.cpp:365 query/execute parity; modified a6b79add crash build, GS9,
  OpenGFX/no NewGRFs, save367. Exact terrain contribution remains unisolated.
- Current binary reports abbcd7e077; hashes/config in audit evidence.
-306 CTests registered (unit suite included);67 selected existing tests pass once.
- Copied crash and v1.1 saves complete null-video1000-loop load smoke; no placement.
- Native GUI unavailable: orca-ide command not found; no graphical UAT pass.
- Live-main CI run34956427967 completed failure. Linux production cases286–288
  abort because test fixture makes required void edges grass. Exact local test
  preprocessing yields no-op assert despite WITH_ASSERT; see OST-TEST-001. Catch2
  checks still execute. Fix fixture/coverage separately; never weaken assertions.
- GitHub repository reports fork:false, licence classifier NOASSERTION; actual
  GPLv2 headers/COPYING and retained upstream history still govern provenance.

## Priority order

1. WP-01 graphical closure for the implemented Blueprint repair. The 1x1 depot
  failure is reproduced and repaired; do not hide regressions with NoTest/assert suppression.
2. WP-02 one destination for hub unloading, preventing industry+stockpile duplication.
3. WP-03 allocate before hub withdrawal, preventing cargo loss on allocation failure.
4. Hub binding/GUI authority, parser/storage/capture/topology, colonisation/player
  controls, honest conduit output, content mapping and conserved player vertical slice.
5. Keep federation repair isolated; native external entry is closed, custody/
  identity/order/recovery not accepted. AI and showcase follow recovery gates.

## Documents and limits

Master plan links defect register, implementation/UI/test/human matrix, existing
UAT checklist/results, corrected AI example audit, provenance register, title
milestones, gates and first prompt. All discovered project-authored Markdown read
in full across bounded read-only reviews; exact inventory/hashes retained. No
Antigravity file link found; no unrelated personal-directory scan. No full build,
full-suite rerun, GUI replay, fresh active-content/moving-chain/network recovery,
model inference/training or deployment. Main plan records smallest missing checks.

Commands/results/configs/logs: docs/audit/2026-09-15/checks.json. Original crash
artifacts stay in user data directory; temporary working copies are in
/tmp/openspacettd-audit-20260915. Preserve failures and untouched fixtures.
