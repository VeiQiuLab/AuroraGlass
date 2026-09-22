#pragma once

// ============================================================
// AuroraGlass Controls (P3) - GlassSlider (semantic control).
//
// Pure CPU / no GPU / no HWND / no D3D / no GlassSurface.
// Minimal HORIZONTAL slider: value in [min,max] mapped from x within bounds.
// No vertical mode, ticks, labels, snapping, range slider, keyboard repeat,
// or accessibility framework.
//
// Restrained pointer model (avoids instant jump on click):
//   - press ON the thumb             -> begin dragging
//   - press on the track (off thumb) -> armed; becomes a drag after
//                                       dragThreshold px of movement,
//                                       otherwise a deliberate click-to-set on up
//   - press OUTSIDE the track        -> ignored (no jump)
// There is no value change on press-down.
// ============================================================

#include "controls/control_geometry.h"
#include "controls/control_interaction.h"
#include "controls/control_visual_style.h"
#include "core/result.h"

#include <cmath>
#include <functional>

namespace AuroraGlass {

class GlassSlider {
public:
    ControlBounds      bounds{};
    ControlVisualStyle style{};
    std::function<void(float)> onValueChanged;   // minimal event

    // Thumb (handle) half-width in px; base grab radius around the thumb.
    float thumbHalfWidth = 8.0f;
    // Horizontal movement (px) before an off-thumb press becomes a drag.
    float dragThreshold = 3.0f;

    // Range. Defaults to [0,1]. SetRange rejects a non-finite or inverted range.
    Status SetRange(float lo, float hi) noexcept {
        auto finite = [](float v) { return v == v && v != (float)INFINITY && v != -(float)INFINITY; };
        if (!finite(lo) || !finite(hi) || !(lo < hi)) return Status::InvalidArgument();
        minValue_ = lo;
        maxValue_ = hi;
        value_ = Clamp(value_);
        return Status::Ok();
    }
    float MinValue() const noexcept { return minValue_; }
    float MaxValue() const noexcept { return maxValue_; }
    float Value() const noexcept { return value_; }

    float NormalizedValue() const noexcept {
        const float span = maxValue_ - minValue_;
        if (!(span > 0.0f)) return 0.0f;
        float t = (value_ - minValue_) / span;
        return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }
    float ThumbCenterX() const noexcept {
        return bounds.Left() + NormalizedValue() * bounds.width;
    }

    // Sets value (clamped). Fires onValueChanged only if the value changes.
    void SetValue(float v) noexcept {
        const float nv = Clamp(v);
        if (nv == value_) return;
        value_ = nv;
        if (onValueChanged) onValueChanged(value_);
    }

    void SetEnabled(bool e) noexcept { interaction_.SetEnabled(e); }
    void SetFocused(bool f) noexcept { interaction_.SetFocused(f); }
    bool IsEnabled() const noexcept { return interaction_.IsEnabled(); }
    ControlInteractionState State() const noexcept { return interaction_.State(); }
    const GlassMaterial&    Material() const noexcept {
        return style.MaterialFor(interaction_.State());
    }

    void PointerDown(ControlPoint p) noexcept {
        if (!IsEnabled() || !InTrack(p)) { mode_ = Mode::None; interaction_.PointerLeave(); return; }
        interaction_.PointerDown(ClampInside(p), bounds);
        if (!interaction_.IsPressed()) { mode_ = Mode::None; return; }
        downX_ = p.x;
        const float grab = thumbHalfWidth + 6.0f;
        mode_ = (std::fabs(p.x - ThumbCenterX()) <= grab) ? Mode::Dragging : Mode::Armed;
        // Intentionally no value change on press-down.
    }
    void PointerMove(ControlPoint p) noexcept {
        if (!interaction_.IsPressed()) { interaction_.PointerMove(ClampInside(p), bounds); return; }
        interaction_.PointerMove(ClampInside(p), bounds);
        if (mode_ == Mode::Armed && std::fabs(p.x - downX_) > dragThreshold) mode_ = Mode::Dragging;
        if (mode_ == Mode::Dragging) SetValue(ValueAtX(p.x));
    }
    bool PointerUp(ControlPoint p) noexcept {
        const bool wasPressed = interaction_.IsPressed();
        const bool inside = wasPressed && InTrack(p);
        interaction_.PointerUp(ClampInside(p), bounds);
        // Deliberate click-to-set: press + release without a drag.
        if (wasPressed && mode_ == Mode::Armed && inside) SetValue(ValueAtX(p.x));
        mode_ = Mode::None;
        return inside;
    }
    void PointerLeave() noexcept { interaction_.PointerLeave(); mode_ = Mode::None; }

private:
    enum class Mode { None, Armed, Dragging };
    static constexpr float kEps = 0.001f;

    bool InTrack(ControlPoint p) const noexcept {
        return p.x >= bounds.Left() && p.x <= bounds.Right() &&
               p.y >= bounds.Top()  && p.y <= bounds.Bottom();
    }
    // Pull a point (already in-track) just inside the half-open edge.
    ControlPoint ClampInside(ControlPoint p) const noexcept {
        ControlPoint q = p;
        if (q.x < bounds.Left())          q.x = bounds.Left();
        if (q.x > bounds.Right()  - kEps) q.x = bounds.Right()  - kEps;
        if (q.y < bounds.Top())           q.y = bounds.Top();
        if (q.y > bounds.Bottom() - kEps) q.y = bounds.Bottom() - kEps;
        return q;
    }
    float Clamp(float v) const noexcept {
        if (!(v == v)) return minValue_;                 // NaN -> min
        return v < minValue_ ? minValue_ : (v > maxValue_ ? maxValue_ : v);
    }
    float ValueAtX(float x) const noexcept {
        if (!(bounds.width > 0.0f)) return minValue_;
        float t = (x - bounds.Left()) / bounds.width;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return minValue_ + t * (maxValue_ - minValue_);
    }

    float minValue_ = 0.0f;
    float maxValue_ = 1.0f;
    float value_    = 0.0f;
    float downX_    = 0.0f;
    Mode  mode_     = Mode::None;
    ControlInteraction interaction_;
};

} // namespace AuroraGlass
