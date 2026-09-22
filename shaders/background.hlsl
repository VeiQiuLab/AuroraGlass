
// AuroraGlass P0 - Procedural animated test background.
// Deliberately rich in high-frequency content (thin lines / rings) so that
// blur, refraction and dispersion stages are visibly distinguishable.
// UPDATED: Brighter colors for better visibility.

cbuffer FrameCB : register(b0)
{
    float4 u_Resolution;      // xy = target size (px), zw unused
    float4 u_Time;            // x = time (s), yzw unused
};

float3 blob(float2 p, float2 c, float r, float3 col, float intensity)
{
    float d = length(p - c);
    float g = exp(-(d * d) / (r * r + 1e-5));
    return col * g * intensity;
}

float4 BackgroundPS(float4 pos : SV_Position) : SV_Target
{
    float2 res  = u_Resolution.xy;
    float2 uv   = pos.xy / res;                 // 0..1
    float2 asp  = float2(res.x / res.y, 1.0);
    float2 p    = uv * asp;
    float  t    = u_Time.x;

    // Brighter vertical base gradient (was too dark)
    float3 col = lerp(float3(0.25, 0.35, 0.65), float3(0.15, 0.20, 0.45), uv.y);

    // Moving color blobs - increased intensity and size
    col += blob(p, float2(0.50 * asp.x + 0.28 * sin(t * 0.70), 0.35 + 0.18 * cos(t * 0.53)), 0.30, float3(1.00, 0.45, 0.25), 1.20);
    col += blob(p, float2(0.30 * asp.x + 0.22 * cos(t * 0.61), 0.62 + 0.15 * sin(t * 0.82)), 0.35, float3(0.20, 0.85, 0.55), 1.10);
    col += blob(p, float2(0.72 * asp.x + 0.20 * sin(t * 0.47), 0.68 + 0.20 * cos(t * 0.66)), 0.32, float3(0.30, 0.55, 1.00), 1.25);
    col += blob(p, float2(0.85 * asp.x + 0.10 * cos(t * 0.90), 0.25 + 0.10 * sin(t * 1.10)), 0.22, float3(1.00, 0.90, 0.35), 1.00);

    // Bright grid lines (high frequency reference for blur/refraction diagnosis)
    float2 g = frac(uv * 24.0);
    float gridW = (step(g.x, 0.08) + step(g.y, 0.08));
    col = lerp(col, float3(0.95, 0.95, 1.00), saturate(gridW) * 0.50);

    // Bright rings
    for (int i = 0; i < 3; ++i)
    {
        float fi = (float)i;
        float2 c = float2((0.25 + 0.25 * fi) * asp.x + 0.08 * sin(t * (0.4 + 0.1 * fi)),
                          0.50 + 0.25 * cos(t * (0.5 + 0.13 * fi) + fi * 2.1));
        float ring = abs(length(p - c) - (0.12 + 0.04 * fi));
        col += float3(1.0, 1.0, 1.0) * smoothstep(0.006, 0.0, ring) * 1.2;
    }

    // Add a bright test pattern in corners to verify shader execution
    if (uv.x < 0.05 && uv.y < 0.05) col = float3(1.0, 0.0, 0.0);  // Red top-left
    if (uv.x > 0.95 && uv.y < 0.05) col = float3(0.0, 1.0, 0.0);  // Green top-right
    if (uv.x < 0.05 && uv.y > 0.95) col = float3(0.0, 0.0, 1.0);  // Blue bottom-left
    if (uv.x > 0.95 && uv.y > 0.95) col = float3(1.0, 1.0, 0.0);  // Yellow bottom-right

    return float4(saturate(col), 1.0);
}
