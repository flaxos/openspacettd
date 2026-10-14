#!/usr/bin/env python3
"""One explicitly dispatched Vale preparation, route check, or fresh-process reload.

Uses the existing offline FIFO lifecycle and absolute process watchdog. Python
checks native receipts; it neither simulates production nor awards objectives.
Every run reserves a unique retained lock; failure ends the run without retry.
"""
import argparse
import configparser
import copy
import json
import os
from pathlib import Path
import queue
import re
import shutil
import subprocess
import time

from first_line_diagnostic import DiagnosticEngine, _check, _copy, _digest
from first_permanent_line import PACKS, ROOT, comparable, digest, record, require, validate_audit


class ValeEngine(DiagnosticEngine):
    def __init__(self, *args, **options):
        require('startup_bridge' not in options, 'Vale startup bridge is fixed')
        super().__init__(*args, startup_bridge='vale bridge', **options)

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
            require(not any(text in line for text in ('VALE FAIL', 'CONNECTED FAIL', 'Assertion failed', 'Saving map failed')), line.strip())
            if marker in line:
                self.check_script()
                return line.split(marker, 1)[1].strip()

    def state(self):
        return json.loads(self.command('vale status', 'VALE state '))


def load_comparable(state):
    """Only native CommandDuringPause bookkeeping is transient on cold load."""
    value = copy.deepcopy(comparable(state))
    require(value['paused'] is True and type(value['pause_mode']) is int and value['pause_mode'] & ~128 == 1,
            'Cold comparison requires only the native Normal pause flag')
    value['pause_mode'] &= ~128
    return value


def preflight(args, deadline, clock=time.monotonic):
    _check(deadline, clock, "preflight")
    require(re.fullmatch(r'[a-z0-9][a-z0-9-]{0,47}', args.attempt_id or ''), 'Unique attempt ID required')
    require(re.fullmatch(r'[0-9a-f]{40}', args.reviewed_source or ''), 'Exact parent-reviewed source commit required')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True, timeout=max(0.01, deadline - clock())).strip()
    require(head == args.reviewed_source, 'Checkout differs from reviewed source')
    require(not subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True, timeout=max(0.01, deadline - clock())).strip(), 'Reviewed checkout must be clean')
    require(_digest(args.runtime_manifest, deadline, clock) == args.runtime_sha256, 'Reviewed runtime manifest bytes differ')
    manifest = json.loads(args.runtime_manifest.read_text())
    require(manifest['source_commit'] == args.reviewed_source, 'Runtime source identity differs')
    raw_binary = Path(manifest['binary'])
    require(raw_binary.is_absolute(), 'Runtime executable must have an absolute path')
    for path in (raw_binary, *raw_binary.parents):
        require(not path.is_symlink(), 'Runtime executable path contains a symlink')
    binary = raw_binary.resolve()
    require(binary.is_file() and os.access(binary, os.X_OK), 'Pinned native executable unavailable')
    require(_digest(binary, deadline, clock) == manifest['binary_sha256'], 'Pinned executable bytes differ')
    require(not binary.is_symlink(), 'Runtime executable must not be a symlink')
    inventory = set()
    for path in binary.parent.rglob('*'):
        _check(deadline, clock, 'runtime inventory')
        require(not path.is_symlink(), 'Runtime inventory contains a symlink')
        if path.is_file():
            inventory.add(str(path.relative_to(binary.parent)))
    require(inventory == {'openttd'} | set(manifest['files']), 'Pinned runtime inventory differs')
    for relative, expected in manifest['files'].items():
        require(not Path(relative).is_absolute() and '..' not in Path(relative).parts, 'Invalid runtime manifest path')
        require(_digest(binary.parent / relative, deadline, clock) == expected, f'Pinned runtime content differs: {relative}')
    graphics = 'baseset/opengfx/opengfx.obg'
    require(graphics in manifest['files'], 'Pinned OpenGFX descriptor missing')
    descriptor = configparser.ConfigParser(interpolation=None)
    descriptor.read_string((binary.parent / graphics).read_text())
    require(descriptor.get('metadata', 'name') == 'OpenGFX', 'Pinned graphics set is not OpenGFX')
    require(set(descriptor['files']) == {'base', 'arctic', 'tropical', 'toyland', 'logos', 'extra'}, 'Pinned OpenGFX file roles differ')
    for name in descriptor['files'].values():
        require(name == Path(name).name and name not in ('', '.', '..'), 'Unsafe OpenGFX reference')
        require(f'baseset/opengfx/{name}' in manifest['files'], f'Pinned OpenGFX reference missing: {name}')
    for relative, expected in manifest['source_sha256'].items():
        require(not Path(relative).is_absolute() and '..' not in Path(relative).parts, 'Invalid source manifest path')
        require(_digest(ROOT / relative, deadline, clock) == expected, f'Compiled source receipt differs: {relative}')
    require(shutil.disk_usage(ROOT).free > 1024 ** 3, 'Less than 1 GiB headroom before native saves')
    output = args.output
    require(output.is_absolute() and output.parent.resolve() == (ROOT / 'build').resolve() and not output.exists() and not output.is_symlink(), 'Output must be a fresh direct build sibling')
    require(output.resolve() != binary.parent and output.resolve() not in binary.parents, 'Output overlaps runtime')
    if args.phase != 'prepare':
        require(args.save is not None and args.save.is_file(), 'A separately retained native save is required')
        require(args.expected_state is not None and args.expected_state.is_file(), 'Exact paused native comparison state required')
        require(_digest(args.save, deadline, clock) == args.save_sha256, 'Reviewed input save bytes differ')
        require(_digest(args.expected_state, deadline, clock) == args.state_sha256, 'Reviewed native comparison bytes differ')
        predecessor = json.loads((args.save.parent / 'evidence.json').read_text())
        require(predecessor['passed'] and predecessor.get('process_exited') is True, 'Predecessor was not successful and confirmed closed')
    return manifest, binary


def validate_state(state, baseline=False):
    require(state['paused'] is True, 'Native snapshot is not paused')
    m = state['mission']
    require(m['active'] and not m['unavailable'], f"Native mission unavailable: {m.get('reason')}")
    require(len(state['worlds']) == 1 and state['worlds'][0]['id'] == 0 and state['worlds'][0]['phase'] == 3 and state['worlds'][0]['economic_role'] == 0, 'Native Phase3/Frontier one-world profile differs')
    require(len(state['towns']) == 3 and all(town['population'] > 0 for town in state['towns']), 'Three native populated settlements required')
    require(state['money'] > 0, 'Founding capital exhausted')
    require(not any(t.get('lost') or t.get('crashed') for t in state['trains']), 'Native train lost/crashed')
    if baseline:
        require(m['economy_evaluations'] == 0 and m['accepted_silc'] == 0 and m['qualified_batches'] == 0 and m['stored_ball'] == 0 and not m['journeys'] and not m['awarded'], 'Opening already has objective evidence')
        require(state['money'] == 750000 and len(state['industries']) == 2 and len(state['stations']) == 4, 'Declared founding assets differ')
        require(len(state['trains']) == 3 and all(t['cargo'] == 0 and (not t['front'] or t['stopped'] and not t['orders']) for t in state['trains']), 'Starter consist must be empty/stopped without orders')
        require(sum(state['custody']['total']) == 80 and all(sum(state['custody'][key]) == 0 for key in ('industry_inputs', 'industry_outputs', 'stations', 'trains', 'cities', 'research')), 'Only disclosed 80 STEL import may exist at baseline')


def advance(engine, report, output, phase):
    state = json.loads(engine.command('vale advance', 'VALE advance '))
    report[phase].append(state)
    record(report, output)  # Retain the first failure receipt before checking.
    require(not state.get('train_integrity_failure'), 'Native train integrity failed within this evaluation')
    require(state['evaluations_advanced'] == 1, 'Exactly one actual native economy evaluation required')
    require(not state['cargo_errors'] and state['cash_conserved'], 'Native cargo/cash ledger failed')
    require(not state['audit']['gate_tolls'], 'Single-world opening unexpectedly used a gate')
    validate_state(state)
    return state


def useful_input_visits(samples, front, station, cargo):
    """Check native positive industry receipts per native arrival; fragments share a visit."""
    visits = sorted(row[0] for sample in samples for row in sample['audit']['arrivals'] if row[1] == front and row[2] == station)
    credited = set()
    for sample in samples:
        for row in sample['audit']['payments']:
            if row[1] != front or row[2] != station or row[3] != cargo or row[4] <= 0:
                continue
            prior = [tick for tick in visits if tick <= row[0]]
            require(prior, 'Native accepted industry receipt lacks an arrival')
            credited.add(prior[-1])
    return len(credited)


def run(args, engine_factory=ValeEngine, clock=time.monotonic):
    start = clock()
    deadline = start + 300
    manifest, binary = preflight(args, deadline, clock)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    lock = ROOT / 'build' / f'vale-native-{args.attempt_id}.lock'
    with lock.open('x') as stream:
        stream.write(str(output) + '\n')
    report = {'schema': 1, 'scenario': 'The Vale Railway v0.1', 'phase': args.phase,
              'status': 'RUNNING', 'passed': False, 'human_acceptance': 'Pending',
              'reviewed_source': args.reviewed_source, 'runtime_manifest': str(args.runtime_manifest.resolve()),
              'runtime_manifest_sha256': digest(args.runtime_manifest), 'binary_sha256': manifest['binary_sha256'],
              'attempt_lock': str(lock), 'wall_cap_seconds': 300, 'evaluation_cap': 0 if args.phase == 'prepare' else 12 if args.phase == 'routes' else 4,
              'evaluations': [], 'native_processes': [], 'save_sha256': {}}
    record(report, output)
    engine = None
    processes = []

    def process_started(process):
        processes.append(process)
        report['native_processes'].append(process.pid)
        record(report, output)

    try:
        config = output / 'vale.cfg'
        text = (ROOT / 'demo/vale_railway/vale.cfg').read_text()
        for pack in PACKS:
            text = text.replace(f'openspacettd_{pack}.grf =', f'{binary.parent}/newgrf/openspacettd_{pack}.grf =')
        config.write_text(text)
        report['config_sha256'] = digest(config)
        _check(deadline, clock, 'startup')
        input_save = None
        if args.save is not None:
            input_save = output / 'input.sav'
            _copy(args.save, input_save, deadline, clock)
            _copy(args.expected_state, output / 'input-state.json', deadline, clock)
            require(_digest(input_save, deadline, clock) == args.save_sha256, 'Copied save differs')
            report['input_save_sha256'] = args.save_sha256
            report['input_state_sha256'] = args.state_sha256
        engine = engine_factory(binary, config, output, args.phase, save=input_save, world_count=1, deadline=deadline, editor=args.phase == 'prepare', process_started=process_started)
        if getattr(engine, 'process', None) is not None:
            if engine.process not in processes:
                process_started(engine.process)
        record(report, output)
        if args.phase == 'prepare':
            prepared = json.loads(engine.command('vale prepare', 'VALE prepared '))
            report['prepared'] = prepared
            record(report, output)
            state = engine.state()
            validate_state(state, baseline=True)
        else:
            state = engine.state()
            report['cold_state'] = state
            record(report, output)
            expected = json.loads((output / 'input-state.json').read_text())
            require(load_comparable(state) == load_comparable(expected), 'Fresh-process native save comparison differs')
            validate_state(state, baseline=args.phase == 'routes')
        audit = json.loads(engine.command('vale audit', 'VALE audit '))
        report['terrain_english_audit'] = audit
        record(report, output)
        validate_audit(audit)
        require(comparable(engine.state()) == comparable(state), 'Audit/query changed native state')
        save_name = 'founding-opening.sav' if args.phase == 'prepare' else 'route-smoke.sav' if args.phase == 'routes' else 'reloaded.sav'
        if args.phase == 'routes':
            built = json.loads(engine.command('vale build', 'VALE built '))
            report['paid_construction'] = built
            record(report, output)
            feeder = built['feeder']
            stopped_at = None
            starved = False
            recovered = False
            for _ in range(12):
                _check(deadline, clock, 'route evaluation')
                state = advance(engine, report, output, 'evaluations')
                m = state['mission']
                if stopped_at is None and m['input_goal']:
                    report['interruption_before'] = json.loads(engine.command(f'vale stop {feeder}', 'VALE train '))
                    stopped_at = m['economy_evaluations']
                elif stopped_at is not None and 'interruption_after' not in report:
                    crusher = next(row for row in state['industries'] if row['id'] == m['binding'][1])
                    starved |= all(row[1] == 0 for row in crusher['inputs'])
                    if m['economy_evaluations'] - stopped_at >= 2 and starved:
                        report['interruption_after'] = json.loads(engine.command(f'vale restart {feeder}', 'VALE train '))
                        report['restart_batches'] = m['qualified_batches']
                elif 'interruption_after' in report:
                    recovered |= m['qualified_batches'] > report['restart_batches']
                if m['awarded'] and starved and recovered:
                    break
            require(state['mission']['awarded'], 'Finite route cap reached without native founding award')
            require(starved and recovered, 'Finite cap did not witness native empty-input/recovery')
            source = state['quarry_source_station']
            visits = [arrival for sample in report['evaluations'] for arrival in sample['audit']['arrivals'] if arrival[1] == feeder and arrival[2] == source]
            require(len(visits) >= 2, 'Two real quarry return calls were not witnessed')
            input_station = next(row['id'] for row in state['stations'] if row['name'] == 'Stonecross Works Input')
            silc = next(row['cargo_type'] for row in expected['trains'] if not row['front'])
            require(useful_input_visits(report['evaluations'], feeder, input_station, silc) >= 2, 'Two distinct useful native quarry-to-works deliveries were not witnessed')
            export = built['export']
            yard = state['mission']['binding'][2]
            payments = [row for sample in report['evaluations'] for row in sample['audit']['payments'] if row[1] == export and row[2] == yard]
            require(state['mission']['stored_ball'] >= 40 and all(row[5] == 0 for row in payments), 'Native BALL storage must have zero sale revenue')
            require(not any(row[1] == export and row[2] == yard and row[3] > 0 for sample in report['evaluations'] for row in sample['audit']['cash_payments']), 'Warehouse freight unexpectedly earned sale cash')
        elif args.phase == 'reload':
            # Exact comparison above is first proof. Native GS resumption gets at most four evaluations.
            for _ in range(4):
                state = advance(engine, report, output, 'evaluations')
            require(len(state['goals']) == 3 and len({row[0] for row in state['goals']}) == 3, 'Duplicated/missing native mission goals after reload')
            titles = [row[2] for row in state['story_pages']]
            require(titles.count('The Vale Railway') == 1 and titles.count('Vale Railway is established') <= 1, 'Duplicated native mission pages after reload')
        _check(deadline, clock, 'save')
        engine.save(output / save_name)
        report['save_sha256'][save_name] = _digest(output / save_name, deadline, clock)
        state = engine.state()
        (output / 'expected-state.json').write_text(json.dumps(comparable(state), indent=2) + '\n')
        report['expected_state_sha256'] = _digest(output / 'expected-state.json', deadline, clock)
        report['final_state'] = state
        report['status'] = 'PAUSED_OPENING_PREPARED' if args.phase == 'prepare' else 'NATIVE_ROUTE_CHECK_PASSED' if args.phase == 'routes' else 'FRESH_PROCESS_RELOAD_PASSED'
        report['passed'] = True
        return report
    except Exception as error:
        report['status'] = 'STOPPED_AT_FIRST_FAILURE'
        report['blocker'] = str(error)
        if engine is not None:
            engine.abort()
        return report
    finally:
        if engine is not None:
            try:
                engine.close()
                process = getattr(engine, 'process', None)
                report['process_exited'] = process is not None and process.poll() is not None
                report['process_exit_code'] = None if process is None else process.poll()
                require(report['process_exited'], 'Native process exit was not confirmed')
                require(report['process_exit_code'] == 0, 'Native process did not exit cleanly')
                if hasattr(engine, 'check_script'):
                    engine.check_script()
                _check(deadline, clock, 'final process exit')
            except Exception as error:
                if report.get('blocker'):
                    report['cleanup_failure'] = str(error)
                else:
                    report['blocker'] = f'Process closure failed: {error}'
                report['status'] = 'STOPPED_AT_FIRST_FAILURE'
                report['passed'] = False
        elif processes:
            # Startup may fail inside the constructor after Popen and cleanup.
            report['process_exited'] = all(process.poll() is not None for process in processes)
            report['startup_process_exit_codes'] = [process.poll() for process in processes]
        report['elapsed_seconds'] = clock() - start
        record(report, output)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('prepare', 'routes', 'reload'), required=True)
    parser.add_argument('--reviewed-source', required=True)
    parser.add_argument('--runtime-manifest', type=Path, required=True)
    parser.add_argument('--runtime-sha256', required=True)
    parser.add_argument('--attempt-id', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--save', type=Path)
    parser.add_argument('--expected-state', type=Path)
    parser.add_argument('--save-sha256')
    parser.add_argument('--state-sha256')
    args = parser.parse_args(argv)
    report = run(args)
    print(f"Vale Railway: {report['status']}; evidence {args.output}")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
