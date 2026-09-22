#pragma once

// ============================================================
// AuroraGlass Controls (P3) — minimal interaction state machine.
//
// Pure CPU. No GPU, no HWND, no Core dependency, no event framework.
//
// The host converts OS input (WM_*, virtual keys, DPI) into physical-pixel
// calls here. This class does NOT know about HWND / WM_* / virtual keys and
// does NOT dispatch input. It only tracks observable state.
//
// Focus is represented as STATE only — there is no focus manager.
// Toggle "checked" / Slider "value" belong to those specific controls and
// are intentionally NOT part of this shared state.
// ============================================================

#include "controls/control_geometry.h"

namespace AuroraGlass {

enum class ControlInteractionState {
    Normal,
    Hover,
    Pressed,
    Disabled,
    Focused,
};

class ControlInteraction {
public:
    ControlInteraction() noexcept = default;

    // Disabled is a mode that overrides pointer states and ignores input.
    void SetEnabled(bool enabled) noexcept;
    // Focus is state only (no focus manager).
    void SetFocused(bool focused) noexcept;

    bool IsEnabled() const noexcept { return enabled_; }
    bool IsFocused() const noexcept { return focused_; }
    bool IsHovered() const noexcept { return hovered_; }
    bool IsPressed() const noexcept { return pressed_; }

    // Pointer input in physical pixels. bounds/cornerRadius describe the
    // control; cornerRadius <= 0 uses a plain rectangle hit-test.
    void PointerMove(ControlPoint p, const ControlBounds& bounds,
                     float cornerRadius = 0.0f) noexcept;
    void PointerDown(ControlPoint p, const ControlBounds& bounds,
                     float cornerRadius = 0.0f) noexcept;
    // Returns true if the release completed inside the control (a potential
    // activation). No event/callback is raised here.
    bool PointerUp(ControlPoint p, const ControlBounds& bounds,
                   float cornerRadius = 0.0f) noexcept;
    void PointerLeave() noexcept;

    // Minimal semantic activation (host maps Enter/Space to this call).
    // The control does not interpret Windows virtual keys. Returns enabled.
    bool KeyActivate() noexcept;

    // Derived, deterministic state.
    ControlInteractionState State() const noexcept;

    // Clears transient pointer state; preserves enabled. Focus is cleared too.
    void Reset() noexcept;

private:
    static bool Hit(const ControlBounds& b, ControlPoint p, float r) noexcept {
        return r > 0.0f ? HitTestRoundedRect(b, p, r) : HitTestRect(b, p);
    }

    bool enabled_ = true;
    bool focused_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
};

} // namespace AuroraGlass
