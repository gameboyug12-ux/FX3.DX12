# FX3.DX12

## 0.3.8 — First usable scene-editor workflow

FX3.DX12 0.3.8 is an incremental 0.3.x release. It is the first release in this line with a usable basic scene-document workflow; it is not a 0.4, 0.7 or 1.0 milestone.

- Separate native `FX3Editor.exe` built with Qt 6 Widgets and C++20.
- Create, open, save and save-as versioned `.fx3scene` documents. A starter scene is provided at `examples/BasicScene.fx3scene`.
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


---

# FX3 Engine v0.4 — Renderer milestone

This branch layers the v0.4 renderer onto `release/0.3.9`. It intentionally adds no scene system, ECS, editor, or gameplay layer.

- Windows x64 / C++20 / Direct3D 12 renderer with Win32 window, hardware adapter and WARP fallback, swap chain, RTV/DSV, depth testing, command queue/list and fence synchronization.
- Single static OBJ mesh loader with positions, UVs, normals, polygon triangulation, positive/negative indices, and initial clip-space fitting.
- Custom FXTEX v1 binary texture format: little-endian width/height followed by tightly packed RGBA8.
- Textured mesh pipeline, runtime HLSL compilation, sample OBJ/FXTEX generator and PPM-to-FXTEX converter.

## Build and smoke test

```powershell
python tools/make_sample_assets.py
cmake -S . -B build -A x64
cmake --build build --config Debug
```

Run `build/Debug/FX3DX12.exe` from the repository root.

The renderer currently supports one static mesh and one albedo texture. Camera controls, scene graph, ECS, material system, mipmaps, compressed textures, and resize-aware swap-chain recreation are outside v0.4 scope.

**Validation:** a Windows/MSVC/D3D12 build has not been run in this environment; verify on Windows 10/11 with the D3D12 debug layer before merging.


## 0.4.2 — Python PySide6 editor + XWe engine DLL

The 0.4.2 workflow is Python-first. The editor is PySide6; the engine is
exposed as the native `xwe_engine.dll`; a standalone Python ctypes test
program loads that DLL and exercises its public C ABI.

- PySide6 editor: `editor/pyside6/fx3_editor.py`
- Engine DLL C ABI: `include/XWeApplication.h` and `src/xwe_application.cpp`
- DLL-only build helper: `engine/build_engine_dll.ps1`
- Python DLL test: `tests/python/engine_dll_test.py`
- No native C++ editor executable or standalone renderer is part of this
  release workflow. The build helper sets `FX3_BUILD_RENDERER=OFF` and
  `XWE_BUILD_ENGINE=ON`.

Build the DLL from a Visual Studio 2022 Developer PowerShell:

```powershell
.\engine\build_engine_dll.ps1 -Configuration Release
```

Run a DLL load/create/destroy smoke test:

```powershell
py -3 tests/python/engine_dll_test.py --smoke
```

Launch the native engine window through the DLL:

```powershell
py -3 tests/python/engine_dll_test.py --run
```

Install and start the editor:

```powershell
py -3 -m pip install -r editor/pyside6/requirements.txt
py -3 editor/pyside6/fx3_editor.py
```

The editor's **Engine → Run Engine DLL Test** command launches the Python
test program in `--run` mode. The engine DLL currently exposes an application
lifecycle API; it creates and runs its own Win32/D3D12 window rather than
rendering into the PySide6 viewport. The PySide6 viewport remains a scene
document preview. Build and runtime validation require Windows x64 and have
not been performed in this environment.

See `docs/0.4.2-python-editor-and-engine-dll.md` for details.
