#!/usr/bin/env python3
"""Verify organic UAT geometry, safe legacy recovery, cold reload and SDL drawing."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from test_wp11_slice import Engine, ROOT, require


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(engine):
    result = engine.command("verify_uat_world", "[UAT Verification: ")
    require(result.startswith("PASS]"), result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "build/openttd")
    parser.add_argument("--save", type=Path, action="append")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gui", action="store_true", help="Also run isolated SDL dummy-video rendering smoke tests")
    args = parser.parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    config = output / "recovery.cfg"
    config.write_text("[gui]\nautosave_on_exit = false\nautosave_interval = 0\nthreaded_saves = false\n"
                      "[network]\nmin_active_clients = 1\n[game_creation]\nmap_x = 6\nmap_y = 6\ncst_sector = false\n")
    report = {"binary_sha256": digest(binary), "recovery": [], "generation": [], "gui": [], "passed": False}
    sources = args.save or [ROOT / "demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav"]
    for index, supplied in enumerate(sources):
        source = supplied.resolve()
        source_hash = digest(source)
        engine = Engine(binary, config, output, f"source-{index}", save=source)
        recovered = output / f"recovered-{index}.sav"
        try:
            audit = verify(engine)
            before = engine.command("getdate", "Date: ")
            engine.command("setting network.min_active_clients 0", "unpaused")
            engine.command("unpause", "unpaused")
            deadline = time.monotonic() + 20
            while True:
                time.sleep(1)
                after = engine.command("getdate", "Date: ")
                if after != before:
                    break
                require(time.monotonic() < deadline, "Simulation date did not advance")
            engine.command("setting network.min_active_clients 1", "paused")
            engine.command("pause", "paused")
            saved_audit = verify(engine)
            saved_date = engine.command("getdate", "Date: ")
            engine.save(recovered)
        finally:
            engine.close()
        engine = Engine(binary, config, output, f"reload-{index}", save=recovered)
        try:
            reloaded = verify(engine)
            require(reloaded == saved_audit, "Reload changed verified entity counts")
            require(engine.command("getdate", "Date: ") == saved_date, "Reload changed the date")
        finally:
            engine.close()
        require(digest(source) == source_hash, "Original source changed")
        report["recovery"].append({"source": source.name, "sha256": source_hash, "audit": audit,
                                   "before": before, "after": saved_date, "saved_audit": saved_audit, "reload": reloaded,
                                   "recovered_sha256": digest(recovered)})
    for count in (3, 4, 6):
        engine = Engine(binary, config, output, f"generate-{count}", world_count=1)
        target = output / f"fresh-{count}.sav"
        try:
            result = engine.command(f'generate_uat_world "{target}" {count}', "[UAT Generation: ")
            require(result.startswith("SUCCESS]"), result)
            generated = verify(engine)
        finally:
            engine.close()
        engine = Engine(binary, config, output, f"fresh-reload-{count}", save=target)
        try:
            loaded = verify(engine)
            require(loaded == generated, "Fresh reload changed verified entity counts")
        finally:
            engine.close()
        report["generation"].append({"worlds": count, "audit": generated, "reload": loaded, "sha256": digest(target)})
    if args.gui:
        for index, source in enumerate([*sources, output / "fresh-4.sav"]):
            log = output / f"sdl-{index}.log"
            with log.open("w") as stream:
                process = subprocess.Popen([str(binary), "-v", "sdl", "-s", "null", "-m", "null", "-x",
                    "-c", str(config), "-g", str(source.resolve()), "-r", "2560x1389", "-d", "sl=2"],
                    cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                    env={**os.environ, "SDL_VIDEODRIVER": "dummy"})
                try:
                    time.sleep(8)
                    require(process.poll() is None, f"SDL process exited: {log}")
                finally:
                    # SIGTERM opens the interactive exit confirmation; this disposable
                    # process must not save or wait for input during cleanup.
                    if process.poll() is None:
                        process.kill()
                    process.wait(timeout=10)
            text = log.read_text()
            require("Loading chunk TRAD" in text, "SDL did not load the scenario")
            require(not any(word in text for word in ("Assertion failed", "NOT_REACHED", "Crash encountered")), str(log))
            report["gui"].append({"source": source.name, "seconds": 8, "video": "sdl/dummy", "resolution": "2560x1389"})
    report["passed"] = True
    (output / "evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Organic UAT recovery, geometry, generation and cold reload passed", flush=True)


if __name__ == "__main__":
    main()
