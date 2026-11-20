# FX3.DX12

FX3.DX12 is a standalone 3D renderer and game-engine foundation. This repository contains engine/runtime code only. Game-specific source, gameplay, levels, and assets belong in a separate game repository.

## 0.2.0 — 3D renderer foundation

The 0.2.0 milestone moves beyond the clear-only bootstrap and renders an animated, indexed 3D cube using Direct3D 12.

Implemented:
- Win32 window and engine loop
- DXGI hardware adapter selection and D3D12 device
- Direct command queue, command allocators, command list, and fence synchronization
- Two-buffer flip-discard swap chain
- RTV and DSV descriptor heaps
- Depth buffer with depth testing and depth writes
- Vertex and index buffers
- HLSL vertex/pixel shaders compiled with DXC
- Root signature and graphics pipeline state object
- Per-frame constant buffers
- World, view, and perspective projection matrices using DirectXMath
- Animated 3D cube with per-face color and directional diffuse lighting
- Resize handling for swap-chain and depth resources

## Build requirements

- Windows 10/11 x64
- Visual Studio 2022 with MSVC
- Windows 10/11 SDK
- CMake 3.24+
- DirectX Shader Compiler (dxc.exe) available on PATH

```powershell
git clone -b release/0.2.0 https://github.com/gameboyug12-ux/FX3.DX12.git
cd FX3.DX12
cmake -S . -B build -A x64
cmake --build build --config Debug
```

The executable is generated under build/bin/Debug/ with Visual Studio generators. Compiled HLSL shaders are written to build/bin/shaders/.

## Repository layout

- include/fx3/ — public engine API
- src/ — engine runtime and initial D3D12 renderer
- shaders/ — HLSL source
- docs/ — architecture notes

## Current limitations

This is an early 3D renderer milestone, not a production-ready engine. The renderer currently draws one built-in mesh. It does not yet provide a general mesh/material system, camera input controls, texture loading, model import, render graph, ECS, physics, audio, or editor.

## Next milestone

The next iteration should extract the D3D12 backend from the engine runtime and add reusable GPU buffers, descriptor allocation, material/shader management, camera controls, mesh loading, and a basic scene interface.
