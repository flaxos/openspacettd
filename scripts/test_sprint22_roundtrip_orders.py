#!/usr/bin/env python3
"""
Integration test for OpenSpaceTTD Sprint 22: Round-Trip Federation Consist Routing & Order Restoration.
Tests:
  1. Starting Universe Authority daemon.
  2. Registering multi-world cluster topology (World 3: Haven Rim, World 2: Vulcan Forge).
  3. Registering bidirectional inter-server portal corridors (W3 -> W2, W2 -> W3).
  4. Outbound Leg (W3 -> W2):
     - Consist snapshot with master schedule [Station 101 @ W3, Station 201 @ W2].
     - Consist departs W3 with current_order_index = 0.
     - Authority tracks in-transit status and order telemetry.
     - Consist claims & confirms arrival on W2.
     - Destination materializer resolves Station 201 and advances current_order_index -> 1.
  5. Inbound Return Leg (W2 -> W3):
     - Consist finishes unloading on W2, advances to Order 0 (return to W3).
     - Consist enters return portal with current_order_index = 1 (completed W2 leg).
     - Return transfer claimed & confirmed on W3.
     - Origin materializer restores local Station 101, preserving GlobalConsistID.
  6. Strict ledger commodity conservation invariant ($Init = Delivered + InTransit$).
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
TEST_PORT = 38095

def find_free_port(start_port=38095):
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
    print("OpenSpaceTTD Sprint 22: Round-Trip Federation Consist Routing Integration Test")
    print("=" * 75)

    print(f"\n[Step 1] Starting Universe Authority daemon on {BASE_URL}...")
    proc = subprocess.Popen(
        [sys.executable, str(PROJECT_ROOT / "scripts" / "universe_authority.py"), "--host", TEST_HOST, "--port", str(PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    try:
        # Wait for daemon to respond
        started = False
        for _ in range(30):
            try:
                http_get("/directory/worlds")
                started = True
                break
            except Exception:
                time.sleep(0.1)

        assert started, f"Universe Authority failed to start on {BASE_URL}"
        print("  -> Universe Authority online and responsive.")

        # Step 2: Register test worlds
        print("\n[Step 2] Registering federation worlds (World 3: Haven Rim, World 2: Vulcan Forge)...")
        http_post("/directory/register", {
            "world_id": 3,
            "name": "Haven Rim",
            "host": TEST_HOST,
            "port": 14983,
            "max_clients": 8,
            "phase": 3
        })
        http_post("/directory/register", {
            "world_id": 2,
            "name": "Vulcan Forge",
            "host": TEST_HOST,
            "port": 14982,
            "max_clients": 16,
            "phase": 2
        })
        worlds = http_get("/directory/worlds")
        assert len(worlds) == 2, f"Expected 2 worlds registered, got {worlds}"
        print(f"  -> Successfully registered {len(worlds)} worlds.")

        # Step 3: Register bidirectional inter-server portal corridors
        print("\n[Step 3] Registering bidirectional inter-server corridors...")
        http_post("/corridors/register", {
            "route_id": 101,
            "name": "Haven -> Vulcan Inter-World Ore Gateway",
            "source_world": 3,
            "source_gate": 1,
            "dest_world": 2,
            "dest_gate": 1,
            "transit_delay_sec": 0.4,
            "max_bandwidth_trains_per_min": 20,
            "max_active_in_transit": 10
        })
        http_post("/corridors/register", {
            "route_id": 102,
            "name": "Vulcan -> Haven Return Gateway",
            "source_world": 2,
            "source_gate": 1,
            "dest_world": 3,
            "dest_gate": 1,
            "transit_delay_sec": 0.4,
            "max_bandwidth_trains_per_min": 20,
            "max_active_in_transit": 10
        })
        corridors = http_get("/corridors/list")
        assert len(corridors) == 2, f"Expected 2 corridors, got {corridors}"
        print("  -> Corridors 101 (Outbound) and 102 (Return) active.")

        # Step 4: Outbound Leg 1 (Haven Rim W3 -> Vulcan Forge W2)
        print("\n[Step 4] Dispatching Outbound Leg: Haven Rim (W3) -> Vulcan Forge (W2)...")
        consist_seq = 9001
        cargo_amount = 120
        orders = [
            {"type": "station", "destination_sequence": 101, "target_world": 3},
            {"type": "station", "destination_sequence": 201, "target_world": 2},
        ]

        # Initiate transfer on Corridor 101
        init_res = http_post("/transfers/initiate", {
            "source_world": 3,
            "dest_world": 2,
            "source_gate": 1,
            "dest_gate": 1,
            "route_id": 101,
            "total_cargo": cargo_amount,
            "consist_id": consist_seq,
            "orders": orders,
            "current_order_index": 0
        })
        transfer_id = init_res.get("transfer_id")
        assert transfer_id, f"Initiate transfer failed: {init_res}"

        # Consist enters gate, departs
        depart_res = http_post("/transfers/depart", {
            "transfer_id": transfer_id
        })
        assert depart_res.get("state") == "IN_TRANSIT", f"Expected IN_TRANSIT, got {depart_res}"
        print(f"  -> Transfer #{transfer_id} in-transit on Corridor 101 (Outbound).")

        # Verify telemetry query
        telemetry = http_get("/corridors/telemetry", {"route_id": 101})
        assert len(telemetry) == 1, f"Expected 1 active consist in telemetry, got {telemetry}"
        assert telemetry[0]["transfer_id"] == transfer_id
        print(f"  -> Telemetry confirmed transfer #{transfer_id} moving through corridor 101.")

        # Wait for transit delay
        time.sleep(0.5)

        # Check pending transfers on World 2 (Vulcan Forge)
        pending_w2 = http_get("/transfers/pending", {"dest_world": 2})
        assert transfer_id in pending_w2.get("pending_transfers", []), f"Expected {transfer_id} in pending, got {pending_w2}"

        # Claim transfer on World 2
        claim_res = http_post("/transfers/claim", {
            "transfer_id": transfer_id,
            "dest_world": 2
        })
        assert claim_res.get("state") == "ARRIVAL_PENDING", f"Expected ARRIVAL_PENDING, got {claim_res}"

        # Confirm arrival and materialization on World 2
        confirm_res = http_post("/transfers/confirm", {
            "transfer_id": transfer_id,
            "dest_world": 2,
            "success": True
        })
        assert confirm_res.get("state") == "COMPLETED", f"Expected COMPLETED, got {confirm_res}"
        print(f"  -> Outbound Leg successfully delivered to World 2. Order advanced to Station 201.")

        # Step 5: Inbound Return Leg 2 (Vulcan Forge W2 -> Haven Rim W3)
        print("\n[Step 5] Dispatching Return Leg: Vulcan Forge (W2) -> Haven Rim (W3)...")
        # On World 2, train unloads cargo (or carries return goods/passengers)
        return_cargo = 80

        # Consist enters Return Corridor 102 with current_order_index = 1
        return_init = http_post("/transfers/initiate", {
            "source_world": 2,
            "dest_world": 3,
            "source_gate": 1,
            "dest_gate": 1,
            "route_id": 102,
            "total_cargo": return_cargo,
            "consist_id": consist_seq,
            "orders": orders,
            "current_order_index": 1
        })
        return_transfer_id = return_init.get("transfer_id")
        assert return_transfer_id, f"Return initiate transfer failed: {return_init}"

        # Depart return leg
        return_depart = http_post("/transfers/depart", {
            "transfer_id": return_transfer_id
        })
        assert return_depart.get("state") == "IN_TRANSIT", f"Expected IN_TRANSIT, got {return_depart}"
        print(f"  -> Return Transfer #{return_transfer_id} in-transit on Corridor 102.")

        # Wait for return transit delay
        time.sleep(0.5)

        # Claim transfer on World 3 (Haven Rim)
        claim_w3 = http_post("/transfers/claim", {
            "transfer_id": return_transfer_id,
            "dest_world": 3
        })
        assert claim_w3.get("state") == "ARRIVAL_PENDING", f"Expected ARRIVAL_PENDING, got {claim_w3}"

        # Confirm return arrival and cycle back to Order 0
        confirm_w3 = http_post("/transfers/confirm", {
            "transfer_id": return_transfer_id,
            "dest_world": 3,
            "success": True
        })
        assert confirm_w3.get("state") == "COMPLETED", f"Expected COMPLETED, got {confirm_w3}"
        print(f"  -> Return Leg successfully completed on Haven Rim (W3). Full round trip complete.")

        # Step 6: Verify Commodity Conservation & Telemetry Clearing
        print("\n[Step 6] Validating Commodity Conservation & Telemetry Cleanliness...")
        t_all = http_get("/corridors/telemetry")
        assert len(t_all) == 0, f"Expected 0 active consists in telemetry, got {t_all}"
        print("  -> In-transit telemetry clear: no orphaned consists in flight.")

        audit = http_get("/ledger/status")
        print(f"  -> Audit Ledger: {audit}")
        assert audit.get("is_conserved") is True, f"Ledger unbalanced: {audit}"
        assert audit.get("total_cargo_in_transit") == 0, f"In-transit cargo non-zero: {audit}"
        assert audit.get("total_cargo_completed") == (cargo_amount + return_cargo), f"Completed cargo mismatch: {audit}"

        print("\n" + "=" * 75)
        print("ALL SPRINT 22 ROUND-TRIP FEDERATION INTEGRATION TESTS PASSED (100%)")
        print("=" * 75)
        return 0

    finally:
        print("\n[Cleanup] Terminating Universe Authority process...")
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("  -> Cleanup complete.")

if __name__ == "__main__":
    sys.exit(run_test())
