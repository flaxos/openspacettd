#!/usr/bin/env python3
"""
Integration test for OpenSpaceTTD Sprint 21: High-Capacity Gateway Arrays & Consist Telemetry.
Tests:
  1. Registering standard vs Twin Gateway Array freight corridors.
  2. Verifying 2x bandwidth and capacity multiplier on twin gateway arrays.
  3. Verifying 50% congestion penalty relief on twin gateway corridors.
  4. Querying live in-transit consist telemetry (GET /corridors/telemetry).
  5. Verifying route-specific filtering and all-corridor aggregation.
  6. Lifecycle progression from in-transit to delivery and automatic telemetry clearing.
  7. Ledger commodity conservation invariant preservation.
"""

import json
import os
import signal
import socket
import subprocess
import sys
import time
import urllib.request
from urllib.parse import urlencode
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

TEST_HOST = "127.0.0.1"
TEST_PORT = 38090

def find_free_port(start_port=38090):
    port = start_port
    while port < 65535:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((TEST_HOST, port))
                return port
            except OSError:
                port += 1
    return start_port

PORT = find_free_port(TEST_PORT)
BASE_URL = f"http://{TEST_HOST}:{PORT}"

def http_get(path, query=None):
    url = f"{BASE_URL}{path}"
    if query:
        url += f"?{urlencode(query)}"
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req, timeout=3.0) as resp:
        return json.loads(resp.read().decode("utf-8"))

def http_post(path, data):
    url = f"{BASE_URL}{path}"
    payload = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=3.0) as resp:
        return json.loads(resp.read().decode("utf-8"))

def run_test():
    print("=" * 75)
    print("OpenSpaceTTD Sprint 21: Gateway Arrays & Consist Telemetry Integration Test")
    print("=" * 75)

    print(f"\n[Step 1] Starting Universe Authority daemon on {BASE_URL}...")
    proc = subprocess.Popen(
        [sys.executable, str(PROJECT_ROOT / "scripts" / "universe_authority.py"), "--host", TEST_HOST, "--port", str(PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    try:
        # Wait for daemon
        started = False
        for _ in range(30):
            try:
                http_get("/directory/worlds")
                started = True
                break
            except Exception:
                time.sleep(0.1)

        assert started, f"Universe Authority failed to start on {BASE_URL}"
        print("  -> Daemon online and responsive.")

        # Step 2: Register worlds
        print("\n[Step 2] Registering test worlds...")
        http_post("/directory/register", {
            "world_id": 1,
            "name": "Earth Core",
            "host": TEST_HOST,
            "port": 14981,
            "max_clients": 16,
            "phase": 1
        })
        http_post("/directory/register", {
            "world_id": 2,
            "name": "Pelican Mining",
            "host": TEST_HOST,
            "port": 14982,
            "max_clients": 16,
            "phase": 3
        })
        worlds = http_get("/directory/worlds")
        assert len(worlds) >= 2, "Failed to register worlds"
        print("  -> Worlds registered successfully.")

        # Step 3: Register Corridors (Single vs Twin Array)
        print("\n[Step 3] Registering standard and twin gateway array corridors...")
        # Standard corridor (route 101)
        r1_res = http_post("/corridors/register", {
            "route_id": 101,
            "source_world": 1,
            "source_gate": 11,
            "dest_world": 2,
            "dest_gate": 22,
            "transit_delay_sec": 4.0,
            "max_bandwidth_trains_per_min": 10,
            "max_active_in_transit": 2,
            "is_twin_array": False
        })
        assert r1_res.get("is_twin_array") is False
        assert r1_res.get("max_bandwidth_trains_per_min") == 10

        # Twin Array corridor (route 102)
        r2_res = http_post("/corridors/register", {
            "route_id": 102,
            "source_world": 1,
            "source_gate": 33,
            "dest_world": 2,
            "dest_gate": 44,
            "transit_delay_sec": 4.0,
            "max_bandwidth_trains_per_min": 10,
            "max_active_in_transit": 2,
            "is_twin_array": True
        })
        assert r2_res.get("is_twin_array") is True
        # 2x bandwidth applied
        assert r2_res.get("max_bandwidth_trains_per_min") == 20, f"Expected 20 bandwidth, got {r2_res.get('max_bandwidth_trains_per_min')}"
        print(f"  -> Route 101 (Standard): bandwidth = {r1_res['max_bandwidth_trains_per_min']}, is_twin = {r1_res['is_twin_array']}")
        print(f"  -> Route 102 (Twin Array): bandwidth = {r2_res['max_bandwidth_trains_per_min']}, is_twin = {r2_res['is_twin_array']}")

        # Step 4: Test congestion and twin array capacity doubling
        print("\n[Step 4] Dispatching transfers to test twin array capacity and congestion mitigation...")
        # Dispatch 1 train on route 101 (util = 1/2 = 50% -> MODERATE)
        tx1 = http_post("/transfers/initiate", {
            "source_world": 1,
            "dest_world": 2,
            "source_gate": 11,
            "dest_gate": 22,
            "total_cargo": 150,
            "cargo_breakdown": {"1": 150},
            "priority": "STANDARD"
        })
        assert tx1.get("transfer_id")
        tid1 = tx1["transfer_id"]
        # In route 101: 50% util -> Moderate (1.2x delay: 4.0 * 1.2 = 4.8s)
        assert abs(tx1["transit_delay_sec"] - 4.8) < 0.01, f"Expected 4.8s, got {tx1['transit_delay_sec']}"

        # Dispatch 1 train on route 102 (util = 1/4 = 25% due to 2x capacity -> CLEAR)
        tx2 = http_post("/transfers/initiate", {
            "source_world": 1,
            "dest_world": 2,
            "source_gate": 33,
            "dest_gate": 44,
            "total_cargo": 200,
            "cargo_breakdown": {"2": 200},
            "priority": "STANDARD"
        })
        assert tx2.get("transfer_id")
        tid2 = tx2["transfer_id"]
        # In route 102: 25% util -> Clear (1.0x delay: 4.0s)
        assert abs(tx2["transit_delay_sec"] - 4.0) < 0.01, f"Expected 4.0s, got {tx2['transit_delay_sec']}"
        print(f"  -> Standard corridor delay under 1 train: {tx1['transit_delay_sec']}s (Moderate 1.2x)")
        print(f"  -> Twin array corridor delay under 1 train: {tx2['transit_delay_sec']}s (Clear 1.0x)")

        # Dispatch 2 more trains on route 102 (util = 3/4 = 75% -> Moderate, penalty 0.2 halved to 0.1 -> 1.1x = 4.4s)
        http_post("/transfers/initiate", {
            "source_world": 1,
            "dest_world": 2,
            "source_gate": 33,
            "dest_gate": 44,
            "total_cargo": 100,
            "priority": "STANDARD"
        })
        tx3 = http_post("/transfers/initiate", {
            "source_world": 1,
            "dest_world": 2,
            "source_gate": 33,
            "dest_gate": 44,
            "total_cargo": 100,
            "priority": "STANDARD"
        })
        tid3 = tx3["transfer_id"]
        assert abs(tx3["transit_delay_sec"] - 4.4) < 0.01, f"Expected 4.4s (50% mitigated), got {tx3['transit_delay_sec']}"
        print(f"  -> Twin array corridor delay under 3 trains: {tx3['transit_delay_sec']}s (50% congestion mitigated 1.1x)")

        # Step 5: Live In-Transit Consist Telemetry
        print("\n[Step 5] Querying live in-transit telemetry (/corridors/telemetry)...")
        # Route 101 telemetry
        telem101 = http_get("/corridors/telemetry", {"route_id": 101})
        assert len(telem101) == 1, f"Expected 1 in-transit consist on route 101, got {len(telem101)}"
        assert telem101[0]["transfer_id"] == tid1
        assert telem101[0]["route_id"] == 101
        assert telem101[0]["total_cargo"] == 150
        assert telem101[0]["priority"] == "STANDARD"
        assert telem101[0]["remaining_eta_sec"] > 0
        print(f"  -> Route 101 Telemetry: Consist {telem101[0]['transfer_id']}, Cargo: {telem101[0]['total_cargo']}, ETA: {telem101[0]['remaining_eta_sec']}s")

        # Route 102 telemetry
        telem102 = http_get("/corridors/telemetry", {"route_id": 102})
        assert len(telem102) == 3, f"Expected 3 in-transit consists on route 102, got {len(telem102)}"
        tids_102 = [t["transfer_id"] for t in telem102]
        assert tid2 in tids_102
        assert tid3 in tids_102
        print(f"  -> Route 102 Telemetry: {len(telem102)} consists active in transit")

        # All corridors telemetry
        telem_all = http_get("/corridors/telemetry")
        assert len(telem_all) == 4, f"Expected 4 consists total, got {len(telem_all)}"
        print(f"  -> Aggregate Telemetry: {len(telem_all)} total consists active across federation")

        # Step 6: Consist delivery lifecycle & telemetry cleanup
        print("\n[Step 6] Progressing consist delivery and verifying telemetry pruning...")
        http_post("/transfers/depart", {"transfer_id": tid1, "world_id": 1})
        http_post("/transfers/claim", {"transfer_id": tid1, "world_id": 2})
        confirm_res = http_post("/transfers/confirm", {"transfer_id": tid1, "world_id": 2, "success": True})
        assert confirm_res.get("state") == "COMPLETED"

        # Check route 101 telemetry now empty
        telem101_after = http_get("/corridors/telemetry", {"route_id": 101})
        assert len(telem101_after) == 0, f"Expected 0 active consists on route 101 after delivery, got {len(telem101_after)}"
        print("  -> Route 101 telemetry cleared on consist arrival.")

        # Step 7: Commodity conservation check
        print("\n[Step 7] Checking ledger commodity conservation...")
        audit = http_get("/ledger/status")
        total_init = audit.get("total_cargo_initiated", 0)
        total_in_transit = audit.get("total_cargo_in_transit", 0)
        total_completed = audit.get("total_cargo_completed", 0)
        print(f"  -> Initiated: {total_init} | In-Transit: {total_in_transit} | Completed: {total_completed}")
        assert total_init == total_in_transit + total_completed, "Commodity conservation invariant violated!"
        print("  -> Commodity conservation strictly preserved.")

        print("\n" + "=" * 75)
        print("SUCCESS: All Sprint 21 Telemetry & Navigation integration checks PASSED!")
        print("=" * 75)

    finally:
        print("\nStopping Universe Authority daemon...")
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("Daemon stopped.")

if __name__ == "__main__":
    run_test()
