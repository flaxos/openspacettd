#!/usr/bin/env python3
"""
OpenSpaceTTD Cluster Supervisor Daemon (Sprint 19)
Automates multi-instance dedicated server cluster orchestration, dynamic directory
registration, health monitoring, heartbeats, automatic crash quarantine, and failover recovery.
"""

import argparse
import json
import os
import signal
import socket
import subprocess
import sys
import time
import urllib.request
import urllib.error
from pathlib import Path

DEFAULT_CONFIG_PATH = "config/cluster.json"

class ClusterSupervisor:
    def __init__(self, config_path=DEFAULT_CONFIG_PATH, authority_only=False, no_servers=False, state_file=None):
        self.config_path = config_path
        self.authority_only = authority_only
        self.no_servers = no_servers
        self.config = self._load_config()
        self.state_file = state_file or self.config.get("authority", {}).get("state_file")
        self.running = False

        # State tracking
        self.authority_process = None
        self.server_processes = {} # world_id -> {process, log_file, restart_count, last_start, status, config}
        self.authority_base_url = f"http://{self.config['authority']['host']}:{self.config['authority']['port']}"

        # Setup logging directory
        self.log_dir = Path(self.config.get("supervisor", {}).get("log_dir", "logs/cluster"))
        self.log_dir.mkdir(parents=True, exist_ok=True)

    def _load_config(self):
        cfg_file = Path(self.config_path)
        if not cfg_file.exists():
            print(f"[Supervisor] Error: Config file '{self.config_path}' not found.")
            sys.exit(1)
        with open(cfg_file, "r") as f:
            return json.load(f)

    def _is_port_open(self, host, port):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(0.5)
            try:
                s.connect((host, port))
                return True
            except (socket.timeout, ConnectionRefusedError, OSError):
                return False

    def _http_request(self, endpoint, data=None, method=None):
        url = f"{self.authority_base_url}{endpoint}"
        req_data = None
        headers = {}
        if data is not None:
            req_data = json.dumps(data).encode("utf-8")
            headers["Content-Type"] = "application/json"

        req = urllib.request.Request(url, data=req_data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=3.0) as resp:
                raw = resp.read().decode("utf-8")
                return True, json.loads(raw) if raw else {}
        except urllib.error.HTTPError as e:
            err_body = e.read().decode("utf-8") if e.fp else str(e)
            return False, f"HTTP {e.code}: {err_body}"
        except Exception as e:
            return False, str(e)

    def _is_authority_responsive(self):
        try:
            ok, res = self._http_request("/directory/worlds")
            return ok and isinstance(res, list)
        except Exception:
            return False

    # -------------------------------------------------------------------------
    # Universe Authority Management
    # -------------------------------------------------------------------------
    def ensure_authority_running(self):
        host = self.config["authority"]["host"]
        port = self.config["authority"]["port"]

        if self._is_port_open(host, port):
            if self._is_authority_responsive():
                print(f"[Supervisor] Universe Authority is already active and responsive at http://{host}:{port}")
                return True
            else:
                print(f"[Supervisor] Error: Port {host}:{port} is in use by another non-Universe-Authority service!")
                return False

        if not self.config["authority"].get("auto_spawn", True):
            print(f"[Supervisor] Universe Authority not running and auto_spawn is disabled.")
            return False

        print(f"[Supervisor] Launching Universe Authority daemon on {host}:{port}...")
        auth_log = open(self.log_dir / "universe_authority.log", "a")
        cmd = [sys.executable, "scripts/universe_authority.py", "--host", host, "--port", str(port)]
        if self.state_file:
            cmd.extend(["--state-file", str(self.state_file)])
        self.authority_process = subprocess.Popen(cmd, stdout=auth_log, stderr=subprocess.STDOUT)

        # Wait for Authority to become responsive
        for _ in range(30):
            time.sleep(0.2)
            if self._is_authority_responsive():
                print(f"[Supervisor] Universe Authority successfully listening at http://{host}:{port}")
                return True

        print(f"[Supervisor] Failed to reach Universe Authority after launch.")
        return False

    def bootstrap_topology(self):
        print("[Supervisor] Bootstrapping cluster topology with Universe Authority...")
        # 1. Register Worlds
        for w in self.config.get("worlds", []):
            if not w.get("enabled", True):
                continue
            payload = {
                "world_id": w["world_id"],
                "name": w["name"],
                "phase": w.get("phase", 3),
                "address": f"{w['host']}:{w['port']}",
                "description": w.get("description", ""),
                "max_clients": w.get("max_clients", 16),
                "active_clients": 0,
                "status": "online"
            }
            ok, res = self._http_request("/directory/register", payload)
            if ok:
                print(f"  -> Registered World {w['world_id']} ('{w['name']}', Phase {w.get('phase')})")
            else:
                print(f"  -> Error registering World {w['world_id']}: {res}")

        # 2. Register Corridors
        for c in self.config.get("corridors", []):
            payload = {
                "route_id": c["route_id"],
                "source_world": c["source_world"],
                "source_gate": c.get("source_gate", 1),
                "dest_world": c["dest_world"],
                "dest_gate": c.get("dest_gate", 1),
                "transit_delay_sec": c.get("transit_delay_sec", 4.0),
                "max_bandwidth_trains_per_min": c.get("max_bandwidth_trains_per_min", 10),
                "max_active_in_transit": c.get("max_active_in_transit", 8)
            }
            ok, res = self._http_request("/corridors/register", payload)
            if ok:
                print(f"  -> Registered Freight Corridor {c['route_id']} ('{c.get('name', 'Route')}') World {c['source_world']} -> World {c['dest_world']}")
            else:
                print(f"  -> Error registering Corridor {c['route_id']}: {res}")

    # -------------------------------------------------------------------------
    # Server Instance Lifecycle Management
    # -------------------------------------------------------------------------
    def spawn_server(self, world_cfg):
        wid = world_cfg["world_id"]
        binary = self.config.get("supervisor", {}).get("binary_path", "./build/openttd")
        if not Path(binary).exists():
            print(f"[Supervisor] Dedicated server binary '{binary}' not found! Build project first.")
            return False

        host = world_cfg.get("host", "127.0.0.1")
        port = world_cfg.get("port", 3979 + wid)

        # Dedicated headless execution with -D <host>:<port> -x
        # -x prevents saving on exit to keep tests clean and fast
        cmd = [binary, "-D", f"{host}:{port}", "-x"]
        log_path = self.log_dir / f"world_{wid}.log"
        log_file = open(log_path, "a")

        print(f"[Supervisor] Spawning World {wid} ('{world_cfg['name']}') on {host}:{port}...")
        proc = subprocess.Popen(
            cmd,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            preexec_fn=os.setsid if hasattr(os, 'setsid') else None
        )

        self.server_processes[wid] = {
            "process": proc,
            "log_file": log_file,
            "restart_count": self.server_processes.get(wid, {}).get("restart_count", 0),
            "last_start": time.time(),
            "status": "RUNNING",
            "config": world_cfg
        }
        return True

    def start_servers(self):
        if self.no_servers or self.authority_only:
            print("[Supervisor] Skipping game server startup (flags specified).")
            return

        for w in self.config.get("worlds", []):
            if w.get("enabled", True):
                self.spawn_server(w)

    def handle_server_crash(self, wid):
        rec = self.server_processes.get(wid)
        if not rec:
            return

        exit_code = rec["process"].poll()
        print(f"\n[Supervisor] [ALERT] Server World {wid} ('{rec['config']['name']}') crashed with exit code {exit_code}!")
        rec["status"] = "CRASHED"

        # 1. Quarantine in-flight cargo destined for this crashed world
        if self.config.get("supervisor", {}).get("quarantine_on_crash", True):
            print(f"[Supervisor] Quarantining in-flight transfers destined for World {wid}...")
            ok, res = self._http_request("/transfers/quarantine", {
                "world_id": wid,
                "reason": f"Destination node World {wid} crashed with exit code {exit_code}"
            })
            if ok:
                q_count = res.get("quarantined_count", 0)
                print(f"[Supervisor] Successfully secured {q_count} transfers in Quarantine Bay.")
            else:
                print(f"[Supervisor] Error invoking quarantine: {res}")

        # 2. Check restart policy
        restart_cfg = self.config.get("supervisor", {}).get("restart_policy", {})
        if not restart_cfg.get("auto_restart", True):
            print(f"[Supervisor] Auto-restart disabled for World {wid}.")
            return

        max_retries = restart_cfg.get("max_retries", 5)
        if rec["restart_count"] >= max_retries:
            print(f"[Supervisor] [CRITICAL] World {wid} reached maximum restart attempts ({max_retries}). Giving up.")
            rec["status"] = "FAILED"
            return

        rec["restart_count"] += 1
        backoff = restart_cfg.get("initial_delay_sec", 1.0) * (restart_cfg.get("backoff_factor", 1.5) ** (rec["restart_count"] - 1))
        print(f"[Supervisor] Scheduled restart for World {wid} in {backoff:.1f}s (Attempt {rec['restart_count']}/{max_retries})...")

        time.sleep(backoff)
        if self.spawn_server(rec["config"]):
            print(f"[Supervisor] World {wid} successfully restarted.")
            # 3. Recover quarantined consists if enabled
            if self.config.get("supervisor", {}).get("auto_recover_on_restart", True):
                time.sleep(0.5) # Brief pause for process initialization
                print(f"[Supervisor] Releasing quarantined transfers for recovered World {wid}...")
                ok, res = self._http_request("/transfers/recover", {"world_id": wid})
                if ok:
                    r_count = res.get("recovered_count", 0)
                    print(f"[Supervisor] Released {r_count} transfers back to active arrival queue.")
                else:
                    print(f"[Supervisor] Error releasing quarantined transfers: {res}")

    def send_heartbeats(self):
        for wid, rec in self.server_processes.items():
            if rec["status"] == "RUNNING" and rec["process"].poll() is None:
                payload = {
                    "world_id": wid,
                    "active_clients": 0,
                    "active_trains": 0,
                    "status": "online"
                }
                self._http_request("/directory/heartbeat", payload)

    # -------------------------------------------------------------------------
    # Main Supervision Loop
    # -------------------------------------------------------------------------
    def run(self, duration=None):
        self.running = True
        print("=" * 70)
        print(f"[Supervisor] Starting OpenSpaceTTD Cluster: {self.config.get('cluster_name')}")
        print("=" * 70)

        # 1. Authority
        if not self.ensure_authority_running():
            print("[Supervisor] Could not start Universe Authority. Exiting.")
            return False

        # 2. Topology
        self.bootstrap_topology()

        # 3. Servers
        if not self.authority_only:
            self.start_servers()

        print("[Supervisor] Cluster orchestration online and supervising. Press Ctrl+C to terminate.")

        start_time = time.time()
        last_hb = 0.0
        hb_interval = self.config.get("supervisor", {}).get("heartbeat_interval_sec", 3.0)
        check_interval = self.config.get("supervisor", {}).get("health_check_interval_sec", 1.0)

        try:
            while self.running:
                now = time.time()

                # Check duration limit
                if duration and (now - start_time) >= duration:
                    print(f"[Supervisor] Duration limit ({duration}s) reached. Initiating shutdown.")
                    break

                # Send heartbeats
                if (now - last_hb) >= hb_interval:
                    self.send_heartbeats()
                    last_hb = now

                # Monitor server processes
                for wid, rec in list(self.server_processes.items()):
                    if rec["status"] == "RUNNING":
                        code = rec["process"].poll()
                        if code is not None:
                            self.handle_server_crash(wid)

                time.sleep(check_interval)

        except KeyboardInterrupt:
            print("\n[Supervisor] Interrupted by user. Shutting down...")
        finally:
            self.shutdown()

        return True

    def shutdown(self):
        print("\n[Supervisor] Coordinating clean cluster shutdown...")
        self.running = False

        # 1. Stop all server processes
        for wid, rec in self.server_processes.items():
            proc = rec["process"]
            if proc and proc.poll() is None:
                print(f"  -> Terminating World {wid} server (PID {proc.pid})...")
                try:
                    if hasattr(os, 'killpg'):
                        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                    else:
                        proc.terminate()
                except Exception:
                    pass

        # Wait briefly for servers to exit cleanly
        for wid, rec in self.server_processes.items():
            proc = rec["process"]
            if proc and proc.poll() is None:
                try:
                    proc.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    print(f"  -> Killing unresponsive World {wid} server (PID {proc.pid})...")
                    proc.kill()
            if rec.get("log_file") and not rec["log_file"].closed:
                rec["log_file"].close()

        # 2. Stop Universe Authority if we spawned it
        if self.authority_process and self.authority_process.poll() is None:
            print(f"  -> Terminating Universe Authority daemon (PID {self.authority_process.pid})...")
            self.authority_process.terminate()
            try:
                self.authority_process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self.authority_process.kill()

        print("[Supervisor] All cluster processes cleanly terminated.")

    def print_status(self):
        print("=" * 70)
        print(f"OpenSpaceTTD Cluster Status: {self.config.get('cluster_name')}")
        print("=" * 70)
        host = self.config['authority']['host']
        port = self.config['authority']['port']
        auth_up = self._is_port_open(host, port)
        print(f"Universe Authority : {'ONLINE' if auth_up else 'OFFLINE'} (http://{host}:{port})")

        if auth_up:
            ok, worlds = self._http_request("/directory/worlds")
            if ok:
                print("\nRegistered Worlds:")
                for w in worlds:
                    print(f"  [{w['world_id']}] {w['name']:<20} Phase {w['phase']} | Addr: {w['address']:<18} | Status: {w.get('status')}")

            ok, corridors = self._http_request("/corridors/list")
            if ok:
                print("\nActive Freight Corridors:")
                for c in corridors:
                    print(f"  Route #{c['route_id']:<2} W{c['source_world']} -> W{c['dest_world']} | Delay: {c.get('transit_delay_sec')}s | Active: {c.get('current_in_transit_count')}/{c.get('max_active_in_transit')} | Congestion: {c.get('congestion_level')}")

            ok, audit = self._http_request("/ledger/status")
            if ok:
                print("\nLedger & Commodity Conservation:")
                print(f"  Initiated Cargo  : {audit.get('total_cargo_initiated', 0)} units ({audit.get('total_transfers_initiated', 0)} transfers)")
                print(f"  Completed Cargo  : {audit.get('total_cargo_completed', 0)} units ({audit.get('total_transfers_completed', 0)} transfers)")
                print(f"  In-Transit Cargo : {audit.get('total_cargo_in_transit', 0)} units ({audit.get('total_transfers_in_transit', 0)} transfers)")
                print(f"  Conserved Invar. : {'YES (PASSED)' if audit.get('is_conserved') else 'NO (VIOLATION)'}")

            ok, quarantined = self._http_request("/transfers/quarantined")
            if ok and quarantined:
                print(f"\nQuarantine Bay: {len(quarantined)} transfer(s) secured")
                for q in quarantined:
                    print(f"  - {q.get('transfer_id')}: W{q.get('source_world')} -> W{q.get('dest_world')} ({q.get('total_cargo')} units) [{q.get('status_message')}]")
        print("=" * 70)

def main():
    parser = argparse.ArgumentParser(description="OpenSpaceTTD Dedicated Server Cluster Supervisor")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH, help="Path to cluster configuration JSON")
    parser.add_argument("--authority-only", action="store_true", help="Start only the Universe Authority service")
    parser.add_argument("--no-servers", action="store_true", help="Bootstrap authority & routes without spawning game servers")
    parser.add_argument("--state-file", default=None, help="Path to state persistence JSON file for authority")
    parser.add_argument("--status", action="store_true", help="Query and display cluster live status")
    parser.add_argument("--duration", type=float, default=None, help="Run supervisor for N seconds then shut down cleanly")
    parser.add_argument("--run-acceptance", action="store_true", help="Execute the Sprint 29 Federation Acceptance Kit")
    args = parser.parse_args()

    if args.run_acceptance:
        acceptance_script = Path(__file__).parent / "test_sprint29_acceptance_kit.py"
        if not acceptance_script.exists():
            print(f"[Supervisor] Acceptance kit script not found at {acceptance_script}")
            sys.exit(1)
        res = subprocess.run([sys.executable, str(acceptance_script)])
        sys.exit(res.returncode)

    supervisor = ClusterSupervisor(
        config_path=args.config,
        authority_only=args.authority_only,
        no_servers=args.no_servers,
        state_file=args.state_file
    )

    if args.status:
        supervisor.print_status()
        return

    supervisor.run(duration=args.duration)

if __name__ == "__main__":
    main()
