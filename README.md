# FX3.DX12

## 0.3.7 — Standalone Qt editor and hardware DXR viewport

- Added a separate native `FX3Editor.exe` application built with Qt 6 Widgets and C++20.
- Added Blender-inspired workspace foundations: central 3D viewport, Scene Outliner, Properties and Console docks.
- Added a native Win32 viewport with a Direct3D 12 swap chain.
- Added a hardware DXR test renderer: triangle BLAS/TLAS, ray-tracing state object, shader binding table, `DispatchRays` into a UAV texture, and presentation through the DXGI swap chain.
- The editor is a separate executable; it does not replace or embed itself in `main.exe`.

See `docs/0.3.7-EDITOR-AND-DXR.md` for build instructions and limitations.

## 0.3.6 — DX12 object model and raytracing API

- Added the eight requested public DX12 objects under `XWe::DX::DX12`, each with its own header, plus an umbrella `DX12.hpp`.
- Added DX12 context, command submission, graphics/compute/stream PSO objects, renderer state, textures, materials, shader bytecode and GBuffer resources.
- Added BLAS/TLAS builders, a DXR state-object API, shader binding table and descriptor-bound `DispatchRays`.

## Earlier releases

- **0.3.5:** Java 22+ Panama FFM bindings, XWe public headers and PSO caching.
- **0.3.4:** Java API specification.
- **0.3.3:** DX12 feature probing, DXR state-object foundation and PSO cache.
- **0.3.2:** PySide6 editor prototype and XWe C ABI.

## Build requirements

- Windows 10/11 x64, Visual Studio 2022 with Desktop C++ workload and Windows SDK.
- CMake 3.24+, DXC available on PATH.
- Qt 6.5+ Widgets for MSVC 2022; set `CMAKE_PREFIX_PATH` to the Qt installation.
- A DXR-capable GPU and current graphics driver to run the ray-traced viewport.

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

With the Visual Studio multi-configuration generator, Release outputs are placed in `build/bin/Release/`: `FX3Editor.exe`, `main.exe` and `xwe_engine.dll`. The editor and engine are separate applications/targets.

## DXR scope

The viewport traces a hard-coded triangle to validate the DXR dispatch-and-present path. The Outliner and transform controls are UI foundations and are not yet connected to editable scene objects or dynamic acceleration-structure rebuilds. This is not yet a full Blender-equivalent editor or a production path tracer. Mesh import, camera navigation, selection, manipulators, material editing, multi-bounce lighting, accumulation, denoising, render graph and FSR integration remain future work.

The Python editor files remain as a legacy prototype; the new native editor is under `editor/cpp`.
