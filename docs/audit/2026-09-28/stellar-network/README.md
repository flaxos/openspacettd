# CST stellar network verification — 28 September 2026

Branch `codex/cst-star-map-expansion`, based on main `8464e9374a`.
[Player/system contract](../../../CST_STELLAR_NETWORK.md) and
[human checklist](../../../../demo/CST-STELLAR-UAT.md).

## Frozen executable and content

- Executable used by the final process tests: `build/openttd-cst-verified`.
- SHA-256: `e71b4883cd0f93e90461bcc2c9eba149a322c310a59b97e24cbf248fc66870e5`.
- Published industry v4: `7f709d575d4ae1507865bf94351a698c5106be3c4f8099a6753c12f35960823d`.
- Published rail v3: `1561bc45c46c356d897dd92d81327445a0edd3107731506dd5db726d8d3c1eea`.
- Additive equipment v1: `afc62b4ea9ba8733be4010dd390d30b98acbb668184b09f00561227c1644cf21`.

The executable is a precommit build; its embedded version names the base revision
and dirty branch. Rebuilding after the delivery commit changes that embedded
version and therefore the binary hash. The process evidence uses the frozen copy
throughout. Do not mix checkpoint binaries.

## Native and generation results

- **457 unit cases / 67,032 assertions**: [combined run](unit.log).
- **469/469 CTests**: [isolated run](ctest.log).
- **10 focused stellar cases / 193 assertions**: [focused run](focused.log).
- Both repository linters and `git diff --check` pass.
- [Classic WP-11 economy regression](classic-economy.json) passes with the frozen
  binary: active catalog, native moving freight, reload, three deliveries and
  fabrication. [Full interval/catalog data](classic-economy.full.json.gz) is retained
  compressed. This keeps the old-map compatibility claim separate from the new
  sector checks.
- Reproducible GRF build verification: [compiler result](content.log).
- Python directory tests cover host credentials, duplicate world/host identities,
  expiry and restart; checkpoint tests cover two/three-world completeness,
  changed saves/binary and complete stdout records. Included in CTest.
- [Fresh preset + cold reload](generation.json): seven immutable worlds,
  21 distinct arrival zones, three public CST links, 18 primary resource sites
  (11 unoccupied), 15 bounded starter industries (eight processors), MACH cargo
  slot 29 and zero invalid adjacent terrain height edges.
- [Second-host world-ID range + cold reload](generation-base100.json) verifies
  generation with `OPENSPACETTD_WORLD_ID_BASE=100` using the same frozen binary.

The native window test opens Universe Directory, checks that Star map is enabled,
clicks through reservation and activation and checks real portal endpoints. Native
save/load tests cover local project policy/tolls, remote reservation/commit replay,
proxy station/gate destinations and explicit company mappings. These are automated
window/command tests, not human visual acceptance.

The generation save is retained as
[OpenSpaceTTD-CST-Sector-UAT.sav](../../../../demo/OpenSpaceTTD-CST-Sector-UAT.sav)
(SHA-256 `582f1650f3bf8a15b457ffb29b31ff6d15d65ff4c9fcbf1071474f042c9be718`).
It is a fresh sector, without an authored transport showcase or free equipment.

## Three-server freight

**Passed with the frozen binary:** five complete loaded outbound / empty return
cycles across three hosts, followed by a full fresh-process cluster restart and
one further cycle. All three joined clients remained healthy; no desync was
detected. The same global train identity and two station orders survived all
24 native gate crossings. [Machine results](multihop.json), [run log](multihop.log),
[initial checkpoint](checkpoint.json) and [reloaded checkpoint](reload-checkpoint.json).

| Accounting interval | Consumed coal | Cash reconciled | In-transit ledger at checkpoint |
|---|---:|---|---:|
| First five round trips | 300 | Yes, all three hosts | 0 |
| Cold-reload continuation | 60 | Yes, all three hosts | 0 |

Cargo conservation includes native production, unallocated output, station losses,
remaining station/vehicle cargo and actual consumption. It does not claim zero
ordinary station losses. A [post-run provenance audit](provenance-audit.json)
checks every retained train snapshot: one global identity and local company 0 on
all three explicitly mapped hosts. Every cargo packet observed on receiving hosts
2 and 3 retains its nonempty origin identity and world 1 origin. Ordinary source
packets are assigned federation provenance at departure.

[Raw logs, configs, authority state and native saves](multihop-artifacts.tar.gz)
are retained alongside [process/binary metadata](session.json) and
[reload metadata](reload-session.json). All addresses and credentials belong to
the disposable loopback fixture. Original evidence paths in JSON refer to the
verification machine; the archive supplies their retained contents. Source
and content fingerprints are in [the build manifest](build-manifest.json).

## Acceptance boundaries

Human graphical acceptance is **Pending**, particularly normal-scale star labels,
remote picker navigation, Visit authentication/camera restoration, train follow,
carrier graphics/refits and the complete equipment-production play loop.

The multi-hop fixture uses native coal and no custom GRFs to isolate routing and
custody; it does not demonstrate the whole Commonwealth equipment economy. Remote
construction interruption/replay is tested at the replicated command/state layer,
not a full two-host GUI fault matrix. Recovery uses only coordinated all-host and
authority checkpoints. Arbitrary crashes, mixed saves, shared/conditional/timetable
orders, cross-host company mergers and large-universe scaling are not accepted.

The initial development run completed five trips but stopped at the old checkpoint
helper's two-world restriction. It is diagnostic evidence only. The helper now
validates every registered world; the final run starts again from fresh maps.
