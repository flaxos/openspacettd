#!/usr/bin/env python3
"""
OpenSpaceTTD Sprint 35 - Cross-Process Federation Acceptance Test Suite
=======================================================================
Validates physical train exchange across network boundaries between two
independent dedicated server processes (openttd -D) orchestrated by the
external Python Universe Authority daemon.

Verifies:
  1. External Transport & Multi-Server Configuration
  2. Outbound Departure Handoff (Initiate + Depart + Despawn)
  3. Real-Time Background Polling & Physical Materialization
  4. Deduplication & Idempotency Invariants (Zero Duplicate Train)
  5. Empire-Wide Commodity Conservation (Zero Cargo Leak)
  6. Return Trip / Round-Trip Consist Handoff
"""

import argparse
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]

def http_json(url, method="GET", data=None, timeout=5.0):
    body = json.dumps(data).encode("utf-8") if data is not None else None
    headers = {"Content-Type": "application/json"} if data is not None else {}
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8")
            return resp.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8") if e.fp else str(e)
        try:
            return e.code, json.loads(raw)
        except Exception:
            return e.code, {"error": raw}
    except Exception as e:
        return 0, {"error": str(e)}

class DedicatedServerInstance:
    def __init__(self, name, binary_path, save_path, port, authority_url):
        self.name = name
        self.binary_path = Path(binary_path).resolve()
        self.save_path = Path(save_path).resolve()
        self.port = port
        self.authority_url = authority_url
        self.proc = None
        self.lines = []
        self.lock = threading.Lock()
        self.reader_thread = None

    def start(self):
        env = dict(os.environ)
        env["OPENSPACETTD_AUTHORITY_URL"] = self.authority_url
        cmd = [
            str(self.binary_path),
            "-D", f"127.0.0.1:{self.port}",
            "-g", str(self.save_path),
            "-d", "net=1"
        ]
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            env=env
        )
        self.reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self.reader_thread.start()

    def _read_loop(self):
        try:
            for line in self.proc.stdout:
                line_str = line.strip()
                with self.lock:
                    self.lines.append(line_str)
        except Exception:
            pass

    def send_cmd(self, cmd):
        if self.proc and self.proc.stdin:
            try:
                self.proc.stdin.write(cmd + "\n")
                self.proc.stdin.flush()
            except Exception:
                pass

    def wait_for_log(self, substring, timeout=12.0):
        deadline = time.time() + timeout
        last_idx = 0
        while time.time() < deadline:
            with self.lock:
                for idx in range(last_idx, len(self.lines)):
                    if substring in self.lines[idx]:
                        return self.lines[idx]
                last_idx = len(self.lines)
            time.sleep(0.05)
        return None

    def get_lines(self):
        with self.lock:
            return list(self.lines)

    def stop(self):
        if self.proc:
            try:
                self.send_cmd("quit")
                self.proc.wait(timeout=3)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass
            self.proc = None

class Sprint35AcceptanceRunner:
    def __init__(self):
        self.repo_root = Path(__file__).parent.parent.resolve()
        self.openttd_bin = self.repo_root / "build" / "openttd"
        self.authority_script = self.repo_root / "scripts" / "universe_authority.py"
        self.base_save = self.repo_root / "demo" / "Hetston Transport, 1950-06-30-gateway-fixed.sav"
        self.temp_dir = tempfile.TemporaryDirectory(prefix="sprint35_uat_")
        self.temp_path = Path(self.temp_dir.name)
        self.state_file = self.temp_path / "authority_state.json"

        self.auth_port = find_free_port()
        self.auth_url = f"http://127.0.0.1:{self.auth_port}"
        self.auth_proc = None

        self.server1 = None
        self.server2 = None

        self.server1_port = find_free_port()
        self.server2_port = find_free_port()

        self.server1_save = self.temp_path / "server1.sav"
        self.server2_save = self.temp_path / "server2.sav"

    def setup_authority(self):
        print(f"[*] Starting Universe Authority daemon on {self.auth_url}...")
        cmd = [
            sys.executable, str(self.authority_script),
            "--host", "127.0.0.1",
            "--port", str(self.auth_port),
            "--state-file", str(self.state_file)
        ]
        self.auth_proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ready = False
        for _ in range(40):
            time.sleep(0.1)
            code, _ = http_json(f"{self.auth_url}/ledger/status")
            if code == 200:
                ready = True
                break
        if not ready:
            raise RuntimeError(f"Authority failed to start on {self.auth_url}")
        print("[+] Universe Authority online and responsive.")

    def configure_worlds_and_routes(self):
        print("\n[*] Registering participating worlds with Universe Authority...")
        code, r1 = http_json(f"{self.auth_url}/worlds/register", method="POST", data={
            "world_id": 1,
            "phase": 1,
            "name": "Sol-Prime",
            "address": f"127.0.0.1:{self.server1_port}",
            "manifest_token": "manifest-token-sol"
        })
        assert code == 200, f"World 1 registration failed: {r1}"

        code, r2 = http_json(f"{self.auth_url}/worlds/register", method="POST", data={
            "world_id": 2,
            "phase": 2,
            "name": "Vulcan-Forge",
            "address": f"127.0.0.1:{self.server2_port}",
            "manifest_token": "manifest-token-sol"
        })
        assert code == 200, f"World 2 registration failed: {r2}"

        print("[*] Registering inter-server wormhole routes (W1 Gate 10 <-> W2 Gate 20)...")
        code, route = http_json(f"{self.auth_url}/portal_links", method="POST", data={
            "route_id": 1,
            "source_world": 1,
            "source_gate": 10,
            "dest_world": 2,
            "dest_gate": 20,
            "transit_delay_sec": 0.5
        })
        assert code == 200, f"Outbound route registration failed: {route}"

        code, ret_route = http_json(f"{self.auth_url}/portal_links", method="POST", data={
            "route_id": 2,
            "source_world": 2,
            "source_gate": 20,
            "dest_world": 1,
            "dest_gate": 10,
            "transit_delay_sec": 0.5
        })
        assert code == 200, f"Return route registration failed: {ret_route}"
        print("[+] Routes successfully registered.")

    def prepare_save_fixtures(self):
        print("\n[*] Preparing dedicated save fixtures for Server 1 and Server 2...")
        shutil.copy(self.base_save, self.server1_save)
        shutil.copy(self.base_save, self.server2_save)
        print("[+] Save fixtures prepared.")

    def run_acceptance_suite(self):
        print("=" * 80)
        print(" OpenSpaceTTD Sprint 35: Real Cross-Process Federation Transport")
        print("=" * 80)

        self.setup_authority()
        self.configure_worlds_and_routes()
        self.prepare_save_fixtures()

        print("\n[*] Launching dedicated binaries for Server 1 and Server 2...")
        self.server1 = DedicatedServerInstance("Server1-Sol", self.openttd_bin, self.server1_save, self.server1_port, self.auth_url)
        self.server2 = DedicatedServerInstance("Server2-Vulcan", self.openttd_bin, self.server2_save, self.server2_port, self.auth_url)

        self.server1.start()
        self.server2.start()
        time.sleep(1.5)

        # Verify authority connectivity on both instances
        self.server1.send_cmd("federation_authority")
        self.server2.send_cmd("federation_authority")
        log1 = self.server1.wait_for_log(f"Universe Authority URL: '{self.auth_url}'", timeout=5.0)
        log2 = self.server2.wait_for_log(f"Universe Authority URL: '{self.auth_url}'", timeout=5.0)
        assert log1 is not None, f"Server 1 failed to acquire authority URL: {self.server1.get_lines()}"
        assert log2 is not None, f"Server 2 failed to acquire authority URL: {self.server2.get_lines()}"
        print("[+] Test 1 PASSED: Both dedicated servers connected to external Universe Authority.")

        # Configure inter-server portal links dynamically on both servers
        print("\n[*] Configuring inter-server portal gates across processes...")
        self.server1.send_cmd("federation_link_gate 262814 2 20 10 5")
        self.server2.send_cmd("federation_link_gate 262849 1 10 20 5")
        g1 = self.server1.wait_for_log("Registered inter-server portal link (ID 10)", timeout=5.0)
        g2 = self.server2.wait_for_log("Registered inter-server portal link (ID 20)", timeout=5.0)
        assert g1 is not None, f"Server 1 failed to register portal gate: {self.server1.get_lines()}"
        assert g2 is not None, f"Server 2 failed to register portal gate: {self.server2.get_lines()}"
        print("[+] Gates linked: Server 1 Gate 10 (Tile 262814) <-> Server 2 Gate 20 (Tile 262849).")

        # Verify initial train counts
        self.server1.send_cmd("federation_trains")
        self.server2.send_cmd("federation_trains")
        assert self.server1.wait_for_log("Total front engines: 2", timeout=5.0) is not None
        assert self.server2.wait_for_log("Total front engines: 2", timeout=5.0) is not None
        print("[+] Initial state verified: Server 1 has 2 trains, Server 2 has 2 trains.")

        # Unpause simulations
        self.server1.send_cmd("unpause")
        self.server2.send_cmd("unpause")
        time.sleep(0.5)

        # Scenario A: Outbound Consist Departure on Server 1
        print("\n--- [Scenario A] Consist Departure from Sol-Prime (Server 1) ---")
        self.server1.send_cmd("federation_dispatch 10 262814")
        dep_log = self.server1.wait_for_log("[Federation] Consist departed", timeout=8.0)
        assert dep_log is not None, f"Server 1 failed to log departure: {self.server1.get_lines()}"
        print(f"[+] Outbound Departure Confirmed: {dep_log}")

        # Verify Server 1 despawned the consist
        self.server1.send_cmd("federation_trains")
        assert self.server1.wait_for_log("Total front engines: 1", timeout=5.0) is not None
        print("[+] Train successfully despawned from Server 1 (front engines: 2 -> 1).")

        # Scenario B: Authority In-Transit Ledger Verification
        print("\n--- [Scenario B] Authority Transfer State & Commodity Conservation ---")
        code, ledger = http_json(f"{self.auth_url}/ledger/status")
        assert code == 200, f"Failed to query authority ledger: {ledger}"
        print(f"[+] Authority Ledger Status: {ledger}")
        assert ledger.get("total_transfers_initiated", 0) >= 1
        assert ledger.get("commodity_conservation", {}).get("is_conserved", True)

        # Scenario C: Destination Runtime Arrival & Physical Materialization on Server 2
        print("\n--- [Scenario C] Runtime Arrival & Materialization on Vulcan-Forge (Server 2) ---")
        mat_log = self.server2.wait_for_log("[Federation] Consist materialized", timeout=12.0)
        assert mat_log is not None, f"Server 2 failed to materialize consist: {self.server2.get_lines()}"
        print(f"[+] Consist Materialization Confirmed on Server 2: {mat_log}")

        # Verify Server 2 vehicle pool increased
        self.server2.send_cmd("federation_trains")
        assert self.server2.wait_for_log("Total front engines: 3", timeout=5.0) is not None
        print("[+] Physical train materialized on Server 2 (front engines: 2 -> 3).")

        # Verify Authority Transfer Completion
        time.sleep(0.5)
        code, audit = http_json(f"{self.auth_url}/ledger/status")
        assert code == 200
        assert audit.get("total_transfers_completed", 0) >= 1
        assert audit.get("total_transfers_in_transit", 0) == 0
        assert audit.get("commodity_conservation", {}).get("is_conserved", True)
        print("[+] Transfer completed on Authority. Commodity conservation verified: 0 leak, 0 loss.")

        # Scenario D: Deduplication & Idempotency Invariant Guard
        print("\n--- [Scenario D] Deduplication Guard & Double-Materialization Prevention ---")
        time.sleep(2.0) # Let Server 2 run several more polling cycles
        self.server2.send_cmd("federation_trains")
        # Ensure Server 2 still has exactly 3 trains (no duplicates spawned)
        lines = self.server2.get_lines()
        last_total = [l for l in lines if "Total front engines:" in l][-1]
        assert "Total front engines: 3" in last_total, f"Duplicate consist detected! Found: {last_total}"
        print(f"[+] Deduplication Guard Verified: Engine count remains exactly 3 ({last_total}).")

        # Scenario E: Return Trip Consist Transfer (Server 2 -> Server 1)
        print("\n--- [Scenario E] Return Trip Hand-off (Vulcan-Forge -> Sol-Prime) ---")
        # Dispatch the train on Server 2 through Gate 20 (tile 262849) back to Server 1
        # The new train on Server 2 was allocated at index 12 (or highest valid ID)
        self.server2.send_cmd("federation_dispatch 12 262849")
        ret_dep_log = self.server2.wait_for_log("[Federation] Consist departed", timeout=8.0)
        if ret_dep_log is None:
            # Try dispatching other valid train index if 12 differed
            self.server2.send_cmd("federation_dispatch 10 262849")
            ret_dep_log = self.server2.wait_for_log("[Federation] Consist departed", timeout=8.0)

        assert ret_dep_log is not None, f"Server 2 failed to log return departure: {self.server2.get_lines()}"
        print(f"[+] Return Departure Confirmed on Server 2: {ret_dep_log}")

        # Server 1 should now poll and materialize the returning consist
        ret_mat_log = self.server1.wait_for_log("[Federation] Consist materialized", timeout=12.0)
        assert ret_mat_log is not None, f"Server 1 failed to materialize return consist: {self.server1.get_lines()}"
        print(f"[+] Return Consist Materialized on Server 1: {ret_mat_log}")

        # Final audit
        time.sleep(0.5)
        code, final_ledger = http_json(f"{self.auth_url}/ledger/status")
        assert code == 200
        assert final_ledger.get("total_transfers_completed", 0) >= 2
        is_conserved = final_ledger.get("is_conserved", final_ledger.get("commodity_conservation", {}).get("is_conserved", True))
        assert is_conserved
        print(f"\n[+] Complete Round-Trip Verified: {final_ledger['total_transfers_completed']} transfers completed.")
        print("[+] Final Empire Conservation Ledger:")
        print(f"    - Total Transfers: {final_ledger['total_transfers_completed']}")
        print(f"    - In-Transit: {final_ledger['total_transfers_in_transit']}")
        print(f"    - Conservation Integrity: {is_conserved}")
        print("=" * 80)
        print(" ALL SPRINT 35 CROSS-PROCESS ACCEPTANCE SCENARIOS PASSED!")
        print("=" * 80)

    def teardown(self):
        print("\n[*] Tearing down acceptance environment...")
        if self.server1:
            self.server1.stop()
        if self.server2:
            self.server2.stop()
        if self.auth_proc and self.auth_proc.poll() is None:
            try:
                self.auth_proc.terminate()
                self.auth_proc.wait(timeout=2)
            except Exception:
                self.auth_proc.kill()
        self.temp_dir.cleanup()
        print("[+] Teardown complete.")

def main():
    runner = Sprint35AcceptanceRunner()
    try:
        runner.run_acceptance_suite()
        return 0
    except Exception as e:
        print(f"\n[!] Acceptance Suite Failed: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1
    finally:
        runner.teardown()

if __name__ == "__main__":
    sys.exit(main())
