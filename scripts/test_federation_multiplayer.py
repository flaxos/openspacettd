#!/usr/bin/env python3
"""Prove natural gate entry between two dedicated servers with joined game clients.

Uses disposable native-engine fixtures; never calls federation_dispatch. Cargo is
seeded before clients join. Every process and log belongs to this invocation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Session:
    def __init__(self, output, binary, gui):
        self.output, self.binary, self.gui = output, binary, gui
        self.processes = []
        self.servers = []
        self.url = f"http://127.0.0.1:{free_port()}"

    def spawn(self, name, args, env=None, console=False):
        fifo = self.output / f"{name}.console"
        fd = subprocess.DEVNULL
        if console:
            os.mkfifo(fifo)
            fd = os.open(fifo, os.O_RDWR)
        log = self.output / f"{name}.log"
        with log.open("w") as stream:
            process = subprocess.Popen(args, cwd=self.output, env=env, stdin=fd,
                                       stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        if console:
            os.close(fd)
        record = {"name": name, "process": process, "log": log, "console": fifo}
        self.processes.append(record)
        return record

    @staticmethod
    def text(record):
        return record["log"].read_text(errors="replace")

    def healthy(self):
        for record in self.processes:
            if record["process"].poll() is not None:
                raise RuntimeError(f"{record['name']} exited: {self.text(record)[-2500:]}")
            log = self.text(record).lower()
            if any(marker in log for marker in ("sync error detected", "desync error", "assertion failed")):
                raise RuntimeError(f"{record['name']} failed: {self.text(record)[-2500:]}")

    def wait(self, record, marker, start=0, timeout=60):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.healthy()
            text = self.text(record)[start:]
            if marker in text:
                return text
            time.sleep(.1)
        raise RuntimeError(f"Timed out waiting for {record['name']}: {marker}\n{self.text(record)[-2500:]}")

    def command(self, record, text, marker=None):
        start = len(self.text(record))
        fd = os.open(record["console"], os.O_WRONLY | os.O_NONBLOCK)
        os.write(fd, (text + "\n").encode())
        os.close(fd)
        if marker:
            return self.wait(record, marker, start)
        time.sleep(.2)

    def api(self, path, data=None):
        payload = None if data is None else json.dumps(data).encode()
        request = urllib.request.Request(self.url + path, data=payload,
                                         headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(request, timeout=5) as response:
            return json.load(response)

    def fleet(self, server):
        text = self.command(server, "federation_trains", "Total front engines:")
        server["consists"] = [json.loads(line.split("Consist state ", 1)[1]) for line in text.splitlines() if "Consist state " in line]
        return [tuple(map(int, row)) for row in re.findall(
            r"Train ID (\d+): Tile (\d+), Speed (\d+) km/h, Cargo (\d+), Orders (\d+)", text)]

    def setup(self):
        self.spawn("authority", ["python3", str(ROOT / "scripts/universe_authority.py"),
                                  "--host", "127.0.0.1", "--port", self.url.rsplit(":", 1)[1],
                                  "--state-file", str(self.output / "authority.json")])
        end = time.monotonic() + 10
        while True:
            try:
                self.api("/ledger/status")
                break
            except OSError:
                if time.monotonic() > end:
                    raise
                time.sleep(.1)
        base = (ROOT / "demo/wp11_slice.cfg").read_text().split("[newgrf]")[0]

        base = base.replace("min_active_clients = 0", "min_active_clients = 1")
        base = base.replace("autosave_on_exit = true", "autosave_on_exit = false")
        base = base.replace("threaded_saves = true", "threaded_saves = false")

        for world in (1, 2):
            port = free_port()
            config = self.output / f"server{world}.cfg"
            config.write_text(base.replace("server_game_type = local", f"server_game_type = local\nserver_name = Federation Native {world}"))
            env = {**os.environ, "OPENSPACETTD_WORLD_COUNT": "1", "OPENSPACETTD_AUTHORITY_URL": self.url}
            server = self.spawn(f"server{world}", [str(self.binary), "-D", f"127.0.0.1:{port}", "-c", str(config),
                                "-x", "-I", "OpenGFX", "-g", "-G", "11", "-t", "1950", "-d", "net=2,desync=2"], env, True)
            server["port"] = port
            self.servers.append(server)
            self.wait(server, "Game paused (number of players)", timeout=90)
            self.command(server, f"federation_test_fixture {world}", "Federation fixture ready:")
            self.command(server, f"save {self.output}/server{world}-ready", "Map successfully saved")
            self.api("/worlds/register", {"world_id": world, "name": f"Native {world}", "phase": 1,
                                         "address": f"127.0.0.1:{port}"})
            self.api("/portal_links", {"route_id": world, "source_world": world, "dest_world": 3-world,
                                      "source_gate": world*10, "dest_gate": (3-world)*10, "transit_delay_sec": 1.0})
        for world, server in enumerate(self.servers, 1):
            config = self.output / f"client{world}.cfg"
            config.write_text(f"[network]\nclient_name = Native Observer {world}\n[video]\nfullscreen = false\n[gui]\nautosave_on_exit = false\n")
            env = dict(os.environ)
            env.pop("OPENSPACETTD_AUTHORITY_URL", None)
            env["SDL_VIDEO_WINDOW_POS"] = "0,40" if world == 1 else "960,40"
            args = [str(self.binary), "-n", f"127.0.0.1:{server['port']}", "-c", str(config), "-x", "-I", "OpenGFX",
                    "-r", "960x700", "-s", "null", "-m", "null", "-d", "net=2,desync=2"]
            if not self.gui:
                env["SDL_VIDEODRIVER"] = "dummy"
                args += ["-v", "sdl"]
            self.spawn(f"client{world}", args, env)
            self.wait(server, f"Native Observer {world} has joined the game")
            self.command(server, "move 2 1", "has joined Federation Native Test")
        # Exercise changing a gate while clients are already joined. Metadata
        # changes must travel through the command queue, not server-only mutation.
        for world, server in enumerate(self.servers, 1):
            self.command(server, f"federation_link_gate 20530 {3-world} {(3-world)*10} {world*10} 5 {world}")
            self.command(server, "unpause")
        self.write_manifest()

    def write_manifest(self):
        data = {"binary": str(self.binary), "binary_sha256": hashlib.sha256(self.binary.read_bytes()).hexdigest(),
                "authority": self.url, "processes": [{"name": p["name"], "pid": p["process"].pid,
                "port": p.get("port"), "console": str(p["console"])} for p in self.processes]}
        (self.output / "session.json").write_text(json.dumps(data, indent=2))

    def run(self):
        self.setup()
        source, destination = self.servers
        print("Both multiplayer clients joined; waiting for natural loaded departure", flush=True)
        self.wait(source, "[Federation] Consist departed", timeout=120)
        self.wait(destination, "[Federation] Consist materialized", timeout=60)
        assert not self.fleet(source), "Source still owns a physical train"
        inbound = self.fleet(destination)
        assert len(inbound) == 1 and inbound[0][3] == 10, inbound
        # Let the entire consist clear the throat before requesting a native reversal.
        end = time.monotonic() + 60
        while inbound[0][1] % 512 > 40:
            if time.monotonic() > end:
                raise RuntimeError(f"Arrived train did not clear the gate: {inbound}")
            time.sleep(.5)
            inbound = self.fleet(destination)
        detail = destination["consists"][0]
        assert len(detail["units"]) == 2 and all(not unit["hidden"] for unit in detail["units"]), detail
        a, b = detail["units"]
        assert abs(a["x"] - b["x"]) + abs(a["y"] - b["y"]) == 8, detail
        original_identity = detail["global_id"]
        print("Loaded train arrived and drove clear; reversing through native command", flush=True)
        self.command(destination, f"federation_train_action {inbound[0][0]} reverse")
        self.wait(destination, "[Federation] Consist departed", timeout=120)
        self.wait(source, "[Federation] Consist materialized", timeout=60)
        assert not self.fleet(destination), "Destination retained a duplicate train"
        returned = self.fleet(source)
        assert len(returned) == 1 and returned[0][3] == 10, returned
        assert source["consists"][0]["global_id"] == original_identity, source["consists"]
        time.sleep(6)
        # Leave the returned train facing the gate for a visible replay on resume.
        self.command(source, f"federation_train_action {returned[0][0]} reverse")
        for server in self.servers:
            clients = self.command(server, "clients", "IP: 127.0.0.1")
            assert "company: 1" in clients, clients
            self.command(server, "pause")
            self.command(server, f"save {self.output}/{server['name']}-verified", "Map successfully saved")
        ledger = self.api("/ledger/status")
        assert ledger["total_transfers_completed"] == 2, ledger
        assert ledger["is_conserved"] and ledger["total_cargo_in_transit"] == 0, ledger
        self.healthy()
        report = {"passed": True, "natural_entry": True, "two_joined_clients": True,
                  "round_trip": True, "global_consist_id": original_identity,
                  "physical_cargo_returned": returned[0][3], "ledger": ledger}
        (self.output / "result.json").write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2), flush=True)

    def close(self):
        for record in reversed(self.processes):
            process = record["process"]
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--gui", action="store_true", help="Open two visible game clients")
    parser.add_argument("--keep", action="store_true", help="Keep this session paused after a successful run")
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    session = Session(args.output, args.binary.resolve(), args.gui)
    passed = False
    try:
        session.run()
        passed = True
    finally:
        session.write_manifest()
        if not (passed and args.keep):
            session.close()


if __name__ == "__main__":
    main()
