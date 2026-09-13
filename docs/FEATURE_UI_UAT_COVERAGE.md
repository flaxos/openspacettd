# OpenSpaceTTD Feature, UI and UAT Coverage

Status: **SPRINT 23 AUDIT BASELINE**  
Date: **2026-09-13**

This matrix is the release checklist for Sprints 24–29. **Present** means a source-backed UI route exists. **Partial** means the UI exposes only part of the workflow or the current demo does not exercise it. **Planned** means no implementation exists yet.

| Feature family | Current player/operator surface | Current coverage | Required UI/UAT closure |
|---|---|---|---|
| World regions, phases and biomes | World labels, viewport context and `Ctrl+Alt+1..3` demo navigation | Partial | Expose discoverable world navigation/help and prove distinct biome presentation in Sprint 24/28. |
| Portal gate build and link | Rail toolbar portal picker with build/link modes and directional controls | Present | Tutorial covers build, pair, reject invalid links, traverse and demolish safely. |
| Portal terminals | Automatic terminal construction and portal status | Partial | Surface footprint/capacity failures and exercise newly built terminals in the new save. |
| Edge Conduits | Rail toolbar construction; Land Area Information status | Present | Guided boundary placement, invalid placement, output and federation feeder case. |
| Spaceports | Station window designation, tier upgrade and trade telemetry | Present | Guided designation, supply, upgrade, dispatch and destination receipt. |
| Megacity demand | Town button and Megacity Overview window | Present | Deliver all three tiers and demonstrate each growth state. |
| Freight corridors | Map menu monitor with route, utilization, congestion and gate location | Present | Add player help for capacity/priority and demonstrate escalation and relief. |
| Supply-chain matrix and trade ledger | Map menu window (`TradeLedgerWindow`) | Present | Delivered in Sprint 27: dual-tab view with empire phase flows, infrastructure throughput, inter-world trade balances, and conservation auditing. |
| Federation authentication and charters | Map menu window (`FederationAuthWindow`) | Present | Delivered in Sprint 27: in-game account login/register, session badge, corporate chartering, owner delegation, and world presence expansion. |
| Consist handoff, identity and content admission | Automatic simulation with console/daemon diagnostics | Partial | Federation UAT shows identity continuity, mismatch rejection and cargo conservation. |
| Round-trip order restoration | Automatic materialization and integration tests | Partial | Federation UAT runs a visible outbound and return service with restored orders. |
| Cluster launch, health and recovery | `scripts/run_cluster.py` and configuration | Present for operators | Supply versioned topology, clean reset, health checks, failure injection and recovery guide. |
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
