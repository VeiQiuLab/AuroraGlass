#include "controls/control_interaction.h"

namespace AuroraGlass {

void ControlInteraction::SetEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        // Disabled ignores input; drop transient pointer state.
        hovered_ = false;
        pressed_ = false;
    }
}

void ControlInteraction::SetFocused(bool focused) noexcept {
    focused_ = focused;
}

void ControlInteraction::PointerMove(ControlPoint p, const ControlBounds& bounds,
                                     float cornerRadius) noexcept {
    if (!enabled_) return;
    hovered_ = Hit(bounds, p, cornerRadius);
}

void ControlInteraction::PointerDown(ControlPoint p, const ControlBounds& bounds,
                                     float cornerRadius) noexcept {
    if (!enabled_) return;
    hovered_ = Hit(bounds, p, cornerRadius);
    if (hovered_) pressed_ = true;
}

bool ControlInteraction::PointerUp(ControlPoint p, const ControlBounds& bounds,
                                   float cornerRadius) noexcept {
    if (!enabled_) return false;
    const bool inside = Hit(bounds, p, cornerRadius);
    hovered_ = inside;
    const bool activated = pressed_ && inside;
    pressed_ = false;
    return activated;
}

void ControlInteraction::PointerLeave() noexcept {
    if (!enabled_) return;
    hovered_ = false;
    pressed_ = false;
}

bool ControlInteraction::KeyActivate() noexcept {
    return enabled_;
}

ControlInteractionState ControlInteraction::State() const noexcept {
    if (!enabled_) return ControlInteractionState::Disabled;
    if (pressed_)  return ControlInteractionState::Pressed;
    if (hovered_)  return ControlInteractionState::Hover;
    if (focused_)  return ControlInteractionState::Focused;
    return ControlInteractionState::Normal;
}

void ControlInteraction::Reset() noexcept {
    focused_ = false;
    hovered_ = false;
    pressed_ = false;
    // enabled_ intentionally preserved.
}

} // namespace AuroraGlass
