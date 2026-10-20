#!/usr/bin/env python3
"""Run the scripted agent UAT walkthrough on a copied save and private SDL display.

Opens the Facilities window (1x and 2x interface scale), tries Retire and
declines, opens the Megacity window, and, in WITH_CAB_VIEW builds, drives the
cab's look controls and rides a train over a bridge. Every step is a real
screenshot for review. This never builds, downloads, modifies the supplied save,
or connects to a desktop display. Agent review is not human acceptance.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from test_commonwealth_platform_art import ROOT, configuration, profile_environment

DEFAULT_SAVE = ROOT / "docs/audit/2026-10-20/reserve-pilot/checkpoint-24.sav"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--save", type=Path, default=DEFAULT_SAVE, help="Save to walk through; copied before use")
    parser.add_argument("--output", required=True, type=Path, help="New evidence directory; must not exist")
    parser.add_argument("--test-binary", type=Path, default=ROOT / "build/openttd_test")
    args = parser.parse_args()
    source = args.save.resolve(strict=True)
    binary = args.test_binary.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    save = output / "walkthrough-copy.sav"
    shutil.copyfile(source, save)
    config = configuration(output)
    config.write_text(config.read_text().replace("[gui]\n", "[gui]\nerrmsg_duration = 0\n"))
    (output / "scripts").mkdir()
    (output / "scripts/game_start.scr").write_text("agent_uat_start\n")
    env = {**os.environ, **profile_environment(output), "SDL_VIDEODRIVER": "offscreen",
           "SDL_RENDER_DRIVER": "software",
           "OSTTD_AGENT_UAT_CONFIG": str(config), "OSTTD_AGENT_UAT_SAVE": str(save),
           "OSTTD_AGENT_UAT_OUTPUT": str(output), "OSTTD_AGENT_UAT_BINARY": str(binary)}
    for key in ("DISPLAY", "WAYLAND_DISPLAY"):
        env.pop(key, None)
    baseset = Path(env["XDG_DATA_HOME"]) / "openttd/baseset"
    if baseset.is_symlink():
        baseset.unlink()  # Only the just-created profile symlink is replaced.
    baseset.mkdir(exist_ok=True)
    installed = Path("/usr/share/games/openttd/baseset/opengfx")
    if not installed.is_dir():
        raise RuntimeError("Installed OpenGFX directory unavailable; no download attempted")
    (baseset / "opengfx").symlink_to(installed, target_is_directory=True)
    command = [str(binary), "Agent UAT walkthrough captures player-facing windows"]
    report = {"passed": False, "command": command, "source_save": str(source),
              "source_save_sha256": sha256(source), "binary_sha256": sha256(binary),
              "config_sha256": sha256(config), "display": "SDL offscreen / software, 1920x1200",
              "review": "agent", "human_uat": "Pending", "timeout_seconds": 300}
    started = time.monotonic()
    try:
        with (output / "native-process.log").open("x") as log:
            result = subprocess.run(command, cwd=output, env=env, stdin=subprocess.DEVNULL,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=300)
        report["exit_code"] = result.returncode
        manifest = output / "manifest.tsv"
        report["manifest_pass"] = manifest.is_file() and "result\tPASS\n" in manifest.read_text()
        report["passed"] = result.returncode == 0 and report["manifest_pass"]
    except subprocess.TimeoutExpired:
        report["error"] = "Native process exceeded 300 second hard stop; subprocess killed"
    finally:
        report["elapsed_seconds"] = time.monotonic() - started
        report["source_save_unchanged"] = sha256(source) == report["source_save_sha256"]
        report["screenshots"] = {p.name: {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in sorted(output.glob("*.png"))}
        (output / "runner-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report.get(key) for key in ("passed", "exit_code", "elapsed_seconds", "source_save_unchanged", "error")}))
    return 0 if report["passed"] and report["source_save_unchanged"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
