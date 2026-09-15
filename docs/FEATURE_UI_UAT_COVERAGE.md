# Feature, UI and human UAT coverage

Reconciled 2026-09-15 against `a6b79add03` plus the v1.1 UAT refresh.
The [player checklist](../demo/ALL-FEATURES-UAT.md) is authoritative for procedures
and includes every sprint 1–42 and the recorded spikes.

| Feature | Player/operator entry | Cases | Human acceptance / gap |
|---|---|---|---|
| Worlds, phases, navigation, six biomes | Map, hotkeys, directory | 02, 08 | Not run; original art separate |
| Player-owned prebuilt gates and terminals | Land information, rail toolbar | 01 | Ownership repair and regression supplied; graphical run Not run |
| Local portal construction, linking and transit | Rail toolbar, train orders | 01, 03 | Not run |
| CST prefabs, captured/exported blueprints | Blueprint Library | 04 | Not run; rail-only scope |
| Spaceports and Edge Conduits | Station controls, rail toolbar | 05 | Not run; validate fixture existence, not just pins |
| Megacities, town growth, corridor traffic | Town, Map, directory | 06 | Not run; needs actual deliveries and monthly observations |
| Trade ledger, accounts and charters | Map | 07 | Not run; local UI is not remote integration evidence |
| Colonisation, development, technology/industry restrictions | Directory, construction | 08 | Not run; missing prerequisites/controls block dependent steps |
| Save integrity, portal ownership, orders, Story Book | Save/load and company windows | 09 | Headless checks separate from human run |
| HQ, stockpiles, hubs and reserve floors | Corporate HQ tabs and stations | 10 | Not run; require real same-world station attachment |
| In-kind fabrication | Corporate HQ Fabrication tab | 11 | Not run; compare cash/materials, not role labels alone |
| Commonwealth research | Corporate HQ Tech Tree tab | 12 | Not run; observe monthly funding and completion |
| Commonwealth cargo/rolling-stock packs | Active NewGRFs, depot and cargo lists | 13 | Full content run Blocked in migrated save; validate fresh content-enabled scenario |
| Pipelines A–D production | Station recipe dropdown, buffers, production status and removal | 14 | Gameplay path implemented; human chain acceptance Not run; distinct pack cargos depend on UAT-13 |
| Bespoke Commonwealth/alien art | Visual comparisons | 15 | Blocked pending Sprint 38 assets |
| Federation, admission, identities, orders, recovery | Two servers + authority and operator guide | 16 | Separate run required; current script uses manual dispatch |
| Tutorial and documentation milestones | Story Book, player guide, results sheet | 01–16 | Coverage supplied, not automatic human acceptance |

Evidence categories remain distinct: **implemented**, **automated tested**,
**human observed**, **visual reviewed**, **live runtime observed**. Human results
are Not run, Pass, Fail or Blocked, with build, save, steps and evidence recorded
in [UAT-RESULTS.md](../demo/UAT-RESULTS.md). Historical sprint completion claims
must not override a current blocker or the user's reported ownership failure.
