# FX3.DX12

## 0.3.8 — First usable scene-editor workflow

FX3.DX12 0.3.8 is an incremental 0.3.x release. It is the first release in this line with a usable basic scene-document workflow; it is not a 0.4, 0.7 or 1.0 milestone.

- Separate native `FX3Editor.exe` built with Qt 6 Widgets and C++20.
- Create, open, save and save-as versioned `.fx3scene` documents.
- Scene Outliner supports object selection, adding triangle entries and deleting selected entries.
- Properties panel edits object names and location/rotation/scale values.
- Unsaved-change prompts, dirty title marker and editor console.
- Scene writes use Qt `QSaveFile`; loading validates the schema before replacing the active scene.
- Retains the existing Direct3D 12 engine executable and the 0.3.7 DXR viewport validation path.

See `docs/0.3.8-FIRST-USABLE-EDITOR.md` for the exact feature scope, scene format and limitations.

## 0.3.7 — Standalone Qt editor and hardware DXR viewport

- Added separate `FX3Editor.exe`, native Win32 viewport, Outliner, Properties and Console docks.
- Added a DXR validation renderer with triangle BLAS/TLAS, ray-tracing state object, shader binding table and `DispatchRays`.

## Earlier releases

- **0.3.6:** DX12 object model and raytracing API foundation.
- **0.3.5:** Java 22+ Panama FFM bindings, XWe public headers and PSO caching.
- **0.3.4:** Java API specification.
- **0.3.3:** DX12 feature probing, DXR state-object foundation and PSO cache.
- **0.3.2:** PySide6 editor prototype and XWe C ABI.

## Build requirements

- Windows 10/11 x64, Visual Studio 2022 with Desktop C++ workload and Windows SDK.
- CMake 3.24+, DirectX Shader Compiler (`dxc.exe`) on PATH.
- Qt 6.5+ Widgets for MSVC 2022 x64; set `CMAKE_PREFIX_PATH` to the Qt installation.
- A DXR-capable GPU and current driver for the ray-traced viewport.

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

Release outputs are placed under `build/bin/Release/`: `FX3Editor.exe`, `main.exe`, `xwe_engine.dll` and compiled shaders.

## Known limitations

Scene objects and transforms are persisted in `.fx3scene`, but the DXR viewport still renders the fixed validation triangle. Scene entries are not yet converted into GPU mesh instances; transform edits do not rebuild BLAS/TLAS. Mesh import, camera controls, dynamic rendering, materials, multi-bounce path tracing, accumulation, denoising and FSR remain future work. The editor/renderer has not been declared production-ready.
