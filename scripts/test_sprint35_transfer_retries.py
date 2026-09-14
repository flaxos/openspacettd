#!/usr/bin/env python3
"""Authority retry protocol tests; these do not prove live engine handoffs."""

import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from universe_authority import AuthorityHandler, PersistenceError, UniverseAuthority, durable_request


class TransferRetryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="openspace_s35_")
        self.addCleanup(self.directory.cleanup)
        self.state_file = str(Path(self.directory.name) / "authority.json")
        self.authority = UniverseAuthority(self.state_file)
        self.payload = {
            "request_id": "world-1-departure-1",
            "source_world": 1,
            "dest_world": 2,
            "consist_id": "train-1",
            "total_cargo": 25,
            "cargo_breakdown": {"0": 25},
            "transit_delay_sec": 0,
            "orders": [{"world_id": 1}, {"world_id": 2}],
        }

    def initiate(self, payload=None):
        ok, result = self.authority.initiate_transfer(payload or self.payload)
        self.assertTrue(ok, result)
        return result["transfer_id"]

    def restart(self):
        self.authority = UniverseAuthority(self.state_file)

    def test_lost_initiation_reply_survives_restart_without_duplicate_cargo(self):
        tx = self.initiate()
        before = copy.deepcopy(self.authority.export_state())
        self.restart()
        self.assertEqual(self.initiate(), tx)
        self.assertEqual(self.authority.export_state(), before)
        self.assertEqual(self.authority.commodity_initiated, {0: 25})

    def test_conflicting_retry_is_rejected_without_mutation(self):
        self.initiate()
        before = copy.deepcopy(self.authority.export_state())
        for field, value in (("dest_world", 3), ("total_cargo", 26),
                             ("consist_id", "train-2")):
            with self.subTest(field=field):
                ok, _ = self.authority.initiate_transfer(dict(self.payload, **{field: value}))
                self.assertFalse(ok)
                self.assertEqual(self.authority.export_state(), before)

    def test_request_identity_is_scoped_to_source_and_departure(self):
        first = self.initiate()
        other_source = self.initiate(dict(self.payload, source_world=3))
        next_trip = self.initiate(dict(self.payload, request_id="world-1-departure-2"))
        self.assertEqual(len({first, other_source, next_trip}), 3)

    def test_departure_retry_does_not_reset_arrival_deadline(self):
        tx = self.initiate(dict(self.payload, transit_delay_sec=10))
        with patch("universe_authority.time.time", return_value=100):
            self.assertTrue(self.authority.depart_transfer({"transfer_id": tx})[0])
        self.restart()
        with patch("universe_authority.time.time", return_value=105):
            self.assertTrue(self.authority.depart_transfer({"transfer_id": tx})[0])
            self.assertFalse(self.authority.claim_transfer({"transfer_id": tx, "dest_world": 2})[0])
        self.assertEqual(self.authority.transfers[tx]["arrival_time"], 110)

    def test_claim_retry_and_obstructed_arrival_remain_discoverable(self):
        tx = self.initiate()
        self.assertTrue(self.authority.depart_transfer({"transfer_id": tx})[0])
        claim = {"transfer_id": tx, "dest_world": 2}
        self.assertTrue(self.authority.claim_transfer(claim)[0])
        self.restart()
        self.assertEqual(self.authority.query_pending(2), [tx])
        self.assertTrue(self.authority.claim_transfer(claim)[0])
        self.assertFalse(self.authority.claim_transfer(dict(claim, dest_world=3))[0])

    def test_completion_receipt_prevents_double_accounting_after_restart(self):
        tx = self.initiate()
        self.authority.depart_transfer({"transfer_id": tx})
        self.authority.claim_transfer({"transfer_id": tx, "dest_world": 2})
        confirm = {"transfer_id": tx, "dest_world": 2, "success": True,
                   "arrival_receipt": "world-2-receipt-1"}
        self.assertFalse(self.authority.confirm_arrival(dict(confirm, arrival_receipt=""))[0])
        self.assertTrue(self.authority.confirm_arrival(confirm)[0])
        before = copy.deepcopy(self.authority.export_state())
        self.restart()
        self.assertTrue(self.authority.confirm_arrival(confirm)[0])
        self.assertTrue(self.authority.depart_transfer({"transfer_id": tx})[0])
        self.assertEqual(self.initiate(), tx)
        self.assertEqual(self.authority.export_state(), before)
        self.assertEqual(self.authority.commodity_completed, {0: 25})
        self.assertEqual(self.authority.transfers[tx]["current_order_index"], 1)
        self.assertEqual(self.authority.query_pending(2), [])
        self.assertFalse(self.authority.confirm_arrival(dict(confirm, arrival_receipt="duplicate"))[0])
        self.assertFalse(self.authority.confirm_arrival(dict(confirm, dest_world=3))[0])

    def test_legacy_transfers_remain_compatible(self):
        payload = dict(self.payload)
        del payload["request_id"]
        tx = self.initiate(payload)
        self.assertTrue(self.authority.depart_transfer({"transfer_id": tx})[0])
        self.assertTrue(self.authority.claim_transfer({"transfer_id": tx, "dest_world": 2})[0])
        self.assertTrue(self.authority.confirm_arrival({"transfer_id": tx, "dest_world": 2})[0])

    def test_failed_write_never_acknowledges_and_retry_saves_only_one_transfer(self):
        with patch.object(self.authority, "save_to_disk", return_value=(False, "disk full")):
            with self.assertRaises(PersistenceError):
                self.authority.initiate_transfer(self.payload)
            with self.assertRaises(PersistenceError):
                self.authority.initiate_transfer(self.payload)
            self.assertTrue(self.authority.persistence_pending)
        tx = self.initiate()
        self.assertFalse(self.authority.persistence_pending)
        self.restart()
        self.assertEqual(self.initiate(), tx)
        self.assertEqual(len(self.authority.transfers), 1)
        self.assertEqual(self.authority.commodity_initiated, {0: 25})

    def test_http_gate_does_not_publish_pending_state(self):
        class Handler:
            calls = 0
            response = None

            def _send_json(self, code, body):
                self.response = (code, body)

            @durable_request
            def request(self):
                self.calls += 1

        handler = Handler()
        self.authority.persistence_pending = True
        with patch("universe_authority.AUTHORITY", self.authority):
            with patch.object(self.authority, "save_to_disk", return_value=(False, "disk full")):
                handler.request()
            self.assertEqual(handler.calls, 0)
            self.assertEqual(handler.response[0], 503)
            handler.request()
            self.assertEqual(handler.calls, 1)

    def test_corrupt_existing_state_fails_startup_instead_of_resetting_ledger(self):
        Path(self.state_file).write_text("{corrupted", encoding="utf-8")
        with self.assertRaises(PersistenceError):
            self.restart()

    def test_retry_safe_mode_requires_persistence(self):
        self.assertFalse(UniverseAuthority().initiate_transfer(self.payload)[0])

    def test_real_http_handlers_route_retries_and_fail_closed_without_sockets(self):
        class HandlerHarness:
            def __init__(self, path, data=None):
                self.path = path
                self.data = data
                self.response = None

            def _read_json(self):
                return self.data

            def _send_json(self, status, body):
                self.response = (status, copy.deepcopy(body))

        with patch("universe_authority.AUTHORITY", self.authority):
            initiate = HandlerHarness("/transfers/initiate", self.payload)
            with patch.object(self.authority, "save_to_disk", return_value=(False, "disk full")):
                AuthorityHandler.do_POST(initiate)
                self.assertEqual(initiate.response[0], 503)
                query = HandlerHarness("/transfers/pending?dest_world=2")
                AuthorityHandler.do_GET(query)
                self.assertEqual(query.response[0], 503)
            AuthorityHandler.do_POST(initiate)
            self.assertEqual(initiate.response[0], 200)
            tx = initiate.response[1]["transfer_id"]
            for path, body in (
                ("depart", {"transfer_id": tx}),
                ("claim", {"transfer_id": tx, "dest_world": 2}),
                ("confirm", {"transfer_id": tx, "dest_world": 2,
                             "arrival_receipt": "receipt-1"}),
            ):
                for _ in range(2):
                    handler = HandlerHarness("/transfers/" + path, body)
                    AuthorityHandler.do_POST(handler)
                    self.assertEqual(handler.response[0], 200, handler.response)
            self.assertEqual(self.authority.commodity_completed, {0: 25})


if __name__ == "__main__":
    unittest.main()
