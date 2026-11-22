#!/usr/bin/env python3
"""Convert a binary PPM (P6, maxval 255) image to FXTEX v1."""
import argparse
import struct
from pathlib import Path

def tokens(data):
    i = 0
    while i < len(data):
        while i < len(data) and data[i] in b" \t\r\n":
            i += 1
        if i < len(data) and data[i] == 35:
            while i < len(data) and data[i] not in b"\r\n":
                i += 1
            continue
        start = i
        while i < len(data) and data[i] not in b" \t\r\n#":
            i += 1
        if start < i:
            yield data[start:i], i

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    data = args.input.read_bytes()
    it = iter(tokens(data))
    magic, _ = next(it)
    if magic != b"P6":
        raise SystemExit("Only binary PPM P6 is supported")
    width_token, _ = next(it)
    height_token, _ = next(it)
    max_token, end = next(it)
    width, height, maxval = int(width_token), int(height_token), int(max_token)
    if width <= 0 or height <= 0 or width > 16384 or height > 16384 or maxval != 255:
        raise SystemExit("Expected dimensions 1..16384 and maxval 255")
    # Consume the single whitespace separator after maxval (CRLF is treated as one line ending).
    offset = end
    if data[offset:offset+2] == b"\r\n":
        offset += 2
    elif data[offset:offset+1] in (b" ", b"\t", b"\r", b"\n"):
        offset += 1
    rgb = data[offset:]
    expected = width * height * 3
    if len(rgb) != expected:
        raise SystemExit(f"Expected {expected} RGB bytes, got {len(rgb)}")
    rgba = bytearray(width * height * 4)
    rgba[0::4] = rgb[0::3]
    rgba[1::4] = rgb[1::3]
    rgba[2::4] = rgb[2::3]
    rgba[3::4] = b"\xff" * (width * height)
    args.output.write_bytes(b"FXTEX001" + struct.pack("<II", width, height) + rgba)
    print(f"Wrote {args.output} ({width}x{height}, RGBA8)")

if __name__ == "__main__":
    main()
