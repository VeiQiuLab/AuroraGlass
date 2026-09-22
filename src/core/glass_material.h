#pragma once

// ============================================================
// AuroraGlass Core — GlassMaterial (value type).
//
// GlassMaterial expresses the stable, user-understandable
// material properties of a liquid-glass surface.
//
// Thread safety:
//   GlassMaterial is a pure value type with no GPU resources.
//   It may be constructed and modified on any thread.
//   Only GlassSurface (which consumes it) requires render-thread contract.
//
// Validation:
//   Every setter validates its input:
//     - NaN and Inf are rejected (Status::InvalidArgument).
//     - Values outside the documented legal range are rejected.
//   Successful setters return Status::Ok().
//
// The internal constant-buffer layout is NOT exposed.
// ============================================================

#include "core/result.h"

namespace AuroraGlass {

class GlassMaterial {
public:
    // Constructs with the P0-verified default material.
    GlassMaterial() noexcept;

    // ---- Validated setters ----
    // All setters reject NaN / Inf and values outside the documented range.

    Status SetBlurRadius(float v) noexcept;            // [0, 24] px
    Status SetRefractionStrength(float v) noexcept;    // [0, 1]
    Status SetDispersionStrength(float v) noexcept;    // [0, 1]
    Status SetThickness(float v) noexcept;             // [0, 1]
    Status SetEdgeFresnel(float v) noexcept;           // [0, 1]
    Status SetSpecularStrength(float v) noexcept;      // [0, 2]
    Status SetTintAmount(float v) noexcept;            // [0, 1]
    Status SetSaturation(float v) noexcept;            // [0, 2]
    Status SetBrightness(float v) noexcept;            // [0, 2]
    Status SetNoiseAmount(float v) noexcept;           // [0, 0.2]
    Status SetCornerRadius(float v) noexcept;          // [0, 200] px
    Status SetOpacity(float v) noexcept;               // [0, 1]
    Status SetHighlightPosition(float x, float y) noexcept; // [-1, 1] per component

    // ---- Getters ----
    float GetBlurRadius() const noexcept         { return blurRadius_; }
    float GetRefractionStrength() const noexcept { return refractionStrength_; }
    float GetDispersionStrength() const noexcept { return dispersionStrength_; }
    float GetThickness() const noexcept          { return thickness_; }
    float GetEdgeFresnel() const noexcept        { return edgeFresnel_; }
    float GetSpecularStrength() const noexcept   { return specularStrength_; }
    float GetTintAmount() const noexcept         { return tintAmount_; }
    float GetSaturation() const noexcept         { return saturation_; }
    float GetBrightness() const noexcept         { return brightness_; }
    float GetNoiseAmount() const noexcept        { return noiseAmount_; }
    float GetCornerRadius() const noexcept       { return cornerRadius_; }
    float GetOpacity() const noexcept            { return opacity_; }
    void  GetHighlightPosition(float& outX, float& outY) const noexcept {
        outX = highlightX_;
        outY = highlightY_;
    }

private:
    // Stored values — always within documented valid ranges.
    float blurRadius_         = 12.0f;
    float refractionStrength_ = 0.60f;
    float dispersionStrength_ = 0.50f;
    float thickness_          = 0.50f;
    float edgeFresnel_        = 0.60f;
    float specularStrength_   = 1.00f;
    float tintAmount_         = 0.25f;
    float saturation_         = 1.05f;
    float brightness_         = 1.02f;
    float noiseAmount_        = 0.03f;
    float cornerRadius_       = 28.0f;
    float opacity_            = 0.92f;
    float highlightX_         = 0.50f;
    float highlightY_         = 0.35f;
};

// ============================================================
// DiagnosticStages — P0-compatible independent stage toggles.
//
// KEPT SEPARATE from GlassMaterial: these toggles exist so that
// individual visual stages can be disabled for diagnosis. They
// are NOT part of the stable public material API, and must not
// be used to drive material authoring. Host code that only
// wants to render a glass surface should not need to touch these.
// ============================================================

struct DiagnosticStages {
    bool blur          = true;
    bool refraction    = true;
    bool dispersion    = true;
    bool fresnel       = true;
    bool specular      = true;
    bool mask          = true;
    bool colorAdjust   = true;

    // All stages enabled (the default visual).
    static DiagnosticStages AllEnabled() noexcept { return {}; }

    // All stages disabled (useful as a baseline).
    static DiagnosticStages AllDisabled() noexcept {
        DiagnosticStages s;
        s.blur = s.refraction = s.dispersion = s.fresnel =
        s.specular = s.mask = s.colorAdjust = false;
        return s;
    }
};

} // namespace AuroraGlass
