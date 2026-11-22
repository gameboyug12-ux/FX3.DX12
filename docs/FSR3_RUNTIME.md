# FX3.DX12 0.3.9 — AMD FSR 3 runtime loader

## Runtime files found in the supplied SDK directory

The supplied directory is:

    C:/XWE0.2/THIRD_PARTY/FSR

It contains the Release x64 runtime files required by the current loader:

- ffx_fsr3_x64.dll
- ffx_fsr3upscaler_x64.dll
- ffx_opticalflow_x64.dll
- ffx_frameinterpolation_x64.dll
- ffx_backend_dx12_x64.dll

The directory also contains Debug and drel variants (for example, ffx_fsr3_x64d.dll
and ffx_fsr3_x64drel.dll). The current Release deployment copies only the unsuffixed
Release DLLs listed above. Do not mix Debug or drel binaries with a Release build.

The loader uses LoadLibraryExW and GetProcAddress. It does not link AMD import
libraries and does not require the AMD DLLs at build time.

## Current integration level

Version 0.3.9 adds runtime discovery, dependency loading and export validation.
It checks for the expected FSR 3 upscaler, optical-flow, frame-interpolation and
DX12 backend export names. The DLL filenames are confirmed by the supplied directory
listing, but the export tables have not yet been inspected. A successful file listing
does not prove that the expected symbol names or ABI match this SDK build.

This is a loader milestone, not a claim that GPU upscaling or generated-frame
presentation is already active. FSR 3's API uses version-specific context
descriptions, callback tables, resource descriptors and DX12 interface structures.
The engine therefore does not cast or invoke unverified function pointers.

Before enabling dispatch, match the DLL build to its AMD SDK headers and wire:
- render-resolution and jitter updates into the camera;
- color, depth, motion-vector and exposure resources;
- reactive/transparency masks where required;
- DX12 backend interface and scratch memory;
- context creation, per-frame dispatch and fence-safe destruction;
- frame-interpolation swapchain/present integration.

## Verify the actual DLL exports

Run one of these commands in a Visual Studio Developer PowerShell:

    dumpbin /exports "C:\\XWE0.2\\THIRD_PARTY\\FSR\\ffx_fsr3_x64.dll"
    dumpbin /exports "C:\\XWE0.2\\THIRD_PARTY\\FSR\\ffx_fsr3upscaler_x64.dll"
    dumpbin /exports "C:\\XWE0.2\\THIRD_PARTY\\FSR\\ffx_opticalflow_x64.dll"
    dumpbin /exports "C:\\XWE0.2\\THIRD_PARTY\\FSR\\ffx_frameinterpolation_x64.dll"
    dumpbin /exports "C:\\XWE0.2\\THIRD_PARTY\\FSR\\ffx_backend_dx12_x64.dll"

Alternatively, with LLVM installed:

    llvm-readobj --coff-exports "C:\\XWE0.2\\THIRD_PARTY\\FSR\\ffx_fsr3_x64.dll"

Compare the output with the required names in src/rendering/upscaling/FSR3Loader.cpp.
Do not treat the loader's capability flags as proof that GPU dispatch is implemented.

## Deployment

Configure CMake using the exact directory reported above:

    cmake -S . -B build-fx3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe" -DCMAKE_CXX_FLAGS="-fuse-ld=lld" -DDXC_EXECUTABLE="C:/XWe0.2/cmp-utils/dx/dxc.exe" -DFX3_FSR3_DLL_DIR="C:/XWE0.2/THIRD_PARTY/FSR" -DFX3_ENABLE_FSR3=ON

Then build:

    cmake --build build-fx3 --target XWeShaders xwe_engine_shared xwe_engine_static FX3DX12 FX3Editor

When FX3_FSR3_DLL_DIR is set, CMake copies the five Release runtime DLLs beside
each executable under third_party/FSR. The engine searches for that directory
relative to the executable. Keep AMD's original DLL files and licensing notices
intact.

If the DLL directory is absent at runtime, FX3.DX12 continues with native rendering.
The separate spatial upscaler remains a linear-filter fallback; it is not AMD FSR 3
temporal upscaling. FSR 3 dispatch and frame generation are not active yet.
