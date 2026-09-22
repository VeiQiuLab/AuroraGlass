#include "controls/control_geometry.h"

#include <algorithm>
#include <cmath>

namespace AuroraGlass {

bool HitTestRect(const ControlBounds& bounds, ControlPoint p) noexcept {
    if (!bounds.IsValid()) return false;
    if (bounds.width == 0.0f || bounds.height == 0.0f) return false;
    // NaN point coordinates make these comparisons false, so they are rejected.
    return p.x >= bounds.Left() && p.x < bounds.Right() &&
           p.y >= bounds.Top()  && p.y < bounds.Bottom();
}

bool HitTestRoundedRect(const ControlBounds& bounds, ControlPoint p,
                        float cornerRadius) noexcept {
    if (!bounds.IsValid()) return false;
    if (bounds.width == 0.0f || bounds.height == 0.0f) return false;

    // Outer bounds: identical half-open convention to HitTestRect, so that a
    // zero-radius rounded test matches the plain rectangle exactly (and adjacent
    // controls sharing an edge never both hit the same boundary point).
    if (!(p.x >= bounds.Left() && p.x < bounds.Right() &&
          p.y >= bounds.Top()  && p.y < bounds.Bottom())) {
        return false;
    }

    const float halfW = bounds.width  * 0.5f;
    const float halfH = bounds.height * 0.5f;
    const float cx = bounds.x + halfW;
    const float cy = bounds.y + halfH;

    float r = cornerRadius;
    if (!(r >= 0.0f)) r = 0.0f;                  // NaN / negative -> 0
    r = std::min(r, std::min(halfW, halfH));     // clamp to half-min-dimension

    const float qx = std::fabs(p.x - cx) - halfW + r;
    const float qy = std::fabs(p.y - cy) - halfH + r;
    const float ax = std::max(qx, 0.0f);
    const float ay = std::max(qy, 0.0f);
    const float sdf = std::sqrt(ax * ax + ay * ay) +
                      std::min(std::max(qx, qy), 0.0f) - r;
    return sdf <= 0.0f;
}

} // namespace AuroraGlass
