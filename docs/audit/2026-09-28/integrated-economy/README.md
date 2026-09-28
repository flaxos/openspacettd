# Unified Commonwealth economy — automated acceptance

Date: 28 September 2026. Branch: `codex/unified-commonwealth-economy`.
Base: `959fe33f18` (PR #43). Source, exact save/content hashes and the tested
binary hash are recorded in [manifest.json](manifest.json). Human graphical
acceptance is **Pending**; see [UAT](../../../../demo/INTEGRATED-ECONOMY-UAT.md).

## Checks

| Check | Result / evidence |
|---|---|
| Build | Configured Ninja build succeeds |
| Full unit suite | 473 cases / 67,508 assertions; [log](unit-suite.log) |
| CTests | 485/485; [log](ctest.log) |
| Integrated regressions | 16 cases / 476 assertions; [log](integrated-unit.log) |
| Authority directory | 5 Python tests, including home conflicts, stale/duplicate updates and mixed rulesets |
| Linters | file-descriptions, unused-strings and `git diff --check` pass |
| Content reproducibility | Pinned NML compiler; `build_commonwealth_grf.py --verify` passes |
| Native company removal | 13 factories become neutral; research/stock cleanup and cold reload pass; [evidence](company-removal.json) |
| Old random-industry preference | Integrated rules still require surveying; [native generation/reload](generation-toggle.json) |
| Fresh sector | Seeds 11, 101 and 2026; [11](generation.json), [101](generation-101.json), [2026](generation-2026.json) |
| Full production progression | [Compact state and audits](progression.json) |
| Three-host research/freight and cold restart | Six round trips, three joined clients; [evidence](federation.json) |
| Legacy published save | [Current connected UAT](legacy-current.json) |
| Legacy original-content fixture | [Original terrain regression](legacy-original.json) |

The 16 integrated cases cover every recipe, starvation and saturation, persistent
yield rounding, research escrow/cash gates, stable roles after promotion, city
basket states and capacity reductions, malformed state, ownership/capacity changes,
confirmed research replay/conflict/offline load, legacy research, and native
construction/conversion/blueprint/clone/autoreplace material atomicity. The full
suite also retains the existing command, blueprint, portal and federation tests.

## Native connected progression

The existing authoring harness creates an empty-stock, empty-city-reserve network
with 40 conventional freight services and three bootstrap processors. It funds
later physical industries through the native construction command, spends local
research cash, and transports every factory input and warehouse deposit by train.
No recurring cargo or money injection is used. All 13 recipes produce; all 12
technologies complete; a project receives its steel/machine equipment and becomes
an active gate. Cash-mode electric construction requires and consumes stock while
its preview preserves inventory.

Progression completes in October 1956. A 24-month operating soak earns 4,972,114
net cash units. All 116 progression/soak samples reconcile physical cargo and have
no crashed trains. Food service then stops long enough to exhaust reserves and
halt growth, resumes and recovers. A separate process loads the final save with
identical economy/stock/escrow/technology/money and advances with cargo conserved.
Terrain, full-map pixel/viewport queries, all active cargo text fields and all
20 required freight refits are audited in English-US and English.

This is an **authored acceptance network**, with substantial starting capital and
infrastructure. It proves the integrated progression and profitable operation;
it does not prove a minimum-loan organic startup. Its commissioned additional gate
leads to an already accessible fixture world. The seven-world generation checks
separately verify closed frontier access, rare-mineral sites, accessible bootstrap
resources, stable roles, empty city reserves, legal terrain and cold reload.

## Native three-host federation

Three dedicated hosts with three joined SDL-dummy clients complete five round
trips, delivering 400 units to a physical processor. Host 2 alone conducts company
research; all hosts confirm Materials I and retain it through a 35-second
authority outage. After a coordinated checkpoint and full process shutdown, all
three hosts and clients reload and complete one further trip: 480 delivered units
in total, the same global train identity, and 24 completed transfer legs with no
in-transit or quarantined transfers. Both phases reconcile cargo and local cash.
The transfer ledger counts each cargo-bearing leg separately (960 units across
legs, not 960 unique delivered units).

[Full state and reconciliation](federation.json),
[driver log](federation.log), [first checkpoint](federation-checkpoint.json),
[reloaded checkpoint](federation-reloaded-checkpoint.json).
Authority unit tests additionally cover duplicate/stale updates and conflicting
homes/rulesets. This is coordinated recovery, not independent host rollback.

## Legacy compatibility and content

Both representative existing saves retain original GRFs and legacy economics.
Each passes two-language cargo/terrain audits, exact native save/reload and four
live simulation intervals. Old content files are byte-for-byte preserved; hashes
are in the manifest. The integrated pack is a separate filename with OST01 version
5/minimum compatible 5, not a migration of industry v4.

## Reproduction

Build content before the engine. Freeze the binary **with its matching language,
base-set and NewGRF files** before long runs. Do not rebuild language files under
a running acceptance binary. Commands from the repository root:

```sh
NMLC=/path/to/pinned/nmlc python3 scripts/build_commonwealth_grf.py --verify
ninja -C build
./build/openttd_test
ctest --test-dir build --output-on-failure
python3 .github/file-descriptions.py
python3 .github/unused-strings.py
git diff --check
python3 scripts/test_stellar_directory.py
python3 scripts/test_integrated_lifecycle.py --output /tmp/company-removal
python3 scripts/test_stellar_generation.py --integrated --output /tmp/new-sector
python3 scripts/test_integrated_economy.py --output /tmp/new-economy
python3 scripts/test_integrated_federation.py --output /tmp/new-federation --deliveries 5
python3 scripts/test_connected_uat_recovery.py --output /tmp/legacy-current
python3 scripts/test_connected_uat_recovery.py --save demo/regression/connected-legacy-terrain.sav --output /tmp/legacy-original
```

Supply `--binary` when using an isolated runtime. The authoring harness flushes
native signal edits before advancing simulation. Checkpoints and test output are
disposable; the two newly published saves do not overwrite any prior UAT save.

## Acceptance boundaries

Human UI-scale/language readability and balance remain Pending. Three-host
acceptance does not establish arbitrary cluster scale or independent rollback
recovery. Core flow counters and reconciled scenarios do not claim exhaustive
coverage of every crash, demolition, spaceport or edge-conduit path. See the
[current limitations](../../../KNOWN_LIMITATIONS.md).
