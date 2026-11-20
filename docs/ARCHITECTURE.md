# FX3.DX12 architecture — 0.1.0

## Scope

FX3.DX12 is the engine/renderer repository, not a game repository. Game-specific gameplay, assets, levels, and application source are intentionally excluded.

## Runtime flow

1. `fx3::Engine` owns the application lifetime.
2. The Win32 platform layer registers and creates the native window.
3. `D3D12Renderer` creates the DXGI factory, selects a hardware adapter, and creates the D3D12 device.
4. A direct command queue and a two-buffer flip-discard swap chain are created for the window.
5. Each frame waits for the selected frame resource, resets its command allocator, records a back-buffer transition and clear, executes the command list, and presents.
6. A D3D12 fence tracks GPU completion before command allocators or swap-chain buffers are reused.

## Current modules

- Public engine entry: `include/fx3/engine.hpp`
- Runtime and Win32 window procedure: `src/engine.cpp`
- Windows GUI entry point: `src/main.cpp`
- Build configuration: `CMakeLists.txt`

## Current limitations

This is a bootstrap renderer, not a complete production engine. It currently has no shader pipeline, geometry buffers, descriptor allocator abstraction, resource state tracker, frame graph, input abstraction, asset pipeline, ECS, audio, physics, editor, or game layer. Those systems should be added behind stable interfaces in later milestones.

## Next milestones

- 0.2: logging/assertions, platform abstraction, input events, renderer interfaces
- 0.3: GPU resource wrappers, descriptor allocation, upload buffers, barriers
- 0.4: HLSL/DXC shader compilation and pipeline state management
- 0.5: vertex/index buffers, depth buffer, camera, indexed draw
- 0.6: render graph foundation, profiling markers, GPU timing
