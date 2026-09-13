#!/usr/bin/env python3
"""
OpenSpaceTTD Sprint 32 - Planetary Settlement Lifecycle & Technology Progression Kit
Automated end-to-end validation suite testing multi-tier phase elevation,
cargo delivery development stimulation, Universe Authority REST promotion endpoints,
daemon checkpoint crash persistence, and post-promotion freight flows.
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

class LifecycleAcceptanceRunner:
    def __init__(self):
        self.repo_root = Path(__file__).parent.parent.resolve()
        self.authority_script = self.repo_root / "scripts" / "universe_authority.py"
        self.temp_dir = tempfile.TemporaryDirectory(prefix="openspace_s32_")
        self.state_file = Path(self.temp_dir.name) / "authority_state.json"
        self.auth_port = find_free_port()
        self.auth_process = None

    def cleanup(self):
        if self.auth_process:
            try:
                self.auth_process.terminate()
                self.auth_process.wait(timeout=2)
            except Exception:
                try:
                    self.auth_process.kill()
                except Exception:
                    pass
            self.auth_process = None
        self.temp_dir.cleanup()

    def start_authority(self, port=None):
        if port is not None:
            self.auth_port = port
        cmd = [
            sys.executable, str(self.authority_script),
            "--port", str(self.auth_port),
            "--state-file", str(self.state_file)
        ]
        self.auth_process = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        url = f"http://127.0.0.1:{self.auth_port}/health"
        for _ in range(40):
            status, res = http_request(url, timeout=0.5)
            if status == 200:
                return True
            time.sleep(0.1)
        return False

    def run_all(self):
        print("===========================================================================")
        print(" OpenSpaceTTD Sprint 32: Settlement Lifecycle & Progression Kit")
        print("===========================================================================")
        print(f"[*] State Persistence: {self.state_file}")

        if not self.start_authority():
            print("[-] Failed to start Universe Authority daemon.")
            return False
        print(f"[+] Universe Authority daemon online on http://127.0.0.1:{self.auth_port}")

        tests = [
            ("Scenario 1: Directory Setup & Baseline Phase Classification", self.test_baseline_setup),
            ("Scenario 2: Outpost Colonization & Initial Frontier Elevation", self.test_outpost_colonization),
            ("Scenario 3: Remote Phase Promotion via REST API (Phase 3 -> 2 -> 1)", self.test_phase_promotions),
            ("Scenario 4: Daemon Crash Persistence Across Multi-Tier Promotions", self.test_persistence_reload),
            ("Scenario 5: Post-Promotion Freight Corridor Trade & Conservation", self.test_post_promotion_trade),
        ]

        passed = 0
        for name, test_fn in tests:
            print(f"\n--- [{name}] ---")
            try:
                if test_fn():
                    print(f"[+] {name} PASS")
                    passed += 1
                else:
                    print(f"[-] {name} FAIL")
            except Exception as e:
                print(f"[-] {name} EXCEPTION: {e}")

        print("\n===========================================================================")
        print(f" ALL {passed}/{len(tests)} LIFECYCLE ACCEPTANCE SCENARIOS PASSED ({(passed/len(tests))*100:.0f}%)")
        print("===========================================================================")
        return passed == len(tests)

    def test_baseline_setup(self):
        worlds = [
            {"world_id": 1, "phase": 1, "biome": "Temperate", "name": "Earth Core", "manifest_token": "MANIFEST-CORE"},
            {"world_id": 2, "phase": 2, "biome": "Volcanic", "name": "Vulcan Forge", "manifest_token": "MANIFEST-DEV"},
            {"world_id": 3, "phase": 3, "biome": "SubArctic", "name": "Calyx Colony", "manifest_token": "MANIFEST-FRONT"},
            {"world_id": 4, "phase": 4, "biome": "Oceanic", "name": "Pelagios Sea", "manifest_token": "MANIFEST-EXP"},
        ]

        for w in worlds:
            st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/register", "POST", w)
            if st != 200:
                print(f"[-] Failed to register world {w['world_id']}: {res}")
                return False

        st, dir_res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds")
        if st != 200 or len(dir_res) != 4:
            print(f"[-] Unexpected directory response: {dir_res}")
            return False

        phases = {w["world_id"]: w.get("phase") for w in dir_res}
        if phases[1] != 1 or phases[2] != 2 or phases[3] != 3 or phases[4] != 4:
            print(f"[-] Phase distribution incorrect: {phases}")
            return False

        print(f"  [+] Baseline setup verified: Worlds 1-4 across Phases 1-4.")
        return True

    def test_outpost_colonization(self):
        # Colonize World 4 (Pelagios Sea, Oceanic Phase 4)
        payload = {"world_id": 4, "outpost_name": "Port Pelagios"}
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/4/colonize", "POST", payload)
        if st != 200 or res.get("phase") != 3 or res.get("name") != "Port Pelagios":
            print(f"[-] Colonization failed: {res}")
            return False

        print(f"  [+] World 4 colonized: elevated from Phase 4 to Phase 3 Frontier ('Port Pelagios').")
        return True

    def test_phase_promotions(self):
        # 1. Promote World 4 from Phase 3 (Frontier) -> Phase 2 (Developed)
        st, res1 = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/4/promote", "POST")
        if st != 200 or res1.get("phase") != 2:
            print(f"[-] Phase 3 -> 2 promotion failed: {res1}")
            return False
        print(f"  [+] World 4 elevated to Phase 2 Developed (Development Score: {res1.get('development_score')})")

        # 2. Promote World 4 from Phase 2 (Developed) -> Phase 1 (Core)
        st, res2 = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/4/promote", "POST")
        if st != 200 or res2.get("phase") != 1:
            print(f"[-] Phase 2 -> 1 promotion failed: {res2}")
            return False
        print(f"  [+] World 4 elevated to Phase 1 Core Metropolis (Development Score: {res2.get('development_score')})")

        # 3. Attempting to promote beyond Phase 1 must be rejected
        st, res3 = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/4/promote", "POST")
        if st == 200:
            print(f"[-] Promotion beyond Phase 1 was not rejected: {res3}")
            return False
        print(f"  [+] Maximum phase guard enforced: {res3.get('error')}")

        # 4. Non-existent world rejected
        st, res_ghost = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/999/promote", "POST")
        if st == 200:
            return False
        print(f"  [+] Non-existent world guard enforced: {res_ghost.get('error')}")

        return True

    def test_persistence_reload(self):
        print(f"  [*] Injecting daemon crash (SIGKILL on PID {self.auth_process.pid})...")
        self.auth_process.kill()
        self.auth_process.wait()
        self.auth_process = None

        new_port = find_free_port()
        print(f"  [*] Restarting daemon on port {new_port} from {self.state_file}...")
        if not self.start_authority(port=new_port):
            print("[-] Daemon restart failed.")
            return False

        st, dir_res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds")
        if st != 200:
            print(f"[-] Failed to query restored directory: {dir_res}")
            return False

        w4 = next((w for w in dir_res if w["world_id"] == 4), None)
        if w4 is None or w4.get("phase") != 1 or w4.get("name") != "Port Pelagios":
            print(f"[-] Restored state mismatch: {w4}")
            return False

        print(f"  [+] State verified: World 4 remained Phase 1 Core Metropolis ('{w4['name']}') with score {w4.get('development_score')}.")
        return True

    def test_post_promotion_trade(self):
        # Register freight corridor from World 4 (now Phase 1 Core) to World 2 (Phase 2 Developed)
        route_payload = {
            "route_id": 84,
            "source_world": 4,
            "source_gate_id": 140,
            "dest_world": 2,
            "dest_gate_id": 120,
            "max_bandwidth": 10,
            "max_in_transit": 10
        }
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/corridors/register", "POST", route_payload)
        if st != 200:
            print(f"[-] Corridor registration failed: {res}")
            return False

        dummy_snapshot = base64.b64encode(b"OPENTTD_CONSIST_SNAPSHOT_HIGH_TECH_CORE_001").decode("ascii")
        transfer_payload = {
            "source_world": 4,
            "dest_world": 2,
            "source_gate_id": 140,
            "dest_gate_id": 120,
            "consist_manifest": "MANIFEST-DEV",
            "snapshot_bytes": dummy_snapshot,
            "cargo_units": 150,
            "cargo_type": 5, # High-tech manufactured goods
            "priority": "Express",
            "orders": [{"dest_world": 2, "dest_station": 5}],
            "current_order_index": 0
        }

        st, t_res = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/initiate", "POST", transfer_payload)
        if st != 200:
            print(f"[-] Transfer initiation failed: {t_res}")
            return False
        transfer_id = t_res["transfer_id"]

        # Confirm departure
        st, _ = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/depart", "POST", {"transfer_id": transfer_id})
        if st != 200:
            return False

        # Claim
        st, _ = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/claim", "POST", {"transfer_id": transfer_id, "dest_world": 2})
        if st != 200:
            return False

        # Confirm arrival
        st, _ = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/confirm", "POST", {"transfer_id": transfer_id, "dest_world": 2, "success": True})
        if st != 200:
            return False

        # Audit ledger
        st, audit_res = http_request(f"http://127.0.0.1:{self.auth_port}/audit/commodity")
        if st != 200 or not (audit_res.get("conserved") or audit_res.get("all_conserved")):
            print(f"[-] Commodity conservation audit failed: {audit_res}")
            return False

        print(f"  [+] Post-promotion express transfer completed (150 units high-tech goods). Invariant conserved: True")
        return True

def main():
    runner = LifecycleAcceptanceRunner()
    try:
        ok = runner.run_all()
        sys.exit(0 if ok else 1)
    finally:
        runner.cleanup()

if __name__ == "__main__":
    main()
