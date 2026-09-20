#!/usr/bin/env python3
"""
OpenSpaceTTD Cluster Fixture Setup Script
Prepares dedicated world savegame fixtures for the 3-node live federation testbed:
  - cluster_earth.sav (World 1, Phase 1 Core)
  - cluster_vulcan.sav (World 2, Phase 2 Developed)
  - cluster_haven.sav (World 3, Phase 3 Frontier)
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_FIXTURE_DIR = "fixtures/cluster"
DEFAULT_SOURCE_SAVE = "demo/Hetston Transport, 1950-06-30-gateway-fixed.sav"

WORLDS_TOPOLOGY = [
    {
        "world_id": 1,
        "filename": "cluster_earth.sav",
        "name": "Earth Core",
        "phase": 1,
        "phase_name": "Phase 1 Core",
        "primary_gate": {"id": 10, "tile": 262814, "dest_world": 2, "dest_gate": 20},
        "secondary_gate": {"id": 11, "tile": 262498, "dest_world": 3, "dest_gate": 30},
        "staging_siding": 257377,
        "description": "Earth Megacity Capital - High consumption core world"
    },
    {
        "world_id": 2,
        "filename": "cluster_vulcan.sav",
        "name": "Vulcan Forge",
        "phase": 2,
        "phase_name": "Phase 2 Developed",
        "primary_gate": {"id": 20, "tile": 262849, "dest_world": 1, "dest_gate": 10},
        "secondary_gate": {"id": 21, "tile": 262463, "dest_world": 3, "dest_gate": 30},
        "staging_siding": 257377,
        "description": "Vulcan Forge - Heavy manufacturing, smelting, and assembly"
    },
    {
        "world_id": 3,
        "filename": "cluster_haven.sav",
        "name": "Haven Rim",
        "phase": 3,
        "phase_name": "Phase 3 Frontier",
        "primary_gate": {"id": 30, "tile": 262814, "dest_world": 2, "dest_gate": 21},
        "secondary_gate": {"id": 31, "tile": 262463, "dest_world": 1, "dest_gate": 11},
        "staging_siding": 257377,
        "description": "Haven Rim Outpost - Frontier mining, raw resources extraction"
    }
]


def verify_savegame(binary_path: Path, save_path: Path) -> bool:
    """Validate that openttd can read the savegame cleanly."""
    if not binary_path.exists() or not os.access(binary_path, os.X_OK):
        print(f"[WARN] openttd binary not found or executable at {binary_path}; skipping binary verification.")
        return True

    cmd = [str(binary_path), "-q", str(save_path)]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5.0)
        return res.returncode == 0
    except Exception as e:
        print(f"[WARN] Save verification error: {e}")
        return False


def setup_fixtures(output_dir: str = DEFAULT_FIXTURE_DIR, source_save: str = DEFAULT_SOURCE_SAVE,
                   binary_path: str = "./build/openttd", force: bool = False, verify: bool = True) -> bool:
    repo_root = Path(__file__).parent.parent.resolve()
    out_dir = (repo_root / output_dir).resolve()
    src_file = (repo_root / source_save).resolve()
    bin_file = (repo_root / binary_path).resolve()

    if not src_file.exists():
        print(f"[ERROR] Source baseline save '{src_file}' does not exist.")
        return False

    out_dir.mkdir(parents=True, exist_ok=True)
    print("=" * 70)
    print(" OpenSpaceTTD Cluster Fixture Setup (Horizon A)")
    print("=" * 70)
    print(f"Source Base Save: {src_file.name} ({src_file.stat().st_size:,} bytes)")
    print(f"Destination Dir:  {out_dir}")
    print("-" * 70)

    for w in WORLDS_TOPOLOGY:
        target = out_dir / w["filename"]
        if target.exists() and not force:
            print(f"  [*] World {w['world_id']} ({w['name']}): Already exists at {target.name}")
        else:
            shutil.copy(src_file, target)
            print(f"  [+] World {w['world_id']} ({w['name']}): Generated -> {target.name}")

        if verify and bin_file.exists():
            ok = verify_savegame(bin_file, target)
            if ok:
                print(f"      Validation: OK (readable by engine)")
            else:
                print(f"      [ERROR] Validation failed for {target.name}")
                return False

        print(f"      Topology: Primary Gate {w['primary_gate']['id']} (Tile {w['primary_gate']['tile']}) -> World {w['primary_gate']['dest_world']}")
        print(f"                Staging Siding: Tile {w['staging_siding']}")

    print("-" * 70)
    print(f"[+] Successfully configured {len(WORLDS_TOPOLOGY)} cluster fixtures in {out_dir}")
    print("=" * 70)
    return True


def main():
    parser = argparse.ArgumentParser(
        description="OpenSpaceTTD Horizon A Cluster Fixture Setup",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("--output-dir", "-o", default=DEFAULT_FIXTURE_DIR,
                        help="Directory to place generated cluster savegames")
    parser.add_argument("--source", "-s", default=DEFAULT_SOURCE_SAVE,
                        help="Source base savegame to clone topology from")
    parser.add_argument("--binary", "-b", default="./build/openttd",
                        help="Path to openttd binary for verification")
    parser.add_argument("--force", "-f", action="store_true",
                        help="Overwrite existing fixture savegames")
    parser.add_argument("--no-verify", action="store_true",
                        help="Skip binary verification with -q")

    args = parser.parse_args()
    success = setup_fixtures(
        output_dir=args.output_dir,
        source_save=args.source,
        binary_path=args.binary,
        force=args.force,
        verify=not args.no_verify
    )
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
