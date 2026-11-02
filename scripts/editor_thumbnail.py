#!/usr/bin/env python3
"""Emit a reviewable PNG of a CI editor BMP using only the Python standard library."""
import base64
import struct
import sys
import zlib
from pathlib import Path


def chunk(tag, data):
    return (struct.pack('>I', len(data)) + tag + data +
            struct.pack('>I', zlib.crc32(tag + data)))


def encode_bmp(path):
    bmp = path.read_bytes()
    if bmp[:2] != b'BM':
        raise ValueError('not a BMP')
    offset = struct.unpack_from('<I', bmp, 10)[0]
    width, signed_height, planes, depth = struct.unpack_from('<iiHH', bmp, 18)
    if width <= 0 or signed_height == 0 or planes != 1 or depth != 32:
        raise ValueError('expected a 32-bit editor BMP')
    height = abs(signed_height)
    stride = width * 4
    raw = bytearray()
    for y in range(height):
        source_y = height - 1 - y if signed_height > 0 else y
        row = bmp[offset + source_y * stride:offset + (source_y + 1) * stride]
        if len(row) != stride:
            raise ValueError('truncated BMP')
        raw.append(0)
        for x in range(0, stride, 4):
            raw.extend((row[x + 2], row[x + 1], row[x]))
    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


if __name__ == '__main__':
    path = Path(sys.argv[1])
    if not path.is_file():
        print('EDITOR_PNG_MISSING')
        sys.exit(0)
    encoded = base64.b64encode(encode_bmp(path)).decode('ascii')
    print('EDITOR_PNG_BEGIN')
    for offset in range(0, len(encoded), 4096):
        print(encoded[offset:offset + 4096])
    print('EDITOR_PNG_END')
