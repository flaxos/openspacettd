#!/usr/bin/env python3
"""Run the opt-in native cab proof on a copied save and private SDL display.

Requires an existing WITH_CAB_VIEW build, SDL2 offscreen support, and installed
base graphics. This never builds, downloads, modifies the supplied save, or
connects to a desktop display. Automated screenshots are not human acceptance.
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


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--save", required=True, type=Path, help="Disposable WP11 fixture save; copied before use")
    parser.add_argument("--output", required=True, type=Path, help="New evidence directory; must not exist")
    parser.add_argument("--test-binary", type=Path, default=ROOT / "build/openttd_test")
    args = parser.parse_args()
    source = args.save.resolve(strict=True)
    binary = args.test_binary.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    save = output / "fixture-copy.sav"
    shutil.copyfile(source, save)
    config = configuration(output)
    config.write_text(config.read_text().replace("[gui]\n", "[gui]\nerrmsg_duration = 0\n"))
    (output / "scripts").mkdir()
    (output / "scripts/game_start.scr").write_text("cab_native_start\n")
    env = {**os.environ, **profile_environment(output), "SDL_VIDEODRIVER": "offscreen",
           "SDL_RENDER_DRIVER": "software", "OPENSPACETTD_WORLD_COUNT": "3",
           "OSTTD_CAB_NATIVE_CONFIG": str(config), "OSTTD_CAB_NATIVE_SAVE": str(save),
           "OSTTD_CAB_NATIVE_OUTPUT": str(output), "OSTTD_CAB_NATIVE_BINARY": str(binary)}
    for key in ("DISPLAY", "WAYLAND_DISPLAY"):
        env.pop(key, None)
    # Pin the same installed OpenGFX files in this process's private search tree.
    baseset = Path(env["XDG_DATA_HOME"]) / "openttd/baseset"
    if baseset.is_symlink():
        baseset.unlink()  # Only the just-created profile symlink is replaced.
    baseset.mkdir(exist_ok=True)
    installed = Path("/usr/share/games/openttd/baseset/opengfx")
    if not installed.is_dir():
        raise RuntimeError("Installed OpenGFX directory unavailable; no download attempted")
    (baseset / "opengfx").symlink_to(installed, target_is_directory=True)
    command = [str(binary), "Native SDL offscreen cab window paints moves masks and closes"]
    report = {"passed": False, "command": command, "source_save": str(source),
              "source_save_sha256": sha256(source), "binary_sha256": sha256(binary),
              "config_sha256": sha256(config), "display": "SDL offscreen / software",
              "human_uat": "Pending", "timeout_seconds": 120,
              "content_sha256": {str(p): sha256(p) for p in sorted(installed.iterdir()) if p.is_file()},
              "newgrf_sha256": {str(p): sha256(p) for p in sorted((ROOT / "bin/newgrf").glob("*.grf"))}}
    started = time.monotonic()
    try:
        with (output / "native-process.log").open("x") as log:
            result = subprocess.run(command, cwd=output, env=env, stdin=subprocess.DEVNULL,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=120)
        report["exit_code"] = result.returncode
        manifest = output / "manifest.tsv"
        report["native_manifest_pass"] = manifest.is_file() and "result\tPASS\n" in manifest.read_text()
        report["passed"] = result.returncode == 0 and report["native_manifest_pass"]
    except subprocess.TimeoutExpired:
        report["error"] = "Native process exceeded 120 second hard stop; subprocess killed"
    finally:
        report["elapsed_seconds"] = time.monotonic() - started
        report["source_save_unchanged"] = sha256(source) == report["source_save_sha256"]
        report["copy_sha256_after"] = sha256(save)
        report["screenshots"] = {p.name: {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in sorted(output.glob("*.png"))}
        (output / "runner-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report.get(key) for key in ("passed", "exit_code", "elapsed_seconds", "source_save_unchanged", "error")}))
    return 0 if report["passed"] and report["source_save_unchanged"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
