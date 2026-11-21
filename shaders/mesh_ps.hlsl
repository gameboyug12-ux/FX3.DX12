struct PSInput
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
};

float4 PSMain(PSInput input) : SV_Target0
{
    const float3 normal = normalize(input.Normal);
    const float3 lightDirection = normalize(float3(-0.45f, 0.8f, -0.35f));
    const float diffuse = saturate(dot(normal, lightDirection));
    const float lighting = 0.18f + diffuse * 0.82f;
    return float4(input.Color.rgb * lighting, input.Color.a);
}
