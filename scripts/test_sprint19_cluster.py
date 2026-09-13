#!/usr/bin/env python3
"""
Integration test for Sprint 19: Dedicated Server Cluster Orchestration & Daemons.
Tests multi-instance server cluster lifecycle, dynamic registration, heartbeats,
corridor routing, node crash quarantine, auto-restart recovery, and strict commodity conservation.
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

from run_cluster import ClusterSupervisor

# Use dedicated test ports to avoid collisions
TEST_AUTHORITY_HOST = "127.0.0.1"
TEST_AUTHORITY_PORT = 8180
TEST_WORLD1_PORT = 14979
TEST_WORLD2_PORT = 14980
TEST_WORLD3_PORT = 14981

TEST_CONFIG_PATH = "config/test_cluster.json"

def create_test_config():
    config = {
        "cluster_name": "Test Federation Cluster",
        "version": "1.0",
        "authority": {
            "host": TEST_AUTHORITY_HOST,
            "port": TEST_AUTHORITY_PORT,
            "scheme": "http",
            "auto_spawn": True
        },
        "worlds": [
            {
                "world_id": 1,
                "name": "Earth Core Test",
                "phase": 1,
                "host": "127.0.0.1",
                "port": TEST_WORLD1_PORT,
                "max_clients": 16,
                "description": "Phase 1 Core Megacity World",
                "enabled": True
            },
            {
                "world_id": 2,
                "name": "Vulcan Forge Test",
                "phase": 2,
                "host": "127.0.0.1",
                "port": TEST_WORLD2_PORT,
                "max_clients": 16,
                "description": "Phase 2 Manufacturing Hub",
                "enabled": True
            },
            {
                "world_id": 3,
                "name": "Haven Rim Test",
                "phase": 3,
                "host": "127.0.0.1",
                "port": TEST_WORLD3_PORT,
                "max_clients": 8,
                "description": "Phase 3 Frontier Mining World",
                "enabled": True
            }
        ],
        "corridors": [
            {
                "route_id": 1,
                "name": "Haven-Vulcan Raw Ore Conduit",
                "source_world": 3,
                "source_gate": 1,
                "dest_world": 2,
                "dest_gate": 1,
                "transit_delay_sec": 1.0,
                "max_bandwidth_trains_per_min": 12,
                "max_active_in_transit": 6
            },
            {
                "route_id": 2,
                "name": "Vulcan-Earth Alloy Line",
                "source_world": 2,
                "source_gate": 1,
                "dest_world": 1,
                "dest_gate": 1,
                "transit_delay_sec": 1.0,
                "max_bandwidth_trains_per_min": 16,
                "max_active_in_transit": 8
            }
        ],
        "supervisor": {
            "binary_path": "./build/openttd",
            "heartbeat_interval_sec": 1.0,
            "health_check_interval_sec": 0.5,
            "stale_threshold_sec": 10.0,
            "quarantine_on_crash": True,
            "auto_recover_on_restart": True,
            "restart_policy": {
                "auto_restart": True,
                "max_retries": 3,
                "backoff_factor": 1.0,
                "initial_delay_sec": 0.5,
                "reset_retries_after_stable_sec": 10.0
            },
            "log_dir": "logs/test_cluster"
        }
    }
    os.makedirs("config", exist_ok=True)
    with open(TEST_CONFIG_PATH, "w") as f:
        json.dump(config, f, indent=2)

def http_get(path):
    url = f"http://{TEST_AUTHORITY_HOST}:{TEST_AUTHORITY_PORT}{path}"
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req, timeout=3.0) as resp:
        return json.loads(resp.read().decode("utf-8"))

def http_post(path, data):
    url = f"http://{TEST_AUTHORITY_HOST}:{TEST_AUTHORITY_PORT}{path}"
    payload = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=3.0) as resp:
        return json.loads(resp.read().decode("utf-8"))

def run_test():
    print("=" * 70)
    print("OpenSpaceTTD Sprint 19 Integration Test: Cluster Orchestration & Daemons")
    print("=" * 70)

    create_test_config()
    supervisor = ClusterSupervisor(config_path=TEST_CONFIG_PATH)

    try:
        # Step 1: Start Supervisor & Authority
        print("\n[Step 1] Initializing Authority and bootstrapping topology...")
        assert supervisor.ensure_authority_running(), "Failed to start Universe Authority"
        supervisor.bootstrap_topology()

        # Verify Directory
        worlds = http_get("/directory/worlds")
        assert len(worlds) == 3, f"Expected 3 worlds, found {len(worlds)}"
        world_names = {w["world_id"]: w["name"] for w in worlds}
        assert world_names[1] == "Earth Core Test"
        assert world_names[2] == "Vulcan Forge Test"
        assert world_names[3] == "Haven Rim Test"
        print("  -> Verified 3 Worlds registered in Universe Directory.")

        corridors = http_get("/corridors/list")
        assert len(corridors) == 2, f"Expected 2 corridors, found {len(corridors)}"
        print("  -> Verified 2 Inter-World Freight Corridors active.")

        # Step 2: Spawn Headless Dedicated Server Instances
        print("\n[Step 2] Launching dedicated server instances...")
        supervisor.start_servers()
        time.sleep(2.0) # Give servers time to generate map and initialize network

        for wid, rec in supervisor.server_processes.items():
            assert rec["process"].poll() is None, f"World {wid} server exited prematurely!"
            print(f"  -> World {wid} server running (PID {rec['process'].pid})")

        # Step 3: Initiate and Depart Inter-Server Consist Transfers
        print("\n[Step 3] Initiating cross-server transfers...")
        # Consist A: Haven Rim (World 3) -> Vulcan Forge (World 2), 100 units raw ore
        tx_a_data = {
            "source_world": 3,
            "dest_world": 2,
            "source_gate": 1,
            "dest_gate": 1,
            "transit_delay_sec": 1.0,
            "total_cargo": 100,
            "cargo_breakdown": {2: 100},
            "valuation_credits": 1000,
            "priority": "STANDARD"
        }
        res_a = http_post("/transfers/initiate", tx_a_data)
        tx_a = res_a["transfer_id"]
        assert http_post("/transfers/depart", {"transfer_id": tx_a}), "Failed to depart Consist A"
        print(f"  -> Consist A ({tx_a}): 100 units Ore dispatched Haven (W3) -> Vulcan (W2)")

        # Consist B: Vulcan Forge (World 2) -> Earth Core (World 1), 60 units alloys
        tx_b_data = {
            "source_world": 2,
            "dest_world": 1,
            "source_gate": 1,
            "dest_gate": 1,
            "transit_delay_sec": 1.0,
            "total_cargo": 60,
            "cargo_breakdown": {5: 60},
            "valuation_credits": 600,
            "priority": "EXPRESS"
        }
        res_b = http_post("/transfers/initiate", tx_b_data)
        tx_b = res_b["transfer_id"]
        assert http_post("/transfers/depart", {"transfer_id": tx_b}), "Failed to depart Consist B"
        print(f"  -> Consist B ({tx_b}): 60 units Goods dispatched Vulcan (W2) -> Earth Core (W1)")

        # Verify Supply Chain Matrix & Commodity Ledger
        matrix = http_get("/economy/matrix")
        assert matrix["frontier_to_refinery_cargo"] == 100, f"Expected 100, got {matrix['frontier_to_refinery_cargo']}"
        assert matrix["refinery_to_core_cargo"] == 60, f"Expected 60, got {matrix['refinery_to_core_cargo']}"
        assert matrix["total_interplanetary_cargo"] == 160
        print("  -> Verified Empire Supply Chain Matrix aggregation.")

        audit = http_get("/ledger/status")
        assert audit["total_cargo_initiated"] == 160
        assert audit["total_cargo_in_transit"] == 160
        assert audit["is_conserved"] is True, "Commodity conservation violation before crash!"
        print("  -> Verified strict commodity conservation (160 in-transit == 160 initiated).")

        # Step 4: Simulate Crash of Server 2 (Vulcan Forge) mid-transit
        print("\n[Step 4] Simulating server crash on World 2 (Vulcan Forge)...")
        w2_proc = supervisor.server_processes[2]["process"]
        w2_pid = w2_proc.pid
        print(f"  -> Sending SIGKILL to World 2 server (PID {w2_pid})...")
        try:
            if hasattr(os, 'killpg'):
                os.killpg(os.getpgid(w2_pid), signal.SIGKILL)
            else:
                w2_proc.kill()
        except Exception:
            w2_proc.kill()

        # Quarantine in-flight transfers destined for World 2
        print("  -> Quarantining in-flight transfers destined for World 2...")
        http_post("/transfers/quarantine", {"world_id": 2, "reason": "Simulated node crash"})

        # Verify Consist A is quarantined in RECOVERY_REQUIRED
        rec_a = http_get(f"/transfers/{tx_a}")
        assert rec_a["state"] == "RECOVERY_REQUIRED", f"Expected RECOVERY_REQUIRED, got {rec_a['state']}"
        print(f"  -> Consist A ({tx_a}) successfully transitioned to RECOVERY_REQUIRED quarantine status.")

        # Verify Consist B destined for World 1 is UNAFFECTED
        rec_b = http_get(f"/transfers/{tx_b}")
        assert rec_b["state"] in ("IN_TRANSIT", "ARRIVAL_PENDING"), f"Expected IN_TRANSIT, got {rec_b['state']}"
        print(f"  -> Consist B ({tx_b}) destined for World 1 remained unaffected in transit.")

        # Verify Quarantine Bay Query
        quarantined = http_get("/transfers/quarantined")
        assert len(quarantined) >= 1
        assert any(q["transfer_id"] == tx_a for q in quarantined)
        print("  -> Verified /transfers/quarantined returns quarantined consist.")

        # CRITICAL CHECK: Invariant conservation during crash/quarantine
        audit_crash = http_get("/ledger/status")
        assert audit_crash["is_conserved"] is True, "Commodity conservation violation during crash quarantine!"
        print("  -> CRITICAL INVARIANT: Commodity strictly conserved during node crash!")

        # Step 5: Supervisor detects crash, triggers quarantine, and auto-restarts
        print("\n[Step 5] Triggering Supervisor crash handling, auto-restart, and failover...")
        supervisor.handle_server_crash(2)

        # Step 6: World 2 Recovery & Consist Release
        print("\n[Step 6] Verifying server restart and quarantine release...")
        # Check that World 2 was restarted with new PID
        new_w2_proc = supervisor.server_processes[2]["process"]
        assert new_w2_proc.poll() is None, "Restarted World 2 process died!"
        assert new_w2_proc.pid != w2_pid, "World 2 PID did not change upon restart!"
        print(f"  -> World 2 server successfully running on new PID {new_w2_proc.pid}.")

        # Verify Consist A recovered back to IN_TRANSIT
        rec_a_recovered = http_get(f"/transfers/{tx_a}")
        assert rec_a_recovered["state"] == "IN_TRANSIT", f"Expected IN_TRANSIT after recovery, got {rec_a_recovered['state']}"
        print(f"  -> Consist A released from quarantine and back in transit.")

        # Step 7: Emergence and Arrival Confirmation
        print("\n[Step 7] Confirming arrival on destination servers...")
        # Claim and confirm Consist A on World 2
        claim_a = http_post("/transfers/claim", {"transfer_id": tx_a, "dest_world": 2})
        assert claim_a["state"] == "ARRIVAL_PENDING"
        confirm_a = http_post("/transfers/confirm", {"transfer_id": tx_a, "dest_world": 2, "success": True})
        assert confirm_a["state"] == "COMPLETED"
        print(f"  -> Consist A emerged and completed successfully on World 2.")

        # Claim and confirm Consist B on World 1
        claim_b = http_post("/transfers/claim", {"transfer_id": tx_b, "dest_world": 1})
        assert claim_b["state"] == "ARRIVAL_PENDING"
        confirm_b = http_post("/transfers/confirm", {"transfer_id": tx_b, "dest_world": 1, "success": True})
        assert confirm_b["state"] == "COMPLETED"
        print(f"  -> Consist B emerged and completed successfully on World 1.")

        # Step 8: Final Ledger and Trade Balances Audit
        print("\n[Step 8] Final ledger and trade balance verification...")
        final_audit = http_get("/ledger/status")
        assert final_audit["total_cargo_completed"] == 160
        assert final_audit["total_cargo_in_transit"] == 0
        assert final_audit["is_conserved"] is True
        print(f"  -> All 160 cargo units delivered with 100% commodity conservation.")

        detailed = http_get("/ledger/audit_detailed")
        assert detailed["all_conserved"] is True
        print("  -> Per-cargo-type detailed audit confirmed conserved.")

        trade = http_get("/ledger/trade_balance")
        balances = {b["world_id"]: b["net_credits"] for b in trade["world_balances"]}
        # World 3 exported 100 units -> +1000 credits
        assert balances[3] == 1000
        # World 1 imported 60 units -> -600 credits
        assert balances[1] == -600
        # World 2 exported 60 (+600) and imported 100 (-1000) -> -400 credits
        assert balances[2] == -400
        print(f"  -> Trade balances verified: W3={balances[3]} Cr, W2={balances[2]} Cr, W1={balances[1]} Cr.")

        print("\n[Step 9] Supervisor status report check...")
        supervisor.print_status()

    finally:
        print("\n[Teardown] Cleaning up cluster...")
        supervisor.shutdown()
        # Clean up test config
        if os.path.exists(TEST_CONFIG_PATH):
            os.remove(TEST_CONFIG_PATH)

    print("\n" + "=" * 70)
    print("SUCCESS: Sprint 19 Cluster Orchestration & Daemons Test PASSED!")
    print("=" * 70)

if __name__ == "__main__":
    run_test()
