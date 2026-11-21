# FX3.DX12 architecture — 0.2.0

## Repository boundary

FX3.DX12 contains the reusable engine and renderer only. A game should consume the engine from a separate project and must not place game-specific gameplay or assets in this repository.

## 0.2 renderer flow

1. Engine creates the Win32 window and initializes the D3D12 renderer.
2. DXGI enumerates hardware adapters and creates the D3D12 device.
3. Renderer creates a direct command queue, a two-buffer swap chain, per-frame command allocators, and a fence.
4. Renderer creates RTV/DSV heaps, swap-chain render targets, and a D32 depth buffer.
5. DXC compiles mesh_vs.hlsl and mesh_ps.hlsl to shader-model 6.0 DXIL binaries.
6. Renderer creates a root signature with one root CBV and a graphics PSO with position, normal, and color vertex inputs.
7. A built-in indexed cube is stored in vertex/index upload buffers for this bootstrap milestone.
8. Each frame waits for the frame fence, updates that frame's constant buffer, transitions the back buffer, clears color/depth, binds the pipeline and geometry, draws 36 indices, transitions to PRESENT, and presents.
9. Window resize waits for GPU idle, resizes swap-chain buffers, and recreates the depth resource.

## Current source map

- include/fx3/engine.hpp — public engine configuration and entry point
- src/main.cpp — Windows GUI entry point
- src/engine.cpp — initial Win32 runtime and D3D12 renderer implementation
- shaders/mesh_vs.hlsl — world/view-projection transform
- shaders/mesh_ps.hlsl — directional diffuse lighting
- CMakeLists.txt — C++20 build and DXC shader compilation

## Important implementation notes

- Constant buffers are allocated per frame and aligned to 256 bytes.
- Frame allocator reuse is guarded by per-frame fence values.
- Depth is D32_FLOAT and is recreated when the client area changes size.
- The sample cube rotates continuously so that depth testing and 3D transforms are visible immediately.
- The first milestone uses upload-heap geometry buffers for simplicity. A later version should add default-heap resources and explicit upload/copy commands.

## Deliberate omissions

This milestone is a 3D renderer proof of foundation, not a complete engine. It has no general scene graph, user camera controller, mesh importer, texture/material system, descriptor allocator, render graph, ECS, physics, audio, or editor. These should be introduced as reusable engine modules rather than game-specific code.

## Suggested next milestones

- 0.3: renderer/backend separation, GPU resource wrappers, default-heap geometry, upload staging, descriptor allocator
- 0.4: material system, shader reflection, texture resources and sampler management
- 0.5: camera/input abstraction, reusable mesh and scene interfaces, model import
- 0.6: render graph foundation, GPU profiling, debug markers, resize/minimize hardening
