# Hybrid DXR descriptor and buffer contract

`DX12RaytracingApi::CreatePipeline` creates two root descriptor tables. The GPU handles
passed to `DX12RaytracingDispatch::Dispatch` point to the first entry of each table.
All descriptors in each table must be contiguous in the same shader-visible
CBV/SRV/UAV heap.

## SRV table — five descriptors

| Register | Resource | Required layout |
|---|---|---|
| t0 | TLAS | Raytracing acceleration-structure SRV |
| t1 | Vertex data | StructuredBuffer of `RTVertex`, 48-byte stride |
| t2 | Indices | StructuredBuffer of 32-bit unsigned indices |
| t3 | Geometry table | StructuredBuffer of `RTGeometryInfo`, 16-byte stride |
| t4 | Materials | StructuredBuffer of `RTMaterial`, 48-byte stride |

## UAV table — one descriptor

| Register | Resource | Required format |
|---|---|---|
| u0 | Output | 2D UAV texture, preferably R16G16B16A16_FLOAT or R32G32B32A32_FLOAT |

## Instance and geometry indexing

Set `D3D12_RAYTRACING_INSTANCE_DESC::InstanceID` to the scene instance slot.
The shader reads `GeometryTable[InstanceID * 16 + GeometryIndex()]`; reserve 16
geometry metadata entries per instance. Each record stores a base vertex offset,
first index and material index. The index buffer uses 32-bit triangle-list indices.
Vertex positions are object-space and are transformed by `ObjectToWorld3x4()` when
the closest-hit shader reconstructs the geometric world normal.

The matching C++ structures and compile-time size assertions are in
`include/XWe/DX/DX12/DX12RaytracingSceneData.hpp`.

## Dispatch and lifetime

- The SRV and UAV handles must belong to the shader-visible heap passed to Dispatch.
- The SBT identifiers must be obtained from the hybrid pipeline state object's
  `ID3D12StateObjectProperties`.
- Build BLAS/TLAS before dispatch and keep all geometry, material, descriptor and
  output resources alive until the queue fence for that dispatch has completed.
- The shader performs four spatial samples and up to three path bounces. It does not
  perform temporal accumulation or denoising.
