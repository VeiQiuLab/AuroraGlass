#pragma once
// ============================================================
// AuroraGlass P3 interactive sample - SAMPLE-ONLY drag helper.
//
// Pure CPU bounds math for the sample's draggable test object. Kept out of
// Controls/Core on purpose (it is host interaction, not SDK semantics).
// ============================================================

#include "controls/control_geometry.h"

namespace sample {

// Begin a drag: capture the grab offset so the object does not jump.
struct DragState {
    bool  active = false;
    float offX = 0.0f;
    float offY = 0.0f;
};

inline bool BeginDrag(const AuroraGlass::ControlBounds& obj,
                      AuroraGlass::ControlPoint p, DragState& st) {
    if (!AuroraGlass::HitTestRect(obj, p)) return false;
    st.active = true;
    st.offX = p.x - obj.x;
    st.offY = p.y - obj.y;
    return true;
}

// Returns the new bounds for the current cursor position.
inline AuroraGlass::ControlBounds UpdateDrag(const AuroraGlass::ControlBounds& obj,
                                             AuroraGlass::ControlPoint p,
                                             const DragState& st) {
    AuroraGlass::ControlBounds o = obj;
    if (st.active) { o.x = p.x - st.offX; o.y = p.y - st.offY; }
    return o;
}

inline void EndDrag(DragState& st) { st.active = false; }

} // namespace sample
