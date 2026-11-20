# FX3.DX12

Standalone engine and renderer. Game-specific gameplay, levels and assets belong in a separate game repository.

## 0.3.1 — PySide6 editor prototype

- Scene and Inspector panels
- OpenGL-based 3D viewport for Wavefront OBJ files
- OBJ polygon triangulation and negative-index support
- Engine executable selection and process launch (F5)
- Initial AA, FSR 3 and path-tracing settings UI

Important: OBJ preview currently uses OpenGL inside the editor. It is not D3D12/DXR. Path tracing and FSR 3 controls are placeholders until native renderer integration is implemented.

## Run the editor

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r editor\requirements.txt
python editor\main.py
```

Default engine path: `C:/XWe0.2/bulid/main.exe`. Change it in File > Set engine executable.

See docs/0.3.1-EDITOR.md for architecture and limitations.
