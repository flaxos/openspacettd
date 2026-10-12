#!/usr/bin/env python3
"""Offline custody guards for the Vale resume launcher; no native process."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from first_permanent_line import digest
from resume_vale_railway import main, verified_delivery


class ResumeCustodyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='vale-resume-')
        self.root = Path(self.temp.name)
        engine = self.root / 'engine'
        engine.mkdir()
        (engine / 'openttd').write_bytes(b'fixture executable')
        (engine / 'cargo.grf').write_bytes(b'fixture content')
        self.manifest = self.root / 'manifest.json'
        self.manifest.write_text(json.dumps({'binary': str(engine / 'openttd'), 'binary_sha256': digest(engine / 'openttd'),
                                             'files': {'cargo.grf': digest(engine / 'cargo.grf')}}))
        self.save, self.config = self.root / 'owner.sav', self.root / 'vale.cfg'
        self.save.write_bytes(b'fixture owner progress')
        self.config.write_text('fixture config')
        self.delivery = self.root / 'delivery.json'
        self.delivery.write_text(json.dumps({'runtime_manifest': str(self.manifest), 'runtime_manifest_sha256': digest(self.manifest),
                                             'save': str(self.save), 'save_sha256': digest(self.save),
                                             'config': str(self.config), 'config_sha256': digest(self.config)}))

    def tearDown(self):
        self.temp.cleanup()

    def test_verify_only_cannot_launch_or_create_profile(self):
        with patch('subprocess.run', side_effect=AssertionError('No game permitted')):
            self.assertEqual(main(['--delivery', str(self.delivery), '--verify-only']), 0)

    def test_changed_owner_save_rejected(self):
        self.save.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'save differs'):
            verified_delivery(self.delivery)

    def test_extra_runtime_content_rejected(self):
        (self.root / 'engine' / 'extra.grf').write_bytes(b'extra')
        with self.assertRaisesRegex(RuntimeError, 'inventory differs'):
            verified_delivery(self.delivery)

    def test_same_bytes_through_symlink_rejected(self):
        target = self.root / 'actual.sav'
        self.save.rename(target)
        self.save.symlink_to(target)
        with self.assertRaisesRegex(RuntimeError, 'symlink'):
            verified_delivery(self.delivery)


if __name__ == '__main__':
    unittest.main()
