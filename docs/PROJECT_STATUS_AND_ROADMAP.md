# OpenSpaceTTD Project Status and Roadmap

Status: **CANONICAL**  
As of: **2026-09-14**  
Audited implementation commit: `f614ef4f339500215f039856903faf70b12537c5`  
Audited branch: `fix/portal-gate-lifecycle-crashes`

This is the authoritative answer to what is implemented, what has been tested, and what remains planned. Sprint specifications preserve the evidence and decisions available when each sprint closed; where they conflict with this page, this page governs current status.

## Status and evidence terms

| Term | Meaning |
|---|---|
| **Implemented** | Source and a corresponding commit exist. |
| **Automated verified** | Dedicated automated tests exist and the sprint document records a passing run. |
| **Playable accepted** | A supplied save or operator procedure exercises the feature in the game. |
| **Protocol accepted** | The C++ domain model and/or Python authority API is tested without proving a live transfer between independent game processes. |
| **Partial** | Part of the stated outcome exists, with a material portion still missing. |
| **Planned** | Approved next work with an assigned sprint and acceptance boundary. |
| **Vision** | Desired direction without an assigned delivery commitment. |
| **Historical** | Superseded plan or point-in-time evidence retained for traceability. |

The configured build registers **270 CTest cases** (all 270 automated unit and regression tests passing cleanly as of Sprint 40).

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
| 25 | Player rail blueprints, portable JSON and deterministic placement | Implemented; automated verified |
| 26 | Eight built-in CST prefab rail blocks | Implemented; automated verified |
| 27 | Trade ledger and federation account/charter UI | Implemented; automated verified |
| 28 | Guided v0.4 solo UAT for features through Sprint 27 | Playable artifact delivered; manual results are tester-dependent |
| 29 | Authority API acceptance, persistence, congestion and recovery suite | Protocol accepted; live game-process handoff not proven |
| 30 | Six procedural biome behaviours and Phase 4 colonisation | Implemented; automated verified |
| 31 | Colonisation GUI and authority API expansion | Implemented; protocol accepted |
| 32 | Development scoring, phase promotion and rail technology restrictions | Implemented; protocol accepted |
| 33 | Town growth, Megacity supply integration and biome industry rules | Implemented; protocol accepted |
| 34 | Documentation consolidation and evidence correction | **Complete:** this status register, documentation index and corrected scope boundaries |
| 36 | All-Feature Guided Solo UAT (Sprints 1–40) | Implemented; automated verified & playable accepted |
| 39 | Corporate Headquarters, Planetary Stockpiles & Logistics Hubs | Implemented; automated verified |
| 40 | In-Kind Fabrication Engine & Bill of Materials (BOM) | Implemented; automated verified |

Dedicated sprint documents exist for Sprints 11–17 and 23–34, 39, and 40. Sprints 1–10 and 18–22 are evidenced by commits, tests, UAT records and the grouped plans; missing individual files are a documentation-history gap, not an implementation gap.

Evidence is grouped in the [documentation index](README.md). The principal milestone records are the [Sprint 10 stabilisation report](STABILISATION_UAT_2026-09-11.md), [Sprint 17 federation economy report](SPRINT17_MEGACITY_ECONOMY_2026-09-13.md), [Sprint 24 procedural worlds report](SPRINT24_PLAYABLE_ALIEN_WORLDS_2026-09-13.md), [Sprint 29 protocol acceptance report](SPRINT29_FEDERATION_ACCEPTANCE_KIT_2026-09-13.md), [Sprint 33 planetary economy report](SPRINT33_MEGACITY_AND_COLONIAL_INDUSTRY_2026-09-14.md), [Sprint 39 corporate HQ report](SPRINT39_CORPORATE_HQ_AND_LOGISTICS_HUBS_2026-09-14.md), [Sprint 40 in-kind fabrication report](SPRINT40_IN_KIND_FABRICATION_ENGINE_2026-09-14.md), [solo UAT guide](../demo/ALL-FEATURES-UAT.md), [legacy solo UAT guide](../demo/SPRINT28-UAT.md) and [federation protocol guide](../demo/FEDERATION-UAT.md).

## Track status

| Track | Status | Boundary |
|---|---|---|
| Single-map planetary simulation | Implemented | Regions, void separation, portal transit, phases, six biome behaviours and colonisation are present. |
| Player rail construction | Implemented | Portal terminals, blueprints and eight CST prefabs are present. |
| Planetary economy | Implemented foundation | Revenue, development, Megacity demand, basic phase/biome restrictions and infrastructure throughput are present. The bespoke 12-cargo economy is pending. |
| Federation domain and authority protocol | Implemented; protocol accepted | Transfer, identity, admission, ledger, directory, congestion and recovery rules have automated coverage. |
| Federation runtime | Implemented & Verified | Independent dedicated servers connect to external Python Universe Authority. Live cross-process consist transfer, departure despawn, network transport, arrival materialization, order restoration, deduplication, and return trip verified in test_sprint35_cross_process.py. |
| Player and operator UI | Implemented through Sprint 40 | Main gameplay actions have native UI including Corporate HQ, Stockpiles, Logistics Hubs, and In-Kind Fabrication controls. Operator console commands allow runtime federation link management and status inspection. |
| Guided UAT | Implemented | `v1.0` covers all features through Sprint 40 (6 worlds, 6 biomes, 12 chapters, 25 goals, stockpiles, HQ, logistics hubs, fabrication); `v0.4` preserved for regression. |
| Commonwealth Track A — naming | Implemented | English and regional string alignment is present. |
| Commonwealth Track B — gameplay/content/art | Partial | Procedural biome rules and portal recolouring exist. Bespoke industry, rolling-stock, terrain, flora, portal and arcology packs do not. |
| Corporate HQ, Stockpiles, Logistics Hubs & Fabrication | Implemented; automated verified | Sprints 39–40 delivered Corporate HQ placement, multi-world stockpile accounting (`STCK`), bi-directional logistics hubs with reserve floors (`LHUB`), in-kind fabrication engine with 80% discount and BOM consumption (`FABR`), and GUI. Tech Tree (Sprint 41) sequenced next. |

## Planned sprints

### Sprint 35 — Real cross-process federation transport [COMPLETED]

**Completed and accepted.** See the [Sprint 35 implementation and acceptance checklist](SPRINT35_CROSS_PROCESS_FEDERATION_2026-09-14.md). External transport via `AuthorityTransportClient`, server game-loop tick polling via `FederationTransferManager::OnGameTick`, Base64 consist snapshot marshalling, deduplication guards, and `scripts/test_sprint35_cross_process.py` automated multi-process acceptance suite connecting two dedicated servers to the external Universe Authority with zero cargo leak and restored train orders.


### Sprint 36 — All-Feature Guided Solo UAT (Sprints 1–40) [COMPLETED]

Delivered canonical 6-world guided solo UAT savegame artifact (`demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav`), GameScript v8 with 12 Story Book chapters and 25 measurable acceptance goals (`bin/game/openspacettd_uat/`), operator console command `setup_uat_fixtures`, Catch2 test suite (`src/tests/test_sprint36_all_features_uat.cpp`), and player guide (`demo/ALL-FEATURES-UAT.md`).

### Sprint 37 — Commonwealth economy and rolling-stock pack

Build reproducible in-tree NML packages for the planned cargo/industry chains and CST vehicle families. Define compatibility IDs, licensing, source assets, build integration, save compatibility and content-admission tests. Do not mark the 12-cargo economy complete until every cargo has a producing, processing or consuming role and a playable delivery loop.

### Sprint 38 — Bespoke world and CST art

Produce original terrain, flora, portal, arcology, station and infrastructure source art with palette and zoom variants. Replace the current proxy use of base sprites and palette recolouring where the art direction requires new silhouettes. Acceptance includes provenance, reproducible exports and reviewed comparison captures for all six biomes.

### Sprint 39 — Corporate Headquarters & Dedicated Logistics Hubs [COMPLETED]

Delivered corporate headquarters placement on Phase 1 Core worlds, `CompanyWorldStockpile` data model with `STCK` save/load chunk persistence, dedicated Logistics Hubs (Corporate Warehouses) with bi-directional buffering and reserve floors (`LHUB`), and Corporate HQ GUI.

### Sprint 40 — In-Kind Fabrication Engine & Bill of Materials [COMPLETED]

Delivered `FabricationManager` with physical Bill of Materials (BOM) recipe registry, company-level dual-mode construction setting, command interception for rail, signal, depot, and vehicle construction with 80% discount and physical stockpile deduction, `FABR` save/load chunk persistence, and GUI integration.

### Sprint 41 — In-Lore Commonwealth Tech Tree & R&D Projects

Deliver the Commonwealth Tech Tree manager (`TechTreeManager` and `TECH` chunk persistence) within the Corporate HQ window. Implement R&D project trees covering traction tiers, portal throughput, superconductor physics, and advanced metallurgy. Support continuous monthly research progression powered by Research Points (RP) from Enriched Quantum Data Crystals (carrying advanced mathematical proofs and telemetry from frontier observatories), high-tech processors, and allocated monthly R&D budgets.

### Sprint 42 — Factorio-Scale Multi-World Production Chains

Harmonized with the Sprint 37 Commonwealth industry set: introduce specialized industrial fabrication chains (Silicates $\to$ Silicon Wafers $\to$ Microchips, Copper Extraction $\to$ Catenary Wire, Superalloys $\to$ Maglev Guideways, Blank Data Crystal Synthesis $\to$ Consumer Mail vs Enriched Quantum Proofs). Balance multi-world supply loops feeding both Megacity consumption tiers and high-tech R&D fabrication stockpiles.

## Unscheduled visions

These remain ideas rather than incomplete commitments: space combat or planetary defence, off-rail spacecraft, a detailed electrical-grid simulation, procedural alien languages, and dynamic climate change or terraforming.

## Current evidence gaps

- Full CTest has not been rerun at the audited Sprint 34 documentation state.
- The automated federation runner exercises the Python authority API; it does not launch trains inside independent OpenSpaceTTD processes.
- The v0.4 UAT save predates Sprints 30–33.
- Required art-direction comparison captures and a signed visual acceptance record are absent.
- No `assets/` directory, OpenSpaceTTD NML source or compiled OpenSpaceTTD content GRF exists.
