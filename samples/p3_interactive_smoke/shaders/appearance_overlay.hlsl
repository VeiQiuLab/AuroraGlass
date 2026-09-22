// AuroraGlass P3 interactive sample - SAMPLE-ONLY appearance overlay.
// Not glass. NOT part of the Core glass pipeline.
//
// A minimal UI-affordance decoration: a ~1px neutral rounded outline plus very
// subtle inner light (top) / inner dark (bottom) tonal cues. It performs NO
// blur / refraction / dispersion / Fresnel / specular. It is not a second glass
// pipeline; it only helps a control be discoverable on a plain background.

cbuffer OverlayCB : register(b0)
{
    float4 u_Res;      // xy = target size (px)
    float4 u_Rect;     // xy = center (px), zw = halfSize (px)
    float4 u_Params;   // x=cornerRadius y=outlineAlpha z=lightAlpha w=darkAlpha
    float4 u_Tint;     // rgb = neutral outline color
};

float sdRoundRect(float2 p, float2 h, float r)
{
    float2 q = abs(p) - h + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float4 OverlayPS(float4 pos : SV_Position) : SV_Target
{
    float2 p = pos.xy - u_Rect.xy;
    float  r = u_Params.x;
    float  sdf = sdRoundRect(p, u_Rect.zw, r);

    // ~1px neutral outline centered on the boundary.
    float outlineBand = 1.0 - smoothstep(0.0, 1.5, abs(sdf));
    float outA = u_Params.y * outlineBand;

    // Inner tonal cue within ~6px of the inside boundary: light near top,
    // dark near bottom (gives a faint "thickness" impression).
    float inside = 1.0 - smoothstep(-1.0, -6.0, sdf);
    float topness = saturate((p.y + u_Rect.w) / max(u_Rect.w * 2.0, 1.0)); // 0 top -> 1 bottom
    float cueA = (u_Params.z * (1.0 - topness) + u_Params.w * topness) * inside;
    float3 cueCol = lerp(float3(1.0, 1.0, 1.0), float3(0.0, 0.0, 0.0), topness);

    float a = saturate(outA + cueA);
    float3 col = (outA * u_Tint.rgb + cueA * cueCol) / max(a, 1e-4);
    return float4(col, a);
}
