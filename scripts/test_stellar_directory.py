#!/usr/bin/env python3
"""Host authentication, collision, heartbeat and durable directory regressions."""
import unittest
from unittest.mock import patch
from stellar_directory import StellarDirectory

class DirectoryTests(unittest.TestCase):
    def host(self, namespace="1:1", world=1):
        return {"namespace": namespace, "address": "localhost:3979", "manifest": "a"*64,
                "worlds": [{"id": world, "name": "Mito", "opened": True, "x": 0, "y": 0, "catalogue": "mito"}],
                "stations": [], "gates": [], "companies": []}

    def test_authentication_and_no_partial_write(self):
        d = StellarDirectory("secret")
        self.assertFalse(d.publish({"host": self.host()})[0])
        self.assertFalse(StellarDirectory().publish({"token": "", "host": self.host()})[0])
        self.assertEqual(d.snapshot()["hosts"], [])
        self.assertTrue(d.publish({"token": "secret", "host": self.host()})[0])
        self.assertFalse(d.publish({"token": "secret", "host": self.host("2:2")})[0])
        self.assertEqual(len(d.snapshot()["hosts"]), 1)

    def test_offline_restart_and_refresh(self):
        d=StellarDirectory("secret")
        with patch("stellar_directory.time.time", return_value=100):
            self.assertTrue(d.publish({"token":"secret", "host":self.host()})[0])
            self.assertTrue(d.snapshot()["hosts"][0]["online"])
        with patch("stellar_directory.time.time", return_value=131):
            self.assertFalse(d.snapshot()["hosts"][0]["online"])
        other=StellarDirectory("secret"); other.import_state(d.export_state())
        self.assertFalse(other.snapshot()["hosts"][0]["online"])
        self.assertTrue(other.publish({"token":"secret", "host":self.host()})[0])
        self.assertTrue(other.snapshot()["hosts"][0]["online"])

    def test_live_duplicate_host_is_rejected_until_offline(self):
        d = StellarDirectory("secret")
        with patch("stellar_directory.time.time", return_value=100):
            self.assertTrue(d.publish({"token": "secret", "host": self.host()})[0])
            other = self.host(); other["address"] = "localhost:3980"
            self.assertFalse(d.publish({"token": "secret", "host": other})[0])
        with patch("stellar_directory.time.time", return_value=131):
            self.assertTrue(d.publish({"token": "secret", "host": other})[0])

    def test_invalid_destination_records(self):
        d=StellarDirectory("secret")
        for field,value in [("namespace","bad"),("manifest","x"*64),("address","http://bad/")]:
            h=self.host(); h[field]=value
            self.assertFalse(d.publish({"token":"secret", "host":h})[0])
        h=self.host(); h["stations"]=[{"world":2,"namespace":"1:1","sequence":1,"name":"bad"}]
        self.assertFalse(d.publish({"token":"secret", "host":h})[0])

    def test_research_home_monotonicity_and_cold_reload(self):
        d = StellarDirectory("secret")
        h = self.host()
        h["ruleset"] = 1
        h["companies"] = [{"identity": "1:1:1", "local": 0}]
        h["researchs"] = [{"id": "1:1:1", "home": "1:1", "revision": 1, "unlocks": [301]}]
        self.assertTrue(d.publish({"token": "secret", "host": h})[0])
        self.assertTrue(d.publish({"token": "secret", "host": h})[0])
        other = StellarDirectory("secret")
        other.import_state(d.export_state())
        self.assertEqual(other.export_state()["1:1"]["researchs"], d.export_state()["1:1"]["researchs"])
        h["researchs"][0].update(revision=2, unlocks=[301, 302])
        self.assertTrue(other.publish({"token": "secret", "host": h})[0])
        h["researchs"][0].update(revision=1, unlocks=[301])
        self.assertFalse(other.publish({"token": "secret", "host": h})[0])
        remote = self.host("2:2", 2)
        remote.update(ruleset=1, companies=h["companies"], researchs=[{"id": "1:1:1", "home": "2:2", "revision": 2, "unlocks": [301,302]}])
        self.assertFalse(other.publish({"token": "secret", "host": remote})[0])
        remote.update(ruleset=0, researchs=[])
        self.assertFalse(other.publish({"token": "secret", "host": remote})[0])

if __name__ == "__main__":
    unittest.main()
