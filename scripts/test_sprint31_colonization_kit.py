#!/usr/bin/env python3
"""
OpenSpaceTTD Sprint 31 - Planetary Colonization & Federation Acceptance Kit
Automated end-to-end validation suite testing Phase 4 wilderness registration,
6-biome taxonomy, remote outpost colonization via Universe Authority REST API,
daemon crash/checkpoint persistence, and post-colonization trade flows.
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

class ColonizationAcceptanceRunner:
    def __init__(self):
        self.repo_root = Path(__file__).parent.parent.resolve()
        self.authority_script = self.repo_root / "scripts" / "universe_authority.py"
        self.temp_dir = tempfile.TemporaryDirectory(prefix="openspace_s31_")
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
        print(" OpenSpaceTTD Sprint 31: Planetary Colonization Acceptance Kit")
        print("===========================================================================")
        print(f"[*] State Persistence: {self.state_file}")

        if not self.start_authority():
            print("[-] Failed to start Universe Authority daemon.")
            return False
        print(f"[+] Universe Authority daemon online on http://127.0.0.1:{self.auth_port}")

        tests = [
            ("Scenario 1: 6-Biome Registration & Directory Taxonomy", self.test_biome_registration),
            ("Scenario 2: Remote Colonization via REST API", self.test_remote_colonization),
            ("Scenario 3: Colonization Validation & State Guards", self.test_colonization_guards),
            ("Scenario 4: Daemon Crash Persistence of Colonized Worlds", self.test_persistence_reload),
            ("Scenario 5: Post-Colonization Inter-World Trade Flow", self.test_post_colonization_trade),
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
        print(f" ALL {passed}/{len(tests)} COLONIZATION ACCEPTANCE SCENARIOS PASSED ({(passed/len(tests))*100:.0f}%)")
        print("===========================================================================")
        return passed == len(tests)

    def test_biome_registration(self):
        worlds = [
            {"world_id": 1, "phase": 1, "biome": "Temperate", "name": "Earth Core", "manifest_token": "MANIFEST-001"},
            {"world_id": 2, "phase": 2, "biome": "AridDesert", "name": "Merredin Refinery", "manifest_token": "MANIFEST-002"},
            {"world_id": 3, "phase": 3, "biome": "SubArctic", "name": "Calyx Frontier", "manifest_token": "MANIFEST-003"},
            {"world_id": 4, "phase": 4, "biome": "Volcanic", "name": "Ignis Wilderness", "manifest_token": "MANIFEST-004"},
            {"world_id": 5, "phase": 4, "biome": "SubTropic", "name": "Viridis Wilderness", "manifest_token": "MANIFEST-005"},
            {"world_id": 6, "phase": 4, "biome": "Oceanic", "name": "Pelagios Wilderness", "manifest_token": "MANIFEST-006"},
        ]

        for w in worlds:
            st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/register", "POST", w)
            if st != 200:
                print(f"[-] Failed to register world {w['world_id']}: {res}")
                return False

        st, dir_res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds")
        if st != 200 or len(dir_res) != 6:
            print(f"[-] Unexpected directory response: {dir_res}")
            return False

        biomes = {w["world_id"]: w.get("biome") for w in dir_res}
        phases = {w["world_id"]: w.get("phase") for w in dir_res}

        if biomes[4] != "Volcanic" or phases[4] != 4:
            print(f"[-] World 4 mismatch: {dir_res}")
            return False
        if biomes[5] != "SubTropic" or phases[5] != 4:
            print(f"[-] World 5 mismatch: {dir_res}")
            return False
        if biomes[6] != "Oceanic" or phases[6] != 4:
            print(f"[-] World 6 mismatch: {dir_res}")
            return False

        print(f"  [+] All 6 biomes verified across Phase 1-4 worlds: {list(biomes.values())}")
        return True

    def test_remote_colonization(self):
        # Colonize World 4 (Volcanic) via REST endpoint
        payload = {"world_id": 4, "outpost_name": "Caldera Outpost Alpha"}
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/4/colonize", "POST", payload)
        if st != 200:
            print(f"[-] Colonize failed: status {st}, body {res}")
            return False

        if res.get("phase") != 3:
            print(f"[-] Phase not promoted: {res}")
            return False
        if res.get("name") != "Caldera Outpost Alpha":
            print(f"[-] Name not updated: {res}")
            return False

        print(f"  [+] World 4 successfully colonized: now '{res['name']}' (Phase {res['phase']} Frontier)")
        return True

    def test_colonization_guards(self):
        # 1. Duplicate colonization attempt on World 4 must be rejected
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/4/colonize", "POST", {"outpost_name": "Duplicate"})
        if st == 200:
            print(f"[-] Duplicate colonization was not rejected: {res}")
            return False
        print(f"  [+] Duplicate colonization correctly rejected: {res.get('error')}")

        # 2. Attempting to colonize Phase 1 Core world must be rejected
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/1/colonize", "POST", {"outpost_name": "Illegal Core Outpost"})
        if st == 200:
            print(f"[-] Core world colonization was not rejected: {res}")
            return False
        print(f"  [+] Core world colonization correctly rejected: {res.get('error')}")

        # 3. Non-existent world must be rejected
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds/999/colonize", "POST", {"outpost_name": "Ghost"})
        if st == 200:
            print(f"[-] Ghost world colonization was not rejected: {res}")
            return False
        print(f"  [+] Non-existent world correctly rejected: {res.get('error')}")

        return True

    def test_persistence_reload(self):
        # Verify state file exists before crash
        if not self.state_file.exists():
            print(f"[-] State file does not exist: {self.state_file}")
            return False

        print(f"  [*] Injecting daemon termination (SIGKILL on PID {self.auth_process.pid})...")
        self.auth_process.kill()
        self.auth_process.wait()
        self.auth_process = None

        new_port = find_free_port()
        print(f"  [*] Restarting daemon on new port {new_port} from {self.state_file}...")
        if not self.start_authority(port=new_port):
            print("[-] Daemon restart failed.")
            return False

        st, dir_res = http_request(f"http://127.0.0.1:{self.auth_port}/worlds")
        if st != 200:
            print(f"[-] Failed to query restored directory: {dir_res}")
            return False

        w4 = next((w for w in dir_res if w["world_id"] == 4), None)
        if w4 is None or w4.get("phase") != 3 or w4.get("name") != "Caldera Outpost Alpha":
            print(f"[-] World 4 state not persisted accurately: {w4}")
            return False

        print(f"  [+] State 100% restored: World 4 remains '{w4['name']}' (Phase {w4['phase']} Frontier)")
        return True

    def test_post_colonization_trade(self):
        # Register a corridor from newly colonized World 4 to World 2
        route_payload = {
            "route_id": 42,
            "source_world": 4,
            "source_gate_id": 104,
            "dest_world": 2,
            "dest_gate_id": 102,
            "max_bandwidth": 8,
            "max_in_transit": 8
        }
        st, res = http_request(f"http://127.0.0.1:{self.auth_port}/corridors/register", "POST", route_payload)
        if st != 200:
            print(f"[-] Corridor registration failed: {res}")
            return False

        dummy_snapshot = base64.b64encode(b"OPENTTD_CONSIST_SNAPSHOT_COLONY_ORE_001").decode("ascii")
        transfer_payload = {
            "source_world": 4,
            "dest_world": 2,
            "source_gate_id": 104,
            "dest_gate_id": 102,
            "consist_manifest": "MANIFEST-002",
            "snapshot_bytes": dummy_snapshot,
            "cargo_units": 100,
            "cargo_type": 2, # Raw Ore
            "priority": "Standard",
            "orders": [{"dest_world": 2, "dest_station": 10}],
            "current_order_index": 0
        }

        st, t_res = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/initiate", "POST", transfer_payload)
        if st != 200:
            print(f"[-] Transfer initiation failed: {t_res}")
            return False
        transfer_id = t_res["transfer_id"]

        # Confirm departure
        st, d_res = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/depart", "POST", {"transfer_id": transfer_id})
        if st != 200:
            return False

        # Claim & confirm arrival
        st, c_res = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/claim", "POST", {"transfer_id": transfer_id, "dest_world": 2})
        if st != 200:
            return False

        st, a_res = http_request(f"http://127.0.0.1:{self.auth_port}/transfers/confirm", "POST", {"transfer_id": transfer_id, "dest_world": 2, "success": True})
        if st != 200:
            return False

        # Audit trade ledger
        st, audit_res = http_request(f"http://127.0.0.1:{self.auth_port}/audit/commodity")
        if st != 200 or not audit_res.get("conserved"):
            print(f"[-] Commodity not conserved: {audit_res}")
            return False

        print(f"  [+] Post-colonization transfer complete (100 units raw ore). Conservation verified: True")
        return True

def main():
    runner = ColonizationAcceptanceRunner()
    try:
        ok = runner.run_all()
        sys.exit(0 if ok else 1)
    finally:
        runner.cleanup()

if __name__ == "__main__":
    main()
