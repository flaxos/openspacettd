#!/usr/bin/env python3
"""Offline orchestration regressions; never launch or generate a native world."""

import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import first_line_diagnostic as diagnostic
import first_permanent_line as runner


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def custody():
    return {key: [0] * 64 for key in ('industry_inputs', 'industry_outputs', 'stations',
                                    'trains', 'cities', 'stocks', 'research', 'total')}


def catalog():
    names = ('Mito', 'Merredin', 'Clonclurry', 'Fourth', 'Fifth', 'Sixth', 'Seventh')
    return {'player_built': True, 'sites': 2, 'industries': 0, 'unoccupied_sites': 2,
            'integrated_economy': {'version': 1, 'roles': [[0, 2], [1, 1], [2, 0]]},
            'stellar_worlds': [[i, 100 + i, name, i * 64, 0, i < 4]
                              for i, name in enumerate(names)],
            'site_worlds': {'2': 2}, 'resource_details': [[2, 'IRON', 100], [2, 'SILC', 200]],
            'invalid_height_edges': 0}


def snapshot():
    return {'paused': True, 'tick': 100, 'custody': custody(),
            'mission': {'active': True, 'invalid': False, 'economy_evaluations': 0},
            'economy': {'version': 2}, 'trains': [{'stopped': True, 'cargo': 0}]}


class Clock:
    def __init__(self):
        self.value = 100.0

    def __call__(self):
        return self.value

    def advance(self, seconds):
        self.value += seconds


class Process:
    def __init__(self, pid=12345):
        self.pid = pid
        self.stdin = io.StringIO()
        self.running = True
        self.kills = 0
        self.waits = []

    def poll(self):
        return None if self.running else 0

    def kill(self):
        self.kills += 1
        self.running = False

    def wait(self, timeout=None):
        self.waits.append(timeout)
        if self.running:
            raise subprocess.TimeoutExpired('offline mock', timeout)
        return 0


class NativeMock:
    """Strict native-shaped responses, with checkpoints observed at dispatch."""
    def __init__(self, owner, binary, config, output, name, **kwargs):
        self.owner = owner
        self.output = output
        self.lines = []
        self.process = Process()
        self.aborted = False
        self.closed = False
        owner.factory_calls.append((binary, config, output, name, kwargs))
        owner.engine = self
        owner.hook('startup', self)

    def command(self, text, marker):
        self.owner.events.append(text)
        self.owner.hook(text, self)
        if text == 'pause':
            self.owner.test.assertEqual(marker, 'paused.')
            return ''
        if text == 'connected_economy basket-status':
            return json.dumps(self.owner.pre)
        if text == 'resource_sites':
            return json.dumps(self.owner.catalog)
        if text == 'first_line prepare':
            # The map, catalog and their hashes must already be durable before
            # the only mutating scenario command can be dispatched.
            evidence = json.loads((self.output / 'evidence.json').read_text())
            self.owner.test.assertEqual(evidence['save_sha256']['diagnostic-map.sav'],
                                        sha(self.output / 'diagnostic-map.sav'))
            self.owner.test.assertEqual(evidence['resource_catalog_sha256'],
                                        sha(self.output / 'resource-catalog.json'))
            self.lines.append('FIRSTLINE funding ' + json.dumps({'schema': 1, 'status': 'funded'}))
            if self.owner.prepare_error:
                raise RuntimeError(self.owner.prepare_error)
            return json.dumps({'stake': 100000000, 'baseline': self.owner.opening})
        if text == 'first_line status':
            return json.dumps(self.owner.post)
        raise AssertionError(f'Unapproved diagnostic command: {text}')

    def state(self):
        return json.loads(self.command('first_line status', 'FIRSTLINE state '))

    def save(self, path):
        self.owner.events.append('save ' + path.name)
        self.owner.hook('save ' + path.name, self)
        path.write_bytes(b'offline fixture, not a native save: ' + path.name.encode())

    def abort(self):
        self.aborted = True
        self.process.kill()

    def close(self):
        self.closed = True
        self.process.running = False


class RunFixture:
    def __init__(self, test, root):
        self.test = test
        self.root = root
        self.clock = Clock()
        self.events = []
        self.factory_calls = []
        self.engine = None
        self.hook = lambda stage, engine: None
        self.prepare_error = None
        self.pre = {'version': 1, 'operation': 'basket-status', 'state': {'tick': 100},
                    'observation': {'paused': True, 'custody': custody()}}
        self.catalog = catalog()
        self.opening = snapshot()
        self.post = snapshot()
        (root / 'build').mkdir()
        (root / 'demo').mkdir()
        (root / 'demo/integrated_economy.cfg').write_bytes(
            (runner.ROOT / 'demo/integrated_economy.cfg').read_bytes())
        self.binary = root / 'build/openttd'
        self.binary.write_text('offline fixture, never execute\n')
        self.binary.chmod(0o755)
        for directory in ('lang', 'game', 'newgrf', 'baseset'):
            (root / 'build' / directory).mkdir()
            (root / 'build' / directory / 'fixture').write_bytes(directory.encode())
        for pack in runner.PACKS:
            (root / f'build/newgrf/openspacettd_{pack}.grf').write_bytes(pack.encode())
        self.old_lock = root / 'build/first-line-native-attempt-owner-old.lock'
        self.old_lock.write_bytes(b'historical failure custody\n')
        self.old_hash = sha(self.old_lock)
        self.args = SimpleNamespace(binary=self.binary, output=root / 'diagnostic-output',
                                    attempt_id='offline-test')

    def factory(self, *args, **kwargs):
        return NativeMock(self, *args, **kwargs)

    def run(self):
        with patch.object(diagnostic, 'ROOT', self.root), \
                patch.object(diagnostic.shutil, 'disk_usage', return_value=SimpleNamespace(free=2 ** 31)), \
                patch('builtins.print'):
            report = diagnostic.run_diagnostic(self.args, engine_factory=self.factory, clock=self.clock)
        self.test.assertEqual(sha(self.old_lock), self.old_hash)
        if self.args.output.exists() and (self.args.output / 'evidence.json').exists():
            self.test.assertEqual(json.loads((self.args.output / 'evidence.json').read_text()), report)
        return report


class OrchestrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='first-line-diagnostic-test-')
        self.addCleanup(self.temp.cleanup)
        self.fixture = RunFixture(self, Path(self.temp.name))

    def test_one_paused_preparation_retains_native_inputs_and_prior_locks(self):
        f = self.fixture
        result = f.run()
        self.assertEqual(result['status'], 'DIAGNOSTIC_PREPARED')
        self.assertEqual(f.events, ['pause', 'connected_economy basket-status', 'save diagnostic-map.sav',
                                    'resource_sites', 'first_line prepare', 'save prepared-diagnostic.sav',
                                    'first_line status'])
        self.assertEqual(len(f.factory_calls), 1)
        binary, config, output, name, options = f.factory_calls[0]
        self.assertEqual(options, {'seed': 11, 'world_count': 7, 'deadline': 400.0})
        self.assertEqual(name, 'diagnostic')
        self.assertEqual(binary, output / 'engine/openttd')
        self.assertTrue(result['diagnostic_only'])
        self.assertFalse(result['passed'])
        self.assertFalse(result['native_acceptance'])
        self.assertEqual((result['smoke'], result['proof'], result['reload']), ([], [], []))
        self.assertEqual(result['native_processes'], [12345])
        self.assertEqual(result['funding_receipts'], [{'schema': 1, 'status': 'funded'}])
        self.assertEqual(set(result['save_sha256']), {'diagnostic-map.sav', 'prepared-diagnostic.sav'})
        self.assertEqual(result['binary_sha256'], sha(f.binary))
        text = config.read_text()
        self.assertIn('resource_density = 0', text)
        self.assertIn('language = english_US.lng', text)
        self.assertIn('OpenSpaceTTD-First-Permanent-Line =', text)
        for pack in runner.PACKS:
            self.assertEqual(result['content_sha256'][pack],
                             sha(f.root / f'build/newgrf/openspacettd_{pack}.grf'))
        self.assertTrue(f.engine.closed)
        self.assertFalse(f.engine.aborted)

    def test_first_native_prepare_failure_keeps_map_catalog_receipts_and_stops(self):
        f = self.fixture
        f.prepare_error = 'FIRSTLINE FAIL native industry funding execute failure'
        result = f.run()
        self.assertEqual(result['status'], 'STOPPED_AT_FIRST_FAILURE')
        self.assertEqual(result['blocker'], f.prepare_error)
        self.assertEqual(f.events[-1], 'first_line prepare')
        self.assertEqual(f.events.count('first_line prepare'), 1)
        self.assertEqual(set(result['save_sha256']), {'diagnostic-map.sav'})
        self.assertTrue(result['resource_catalog_sha256'])
        self.assertTrue(result['funding_receipts'])
        self.assertTrue(f.engine.aborted and f.engine.closed)

    def test_catalog_integrity_failure_stops_before_prepare_and_keeps_premap(self):
        f = self.fixture
        f.catalog['invalid_height_edges'] = 1
        result = f.run()
        self.assertEqual(f.events[-1], 'resource_sites')
        self.assertNotIn('first_line prepare', f.events)
        self.assertEqual(set(result['save_sha256']), {'diagnostic-map.sav'})
        self.assertIn('invalid height edges', result['blocker'])
        self.assertTrue((f.args.output / 'resource-catalog.json').exists())

    def test_pause_and_empty_prestate_are_required_before_any_save(self):
        f = self.fixture
        f.pre['observation']['paused'] = False
        result = f.run()
        self.assertEqual(f.events, ['pause', 'connected_economy basket-status'])
        self.assertEqual(result['save_sha256'], {})
        self.assertIn('pause did not hold', result['blocker'])

    def test_nonempty_prestate_stops_before_any_mutation(self):
        f = self.fixture
        f.pre['observation']['custody']['trains'][3] = 1
        result = f.run()
        self.assertEqual(f.events[-1], 'connected_economy basket-status')
        self.assertIn('trains custody is not empty', result['blocker'])

    def test_prepare_tick_drift_stops_without_prepared_save(self):
        f = self.fixture
        f.opening['tick'] += 1
        result = f.run()
        self.assertEqual(f.events[-1], 'first_line prepare')
        self.assertIn('tick advanced', result['blocker'])
        self.assertEqual(set(result['save_sha256']), {'diagnostic-map.sav'})

    def test_poststate_evaluation_or_cargo_integrity_failure_is_not_success(self):
        f = self.fixture
        f.post['mission']['economy_evaluations'] = 1
        result = f.run()
        self.assertEqual(result['status'], 'STOPPED_AT_FIRST_FAILURE')
        self.assertEqual(f.events[-1], 'first_line status')
        self.assertTrue(f.engine.aborted)

    def test_nonempty_prepared_baseline_stops_without_prepared_save(self):
        f = self.fixture
        f.opening['custody']['industry_outputs'][4] = 1
        result = f.run()
        self.assertEqual(f.events[-1], 'first_line prepare')
        self.assertIn('industry_outputs custody is not empty', result['blocker'])

    def test_startup_consumes_same_absolute_budget_and_expiry_prevents_commands(self):
        f = self.fixture
        f.hook = lambda stage, engine: f.clock.advance(301) if stage == 'startup' else None
        result = f.run()
        self.assertEqual(f.events, [])
        self.assertEqual(len(f.factory_calls), 1)
        self.assertEqual(f.factory_calls[0][4]['deadline'], 400)
        self.assertIn('deadline exhausted', result['blocker'])
        self.assertTrue(f.engine.aborted and f.engine.closed)

    def test_budget_is_not_reset_after_preparation(self):
        f = self.fixture
        def delay(stage, engine):
            if stage == 'startup':
                f.clock.advance(200)
            elif stage == 'first_line prepare':
                f.clock.advance(101)
        f.hook = delay
        result = f.run()
        self.assertEqual(f.events[-1], 'first_line prepare')
        self.assertEqual(result['elapsed_seconds'], 301)
        self.assertIn('preparation response', result['blocker'])
        self.assertEqual(set(result['save_sha256']), {'diagnostic-map.sav'})

    def test_exact_deadline_prevents_dispatch(self):
        f = self.fixture
        f.hook = lambda stage, engine: f.clock.advance(300) if stage == 'startup' else None
        result = f.run()
        self.assertEqual(f.events, [])
        self.assertIn('pause dispatch', result['blocker'])

    def test_save_expiry_prevents_catalog_or_prepare(self):
        f = self.fixture
        f.hook = lambda stage, engine: f.clock.advance(300) if stage == 'save diagnostic-map.sav' else None
        result = f.run()
        self.assertEqual(f.events[-1], 'save diagnostic-map.sav')
        self.assertIn('preparation map saved', result['blocker'])
        self.assertTrue((f.args.output / 'diagnostic-map.sav').exists())

    def test_freeze_time_is_included_and_expiry_does_not_start_engine(self):
        f = self.fixture
        original = diagnostic._freeze
        def delayed(*args):
            value = original(*args)
            f.clock.advance(300)
            return value
        with patch.object(diagnostic, '_freeze', side_effect=delayed):
            result = f.run()
        self.assertEqual(f.factory_calls, [])
        self.assertEqual(result['status'], 'STOPPED_AT_FIRST_FAILURE')
        self.assertTrue(Path(result['attempt_lock']).exists())

    def test_startup_exception_is_recorded_without_retry_or_proof(self):
        f = self.fixture
        def fail(*args, **kwargs):
            f.factory_calls.append((args, kwargs))
            raise RuntimeError('native startup failed')
        f.factory = fail
        result = f.run()
        self.assertEqual(result['blocker'], 'native startup failed')
        self.assertEqual(len(f.factory_calls), 1)
        self.assertEqual(f.events, [])
        self.assertEqual(result['native_processes'], [])

    def test_existing_shared_attempt_lock_is_never_replaced(self):
        f = self.fixture
        f.args.attempt_id = 'owner-old'
        with self.assertRaises(FileExistsError):
            f.run()
        self.assertEqual(f.factory_calls, [])
        self.assertEqual(sha(f.old_lock), f.old_hash)

    def test_existing_output_is_never_replaced(self):
        f = self.fixture
        f.args.output.mkdir()
        marker = f.args.output / 'retained'
        marker.write_bytes(b'original evidence')
        with self.assertRaises(FileExistsError):
            f.run()
        self.assertEqual(marker.read_bytes(), b'original evidence')
        self.assertEqual(f.factory_calls, [])

    def test_explicit_id_and_output_required_before_creating_artifacts(self):
        f = self.fixture
        for bad_id in (None, '', '../foreign', 'UPPER', 'x' * 49):
            f.args.attempt_id = bad_id
            with self.subTest(attempt_id=bad_id), self.assertRaises(RuntimeError):
                f.run()
        f.args.attempt_id = 'offline-test'
        f.args.output = None
        with self.assertRaises(RuntimeError):
            f.run()
        self.assertEqual(f.factory_calls, [])


class DispatchTests(unittest.TestCase):
    def test_diagnostic_cli_never_enters_original_proof_runner(self):
        with patch.object(runner, 'run') as proof, \
                patch.object(diagnostic, 'run_diagnostic', return_value={'diagnostic_only': True}) as single:
            result = runner.main(['--diagnostic-only', '--attempt-id', 'offline-dispatch', '--output', '/tmp/mock'])
        proof.assert_not_called()
        single.assert_called_once()
        self.assertTrue(result['diagnostic_only'])

    def test_default_cli_keeps_original_runner(self):
        with patch.object(runner, 'run', return_value='original') as proof, \
                patch.object(diagnostic, 'run_diagnostic') as single:
            self.assertEqual(runner.main([]), 'original')
        proof.assert_called_once()
        single.assert_not_called()


class FakeThread:
    def __init__(self, target, daemon):
        self.target = target
        self.started = False
        self.joins = []

    def start(self):
        self.started = True

    def join(self, timeout):
        self.joins.append(timeout)


class WatchdogTests(unittest.TestCase):
    def make_engine(self, clock, native_init):
        with patch.object(diagnostic.threading, 'Thread', FakeThread), \
                patch.object(diagnostic.time, 'monotonic', clock), \
                patch.object(diagnostic.MissionEngine, '__init__', native_init):
            return diagnostic.DiagnosticEngine(None, None, None, 'mock', deadline=400)

    def test_watchdog_expiry_kills_only_the_owned_process(self):
        clock = Clock()
        owned = Process()
        foreign = Process(pid=98765)
        def init(engine, *args, **kwargs):
            engine.process = owned
        engine = self.make_engine(clock, init)
        class ExpiredEvent:
            def wait(self, timeout):
                clock.advance(timeout)
                return False

            def is_set(self):
                return False
        engine._watchdog_stop = ExpiredEvent()
        with patch.object(diagnostic.time, 'monotonic', clock):
            engine._watch()
        self.assertEqual(clock(), 400)
        self.assertEqual(owned.kills, 1)
        self.assertEqual(foreign.kills, 0)

    def test_startup_failure_aborts_and_closes_partial_resources(self):
        clock = Clock()
        owned = Process()
        streams = [io.StringIO() for _ in range(3)]
        def init(engine, *args, **kwargs):
            engine.process = owned
            engine.commands, engine.stdout_log, engine.log = streams
            raise RuntimeError('mock startup failure')
        with self.assertRaisesRegex(RuntimeError, 'mock startup failure'):
            self.make_engine(clock, init)
        self.assertEqual(owned.kills, 1)
        self.assertTrue(all(stream.closed for stream in streams))
        self.assertTrue(all(wait is not None and wait <= 1 for wait in owned.waits))

    def test_cleanup_wait_is_bounded_and_cancels_watchdog(self):
        clock = Clock()
        owned = Process()
        def init(engine, *args, **kwargs):
            engine.process = owned
        engine = self.make_engine(clock, init)
        with patch.object(diagnostic.time, 'monotonic', clock):
            engine.close()
        self.assertEqual(owned.kills, 1)
        self.assertEqual(owned.waits, [1, 0.1])
        self.assertTrue(engine._watchdog_stop.is_set())
        self.assertEqual(engine._watchdog.joins, [0.1])

    def test_expired_cleanup_does_not_send_another_native_command(self):
        clock = Clock()
        owned = Process()
        def init(engine, *args, **kwargs):
            engine.process = owned
        engine = self.make_engine(clock, init)
        # Record what is sent even after close, without involving any native IO.
        writes = []
        original_write = owned.stdin.write
        owned.stdin.write = lambda value: (writes.append(value), original_write(value))[1]
        clock.advance(300)
        with patch.object(diagnostic.time, 'monotonic', clock):
            engine.close()
        self.assertEqual(writes, [])
        self.assertEqual(owned.kills, 1)
        self.assertEqual(owned.waits, [0, 0.1])


if __name__ == '__main__':
    unittest.main()
