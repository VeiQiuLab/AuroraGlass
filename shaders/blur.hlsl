
// AuroraGlass P0 - Separable Gaussian blur (run twice: horizontal then vertical).
// 9-tap Gaussian; offsets are spread by the blur radius so one shader serves
// all radii. Radius 0 collapses to a copy (weights sum to ~1).

cbuffer BlurCB : register(b0)
{
    float4 u_Texel;   // x = 1/w, y = 1/h, z = dir.x, w = dir.y
    float4 u_Param;   // x = blur radius (px), yzw unused
};

Texture2D    t_Input   : register(t0);
SamplerState s_Linear  : register(s0);

float4 BlurPS(float4 pos : SV_Position) : SV_Target
{
    float2 uv    = pos.xy * u_Texel.xy;
    float2 dir   = u_Texel.zw;              // (1,0) horizontal, (0,1) vertical
    float radius = max(u_Param.x, 0.0);
    float spread = radius / 4.0;            // px per tap step

    // Classic 9-tap Gaussian weights (sum ~1).
    float w0 = 0.227027;
    float w1 = 0.1945946;
    float w2 = 0.1216216;
    float w3 = 0.054054;
    float w4 = 0.016216;

    float3 col = t_Input.Sample(s_Linear, uv).rgb * w0;

    float2 step1 = dir * u_Texel.xy * (spread * 1.0);
    float2 step2 = dir * u_Texel.xy * (spread * 2.0);
    float2 step3 = dir * u_Texel.xy * (spread * 3.0);
    float2 step4 = dir * u_Texel.xy * (spread * 4.0);

    col += t_Input.Sample(s_Linear, uv + step1).rgb * w1;
    col += t_Input.Sample(s_Linear, uv - step1).rgb * w1;
    col += t_Input.Sample(s_Linear, uv + step2).rgb * w2;
    col += t_Input.Sample(s_Linear, uv - step2).rgb * w2;
    col += t_Input.Sample(s_Linear, uv + step3).rgb * w3;
    col += t_Input.Sample(s_Linear, uv - step3).rgb * w3;
    col += t_Input.Sample(s_Linear, uv + step4).rgb * w4;
    col += t_Input.Sample(s_Linear, uv - step4).rgb * w4;

    return float4(col, 1.0);
}
