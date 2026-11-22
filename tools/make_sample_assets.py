#!/usr/bin/env python3
"""Generate a small OBJ cube and checker FXTEX v1 texture for a smoke test."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
assets = root / "assets"
assets.mkdir(exist_ok=True)

obj = """# FX3 v0.4 sample cube
v -1 -1 -1
v  1 -1 -1
v  1  1 -1
v -1  1 -1
v -1 -1  1
v  1 -1  1
v  1  1  1
v -1  1  1
vt 0 1
vt 1 1
vt 1 0
vt 0 0
vn 0 0 -1
vn 0 0 1
vn -1 0 0
vn 1 0 0
vn 0 -1 0
vn 0 1 0
f 1/1/1 4/4/1 3/3/1 2/2/1
f 5/1/2 6/2/2 7/3/2 8/4/2
f 1/1/3 5/2/3 8/3/3 4/4/3
f 2/1/4 3/4/4 7/3/4 6/2/4
f 1/1/5 2/2/5 6/3/5 5/4/5
f 4/1/6 8/2/6 7/3/6 3/4/6
"""
(assets / "model.obj").write_text(obj, encoding="utf-8")

pixels = bytes([
    235, 70, 65, 255,  45, 170, 230, 255,
    45, 170, 230, 255, 235, 70, 65, 255,
])
(assets / "albedo.fxtex").write_bytes(b"FXTEX001" + struct.pack("<II", 2, 2) + pixels)
print("Generated assets/model.obj and assets/albedo.fxtex")
