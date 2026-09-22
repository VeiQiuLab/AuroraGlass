
// AuroraGlass P0 - Final glass compositing pass.
// Applies (with independent stage toggles): refraction, chromatic dispersion,
// Fresnel edge light, specular highlight, tint/saturation/brightness, noise,
// and a rounded-rectangle mask, then composites over the sharp background.

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
    float4 m_Stages2;  // x=mask y=colorAdjust z=rectMode (0=legacy,1=discard outside) w unused
};

Texture2D    t_Background : register(t0);
Texture2D    t_Blurred    : register(t1);
SamplerState s_Linear     : register(s0);

float sdRoundRect(float2 p, float2 halfSize, float r)
{
    float2 q = abs(p) - halfSize + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float4 GlassPS(float4 pos : SV_Position) : SV_Target
{
    float2 res = u_Resolution.xy;
    float2 px  = pos.xy;
    float2 uv  = px / res;

    // ---- unpack material ----
    float refraction   = m_A.y;
    float dispersion   = m_A.z;
    float thickness    = m_A.w;
    float edgeFresnel  = m_B.x;
    float specular     = m_B.y;
    float tintAmount   = m_B.z;
    float saturation   = m_B.w;
    float brightness   = m_C.x;
    float noiseAmt     = m_C.y;
    float cornerRadius = m_C.z;
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
    float rectMode = m_Stages2.z;   // 0 = legacy (bgSharp outside), 1 = discard outside

    float2 p = px - center;
    float sdf = sdRoundRect(p, halfSize, cornerRadius);

    // ---- coverage (mask stage) ----
    float coverage;
    if (sMask > 0.5) {
        float feather = 1.5; // px anti-aliasing band
        coverage = 1.0 - smoothstep(-feather, feather, sdf);
    } else {
        float2 q = abs(p) - halfSize;
        coverage = (max(q.x, q.y) <= 0.0) ? 1.0 : 0.0;
    }
    coverage *= opacity;

    float3 bgSharp = t_Background.Sample(s_Linear, uv).rgb;
    if (coverage <= 0.0) {
        // Legacy: composite the sharp background everywhere (unchanged behavior).
        // Rect mode: write nothing outside the rounded glass shape so a later
        // control never erases the target outside its own shape.
        if (rectMode > 0.5) discard;
        return float4(bgSharp, 1.0);
    }

    // ---- refraction offset from SDF gradient ----
    float e = 1.0;
    float gx = sdRoundRect(p + float2(e, 0), halfSize, cornerRadius) - sdRoundRect(p - float2(e, 0), halfSize, cornerRadius);
    float gy = sdRoundRect(p + float2(0, e), halfSize, cornerRadius) - sdRoundRect(p - float2(0, e), halfSize, cornerRadius);
    float2 grad = float2(gx, gy);
    float glen = length(grad);
    float2 gdir = glen > 1e-4 ? (grad / glen) : float2(0.0, 0.0);

    float distInside = max(-sdf, 0.0);
    float band = max(thickness * 40.0, 8.0);
    float edgeW = exp(-distInside / band);

    float2 refrOffsetPx = -gdir * refraction * edgeW * 20.0;
    if (sRefr < 0.5) refrOffsetPx = float2(0.0, 0.0);
    float2 baseOff = refrOffsetPx / res;

    // ---- sample through glass (blur handled upstream; dispersion here) ----
    float3 col;
    if (sDisp > 0.5) {
        float2 uvR = uv + baseOff * (1.0 + dispersion);
        float2 uvG = uv + baseOff;
        float2 uvB = uv + baseOff * (1.0 - dispersion);
        col.r = t_Blurred.Sample(s_Linear, uvR).r;
        col.g = t_Blurred.Sample(s_Linear, uvG).g;
        col.b = t_Blurred.Sample(s_Linear, uvB).b;
    } else {
        col = t_Blurred.Sample(s_Linear, uv + baseOff).rgb;
    }

    // ---- Fresnel edge light ----
    if (sFres > 0.5) {
        float band2 = 14.0;
        float edgeProx = 1.0 - saturate(distInside / band2);
        float fres = pow(edgeProx, 2.0) * edgeFresnel;
        col += fres * float3(0.9, 0.95, 1.0) * 0.7;
    }

    // ---- specular highlight ----
    if (sSpec > 0.5) {
        float2 np = p / max(halfSize, float2(1.0, 1.0));
        float3 n = normalize(float3(np * 0.6, 1.0));
        float2 lightPx = center + highlight * halfSize;
        float3 L = normalize(float3(lightPx - px, 120.0));
        float3 V = float3(0.0, 0.0, 1.0);
        float3 H = normalize(L + V);
        float spec = pow(saturate(dot(n, H)), 60.0) * specular;
        col += spec * float3(1.0, 1.0, 1.0);
    }

    // ---- tint / saturation / brightness ----
    if (sColor > 0.5) {
        float3 tintCol = float3(0.85, 0.92, 1.0);
        col = lerp(col, col * tintCol * 1.15, tintAmount);
        float luma = dot(col, float3(0.299, 0.587, 0.114));
        col = lerp(float3(luma, luma, luma), col, saturation);
        col *= brightness;
    }

    // ---- noise ----
    float nse = frac(sin(dot(px, float2(12.9898, 78.233)) + u_Time.x) * 43758.5453);
    col += (nse - 0.5) * noiseAmt;

    // ---- composite over sharp background ----
    float3 finalCol = lerp(bgSharp, col, coverage);
    return float4(finalCol, 1.0);
}
