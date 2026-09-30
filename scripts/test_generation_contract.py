#!/usr/bin/env python3
"""Regression for bounded generation evidence and retained freight snapshot separation."""
import unittest
import json
import tempfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from test_integrated_economy import first_freight, freight_advance_limit, generation_contract, generation_projection, validate_generation_paid


class GenerationEvidenceTests(unittest.TestCase):
    def test_freight_limit_cannot_expand_approved_tick_budget(self):
        self.assertEqual(freight_advance_limit(500), 240)
        self.assertEqual(freight_advance_limit(240), 240)
        self.assertEqual(freight_advance_limit(17), 17)
        for value in (0, -1):
            with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, 'positive'):
                freight_advance_limit(value)

    def test_cold_projection_keeps_rng_towns_and_native_assets(self):
        saved = {'rng': [11, 101], 'towns': [{'id': 0, 'houses': [77]}], 'rail': [[55, 16, 2]],
                 'town_generation': {'probes': 9, 'probe_hash': 123}}
        projection = generation_projection(saved)
        self.assertEqual(projection, {'rng': [11, 101], 'towns': [{'id': 0, 'houses': [77]}], 'rail': [[55, 16, 2]]})
        self.assertIn('town_generation', saved)
        self.assertNotEqual(projection, {**projection, 'rng': [11, 102]})
        self.assertNotEqual(projection, {**projection, 'towns': [{'id': 0, 'houses': []}]})

    def test_paid_evidence_rejects_free_or_unmatched_cost(self):
        initial = {'money': 100000, 'loan': 100000}
        for cost in (0, 5):
            paid = {'joins': [{'paid': cost, 'quote': 10}], 'station': {'paid': 10, 'quote': 10},
                    'state': {'money': 99980, 'loan': 100000}}
            with self.subTest(cost=cost), self.assertRaisesRegex(RuntimeError, 'cost differs or was free'):
                validate_generation_paid(initial, {'total_quote': 20}, paid)

    def test_paid_evidence_rejects_borrowing_or_unpaid_construction(self):
        initial = {'money': 100000, 'loan': 100000}
        for money, loan in ((100000, 100000), (99980, 110000)):
            paid = {'joins': [{'paid': 10, 'quote': 10}], 'station': {'paid': 10, 'quote': 10},
                    'state': {'money': money, 'loan': loan}}
            with self.subTest(money=money, loan=loan), self.assertRaisesRegex(RuntimeError, 'cash/debt identity'):
                validate_generation_paid(initial, {'total_quote': 20}, paid)

    def test_native_failure_stops_and_retains_remaining_cases_not_run(self):
        # The subprocess launch fails before any native mutation. Both modes must
        # retain that failure and never launch the next authorized seed/repetition.
        for runner in (generation_contract, first_freight):
            with self.subTest(mode=runner.__name__), tempfile.TemporaryDirectory() as folder:
                output = Path(folder) / 'evidence'
                args = SimpleNamespace(output=output, binary=Path('/unused/openttd'), seeds=[11, 101, 2026], steps=500)
                with patch('test_integrated_economy.acceptance_inputs', return_value={}), \
                     patch('test_integrated_economy.ordinary_config', return_value='ordinary-config'), \
                     patch('test_integrated_economy.Engine', side_effect=RuntimeError('native generation failed')) as launch:
                    with self.assertRaisesRegex(RuntimeError, 'stopped at seed 11'):
                        runner(args)
                self.assertEqual(launch.call_count, 1)
                report = json.loads((output / 'evidence.json').read_text())
                self.assertFalse(report['passed'])
                self.assertEqual(report['seeds'][0]['status'], 'failed')
                self.assertTrue(all(row['status'] == 'not_run' and row['passed'] is None for row in report['seeds'][1:]))
                if runner is generation_contract:
                    self.assertEqual(report['seeds'][0]['repetitions'][1]['status'], 'not_run')


if __name__ == '__main__':
    unittest.main()
