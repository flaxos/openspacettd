#!/usr/bin/env python3
"""Prove natural gate entry between two dedicated servers with joined game clients.

Uses disposable native-engine fixtures; never calls federation_dispatch. Only the transport-only smoke fixture seeds cargo; scheduled freight uses native production. Every process and log belongs to this invocation.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time
import urllib.request

from federation_checkpoint import digest, publish, validate

ROOT = Path(__file__).resolve().parents[1]


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class Session:
    def __init__(self, output, binary, gui, scheduled=False):
        self.output, self.binary, self.gui = output, binary, gui
        self.scheduled = scheduled
        self.processes = []
        self.servers = []
        self.clients = []
        self.latest_checkpoint = None
        self.url = f"http://127.0.0.1:{free_port()}"

    def spawn(self, name, args, env=None, console=False):
        generation = sum(record["name"] == name for record in self.processes)
        stem = name if generation == 0 else f"{name}-restart{generation}"
        fifo = self.output / f"{stem}.console"
        fd = subprocess.DEVNULL
        if console:
            os.mkfifo(fifo)
            fd = os.open(fifo, os.O_RDWR)
        log = self.output / f"{stem}.log"
        with log.open("w") as stream:
            process = subprocess.Popen(args, cwd=self.output, env=env, stdin=fd,
                                       stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        if console:
            os.close(fd)
        record = {"name": name, "process": process, "log": log, "console": fifo,
                  "args": list(args), "env": env, "has_console": console, "stopped": False}
        self.processes.append(record)
        return record

    @staticmethod
    def text(record):
        return record["log"].read_text(errors="replace")

    def healthy(self):
        for record in self.processes:
            if record["stopped"]:
                continue
            if record["process"].poll() is not None:
                raise RuntimeError(f"{record['name']} exited: {self.text(record)[-2500:]}")
            log = self.text(record).lower()
            if any(marker in log for marker in ("sync error detected", "desync error", "assertion failed",
                                               "not_reached", "crash encountered")):
                raise RuntimeError(f"{record['name']} failed: {self.text(record)[-2500:]}")

    def wait(self, record, marker, start=0, timeout=60):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.healthy()
            text = self.text(record)[start:]
            if "WP11 FAIL" in text or "Federation fixture terrain preflight failed" in text:
                raise RuntimeError(f"Fixture failed: {text}")
            position = text.find(marker)
            # A large diagnostic record can span several stdout writes. The
            # prefix is not a complete response until its newline has arrived.
            if position >= 0 and "\n" in text[position:]:
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

    def setup(self, checkpoint=None):
        if checkpoint is not None:
            validate(checkpoint, self.binary)
            shutil.copyfile(Path(checkpoint) / "authority.json", self.output / "authority.json")
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

        base = base.replace("[construction]", "[construction]\ncommand_pause_level = 3")
        base = base.replace("min_active_clients = 0", "min_active_clients = 1")
        base = base.replace("autosave_on_exit = true", "autosave_on_exit = false")
        base = base.replace("threaded_saves = true", "threaded_saves = false")

        for world in (1, 2):
            port = free_port()
            config = self.output / f"server{world}.cfg"
            config.write_text(base.replace("server_game_type = local", f"server_game_type = local\nserver_name = Federation Native {world}"))
            env = {**os.environ, "OPENSPACETTD_WORLD_COUNT": "1", "OPENSPACETTD_AUTHORITY_URL": self.url}
            game = ["-g", str(Path(checkpoint) / f"world{world}.sav")] if checkpoint is not None else ["-g"]
            server = self.spawn(f"server{world}", [str(self.binary), "-D", f"127.0.0.1:{port}", "-c", str(config),
                                "-x", "-I", "OpenGFX", *game, "-G", "11", "-t", "1950", "-d", "net=2,desync=2"], env, True)
            server["port"] = port
            self.servers.append(server)
            self.wait(server, "paused (", timeout=90)
            if checkpoint is None:
                mode = " scheduled" if self.scheduled else ""
                self.command(server, f"federation_test_fixture {world}{mode}", "Federation fixture ready:")
                self.command(server, f"save {self.output}/server{world}-ready", "Map successfully saved")
            self.api("/worlds/register", {"world_id": world, "name": f"Native {world}", "phase": 3 if self.scheduled and world == 1 else 1,
                                         "address": f"127.0.0.1:{port}"})
            self.api("/portal_links", {"route_id": world, "source_world": world, "dest_world": 3-world,
                                      "source_gate": world*10, "dest_gate": (3-world)*10, "transit_delay_sec": 1.0})
        for world, server in enumerate(self.servers, 1):
            config = self.output / f"client{world}.cfg"
            config.write_text(f"[network]\nclient_name = Native Observer {world}\n[video]\nfullscreen = false\n[gui]\nautosave_on_exit = false\n")
            env = dict(os.environ)
            env.pop("OPENSPACETTD_AUTHORITY_URL", None)
            env["SDL_VIDEO_WINDOW_POS"] = "0,40" if world == 1 else "960,40"
            args = [str(self.binary), "-n", f"127.0.0.1:{server['port']}#255", "-c", str(config), "-x", "-I", "OpenGFX",
                    "-r", "960x700", "-s", "null", "-m", "null", "-d", "net=2,desync=2"]
            if not self.gui:
                env["SDL_VIDEODRIVER"] = "dummy"
                args += ["-v", "sdl"]
            self.clients.append(self.spawn(f"client{world}", args, env))
            self.wait(server, f"Native Observer {world} has joined the game")
            self.join_company(world)
        # Exercise changing a gate while clients are already joined. Metadata
        # changes must travel through the command queue, not server-only mutation.
        for world, server in enumerate(self.servers, 1):
            if checkpoint is None:
                self.command(server, f"federation_link_gate 20530 {3-world} {(3-world)*10} {world*10} 5 {world}")
            if not self.scheduled:
                self.command(server, "unpause")
        self.write_manifest()

    def write_manifest(self):
        data = {"binary": str(self.binary), "binary_sha256": digest(self.binary),
                "authority": self.url, "processes": [{"name": p["name"], "pid": p["process"].pid,
                "port": p.get("port"), "console": str(p["console"]), "log": str(p["log"])}
                for p in self.processes if not p["stopped"]]}
        (self.output / "session.json").write_text(json.dumps(data, indent=2))

    def transport(self, server, action="status"):
        text = self.command(server, f"federation_transport {action}", "Federation transport ")
        return json.loads(next(line.split("Federation transport ", 1)[1]
                               for line in text.splitlines() if "Federation transport " in line))

    def freight(self, server, action=""):
        text = self.command(server, f"federation_freight_status {action}", "Federation freight ")
        return json.loads(next(line.split("Federation freight ", 1)[1]
                               for line in text.splitlines() if "Federation freight " in line))

    def state(self):
        states = [self.freight(server) for server in self.servers]
        for state in states:
            state.pop("audit", None)  # Process-local observers are not savegame state.
        return states

    def wait_deliveries(self, count, *, completed_before=0, timeout=None):
        """Count empty confirmed returns, each paired with a loaded outbound trip."""
        # Native startup and approximately 170 seconds per round trip at normal speed.
        end = time.monotonic() + (timeout if timeout is not None else 240 + count * 240)
        observations = []
        last_returns = completed_before
        while time.monotonic() < end:
            self.healthy()
            authority = json.loads((self.output / "authority.json").read_text())
            transfers = list(authority["transfers"].values())
            outbound = [tx for tx in transfers if tx["source_world"] == 1 and tx["state"] == "COMPLETED"]
            returned = [tx for tx in transfers if tx["source_world"] == 2 and tx["state"] == "COMPLETED"]
            assert all(tx["total_cargo"] > 0 for tx in outbound), outbound
            assert all(tx["total_cargo"] == 0 for tx in returned), returned
            states = self.state()
            trains = [train for state in states for train in state["trains"] if train["global_id"]]
            assert len(trains) <= 1, states
            for world, state in enumerate(states, 1):
                for train in state["trains"]:
                    if not train["global_id"]:
                        continue
                    assert len(train["units"]) == 3, train
                    assert [order["world"] for order in train["orders"]] == [1, 2], train
                    assert train["orders"][0]["load"] == 2, train
                    assert all(unit["owner"] == 0 for unit in train["units"]), train
                    if world == 2:
                        for unit in train["units"]:
                            assert all(packet["origin_world"] == 1 and packet["origin_id"]
                                       for packet in unit["packets"]), train
            if trains:
                observations.append(trains[0]["global_id"])
                assert len(set(observations)) == 1, observations
            if len(returned) > last_returns:
                last_returns = len(returned)
                print(f"Scheduled round trips: {last_returns}; delivered coal: {sum(tx['total_cargo'] for tx in outbound)}", flush=True)
            if len(returned) >= completed_before + count:
                print(f"Completed {len(returned)} scheduled deliveries and returns", flush=True)
                return {"returns": len(returned), "outbound_cargo": sum(tx["total_cargo"] for tx in outbound),
                        "global_id": observations[0], "states": states}
            time.sleep(.3)
        raise RuntimeError(f"Scheduled delivery timeout: {self.state()}")

    def begin_accounting(self):
        """Enable existing read-only native audit hooks on paused servers."""
        return [self.freight(server, "audit-start") for server in self.servers]

    def finish_accounting(self, before, initial_transit=0):
        after = [self.freight(server) for server in self.servers]
        initial_held = sum(s["waiting"] + s["onboard"] for s in before)
        final_held = sum(s["waiting"] + s["onboard"] for s in after)
        audits = [s["audit"] for s in after]
        supplied = sum(a["produced"] - a["unallocated"] for a in audits)
        discarded = sum(a["discarded"] for a in audits)
        consumed = sum(a["consumed"] for a in audits)
        assert initial_held + initial_transit + supplied == final_held + discarded + consumed, (before, after)
        for start, end in zip(before, after):
            assert start["companies"][0]["money"] - end["companies"][0]["money"] == end["audit"]["cash_debits"], (start, end)
        ledger = self.api("/ledger/status")
        assert ledger["is_conserved"] and ledger["total_cargo_in_transit"] == 0, ledger
        return {"initial": before, "final": after, "supplied": supplied, "discarded": discarded,
                "consumed": consumed, "cash_reconciled": True, "ledger": ledger}

    def run_scheduled(self, deliveries):
        self.setup()
        initial = self.begin_accounting()
        assert initial[0]["companies"][0]["global_id"] == initial[1]["companies"][0]["global_id"], initial
        self.resume()
        route = self.wait_deliveries(deliveries)
        checkpoint = self.checkpoint("scheduled-verified", "confirmed-return")
        accounting = self.finish_accounting(initial)
        final = accounting["final"]
        assert final[1]["audit"]["consumed"] == final[1]["companies"][0]["delivered"] == route["outbound_cargo"], final
        assert final[1]["companies"][0]["income"] > initial[1]["companies"][0]["income"], final
        report = {"passed": route["returns"] >= 5, "scope": "scheduled native freight; acceptance requires at least five cycles",
                  "deliveries": route["returns"], "global_id": route["global_id"], "checkpoint": str(checkpoint),
                  "accounting": accounting}
        (self.output / "scheduled-result.json").write_text(json.dumps(report, indent=2))
        status = "PASS" if report["passed"] else "DIAGNOSTIC ONLY"
        print(f"Scheduled freight {status}: {route['returns']} round trips, {accounting['consumed']} coal; cargo and cash reconciled", flush=True)

    def checkpoint(self, name, phase):
        """Freeze physical movement, then drain outstanding HTTP/native commands."""
        for server in self.servers:
            self.command(server, "pause", "paused")
        for server in self.servers:
            self.transport(server, "quiesce")
        end = time.monotonic() + 60
        while True:
            states = [self.transport(server) for server in self.servers]
            if all(state["quiescing"] and state["requests"] == 0 and state["commands"] == 0 for state in states):
                break
            if time.monotonic() >= end:
                raise RuntimeError(f"Checkpoint failed to drain: {states}")
            time.sleep(.1)
        saves = {}
        for world, server in enumerate(self.servers, 1):
            state = self.transport(server)
            assert state["requests"] == 0 and state["commands"] == 0, state
            path = self.output / f"{name}-world{world}.sav"
            self.command(server, f"save {path.with_suffix('')}", "Map successfully saved")
            saves[world] = path
        directory = self.output / name
        publish(directory, self.binary, saves, self.output / "authority.json", phase=phase)
        self.latest_checkpoint = directory
        (self.output / "latest-checkpoint.json").write_text(json.dumps({"path": str(directory)}))
        return directory

    def resume(self):
        for server in self.servers:
            self.transport(server, "resume")
            self.command(server, "unpause")

    @staticmethod
    def stop_record(record):
        record["stopped"] = True
        process = record["process"]
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    def restart(self, record, args=None):
        self.stop_record(record)
        replacement = self.spawn(record["name"], args or record["args"], record["env"], record["has_console"])
        if "port" in record:
            replacement["port"] = record["port"]
        return replacement

    def join_company(self, world):
        server = self.servers[world - 1]
        text = self.command(server, "clients", f"Native Observer {world}")
        client = re.search(rf"Client #(\d+).*Native Observer {world}.*company: (\d+)", text)
        assert client, text
        if client[2] != "1":
            self.command(server, f"move {client[1]} 1")
        text = self.command(server, "clients", f"Native Observer {world}")
        assert re.search(rf"Native Observer {world}.*company: 1\b", text), text

    def reconnect(self, world):
        server = self.servers[world - 1]
        start = len(self.text(server))
        self.clients[world - 1] = self.restart(self.clients[world - 1])
        self.wait(server, f"Native Observer {world} has joined the game", start=start)
        self.join_company(world)
        self.write_manifest()

    def restart_destination(self, checkpoint, world=2):
        self.validate_latest(checkpoint)
        self.stop_record(self.clients[world - 1])
        old = self.servers[world - 1]
        args = old["args"].copy()
        position = args.index("-g")
        if position + 1 < len(args) and not args[position + 1].startswith("-"):
            args[position + 1] = str(Path(checkpoint) / f"world{world}.sav")
        else:
            args.insert(position + 1, str(Path(checkpoint) / f"world{world}.sav"))
        self.servers[world - 1] = self.restart(old, args)
        self.wait(self.servers[world - 1], "paused (", timeout=90)
        self.reconnect(world)

    def validate_latest(self, checkpoint):
        if Path(checkpoint) != self.latest_checkpoint:
            raise ValueError("Recovery is restricted to this session's latest coordinated checkpoint")
        return validate(checkpoint, self.binary)

    def restart_authority(self):
        authority = next(record for record in reversed(self.processes) if record["name"] == "authority")
        self.restart(authority)
        end = time.monotonic() + 10
        while True:
            try:
                self.api("/ledger/status")
                return
            except OSError:
                if time.monotonic() >= end:
                    raise
                time.sleep(.1)

    def reload_checkpoint(self, checkpoint):
        self.validate_latest(checkpoint)
        for record in self.clients + self.servers:
            self.stop_record(record)
        authority = next(record for record in reversed(self.processes) if record["name"] == "authority")
        self.stop_record(authority)
        shutil.copyfile(Path(checkpoint) / "authority.json", self.output / "authority.json")
        self.restart_authority()
        for world in (1, 2):
            self.restart_destination(checkpoint, world)
        self.write_manifest()

    def wait_phase(self, predicate, description, timeout=300):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.healthy()
            states = self.state()
            authority = json.loads((self.output / "authority.json").read_text())
            if predicate(states, authority):
                return states, authority
            time.sleep(.2)
        raise RuntimeError(f"Timed out reaching {description}: {self.state()}")

    def pause(self):
        for server in self.servers:
            self.command(server, "pause", "paused")

    def block_action(self, action):
        self.command(self.servers[1], f"federation_fixture_block {action}")
        end = time.monotonic() + 10
        while self.transport(self.servers[1])["commands"]:
            if time.monotonic() > end:
                raise RuntimeError(f"Obstruction command did not drain: {action}")
            time.sleep(.1)

    def run_scheduled_recovery(self):
        """Each independent fault uses the latest coordinated set, then three cycles."""
        baseline = json.loads((self.output / "scheduled-result.json").read_text())
        assert baseline["passed"] and baseline["deliveries"] >= 5, baseline
        progress = self.output / "recovery-progress.json"
        cases = json.loads(progress.read_text()) if progress.exists() else []
        completed = baseline["deliveries"] + sum(case["additional_deliveries"] for case in cases)
        names = ("blocked-arrival", "loaded-arrival-reload", "custody-reload", "pre-departure-reload",
                 "authority-outage", "destination-restart", "source-restart", "client-reconnect", "complete-reload")
        for name in names:
            if any(case["case"] == name for case in cases):
                continue
            print(f"Recovery case: {name}", flush=True)
            checkpoint = self.latest_checkpoint
            detail = {}
            saved_phase = validate(checkpoint, self.binary)["phase"]
            if name in ("pre-departure-reload", "custody-reload", "loaded-arrival-reload", "authority-outage") and saved_phase != name:
                old_requests = {j["request"] for state in self.state() for j in state["journal"]}
                if name in ("pre-departure-reload", "authority-outage"):
                    self.transport(self.servers[1], "resume")
                elif name == "custody-reload":
                    self.transport(self.servers[0], "resume")
                else:
                    for server in self.servers:
                        self.transport(server, "resume")
                for server in self.servers:
                    self.command(server, "unpause")
                def reached(states, authority):
                    if name in ("pre-departure-reload", "authority-outage"):
                        return any(j["request"].startswith("DEP-") and j["state"] == 0 and j["request"] not in old_requests for j in states[0]["journal"])
                    if name == "custody-reload":
                        return not any(s["trains"] for s in states) and any(t["state"] == "IN_TRANSIT" for t in authority["transfers"].values())
                    return states[1]["onboard"] == 60 and any(j["state"] == 3 and j["request"] not in old_requests for j in states[1]["journal"])
                self.wait_phase(reached, name)
                checkpoint = self.checkpoint(name + "-before", name)
                detail["phase_state"] = self.state()
            before = self.state()
            if name == "destination-restart":
                self.restart_destination(checkpoint)
            elif name == "source-restart":
                self.restart_destination(checkpoint, 1)
            elif name == "client-reconnect":
                self.reconnect(1)
                self.reconnect(2)
            elif name in ("complete-reload", "pre-departure-reload", "custody-reload", "loaded-arrival-reload"):
                self.reload_checkpoint(checkpoint)
            if name not in ("authority-outage", "blocked-arrival"):
                assert self.state() == before, {"case": name, "before": before, "after": self.state()}
            initial = self.begin_accounting()
            # A custody checkpoint holds its cargo in authority rather than either game.
            initial_transit = self.api("/ledger/status")["total_cargo_in_transit"]
            if name == "blocked-arrival":
                self.block_action("clear")
                self.block_action("depot")
                self.block_action("engine")
                blockers = [t for t in self.state()[1]["trains"] if not t["global_id"]]
                assert len(blockers) == 1 and blockers[0]["tile"] == 20529, blockers
                self.resume()
                states, authority = self.wait_phase(lambda st, au: not st[0]["trains"] and
                    any(j["request"].startswith("ARR-") and j["state"] == 0 for j in st[1]["journal"]) and
                    any(t["state"] != "COMPLETED" for t in au["transfers"].values()), "physical blocked arrival")
                pending = [t["transfer_id"] for t in authority["transfers"].values() if t["state"] != "COMPLETED"]
                end = time.monotonic() + 10
                while time.monotonic() < end:
                    self.healthy()
                    assert not any(t["global_id"] for t in self.state()[1]["trains"])
                    time.sleep(.3)
                self.pause()
                for action in ("sell", "clear", "rail"):
                    self.block_action(action)
                detail["blocked_transfers"] = pending
                detail["held_without_materialization_seconds"] = 10
            if name == "authority-outage":
                authority = next(p for p in reversed(self.processes) if p["name"] == "authority")
                self.stop_record(authority)
                self.resume()
                began, samples = time.monotonic(), []
                while time.monotonic() - began < 30:
                    for world, server in enumerate(self.servers, 1):
                        start = time.monotonic()
                        self.transport(server)
                        clients = self.command(server, "clients", f"Native Observer {world}")
                        assert "company: 1" in clients, clients
                        samples.append(time.monotonic() - start)
                    self.healthy()
                    time.sleep(.3)
                assert max(samples) < 3, samples
                self.restart_authority()
                detail.update(outage_seconds=30, maximum_console_response_seconds=max(samples))
                detail["prepared_request"] = next(j["request"] for j in before[0]["journal"] if j["state"] == 0 and j["request"].startswith("DEP-"))
            self.resume()
            result = self.wait_deliveries(3, completed_before=completed)
            assert result["global_id"] == baseline["global_id"], result
            completed = result["returns"]
            if name == "authority-outage":
                transfers = json.loads((self.output / "authority.json").read_text())["transfers"].values()
                matches = [t for t in transfers if t["request_id"] == detail["prepared_request"]]
                assert len(matches) == 1 and matches[0]["state"] == "COMPLETED", matches
                detail["recovered_transfer"] = matches[0]["transfer_id"]
            checkpoint = self.checkpoint(name + "-after", "confirmed-return")
            accounting = self.finish_accounting(initial, initial_transit)
            cases.append({"case": name, "additional_deliveries": 3, "checkpoint": str(checkpoint),
                          "accounting": accounting, **detail})
            (self.output / "recovery-progress.json").write_text(json.dumps(cases, indent=2))
            print(f"Recovery PASS: {name}; cargo and cash reconciled", flush=True)
        report = {"passed": True, "scope": "latest coordinated checkpoints; ordinary native station schedule",
                  "cases": cases, "ledger": self.api("/ledger/status")}
        (self.output / "scheduled-recovery.json").write_text(json.dumps(report, indent=2))
        print("Scheduled recovery matrix PASS", flush=True)

    def resume_recovery(self, checkpoint):
        """Resume only the latest checkpoint of a prior cold-start acceptance run."""
        checkpoint = checkpoint.resolve()
        previous = checkpoint.parent
        candidates = list(previous.glob("*/checkpoint.json"))
        latest = max(candidates, key=lambda path: path.stat().st_mtime_ns).parent
        if latest != checkpoint:
            raise ValueError("Refusing an older checkpoint from the previous acceptance session")
        validate(checkpoint, self.binary)
        baseline = json.loads((previous / "scheduled-result.json").read_text())
        assert baseline["passed"] and baseline["deliveries"] >= 5, baseline
        shutil.copyfile(previous / "scheduled-result.json", self.output / "scheduled-result.json")
        progress = previous / "recovery-progress.json"
        if progress.exists():
            shutil.copyfile(progress, self.output / progress.name)
        self.setup(checkpoint)
        self.latest_checkpoint = checkpoint
        for server in self.servers:
            self.transport(server, "quiesce")
        self.run_scheduled_recovery()

    def run_transport_recovery(self):
        """Transport-only gate, kept separate from scheduled-freight acceptance."""
        self.setup()
        before = self.checkpoint("before-outage", "pre-departure")
        authority = next(record for record in self.processes if record["name"] == "authority")
        self.stop_record(authority)
        self.resume()
        start = time.monotonic()
        samples = []
        while time.monotonic() - start < 30:
            # These responses prove the game thread is responsive throughout.
            for server in self.servers:
                began = time.monotonic()
                state = self.transport(server)
                elapsed = time.monotonic() - began
                assert elapsed < 3, (server["name"], elapsed, state)
                clients = self.command(server, "clients", "IP: 127.0.0.1")
                assert "company: 1" in clients, clients
                samples.append(elapsed)
            self.healthy()
            time.sleep(.5)
        self.restart(authority)
        self.wait(self.servers[1], "[Federation] Consist materialized", timeout=90)
        # Wait for both server journals and the authority to confirm this arrival.
        end = time.monotonic() + 60
        while self.api("/ledger/status")["total_transfers_completed"] != 1:
            self.healthy()
            if time.monotonic() >= end:
                raise RuntimeError("Arrival was not confirmed after authority recovery")
            time.sleep(.2)
        assert not self.fleet(self.servers[0])
        inbound = self.fleet(self.servers[1])
        assert len(inbound) == 1 and inbound[0][3] == 10, inbound
        identity = self.servers[1]["consists"][0]["global_id"]
        checkpoint = self.checkpoint("confirmed-arrival", "confirmed-arrival")
        self.restart_destination(checkpoint)
        restored = self.fleet(self.servers[1])
        assert len(restored) == 1 and restored[0][3] == 10, restored
        assert self.servers[1]["consists"][0]["global_id"] == identity
        self.healthy()
        report = {"passed": True, "scope": "transport-only; scheduled freight not tested",
                  "authority_outage_seconds": 30, "maximum_console_response_seconds": max(samples),
                  "destination_restart": True, "client_reconnect": True, "global_consist_id": identity,
                  "checkpoints": [str(before), str(checkpoint)], "ledger": self.api("/ledger/status")}
        (self.output / "transport-recovery.json").write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2), flush=True)

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
            self.stop_record(record)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--gui", action="store_true", help="Open two visible game clients")
    parser.add_argument("--keep", action="store_true", help="Keep this session paused after a successful run")
    parser.add_argument("--transport-recovery", action="store_true", help="Run the transport outage/restart gate")
    parser.add_argument("--scheduled", action="store_true", help="Run native mine-to-power-station cyclic freight")
    parser.add_argument("--deliveries", type=int, default=5)
    parser.add_argument("--recovery", action="store_true", help="Continue scheduled freight through restart/reconnect cases")
    parser.add_argument("--resume-checkpoint", type=Path, help="Resume recovery from the previous acceptance session latest checkpoint")
    args = parser.parse_args()
    if args.deliveries < 1 or (args.recovery and args.deliveries < 5):
        parser.error("Deliveries must be positive; recovery acceptance requires at least five baseline cycles")
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    session = Session(args.output, args.binary.resolve(), args.gui, args.scheduled)
    passed = False
    try:
        if args.resume_checkpoint:
            if not args.scheduled or not args.recovery:
                parser.error("--resume-checkpoint requires --scheduled --recovery")
            session.resume_recovery(args.resume_checkpoint)
        elif args.scheduled:
            session.run_scheduled(args.deliveries)
            if args.recovery:
                session.run_scheduled_recovery()
        elif args.transport_recovery:
            session.run_transport_recovery()
        else:
            session.run()
        passed = True
    finally:
        session.write_manifest()
        if not (passed and args.keep):
            session.close()


if __name__ == "__main__":
    main()
