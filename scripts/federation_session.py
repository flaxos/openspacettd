#!/usr/bin/env python3
"""Control only the disposable processes recorded by test_federation_multiplayer.py."""
import argparse
import json
import os
from pathlib import Path
import signal
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", required=True, type=Path)
    parser.add_argument("action", choices=("status", "pause", "resume", "reverse", "stop"))
    parser.add_argument("--world", choices=(1, 2), type=int, default=1)
    parser.add_argument("--vehicle", type=int, default=0)
    args = parser.parse_args()
    directory = args.session.resolve()
    manifest = json.loads((directory / "session.json").read_text())
    records = manifest["processes"]
    # Refuse stale manifests/PID reuse rather than touching an unrelated game.
    for record in records:
        command = Path(f"/proc/{record['pid']}/cmdline")
        if not command.exists() or str(directory).encode() not in command.read_bytes():
            raise SystemExit(f"{record['name']} is no longer this session's process; no action taken")
    if args.action == "stop":
        for record in reversed(records):
            os.kill(record["pid"], signal.SIGTERM)
        print("Stopped the recorded test session.")
        return
    for record in records:
        if not record["name"].startswith("server"):
            continue
        if args.action == "reverse" and record["name"] != f"server{args.world}":
            continue
        commands = {"pause": "pause", "resume": "unpause", "status": "federation_trains",
                    "reverse": f"federation_train_action {args.vehicle} reverse"}
        descriptor = os.open(directory / f"{record['name']}.console", os.O_WRONLY | os.O_NONBLOCK)
        try:
            os.write(descriptor, (commands[args.action] + "\n").encode())
        finally:
            os.close(descriptor)
        if args.action == "status":
            time.sleep(.3)
            log = (directory / f"{record['name']}.log").read_text(errors="replace")
            index = log.rfind("Trains on this server:")
            print(record["name"], "port", record["port"])
            print(log[index:] if index != -1 else "\n".join(log.splitlines()[-10:]))
    if args.action != "status":
        print(f"Sent {args.action} to the test server(s).")


if __name__ == "__main__":
    main()
