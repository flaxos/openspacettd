#!/usr/bin/env python3
"""One bounded native attempt for The First Permanent Line v0.1.

This is scenario authoring/UAT, not a cargo or town simulator. All gameplay is
performed by native commands and StateGameLoop. A failed attempt is retained;
another attempt needs a new explicit proof budget.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import shutil
import subprocess
import tempfile
import time

from test_wp11_slice import OfflineEngine, ROOT, require

PACKS = ('integrated_v1', 'rail_v3', 'equipment_v1', 'gates_v1')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class MissionEngine(OfflineEngine):
    def wait(self, marker):
        try:
            return self.wait_marker(marker)
        except BaseException:
            self.abort()
            raise

    def abort(self):
        if hasattr(self, 'process') and self.process.poll() is None:
            self.process.kill()
            self.process.wait()

    def wait_marker(self, marker):
        deadline = min(self.deadline, time.monotonic() + 60)
        while True:
            remaining = deadline - time.monotonic()
            require(remaining > 0, f'Timed out waiting for {marker}')
            try:
                line = self.queue.get(timeout=min(remaining, 1))
            except queue.Empty:
                require(self.process.poll() is None, f'Native process exited waiting for {marker}')
                self.check_script()
                continue
            require(line is not None, f'Native process exited waiting for {marker}')
            self.log.write(line)
            self.log.flush()
            self.lines.append(line.strip())
            require(not any(text in line for text in ('FIRSTLINE FAIL', 'CONNECTED FAIL', 'Assertion failed', 'Saving map failed')),
                    line.strip())
            if marker in line:
                self.check_script()
                return line.split(marker, 1)[1].strip()

    def check_script(self):
        if not hasattr(self, 'stdout_log'):
            return
        text = Path(self.stdout_log.name).read_text(errors='replace')
        require(not re.search(r'Assertion failed|Your script made an error|script has crashed|Script died', text, re.I),
                'Native assertion or GameScript failure; see retained stdout log')

    def state(self):
        return json.loads(self.command('first_line status', 'FIRSTLINE state '))


def record(report, output):
    (output / 'evidence.json').write_text(json.dumps(report, indent=2) + '\n')


def advance(engine, report, output, phase):
    state = json.loads(engine.command('first_line advance', 'FIRSTLINE advance '))
    report[phase].append(state)
    record(report, output)  # Retain the failure sample before checking it.
    require(state['evaluations_advanced'] == 1, 'Advance did not perform exactly one native economy evaluation')
    require(not state['mission']['invalid'], f"Observer invalid: {state['mission']['reason']}")
    require(not state['cargo_errors'], f"Cargo custody mismatch: {state['cargo_errors']}")
    require(state['cash_conserved'], 'Cash custody differs from native transaction ledger')
    require(state['money'] > 0, 'Development stake exhausted')
    require(not any(train.get('crashed', False) for train in state.get('trains', [])), 'Train crashed')
    return state


def validate_audit(audits):
    require(set(audits) == {'english_US.lng', 'english.lng'}, 'Both English languages were not audited')
    for filename, audit in audits.items():
        require(audit['language'] == ('en_US' if filename == 'english_US.lng' else 'en_GB'), 'Wrong loaded audit language')
        require(audit['invalid_slopes'] == 0 and audit['invalid_foundations'] == 0, 'Terrain/foundation integrity failure')
        require(audit['pixel_queries'] > 0 and audit['viewport_queries'] > 0, 'Native tile queries missing')
        for cargo in audit['cargos']:
            require(all(isinstance(value, str) and value and '(undefined' not in value and '(invalid' not in value
                        for key, value in cargo.items() if key != 'label'), f'Invalid loaded cargo text: {cargo}')


def comparable(state):
    # Flow audit is an interval receipt, not persistent state. Everything else
    # including native custody, vehicle orders, actual houses, cash and FLIN is
    # compared without reconstruction or Python-calculated house counts.
    return {key: value for key, value in state.items() if key not in
            ('audit', 'cash_conserved', 'cargo_errors', 'evaluations_advanced')}


def run(args):
    binary = args.binary.resolve()
    require(binary.is_file() and os.access(binary, os.X_OK), 'Built native binary missing')
    require(shutil.disk_usage(ROOT).free > 1024 * 1024 * 1024, 'Less than 1 GiB disk headroom before native saves')
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='first-line-', dir=ROOT / 'build'))
    if args.output:
        output.mkdir(parents=True, exist_ok=False)
    require(args.attempt_id is None or re.fullmatch(r'[a-z0-9][a-z0-9-]{0,47}', args.attempt_id),
            'Attempt identifier must be 1–48 lowercase letters, digits or hyphens')
    # A renewed owner-approved budget gets its own exclusive lock. The original
    # failed attempt lock is never removed, renamed or reused.
    lock_name = 'first-line-native-attempt' + (f'-{args.attempt_id}' if args.attempt_id else '') + '.lock'
    lock = ROOT / 'build' / lock_name
    with lock.open('x') as stream:
        stream.write(str(output) + '\n')
    # Freeze the exact executable, language, script and GRF content used by the
    # attempt. Later build work must not silently change a retained save's player.
    frozen = output / 'engine'
    frozen.mkdir()
    shutil.copy2(binary, frozen / 'openttd')
    for directory in ('lang', 'game', 'newgrf', 'baseset'):
        shutil.copytree(binary.parent / directory, frozen / directory)
    binary = frozen / 'openttd'
    config = output / 'first-line.cfg'
    text = (ROOT / 'demo/integrated_economy.cfg').read_text()
    # Minimal (1) still creates forced neutral industries. This authored opening
    # requires FundedOnly (0); native generation retains legal resource sites.
    require(text.count('resource_density = 1') == 1, 'Canonical resource-density setting differs')
    text = text.replace('resource_density = 1', 'resource_density = 0')
    text = text.replace('[gui]\n', '[gui]\nlanguage = english_US.lng\n')
    text = text.replace('none =', 'OpenSpaceTTD-First-Permanent-Line =')
    for pack in PACKS:
        text = text.replace(f'openspacettd_{pack}.grf =', f'{binary.parent}/newgrf/openspacettd_{pack}.grf =')
    config.write_text(text)
    report = {'schema': 1, 'scenario': 'The First Permanent Line v0.1', 'status': 'RUNNING', 'passed': False,
              'attempt_id': args.attempt_id or 'original', 'attempt_lock': str(lock),
              'base': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'binary_path': str(binary),
              'binary_sha256': digest(binary), 'config_sha256': digest(config),
              'content_sha256': {pack: digest(binary.parent / 'newgrf' / f'openspacettd_{pack}.grf') for pack in PACKS},
              'engine_file_sha256': {str(path.relative_to(frozen)): digest(path)
                                     for path in sorted(frozen.rglob('*')) if path.is_file()},
              'source_sha256': {path: digest(ROOT / path) for path in subprocess.check_output(
                  ['git', 'ls-files', '--modified', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
                  if (ROOT / path).is_file()},
              'budget': {'smoke_evaluations': 2, 'proof_evaluations': 24, 'proof_seconds': 1800,
                         'reload_evaluations': 4, 'reload_seconds': 300},
              'human_acceptance': 'PENDING: fun, comprehension, 30–60 minute pacing, UI readability',
              'smoke': [], 'proof': [], 'reload': [], 'native_processes': [], 'post_baseline_grants': 0}
    record(report, output)
    engine = None
    try:
        smoke = output / 'smoke'; smoke.mkdir()
        engine = MissionEngine(binary, config, smoke, 'smoke')
        report['native_processes'].append(engine.process.pid)
        report['opening'] = json.loads(engine.command('first_line prepare', 'FIRSTLINE prepared '))
        require(not any(report['opening']['baseline']['custody']['total']), 'Authored baseline contains cargo')
        require(report['opening']['baseline']['mission']['economy_evaluations'] == 0, 'Opening already advanced')
        manifest = json.loads((ROOT / 'demo/first_permanent_line/manifest.json').read_text())
        manifest['native_bindings'] = report['opening']['baseline']['mission']['binding']
        manifest['native_opening_assets'] = report['opening']['baseline']
        manifest['cash_stake_gbp'] = report['opening']['stake']
        manifest['binary_sha256'] = report['binary_sha256']
        manifest['content_sha256'] = report['content_sha256']
        (output / 'scenario-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        engine.save(output / 'opening.sav')
        report['save_sha256'] = {'opening.sav': digest(output / 'opening.sav')}
        record(report, output)
        print(f"First Permanent Line opening: {output / 'opening.sav'}; SHA256 {report['save_sha256']['opening.sav']}; smoke pending", flush=True)
        report['terrain_and_text'] = json.loads(engine.command('first_line audit', 'FIRSTLINE audit '))
        record(report, output)
        validate_audit(report['terrain_and_text'])
        for _ in range(2):
            advance(engine, report, output, 'smoke')
        require(report['smoke'][-1]['goals'] == 4 and report['smoke'][-1]['story_pages'] >= 1, 'Native mission Story Book/goals did not initialize')
        engine.save(output / 'smoke.sav')
        engine.close(); engine = None
        # The only real play attempt starts again from the frozen opening.
        proof = output / 'proof'; proof.mkdir()
        start = time.monotonic(); deadline = start + 1800
        engine = MissionEngine(binary, config, proof, 'proof', save=output / 'opening.sav', deadline=deadline)
        engine.deadline = deadline
        report['native_processes'].append(engine.process.pid)
        report['opening_cold_loaded'] = engine.state()
        record(report, output)
        require(comparable(report['opening_cold_loaded']) == comparable(report['opening']['baseline']), 'Opening changed across fresh-process load')
        report['construction'] = json.loads(engine.command('first_line build', 'FIRSTLINE built '))
        engine.save(output / 'operational.sav')
        for _ in range(24):
            state = advance(engine, report, output, 'proof')
            mission = state['mission']
            if all(mission['delivered']) and all(mission['produced']) and not mission['commissioned'] and mission['stored'][0] >= 160 and mission['stored'][1] >= 80:
                engine.save(output / 'before-commission.sav')
                receipt = json.loads(engine.command('first_line commission', 'FIRSTLINE commissioned '))
                report['commission'] = receipt
                record(report, output)
                before, after = receipt['before'], receipt['after']
                require(receipt['cost'] == 50000 and before['money'] - after['money'] == 50000, 'Commission cash cost differs')
                require(before['mito_kit'][0] - after['mito_kit'][0] == 160 and
                        before['mito_kit'][1] - after['mito_kit'][1] == 80, 'Commission did not consume exact native Mito kit')
                require(after['mission']['commissioned'] and not after['mission']['invalid'], 'Native commission did not validate')
                state = after
                engine.save(output / 'commissioned.sav')
            if state['mission']['awarded']:
                break
        report['proof_elapsed_seconds'] = time.monotonic() - start
        report['terminal'] = engine.state()
        report['status'] = 'NATIVE_VICTORY' if report['terminal']['mission']['awarded'] else 'BUDGET_EXHAUSTED'
        engine.save(output / 'terminal.sav')
        engine.close(); engine = None
        reload = output / 'reload'; reload.mkdir()
        deadline = time.monotonic() + 300
        engine = MissionEngine(binary, config, reload, 'reload', save=output / 'terminal.sav', deadline=deadline)
        engine.deadline = deadline
        report['native_processes'].append(engine.process.pid)
        report['cold_loaded'] = engine.state()
        require(comparable(report['terminal']) == comparable(report['cold_loaded']), 'Fresh-process terminal reload changed persistent/native state')
        report['fresh_process_reload_equal'] = True
        for _ in range(4):
            advance(engine, report, output, 'reload')
        require(all(sample['goals'] == 4 and sample['story_pages'] <= 2 for sample in report['reload']), 'Mission UI duplicated on fresh-process reload')
        if report['terminal']['mission']['awarded']:
            require(report['reload'][-1]['mission']['awarded'] and report['reload'][-1]['story_pages'] == 2, 'Latched victory or single ending did not persist')
        engine.save(output / 'reloaded.sav')
        report['passed'] = report['terminal']['mission']['awarded']
        report['status'] += '_RELOAD_OK'
    except Exception as error:
        report['status'] = 'STOPPED_AT_FIRST_FAILURE'
        report['blocker'] = str(error)
        if engine is not None:
            engine.abort()
        raise
    finally:
        if engine is not None:
            engine.close()
        report['save_sha256'] = {path.name: digest(path) for path in output.glob('*.sav')}
        record(report, output)
        print(f"First Permanent Line: {report['status']}; evidence {output}", flush=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/openttd')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--attempt-id', help='Unique identifier for one explicitly renewed owner-approved budget; keeps the original lock intact')
    parser.add_argument('--diagnostic-only', action='store_true',
                        help='One paused native preparation diagnostic, with no gameplay advancement')
    args = parser.parse_args(argv)
    if args.diagnostic_only:
        from first_line_diagnostic import run_diagnostic
        return run_diagnostic(args)
    return run(args)


if __name__ == '__main__':
    main()
