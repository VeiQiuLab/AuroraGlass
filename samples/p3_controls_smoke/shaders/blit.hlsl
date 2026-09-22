// AuroraGlass P3 sample support - minimal full-screen blit.
// Copies a source texture to the current render target (no effects).
// SAMPLE-ONLY host blit shader. NOT part of the AuroraGlass Core glass pipeline.

cbuffer BlitCB : register(b0)
{
    float4 u_Texel;   // xy = 1/width, 1/height, zw unused
};

Texture2D    t_Src    : register(t0);
SamplerState s_Linear : register(s0);

float4 BlitPS(float4 pos : SV_Position) : SV_Target
{
    float2 uv = pos.xy * u_Texel.xy;
    return float4(t_Src.Sample(s_Linear, uv).rgb, 1.0);
}
