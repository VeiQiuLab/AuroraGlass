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

    void SetReducedMotion(bool enabled) noexcept;
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

    bool reducedMotion_ = false;
    ButtonPresentation presentation_{};
};

struct TogglePresentation {
    float progress = 0.0f;
};

class ToggleMotion {
public:
    ToggleMotion() noexcept = default;
    explicit ToggleMotion(bool checked) noexcept;

    void SetReducedMotion(bool enabled) noexcept;

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
    bool reducedMotion_ = false;
    TogglePresentation presentation_{};
};

struct LightFollowPresentation {
    // Same normalized highlight coordinate contract as GlassMaterial:
    // [-1, 1] on each axis. This type does not depend on GlassMaterial.
    float x = 0.50f;
    float y = 0.35f;
};

class LightFollowMotion {
public:
    LightFollowMotion() noexcept;

    void SetReducedMotion(bool enabled) noexcept;

    // Host-owned pointer position is projected into control-local normalized
    // coordinates. This does not perform input dispatch or mutate semantics.
    void RetargetPointer(
        ControlPoint pointer,
        const ControlBounds& bounds) noexcept;

    // Pointer ownership/lifetime remains with the host. On leave, presentation
    // returns to the existing GlassMaterial default highlight position.
    void RetargetRest() noexcept;

    void Step(float deltaSeconds) noexcept;

    const LightFollowPresentation& Presentation() const noexcept {
        return presentation_;
    }

private:
    void Retarget(float x, float y) noexcept;

    Tween1D x_{0.50f};
    Tween1D y_{0.35f};

    float targetX_ = 0.50f;
    float targetY_ = 0.35f;

    bool reducedMotion_ = false;
    LightFollowPresentation presentation_{};
};

struct SliderPresentation {
    float thumbSizePx = 18.0f;
};

class SliderMotion {
public:
    SliderMotion() noexcept;

    void SetReducedMotion(bool enabled) noexcept;

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
    bool reducedMotion_ = false;
    SliderPresentation presentation_{};
};

} // namespace AuroraGlass::Motion