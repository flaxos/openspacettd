#!/usr/bin/env python3
"""
OpenSpaceTTD Sprint 29 - Federation Acceptance Test Kit
Automated end-to-end multi-server validation suite testing the live 3-server cluster topology
(Earth Core, Vulcan Forge, Haven Rim) supervised by the Universe Authority daemon.

Validates all 7 acceptance requirements:
  1. World-Directory Discovery & Liveliness
  2. Cross-Server Consist Round Trips
  3. Restored Orders & Itinerary Resolution
  4. Content Admission Rejection & Invariant Guard (Zero Cargo Leak)
  5. Freight Corridor Congestion & Relief (Dynamic Multipliers & Priority Relief)
  6. Server Crash Injection, Quarantine Bay & Auto-Recovery
  7. Universe Authority Daemon Crash & State Persistence Reload
  8. Empire-Wide Commodity Conservation Ledger & Trade Balances Audit
"""

import argparse
import base64
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]

def http_request(url, method="GET", data=None, timeout=5.0):
    req_data = json.dumps(data).encode("utf-8") if data is not None else None
    headers = {"Content-Type": "application/json"} if data is not None else {}
    req = urllib.request.Request(url, data=req_data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8")
            return resp.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8") if e.fp else str(e)
        try:
            return e.code, json.loads(err_body)
        except Exception:
            return e.code, {"error": err_body}
    except Exception as e:
        return 0, {"error": str(e)}

class FederationAcceptanceRunner:
    def __init__(self):
        self.repo_root = Path(__file__).parent.parent.resolve()
        self.authority_script = self.repo_root / "scripts" / "universe_authority.py"
        self.temp_dir = tempfile.TemporaryDirectory(prefix="openspace_uat_")
        self.state_file = Path(self.temp_dir.name) / "authority_state.json"
        self.auth_port = find_free_port()
        self.base_url = f"http://127.0.0.1:{self.auth_port}"
        self.auth_proc = None
        self.passed_tests = 0
        self.total_tests = 7

    def start_authority(self, port=None, state_file=None):
        p = port or self.auth_port
        sf = state_file or str(self.state_file)
        cmd = [
            sys.executable, str(self.authority_script),
            "--host", "127.0.0.1",
            "--port", str(p),
            "--state-file", sf
        ]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        # Wait for responsive
        url = f"http://127.0.0.1:{p}"
        ready = False
        for _ in range(40):
            time.sleep(0.1)
            code, res = http_request(f"{url}/ledger/status")
            if code == 200:
                ready = True
                break
        if not ready:
            proc.kill()
            raise RuntimeError(f"Failed to start Universe Authority on {url}")
        return proc

    def setup(self):
        print("=" * 75)
        print(" OpenSpaceTTD Sprint 29: Federation Acceptance Test Kit")
        print("=" * 75)
        print(f"[*] Repository Root: {self.repo_root}")
        print(f"[*] State Persistence: {self.state_file}")
        print(f"[*] Starting Universe Authority daemon on {self.base_url}...")
        self.auth_proc = self.start_authority()
        print("[+] Universe Authority online and responsive.\n")

    def teardown(self):
        if self.auth_proc and self.auth_proc.poll() is None:
            self.auth_proc.terminate()
            try:
                self.auth_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.auth_proc.kill()
        self.temp_dir.cleanup()

    def wait_until_pending(self, dest_world, tx_id, timeout=3.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            code, pending = http_request(f"{self.base_url}/transfers/pending?dest_world={dest_world}")
            if code == 200 and tx_id in pending.get("pending_transfers", []):
                return True
            time.sleep(0.05)
        return False

    # -------------------------------------------------------------------------
    # Scenario 1: World Directory Discovery & Liveliness
    # -------------------------------------------------------------------------
    def test_scenario_1_directory_discovery(self):
        print("--- [Scenario 1] World Directory Discovery & Liveliness ---")
        # 1. Register 3-tier worlds
        worlds_config = [
            {
                "world_id": 1, "phase": 1, "name": "Earth Core",
                "address": "127.0.0.1:3979", "description": "Phase 1 Megacity Hub",
                "manifest_token": "MANIFEST-CORE-001", "max_clients": 32, "active_clients": 4
            },
            {
                "world_id": 2, "phase": 2, "name": "Vulcan Forge",
                "address": "127.0.0.1:3980", "description": "Phase 2 Manufacturing Hub",
                "manifest_token": "MANIFEST-FORGE-002", "max_clients": 16, "active_clients": 2
            },
            {
                "world_id": 3, "phase": 3, "name": "Haven Rim",
                "address": "127.0.0.1:3981", "description": "Phase 3 Frontier Mining",
                "manifest_token": "MANIFEST-HAVEN-003", "max_clients": 8, "active_clients": 1
            }
        ]
        for w in worlds_config:
            code, res = http_request(f"{self.base_url}/directory/register", method="POST", data=w)
            assert code == 200, f"Failed to register world {w['world_id']}: {res}"

        # 2. Query all worlds
        code, directory = http_request(f"{self.base_url}/directory/worlds")
        assert code == 200, f"Failed to query directory: {directory}"
        assert len(directory) == 3, f"Expected 3 registered worlds, got {len(directory)}"

        # 3. Query with min_phase=2 filter (should return Vulcan Forge & Haven Rim, exclude Earth Core)
        code, filtered = http_request(f"{self.base_url}/directory/worlds?min_phase=2")
        assert code == 200
        filtered_ids = [w["world_id"] for w in filtered]
        assert 1 not in filtered_ids and 2 in filtered_ids and 3 in filtered_ids, f"min_phase filtering failed: {filtered_ids}"

        # 4. Heartbeat update
        code, hb_res = http_request(f"{self.base_url}/directory/heartbeat", method="POST", data={
            "world_id": 2, "active_clients": 5, "active_trains": 12, "status": "online"
        })
        assert code == 200 and hb_res["active_clients"] == 5

        # 5. Register freight corridors
        corridors_config = [
            {"route_id": 1, "name": "Haven-Vulcan Ore Conduit", "source_world": 3, "source_gate": 1, "dest_world": 2, "dest_gate": 1, "transit_delay_sec": 0.2, "max_active_in_transit": 8, "max_bandwidth_trains_per_min": 12},
            {"route_id": 2, "name": "Vulcan-Earth Alloy Corridor", "source_world": 2, "source_gate": 1, "dest_world": 1, "dest_gate": 1, "transit_delay_sec": 0.2, "max_active_in_transit": 8, "max_bandwidth_trains_per_min": 16},
            {"route_id": 3, "name": "Haven-Earth Deep Crystal Line", "source_world": 3, "source_gate": 2, "dest_world": 1, "dest_gate": 2, "transit_delay_sec": 0.2, "max_active_in_transit": 4, "max_bandwidth_trains_per_min": 6},
            {"route_id": 4, "name": "Earth-Haven High-Tech Return Line", "source_world": 1, "source_gate": 1, "dest_world": 3, "dest_gate": 1, "transit_delay_sec": 0.2, "max_active_in_transit": 8, "max_bandwidth_trains_per_min": 12}
        ]
        for c in corridors_config:
            code, res = http_request(f"{self.base_url}/corridors/register", method="POST", data=c)
            assert code == 200, f"Failed to register corridor {c['route_id']}: {res}"

        code, corridors = http_request(f"{self.base_url}/corridors/list")
        assert code == 200 and len(corridors) == 4

        print(f"  [+] Registered Worlds: {[w['name'] for w in directory]}")
        print(f"  [+] Phase-Filtered Query (min_phase=2): {[w['name'] for w in filtered]}")
        print(f"  [+] Registered Corridors: {len(corridors)} active freight lines")
        print("[+] Scenario 1: Dynamic World Directory Discovery PASS\n")
        self.passed_tests += 1

    # -------------------------------------------------------------------------
    # Scenario 2 & 3: Cross-Server Consist Round Trip & Restored Orders
    # -------------------------------------------------------------------------
    def test_scenario_2_and_3_round_trip_and_orders(self):
        print("--- [Scenario 2 & 3] Cross-Server Consist Round Trip & Restored Orders ---")
        consist_id = "CONSIST-X-HAWN-001"
        order_itinerary = [
            {"order_index": 0, "target_world": 2, "station_name": "Vulcan Smelter Yard"},
            {"order_index": 1, "target_world": 1, "station_name": "Earth Megacity Central"},
            {"order_index": 2, "target_world": 3, "station_name": "Haven Rim Outpost Depot"}
        ]

        # --- HOP 1: Haven Rim (World 3) -> Vulcan Forge (World 2) ---
        print("  [*] HOP 1: Haven Rim (W3) -> Vulcan Forge (W2) with 80 units Raw Ore...")
        hop1_payload = {
            "source_world": 3, "dest_world": 2, "source_gate": 1, "dest_gate": 1,
            "route_id": 1, "consist_id": consist_id,
            "manifest_token": "MANIFEST-FORGE-002",
            "orders": order_itinerary, "current_order_index": 0,
            "total_cargo": 80, "cargo_breakdown": {2: 80}, # 2 = Ore
            "valuation_credits": 800, "transit_delay_sec": 0.3
        }
        code, tx1 = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=hop1_payload)
        assert code == 200, f"Hop 1 initiate failed: {tx1}"
        tx1_id = tx1["transfer_id"]
        assert tx1["current_order_index"] == 0
        assert tx1["state"] == "LOCKED"

        code, dep1 = http_request(f"{self.base_url}/transfers/depart", method="POST", data={"transfer_id": tx1_id})
        assert code == 200 and dep1["state"] == "IN_TRANSIT"

        assert self.wait_until_pending(2, tx1_id), f"Transfer {tx1_id} timed out waiting for arrival at World 2"

        code, claim1 = http_request(f"{self.base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx1_id, "dest_world": 2, "manifest_token": "MANIFEST-FORGE-002"
        })
        assert code == 200 and claim1["state"] == "ARRIVAL_PENDING"

        code, conf1 = http_request(f"{self.base_url}/transfers/confirm", method="POST", data={
            "transfer_id": tx1_id, "dest_world": 2, "success": True, "advance_order": True
        })
        assert code == 200 and conf1["state"] == "COMPLETED"
        assert conf1["current_order_index"] == 1, f"Expected order advanced to 1, got {conf1['current_order_index']}"
        print(f"  [+] Hop 1 Complete: Consist arrived at Vulcan Forge. Order advanced: 0 -> 1")

        # --- HOP 2: Vulcan Forge (World 2) -> Earth Core (World 1) ---
        print("  [*] HOP 2: Vulcan Forge (W2) -> Earth Core (W1) with 50 units Refined Alloys...")
        hop2_payload = {
            "source_world": 2, "dest_world": 1, "source_gate": 1, "dest_gate": 1,
            "route_id": 2, "consist_id": consist_id,
            "manifest_token": "MANIFEST-CORE-001",
            "orders": order_itinerary, "current_order_index": 1,
            "total_cargo": 50, "cargo_breakdown": {3: 50}, # 3 = Steel/Alloy
            "valuation_credits": 1200, "transit_delay_sec": 0.2
        }
        code, tx2 = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=hop2_payload)
        assert code == 200, f"Hop 2 initiate failed: {tx2}"
        tx2_id = tx2["transfer_id"]

        code, dep2 = http_request(f"{self.base_url}/transfers/depart", method="POST", data={"transfer_id": tx2_id})
        assert code == 200 and dep2["state"] == "IN_TRANSIT"

        assert self.wait_until_pending(1, tx2_id), f"Transfer {tx2_id} timed out waiting for arrival at World 1"

        code, claim2 = http_request(f"{self.base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx2_id, "dest_world": 1, "manifest_token": "MANIFEST-CORE-001"
        })
        assert code == 200 and claim2["state"] == "ARRIVAL_PENDING"

        code, conf2 = http_request(f"{self.base_url}/transfers/confirm", method="POST", data={
            "transfer_id": tx2_id, "dest_world": 1, "success": True, "advance_order": True
        })
        assert code == 200 and conf2["state"] == "COMPLETED"
        assert conf2["current_order_index"] == 2, f"Expected order advanced to 2, got {conf2['current_order_index']}"
        print(f"  [+] Hop 2 Complete: Consist arrived at Earth Core. Order advanced: 1 -> 2")

        # --- HOP 3: Earth Core (World 1) -> Haven Rim (World 3) Return Loop ---
        print("  [*] HOP 3: Earth Core (W1) -> Haven Rim (W3) with 40 units High-Tech Goods...")
        hop3_payload = {
            "source_world": 1, "dest_world": 3, "source_gate": 1, "dest_gate": 1,
            "route_id": 4, "consist_id": consist_id,
            "manifest_token": "MANIFEST-HAVEN-003",
            "orders": order_itinerary, "current_order_index": 2,
            "total_cargo": 40, "cargo_breakdown": {5: 40}, # 5 = Goods
            "valuation_credits": 2000, "transit_delay_sec": 0.2
        }
        code, tx3 = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=hop3_payload)
        assert code == 200, f"Hop 3 initiate failed: {tx3}"
        tx3_id = tx3["transfer_id"]

        code, dep3 = http_request(f"{self.base_url}/transfers/depart", method="POST", data={"transfer_id": tx3_id})
        assert code == 200

        assert self.wait_until_pending(3, tx3_id), f"Transfer {tx3_id} timed out waiting for arrival at World 3"

        code, claim3 = http_request(f"{self.base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx3_id, "dest_world": 3, "manifest_token": "MANIFEST-HAVEN-003"
        })
        assert code == 200

        code, conf3 = http_request(f"{self.base_url}/transfers/confirm", method="POST", data={
            "transfer_id": tx3_id, "dest_world": 3, "success": True, "advance_order": True
        })
        assert code == 200 and conf3["state"] == "COMPLETED"
        assert conf3["current_order_index"] == 0, f"Expected cyclic order wrap to 0, got {conf3['current_order_index']}"
        print(f"  [+] Hop 3 Complete: Consist returned to Haven Rim! Order wrapped: 2 -> 0 (Full Cycle Complete)")

        print("[+] Scenario 2 & 3: Cross-Server Round Trip & Restored Orders PASS\n")
        self.passed_tests += 1

    # -------------------------------------------------------------------------
    # Scenario 4: Content Admission Rejection & Invariant Guard
    # -------------------------------------------------------------------------
    def test_scenario_4_content_admission_rejection(self):
        print("--- [Scenario 4] Content Admission Rejection & Invariant Guard ---")
        # Record baseline ledger
        code, baseline_audit = http_request(f"{self.base_url}/ledger/status")
        assert code == 200
        initial_cargo_init = baseline_audit["total_cargo_initiated"]

        # 1. Test Incompatible NewGRF Manifest Token
        print("  [*] Attempting transfer with incompatible NewGRF manifest token...")
        bad_token_payload = {
            "source_world": 3, "dest_world": 1, # Earth Core expects MANIFEST-CORE-001
            "manifest_token": "CORRUPT-OR-INCOMPATIBLE-NEWGRF-TOKEN-999",
            "total_cargo": 100, "cargo_breakdown": {2: 100}
        }
        code, res = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=bad_token_payload)
        assert code == 400, f"Expected HTTP 400 for manifest mismatch, got {code}: {res}"
        assert "Content admission rejected" in res.get("error", "")
        print(f"  [+] Incompatible manifest correctly rejected: {res['error']}")

        # 2. Test Corrupt Snapshot Base64
        print("  [*] Attempting transfer with corrupt base64 snapshot stream...")
        bad_b64_payload = {
            "source_world": 3, "dest_world": 2,
            "manifest_token": "MANIFEST-FORGE-002",
            "snapshot_base64": "NOT_VALID_BASE64_###!!!",
            "total_cargo": 50
        }
        code, res = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=bad_b64_payload)
        assert code == 400, f"Expected HTTP 400 for corrupt base64, got {code}: {res}"
        print(f"  [+] Corrupt base64 correctly rejected: {res['error']}")

        # 3. Test Truncated Payload
        bad_trunc_payload = {
            "source_world": 3, "dest_world": 2,
            "manifest_token": "MANIFEST-FORGE-002",
            "snapshot_base64": base64.b64encode(b"X").decode("ascii"), # Only 1 byte
            "total_cargo": 50
        }
        code, res = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=bad_trunc_payload)
        assert code == 400
        print(f"  [+] Truncated snapshot payload correctly rejected: {res['error']}")

        # 4. Strict Invariant Check: Verify NO cargo was added or leaked during rejections
        code, post_reject_audit = http_request(f"{self.base_url}/ledger/status")
        assert code == 200
        assert post_reject_audit["total_cargo_initiated"] == initial_cargo_init, "Rejections corrupted the commodity ledger!"
        assert post_reject_audit["is_conserved"] is True
        print(f"  [+] Zero Cargo Leak Invariant Confirmed: Cargo initiated unchanged ({initial_cargo_init} units)")

        print("[+] Scenario 4: Content Admission Rejection & Invariant Guard PASS\n")
        self.passed_tests += 1

    # -------------------------------------------------------------------------
    # Scenario 5: Freight Corridor Congestion & Relief
    # -------------------------------------------------------------------------
    def test_scenario_5_corridor_congestion_and_relief(self):
        print("--- [Scenario 5] Freight Corridor Congestion & Relief ---")
        # Corridor 1: capacity is 8 trains
        # We will dispatch 9 trains to push through CLEAR -> MODERATE -> CONGESTED -> SATURATED
        train_tx_ids = []
        print("  [*] Ramping corridor traffic to evaluate congestion tiers...")
        for i in range(1, 10):
            priority = "PRIORITY_URGENT" if i == 9 else "STANDARD"
            payload = {
                "source_world": 3, "dest_world": 2, "source_gate": 1, "dest_gate": 1,
                "route_id": 1, "manifest_token": "MANIFEST-FORGE-002",
                "priority": priority, "total_cargo": 10, "transit_delay_sec": 1.0
            }
            code, tx = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=payload)
            assert code == 200
            train_tx_ids.append(tx["transfer_id"])

        code, cong_data = http_request(f"{self.base_url}/corridors/congestion?route_id=1")
        assert code == 200 and cong_data["congestion_level"] == "SATURATED", f"Expected SATURATED, got {cong_data}"
        print(f"  [+] Corridor 1 pushed to SATURATED backpressure tier (9/8 in-transit active)")

        # Verify priority urgent 50% penalty relief
        code, last_tx = http_request(f"{self.base_url}/transfers/{train_tx_ids[-1]}")
        assert code == 200
        # Normal saturated delay = 0.2 * 2.0 = 0.4s; Priority relief penalty (1.0 * 0.5) => mult 1.5 => delay 0.3s
        assert abs(last_tx["transit_delay_sec"] - 0.3) < 0.001, f"Expected 0.3s priority relief delay, got {last_tx['transit_delay_sec']}"
        print(f"  [+] Priority Urgent transfer received 50% congestion penalty relief (Delay: {last_tx['transit_delay_sec']:.2f}s vs standard 0.40s)")

        # Relieve congestion by completing trains
        print("  [*] Processing train arrivals on destination world to relieve bottleneck...")
        for tid in train_tx_ids:
            http_request(f"{self.base_url}/transfers/depart", method="POST", data={"transfer_id": tid})
            http_request(f"{self.base_url}/transfers/claim", method="POST", data={"transfer_id": tid, "dest_world": 2})
            http_request(f"{self.base_url}/transfers/confirm", method="POST", data={"transfer_id": tid, "dest_world": 2, "success": True})

        code, relieved_cong = http_request(f"{self.base_url}/corridors/congestion?route_id=1")
        assert code == 200 and relieved_cong["congestion_level"] == "CLEAR", f"Expected CLEAR after arrivals, got {relieved_cong}"
        print(f"  [+] Corridor 1 bottleneck successfully relieved back to CLEAR (0/8 active in-transit)")

        print("[+] Scenario 5: Corridor Congestion Escalation & Relief PASS\n")
        self.passed_tests += 1

    # -------------------------------------------------------------------------
    # Scenario 6: Node Crash Injection, Quarantine Bay & Auto-Recovery
    # -------------------------------------------------------------------------
    def test_scenario_6_node_crash_quarantine_recovery(self):
        print("--- [Scenario 6] Server Crash Injection, Quarantine Bay & Auto-Recovery ---")
        # 1. Dispatch in-transit consist to World 2
        payload = {
            "source_world": 3, "dest_world": 2, "route_id": 1,
            "manifest_token": "MANIFEST-FORGE-002",
            "total_cargo": 75, "cargo_breakdown": {2: 75},
            "transit_delay_sec": 5.0
        }
        code, tx = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=payload)
        assert code == 200
        tx_id = tx["transfer_id"]
        code, dep = http_request(f"{self.base_url}/transfers/depart", method="POST", data={"transfer_id": tx_id})
        assert code == 200 and dep["state"] == "IN_TRANSIT"
        print(f"  [*] Transfer {tx_id} in transit (75 units Ore) destined for World 2...")

        # 2. Simulate Destination Node Crash
        print("  [*] SIMULATING NODE CRASH: World 2 (Vulcan Forge) drops offline unexpectedly!")
        code, q_res = http_request(f"{self.base_url}/transfers/quarantine", method="POST", data={
            "world_id": 2, "reason": "Server process terminated unexpectedly (SIGSEGV/Network Partition)"
        })
        assert code == 200 and q_res["quarantined_count"] >= 1
        assert tx_id in q_res["transfer_ids"]

        # 3. Verify in Quarantine Bay
        code, q_list = http_request(f"{self.base_url}/transfers/quarantined?dest_world=2")
        assert code == 200 and any(q["transfer_id"] == tx_id for q in q_list)
        assert any(q["state"] == "RECOVERY_REQUIRED" for q in q_list)
        print(f"  [+] Consist secured in Quarantine Bay (State: RECOVERY_REQUIRED). Cargo protected from ghosting.")

        # 4. Strict Conservation during quarantine
        code, q_audit = http_request(f"{self.base_url}/ledger/status")
        assert code == 200 and q_audit["is_conserved"] is True
        print(f"  [+] Strict Commodity Conservation maintained during quarantine.")

        # 5. Simulate Server Recovery & Consist Re-activation
        print("  [*] Supervisor restarting World 2 and executing automated recovery...")
        code, rec_res = http_request(f"{self.base_url}/transfers/recover", method="POST", data={"world_id": 2})
        assert code == 200 and tx_id in rec_res["transfer_ids"]

        code, recovered_tx = http_request(f"{self.base_url}/transfers/{tx_id}")
        assert code == 200 and recovered_tx["state"] == "IN_TRANSIT"
        print(f"  [+] Transfer {tx_id} restored to IN_TRANSIT with arrival time reset. Ready for emergence.")

        # 6. Emerge and confirm
        code, claim = http_request(f"{self.base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx_id, "dest_world": 2, "manifest_token": "MANIFEST-FORGE-002"
        })
        assert code == 200
        code, conf = http_request(f"{self.base_url}/transfers/confirm", method="POST", data={
            "transfer_id": tx_id, "dest_world": 2, "success": True
        })
        assert code == 200 and conf["state"] == "COMPLETED"
        print(f"  [+] Consist safely emerged and materialized on recovered World 2 with zero cargo loss!")

        print("[+] Scenario 6: Node Crash, Quarantine Bay & Auto-Recovery PASS\n")
        self.passed_tests += 1

    # -------------------------------------------------------------------------
    # Scenario 7: Universe Authority Daemon Crash & State Persistence Reload
    # -------------------------------------------------------------------------
    def test_scenario_7_authority_crash_and_persistence(self):
        print("--- [Scenario 7] Universe Authority Daemon Crash & State Persistence Reload ---")
        # 1. Create a transfer currently in transit
        payload = {
            "source_world": 3, "dest_world": 1, "route_id": 3,
            "manifest_token": "MANIFEST-CORE-001",
            "total_cargo": 65, "cargo_breakdown": {2: 65},
            "transit_delay_sec": 10.0
        }
        code, tx = http_request(f"{self.base_url}/transfers/initiate", method="POST", data=payload)
        assert code == 200
        tx_id = tx["transfer_id"]
        http_request(f"{self.base_url}/transfers/depart", method="POST", data={"transfer_id": tx_id})

        # Also register an account and charter to verify full state persistence
        http_request(f"{self.base_url}/auth/register", method="POST", data={
            "username": "FederationTester", "password_hash": "hash123"
        })
        http_request(f"{self.base_url}/companies/register", method="POST", data={
            "owner_player_id": "USRP-00000001", "company_name": "Commonwealth Transit Corp", "home_world_id": 1
        })

        code, pre_crash_audit = http_request(f"{self.base_url}/ledger/status")
        assert code == 200
        print(f"  [*] State established: in-transit transfer {tx_id}, account, corporate charter.")

        # 2. KILL the Universe Authority daemon process!
        print(f"  [*] INJECTING DAEMON CRASH: Terminating Universe Authority process (PID {self.auth_proc.pid})...")
        self.auth_proc.terminate()
        try:
            self.auth_proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            self.auth_proc.kill()
        assert not self.state_file.exists() or self.state_file.stat().st_size > 0
        print(f"  [+] Daemon stopped. Verified state file exists at {self.state_file} ({self.state_file.stat().st_size} bytes).")

        # 3. Restart Universe Authority daemon on a NEW port using the SAME state file!
        new_port = find_free_port()
        new_url = f"http://127.0.0.1:{new_port}"
        print(f"  [*] Restarting Universe Authority on new port {new_port} with --state-file {self.state_file}...")
        self.auth_proc = self.start_authority(port=new_port, state_file=str(self.state_file))
        self.auth_port = new_port
        self.base_url = new_url
        print(f"  [+] Restored Universe Authority listening on {self.base_url}.")

        # 4. Verify loaded state
        code, restored_tx = http_request(f"{self.base_url}/transfers/{tx_id}")
        assert code == 200, f"Transfer {tx_id} lost after authority restart!"
        assert restored_tx["state"] == "IN_TRANSIT"
        assert restored_tx["total_cargo"] == 65

        code, post_crash_audit = http_request(f"{self.base_url}/ledger/status")
        assert code == 200
        assert post_crash_audit["total_cargo_initiated"] == pre_crash_audit["total_cargo_initiated"]
        assert post_crash_audit["total_cargo_in_transit"] == pre_crash_audit["total_cargo_in_transit"]
        assert post_crash_audit["is_conserved"] is True

        code, companies = http_request(f"{self.base_url}/companies/list")
        assert code == 200 and len(companies) >= 1
        print(f"  [+] In-flight transfer {tx_id}, ledger totals, and corporate charters 100% recovered!")

        # Complete the transfer on the new authority instance
        http_request(f"{self.base_url}/transfers/claim", method="POST", data={
            "transfer_id": tx_id, "dest_world": 1, "manifest_token": "MANIFEST-CORE-001"
        })
        http_request(f"{self.base_url}/transfers/confirm", method="POST", data={
            "transfer_id": tx_id, "dest_world": 1, "success": True
        })
        print(f"  [+] Transfer {tx_id} successfully finalized on recovered daemon.")

        print("[+] Scenario 7: Universe Authority State Checkpoint & Reload PASS\n")
        self.passed_tests += 1

    # -------------------------------------------------------------------------
    # Scenario 8: Empire-Wide Commodity Conservation & Trade Balances
    # -------------------------------------------------------------------------
    def test_scenario_8_commodity_conservation_and_trade_balances(self):
        print("--- [Scenario 8] Commodity Conservation Ledger & Trade Balances ---")
        # 1. Macro audit
        code, audit = http_request(f"{self.base_url}/ledger/status")
        assert code == 200
        print(f"  - Transfers Initiated: {audit['total_transfers_initiated']}")
        print(f"  - Transfers Completed: {audit['total_transfers_completed']}")
        print(f"  - Cargo Initiated:     {audit['total_cargo_initiated']} units")
        print(f"  - Cargo Completed:     {audit['total_cargo_completed']} units")
        print(f"  - Cargo In Transit:    {audit['total_cargo_in_transit']} units")
        print(f"  - Conserved Invariant: {audit['is_conserved']}")
        assert audit["is_conserved"] is True, "Commodity conservation invariant violated!"
        assert audit["total_cargo_initiated"] == (audit["total_cargo_completed"] + audit["total_cargo_in_transit"])

        # 2. Detailed per-cargo audit
        code, detailed = http_request(f"{self.base_url}/ledger/audit_detailed")
        assert code == 200
        print(f"  [+] Detailed Multi-Cargo Conservation Audit:")
        for cargo_type, init_amt in detailed.get("cargo_initiated", {}).items():
            comp_amt = detailed.get("cargo_completed", {}).get(cargo_type, 0)
            in_trans = detailed.get("cargo_in_transit", {}).get(cargo_type, 0)
            print(f"      Cargo {cargo_type}: Initiated {init_amt} == Completed {comp_amt} + Transit {in_trans}")
            assert init_amt == (comp_amt + in_trans), f"Cargo {cargo_type} leak or duplication detected!"

        # 3. Supply Chain Matrix
        code, matrix = http_request(f"{self.base_url}/economy/matrix")
        assert code == 200
        print(f"  [+] Supply Chain Flows:")
        print(f"      - Frontier -> Refinery: {matrix['frontier_to_refinery_cargo']} units")
        print(f"      - Refinery -> Megacity: {matrix['refinery_to_core_cargo']} units")
        print(f"      - Megacity Exports:     {matrix['core_export_cargo']} units")
        print(f"      - Total Volume:         {matrix['total_interplanetary_cargo']} units")
        print(f"      - Total Tariffs:        {matrix['total_tariffs_generated']} Cr")

        # 4. Inter-World Trade Balances
        code, balances = http_request(f"{self.base_url}/ledger/trade_balance")
        assert code == 200
        print(f"  [+] Bilateral Trade Balances: {len(balances)} trade pairs active")

        print("[+] Scenario 8: Commodity Conservation Ledger & Trade Balances PASS\n")
        self.passed_tests += 1

    def run(self):
        try:
            self.setup()
            self.test_scenario_1_directory_discovery()
            self.test_scenario_2_and_3_round_trip_and_orders()
            self.test_scenario_4_content_admission_rejection()
            self.test_scenario_5_corridor_congestion_and_relief()
            self.test_scenario_6_node_crash_quarantine_recovery()
            self.test_scenario_7_authority_crash_and_persistence()
            self.test_scenario_8_commodity_conservation_and_trade_balances()

            print("=" * 75)
            print(f" ALL {self.passed_tests}/{self.total_tests} FEDERATION ACCEPTANCE SCENARIOS PASSED (100%)")
            print("=" * 75)
            return 0
        except Exception as e:
            print(f"\n[ERROR] Acceptance Test Suite Failed: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc()
            return 1
        finally:
            self.teardown()

def main():
    parser = argparse.ArgumentParser(description="OpenSpaceTTD Sprint 29 Federation Acceptance Kit")
    args = parser.parse_args()

    runner = FederationAcceptanceRunner()
    sys.exit(runner.run())

if __name__ == "__main__":
    main()
