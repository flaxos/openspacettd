# Sprint 29 Federation UAT: Protocol and Cluster-Supervisor Guide

> **Recovery qualification:** [Master plan](../docs/RECOVERY_PLAN_2026-09-15.md),
> OST-FED-001/003 and UAT-16 govern current acceptance. Native external entry is
> source-confirmed blocked; full physical custody/order/recovery fixture is pending.
> This guide retains protocol/operator procedures, not player-route proof.

Current qualification: see UAT-16 in [the player checklist](ALL-FEATURES-UAT.md).
The Sprint 35 runner manually dispatches both directions. Record natural entry,
exact arrival, visible cargo/consist, return orders, mismatch, obstruction,
duplicates and restart recovery separately in [UAT-RESULTS.md](UAT-RESULTS.md).
This historical protocol guide and the solo save cannot establish those passes.

**Milestone:** Sprint 29 (Federation Acceptance Kit)  
**Target:** Universe Authority protocol plus cluster process supervision  
**Supervisors & Runners:** `scripts/run_acceptance_kit.sh`, `scripts/run_cluster.py`  

> **Evidence boundary:** The automated runner starts the Python Universe Authority and drives world registration, transfer, congestion, recovery and ledger endpoints directly. It does not launch trains inside independent OpenSpaceTTD game processes. `run_cluster.py` can launch three dedicated game processes, but the documented manual HTTP lifecycle is not proof of an engine-driven cross-process handoff. Sprint 35 later delivered transport and a manual-dispatch process runner; natural entry and recovery remain unaccepted.

---

## 1. Quick Start: Automated Acceptance Kit

To run the automated authority/API acceptance groups:

```bash
./scripts/run_acceptance_kit.sh
```

Or run via the cluster supervisor:

```bash
python3 scripts/run_cluster.py --run-acceptance
```

Expected Output:
```
===========================================================================
 ALL 7/7 FEDERATION ACCEPTANCE SCENARIOS PASSED (100%)
===========================================================================
```

---

## 2. Interactive Operator Walkthrough

For an operator inspecting the supervisor and authority against running server processes:

### Step 1: Launch Federation Cluster with Persistent Authority

Operator-only synthetic fixture: create a new `/tmp/openspace-federation-uat/`
directory first, check configured ports are unused, and preserve any existing state.
The following bounded command uses current config port38080; it does not create
the full F1 natural train/cargo/order acceptance fixture. Start the 3-node world
cluster and Universe Authority daemon:

```bash
# Terminal 1: Launch cluster supervisor with state persistence
python3 scripts/run_cluster.py \
  --config config/cluster.json \
  --state-file /tmp/openspace-federation-uat/authority_state.json \
  --duration 300
```

Verify the supervisor output:
- Universe Authority online on `http://127.0.0.1:38080`
- World 1 (Earth Core) online on port `3979`
- World 2 (Vulcan Forge) online on port `3980`
- World 3 (Haven Rim) online on port `3981`
- 4 inter-server freight corridors registered and initialized

### Step 2: Query Live World Directory & Liveliness

In another terminal, query the directory:

```bash
# Query all registered worlds
curl -s http://127.0.0.1:38080/directory | jq .

# Query developed and frontier worlds only (min_phase=2)
curl -s "http://127.0.0.1:38080/directory?min_phase=2" | jq .
```

Confirm that Earth Core (Phase 1), Vulcan Forge (Phase 2), and Haven Rim (Phase 3) report status `ONLINE` with active heartbeat timestamps.

### Step 3: Dispatch Consist Handoff & Verify Order Progression

Initiate a test transfer from World 3 to World 2 with a portable order itinerary:

```bash
curl -X POST http://127.0.0.1:38080/transfers/initiate \
  -H "Content-Type: application/json" \
  -d '{
    "source_world": 3,
    "dest_world": 2,
    "source_gate": 1,
    "dest_gate": 1,
    "total_cargo": 80,
    "cargo_breakdown": {"2": 80},
    "orders": [
      {"world_id": 2, "station_seq": 101},
      {"world_id": 1, "station_seq": 102},
      {"world_id": 3, "station_seq": 103}
    ],
    "current_order_index": 0,
    "priority": "STANDARD"
  }' | jq .
```

Take the returned `transfer_id` (e.g. `TRANSFER-X000000001`):

```bash
# Depart transfer into corridor wormhole
curl -X POST http://127.0.0.1:38080/transfers/depart \
  -H "Content-Type: application/json" \
  -d '{"transfer_id": "TRANSFER-X000000001"}' | jq .

# World 2 claims transfer
curl -X POST http://127.0.0.1:38080/transfers/claim \
  -H "Content-Type: application/json" \
  -d '{"transfer_id": "TRANSFER-X000000001", "dest_world": 2}' | jq .

# World 2 confirms arrival and materialization
curl -X POST http://127.0.0.1:38080/transfers/confirm \
  -H "Content-Type: application/json" \
  -d '{"transfer_id": "TRANSFER-X000000001", "dest_world": 2, "success": true}' | jq .
```

Observe that `current_order_index` has advanced from `0` to `1`.

### Step 4: Test Strict Content Admission & Invariant Protection

Attempt to inject an incompatible NewGRF token:

```bash
curl -X POST http://127.0.0.1:38080/transfers/initiate \
  -H "Content-Type: application/json" \
  -d '{
    "source_world": 3,
    "dest_world": 1,
    "manifest_token": "CORRUPT-MANIFEST-BAD-TOKEN",
    "total_cargo": 500
  }' | jq .
```

Verify response:
```json
{
  "error": "Content admission rejected: manifest mismatch (required: 'MANIFEST-CORE-001', got: 'CORRUPT-MANIFEST-BAD-TOKEN')"
}
```

Verify commodity audit:
```bash
curl -s http://127.0.0.1:38080/ledger/commodity-audit | jq .
```
Confirm that `total_cargo_initiated` was **not** incremented, guaranteeing zero cargo leakage.

### Step 5: Test Corridor Congestion & Priority Relief

Query corridor congestion levels:

```bash
curl -s http://127.0.0.1:38080/corridors/congestion | jq .
```

Dispatch multiple consists along Route 1 until utilization exceeds capacity.
Observe:
- Congestion transitions: `CLEAR` -> `MODERATE` -> `CONGESTED` -> `SATURATED`.
- When `SATURATED`, standard consists incur a 2.0x delay multiplier.
- Consists dispatched with `"priority": "PRIORITY_URGENT"` receive 50% congestion penalty relief (1.5x multiplier).

Confirm arrivals to relieve backpressure back to `CLEAR`.

### Step 6: Inject Destination Server Crash & Test Quarantine Bay

1. Dispatch a transfer to World 2:
   ```bash
   curl -X POST http://127.0.0.1:38080/transfers/initiate \
     -H "Content-Type: application/json" \
     -d '{"source_world": 1, "dest_world": 2, "total_cargo": 75, "cargo_breakdown": {"2": 75}}'
   ```
2. While the consist is `IN_TRANSIT`, simulate a sudden process drop on World 2:
   ```bash
   curl -X POST http://127.0.0.1:38080/transfers/quarantine \
     -H "Content-Type: application/json" \
     -d '{"world_id": 2, "reason": "Server dropped offline (SIGTERM)"}' | jq .
   ```
3. Inspect quarantine bay:
   ```bash
   curl -s http://127.0.0.1:38080/transfers/quarantined | jq .
   ```
   Confirm transfer state is `RECOVERY_REQUIRED`.
4. Trigger recovery once World 2 is restored:
   ```bash
   curl -X POST http://127.0.0.1:38080/transfers/recover \
     -H "Content-Type: application/json" \
     -d '{"world_id": 2}' | jq .
   ```
   Confirm transfer is restored to `IN_TRANSIT` with arrival timestamp reset.

### Step 7: Inject Universe Authority Crash & Verify State Persistence

1. Send a checkpoint command (or rely on automatic background save):
   ```bash
   curl -X POST http://127.0.0.1:38080/admin/state/save \
     -H "Content-Type: application/json" \
     -d '{"filepath": "demo/authority_state.json"}' | jq .
   ```
2. Terminate only the recorded Universe Authority PID from the supervisor output:
   ```bash
   kill <authority-pid>
   ```
3. Restart the daemon pointing to the checkpoint file:
   ```bash
   python3 scripts/universe_authority.py --state-file /tmp/openspace-federation-uat/authority_state.json \
  --duration 300
   ```
4. Query `/transfers/active` and `/ledger/commodity-audit`. Confirm 100% state restoration with zero loss of accounts, charters, or in-flight shipments.

### Step 8: Final Commodity Conservation Audit

Execute the final ledger audit:

```bash
curl -s http://127.0.0.1:38080/ledger/commodity-audit | jq .
curl -s http://127.0.0.1:38080/ledger/trade-balances | jq .
```

Verify that:
- `is_conserved` is `true`.
- Total cargo initiated equals total completed plus total in transit.
- Net trade credit balance across all worlds sums to exactly 0.
