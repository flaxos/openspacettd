#!/usr/bin/env python3
"""Offline unit tests for verify_commonwealth_works validation logic.

These guards import the runner's own checks and execute without launching an
engine process. They prove the runner deterministically rejects:
- Duplicated or out-of-order monthly evaluations
- Commissioning cash or kit debit mismatches, including extra cargo debits
- A crusher SILC buffer above its enforced input cap
- A works kit that is already present instead of hauled
"""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from verify_commonwealth_works import (CRUSHER_WORLD, EvaluationCounter, check_commission_debit, crusher_buffer,  # noqa: E402
                                       kit_cargos, kit_ready, world_stock)

STEL, BALL, SILC, IRON = 18, 30, 16, 17


def status(money=100000000, stel=160, ball=80, silc=0, input_cap=300, work=0):
    """Minimal status snapshot in the native `connected_economy status` shape."""
    return {
        "money": money,
        "stocks": [{"world": CRUSHER_WORLD, "inventory": [[STEL, stel], [BALL, ball]]}, {"world": 0, "inventory": [[STEL, 5]]}],
        "facilities": [
            {"id": 5, "recipe": 101, "batches": 0, "inputs": [[SILC, silc]], "outputs": [[BALL, 0]]},
            {"id": 6, "recipe": 102, "batches": 0, "inputs": [[IRON, 0]], "outputs": [[STEL, 0]]},
        ],
        "economy": {"version": 2, "factories": [[4, 0, 100, 0, [], 0, work, input_cap, 300], [5, 0, 100, 0, [], 0, 0, 300, 300]]},
    }


class TestVerifyRunnerGuards(unittest.TestCase):
    def setUp(self):
        self.cargos = kit_cargos(status())

    def test_cargo_slots_come_from_recipes(self):
        self.assertEqual(self.cargos, {"STEL": STEL, "BALL": BALL, "SILC": SILC})

    def test_world_stock_is_per_world(self):
        self.assertEqual(world_stock(status(), CRUSHER_WORLD), {STEL: 160, BALL: 80})
        self.assertEqual(world_stock(status(), 0), {STEL: 5})

    def test_kit_ready_needs_both_cargos(self):
        self.assertTrue(kit_ready(status(), self.cargos))
        self.assertFalse(kit_ready(status(stel=159), self.cargos))
        self.assertFalse(kit_ready(status(ball=79), self.cargos))

    def test_valid_commission_debit_passes(self):
        check_commission_debit(status(stel=200, ball=90), status(money=99950000, stel=40, ball=10), self.cargos)

    def test_reject_cash_debit_mismatch(self):
        with self.assertRaisesRegex(RuntimeError, "commission debit"):
            check_commission_debit(status(), status(money=99990000, stel=0, ball=0), self.cargos)

    def test_reject_kit_debit_mismatch(self):
        with self.assertRaisesRegex(RuntimeError, f"cargo {BALL} debit 40"):
            check_commission_debit(status(), status(money=99950000, stel=0, ball=40), self.cargos)

    def test_reject_extra_cargo_debit(self):
        before = status()
        before["stocks"][0]["inventory"].append([SILC, 10])
        with self.assertRaisesRegex(RuntimeError, f"cargo {SILC} debit 10"):
            check_commission_debit(before, status(money=99950000, stel=0, ball=0), self.cargos)

    def test_crusher_buffer_reports_reserve_cap(self):
        buffer = crusher_buffer(status(silc=900, input_cap=600, work=2), self.cargos)
        self.assertEqual(buffer, {"silc_waiting": 900, "silc_cap": 1200, "work": 2, "batches": 0})

    def test_reject_buffer_above_cap(self):
        with self.assertRaisesRegex(RuntimeError, "above its 600 input cap"):
            crusher_buffer(status(silc=601), self.cargos)

    def test_evaluations_count_and_stay_monotonic(self):
        counter = EvaluationCounter()
        self.assertEqual(counter.add([[100, 1, 20, 40]]), 1)
        self.assertEqual(counter.add([]), 0)
        self.assertEqual(counter.add([[131, 1, 20, 40]]), 1)
        self.assertEqual(counter.total, 2)

    def test_reject_duplicate_evaluation(self):
        counter = EvaluationCounter()
        counter.add([[100, 1, 20, 40]])
        with self.assertRaisesRegex(RuntimeError, "not after 100"):
            counter.add([[100, 1, 20, 40]])

    def test_reload_counter_continues_from_last_date(self):
        with self.assertRaisesRegex(RuntimeError, "not after 200"):
            EvaluationCounter(200).add([[150, 1, 20, 40]])

    def test_status_shape_is_not_mutated(self):
        before = status()
        snapshot = copy.deepcopy(before)
        crusher_buffer(before, self.cargos)
        world_stock(before, CRUSHER_WORLD)
        self.assertEqual(before, snapshot)


if __name__ == "__main__":
    unittest.main()
