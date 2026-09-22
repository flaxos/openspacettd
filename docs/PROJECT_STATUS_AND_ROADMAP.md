# OpenSpaceTTD Project Status and Roadmap

Status: **CANONICAL**<br>
As of: **2026-09-22**

Audited main branch HEAD: `e4baa35623` (Sprints 1–48 and WP-01–11 merged via PRs #4–#25)<br>
Current working HEAD: `a6cf83e6a6` (branch `feature/sprint-50-gateway-staging-and-charters`; Sprints 49–50 **not yet merged to main**)

This is the authoritative answer to what is implemented, what has been tested, and what remains planned. Sprint specifications preserve the evidence and decisions available when each sprint closed; where they conflict with this page, this page governs current status. For source-level architecture details, see [CURRENT_ARCHITECTURE.md](CURRENT_ARCHITECTURE.md). For known gaps and unproven claims, see [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md).

## Project Vision

OpenSpaceTTD is a railway-heavy interstellar logistics and economic simulation built on OpenTTD, inspired by Factorio-scale production chains and Peter F. Hamilton's Commonwealth saga. Players construct and operate high-capacity planetary rail networks across multiple distinct worlds connected via fixed wormhole portal gates. Worlds have defined development phases (Core, Industrial, Emerging, Wilderness) and specialized economic profiles, creating systemic interdependence where mature core worlds consume advanced commodities and off-world food, while frontier colonies extract raw resources and require industrial fabrication support—all simulated deterministically on OpenTTD's high-throughput transport engine.

## Current Architecture Reality

OpenSpaceTTD operates on a **single global OpenTTD map** (up to 4096×4096 tiles) partitioned into discrete logical world regions separated by unbuildable void space. It does **not** employ a multi-map engine. World identity is resolved via an $O(1)$ spatial hash grid (`PlanetManager`). Inter-world railway transit adapts OpenTTD's tunnel/bridge wormhole subsystem (`Track::Wormhole`), allowing consists to traverse distant coordinates without intermediate physical track tiles. Custom state is persisted across 20 dedicated save/load chunks in `src/saveload/planet_sl.cpp`. All custom gameplay logic is isolated in `src/portal/` and `src/blueprint/`. At current working HEAD `a6cf83e6a6`, 435+ registered CTests pass cleanly. For complete subsystem details, see [CURRENT_ARCHITECTURE.md](CURRENT_ARCHITECTURE.md). For gaps and unproven claims, see [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md). For the complete 52-sprint ledger, see [SPRINT_LEDGER.md](SPRINT_LEDGER.md).

## Current Project Milestone

- **Main Branch Milestone (`e4baa35623`):** Sprints 1–48, WP-01–11 recovery fixes, WP-F1/F2 federation custody, and Horizon A cluster testbed are fully merged (PRs #4–#25).
- **Working HEAD Milestone (`a6cf83e6a6`):** Sprints 49 and 50 are implemented on feature branches (`feature/sprint-49-commonwealth-graph-engine`, `feature/sprint-50-gateway-staging-and-charters`) with passing unit tests, but are **not yet merged to `main`**.
- **Active Acceptance Gap:** Sprints 43–50 have zero recorded human visual UAT evidence. The v1.1 guided solo UAT save (`demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav`) has not been played through by a human tester.
- **Immediate Milestone Goal:** Merge Sprints 49–50 to `main`, execute human UAT for Sprints 43–50, and commence Sprint 51 (Expeditionary Survey Logistics).

## Current recovery and post-recovery status

The [master recovery plan](RECOVERY_PLAN_2026-09-15.md) tracks historical recovery delivery.
The [feature matrix](FEATURE_UI_UAT_COVERAGE.md), [defect register](CRITICAL_BUG_REVIEW_2026-09-15.md),
[sprint ledger](SPRINT_LEDGER.md), and [player UAT](../demo/ALL-FEATURES-UAT.md) govern current acceptance.
Recovery work packages WP-01 through WP-11 and Federation packages WP-F1 and WP-F2
have been implemented and merged to `main` (PRs #8–#17). All six strategic post-recovery
sprints (Sprints 43–48) have likewise been implemented and merged (PRs #18–#24).
Passing automated tests do not substitute for human acceptance; visual UAT remains
tracked in [UAT results](../demo/UAT-RESULTS.md).

User-confirmed Blueprint **placement** crash has saved assertion/stack/screenshot
evidence. The later WP-01 local diff repairs Blueprint preflight/cost/material
handling, with 78 selected automated tests passing; graphical retest is pending.
See [WP-01 evidence](audit/2026-09-15/wp01/README.md). The later WP-02 repair makes
owned-hub freight storage exclusive of consumer delivery and accepts hub-only
freight; [WP-02 evidence](audit/2026-09-15/wp02/README.md) records regressions and
save/reload. WP-03 now allocates before hub withdrawal and preserves cargo when
allocation or a later split fails; [WP-03 evidence](audit/2026-09-15/wp03/README.md)
records failure injection, reserve/rights boundaries and save/reload.
WP-10 honest conduit delivery is merged (PR #8) with 6 dedicated Catch2 tests passing.
External gate custody (WP-F1/WP-F2) and Horizon A multi-node cluster testbed are merged (PRs #12–#17, #21).
Human acceptance is pending. Audit checks: 435+ registered CTests pass.

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
| 9 | Spaceports, off-world trade and Edge Conduits | Implemented; automated verified (WP-10 honest allocation merged) |
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
| 43 | Empire Facility Operations, Viewport Overlays & Closed Loops | Implemented; automated verified (PR #22) |
| 44 | Autonomous Lore AI Competitors (CST vs Grand Central) | Implemented; automated verified (PR #23) |
| 45 | Unified Commonwealth Visual Overhaul Pack | Implemented; automated verified (PR #24) |
| 46 | Seamless Multi-Server Federation Universe & Live Cluster | Implemented; automated cluster testbed verified (PR #21) |
| 47 | Colonial Megaprojects, Corporate Alliances & Arcologies | Implemented; automated verified (PR #18) |
| 48 | LLM Narrative Scenario Synthesis & Autonomous Balancing Critic | Implemented; automated verified (PR #19, #20) |
| 49 | Commonwealth Graph Engine & Prebuilt Lore Economies | Implemented; automated verified (feature branch; **not yet on main**) |
| 50 | High-Capacity Gateway Staging & Corporate Charters | Implemented; automated verified (feature branch; **not yet on main**) |
| 51 | Expeditionary Survey Logistics & Silfen Intermodal Paths | **Planned** |
| 52 | Galactic Commonwealth Hegemony & Narrative Lore Scenarios | **Planned** |

Dedicated sprint documents exist for Sprints 11–17, 23–35, 37 and 39–42; Sprint 49–52 are described in `docs/SPRINTS_49_52_COMMONWEALTH_EXPANSION.md`. Sprint 36 is represented by the UAT guide/tests; Sprint 38 remains in the roadmap/art direction. Sprints 1–10 and 18–22 are evidenced by commits, tests, UAT records and grouped plans; missing individual files are a documentation-history gap, not by themselves an implementation gap.

Evidence is grouped in the [documentation index](README.md). The principal milestone records are the [Sprint 10 stabilisation report](STABILISATION_UAT_2026-09-11.md), [Sprint 17 federation economy report](SPRINT17_MEGACITY_ECONOMY_2026-09-13.md), [Sprint 24 procedural worlds report](SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md), [Sprint 29 protocol acceptance report](SPRINT29_FEDERATION_ACCEPTANCE_KIT_2026-09-13.md), [Sprint 33 planetary economy report](SPRINT33_MEGACITY_AND_COLONIAL_INDUSTRY_2026-09-14.md), [Sprint 37 Commonwealth pack report](SPRINT37_COMMONWEALTH_ECONOMY_AND_ROLLING_STOCK_2026-09-15.md), [Sprint 39 corporate HQ report](SPRINT39_CORPORATE_HQ_AND_LOGISTICS_HUBS_2026-09-14.md), [Sprint 40 in-kind fabrication report](SPRINT40_IN_KIND_FABRICATION_ENGINE_2026-09-14.md), [Sprint 41 tech tree report](SPRINT41_COMMONWEALTH_TECH_TREE_2026-09-15.md), [Sprint 42 production chains report](SPRINT42_FACTORIO_SCALE_PRODUCTION_CHAINS_2026-09-15.md), [solo UAT guide](../demo/ALL-FEATURES-UAT.md), [legacy solo UAT guide](../demo/SPRINT28-UAT.md) and [federation protocol guide](../demo/FEDERATION-UAT.md).

## Track status

| Track | Status | Boundary |
|---|---|---|
| Single-map planetary simulation | Implemented | Regions, void separation, portal transit, phases, six biome behaviours and colonisation are present. |
| Player rail construction | Implemented | Portal terminals, blueprints and eight CST prefabs are present. |
| Planetary economy | Implemented foundation & 12-cargo chains | Revenue, development, Megacity demand, basic phase/biome restrictions, infrastructure throughput, and 12-cargo Commonwealth multi-world production pipelines (Pipelines A–D) with `PROD` persistence are present. |
| Federation domain and authority protocol | Implemented; protocol accepted | Transfer, identity, admission, ledger, directory, congestion and recovery rules have automated coverage. |
| Federation runtime | Implemented & verified via cluster testbed | Transport, WP-F1/F2 authority custody, and Horizon A 3-node live cluster testbed implemented and automated verified (`scripts/cluster_testbed.py`). |
| Player and operator UI | Implemented through Sprint 48 | Main gameplay actions have native UI including Corporate HQ, Stockpiles, Logistics Hubs, In-Kind Fabrication controls, Commonwealth Tech Tree R&D tab, Empire Facility Overlays (Sprint 43), AI competitor controls (Sprint 44), and Prompt-to-Savegame Scenario Generator GUI (Sprint 48). Operator console commands allow runtime federation link management and status inspection. |
| Guided UAT | Coverage refreshed; human acceptance pending | v1.1 maps Sprints 1–42, repairs player terminal ownership and hub attachment, and explicitly records blocked concepts. v1.0/v0.4 preserved for regression. |
| Commonwealth Track A — naming | Implemented | English and regional string alignment is present. |
| Commonwealth Track B — gameplay/content/art | Implemented with Sprint 45 visuals | In-tree NML industry/cargo pack (`OST\x01`), CST rolling-stock pack (`OST\x02`), and Unified Commonwealth Visual Overhaul Pack (Sprint 45: monumental portal arches, animated wormhole horizons, 4 distinct biomes, arcologies, and 12-cargo fleet). |
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

### Sprint 43 — Empire Facility Operations & Viewport Overlays [COMPLETED]

Delivered Empire-Wide Facility Dashboard (`Map > Industrial Facilities & Supply Chain` / `Ctrl+I`), viewport overlay status badges for active facilities and resource starvation, station platform outbound publishing with automatic overflow routing into planetary logistics hubs, facility decommissioning server command, and 4 closed production loop recipes. Verified with unit tests (`src/tests/test_sprint43_empire_facilities.cpp`) and merged via PR #22.

### Sprint 44 — Autonomous Lore-Driven AI Competitors [COMPLETED]

Delivered CST vs Grand Central AI competitors (`LoreAIManager`), scripted home networks on Phase 2 Merredin (CST heavy freight) and Phase 1 Earth/Augusta (Grand Central express commuter), autonomous expansion toward contested gateway corridors using 8 canonical CST prefabs (`Commands::PlaceBlueprint`), and gateway capacity transit bidding. Verified with unit tests (`src/tests/test_sprint44_lore_ai.cpp`) and merged via PR #23.

### Sprint 45 — Unified Commonwealth Visual Overhaul Pack [COMPLETED]

Delivered comprehensive visual overhaul NewGRF (`pkg/commonwealth_visuals/` / `OST\x03`), monumental 18-tile portal arch architecture, animated wormhole horizon effects and transit particles, 4 distinct planetary biome surface textures (Arid Frontier, Boreal Tundra, Volcanic Barren, Lush Earth-like), 3-tier Megacity Arcology urban evolution, and custom 12-cargo rolling stock fleet graphics. Verified with Catch2 suite (`src/tests/test_sprint45_visual_overhaul.cpp`) and merged via PR #24.

### Sprint 46 — Seamless Multi-Server Federation & Horizon A Cluster [COMPLETED]

Delivered multi-node live cluster testbed (`scripts/cluster_testbed.py`), authority custody handoff, automatic holding and staging sidings, in-game Galaxy Directory, and multi-server economic clearinghouse. Verified with automated cluster testbed harness across Sprints 43–48 and merged via PR #21.

### Sprint 47 — Colonial Megaprojects & Arcology Metropolises [COMPLETED]

Delivered 3-tier supply delivery colonisation megaprojects for Phase 4 Wilderness worlds, planetary phase promotion events, corporate alliances with neutral track running rights, and Megacity arcology evolution driven by commodity quotas. Verified with unit tests (`src/tests/test_sprint47_colonial_megaprojects.cpp`) and merged via PR #18.

### Sprint 48 — LLM Narrative Scenario Synthesis & Autonomous Balancing Critic [COMPLETED]

Delivered Prompt-to-Savegame Generator with CLI, GUI, and procedural synthesis, plus 50-year headless fast-forward simulation harness and autonomous economic balancing critic. Verified with unit tests (`src/tests/test_sprint48_narrative_and_critic.cpp`) and merged via PRs #19 and #20.

## Dependency-Ordered Strategic Roadmap

Future development work must follow this dependency order to prevent building advanced gameplay on fragile foundations:

### Phase 1: Foundation & Correctness (Current Priority)
1. **Authoritative Documentation & Baseline Alignment (Sprint 50/Audit):** Source-of-truth documentation tied to exact HEAD (`a6cf83e6a6`), resolving contradictions and establishing strict status definitions.
2. **Central Validation of References:** Post-load and post-destruction reference validation across custom world, portal, and entity indices in `afterload.cpp`.
3. **Complete Portal State Persistence:** Operational state, transit queue, and signal reservation persistence across save/load cycles.
4. **Safe Local & External Gate Classification:** Deterministic separation of intra-system wormholes vs external federation gates.
5. **End-to-End Natural Traversal Verification:** Black-box automated and human verification of train movement through portals without manual operator intervention.
6. **Save/Load Mid-Transit Recovery:** Proven deterministic recovery of consists saved while inside `Track::Wormhole`.

### Phase 2: Network Architecture
1. **Decoupled Topology Model:** Complete separation of universe/world graph routing concepts (`UniverseGraphManager`) from physical tile and gate metadata (`PortalRegistry`).
2. **Multi-Hop & Branching Routing:** Proven YAPF routing across A→B→C world topologies with dynamic path invalidation upon portal disruption.
3. **Capacity, Congestion & Staging:** Gate throat signalling, automated holding sidings, and schedule recovery under portal bottleneck conditions.
4. **WorldContext Boundary:** World-qualified references and explicit world boundaries maintained while preserving the single-map engine.

### Phase 3: Game Systems & Coherent Economy
1. **Unified Inter-World Economy:** Consolidate overlapping overlays (standard industries, Commonwealth 12-cargo pipelines, Megacity quotas, Prebuilt trade) into a unified simulation model.
2. **Phase & Tier System Integration:** Strict planetary phase restrictions governing resource extraction, manufacturing, tech access, and vehicle tiers.
3. **Core / Frontier Interdependence:** Economic modeling requiring mature core worlds to import bulk foodstuffs and raw ores, making inter-world logistics indispensable.
4. **Logistics-Driven Population Growth:** Town and colony growth driven by sustained multi-world supply chains rather than local passenger loops.
5. **Content & Visual Progression:** NewGRF packs (`OST\x01`, `OST\x02`, `OST\x03`) and bespoke Commonwealth art layered cleanly onto the verified simulation.

---

## Explicit Deferrals

To maintain project focus on core rail logistics and avoid premature scope expansion, the following areas are **explicitly deferred**:

1. **True Multi-Map Engine Refactoring:** Independent maps, world dynamic loading/unloading, background LOD simulation, and asynchronous world tick rates are deferred unless an architectural decision proves single-map limits are permanently exceeded.
2. **Premature Performance Micro-Optimisations:** Optimising tile arrays or spatial lookups before profiling demonstrates measurable frame bottlenecks.
3. **Broad Cosmetic Art Expansion:** Producing dozens of bespoke sprite sets before underlying vehicle physics, BOM fabrication, and delivery loops are accepted.
4. **Off-Rail Mechanics:** Space combat, orbital dogfighting, planetary defence lasers, terraforming dynamics, and procedural alien linguistics remain unscheduled visions outside the project's transport core.

---

## Unresolved Architecture Decisions Requiring Owner Input

The following decisions represent fundamental forks in engine design and require explicit owner direction:

1. **Multi-Map Migration Threshold:**
   *Current state:* Single 4096×4096 map supporting ~6–8 simultaneous worlds.
   *Question:* What specific gameplay requirement (e.g. 20+ playable worlds, memory constraints, independent tick rates) triggers a migration to a true multi-map engine architecture, and who approves the prerequisite rewrite?
2. **Economic Engine Authority:**
   *Current state:* Five loosely-coupled economic overlays (OpenTTD standard, `ProductionChainManager`, `MegacityManager`, `PrebuiltTradeManager`, `EdgeConduit`).
   *Question:* Which subsystem should become the single source of truth for cargo supply, demand, and pricing, and how should legacy OpenTTD industry loops be reconciled with Commonwealth 12-cargo pipelines?
3. **Federation Gameplay Role:**
   *Current state:* Working protocol and cluster testbed, but transport requires manual console dispatch.
   *Question:* Is federation intended as a seamless player-facing MMO-style cluster, or primarily as an offline/asynchronous scenario and savegame exchange protocol?
4. **NewGRF Content vs Native Engine Integration:**
   *Current state:* Dual-representation with in-tree compiled NML NewGRFs (`OST\x01`–`OST\x03`) alongside C++ hardcoded `CommonwealthCargoID` structures.
   *Question:* Should Commonwealth content be fully standardized as standalone NewGRF packages loaded at game creation, or integrated natively into the core C++ engine tables?

---

## Historical Recovery Retrospective

The recovery phase initiated on 2026-09-15 addressed critical blockers in the experimental codebase:
- **WP-01:** Fixed Blueprint placement crash, preflight costing, and material reservation.
- **WP-02 / WP-03:** Enforced exclusive company stockpile allocation and prevented cargo duplication during hub transfers.
- **WP-04:** Restored Logistics Hub station binding and savegame lifecycle integrity.
- **WP-05:** Decoupled GUI action handlers from local state to ensure server-authoritative network command replication.
- **WP-06:** Hardened Blueprint JSON parser and storage against malformed files.
- **WP-10:** Replaced speculative Edge Conduit cargo credit with honest OpenTTD station acceptance verification (PR #8).
- **WP-11:** Delivered active economic vertical slice with in-kind steel and ballast production (PR #9).
- **WP-F1 / WP-F2:** Hardened external portal gate classification and cross-server transfer custody journal checkpoints (PRs #12–#20).

All recovery repairs are committed and merged into `main`. For detailed historical audit logs, see [RECOVERY_PLAN_2026-09-15.md](RECOVERY_PLAN_2026-09-15.md) and [CRITICAL_BUG_REVIEW_2026-09-15.md](CRITICAL_BUG_REVIEW_2026-09-15.md).
