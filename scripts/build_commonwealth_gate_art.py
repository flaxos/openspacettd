#!/usr/bin/env python3
"""Render, verify and package the Commonwealth Wormhole Gate Family art.

Standalone parametric vector renderer using standard Python 3 and Pillow.
Generates four diagonal orientations (NE, SE, SW, NW) with rear/ground and
front/sorted components in two static state variants:
- Active linked state: restrained cyan conduit excitation and horizon core
- Standby unlinked state: restrained amber excitation and horizon core

Total 16 sprites (8 Active + 8 Standby):
- gate_32bpp.png: normal zoom RGBA 32bpp sheet (48x768)
- gate_mask.png: normal zoom recolour mask (48x768)
- gate_8bpp.png: normal zoom native DOS palette fallback (48x768)
- gate_2x_32bpp.png: 2x zoom RGBA 32bpp sheet (96x1536)
- gate_2x_mask.png: 2x zoom recolour mask (96x1536)

No external nml/nmlc dependency required for standalone image production.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import sys

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent.parent
PKG_DIR = ROOT / "pkg/commonwealth_gates"
SPRITES_DIR = PKG_DIR / "sprites"
PLATFORM_8BPP = ROOT / "pkg/commonwealth_platforms/sprites/platform_8bpp.png"

CELL_NORMAL = (48, 48)
CELL_2X = (96, 96)
TOTAL_SPRITES = 16

ORDINARY_INDICES = [i for i in range(1, 198) if i not in range(0x50, 0x58)]


def load_native_palette() -> list[int]:
    """Extract the canonical 768-byte OpenTTD DOS palette from verified platform asset."""
    if PLATFORM_8BPP.exists():
        im = Image.open(PLATFORM_8BPP)
        palette = im.getpalette()
        if palette and len(palette) >= 768:
            return palette[:768]
    raise RuntimeError(f"Could not load reference palette from {PLATFORM_8BPP}")


def parse_colour(hex_val: str) -> tuple[int, int, int]:
    return tuple(bytes.fromhex(hex_val.removeprefix("#")))


def find_closest_palette_index(rgb: tuple[int, int, int], palette: list[int]) -> int:
    """Find closest ordinary DOS palette index using Euclidean distance squared."""
    best_idx = 1
    best_dist = float("inf")
    r, g, b = rgb
    for idx in ORDINARY_INDICES:
        pr = palette[idx * 3]
        pg = palette[idx * 3 + 1]
        pb = palette[idx * 3 + 2]
        dist = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if dist < best_dist:
            best_dist = dist
            best_idx = idx
    return best_idx


def render_gate_sheets(model: dict, scale: int, palette: list[int]) -> tuple[Image.Image, Image.Image]:
    """Render the 16 gate sprite cells directly at the specified scale factor."""
    cell_w = CELL_NORMAL[0] * scale
    cell_h = CELL_NORMAL[1] * scale
    total_h = cell_h * TOTAL_SPRITES

    sheet = Image.new("RGBA", (cell_w, total_h), (0, 0, 0, 0))
    mask = Image.new("P", (cell_w, total_h), 0)
    mask.putpalette(palette)

    draw = ImageDraw.Draw(sheet)
    draw_mask = ImageDraw.Draw(mask)

    materials = {k: parse_colour(v) for k, v in model["materials"].items()}

    def poly(row: int, points: list[tuple[float, float]], mat: str):
        offset_y = row * cell_h
        pts = [(p[0] * scale, p[1] * scale + offset_y) for p in points]
        draw.polygon(pts, fill=(*materials[mat], 255))
        draw_mask.polygon(pts, fill=0)

    def ellipse(row: int, bbox: tuple[float, float, float, float], mat: str):
        offset_y = row * cell_h
        b = (bbox[0] * scale, bbox[1] * scale + offset_y, bbox[2] * scale, bbox[3] * scale + offset_y)
        draw.ellipse(b, fill=(*materials[mat], 255))
        draw_mask.ellipse(b, fill=0)

    def line(row: int, p0: tuple[float, float], p1: tuple[float, float], mat: str, width: float = 1.0):
        offset_y = row * cell_h
        draw.line(
            [(p0[0] * scale, p0[1] * scale + offset_y), (p1[0] * scale, p1[1] * scale + offset_y)],
            fill=(*materials[mat], 255),
            width=max(1, int(round(width * scale))),
        )

    for variant_idx, variant_name in enumerate(["active_cyan", "standby_amber"]):
        base_row = variant_idx * 8
        state_cue = "cyan_active" if variant_name == "active_cyan" else "amber_standby"

        # --- DIRECTION 0: NE (Axis X, track SW -> NE) ---
        # Row base + 0: NE REAR
        r0 = base_row + 0
        poly(r0, [(8, 30), (24, 38), (40, 30), (24, 22)], "graphite")
        poly(r0, [(12, 30), (24, 36), (36, 30), (24, 24)], "track_ballast")
        line(r0, (10, 31), (38, 17), "rail_steel", 1.2)
        line(r0, (14, 33), (42, 19), "rail_steel", 1.2)
        poly(r0, [(10, 33), (24, 40), (38, 33), (33, 29), (24, 34), (15, 29)], "steel_dark")
        poly(r0, [(10, 20), (15, 22), (15, 30), (10, 28)], "steel_dark")
        poly(r0, [(33, 22), (38, 20), (38, 28), (33, 30)], "steel_mid")
        poly(r0, [(17, 20), (24, 16), (31, 20), (31, 28), (24, 32), (17, 28)], "horizon_dark")
        poly(r0, [(19, 21), (24, 18), (29, 21), (29, 27), (24, 30), (19, 27)], "horizon_field")
        poly(r0, [(21, 22), (24, 20), (27, 22), (27, 26), (24, 28), (21, 26)], "horizon_core")
        ellipse(r0, (22, 22, 26, 26), state_cue)
        ellipse(r0, (17, 23, 19, 25), state_cue)
        ellipse(r0, (29, 23, 31, 25), state_cue)

        # Row base + 1: NE FRONT
        r1 = base_row + 1
        poly(r1, [(10, 15), (24, 8), (24, 13), (15, 18)], "steel_dark")
        poly(r1, [(24, 8), (38, 15), (33, 18), (24, 13)], "steel_mid")
        poly(r1, [(10, 15), (10, 33), (15, 29), (15, 18)], "steel_dark")
        poly(r1, [(38, 15), (38, 33), (33, 29), (33, 18)], "steel_light")
        line(r1, (15, 29), (15, 18), "ceramic", 1.5)
        line(r1, (15, 18), (24, 13), "ceramic", 1.5)
        line(r1, (24, 13), (33, 18), "ceramic", 1.5)
        line(r1, (33, 18), (33, 29), "ceramic", 1.5)
        ellipse(r1, (22, 6, 26, 10), state_cue)
        ellipse(r1, (13, 13, 17, 17), state_cue)
        ellipse(r1, (31, 13, 35, 17), state_cue)
        poly(r1, [(9, 21), (10, 21), (10, 27), (9, 27)], "steel_dark")
        poly(r1, [(38, 21), (39, 21), (39, 27), (38, 27)], "steel_mid")

        # --- DIRECTION 1: SE (Axis Y, track NW -> SE) ---
        # Row base + 2: SE REAR
        r2 = base_row + 2
        poly(r2, [(8, 30), (24, 22), (40, 30), (24, 38)], "graphite")
        poly(r2, [(12, 30), (24, 24), (36, 30), (24, 36)], "track_ballast")
        line(r2, (10, 17), (38, 31), "rail_steel", 1.2)
        line(r2, (14, 19), (42, 33), "rail_steel", 1.2)
        poly(r2, [(10, 33), (24, 40), (38, 33), (33, 29), (24, 34), (15, 29)], "steel_mid")
        poly(r2, [(10, 28), (15, 30), (15, 22), (10, 20)], "steel_mid")
        poly(r2, [(33, 30), (38, 28), (38, 20), (33, 22)], "steel_dark")
        poly(r2, [(17, 20), (24, 16), (31, 20), (31, 28), (24, 32), (17, 28)], "horizon_dark")
        poly(r2, [(19, 21), (24, 18), (29, 21), (29, 27), (24, 30), (19, 27)], "horizon_field")
        poly(r2, [(21, 22), (24, 20), (27, 22), (27, 26), (24, 28), (21, 26)], "horizon_core")
        ellipse(r2, (22, 22, 26, 26), state_cue)
        ellipse(r2, (17, 23, 19, 25), state_cue)
        ellipse(r2, (29, 23, 31, 25), state_cue)

        # Row base + 3: SE FRONT
        r3 = base_row + 3
        poly(r3, [(10, 15), (24, 8), (24, 13), (15, 18)], "steel_mid")
        poly(r3, [(24, 8), (38, 15), (33, 18), (24, 13)], "steel_dark")
        poly(r3, [(10, 15), (10, 33), (15, 29), (15, 18)], "steel_light")
        poly(r3, [(38, 15), (38, 33), (33, 29), (33, 18)], "steel_dark")
        line(r3, (15, 29), (15, 18), "ceramic", 1.5)
        line(r3, (15, 18), (24, 13), "ceramic", 1.5)
        line(r3, (24, 13), (33, 18), "ceramic", 1.5)
        line(r3, (33, 18), (33, 29), "ceramic", 1.5)
        ellipse(r3, (22, 6, 26, 10), state_cue)
        ellipse(r3, (13, 13, 17, 17), state_cue)
        ellipse(r3, (31, 13, 35, 17), state_cue)
        poly(r3, [(9, 21), (10, 21), (10, 27), (9, 27)], "steel_light")
        poly(r3, [(38, 21), (39, 21), (39, 27), (38, 27)], "steel_dark")

        # --- DIRECTION 2: SW (Axis X, track NE -> SW) ---
        # Row base + 4: SW REAR
        r4 = base_row + 4
        poly(r4, [(8, 30), (24, 38), (40, 30), (24, 22)], "graphite")
        poly(r4, [(12, 30), (24, 36), (36, 30), (24, 24)], "track_ballast")
        line(r4, (38, 17), (10, 31), "rail_steel", 1.2)
        line(r4, (42, 19), (14, 33), "rail_steel", 1.2)
        poly(r4, [(10, 33), (24, 40), (38, 33), (33, 29), (24, 34), (15, 29)], "steel_dark")
        poly(r4, [(10, 20), (15, 22), (15, 30), (10, 28)], "steel_dark")
        poly(r4, [(33, 22), (38, 20), (38, 28), (33, 30)], "steel_mid")
        poly(r4, [(17, 20), (24, 16), (31, 20), (31, 28), (24, 32), (17, 28)], "horizon_dark")
        poly(r4, [(19, 21), (24, 18), (29, 21), (29, 27), (24, 30), (19, 27)], "horizon_field")
        poly(r4, [(21, 22), (24, 20), (27, 22), (27, 26), (24, 28), (21, 26)], "horizon_core")
        ellipse(r4, (22, 22, 26, 26), state_cue)
        ellipse(r4, (17, 23, 19, 25), state_cue)
        ellipse(r4, (29, 23, 31, 25), state_cue)

        # Row base + 5: SW FRONT
        r5 = base_row + 5
        poly(r5, [(10, 15), (24, 8), (24, 13), (15, 18)], "steel_dark")
        poly(r5, [(24, 8), (38, 15), (33, 18), (24, 13)], "steel_light")
        poly(r5, [(10, 15), (10, 33), (15, 29), (15, 18)], "steel_dark")
        poly(r5, [(38, 15), (38, 33), (33, 29), (33, 18)], "steel_mid")
        line(r5, (15, 29), (15, 18), "ceramic", 1.5)
        line(r5, (15, 18), (24, 13), "ceramic", 1.5)
        line(r5, (24, 13), (33, 18), "ceramic", 1.5)
        line(r5, (33, 18), (33, 29), "ceramic", 1.5)
        ellipse(r5, (22, 6, 26, 10), state_cue)
        ellipse(r5, (13, 13, 17, 17), state_cue)
        ellipse(r5, (31, 13, 35, 17), state_cue)
        poly(r5, [(9, 21), (10, 21), (10, 27), (9, 27)], "steel_dark")
        poly(r5, [(38, 21), (39, 21), (39, 27), (38, 27)], "steel_light")

        # --- DIRECTION 3: NW (Axis Y, track SE -> NW) ---
        # Row base + 6: NW REAR
        r6 = base_row + 6
        poly(r6, [(8, 30), (24, 22), (40, 30), (24, 38)], "graphite")
        poly(r6, [(12, 30), (24, 24), (36, 30), (24, 36)], "track_ballast")
        line(r6, (38, 31), (10, 17), "rail_steel", 1.2)
        line(r6, (42, 33), (14, 19), "rail_steel", 1.2)
        poly(r6, [(10, 33), (24, 40), (38, 33), (33, 29), (24, 34), (15, 29)], "steel_mid")
        poly(r6, [(10, 20), (15, 22), (15, 30), (10, 28)], "steel_mid")
        poly(r6, [(33, 22), (38, 20), (38, 28), (33, 30)], "steel_dark")
        poly(r6, [(17, 20), (24, 16), (31, 20), (31, 28), (24, 32), (17, 28)], "horizon_dark")
        poly(r6, [(19, 21), (24, 18), (29, 21), (29, 27), (24, 30), (19, 27)], "horizon_field")
        poly(r6, [(21, 22), (24, 20), (27, 22), (27, 26), (24, 28), (21, 26)], "horizon_core")
        ellipse(r6, (22, 22, 26, 26), state_cue)
        ellipse(r6, (17, 23, 19, 25), state_cue)
        ellipse(r6, (29, 23, 31, 25), state_cue)

        # Row base + 7: NW FRONT
        r7 = base_row + 7
        poly(r7, [(10, 15), (24, 8), (24, 13), (15, 18)], "steel_light")
        poly(r7, [(24, 8), (38, 15), (33, 18), (24, 13)], "steel_dark")
        poly(r7, [(10, 15), (10, 33), (15, 29), (15, 18)], "steel_mid")
        poly(r7, [(38, 15), (38, 33), (33, 29), (33, 18)], "steel_dark")
        line(r7, (15, 29), (15, 18), "ceramic", 1.5)
        line(r7, (15, 18), (24, 13), "ceramic", 1.5)
        line(r7, (24, 13), (33, 18), "ceramic", 1.5)
        line(r7, (33, 18), (33, 29), "ceramic", 1.5)
        ellipse(r7, (22, 6, 26, 10), state_cue)
        ellipse(r7, (13, 13, 17, 17), state_cue)
        ellipse(r7, (31, 13, 35, 17), state_cue)
        poly(r7, [(9, 21), (10, 21), (10, 27), (9, 27)], "steel_mid")
        poly(r7, [(38, 21), (39, 21), (39, 27), (38, 27)], "steel_dark")

    return sheet, mask


def get_image_flattened(img: Image.Image) -> list:
    """Helper for Pillow cross-version compatibility without deprecation warning."""
    if hasattr(img, "get_flattened_data"):
        return list(img.get_flattened_data())
    return list(img.getdata())


def generate_8bpp_fallback(sheet_32bpp: Image.Image, mask_p: Image.Image, palette: list[int]) -> Image.Image:
    """Convert 32bpp RGBA sheet to 8bpp DOS-palette image with transparent index 0."""
    fallback_im = Image.new("P", sheet_32bpp.size, 0)
    fallback_im.putpalette(palette)

    rgb_cache = {}
    data_32 = get_image_flattened(sheet_32bpp)
    fallback_data = []

    for (r, g, b, a) in data_32:
        if a == 0:
            fallback_data.append(0)
        else:
            rgb = (r, g, b)
            if rgb not in rgb_cache:
                rgb_cache[rgb] = find_closest_palette_index(rgb, palette)
            fallback_data.append(rgb_cache[rgb])

    fallback_im.putdata(fallback_data)
    fallback_im.info["transparency"] = 0
    return fallback_im


def file_digest(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def build_and_save(verify: bool = False) -> int:
    SPRITES_DIR.mkdir(parents=True, exist_ok=True)
    gates_json_path = PKG_DIR / "gates.json"
    manifest_path = PKG_DIR / "manifest.json"

    if not gates_json_path.exists():
        print(f"Error: missing model file {gates_json_path}", file=sys.stderr)
        return 1

    with open(gates_json_path) as f:
        model = json.load(f)

    palette = load_native_palette()

    print("[*] Rendering normal zoom (1x)...")
    sheet_1x, mask_1x = render_gate_sheets(model, scale=1, palette=palette)
    fallback_1x = generate_8bpp_fallback(sheet_1x, mask_1x, palette)

    print("[*] Rendering 2x zoom...")
    sheet_2x, mask_2x = render_gate_sheets(model, scale=2, palette=palette)

    outputs = {
        "gate_32bpp.png": sheet_1x,
        "gate_mask.png": mask_1x,
        "gate_8bpp.png": fallback_1x,
        "gate_2x_32bpp.png": sheet_2x,
        "gate_2x_mask.png": mask_2x,
    }

    if verify:
        print("[*] Verifying existing outputs against regenerated model...")
        all_ok = True
        for name, img in outputs.items():
            path = SPRITES_DIR / name
            if not path.exists():
                print(f"[-] Missing output file: {path}", file=sys.stderr)
                all_ok = False
                continue
            tmp_path = SPRITES_DIR / f".tmp_{name}"
            img.save(tmp_path)
            new_hash = file_digest(tmp_path)
            old_hash = file_digest(path)
            tmp_path.unlink()
            if new_hash != old_hash:
                print(f"[-] Hash mismatch for {name}: {old_hash} != {new_hash}", file=sys.stderr)
                all_ok = False
            else:
                print(f"[OK] {name}: {new_hash[:16]}... matched")
        return 0 if all_ok else 1

    print("[*] Writing sprite sheets...")
    for name, img in outputs.items():
        out_path = SPRITES_DIR / name
        img.save(out_path)
        print(f"    Saved {out_path.name} ({out_path.stat().st_size} bytes)")

    manifest_data = {
        "schema_version": 2,
        "name": model["name"],
        "license": model["license"],
        "source": "pkg/commonwealth_gates/gates.json",
        "source_sha256": file_digest(gates_json_path),
        "geometry": model["geometry"],
        "tools": {
            "Pillow": Image.__version__,
            "Python": sys.version.split()[0],
        },
        "zooms": [
            "normal-8bpp",
            "normal-32bpp",
            "2x-32bpp",
        ],
        "sprites": model["sprites"],
        "files": {},
    }

    total_bytes = 0
    for name in sorted(outputs.keys()):
        p = SPRITES_DIR / name
        size = p.stat().st_size
        sha = file_digest(p)
        manifest_data["files"][f"pkg/commonwealth_gates/sprites/{name}"] = {
            "bytes": size,
            "sha256": sha,
        }
        total_bytes += size

    with open(manifest_path, "w") as f:
        json.dump(manifest_data, f, indent=2)
    print(f"[*] Manifest written to {manifest_path} (total asset bytes: {total_bytes})")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Build Commonwealth Wormhole Gate art assets")
    parser.add_argument("--verify", action="store_true", help="Verify existing outputs match regenerated model")
    args = parser.parse_args()
    sys.exit(build_and_save(verify=args.verify))


if __name__ == "__main__":
    main()
