# OpenSpaceTTD Feature, UI and UAT Coverage

Status: **SPRINTS 24–29 COMPLETE (ALL FEATURES ACCEPTED)**  
Date: **2026-09-13**

This matrix is the release checklist for Sprints 24–29. **Present** means a source-backed UI or operator acceptance route exists. **Partial** means the UI exposes only part of the workflow. **Planned** means no implementation exists yet.

| Feature family | Current player/operator surface | Current coverage | Required UI/UAT closure |
|---|---|---|---|
| World regions, phases and biomes | World labels, viewport context, `Ctrl+Alt+1..3` navigation, Map menu jump buttons | Present | Delivered in Sprints 24 & 28: distinct Temperate Core, Arid Industrial, and Sub-Arctic Frontier biomes with persistent Story Book navigation pins. |
| Portal gate build and link | Rail toolbar portal picker with build/link modes and directional controls | Present | Delivered in Sprints 11 & 28: interactive gate placement, inter-world pairing, invalid link rejection, and consist transit. |
| Portal terminals | Automatic terminal construction and portal status | Present | Delivered in Sprints 11, 24 & 28: 18-tile dual-track parallel holding loop with automated path signaling. |
| Edge Conduits | Rail toolbar construction; Land Area Information status | Present | Delivered in Sprints 20 & 28: boundary placement, resource extraction telemetry, and feeder credit. |
| Spaceports | Station window designation, tier upgrade and trade telemetry | Present | Delivered in Sprints 20 & 28: multi-tier spaceport upgrades, launch countdown, and cargo export telemetry. |
| Megacity demand | Town button and Megacity Overview window | Present | Delivered in Sprints 18 & 28: 3-tier demand progress bars (Sustenance, Expansion, Prosperity) and growth states. |
| Freight corridors | Map menu monitor with route, utilization, congestion and gate location | Present | Delivered in Sprints 18, 28 & 29: 4-tier congestion monitoring (Clear/Moderate/Congested/Saturated), dynamic transit scaling, and priority relief. |
| Supply-chain matrix and trade ledger | Map menu window (`TradeLedgerWindow`) | Present | Delivered in Sprints 27 & 29: dual-tab view with empire phase flows, infrastructure throughput, inter-world trade balances, and conservation auditing. |
| Federation authentication and charters | Map menu window (`FederationAuthWindow`) | Present | Delivered in Sprint 27: in-game account login/register, session badge, corporate chartering, owner delegation, and world presence expansion. |
| Consist handoff, identity and content admission | Automatic simulation with console/daemon diagnostics; Federation Acceptance Kit | Present | Delivered in Sprint 29: live consist snapshot handoff, strict NewGRF manifest validation, corrupt stream rejection, and zero-cargo leak invariant. |
| Round-trip order restoration | Automatic materialization and integration tests; Federation Acceptance Kit | Present | Delivered in Sprint 29: 3-hop multi-world circuit, global order indexing, and return loop order wrap-around (`current_order_index`). |
| Cluster launch, health and recovery | `scripts/run_cluster.py`, `scripts/run_acceptance_kit.sh`, `--state-file` checkpointing | Present | Delivered in Sprint 29: supervisor topology bootstrap, automated acceptance suite (`--run-acceptance`), node crash quarantine bay, and daemon state reload. |
| Alien biome and Commonwealth assets | Biome styling across showcase worlds, procedural MultiWorldGen, CST cyan-blue/yellow portal recolouring | Present | Delivered in Sprint 24. |
| Player rail blueprints | Rail toolbar button (`WID_RAT_BLUEPRINT`), Blueprint Library window (`BlueprintLibraryWindow`) | Present | Delivered in Sprint 25: map capture, library, JSON import/export, 90°/180°/270° rotation, horizontal flip, and deterministic placement command. |
| CST prefab rail blocks | Built-in CST templates in Blueprint Library | Present | Delivered in Sprint 26: 8 canonical CST layouts, RHD/LHD invariant transforms, and operating guidance. |
| Story Book and goals | In-game Story Book (`Manage Company` > `Story Book`) and Goal window | Present | Delivered in Sprint 28: 7 structured chapters, 16 persistent measurable goals, and clickable location pins in `OpenSpaceTTD-Phase1-2-3-UAT-v0.4.sav` (`OpenSpaceTTD-UAT-Demo` v7). |

## Definition of UI-wired

A player-facing feature is UI-wired when it has a discoverable entry point, visible current state, enabled/disabled explanation, cost or consequence before confirmation, actionable error feedback and relevant contextual help. A console command may remain for debugging but cannot be the only release path for routine play.

Operator-only federation functions may use a dedicated operator surface rather than an in-game player window. The documentation must identify permissions, target instance, expected result and recovery action.

## UAT evidence levels

- **Automated:** unit/regression or integration evidence for deterministic rules, formats and invariants.
- **Playable:** a tester performs the operation in the supplied save or federation kit and observes the result.
- **Visual:** captured evidence confirms art, layout and legibility criteria.

Release acceptance requires all applicable evidence levels. Protocol mocks support the federation kit but do not replace a live game-instance scenario.

## Demo goal catalogue

The Sprint 28 Story Book must include goals for navigating all three worlds; constructing and linking gates; running a portal consist; building a CST prefab; capturing and replacing a custom blueprint; operating an Edge Conduit; designating and upgrading a Spaceport; supplying Megacity demand tiers; and observing corridor state. Sprint 29 adds world-directory discovery, cross-server round trip, restored orders, content rejection, congestion relief, authority/server restart recovery and ledger conservation checks.
