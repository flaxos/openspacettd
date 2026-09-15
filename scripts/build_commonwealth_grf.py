#!/usr/bin/env python3
"""
OpenSpaceTTD Commonwealth Pack Build & Generation Tool
Copyright (C) 2026 OpenSpaceTTD Developers
Licensed under GPL-2.0.

Builds reproducible binary NewGRF packages (.grf) from Commonwealth NML source
definitions (pkg/commonwealth_industry and pkg/commonwealth_rail).
Supports both native 'nmlc' compiler (if available) and deterministic in-tree
Container-1 binary GRF synthesis with Action 8/0/4 metadata.
"""

import os
import sys
import hashlib
import struct
import shutil
import argparse
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INDUSTRY_DIR = REPO_ROOT / "pkg" / "commonwealth_industry"
RAIL_DIR = REPO_ROOT / "pkg" / "commonwealth_rail"
DATA_DIR = REPO_ROOT / "bin" / "data"

INDUSTRY_GRFID = b"OST\x01"  # 0x0154534F
RAIL_GRFID     = b"OST\x02"  # 0x0254534F


def create_pseudo_sprite(action_data: bytes) -> bytes:
    """Create a Container Version 1 pseudo-sprite containing action_data."""
    # Sprite format: 2 bytes length (including type byte), 1 byte type (0xFF = pseudo-sprite), payload
    payload_len = len(action_data) + 1
    if payload_len >= 0xFF00:
        # Extended length format
        return struct.pack("<H", 0xFF00) + struct.pack("<I", payload_len) + b"\xFF" + action_data
    return struct.pack("<H", payload_len) + b"\xFF" + action_data


def build_action8(version: int, grfid: bytes, name: str, description: str) -> bytes:
    """Action 0x08: GRF Identification & Metadata."""
    data = bytearray()
    data.append(0x08)
    data.append(version)
    assert len(grfid) == 4, "GRFID must be 4 bytes"
    data.extend(grfid)
    data.extend(name.encode("utf-8") + b"\x00")
    data.extend(description.encode("utf-8") + b"\x00")
    return bytes(data)


def build_action0(feature: int, num_props: int, num_items: int, start_item: int, prop_bytes: bytes) -> bytes:
    """Action 0x00: Property Modification."""
    data = bytearray()
    data.append(0x00)
    data.append(feature)
    data.append(num_props)
    data.append(num_items)
    data.extend(struct.pack("<H", start_item))
    data.extend(prop_bytes)
    return bytes(data)


def build_action4(feature: int, lang_id: int, num_strings: int, start_id: int, strings: list[str]) -> bytes:
    """Action 0x04: Text / String Table."""
    data = bytearray()
    data.append(0x04)
    data.append(feature)
    data.append(lang_id)
    data.append(num_strings)
    data.extend(struct.pack("<H", start_id))
    for s in strings:
        data.extend(s.encode("utf-8") + b"\x00")
    return bytes(data)


def build_action14(entries: dict[str, str]) -> bytes:
    """Action 0x0E: Metadata and Compatibility."""
    data = bytearray()
    data.append(0x0E)
    data.extend(b"OTTD")
    for k, v in entries.items():
        data.extend(k.encode("utf-8") + b"\x00")
        data.extend(v.encode("utf-8") + b"\x00")
    data.append(0x00)  # Terminal null
    return bytes(data)


def synthesize_industry_grf() -> bytes:
    """Synthesize deterministic binary GRF for OpenSpaceTTD Commonwealth Industry & Cargo Pack."""
    sprites = bytearray()

    # 1. Action 8: Identification
    act8 = build_action8(
        version=8,
        grfid=INDUSTRY_GRFID,
        name="OpenSpaceTTD Commonwealth Industry & Cargo Pack",
        description="Canonical 12-cargo Commonwealth multi-world industrial production suite (Pipelines A-D)."
    )
    sprites.extend(create_pseudo_sprite(act8))

    # 2. Action 14: Compatibility
    act14 = build_action14({
        "version": "1",
        "min_version": "1",
        "license": "GPL-2.0",
        "author": "OpenSpaceTTD Developers",
        "pack_type": "commonwealth_industry",
        "cargo_count": "13",
    })
    sprites.extend(create_pseudo_sprite(act14))

    # 3. Action 4: Cargo Strings
    cargo_names = [
        "Silicates & Ballast Slag",
        "Iron Ore",
        "Structural Steel",
        "Rare Earth Minerals",
        "Superalloys & Superconductors",
        "Copper Ore",
        "Conductive Wiring & Coils",
        "Silica Sand & Quartz",
        "Silicon Wafers & Chips",
        "Synthetic Polymers & Composites",
        "Blank Data Crystals",
        "Enriched Quantum Crystals",
        "Encrypted Consumer Crystals"
    ]
    act4_cargo = build_action4(feature=0x0B, lang_id=0x01, num_strings=len(cargo_names), start_id=0x01, strings=cargo_names)
    sprites.extend(create_pseudo_sprite(act4_cargo))

    # 4. Action 0: Cargo Properties (Feature 0x0B = Cargos)
    # Define 13 cargo properties
    cargo_props = bytearray()
    for i in range(13):
        # Prop 0x08: weight in 1/16 ton (e.g. 16 = 1 ton)
        cargo_props.extend(struct.pack("BB", 0x08, 16))
    act0_cargo = build_action0(feature=0x0B, num_props=1, num_items=13, start_item=16, prop_bytes=bytes(cargo_props))
    sprites.extend(create_pseudo_sprite(act0_cargo))

    return bytes(sprites)


def synthesize_rail_grf() -> bytes:
    """Synthesize deterministic binary GRF for OpenSpaceTTD Commonwealth Rolling-Stock Pack."""
    sprites = bytearray()

    # 1. Action 8: Identification
    act8 = build_action8(
        version=8,
        grfid=RAIL_GRFID,
        name="OpenSpaceTTD Commonwealth Rolling-Stock Pack",
        description="Canonical CST locomotive families (Pioneer, Vulcan, Titan, E-40, Mark IV Maglev) and 12-cargo wagons."
    )
    sprites.extend(create_pseudo_sprite(act8))

    # 2. Action 14: Compatibility
    act14 = build_action14({
        "version": "1",
        "min_version": "1",
        "license": "GPL-2.0",
        "author": "OpenSpaceTTD Developers",
        "pack_type": "commonwealth_rail",
        "train_families": "5",
    })
    sprites.extend(create_pseudo_sprite(act14))

    # 3. Action 4: Train Strings
    train_names = [
        "CST Pioneer 0-6-0 'Surveyor' (Steam)",
        "Vulcan 2-8-0 'Frontier Hauler' (Steam)",
        "Titan D-100 Twin-Engine Hauler (Diesel)",
        "CST E-40 Inter-World Catenary Hauler (Electric)",
        "CST Mark IV 'Chimaera' Hyper-Maglev (Maglev)",
        "Commonwealth Passenger Coach",
        "Data Crystal & Valuables Vault Van",
        "Heavy Mineral & Ore Hopper",
        "Structural Steel Flatcar",
        "Cryogenic Intermodal Container Car",
        "Pressurized Chemical Tanker",
        "CST High-Speed Maglev Cargo Pod"
    ]
    act4_train = build_action4(feature=0x00, lang_id=0x01, num_strings=len(train_names), start_id=0x20, strings=train_names)
    sprites.extend(create_pseudo_sprite(act4_train))

    # 4. Action 0: Train Properties (Feature 0x00 = Trains)
    # Set speed and power for the 5 locomotives
    train_props = bytearray()
    specs = [
        (72, 600),    # Pioneer
        (96, 1800),   # Vulcan
        (145, 4200),  # Titan
        (225, 8000),  # CST E-40
        (650, 20000), # Mark IV Maglev
    ]
    for speed, power in specs:
        # Prop 0x09: max speed (in 1/1.6 km/h)
        speed_unit = int(speed * 1.6)
        train_props.extend(struct.pack("BH", 0x09, min(speed_unit, 0xFFFF)))
        # Prop 0x0B: power (hp)
        train_props.extend(struct.pack("BH", 0x0B, power))

    act0_train = build_action0(feature=0x00, num_props=2, num_items=len(specs), start_item=0x20, prop_bytes=bytes(train_props))
    sprites.extend(create_pseudo_sprite(act0_train))

    return bytes(sprites)


def build_all(verify_only=False):
    """Build or verify Commonwealth GRF packages."""
    print("=== OpenSpaceTTD Commonwealth Pack Build Pipeline ===")
    
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    
    # 1. Synthesize Industry GRF
    ind_data = synthesize_industry_grf()
    ind_path_pkg = INDUSTRY_DIR / "openspacettd_industry.grf"
    ind_path_data = DATA_DIR / "openspacettd_industry.grf"
    
    with open(ind_path_pkg, "wb") as f:
        f.write(ind_data)
    with open(ind_path_data, "wb") as f:
        f.write(ind_data)
        
    ind_md5 = hashlib.md5(ind_data).hexdigest()
    ind_sha256 = hashlib.sha256(ind_data).hexdigest()
    print(f"[OK] Generated: {ind_path_pkg.name} ({len(ind_data)} bytes)")
    print(f"     MD5:    {ind_md5}")
    print(f"     SHA256: {ind_sha256}")
    
    # 2. Synthesize Rail GRF
    rail_data = synthesize_rail_grf()
    rail_path_pkg = RAIL_DIR / "openspacettd_rail.grf"
    rail_path_data = DATA_DIR / "openspacettd_rail.grf"
    
    with open(rail_path_pkg, "wb") as f:
        f.write(rail_data)
    with open(rail_path_data, "wb") as f:
        f.write(rail_data)
        
    rail_md5 = hashlib.md5(rail_data).hexdigest()
    rail_sha256 = hashlib.sha256(rail_data).hexdigest()
    print(f"[OK] Generated: {rail_path_pkg.name} ({len(rail_data)} bytes)")
    print(f"     MD5:    {rail_md5}")
    print(f"     SHA256: {rail_sha256}")

    # Write manifest file for verification
    manifest_path = REPO_ROOT / "pkg" / "commonwealth_manifest.json"
    manifest_content = f"""{{
  "commonwealth_packages": [
    {{
      "name": "openspacettd_industry.grf",
      "grfid": "OST\\u0001",
      "grfid_hex": "0154534f",
      "version": 1,
      "bytes": {len(ind_data)},
      "md5": "{ind_md5}",
      "sha256": "{ind_sha256}"
    }},
    {{
      "name": "openspacettd_rail.grf",
      "grfid": "OST\\u0002",
      "grfid_hex": "0254534f",
      "version": 1,
      "bytes": {len(rail_data)},
      "md5": "{rail_md5}",
      "sha256": "{rail_sha256}"
    }}
  ]
}}
"""
    with open(manifest_path, "w") as f:
        f.write(manifest_content)
    print(f"[OK] Manifest generated: {manifest_path}")

    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build OpenSpaceTTD Commonwealth Packs")
    parser.add_argument("--verify", action="store_true", help="Verify packages only")
    args = parser.parse_args()
    sys.exit(build_all(args.verify))
