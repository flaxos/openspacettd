#!/usr/bin/env python3
"""Run the bounded WP-11 active-content economy acceptance on disposable saves.

Requires a built OpenSpaceTTD and an installed OpenGFX base set. No external
server or downloads are used. All commands target a fresh loopback server.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import socket
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


class Engine:
    def __init__(self, binary, config, output, name, save=None, year=1950):
        self.log = (output / f"{name}.log").open("w")
        self.lines = []
        self.deadline = time.monotonic() + 120
        self.queue = queue.Queue()
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        args = [str(binary), f"-D127.0.0.1:{port}", "-s", "null", "-m", "null",
                "-x", "-c", str(config), "-G", "11", "-t", str(year), "-g"]
        if save:
            args.append(str(save))
        self.process = subprocess.Popen(args, cwd=ROOT, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        text=True, bufsize=1,
                                        env={**os.environ, "OPENSPACETTD_WORLD_COUNT": "1"})
        threading.Thread(target=self.read, daemon=True).start()
        try:
            if save:
                self.wait('use "help" for more information.')
                self.command("echo WP11_READY", "WP11_READY")
            else:
                self.wait("Map generated, starting game")
        except BaseException:
            self.close()
            raise

    def read(self):
        for line in self.process.stdout:
            self.queue.put(line)
        self.queue.put(None)

    def wait(self, marker):
        deadline = min(self.deadline, time.monotonic() + 45)
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError(f"Timed out waiting for {marker}")
            try:
                line = self.queue.get(timeout=remaining)
            except queue.Empty as exc:
                raise RuntimeError(f"Timed out waiting for {marker}") from exc
            if line is None:
                raise RuntimeError(f"Engine exited while waiting for {marker}; see log")
            self.log.write(line)
            self.log.flush()
            self.lines.append(line.strip())
            if "WP11 FAIL" in line or "CONNECTED FAIL" in line or "Assertion failed" in line or "Saving map failed" in line:
                raise RuntimeError(line.strip())
            if marker in line:
                return line.split(marker, 1)[1].strip()

    def command(self, command, marker):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        return self.wait(marker)

    def state(self):
        return json.loads(self.command("wp11_slice status", "WP11 state "))

    def advance(self):
        state = json.loads(self.command("wp11_slice advance", "WP11 advance "))
        require(state["conserved"], "Cargo ledger did not balance")
        require(state["cash_conserved"], "Cash differs from native charges and revenue")
        require(len(state["trains"]) == 4, "Train consist count changed")
        return state

    def save(self, path):
        self.command(f'save "{path.with_suffix("")}"', "Map successfully saved to")
        require(path.is_file() and path.stat().st_size > 0, "Save was not written")

    def close(self):
        if self.process.poll() is None:
            try:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
            except BrokenPipeError:
                pass
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.log.close()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def console_payload(line):
    return re.sub(r"^\[[^]]+\]\s*", "", line).strip()


def catalog(engine, native_refits=False):
    start = len(engine.lines)
    engine.command("commonwealth_status", "Commonwealth catalog:")
    lines = [console_payload(line) for line in engine.lines[start:]]
    require(any(line.startswith("Commonwealth status: 1") for line in lines), "Content is not active")
    cargos = dict(tuple(map(int, re.findall(r"\d+", line))) for line in lines if line.startswith("Commonwealth cargo "))
    require(set(cargos) == set(range(13)) and len(set(cargos.values())) == 13, "Cargo labels alias or are absent")
    industries = [list(map(int, re.findall(r"\d+", line.replace("true", "1").replace("false", "0")))) for line in lines if line.startswith("Commonwealth industry ")]
    require(len(industries) == 14 and {row[1] for row in industries} == set(range(16, 30)), "Industry catalog differs")
    require(all(row[2] == 1 and row[3] > 0 for row in industries), "Disabled industry or missing layout")
    vehicles = {}
    for line in lines:
        match = re.fullmatch(r"Commonwealth vehicle (\d+) local (\d+) wagon (true|false) refits (\d+)", line)
        if match:
            _, local, wagon, mask = match.groups()
            vehicles[int(local)] = (wagon == "true", int(mask))
    require(set(vehicles) == set(range(32, 37)) | set(range(48, 56 if native_refits else 55)), "Vehicle catalog differs")
    refits = {49: [10, 11, 12], 50: [0, 1, 3, 5, 7], 51: [2, 4], 52: [6, 8], 53: [9], 54: list(range(13))}
    for local, indexes in refits.items():
        expected = sum(1 << cargos[i] for i in indexes)
        actual = vehicles[local][1]
        if native_refits and local in (50, 52):
            require(vehicles[local][0] and actual & ~0xffff == expected and actual & 0xffff, f"Missing native refits for wagon {local}")
        else:
            require(vehicles[local] == (True, expected), f"Wrong refits for wagon {local}")
    require(all(not vehicles[local][0] for local in range(32, 37)), "Locomotive classified as wagon")
    return {"cargo_slots": cargos, "industries": industries, "vehicles": vehicles}


def run(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config = output / "wp11.cfg"
    text = (ROOT / "demo/wp11_slice.cfg").read_text()
    text = text.replace("threaded_saves = true", "threaded_saves = false")
    text = text.replace("autosave_on_exit = true", "autosave_on_exit = false")
    # Pin the freshly built pack files; never load or replace GRFs in an old save.
    for name in ("industry", "rail"):
        text = text.replace(f"openspacettd_{name}.grf =", f"{args.binary.resolve().parent}/newgrf/openspacettd_{name}.grf =")
    config.write_text(text)
    report = {"fixture_version": 1, "advances": [], "passed": False,
              "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
              "pack_sha256": {name: hashlib.sha256((args.binary.resolve().parent / "newgrf" / f"openspacettd_{name}.grf").read_bytes()).hexdigest() for name in ("industry", "rail")}}
    engine = Engine(args.binary.resolve(), config, output, "fresh", year=args.year)
    try:
        report["catalog"] = catalog(engine)
        engine.command("wp11_slice prepare", "WP11 prepared")
        engine.save(output / "construction-ready.sav")
        engine.command("wp11_slice build", "WP11 built")
        report["availability"] = json.loads(engine.command("wp11_slice availability", "WP11 availability "))
        require(report["availability"]["passed"], "Research or phase purchase check failed")
        initial = engine.state()
        require(initial["steel_stock"] == 0 and sum(initial["held"]) == 5, "Unexpected starter materials")
        require(initial["money"] == 100000000 - sum(initial["expenses"]), "Setup cash ledger did not balance")
        report["initial"] = initial
        engine.save(output / "operational.sav")
        engine.command("wp11_slice block", "WP11 blocked")
        for _ in range(160):
            state = engine.advance()
            report["advances"].append(state)
            if any(t["cargo"] and 30 < t["x"] < 140 for t in state["trains"]):
                break
        else:
            raise RuntimeError("No loaded ore train reached the route")
        before_reload = engine.state()
        engine.save(output / "mid-route.sav")
    finally:
        engine.close()
    engine = Engine(args.binary.resolve(), config, output, "reload", output / "mid-route.sav")
    try:
        after_reload = engine.state()
        require(before_reload == after_reload, "Physical cargo, trains, buffers or cash changed on reload")
        report["reload_equal"] = True
        for _ in range(35):
            blocked = engine.advance()
            report["advances"].append(blocked)
            require(blocked["deposited"] == 0, "Cargo crossed the removed track")
        require(blocked["batches"] > 0 and any(t["cargo"] and t["id"] == 3 for t in blocked["trains"]), "Blocked route never carried steel")
        report["blocked_route_conserved"] = True
        engine.command("wp11_slice unblock", "WP11 unblocked")
        deliveries = 0
        last_deposit = after_reload["deposited"]
        for _ in range(240):
            state = engine.advance()
            report["advances"].append(state)
            if state["deposited"] > last_deposit:
                deliveries = (state["deposited"] - after_reload["deposited"]) // 40
                last_deposit = state["deposited"]
            if deliveries >= 3:
                break
        require(deliveries >= 3, "Fewer than three deliveries after reload")
        report["post_reload_deliveries"] = deliveries
        require(state["steel_stock"] >= 10 and state["batches"] > 0, "Native ore never became delivered steel")
        result = json.loads(engine.command("wp11_slice fabricate", "WP11 fabricated "))
        require(result["exact"] and result["depot"], "Construction cost or material debit differs")
        report["fabrication"] = result
        engine.save(output / "completed.sav")
        report["passed"] = True
    finally:
        engine.close()
        report["artifacts"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob("*.sav")}
        (output / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"WP-11 PASS: active catalog, moving cargo, reload, three deliveries, fabrication. Evidence: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--year", type=int, default=1950)
    parser.add_argument("--output", type=Path, default=ROOT / "build/wp11-acceptance-v1")
    args = parser.parse_args()
    try:
        run(args)
    except Exception as error:
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / "failure.json").write_text(json.dumps({"passed": False, "error": str(error)}, indent=2) + "\n")
        raise
