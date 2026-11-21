# FX3.DX12

FX3.DX12 is a standalone Windows engine and Direct3D 12 renderer. Game-specific gameplay, levels and assets belong in separate game projects.

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

Include `XWeApplication.h`, link against `xwe_engine.lib`, and deploy `xwe_engine.dll` beside the game executable. The C ABI exports create, run, destroy and version functions. See `docs/0.3.2-EDITOR-AND-DLL.md`.

## Rendering status

The 0.3.2 UI defines AA, FSR 3.1 upscaling and frame-generation options, but these are not active renderer features yet. The 0.3.3 runtime probes optional DX12 features and compiles a DXR shader library; it still does not build acceleration structures or dispatch path-tracing rays. The current viewport is an editor-side OpenGL preview. Native FSR 3.1 requires the AMD FidelityFX SDK, D3D12 render integration, depth, motion vectors, jitter and frame pacing. Full DXR path tracing is not yet completed in this milestone.
