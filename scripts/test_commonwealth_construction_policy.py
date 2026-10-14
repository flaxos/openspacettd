#!/usr/bin/env python3
"""Cold persistence using two separate native unit workers per ruleset, no game loop.

Each invocation owns an exclusive directory. Artifacts and worker output are kept
on success or failure, so this cannot overwrite another test or retained save.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("test_binary", type=Path)
    parser.add_argument("--artifact-root", type=Path, required=True)
    args = parser.parse_args()
    binary = args.test_binary.resolve(strict=True)
    args.artifact_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="construction-policy-", dir=args.artifact_root))
    print(f"Native unit fixture artifacts: {root}", flush=True)
    records = []
    try:
        for mode in (0, 1, 2):
            save, state = root / f"mode-{mode}.sav", root / f"mode-{mode}.json"
            env = dict(os.environ, OSTTD_POLICY_SAVE=str(save), OSTTD_POLICY_STATE=str(state),
                       OSTTD_POLICY_MODE=str(mode), OSTTD_TEST_BINARY=str(binary))
            for action in ("writer", "reader"):
                case = f"Construction policy cold fixture {action}"
                try:
                    result = subprocess.run([str(binary), case], env=env, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT, timeout=45, text=True)
                except subprocess.TimeoutExpired as error:
                    output = error.stdout or ""
                    if isinstance(output, bytes):
                        output = output.decode("utf-8", errors="replace")
                    (root / f"mode-{mode}-{action}.log").write_text(output)
                    records.append({"mode": mode, "worker": action, "timed_out": True,
                                    "limit_seconds": 45})
                    raise RuntimeError(f"mode {mode} {action} timed out; partial output retained at {root}") from error
                (root / f"mode-{mode}-{action}.log").write_text(result.stdout)
                records.append({"mode": mode, "worker": action, "returncode": result.returncode})
                if result.returncode:
                    raise RuntimeError(f"mode {mode} {action} failed ({result.returncode}):\n{result.stdout}")
            records.append({"save": save.name, "sha256": hashlib.sha256(save.read_bytes()).hexdigest(),
                            "state": json.loads(state.read_text())})
        print("Classic/v1/v2 cold unit worker load and ordinary command permission: PASS")
    finally:
        (root / "results.json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    main()
