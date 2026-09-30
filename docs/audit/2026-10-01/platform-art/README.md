# Optional platform art proof

**Automated status: TESTED within the scope below. Human visual acceptance: Pending.**

Source base: `294e1a947e267778c2404fb6b2e8c3431dc632f9`, independent
`codex/art-commonwealth-platform-proof`. Engine binary SHA-256:
`93d9e45a0976b6c8f720e0e2f8313e70f742d10fc10c90c7c7f27ae8da43096f`.
The engine rebuild after content packaging changed only generated revision metadata;
no simulation/rendering source was changed. Full pack SHA-256:
`0ea503e0667bff7f4dc4733d5053ade456a4f2598f935d7b7efb7ea9bb767202`.
Native OpenGFX, software SDL/Xvfb, 1024×768 captures and 100% UI scale are the
bounded graphical environment. A separate private preview bundle launcher also
loaded its copied scene under SDL/Xvfb.

## Decisive checks

- Pinned exporter rebuilds byte-identical 8bpp, RGBA/mask, NML and GRF outputs.
  Compiled actions are exactly 14/08/0A; the replacement block is 1069–1072.
  Primary company index 201 is confined to inset masks; ordinary materials exclude
  secondary company 0x50–0x57, primary company and animated indices.
- All seven original NewGRFs are byte-identical to the source base. Static OST06
  admission succeeds before native map generation. Existing content IDs/hashes
  are preserved.
- Paired native WP11 runs complete 39 advances of 1024 ticks, depositing 120 steel
  units through six distinct deposit observations. Exposed state/catalog/date,
  including cash, cargo and trains, matches at every sampled step.
- Cold reload and bounded continuation match with the art present and removed.
  Static art is absent from required saved GRFs. Save bytes are **not identical**;
  this establishes the exposed native fixture contract, not full-state/bitwise
  equivalence or a general multiplayer/federation guarantee.
- Clean normal/2x 32bpp captures cover Axis X, native shelters/buildings and a
  separately command-built Axis Y station in disposable graphical clones.
  That native UI construction costs £990 in both profiles. Station transparency
  and an 8bpp-optimized fallback run cover both axes. Matching graphical snapshots
  preserve trains/cargo/date/money across the two 32bpp profiles and fallback.
- Native `fps` averages and Linux RSS are recorded only as short paused-scene
  diagnostics. They are not p95 frame-time, large-network, desktop GPU or human
  acceptance results.
- 475 unit cases / 67,647 assertions and 488/488 CTests pass. Both repo linters,
  Python compilation/help, exporter verification and diff checks pass.

[Evidence summary](evidence.json) records report/capture hashes and observations.
Full native logs, reports, paired images, original production sources and the
clearly labelled non-production concept are retained in the private preview
bundle. Build logs remain in `build/agent-logs/platform-*`.

## Pending acceptance and scope

Review all company colours, busy routes, station transparency and moving-train
occlusion on the owner's desktop. Inspect seams and edge contrast at normal zoom,
then save/quit/reload with exact content. No human result has been supplied.
Use [the isolated mission](../../../COMMONWEALTH_PLATFORM_ART_PROOF.md); its
authored capital/routes do not establish A1/A2 or ordinary-start progression.
Broader art, vehicle admission, world-specific terrain, UI changes and deployment
to default gameplay require their own scope and acceptance.
