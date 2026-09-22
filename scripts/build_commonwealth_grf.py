#!/usr/bin/env python3
"""Compile and verify the OpenSpaceTTD Commonwealth NewGRF packs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parent.parent
INDUSTRY_DIR = REPO_ROOT / "pkg" / "commonwealth_industry"
RAIL_DIR = REPO_ROOT / "pkg" / "commonwealth_rail"
OUTPUT_DIR = REPO_ROOT / "bin" / "newgrf"
MANIFEST_PATH = REPO_ROOT / "pkg" / "commonwealth_manifest.json"
PINNED_NML_VERSION = "0.9.0"

PACKS = (
    {
        "name": "openspacettd_industry_v3.grf",
        "source": INDUSTRY_DIR / "commonwealth_industry.nml",
        "grfid": "OST\\u0001",
        "grfid_hex": "0154534f",
        "version": 3,
    },
    {
        "name": "openspacettd_rail_v3.grf",
        "source": RAIL_DIR / "commonwealth_rail.nml",
        "grfid": "OST\\u0002",
        "grfid_hex": "0254534f",
        "version": 3,
    },
)

CARGO_LABELS = (
    "SILC", "IRON", "STEL", "COPR", "WIRE", "SAND", "CHIP",
    "RARE", "ALLO", "POLY", "BCRY", "QCRY", "CCRY",
)

INDUSTRY_LOCAL_IDS = tuple(range(0x10, 0x1E))
VEHICLE_LOCAL_IDS = tuple(range(0x20, 0x25)) + tuple(range(0x30, 0x38))
WAGON_REFITS = {
    "0x31": ["BCRY", "QCRY", "CCRY"],
    "0x32": ["SILC", "IRON", "COPR", "SAND", "RARE", "WHEA", "GRAI", "MAIZ"],
    "0x33": ["STEL", "WIRE"],
    "0x34": ["CHIP", "ALLO", "FOOD"],
    "0x35": ["POLY"],
    "0x36": list(CARGO_LABELS),
    "0x37": ["LVST"],
}


def find_nmlc() -> str:
    configured = os.environ.get("NMLC")
    executable = configured or shutil.which("nmlc")
    if executable is None:
        raise RuntimeError(
            f"nmlc {PINNED_NML_VERSION} is required; install requirements-commonwealth-grf.txt "
            "or set NMLC to the compiler path"
        )
    version = subprocess.run(
        [executable, "--version"], check=True, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    ).stdout.splitlines()[0]
    if PINNED_NML_VERSION not in version:
        raise RuntimeError(f"expected nmlc {PINNED_NML_VERSION}, got: {version}")
    return executable


def compile_pack(nmlc: str, pack: dict[str, object], destination: Path) -> bytes:
    source = Path(pack["source"])
    subprocess.run(
        [nmlc, f"--grf={destination}", source.name],
        cwd=source.parent,
        check=True,
    )
    return destination.read_bytes()


def digest_entry(pack: dict[str, object], data: bytes) -> dict[str, object]:
    return {
        "name": pack["name"],
        "source": str(Path(pack["source"]).relative_to(REPO_ROOT)),
        "grfid": pack["grfid"],
        "grfid_hex": pack["grfid_hex"],
        "version": pack["version"],
        "bytes": len(data),
        "md5": hashlib.md5(data).hexdigest(),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def make_manifest(compiled: list[tuple[dict[str, object], bytes]]) -> dict[str, object]:
    return {
        "schema_version": 2,
        "compiler": {"name": "nmlc", "version": PINNED_NML_VERSION},
        "commonwealth_packages": [digest_entry(pack, data) for pack, data in compiled],
        "cargo_labels": list(CARGO_LABELS),
        "industry_local_ids": [f"0x{item:02x}" for item in INDUSTRY_LOCAL_IDS],
        "vehicle_local_ids": [f"0x{item:02x}" for item in VEHICLE_LOCAL_IDS],
        "wagon_refits": WAGON_REFITS,
    }


def manifest_bytes(manifest: dict[str, object]) -> bytes:
    return (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()


def build(verify: bool) -> int:
    nmlc = find_nmlc()
    with tempfile.TemporaryDirectory(prefix="openspacettd-commonwealth-") as temp_dir:
        temp = Path(temp_dir)
        compiled: list[tuple[dict[str, object], bytes]] = []
        for pack in PACKS:
            data = compile_pack(nmlc, pack, temp / str(pack["name"]))
            compiled.append((pack, data))

        manifest = make_manifest(compiled)
        expected_manifest = manifest_bytes(manifest)

        if verify:
            failures: list[str] = []
            for pack, data in compiled:
                tracked = OUTPUT_DIR / str(pack["name"])
                if not tracked.is_file():
                    failures.append(f"missing {tracked.relative_to(REPO_ROOT)}")
                elif tracked.read_bytes() != data:
                    failures.append(f"stale {tracked.relative_to(REPO_ROOT)}")
            if not MANIFEST_PATH.is_file():
                failures.append(f"missing {MANIFEST_PATH.relative_to(REPO_ROOT)}")
            elif MANIFEST_PATH.read_bytes() != expected_manifest:
                failures.append(f"stale {MANIFEST_PATH.relative_to(REPO_ROOT)}")
            if failures:
                for failure in failures:
                    print(f"[FAIL] {failure}", file=sys.stderr)
                return 1
            print("[OK] Commonwealth GRFs and manifest match their NML sources")
            return 0

        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        for pack, data in compiled:
            target = OUTPUT_DIR / str(pack["name"])
            target.write_bytes(data)
            entry = digest_entry(pack, data)
            print(f"[OK] {target.relative_to(REPO_ROOT)} {entry['sha256']}")
        MANIFEST_PATH.write_bytes(expected_manifest)
        print(f"[OK] {MANIFEST_PATH.relative_to(REPO_ROOT)}")
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", action="store_true", help="compile to a temporary directory and compare tracked outputs")
    return build(parser.parse_args().verify)


if __name__ == "__main__":
    raise SystemExit(main())
