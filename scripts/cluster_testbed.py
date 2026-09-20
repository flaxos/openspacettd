#!/usr/bin/env python3
"""
OpenSpaceTTD Live Multi-Node Federation Cluster Testbed (Horizon A)
==================================================================
Orchestrates an operational 3-node live multi-server cluster testbed running
independent OS processes (openttd -D) communicating across TCP network sockets
and coordinated by the centralized Universe Authority daemon (scripts/universe_authority.py).

Topology:
  - Central Universe Authority: http://127.0.0.1:38080 (or dynamic port)
  - World 1: Earth Core      (Phase 1 Core,        127.0.0.1:3979)
  - World 2: Vulcan Forge    (Phase 2 Developed,   127.0.0.1:3980)
  - World 3: Haven Rim       (Phase 3 Frontier,    127.0.0.1:3981)

Subcommands / Modes:
  --start                Launch cluster daemon and 3 world servers in background.
  --status               Show live health, pings, client counts, and commodity ledger.
  --stop                 Gracefully stop all running cluster nodes.
  --test-all             Run the complete 5-scenario automated validation suite.
  --test-multi-hop       Execute Scenario 2: World 3 -> World 2 -> World 1 multi-hop transit.
  --test-staging         Execute Scenario 3: Latency holding & staging siding divert.
  --test-crash-recovery  Execute Scenario 4: Destination server crash, quarantine, restart.
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

DEFAULT_AUTHORITY_PORT = 38080
DEFAULT_SERVER_PORTS = {1: 3979, 2: 3980, 3: 3981}
PID_FILE_PATH = "logs/cluster/cluster_pids.json"


# =============================================================================
# Terminal ANSI Color Formatting
# =============================================================================
class Color:
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"

    @staticmethod
    def ok(msg):
        return f"{Color.GREEN}{Color.BOLD}{msg}{Color.RESET}"

    @staticmethod
    def err(msg):
        return f"{Color.RED}{Color.BOLD}{msg}{Color.RESET}"

    @staticmethod
    def warn(msg):
        return f"{Color.YELLOW}{Color.BOLD}{msg}{Color.RESET}"

    @staticmethod
    def info(msg):
        return f"{Color.CYAN}{msg}{Color.RESET}"

    @staticmethod
    def bold(msg):
        return f"{Color.BOLD}{msg}{Color.RESET}"


def find_free_port():
    """Allocate an available ephemeral port on loopback."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def is_port_open(host, port):
    """Check if TCP port is open and accepting connections."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(0.5)
        try:
            s.connect((host, port))
            return True
        except (socket.timeout, ConnectionRefusedError, OSError):
            return False


def http_json(url, method="GET", data=None, timeout=5.0):
    """Execute HTTP JSON request against Universe Authority."""
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


# =============================================================================
# Dedicated Server Process Wrapper
# =============================================================================
class DedicatedServerInstance:
    def __init__(self, name, world_id, binary_path, save_path, port, authority_url, verbose=False):
        self.name = name
        self.world_id = world_id
        self.binary_path = Path(binary_path).resolve()
        self.save_path = Path(save_path).resolve()
        self.port = port
        self.authority_url = authority_url
        self.verbose = verbose
        self.proc = None
        self.lines = []
        self.lock = threading.Lock()
        self.reader_thread = None

    def start(self):
        """Start the dedicated server process with stdin/stdout piped."""
        env = dict(os.environ)
        env["OPENSPACETTD_AUTHORITY_URL"] = self.authority_url
        cmd = [
            str(self.binary_path),
            "-D", f"127.0.0.1:{self.port}",
            "-g", str(self.save_path),
            "-d", "net=1"
        ]
        log_path = Path(f"logs/cluster/world_{self.world_id}.log")
        log_path.parent.mkdir(parents=True, exist_ok=True)
        self.out_f = open(log_path, "a")
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=self.out_f,
            stderr=subprocess.STDOUT,
            text=True,
            env=env,
            start_new_session=True
        )
        self.reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self.reader_thread.start()

    def _read_loop(self):
        log_path = Path(f"logs/cluster/world_{self.world_id}.log")
        try:
            with open(log_path, "r") as f:
                f.seek(0, os.SEEK_END)
                while self.proc and self.proc.poll() is None:
                    line = f.readline()
                    if line:
                        line_str = line.strip()
                        with self.lock:
                            self.lines.append(line_str)
                        if self.verbose:
                            print(f"[{self.name}] {line_str}")
                    else:
                        time.sleep(0.05)
                for line in f.readlines():
                    line_str = line.strip()
                    with self.lock:
                        self.lines.append(line_str)
                    if self.verbose:
                        print(f"[{self.name}] {line_str}")
        except Exception:
            pass

    def send_cmd(self, cmd):
        """Send console command over stdin."""
        if self.proc and self.proc.stdin:
            try:
                self.proc.stdin.write(cmd + "\n")
                self.proc.stdin.flush()
            except Exception:
                pass

    def line_count(self):
        """Current number of captured stdout lines."""
        with self.lock:
            return len(self.lines)

    def wait_for_log(self, substring, timeout=12.0, start_idx=None):
        """Poll stdout buffer for a specific log substring starting from start_idx."""
        deadline = time.time() + timeout
        last_idx = start_idx if start_idx is not None else 0
        while time.time() < deadline:
            with self.lock:
                for idx in range(last_idx, len(self.lines)):
                    if substring in self.lines[idx]:
                        return self.lines[idx]
                last_idx = len(self.lines)
            time.sleep(0.05)
        with self.lock:
            search_from = start_idx if start_idx is not None else 0
            for line in self.lines[search_from:]:
                if substring in line:
                    return line
            print(f"[TIMEOUT] [{self.name}] wait_for_log('{substring}') failed after {timeout}s! (from line {search_from}/{len(self.lines)})")
            if len(self.lines) > 0:
                print(f"          Last 10 lines: {self.lines[-10:]}")
        return None

    def get_lines(self):
        with self.lock:
            return list(self.lines)

    def is_alive(self):
        return self.proc is not None and self.proc.poll() is None

    def get_train_count(self, timeout=4.0):
        """Query train count via federation_trains command."""
        with self.lock:
            start_idx = len(self.lines)
        self.send_cmd("federation_trains")
        deadline = time.time() + timeout
        last_idx = start_idx
        while time.time() < deadline:
            with self.lock:
                for idx in range(last_idx, len(self.lines)):
                    if "Total front engines:" in self.lines[idx]:
                        parts = self.lines[idx].split("Total front engines:")
                        try:
                            return int(parts[1].strip())
                        except Exception:
                            pass
                last_idx = len(self.lines)
            time.sleep(0.05)
        return None

    def get_front_engine_ids(self, timeout=3.0):
        """Query active front engine vehicle IDs."""
        with self.lock:
            start_idx = len(self.lines)
        self.send_cmd("federation_trains")
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if any("Total front engines:" in l for l in self.lines[start_idx:]):
                    ids = []
                    for line in self.lines[start_idx:]:
                        if "Train ID " in line and ":" in line:
                            try:
                                part = line.split("Train ID ")[1].split(":")[0].strip()
                                ids.append(int(part))
                            except Exception:
                                pass
                    return ids
            time.sleep(0.05)
        return []

    def kill(self):
        """Abruptly terminate process (simulating crash)."""
        if self.proc:
            try:
                self.proc.kill()
                self.proc.wait(timeout=2.0)
            except Exception:
                pass
            self.proc = None

    def stop(self):
        """Gracefully request shutdown."""
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


# =============================================================================
# Cluster Manager & Test Suite
# =============================================================================
class ClusterManager:
    def __init__(self, repo_root=None, dynamic_ports=False, verbose=False, state_file=None):
        self.repo_root = Path(repo_root or Path(__file__).parent.parent).resolve()
        self.openttd_bin = self.repo_root / "build" / "openttd"
        self.authority_script = self.repo_root / "scripts" / "universe_authority.py"
        self.fixtures_dir = self.repo_root / "fixtures" / "cluster"
        self.verbose = verbose
        self.dynamic_ports = dynamic_ports

        if dynamic_ports:
            self.auth_port = find_free_port()
            self.server_ports = {1: find_free_port(), 2: find_free_port(), 3: find_free_port()}
        else:
            self.auth_port = DEFAULT_AUTHORITY_PORT
            self.server_ports = dict(DEFAULT_SERVER_PORTS)

        self.auth_url = f"http://127.0.0.1:{self.auth_port}"
        self.state_file = state_file

        self.auth_proc = None
        self.servers = {}

    def ensure_fixtures(self):
        """Ensure the cluster world savegames exist."""
        earth_sav = self.fixtures_dir / "cluster_earth.sav"
        vulcan_sav = self.fixtures_dir / "cluster_vulcan.sav"
        haven_sav = self.fixtures_dir / "cluster_haven.sav"

        if not (earth_sav.exists() and vulcan_sav.exists() and haven_sav.exists()):
            print("[Cluster] Setting up missing cluster savegame fixtures...")
            setup_script = self.repo_root / "scripts" / "setup_cluster_fixtures.py"
            cmd = [sys.executable, str(setup_script), "-o", str(self.fixtures_dir)]
            subprocess.run(cmd, check=True)

    def start_authority(self):
        """Start the Universe Authority HTTP daemon."""
        print(f"[*] Starting Universe Authority daemon on {self.auth_url}...")
        cmd = [
            sys.executable, str(self.authority_script),
            "--host", "127.0.0.1",
            "--port", str(self.auth_port)
        ]
        if self.state_file:
            cmd.extend(["--state-file", str(self.state_file)])

        log_dir = self.repo_root / "logs" / "cluster"
        log_dir.mkdir(parents=True, exist_ok=True)
        self.auth_log_file = open(log_dir / "authority.log", "a")
        self.auth_proc = subprocess.Popen(
            cmd,
            stdout=self.auth_log_file if not self.verbose else None,
            stderr=subprocess.STDOUT if not self.verbose else None,
            start_new_session=True
        )

        ready = False
        for _ in range(40):
            time.sleep(0.1)
            code, _ = http_json(f"{self.auth_url}/ledger/status")
            if code == 200:
                ready = True
                break

        if not ready:
            raise RuntimeError(f"Authority failed to listen on {self.auth_url}")
        print(Color.ok(f"[+] Universe Authority online at {self.auth_url}"))

    def bootstrap_authority_topology(self):
        """Register worlds and inter-server corridors in Universe Authority."""
        print("[*] Registering 3 cluster worlds with Universe Authority...")
        worlds = [
            {"world_id": 1, "name": "Earth Core", "phase": 1, "address": f"127.0.0.1:{self.server_ports[1]}"},
            {"world_id": 2, "name": "Vulcan Forge", "phase": 2, "address": f"127.0.0.1:{self.server_ports[2]}"},
            {"world_id": 3, "name": "Haven Rim", "phase": 3, "address": f"127.0.0.1:{self.server_ports[3]}"},
        ]
        for w in worlds:
            code, res = http_json(f"{self.auth_url}/worlds/register", method="POST", data=w)
            assert code == 200, f"Failed to register World {w['world_id']}: {res}"
            print(f"    -> Registered World {w['world_id']} ('{w['name']}', Phase {w['phase']})")

        print("[*] Registering inter-server corridors...")
        routes = [
            {"route_id": 1, "name": "Haven-Vulcan Ore Conduit", "source_world": 3, "source_gate": 30, "dest_world": 2, "dest_gate": 21, "transit_delay_sec": 1.0},
            {"route_id": 2, "name": "Vulcan-Haven Return Line", "source_world": 2, "source_gate": 21, "dest_world": 3, "dest_gate": 30, "transit_delay_sec": 1.0},
            {"route_id": 3, "name": "Vulcan-Earth Alloy Corridor", "source_world": 2, "source_gate": 20, "dest_world": 1, "dest_gate": 10, "transit_delay_sec": 1.0},
            {"route_id": 4, "name": "Earth-Vulcan Return Line", "source_world": 1, "source_gate": 10, "dest_world": 2, "dest_gate": 20, "transit_delay_sec": 1.0},
        ]
        for r in routes:
            code, res = http_json(f"{self.auth_url}/portal_links", method="POST", data=r)
            assert code == 200, f"Failed to register Route {r['route_id']}: {res}"
            print(f"    -> Registered Corridor {r['route_id']} ('{r['name']}'): W{r['source_world']}:G{r['source_gate']} -> W{r['dest_world']}:G{r['dest_gate']}")

    def start_servers(self, temp_dir=None):
        """Spawn dedicated server instances for all 3 worlds."""
        self.ensure_fixtures()
        save_dir = Path(temp_dir) if temp_dir else self.fixtures_dir

        server_configs = [
            ("Earth-Core", 1, save_dir / "cluster_earth.sav"),
            ("Vulcan-Forge", 2, save_dir / "cluster_vulcan.sav"),
            ("Haven-Rim", 3, save_dir / "cluster_haven.sav"),
        ]

        print("[*] Spawning 3 dedicated server instances...")
        for name, wid, sav in server_configs:
            srv = DedicatedServerInstance(
                name=name,
                world_id=wid,
                binary_path=self.openttd_bin,
                save_path=sav,
                port=self.server_ports[wid],
                authority_url=self.auth_url,
                verbose=self.verbose
            )
            srv.start()
            self.servers[wid] = srv
            print(f"    -> World {wid} ('{name}') started on 127.0.0.1:{self.server_ports[wid]}")

        time.sleep(1.5)

        # Verify connectivity to authority
        for wid, srv in self.servers.items():
            srv.send_cmd("federation_authority")
            log = srv.wait_for_log(f"Universe Authority URL: '{self.auth_url}'", timeout=5.0)
            assert log is not None, f"World {wid} failed to connect to Authority!"

        print(Color.ok("[+] All 3 game servers connected to Universe Authority."))

    def configure_gates_and_sidings(self):
        """Configure portal gates and staging sidings on all 3 server instances."""
        print("[*] Linking portal gates and designating staging sidings across instances...")

        # Server 1 (Earth Core):
        # Gate 10 (Tile 262814) -> Remote World 2, Gate 20. Staging siding on tile 257377. Local world 1.
        s1 = self.servers[1]
        s1.send_cmd("federation_link_gate 262814 2 20 10 5 1")
        time.sleep(0.2)
        assert s1.wait_for_log("Registered inter-server portal link (ID 10)", timeout=5.0) is not None
        s1.send_cmd("federation_link_staging 262814 257377")
        assert s1.wait_for_log("Configured staging siding:", timeout=5.0) is not None

        # Server 2 (Vulcan Forge):
        # Gate 20 (Tile 262849) -> Remote World 1, Gate 10. Staging siding on tile 257377. Local world 2.
        # Gate 21 (Tile 262463) -> Remote World 3, Gate 30. Local world 2.
        s2 = self.servers[2]
        s2.send_cmd("federation_link_gate 262849 1 10 20 5 2")
        time.sleep(0.2)
        assert s2.wait_for_log("Registered inter-server portal link (ID 20)", timeout=5.0) is not None
        s2.send_cmd("federation_link_gate 262463 3 30 21 5 2")
        time.sleep(0.2)
        assert s2.wait_for_log("Registered inter-server portal link (ID 21)", timeout=5.0) is not None
        s2.send_cmd("federation_link_staging 262849 257377")
        assert s2.wait_for_log("Configured staging siding:", timeout=5.0) is not None

        # Server 3 (Haven Rim):
        # Gate 30 (Tile 262814) -> Remote World 2, Gate 21. Staging siding on tile 257377. Local world 3.
        s3 = self.servers[3]
        s3.send_cmd("federation_link_gate 262814 2 21 30 5 3")
        time.sleep(0.2)
        assert s3.wait_for_log("Registered inter-server portal link (ID 30)", timeout=5.0) is not None
        s3.send_cmd("federation_link_staging 262814 257377")
        assert s3.wait_for_log("Configured staging siding:", timeout=5.0) is not None

        # Unpause all instances
        for srv in self.servers.values():
            srv.send_cmd("unpause")
        time.sleep(0.5)
        print(Color.ok("[+] Portal gates and staging sidings linked and active."))

    def stop_all(self):
        """Stop all game servers and the authority."""
        print("[*] Shutting down cluster instances...")
        for wid, srv in self.servers.items():
            srv.stop()
        self.servers.clear()

        if self.auth_proc:
            try:
                self.auth_proc.terminate()
                self.auth_proc.wait(timeout=2.0)
            except Exception:
                try:
                    self.auth_proc.kill()
                except Exception:
                    pass
            self.auth_proc = None

        print(Color.ok("[+] Cluster shutdown complete."))


# =============================================================================
# Automated Test Suite (Horizon A Scenarios 1–5)
# =============================================================================
class ClusterAcceptanceSuite:
    def __init__(self, verbose=False):
        self.verbose = verbose
        self.temp_dir = tempfile.TemporaryDirectory(prefix="cluster_testbed_")
        self.temp_path = Path(self.temp_dir.name)
        self.state_file = self.temp_path / "authority_state.json"

        # Copy fixture saves into temporary directory for mutation isolation
        fixtures_dir = Path(__file__).parent.parent / "fixtures" / "cluster"
        for sav in ("cluster_earth.sav", "cluster_vulcan.sav", "cluster_haven.sav"):
            shutil.copy(fixtures_dir / sav, self.temp_path / sav)

        self.cluster = ClusterManager(
            dynamic_ports=True,
            verbose=self.verbose,
            state_file=self.state_file
        )

    def run_all(self):
        print("=" * 80)
        print(Color.bold(" OpenSpaceTTD Horizon A: Live Multi-Node Cluster Acceptance Suite"))
        print("=" * 80)

        passed = 0
        total = 5

        try:
            # Setup cluster
            self.cluster.start_authority()
            self.cluster.bootstrap_authority_topology()
            self.cluster.start_servers(temp_dir=self.temp_path)
            self.cluster.configure_gates_and_sidings()

            # Scenario 1: Discovery & Directory Initialization
            if self.test_scenario_1_directory_discovery():
                passed += 1
            else:
                return False

            # Scenario 2: Multi-Hop Traversal (Haven -> Vulcan -> Earth)
            if self.test_scenario_2_multi_hop_traversal():
                passed += 1
            else:
                return False

            # Scenario 3: Staging Siding Latency Divert & Recovery
            if self.test_scenario_3_staging_siding_divert():
                passed += 1
            else:
                return False

            # Scenario 4: Destination Server Crash, Quarantine, Restart & Reconciliation
            if self.test_scenario_4_crash_recovery():
                passed += 1
            else:
                return False

            # Scenario 5: Commodity Conservation Ledger Audit
            if self.test_scenario_5_commodity_conservation():
                passed += 1
            else:
                return False

            print("\n" + "=" * 80)
            print(Color.ok(f" ALL {total}/{total} HORIZON A MULTI-NODE ACCEPTANCE SCENARIOS PASSED!"))
            print("=" * 80)
            return True

        finally:
            self.cluster.stop_all()
            try:
                self.temp_dir.cleanup()
            except Exception:
                pass

    # -------------------------------------------------------------------------
    # Scenario 1: Directory Discovery & Topology Initialization
    # -------------------------------------------------------------------------
    def test_scenario_1_directory_discovery(self):
        print("\n" + "-" * 70)
        print(Color.bold("[Scenario 1] Directory Discovery & Cluster Topology Initialization"))
        print("-" * 70)

        code, worlds = http_json(f"{self.cluster.auth_url}/directory/worlds")
        assert code == 200, f"Failed to fetch world directory: {worlds}"
        assert len(worlds) >= 3, f"Expected >= 3 registered worlds, got {len(worlds)}"

        world_names = {w["world_id"]: w["name"] for w in worlds}
        print(f"[+] Discovered worlds in Authority Directory:")
        for wid in (1, 2, 3):
            assert wid in world_names, f"World {wid} missing from directory!"
            print(f"    - World {wid}: '{world_names[wid]}'")

        # Verify initial fleet count on each node (2 engines each)
        for wid in (1, 2, 3):
            srv = self.cluster.servers[wid]
            count = srv.get_train_count()
            assert count == 2, f"Expected 2 initial trains on World {wid}, found {count}"
            print(f"    - World {wid} fleet: {count} trains active.")

        print(Color.ok("[+] Scenario 1 PASSED: Directory and initial fleet verified."))
        return True

    # -------------------------------------------------------------------------
    # Scenario 2: Multi-Hop Traversal (World 3 -> World 2 -> World 1)
    # -------------------------------------------------------------------------
    def test_scenario_2_multi_hop_traversal(self):
        print("\n" + "-" * 70)
        print(Color.bold("[Scenario 2] Multi-Hop Traversal: Haven Rim -> Vulcan Forge -> Earth Core"))
        print("-" * 70)

        s1 = self.cluster.servers[1] # Earth Core
        s2 = self.cluster.servers[2] # Vulcan Forge
        s3 = self.cluster.servers[3] # Haven Rim

        # Hop 1: Haven Rim (World 3) -> Vulcan Forge (World 2)
        print("[*] Hop 1: Dispathing consist from Haven Rim Gate 30 (Tile 262814)...")
        mark3 = s3.line_count()
        s3.send_cmd("federation_dispatch 10 262814")
        dep3 = s3.wait_for_log("[Federation] Consist departed", timeout=8.0, start_idx=mark3)
        assert dep3 is not None, f"Haven Rim failed to log departure: {s3.get_lines()}"
        print(f"    -> Consist departed Haven: {dep3}")

        mark2 = s2.line_count()
        mat2 = s2.wait_for_log("[Federation] Consist materialized", timeout=12.0, start_idx=mark2)
        assert mat2 is not None, f"Vulcan Forge failed to materialize consist: {s2.get_lines()}"
        print(f"    -> Consist materialized on Vulcan Forge: {mat2}")

        # Assert fleet adjustments on Hop 1
        assert s3.get_train_count() == 1, "Haven fleet should have decreased to 1"
        assert s2.get_train_count() == 3, "Vulcan fleet should have increased to 3"
        print("    -> Fleet counts verified: Haven: 1, Vulcan: 3, Earth: 2")

        # Hop 2: Vulcan Forge (World 2) -> Earth Core (World 1)
        print("[*] Hop 2: Forwarding consist from Vulcan Forge Gate 20 (Tile 262849)...")
        v_ids = s2.get_front_engine_ids()
        dep2 = None
        for vid in (v_ids if v_ids else [10, 12]):
            mark2 = s2.line_count()
            s2.send_cmd(f"federation_dispatch {vid} 262849")
            dep2 = s2.wait_for_log("[Federation] Consist departed", timeout=5.0, start_idx=mark2)
            if dep2 is not None:
                break
        assert dep2 is not None, f"Vulcan Forge failed to log forward departure: {s2.get_lines()}"
        print(f"    -> Consist departed Vulcan: {dep2}")

        mark1 = s1.line_count()
        mat1 = s1.wait_for_log("[Federation] Consist materialized", timeout=12.0, start_idx=mark1)
        assert mat1 is not None, f"Earth Core failed to materialize consist: {s1.get_lines()}"
        print(f"    -> Consist materialized on Earth Core: {mat1}")

        # Assert final fleet adjustments on Hop 2
        assert s2.get_train_count() == 2, "Vulcan fleet should have returned to 2"
        assert s1.get_train_count() == 3, "Earth fleet should have increased to 3"
        print("    -> Final multi-hop fleet distribution: Haven: 1, Vulcan: 2, Earth: 3")

        code, ledger = http_json(f"{self.cluster.auth_url}/ledger/status")
        assert code == 200 and ledger.get("total_transfers_completed", 0) >= 2
        print(Color.ok(f"[+] Scenario 2 PASSED: Multi-hop transit completed ({ledger['total_transfers_completed']} transfers verified)."))
        return True

    # -------------------------------------------------------------------------
    # Scenario 3: Staging Siding Latency Divert & Recovery
    # -------------------------------------------------------------------------
    def test_scenario_3_staging_siding_divert(self):
        print("\n" + "-" * 70)
        print(Color.bold("[Scenario 3] Network Latency Staging Siding Divert & Auto-Release"))
        print("-" * 70)

        s1 = self.cluster.servers[1]
        s2 = self.cluster.servers[2]

        # 1. Inject high simulated network latency to World 2 on Server 1 (350ms > 250ms threshold)
        print("[*] Injecting high network latency to World 2 (350 ms > 250 ms threshold)...")
        mark1 = s1.line_count()
        s1.send_cmd("federation_set_world 2 online 350")
        assert s1.wait_for_log("Updated World 2: status=online, ping=350ms", timeout=5.0, start_idx=mark1) is not None

        # 2. Attempt dispatch of a train into Gate 10 towards World 2
        s1_ids = s1.get_front_engine_ids()
        dispatch_vid = s1_ids[0] if s1_ids else 11
        print(f"[*] Attempting consist dispatch (Train {dispatch_vid}) towards high-latency World 2...")
        mark1 = s1.line_count()
        s1.send_cmd(f"federation_dispatch {dispatch_vid} 262814")

        # 3. Assert train was diverted to staging siding instead of wormhole
        divert_log = s1.wait_for_log("diverted to staging siding", timeout=5.0, start_idx=mark1)
        assert divert_log is not None, f"Train {dispatch_vid} failed to divert to staging: {s1.get_lines()}"
        print(f"    -> Siding Divert Confirmed: {divert_log}")

        # Verify train remains safely on Server 1
        assert s1.get_train_count() == 3, "Train must not despawn when diverted to siding"
        print("    -> Mainline preserved: Train held safely in staging siding (fleet count remains 3).")

        # 4. Clear simulated latency (restore to 20ms)
        print("[*] Clearing network latency (restoring to 20 ms)...")
        mark1 = s1.line_count()
        s1.send_cmd("federation_set_world 2 online 20")
        assert s1.wait_for_log("Updated World 2: status=online, ping=20ms", timeout=5.0, start_idx=mark1) is not None

        # 5. Assert automatic holding release restores train
        rel_log = s1.wait_for_log("Released 1 held trains", timeout=8.0, start_idx=mark1)
        assert rel_log is not None, f"Held train failed to release: {s1.get_lines()}"
        print(f"    -> Staging Auto-Release Confirmed: {rel_log}")

        # 6. Dispatch cleared consist into portal
        mark1 = s1.line_count()
        s1.send_cmd(f"federation_dispatch {dispatch_vid} 262814")
        dep1 = s1.wait_for_log("[Federation] Consist departed", timeout=8.0, start_idx=mark1)
        assert dep1 is not None
        print(f"    -> Consist now safely departed into cleared portal: {dep1}")

        mark2 = s2.line_count()
        mat2 = s2.wait_for_log("[Federation] Consist materialized", timeout=12.0, start_idx=mark2)
        assert mat2 is not None
        print(f"    -> Consist successfully materialized on Vulcan Forge: {mat2}")

        print(Color.ok("[+] Scenario 3 PASSED: Holding divert and release verified."))
        return True

    # -------------------------------------------------------------------------
    # Scenario 4: Destination Server Crash, Quarantine, Restart & Reconciliation
    # -------------------------------------------------------------------------
    def test_scenario_4_crash_recovery(self):
        print("\n" + "-" * 70)
        print(Color.bold("[Scenario 4] Destination Crash Recovery & Automatic Journal Reconciliation"))
        print("-" * 70)

        s2 = self.cluster.servers[2] # Vulcan Forge (source)
        s3 = self.cluster.servers[3] # Haven Rim (destination)

        # 1. Dispatch consist from Vulcan Forge (Gate 21) towards Haven Rim (World 3)
        # Configure generous transit delay (3.5s) so we can kill Haven while in-transit
        mark2 = s2.line_count()
        s2.send_cmd("federation_link_gate 262463 3 30 21 15 2")
        assert s2.wait_for_log("Registered inter-server portal link (ID 21)", timeout=5.0, start_idx=mark2) is not None

        print("[*] Dispatching consist from Vulcan Forge towards Haven Rim...")
        v2_ids = s2.get_front_engine_ids()
        dep_log = None
        for vid in (v2_ids if v2_ids else [10, 11, 12]):
            mark2 = s2.line_count()
            s2.send_cmd(f"federation_dispatch {vid} 262463")
            dep_log = s2.wait_for_log("[Federation] Consist departed", timeout=5.0, start_idx=mark2)
            if dep_log is not None:
                break
        assert dep_log is not None, f"Vulcan failed to log departure: {s2.get_lines()}"
        print(f"    -> In-transit departure confirmed: {dep_log}")

        # 2. Kill destination server (Haven Rim) mid-transit using SIGKILL
        print("[*] Abruptly killing Haven Rim server mid-transit (SIGKILL)...")
        s3.kill()
        assert not s3.is_alive(), "Haven server should be terminated!"
        print("    -> Haven Rim server crashed successfully.")

        # 3. Trigger Quarantine on Universe Authority
        print("[*] Invoking Authority Quarantine for in-flight cargo destined for Haven Rim...")
        code, q_res = http_json(f"{self.cluster.auth_url}/transfers/quarantine", method="POST", data={
            "world_id": 3,
            "reason": "Destination Haven Rim node crashed unexpectedly"
        })
        assert code == 200, f"Quarantine failed: {q_res}"
        q_count = q_res.get("quarantined_count", 0)
        assert q_count >= 1, f"Expected at least 1 quarantined transfer, got {q_count}"
        print(f"    -> Transfers secured in Authority Quarantine Bay: {q_count} ({q_res.get('transfer_ids')})")

        # 4. Verify no premature emergence while quarantined
        code, pend = http_json(f"{self.cluster.auth_url}/transfers/pending?dest_world=3")
        assert code == 200 and len(pend.get("pending_transfers", [])) == 0
        print("    -> Emergence locked: 0 pending transfers visible to quarantined destination.")

        # 5. Reboot Haven Rim server from savegame
        print("[*] Rebooting Haven Rim dedicated server from savegame...")
        s3.start()
        time.sleep(1.5)
        mark3 = s3.line_count()
        s3.send_cmd("federation_authority")
        assert s3.wait_for_log(f"Universe Authority URL: '{self.cluster.auth_url}'", timeout=5.0, start_idx=mark3) is not None
        mark3 = s3.line_count()
        s3.send_cmd("federation_link_gate 262814 2 21 30 5 3")
        assert s3.wait_for_log("Registered inter-server portal link (ID 30)", timeout=5.0, start_idx=mark3) is not None
        print("    -> Haven Rim rebooted and reconnected to Authority.")

        # 6. Release Authority Quarantine
        print("[*] Releasing Authority Quarantine for recovered Haven Rim...")
        code, r_res = http_json(f"{self.cluster.auth_url}/transfers/recover", method="POST", data={"world_id": 3})
        assert code == 200, f"Recovery release failed: {r_res}"
        assert r_res.get("recovered_count", 0) >= 1
        print(f"    -> Quarantined transfers released to active queue: {r_res.get('recovered_count')}")

        # 7. Unpause and await automated journal reconciliation & materialization
        mark3 = s3.line_count()
        s3.send_cmd("unpause")
        mat_log = s3.wait_for_log("[Federation] Consist materialized", timeout=12.0, start_idx=mark3)
        assert mat_log is not None, f"Haven Rim failed to reconcile and materialize consist: {s3.get_lines()}"
        print(f"    -> Reconciled Consist Materialization Confirmed: {mat_log}")

        print(Color.ok("[+] Scenario 4 PASSED: Crash recovery and journal reconciliation verified."))
        return True

    # -------------------------------------------------------------------------
    # Scenario 5: Commodity Conservation Ledger Audit
    # -------------------------------------------------------------------------
    def test_scenario_5_commodity_conservation(self):
        print("\n" + "-" * 70)
        print(Color.bold("[Scenario 5] Empire-Wide Commodity Conservation Ledger Audit"))
        print("-" * 70)

        # Allow any in-flight transits to complete and clear from wormhole corridors
        deadline = time.time() + 8.0
        while time.time() < deadline:
            code, audit = http_json(f"{self.cluster.auth_url}/ledger/audit_detailed")
            if code == 200 and audit.get("total_transfers_in_transit", 0) == 0:
                break
            time.sleep(0.4)

        code, audit = http_json(f"{self.cluster.auth_url}/ledger/audit_detailed")
        assert code == 200, f"Failed to query detailed audit: {audit}"

        print("[*] Detailed Universe Commodity Conservation Ledger:")
        print(f"    - Total Transfers Initiated: {audit.get('total_transfers_initiated', 0)}")
        print(f"    - Total Transfers Completed: {audit.get('total_transfers_completed', 0)}")
        print(f"    - Total In-Transit:         {audit.get('total_transfers_in_transit', 0)}")
        print(f"    - Total Quarantined:        {audit.get('total_transfers_quarantined', 0)}")
        print(f"    - Conservation Status:      {audit.get('conserved', False)}")

        assert audit.get("total_transfers_completed", 0) >= 4, "Expected >= 4 completed transfers"
        assert audit.get("total_transfers_in_transit", 0) == 0, "No transfers should be left in-transit"
        assert audit.get("total_transfers_quarantined", 0) == 0, "No transfers should be left in quarantine"
        assert audit.get("conserved", False) is True, "Commodity conservation must hold strictly!"

        print(Color.ok("[+] Scenario 5 PASSED: 100% Commodity Conservation strictly verified (0 leaks)."))
        return True


# =============================================================================
# CLI Operations (Start, Status, Stop)
# =============================================================================
def do_start(config_file=None):
    fixtures_dir = Path("fixtures/cluster")
    earth_sav = fixtures_dir / "cluster_earth.sav"
    if not earth_sav.exists():
        subprocess.run([sys.executable, "scripts/setup_cluster_fixtures.py"], check=True)

    manager = ClusterManager(dynamic_ports=False)
    manager.start_authority()
    manager.bootstrap_authority_topology()
    manager.start_servers()
    manager.configure_gates_and_sidings()

    pid_data = {
        "authority": {
            "pid": manager.auth_proc.pid if manager.auth_proc else None,
            "port": manager.auth_port,
            "url": manager.auth_url
        },
        "servers": {
            wid: {
                "pid": srv.proc.pid if srv.proc else None,
                "port": srv.port,
                "name": srv.name
            } for wid, srv in manager.servers.items()
        }
    }

    pid_file = Path(PID_FILE_PATH)
    pid_file.parent.mkdir(parents=True, exist_ok=True)
    with open(pid_file, "w") as f:
        json.dump(pid_data, f, indent=2)

    print("\n" + "=" * 70)
    print(Color.ok("[+] Cluster successfully launched and running in background!"))
    print(f"    Universe Authority: {manager.auth_url}")
    for wid, srv in manager.servers.items():
        print(f"    World {wid} ({srv.name}): 127.0.0.1:{srv.port}")
    print(f"    PID record saved to {PID_FILE_PATH}")
    print("=" * 70)


def do_status():
    pid_file = Path(PID_FILE_PATH)
    if not pid_file.exists():
        print(Color.warn(f"No cluster PID file found at {PID_FILE_PATH}. Is the cluster running?"))
        # Still check default ports
        auth_url = f"http://127.0.0.1:{DEFAULT_AUTHORITY_PORT}"
    else:
        with open(pid_file, "r") as f:
            pids = json.load(f)
        auth_url = pids.get("authority", {}).get("url", f"http://127.0.0.1:{DEFAULT_AUTHORITY_PORT}")

    print("=" * 75)
    print(Color.bold(" OpenSpaceTTD Live Multi-Node Cluster Status Dashboard"))
    print("=" * 75)

    code, worlds = http_json(f"{auth_url}/directory/worlds")
    if code != 200:
        print(Color.err(f"[!] Universe Authority unreachable at {auth_url}"))
        return

    print(f"{Color.bold('Authority:')} {auth_url}  [{Color.ok('ONLINE')}]")
    print("\n" + Color.bold("WORLD DIRECTORY NODES:"))
    print(f"{'ID':<4} {'Name':<18} {'Phase':<10} {'Address':<18} {'Status':<12} {'Clients':<8} {'Trains':<8}")
    print("-" * 75)

    for w in worlds:
        status_col = Color.ok(w.get("status", "online")) if w.get("status") == "online" else Color.err(w.get("status"))
        print(f"{w.get('world_id'):<4} {w.get('name', 'World'):<18} {w.get('phase', 0):<10} "
              f"{w.get('address', '-'):<18} {status_col:<20} {w.get('active_clients', 0):<8} {w.get('active_trains', 0):<8}")

    code, corridors = http_json(f"{auth_url}/corridors/list")
    if code == 200 and corridors:
        print("\n" + Color.bold("ACTIVE FREIGHT CORRIDORS:"))
        print(f"{'Route':<6} {'Name':<30} {'From -> To':<18} {'Congestion':<12} {'In-Transit':<10}")
        print("-" * 75)
        for c in corridors:
            path_str = f"W{c.get('source_world')}:G{c.get('source_gate', 1)} -> W{c.get('dest_world')}:G{c.get('dest_gate', 1)}"
            print(f"{c.get('route_id'):<6} {c.get('name', 'Corridor'):<30} {path_str:<18} "
                  f"{c.get('congestion_level', 'Clear'):<12} {c.get('current_in_transit_count', 0):<10}")

    code, audit = http_json(f"{auth_url}/ledger/audit_detailed")
    if code == 200:
        print("\n" + Color.bold("EMPIRE COMMODITY CONSERVATION LEDGER:"))
        print(f"  Transfers:  Initiated={audit.get('total_transfers_initiated', 0)}, "
              f"Completed={audit.get('total_transfers_completed', 0)}, "
              f"In-Transit={audit.get('total_transfers_in_transit', 0)}, "
              f"Quarantined={audit.get('total_transfers_quarantined', 0)}")
        print(f"  Cargo:      Initiated={audit.get('total_cargo_initiated', 0)}, "
              f"Completed={audit.get('total_cargo_completed', 0)}, "
              f"In-Transit={audit.get('total_cargo_in_transit', 0)}")
        cons_str = Color.ok("CONSERVED (0 Leaks)") if audit.get("conserved", False) else Color.err("VIOLATION DETECTED")
        print(f"  Integrity:  {cons_str}")

    print("=" * 75)


def do_stop():
    pid_file = Path(PID_FILE_PATH)
    if not pid_file.exists():
        print(Color.warn("No cluster PID file found. Terminating any running openttd/authority processes..."))
        return

    with open(pid_file, "r") as f:
        data = json.load(f)

    # Stop servers
    for wid_str, s_info in data.get("servers", {}).items():
        pid = s_info.get("pid")
        if pid:
            try:
                os.kill(pid, signal.SIGTERM)
                print(f"[+] Sent SIGTERM to World {wid_str} (PID {pid})")
            except OSError:
                pass

    # Stop authority
    auth_pid = data.get("authority", {}).get("pid")
    if auth_pid:
        try:
            os.kill(auth_pid, signal.SIGTERM)
            print(f"[+] Sent SIGTERM to Authority (PID {auth_pid})")
        except OSError:
            pass

    pid_file.unlink(missing_ok=True)
    print(Color.ok("[+] Cluster stopped and PID file removed."))


# =============================================================================
# Main Dispatcher
# =============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="OpenSpaceTTD Horizon A Live Multi-Node Cluster Testbed",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--start", action="store_true", help="Launch cluster in background")
    parser.add_argument("--status", action="store_true", help="Display cluster status dashboard")
    parser.add_argument("--stop", action="store_true", help="Stop running cluster instances")
    parser.add_argument("--test-all", action="store_true", help="Run full 5-scenario acceptance suite")
    parser.add_argument("--test-multi-hop", action="store_true", help="Run Scenario 2: Multi-Hop transit")
    parser.add_argument("--test-staging", action="store_true", help="Run Scenario 3: Staging siding divert")
    parser.add_argument("--test-crash-recovery", action="store_true", help="Run Scenario 4: Crash recovery")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose engine log streaming")

    args = parser.parse_args()

    if args.start:
        do_start()
    elif args.status:
        do_status()
    elif args.stop:
        do_stop()
    elif args.test_all or args.test_multi_hop or args.test_staging or args.test_crash_recovery:
        suite = ClusterAcceptanceSuite(verbose=args.verbose)
        success = suite.run_all()
        sys.exit(0 if success else 1)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
