
#pragma once

// ============================================================
// AuroraGlass P0 - INTERNAL material/stage data.
//
// NOTE: GlassMaterialParams is P0-internal implementation data only.
// It is NOT the P1 public GlassMaterial API and must not be frozen
// or exposed as a stable boundary during P0.
// ============================================================

namespace AuroraGlass {

struct GlassMaterialParams {
    float blurRadius        = 12.0f;   // px, 0..24
    float refractionStrength = 0.60f;  // 0..1
    float dispersionStrength = 0.50f;  // 0..1
    float thickness          = 0.50f;  // 0..1 -> surface normal z / specular shape
    float edgeFresnel        = 0.60f;  // 0..1
    float specularStrength   = 1.00f;  // 0..2
    float tintAmount         = 0.25f;  // 0..1, blends toward cool tint
    float saturation         = 1.05f;  // 0..2
    float brightness         = 1.02f;  // 0..2
    float noiseAmount        = 0.03f;  // 0..0.2
    float cornerRadius       = 28.0f;  // px
    float opacity            = 0.92f;  // 0..1
    float highlightPos[2]    = { 0.5f, 0.35f }; // surface UV, mouse-driven in later milestone

    // Clamp all parameters into valid ranges (parameter validation).
    void Clamp();
};

// P0 diagnostic stage toggles. Each visual stage can be disabled
// independently; when disabled the pipeline falls back safely.
struct StageFlags {
    bool blur       = true;
    bool refraction = true;
    bool dispersion = true;
    bool fresnel    = true;
    bool specular   = true;
    bool mask       = true;   // rounded-rect mask + feathered edge
    bool colorAdjust = true;  // tint / saturation / brightness
};

} // namespace AuroraGlass
