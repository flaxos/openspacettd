# WP-01 verification record

This is supporting evidence for the [single recovery plan](../../../RECOVERY_PLAN_2026-09-15.md),
not a separate backlog. The user authorized implementation after the documentation audit.

## Result

Local Blueprint placement repair on `fix/portal-gate-lifecycle-crashes`, base
`abbcd7e07737bcd83c3f830f539313a45bef5f01`. Source remains uncommitted; no push,
merge, branch switch, second build or original-save overwrite. Exact final
[gameplay/test patch](source.patch), [hashes](sha256.txt), [commands/results](checks.json),
[game identity](built-game-help.log) and [disk usage](disk-final.txt) are retained.

- [Before](depot-before-initialized.log): depot quote500, canonical/actual600;
  2 failed assertions. Initial misleading pass with zero railtype multipliers is
  retained as `depot-before.log`, explicitly invalidated in the check registry.
- [17 Blueprint/CST CTests passed](blueprint-ctest-final.log), including depot
  parity/dispatcher charges, tree/foundation/signal-conversion prices, phase and
  foreign-owner rejection, aggregate cash/material shortage and full map/resource
  preservation, and explicit unsupported station layout rejection.
- The CST case passed26,512 assertions across128 combinations (8 layouts ×4
  rotations ×2 mirror states ×2 payment modes), checking every occupied cell,
  signal direction/type, one station identity per block, exact material depletion
  and free repeat stamping. This tests construction, not advertised train routes.
- [One additional persistence CTest passed](persistence-ctest-passed.log),28
  assertions: actual save/reload preserves mixed tracks/signals/depot/station,
  money and stockpile inventory; repeat quote is zero. The no-video Catch2 harness
  skips graphics/NewGRF engine creation during load. It restores the vanilla
  engine catalogue with normal `SetupEngines`/`StartupEngines` afterward; it does
  not establish saved engine-catalogue or graphical loading acceptance.
- [60 broader CTests passed](broader-ctest.log): placement, portals, conduits,
  fabrication, stockpiles, HQ, logistics and production. **78 distinct selected
  test cases passed overall.** [313 tests are registered](ctest-inventory-final.json);
  the full suite was not run. OST-TEST-001 remains: some existing production fixture
  invariants are not checked by assertions in the local test translation unit.
- The rebuilt game loaded the unchanged temporary copies of the original
  [Blueprint crash save](blueprint-crash-load-smoke.log) and [M1 v1.1 save](uat-v1.1-load-smoke.log)
  with a real null video driver for1,000 loops each; both exit0. Autosaves were off,
  configuration/working directory isolated in `/tmp/openspacettd-audit-20260915`.
  These smokes execute no placement and are not graphical acceptance.

## Implementation boundaries

The plan uses canonical station/depot quotes and shared rail slope/cash/signal
rules, then verifies aggregate world materials and funds before execution. It
models prospective tracks for later pieces/signals; individual child failures
are propagated. Copied-orientation signals now consume their material recipe.
Command parity assertions remain intact; no `NoTest` exemption or test skipping.

Tracks/depots support clear land and ordinary trees; existing owned rail of the
same type can be extended or matched. Matching depots are no-ops. Stations support
one new default rectangular block on clear land, or complete matching existing
station cells. Partial/custom/irregular station overbuilding, multiple new blocks,
trees under stations, rail/depot conversion and destructive clearing of roads,
buildings or objects reject. Sparse unused cells remain untouched. Capture,
import/export, route topology and later recovery packages are unchanged.

## Failed attempts and evidence limits

All failed builds/tests are listed in `checks.json`. Besides the intentional
pre-fix cost regression, failures identified fixture/compile issues: missing
includes, obsolete API names, allocation preconditions, absent language/cursor
resources, a slope incorrectly expected to be illegal, and skipped engine setup
in the no-video load fixture. The large failed map dump is preserved losslessly
as `blueprint-ctest-fixture-fixed.log.gz`; comparison diagnostics now avoid dumping
an entire map. Assertions and acceptance cases were preserved.

The original Mainline crash action is user-confirmed, but the exact historical
target tile/child cost contribution is still unisolated. Native GUI automation
is unavailable (`orca-ide` absent; native CUA disabled). No human case is marked
passed. Next: UAT-00/04a–c and09 with the rebuilt game on copied fixtures, including
Mainline placement/transforms, rejected obstruction, exact cash/material deltas
and visible reload. Stop before WP-02.
