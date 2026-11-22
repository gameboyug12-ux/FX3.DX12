// FX3 hybrid DXR path tracer.
// Multi-bounce transport with direct shadow rays, rough/metallic reflections and diffuse GI.
// SRV table: t0 TLAS, t1 RTVertex[], t2 uint32 indices[], t3 RTGeometryInfo[], t4 RTMaterial[].
// UAV table: u0 output RGBA16F/RGBA32F texture.

struct RTVertex { float3 position; float3 normal; float2 uv; float4 color; };
struct RTGeometryInfo { uint vertexOffset; uint firstIndex; uint materialIndex; uint flags; };
struct RTMaterial { float4 baseColor; float4 emission; float metallic; float roughness; float2 padding; };

struct Payload {
    float3 radiance;
    float hitT;
    float3 normal;
    float3 albedo;
    float roughness;
    float metallic;
    float3 emission;
    uint hit;
    uint mode;
    uint seed;
};

RaytracingAccelerationStructure Scene : register(t0);
StructuredBuffer<RTVertex> Vertices : register(t1);
StructuredBuffer<uint> Indices : register(t2);
StructuredBuffer<RTGeometryInfo> GeometryTable : register(t3);
StructuredBuffer<RTMaterial> Materials : register(t4);
RWTexture2D<float4> Output : register(u0);

static const uint kMaxGeometriesPerInstance = 16;
static const uint kSamplesPerPixel = 4;
static const uint kMaxBounces = 3;
static const float kRayEpsilon = 0.0015;
static const float kPi = 3.14159265358979323846;

Payload MakePayload(uint mode, uint seed) {
    Payload p;
    p.radiance = float3(0, 0, 0); p.hitT = 0;
    p.normal = float3(0, 0, 0); p.albedo = float3(0, 0, 0);
    p.roughness = 1; p.metallic = 0; p.emission = float3(0, 0, 0);
    p.hit = 0; p.mode = mode; p.seed = seed;
    return p;
}

uint Hash(uint x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15;
    x *= 0x846ca68b; x ^= x >> 16; return x;
}
float Random01(inout uint state) {
    state = Hash(state + 0x9e3779b9);
    return (state & 0x00ffffff) / 16777216.0;
}
float3 Sky(float3 d) {
    float t = saturate(d.y * 0.5 + 0.5);
    return lerp(float3(0.035, 0.045, 0.065), float3(0.30, 0.48, 0.72), t);
}
float3 RandomUnitVector(inout uint seed) {
    float z = Random01(seed) * 2.0 - 1.0;
    float phi = Random01(seed) * 2.0 * kPi;
    float r = sqrt(max(0.0, 1.0 - z * z));
    return float3(r * cos(phi), z, r * sin(phi));
}
float3 CosineHemisphere(float3 n, inout uint seed) {
    float u1 = Random01(seed), u2 = Random01(seed);
    float phi = 2.0 * kPi * u1;
    float r = sqrt(u2);
    float3 local = float3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u2)));
    float3 helper = abs(n.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(helper, n));
    float3 bitangent = cross(n, tangent);
    return normalize(tangent * local.x + bitangent * local.y + n * local.z);
}

[shader("raygeneration")]
void RayGen() {
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dimensions = DispatchRaysDimensions().xy;
    float aspect = float(dimensions.x) / max(1.0, float(dimensions.y));
    uint baseSeed = Hash(pixel.x + pixel.y * dimensions.x + 0x1234abcd);
    float3 sum = float3(0, 0, 0);

    [loop]
    for (uint sampleIndex = 0; sampleIndex < kSamplesPerPixel; ++sampleIndex) {
        uint seed = Hash(baseSeed + sampleIndex * 0x68bc21eb);
        float2 jitter = float2(Random01(seed), Random01(seed)) - 0.5;
        float2 uv = (float2(pixel) + 0.5 + jitter) / max(float2(dimensions), float2(1, 1));

        RayDesc ray;
        ray.Origin = float3(0, 0, -4);
        ray.Direction = normalize(float3((uv.x * 2 - 1) * aspect, 1 - uv.y * 2, 1.5));
        ray.TMin = kRayEpsilon; ray.TMax = 10000;

        float3 radiance = float3(0, 0, 0);
        float3 throughput = float3(1, 1, 1);
        [loop]
        for (uint bounce = 0; bounce < kMaxBounces; ++bounce) {
            Payload surface = MakePayload(0, seed);
            TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, surface);
            if (surface.hit == 0) {
                radiance += throughput * Sky(ray.Direction);
                break;
            }

            float3 hitPosition = ray.Origin + ray.Direction * surface.hitT;
            float3 n = normalize(surface.normal);
            radiance += throughput * surface.emission;

            // Hard direct-light visibility via a secondary shadow ray.
            float3 lightDirection = normalize(float3(-0.45, 0.82, -0.34));
            float nDotL = saturate(dot(n, lightDirection));
            if (nDotL > 0) {
                RayDesc shadowRay;
                shadowRay.Origin = hitPosition + n * (kRayEpsilon * 2);
                shadowRay.Direction = lightDirection;
                shadowRay.TMin = kRayEpsilon; shadowRay.TMax = 10000;
                Payload visibility = MakePayload(1, seed);
                TraceRay(Scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 1, 0, shadowRay, visibility);
                if (visibility.hit == 0) {
                    radiance += throughput * surface.albedo * float3(3.5, 3.25, 2.9) * (nDotL / kPi);
                }
            }

            // Metallic/dielectric reflection lobe mixed with cosine-weighted GI.
            float3 viewDirection = normalize(-ray.Direction);
            float3 f0 = lerp(float3(0.04, 0.04, 0.04), surface.albedo, saturate(surface.metallic));
            float fresnelTerm = pow(1 - saturate(dot(viewDirection, n)), 5);
            float3 fresnel = f0 + (float3(1, 1, 1) - f0) * fresnelTerm;
            float specularProbability = saturate(max(fresnel.r, max(fresnel.g, fresnel.b)));
            float3 diffuseDirection = CosineHemisphere(n, seed);
            float3 perfectReflection = reflect(ray.Direction, n);
            float3 roughReflection = normalize(perfectReflection + RandomUnitVector(seed) * saturate(surface.roughness) * 0.45);
            float chooseSpecular = Random01(seed);
            float3 nextDirection = normalize(chooseSpecular < specularProbability ? roughReflection : diffuseDirection);

            throughput *= chooseSpecular < specularProbability
                ? fresnel / max(specularProbability, 0.05)
                : surface.albedo * (1 - saturate(surface.metallic)) / max(1 - specularProbability, 0.05);
            if (bounce >= 1) {
                float survival = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.08, 0.95);
                if (Random01(seed) > survival) break;
                throughput /= survival;
            }
            ray.Origin = hitPosition + n * (kRayEpsilon * 2);
            ray.Direction = nextDirection;
            ray.TMin = kRayEpsilon; ray.TMax = 10000;
        }
        sum += radiance;
    }

    float3 color = sum / float(kSamplesPerPixel);
    color = color / (float3(1, 1, 1) + color);
    color = pow(saturate(color), 1.0 / 2.2);
    Output[pixel] = float4(color, 1);
}

[shader("miss")]
void Miss(inout Payload payload) {
    payload.hit = 0;
    payload.radiance = Sky(WorldRayDirection());
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, BuiltInTriangleIntersectionAttributes attributes) {
    payload.hit = 1;
    payload.hitT = RayTCurrent();
    if (payload.mode == 1) return; // shadow rays only need the hit bit

    uint geometryCount = 0, geometryStride = 0;
    GeometryTable.GetDimensions(geometryCount, geometryStride);
    uint geometrySlot = InstanceID() * kMaxGeometriesPerInstance + GeometryIndex();
    if (geometrySlot >= geometryCount) {
        payload.normal = -normalize(WorldRayDirection());
        payload.albedo = float3(0.72, 0.74, 0.78);
        payload.roughness = 0.65; payload.metallic = 0;
        payload.emission = float3(0, 0, 0);
        return;
    }

    RTGeometryInfo geometry = GeometryTable[geometrySlot];
    uint indexCount = 0, indexStride = 0;
    Indices.GetDimensions(indexCount, indexStride);
    uint first = geometry.firstIndex + PrimitiveIndex() * 3;
    if (first + 2 >= indexCount) {
        payload.normal = -normalize(WorldRayDirection());
        payload.albedo = float3(0.72, 0.74, 0.78);
        payload.roughness = 0.65; payload.metallic = 0;
        payload.emission = float3(0, 0, 0);
        return;
    }

    uint i0 = geometry.vertexOffset + Indices[first];
    uint i1 = geometry.vertexOffset + Indices[first + 1];
    uint i2 = geometry.vertexOffset + Indices[first + 2];
    uint vertexCount = 0, vertexStride = 0;
    Vertices.GetDimensions(vertexCount, vertexStride);
    if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
        payload.normal = -normalize(WorldRayDirection());
        payload.albedo = float3(0.72, 0.74, 0.78);
        payload.roughness = 0.65; payload.metallic = 0;
        payload.emission = float3(0, 0, 0);
        return;
    }

    RTVertex v0 = Vertices[i0], v1 = Vertices[i1], v2 = Vertices[i2];
    float3 p0 = mul(float4(v0.position, 1), ObjectToWorld3x4());
    float3 p1 = mul(float4(v1.position, 1), ObjectToWorld3x4());
    float3 p2 = mul(float4(v2.position, 1), ObjectToWorld3x4());
    float3 geometricNormal = normalize(cross(p1 - p0, p2 - p0));
    if (dot(geometricNormal, WorldRayDirection()) > 0) geometricNormal = -geometricNormal;
    payload.normal = geometricNormal;

    float3 bary = float3(1 - attributes.barycentrics.x - attributes.barycentrics.y,
                         attributes.barycentrics.x, attributes.barycentrics.y);
    float3 vertexColor = v0.color.rgb * bary.x + v1.color.rgb * bary.y + v2.color.rgb * bary.z;
    uint materialCount = 0, materialStride = 0;
    Materials.GetDimensions(materialCount, materialStride);
    if (geometry.materialIndex < materialCount) {
        RTMaterial material = Materials[geometry.materialIndex];
        payload.albedo = saturate(material.baseColor.rgb * vertexColor);
        payload.emission = max(material.emission.rgb, float3(0, 0, 0));
        payload.metallic = saturate(material.metallic);
        payload.roughness = clamp(material.roughness, 0.025, 1.0);
    } else {
        payload.albedo = saturate(vertexColor);
        payload.emission = float3(0, 0, 0);
        payload.metallic = 0;
        payload.roughness = 0.65;
    }
}
