#!/usr/bin/env python3
"""Inject real Blueprint file operation failures in isolated Linux test processes."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("test_binary")
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("The test-only LD_PRELOAD interposer requires Linux.")
    root = Path(__file__).resolve().parent.parent
    binary = Path(args.test_binary).resolve()
    case = "Blueprint storage injected IO failures preserve the prior file and memory"
    with tempfile.TemporaryDirectory(prefix="ost-bp-io-faults-") as directory:
        library = Path(directory) / "faults.so"
        subprocess.run(["cc", "-shared", "-fPIC", str(root / "src/tests/blueprint_io_faults.c"), "-o", str(library), "-ldl"], check=True, timeout=30)
        for operation in ("open", "write", "fsync", "close", "rename", "link"):
            env = os.environ.copy()
            env["LD_PRELOAD"] = str(library)
            env["OST_BP_FAULT_OP"] = operation
            result = subprocess.run([str(binary), case], cwd=root, env=env, capture_output=True, text=True, timeout=30)
            print(f"{operation}: {'PASS' if result.returncode == 0 else 'FAIL'}", flush=True)
            print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, file=sys.stderr, end="")
            if result.returncode != 0:
                return result.returncode
    print("Six real IO failure points preserve old bytes and memory, survive reopen, and allow retry: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
