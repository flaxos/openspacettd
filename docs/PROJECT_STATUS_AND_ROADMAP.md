# OpenSpaceTTD Project Status and Roadmap

Status: **CANONICAL**  
As of: **2026-09-15**

Audited implementation commit: `abbcd7e07737bcd83c3f830f539313a45bef5f01`
Audited branch: `fix/portal-gate-lifecycle-crashes`

This is the authoritative answer to what is implemented, what has been tested, and what remains planned. Sprint specifications preserve the evidence and decisions available when each sprint closed; where they conflict with this page, this page governs current status.

## Current recovery decision

The [master recovery plan](RECOVERY_PLAN_2026-09-15.md) is the single delivery plan.
The [feature matrix](FEATURE_UI_UAT_COVERAGE.md), [defect register](CRITICAL_BUG_REVIEW_2026-09-15.md)
and [player UAT](../demo/ALL-FEATURES-UAT.md) govern current acceptance. Feature
expansion is paused pending crash/conservation/authority and vertical-slice gates.
No new sprint sequence is assigned.

User-confirmed Blueprint **placement** crash has saved assertion/stack/screenshot
evidence. The later WP-01 local diff repairs Blueprint preflight/cost/material
handling, with 78 selected automated tests passing; graphical retest is pending.
See [WP-01 evidence](audit/2026-09-15/wp01/README.md). The later WP-02 repair makes
owned-hub freight storage exclusive of consumer delivery and accepts hub-only
freight; [WP-02 evidence](audit/2026-09-15/wp02/README.md) records regressions and
save/reload. WP-03 now allocates before hub withdrawal and preserves cargo when
allocation or a later split fails; [WP-03 evidence](audit/2026-09-15/wp03/README.md)
records failure injection, reserve/rights boundaries and save/reload.
Human acceptance is pending. Native external gate entry is
blocked; HQ/hub/reserve and colonisation workflows are incomplete. These are
concrete repair needs, not simply “more UAT”. No fix was implemented by this audit.
Audit checks:306 registered CTests,67 selected existing cases pass, two copied-save
load smokes pass. No current graphical acceptance or full-suite rerun. Live-main CI later failed three production fixtures; local assert-macro coverage is qualified by OST-TEST-001. Live main
is ahead of this checkout with CI repairs; see exact refs/checks in master plan.

Historical milestone descriptions below retain previous scope/test claims; they
must not override this correction or be read as current player acceptance.

## Current acceptance correction — v1.1 UAT refresh

The [Sprints 1–42 player checklist](../demo/ALL-FEATURES-UAT.md) and
[coverage matrix](FEATURE_UI_UAT_COVERAGE.md) supersede historical playable
acceptance claims below. The user's v1.0 ownership failure is reproduced by
generated gate heads/terminals using OWNER_NONE. v1.1 assigns complete demo
terminals to human Company 0; ordinary world generation is unchanged.

Current-build human cases remain **Not run**, except explicit blockers; the recorded prior-build Blueprint placement is **Fail**, pending current retest. Sprint 35's runner
uses manual `federation_dispatch`; natural gate-entry and complete recovery UAT
remain unproven. Sprint 37 content is not active in the migrated save. Sprint 42
now has station-based facility construction, delivery, output and lifecycle integration; human chain acceptance is pending. Sprint 38 original
art remains planned. These are acceptance/integration gaps even where historical
sprint reports say completed. The v1.1 artifact refresh itself added no missing gameplay. The subsequent [production integration fix](PRODUCTION_GAMEPLAY_INTEGRATION_2026-09-15.md) adds station-based facilities.

## Status and evidence terms

| Term | Meaning |
|---|---|
| **Implemented** | Source and a corresponding commit exist. |
| **Automated verified** | Dedicated automated tests exist and the sprint document records a passing run. |
| **Playable accepted** | A human run records observed results and evidence; supplying a save or procedure alone is not acceptance. |
| **Protocol accepted** | The C++ domain model and/or Python authority API is tested without proving a live transfer between independent game processes. |
| **Partial** | Part of the stated outcome exists, with a material portion still missing. |
| **Planned** | Approved next work with an assigned sprint and acceptance boundary. |
| **Vision** | Desired direction without an assigned delivery commitment. |
| **Historical** | Superseded plan or point-in-time evidence retained for traceability. |

The earlier blocker-fix record reports **306/306 CTest cases** (2026-09-15), including production lifecycle and delayed HTTP callback regressions. See [the critical review](CRITICAL_BUG_REVIEW_2026-09-15.md). This automated result does not close the human UAT cases.

## Completed spikes and stabilisation gates

| Item | Current status | Outcome |
|---|---|---|
| Portal/YAPF architecture spike | Implemented; automated verified | Proved explicit O(1) portal pairing, arbitrary non-colinear endpoints and pathfinder traversal using the tunnel/bridge wormhole model. |
| Multi-wagon wormhole traversal spike | Implemented; automated verified through Sprint 3 | Proved deterministic consist transit without relying on Euclidean continuity across remote portal heads. |
| Sprint 10 UAT stabilisation gate | Implemented; playable and automated evidence recorded | Fixed the Edge Conduit invalid-tile crash, hardened portal placement and preserved old save compatibility. |
| Federation F2 prototype | Implemented; protocol accepted | Proved snapshot despawn/materialisation, transaction state and conservation in C++ tests and service-level integration. It did not prove external authority transport between live game processes. |
| Federation F3 persistent-universe spike | Implemented; protocol accepted | Added identities, accounts, charters, directory, commodity ledger and trade balances. |
| Federation F4 economy spike | Implemented; automated verified | Added Megacity demand, freight-corridor congestion, priority relief and empire supply-chain accounting. |

## Sprint register

| Sprint | Delivered outcome | Current status |
|---:|---|---|
| 1 | Planet regions, world phases and O(1) tile-to-world lookup | Implemented; automated verified |
| 2 | World-phase construction restrictions | Implemented; automated verified |
| 3 | End-to-end local consist portal traversal | Implemented; automated verified |
| 4 | Multi-world generation with void isolation and paired gates | Implemented; automated verified |
| 5 | Interplanetary cargo attribution, revenue and industry rules | Implemented; automated verified |
| 6 | World-aware UI, viewport context and quick navigation | Implemented; automated verified |
| 7 | Planet, portal and transit save/load persistence | Implemented; automated verified |
| 8 | Player gate construction, linking and demolition safeguards | Implemented; automated verified |
| 9 | Spaceports, off-world trade and Edge Conduits | Implemented; automated verified |
| 10 | Planetary-operations UAT and crash stabilisation | Implemented; historical playable evidence |
| 11 | Atomic high-capacity portal terminals and PBS approaches | Implemented; automated verified |
| 12 | Federation namespace and consist snapshot foundation | Implemented; automated verified |
| 13 | Deterministic content manifest and admission boundary | Implemented; automated verified |
| 14 | Global identities, cargo provenance and snapshot v2 orders | Implemented; automated verified |
| 15 | Federation transfer/materialisation prototype | Implemented; protocol accepted |
| 16 | Persistent accounts, charters, directory and ledgers | Implemented; protocol accepted |
| 17 | Megacity demand and federation economy | Implemented; automated verified |
| 18 | Megacity, freight-corridor and universe-directory windows | Implemented; automated verified |
| 19 | Cluster supervisor, topology bootstrap and process recovery | Implemented; operator tooling verified |
| 20 | Spaceport and Edge Conduit federation routing | Implemented; protocol accepted |
| 21 | Gateway telemetry/navigation and Commonwealth strings | **Partial:** mechanics and strings implemented; proposed NewGRFs and bespoke assets absent |
| 22 | Round-trip order restoration and scheduling | Implemented; in-process automated verified |
| 23 | Scope, art and UAT audit | Complete historical documentation milestone |
| 24 | Procedural styling for three showcase biomes and portal palette states | Implemented; automated verified; manual visual evidence outstanding |
| 25 | Player rail blueprints, portable JSON and placement | Partial: WP-01 placement and WP-06 parser/I/O repaired locally; capture workflow/routing and graphical acceptance remain |
| 26 | Eight built-in CST prefab rail blocks | Catalogue implemented; functional routing and complete command parity not accepted |
| 27 | Trade ledger and federation account/charter UI | Implemented; automated verified |
| 28 | Guided v0.4 solo UAT for features through Sprint 27 | Playable artifact delivered; manual results are tester-dependent |
| 29 | Authority API acceptance, persistence, congestion and recovery suite | Protocol accepted; live game-process handoff not proven |
| 30 | Six procedural biome behaviours and Phase 4 colonisation | Implemented; automated verified |
| 31 | Colonisation GUI and authority API expansion | Implemented; protocol accepted |
| 32 | Development scoring, phase promotion and rail technology restrictions | Implemented; protocol accepted |
| 33 | Town growth, Megacity supply integration and biome industry rules | Implemented; protocol accepted |
| 34 | Documentation consolidation and evidence correction | **Complete:** this status register, documentation index and corrected scope boundaries |
| 35 | Cross-process transport | Implemented; operator-dispatch script exists; natural-entry/recovery human UAT outstanding |
| 36 | All-Feature Guided Solo UAT | v1.1 coverage through Sprint 42; human acceptance Not run |
| 37 | Commonwealth economy and rolling-stock pack | Implemented; automated verified |
| 38 | Bespoke alien world and CST art | Planned; no original-art acceptance |
| 39 | Corporate Headquarters, Planetary Stockpiles & Logistics Hubs | Implemented; automated verified |
| 40 | In-Kind Fabrication Engine & Bill of Materials (BOM) | Implemented; automated verified |
| 41 | In-Lore Commonwealth Tech Tree & R&D Projects | Implemented; automated verified |
| 42 | Factorio-Scale Multi-World Production Chains | Implemented; automated verified |

Dedicated sprint documents exist for Sprints 11–17, 23–35, 37 and 39–42. Sprint 36 is represented by the UAT guide/tests; Sprint 38 remains in the roadmap/art direction. Sprints 1–10 and 18–22 are evidenced by commits, tests, UAT records and grouped plans; missing individual files are a documentation-history gap, not by themselves an implementation gap.

Evidence is grouped in the [documentation index](README.md). The principal milestone records are the [Sprint 10 stabilisation report](STABILISATION_UAT_2026-09-11.md), [Sprint 17 federation economy report](SPRINT17_MEGACITY_ECONOMY_2026-09-13.md), [Sprint 24 procedural worlds report](SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md), [Sprint 29 protocol acceptance report](SPRINT29_FEDERATION_ACCEPTANCE_KIT_2026-09-13.md), [Sprint 33 planetary economy report](SPRINT33_MEGACITY_AND_COLONIAL_INDUSTRY_2026-09-14.md), [Sprint 37 Commonwealth pack report](SPRINT37_COMMONWEALTH_ECONOMY_AND_ROLLING_STOCK_2026-09-15.md), [Sprint 39 corporate HQ report](SPRINT39_CORPORATE_HQ_AND_LOGISTICS_HUBS_2026-09-14.md), [Sprint 40 in-kind fabrication report](SPRINT40_IN_KIND_FABRICATION_ENGINE_2026-09-14.md), [Sprint 41 tech tree report](SPRINT41_COMMONWEALTH_TECH_TREE_2026-09-15.md), [Sprint 42 production chains report](SPRINT42_FACTORIO_SCALE_PRODUCTION_CHAINS_2026-09-15.md), [solo UAT guide](../demo/ALL-FEATURES-UAT.md), [legacy solo UAT guide](../demo/SPRINT28-UAT.md) and [federation protocol guide](../demo/FEDERATION-UAT.md).

## Track status

| Track | Status | Boundary |
|---|---|---|
| Single-map planetary simulation | Implemented | Regions, void separation, portal transit, phases, six biome behaviours and colonisation are present. |
| Player rail construction | Implemented | Portal terminals, blueprints and eight CST prefabs are present. |
| Planetary economy | Implemented foundation & 12-cargo chains | Revenue, development, Megacity demand, basic phase/biome restrictions, infrastructure throughput, and 12-cargo Commonwealth multi-world production pipelines (Pipelines A–D) with `PROD` persistence are present. |
| Federation domain and authority protocol | Implemented; protocol accepted | Transfer, identity, admission, ledger, directory, congestion and recovery rules have automated coverage. |
| Federation runtime | Partial; repair/acceptance required | Transport exists; runner manually dispatches. External native entry is blocked in source; exact identity/orders/custody and recovery require WP-F1/F2. |
| Player and operator UI | Implemented through Sprint 41 | Main gameplay actions have native UI including Corporate HQ, Stockpiles, Logistics Hubs, In-Kind Fabrication controls, and Commonwealth Tech Tree R&D tab. Operator console commands allow runtime federation link management and status inspection. |
| Guided UAT | Coverage refreshed; human acceptance pending | v1.1 maps Sprints 1–42, repairs player terminal ownership and hub attachment, and explicitly records blocked concepts. v1.0/v0.4 preserved for regression. |
| Commonwealth Track A — naming | Implemented | English and regional string alignment is present. |
| Commonwealth Track B — gameplay/content/art | Partial | In-tree NML industry/cargo pack (`OST\x01`) and CST rolling-stock pack (`OST\x02`) implemented with reproducible Python GRF generator, Tech Tree vehicle gating, and closed 12-cargo loops. Bespoke terrain, flora, portal and arcology art packs remain open for Sprint 38. |
| Corporate HQ, Stockpiles, Fabrication, Tech Tree & Industry | Implemented; automated verified | Sprints 39–42 delivered Corporate HQ placement, multi-world stockpile accounting (`STCK`), bi-directional logistics hubs with reserve floors (`LHUB`), in-kind fabrication engine (`FABR`), Commonwealth Tech Tree R&D manager (`TECH`), and Factorio-scale 12-cargo production chains (`PROD`) across Pipelines A–D. |

## Historical milestone descriptions and remaining art direction

“Completed” labels below reproduce implementation milestones, not current acceptance.
Use the recovery plan and matrix for open defects/workflow/content gates.

### Sprint 35 — Real cross-process federation transport [COMPLETED]

**Implementation delivered; acceptance qualified.** See the [Sprint 35 implementation report](SPRINT35_CROSS_PROCESS_FEDERATION_2026-09-14.md). External transport, tick polling and snapshot marshalling exist. The runner calls manual dispatch for both directions; it does not establish natural gate-entry departure or every UAT-16 recovery/order criterion. Record those separately rather than inheriting the historical completed label.


### Sprint 36 — Guided Solo UAT [ARTIFACT DELIVERED; HUMAN ACCEPTANCE PENDING]

Current artifact is `demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav`, GameScript v9 with 15 chapters and 27 checklist goals. The player guide maps all Sprints 1–42, while ownership repair, hub station attachment and explicit integration blockers correct gaps in v1.0. Goals are prompts, not automatic human acceptance evidence.

### Sprint 37 — Commonwealth economy and rolling-stock pack [COMPLETED]

Delivered in-tree NML packages for Commonwealth industries and 12-cargo suite (`pkg/commonwealth_industry/`) and CST rolling stock (`pkg/commonwealth_rail/`), deterministic Python GRF build pipeline (`scripts/build_commonwealth_grf.py`), `CommonwealthPackManager` with Tech Tree gating (`TECH_TRACTION_1..4`), World Phase operational restrictions, In-Kind BOM linkage, content admission boundary integration, and closed delivery loops for all 12 Commonwealth cargos across Pipelines A–D. Fully verified with Catch2 test suite (`src/tests/test_sprint37_commonwealth_pack.cpp`). See [Sprint 37 milestone report](SPRINT37_COMMONWEALTH_ECONOMY_AND_ROLLING_STOCK_2026-09-15.md).

### Sprint 38 — Bespoke world and CST art

Produce original terrain, flora, portal, arcology, station and infrastructure source art with palette and zoom variants. Replace the current proxy use of base sprites and palette recolouring where the art direction requires new silhouettes. Acceptance includes provenance, reproducible exports and reviewed comparison captures for all six biomes.

### Sprint 39 — Corporate Headquarters & Dedicated Logistics Hubs [COMPLETED]

Delivered corporate headquarters placement on Phase 1 Core worlds, `CompanyWorldStockpile` data model with `STCK` save/load chunk persistence, dedicated Logistics Hubs (Corporate Warehouses) with bi-directional buffering and reserve floors (`LHUB`), and Corporate HQ GUI.

### Sprint 40 — In-Kind Fabrication Engine & Bill of Materials [COMPLETED]

Delivered `FabricationManager` with physical Bill of Materials (BOM) recipe registry, company-level dual-mode construction setting, command interception for rail, signal, depot, and vehicle construction with 80% discount and physical stockpile deduction, `FABR` save/load chunk persistence, and GUI integration.

### Sprint 41 — In-Lore Commonwealth Tech Tree & R&D Projects [COMPLETED]

Delivered `TechTreeManager` domain model with 3 lore branches (`Traction & Propulsion`, `Wormhole & Portal Physics`, `Materials & Fabrication`) and 12 canonical technologies (Tiers 1–4). Requires Corporate HQ on Phase 1 Core world and prerequisite DAG validation. Integrated monthly R&D progression loop powered by dual-input funding: cash budget ($1\text{ RP}/1,000\text{ Cr}$) plus HQ world feedstock burning (Enriched Quantum Data Crystals at $10\text{ RP}/\text{unit}$ up to 5/mo; High-Tech Electronics at $5\text{ RP}/\text{unit}$ up to 10/mo). Unlocking `TECH_MATERIALS_3` dynamically upgrades fabrication discount to 90% (leaving only a 10% labor fee). Server commands `Commands::SelectResearchProject` and `Commands::SetResearchBudget`, `TECH` table chunk persistence, and Corporate HQ GUI 5th tab integration. Fully verified with Catch2 test suite (`src/tests/test_sprint41_tech_tree.cpp`).

### Sprint 42 — Factorio-Scale Multi-World Production Chains [COMPLETED]

Delivered `ProductionChainManager` with 12-cargo Commonwealth suite across 4 interlocking pipelines (Pipeline A: Structural; Pipeline B: Electronics; Pipeline C: Propulsion; Pipeline D: Data Crystals & R&D). Enforces planetary world phase constraints (Phase 3 raw extraction and quantum telemetry; Phase 2 heavy industrial processing; Phase 1 Megacity formatting). Monthly conversion simulation inside `_economy_spaceports_conduits_monthly` with +15% yield bonus for `TECH_MATERIALS_3` (Automated Nanofabrication Lines). Automatic buffering to planetary stockpiles when a Logistics Hub is present. `PROD` table chunk save/load persistence. Verified with Catch2 test suite (`src/tests/test_sprint42_production_chains.cpp`).

## Unscheduled visions

These remain ideas rather than incomplete commitments: space combat or planetary defence, off-rail spacecraft, a detailed electrical-grid simulation, procedural alien languages, and dynamic climate change or terraforming.

## Current evidence gaps

- Human v1.1 UAT and visual acceptance remain outstanding; automated results do not establish player acceptance.
- The independent-process federation runner uses manual dispatch. Natural gate entry and the full recovery matrix still require acceptance evidence.
- Sprint 37 pack sources and compiled GRFs exist, but the migrated v1.1 save does not activate them.
- Sprint 42 station production upgrades have a gameplay path; human cross-world chain acceptance remains outstanding, including distinct active-pack cargos.
- Bespoke Sprint 38 art and required biome comparison captures remain outstanding.

See [the critical bug review](CRITICAL_BUG_REVIEW_2026-09-15.md) for the Sprint 37 engine-identity fix and prioritised follow-up.


## Recovery supersedes unqualified milestone completion

Finish WP-01 graphical acceptance and WP-02/03 hub delivery/pickup acceptance with
the rebuilt local executable; the user has reported no further Blueprint crash.
WP-04 hub binding, station lifecycle and persistence are now repaired locally
([evidence](audit/2026-09-15/wp04/README.md)); human hub acceptance remains open.
WP-05 HQ/Directory command authority and outpost-location persistence are also
repaired locally:95 selected CTests and a native-command TCP server/two-client
replay pass ([evidence](audit/2026-09-15/wp05/README.md)). Graphical UAT-08/10 and
full multiplayer join acceptance remain separate. WP-06 Blueprint parser/storage
safety is now implemented:35 selected CTests and six real I/O fault injections pass
([evidence](audit/2026-09-15/wp06/README.md)); UAT-04f/g graphical acceptance is pending.
WP-07 capture and prefab routes is next. Independent provenance work may proceed.
Do not start Sprint38 art or new features to bypass these gates. The current
13-cargo content, actual player establishment, durable federation and graphical
acceptance gaps are detailed in the master plan; preserve original product intent.
