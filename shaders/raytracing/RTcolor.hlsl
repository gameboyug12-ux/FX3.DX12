// Dinamički TLAS koji se ažurira kako se svet strimuje oko igrača
RaytracingAccelerationStructure WorldTLAS : register(t0);

// G-Buffer inputi
Texture2D<float>  GBufferDepth  : register(t1);
Texture2D<float4> GBufferNormal : register(t2); // xyz = World Normal, w = Roughness

// Bindless niz sa podacima o trenutno učitanim objektima/materijalima u svetu
struct InstanceData {
    float4 baseColor;
    uint   albedoTexIndex;
    uint   vertexBufferIndex;
    uint   indexBufferIndex;
    uint   pad;
};
StructuredBuffer<InstanceData> ActiveInstances : register(t3);

// Izlaz: RGB = Refleksija, A = Senka (0 = senka, 1 = svetlo)
RWTexture2D<float4> OutRTColor : register(u0);

cbuffer FrameCB : register(b0)
{
    float4x4 InvViewProj;
    float3   CameraPos;
    float    MaxRTDistance; // Npr. 200.0f za Open World optimizaciju
    float3   SunDirection;  // Pravac ka suncu (normalizovan)
    float    Pad;
};

float3 ReconstructWorldPos(uint2 pix, float depth, uint2 dim)
{
    float2 uv = (float2(pix) + 0.5f) / float2(dim);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 worldPos = mul(float4(ndc, depth, 1.0f), InvViewProj);
    return worldPos.xyz / worldPos.w;
}

void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width, height;
    OutRTColor.GetDimensions(width, height);
    uint2 pix = dispatchThreadID.xy;

    if (pix.x >= width || pix.y >= height)
        return;

    float depth = GBufferDepth[pix];
    if (depth >= 1.0f) // Nebo - preskačemo
    {
        OutRTColor[pix] = float4(0, 0, 0, 1);
        return;
    }

    float4 normRough = GBufferNormal[pix];
    float3 N = normalize(normRough.xyz);
    float roughness = normRough.w;

    float3 worldPos = ReconstructWorldPos(pix, depth, uint2(width, height));
    float distToCam = length(worldPos - CameraPos);

    // Open World LOD: Ako je piksel predaleko od kamere, ne radimo RT
    if (distToCam > MaxRTDistance)
    {
        OutRTColor[pix] = float4(0, 0, 0, 1);
        return;
    }

    // ---------------------------------------------------------
    // 1. RT SENKE (Shadow Ray ka Suncu)
    // ---------------------------------------------------------
    float shadowFactor = 1.0f;
    float NdotL = dot(N, SunDirection);

    // Ispaljujemo zrak samo ako je površina okrenuta ka suncu (backface culling ušteda)
    if (NdotL > 0.0f)
    {
        RayDesc shadowRay;
        shadowRay.Origin = worldPos + N * 0.02f; // Malo veći bias za velike open-world koordinate
        shadowRay.Direction = SunDirection;
        shadowRay.TMin = 0.01f;
        shadowRay.TMax = MaxRTDistance;

        // RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH staje čim udari u bilo šta (najbrže za senke!)
        RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadowQuery;
        shadowQuery.TraceRayInline(WorldTLAS, RAY_FLAG_NONE, 0xFF, shadowRay);
        shadowQuery.Proceed();

        if (shadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            shadowFactor = 0.0f; // U senci je
        }
    }

    // ---------------------------------------------------------
    // 2. RT REFLEKSIJE (Reflection Ray)
    // ---------------------------------------------------------
    float3 reflColor = float3(0, 0, 0);

    // Preskačemo refleksije na veoma hrapavim površinama (trava, zemlja, beton)
    if (roughness < 0.5f)
    {
        float3 viewDir = normalize(worldPos - CameraPos);
        RayDesc reflRay;
        reflRay.Origin = worldPos + N * 0.02f;
        reflRay.Direction = reflect(viewDir, N);
        reflRay.TMin = 0.01f;
        reflRay.TMax = MaxRTDistance;

        RayQuery<RAY_FLAG_FORCE_OPAQUE> reflQuery;
        reflQuery.TraceRayInline(WorldTLAS, RAY_FLAG_NONE, 0xFF, reflRay);
        reflQuery.Proceed();

        if (reflQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            // Pomoću InstanceID direktno čitamo materijal strimovanog objekta
            uint instID = reflQuery.CommittedInstanceID();
            InstanceData hitObj = ActiveInstances[instID];
            
            reflColor = hitObj.baseColor.rgb; 
            // Ovde po želji semplujete teksturu iz bindless heap-a (ResourceDescriptorHeap[hitObj.albedoTexIndex])
        }
        else
        {
            // Boja neba / atmosfere u daljini
            reflColor = float3(0.45f, 0.65f, 0.9f);
        }
    }

    // Upisujemo refleksiju u RGB, a senku u A kanal
    OutRTColor[pix] = float4(reflColor, shadowFactor);
}
