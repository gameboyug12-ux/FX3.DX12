#!/usr/bin/env python3
"""Python ctypes smoke test for the XWe engine DLL."""
from __future__ import annotations
import argparse
import ctypes
import os
import sys
from pathlib import Path
from ctypes import wintypes

ROOT = Path(__file__).resolve().parents[2]

class XWeApplicationDesc(ctypes.Structure):
    _fields_ = [
        ("struct_size", wintypes.UINT),
        ("window_title", wintypes.LPCWSTR),
        ("window_width", wintypes.UINT),
        ("window_height", wintypes.UINT),
        ("enable_debug_layer", wintypes.UINT),
        ("upscaling_mode", wintypes.UINT),
    ]

def find_dll(explicit):
    paths = [Path(explicit)] if explicit else []
    if os.environ.get("XWE_ENGINE_DLL"):
        paths.append(Path(os.environ["XWE_ENGINE_DLL"]))
    paths += [
        ROOT / "build-engine/bin/Release/xwe_engine.dll",
        ROOT / "build-engine/bin/Debug/xwe_engine.dll",
        ROOT / "build/bin/Release/xwe_engine.dll",
        ROOT / "build/bin/Debug/xwe_engine.dll",
    ]
    for path in paths:
        path = path.expanduser().resolve()
        if path.is_file():
            return path
    raise FileNotFoundError("xwe_engine.dll not found. Run engine/build_engine_dll.ps1 or pass --dll PATH.")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll")
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()
    if os.name != "nt":
        print("XWe engine DLL requires Windows x64.", file=sys.stderr)
        return 2
    if not args.smoke and not args.run:
        args.smoke = True
    dll_path = find_dll(args.dll)
    print("[Python] Loading", dll_path)
    cookie = os.add_dll_directory(str(dll_path.parent))
    try:
        dll = ctypes.WinDLL(str(dll_path))
        dll.XWeGetVersion.argtypes = []
        dll.XWeGetVersion.restype = wintypes.LPCWSTR
        dll.XWeCreateApplication.argtypes = [ctypes.POINTER(XWeApplicationDesc)]
        dll.XWeCreateApplication.restype = ctypes.c_void_p
        dll.XWeRunApplication.argtypes = [ctypes.c_void_p]
        dll.XWeRunApplication.restype = ctypes.c_int32
        dll.XWeDestroyApplication.argtypes = [ctypes.c_void_p]
        dll.XWeDestroyApplication.restype = None
        print("[Python] Engine version:", dll.XWeGetVersion())
        desc = XWeApplicationDesc(ctypes.sizeof(XWeApplicationDesc),
            "FX3.DX12 - Python DLL Test", max(320, args.width),
            max(240, args.height), 1, 0)
        handle = dll.XWeCreateApplication(ctypes.byref(desc))
        if not handle:
            print("XWeCreateApplication returned NULL", file=sys.stderr)
            return 3
        print("[Python] XWeCreateApplication: OK")
        try:
            if args.run:
                print("Engine window running; close it to return.")
                result = dll.XWeRunApplication(handle)
                print("XWeRunApplication returned:", result)
                return int(result)
            print("Smoke test passed.")
            return 0
        finally:
            dll.XWeDestroyApplication(handle)
            print("XWeDestroyApplication: OK")
    finally:
        cookie.close()

if __name__ == "__main__":
    raise SystemExit(main())
