# OpenSpaceTTD Feature, UI and UAT Coverage

Status: **CURRENT THROUGH SPRINT 34 — MATERIAL ACCEPTANCE GAPS OPEN**  
Date reconciled: **2026-09-14**

This matrix complements [PROJECT_STATUS_AND_ROADMAP.md](PROJECT_STATUS_AND_ROADMAP.md). “Implemented” describes source availability. Automated, playable, visual and live-runtime evidence are recorded separately so one type cannot be mistaken for another.

| Feature family | UI or operator surface | Implementation | Acceptance evidence | Remaining gap |
|---|---|---|---|---|
| World regions and navigation | Viewport context, hotkeys and Map menu | Implemented | Automated; v1.0 playable for six worlds (Sprint 36) | None |
| Six biome behaviours | Generated terrain rules and directory badges | Implemented | Automated; v1.0 playable | Bespoke sprites and visual review pending |
| Portal construction and linking | Rail toolbar portal picker | Implemented | Automated; v1.0 playable | None for local single-map portals |
| Portal terminals | Automatic construction and land information | Implemented | Automated; v1.0 playable | None for local terminal construction |
| Edge Conduits | Rail toolbar and Land Area Information | Implemented | Automated; v1.0 playable | External federation dispatch depends on live-runtime work |
| Spaceports | Station designation, upgrade and telemetry controls | Implemented | Automated; v1.0 playable | External federation dispatch depends on live-runtime work |
| Megacity demand and growth | Town and Universe Directory buttons; overview | Implemented | Automated; v1.0 covers demand UI & growth states | None |
| Phase 4 colonisation | Universe Directory & command colonisation | Implemented | Automated; v1.0 playable (Chapter 8) | None |
| Development and promotion | Directory progress and promotion command | Implemented | Automated; v1.0 playable (Chapter 9) | None |
| Rail technology restrictions | Construction validation and errors | Implemented | Automated; v1.0 playable | None |
| Biome industry restrictions | Construction validation and errors | Implemented | Automated; v1.0 playable | None |
| Freight corridors | Map menu monitor | Implemented | Automated and authority API scenarios | Live traffic from independent game servers pending |
| Supply chain and trade ledger | Map menu ledger window | Implemented | Automated and v1.0 UI walkthrough | Remote live-authority data pending |
| Federation accounts and charters | Map menu federation window | Implemented | Automated and v1.0 UI walkthrough | Remote live-authority session integration pending |
| Content admission and consist identity | Automatic transfer domain logic and diagnostics | Implemented | C++ and Python protocol tests | Live cross-process departure/materialisation pending |
| Round-trip order restoration | Automatic transfer domain logic | Implemented | In-process C++ and Python protocol tests | Visible train round trip across game processes pending |
| Cluster supervision and recovery | `run_cluster.py` and authority endpoints | Implemented | Process/API tests and operator guide | Recovery of a real in-game consist pending |
| Player blueprints | Rail toolbar and Blueprint Library | Implemented | Automated; v1.0 playable | None within rail-only v1 scope |
| CST prefabs | Built-ins in Blueprint Library | Implemented | Automated; v1.0 playable | Bespoke CST visual assets pending |
| Solo Story Book and goals | v1.0 GameScript v8 and save | Implemented through Sprint 40 | Artifact and scripted walkthrough (12 ch, 25 goals) | None |
| Corporate HQ & Campuses | Map menu and dedicated Corporate HQ window | Implemented | Automated; v1.0 playable (Chapter 10) | None |
| Planetary Stockpiles & Logistics Hubs | Corporate HQ Stockpile tab and Station Logistics Hub | Implemented | Automated; v1.0 playable (Chapter 11) | None |
| In-Kind Fabrication & BOM Engine | Corporate HQ toggle, BOM deduction, 80% discount | Implemented | Automated; v1.0 playable (Chapter 12) | None |
| Commonwealth 12-cargo economy | None | Not implemented | None | Planned Sprint 37 |
| Bespoke CST and alien art | None beyond palette/base-sprite treatment | Not implemented | No visual evidence pack | Planned Sprint 38 |

## Acceptance definitions

- **Automated:** unit, regression or service integration evidence for rules, formats and invariants.
- **Playable:** a tester performs the operation in a supplied game save and observes the intended result.
- **Visual:** reviewed captures verify art identity, layout and legibility.
- **Live runtime:** independent game processes exchange real engine state through the external authority without manual lifecycle calls.

A player feature is UI-wired when it has a discoverable entry point, visible state, cost or consequence before confirmation, disabled-state explanation, actionable errors and contextual help. Operator-only functions may use a documented operator interface.

## UAT closure requirements

Sprint 36 must extend the guided UAT with Phase 4 discovery, all six biome badges, outpost founding, development scoring, phase promotion, technology unlocks, town growth, Megacity lifecycle and biome industry restrictions. Sprint 35 must first supply the live-runtime federation path used by the matching multi-server exercise.

Sprint 38 visual acceptance must include comparable normal-zoom captures for all six biomes, active/inactive portal states, stations, industries, settlements and busy rail layouts.
