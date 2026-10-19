#!/usr/bin/env python3
"""Converts 1-bit logical frames (PBM P1/P4, or the ASCII golden files of tests/golden) to PNG for documentation and review.

The PNG is only a *presentation* of the logical frame: ink dots are dark, the background is a light neutral grey
(positive polarity, ADR-015). The real panel's contrast is NOT modelled and must be measured on hardware.
Standard library only.
"""
import struct
import sys
import zlib

INK = (0x1B, 0x1D, 0x1A)
PAPER = (0xC9, 0xCD, 0xC2)


def read_ascii(path):
    rows = [line.rstrip("\n") for line in open(path) if line.strip()]
    return len(rows[0]), len(rows), [[1 if ch == "#" else 0 for ch in row] for row in rows]


def read_pbm(path):
    if path.endswith(".txt"):
        return read_ascii(path)
    data = open(path, "rb").read()
    magic = data[:2]
    tokens = []
    i = 2
    while len(tokens) < (2 if magic == b"P4" else 2):
        while data[i:i + 1].isspace():
            i += 1
        if data[i:i + 1] == b"#":
            while data[i:i + 1] not in (b"\n", b""):
                i += 1
            continue
        j = i
        while not data[j:j + 1].isspace():
            j += 1
        tokens.append(int(data[i:j]))
        i = j
    w, h = tokens
    if magic == b"P4":
        i += 1
        stride = (w + 7) // 8
        rows = [[(data[i + y * stride + x // 8] >> (7 - x % 8)) & 1 for x in range(w)] for y in range(h)]
    elif magic == b"P1":
        digits = [c for c in data[i:] if c in b"01"]
        rows = [[1 if digits[y * w + x] == 0x31 else 0 for x in range(w)] for y in range(h)]
    else:
        raise SystemExit("unsupported PBM")
    return w, h, rows


def write_png(path, w, h, rows, scale):
    raw = bytearray()
    for y in range(h):
        line = bytearray()
        for x in range(w):
            line += bytes(INK if rows[y][x] else PAPER) * scale
        for _ in range(scale):
            raw += b"\x00" + line

    def chunk(tag, payload):
        body = tag + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    open(path, "wb").write(png)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit("usage: frames_to_png.py in.(pbm|txt) out.png [scale]")
    width, height, pixels = read_pbm(sys.argv[1])
    write_png(sys.argv[2], width, height, pixels, int(sys.argv[3]) if len(sys.argv) > 3 else 4)
