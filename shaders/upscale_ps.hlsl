Texture2D<float4> SourceColor : register(t0);
SamplerState LinearClamp : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_Target0 {
    return SourceColor.Sample(LinearClamp, input.uv);
}
