#!/usr/bin/env python3
"""One finite SDL rendering proof on a paused byte-identical owner-save copy."""
import argparse
import json
from pathlib import Path
import re
import shutil
import threading
import time

from first_permanent_line import ROOT, digest, require
from resume_vale_railway import verified_delivery
from inspect_first_line_map import decode_chunks, decode_table
from test_wp11_slice import OfflineEngine, console_payload


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--delivery', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    delivery, manifest, binary = verified_delivery(args.delivery)
    output = args.output
    require(output.is_absolute() and output.parent.resolve() == (ROOT / 'build').resolve(), 'Fresh direct build output required')
    require(not output.exists() and not output.is_symlink(), 'Output already exists')
    require(shutil.disk_usage(ROOT).free > 1024 ** 3, 'Less than 1 GiB headroom')
    lock = output.with_suffix('.lock')
    with lock.open('x') as stream:
        json.dump({'delivery': str(args.delivery), 'wall_cap': 300, 'evaluations': 0}, stream)
    output.mkdir(exist_ok=False)
    for key, name in (('save', 'owner-progress.sav'), ('config', 'vale.cfg')):
        shutil.copyfile(delivery[key], output / name)
        require(digest(output / name) == delivery[f'{key}_sha256'], f'Copy differs: {key}')
    chunks = decode_chunks((output / 'owner-progress.sav').read_bytes())
    saved_mission = json.loads(decode_table(chunks['W0OP'])[0]['data'])
    saved_clock = decode_table(chunks['DATE'])[0]
    started = time.monotonic()
    evidence = {'passed': False, 'evaluation_cap': 0, 'actual_evaluations': 0, 'wall_cap_seconds': 300,
                'input_save_sha256': delivery['save_sha256'], 'binary_sha256': manifest['binary_sha256']}
    engine = timer = None
    try:
        def watchdog(process):
            nonlocal timer
            timer = threading.Timer(max(0.01, 300 - (time.monotonic() - started)), process.kill)
            timer.start()
        engine = OfflineEngine(binary, output / 'vale.cfg', output, 'purchase-ui', save=output / 'owner-progress.sav',
                               world_count=1, deadline=started + 300, startup_bridge='vale bridge',
                               pause_loaded_save=True, process_started=watchdog)
        engine.deadline = started + 300
        initial = json.loads(engine.command('vale status', 'VALE state '))
        require(initial['mission'] == saved_mission, 'Native cold load changed persistent mission state')
        require(initial['mission']['economy_evaluations'] == saved_mission['economy_evaluations'] and
                initial['tick'] == saved_clock['tick_counter'] and
                initial['rng'] == [saved_clock['random_state[0]'], saved_clock['random_state[1]']],
                'Native cold load advanced the retained clock, evaluation counter or simulation RNG')
        engine.command('vale purchase-ui', 'VALE purchase-ui queued')
        # Keep deferred native receipts logged, then EOF releases the queued draw work.
        engine.process.stdin.write(f'script "{engine.console_path}"\n')
        engine.process.stdin.flush()
        engine.process.stdin.close()
        engine.process.wait(timeout=max(0.01, started + 300 - time.monotonic()))
        require(engine.process.returncode == 0, 'Native process did not exit successfully')
        lines = engine.console_path.read_text().splitlines()
        complete = '\n'.join(lines) + Path(engine.stdout_log.name).read_text(errors='replace')
        require(not re.search(r'VALE FAIL|CONNECTED FAIL|Assertion failed|Your script made an error|script has crashed|Script died|Saving map failed', complete, re.I),
                'Native UI integrity, command or script failure')
        def receipt(marker):
            rows = [console_payload(line).split(marker, 1)[1] for line in lines if marker in console_payload(line)]
            require(len(rows) == 1, f'Exactly one native receipt required: {marker}')
            return json.loads(rows[0])
        before, after = receipt('VALE purchase-ui before '), receipt('VALE purchase-ui after ')
        require(initial['rng'] == before['rng'], 'Simulation RNG changed before purchase')
        require(initial['money'] == before['money'] and initial['trains'] == before['trains'],
                'Cash or fleet changed before the GUI click')
        for key in ('mission', 'economy', 'tick', 'map_fnv1a64', 'industries', 'stations', 'stocks', 'research'):
            require(initial[key] == before[key] == after[key], f'Paused integrity differs: {key}')
        old = {row['id']: row for row in before['trains']}
        new = [row for row in after['trains'] if row['id'] not in old]
        require(len(new) == 1, 'GUI did not purchase exactly one wagon')
        for row in after['trains']:
            if row['id'] in old:
                require(row == old[row['id']], 'Existing train changed')
        require(new[0]['cargo_type'] == before['cargo_labels']['BALL'] and new[0]['capacity'] == 40 and new[0]['cargo'] == 0,
                'Purchased wagon is not an empty forty-unit BALL car')
        require(before['money'] > after['money'], 'GUI purchase was not paid')
        screenshots = sorted(output.rglob('vale-ball-purchase-*.png'))
        require(len(screenshots) == 2, 'Both native viewport screenshots required')
        evidence.update(passed=True, initial=initial, before=before, after=after,
                        actual_evaluations=after['mission']['economy_evaluations'] - saved_mission['economy_evaluations'],
                        cash_debit=before['money'] - after['money'], new_wagon=new[0],
                        screenshots={str(path.relative_to(output)): digest(path) for path in screenshots})
    except BaseException as error:
        evidence['error'] = str(error)
        raise
    finally:
        if engine is not None:
            engine.close()
            evidence['process_exit_code'] = engine.process.returncode
        if timer is not None:
            timer.cancel()
        evidence['elapsed_seconds'] = time.monotonic() - started
        evidence['owner_save_unchanged'] = digest(Path(delivery['save'])) == delivery['save_sha256']
        (output / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
