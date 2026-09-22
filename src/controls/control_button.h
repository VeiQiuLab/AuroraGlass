#pragma once

// ============================================================
// AuroraGlass Controls (P3) — GlassButton (semantic control).
//
// Pure CPU / no GPU / no HWND / no D3D / no GlassSurface.
// Composes: ControlBounds + ControlInteraction + ControlVisualStyle.
//
// The control owns only: bounds, interaction state, style, and a minimal
// activation event. The host owns PrepareFrame / SetMaterial / RenderRect.
// ============================================================

#include "controls/control_geometry.h"
#include "controls/control_interaction.h"
#include "controls/control_visual_style.h"

#include <functional>

namespace AuroraGlass {

class GlassButton {
public:
    ControlBounds      bounds{};
    ControlVisualStyle style{};
    // Minimal event: a single callback. No event bus / registry / signal-slot.
    std::function<void()> onClick;

    void SetEnabled(bool e) noexcept { interaction_.SetEnabled(e); }
    void SetFocused(bool f) noexcept { interaction_.SetFocused(f); }
    bool IsEnabled() const noexcept { return interaction_.IsEnabled(); }
    bool IsFocused() const noexcept { return interaction_.IsFocused(); }

    ControlInteractionState State() const noexcept { return interaction_.State(); }
    const GlassMaterial&    Material() const noexcept {
        return style.MaterialFor(interaction_.State());
    }

    // Pointer input in physical pixels. Down inside -> Pressed; Up inside after a
    // valid press -> activation (onClick); Up outside -> no activation.
    void PointerMove(ControlPoint p) noexcept { interaction_.PointerMove(p, bounds); }
    void PointerDown(ControlPoint p) noexcept { interaction_.PointerDown(p, bounds); }
    bool PointerUp(ControlPoint p) noexcept {
        const bool activated = interaction_.PointerUp(p, bounds);
        if (activated && onClick) onClick();
        return activated;
    }
    void PointerLeave() noexcept { interaction_.PointerLeave(); }

    // Semantic activation (host maps Enter/Space). No virtual-key interpretation.
    bool KeyActivate() noexcept {
        const bool activated = interaction_.KeyActivate();
        if (activated && onClick) onClick();
        return activated;
    }

private:
    ControlInteraction interaction_;
};

} // namespace AuroraGlass
