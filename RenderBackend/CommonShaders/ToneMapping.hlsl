struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    static const float2 positions[3] = {
        float2(-1.0, 1.0),
        float2(3.0, 1.0),
        float2(-1.0, -3.0)};
    static const float2 uvs[3] = {
        float2(0.0, 0.0),
        float2(2.0, 0.0),
        float2(0.0, 2.0)};

    VSOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = uvs[vertexId];
    return output;
}

Texture2D<float4> i_hdrColor : register(t0);
SamplerState i_hdrSampler : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 hdrColor = i_hdrColor.Sample(i_hdrSampler, input.uv).rgb;
    float3 toneMappedColor = hdrColor / (hdrColor + 1.0);
    float3 gammaCorrectedColor = pow(max(toneMappedColor, 0.0), 1.0 / 2.2);
    return float4(gammaCorrectedColor, 1.0);
}
