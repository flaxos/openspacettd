#!/usr/bin/env python3
"""Migrate the preserved v1.0 demo to v1.1 using acknowledged engine commands.

Run from the repository root. Never overwrites an existing output. No new GRFs
are injected into the saved game; unplayable content stays a documented blocker.
"""
import argparse
import queue
import subprocess
import threading
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', default='demo/OpenSpaceTTD-All-Features-UAT-v1.1.sav')
    parser.add_argument('--verify', help='Load and verify an existing save instead of creating one')
    args = parser.parse_args()
    target = Path(args.output).resolve()
    if target.exists() and not args.verify:
        raise SystemExit(f'Refusing to overwrite {target}')
    messages = queue.Queue()
    proc = subprocess.Popen([
        './build/openttd', '-D127.0.0.1:0', '-s', 'null', '-m', 'null', '-x', '-X',
        '-c', 'demo/uat_demo.cfg', '-d', 'script=5',
        '-g', args.verify or 'demo/OpenSpaceTTD-All-Features-UAT-v1.0.sav',
    ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)

    def read():
        for line in proc.stdout:
            messages.put(line)
        messages.put(None)

    threading.Thread(target=read, daemon=True).start()
    seen = []

    def wait_for(marker, timeout=90):
        if any(marker in line for line in seen):
            return
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = messages.get(timeout=max(.01, deadline-time.monotonic()))
            except queue.Empty:
                break
            if line is None:
                raise RuntimeError(f'Engine exited before {marker}')
            print(line, end='', flush=True)
            seen.append(line)
            if 'UAT verification failed:' in line or 'UAT setup incomplete:' in line or 'UAT setup failed:' in line:
                raise RuntimeError(line.strip())
            if marker in line:
                return
        raise RuntimeError(f'Timed out awaiting {marker}')

    def send(command):
        proc.stdin.write(command + '\n')
        proc.stdin.flush()

    try:
        if args.verify:
            wait_for('OpenSpaceTTD UAT Demo restored from savegame;')
            send('setup_uat_fixtures verify')
            wait_for('UAT verification passed:')
            send('quit')
            proc.wait(timeout=15)
            return
        wait_for('UAT v1.1 coverage ready:')
        send('setup_uat_fixtures ownership')
        wait_for('UAT ownership verified: 10 complete terminals')
        wait_for('Sprint 28 fixtures ready:')
        send('setup_uat_fixtures verify')
        wait_for('UAT verification passed:')
        send(f'save "{target.with_suffix("")}"')
        wait_for('Map successfully saved to')
        send('quit')
        proc.wait(timeout=15)
        if proc.returncode != 0 or not target.is_file():
            raise RuntimeError('Save generation did not complete')
        print(f'Created {target}; human acceptance remains Not run.')
    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=10)


if __name__ == '__main__':
    main()
