# FX3.DX12 0.3.9 — AMD FSR 3 runtime loader

## Runtime files

The loader expects these x64 AMD runtime DLLs in one directory:

- ffx_fsr3_x64.dll
- ffx_fsr3upscaler_x64.dll
- ffx_opticalflow_x64.dll
- ffx_frameinterpolation_x64.dll
- ffx_backend_dx12_x64.dll

The loader uses AddDllDirectory, LoadLibraryExW and GetProcAddress. It does not link
AMD import libraries and does not require AMD DLLs at build time.

## Current integration level

Version 0.3.9 adds runtime discovery, dependency loading and export validation.
It verifies the FSR 3 upscaler, optical-flow, frame-interpolation and DX12 backend
entry points observed in the supplied DLL export tables.

This is a loader milestone, not a claim that GPU upscaling or generated-frame
presentation is already active. FSR 3's legacy API takes version-specific context
descriptions, callback tables, resource descriptors and DX12 interface structures.
The exact SDK headers/build identity for the supplied DLL set have not yet been
established. The engine therefore does not cast or invoke unverified function pointers.

Before enabling dispatch, match the DLL build to its AMD SDK headers and wire:
- render-resolution and jitter updates into the camera;
- color, depth, motion-vector and exposure resources;
- reactive/transparency masks where required;
- DX12 backend interface and scratch memory;
- context creation, per-frame dispatch and fence-safe destruction;
- frame-interpolation swapchain/present integration.

## Deployment

Configure CMake with:

    cmake -S . -B build-fx3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe" -DCMAKE_CXX_FLAGS="-fuse-ld=lld" -DDXC_EXECUTABLE="C:/XWe0.2/cmp-utils/dx/dxc.exe" -DFX3_FSR3_DLL_DIR="C:/XWe0.2/third_party/FSR" -DFX3_ENABLE_FSR3=ON

When FX3_FSR3_DLL_DIR is set, CMake copies the five runtime DLLs beside the
executable under third_party/FSR. The engine looks for that directory relative to
the executable. Keep AMD's original DLL files and licensing notices intact.

If the DLL directory is absent at runtime, FX3.DX12 continues with native rendering.
