// Raster-primary / DXR-secondary deferred hybrid pass.
// t0 TLAS; t1 Albedo; t2 Normal; t3 Material; t4 Velocity; t5 Emissive;
// t6 Depth; t7 RTMaterial[]; t8 RTVertex[]; t9 uint32 indices[];
// t10 RTGeometryInfo[]; t11 RTFrameData[1]. UAV u0 Output.
struct RTVertex { float3 position; float3 normal; float2 uv; float4 color; };
struct RTGeometryInfo { uint vertexOffset; uint firstIndex; uint materialIndex; uint flags; };
struct RTMaterial { float4 baseColor; float4 emission; float metallic; float roughness; float2 padding; };
struct RTFrameData {
    row_major float4x4 inverseViewProjection;
    float3 cameraPosition; float exposure;
    float3 sunDirection; float sunIntensity;
    float3 sunColor; float giIntensity;
    float3 backgroundColor; float padding;
};
struct Payload {
    float3 radiance; float hitT; float3 normal; float3 albedo;
    float roughness; float metallic; float3 emission; uint hit; uint mode; uint seed;
};
RaytracingAccelerationStructure Scene : register(t0);
Texture2D<float4> GBufferAlbedo : register(t1);
Texture2D<float4> GBufferNormal : register(t2);
Texture2D<float4> GBufferMaterial : register(t3);
Texture2D<float2> GBufferVelocity : register(t4);
Texture2D<float4> GBufferEmissive : register(t5);
Texture2D<float> GBufferDepth : register(t6);
StructuredBuffer<RTMaterial> Materials : register(t7);
StructuredBuffer<RTVertex> Vertices : register(t8);
StructuredBuffer<uint> Indices : register(t9);
StructuredBuffer<RTGeometryInfo> GeometryTable : register(t10);
StructuredBuffer<RTFrameData> FrameData : register(t11);
RWTexture2D<float4> Output : register(u0);
static const uint kMaxGeometriesPerInstance=16;
static const float kRayEpsilon=0.0015;
static const float kPi=3.14159265358979323846;

Payload MakePayload(uint mode,uint seed) {
    Payload p; p.radiance=float3(0,0,0); p.hitT=0; p.normal=float3(0,0,0);
    p.albedo=float3(0,0,0); p.roughness=1; p.metallic=0; p.emission=float3(0,0,0);
    p.hit=0; p.mode=mode; p.seed=seed; return p;
}
uint Hash(uint x) { x^=x>>16; x*=0x7feb352d; x^=x>>15; x*=0x846ca68b; x^=x>>16; return x; }
float Random01(inout uint s) { s=Hash(s+0x9e3779b9); return (s&0x00ffffff)/16777216.0; }
float3 RandomUnitVector(inout uint s) {
    float z=Random01(s)*2-1,phi=Random01(s)*2*kPi,r=sqrt(max(0,1-z*z));
    return float3(r*cos(phi),z,r*sin(phi));
}
float3 CosineHemisphere(float3 n,inout uint s) {
    float u1=Random01(s),u2=Random01(s),phi=2*kPi*u1,r=sqrt(u2);
    float3 local=float3(r*cos(phi),r*sin(phi),sqrt(max(0,1-u2)));
    float3 helper=abs(n.y)<0.999?float3(0,1,0):float3(1,0,0);
    float3 tangent=normalize(cross(helper,n)),bitangent=cross(n,tangent);
    return normalize(tangent*local.x+bitangent*local.y+n*local.z);
}

[shader("raygeneration")]
void RayGen() {
    uint2 pixel=DispatchRaysIndex().xy,dimensions=DispatchRaysDimensions().xy;
    RTFrameData frame=FrameData[0];
    float depth=GBufferDepth.Load(int3(pixel,0));
    if(depth>=0.999999) {
        float3 bg=frame.backgroundColor*0.5+float3(0.15,0.24,0.36);
        Output[pixel]=float4(bg,1); return;
    }
    float3 albedo=saturate(GBufferAlbedo.Load(int3(pixel,0)).rgb);
    float3 normal=normalize(GBufferNormal.Load(int3(pixel,0)).xyz);
    float4 mat=GBufferMaterial.Load(int3(pixel,0));
    float3 emissive=GBufferEmissive.Load(int3(pixel,0)).rgb;
    // Material target: R=metallic, G=ambient occlusion, B=roughness.
    float metallic=saturate(mat.r),ao=saturate(mat.g),roughness=clamp(mat.b,0.025,1.0);
    float2 uv=(float2(pixel)+0.5)/max(float2(dimensions),float2(1,1));
    float2 ndc=uv*2-1; ndc.y=-ndc.y;
    float4 worldH=mul(float4(ndc,depth,1),frame.inverseViewProjection);
    float worldW=abs(worldH.w)>1e-6?worldH.w:1e-6;
    float3 worldPosition=worldH.xyz/worldW;
    float3 viewDirection=normalize(frame.cameraPosition-worldPosition);
    uint seed=Hash(pixel.x+pixel.y*dimensions.x+0x51ed270b);

    float3 lightDirection=normalize(frame.sunDirection);
    float nDotL=saturate(dot(normal,lightDirection));
    float3 direct=float3(0,0,0);
    if(nDotL>0) {
        RayDesc shadowRay; shadowRay.Origin=worldPosition+normal*(kRayEpsilon*2);
        shadowRay.Direction=lightDirection; shadowRay.TMin=kRayEpsilon; shadowRay.TMax=10000;
        Payload shadow=MakePayload(1,seed);
        TraceRay(Scene,RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,0xFF,0,1,0,shadowRay,shadow);
        if(shadow.hit==0) direct=albedo*frame.sunColor*frame.sunIntensity*(nDotL/kPi);
    }

    float3 reflectionDirection=normalize(reflect(-viewDirection,normal)+RandomUnitVector(seed)*roughness*0.35);
    RayDesc reflectionRay; reflectionRay.Origin=worldPosition+normal*(kRayEpsilon*2);
    reflectionRay.Direction=reflectionDirection; reflectionRay.TMin=kRayEpsilon; reflectionRay.TMax=10000;
    Payload reflection=MakePayload(2,seed);
    TraceRay(Scene,RAY_FLAG_NONE,0xFF,0,1,0,reflectionRay,reflection);

    RayDesc giRay; giRay.Origin=worldPosition+normal*(kRayEpsilon*2);
    giRay.Direction=CosineHemisphere(normal,seed); giRay.TMin=kRayEpsilon; giRay.TMax=10000;
    Payload gi=MakePayload(2,seed);
    TraceRay(Scene,RAY_FLAG_NONE,0xFF,0,1,0,giRay,gi);

    float3 f0=lerp(float3(0.04,0.04,0.04),albedo,metallic);
    float fresnelTerm=pow(1-saturate(dot(viewDirection,normal)),5);
    float3 fresnel=f0+(float3(1,1,1)-f0)*fresnelTerm;
    float3 reflectionContribution=reflection.radiance*fresnel*(1-roughness*0.65);
    float3 diffuseGI=gi.radiance*albedo*frame.giIntensity*ao;
    float3 color=emissive+albedo*0.025*ao+direct+diffuseGI+reflectionContribution;
    color*=max(frame.exposure,0.001);
    color=color/(float3(1,1,1)+color); color=pow(saturate(color),1.0/2.2);
    Output[pixel]=float4(color,1);
}

[shader("miss")]
void Miss(inout Payload payload) {
    RTFrameData frame=FrameData[0];
    float t=saturate(WorldRayDirection().y*0.5+0.5);
    payload.hit=0;
    payload.radiance=frame.backgroundColor*(1-t)+float3(0.30,0.48,0.72)*t;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload,BuiltInTriangleIntersectionAttributes attributes) {
    payload.hit=1; payload.hitT=RayTCurrent();
    if(payload.mode==1) return;
    uint geometryCount=0,geometryStride=0; GeometryTable.GetDimensions(geometryCount,geometryStride);
    uint geometrySlot=InstanceID()*kMaxGeometriesPerInstance+GeometryIndex();
    if(geometrySlot>=geometryCount) { payload.radiance=float3(0.12,0.12,0.12); return; }
    RTGeometryInfo geometry=GeometryTable[geometrySlot];
    uint indexCount=0,indexStride=0; Indices.GetDimensions(indexCount,indexStride);
    uint first=geometry.firstIndex+PrimitiveIndex()*3;
    if(first+2>=indexCount) { payload.radiance=float3(0.12,0.12,0.12); return; }
    uint i0=geometry.vertexOffset+Indices[first],i1=geometry.vertexOffset+Indices[first+1],i2=geometry.vertexOffset+Indices[first+2];
    uint vertexCount=0,vertexStride=0; Vertices.GetDimensions(vertexCount,vertexStride);
    if(i0>=vertexCount||i1>=vertexCount||i2>=vertexCount) { payload.radiance=float3(0.12,0.12,0.12); return; }
    RTVertex v0=Vertices[i0],v1=Vertices[i1],v2=Vertices[i2];
    float3 p0=mul(float4(v0.position,1),ObjectToWorld3x4());
    float3 p1=mul(float4(v1.position,1),ObjectToWorld3x4());
    float3 p2=mul(float4(v2.position,1),ObjectToWorld3x4());
    float3 n=normalize(cross(p1-p0,p2-p0)); if(dot(n,WorldRayDirection())>0)n=-n;
    float3 bary=float3(1-attributes.barycentrics.x-attributes.barycentrics.y,attributes.barycentrics.x,attributes.barycentrics.y);
    float3 vertexColor=v0.color.rgb*bary.x+v1.color.rgb*bary.y+v2.color.rgb*bary.z;
    uint materialCount=0,materialStride=0; Materials.GetDimensions(materialCount,materialStride);
    if(geometry.materialIndex<materialCount) {
        RTMaterial m=Materials[geometry.materialIndex];
        payload.normal=n; payload.albedo=saturate(m.baseColor.rgb*vertexColor);
        payload.emission=max(m.emission.rgb,float3(0,0,0));
        payload.metallic=saturate(m.metallic); payload.roughness=clamp(m.roughness,0.025,1);
        payload.radiance=payload.emission+payload.albedo*0.18;
    } else {
        payload.normal=n; payload.albedo=saturate(vertexColor); payload.emission=float3(0,0,0);
        payload.metallic=0; payload.roughness=0.65; payload.radiance=payload.albedo*0.18;
    }
}
