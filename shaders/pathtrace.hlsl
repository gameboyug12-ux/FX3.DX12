// DXR 1.0 single-bounce ray-tracing preview.
struct Payload { float4 color; };
RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> Output : register(u0);

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 size = DispatchRaysDimensions().xy;
    float2 uv = (float2(pixel) + 0.5) / float2(size);
    float aspect = float(size.x) / max(1.0, float(size.y));
    RayDesc ray;
    ray.Origin = float3(0.0, 0.0, -4.0);
    ray.Direction = normalize(float3((uv.x * 2.0 - 1.0) * aspect, 1.0 - uv.y * 2.0, 1.5));
    ray.TMin = 0.001; ray.TMax = 10000.0;
    Payload payload; payload.color = float4(0.035, 0.045, 0.065, 1.0);
    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    Output[pixel] = payload.color;
}
[shader("miss")]
void Miss(inout Payload payload) { payload.color = float4(0.035, 0.045, 0.065, 1.0); }
[shader("closesthit")]
void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes) {
    float shade = saturate(0.45 + 0.55 * (1.0 - attributes.barycentrics.y));
    payload.color = float4(float3(0.15, 0.62, 0.95) * shade, 1.0);
}
