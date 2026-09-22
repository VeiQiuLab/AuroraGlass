#include "core/glass_material.h"

#include <cmath>

namespace AuroraGlass {

namespace {

// Returns true if v is NaN or ±Inf.
// Note: std::isnan / std::isinf are not constexpr in C++20, so this is inline, not constexpr.
inline bool IsInvalidFloat(float v) noexcept {
    return std::isnan(v) || std::isinf(v);
}

// Validate + clamp. Returns InvalidArgument if v is NaN/Inf; otherwise
// stores clamped(v, lo, hi) into out and returns Ok.
Status ValidateAndClamp(float v, float lo, float hi, float& out) noexcept {
    if (IsInvalidFloat(v)) return Status::InvalidArgument();
    out = v < lo ? lo : (v > hi ? hi : v);
    return Status::Ok();
}

} // namespace

GlassMaterial::GlassMaterial() noexcept = default;

Status GlassMaterial::SetBlurRadius(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 24.0f, blurRadius_);
}
Status GlassMaterial::SetRefractionStrength(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 1.0f, refractionStrength_);
}
Status GlassMaterial::SetDispersionStrength(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 1.0f, dispersionStrength_);
}
Status GlassMaterial::SetThickness(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 1.0f, thickness_);
}
Status GlassMaterial::SetEdgeFresnel(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 1.0f, edgeFresnel_);
}
Status GlassMaterial::SetSpecularStrength(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 2.0f, specularStrength_);
}
Status GlassMaterial::SetTintAmount(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 1.0f, tintAmount_);
}
Status GlassMaterial::SetSaturation(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 2.0f, saturation_);
}
Status GlassMaterial::SetBrightness(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 2.0f, brightness_);
}
Status GlassMaterial::SetNoiseAmount(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 0.2f, noiseAmount_);
}
Status GlassMaterial::SetCornerRadius(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 200.0f, cornerRadius_);
}
Status GlassMaterial::SetOpacity(float v) noexcept {
    return ValidateAndClamp(v, 0.0f, 1.0f, opacity_);
}
Status GlassMaterial::SetHighlightPosition(float x, float y) noexcept {
    if (IsInvalidFloat(x) || IsInvalidFloat(y)) return Status::InvalidArgument();
    highlightX_ = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
    highlightY_ = y < -1.0f ? -1.0f : (y > 1.0f ? 1.0f : y);
    return Status::Ok();
}

} // namespace AuroraGlass
