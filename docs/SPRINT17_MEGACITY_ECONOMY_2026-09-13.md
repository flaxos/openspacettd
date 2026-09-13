# OpenSpaceTTD Sprint 17 — Megacity & Empire Economy (Technical Spike F4)

Status: **COMPLETE**

Sprint 17 implements and validates **Phase F4: Megacity & Empire Economy**. It introduces sustained multi-tier commodity demand mechanics for metropolitan core worlds, high-throughput freight corridors with dynamic transit delay scaling and priority QoS congestion mitigation, a multi-planet empire supply chain matrix, in-engine console commands, Universe Authority daemon REST endpoints, Catch2 unit test coverage, and automated integration test spikes.

---

## Player & Architecture Outcomes

- **Megacity Multi-Tier Commodity Quotas:** Core world towns designated as Megacities track sustained monthly quotas across three demand tiers:
  - **Tier 1 (Sustenance):** Food, Water, and basic survival consumables ($P / 20$).
  - **Tier 2 (Expansion):** Steel, Goods, Building materials, and structural components ($P / 40$).
  - **Tier 3 (Prosperity):** High-Tech, Electronics, Data Crystals, and Valuables ($P / 100$).
- **Dynamic Growth Multipliers & Population Mechanics:** Monthly supply evaluation calculates tier satisfaction percentages and sets the city's growth stage:
  - **Starvation ($0.0\times$ growth, $0.5\times$ pax/mail):** Triggered if Tier 1 satisfaction $< 50\%$.
  - **Subsistence ($1.0\times$ baseline growth, $1.0\times$ pax/mail):** Tier 1 satisfied ($\ge 50\%$), Tier 2 incomplete.
  - **Metropolitan Boom ($1.5\times$ growth, $1.25\times$ pax/mail):** Tier 1 and Tier 2 fully met ($\ge 100\%$), Tier 3 incomplete.
  - **HyperGrowth ($2.0\times$ growth, $1.5\times$ pax/mail):** All three tiers fully met ($\ge 100\%$).
- **Freight Corridor Congestion & Priority QoS:** Portal routes track bandwidth capacity and active in-transit trains. As utilization rises, congestion escalates:
  - **Clear ($< 50\%$ utilization):** $1.0\times$ transit duration.
  - **Moderate ($50-80\%$ utilization):** $1.2\times$ transit duration.
  - **Congested ($80-100\%$ utilization):** $1.5\times$ transit duration.
  - **Saturated ($> 100\%$ utilization):** $2.0\times$ transit duration (backpressure delay).
  - **Priority QoS Relief:** Express and PriorityUrgent freight shipments receive a 50% reduction on congestion delay penalties.
- **Empire Supply Chain Matrix & Tariffs:** Aggregates macro flows across developmental world tiers:
  - **Frontier $\to$ Refinery:** Phase 3 $\to$ Phase 2 raw cargo flow.
  - **Refinery $\to$ Core:** Phase 2 $\to$ Phase 1 refined industrial inputs.
  - **Frontier $\to$ Core:** Phase 3 $\to$ Phase 1 direct sustenance shipments.
  - **Core Export:** Phase 1 $\to$ Any high-tech and consumer exports.
  - **Interplanetary Tariffs:** $10$ Cr per unit generated across world boundaries.
- **Strict Commodity Conservation:** The zero-loss, zero-duplication invariant holds across all corridor congestion escalations, queue delays, and multi-leg transfers:
  $$\forall c \in \text{Commodities},\quad \text{Cargo}_{\text{Init}}(c) = \text{Cargo}_{\text{Done}}(c) + \text{Cargo}_{\text{InTransit}}(c)$$

---

## Technical Components Delivered

### 1. Megacity Demand Manager (`src/portal/megacity_manager.h`, `src/portal/megacity_manager.cpp`)
- **`MegacityDemandTier`:** `Tier1_Sustenance`, `Tier2_Expansion`, `Tier3_Prosperity`.
- **`MegacityGrowthState`:** `Starvation`, `Subsistence`, `MetropolitanBoom`, `HyperGrowth`.
- **`MegacityProfile`:** Tracks town ID, world ID, population, 3-tier quotas, current and last monthly deliveries, satisfaction percentages, overall supply index, and growth/passenger multipliers.
- **`MegacityManager`:**
  - `RegisterMegacity`, `UnregisterMegacity`, `IsMegacity`, `GetProfile`, `GetAllMegacities`.
  - `UpdatePopulation`, `SetCustomQuotas`.
  - `RecordDelivery`, `RecordDeliveryByCargo` with cargo type classification.
  - `EvaluateMonthlySupply` for cyclical evaluation and delivery reset.

### 2. Freight Corridor Congestion & Empire Supply Matrix (`src/portal/universe_authority.h`, `src/portal/universe_authority.cpp`)
- **`CorridorCongestionLevel`:** `Clear`, `Moderate`, `Congested`, `Saturated`.
- **`FreightPriority`:** `Bulk`, `Standard`, `Express`, `PriorityUrgent`.
- **`InterServerRoute`:** Extended with `max_bandwidth_trains_per_min`, `max_active_in_transit`, `priority`, `congestion_level`, `current_in_transit_count`, and `total_trains_dispatched`.
- **`EmpireSupplyChainMatrix`:** Tracks macro-phase cargo movements and tariff revenue.
- **`UniverseAuthorityService`:**
  - `GetFreightCorridors`, `UpdateCorridorLimits`, `EvaluateCorridorCongestion`.
  - `InitiateTransfer`: Dynamically computes effective transit ticks via congestion multiplier and priority relief, increments active corridor count, and updates supply chain matrix.
  - `ConfirmTransferArrival`: Relieves corridor in-transit count, re-evaluates congestion level, and moves cargo to completed ledger.
  - `GetEmpireSupplyChainMatrix`.

### 3. Universe Authority Daemon Extensions (`scripts/universe_authority.py`)
- Added `megacities` registry and `supply_chain_matrix` state to `UniverseAuthority`.
- Extended `register_route`, `update_corridor_limits`, `evaluate_corridor_congestion`.
- Integrated dynamic delay scaling with priority QoS mitigation and phase-based supply chain accounting in `initiate_transfer`.
- Added corridor congestion relief to `confirm_arrival`.
- Implemented REST endpoints:
  - `GET /corridors/list`, `POST /corridors/register`, `POST /corridors/update`
  - `POST /megacity/register`, `POST /megacity/demand`, `POST /megacity/eval`, `GET /megacity/status`
  - `GET /economy/matrix`

### 4. In-Game Console Administration Commands (`src/console_cmds.cpp`)
- **`universe_corridors [update <id> <bandwidth> <cap>]`:** Inspects all freight corridors, active in-transit counts, utilization, congestion level, and allows tuning limits.
- **`universe_megacity <list | register <tid> <name> [pop] | eval>`:** Manages and inspects Megacity demand tiers, delivery quotas, satisfaction indices, and triggers monthly evaluation cycles.
- **`universe_economy`:** Displays the empire-wide supply chain matrix (Frontier $\to$ Refinery, Refinery $\to$ Core, Frontier $\to$ Core, Core Export, and total tariffs).

---

## Automated Acceptance & Verification

- [x] **Catch2 Megacity & Empire Economy Suite (`src/tests/test_federation_megacity_economy.cpp`):**
  - Megacity registration, dynamic population quota scaling, custom quotas, cargo delivery recording, and evaluation across Starvation, Subsistence, Boom, and HyperGrowth states.
  - Freight corridor limits, congestion escalation (`Clear` $\to$ `Moderate` $\to$ `Congested` $\to$ `Saturated`), dynamic delay calculation, Express/PriorityUrgent QoS penalty relief, and stepwise arrival relief back to `Clear`.
  - Multi-world empire supply chain matrix tracking across Phase 3 $\to$ Phase 2 $\to$ Phase 1 flows with tariffs and zero commodity loss.
- [x] **Full Catch2 Unit Suite (`./build/openttd_test "Federation*"`):**
  - 17 test cases, 407 assertions passing with 0 failures.
- [x] **Full Regression & Unit Suite (`ctest --test-dir build`):**
  - 185/185 tests passing (100% pass rate).
- [x] **Backward Compatibility Protocol Spike (`scripts/test_two_server_federation.py`):**
  - F2 multi-server transfer handoff and binary execution continue to pass cleanly.
- [x] **Persistent Universe Suite (`scripts/test_f3_persistent_universe.py`):**
  - F3 player accounts, corporate charters, directory heartbeats, and per-cargo ledgers pass cleanly.
- [x] **Federation F4 Integration Spike (`scripts/test_f4_megacity_economy.py`):**
  - End-to-end multi-process test validating live daemon REST endpoints: megacity demand cycles, corridor congestion escalation & relief, priority QoS, empire supply chain matrix, and strict conservation audit.
