#!/usr/bin/env python3
"""Publish and validate an explicitly coordinated federation checkpoint.

The caller must first drain commands/HTTP and pause all participating game servers. This
module validates the resulting files; it cannot make live, unrelated saves atomic.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil


def digest(path):
    """Hash an artifact without loading a game binary into memory."""
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def publish(directory, binary, saves, authority, *, phase):
    """Publish a new checkpoint after the caller's coordination barrier."""
    directory = Path(directory)
    if directory.exists():
        raise ValueError("Checkpoint directory must be new; preserve the previous checkpoint")
    if len(saves) < 2 or any(not isinstance(world, int) or world < 0 or world >= 2**32-1 for world in saves):
        raise ValueError("Checkpoint requires saves for at least two valid worlds")
    state = json.loads(Path(authority).read_text())
    if not isinstance(state.get("transfers"), dict) or set(state.get("worlds", {})) != {str(world) for world in saves}:
        raise ValueError("Checkpoint requires saves for every authority world")
    sources = {f"world{world}.sav": Path(path) for world, path in sorted(saves.items())}
    sources["authority.json"] = Path(authority)
    for source in sources.values():
        if not source.is_file() or source.stat().st_size == 0:
            raise ValueError(f"Missing or empty checkpoint artifact: {source}")
    directory.mkdir(parents=True)
    manifest = {"version": 1, "contract": "latest-coordinated-checkpoint", "phase": phase,
                "binary_sha256": digest(binary), "files": {}}
    for name, source in sources.items():
        target = directory / name
        shutil.copyfile(source, target)
        with target.open("rb") as stream:
            os.fsync(stream.fileno())
        manifest["files"][name] = digest(target)
    pending = directory / "checkpoint.json.tmp"
    with pending.open("w") as stream:
        json.dump(manifest, stream, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    pending.replace(directory / "checkpoint.json")
    fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)
    return manifest


def validate(directory, binary):
    """Reject incomplete, mixed or modified checkpoint artifacts before restart."""
    directory = Path(directory)
    manifest = json.loads((directory / "checkpoint.json").read_text())
    if manifest.get("version") != 1 or manifest.get("contract") != "latest-coordinated-checkpoint":
        raise ValueError("Unsupported checkpoint contract")
    if manifest.get("binary_sha256") != digest(binary):
        raise ValueError("Checkpoint binary differs from the verified session")
    state = json.loads((directory / "authority.json").read_text())
    worlds = state.get("worlds", {})
    if len(worlds) < 2 or any(not world.isdigit() or str(int(world)) != world or int(world) >= 2**32-1 for world in worlds):
        raise ValueError("Invalid checkpoint world set")
    if set(manifest.get("files", {})) != {"authority.json", *(f"world{world}.sav" for world in worlds)}:
        raise ValueError("Checkpoint artifact set is incomplete")
    for name, expected in manifest["files"].items():
        if digest(directory / name) != expected:
            raise ValueError(f"Checkpoint artifact changed: {name}")
    return manifest
