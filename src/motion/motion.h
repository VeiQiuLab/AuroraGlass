#pragma once
// ============================================================
// AuroraGlass P4 Motion Layer - small CPU-only motion primitives.
//
// This module is intentionally independent of Core, Controls, D3D, HWND,
// layout, input routing, and host-framework concerns.
// ============================================================

namespace AuroraGlass::Motion {

enum class TweenCurve {
    Linear,
    SmoothStep,
};

class Tween1D {
public:
    Tween1D() noexcept = default;
    explicit Tween1D(float value) noexcept;

    void Snap(float value) noexcept;

    void Start(
        float from,
        float to,
        float durationSeconds,
        TweenCurve curve = TweenCurve::SmoothStep) noexcept;

    // Starts a new tween from the CURRENT value. This preserves continuity
    // when a host changes its target while a previous tween is in flight.
    void Retarget(
        float target,
        float durationSeconds,
        TweenCurve curve = TweenCurve::SmoothStep) noexcept;

    void Step(float deltaSeconds) noexcept;

    float Value() const noexcept { return value_; }
    float Target() const noexcept { return target_; }
    bool Active() const noexcept { return active_; }

private:
    float start_ = 0.0f;
    float target_ = 0.0f;
    float value_ = 0.0f;
    float elapsed_ = 0.0f;
    float duration_ = 0.0f;
    TweenCurve curve_ = TweenCurve::SmoothStep;
    bool active_ = false;
};

class Spring1D {
public:
    Spring1D() noexcept = default;
    explicit Spring1D(float value) noexcept;

    void Snap(float value) noexcept;

    // Approximate settle response. Internally this is a critically damped
    // analytic spring, so stepping is deterministic and stable across
    // reasonable frame segmentation.
    void SetResponseSeconds(float seconds) noexcept;
    float ResponseSeconds() const noexcept { return responseSeconds_; }

    // Retargeting preserves current value and velocity.
    void SetTarget(float target) noexcept;

    void Step(float deltaSeconds) noexcept;

    float Value() const noexcept { return value_; }
    float Velocity() const noexcept { return velocity_; }
    float Target() const noexcept { return target_; }

    bool IsSettled(
        float valueEpsilon = 0.0001f,
        float velocityEpsilon = 0.0001f) const noexcept;

private:
    float value_ = 0.0f;
    float velocity_ = 0.0f;
    float target_ = 0.0f;
    float responseSeconds_ = 0.20f;
};

} // namespace AuroraGlass::Motion