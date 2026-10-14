#!/usr/bin/env python3
"""Open the frozen mission in a new private local profile, preserving evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence', type=Path, help='Directory from the one bounded native attempt')
    parser.add_argument('--binary', type=Path, help='Explicit exact retained binary; defaults to the frozen evidence engine')
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    binary = args.binary.resolve() if args.binary else evidence / 'engine/openttd'
    report = json.loads((evidence / 'evidence.json').read_text())
    opening = evidence / 'opening.sav'
    if not opening.is_file() or report['save_sha256'].get('opening.sav') != hashlib.sha256(opening.read_bytes()).hexdigest():
        raise SystemExit('The frozen opening is absent or its hash differs; preserve the evidence and inspect its blocker.')
    if report['binary_sha256'] != hashlib.sha256(binary.read_bytes()).hexdigest():
        raise SystemExit('Binary differs from the retained opening proof. Use its exact binary or revalidate compatibility explicitly.')
    for filename, expected in report['engine_file_sha256'].items():
        if filename == 'openttd':
            continue  # The explicitly selected executable was checked above.
        path = binary.parent / filename
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit(f'Frozen runtime file is absent or changed: {filename}')
    for pack, expected in report['content_sha256'].items():
        if hashlib.sha256((binary.parent / 'newgrf' / f'openspacettd_{pack}.grf').read_bytes()).hexdigest() != expected:
            raise SystemExit(f'Content hash differs: {pack}')
    profile = Path(tempfile.mkdtemp(prefix='first-line-play-', dir=ROOT / 'build'))
    shutil.copy2(opening, profile / 'opening.sav')
    config = (evidence / 'first-line.cfg').read_text()
    for pack in report['content_sha256']:
        lines = config.splitlines()
        indexes = [i for i, line in enumerate(lines) if line.strip().endswith(f'openspacettd_{pack}.grf =')]
        if len(indexes) != 1:
            raise SystemExit(f'Private config does not name exactly one pack: {pack}')
        lines[indexes[0]] = f'{binary.parent}/newgrf/openspacettd_{pack}.grf ='
        config = '\n'.join(lines) + '\n'
    (profile / 'first-line.cfg').write_text(config)
    # -c places saves, hotkeys, windows and private configuration in cwd.
    # Never copy the proof startup FIFO script into a human play profile.
    print(f'Private mission profile: {profile}', flush=True)
    print(f"Verification status: {report['status']}", flush=True)
    if 'blocker' in report:
        print(f"Retained blocker: {report['blocker']}", flush=True)
    print('Unpause to receive the Story Book. Save your progress under a new name.', flush=True)
    os.chdir(profile)
    environment = {**os.environ, 'OPENSPACETTD_WORLD_COUNT': '7'}
    environment.pop('SDL_VIDEODRIVER', None)
    os.execve(binary, [str(binary), '-x', '-c', str(profile / 'first-line.cfg'), '-g', str(profile / 'opening.sav')], environment)


if __name__ == '__main__':
    main()
