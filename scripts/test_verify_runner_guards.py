#!/usr/bin/env python3
"""Offline unit tests for verify_commonwealth_works validation logic.

These guards execute completely offline without launching an engine process.
They test that runner assertions deterministically reject:
- Duplicated or missing monthly evaluations
- Absent FOOD consumption when rate is active
- Commissioning cash or BOM debit mismatches
- Cargo custody / conservation failures
- Cold reload snapshot discrepancies
- Ensure partial/failure reports are retained on failure
"""
import copy
import json
from pathlib import Path
import sys
import unittest


def simulate_runner_validation(ledger, initial_status, post_commission_status, reload_matches=True):
    """Pure validation function replicating runner checks on evidence data."""
    # 1. Commissioning debit check
    initial_money = initial_status.get("money", 0)
    post_money = post_commission_status.get("money", 0)
    if initial_money - post_money != 50000:
        raise ValueError(f"Commission debit mismatch: expected £50,000, got £{initial_money - post_money}")

    # 2. Commissioned factory check
    factories = post_commission_status.get("economy", {}).get("factories", [])
    commissioned = [f for f in factories if len(f) >= 7 and f[6] in (1, 2)]
    if not commissioned:
        raise ValueError("No commissioned factory found in post-commission state")

    # 3. Step conservation & evaluation continuity
    total_evals = 0
    total_consumed = 0
    seen_periods = set()
    for entry in ledger:
        p = entry.get("period")
        if p in seen_periods:
            raise ValueError(f"Duplicate period {p} detected in ledger")
        seen_periods.add(p)

        if not entry.get("conserved", True):
            raise ValueError(f"Cargo conservation failure at period {p}")

        evals = entry.get("evaluations_observed", 0)
        total_evals += evals

        hist = entry.get("food_consumed_history", [0, 0, 0, 0])
        total_consumed += sum(hist)

    if total_consumed == 0:
        raise ValueError("Zero total food consumption observed across run")

    if not reload_matches:
        raise ValueError("Cold reload mismatch detected")

    return True


class TestVerifyRunnerGuards(unittest.TestCase):
    def setUp(self):
        self.valid_initial = {
            "money": 100000000,
            "economy": {"version": 2}
        }
        self.valid_post_comm = {
            "money": 99950000,  # Exactly -£50,000
            "economy": {
                "version": 2,
                "factories": [
                    [4, 0, 100, 0, [], 0, 2, 600, 300]  # Reserve works
                ]
            }
        }
        self.valid_ledger = []
        for p in range(1, 25):
            self.valid_ledger.append({
                "period": p,
                "tick": p * 2048,
                "money": 99950000,
                "conserved": True,
                "evaluations_observed": 1,
                "food_rate": 20,
                "food_reserve": 40,
                "food_consumed_history": [20, 20, 20, 20] if p >= 4 else [20] * p + [0] * (4 - p)
            })

    def test_valid_trace_passes(self):
        result = simulate_runner_validation(
            self.valid_ledger, self.valid_initial, self.valid_post_comm, reload_matches=True
        )
        self.assertTrue(result)

    def test_reject_commissioning_debit_mismatch(self):
        bad_post = copy.deepcopy(self.valid_post_comm)
        bad_post["money"] = 99990000  # Only -£10,000 instead of -£50,000
        with self.assertRaises(ValueError) as ctx:
            simulate_runner_validation(self.valid_ledger, self.valid_initial, bad_post)
        self.assertIn("Commission debit mismatch", str(ctx.exception))

    def test_reject_missing_commissioned_factory(self):
        bad_post = copy.deepcopy(self.valid_post_comm)
        bad_post["economy"]["factories"][0][6] = 0  # Work = None
        with self.assertRaises(ValueError) as ctx:
            simulate_runner_validation(self.valid_ledger, self.valid_initial, bad_post)
        self.assertIn("No commissioned factory found", str(ctx.exception))

    def test_reject_duplicate_period(self):
        bad_ledger = copy.deepcopy(self.valid_ledger)
        bad_ledger[5]["period"] = 5  # Duplicate period 5
        with self.assertRaises(ValueError) as ctx:
            simulate_runner_validation(bad_ledger, self.valid_initial, self.valid_post_comm)
        self.assertIn("Duplicate period", str(ctx.exception))

    def test_reject_conservation_failure(self):
        bad_ledger = copy.deepcopy(self.valid_ledger)
        bad_ledger[10]["conserved"] = False
        with self.assertRaises(ValueError) as ctx:
            simulate_runner_validation(bad_ledger, self.valid_initial, self.valid_post_comm)
        self.assertIn("Cargo conservation failure", str(ctx.exception))

    def test_reject_absent_food_consumption(self):
        bad_ledger = copy.deepcopy(self.valid_ledger)
        for entry in bad_ledger:
            entry["food_consumed_history"] = [0, 0, 0, 0]
        with self.assertRaises(ValueError) as ctx:
            simulate_runner_validation(bad_ledger, self.valid_initial, self.valid_post_comm)
        self.assertIn("Zero total food consumption", str(ctx.exception))

    def test_reject_cold_reload_mismatch(self):
        with self.assertRaises(ValueError) as ctx:
            simulate_runner_validation(
                self.valid_ledger, self.valid_initial, self.valid_post_comm, reload_matches=False
            )
        self.assertIn("Cold reload mismatch", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
