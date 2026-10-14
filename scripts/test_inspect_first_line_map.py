#!/usr/bin/env python3
"""Bounded offline native save decoding; no private packet or game process."""

import lzma
import unittest

import inspect_first_line_map as inspector



class SavedBytesTest(unittest.TestCase):
    def test_gamma_boundaries_and_truncation(self):
        for raw, expected in ((b'\x7f', 127), (b'\x80\x80', 128),
                              (b'\xc0\x40\x00', 16384),
                              (b'\xe0\x20\x00\x00', 2097152),
                              (b'\xf0\x10\x00\x00\x00', 268435456)):
            self.assertEqual(inspector.Reader(raw).gamma(), expected)
        for raw in (b'', b'\x80', b'\xf8'):
            with self.assertRaises(ValueError):
                inspector.Reader(raw).gamma()

    def test_header_eof_and_trailing_bytes_are_required(self):
        for raw in (b'', b'OTTX\x01\x6e\0\0',
                    b'OTTX\x01\x6f\0\0' + lzma.compress(b''),
                    b'OTTX\x01\x6f\0\0' + lzma.compress(bytes(4)) + b'extra'):
            with self.assertRaises((ValueError, lzma.LZMAError)):
                inspector.decode_chunks(raw)

    def test_table_string_and_numeric_array_flags(self):
        # Native PLNT name uses26 (SLF_STRING with length flag), RSRC grfid18.
        rows = [b'\x1a\x04name\x12\x05grfid\0',
                b'\x04Town\x04OST\x01']
        self.assertEqual(inspector.decode_table(rows), [{'name': 'Town', 'grfid': [79, 83, 84, 1]}])
        with self.assertRaises(ValueError):
            inspector.decode_table([rows[0], rows[1][:-1]])
        with self.assertRaises(ValueError):
            inspector.decode_table([rows[0], rows[1] + b'extra'])

    def test_complete_chunk_stream_and_duplicate_rejection(self):
        chunk = b'TEST\0\0\0\x01x'
        header = b'OTTX\x01\x6f\0\0'
        self.assertEqual(inspector.decode_chunks(header + lzma.compress(chunk + bytes(4))), {'TEST': b'x'})
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            inspector.decode_chunks(header + lzma.compress(chunk + chunk + bytes(4)))


if __name__ == '__main__':
    unittest.main()
