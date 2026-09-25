#!/usr/bin/env python3
"""Check that federation recovery rejects mixed or incomplete checkpoint sets."""
import json
from pathlib import Path
import tempfile
import unittest

from federation_checkpoint import publish, validate


class CheckpointTests(unittest.TestCase):
    def test_complete_checkpoint_and_mixed_save_rejection(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            binary = root / "binary"
            binary.write_bytes(b"verified binary")
            saves = {world: root / f"input{world}.sav" for world in (1, 2)}
            for world, path in saves.items():
                path.write_bytes(f"world {world}".encode())
            authority = root / "authority.json"
            authority.write_text(json.dumps({"worlds": {"1": {}, "2": {}}, "transfers": {}}))
            checkpoint = root / "checkpoint"
            expected = publish(checkpoint, binary, saves, authority, phase="pre-departure")
            self.assertEqual(validate(checkpoint, binary), expected)
            with self.assertRaisesRegex(ValueError, "must be new"):
                publish(checkpoint, binary, saves, authority, phase="pre-departure")
            (checkpoint / "world2.sav").write_bytes(b"unrelated older save")
            with self.assertRaisesRegex(ValueError, "artifact changed"):
                validate(checkpoint, binary)

    def test_incomplete_set_and_changed_binary_rejection(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            binary = root / "binary"
            binary.write_bytes(b"verified binary")
            save = root / "input.sav"
            save.write_bytes(b"save")
            authority = root / "authority.json"
            authority.write_text(json.dumps({"worlds": {"1": {}, "2": {}}, "transfers": {}}))
            checkpoint = root / "checkpoint"
            with self.assertRaisesRegex(ValueError, "requires saves"):
                publish(checkpoint, binary, {1: save}, authority, phase="arrival")
            self.assertFalse(checkpoint.exists())
            publish(checkpoint, binary, {1: save, 2: save}, authority, phase="arrival")
            binary.write_bytes(b"different binary")
            with self.assertRaisesRegex(ValueError, "binary differs"):
                validate(checkpoint, binary)


if __name__ == "__main__":
    unittest.main()
