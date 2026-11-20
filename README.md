# FX3.DX12

FX3.DX12 is a standalone renderer and game-engine foundation. This repository contains engine and renderer code only; game-specific source code belongs in a separate game project.

## Initial target

- Windows 10/11 x64
- C++20
- Win32 platform layer
- Direct3D 12 / DXGI renderer
- HLSL shaders compiled with DXC
- CMake build system

## Development status

Early development. The first milestone (`0.1.0`) establishes the engine application shell, Win32 window, D3D12 device and command queue lifecycle, swap chain, frame synchronization, and a minimal renderer loop. It is not yet a production-ready engine.

## Repository layout

- `include/fx3/` — public engine headers
- `src/core/` — engine lifecycle, logging, and core utilities
- `src/platform/` — Win32 window and input layer
- `src/rhi/d3d12/` — Direct3D 12 backend
- `src/renderer/` — renderer orchestration and frame lifecycle
- `shaders/` — HLSL source
- `docs/` — architecture and development notes
- `tests/` — automated tests

## Build requirements

- Visual Studio 2022 (MSVC) or compatible MSVC toolchain
- Windows 10/11 SDK with Direct3D 12 headers and libraries
- CMake 3.24 or newer

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

No game-specific gameplay code or game assets are included.