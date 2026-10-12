#!/usr/bin/env python3
"""Decode bounded native save chunks offline; this does not prove an engine reload."""

import lzma

MAX_RAW = 32 * 1024 * 1024
MAX_ROWS = 65536


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def take(self, count):
        if count < 0 or count > len(self.data) - self.pos:
            raise ValueError('truncated save chunk')
        result = self.data[self.pos:self.pos + count]
        self.pos += count
        return result

    def byte(self):
        return self.take(1)[0]

    def gamma(self):
        value = self.byte()
        if value < 128:
            return value
        if value < 192:
            count, value = 1, value & 63
        elif value < 224:
            count, value = 2, value & 31
        elif value < 240:
            count, value = 3, value & 15
        elif value < 248:
            count, value = 4, 0
        else:
            raise ValueError('unsupported save gamma prefix')
        return (value << (count * 8)) | int.from_bytes(self.take(count), 'big')

    def string(self):
        return self.take(self.gamma()).decode('utf-8')

    def finish(self):
        if self.pos != len(self.data):
            raise ValueError('unconsumed save bytes')


def decode_chunks(raw):
    """Accept only the version367 OTTX format, with bounded decompression."""
    if len(raw) < 8 or raw[:8] != b'OTTX\x01\x6f\x00\x00':
        raise ValueError('expected OTTX save version367')
    decoder = lzma.LZMADecompressor(memlimit=64 * 1024 * 1024)
    data = decoder.decompress(raw[8:], max_length=MAX_RAW + 1)
    if len(data) > MAX_RAW or not decoder.eof or decoder.unused_data:
        raise ValueError('save decompression exceeds bounds or has trailing bytes')
    reader, chunks = Reader(data), {}
    while True:
        name = reader.take(4)
        if name == b'\0\0\0\0':
            break
        name = name.decode('ascii')
        if name in chunks:
            raise ValueError('duplicate save chunk')
        mode = reader.byte()
        if mode & 15 == 0:
            length = ((mode >> 4) << 24) | int.from_bytes(reader.take(3), 'big')
            chunks[name] = reader.take(length)
        elif mode & 15 in (1, 2, 3, 4):
            rows = []
            while (length := reader.gamma()):
                if len(rows) >= MAX_ROWS:
                    raise ValueError('save row bound exceeded')
                rows.append(reader.take(length - 1))
            chunks[name] = rows
        else:
            raise ValueError('unsupported save chunk mode')
    reader.finish()
    return chunks


def decode_table(rows):
    """Read scalar/string and length-prefixed numeric fields used by MAPS/PLNT/RSRC."""
    if not isinstance(rows, list) or not rows:
        raise ValueError('missing table header')
    header, fields = Reader(rows[0]), []
    while (field_type := header.byte()):
        name = header.string()
        if field_type & ~31 or any(name == field[0] for field in fields):
            raise ValueError('unsupported or duplicate table field')
        fields.append((name, field_type))
    header.finish()
    output = []
    for row in rows[1:]:
        reader, values = Reader(row), {}
        for name, field_type in fields:
            kind = field_type & 15
            if kind == 10:
                values[name] = reader.string()
            elif kind in range(1, 10):
                size = {1: 1, 2: 1, 3: 2, 4: 2, 5: 4, 6: 4, 7: 8, 8: 8, 9: 2}[kind]
                count = reader.gamma() if field_type & 16 else 1
                if count > MAX_ROWS:
                    raise ValueError('save array bound exceeded')
                numbers = [int.from_bytes(reader.take(size), 'big', signed=kind in (1, 3, 5, 7))
                           for _ in range(count)]
                values[name] = numbers if field_type & 16 else numbers[0]
            else:
                raise ValueError('unsupported table field type')
        reader.finish()
        output.append(values)
    return output
