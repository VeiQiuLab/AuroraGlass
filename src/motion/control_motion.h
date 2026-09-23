#pragma once
// ============================================================
// AuroraGlass P4 Slice B - motion/presentation integration.
//
// Semantic controls remain authoritative for input and logical state.
// These bindings consume semantic state and produce presentation-only values.
//
// No input ownership.
// No event routing.
// No timing source.
// Host advances deterministically with Step(dt).
// ============================================================

#include "controls/control_interaction.h"
#include "motion/motion.h"

namespace AuroraGlass::Motion {

struct ButtonPresentation {
    float scale = 1.0f;
    float response = 0.0f; // 0 normal, restrained positive visual response
};

class ButtonMotion {
public:
    ButtonMotion() noexcept;

    void Sync(ControlInteractionState state) noexcept;
    void Step(float deltaSeconds) noexcept;

    const ButtonPresentation& Presentation() const noexcept {
        return presentation_;
    }

private:
    void RetargetScale(float target) noexcept;
    void RetargetResponse(float target) noexcept;

    Tween1D scale_{1.0f};
    Tween1D response_{0.0f};

    float scaleTarget_ = 1.0f;
    float responseTarget_ = 0.0f;

    ButtonPresentation presentation_{};
};

struct TogglePresentation {
    float progress = 0.0f;
};

class ToggleMotion {
public:
    ToggleMotion() noexcept = default;
    explicit ToggleMotion(bool checked) noexcept;

    // Semantic checked state changes immediately in GlassToggle.
    // This only retargets the visual progress from its CURRENT value.
    void Sync(bool checked) noexcept;
    void Step(float deltaSeconds) noexcept;

    const TogglePresentation& Presentation() const noexcept {
        return presentation_;
    }

private:
    Tween1D progress_{0.0f};
    float progressTarget_ = 0.0f;
    TogglePresentation presentation_{};
};

struct SliderPresentation {
    float thumbSizePx = 18.0f;
};

class SliderMotion {
public:
    SliderMotion() noexcept;

    // Slider semantic value is deliberately absent from this API.
    // The real GlassSlider remains pointer-driven with zero animation latency.
    void Sync(ControlInteractionState state) noexcept;
    void Step(float deltaSeconds) noexcept;

    const SliderPresentation& Presentation() const noexcept {
        return presentation_;
    }

private:
    Spring1D thumbSize_{18.0f};
    float thumbTarget_ = 18.0f;
    SliderPresentation presentation_{};
};

} // namespace AuroraGlass::Motion