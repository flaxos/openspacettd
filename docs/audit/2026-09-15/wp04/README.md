# WP-04 — hub station authority and lifecycle

**Implemented locally, 15 September 2026.** Both binaries rebuilt; **68 distinct
selected CTests pass**, including seven new hub authority cases. There are330
registered CTests. This is automated evidence; human UAT-10 lifecycle acceptance
remains open. The earlier user report that Blueprint no longer crashes remains
valid as separately recorded in `demo/UAT-RESULTS.md`.

## Change and rules

- BuildLogisticsHub query and execution share station resolution before payment.
  Invalid/out-of-map/company, missing/foreign/nonrail/waypoint/empty/distant and
  wrong-world attachments reject. One hub per station and per anchor tile.
- Four-tile Manhattan range is measured to an actual rail platform owned by the
  hub company in its world. The station's reference tile must also be in that
  world. Automatic selection chooses nearest platform, then lowest station ID,
  skipping already attached stations. Existing cost remains75,000.
- RegisterHub and runtime station/production/cargo lookups validate the binding.
  Deposit no longer sends cargo from arbitrary tiles to a region's stockpile.
- Native last-platform demolition and station destruction retire the hub;
  partial removal keeps it only when remaining rail is still in range. The
  anchor does not move and the attachment never follows a reused station ID.
- Native acquisition transfers hub ownership after station/tile ownership;
  bankruptcy removes the binding. Reserve floors and lifetime counters persist.
- Full engine after-load validation discards invalid/zero-ID records and duplicate
  station/anchor records, retaining the lowest valid hub ID. It neither rebinds
  records nor changes planetary stock quantities. Normal inventory remains in
  its existing world/company ledger. Saved maximum IDs cannot wrap allocation to
  zero or overwrite existing hubs. LHUB format is unchanged.

## Reproduction and verification

| Run | Result | Evidence |
| --- | --- | --- |
| Baseline new negative command test | Failed39 assertions: invalid builds could create records and charge75,000 | [Baseline](logs/baseline-test.log) |
| First six authority cases |6/6 passed | [First focused run](logs/authority-tests-first.log) |
| Final source build plus edge cases | Game and test targets built with existing Ninja directory, -j2 | [Build](logs/build-final.log) |
| Related Blueprint/CST, cargo/hub, production, HQ/stockpile, fabrication, tech and spaceport selection |67/68 passed; seven new authority cases all passed | [Related run](logs/related-tests.log) |
| Corrected old rights fixture and rerun |1/1 passed; all68 selected cases now have passing final results | [Fixture rebuild](logs/build-fixture-final.log), [Rerun](logs/rights-fixture-final.log) |
| Rebuilt game executable help | Exit0 | [Help](logs/executable-help.log) |

The sole broad-run failure was an outdated WP-03 fixture: it changed a station
owner to OWNER_NONE but expected the old company to withdraw hub inventory under
town exclusivity. WP-04 rejects that stale ownership. The fixture now verifies
stock100/dispatch0 and successful loading of20 ordinary waiting cargo for the
rights holder, plus a control where town exclusivity does not block an owned hub.
No production checks were weakened to fix this result. Only that affected case
was rerun after the test-only change; passing unrelated tests were not repeated.

New authority coverage includes16 negative scenarios with query/execute record,
cash and inventory checks; explicit/automatic four-tile boundary parity and
single charging; nearest/tie/foreign/nonrail/bound selection; full/partial platform
removal and actual station destructor with ID reuse; native company acquisition,
new-owner deposit/withdrawal, bankruptcy and save/reload; nine legacy invalid or
duplicate record scenarios, each persisted twice; and a valid maximum-ID save
followed by successful new construction. All save fixtures are64×64 temporary
worlds using actual SaveOrLoad and native station tiles. Existing synthetic
Sprint39/42 hub fixtures now supply real stations. Original crash/player saves
were not modified.

## Review artifacts

- [Checks and SHA256 hashes](checks.json) contain exact commands, exit codes,
  final game/test/source hashes and scope boundaries.
- [Source patch](source.patch) contains only the WP-04 delta against the captured
  dirty baseline, preserving the earlier WP-01/02/03 work.
- [Baseline location and owned files](baseline.json).
- Branch: `fix/portal-gate-lifecycle-crashes`; base HEAD:
  `abbcd7e07737bcd83c3f830f539313a45bef5f01`, with local changes.
- Game SHA256: `d51c0adfe55d0908c94bc5cd471e03cf36158c3a8818b1b9dfb2821b5e120b61`.
- Test SHA256: `d760d5063d26cd616d08de732b0f656cfbe5f747cbe8c8022538288c72161af0`.

## Boundaries and next acceptance

UAT-10 now includes disposable-copy partial/final platform demolition and reload.
Fresh hub construction/reserve controls are still WP-09; do not invent GUI steps.
No new native GUI or two-client multiplayer pass is claimed. Existing Release
configuration with OPTION_USE_ASSERTS=ON was reused; no separate assertion-active
CI run or full suite rerun was performed.

Company-wide inventory/HQ/research migration during acquisition remains a
pre-existing separate lifecycle gap. Transferring a hub does not merge its former
company's stockpile into the buyer's ledger. That repair needs an explicit cargo
merge/overflow policy; demolition and invalid-record repair here preserve ledger
quantities. WP-05 command-only GUI state is the next planned implementation
package; this run stops at WP-04.
