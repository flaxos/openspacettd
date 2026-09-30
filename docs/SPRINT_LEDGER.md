# OpenSpaceTTD Sprint Ledger & Index

## First Functional Core Supply and Materials I — 1 October 2026

**Draft prerequisite implementation; ASSISTED FUNCTIONAL FAIL at offline startup;
human UAT Pending.** Normal/spectator designation uses native replicated commands;
research queries and actual Tech Tree panel inspection preserve absent/existing
TECH state. Full native gates and focused fixture/transport records are retained
in the [execution audit](audit/2026-10-01/functional-core/README.md).

One campaign at `d7a3a12006` stopped before its startup marker: £0 assistance,
no checkpoint, commands, advances, ending financial observation or mission phase.
All functional phases and replay are NOT RUN. `7bb7587d59` corrects only log framing
and complete-line reading, supported by small console/synthetic regressions; no
full retry followed. The branch starts at exact unmerged PR66 and normally carries
the reviewed docs-only PR67 commit. Old failures/art/archives remain unchanged.
No economic claim, human acceptance, balance change, merge or second slice occurs.

## Functional progression priority revision — 1 October 2026

**PLANNED; documentation only.** Owner-approved functional-first direction is
explicit in the [current roadmap](PROJECT_STATUS_AND_ROADMAP.md#functional-first-direction--1-october-2026)
and the sole [next-slice proposal](ACTIVE_EXECUTION_PLAN.md). Implemented systems,
functional end-to-end proof, ordinary-start viability, human UAT and balance/pacing
have separate acceptance definitions. No gameplay, save, content or architecture
changes are delivered here; no gameplay milestone or human acceptance advances.

Fresh ancestry confirms PR65 owner-merged at `91846666f6`, PR64 art preserved and
PR62 confined to the old A1 branch. Generation's T+C+D+S proof is assessed on its
own contract; revised A1's −£170 cash failure remains failed economic evidence.
PR66 separately owns portability. The recommended FOOD + paid Materials I mission
has one disclosed cash allowance and fixed test bounds; all its gameplay evidence
is NOT RUN. [Review, source/evidence custody and docs gates](audit/2026-10-01/functional-replan/README.md).

## New integrated-game generation — 1 October 2026

> Historical pre-replan delivery status; the current entry above supersedes its
> blanket active block and records the subsequently verified merge.

**TESTED T+C+D+S; BLOCKED revised A1; human UAT Pending.** Approved PR63 scope
implements complete generation-local terminal/join claims, two pre-publication
audits, bounded native Core founding/cleanup and transient proof lifecycle guards.
Six actual fresh starts plus paid native/cold proof and unchanged old-save equality
pass. Final gates: 487 unit cases/306,025 assertions and 500 sequential CTests;
six focused SDL views and repository/helper checks pass. Revised seed 11 iron
service exhausts cash after cold reload; subsequent seed101/2026 operation remain
NOT RUN. [Full audit, failed proof and bounded options](audit/2026-10-01/generation/README.md).
Original A1/#59/PR62 evidence stays intact. No merge, migration, FOOD outcome,
balance change or roadmap advancement is recorded.

## Parallel Commonwealth platform art proof — 1 October 2026

**TESTED bounded content proof; human visual UAT Pending.** Four original native
platform replacements retain anchors/bounds, with normal 8bpp and normal/2x 32bpp
variants. Pinned exporter, compiled-action scope, original-content preservation,
paired freight/cold reload and SDL captures are recorded in
[the proof](COMMONWEALTH_PLATFORM_ART_PROOF.md). This is an independent art
delivery, not another gameplay execution slice or a renumbered historical sprint.

## A1 First Sustainable Freight — 30 September 2026

**TESTED: bounded native seeds 11/101/2026, cargo/cash, output relief and cold
continuation pass; human UAT Pending.** Native ordinary
New Game proof extends the existing harness and shared runner, with actual seed
forwarding, normal commands/loan, physical cargo/payment, toll/cost observers,
processor buffers and cold-reload continuation. No economy/content/generator policy
or A2 change. Discovery failures and the unprofitable one-wagon seed 101 continuation
are retained in [the audit](audit/2026-09-30/first-freight/README.md).
Earlier three-seed labels are superseded by the documented hardcoded-seed correction;
all historical evidence remains preserved.

## Organic UAT crash repair — 30 September 2026

| Delivery | Automated evidence | Human UAT | Status |
|---|---|---|---|
| Complete terminal-corner anchors, bounded height relaxation, foundation audit, narrow atomic legacy recovery; merged tooltip fix incorporated | 473 unit cases / 67,547 assertions; 486 CTests; recovery/refusal regressions, original/crash saves, fresh 3/4/6-world reloads, SDL drawing smoke | Pending | TESTED |

[Evidence, hashes and boundaries](audit/2026-09-30/organic-uat-crash/README.md).
This supersedes the earlier claim that headless organic save verification alone
proved rendering safety; no broader human acceptance is inferred.

## Empty-tooltip crash repair — 29 September 2026

**TESTED:** 473 unit cases / 67,508 assertions; 486/486 CTests.
Human desktop retest remains Pending.

GUI-only correction for `crash20260929060130`: reject zero-width decoded
help and use `STR_NULL` for absent tooltips. Regression exercises real window
creation at three UI scales for hover and right-click. No save migration.
Automated verification and human-UAT status are recorded in the
[repair evidence](audit/2026-09-29/tooltip-crash/README.md).

## Organic UAT World Layouts & Realistic Civil Rail Infrastructure — 29 September 2026

| Feature | Source and dependency order | Automated evidence | Human UAT | Base / branch | Status |
|---|---|---|---|---|---|
| Organic UAT World Layouts & Civil Infrastructure | Procedural biome elevation → multi-block street grids → central terminals with scissors crossovers & PBS signals → safe industry placement → pre-seeded consist fleets | 81/81 assertions in Catch2 `[prefab_world]`; 485/485 CTests; clean generation and verification of `demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav` under dedicated server | Pending; Track 9 in [HUMAN-GRAPHICAL-UAT-GUIDE.md](../demo/HUMAN-GRAPHICAL-UAT-GUIDE.md) | `1a7fe3348b` / `feature/organic-uat-world-layouts` | TESTED |

## Unified Commonwealth economy delivery — 28 September 2026

| Feature | Source and dependency order | Automated evidence | Human UAT | Base / branch | Status |
|---|---|---|---|---|---|
| Integrated Commonwealth ruleset 1 | Rules/content → physical factories/freight → research/construction/cities → federation/UI → native acceptance | 473 unit cases / 67,508 assertions; 485 CTests; 13-chain progression, 24-month soak, food recovery, cold reload; legacy saves; three hosts, shared research/outage and 5+1 trips across cold restart | Pending; [checklist](../demo/INTEGRATED-ECONOMY-UAT.md) | `959fe33f18` / `codex/unified-commonwealth-economy` | TESTED |

[Implementation contract](UNIFIED_COMMONWEALTH_ECONOMY.md) ·
[Evidence and limits](audit/2026-09-28/integrated-economy/README.md).
This is an additional feature delivery after CST star-map PR #43, not a renumbering
or reopening of the historical sprint register. Automated status does not imply
human acceptance. Bootstrap balance and visual review remain recorded limitations.

**Canonical Sprint Index · Sprints 1 through 52**
**Audited at commit:** `bc18c2c8fc` (main branch HEAD, PR #44); Sprints 1–50 and deliveries through Unified Commonwealth Economy merged
**Date:** 2026-09-29

> **Status Values Governing This Ledger (Strict Subset):**
> `PLANNED` · `IMPLEMENTED` · `TESTED` · `UAT-ACCEPTED` · `SUPERSEDED` · `BLOCKED`
> *Note:* "Implemented" means source exists. "Tested" means dedicated automated unit/regression tests pass. "UAT-Accepted" requires recorded human visual acceptance. Passing automated tests do not substitute for human acceptance.

---

## 1. Architectural Spikes & Foundational Prototypes

| Milestone | Objective | Key Files / Subsystems | Source Today | Automated Tests | Human / UAT Evidence | Regressions & Known Issues | Commit / Range | Link | Status |
|---|---|---|---|---|---|---|---|---|---|
| **Spike 1: Wormhole Portals** | Prove tunnel/bridge wormhole model connects arbitrary distant tiles | `src/portal/portal_registry.cpp`, `src/train_cmd.cpp` | Wormhole traversal, YAPF destrail integration | Unit test in `test_portal_wormhole.cpp` | Visual train hop verified in devbox | None | `07d89a1099` | [PORTAL_WORMHOLE_SPIKE.md](PORTAL_WORMHOLE_SPIKE.md) | TESTED |
| **Spike 2: Consist Decoupling** | Multi-wagon distance decoupling and emergence across wormhole | `src/portal/consist_traversal.cpp`, `src/train_cmd.cpp` | Consist tail tracking in wormhole | `test_consist_traversal.cpp` | Demo visual check | Mid-transit save/load edge cases | `668badf063` | [PORTAL_WORMHOLE_SPIKE.md](PORTAL_WORMHOLE_SPIKE.md) | TESTED |

---

## 2. Comprehensive Sprint Ledger (Sprints 1–52)

### Sprints 1–10: Planetary Regions, Portals & Operations Stabilisation

| Sprint | Objective | Major Systems / Files Changed | Source Implemented Today | Automated Tests | Human / UAT Evidence | Known Regressions & Acceptance Notes | Commit / Range | Sprint Doc / Ref | Status |
|---:|---|---|---|---|---|---|---|---|---|
| **1** | Planet regions, world phases and O(1) tile-to-world lookup | `src/portal/planet_manager.h/.cpp`, `src/portal/world_gen.cpp` | `PlanetManager`, `PlanetRegion`, 64×64 spatial grid | `test_planet_manager.cpp`, `test_world_gen.cpp` | UAT-02, UAT-08 | None | `351d105916` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **2** | World-phase construction restrictions | `src/portal/planet_manager.cpp`, placement check hooks | Void build prevention (`STR_ERROR_CANNOT_BUILD_IN_VOID_SPACE`) | `test_placement_restrictions.cpp` | UAT-02, UAT-08 | None | `e1a49b9474` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **3** | End-to-end local consist portal traversal | `src/portal/portal_wormhole.cpp`, `src/train_cmd.cpp` | Deterministic train transit through wormhole portals | `test_consist_traversal.cpp`, `test_portal_wormhole.cpp` | UAT-01, UAT-02, UAT-03 | YAPF portal shortcut assertion fixed in `174e571780` | `eb1c33ad49` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **4** | Multi-world generation with void isolation and paired gates | `src/portal/world_gen.cpp` | Procedural multi-world layout with buffer void tiles | `test_world_gen.cpp` | UAT-01, UAT-02, UAT-03 | Maximum practical limit ~6-8 worlds on 4096² map | `b5c635a947` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **5** | Interplanetary cargo attribution, revenue and industry rules | `src/portal/interplanetary_cargo.cpp`, `src/economy.cpp` | Cross-world distance scaling for cargo delivery revenue | `test_interplanetary_cargo.cpp` | UAT-05, UAT-06, UAT-07, UAT-08 | Cargo pricing formula requires late-game balance review | `f6fac060cc` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **6** | World-aware UI, viewport context and quick navigation | `src/portal/planet_ui.cpp`, `src/viewport.cpp` | World status banner, planet drop-down, jump-to-world | `test_planet_ui.cpp` | UAT-02 | WP-05 repaired GUI network command decoupling | `076379ba90` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **7** | Planet, portal and transit save/load persistence | `src/saveload/planet_sl.cpp` (`PLNT`, `PORT`, `PRTX`) | Full chunk serialization for worlds, portal links and terminals | `test_saveload_planet.cpp` | UAT-09 | Backward compatibility logic maintained | `599e3b4997` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **8** | Player gate construction, linking and demolition safeguards | `src/portal/portal_cmd.cpp`, `src/portal/portal_registry.cpp` | `CmdBuildPortalGate`, `CmdLinkPortalGate`, `CmdDemolishPortalGate` | `test_portal_construction.cpp` | UAT-03 | Gate linking bounds validation hardened | `a652292b52` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **9** | Spaceports, off-world trade and Edge Conduits | `src/portal/edge_conduit.cpp`, `src/portal/spaceport_manager.cpp` | Virtual import/export conduits via edge spaceports | `test_spaceports_and_conduits.cpp` | UAT-05, UAT-09 | WP-10 repaired honest station allocation (PR #8) | `043388f948` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **10** | Planetary-operations UAT and crash stabilisation | Multiple core fixes in `src/portal/`, `src/viewport.cpp` | Edge Conduit crash fix, portal placement safety, save compatibility | Suite regression tests | Recorded in `demo/SPRINT10-UAT.md` | Milestone accepted historically | `1d60095c1f` | [STABILISATION_UAT_2026-09-11.md](STABILISATION_UAT_2026-09-11.md) | UAT-ACCEPTED |

---

### Sprints 11–20: High-Capacity Portals, Federation Foundations & Megacities

| Sprint | Objective | Major Systems / Files Changed | Source Implemented Today | Automated Tests | Human / UAT Evidence | Known Regressions & Acceptance Notes | Commit / Range | Sprint Doc / Ref | Status |
|---:|---|---|---|---|---|---|---|---|---|
| **11** | Atomic high-capacity portal terminals and PBS approaches | `src/portal/portal_terminal.cpp` | Multi-track throat geometry, PBS reservation across portal heads | `test_portal_construction.cpp` | `demo/SPRINT11-UAT.md`, UAT-01 | Terminal ownership assigned to Company 0 in v1.1 | `f4f725ef0d` | [SPRINT11_PORTAL_OPERATIONS_2026-09-13.md](SPRINT11_PORTAL_OPERATIONS_2026-09-13.md) | TESTED |
| **12** | Federation namespace and consist snapshot foundation | `src/portal/consist_snapshot.cpp`, `src/portal/consist_snapshot.h` | In-memory consist serialization (engines, wagons, cargo, orders) | `test_federation.cpp` | UAT-16 | Base64 serialization verified | `977cf08ddf` | [SPRINT12_FEDERATION_FOUNDATION_2026-09-13.md](SPRINT12_FEDERATION_FOUNDATION_2026-09-13.md) | TESTED |
| **13** | Deterministic content manifest and admission boundary | `src/portal/content_manifest.cpp` | NewGRF/vehicle/cargo compatibility hash validation | `test_federation.cpp` | UAT-16 | Disallows mismatched GRF consist transfers | `977cf08ddf` | [SPRINT13_CONTENT_ADMISSION_2026-09-13.md](SPRINT13_CONTENT_ADMISSION_2026-09-13.md) | TESTED |
| **14** | Global identities, cargo provenance and snapshot v2 orders | `src/portal/federation_identity.cpp`, `src/saveload/planet_sl.cpp` (`FIDS`) | Universe-unique company/train IDs, origin world cargo tracking | `test_federation.cpp` | UAT-16 | Legacy ID mapping handled | `977cf08ddf` | [SPRINT14_FEDERATION_IDENTITIES_2026-09-13.md](SPRINT14_FEDERATION_IDENTITIES_2026-09-13.md) | TESTED |
| **15** | Federation transfer/materialisation prototype | `src/portal/consist_materializer.cpp` | Despawn at source gate, materialize at destination gate | `test_federation_transfer.cpp` | UAT-16 | Live external network transport not tested | `21ba79d845` | [SPRINT15_FEDERATION_PROTOTYPE_2026-09-13.md](SPRINT15_FEDERATION_PROTOTYPE_2026-09-13.md) | IMPLEMENTED |
| **16** | Persistent accounts, charters, directory and ledgers | `src/portal/universe_authority.cpp` | External Universe Authority data model: charters, accounts, fees | `test_federation_persistent_universe.cpp` | UAT-07, UAT-16 | Requires mock or live Python authority service | `21ba79d845` | [SPRINT16_PERSISTENT_UNIVERSE_2026-09-13.md](SPRINT16_PERSISTENT_UNIVERSE_2026-09-13.md) | IMPLEMENTED |
| **17** | Megacity demand and federation economy | `src/portal/megacity_manager.cpp`, `src/saveload/planet_sl.cpp` (`MEGA`) | Megacity resource demand quotas, supply fulfilment scaling | `test_federation_megacity_economy.cpp` | UAT-06 | Standalone domain test; not integrated with player win condition | `21ba79d845` | [SPRINT17_MEGACITY_ECONOMY_2026-09-13.md](SPRINT17_MEGACITY_ECONOMY_2026-09-13.md) | TESTED |
| **18** | Megacity, freight-corridor and universe-directory windows | `src/portal/megacity_gui.cpp`, `src/portal/universe_directory_gui.cpp` | GUI windows for megacity consumption, corridor status, directory | `test_megacity_gui.cpp` | UAT-06 | WP-05 repaired network command authority for GUI actions | `21ba79d845` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **19** | Cluster supervisor, topology bootstrap and process recovery | `scripts/cluster_testbed.py`, `src/portal/federation_staging.cpp` | Multi-node process supervision, automated restart and topology sync | `test_federation_cluster.cpp` | UAT-05, UAT-16 | Tooling verified; gameplay cluster not yet player-facing | `dad381b155` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **20** | Spaceport and Edge Conduit federation routing | `src/portal/edge_conduit.cpp`, `src/portal/authority_transport.cpp` | Cross-server conduit cargo transfers via HTTP authority | `test_spaceport_conduit_bridge.cpp` | UAT-05, UAT-16 | Dependent on external HTTP authority response | `dad381b155` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | IMPLEMENTED |

---

### Sprints 21–30: Telemetry, Blueprints, Prefabs & Biomes

| Sprint | Objective | Major Systems / Files Changed | Source Implemented Today | Automated Tests | Human / UAT Evidence | Known Regressions & Acceptance Notes | Commit / Range | Sprint Doc / Ref | Status |
|---:|---|---|---|---|---|---|---|---|---|
| **21** | Gateway telemetry/navigation and Commonwealth strings | `src/portal/gateway_arrays_and_telemetry.cpp`, `src/lang/` | Transit telemetry counters, Commonwealth terminology in strings | `test_gateway_arrays_and_telemetry.cpp` | UAT-02, UAT-06, UAT-13 | Bespoke original artwork absent | `dad381b155` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | IMPLEMENTED |
| **22** | Round-trip order restoration and scheduling | `src/portal/consist_materializer.cpp` | Consist order schedule restoration upon return to origin world | `test_federation_order_restoration.cpp` | UAT-09, UAT-16 | Tested in single process simulation | `c097822041` | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | TESTED |
| **23** | Scope, art and UAT audit | `docs/` | Historical milestone audit: scope boundaries, art gap register | None (docs only) | UAT-15 | Historical audit record | `2c5355436b` | [SPRINT23_SCOPE_ART_AND_UAT_AUDIT_2026-09-13.md](SPRINT23_SCOPE_ART_AND_UAT_AUDIT_2026-09-13.md) | IMPLEMENTED |
| **24** | Procedural styling for three showcase biomes and portal palette states | `src/portal/world_gen.cpp`, biome palette hooks | Procedural styling for Arid, Tundra, Volcanic biomes | `test_sprint24_alien_biomes.cpp` | UAT-02, UAT-15 | Palette recolouring proxy; bespoke source sprites pending | `5be256c1a8` | [SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md](SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md) | TESTED |
| **25** | Player rail blueprints, portable JSON and placement | `src/blueprint/blueprint.cpp`, `src/blueprint/blueprint_cmd.cpp` | JSON layout export/import, server placement command | `test_blueprint.cpp`, `test_blueprint_storage.cpp` | UAT-04 | User-reported placement crash fixed by WP-01; graphical retest pending | `bca2ada1cd` | [SPRINT25_PLAYER_RAIL_BLUEPRINTS_2026-09-13.md](SPRINT25_PLAYER_RAIL_BLUEPRINTS_2026-09-13.md) | BLOCKED |
| **26** | Eight built-in CST prefab rail blocks | `src/blueprint/cst_prefabs.cpp` | Catalogue of 8 standard CST layouts (throat, terminal, junction) | `test_cst_prefabs.cpp` | UAT-04 | WP-07: functional routing and signal validation partially accepted | `46e6ad8c8c` | [SPRINT26_CST_PREFAB_RAIL_BLOCKS_2026-09-13.md](SPRINT26_CST_PREFAB_RAIL_BLOCKS_2026-09-13.md) | BLOCKED |
| **27** | Trade ledger and federation account/charter UI | `src/portal/trade_ledger_gui.cpp`, `src/portal/federation_auth_gui.cpp` | Ledger and Charter management windows | `test_sprint27_feature_ui.cpp` | UAT-04-08 | Tested via domain and GUI mock harnesses | `80a4fbfb1d` | [SPRINT27_FEATURE_UI_COMPLETION_2026-09-13.md](SPRINT27_FEATURE_UI_COMPLETION_2026-09-13.md) | TESTED |
| **28** | Guided v0.4 solo UAT for features through Sprint 27 | `demo/` savegame and GameScript | Solo playable fixture with Story Book objectives | `test_sprint28_guided_uat.cpp` | UAT-01–09, UAT-16 | Historical v0.4 artifact superseded by v1.1 | `bda8b0599e` | [SPRINT28_GUIDED_SOLO_UAT_2026-09-13.md](SPRINT28_GUIDED_SOLO_UAT_2026-09-13.md) | UAT-ACCEPTED |
| **29** | Authority API acceptance, persistence, congestion and recovery suite | `src/portal/universe_authority.cpp` | Transactional transfer journal, congestion billing rules | `test_sprint29_federation_acceptance.cpp` | UAT-01–09, UAT-16 | Protocol-tested; independent live game-process handoff not proven | `2677feeb3a` | [SPRINT29_FEDERATION_ACCEPTANCE_KIT_2026-09-13.md](SPRINT29_FEDERATION_ACCEPTANCE_KIT_2026-09-13.md) | IMPLEMENTED |
| **30** | Six procedural biome behaviours and Phase 4 colonisation | `src/portal/world_gen.cpp`, `src/portal/planet_manager.cpp` | 6 biomes, Phase 4 colonization requirements | `test_sprint30_biomes_colonization.cpp` | UAT-02, UAT-06, UAT-08 | WP-08: colonization founding logic requires atomic town spawn | `9e023914be` | [SPRINT30_EXOTIC_BIOMES_AND_COLONIZATION_2026-09-14.md](SPRINT30_EXOTIC_BIOMES_AND_COLONIZATION_2026-09-14.md) | TESTED |

---

### Sprints 31–40: Colonisation, Lifecycle, In-Kind Fabrication & Corporate HQ

| Sprint | Objective | Major Systems / Files Changed | Source Implemented Today | Automated Tests | Human / UAT Evidence | Known Regressions & Acceptance Notes | Commit / Range | Sprint Doc / Ref | Status |
|---:|---|---|---|---|---|---|---|---|---|
| **31** | Colonisation GUI and authority API expansion | `src/portal/colonization_gui.cpp`, `src/portal/universe_authority.cpp` | Colonization progress window, planetary charter registration | `test_sprint31_colonization_gui.cpp` | UAT-02, UAT-06, UAT-08 | Command authority decoupled under WP-05 | `fd3e4f0e55` | [SPRINT31_COLONIZATION_GUI_AND_FEDERATION_2026-09-14.md](SPRINT31_COLONIZATION_GUI_AND_FEDERATION_2026-09-14.md) | IMPLEMENTED |
| **32** | Development scoring, phase promotion and rail technology restrictions | `src/portal/planet_manager.cpp`, `src/portal/tech_tree.cpp` | World development scoring, phase promotion thresholds | `test_sprint32_lifecycle_and_tech.cpp` | UAT-02, UAT-06, UAT-08 | Simulation progression verified in C++ tests | `55f954d6c6` | [SPRINT32_LIFECYCLE_AND_TECH_2026-09-14.md](SPRINT32_LIFECYCLE_AND_TECH_2026-09-14.md) | IMPLEMENTED |
| **33** | Town growth, Megacity supply integration and biome industry rules | `src/portal/megacity_manager.cpp`, `src/portal/world_gen.cpp` | Town growth gated on cargo supply quotas, biome industry filters | `test_sprint33_planetary_economy_and_megacity.cpp` | UAT-02, UAT-06, UAT-08 | Growth loop functions; balance against standard OpenTTD town growth open | `f614ef4f33` | [SPRINT33_MEGACITY_AND_COLONIAL_INDUSTRY_2026-09-14.md](SPRINT33_MEGACITY_AND_COLONIAL_INDUSTRY_2026-09-14.md) | IMPLEMENTED |
| **34** | Documentation consolidation and evidence correction | `docs/` | Comprehensive documentation baseline and evidence alignment | None (docs only) | None | Superseded by current recovery plan & architecture docs | `30bee5fdce` | [SPRINT34_DOCUMENTATION_CONSOLIDATION_2026-09-14.md](SPRINT34_DOCUMENTATION_CONSOLIDATION_2026-09-14.md) | SUPERSEDED |
| **35** | Cross-process transport | `src/portal/authority_transport.cpp`, scripts | Live process transfer via manual `federation_dispatch` | `scripts/test_federation_dispatch.py` | UAT-16 | Natural gate-entry departure not proven; manual command required | `387dd5d99d` | [SPRINT35_CROSS_PROCESS_FEDERATION_2026-09-14.md](SPRINT35_CROSS_PROCESS_FEDERATION_2026-09-14.md) | IMPLEMENTED |
| **36** | All-Feature Guided Solo UAT | `demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav`, GameScript | Guided solo scenario covering Sprints 1–42 | `test_sprint36_all_features_uat.cpp` | Checklist exists; human test **Not run** | Human acceptance unproven on rebuilt executable | `30bee5fdce` | [ALL-FEATURES-UAT.md](../demo/ALL-FEATURES-UAT.md) | IMPLEMENTED |
| **37** | Commonwealth economy and rolling-stock pack | `pkg/commonwealth_industry/`, `pkg/commonwealth_rail/`, `src/portal/commonwealth_pack.cpp` | In-tree NML packs (`OST\x01`, `OST\x02`), 12-cargo suite, CST rolling stock | `test_sprint37_commonwealth_pack.cpp` | UAT-13, UAT-15 | Migrated save does not activate NewGRF; fresh save or manual setup needed | `a6b79add03` | [SPRINT37_COMMONWEALTH_ECONOMY_AND_ROLLING_STOCK_2026-09-15.md](SPRINT37_COMMONWEALTH_ECONOMY_AND_ROLLING_STOCK_2026-09-15.md) | TESTED |
| **38** | Bespoke alien world and CST art | `pkg/` | Original sprites for alien terrain, monumental gates, arcologies | None | UAT-13, UAT-15 | **No original source art created; uses palette recolours** | None | [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md) | PLANNED |
| **39** | Corporate Headquarters, Planetary Stockpiles & Logistics Hubs | `src/portal/corporate_hq.cpp`, `src/portal/company_stockpile.cpp`, `src/portal/logistics_hub.cpp`, `src/saveload/planet_sl.cpp` (`CHQS`, `STCK`, `LHUB`) | HQ campus (Phase 1 Core), per-world stockpiles, bi-directional logistics hubs | `test_sprint39_corporate_hq_and_stockpile.cpp` | UAT-10, UAT-11 | WP-02/03/04 repaired hub cargo exclusive allocation and persistence | `30bee5fdce` | [SPRINT39_CORPORATE_HQ_AND_LOGISTICS_HUBS_2026-09-14.md](SPRINT39_CORPORATE_HQ_AND_LOGISTICS_HUBS_2026-09-14.md) | TESTED |
| **40** | In-Kind Fabrication Engine & Bill of Materials (BOM) | `src/portal/fabrication_manager.cpp`, `src/saveload/planet_sl.cpp` (`FABR`) | BOM recipes for rail, depot, vehicle construction; 80-90% cash discount | `test_sprint40_fabrication_engine.cpp` | UAT-10, UAT-11 | Fabrication discount logic verified | `30bee5fdce` | [SPRINT40_IN_KIND_FABRICATION_ENGINE_2026-09-14.md](SPRINT40_IN_KIND_FABRICATION_ENGINE_2026-09-14.md) | TESTED |

---

### Sprints 41–52: Production Chains, Expansion, Lore AI & Beyond

| Sprint | Objective | Major Systems / Files Changed | Source Implemented Today | Automated Tests | Human / UAT Evidence | Known Regressions & Acceptance Notes | Commit / Range | Sprint Doc / Ref | Status |
|---:|---|---|---|---|---|---|---|---|---|
| **41** | In-Lore Commonwealth Tech Tree & R&D Projects | `src/portal/tech_tree.cpp`, `src/saveload/planet_sl.cpp` (`TECH`), `src/portal/corporate_hq_gui.cpp` | 3 branches, 12 tech nodes, monthly R&D budget & crystal burning | `test_sprint41_tech_tree.cpp` | UAT-12, UAT-14 | Unit verified; corporate HQ GUI tab 5 integrated | `50df597962` | [SPRINT41_COMMONWEALTH_TECH_TREE_2026-09-15.md](SPRINT41_COMMONWEALTH_TECH_TREE_2026-09-15.md) | TESTED |
| **42** | Factorio-Scale Multi-World Production Chains | `src/portal/production_chain.cpp`, `src/saveload/planet_sl.cpp` (`PROD`), `src/economy.cpp` | 4 pipelines (Structural, Electronics, Propulsion, DataCrystals), station facilities | `test_sprint42_production_chains.cpp` | UAT-12, UAT-14 | WP-11 proved 1-pipeline vertical slice; full 4-pipeline human UAT pending | `a14c6538ea` | [SPRINT42_FACTORIO_SCALE_PRODUCTION_CHAINS_2026-09-15.md](SPRINT42_FACTORIO_SCALE_PRODUCTION_CHAINS_2026-09-15.md) | TESTED |
| **43** | Empire Facility Operations, Viewport Overlays & Closed Loops | `src/portal/empire_facilities_gui.cpp`, `src/portal/production_chain.cpp`, `src/viewport.cpp` | In-game viewport facility badges, dashboard window (`Ctrl+I`), 4 closed loops | `test_sprint43_empire_facilities.cpp` | None (human UAT **Not run**) | Merged to `main` via PR #22 | `d8fe4d9fb8` | [POST_RECOVERY_ROADMAP_SPRINTS_43_48.md](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md) | TESTED |
| **44** | Autonomous Lore AI Competitors (CST vs Grand Central) | `src/portal/lore_competitor.cpp`, `src/saveload/planet_sl.cpp` (`LORE`) | CST (heavy freight) and Grand Central (express commuter) scripted competitors | `test_sprint44_lore_ai.cpp` | None (human UAT **Not run**) | Merged to `main` via PR #23 | `e9c8e75435` | [POST_RECOVERY_ROADMAP_SPRINTS_43_48.md](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md) | TESTED |
| **45** | Unified Commonwealth Visual Overhaul Pack | `pkg/commonwealth_visuals/` (`OST\x03`), `src/portal/visual_overhaul.cpp`, `src/saveload/planet_sl.cpp` (`VISU`) | Monumental 18-tile portal arches, animated horizons, 4 biomes, arcologies | `test_sprint45_visual_overhaul.cpp` | None (human UAT **Not run**) | Merged to `main` via PR #24 | `8f092cb424` | [POST_RECOVERY_ROADMAP_SPRINTS_43_48.md](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md) | TESTED |
| **46** | Seamless Multi-Server Federation Universe & Live Cluster | `scripts/cluster_testbed.py`, `src/portal/federation_staging.cpp`, `src/portal/transfer_journal.cpp` (`FJRN`) | 3-node live cluster testbed, transfer custody, staging sidings | `test_sprint46_live_federation.cpp`, cluster testbed | None (human UAT **Not run**) | Merged to `main` via PR #21 | `6b3de33e96` | [POST_RECOVERY_ROADMAP_SPRINTS_43_48.md](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md) | TESTED |
| **47** | Colonial Megaprojects, Corporate Alliances & Arcologies | `src/portal/corporate_alliance.cpp` (`ALLI`), `src/portal/planet_manager.cpp` | Wilderness megaprojects, alliance treaties, trackage rights, arcology evolution | `test_sprint47_alliances_and_neutral_tracks.cpp` | None (human UAT **Not run**) | Merged to `main` via PR #18 | `f2997bdd3c` | [POST_RECOVERY_ROADMAP_SPRINTS_43_48.md](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md) | TESTED |
| **48** | LLM Narrative Scenario Synthesis & Autonomous Balancing Critic | `src/portal/prompt_scenario_generator.cpp`, `src/portal/balancing_critic.cpp` | CLI/GUI scenario generation, 50-year headless simulation harness | `test_sprint48_prompt_to_savegame.cpp`, `test_sprint48_balancing_critic.cpp` | None (human UAT **Not run**) | Merged to `main` via PRs #19 and #20 | `c2a7ca3830`, `869b234a82` | [POST_RECOVERY_ROADMAP_SPRINTS_43_48.md](POST_RECOVERY_ROADMAP_SPRINTS_43_48.md) | TESTED |
| **49** | Commonwealth Graph Engine & Prebuilt Lore Economies | `src/portal/universe_graph.cpp`, `src/portal/prebuilt_trade.cpp`, `src/saveload/planet_sl.cpp` (`TRAD`) | 108-world graph model, connection topology, off-world trade simulation proxy | `test_universe_graph.cpp`, `test_prebuilt_trade.cpp` | None (human UAT **Not run**) | Merged to `main` via PR #26 | `15149a1109`, `ab88ff9f98` | [SPRINTS_49_52_COMMONWEALTH_EXPANSION.md](SPRINTS_49_52_COMMONWEALTH_EXPANSION.md) | TESTED |
| **50** | High-Capacity Gateway Staging & Corporate Charters | `src/portal/federation_staging.cpp`, `src/portal/corporate_charter.cpp` | Multi-track gateway throat signalling, staging sidings, charter access tolls | `test_sprint50_throat_and_staging.cpp`, `test_sprint50_universal_and_charters.cpp` | None (human UAT **Not run**) | Merged to `main` via PR #28 | `125d6d48b5`, `a6cf83e6a6` | [SPRINTS_49_52_COMMONWEALTH_EXPANSION.md](SPRINTS_49_52_COMMONWEALTH_EXPANSION.md) | TESTED |
| **51** | Expeditionary Survey Logistics & Silfen Intermodal Paths | None yet | Planned: exploration and intermodal transshipment for frontier worlds | None | None | Withdrawn from active scope on 23 Sep 2026; retained as design history | None | [SPRINTS_49_52_COMMONWEALTH_EXPANSION.md](SPRINTS_49_52_COMMONWEALTH_EXPANSION.md) | SUPERSEDED |
| **52** | Galactic Commonwealth Hegemony & Narrative Lore Scenarios | None yet | Planned: 108-world unified dynamic tariffs, hegemon victory condition | None | None | Withdrawn from active scope on 23 Sep 2026; retained as design history | None | [SPRINTS_49_52_COMMONWEALTH_EXPANSION.md](SPRINTS_49_52_COMMONWEALTH_EXPANSION.md) | SUPERSEDED |
| **—** | Scheduled Federation Freight & 9-Scenario Recovery | `src/portal/authority_request_queue.h`, `src/portal/federation_orders.cpp` | Nonblocking async authority, portable schedules, 5-cycle coal transport, 9 recovery scenarios | `test_federation_reliability.cpp`, 59 Catch2 cases | Human UAT pending | Merged to `main` via PRs #40, #41 | `d886260607`, `19f6e249da` | [FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md](FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md) | TESTED |
| **—** | Player-Built Resource Surveys & Gated Extraction | `src/portal/resource_sites.cpp`, `src/portal/resource_sites_gui.cpp` (`RSRC`) | Hidden primary industry sites, paid survey commands, company knowledge, Materials research gating | `test_resource_sites.cpp`, 458 CTests | Human UAT pending | Merged to `main` via PR #42 | `8464e9374a` | [PLAYER_BUILT_RESOURCE_ECONOMY.md](PLAYER_BUILT_RESOURCE_ECONOMY.md) | TESTED |
| **—** | CST Star Map, Commissioned Gates & Remote Schedules | `src/portal/stellar_network.cpp`, `src/portal/universe_network.cpp` (`STLR`, `UNET`) | 7-world Mito–Merredin sector, remote station orders, 3-server live freight, visitor mode | `test_stellar_network.cpp`, 469 CTests | Human UAT pending; [checklist](../demo/CST-STELLAR-UAT.md) | Merged to `main` via PR #43 | `959fe33f18` | [CST_STELLAR_NETWORK.md](CST_STELLAR_NETWORK.md) | TESTED |
| **—** | Unified Commonwealth Economy & Progression | `src/portal/integrated_economy.cpp`, `src/saveload/planet_sl.cpp` (`ECON`) | 13 physical factory recipes, warehouse priority, research kit escrow, mandatory material bills, city baskets | `test_integrated_economy.cpp`, 485 CTests, 473 Catch2 cases | Human UAT pending; [checklist](../demo/INTEGRATED-ECONOMY-UAT.md) | Merged to `main` via PR #44 | `bc18c2c8fc` | [UNIFIED_COMMONWEALTH_ECONOMY.md](UNIFIED_COMMONWEALTH_ECONOMY.md) | TESTED |

---

## 3. Sprint Status Summary

| Status | Count | Sprints Included |
|---|---|---|
| **TESTED** | 37 | 1–9, 11–14, 17–19, 22, 24, 27, 30, 37, 39–50 |
| **IMPLEMENTED** | 8 | 15, 16, 20, 21, 23, 29, 31–33, 35, 36 |
| **UAT-ACCEPTED** | 2 | 10 (stabilisation gate), 28 (v0.4 solo UAT) |
| **BLOCKED** | 2 | 25 (blueprint placement retest pending), 26 (CST prefabs pending WP-07 routing) |
| **SUPERSEDED** | 3 | 34 (documentation consolidation), 51 (withdrawn), 52 (withdrawn) |
| **PLANNED** | 1 | 38 (bespoke original art) |
| **Total** | **53** | 52 Sprints + 1 Historical Super-Milestone |


## 2026-09-22 — CST prefab purchase-mode UAT blocker

Player-reported missing-material rejection exposed a purchase-mode discovery gap:
the ordinary company HQ is separate from corporate management, whose controls
also overflowed a single row. Blueprint Library now has a company purchase-mode
switch; HQ tabs/actions are split into rows with a dedicated mode switch. Both
use the existing authoritative command, without requiring an HQ or changing saved
mode automatically. Cash mode needs no stockpile; fabrication still requires
local-world materials. The library control supports the UAT's 640-pixel width;
the detailed HQ dashboard still requires a wider screen.

Automated verification: incremental build passed; 425 unit cases / 66,224
assertions and 436/436 CTests passed, including the new 56-assertion GUI
placement regression. File-description and unused-string linters and
`git diff --check` passed. Human retest pending.
See [UAT evidence and retest steps](../demo/UAT-RESULTS.md#2026-09-22--cst-prefab-purchase-mode-uat-blocker).


## 2026-09-22 — UAT text readability

Blueprint details and status messages now wrap; related counts share a compact
line, with more room for descriptions. Corporate HQ Overview and Fabrication
use short, wrapped instructions and a compact materials list. Repeated discount
claims and nonessential explanatory text were removed. HQ status messages wrap.
This is a presentation-only change; construction and resource rules are unchanged.

Verification: incremental build, 425 unit cases / 66,224 assertions, 436/436
CTests, both repository linters and `git diff --check` passed. Human visual retest
pending: reopen Blueprint Library and HQ Overview/Fabrication, check readability
at the user's font/UI scale, and resize the windows. Very long imported text still
requires sufficient panel height; the wider HQ dashboard is not redesigned here.

## Acceptance follow-up — Connected economy UAT v2.0 (2026-09-23)

| Scope | Implementation | Automated evidence | Human UAT | Status |
|---|---|---|---|---|
| Fully connected production and megacity showcase | Independent four-world save; 37 trains; 28 stations; eleven factories; food, research and construction supply. Label-based demand, residential acceptance and native-compatible v3 content repairs. | 427 unit cases / 66,233 assertions; 438/438 CTests; linters and content rebuild; repeated deliveries, 24-month soak, reload, starvation/recovery, exact prefab materials and research; legacy v2 regression. | Pending; graphical inspection unavailable in this session. | TESTED |

[Player checklist](../demo/CONNECTED-ECONOMY-UAT.md) ·
[Reproduction and limitations](CONNECTED_ECONOMY_UAT.md) ·
[Machine evidence](../demo/OpenSpaceTTD-Connected-Economy-UAT-v2.0.evidence.json).
This is an acceptance follow-up, not a new sprint or acceptance of Sprints 43–50.

## 2026-09-23 — Connected UAT crash and undefined cargo text

Human UAT **failed** on the initial connected save: `GetPartialPixelZ` reached
`NOT_REACHED` at landscape.cpp:298 while moving the viewport. The preserved
crash save contained 742 tiles with adjacent corners differing by more than one
height level. Direct flattening had left discontinuities at the construction
band boundary. All 13 Commonwealth cargos also lacked their quantity/unit strings.

The generator now smooths exposed terrain; a deterministic, preflighted load
repair handles the marked legacy fixture without touching infrastructure or
changing cargo, trains, cash or towns. Industry v4 supplies complete text; the
unchanged v2/v3 binaries receive a pack-scoped text compatibility repair.

Verification: 429 unit cases / 66,359 assertions; 440/440 CTests; repository
linters, diff whitespace and reproducible GRF checks passed. The actual crash
save passed 3,139,587 pixel-height queries and 1,046,529 inverse viewport queries,
with zero invalid slopes, complete text for all 24 active cargos in English-US
and English, exact state across reload and continued cargo-conserving simulation.
The recovered copy retains 37 trains and the city with 1,107 people / 45 houses.
The original crash save is unchanged. Fresh demo acceptance and legacy v2 WP-11
acceptance passed. Human graphical retest remains **pending**.

[Recovery evidence](../demo/CONNECTED-UAT-RECOVERY.evidence.json) ·
[Reproduction](CONNECTED_ECONOMY_UAT.md#terrain-and-cargo-text-recovery-2026-09-23).

## 2026-09-23 — Savegame authoring workflow

[Savegame authoring](SAVEGAME_AUTHORING.md) now records the reproducible build,
early terrain/text audits, logistics proof, checkpoint use, content compatibility
and publication workflow. `AGENTS.md` requires future scenario work to use it.
Documentation-only follow-up: no engine, content or save changes; human visual
acceptance remains pending. The guide explicitly distinguishes fixture-specific
assumptions from reusable practices.

Documentation verification: guide links and referenced tools checked; 429 unit
cases / 66,359 assertions, 440/440 CTests, both repository linters and
`git diff --check` passed. No executable or save regeneration was needed.

## 2026-09-25 — Federation scheduled freight (draft)

Branch `fix/federation-scheduled-recovery` integrates main with PRs #36/#37
and the existing UAT cargo/terrain recovery and authoring guide. New work adds
nonblocking multiplayer authority requests, explicit company/station mappings,
portable station schedules, packet provenance and coordinated-checkpoint tools.

This is **in progress, not sprint completion**. Combined unit tests previously
passed 438 cases / 66,610 assertions. The transport-only fixture passed a
30-second authority outage and destination restart with joined clients. The
scheduled native coal fixture completed three loaded deliveries and empty
returns (180 coal accepted) before its 600-second timeout; the required five
cycles and full recovery matrix have **not passed**. Conservation/payment checks
after five cycles, nonempty custom-state reload, blocked arrival and checkpoint
phase coverage remain unverified. Human federation UAT remains pending.

See [the implementation record](FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

## Federation freight acceptance follow-up — 27 September 2026

PR #40 is merged on main (`d886260607`). Follow-up work is on
`fix/federation-freight-recovery-acceptance`.

**Scheduled freight passes:** five automatic loaded outbound trips and empty
returns delivered 300 coal. Native production, unallocated cargo, station losses,
remaining stock and consumption reconcile exactly; all native cash changes also
reconcile. The two clients remain connected with no detected desync or duplicate.
The previous 600-second timeout was shorter than the native route's five-cycle
runtime. The harness now budgets startup plus time per requested cycle.

**All nine recovery cases pass:** physical blocked arrival, loaded-arrival
reload, authority-custody reload, prepared-departure reload, a 30-second authority
outage, destination restart, source restart, both clients reconnecting, and full
coordinated reload. Each case completes three further loaded deliveries and
empty returns with exact cargo and cash reconciliation. The retained sequence
contains 32 round trips and 1,920 coal delivered. During the authority outage,
both clients stayed connected and the maximum measured console response was
0.332 seconds; the original prepared request completed exactly once.

The loaded cargo/master-schedule save/load regression passes. Combined tests pass
439 cases / 66,636 assertions, isolated CTests pass 450/450, randomized federation
tests pass 59 cases / 1,172 assertions, and all three Python checkpoint/stdout
regressions pass. Current and legacy solo cargo/terrain recovery regressions also
pass. Graphical federation UAT remains separate and pending.

Machine evidence is recorded in `demo/FEDERATION-FREIGHT-RECOVERY.evidence.json`,
including the frozen binary hash, checkpoint hashes, accounting and continuation
provenance. Recovery uses only the latest coordinated set; arbitrary crash
recovery and old/mixed-save rollback are outside this acceptance boundary.

See [acceptance method and evidence](FEDERATION_RELIABILITY_AND_SCHEDULED_FREIGHT.md).

## Player-built resource economy — 28 September 2026

Feature branch `codex/player-built-resource-surveys` implements the approved
player-built default, basic primary starters, hidden sites for primary industries,
paid surveys, company knowledge, Materials research gating, GUI/script construction
and `RSRC` persistence. Native production, station facilities and fabrication are
retained. Existing-save conversion, depletion, claims and survey trains are excluded.

Automated verification: **447 unit cases / 66,829 assertions, 458/458 CTests**;
linters, fresh generation/two-client joins/cold reload, native command replication,
24-month connected resource economy and Classic WP-11 regression pass.
[Retained evidence](audit/2026-09-28/resource-economy/README.md). Human graphical UAT is **Pending**, not implied by passing
engine tests. [Specification and reproduction](PLAYER_BUILT_RESOURCE_ECONOMY.md).

## Feature delivery: CST star map and gate expansion — 28 September 2026

Current delivery branch: `codex/cst-star-map-expansion`, based on main
`8464e9374a` after PR #42. The opt-in Mito–Merredin preset, public CST backbone,
research-limited player gate projects, delivered machine modules, private/public
access and tolls are implemented. Connected hosts advertise actual worlds and
stations; trains use native remote schedules and gate pins, while multiplayer
clients can visit a registered host and follow a global train. Each server still
uses one global map with immutable logical world regions.

[Design, setup and scope](CST_STELLAR_NETWORK.md) ·
[Automated evidence](audit/2026-09-28/stellar-network/README.md) ·
[Human UAT checklist](../demo/CST-STELLAR-UAT.md).
Automated native checks: **457 unit cases / 67,032 assertions; 469/469 CTests**.
Three-server freight passes five round trips plus a cold-reload continuation:
360 coal consumed, cargo and cash reconciled, no detected client desync.
Human graphical acceptance for this new feature is **Pending**; the user's
23 September acceptance is retained for the earlier build. Verification counts
and recovery boundaries are recorded with the evidence, not inferred from old
sprint totals. Existing historical statuses below retain their original dates.

Next priorities: (1) human star-map, commissioning and host-visit UAT;
(2) interrupted remote-project and company-lifecycle acceptance; (3) progression
and freight balance using the new sector. The historical recovery and strategic
sprint registers remain delivered; removed narrative/Silfen/crisis plans are not
reactivated by this transport feature.

## Agent workflow tooling — 30 September 2026

Tooling delivery: compact AGENTS.md, shared development/UAT/delivery skills, effort
profiles and read-only context helper. No new gameplay sprint.
[Verification and remaining client checks](AGENT_WORKFLOW.md#delivery-record--30-september-2026).
