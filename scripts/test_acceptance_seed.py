#!/usr/bin/env python3
"""Regression for the native generator seed passed by the shared acceptance runner."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch
from test_wp11_slice import Engine


class SeedTests(unittest.TestCase):
    def test_requested_seed_reaches_native_command_line(self):
        # Mock process I/O, not the argument construction that previously hardcoded 11.
        for seed in (11, 101, 2026):
            with self.subTest(seed=seed), tempfile.TemporaryDirectory() as folder:
                process = MagicMock()
                process.stdout = iter(())
                process.poll.return_value = 0
                with patch('test_wp11_slice.subprocess.Popen', return_value=process) as launch, \
                     patch.object(Engine, 'wait', return_value=''):
                    engine = Engine(Path('/unused/openttd'), Path('/unused/config'),
                                    Path(folder), 'seed', seed=seed)
                    engine.close()
                command = launch.call_args.args[0]
                self.assertEqual(command[command.index('-G') + 1], str(seed))


if __name__ == '__main__':
    unittest.main()
