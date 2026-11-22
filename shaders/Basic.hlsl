// FX3 v0.4 basic textured mesh shader. The renderer currently compiles this at runtime.
Texture2D Albedo : register(t0);
SamplerState LinearWrap : register(s0);

struct VSInput { float3 position : POSITION; float2 uv : TEXCOORD0; float3 normal : NORMAL; };
struct PSInput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; float3 normal : NORMAL; };

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    output.normal = input.normal;
    return output;
}
float4 PSMain(PSInput input) : SV_TARGET {
    float3 n = normalize(input.normal);
    float light = 0.25 + 0.75 * saturate(dot(n, normalize(float3(-0.4, 0.8, -0.5))));
    return float4(Albedo.Sample(LinearWrap, input.uv).rgb * light, 1.0);
}
