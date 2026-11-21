# FX3.DX12

## 0.3.6 — DX12 object model and raytracing API

- Added the eight requested public DX12 objects under `XWe::DX::DX12`, each
  with its own header, plus an umbrella `DX12.hpp`.
- Added DX12 context, fence-safe single-list submission, graphics/compute/
  stream PSO object, renderer state, renderer API, textures, material,
  shader bytecode and GBuffer resources/views/transitions.
- Added a starter DXR API: BLAS/TLAS builders, raytracing state object,
  aligned shader binding table and descriptor-bound `DispatchRays`.
- Updated the path-tracing shader to write its result to a UAV texture.
- Bumped CMake and native DLL version to 0.3.6.

See `docs/0.3.6-DX12-OBJECT-MODEL-AND-RT-API.md` for object responsibilities,
RT workflow, ownership rules and current limitations. This is an API and
implementation foundation; the existing sample renderer does not yet execute
the new DXR dispatch path automatically.

## 0.3.5 — XWe standard API, Java Panama and PSO foundation

- Implemented Java 22+ Panama FFM bindings for `xwe_engine.dll` (no JNI).
- Added `XWeApplication`, `XWeApplicationDesc`, `XWeVersion` and `XWeException`.
- Kept the UTF-16 window title Arena alive for the complete native application lifetime.
- Migrated engine C++ namespace from `fx3` to `XWe`; DX12 APIs use `XWe::DX12`.
- Added canonical public headers under `include/XWe`.
- Added graphics, compute and pipeline-state-stream PSO caching through
  `XWe::DX12::PipelineStateCache`.
- Preserved legacy `include/fx3` header paths as forwarding headers only.

FX3.DX12 is a standalone Windows engine and Direct3D 12 renderer. Game-specific gameplay, levels and assets belong in separate game projects.

## 0.3.4 — XWe Java API specification

- Added `src/xwe/java/api/java.txt` with the Java-facing API contract.
- Defines Java 22+ Foreign Function & Memory (Panama) integration with the existing XWe C ABI.
- Bumped CMake project and exported `XWeGetVersion()` to 0.3.4.
- Java classes and native downcalls are specified, not yet implemented.

## 0.3.3 — DX12 Ultimate, DXR/PT foundation and PSO cache

- DX12 optional feature probing (DXR, mesh shaders, sampler feedback, enhanced barriers, VRS)
- DXC ray-tracing library build (`lib_6_3`) and native DXR state-object creation when supported
- Reusable keyed graphics PSO cache
- Raster fallback remains active; full path-tracing dispatch is not yet implemented

## 0.3.2 — Fullscreen editor and XWe application DLL

- Fullscreen PySide6 editor (F11 toggles fullscreen)
- C++17-oriented source editor with syntax highlighting, tabs, open and save
- OBJ preview and Object Inspector with position, rotation and scale controls
- Engine executable launcher (F5)
- Public C ABI in `include/XWeApplication.h`
- Native engine DLL target: `xwe_engine.dll`
- Executable output remains named `main.exe`

## Run the editor

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r editor\requirements.txt
python editor\main.py
```

The default executable is `C:/XWe0.2/bulid/main.exe`; choose the current build using File > Set engine executable.

## Build the engine DLL and executable

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

The build produces `main.exe` and `xwe_engine.dll` in the build output directory. Keep the DLL beside the executable.

## XWe DLL API

Include `XWeApplication.h`, link against `xwe_engine.lib`, and deploy `xwe_engine.dll` beside the game executable. The C ABI exports create, run, destroy and version functions. See `docs/0.3.2-EDITOR-AND-DLL.md`. The Java implementation is in `src/xwe/java/api`; see `docs/0.3.5-JAVA-PANAMA-AND-PSO-API.md`.

## Rendering status

The 0.3.2 UI defines AA, FSR 3.1 upscaling and frame-generation options, but these are not active renderer features yet. The 0.3.3 runtime probes optional DX12 features and creates a DXR state object on capable adapters. Version 0.3.6 adds public APIs for BLAS/TLAS builds, shader binding tables, UAV output and DispatchRays, but the existing sample cube renderer does not automatically execute that path. The current editor viewport remains an OpenGL preview. Native FSR 3.1 still requires the AMD FidelityFX SDK and full D3D12 integration. Full multi-bounce path tracing, accumulation, denoising and presentation are future work.
