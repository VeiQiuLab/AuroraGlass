#pragma once

// ============================================================
// AuroraGlass Controls (P3) — visual style (state -> GlassMaterial).
//
// Pure CPU. No GPU, no HWND, no D3D, no GlassSurface dependency.
// May depend on the frozen Core value type GlassMaterial only.
//
// This is a deterministic value-selection helper, NOT a theme engine:
//   • no global theme registry
//   • no style inheritance / cascading
//   • no dynamic property system
//   • no animation / timeline / transition runtime
//
// Given an interaction state, MaterialFor() selects one of the stored
// materials. Same state -> same material (pure, deterministic).
// ============================================================

#include "controls/control_interaction.h"   // ControlInteractionState
#include "core/glass_material.h"            // GlassMaterial (frozen Core value type)

namespace AuroraGlass {

// A control's per-state material set.
//
// All five default to GlassMaterial{} (the P1-verified default). Materials are
// plain values; their setters already validate on assignment, so this type
// stores only already-valid GlassMaterial values.
struct ControlVisualStyle {
    GlassMaterial normal{};
    GlassMaterial hover{};
    GlassMaterial pressed{};
    GlassMaterial disabled{};
    GlassMaterial focused{};

    // Convenience for the common "same look in every state" case: copies the
    // given material into all five slots. Not a style system — just a bulk set.
    void SetAll(const GlassMaterial& m) noexcept {
        normal = hover = pressed = disabled = focused = m;
    }

    // Pure, deterministic selection.
    const GlassMaterial& MaterialFor(ControlInteractionState s) const noexcept {
        switch (s) {
        case ControlInteractionState::Hover:    return hover;
        case ControlInteractionState::Pressed:  return pressed;
        case ControlInteractionState::Disabled: return disabled;
        case ControlInteractionState::Focused:  return focused;
        case ControlInteractionState::Normal:
        default:                                return normal;
        }
    }
};

} // namespace AuroraGlass
