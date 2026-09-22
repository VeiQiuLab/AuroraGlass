#pragma once

// ============================================================
// AuroraGlass Controls (P3) — geometry primitives.
//
// Pure CPU. No GPU, no HWND, no Core dependency.
//
// Coordinate contract: PHYSICAL PIXELS, origin at the surface top-left,
// +x right, +y down (identical to the Core pixel contract from P2).
//
// This is NOT a layout system. There is no transform, anchor, margin,
// padding, parent/child tree, or measure/arrange. A control carries a
// single axis-aligned bounds rectangle.
// ============================================================

namespace AuroraGlass {

// A point in physical pixels.
struct ControlPoint {
    float x = 0.0f;
    float y = 0.0f;
};

// Axis-aligned bounds in physical pixels.
//
// Valid iff width >= 0 and height >= 0 and no value is NaN
// (NaN compares false, so the >= checks reject it).
struct ControlBounds {
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;

    float Left()   const noexcept { return x; }
    float Top()    const noexcept { return y; }
    float Right()  const noexcept { return x + width; }
    float Bottom() const noexcept { return y + height; }

    bool IsValid() const noexcept { return width >= 0.0f && height >= 0.0f; }

    bool IsEmpty() const noexcept {
        return !IsValid() || width == 0.0f || height == 0.0f;
    }
};

// Rectangle hit-test.
//
// Boundary convention: HALF-OPEN — a point is inside iff
//     left <= x < right   AND   top <= y < bottom.
// Left/top inclusive; right/bottom exclusive. Zero-size or invalid
// bounds contain no point.
bool HitTestRect(const ControlBounds& bounds, ControlPoint p) noexcept;

// Rounded-rectangle hit-test.
//
// The OUTER bounds use the SAME half-open convention as HitTestRect:
//     left <= x < right   AND   top <= y < bottom
// A point must pass this outer test first, then the rounded-corner SDF test
// (inside iff sdf <= 0). cornerRadius is clamped to [0, min(width,height)/2].
//
// Consequence (contract): HitTestRoundedRect(bounds, p, 0.0f) and
// HitTestRect(bounds, p) produce the same result for all finite valid inputs.
// Invalid/empty bounds contain no point; NaN points are rejected.
bool HitTestRoundedRect(const ControlBounds& bounds, ControlPoint p,
                        float cornerRadius) noexcept;

} // namespace AuroraGlass
