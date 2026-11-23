# DX12 hybrid ray-tracing descriptor and buffer contract

`DX12RaytracingApi::CreatePipeline(path, srvCount)` creates a contiguous SRV table
at root parameter 0 and a one-entry UAV table at root parameter 1. Dispatch handles
point to the first descriptor in each table. All entries must be contiguous in the
same shader-visible CBV/SRV/UAV heap.

## Standalone path tracer — five SRVs

Use `CreatePipeline(path, 5)` for `hybrid_pathtrace.dxil`:

| Register | Resource | Layout |
|---|---|---|
| t0 | TLAS | Raytracing acceleration-structure SRV |
| t1 | Vertices | `StructuredBuffer<RTVertex>`, 48-byte stride |
| t2 | Indices | `StructuredBuffer<uint>`, 32-bit triangle-list indices |
| t3 | Geometry table | `StructuredBuffer<RTGeometryInfo>`, 16-byte stride |
| t4 | Materials | `StructuredBuffer<RTMaterial>`, 48-byte stride |

## Raster-primary deferred hybrid — twelve SRVs

Use `CreatePipeline(path, 12)` for `hybrid_deferred.dxil`:

| Register | Resource | Layout |
|---|---|---|
| t0 | TLAS | Raytracing acceleration-structure SRV |
| t1 | G-buffer Albedo | `DX12GBufferSlot::Albedo`, RGBA8 |
| t2 | G-buffer Normal | `DX12GBufferSlot::Normal`, RGBA16F |
| t3 | G-buffer Material | `DX12GBufferSlot::Material`, RGBA8; R=metallic, G=AO, B=roughness |
| t4 | G-buffer Velocity | `DX12GBufferSlot::Velocity`, RG16F (reserved) |
| t5 | G-buffer Emissive | `DX12GBufferSlot::Emissive`, RGBA16F |
| t6 | Depth | `DX12GBuffer::Depth()` SRV, R32_FLOAT |
| t7 | Materials | `StructuredBuffer<RTMaterial>`, 48-byte stride |
| t8 | Vertices | `StructuredBuffer<RTVertex>`, 48-byte stride |
| t9 | Indices | `StructuredBuffer<uint>`, 32-bit triangle-list indices |
| t10 | Geometry table | `StructuredBuffer<RTGeometryInfo>`, 16-byte stride |
| t11 | Frame data | One `StructuredBuffer<RTFrameData>`, 128-byte stride |

## UAV table

| Register | Resource | Format |
|---|---|---|
| u0 | Output | 2D UAV texture, preferably R16G16B16A16_FLOAT or R32G32B32A32_FLOAT |

## Scene and frame layout

Set `D3D12_RAYTRACING_INSTANCE_DESC::InstanceID` to the scene instance slot. The
shader reads `GeometryTable[InstanceID * 16 + GeometryIndex()]`; reserve 16 metadata
slots per instance. Each geometry record stores base vertex offset, first index and
material index. Positions are object-space and transformed by `ObjectToWorld3x4()`.

`RTFrameData` contains a row-major inverse view-projection matrix, camera position,
exposure, directional-light vector/intensity/color, GI intensity and background color.
The deferred shader reconstructs world position from G-buffer depth. The CPU structures
in `DX12RaytracingSceneData.hpp` have compile-time size assertions.

## Resource lifetime

Build BLAS/TLAS and transition resources before dispatch. Keep geometry, material,
descriptor, frame-data and output resources alive until the command-queue fence for
the dispatch has completed. The path tracer uses four spatial samples and up to three
bounces. The deferred pass traces shadow, reflection and diffuse-GI rays per rasterized
pixel. Neither pass performs temporal accumulation or denoising.
