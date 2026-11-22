# FX3.DX12 — v0.4 Renderer

FX3 is a Windows x64, C++20, Direct3D 12 renderer foundation. **v0.4 intentionally has no scene system, ECS, editor, or gameplay layer.** It loads one static OBJ mesh and one FXTEX texture, uploads them to D3D12, and draws the textured mesh.

## v0.4 features

- Win32 window and Direct3D 12 device/adapter initialization (hardware adapter with WARP fallback).
- Direct queue, flip-discard swap chain, RTV/DSV, depth testing, command allocators/list, fence synchronization.
- Basic graphics pipeline, root signature, SRV descriptor heap, linear-wrap sampler, runtime HLSL compilation.
- OBJ geometry loader: positions, UVs, normals, polygon fan triangulation, positive and negative OBJ indices.
- FXTEX v1 RGBA8 texture loader and GPU upload.
- Automatic mesh centering and fitting to the initial clip-space view.

## Asset contract

Put these files in the runtime `assets/` directory:

- `model.obj` — one OBJ mesh. UV coordinates should be present for textured output.
- `albedo.fxtex` — texture in FXTEX v1 format.

### FXTEX v1 binary format

All integer fields are little-endian. Pixel data is tightly packed row-major RGBA8.

| Offset | Type | Meaning |
|---:|---|---|
| 0 | 8 bytes | ASCII magic `FXTEX001` |
| 8 | uint32 | Width |
| 12 | uint32 | Height |
| 16 | width × height × 4 bytes | RGBA8 pixel payload |

The loader rejects zero/oversized dimensions and truncated payloads. v0.4 supports one mip level, uncompressed RGBA8, and a 512 MiB payload limit. Convert a binary PPM (P6, max value 255) with `python tools/ppm_to_fxtex.py input.ppm assets/albedo.fxtex`.

## Build

Requirements: Windows 10/11 x64, Visual Studio 2022 (MSVC), Windows SDK with D3D12, and CMake 3.24+.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
````

Run `build/Debug/FX3DX12.exe` with `assets/model.obj` and `assets/albedo.fxtex` available relative to the working directory.

## Current v0.4 scope / limitations

- Single static mesh + single albedo texture; no material system or multi-mesh batching.
- No scene graph, ECS, camera controls, transforms, animation, lighting system, mip generation, compressed textures, or resize-aware swap-chain recreation.
- OBJ coordinates are normalized to fit the initial clip-space view; this is a renderer bring-up path, not a world-space camera.
- Missing OBJ normals receive a simple up-vector fallback.
- The HLSL used by the current pipeline is embedded in the renderer; `shaders/Basic.hlsl` documents the shader interface.
- The implementation has not yet been built or run on a Windows/D3D12 machine in this environment. Validate with the build commands above and the D3D12 debug layer.

The engine remains renderer-only. Game-specific source and assets belong in a separate game project.
