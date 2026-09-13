#!/usr/bin/env python3
"""
Integration test for OpenSpaceTTD Sprint 20: Planetary Infrastructure Integration.
Tests:
  1. Spaceport hub registration & listing via Universe Authority.
  2. Spaceport off-world trade dispatch into inter-world corridors.
  3. Edge conduit feeder registration & direct mineral piping.
  4. Inter-server consist traversal & confirmation at destination world.
  5. Empire Supply Chain Matrix attribution (spaceport_throughput_cargo & edge_conduit_throughput_cargo).
  6. Strict commodity conservation invariant validation across all infrastructure channels.
"""

import json
import os
import signal
import socket
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

# Add project root and scripts dir to sys.path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

TEST_HOST = "127.0.0.1"
TEST_PORT = 38085

def find_free_port(start_port=38085):
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

def http_get(path):
    url = f"{BASE_URL}{path}"
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
    print("=" * 70)
    print("OpenSpaceTTD Sprint 20: Planetary Infrastructure Integration Test")
    print("=" * 70)

    # Step 1: Start Universe Authority on dedicated test port
    print(f"\n[Step 1] Launching Universe Authority daemon on port {PORT}...")
    proc = subprocess.Popen(
        [sys.executable, str(PROJECT_ROOT / "scripts" / "universe_authority.py"), "--host", TEST_HOST, "--port", str(PORT)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    try:
        # Wait for daemon to become responsive
        started = False
        for _ in range(30):
            try:
                resp = http_get("/directory/worlds")
                started = True
                break
            except Exception:
                time.sleep(0.1)

        assert started, f"Universe Authority failed to start on {BASE_URL}"
        print("  -> Universe Authority daemon responsive.")

        # Step 2: Register Worlds & Corridors
        print("\n[Step 2] Registering test worlds and inter-world freight corridors...")
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
            "name": "Vulcan Forge",
            "host": TEST_HOST,
            "port": 14982,
            "max_clients": 16,
            "phase": 2
        })

        http_post("/corridors/register", {
            "route_id": 1,
            "name": "Earth-Vulcan Interplanetary Gateway",
            "source_world": 1,
            "source_gate": 1,
            "dest_world": 2,
            "dest_gate": 1,
            "transit_delay_sec": 0.5,
            "max_bandwidth_trains_per_min": 20,
            "max_active_in_transit": 10
        })

        worlds = http_get("/directory/worlds")
        assert len(worlds) == 2, f"Expected 2 worlds, got {len(worlds)}"
        corridors = http_get("/corridors/list")
        assert len(corridors) == 1, f"Expected 1 corridor, got {len(corridors)}"
        print(f"  -> Topology ready: {len(worlds)} worlds, {len(corridors)} freight corridor.")

        # Step 3: Spaceport Hub Registration & Listing
        print("\n[Step 3] Registering and inspecting Spaceport Interplanetary Bridge...")
        sp_reg = http_post("/spaceport/register", {
            "world_id": 1,
            "station_id": 42,
            "name": "Armstrong Starport",
            "tier": 2,
            "target_dest_world": 2,
            "target_route_id": 1
        })
        assert sp_reg["station_id"] == 42
        assert sp_reg["target_dest_world"] == 2

        sp_list = http_get("/spaceport/list?world=1")
        assert len(sp_list) == 1, f"Expected 1 spaceport, got {len(sp_list)}"
        assert sp_list[0]["name"] == "Armstrong Starport"
        print("  -> Spaceport Armstrong Starport registered for interplanetary routing to World 2.")

        # Step 4: Spaceport Off-World Trade Dispatch
        print("\n[Step 4] Dispatching staged off-world freight via Spaceport...")
        sp_disp = http_post("/spaceport/dispatch", {
            "source_world": 1,
            "dest_world": 2,
            "station_id": 42,
            "cargo_type": 3, # Goods / Tech
            "amount": 250,
            "route_id": 1
        })
        tx_sp_id = sp_disp["transfer_id"]
        assert sp_disp["state"] == "IN_TRANSIT"
        assert sp_disp["amount"] == 250

        # Verify Spaceport statistics updated
        sp_list = http_get("/spaceport/list?world=1")
        assert sp_list[0]["total_dispatched"] == 250, f"Expected 250 dispatched, got {sp_list[0]['total_dispatched']}"

        # Verify Transfer record metadata
        tx_rec = http_get(f"/transfers/{tx_sp_id}")
        assert tx_rec["source_infrastructure"] == "SPACEPORT"
        assert tx_rec["source_station_id"] == 42
        assert tx_rec["state"] == "IN_TRANSIT"
        print(f"  -> Spaceport dispatch confirmed: tx {tx_sp_id}, 250 units Goods in transit.")

        # Step 5: Edge Conduit Feeder Registration & Direct Piping
        print("\n[Step 5] Registering Edge Conduit and piping minerals directly off-world...")
        cond_reg = http_post("/conduit/register", {
            "world_id": 1,
            "conduit_id": 7,
            "cargo_type": 2, # Raw Ore / Minerals
            "target_dest_world": 2,
            "target_route_id": 1,
            "production_rate": 120
        })
        assert cond_reg["conduit_id"] == 7

        cond_list = http_get("/conduit/list?world=1")
        assert len(cond_list) == 1, f"Expected 1 conduit, got {len(cond_list)}"

        cond_pipe = http_post("/conduit/pipe", {
            "source_world": 1,
            "dest_world": 2,
            "conduit_id": 7,
            "cargo_type": 2,
            "amount": 120,
            "route_id": 1
        })
        tx_cond_id = cond_pipe["transfer_id"]
        assert cond_pipe["state"] == "IN_TRANSIT"
        assert cond_pipe["amount"] == 120

        # Verify Conduit statistics updated
        cond_list = http_get("/conduit/list?world=1")
        assert cond_list[0]["total_piped"] == 120, f"Expected 120 piped, got {cond_list[0]['total_piped']}"

        tx_rec = http_get(f"/transfers/{tx_cond_id}")
        assert tx_rec["source_infrastructure"] == "EDGE_CONDUIT"
        assert tx_rec["source_conduit_id"] == 7
        print(f"  -> Edge Conduit piping confirmed: tx {tx_cond_id}, 120 units Ore in transit.")

        # Step 6: Verify In-Flight Commodity Conservation
        print("\n[Step 6] Validating commodity conservation with active in-flight freight...")
        cons_inflight = http_get("/economy/conservation")
        assert cons_inflight["all_conserved"], f"In-flight conservation violated: {cons_inflight}"
        assert cons_inflight["total_commodities_tracked"] >= 2
        print("  -> Commodity conservation strictly preserved with transfers in flight.")

        # Step 7: Complete Inter-World Consist Traversal & Arrival at Vulcan Forge
        print("\n[Step 7] Processing consist arrival and delivery at destination world...")
        time.sleep(0.6) # Allow transit delay to elapse

        claim_sp = http_post("/transfers/claim", {"transfer_id": tx_sp_id, "dest_world": 2})
        assert claim_sp["state"] == "ARRIVAL_PENDING"
        conf_sp = http_post("/transfers/confirm", {"transfer_id": tx_sp_id, "dest_world": 2, "success": True})
        assert conf_sp["state"] == "COMPLETED"

        claim_cond = http_post("/transfers/claim", {"transfer_id": tx_cond_id, "dest_world": 2})
        assert claim_cond["state"] == "ARRIVAL_PENDING"
        conf_cond = http_post("/transfers/confirm", {"transfer_id": tx_cond_id, "dest_world": 2, "success": True})
        assert conf_cond["state"] == "COMPLETED"
        print("  -> Both Spaceport and Conduit consists arrived and completed delivery on World 2.")

        # Step 8: Validate Empire Supply Chain Matrix Attribution
        print("\n[Step 8] Checking Empire Supply Chain Matrix throughput attributions...")
        matrix = http_get("/economy/matrix")
        assert matrix["spaceport_throughput_cargo"] == 250, (
            f"Expected spaceport_throughput_cargo == 250, got {matrix.get('spaceport_throughput_cargo')}"
        )
        assert matrix["edge_conduit_throughput_cargo"] == 120, (
            f"Expected edge_conduit_throughput_cargo == 120, got {matrix.get('edge_conduit_throughput_cargo')}"
        )
        print(f"  -> Supply Chain Matrix: spaceport throughput = {matrix['spaceport_throughput_cargo']}, "
              f"conduit throughput = {matrix['edge_conduit_throughput_cargo']}.")

        # Step 9: Final Commodity Conservation Audit
        print("\n[Step 9] Auditing final commodity conservation across all worlds...")
        cons_final = http_get("/economy/conservation")
        assert cons_final["all_conserved"], f"Final conservation violated: {cons_final}"
        for report in cons_final["commodity_reports"]:
            if report["cargo_type"] == 3:
                assert report["initiated"] == 250
                assert report["in_transit"] == 0
                assert report["completed"] == 250
                assert report["conserved"]
            elif report["cargo_type"] == 2:
                assert report["initiated"] == 120
                assert report["in_transit"] == 0
                assert report["completed"] == 120
                assert report["conserved"]
        print("  -> Commodity conservation 100% verified across all commodities.")

        print("\n" + "=" * 70)
        print("ALL SPRINT 20 PLANETARY INFRASTRUCTURE INTEGRATION TESTS PASSED!")
        print("=" * 70)

    finally:
        print("\nStopping Universe Authority...")
        proc.terminate()
        try:
            proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

if __name__ == "__main__":
    run_test()
