#!/usr/bin/env python3
"""Offline Vale runner regressions: fake receipts, never a native game process."""
import copy
import json
import os
from pathlib import Path
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import vale_railway as vale
from test_wp11_slice import OfflineEngine


class ValeStartupTests(unittest.TestCase):
    """Exercise the real helper constructors with a fake process, no game."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='vale-startup-')
        self.output = Path(self.temp.name)
        self.process = SimpleNamespace(stdin=None, pid=123456, poll=lambda: 0)
        self.real_open = os.open

    def tearDown(self):
        self.temp.cleanup()

    def launch(self, engine_class, **options):
        def fake_open(path, flags, *args):
            if Path(path) == self.output / 'startup.fifo':
                return self.real_open(self.output / 'fake-sink', os.O_WRONLY | os.O_CREAT, 0o600)
            return self.real_open(path, flags, *args)
        with patch('subprocess.Popen', return_value=self.process) as popen, \
                patch.object(OfflineEngine, 'wait', return_value=''), \
                patch.object(vale.ValeEngine, 'wait_marker', return_value=''), \
                patch.object(OfflineEngine, 'read_console', return_value=None), patch('os.open', side_effect=fake_open):
            engine = engine_class(Path('/fake/openttd'), Path('/fake/config'), self.output, 'startup', **options)
            engine.close()
            return popen.call_args

    def test_old_default_keeps_seven_world_bridge_script(self):
        invocation = self.launch(OfflineEngine)
        self.assertEqual((self.output / 'scripts/game_start.scr').read_text(),
                         f'script "{self.output}/startup-native.log"\necho FUNCTIONAL_OFFLINE_START\nscript\n'
                         f'connected_economy functional-bridge "{self.output}/startup.fifo"\n')
        self.assertEqual(invocation.kwargs['env']['OPENSPACETTD_WORLD_COUNT'], '7')
        self.assertIn('-g', invocation.args[0])
        self.assertNotIn('-X', invocation.args[0])

    def test_vale_editor_selects_guarded_bridge_and_flushes_admission(self):
        invocation = self.launch(vale.ValeEngine, deadline=time.monotonic() + 10, editor=True, world_count=1)
        script = (self.output / 'scripts/editor_start.scr').read_text()
        self.assertIn(f'script "{self.output}/startup-native.log"\nvale bridge "{self.output}/startup.fifo"\nscript\n', script)
        self.assertNotIn('connected_economy', script)
        self.assertIn('-e', invocation.args[0])
        self.assertIn('-X', invocation.args[0])
        self.assertEqual(invocation.kwargs['env']['OPENSPACETTD_WORLD_COUNT'], '1')

    def test_vale_loaded_save_selects_same_bridge_without_editor(self):
        invocation = self.launch(vale.ValeEngine, deadline=time.monotonic() + 10, save=Path('/fake/vale.sav'), world_count=1)
        self.assertIn('vale bridge', (self.output / 'scripts/game_start.scr').read_text())
        self.assertIn('-g', invocation.args[0])
        self.assertNotIn('-e', invocation.args[0])
        self.assertIn('-X', invocation.args[0])
        self.assertEqual(invocation.args[0][-1], '/fake/vale.sav')
        self.assertEqual(invocation.args[0][invocation.args[0].index('-g') + 1], '/fake/vale.sav')

    def test_selector_cannot_inject_commands_or_override_vale(self):
        with patch('subprocess.Popen', side_effect=AssertionError('Popen forbidden')):
            with self.assertRaisesRegex(RuntimeError, 'Unsupported offline startup bridge'):
                OfflineEngine(Path('/fake/openttd'), Path('/fake/config'), self.output, 'startup', startup_bridge='vale bridge\nquit')
            with self.assertRaisesRegex(RuntimeError, 'bridge is fixed'):
                vale.ValeEngine(Path('/fake/openttd'), Path('/fake/config'), self.output, 'startup', startup_bridge='connected_economy functional-bridge')
        self.assertEqual(list(self.output.iterdir()), [])

    def test_owner_copy_is_paused_by_native_command_before_admission(self):
        self.launch(OfflineEngine, save=Path('/fake/vale.sav'), world_count=1,
                    startup_bridge='vale bridge', pause_loaded_save=True)
        self.assertIn('pause\nvale bridge ', (self.output / 'scripts/game_start.scr').read_text())

    def test_pause_option_rejects_other_contexts_before_process_creation(self):
        with patch('subprocess.Popen', side_effect=AssertionError('Popen forbidden')):
            with self.assertRaisesRegex(RuntimeError, 'restricted'):
                OfflineEngine(Path('/fake/openttd'), Path('/fake/config'), self.output, 'startup', pause_loaded_save=True)
        self.assertEqual(list(self.output.iterdir()), [])

    def rejected_startup(self, late=False):
        def fake_start(*args, **kwargs):
            (self.output / 'startup-native.log').write_text('FUNCTIONAL_OFFLINE_START\n' + ('' if late else 'VALE FAIL wrong startup context\n'))
            if late:
                kwargs['stdout'].write('dbg: [misc:0] VALE FAIL startup bridge admission changed before exec\n')
                kwargs['stdout'].flush()
            return self.process
        def reject_fifo(path, flags, *args):
            if Path(path) == self.output / 'startup.fifo':
                raise AssertionError('Rejected startup must not open FIFO')
            return self.real_open(path, flags, *args)
        with patch('subprocess.Popen', side_effect=fake_start), patch.object(vale.ValeEngine, 'wait_marker', return_value=''), \
                patch.object(OfflineEngine, 'read_console', return_value=None), patch('os.open', side_effect=reject_fifo):
            with self.assertRaisesRegex(RuntimeError, 'VALE FAIL .*startup'):
                vale.ValeEngine(Path('/fake/openttd'), Path('/fake/config'), self.output, 'startup', deadline=time.monotonic() + 10)

    def test_flushed_native_rejection_stops_before_fifo_open(self):
        self.rejected_startup()
        self.assertIn('VALE FAIL wrong startup context', (self.output / 'startup-native.log').read_text())

    def test_deferred_native_rejection_in_owned_stdout_stops_before_fifo_open(self):
        self.rejected_startup(late=True)
        self.assertIn('VALE FAIL startup bridge admission changed before exec', (self.output / 'startup-stdout.log').read_text())


def state():
    zeros = [0] * 64
    custody = {key: zeros.copy() for key in ('industry_inputs', 'industry_outputs', 'stations', 'trains', 'cities', 'research', 'stocks', 'total')}
    custody['stocks'][2] = custody['total'][2] = 80
    return {'paused': True, 'pause_mode': 129, 'money': 750000, 'tick': 0,
            'mission': {'active': True, 'unavailable': False, 'reason': '', 'awarded': False, 'economy_evaluations': 0,
                        'accepted_silc': 0, 'qualified_batches': 0, 'stored_ball': 0, 'input_goal': False, 'production_goal': False,
                        'storage_goal': False, 'binding': [0, 1, 3], 'journeys': []},
            'worlds': [{'id': 0, 'phase': 3, 'economic_role': 0}],
            'towns': [{'population': n} for n in (1200, 350, 240)], 'industries': [{'id': 0}, {'id': 1}],
            'stations': [0, 1, 2, 3], 'trains': [{'id': n, 'cargo': 0, 'front': n == 0, 'stopped': True, 'orders': [], 'crashed': False, 'lost': False} for n in range(3)],
            'custody': custody, 'goals': [[i, 0, f'Goal {i}', False] for i in range(3)],
            'story_pages': [[0, 0, 'The Vale Railway']], 'story_elements': [], 'quarry_source_station': 0}


class FakeEngine:
    """The contract test double does not implement economy or movement."""
    instances = []
    fail_first = False
    unclosed = False
    delayed_empty = False

    def __init__(self, binary, config, output, name, **options):
        self.options = options
        self.commands = []
        self.snapshot = state()
        self.output = output
        self.closed = False
        self.process = SimpleNamespace(pid=987654, poll=lambda: None if self.unclosed else 0)
        self.instances.append(self)

    def command(self, text, marker):
        self.commands.append(text)
        if text == 'vale prepare':
            return json.dumps({'baseline': self.snapshot})
        if text == 'vale audit':
            return json.dumps({name: {'language': iso, 'invalid_slopes': 0, 'invalid_foundations': 0,
                                     'pixel_queries': 1, 'viewport_queries': 1, 'cargos': []}
                               for name, iso in (('english.lng', 'en_GB'), ('english_US.lng', 'en_US'))})
        if text == 'vale build':
            return json.dumps({'feeder': 0, 'export': 3})
        if text == 'vale advance':
            self.snapshot['mission']['economy_evaluations'] += 1
            if self.delayed_empty:
                self.snapshot['mission']['input_goal'] = True
                self.snapshot['industries'][1]['inputs'] = [[0, 200 if self.snapshot['mission']['economy_evaluations'] < 4 else 0]]
            value = copy.deepcopy(self.snapshot)
            value.update(evaluations_advanced=1, cash_conserved=True, cargo_errors=[], train_integrity_failure=self.fail_first,
                         audit={'gate_tolls': {}, 'payments': [], 'cash_payments': [], 'arrivals': []})
            return json.dumps(value)
        if text.startswith('vale stop ') or text.startswith('vale restart '):
            return json.dumps(self.snapshot)
        raise AssertionError(f'Unexpected test command {text}')

    def state(self):
        self.commands.append('vale status')
        return copy.deepcopy(self.snapshot)

    def save(self, path):
        self.commands.append('save')
        path.write_bytes(b'offline test save receipt, not a native save')

    def abort(self):
        self.commands.append('abort')

    def close(self):
        self.closed = True


class ValeRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='vale-offline-')
        self.root = Path(self.temp.name)
        (self.root / 'build').mkdir()
        (self.root / 'demo/vale_railway').mkdir(parents=True)
        (self.root / 'demo/vale_railway/vale.cfg').write_text('test config')
        self.runtime = self.root / 'runtime'
        self.runtime.mkdir()
        self.binary = self.runtime / 'openttd'
        self.binary.write_bytes(b'test executable receipt')
        self.binary.chmod(0o755)
        graphics = self.runtime / 'baseset/opengfx'
        graphics.mkdir(parents=True)
        (graphics / 'opengfx.obg').write_text('[metadata]\nname = OpenGFX\n[files]\n' + ''.join(f'{name} = {name}.grf\n' for name in ('base', 'arctic', 'tropical', 'toyland', 'logos', 'extra')))
        for name in ('base', 'arctic', 'tropical', 'toyland', 'logos', 'extra'):
            (graphics / f'{name}.grf').write_bytes(b'fake frozen graphics receipt, never loaded by game')
        self.manifest = {'source_commit': 'a' * 40, 'binary': str(self.binary), 'binary_sha256': vale.digest(self.binary),
                         'files': {str(p.relative_to(self.runtime)): vale.digest(p) for p in graphics.iterdir()}, 'source_sha256': {}}
        self.manifest_path = self.root / 'manifest.json'
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.args = SimpleNamespace(phase='prepare', reviewed_source='a' * 40, runtime_manifest=self.manifest_path,
                                    runtime_sha256=vale.digest(self.manifest_path), attempt_id='mock-one', output=self.root / 'build/mock-output',
                                    save=None, expected_state=None, save_sha256=None, state_sha256=None)
        self.root_patch = patch.object(vale, 'ROOT', self.root)
        self.root_patch.start()
        self.popen = patch('subprocess.Popen', side_effect=AssertionError('Native Popen forbidden in offline tests'))
        self.popen.start()
        FakeEngine.instances = []
        FakeEngine.fail_first = FakeEngine.unclosed = FakeEngine.delayed_empty = False

    def tearDown(self):
        self.popen.stop()
        self.root_patch.stop()
        self.temp.cleanup()

    def run_fake(self):
        with patch.object(vale, 'preflight', return_value=(self.manifest, self.binary)):
            return vale.run(self.args, engine_factory=FakeEngine, clock=lambda: 100)

    def test_prepare_is_paused_zero_evaluation_and_closed(self):
        report = self.run_fake()
        engine = FakeEngine.instances[0]
        self.assertTrue(report['passed'])
        self.assertTrue(report['process_exited'])
        self.assertTrue(engine.closed)
        self.assertNotIn('vale advance', engine.commands)
        self.assertTrue(engine.options['editor'])
        self.assertEqual(engine.options['world_count'], 1)
        self.assertEqual(engine.options['deadline'], 400)

    def routes(self):
        parent = self.root / 'previous'
        parent.mkdir()
        self.args.save = parent / 'opening.sav'
        self.args.save.write_bytes(b'test input')
        self.args.expected_state = parent / 'expected-state.json'
        self.args.expected_state.write_text(json.dumps(state()))
        self.args.save_sha256 = vale.digest(self.args.save)
        self.args.state_sha256 = vale.digest(self.args.expected_state)
        self.args.phase = 'routes'

    def test_route_cap_is_twelve_calls_and_does_not_save_failed_result(self):
        self.routes()
        report = self.run_fake()
        engine = FakeEngine.instances[0]
        self.assertFalse(report['passed'])
        self.assertEqual(engine.commands.count('vale advance'), 12)
        self.assertNotIn('save', engine.commands)
        self.assertTrue(report['process_exited'])
        self.assertEqual(len(report['evaluations']), 12)

    def test_first_integrity_failure_is_retained_and_no_later_query_runs(self):
        self.routes()
        FakeEngine.fail_first = True
        report = self.run_fake()
        engine = FakeEngine.instances[0]
        self.assertFalse(report['passed'])
        self.assertEqual(engine.commands[-2:], ['vale advance', 'abort'])
        self.assertEqual(len(report['evaluations']), 1)
        self.assertTrue(report['evaluations'][0]['train_integrity_failure'])

    def test_restart_waits_for_real_empty_input_with_same_total_cap(self):
        self.routes()
        FakeEngine.delayed_empty = True
        report = self.run_fake()
        commands = FakeEngine.instances[0].commands
        restart = commands.index('vale restart 0')
        self.assertEqual(commands[:restart].count('vale advance'), 4)
        self.assertEqual(commands.count('vale advance'), 12)
        self.assertFalse(report['passed'])

    def test_useful_input_visits_count_fragments_once(self):
        samples = [{'audit': {'arrivals': [[10, 0, 1], [30, 0, 1]],
                             'payments': [[11, 0, 1, 4, 20, 100], [12, 0, 1, 4, 10, 50], [31, 0, 1, 4, 20, 100], [32, 1, 1, 4, 20, 100]]}}]
        self.assertEqual(vale.useful_input_visits(samples, 0, 1, 4), 2)

    def test_useful_receipt_without_native_arrival_is_rejected(self):
        samples = [{'audit': {'arrivals': [], 'payments': [[11, 0, 1, 4, 20, 100]]}}]
        with self.assertRaisesRegex(RuntimeError, 'lacks an arrival'):
            vale.useful_input_visits(samples, 0, 1, 4)

    def test_unconfirmed_exit_never_reports_success(self):
        FakeEngine.unclosed = True
        report = self.run_fake()
        self.assertFalse(report['passed'])
        self.assertFalse(report['process_exited'])
        self.assertIn('closure failed', report['blocker'])

    def test_cold_comparison_ignores_only_command_during_pause(self):
        a = state()
        b = copy.deepcopy(a)
        b['pause_mode'] = 1
        self.assertEqual(vale.load_comparable(a), vale.load_comparable(b))
        b['money'] -= 1
        self.assertNotEqual(vale.load_comparable(a), vale.load_comparable(b))
        b['pause_mode'] = 3
        with self.assertRaises(RuntimeError):
            vale.load_comparable(b)

    def admit(self):
        with patch.object(vale.subprocess, 'check_output', side_effect=['a' * 40 + '\n', '']), patch.object(vale.shutil, 'disk_usage', return_value=SimpleNamespace(free=2 * 1024 ** 3)):
            return vale.preflight(self.args, 400, clock=lambda: 100)

    def test_exact_clean_inventory_is_admitted_without_process(self):
        _, binary = self.admit()
        self.assertEqual(binary, self.binary)

    def test_missing_frozen_graphics_descriptor_is_rejected(self):
        name = 'baseset/opengfx/opengfx.obg'
        (self.runtime / name).unlink()
        del self.manifest['files'][name]
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.args.runtime_sha256 = vale.digest(self.manifest_path)
        with self.assertRaisesRegex(RuntimeError, 'OpenGFX descriptor missing'):
            self.admit()

    def test_missing_frozen_graphics_reference_is_rejected(self):
        name = 'baseset/opengfx/extra.grf'
        (self.runtime / name).unlink()
        del self.manifest['files'][name]
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.args.runtime_sha256 = vale.digest(self.manifest_path)
        with self.assertRaisesRegex(RuntimeError, 'OpenGFX reference missing'):
            self.admit()

    def test_graphics_reference_cannot_escape_frozen_set(self):
        name = 'baseset/opengfx/opengfx.obg'
        p = self.runtime / name
        p.write_text(p.read_text().replace('extra = extra.grf', 'extra = ../ambient.grf'))
        self.manifest['files'][name] = vale.digest(p)
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.args.runtime_sha256 = vale.digest(self.manifest_path)
        with self.assertRaisesRegex(RuntimeError, 'Unsafe OpenGFX reference'):
            self.admit()

    def test_unlisted_or_linked_runtime_content_is_rejected(self):
        extra = self.runtime / 'extra'
        extra.write_bytes(b'extra')
        with self.assertRaisesRegex(RuntimeError, 'inventory differs'):
            self.admit()
        extra.unlink()
        extra.symlink_to(self.binary)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            self.admit()

    def test_output_cannot_be_nested_in_evidence_or_runtime(self):
        self.args.output = self.runtime / 'new-child'
        with self.assertRaisesRegex(RuntimeError, 'direct build sibling'):
            self.admit()

    def test_runtime_symlink_parent_is_rejected_before_resolving(self):
        alias = self.root / 'runtime-alias'
        alias.symlink_to(self.runtime, target_is_directory=True)
        self.manifest['binary'] = str(alias / 'openttd')
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.args.runtime_sha256 = vale.digest(self.manifest_path)
        with self.assertRaisesRegex(RuntimeError, 'contains a symlink'):
            self.admit()

    def test_constructor_failure_retains_started_pid_and_exit(self):
        def failed(binary, config, output, name, **options):
            process = SimpleNamespace(pid=123456, poll=lambda: -9)
            options['process_started'](process)
            raise RuntimeError('retained startup failure')
        with patch.object(vale, 'preflight', return_value=(self.manifest, self.binary)):
            report = vale.run(self.args, engine_factory=failed, clock=lambda: 100)
        self.assertFalse(report['passed'])
        self.assertEqual(report['blocker'], 'retained startup failure')
        self.assertEqual(report['native_processes'], [123456])
        self.assertTrue(report['process_exited'])
        self.assertEqual(report['startup_process_exit_codes'], [-9])

    def test_deadline_rejects_before_process_or_attempt(self):
        with self.assertRaisesRegex(RuntimeError, 'deadline exhausted'):
            vale.preflight(self.args, 100, clock=lambda: 100)
        self.assertFalse(self.args.output.exists())

    def test_input_sha_and_predecessor_exit_are_required(self):
        self.routes()
        with self.assertRaises(FileNotFoundError):
            self.admit()
        (self.args.save.parent / 'evidence.json').write_text(json.dumps({'passed': True, 'process_exited': False}))
        with self.assertRaisesRegex(RuntimeError, 'confirmed closed'):
            self.admit()
        self.args.save.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'save bytes differ'):
            self.admit()


if __name__ == '__main__':
    unittest.main()
