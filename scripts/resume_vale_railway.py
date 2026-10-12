#!/usr/bin/env python3
"""Resume the retained owner save with a verified compatible Vale runtime."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

from first_permanent_line import ROOT, digest, require


def verified_delivery(path):
    delivery = json.loads(path.read_text())
    manifest_path = Path(delivery['runtime_manifest'])
    require(digest(manifest_path) == delivery['runtime_manifest_sha256'], 'Runtime manifest differs')
    manifest = json.loads(manifest_path.read_text())
    binary = Path(manifest['binary'])
    for candidate in (binary, *binary.parents):
        require(not candidate.is_symlink(), 'Runtime path contains a symlink')
    require(digest(binary) == manifest['binary_sha256'], 'Executable differs')
    inventory = set()
    for candidate in binary.parent.rglob('*'):
        require(not candidate.is_symlink(), 'Runtime inventory contains a symlink')
        if candidate.is_file():
            inventory.add(str(candidate.relative_to(binary.parent)))
    require(inventory == {'openttd'} | set(manifest['files']), 'Runtime inventory differs')
    for relative, expected in manifest['files'].items():
        require(not Path(relative).is_absolute() and '..' not in Path(relative).parts, 'Unsafe content path')
        require(digest(binary.parent / relative) == expected, f'Content differs: {relative}')
    for key in ('save', 'config'):
        candidate = Path(delivery[key])
        for ancestor in (candidate, *candidate.parents):
            require(not ancestor.is_symlink(), f'{key} path contains a symlink')
        require(digest(candidate) == delivery[f'{key}_sha256'], f'Retained {key} differs')
    return delivery, manifest, binary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--delivery', type=Path, required=True)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--profile', type=Path)
    args = parser.parse_args(argv)
    delivery, manifest, binary = verified_delivery(args.delivery)
    if args.verify_only:
        print('Verified compatible runtime and retained owner save; no game launched.')
        return 0
    require(shutil.disk_usage(ROOT).free > 1024 ** 3, 'Less than 1 GiB headroom')
    profile = args.profile or ROOT / 'build' / f'vale-resume-player-{time.time_ns()}'
    require(profile.is_absolute() and profile.parent.resolve() == (ROOT / 'build').resolve(), 'Use a new direct build profile')
    profile.mkdir(exist_ok=False)
    shutil.copyfile(delivery['save'], profile / 'owner-progress.sav')
    shutil.copyfile(delivery['config'], profile / 'vale.cfg')
    print('Resuming a verified Vale save copy in a new player profile.', flush=True)
    return subprocess.run([str(binary), '-X', '-x', '-c', str(profile / 'vale.cfg'), '-g', str(profile / 'owner-progress.sav')], cwd=profile).returncode


if __name__ == '__main__':
    raise SystemExit(main())
