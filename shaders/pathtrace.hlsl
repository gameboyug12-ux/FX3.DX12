// DXR 1.0 minimal ray-generation pipeline with UAV output.
struct Payload { float3 radiance; uint hit; };
RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> Output : register(u0);

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dimensions = DispatchRaysDimensions().xy;
    Payload payload;
    payload.radiance = float3(0, 0, 0);
    payload.hit = 0;
    RayDesc ray;
    ray.Origin = float3(0, 0, -5);
    ray.Direction = normalize(float3(
        (float(pixel.x) / max(1.0, float(dimensions.x)) - 0.5) * 2.0,
        (0.5 - float(pixel.y) / max(1.0, float(dimensions.y))) * 2.0,
        1.0));
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    Output[pixel] = float4(payload.radiance, 1.0);
}
[shader("miss")]
void Miss(inout Payload payload) {
    payload.radiance = float3(0.08, 0.12, 0.20);
    payload.hit = 0;
}
[shader("closesthit")]
void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes) {
    payload.radiance = float3(0.8, 0.8, 0.8);
    payload.hit = 1;
}
