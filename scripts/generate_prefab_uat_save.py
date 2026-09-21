#!/usr/bin/env python3
"""OpenSpaceTTD Prefab World Saves Generator & Automated UAT Verification.

Automates the procedural generation and validation of canonical Commonwealth Prefab
world saves (.sav) containing multi-world partitions, interconnected portals,
Big15 lore trade gateways, operational consist fleets, and megacity economies.
"""

import argparse
import os
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path


def run_headless_engine(binary_path: str, commands: list[str], load_save: str = None, timeout: int = 120) -> tuple[int, list[str]]:
    binary = Path(binary_path).resolve()
    if not binary.exists() or not os.access(binary, os.X_OK):
        print(f"Error: OpenSpaceTTD binary not found at '{binary}'. Build first.", file=sys.stderr)
        return 1, []

    cmd = [
        str(binary),
        "-D127.0.0.1:0",
        "-s", "null",
        "-m", "null",
        "-v", "dedicated",
        "-b", "null",
        "-x", "-X",
        "-c", "demo/uat_demo.cfg",
    ]
    if load_save:
        save_file = Path(load_save).resolve()
        if not save_file.is_file():
            print(f"Error: Save file not found at '{save_file}'.", file=sys.stderr)
            return 1, []
        cmd.extend(["-g", str(save_file)])

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    output_lines = []
    msg_queue = queue.Queue()

    def reader():
        for line in proc.stdout:
            msg_queue.put(line)
        msg_queue.put(None)

    threading.Thread(target=reader, daemon=True).start()

    time.sleep(1.5)

    for c in commands:
        if proc.poll() is not None:
            break
        print(f">> {c}")
        proc.stdin.write(c + "\n")
        proc.stdin.flush()
        time.sleep(1.0)

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            line = msg_queue.get(timeout=1.0)
        except queue.Empty:
            if proc.poll() is not None:
                break
            continue
        if line is None:
            break
        print(line, end="", flush=True)
        output_lines.append(line)
        if "Assertion failed" in line or "SIGABRT" in line or "FATAL" in line:
            proc.terminate()
            return 1, output_lines

    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    return proc.returncode, output_lines


def generate_prefab_world(output_path: str, world_count: int = 4, binary_path: str = "./build/openttd") -> bool:
    target = Path(output_path).resolve()
    target.parent.mkdir(parents=True, exist_ok=True)

    commands = [
        f"generate_uat_world \"{target}\" {world_count}",
        "verify_uat_world",
        "quit",
    ]

    print(f"=== Generating Commonwealth Prefab UAT World ({world_count} worlds) ===")
    print(f"Output Target: {target}")

    ret, lines = run_headless_engine(binary_path, commands)
    combined = "".join(lines)

    success = (
        ret == 0
        and "[UAT Generation: SUCCESS]" in combined
        and "[UAT Verification: PASS]" in combined
        and target.is_file()
        and target.stat().st_size > 10000
    )

    if success:
        print(f"\n[OK] Prefab World successfully created: {target} ({target.stat().st_size:,} bytes)")
    else:
        print(f"\n[FAIL] Prefab generation or verification failed (exit code: {ret})", file=sys.stderr)

    return success


def verify_prefab_world(save_path: str, binary_path: str = "./build/openttd") -> bool:
    target = Path(save_path).resolve()
    print(f"=== Verifying Commonwealth UAT Savegame: {target} ===")

    commands = [
        "verify_uat_world",
        "quit",
    ]

    ret, lines = run_headless_engine(binary_path, commands, load_save=str(target))
    combined = "".join(lines)

    passed = (ret == 0 and "[UAT Verification: PASS]" in combined)
    if passed:
        print(f"\n[OK] Verification PASSED: {target} satisfies all Commonwealth UAT invariants.")
    else:
        print(f"\n[FAIL] Verification FAILED for {target}.", file=sys.stderr)

    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="demo/OpenSpaceTTD-Commonwealth-UAT-v1.0.sav", help="Output .sav path")
    parser.add_argument("--worlds", type=int, default=4, help="Number of worlds (3-6)")
    parser.add_argument("--verify", action="store_true", help="Load and verify an existing save without generating")
    parser.add_argument("--binary", default="./build/openttd", help="Path to openttd executable")
    args = parser.parse_args()

    if args.verify:
        ok = verify_prefab_world(args.output, args.binary)
    else:
        ok = generate_prefab_world(args.output, args.worlds, args.binary)

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
