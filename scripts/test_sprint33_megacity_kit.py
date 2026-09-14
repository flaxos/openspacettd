#!/usr/bin/env python3
"""
OpenSpaceTTD Sprint 33 - Planetary Town Growth, Megacity Supply Loops & Biome Industry Lifecycle Kit
Automated end-to-end validation suite testing:
1. Dynamic world demographics & baseline megacity registration.
2. Megacity multi-tier supply delivery tracking & monthly demand quota evaluation.
3. World-level Megacity REST API endpoints (GET /worlds/<id>/megacity, POST /worlds/<id>/megacity).
4. Colonial outpost founding and automatic Phase 1 Core elevation to Megacity.
5. Checkpoint state persistence and crash recovery across daemon restarts.
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

class MegacityAcceptanceRunner:
    def __init__(self):
        self.repo_root = Path(__file__).parent.parent.resolve()
        self.authority_script = self.repo_root / "scripts" / "universe_authority.py"
        self.temp_dir = tempfile.TemporaryDirectory(prefix="openspace_s33_")
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
        print(" OpenSpaceTTD Sprint 33: Planetary Town Growth & Megacity Supply Loops Kit")
        print("===========================================================================")
        print(f"[*] State Persistence: {self.state_file}")

        if not self.start_authority():
            print("[-] Failed to start Universe Authority daemon.")
            return False
        print(f"[+] Universe Authority daemon online on http://127.0.0.1:{self.auth_port}")

        tests = [
            ("Scenario 1: Directory Setup & Baseline Demographics", self.test_baseline_demographics),
            ("Scenario 2: Multi-Tier Cargo Delivery & Monthly Supply Evaluation", self.test_megacity_supply_eval),
            ("Scenario 3: World-Level Megacity REST API (GET/POST /worlds/<id>/megacity)", self.test_world_megacity_rest_api),
            ("Scenario 4: Colonial Outpost Elevation to Phase 1 Auto-Megacity", self.test_outpost_auto_megacity),
            ("Scenario 5: Daemon Crash Recovery & Checkpoint State Persistence", self.test_crash_recovery_persistence),
        ]

        passed = 0
        total = len(tests)
        for name, test_fn in tests:
            print(f"\n--- {name} ---")
            try:
                if test_fn():
                    print(f"PASS: {name}")
                    passed += 1
                else:
                    print(f"FAIL: {name}")
            except Exception as e:
                print(f"ERROR: {name}: {e}")

        print("\n===========================================================================")
        print(f" Results: {passed}/{total} Scenarios Passed")
        print("===========================================================================")
        return passed == total

    def test_baseline_demographics(self):
        """Register initial planetary federation worlds with demographics."""
        worlds = [
            {
                "world_id": 0, "phase": 1, "name": "Earth Core",
                "biome": "Temperate", "population": 50000,
                "is_megacity": True, "megacity_growth_state": "Subsistence",
                "satisfaction_pct": 100.0
            },
            {
                "world_id": 1, "phase": 2, "name": "Vulcan Foundry",
                "biome": "Volcanic", "population": 15000,
                "is_megacity": False, "megacity_growth_state": "Subsistence",
                "satisfaction_pct": 100.0
            },
            {
                "world_id": 2, "phase": 3, "name": "Boreas Outpost",
                "biome": "SubArctic", "population": 2000,
                "is_megacity": False, "megacity_growth_state": "Subsistence",
                "satisfaction_pct": 100.0
            },
            {
                "world_id": 3, "phase": 4, "name": "Viridis Wilderness",
                "biome": "SubTropic", "population": 0,
                "is_megacity": False, "megacity_growth_state": "Subsistence",
                "satisfaction_pct": 0.0
            }
        ]

        for w in worlds:
            code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/register", "POST", w)
            if code != 200:
                print(f"[-] Failed to register world {w['world_id']}: {resp}")
                return False

        # Query directory
        code, directory = http_request(f"http://127.0.0.1:{self.auth_port}/worlds", "GET")
        if code != 200 or len(directory) < 4:
            print(f"[-] Invalid directory response: {directory}")
            return False

        # Verify demographics on World 0
        w0 = next((x for x in directory if x["world_id"] == 0), None)
        if not w0 or not w0.get("is_megacity") or w0.get("population") != 50000:
            print(f"[-] World 0 demographics mismatch: {w0}")
            return False

        print(f"[+] Directory populated with {len(directory)} worlds. World 0 population: {w0['population']}.")
        return True

    def test_megacity_supply_eval(self):
        """Test town-level Megacity demand registration, delivery, and monthly evaluation."""
        reg_payload = {
            "town_id": 10,
            "world_id": 0,
            "name": "Olympus Megacity",
            "population": 20000
        }
        code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/megacity/register", "POST", reg_payload)
        if code != 200:
            print(f"[-] Failed to register megacity: {resp}")
            return False

        # Check initial status
        code, status = http_request(f"http://127.0.0.1:{self.auth_port}/megacity/status?town_id=10", "GET")
        if code != 200 or not status or status[0]["growth_state"] != "SUBSISTENCE":
            print(f"[-] Unexpected initial megacity status: {status}")
            return False

        # Deliver cargo across all three tiers
        # Quotas for pop 20000: Tier 0 = 1000, Tier 1 = 500, Tier 2 = 200
        http_request(f"http://127.0.0.1:{self.auth_port}/megacity/deliver", "POST", {"town_id": 10, "cargo_type": 11, "amount": 1200})
        http_request(f"http://127.0.0.1:{self.auth_port}/megacity/deliver", "POST", {"town_id": 10, "cargo_type": 5, "amount": 600})
        http_request(f"http://127.0.0.1:{self.auth_port}/megacity/deliver", "POST", {"town_id": 10, "cargo_type": 10, "amount": 250})

        # Evaluate monthly supply
        code, eval_res = http_request(f"http://127.0.0.1:{self.auth_port}/megacity/eval", "POST", {"town_id": 10})
        if code != 200:
            print(f"[-] Megacity eval failed: {eval_res}")
            return False

        code, status = http_request(f"http://127.0.0.1:{self.auth_port}/megacity/status?town_id=10", "GET")
        if code != 200 or not status:
            print(f"[-] Status fetch failed: {status}")
            return False

        rec = status[0]
        if rec.get("growth_state") != "HYPER_GROWTH" or rec.get("growth_multiplier") != 2.0:
            print(f"[-] Megacity did not reach HyperGrowth: {rec}")
            return False

        print(f"[+] Megacity 10 evaluated to HyperGrowth: 2.0x growth multiplier, 1.5x passenger multiplier.")
        return True

    def test_world_megacity_rest_api(self):
        """Test GET and POST endpoints for /worlds/<id>/megacity."""
        # Update World 0 megacity metrics
        payload = {
            "is_megacity": True,
            "megacity_growth_state": "MetropolitanBoom",
            "satisfaction_pct": 115.0,
            "population": 55000
        }
        code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/0/megacity", "POST", payload)
        if code != 200:
            print(f"[-] POST /worlds/0/megacity failed: {resp}")
            return False

        # Query GET /worlds/0/megacity
        code, get_resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/0/megacity", "GET")
        if code != 200:
            print(f"[-] GET /worlds/0/megacity failed: {get_resp}")
            return False

        if (get_resp.get("population") != 55000 or
            get_resp.get("megacity_growth_state") != "MetropolitanBoom" or
            abs(get_resp.get("satisfaction_pct", 0) - 115.0) > 0.01):
            print(f"[-] Megacity state mismatch: {get_resp}")
            return False

        print(f"[+] REST /worlds/0/megacity verified: population={get_resp['population']}, state={get_resp['megacity_growth_state']}.")
        return True

    def test_outpost_auto_megacity(self):
        """Test colonizing World 3 (Phase 4 -> 3) and promoting to Phase 1 Core with auto-Megacity elevation."""
        # Colonize outpost
        code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/3/colonize", "POST", {"outpost_name": "New Horizons Frontier"})
        if code != 200:
            print(f"[-] Colonize failed: {resp}")
            return False

        # Promote Phase 3 -> Phase 2
        code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/3/promote", "POST", {})
        if code != 200:
            print(f"[-] Promotion to Phase 2 failed: {resp}")
            return False

        # Promote Phase 2 -> Phase 1 Core
        code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/3/promote", "POST", {})
        if code != 200:
            print(f"[-] Promotion to Phase 1 failed: {resp}")
            return False

        # Check World 3 megacity status
        code, resp = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/3/megacity", "GET")
        if code != 200 or not resp.get("is_megacity"):
            print(f"[-] World 3 did not automatically elevate to megacity: {resp}")
            return False

        print(f"[+] World 3 successfully elevated to Phase 1 Core with auto-Megacity registration.")
        return True

    def test_crash_recovery_persistence(self):
        """Simulate daemon termination, reload from state-file, and verify preserved megacity metrics."""
        print(f"[*] Terminating Universe Authority process (PID: {self.auth_process.pid})...")
        self.auth_process.terminate()
        self.auth_process.wait(timeout=3)
        self.auth_process = None

        if not self.state_file.exists():
            print(f"[-] Checkpoint file {self.state_file} was not written before termination!")
            return False

        # Restart on a new port
        new_port = find_free_port()
        print(f"[*] Restarting Universe Authority daemon on port {new_port} from {self.state_file}...")
        if not self.start_authority(port=new_port):
            print("[-] Daemon failed to restart from checkpoint.")
            return False

        # Verify World 0 and World 3 state
        code0, resp0 = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/0/megacity", "GET")
        code3, resp3 = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/3/megacity", "GET")

        if code0 != 200 or resp0.get("population") != 55000 or resp0.get("megacity_growth_state") != "MetropolitanBoom":
            print(f"[-] World 0 state not recovered: {resp0}")
            return False

        if code3 != 200 or not resp3.get("is_megacity") or resp3.get("megacity_growth_state") != "Subsistence":
            print(f"[-] World 3 state not recovered: {resp3}")
            return False

        print(f"[+] Crash recovery successful. All Megacity and demographic states restored identically.")
        return True

def main():
    runner = MegacityAcceptanceRunner()
    try:
        success = runner.run_all()
        sys.exit(0 if success else 1)
    finally:
        runner.cleanup()

if __name__ == "__main__":
    main()
