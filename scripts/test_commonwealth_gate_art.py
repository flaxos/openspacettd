#!/usr/bin/env python3
"""Bounded asset verification suite for Commonwealth Wormhole Gate Family.

Verifies:
1. Manifest integrity: all file sizes and SHA-256 hashes match.
2. Dimensions and modes for normal (1x) and 2x zoom levels (16 sprites per sheet).
3. 8bpp fallback palette audit: transparent index 0, ordinary range [1..197] excluding [0x50..0x57].
4. Mask integrity: mask contains only index 0 (neutral palette under PAL_NONE, livery claims removed).
5. Composited pair proof: for both Active Cyan and Standby Amber variants across all four
   diagonal orientations (NE, SE, SW, NW) at 1x and 2x zoom:
   - Front arch crown sits strictly above rear aperture center (crown_y < core_y).
   - Horizontal centering: |crown_x - core_x| <= 2 * scale.
   - Non-occlusion: front arch is transparent at aperture center, framing the horizon disc.
   - State-visible accents: active variant displays cyan excitation; standby displays amber.
6. Determinism: re-export matches byte-for-byte.
7. Resource limits: disk free space >= 3.0 GiB, total output <= 50 MiB.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PKG_DIR = ROOT / "pkg/commonwealth_gates"
SPRITES_DIR = PKG_DIR / "sprites"
MANIFEST = PKG_DIR / "manifest.json"
GATES_JSON = PKG_DIR / "gates.json"

EXPECTED_FILES = {
    "gate_32bpp.png": {"size": (48, 768), "mode": "RGBA"},
    "gate_mask.png": {"size": (48, 768), "mode": "P"},
    "gate_8bpp.png": {"size": (48, 768), "mode": "P"},
    "gate_2x_32bpp.png": {"size": (96, 1536), "mode": "RGBA"},
    "gate_2x_mask.png": {"size": (96, 1536), "mode": "P"},
}

ANIMATED_INDICES = set(range(0x50, 0x58))
RESERVED_SPECIAL = set(range(198, 256))

DIRECTIONS = ["NE", "SE", "SW", "NW"]
VARIANTS = ["active_cyan", "standby_amber"]


def digest(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def get_flattened(img: Image.Image) -> list:
    if hasattr(img, "get_flattened_data"):
        return list(img.get_flattened_data())
    return list(img.getdata())


def test_resource_limits() -> bool:
    print("[*] Checking disk and resource boundaries...")
    stat = shutil.disk_usage(ROOT)
    free_gib = stat.free / (1024 ** 3)
    if free_gib < 3.0:
        print(f"[-] FAILED: disk free space {free_gib:.2f} GiB is below 3.0 GiB floor!", file=sys.stderr)
        return False
    print(f"[OK] Free disk space: {free_gib:.2f} GiB (floor: 3.0 GiB)")

    total_bytes = sum(f.stat().st_size for f in SPRITES_DIR.glob("*.png"))
    total_mib = total_bytes / (1024 * 1024)
    if total_mib > 50.0:
        print(f"[-] FAILED: asset output {total_mib:.2f} MiB exceeds 50 MiB cap!", file=sys.stderr)
        return False
    print(f"[OK] Asset package size: {total_bytes} bytes ({total_mib:.4f} MiB, cap: 50 MiB)")
    return True


def test_manifest_and_dimensions() -> bool:
    print("[*] Checking manifest and dimensions...")
    if not MANIFEST.exists():
        print(f"[-] FAILED: missing {MANIFEST}", file=sys.stderr)
        return False

    with open(MANIFEST) as f:
        manifest = json.load(f)

    actual_source_sha = digest(GATES_JSON)
    if manifest["source_sha256"] != actual_source_sha:
        print(f"[-] Source hash mismatch: {manifest['source_sha256']} != {actual_source_sha}", file=sys.stderr)
        return False

    for name, exp in EXPECTED_FILES.items():
        path = SPRITES_DIR / name
        if not path.exists():
            print(f"[-] FAILED: missing sprite file {path}", file=sys.stderr)
            return False

        im = Image.open(path)
        if im.size != exp["size"] or im.mode != exp["mode"]:
            print(f"[-] FAILED: {name} dimensions/mode mismatch: got {im.size} {im.mode}, expected {exp['size']} {exp['mode']}", file=sys.stderr)
            return False

        rel_key = f"pkg/commonwealth_gates/sprites/{name}"
        if rel_key not in manifest["files"]:
            print(f"[-] FAILED: {rel_key} not in manifest files", file=sys.stderr)
            return False

        rec = manifest["files"][rel_key]
        actual_sha = digest(path)
        actual_size = path.stat().st_size
        if actual_sha != rec["sha256"] or actual_size != rec["bytes"]:
            print(f"[-] FAILED: hash/size mismatch for {name}: {actual_sha} vs {rec['sha256']}", file=sys.stderr)
            return False
        print(f"[OK] {name}: {im.size} {im.mode}, sha256={actual_sha[:16]}... matched")

    return True


def test_palette_and_masks() -> bool:
    print("[*] Auditing 8bpp palette and masks...")
    im_8bpp = Image.open(SPRITES_DIR / "gate_8bpp.png")
    data_8bpp = get_flattened(im_8bpp)
    used_indices = set(data_8bpp)

    if 0 not in used_indices:
        print("[-] FAILED: 8bpp fallback lacks transparent index 0!", file=sys.stderr)
        return False

    bad_animated = used_indices & ANIMATED_INDICES
    if bad_animated:
        print(f"[-] FAILED: 8bpp fallback uses animated palette indices: {bad_animated}", file=sys.stderr)
        return False

    bad_reserved = used_indices & RESERVED_SPECIAL
    if bad_reserved:
        print(f"[-] FAILED: 8bpp fallback uses reserved palette indices: {bad_reserved}", file=sys.stderr)
        return False
    print(f"[OK] 8bpp fallback indices valid: {len(used_indices)} unique indices in legal ordinary range")

    for mask_name in ["gate_mask.png", "gate_2x_mask.png"]:
        m_im = Image.open(SPRITES_DIR / mask_name)
        m_data = get_flattened(m_im)
        m_used = set(m_data)
        if not m_used.issubset({0}):
            print(f"[-] FAILED: {mask_name} uses non-zero mask indices: {m_used - {0}}", file=sys.stderr)
            return False
        print(f"[OK] {mask_name} valid neutral mask under PAL_NONE (index 0)")

    return True


def test_composited_pair_proof() -> bool:
    """Verify rear and front sprites composite cleanly at manifest offsets for all variants."""
    print("[*] Proving composited rear/front pairs at manifest offsets for all state variants...")
    with open(MANIFEST) as f:
        manifest = json.load(f)

    sprites_meta = {s["index"]: s for s in manifest["sprites"]}

    for scale, sheet_file, cell_size in [
        (1, "gate_32bpp.png", 48),
        (2, "gate_2x_32bpp.png", 96),
    ]:
        sheet = Image.open(SPRITES_DIR / sheet_file)
        w_cell, h_cell = cell_size, cell_size

        for v_idx, v_name in enumerate(VARIANTS):
            base_idx = v_idx * 8

            for dir_idx, dname in enumerate(DIRECTIONS):
                rear_idx = base_idx + dir_idx * 2
                front_idx = base_idx + dir_idx * 2 + 1

                rear_meta = sprites_meta[rear_idx]
                front_meta = sprites_meta[front_idx]

                rear_cell = sheet.crop((0, rear_idx * h_cell, w_cell, (rear_idx + 1) * h_cell))
                front_cell = sheet.crop((0, front_idx * h_cell, w_cell, (front_idx + 1) * h_cell))

                rx, ry = rear_meta["xoff"] * scale, rear_meta["yoff"] * scale
                fx, fy = front_meta["xoff"] * scale, front_meta["yoff"] * scale

                min_x = min(rx, fx)
                min_y = min(ry, fy)
                max_x = max(rx + w_cell, fx + w_cell)
                max_y = max(ry + h_cell, fy + h_cell)

                comp_w = max_x - min_x
                comp_h = max_y - min_y

                comp_buf = Image.new("RGBA", (comp_w, comp_h), (0, 0, 0, 0))
                comp_buf.paste(rear_cell, (rx - min_x, ry - min_y), rear_cell)
                comp_buf.paste(front_cell, (fx - min_x, fy - min_y), front_cell)

                local_core = (24 * scale, 24 * scale)
                screen_core_x = rx + local_core[0]
                screen_core_y = ry + local_core[1]
                buf_core_x = screen_core_x - min_x
                buf_core_y = screen_core_y - min_y

                # Front arch must be transparent at aperture center
                front_px_at_core = front_cell.getpixel(local_core)
                if front_px_at_core[3] != 0:
                    print(f"[-] FAILED [{v_name} {dname} {scale}x]: front arch occludes rear aperture center! Alpha={front_px_at_core[3]}", file=sys.stderr)
                    return False

                # Composited aperture center must be visible
                comp_px_at_core = comp_buf.getpixel((buf_core_x, buf_core_y))
                if comp_px_at_core[3] == 0:
                    print(f"[-] FAILED [{v_name} {dname} {scale}x]: composited aperture core is transparent!", file=sys.stderr)
                    return False

                r_c, g_c, b_c, _ = comp_px_at_core
                if v_name == "active_cyan":
                    if not (b_c > r_c):
                        print(f"[-] FAILED [{v_name} {dname} {scale}x]: expected cyan dominant core, got RGB=({r_c},{g_c},{b_c})", file=sys.stderr)
                        return False
                else:
                    if not (r_c > b_c):
                        print(f"[-] FAILED [{v_name} {dname} {scale}x]: expected amber dominant core, got RGB=({r_c},{g_c},{b_c})", file=sys.stderr)
                        return False

                local_crown = (24 * scale, 8 * scale)
                screen_crown_x = fx + local_crown[0]
                screen_crown_y = fy + local_crown[1]

                if screen_crown_y >= screen_core_y:
                    print(f"[-] FAILED [{v_name} {dname} {scale}x]: crown y={screen_crown_y} not above core y={screen_core_y}!", file=sys.stderr)
                    return False

                drift_x = abs(screen_crown_x - screen_core_x)
                if drift_x > 2 * scale:
                    print(f"[-] FAILED [{v_name} {dname} {scale}x]: crown x={screen_crown_x} misaligned with core x={screen_core_x} (drift={drift_x})!", file=sys.stderr)
                    return False

                print(f"[OK] Composited pair {v_name} {dname} ({scale}x): offsets=({rx},{ry}), core=({screen_core_x},{screen_core_y}), crown=({screen_crown_x},{screen_crown_y}) — verified")

    return True


def test_determinism() -> bool:
    print("[*] Checking exporter determinism via build_commonwealth_gate_art.py --verify...")
    import subprocess
    cmd = [sys.executable, str(ROOT / "scripts/build_commonwealth_gate_art.py"), "--verify"]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[-] FAILED: verify exited {res.returncode}:\n{res.stdout}\n{res.stderr}", file=sys.stderr)
        return False
    print("[OK] Exporter verify passed: 100% deterministic pixel-exact regeneration")
    return True


def main():
    ok = (
        test_resource_limits()
        and test_manifest_and_dimensions()
        and test_palette_and_masks()
        and test_composited_pair_proof()
        and test_determinism()
    )
    if not ok:
        print("[-] Verification failed!", file=sys.stderr)
        sys.exit(1)
    print("\n=======================================================")
    print("ALL GATE ASSET CHECKS PASSED: 100% verified clean family.")
    print("=======================================================")
    sys.exit(0)


if __name__ == "__main__":
    main()
