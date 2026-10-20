#!/usr/bin/env python3
"""Prove local gate cab transitions and cold reload in private SDL processes.

Requires an existing WITH_CAB_VIEW build, SDL offscreen and installed OpenGFX.
The supplied disposable WP11 save is copied, never edited. No build, download,
owner display, federation connection or human UAT is part of this proof.
"""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import resource
import shutil
import subprocess
import time

from PIL import Image, ImageChops

from test_commonwealth_platform_art import ROOT, configuration, profile_environment


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def no_core_dump():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def validate_stage3d(manifest, phase, output):
    """Require the single declared paired/control experiment; timing is diagnostic."""
    diagnostic = manifest.get("stage3d_paired")
    if phase != "cold":
        return {"passed": diagnostic is None, "errors": [] if diagnostic is None else ["Paired diagnostic ran outside cold phase"]}
    if not isinstance(diagnostic, dict):
        return {"passed": False, "errors": ["Cold paired diagnostic missing"]}
    errors, images = [], {}
    expected = {"capture_label": "back_arrival_mixed", "view_width": 640, "view_height": 372,
                "scene_effect": "arrival", "effect_elapsed_ms": 0, "warmup_off": 20, "warmup_reduced": 20,
                "cycles_declared": 50, "pairs_declared": 200, "frames_declared": 400,
                "completed_pairs": 200, "completed_cycles": 50,
                "even_pair_order": ["OR", "AB", "RO", "BA"], "odd_pair_order": ["BA", "RO", "AB", "OR"],
                "palette_scope": "canonical_static_private"}
    for key, value in expected.items():
        if diagnostic.get(key) != value:
            errors.append(f"Incorrect paired contract: {key}")
    for key in ("unique_measured_frame_ids", "palette_restored", "dpi_restored", "animation_restored", "frozen_state_unchanged"):
        if diagnostic.get(key) is not True:
            errors.append(f"Paired guard failed: {key}")
    if not isinstance(diagnostic.get("frozen_snapshot"), dict) or not isinstance(diagnostic.get("pose"), dict):
        errors.append("Missing paired state/camera identity")
    for key in ("incoming_palette_rgba", "canonical_palette_rgba"):
        values = diagnostic.get(key, [])
        if len(values) != 256 or any(not isinstance(c, list) or len(c) != 4 or
                                   any(type(v) is not int or not 0 <= v <= 255 for v in c) for c in values):
            errors.append(f"Invalid paired palette: {key}")
    warm_cache = diagnostic.get("warm_cache")
    if not isinstance(warm_cache, dict) or warm_cache.get("effect_scratch_capacity_bytes") != 0:
        errors.append("Missing warm cache or unexpected effect scratch")
    cycles = diagnostic.get("cycles", [])
    if len(cycles) != 50:
        errors.append("Expected exactly 50 paired cycle checks")
    for i, cycle in enumerate(cycles):
        if (cycle.get("index") != i or cycle.get("cache_unchanged") is not True or
                cycle.get("frozen_state_unchanged") is not True or cycle.get("cache") != warm_cache):
            errors.append(f"Paired cycle state/cache changed: {i}")
    pairs = diagnostic.get("pairs", [])
    if len(pairs) != 200:
        errors.append("Expected exactly 200 adjacent pairs")
    previous_start, previous_end, previous_id, reference_geometry = -1, -1, -1, None
    labels = {label: 0 for label in "ORAB"}
    orders = {label: 0 for label in ("OR", "AB", "RO", "BA")}
    geometry_keys = ("view_width", "view_height", "internal_width", "internal_height", "scale",
                     "visible_tiles", "triangles", "threads")
    for i, pair in enumerate(pairs):
        cycle, position = divmod(i, 4)
        order = expected["odd_pair_order" if cycle % 2 else "even_pair_order"][position]
        if pair.get("cycle") != cycle or pair.get("position") != position or pair.get("label") != order:
            errors.append(f"Incorrect pair identity/order: {i}")
        orders[order] += 1
        for key in ("first_image_exact", "second_image_exact", "pair_images_equal", "first_geometry_exact", "second_geometry_exact"):
            if pair.get(key) is not True:
                errors.append(f"Pair {i} guard failed: {key}")
        arms = pair.get("arms", [])
        if len(arms) != 2:
            errors.append(f"Expected exactly two members: pair {i}")
        for j, arm in enumerate(arms):
            if j >= 2:
                continue
            label = order[j]
            actual_mode = "reduced" if label == "R" else "off"
            if arm.get("label") != label or arm.get("actual_mode") != actual_mode:
                errors.append(f"Incorrect member label/physical mode: pair {i}/{j}")
            labels[label] += 1
            frame = arm.get("frame", {})
            start, wall, frame_id = arm.get("relative_start_us"), arm.get("wall_us"), frame.get("frame_id")
            if (type(start) is not int or start <= previous_start or start < previous_end or
                    type(wall) is not int or wall <= 0 or type(frame_id) is not int or frame_id <= previous_id):
                errors.append(f"Invalid chronological timing/frame identity: pair {i}/{j}")
            if type(start) is int and type(wall) is int:
                previous_start, previous_end = start, start + wall
            if type(frame_id) is int:
                previous_id = frame_id
            if (frame.get("rendered") is not True or frame.get("cache_hit") is not False or
                    (actual_mode == "off" and frame.get("effect_us") != 0) or frame.get("scale") != 1.0 or
                    frame.get("view_width") != 640 or frame.get("internal_width") != 640 or
                    frame.get("view_height") != 372 or frame.get("internal_height") != 372):
                errors.append(f"Member was not a fresh full-scale mode-null render: pair {i}/{j}")
            if any(type(frame.get(k)) is not int or frame[k] < 0 for k in ("prepare_us", "raster_us", "effect_us", "output_us", "total_us")):
                errors.append(f"Invalid member stage timing: pair {i}/{j}")
            geometry = {k: frame.get(k) for k in geometry_keys}
            if reference_geometry is None:
                reference_geometry = geometry
            elif geometry != reference_geometry:
                errors.append(f"Geometry/work changed: pair {i}/{j}")
    if labels != {label: 100 for label in "ORAB"} or orders != {label: 50 for label in orders}:
        errors.append("Expected 100 samples per label and 50 pairs per order")
    for label in ("OR", "AB", "RO", "BA"):
        name = diagnostic.get("representative_images", {}).get(label)
        path = output / name if isinstance(name, str) else None
        if path is None or path.parent != output or not path.is_file():
            errors.append(f"Missing/unsafe paired image: {label}")
        else:
            with Image.open(path) as picture:
                if picture.size != (640, 372):
                    errors.append(f"Incorrect paired image size: {label}")
            images[label] = {"file": name, "sha256": sha256(path), "bytes": path.stat().st_size}
    if len(images) != 4 or len({i["file"] for i in images.values()}) != 4 or len({i["sha256"] for i in images.values()}) != 1:
        errors.append("Representative images missing, reused or different")
    return {"passed": not errors, "errors": errors, "timing_scope": "one diagnostic; no acceptance threshold applied",
            "sample_count_by_label": labels, "pair_count_by_order": orders, "geometry": reference_geometry,
            "representative_images": images}


def validate_stage3c(manifest, phase, output):
    """Validate the declared diagnostic shape without turning timing into acceptance."""
    errors, images = [], {}
    diagnostic = manifest.get("stage3c_diagnostic")
    if phase != "cold":
        return {"passed": diagnostic is None, "errors": [] if diagnostic is None else ["Diagnostic ran outside cold phase"]}
    if not isinstance(diagnostic, dict):
        return {"passed": False, "errors": ["Cold diagnostic missing"]}
    timing = diagnostic.get("timing", {})
    order = ["off", "reduced", "reduced", "off", "reduced", "off", "off", "reduced"]
    expected = {"capture_label": "back_arrival_mixed", "view_width": 640, "view_height": 372,
                "scene_effect": "arrival", "effect_elapsed_ms": 0, "warmup_per_mode": 20,
                "frames_per_block": 25, "block_order": order, "palette_scope": "canonical_static_private"}
    for key, value in expected.items():
        if timing.get(key) != value:
            errors.append(f"Incorrect timing contract: {key}")
    for key in ("exact_cross_mode_geometry", "exact_cross_mode_scene_cache", "exact_cross_mode_image", "palette_restored",
                "dpi_restored", "animation_restored", "frozen_state_unchanged"):
        if timing.get(key) is not True:
            errors.append(f"Timing guard failed: {key}")
    if not isinstance(timing.get("frozen_snapshot"), dict) or not isinstance(timing.get("pose"), dict):
        errors.append("Missing timing state/camera identity")
    for key in ("incoming_palette_rgba", "canonical_palette_rgba"):
        values = timing.get(key, [])
        if len(values) != 256 or any(not isinstance(c, list) or len(c) != 4 or
                                   any(type(v) is not int or not 0 <= v <= 255 for v in c) for c in values):
            errors.append(f"Invalid palette: {key}")
    blocks = timing.get("blocks", [])
    if len(blocks) != 8:
        errors.append("Expected exactly eight timing blocks")
    frame_ids, previous_start, reference_geometry, reference_cache = set(), -1, None, None
    count_by_mode = {"off": 0, "reduced": 0}
    geometry_keys = ("view_width", "view_height", "internal_width", "internal_height", "scale",
                     "visible_tiles", "triangles", "threads")
    for i, block in enumerate(blocks):
        if i >= 8 or block.get("index") != i or block.get("mode") != order[i]:
            errors.append(f"Invalid block order/index: {i}")
        for key in ("exact_same_mode_image", "exact_geometry", "exact_same_mode_cache", "frozen_state_unchanged"):
            if block.get(key) is not True:
                errors.append(f"Block {i} guard failed: {key}")
        cache = block.get("cache")
        if not isinstance(cache, dict):
            errors.append(f"Missing cache snapshot: {i}")
        elif reference_cache is None:
            reference_cache = cache
        elif cache != reference_cache:
            errors.append(f"Cache differs within/across modes: {i}")
        if isinstance(cache, dict) and cache.get("effect_scratch_capacity_bytes") != 0:
            errors.append(f"Unexpected effect scratch in mode-null scene: {i}")
        samples = block.get("samples", [])
        if len(samples) != 25:
            errors.append(f"Expected exactly 25 samples in block {i}")
        for sample in samples:
            frame = sample.get("frame", {})
            start, wall, frame_id = sample.get("relative_start_us"), sample.get("wall_us"), frame.get("frame_id")
            if (type(start) is not int or start <= previous_start or type(wall) is not int or wall <= 0
                    or type(frame_id) is not int or frame_id in frame_ids):
                errors.append(f"Invalid ordered timing/frame identity: block {i}")
            if type(start) is int:
                previous_start = start
            frame_ids.add(frame_id)
            if block.get("mode") in count_by_mode:
                count_by_mode[block["mode"]] += 1
            if (frame.get("rendered") is not True or frame.get("cache_hit") is not False
                    or (block.get("mode") == "off" and frame.get("effect_us") != 0) or frame.get("scale") != 1.0
                    or frame.get("view_width") != 640 or frame.get("internal_width") != 640
                    or frame.get("view_height") != 372 or frame.get("internal_height") != 372):
                errors.append(f"Frame was not a fresh full-scale mode-null render: block {i}")
            if any(type(frame.get(k)) is not int or frame[k] < 0 for k in ("prepare_us", "raster_us", "effect_us", "output_us", "total_us")):
                errors.append(f"Invalid stage timing: block {i}")
            geometry = {k: frame.get(k) for k in geometry_keys}
            if reference_geometry is None:
                reference_geometry = geometry
            elif geometry != reference_geometry:
                errors.append(f"Geometry/work changed: block {i}")
        before, after = block.get("before", {}), block.get("after", {})
        if type(before.get("supported")) is not bool or before.get("supported") != after.get("supported"):
            errors.append(f"Invalid process-counter availability: {i}")
        if before.get("supported") is True:
            for key in ("user_cpu_us", "system_cpu_us", "voluntary_context_switches", "involuntary_context_switches"):
                if type(before.get(key)) is not int or type(after.get(key)) is not int or not 0 <= before[key] <= after[key]:
                    errors.append(f"Invalid process counter {key}: {i}")
        name = block.get("image")
        path = output / name if isinstance(name, str) else None
        if path is None or path.parent != output or not path.is_file():
            errors.append(f"Missing/unsafe block image: {i}")
        else:
            with Image.open(path) as picture:
                if picture.size != (640, 372):
                    errors.append(f"Incorrect block image size: {i}")
            images[name] = {"sha256": sha256(path), "bytes": path.stat().st_size}
    if count_by_mode != {"off": 100, "reduced": 100} or len(frame_ids) != 200:
        errors.append("Diagnostic did not contain exactly 100 distinct fresh frames per mode")
    if len(images) != 8 or len({i["sha256"] for i in images.values()}) != 1:
        errors.append("Block-end images differ within/across modes")
    preflight = diagnostic.get("vehicle_palette_preflight")
    if not isinstance(preflight, dict):
        errors.append("Vehicle palette preflight missing")
    else:
        if (preflight.get("capture_label") != "back_safe" or preflight.get("palette_anim_start") != 227
                or preflight.get("palette_anim_size") != 28):
            errors.append("Incorrect palette preflight scope/range")
        for key in ("frozen_state_unchanged", "palette_state_unchanged"):
            if preflight.get(key) is not True:
                errors.append(f"Palette preflight guard failed: {key}")
        cars = preflight.get("cars", [])
        frozen_cars = {c["id"]: c for c in preflight.get("frozen_snapshot", {}).get("cars", [])}
        if len(cars) != 4 or preflight.get("visible_car_count") != 4 or {c.get("vehicle_id") for c in cars} != set(frozen_cars):
            errors.append("Palette preflight did not cover the four-car safe consist")
        complete, actual_candidate, other_candidate = bool(cars), False, False
        for car in cars:
            original = frozen_cars.get(car.get("vehicle_id"), {})
            if (original.get("hidden") is not False or original.get("world") != preflight.get("world")
                    or car.get("world") != original.get("world") or car.get("engine_id") != original.get("engine")
                    or car.get("actual_vehicle_direction") != original.get("direction")):
                errors.append("Palette preflight car was hidden, foreign or mismatched")
            directions = car.get("directions", [])
            if len(directions) != 8 or {d.get("query_direction") for d in directions} != set(range(8)):
                errors.append("Missing/duplicate palette query direction")
            for direction in directions:
                actual = direction.get("query_direction") == car.get("actual_renderer_query_direction")
                if direction.get("is_actual_cab_sw") is not actual or direction.get("image_type") != "on_map":
                    errors.append("Palette query identity differs from recorded cab direction")
                layers = direction.get("layers", [])
                if len(layers) != direction.get("raw_sequence_count") or not 1 <= len(layers) <= 8:
                    errors.append("Missing/invalid recorded sprite layers")
                layer_complete = not direction.get("rejected_oversize")
                raw_total, eligible_total = 0, 0
                for layer in layers:
                    counts = layer.get("eligible_slot_counts_227_254", [])
                    raw, eligible = layer.get("raw_animated_pixels"), layer.get("eligible_source_contributions")
                    if (len(counts) != 28 or any(type(n) is not int or n < 0 for n in counts)
                            or type(raw) is not int or raw < 0 or type(eligible) is not int or eligible < 0
                            or sum(counts) != eligible):
                        errors.append("Invalid eligible animated-slot histogram")
                        continue
                    raw_total += raw
                    eligible_total += eligible
                    override = layer.get("palette_override")
                    effective = direction.get("default_palette") if car.get("crashed") or override == 0 else override
                    if layer.get("effective_palette") != effective:
                        errors.append("Recorded effective palette violates native precedence")
                    if any(layer.get(k) is not False for k in ("destination_dependent", "text_recolour", "raw_missing")):
                        layer_complete = False
                if (direction.get("raw_animated_pixels") != raw_total
                        or direction.get("eligible_source_contributions") != eligible_total
                        or direction.get("complete") is not layer_complete):
                    errors.append("Palette sequence totals/completeness are inconsistent")
                complete &= layer_complete
                if eligible_total:
                    if actual:
                        actual_candidate = True
                    else:
                        other_candidate = True
        if (preflight.get("scan_complete") is not complete or preflight.get("candidate_actual_sw") is not actual_candidate
                or preflight.get("candidate_other_direction") is not other_candidate):
            errors.append("Palette aggregate candidate/completeness claims are inconsistent")
    return {"passed": not errors, "errors": errors, "timing_scope": "diagnostic only; no acceptance threshold applied",
            "sample_count_by_mode": count_by_mode, "geometry": reference_geometry, "block_images": images}


def validate_profiles(manifest, phase, rotation, vehicle=False, static_palette=False):
    """Reject incomplete or adaptive data before treating a profiling run as evidence."""
    labels = {"back_approach", "back_near_approach", "back_transit", "back_arrival_mixed"}
    if phase == "roundtrip":
        labels |= {"out_approach", "out_near_approach", "out_transit", "out_arrival_mixed"}
    if vehicle:
        labels = {"back_vehicle_arrival"}
        if phase == "roundtrip":
            labels.add("out_vehicle_arrival")
    expected = {(label, width, height, mode) for label in labels
                for width, height in ((480, 272), (640, 372))
                for mode in ("off", "reduced", "full")}
    found = set()
    mode_groups = {}
    errors = []
    for cell in manifest.get("vehicle_profiles" if vehicle else "profiles", []):
        key = (cell.get("capture_label"), cell.get("view_width"), cell.get("view_height"), cell.get("mode"))
        if key not in expected or key in found:
            errors.append(f"Unexpected or duplicate cell: {key}")
            continue
        found.add(key)
        mode_groups.setdefault(key[:3], []).append(cell)
        if type(cell.get("effect_elapsed_ms")) is not int or cell["effect_elapsed_ms"] < 0 or cell.get("mode_rotation") != rotation:
            errors.append(f"Missing immutable effect descriptor or mode rotation: {key}")
        if key[3] in ("off", "reduced") and cell.get("effect_elapsed_ms") != 0:
            errors.append(f"Static mode has an animated descriptor: {key}")
        if cell.get("warmup_count") != 20 or cell.get("sample_count") != 100 or len(cell.get("samples", [])) != 100:
            errors.append(f"Incorrect warmup/sample count: {key}")
        transit = key[0].endswith("transit")
        if cell.get("phase") != ("transit" if transit else "scene") or not isinstance(cell.get("pose"), dict):
            errors.append(f"Missing or incorrect phase/pose: {key}")
        if cell.get("frozen_state_unchanged") is not True or not isinstance(cell.get("frozen_snapshot"), dict):
            errors.append(f"Missing frozen-state proof: {key}")
        if static_palette:
            if cell.get("palette_scope") != "canonical_static_private" or cell.get("palette_state_restored") is not True:
                errors.append(f"Missing private static palette scope/restoration: {key}")
            for field in ("palette_rgba", "incoming_palette_rgba"):
                colours = cell.get(field)
                if not isinstance(colours, list) or len(colours) != 256 or any(not isinstance(v, list) or len(v) != 4 or any(type(channel) is not int or not 0 <= channel <= 255 for channel in v) for v in colours):
                    errors.append(f"Missing/invalid recorded palette: {key}")
            if not transit and key[3] == "off" and not isinstance(cell.get("diagnostic_image"), str):
                errors.append(f"Missing full-scale static-palette scene image: {key}")
        if vehicle:
            camera, observed = cell.get("diagnostic_camera", {}), cell.get("observed_train_pose", {})
            control, sequences = cell.get("visible_pixel_control", {}), cell.get("vehicle_sequence", [])
            if (not camera or camera.get("world") != cell.get("selected_world") or camera.get("own_exclusion", "missing") is not None
                    or camera.get("offset_from_visible_car") != [0.0, 40.0, 7.0]
                    or any(type(camera.get(k)) not in (int, float) for k in ("x", "y", "z", "yaw")) or not observed):
                errors.append(f"Missing/incorrect separate diagnostic camera and observed train pose: {key}")
            else:
                if any(abs(camera[axis] - observed[f"visible_{axis}"] - offset) > 1e-5
                       for axis, offset in zip(("x", "y", "z"), camera["offset_from_visible_car"])):
                    errors.append(f"Diagnostic camera differs from recorded vehicle offset: {key}")
            if (type(control.get("different_pixels")) is not int or not 0 < control["different_pixels"] <= key[1] * key[2]
                    or control.get("omitted_car_id") != cell.get("visible_car_id")):
                errors.append(f"Missing positive visible-car pixel control: {key}")
            if (not isinstance(sequences, list) or not sequences
                    or not any(v.get("id") == cell.get("visible_car_id") for v in sequences)):
                errors.append(f"Missing actual visible vehicle sequences: {key}")
            else:
                for v in sequences:
                    count, layers = v.get("raw_sequence_count"), v.get("layers", [])
                    if type(count) is not int or not 0 < count <= 8 or count != len(layers) or v.get("world") != cell.get("selected_world"):
                        errors.append(f"Invalid or wrong-world vehicle sequence: {key}")
                        continue
                    for layer in layers:
                        if any(type(layer.get(k)) is not int for k in ("sprite", "base_sprite", "palette_override", "effective_palette")):
                            errors.append(f"Missing layer identity or palette: {key}")
                        expected_pal = v.get("default_palette") if v.get("crashed") or not layer.get("palette_override") else layer["palette_override"]
                        if layer.get("effective_palette") != expected_pal:
                            errors.append(f"Incorrect recorded native palette precedence: {key}")
            if (key[3] == "off") != isinstance(cell.get("diagnostic_image"), str):
                errors.append(f"Missing or unexpected diagnostic image: {key}")
        if key[0].endswith("near_approach"):
            gate = cell.get("gate", {})
            if not 0 < gate.get("signed_face_distance", 999) <= 16 or not 0 <= gate.get("lateral_offset", 999) <= 8 or gate.get("world") != cell.get("selected_world"):
                errors.append(f"Near approach is outside the validated entrance band: {key}")
        for sample in [cell.get("cold_sample", {})] + cell.get("samples", []):
            wall = sample.get("external_total_us")
            if type(wall) is not int or wall < 0:
                errors.append(f"Invalid wall time: {key}")
                break
            frame = sample.get("renderer")
            if transit:
                if frame is not None or sample.get("scene_geometry", "missing") is not None or cell.get("cache") is not None:
                    errors.append(f"Transit includes scene data: {key}")
                    break
                if any(type(sample.get(field)) is not int or sample[field] < 0 for field in ("effect_us", "total_us")) or (key[3] == "off" and sample["effect_us"] != 0):
                    errors.append(f"Missing or invalid transit effect timing: {key}")
                    break
                continue
            if not isinstance(frame, dict) or not frame.get("rendered") or frame.get("cache_hit") or frame.get("scale") != 1.0:
                errors.append(f"Missing full-size render: {key}")
                break
            if (frame.get("internal_width"), frame.get("internal_height")) != key[1:3] or (frame.get("view_width"), frame.get("view_height")) != key[1:3]:
                errors.append(f"Different frame dimensions: {key}")
                break
            fields = ("prepare_us", "raster_us", "effect_us", "output_us", "total_us", "visible_tiles", "triangles", "threads")
            if any(type(frame.get(field)) is not int or frame[field] < 0 for field in fields):
                errors.append(f"Missing/negative renderer measurements: {key}")
                break
            if key[3] == "off" and frame["effect_us"] != 0:
                errors.append(f"Off mode unexpectedly includes effect work: {key}")
                break
        if not transit and not isinstance(cell.get("cache"), dict):
            errors.append(f"Missing cache accounting: {key}")
        if not transit:
            effect_bytes = cell.get("cache", {}).get("effect_scratch_capacity_bytes")
            if type(effect_bytes) is not int or not 0 <= effect_bytes <= 8 * key[1] * key[2] + 256 * 1024 or (key[3] == "off" and effect_bytes != 0):
                errors.append(f"Missing/excessive effect storage: {key}")
            cached = cell.get("cached_sample", {}).get("renderer", {})
            if not cached.get("cache_hit") or cached.get("rendered") or any(cached.get(field) != 0 for field in ("prepare_us", "raster_us", "effect_us", "threads")):
                errors.append(f"Missing/invalid cached-output sample: {key}")
    order = ["off", "reduced", "full"]
    order = order[rotation:] + order[:rotation]
    for key, cells in mode_groups.items():
        if [cell.get("mode") for cell in cells] != order:
            errors.append(f"Incorrect rotated mode order: {key}")
        first = cells[0]
        for cell in cells[1:]:
            if any(cell.get(field) != first.get(field) for field in ("pose", "selected_world", "frozen_snapshot", "diagnostic_camera", "observed_train_pose", "vehicle_sequence", "palette_rgba", "palette_scope")):
                errors.append(f"Modes measured different game/camera states: {key}")
        if first.get("phase") == "scene":
            geometry = lambda cell: [(s.get("renderer", {}).get("visible_tiles"), s.get("renderer", {}).get("triangles")) for s in cell.get("samples", [])]
            if any(geometry(cell) != geometry(first) for cell in cells[1:]):
                errors.append(f"Effects changed scene geometry: {key}")
    if found != expected:
        errors.append(f"Missing cells: {sorted(expected - found)}")
    return {"passed": not errors, "expected_cells": len(expected), "observed_cells": len(found), "errors": errors}


def compare_normal_viewport(output, phase):
    names = [f"gate_{phase}_normal_before_reopen.png",
             f"gate_{phase}_normal_after_reopen_close.png"]
    result = {"passed": False, "screenshots": names,
              "region_reason": "Main viewport below the 22-pixel toolbar; same paused endpoint and scroll position."}
    try:
        with Image.open(output / names[0]) as first, Image.open(output / names[1]) as second:
            if first.size != second.size or first.width != 1280 or first.height != 800:
                result["error"] = "Unexpected or different native screenshot dimensions"
                return result
            region = (0, 22, first.width, first.height)
            difference = ImageChops.difference(first.convert("RGB").crop(region),
                                              second.convert("RGB").crop(region))
            result["region"] = region
            result["difference_bbox_in_region"] = difference.getbbox()
            result["changed_pixels"] = sum(pixel != (0, 0, 0) for pixel in difference.getdata())
            result["passed"] = result["changed_pixels"] == 0
    except (OSError, ValueError) as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
    return result


def compare_effect_pixels(output, phase, manifest, baseline_root):
    labels = ["back_near_approach", "back_transit", "back_arrival_mixed"]
    if phase == "roundtrip":
        labels = ["out_near_approach", "out_transit", "out_arrival_mixed"] + labels
    rect = manifest.get("cab_view_rect", {})
    expected_rect = {"left": 0, "top": 38, "right": 479, "bottom": 307,
                     "width": 480, "height": 270}
    if rect != expected_rect:
        return {"passed": False, "error": "Missing or unexpected live cab widget rectangle",
                "observed_rect": rect, "expected_rect": expected_rect}
    # Native Rect endpoints are inclusive; Pillow's right/bottom are exclusive.
    # The next row belongs to the controls, including the new Effects button.
    region = (rect["left"], rect["top"], rect["right"] + 1, rect["bottom"] + 1)
    result = {"passed": True, "cab_view_region": region, "comparisons": [],
              "timing_scope": "80/160 labels denote injected GUI callback deltas during a naturally observed paused phase; native UI callbacks may also occur. Simulation state remains frozen."}
    captures = manifest.get("effect_captures", [])
    if sorted(c.get("label") for c in captures) != sorted(labels) or any(c.get("frozen_state_unchanged") is not True for c in captures):
        result.update(passed=False, error="Missing complete frozen effect capture sequences")
    baseline = baseline_root / phase
    for label in labels:
        prefix = f"gate_{phase}_{label}"
        pairs = [("full_motion_0_80", output / f"{prefix}.png", output / f"{prefix}_full_80.png", False),
                 ("full_motion_80_160", output / f"{prefix}_full_80.png", output / f"{prefix}_full_160.png", False),
                 ("full_visible", output / f"{prefix}_full_80.png", output / f"{prefix}_off.png", False),
                 ("reduced_static", output / f"{prefix}_reduced_0.png", output / f"{prefix}_reduced_80.png", True),
                 ("off_stable", output / f"{prefix}_off.png", output / f"{prefix}_off_again.png", True),
                 ("off_baseline_exact", output / f"{prefix}_off.png", baseline / f"{prefix}.png", True)]
        for check, first, second, expect_equal in pairs:
            comparison = {"label": label, "check": check, "files": [str(first), str(second)], "passed": False}
            try:
                with Image.open(first) as a, Image.open(second) as b:
                    if a.size != (1280, 800) or b.size != a.size:
                        raise ValueError("Unexpected or unequal native screenshot dimensions")
                    difference = ImageChops.difference(a.convert("RGB").crop(region), b.convert("RGB").crop(region))
                    changed = sum(pixel != (0, 0, 0) for pixel in difference.getdata())
                    comparison.update(changed_pixels=changed, passed=(changed == 0) == expect_equal)
            except (OSError, ValueError) as exc:
                comparison["error"] = f"{type(exc).__name__}: {exc}"
            if not check.startswith("full_motion_"):
                result["passed"] = result["passed"] and comparison["passed"]
            result["comparisons"].append(comparison)
        motion = [c for c in result["comparisons"] if c["label"] == label and c["check"].startswith("full_motion_")]
        # A sinusoid can produce equal adjacent samples around its turning point.
        # Require visible motion across the three observed frames, and retain both differences.
        result["passed"] = result["passed"] and all("error" not in c for c in motion) and any(c["passed"] for c in motion)
    return result


def compare_fixed_images(output, phase, manifest, baseline_root):
    """Exact full-size Off comparisons, separated from adaptive GUI screenshots."""
    result = {"passed": True, "comparisons": [], "scope": "Private native renderer, fixed scale 1; not a live GUI screenshot."}
    try:
        baseline_dir = baseline_root / phase
        previous = json.loads((baseline_dir / "gate-native-manifest.json").read_text())
        for collection in ("profiles", "vehicle_profiles"):
            key = lambda cell: (cell["capture_label"], cell["view_width"], cell["view_height"], cell["mode"])
            old = {key(c): c for c in previous.get(collection, []) if c.get("diagnostic_image")}
            new = {key(c): c for c in manifest.get(collection, []) if c.get("diagnostic_image")}
            if not new or set(old) != set(new):
                raise ValueError(f"Missing/mismatched fixed image cells in {collection}")
            for k, cell in new.items():
                before = old[k]
                identity = ("pose", "selected_world", "diagnostic_camera", "observed_train_pose", "vehicle_sequence", "palette_rgba", "palette_scope")
                if any(cell.get(field) != before.get(field) for field in identity):
                    raise ValueError(f"Different camera/world/vehicle state for {k}")
                with Image.open(output / cell["diagnostic_image"]) as a, Image.open(baseline_dir / before["diagnostic_image"]) as b:
                    if a.size != b.size or a.size != (cell["view_width"], cell["view_height"]):
                        raise ValueError(f"Different fixed image dimensions for {k}")
                    diff = ImageChops.difference(a.convert("RGB"), b.convert("RGB"))
                    changed = sum(pixel != (0, 0, 0) for pixel in diff.getdata())
                row = {"collection": collection, "cell": k, "changed_pixels": changed, "passed": changed == 0,
                       "candidate": str(output / cell["diagnostic_image"]), "baseline": str(baseline_dir / before["diagnostic_image"])}
                result["comparisons"].append(row)
                result["passed"] = result["passed"] and row["passed"]
    except (OSError, ValueError, KeyError) as exc:
        result.update(passed=False, error=f"{type(exc).__name__}: {exc}")
    return result


def run_phase(binary, source, output, phase, baseline, expected=None, profile=False, profile_rotation=0, vehicle_profile=False, fixed_baseline=None, stage3c_diagnostic=False, stage3d_paired=False):
    output.mkdir(parents=True, exist_ok=False)
    save = output / "fixture-copy.sav"
    shutil.copyfile(source, save)
    config = configuration(output)
    config.write_text(config.read_text().replace("[gui]\n", "[gui]\nerrmsg_duration = 0\n"))
    (output / "scripts").mkdir()
    (output / "scripts/game_start.scr").write_text("cab_gate_native_start\n")
    env = {**os.environ, **profile_environment(output), "SDL_VIDEODRIVER": "offscreen",
           "SDL_RENDER_DRIVER": "software", "OPENSPACETTD_WORLD_COUNT": "3",
           "OSTTD_CAB_NATIVE_CONFIG": str(config), "OSTTD_CAB_NATIVE_SAVE": str(save),
           "OSTTD_CAB_NATIVE_OUTPUT": str(output), "OSTTD_CAB_NATIVE_BINARY": str(binary),
           "OSTTD_CAB_GATE_PHASE": phase}
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "OSTTD_CAB_GATE_EXPECTED", "OSTTD_CAB_PROFILE", "OSTTD_CAB_PROFILE_ROTATION", "OSTTD_CAB_VEHICLE_PROFILE", "OSTTD_CAB_STAGE3C_DIAGNOSTIC", "OSTTD_CAB_STAGE3D_PAIRED"):
        env.pop(key, None)
    if profile:
        env["OSTTD_CAB_PROFILE"] = "1"
        env["OSTTD_CAB_PROFILE_ROTATION"] = str(profile_rotation)
    if vehicle_profile:
        env["OSTTD_CAB_VEHICLE_PROFILE"] = "1"
    if stage3c_diagnostic and phase == "cold":
        env["OSTTD_CAB_STAGE3C_DIAGNOSTIC"] = "1"
    if stage3d_paired and phase == "cold":
        env["OSTTD_CAB_STAGE3D_PAIRED"] = "1"
    if expected is not None:
        env["OSTTD_CAB_GATE_EXPECTED"] = str(expected)
    baseset = Path(env["XDG_DATA_HOME"]) / "openttd/baseset"
    if baseset.is_symlink():
        baseset.unlink()  # Only this run's newly created profile symlink.
    baseset.mkdir(exist_ok=True)
    installed = Path("/usr/share/games/openttd/baseset/opengfx")
    if not installed.is_dir():
        raise RuntimeError("Installed OpenGFX unavailable; no download attempted")
    (baseset / "opengfx").symlink_to(installed, target_is_directory=True)
    command = [str(binary), "Native SDL local gate cab round trip and cold reload"]
    report = {"passed": False, "phase": phase, "command": command,
              "source_save": str(source), "source_save_sha256": sha256(source),
              "binary_sha256": sha256(binary), "config_sha256": sha256(config),
              "display": "SDL offscreen / software", "human_uat": "Pending", "profiling_requested": profile,
              "profile_rotation": profile_rotation, "vehicle_profiling_requested": vehicle_profile,
              "stage3c_diagnostic_requested": stage3c_diagnostic and phase == "cold",
              "stage3d_paired_requested": stage3d_paired and phase == "cold",
              "hard_timeout_seconds": 240, "minimum_free_bytes": 100 * 1024 * 1024,
              "content_sha256": {str(p): sha256(p) for p in sorted(installed.iterdir()) if p.is_file()},
              "newgrf_sha256": {str(p): sha256(p) for p in sorted((ROOT / "bin/newgrf").glob("*.grf"))}}
    if profile or stage3c_diagnostic or stage3d_paired:
        cpu_lines = Path("/proc/cpuinfo").read_text().splitlines()
        report["machine"] = {"platform": platform.platform(), "logical_cpus": os.cpu_count(),
                             "cpu_model": next((line.split(":", 1)[1].strip() for line in cpu_lines
                                                if line.startswith("model name")), "unavailable")}
        report["sampled_process_memory_kib"] = {"rss": 0, "high_water": 0,
            "scope": "Maximum observed /proc process RSS and high-water, sampled every 100ms; final unsampled allocations may be absent."}
    started = time.monotonic()
    with (output / "native-process.log").open("x") as log:
        process = subprocess.Popen(command, cwd=output, env=env, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=subprocess.STDOUT, preexec_fn=no_core_dump)
        try:
            while process.poll() is None:
                if profile or stage3c_diagnostic or stage3d_paired:
                    try:
                        lines = Path(f"/proc/{process.pid}/status").read_text().splitlines()
                        values = {line.split(":", 1)[0]: int(line.split()[1]) for line in lines
                                  if line.startswith(("VmRSS:", "VmHWM:"))}
                        memory = report["sampled_process_memory_kib"]
                        memory["rss"] = max(memory["rss"], values.get("VmRSS", 0))
                        memory["high_water"] = max(memory["high_water"], values.get("VmHWM", 0))
                    except (OSError, ValueError):
                        pass
                if time.monotonic() - started > 240:
                    report["error"] = "Native process exceeded 240-second hard stop"
                    process.kill()
                    break
                if shutil.disk_usage(output).free < report["minimum_free_bytes"]:
                    report["error"] = "Stopped own native process below 100 MiB free disk reserve"
                    process.kill()
                    break
                time.sleep(0.1)
            process.wait()
            report["exit_code"] = process.returncode
            manifest = output / "gate-native-manifest.json"
            if manifest.is_file():
                report["native_manifest"] = json.loads(manifest.read_text())
            report["passed"] = process.returncode == 0 and report.get("native_manifest", {}).get("passed") is True
            calls = report.get("native_manifest", {}).get("explicit_state_game_loop_calls")
            report["explicit_call_limit"] = 16384 if phase == "roundtrip" else 8192
            report["explicit_call_budget_valid"] = (type(calls) is int and
                                                    0 < calls <= report["explicit_call_limit"])
            report["passed"] = report["passed"] and report["explicit_call_budget_valid"]
            report["normal_viewport_comparison"] = compare_normal_viewport(output, phase)
            report["passed"] = report["passed"] and report["normal_viewport_comparison"]["passed"]
            report["effect_pixel_comparisons"] = compare_effect_pixels(output, phase, report.get("native_manifest", {}), baseline)
            report["passed"] = report["passed"] and report["effect_pixel_comparisons"]["passed"]
            if stage3c_diagnostic:
                report["stage3c_validation"] = validate_stage3c(report.get("native_manifest", {}), phase, output)
                report["passed"] = report["passed"] and report["stage3c_validation"]["passed"]
            if stage3d_paired:
                report["stage3d_validation"] = validate_stage3d(report.get("native_manifest", {}), phase, output)
                report["passed"] = report["passed"] and report["stage3d_validation"]["passed"]
            if fixed_baseline is not None:
                report["fixed_image_comparisons"] = compare_fixed_images(output, phase, report.get("native_manifest", {}), fixed_baseline)
                report["passed"] = report["passed"] and report["fixed_image_comparisons"]["passed"]
            if profile:
                report["profile_validation"] = validate_profiles(report.get("native_manifest", {}), phase, profile_rotation, static_palette=vehicle_profile)
                report["passed"] = report["passed"] and report["profile_validation"]["passed"]
            if vehicle_profile:
                report["vehicle_profile_validation"] = validate_profiles(report.get("native_manifest", {}), phase, profile_rotation, vehicle=True, static_palette=True)
                report["passed"] = report["passed"] and report["vehicle_profile_validation"]["passed"]
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            report["elapsed_seconds"] = time.monotonic() - started
            report["source_save_unchanged"] = sha256(source) == report["source_save_sha256"]
            report["copy_sha256_after"] = sha256(save)
            report["screenshots"] = {p.name: {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in sorted(output.glob("*.png"))}
            if vehicle_profile:
                report["fixed_diagnostic_images"] = {}
                for cell in report.get("native_manifest", {}).get("profiles", []) + report.get("native_manifest", {}).get("vehicle_profiles", []):
                    name = cell.get("diagnostic_image")
                    if name is None:
                        continue
                    path = output / name
                    valid = path.parent == output and path.is_file()
                    entry = {"passed": False}
                    if valid:
                        with Image.open(path) as picture:
                            valid = picture.size == (cell["view_width"], cell["view_height"])
                        entry.update(bytes=path.stat().st_size, sha256=sha256(path), passed=valid)
                    report["fixed_diagnostic_images"][name] = entry
                    report["passed"] = report["passed"] and valid
            report["passed"] = report["passed"] and report["source_save_unchanged"]
            (output / "runner-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--save", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="New evidence directory")
    parser.add_argument("--test-binary", type=Path, default=ROOT / "build/openttd_test")
    parser.add_argument("--profile", action="store_true", help="Measure naturally paused gate phases without simulation advancement")
    parser.add_argument("--stage3c-diagnostic", action="store_true", help="One cold counterbalanced timing diagnostic and current-fixture vehicle palette preflight; excludes the acceptance matrix")
    parser.add_argument("--stage3d-paired", action="store_true", help="One cold same-renderer Off/Reduced experiment with Off/Off controls; excludes other profiling")
    parser.add_argument("--vehicle-profile", action="store_true", help="Also profile a private camera beside a naturally arrived visible vehicle; requires --profile")
    parser.add_argument("--profile-rotation", type=int, choices=(0, 1, 2), default=0,
                        help="Rotate Off/Reduced/Full measurement order across fresh-process repeats")
    parser.add_argument("--fixed-image-baseline", type=Path, help="Additionally require exact private full-scale scene/vehicle Off images against this Stage3B baseline run")
    parser.add_argument("--baseline", type=Path, default=ROOT / "build/cab-stage3-baseline/near-repeat-01",
                        help="Preserved pre-effect native images, containing roundtrip and cold directories")
    args = parser.parse_args()
    if args.stage3d_paired and (args.stage3c_diagnostic or args.profile or args.vehicle_profile or args.fixed_image_baseline):
        parser.error("--stage3d-paired is separate from other diagnostics and acceptance-matrix profiling")
    if args.stage3c_diagnostic and (args.profile or args.vehicle_profile or args.fixed_image_baseline):
        parser.error("--stage3c-diagnostic is separate from acceptance-matrix profiling")
    if args.vehicle_profile and not args.profile:
        parser.error("--vehicle-profile requires --profile")
    if args.fixed_image_baseline and not args.vehicle_profile:
        parser.error("--fixed-image-baseline requires --vehicle-profile")
    fixed_baseline = args.fixed_image_baseline.resolve(strict=True) if args.fixed_image_baseline else None
    source = args.save.resolve(strict=True)
    binary = args.test_binary.resolve(strict=True)
    baseline = args.baseline.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {"passed": False, "phases": [], "source_save_sha256": sha256(source)}
    try:
        first = run_phase(binary, source, output / "roundtrip", "roundtrip", baseline, profile=args.profile, profile_rotation=args.profile_rotation, vehicle_profile=args.vehicle_profile, fixed_baseline=fixed_baseline, stage3c_diagnostic=args.stage3c_diagnostic, stage3d_paired=args.stage3d_paired)
        report["phases"].append(first)
        if not first["passed"]:
            raise RuntimeError("Initial native round trip failed; cold phase not started")
        checkpoint = output / "roundtrip/gate-checkpoint.sav"
        expected = output / "roundtrip/gate-checkpoint.json"
        if not checkpoint.is_file() or not expected.is_file():
            raise RuntimeError("Initial proof omitted its native save or frozen comparison snapshot")
        report["checkpoint_sha256"] = sha256(checkpoint)
        report["expected_snapshot_sha256"] = sha256(expected)
        cold = run_phase(binary, checkpoint, output / "cold", "cold", baseline, expected, profile=args.profile, profile_rotation=args.profile_rotation, vehicle_profile=args.vehicle_profile, fixed_baseline=fixed_baseline, stage3c_diagnostic=args.stage3c_diagnostic, stage3d_paired=args.stage3d_paired)
        report["phases"].append(cold)
        report["explicit_state_game_loop_calls"] = sum(
            p.get("native_manifest", {}).get("explicit_state_game_loop_calls", 0)
            for p in report["phases"])
        report["combined_explicit_call_limit"] = 24576
        report["passed"] = cold["passed"] and report["explicit_state_game_loop_calls"] <= 24576
    except (RuntimeError, OSError, ValueError) as exc:
        report["error"] = f"{type(exc).__name__}: {exc}"
    finally:
        report["source_save_unchanged"] = sha256(source) == report["source_save_sha256"]
        report["passed"] = report["passed"] and report["source_save_unchanged"]
        (output / "gate-runner-evidence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": report["passed"], "completed_phases": len(report["phases"]),
                      "source_save_unchanged": report["source_save_unchanged"], "error": report.get("error")}))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
