#pragma once

// ============================================================
// AuroraGlass Controls (P3) — GlassToggle (semantic control).
//
// Pure CPU / no GPU / no HWND / no D3D / no GlassSurface.
// Button-like activation plus a bool checked state.
//
// "checked" is specific to this control and is intentionally NOT part of the
// shared ControlInteractionState.
// ============================================================

#include "controls/control_geometry.h"
#include "controls/control_interaction.h"
#include "controls/control_visual_style.h"

#include <functional>

namespace AuroraGlass {

class GlassToggle {
public:
    ControlBounds      bounds{};
    ControlVisualStyle style{};
    // Optional separate style for the checked state. If useCheckedStyle is
    // false, the (unchecked) style is used for both states. This is a minimal
    // per-control extension, not a theme engine.
    ControlVisualStyle checkedStyle{};
    bool               useCheckedStyle = false;

    bool checked = false;
    std::function<void(bool)> onChanged;   // minimal event

    void SetEnabled(bool e) noexcept { interaction_.SetEnabled(e); }
    void SetFocused(bool f) noexcept { interaction_.SetFocused(f); }
    bool IsEnabled() const noexcept { return interaction_.IsEnabled(); }
    bool IsFocused() const noexcept { return interaction_.IsFocused(); }
    bool IsChecked() const noexcept { return checked; }

    ControlInteractionState State() const noexcept { return interaction_.State(); }
    const GlassMaterial&    Material() const noexcept {
        if (checked && useCheckedStyle) return checkedStyle.MaterialFor(interaction_.State());
        return style.MaterialFor(interaction_.State());
    }

    // Sets checked and fires onChanged only if the value actually changed.
    // Disabled toggles ignore the change.
    void SetChecked(bool v) noexcept {
        if (!interaction_.IsEnabled()) return;
        if (v == checked) return;
        checked = v;
        if (onChanged) onChanged(checked);
    }

    void PointerMove(ControlPoint p) noexcept { interaction_.PointerMove(p, bounds); }
    void PointerDown(ControlPoint p) noexcept { interaction_.PointerDown(p, bounds); }
    bool PointerUp(ControlPoint p) noexcept {
        const bool activated = interaction_.PointerUp(p, bounds);
        if (activated) SetChecked(!checked);
        return activated;
    }
    void PointerLeave() noexcept { interaction_.PointerLeave(); }
    bool KeyActivate() noexcept {
        const bool activated = interaction_.KeyActivate();
        if (activated) SetChecked(!checked);
        return activated;
    }

private:
    ControlInteraction interaction_;
};

} // namespace AuroraGlass
