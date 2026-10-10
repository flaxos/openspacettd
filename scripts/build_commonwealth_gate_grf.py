#!/usr/bin/env python3
"""Build, audit and verify the Commonwealth wormhole gate NewGRF.

The gate package uses static NewGRF GRM reservation + Action A:
  param[10] = reserve_sprites(16);
  replace gate_set(param[10], "sprites/gate_8bpp.png")
  alternative_sprites for normal 32bpp and 2x 32bpp.

This module compiles the package to bin/newgrf/openspacettd_gates_v1.grf,
performs a strict semantic binary audit of container records, pseudo actions,
and sprite headers, provides synthetic tests for malformed/unsafe records,
and verifies determinism against pkg/commonwealth_gates/pack-manifest.json.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

import PIL

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "pkg/commonwealth_gates"
OUTPUT_GRF = ROOT / "bin/newgrf/openspacettd_gates_v1.grf"
PACK_MANIFEST = SOURCE / "pack-manifest.json"
ASSET_MANIFEST = SOURCE / "manifest.json"
GRF_NAME = "openspacettd_gates_v1.grf"
VERSIONS = {"nmlc": "0.9.0", "Pillow": "12.3.0"}
TOTAL_SPRITES = 16

EXPECTED_ACTION14 = bytes.fromhex(
    "1443494e464f425652534e040001000000424d494e56040001000000424e5041520100004250414c5301004442424c54520100330000"
)
EXPECTED_ACTIOND_GRM = bytes.fromhex("0d0a0000feff081000")
EXPECTED_ACTION6_PATCH = bytes.fromhex("060a02ff0300ff")
EXPECTED_ACTIONA = bytes.fromhex("0a01100000")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def audit_grf(data: bytes, asset_manifest: dict | None = None) -> dict:
    """Perform a strict semantic binary audit of a compiled GRF v2 container.

    Audits header, pseudo-sprite stream, Action records, and sprite headers.
    Note: Sprite headers (dimensions, zoom, offsets) are audited; compressed
    pixel payload data is not decoded.
    """
    if len(data) < 493:
        raise ValueError(f"container truncated: length {len(data)} < 493 header minimum")

    if data[:10] != b"\x00\x00GRF\x82\r\n\x1a\n":
        raise ValueError("invalid GRF v2 container signature")

    # Byte 10..13: offset to sprite data relative to offset 14
    sprites_offset = struct.unpack_from("<I", data, 10)[0] + 14
    if sprites_offset != 493:
        raise ValueError(f"unexpected sprite section offset {sprites_offset}, expected 493")

    # Byte 14 must be compression mode 0 (uncompressed data container)
    if data[14] != 0:
        raise ValueError(f"unexpected compression mode {data[14]} at byte 14, expected 0")

    # Data records begin at byte 15 and must end exactly at sprites_offset with a 0 terminator
    pos = 15
    records = []
    saw_data_terminator = False
    while pos < sprites_offset:
        if pos + 4 > sprites_offset:
            raise ValueError(f"record size at {pos} extends beyond data section {sprites_offset}")
        size = struct.unpack_from("<I", data, pos)[0]
        pos += 4
        if size == 0:
            if pos != sprites_offset:
                raise ValueError(f"zero terminator at {pos} does not match sprite section start {sprites_offset}")
            saw_data_terminator = True
            break
        if pos + size + 1 > sprites_offset:
            raise ValueError(f"record at {pos} (size {size}) exceeds data section bounds {sprites_offset}")
        kind = data[pos]
        payload = data[pos + 1:pos + 1 + size]
        pos += size + 1
        records.append((kind, payload))

    if not saw_data_terminator or pos != sprites_offset:
        raise ValueError("data records section missing 0-size terminator or improperly bounded")

    if len(records) != 22:
        raise ValueError(f"expected exactly 22 container records, got {len(records)}")

    # Record 0: 4-byte sprite count
    if records[0][0] != 0xFF or len(records[0][1]) != 4:
        raise ValueError("record 0 must be 4-byte pseudo-sprite count")
    sprite_count = struct.unpack_from("<I", records[0][1], 0)[0]
    if sprite_count != 21:
        raise ValueError(f"expected sprite count 21 in record 0, got {sprite_count}")

    # Record 1: Action 14 (Metadata: 54 bytes)
    r1_kind, r1_payload = records[1]
    if r1_kind != 0xFF or len(r1_payload) != 54 or r1_payload != EXPECTED_ACTION14:
        raise ValueError(f"record 1 must be exact 54-byte Action 14 metadata, got {r1_payload.hex()}")

    # Record 2: Action 8 (Package Identity)
    r2_kind, r2_payload = records[2]
    if r2_kind != 0xFF or r2_payload[0] != 0x08:
        raise ValueError("record 2 must be Action 0x08")
    if r2_payload[1] != 8 or r2_payload[2:6] != b"OST\x07":
        raise ValueError(f"Action 8 payload must begin with version 8 + OST\\07, got {r2_payload[:6].hex()}")
    # Verify exact two null-terminated strings and no trailing bytes
    strings = r2_payload[6:].split(b"\x00")
    if len(strings) != 3 or not strings[0] or not strings[1] or strings[2] != b"":
        raise ValueError("Action 8 must have exactly two null-terminated strings with no trailing bytes")

    # Record 3: Action D (GRM Reservation)
    r3_kind, r3_payload = records[3]
    if r3_kind != 0xFF or r3_payload != EXPECTED_ACTIOND_GRM:
        raise ValueError(f"Action D must have exact GRM payload {EXPECTED_ACTIOND_GRM.hex()}, got {r3_payload.hex()}")

    # Record 4: Action 6 (Parameter 10 Patch)
    r4_kind, r4_payload = records[4]
    if r4_kind != 0xFF or r4_payload != EXPECTED_ACTION6_PATCH:
        raise ValueError(f"Action 6 must have exact patch payload {EXPECTED_ACTION6_PATCH.hex()}, got {r4_payload.hex()}")

    # Record 5: Action A (Sprite Replacement)
    r5_kind, r5_payload = records[5]
    if r5_kind != 0xFF or r5_payload != EXPECTED_ACTIONA:
        raise ValueError(f"Action A must have exact payload {EXPECTED_ACTIONA.hex()}, got {r5_payload.hex()}")

    # Records 6..21: Exactly 16 real sprite reference descriptors (kind 0xFD)
    for idx, (kind, payload) in enumerate(records[6:22]):
        if kind != 0xFD:
            raise ValueError(f"record {6 + idx} must be real sprite descriptor 0xFD, got 0x{kind:02x}")
        if len(payload) != 4:
            raise ValueError(f"record {6 + idx} descriptor length must be 4, got {len(payload)}")
        target_id = struct.unpack_from("<I", payload, 0)[0]
        expected_id = 7 + idx
        if target_id != expected_id:
            raise ValueError(f"record {6 + idx} references sprite ID {target_id}, expected {expected_id}")

    # Build expected offset table from manifest if provided, requiring indices 0..15 in order
    expected_offsets_1x = []
    if asset_manifest and "sprites" in asset_manifest:
        sprites_meta = asset_manifest["sprites"]
        if len(sprites_meta) != TOTAL_SPRITES:
            raise ValueError(f"asset manifest must have {TOTAL_SPRITES} sprites, got {len(sprites_meta)}")
        indices = [s["index"] for s in sprites_meta]
        if indices != list(range(TOTAL_SPRITES)):
            raise ValueError(f"asset manifest sprite indices must be 0..15 in order, got {indices}")
        for s in sprites_meta:
            expected_offsets_1x.append((s["xoff"], s["yoff"]))
    else:
        expected_offsets_1x = [
            (-24, -12), (-24, -12), (-24, -12), (-24, -12),  # NE active, SE active
            (-24,  -8), (-24,  -8), (-24,  -8), (-24,  -8),  # SW active, NW active
            (-24, -12), (-24, -12), (-24, -12), (-24, -12),  # NE standby, SE standby
            (-24,  -8), (-24,  -8), (-24,  -8), (-24,  -8),  # SW standby, NW standby
        ]

    # Parse Sprite Section (begins at sprites_offset = 493)
    sp_pos = sprites_offset
    sprite_entries = []
    saw_sprite_terminator = False

    while sp_pos < len(data):
        if sp_pos + 4 > len(data):
            raise ValueError(f"truncated sprite ID at {sp_pos}")
        sp_id = struct.unpack_from("<I", data, sp_pos)[0]
        sp_pos += 4
        if sp_id == 0:
            if sp_pos != len(data):
                raise ValueError(f"terminal sprite ID 0 at {sp_pos}, but container EOF is {len(data)}")
            saw_sprite_terminator = True
            break

        if sp_pos + 4 > len(data):
            raise ValueError(f"truncated sprite size at {sp_pos} for ID {sp_id}")
        sp_size = struct.unpack_from("<I", data, sp_pos)[0]
        sp_pos += 4

        if sp_pos + sp_size > len(data):
            raise ValueError(f"sprite entry ID {sp_id} size {sp_size} exceeds EOF")

        entry_data = data[sp_pos:sp_pos + sp_size]
        sp_pos += sp_size

        if len(entry_data) < 10:
            raise ValueError(f"sprite entry ID {sp_id} header truncated (< 10 bytes)")

        sp_type = entry_data[0]
        sp_zoom = entry_data[1]
        sp_height, sp_width = struct.unpack_from("<HH", entry_data, 2)
        sp_xoff, sp_yoff = struct.unpack_from("<hh", entry_data, 6)

        sprite_entries.append({
            "id": sp_id, "size": sp_size, "type": sp_type, "zoom": sp_zoom,
            "width": sp_width, "height": sp_height, "xoff": sp_xoff, "yoff": sp_yoff,
        })

    if not saw_sprite_terminator or sp_pos != len(data):
        raise ValueError("sprite section missing terminal sprite ID 0 or has trailing data")

    if len(sprite_entries) != 48:
        raise ValueError(f"expected 48 sprite entries (3 per ID x 16), got {len(sprite_entries)}")

    # Audit each of the 16 sprite groups (8bpp, 32bpp 1x, 32bpp 2x)
    for sprite_idx in range(TOTAL_SPRITES):
        group = sprite_entries[sprite_idx * 3:(sprite_idx + 1) * 3]
        e_8bpp, e_32bpp_1x, e_32bpp_2x = group[0], group[1], group[2]
        expected_id = 7 + sprite_idx
        expected_x, expected_y = expected_offsets_1x[sprite_idx]

        # 8bpp entry: type 0x4C, zoom 0, 48x48
        if (e_8bpp["id"], e_8bpp["type"], e_8bpp["zoom"], e_8bpp["width"], e_8bpp["height"]) != (expected_id, 0x4C, 0, 48, 48):
            raise ValueError(f"sprite {sprite_idx} 8bpp format mismatch: {e_8bpp}")
        if (e_8bpp["xoff"], e_8bpp["yoff"]) != (expected_x, expected_y):
            raise ValueError(f"sprite {sprite_idx} 8bpp offset mismatch")

        # 32bpp 1x entry: type 0x4F, zoom 0, 48x48
        if (e_32bpp_1x["id"], e_32bpp_1x["type"], e_32bpp_1x["zoom"], e_32bpp_1x["width"], e_32bpp_1x["height"]) != (expected_id, 0x4F, 0, 48, 48):
            raise ValueError(f"sprite {sprite_idx} 32bpp 1x format mismatch: {e_32bpp_1x}")
        if (e_32bpp_1x["xoff"], e_32bpp_1x["yoff"]) != (expected_x, expected_y):
            raise ValueError(f"sprite {sprite_idx} 32bpp 1x offset mismatch")

        # 32bpp 2x entry: type 0x4F, zoom 2, 96x96
        if (e_32bpp_2x["id"], e_32bpp_2x["type"], e_32bpp_2x["zoom"], e_32bpp_2x["width"], e_32bpp_2x["height"]) != (expected_id, 0x4F, 2, 96, 96):
            raise ValueError(f"sprite {sprite_idx} 32bpp 2x format mismatch: {e_32bpp_2x}")
        if (e_32bpp_2x["xoff"], e_32bpp_2x["yoff"]) != (expected_x * 2, expected_y * 2):
            raise ValueError(f"sprite {sprite_idx} 32bpp 2x offset mismatch")

    return {
        "grfid": "OST\x07",
        "container_version": 2,
        "compression": 0,
        "total_records": len(records),
        "pseudo_actions": [0x14, 0x08, 0x0D, 0x06, 0x0A],
        "grm_reservation": {"param": 10, "operation": 0, "feature": 8, "count": 16},
        "action6_patch": {"param": 10, "size": 2, "target_offset": 3},
        "actionA_replacement": {"sets": 1, "count": 16, "base_sprite": 0},
        "real_sprites_count": 16,
        "sprite_entries_count": 48,
        "total_bytes": len(data),
    }


def run_synthetic_tests(valid_data: bytes, asset_manifest: dict | None) -> None:
    """Run synthetic test suite covering malformed/unsafe records using stdlib."""
    print("[*] Running synthetic malformed/unsafe records audit tests...")

    # Baseline audit must pass
    audit_grf(valid_data, asset_manifest)

    # 1. Truncated header
    try:
        audit_grf(valid_data[:400], asset_manifest)
        raise AssertionError("failed to reject truncated header")
    except ValueError as e:
        assert "container truncated" in str(e)

    # 2. Corrupt signature
    corrupt_sig = b"XX" + valid_data[2:]
    try:
        audit_grf(corrupt_sig, asset_manifest)
        raise AssertionError("failed to reject invalid container signature")
    except ValueError as e:
        assert "container signature" in str(e)

    # 3. Nonzero compression byte
    bad_comp = bytearray(valid_data)
    bad_comp[14] = 1
    try:
        audit_grf(bytes(bad_comp), asset_manifest)
        raise AssertionError("failed to reject nonzero compression mode")
    except ValueError as e:
        assert "compression mode" in str(e)

    # 4. Action 14 corrupt byte
    bad_meta = bytearray(valid_data)
    act14_idx = valid_data.find(EXPECTED_ACTION14)
    assert act14_idx != -1
    bad_meta[act14_idx + 10] ^= 0xFF  # Mutate byte inside Action 14 payload
    try:
        audit_grf(bytes(bad_meta), asset_manifest)
        raise AssertionError("failed to reject altered Action 14 metadata")
    except ValueError as e:
        assert "Action 14" in str(e)

    # 5. Fixed native Action A base (non-zero base, e.g. fixed ID 2365 = 0x093D)
    bad_acta_base = bytearray(valid_data)
    acta_offset = valid_data.find(EXPECTED_ACTIONA)
    assert acta_offset != -1
    bad_acta_base[acta_offset + 3] = 0x3D
    bad_acta_base[acta_offset + 4] = 0x09
    try:
        audit_grf(bytes(bad_acta_base), asset_manifest)
        raise AssertionError("failed to reject fixed native Action A base")
    except ValueError as e:
        assert "Action A" in str(e)

    # 6. Action D GRM parameter != 10
    bad_grm_param = bytearray(valid_data)
    actd_offset = valid_data.find(EXPECTED_ACTIOND_GRM)
    assert actd_offset != -1
    bad_grm_param[actd_offset + 1] = 11  # param 11
    try:
        audit_grf(bytes(bad_grm_param), asset_manifest)
        raise AssertionError("failed to reject wrong Action D param")
    except ValueError as e:
        assert "Action D" in str(e)

    # 7. Action D GRM operation != 0
    bad_grm_op = bytearray(valid_data)
    bad_grm_op[actd_offset + 3] = 1  # op 1
    try:
        audit_grf(bytes(bad_grm_op), asset_manifest)
        raise AssertionError("failed to reject wrong Action D op")
    except ValueError as e:
        assert "Action D" in str(e)

    # 8. Action D GRM feature != 8
    bad_grm_feat = bytearray(valid_data)
    bad_grm_feat[actd_offset + 6] = 9  # feature 9
    try:
        audit_grf(bytes(bad_grm_feat), asset_manifest)
        raise AssertionError("failed to reject wrong Action D feature")
    except ValueError as e:
        assert "Action D" in str(e)

    # 9. Action D GRM count != 16
    bad_grm_cnt = bytearray(valid_data)
    bad_grm_cnt[actd_offset + 7] = 17  # count 17
    try:
        audit_grf(bytes(bad_grm_cnt), asset_manifest)
        raise AssertionError("failed to reject altered Action D count")
    except ValueError as e:
        assert "Action D" in str(e)

    # 10. Action 6 patch parameter != 10
    bad_patch_param = bytearray(valid_data)
    act6_offset = valid_data.find(EXPECTED_ACTION6_PATCH)
    assert act6_offset != -1
    bad_patch_param[act6_offset + 1] = 11  # param 11
    try:
        audit_grf(bytes(bad_patch_param), asset_manifest)
        raise AssertionError("failed to reject altered Action 6 param")
    except ValueError as e:
        assert "Action 6" in str(e)

    # 11. Action 6 patch size != 2
    bad_patch_size = bytearray(valid_data)
    bad_patch_size[act6_offset + 2] = 4  # size 4
    try:
        audit_grf(bytes(bad_patch_size), asset_manifest)
        raise AssertionError("failed to reject altered Action 6 size")
    except ValueError as e:
        assert "Action 6" in str(e)

    # 12. Action 6 patch target offset != 3
    bad_patch_off = bytearray(valid_data)
    bad_patch_off[act6_offset + 4] = 4  # target offset 4
    try:
        audit_grf(bytes(bad_patch_off), asset_manifest)
        raise AssertionError("failed to reject altered Action 6 target offset")
    except ValueError as e:
        assert "Action 6" in str(e)

    # 13. Action 8 trailing bytes
    bad_act8 = bytearray(valid_data)
    act8_offset = valid_data.find(b"\x08\x08OST\x07")
    assert act8_offset != -1
    # Mutate second null terminator inside Action 8
    bad_act8[act8_offset + 220] = 0x20
    try:
        audit_grf(bytes(bad_act8), asset_manifest)
        raise AssertionError("failed to reject invalid Action 8 string termination")
    except ValueError as e:
        assert "Action 8" in str(e)

    # 14. Corrupt real sprite descriptor reference ID (kind 0xFD)
    bad_fd = bytearray(valid_data)
    fd_offset = valid_data.find(b"\xfd\x07\x00\x00\x00")
    if fd_offset != -1:
        bad_fd[fd_offset + 1] = 0x99
        try:
            audit_grf(bytes(bad_fd), asset_manifest)
            raise AssertionError("failed to reject altered sprite ref ID")
        except ValueError as e:
            assert "references sprite ID" in str(e)

    # 15. Corrupt sprite dimensions
    bad_dims = bytearray(valid_data)
    sp_header_offset = 493 + 8  # first sprite header (after ID 7 and size)
    bad_dims[sp_header_offset + 2] = 40  # change height 48 to 40
    try:
        audit_grf(bytes(bad_dims), asset_manifest)
        raise AssertionError("failed to reject altered sprite dimensions")
    except ValueError as e:
        assert "format mismatch" in str(e)

    # 16. Missing data section 0 terminator
    bad_data_term = bytearray(valid_data)
    struct.pack_into("<I", bad_data_term, 489, 1)  # change size 0 to 1
    try:
        audit_grf(bytes(bad_data_term), asset_manifest)
        raise AssertionError("failed to reject missing data section terminator")
    except ValueError as e:
        assert "terminator" in str(e) or "bounds" in str(e)

    # 17. Truncated terminal sprite ID 0 (valid_data[:-4]) - regression caught by Codex!
    try:
        audit_grf(valid_data[:-4], asset_manifest)
        raise AssertionError("failed to reject truncated terminal sprite ID 0")
    except ValueError as e:
        assert "sprite section missing" in str(e) or "truncated" in str(e)

    # 18. Mutated terminal sprite ID 0 to 1
    bad_term_id = bytearray(valid_data)
    bad_term_id[-4:] = b"\x01\x00\x00\x00"
    try:
        audit_grf(bytes(bad_term_id), asset_manifest)
        raise AssertionError("failed to reject non-zero terminal sprite ID")
    except ValueError as e:
        assert "terminal sprite ID 0" in str(e) or "truncated" in str(e)

    # 19. Record bounds overflow
    bad_overflow = bytearray(valid_data)
    struct.pack_into("<I", bad_overflow, 15, 600)  # make record 0 size 600
    try:
        audit_grf(bytes(bad_overflow), asset_manifest)
        raise AssertionError("failed to reject record overflow")
    except ValueError as e:
        assert "bounds" in str(e) or "extends" in str(e)

    # 20. Extra unexpected pseudo-sprite record
    bad_extra = bytearray(valid_data[:489]) + b"\x05\x00\x00\x00\xff\x00\x00\x00\x00\x00" + bytearray(valid_data[489:])
    try:
        audit_grf(bytes(bad_extra), asset_manifest)
        raise AssertionError("failed to reject extra record")
    except ValueError:
        pass

    print("[OK] Synthetic malformed/unsafe records suite passed (20/20 negative tests)")


def compile_in_temp(compiler: str, target_grf: Path, md5_path: Path | None = None) -> bytes:
    """Compile commonwealth_gates.nml in an isolated temporary directory."""
    with tempfile.TemporaryDirectory(prefix="ost-gates-build-") as tmpdir:
        temp = Path(tmpdir)
        (temp / "sprites").mkdir()
        shutil.copytree(SOURCE / "lang", temp / "lang")
        for s in SOURCE.glob("sprites/*.png"):
            shutil.copy(s, temp / "sprites" / s.name)
        nml_file = temp / "commonwealth_gates.nml"
        shutil.copy(SOURCE / "commonwealth_gates.nml", nml_file)

        tmp_grf = temp / GRF_NAME
        tmp_md5 = temp / "engine.md5"
        cmd = [compiler, f"--grf={tmp_grf}", f"--md5={tmp_md5}", nml_file.name]
        subprocess.run(cmd, cwd=temp, check=True)

        data = tmp_grf.read_bytes()
        target_grf.parent.mkdir(parents=True, exist_ok=True)
        target_grf.write_bytes(data)
        if md5_path:
            md5_path.write_text(tmp_md5.read_text().strip())
        return data


def build(verify: bool) -> None:
    if not ASSET_MANIFEST.is_file():
        raise RuntimeError(f"missing asset manifest at {ASSET_MANIFEST}")
    asset_manifest = json.loads(ASSET_MANIFEST.read_text())

    compiler = os.environ.get("NMLC") or shutil.which("nmlc")
    if not compiler:
        print("[ERROR] nmlc compiler is not available via NMLC env or PATH", file=sys.stderr)
        sys.exit(1)

    version_out = subprocess.check_output([compiler, "--version"], text=True).splitlines()[0]
    if VERSIONS["nmlc"] not in version_out:
        raise RuntimeError(f"nmlc {VERSIONS['nmlc']} required, got {version_out}")

    if PIL.__version__ != VERSIONS["Pillow"]:
        raise RuntimeError(f"Pillow {VERSIONS['Pillow']} required, got {PIL.__version__}")

    if verify:
        print("[*] Running read-only verification of delivered package and determinism...")
        if not PACK_MANIFEST.is_file():
            raise RuntimeError(f"missing pack manifest at {PACK_MANIFEST}")
        pack_manifest = json.loads(PACK_MANIFEST.read_text())

        # Check recorded files existence and hashes
        for rel_path, meta in pack_manifest["files"].items():
            file_path = ROOT / rel_path
            if not file_path.is_file():
                raise RuntimeError(f"recorded file missing: {rel_path}")
            actual_sha = digest(file_path.read_bytes())
            if actual_sha != meta["sha256"]:
                raise RuntimeError(f"hash mismatch for {rel_path}: expected {meta['sha256']}, got {actual_sha}")

        if not OUTPUT_GRF.is_file():
            raise RuntimeError(f"delivered binary missing at {OUTPUT_GRF}")
        data = OUTPUT_GRF.read_bytes()
        audit_info = audit_grf(data, asset_manifest)
        run_synthetic_tests(data, asset_manifest)

        # Recompile in temp to verify binary determinism without touching repo
        with tempfile.TemporaryDirectory(prefix="ost-gates-verify-") as tmpdir:
            temp_target = Path(tmpdir) / GRF_NAME
            temp_data = compile_in_temp(compiler, temp_target)
            if temp_data != data:
                raise RuntimeError("recompiled binary does not byte-match delivered GRF")

        print(f"[OK] Read-only verification and binary determinism passed. GRF: {digest(data)[:16]}... ({len(data)} bytes)")
        return

    # Normal build / compile mode:
    print("[*] Compiling Commonwealth wormhole gates NewGRF...")
    temp_md5 = SOURCE / ".engine.md5"
    grf_data = compile_in_temp(compiler, OUTPUT_GRF, temp_md5)
    md5_val = temp_md5.read_text().strip()
    temp_md5.unlink(missing_ok=True)

    audit_info = audit_grf(grf_data, asset_manifest)
    run_synthetic_tests(grf_data, asset_manifest)

    # Write dedicated pack-manifest.json
    pack_manifest = {
        "schema_version": 1,
        "name": "OpenSpaceTTD Commonwealth Wormhole Gates Pack",
        "license": "GPL-2.0-only",
        "grfid_hex": "0754534f",
        "grfid_text": "OST\x07",
        "version": 1,
        "tools": {
            "compiler": compiler,
            "nmlc": VERSIONS["nmlc"],
            "Pillow": VERSIONS["Pillow"],
        },
        "engine_content_md5": md5_val,
        "audit": {
            "container_version": audit_info["container_version"],
            "compression": audit_info["compression"],
            "total_records": audit_info["total_records"],
            "pseudo_actions": ["14", "08", "0D", "06", "0A"],
            "grm_reservation": audit_info["grm_reservation"],
            "action6_patch": audit_info["action6_patch"],
            "actionA_replacement": audit_info["actionA_replacement"],
            "real_sprites_count": audit_info["real_sprites_count"],
            "sprite_entries_count": audit_info["sprite_entries_count"],
        },
        "files": {
            "bin/newgrf/openspacettd_gates_v1.grf": {
                "bytes": len(grf_data),
                "sha256": digest(grf_data),
            },
            "pkg/commonwealth_gates/commonwealth_gates.nml": {
                "bytes": (SOURCE / "commonwealth_gates.nml").stat().st_size,
                "sha256": digest((SOURCE / "commonwealth_gates.nml").read_bytes()),
            },
            "pkg/commonwealth_gates/lang/english.lng": {
                "bytes": (SOURCE / "lang/english.lng").stat().st_size,
                "sha256": digest((SOURCE / "lang/english.lng").read_bytes()),
            },
        },
    }
    PACK_MANIFEST.write_text(json.dumps(pack_manifest, indent=2, sort_keys=True) + "\n")
    print(f"[OK] Delivered bin/newgrf/{GRF_NAME} ({len(grf_data)} bytes) and {PACK_MANIFEST.relative_to(ROOT)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", action="store_true", help="read-only verification of delivered outputs and tests")
    args = parser.parse_args()
    build(args.verify)
