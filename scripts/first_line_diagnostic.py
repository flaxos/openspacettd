#!/usr/bin/env python3
"""A single paused, five-minute native First Permanent Line preparation diagnostic.

The diagnostic retains its exclusive attempt lock and every artifact on failure.
It never advances the game, resumes simulation, or enters the proof runner.
"""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import threading
import time

from first_permanent_line import MissionEngine, PACKS, ROOT, record, require


def _check(deadline, clock, stage):
    require(clock() < deadline, f'Five-minute diagnostic deadline exhausted at {stage}')


def _digest(path, deadline, clock):
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        while True:
            _check(deadline, clock, f'hashing {path.name}')
            data = stream.read(1024 * 1024)
            if not data:
                break
            sha.update(data)
    _check(deadline, clock, f'hashing {path.name}')
    return sha.hexdigest()


def _copy(source, destination, deadline, clock):
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open('rb') as src, destination.open('xb') as dst:
        while True:
            _check(deadline, clock, f'freezing {source.name}')
            data = src.read(1024 * 1024)
            if not data:
                break
            dst.write(data)
    shutil.copystat(source, destination)
    _check(deadline, clock, f'freezing {source.name}')


def _freeze(binary, output, deadline, clock):
    frozen = output / 'engine'
    frozen.mkdir()
    _copy(binary, frozen / 'openttd', deadline, clock)
    for directory in ('lang', 'game', 'newgrf', 'baseset'):
        source = binary.parent / directory
        require(source.is_dir(), f'Missing native content directory: {source}')
        for path in sorted(source.rglob('*')):
            _check(deadline, clock, f'freezing {directory}')
            relative = path.relative_to(source)
            target = frozen / directory / relative
            if path.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            elif path.is_file():
                _copy(path, target, deadline, clock)
            else:
                raise RuntimeError(f'Unsupported native content entry: {path}')
    binary = frozen / 'openttd'
    config = output / 'first-line.cfg'
    _check(deadline, clock, 'reading canonical config')
    text = (ROOT / 'demo/integrated_economy.cfg').read_text()
    require(text.count('resource_density = 1') == 1, 'Canonical resource-density setting differs')
    text = text.replace('resource_density = 1', 'resource_density = 0')
    text = text.replace('[gui]\n', '[gui]\nlanguage = english_US.lng\n')
    text = text.replace('none =', 'OpenSpaceTTD-First-Permanent-Line =')
    for pack in PACKS:
        text = text.replace(f'openspacettd_{pack}.grf =', f'{binary.parent}/newgrf/openspacettd_{pack}.grf =')
    config.write_text(text)
    _check(deadline, clock, 'writing frozen config')
    hashes = {str(path.relative_to(frozen)): _digest(path, deadline, clock)
              for path in sorted(frozen.rglob('*')) if path.is_file()}
    return binary, config, hashes


class DiagnosticEngine(MissionEngine):
    """Offline FIFO engine with a process-owning absolute-deadline watchdog."""

    def __init__(self, binary, config, output, name, save=None, seed=11, world_count=7, deadline=None, editor=False, process_started=None,
                 startup_bridge='connected_economy functional-bridge'):
        require(deadline is not None, 'Diagnostic engine requires an absolute deadline')
        self._hard_deadline = deadline
        self._watchdog_stop = threading.Event()
        self._watchdog = threading.Thread(target=self._watch, daemon=True)
        self._watchdog.start()
        try:
            super().__init__(binary, config, output, name, save=save, seed=seed,
                             world_count=world_count, deadline=deadline, editor=editor, process_started=process_started,
                             startup_bridge=startup_bridge)
            require(time.monotonic() < deadline, 'Diagnostic startup exceeded the absolute deadline')
        except BaseException:
            self.abort()
            self.close()
            raise

    def _watch(self):
        if self._watchdog_stop.wait(max(0, self._hard_deadline - time.monotonic())):
            return
        # Popen may still be starting as the deadline expires. Kill only the
        # process this instance actually owns, once it becomes available.
        while not self._watchdog_stop.is_set():
            process = getattr(self, 'process', None)
            if process is not None:
                if process.poll() is None:
                    process.kill()
                return
            self._watchdog_stop.wait(0.01)

    def abort(self):
        process = getattr(self, 'process', None)
        if process is not None and process.poll() is None:
            process.kill()
            try:
                process.wait(timeout=0.1)
            except subprocess.TimeoutExpired:
                pass

    def close(self):
        process = getattr(self, 'process', None)
        if process is not None:
            stdin = getattr(process, 'stdin', None)
            if stdin is not None and not stdin.closed and time.monotonic() < self._hard_deadline:
                try:
                    stdin.write('quit\n')
                    stdin.flush()
                except (BrokenPipeError, OSError):
                    pass
            if stdin is not None and not stdin.closed:
                try:
                    stdin.close()
                except (BrokenPipeError, OSError):
                    pass
            if process.poll() is None:
                remaining = max(0, self._hard_deadline - time.monotonic())
                try:
                    process.wait(timeout=min(remaining, 1))
                except subprocess.TimeoutExpired:
                    self.abort()
        self._watchdog_stop.set()
        if threading.current_thread() is not self._watchdog:
            self._watchdog.join(timeout=0.1)
        for name in ('commands', 'stdout_log', 'log'):
            stream = getattr(self, name, None)
            if stream is not None and not stream.closed:
                stream.close()


def _empty_custody(custody):
    require(isinstance(custody, dict) and 'total' in custody, 'Native custody observation missing')
    for name in ('industry_inputs', 'industry_outputs', 'stations', 'trains', 'cities',
                 'stocks', 'research', 'total'):
        values = custody.get(name)
        require(isinstance(values, list) and values and all(type(value) is int and value == 0 for value in values),
                f'Native {name} custody is not empty')


def _catalog_valid(catalog):
    require(catalog['player_built'] is True, 'Native resource site manager is not in player-built mode')
    economy = catalog['integrated_economy']
    require(type(economy['version']) is int and economy['version'] > 0,
            'Native integrated economy is not enabled')
    roles = economy['roles']
    require(isinstance(roles, list) and all(isinstance(row, list) and len(row) == 2 for row in roles),
            'Native economic world roles are missing')
    role_map = {row[0]: row[1] for row in roles}
    require(len(role_map) == len(roles) and all(role_map.get(world) == role for world, role in
                                               ((0, 2), (1, 1), (2, 0))),
            'Native Core, Industrial and Frontier world roles differ')
    require(type(catalog['invalid_height_edges']) is int and catalog['invalid_height_edges'] == 0,
            'Native map contains invalid height edges')
    worlds = catalog['stellar_worlds']
    require(isinstance(worlds, list) and len(worlds) == 7,
            'Native stellar world catalog does not contain seven worlds')
    for index, world in enumerate(worlds):
        require(isinstance(world, list) and len(world) >= 6 and world[0] == index and type(world[5]) is bool,
                f'Invalid native stellar world {index}')
        if index < 4:
            require(world[5], f'Native world {index} is not accessible')
    require(isinstance(catalog['site_worlds'], dict) and isinstance(catalog['resource_details'], list),
            'Native resource site/role catalog missing')
    require(catalog['sites'] > 0 and catalog['unoccupied_sites'] > 0 and catalog['resource_details'],
            'Native resource site catalog has no available resources')


def _snapshot_valid(snapshot, tick):
    require(snapshot['paused'] is True, 'Native game is not paused')
    require(type(snapshot['tick']) is int and snapshot['tick'] == tick, 'Native tick advanced while paused')
    mission = snapshot['mission']
    require(mission['active'] is True and mission['invalid'] is False and
            type(mission['economy_evaluations']) is int and mission['economy_evaluations'] == 0,
            'Prepared mission is inactive, invalid, or already evaluated')
    _empty_custody(snapshot['custody'])


def _funding_lines(engine):
    path = getattr(engine, 'console_path', None)
    lines = []
    if path is not None and Path(path).is_file():
        lines = Path(path).read_text(errors='replace').splitlines()
    elif engine is not None:
        lines = getattr(engine, 'lines', [])
    receipts = []
    for line in lines:
        if 'FIRSTLINE funding ' in line:
            raw = line.split('FIRSTLINE funding ', 1)[1].strip()
            try:
                receipts.append(json.loads(raw))
            except json.JSONDecodeError:
                receipts.append({'raw': raw})
    return receipts


def run_diagnostic(args, engine_factory=DiagnosticEngine, clock=time.monotonic):
    """Run exactly one paused native preparation attempt and retain its evidence."""
    start = clock()
    deadline = start + 300
    require(args.attempt_id is not None and re.fullmatch(r'[a-z0-9][a-z0-9-]{0,47}', args.attempt_id),
            'Diagnostic mode needs a unique 1–48 character lowercase attempt ID')
    require(args.output is not None, 'Diagnostic mode needs an explicit new output directory')
    binary = args.binary.resolve()
    require(binary.is_file() and os.access(binary, os.X_OK), 'Built native binary missing')
    require(shutil.disk_usage(ROOT).free > 1024 * 1024 * 1024, 'Less than 1 GiB disk headroom before native saves')
    _check(deadline, clock, 'preflight')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    lock = ROOT / 'build' / f'first-line-native-attempt-{args.attempt_id}.lock'
    with lock.open('x') as stream:
        stream.write(str(output) + '\n')
    report = {'schema': 1, 'scenario': 'The First Permanent Line v0.1',
              'diagnostic_only': True, 'status': 'RUNNING', 'passed': False, 'native_acceptance': False,
              'attempt_id': args.attempt_id, 'attempt_lock': str(lock), 'output': str(output),
              'deadline_seconds': 300, 'smoke': [], 'proof': [], 'reload': [],
              'native_processes': [], 'funding_receipts': [], 'save_sha256': {}}
    record(report, output)
    engine = None
    try:
        binary, config, file_hashes = _freeze(binary, output, deadline, clock)
        report.update(binary_path=str(binary), binary_sha256=file_hashes['openttd'],
                      config_sha256=_digest(config, deadline, clock), engine_file_sha256=file_hashes,
                      content_sha256={pack: file_hashes[f'newgrf/openspacettd_{pack}.grf'] for pack in PACKS})
        record(report, output)
        _check(deadline, clock, 'native startup')
        engine = engine_factory(binary, config, output, 'diagnostic', seed=11, world_count=7, deadline=deadline)
        process = getattr(engine, 'process', None)
        if process is not None:
            report['native_processes'].append(process.pid)
        record(report, output)

        def command(text, marker, stage):
            _check(deadline, clock, stage + ' dispatch')
            value = engine.command(text, marker)
            _check(deadline, clock, stage + ' response')
            return value

        command('pause', 'paused.', 'pause')
        pre = json.loads(command('connected_economy basket-status', 'CONNECTED basket-state ', 'native basket'))
        report['pre_state'] = pre
        record(report, output)
        observation = pre['observation']
        require(observation['paused'] is True, 'Native pause did not hold before preparation')
        _empty_custody(observation['custody'])
        tick = pre['state']['tick']
        require(type(tick) is int and tick >= 0, 'Native pre-preparation tick missing')

        _check(deadline, clock, 'preparation map save')
        engine.save(output / 'diagnostic-map.sav')
        _check(deadline, clock, 'preparation map saved')
        report['save_sha256']['diagnostic-map.sav'] = _digest(output / 'diagnostic-map.sav', deadline, clock)
        record(report, output)

        catalog = json.loads(command('resource_sites', 'RESOURCE state ', 'native resource catalog'))
        report['resource_catalog'] = catalog
        catalog_path = output / 'resource-catalog.json'
        catalog_path.write_text(json.dumps(catalog, indent=2) + '\n')
        report['resource_catalog_sha256'] = _digest(catalog_path, deadline, clock)
        record(report, output)
        _catalog_valid(catalog)

        opening = json.loads(command('first_line prepare', 'FIRSTLINE prepared ', 'preparation'))
        report['opening'] = opening
        record(report, output)
        _snapshot_valid(opening['baseline'], tick)

        _check(deadline, clock, 'prepared map save')
        engine.save(output / 'prepared-diagnostic.sav')
        _check(deadline, clock, 'prepared map saved')
        report['save_sha256']['prepared-diagnostic.sav'] = _digest(output / 'prepared-diagnostic.sav', deadline, clock)
        record(report, output)

        _check(deadline, clock, 'prepared status dispatch')
        post = engine.state()
        _check(deadline, clock, 'prepared status response')
        report['prepared_status'] = post
        record(report, output)
        _snapshot_valid(post, tick)
        report['status'] = 'DIAGNOSTIC_PREPARED'
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
            except Exception as error:
                if report['status'] == 'DIAGNOSTIC_PREPARED':
                    report['status'] = 'STOPPED_AT_FIRST_FAILURE'
                    report['blocker'] = f'Diagnostic engine close failed: {error}'
        report['funding_receipts'] = _funding_lines(engine)
        report['elapsed_seconds'] = clock() - start
        record(report, output)
        print(f"First Permanent Line diagnostic: {report['status']}; evidence {output}", flush=True)
