# FX3 v0.4 assets

Place the startup mesh at `model.obj` and its texture at `albedo.fxtex`.

## FXTEX v1 binary layout

All integer fields are little-endian.

| Offset | Type | Description |
|---:|---|---|
| 0 | char[8] | ASCII magic: `FXTEX001` |
| 8 | uint32 | Image width |
| 12 | uint32 | Image height |
| 16 | byte[] | width × height × 4 tightly packed RGBA8 pixels |

No mip chain or compression is used in v0.4. Texture dimensions must be non-zero and payload is limited to 512 MiB.
