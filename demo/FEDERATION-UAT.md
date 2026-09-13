# Sprint 29 Federation UAT: Live Cluster Acceptance Guide

**Milestone:** Sprint 29 (Federation Acceptance Kit)  
**Target:** Live Multi-Server Commonwealth Cluster  
**Supervisors & Runners:** `scripts/run_acceptance_kit.sh`, `scripts/run_cluster.py`  

---

## 1. Quick Start: Automated Acceptance Kit

To run the complete automated federation acceptance kit across all 8 scenarios:

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

For an operator performing live acceptance testing against a running cluster:

### Step 1: Launch Federation Cluster with Persistent Authority

Start the 3-node world cluster and Universe Authority daemon:

```bash
# Terminal 1: Launch cluster supervisor with state persistence
python3 scripts/run_cluster.py \
  --config config/cluster.json \
  --state-file demo/authority_state.json
```

Verify the supervisor output:
- Universe Authority online on `http://127.0.0.1:8080`
- World 1 (Earth Core) online on port `3979`
- World 2 (Vulcan Forge) online on port `3980`
- World 3 (Haven Rim) online on port `3981`
- 4 inter-server freight corridors registered and initialized

### Step 2: Query Live World Directory & Liveliness

In another terminal, query the directory:

```bash
# Query all registered worlds
curl -s http://127.0.0.1:8080/directory | jq .

# Query developed and frontier worlds only (min_phase=2)
curl -s "http://127.0.0.1:8080/directory?min_phase=2" | jq .
```

Confirm that Earth Core (Phase 1), Vulcan Forge (Phase 2), and Haven Rim (Phase 3) report status `ONLINE` with active heartbeat timestamps.

### Step 3: Dispatch Consist Handoff & Verify Order Progression

Initiate a test transfer from World 3 to World 2 with a portable order itinerary:

```bash
curl -X POST http://127.0.0.1:8080/transfers/initiate \
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
curl -X POST http://127.0.0.1:8080/transfers/depart \
  -H "Content-Type: application/json" \
  -d '{"transfer_id": "TRANSFER-X000000001"}' | jq .

# World 2 claims transfer
curl -X POST http://127.0.0.1:8080/transfers/claim \
  -H "Content-Type: application/json" \
  -d '{"transfer_id": "TRANSFER-X000000001", "dest_world": 2}' | jq .

# World 2 confirms arrival and materialization
curl -X POST http://127.0.0.1:8080/transfers/confirm \
  -H "Content-Type: application/json" \
  -d '{"transfer_id": "TRANSFER-X000000001", "dest_world": 2, "success": true}' | jq .
```

Observe that `current_order_index` has advanced from `0` to `1`.

### Step 4: Test Strict Content Admission & Invariant Protection

Attempt to inject an incompatible NewGRF token:

```bash
curl -X POST http://127.0.0.1:8080/transfers/initiate \
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
curl -s http://127.0.0.1:8080/ledger/commodity-audit | jq .
```
Confirm that `total_cargo_initiated` was **not** incremented, guaranteeing zero cargo leakage.

### Step 5: Test Corridor Congestion & Priority Relief

Query corridor congestion levels:

```bash
curl -s http://127.0.0.1:8080/corridors/congestion | jq .
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
   curl -X POST http://127.0.0.1:8080/transfers/initiate \
     -H "Content-Type: application/json" \
     -d '{"source_world": 1, "dest_world": 2, "total_cargo": 75, "cargo_breakdown": {"2": 75}}'
   ```
2. While the consist is `IN_TRANSIT`, simulate a sudden process drop on World 2:
   ```bash
   curl -X POST http://127.0.0.1:8080/transfers/quarantine \
     -H "Content-Type: application/json" \
     -d '{"world_id": 2, "reason": "Server dropped offline (SIGTERM)"}' | jq .
   ```
3. Inspect quarantine bay:
   ```bash
   curl -s http://127.0.0.1:8080/transfers/quarantined | jq .
   ```
   Confirm transfer state is `RECOVERY_REQUIRED`.
4. Trigger recovery once World 2 is restored:
   ```bash
   curl -X POST http://127.0.0.1:8080/transfers/recover \
     -H "Content-Type: application/json" \
     -d '{"world_id": 2}' | jq .
   ```
   Confirm transfer is restored to `IN_TRANSIT` with arrival timestamp reset.

### Step 7: Inject Universe Authority Crash & Verify State Persistence

1. Send a checkpoint command (or rely on automatic background save):
   ```bash
   curl -X POST http://127.0.0.1:8080/admin/state/save \
     -H "Content-Type: application/json" \
     -d '{"filepath": "demo/authority_state.json"}' | jq .
   ```
2. Kill the running authority process:
   ```bash
   killall python3 # or terminate the specific authority PID
   ```
3. Restart the daemon pointing to the checkpoint file:
   ```bash
   python3 scripts/universe_authority.py --state-file demo/authority_state.json
   ```
4. Query `/transfers/active` and `/ledger/commodity-audit`. Confirm 100% state restoration with zero loss of accounts, charters, or in-flight shipments.

### Step 8: Final Commodity Conservation Audit

Execute the final ledger audit:

```bash
curl -s http://127.0.0.1:8080/ledger/commodity-audit | jq .
curl -s http://127.0.0.1:8080/ledger/trade-balances | jq .
```

Verify that:
- `is_conserved` is `true`.
- Total cargo initiated equals total completed plus total in transit.
- Net trade credit balance across all worlds sums to exactly 0.
