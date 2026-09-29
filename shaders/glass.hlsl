// AuroraGlass - Final glass compositing pass.
//
// OPTICS-1 : multi-rect path (rectMode==1) uses height-field bevel + Snell.
// Geometry: original circular rounded-rect SDF for BOTH paths. The Geometry-1
// L^p / n=3 corner experiment was reverted (Owner Visual Gate FAIL).

cbuffer FrameCB : register(b0)
{
    float4 u_Resolution;   // xy = backbuffer size
    float4 u_Time;         // x = seconds
};

cbuffer MaterialCB : register(b1)
{
    float4 m_A;        // x=blurRadius y=refraction z=dispersion w=thickness
    float4 m_B;        // x=edgeFresnel y=specular z=tintAmount w=saturation
    float4 m_C;        // x=brightness y=noiseAmount z=cornerRadius w=opacity
    float4 m_D;        // x=centerX y=centerY z=halfW w=halfH
    float4 m_E;        // x=highlightX y=highlightY zw unused
    float4 m_Stages;   // x=refraction y=dispersion z=fresnel w=specular
    float4 m_Stages2;  // x=mask y=colorAdjust z=rectMode (0=legacy,1=rect) w unused
};

Texture2D    t_Background : register(t0);
Texture2D    t_Blurred    : register(t1);
SamplerState s_Linear     : register(s0);

float sdRoundRect(float2 p, float2 halfSize, float r)
{
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// ---------------------------------------------------------------------------
// OPTICS-1 lens model (rect path only): height-field bevel + Snell refraction.
// Internal constants (tunable, NOT physical): kIOR 1.40, kVirtualDepth 140px.
// ---------------------------------------------------------------------------
static const float kIOR          = 1.40;
static const float kVirtualDepth = 140.0;

float2 LensOffsetPx(float2 gdir, float distInside, float thickness,
                    float refraction, float minHalfSize)
{
    // Material calibration:
    // thickness now controls two related internal profile dimensions:
    //   1) bevelWidth   = how far the bending zone extends inward;
    //   2) profileDepth = how strongly the surface rises through that zone.
    //
    // The old model used depth = bevelWidth * constant, which kept the maximum
    // slope nearly constant and made thickness behave mostly like edge width.
    // This keeps the same smooth quadratic plateau and Snell projection while
    // allowing shallow controls and thick lenses to differ without merely
    // increasing refractionStrength.
    float th = saturate(thickness);
    float nominalWidth = lerp(8.0, 60.0, th);
    float bevelWidth = min(nominalWidth, max(minHalfSize * 0.85, 1.0));
    // Keep the profile slope when the bevel is compressed for a small control.
    float profileDepth = 36.0 * pow(th, 1.20) * bevelWidth / nominalWidth;

    float t = saturate(distInside / bevelWidth);      // 0 edge -> 1 plateau
    float dhdt = -2.0 * profileDepth * (1.0 - t);
    float2 gradH = (dhdt / bevelWidth) * (-gdir);

    float3 N = normalize(float3(-gradH.x, -gradH.y, 1.0));
    float3 I = float3(0.0, 0.0, -1.0);
    float3 R = refract(I, N, 1.0 / kIOR);
    if (dot(R, R) < 1e-6) R = I;

    float s = kVirtualDepth / max(-R.z, 1e-3);
    float2 offset = R.xy * s * refraction;
    // A thin track must not sample many times its own height away from its edge.
    float maxOffset = minHalfSize * 0.40;
    return offset * min(1.0, maxOffset / max(length(offset), 1e-4));
}

float4 GlassPS(float4 pos : SV_Position) : SV_Target
{
    float2 res = u_Resolution.xy;
    float2 px  = pos.xy;
    float2 uv  = px / res;

    float refraction   = m_A.y;
    float dispersion   = m_A.z;
    float thickness    = m_A.w;
    float edgeFresnel  = m_B.x;
    float specular     = m_B.y;
    float tintAmount   = m_B.z;
    float saturation   = m_B.w;
    float brightness   = m_C.x;
    float noiseAmt     = m_C.y;
    float cornerRadius = min(m_C.z, min(m_D.z, m_D.w));
    float opacity      = m_C.w;
    float2 center      = m_D.xy;
    float2 halfSize    = m_D.zw;
    float2 highlight   = m_E.xy;
    float sRefr  = m_Stages.x;
    float sDisp  = m_Stages.y;
    float sFres  = m_Stages.z;
    float sSpec  = m_Stages.w;
    float sMask  = m_Stages2.x;
    float sColor = m_Stages2.y;
    float rectMode = m_Stages2.z;   // 0 = legacy, 1 = rect
    // The Snell offset is much larger than the legacy UV shift; the same
    // dispersion multiplier otherwise separates channels by several pixels.
    if (rectMode > 0.5) dispersion *= 0.03;
    // The rect lens keeps readable background detail at ordinary blur radii.
    // Strong frost remains available near the top of the existing 0..24 range.
    float frostMix = rectMode > 0.5 ? pow(saturate(m_A.x / 24.0), 2.0) : 1.0;

    float2 p = px - center;
    float sdf = sdRoundRect(p, halfSize, cornerRadius);

    float coverage;
    if (sMask > 0.5) {
        float feather = 1.5;
        coverage = 1.0 - smoothstep(-feather, feather, sdf);
    } else {
        float2 q = abs(p) - halfSize;
        coverage = (max(q.x, q.y) <= 0.0) ? 1.0 : 0.0;
    }
    coverage *= opacity;

    float3 bgSharp = t_Background.Sample(s_Linear, uv).rgb;
    if (coverage <= 0.0) {
        if (rectMode > 0.5) discard;
        return float4(bgSharp, 1.0);
    }

    float e = 1.0;
    float gx = sdRoundRect(p + float2(e, 0), halfSize, cornerRadius) - sdRoundRect(p - float2(e, 0), halfSize, cornerRadius);
    float gy = sdRoundRect(p + float2(0, e), halfSize, cornerRadius) - sdRoundRect(p - float2(0, e), halfSize, cornerRadius);
    float2 grad = float2(gx, gy);
    float glen = length(grad);
    float2 gdir = glen > 1e-4 ? (grad / glen) : float2(0.0, 0.0);

    float distInside = max(-sdf, 0.0);

    float2 refrOffsetPx;
    if (rectMode > 0.5) {
        refrOffsetPx = LensOffsetPx(gdir, distInside, thickness,
                                   refraction, min(halfSize.x, halfSize.y));
        // The edge bevel reaches a flat plateau on large panels. A subpixel
        // interior bend keeps background detail gently curved without adding
        // any light, tint, or distortion to small controls.
        float minHalf = min(halfSize.x, halfSize.y);
        float2 local = p / max(halfSize, float2(1.0, 1.0));
        float interior = smoothstep(36.0, 80.0, minHalf) *
                         smoothstep(0.0, 40.0, distInside);
        refrOffsetPx += local * (1.0 - local * local) *
                        (4.0 * refraction * interior);
    } else {
        float band = max(thickness * 40.0, 8.0);
        float edgeW = exp(-distInside / band);
        refrOffsetPx = -gdir * refraction * edgeW * 20.0;
    }
    if (sRefr < 0.5) refrOffsetPx = float2(0.0, 0.0);
    float2 baseOff = refrOffsetPx / res;

    float3 col;
    if (sDisp > 0.5) {
        float2 uvR = uv + baseOff * (1.0 + dispersion);
        float2 uvG = uv + baseOff;
        float2 uvB = uv + baseOff * (1.0 - dispersion);
        col.r = lerp(t_Background.Sample(s_Linear, uvR).r,
                     t_Blurred.Sample(s_Linear, uvR).r, frostMix);
        col.g = lerp(t_Background.Sample(s_Linear, uvG).g,
                     t_Blurred.Sample(s_Linear, uvG).g, frostMix);
        col.b = lerp(t_Background.Sample(s_Linear, uvB).b,
                     t_Blurred.Sample(s_Linear, uvB).b, frostMix);
    } else {
        col = lerp(t_Background.Sample(s_Linear, uv + baseOff).rgb,
                   t_Blurred.Sample(s_Linear, uv + baseOff).rgb, frostMix);
    }

    // Only the curved rim reflects light. The former panel-wide normal made
    // a movable white specular patch in the flat interior; the 14 px Fresnel
    // band likewise read as a halo rather than a glass edge.
    float rimWidth = min(6.0, min(halfSize.x, halfSize.y) * 0.60);
    float rim = pow(1.0 - saturate(distInside / max(rimWidth, 1e-3)), 3.0);
    if (sFres > 0.5) {
        col += rim * edgeFresnel * 0.05;
    }

    if (sSpec > 0.5) {
        float3 n = normalize(float3(-gdir * 0.4 * rim, 1.0));
        float2 lightPx = center + highlight * halfSize;
        float3 L = normalize(float3(lightPx - px, 120.0));
        float3 V = float3(0.0, 0.0, 1.0);
        float3 H = normalize(L + V);
        float spec = pow(saturate(dot(n, H)), 60.0) * rim * specular;
        col += spec * 0.07;
    }

    if (sColor > 0.5) {
        float3 tintCol = float3(0.85, 0.92, 1.0);
        // The rect path does not need the legacy tint's extra white gain.
        float tintGain = rectMode > 0.5 ? 1.0 : 1.15;
        col = lerp(col, col * tintCol * tintGain, tintAmount);
        float luma = dot(col, float3(0.299, 0.587, 0.114));
        col = lerp(float3(luma, luma, luma), col, saturation);
        col *= brightness;
    }

    float nse = frac(sin(dot(px, float2(12.9898, 78.233)) + u_Time.x) * 43758.5453);
    col += (nse - 0.5) * noiseAmt;

    float3 finalCol = lerp(bgSharp, col, coverage);
    return float4(finalCol, 1.0);
}
