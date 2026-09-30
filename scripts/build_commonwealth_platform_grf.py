#!/usr/bin/env python3
"""Render, compile and verify the four original Commonwealth platform sprites.

The source is a small parametric model, not a transformed third-party image.
The package deliberately has no station items or simulation properties.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

import PIL
from PIL import Image, ImageDraw
from nml.palette import raw_palette_data


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "pkg/commonwealth_platforms"
GRF_NAME = "openspacettd_platforms_v1.grf"
VERSIONS = {"nmlc": "0.9.0", "Pillow": "12.3.0"}
CELL = (44, 25)
PALETTE = raw_palette_data[0]
RGB = [tuple(PALETTE[i:i + 3]) for i in range(0, 768, 3)]
ORDINARY = [i for i in range(1, 198) if i not in range(0x50, 0x58)]
COMPANY = 201  # Primary company ramp, bounded inset panels only.


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def colour(value: str) -> tuple[int, int, int]:
    return tuple(bytes.fromhex(value.removeprefix("#")))


def palette_index(rgb: tuple[int, int, int]) -> int:
    return min(ORDINARY, key=lambda i: sum((a - b) ** 2 for a, b in zip(rgb, RGB[i])))


def render(model: dict, scale: int) -> tuple[Image.Image, Image.Image]:
    """Paint shared world-space polygons at the native zoom, without resampling."""
    sheet = Image.new("RGBA", (CELL[0] * scale, CELL[1] * scale * 4))
    mask = Image.new("P", sheet.size, 0)
    mask.putpalette(PALETTE)
    materials = {name: colour(value) for name, value in model["materials"].items()}
    length, width, height = model["length"], model["width"], model["height"]
    for row, sprite in enumerate(model["sprites"]):
        xoff = -32 if sprite["axis"] == "X" else -10
        yoff = -2
        draw, mapped = ImageDraw.Draw(sheet), ImageDraw.Draw(mask)

        def point(u: float, v: float, z: float) -> tuple[int, int]:
            x, y = (u, v) if sprite["axis"] == "X" else (v, u)
            return (math.floor((2 * (y - x) - xoff) * scale),
                    math.floor((x + y - z - yoff + row * CELL[1]) * scale))

        def polygon(vertices: list[tuple[float, float, float]], material: str) -> None:
            points = [point(*vertex) for vertex in vertices]
            draw.polygon(points, fill=(*materials[material], 255))
            mapped.polygon(points, fill=COMPANY if material == "company" else 0)

        def top(u0: float, v0: float, u1: float, v1: float, material: str) -> None:
            polygon([(u0, v0, height), (u1, v0, height),
                     (u1, v1, height), (u0, v1, height)], material)

        # Visible faces stay inside the native 16x5x2 / 5x16x2 sorting box.
        polygon([(length, 0, 0), (length, width, 0),
                 (length, width, height), (length, 0, height)],
                "steel_x" if sprite["axis"] == "X" else "steel_y")
        polygon([(0, width, 0), (length, width, 0),
                 (length, width, height), (0, width, height)],
                "steel_y" if sprite["axis"] == "X" else "steel_x")
        top(0, 0, length, width, "deck")
        for u in model["modules"]:
            top(u + 0.4, 0.8, u + 3.6, 4.2, "panel")
            if u:
                top(u, 0, u + 0.18, width, "seam")
        edge = sprite["track_edge"]
        v0, v1 = (0, 0.7) if edge == 0 else (4.3, 5)
        top(0, v0, length, v1, "ceramic")
        # Hazard ticks and service lamps repeat every four units; no animation.
        for u in model["modules"]:
            top(u + 0.7, v0, u + 1.1, v1, "amber")
            top(u + 2.7, 1.1, u + 3.3, 1.55, "cyan")
        # Separate company-colour panels stay away from the platform edge.
        cv0, cv1 = (3.4, 4.2) if edge == 0 else (0.8, 1.6)
        top(5, cv0, 7.5, cv1, "company")
        top(13, cv0, 15.5, cv1, "company")
        top(1.6, 2.7, 2.6, 3.2, "repair")
    return sheet, mask


def fallback(sheet: Image.Image, mask: Image.Image) -> Image.Image:
    indexed = Image.new("P", sheet.size, 0)
    indexed.putpalette(PALETTE)
    colours = {rgb[:3]: palette_index(rgb[:3]) for rgb in set(sheet.get_flattened_data()) if rgb[3]}
    indexed.putdata([m if m else colours[p[:3]] if p[3] else 0
                     for p, m in zip(sheet.get_flattened_data(), mask.get_flattened_data())])
    indexed.info["transparency"] = 0
    return indexed


def nml_source(model: dict) -> str:
    rows = []
    for scale in (1, 2):
        coords = []
        for row, sprite in enumerate(model["sprites"]):
            xoff = -32 if sprite["axis"] == "X" else -10
            coords.append(f"\t[0, {row * CELL[1] * scale}, {CELL[0] * scale}, "
                          f"{CELL[1] * scale}, {xoff * scale}, {-2 * scale}, NOCROP]")
        rows.append("\n".join(coords))
    return '''/* Original OpenSpaceTTD art, GPL-2.0-only; generated from platforms.json. */
grf {
    grfid: "OST\\06";
    name: string(STR_GRF_NAME);
    desc: string(STR_GRF_DESC);
    version: 1;
    min_compatible_version: 1;
}
replace platform_set(1069, "sprites/platform_8bpp.png") {
''' + rows[0] + '''
}
alternative_sprites(platform_set, ZOOM_LEVEL_NORMAL, BIT_DEPTH_32BPP,
                    "sprites/platform_32bpp.png", "sprites/platform_mask.png") {
''' + rows[0] + '''
}
alternative_sprites(platform_set, ZOOM_LEVEL_IN_2X, BIT_DEPTH_32BPP,
                    "sprites/platform_2x_32bpp.png", "sprites/platform_2x_mask.png") {
''' + rows[1] + "\n}\n"


def audit_actions(data: bytes) -> list[int]:
    """Reject compiled simulation actions and an expanded replacement range."""
    if data[:10] != b"\x00\x00GRF\x82\r\n\x1a\n":
        raise RuntimeError("expected a GRF v2 container")
    pos, actions, replacements, record = 15, [], [], 0
    while True:
        size = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        if not size:
            break
        kind = data[pos]
        payload = data[pos + 1:pos + 1 + size]
        pos += size + 1
        record += 1
        if record == 1:
            if kind != 0xFF or size != 4:
                raise RuntimeError("invalid GRF record count")
            continue
        if kind == 0xFF:
            action = payload[0]
            actions.append(action)
            if action not in (0x08, 0x0A, 0x14):
                raise RuntimeError(f"unexpected Action {action:02x}")
            if action == 0x0A:
                if len(payload) != 5 or payload[1:] != b"\x01\x04\x2d\x04":
                    raise RuntimeError("replacement scope must be exactly IDs 1069-1072")
                replacements.append(payload)
        elif kind != 0xFD:
            raise RuntimeError(f"unexpected GRF data record {kind:02x}")
    if len(replacements) != 1 or 0x08 not in actions:
        raise RuntimeError("missing package identity or replacement block")
    return actions


def build(verify: bool) -> None:
    if PIL.__version__ != VERSIONS["Pillow"]:
        raise RuntimeError(f"Pillow {VERSIONS['Pillow']} required, got {PIL.__version__}")
    compiler = os.environ.get("NMLC") or shutil.which("nmlc")
    if not compiler:
        raise RuntimeError("install requirements-platform-art.txt or set NMLC")
    version = subprocess.check_output([compiler, "--version"], text=True).splitlines()[0]
    if VERSIONS["nmlc"] not in version:
        raise RuntimeError(f"nmlc {VERSIONS['nmlc']} required, got {version}")
    model = json.loads((SOURCE / "platforms.json").read_text())
    if [s["id"] for s in model["sprites"]] != list(range(1069, 1073)):
        raise RuntimeError("platform ID contract changed")
    if (model["length"], model["width"], model["height"]) != (16, 5, 2):
        raise RuntimeError("native platform bounds changed")
    with tempfile.TemporaryDirectory(prefix="ost-platform-art-") as directory:
        temp = Path(directory)
        (temp / "sprites").mkdir()
        shutil.copytree(SOURCE / "lang", temp / "lang")
        outputs = {}
        for scale in (1, 2):
            sheet, mask = render(model, scale)
            if set(mask.get_flattened_data()) - {0, COMPANY}:
                raise RuntimeError("unexpected recolour mask index")
            if any(m and p[3] != 255 for p, m in zip(sheet.get_flattened_data(), mask.get_flattened_data())):
                raise RuntimeError("recolour mask extends outside opaque geometry")
            prefix = "platform_" if scale == 1 else "platform_2x_"
            for name, image in ((prefix + "32bpp.png", sheet), (prefix + "mask.png", mask)):
                image.save(temp / "sprites" / name, optimize=False, compress_level=9)
                outputs[SOURCE / "sprites" / name] = temp / "sprites" / name
            if scale == 1:
                name = "platform_8bpp.png"
                indexed = fallback(sheet, mask)
                if set(indexed.get_flattened_data()) - {0, COMPANY, *ORDINARY}:
                    raise RuntimeError("fallback contains a reserved palette index")
                indexed.save(temp / "sprites" / name, optimize=False, compress_level=9)
                outputs[SOURCE / "sprites" / name] = temp / "sprites" / name
        nml = temp / "commonwealth_platforms.nml"
        nml.write_text(nml_source(model))
        outputs[SOURCE / nml.name] = nml
        grf = temp / GRF_NAME
        engine_md5 = temp / "engine.md5"
        subprocess.run([compiler, f"--grf={grf}", f"--md5={engine_md5}", nml.name],
                       cwd=temp, check=True)
        actions = audit_actions(grf.read_bytes())
        outputs[ROOT / "bin/newgrf" / GRF_NAME] = grf
        manifest = {
            "schema_version": 1, "grfid_hex": "0654534f", "version": 1,
            "license": model["license"], "source": "pkg/commonwealth_platforms/platforms.json",
            "source_sha256": digest((SOURCE / "platforms.json").read_bytes()),
            "exporter_sha256": digest(Path(__file__).read_bytes()),
            "tools": VERSIONS, "sprite_ids": [s["id"] for s in model["sprites"]],
            "compiled_actions": actions, "zooms": ["normal-8bpp", "normal-32bpp", "2x-32bpp"],
            "engine_content_md5": engine_md5.read_text().strip(),
            "files": {str(target.relative_to(ROOT)): {"bytes": path.stat().st_size,
                       "sha256": digest(path.read_bytes())} for target, path in outputs.items()},
        }
        manifest_file = temp / "manifest.json"
        manifest_file.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        outputs[SOURCE / "manifest.json"] = manifest_file
        for target, path in outputs.items():
            if verify:
                if not target.is_file() or target.read_bytes() != path.read_bytes():
                    raise RuntimeError(f"stale or missing {target.relative_to(ROOT)}")
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(path.read_bytes())
    print("[OK] Four platform sprites, palettes, zooms and compiled action scope verified")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", action="store_true", help="rebuild privately and compare all outputs")
    build(parser.parse_args().verify)
