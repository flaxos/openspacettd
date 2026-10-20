#!/usr/bin/env python3
"""Load a verified untouched Vale opening with an isolated local configuration."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time

from first_permanent_line import ROOT, digest, require


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--opening', type=Path, required=True, help='Successful preparation output directory')
    parser.add_argument('--profile', type=Path, help='New direct build directory for this player session')
    args = parser.parse_args(argv)
    opening = args.opening.resolve()
    evidence = json.loads((opening / 'evidence.json').read_text())
    require(evidence['phase'] == 'prepare' and evidence['passed'] and evidence['process_exited'] and evidence['process_exit_code'] == 0,
            'Opening must have a successful, closed preparation receipt')
    manifest_path = Path(evidence['runtime_manifest'])
    require(digest(manifest_path) == evidence['runtime_manifest_sha256'], 'Runtime manifest differs from opening receipt')
    manifest = json.loads(manifest_path.read_text())
    binary = Path(manifest['binary'])
    for path in (binary, *binary.parents):
        require(not path.is_symlink(), 'Opening runtime path contains a symlink')
    require(digest(binary) == evidence['binary_sha256'] == manifest['binary_sha256'], 'Opening executable differs')
    inventory = set()
    for path in binary.parent.rglob('*'):
        require(not path.is_symlink(), 'Opening runtime contains a symlink')
        if path.is_file():
            inventory.add(str(path.relative_to(binary.parent)))
    require(inventory == {'openttd'} | set(manifest['files']), 'Opening runtime inventory differs')
    for relative, expected in manifest['files'].items():
        require(digest(binary.parent / relative) == expected, f'Opening content differs: {relative}')
    save = opening / 'founding-opening.sav'
    require(digest(save) == evidence['save_sha256'][save.name], 'Untouched opening save differs')
    require(digest(opening / 'vale.cfg') == evidence['config_sha256'], 'Opening configuration differs')
    profile = args.profile or ROOT / 'build' / f'vale-player-{time.time_ns()}'
    require(profile.is_absolute() and profile.parent.resolve() == (ROOT / 'build').resolve(), 'Use a new direct build profile')
    profile.mkdir(exist_ok=False)
    shutil.copyfile(opening / 'vale.cfg', profile / 'vale.cfg')
    shutil.copyfile(save, profile / 'founding-opening.sav')
    print('The Vale Railway: finish Quarry to Works, bring BALL to Greybank, and keep the return service running.', flush=True)
    result = subprocess.run([str(binary), '-X', '-x', '-c', str(profile / 'vale.cfg'), '-g', str(profile / 'founding-opening.sav')], cwd=profile)
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
