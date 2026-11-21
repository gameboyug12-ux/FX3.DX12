// DXR 1.0 path-tracing pipeline library.
// BLAS/TLAS, output UAV, shader table and DispatchRays are not wired in 0.3.3.
struct Payload { float3 radiance; uint hit; };
RaytracingAccelerationStructure Scene : register(t0);
[shader("raygeneration")]
void RayGen() {
 Payload payload; payload.radiance=float3(0,0,0); payload.hit=0;
 RayDesc ray; ray.Origin=float3(0,0,-5); ray.Direction=normalize(float3(0,0,1));
 ray.TMin=0.001; ray.TMax=10000;
 TraceRay(Scene,RAY_FLAG_NONE,0xFF,0,1,0,ray,payload);
}
[shader("miss")]
void Miss(inout Payload payload) { payload.radiance=float3(0.08,0.12,0.20); payload.hit=0; }
[shader("closesthit")]
void ClosestHit(inout Payload payload,BuiltInTriangleIntersectionAttributes attributes) {
 payload.radiance=float3(0.8,0.8,0.8); payload.hit=1;
}
