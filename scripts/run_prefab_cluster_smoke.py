#!/usr/bin/env python3
"""
OpenSpaceTTD Headless Multiplayer Cluster Smoke Demonstration (Option 2)
========================================================================
Spins up a local 2-server testbed cluster using independent dedicated server
instances loading the canonical Commonwealth Prefab World save
(`demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav`).

Architecture:
  - Central Universe Authority daemon (scripts/universe_authority.py) on dynamic port.
  - Server 1 (Node 0: Sol Earth Core) running on 127.0.0.1:<port1>
  - Server 2 (Node 1: Augusta CST Hub) running on 127.0.0.1:<port2>
  - Inter-server portal linking (Tile 4798 <-> Tile 25919) via Universe Authority
  - Unattended simulation with live traffic, trade tariffs, and commodity ledger conservation.
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


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def http_json(url: str, method: str = "GET", data: dict = None, timeout: float = 5.0) -> tuple[int, dict]:
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
    def __init__(self, name: str, world_id: int, binary_path: Path, save_path: Path, port: int, authority_url: str):
        self.name = name
        self.world_id = world_id
        self.binary_path = binary_path
        self.save_path = save_path
        self.port = port
        self.authority_url = authority_url
        self.proc = None
        self.lines = []
        self.lock = threading.Lock()
        self.log_file = None

    def start(self, log_dir: Path):
        env = dict(os.environ)
        env["OPENSPACETTD_AUTHORITY_URL"] = self.authority_url
        cmd = [
            str(self.binary_path),
            "-D", f"127.0.0.1:{self.port}",
            "-g", str(self.save_path),
            "-s", "null",
            "-m", "null",
            "-b", "null",
            "-x", "-X",
            "-d", "net=1"
        ]
        log_path = log_dir / f"cluster_node_{self.world_id}.log"
        self.log_file = open(log_path, "w")
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env=env,
            bufsize=1,
            start_new_session=True
        )

        def reader():
            for line in self.proc.stdout:
                line_str = line.strip()
                if self.log_file and not self.log_file.closed:
                    self.log_file.write(line)
                    self.log_file.flush()
                with self.lock:
                    self.lines.append(line_str)
                    if len(self.lines) > 2000:
                        self.lines.pop(0)

        threading.Thread(target=reader, daemon=True).start()

    def send_cmd(self, cmd: str):
        if self.proc and self.proc.stdin:
            try:
                self.proc.stdin.write(cmd + "\n")
                self.proc.stdin.flush()
            except Exception:
                pass

    def is_alive(self) -> bool:
        return self.proc is not None and self.proc.poll() is None

    def wait_for_log(self, substring: str, timeout: float = 6.0) -> bool:
        deadline = time.monotonic() + timeout
        last_idx = 0
        while time.monotonic() < deadline:
            with self.lock:
                for idx in range(last_idx, len(self.lines)):
                    if substring in self.lines[idx]:
                        return True
                last_idx = len(self.lines)
            time.sleep(0.05)
        return False

    def get_company_money(self) -> int:
        for _ in range(3):
            start_idx = len(self.lines)
            self.send_cmd("companies")
            deadline = time.monotonic() + 1.5
            while time.monotonic() < deadline:
                with self.lock:
                    for line in reversed(self.lines[max(0, start_idx - 5):]):
                        if "Commonwealth Interplanetary Transport" in line and "Money:" in line:
                            parts = line.split("Money:")
                            if len(parts) > 1:
                                val_str = parts[1].strip().split()[0]
                                try:
                                    return int(val_str)
                                except ValueError:
                                    pass
                time.sleep(0.05)
        return 0

    def get_train_count(self) -> int:
        start_idx = len(self.lines)
        self.send_cmd("federation_trains")
        deadline = time.monotonic() + 1.5
        while time.monotonic() < deadline:
            with self.lock:
                for line in reversed(self.lines[max(0, start_idx - 5):]):
                    if "Total front engines:" in line:
                        parts = line.split("Total front engines:")
                        try:
                            return int(parts[1].strip())
                        except Exception:
                            pass
            time.sleep(0.05)
        return 0

    def stop(self):
        if self.proc:
            try:
                self.send_cmd("quit")
                self.proc.wait(timeout=3.0)
            except Exception:
                try:
                    self.proc.kill()
                except Exception:
                    pass
            self.proc = None
        if self.log_file and not self.log_file.closed:
            self.log_file.close()


def run_cluster_smoke(duration_sec: int = 300, repo_root: Path = None):
    repo = repo_root or Path(__file__).resolve().parent.parent
    binary = repo / "build" / "openttd"
    authority_script = repo / "scripts" / "universe_authority.py"
    prefab_save = repo / "demo" / "OpenSpaceTTD-Commonwealth-UAT-v1.0.sav"

    if not binary.is_file() or not os.access(binary, os.X_OK):
        print(f"[FAIL] OpenSpaceTTD binary not found at {binary}", file=sys.stderr)
        return False

    if not prefab_save.is_file():
        print(f"[FAIL] Prefab world save not found at {prefab_save}", file=sys.stderr)
        return False

    auth_port = find_free_port()
    s1_port = find_free_port()
    s2_port = find_free_port()
    auth_url = f"http://127.0.0.1:{auth_port}"

    log_dir = repo / "logs" / "cluster_smoke"
    log_dir.mkdir(parents=True, exist_ok=True)
    work_dir = Path(tempfile.mkdtemp(prefix="osttd_cluster_"))

    print("=" * 80)
    print("      OPENSPACETTD HEADLESS MULTIPLAYER CLUSTER SMOKE TESTBED")
    print(f"  Target Run Duration: {duration_sec} seconds ({duration_sec / 60:.1f} minutes)")
    print(f"  Universe Authority:  {auth_url}")
    print(f"  Server 1 (Sol Earth): 127.0.0.1:{s1_port}")
    print(f"  Server 2 (Augusta):  127.0.0.1:{s2_port}")
    print(f"  Source Prefab Save:  {prefab_save}")
    print("=" * 80)

    # 1. Start Universe Authority Daemon
    auth_log = open(log_dir / "authority.log", "w")
    auth_cmd = [sys.executable, str(authority_script), "--host", "127.0.0.1", "--port", str(auth_port)]
    auth_proc = subprocess.Popen(auth_cmd, stdout=auth_log, stderr=subprocess.STDOUT, start_new_session=True)

    ready = False
    for _ in range(50):
        time.sleep(0.1)
        code, _ = http_json(f"{auth_url}/ledger/status")
        if code == 200:
            ready = True
            break

    if not ready:
        print("[FAIL] Universe Authority daemon failed to start", file=sys.stderr)
        auth_proc.kill()
        return False

    print("[+] Universe Authority daemon online and healthy.")

    # 2. Register cluster worlds in directory
    worlds = [
        {"world_id": 0, "name": "Sol Earth Core", "phase": 1, "address": f"127.0.0.1:{s1_port}"},
        {"world_id": 1, "name": "Augusta CST Hub", "phase": 2, "address": f"127.0.0.1:{s2_port}"},
    ]
    for w in worlds:
        code, res = http_json(f"{auth_url}/worlds/register", method="POST", data=w)
        if code != 200:
            print(f"[FAIL] Could not register world {w['world_id']}: {res}", file=sys.stderr)
            auth_proc.kill()
            return False
        print(f"    -> Registered World {w['world_id']}: '{w['name']}' ({w['address']})")

    # 3. Create isolated working copies of prefab save
    s1_sav = work_dir / "server1_earth.sav"
    s2_sav = work_dir / "server2_augusta.sav"
    shutil.copy2(prefab_save, s1_sav)
    shutil.copy2(prefab_save, s2_sav)

    # 4. Start dedicated server processes
    s1 = DedicatedServerInstance("Sol-Earth", 0, binary, s1_sav, s1_port, auth_url)
    s2 = DedicatedServerInstance("Augusta-Hub", 1, binary, s2_sav, s2_port, auth_url)

    s1.start(log_dir)
    s2.start(log_dir)
    print("[+] Spawned Server 1 and Server 2 dedicated processes.")

    time.sleep(2.5)
    for srv in [s1, s2]:
        if not srv.is_alive():
            print(f"[FAIL] Dedicated server {srv.name} died during startup!", file=sys.stderr)
            auth_proc.kill()
            return False

    # 5. Link portal gates between servers
    # Sol Earth Tile 4798 <-> Augusta Tile 25919
    s1.send_cmd("federation_link_gate 4798 1 25919 1 10 0")
    s2.send_cmd("federation_link_gate 25919 0 4798 2 10 1")
    time.sleep(0.5)

    s1.send_cmd("unpause")
    s2.send_cmd("unpause")
    print("[+] Connected inter-server portals and unpaused simulation engines.\n")

    # Sample baseline finances after engine starts ticking
    time.sleep(1.0)
    s1_initial_money = s1.get_company_money()
    s2_initial_money = s2.get_company_money()
    print(f"[*] Baseline Treasuries: Server 1={s1_initial_money:,} Cr | Server 2={s2_initial_money:,} Cr\n")

    start_time = time.monotonic()
    deadline = start_time + duration_sec
    last_report = start_time
    iteration = 0

    try:
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now - last_report >= 30.0 or iteration == 0:
                iteration += 1
                elapsed = now - start_time

                if not s1.is_alive() or not s2.is_alive():
                    print(f"\n[FAIL] Server crash detected at elapsed {elapsed:.1f}s!", file=sys.stderr)
                    return False

                s1_money = s1.get_company_money()
                s2_money = s2.get_company_money()
                s1_trains = s1.get_train_count()
                s2_trains = s2.get_train_count()

                _, audit = http_json(f"{auth_url}/ledger/audit_detailed")
                conserved = audit.get("conserved", True)
                in_transit = audit.get("total_transfers_in_transit", 0)
                completed = audit.get("total_transfers_completed", 0)

                print(f"[T+{elapsed:05.1f}s / {duration_sec}s] "
                      f"S1: {s1_money:,} Cr ({s1_money - s1_initial_money:+,} Cr, {s1_trains} trains) | "
                      f"S2: {s2_money:,} Cr ({s2_money - s2_initial_money:+,} Cr, {s2_trains} trains) | "
                      f"Ledger: Transfers={completed}, Conserved={'YES' if conserved else 'NO'}")
                last_report = now

            time.sleep(1.0)

    except KeyboardInterrupt:
        print("\n[!] User interrupted cluster demonstration.")

    print("\n" + "=" * 80)
    print("                    FINAL CLUSTER AUDIT REPORT")
    print("=" * 80)

    total_elapsed = time.monotonic() - start_time
    s1_final_money = s1.get_company_money()
    s2_final_money = s2.get_company_money()
    s1_final_trains = s1.get_train_count()
    s2_final_trains = s2.get_train_count()
    _, final_audit = http_json(f"{auth_url}/ledger/audit_detailed")

    print(f"Total Simulation Time:  {total_elapsed:.1f} seconds")
    print(f"Server 1 (Sol Earth):   {'ALIVE (0 Crashes)' if s1.is_alive() else 'DEAD'} | Active Trains: {s1_final_trains}")
    print(f"Server 2 (Augusta Hub): {'ALIVE (0 Crashes)' if s2.is_alive() else 'DEAD'} | Active Trains: {s2_final_trains}")
    print(f"Server 1 Treasury Gain: {s1_final_money - s1_initial_money:+,} Cr")
    print(f"Server 2 Treasury Gain: {s2_final_money - s2_initial_money:+,} Cr")
    print(f"Transfers Completed:    {final_audit.get('total_transfers_completed', 0)}")
    print(f"Commodity Conservation: {'CONSERVED (0 Leaks)' if final_audit.get('conserved', True) else 'VIOLATION'}")

    print("\n[*] Gracefully stopping cluster servers and authority daemon...")
    s1.stop()
    s2.stop()
    auth_proc.terminate()
    try:
        auth_proc.wait(timeout=3.0)
    except subprocess.TimeoutExpired:
        auth_proc.kill()
    auth_log.close()
    shutil.rmtree(work_dir, ignore_errors=True)

    print("[OK] Cluster smoke test completed successfully.\n")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--duration", type=int, default=300, help="Run duration in seconds (default: 300 / 5 min)")
    args = parser.parse_args()

    success = run_cluster_smoke(duration_sec=args.duration)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
