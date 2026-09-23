#include "motion/motion.h"

#include <algorithm>
#include <cmath>

namespace AuroraGlass::Motion {
namespace {

constexpr float kMinimumResponseSeconds = 0.0001f;

bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

float Clamp01(float value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

float ApplyCurve(float t, TweenCurve curve) noexcept {
    t = Clamp01(t);

    switch (curve) {
        case TweenCurve::Linear:
            return t;

        case TweenCurve::SmoothStep:
        default:
            return t * t * (3.0f - 2.0f * t);
    }
}

} // namespace

Tween1D::Tween1D(float value) noexcept {
    Snap(value);
}

void Tween1D::Snap(float value) noexcept {
    if (!IsFinite(value)) {
        value = 0.0f;
    }

    start_ = value;
    target_ = value;
    value_ = value;
    elapsed_ = 0.0f;
    duration_ = 0.0f;
    active_ = false;
}

void Tween1D::Start(
    float from,
    float to,
    float durationSeconds,
    TweenCurve curve) noexcept
{
    if (!IsFinite(from)) {
        from = 0.0f;
    }

    if (!IsFinite(to)) {
        to = from;
    }

    curve_ = curve;

    if (!IsFinite(durationSeconds) || durationSeconds <= 0.0f) {
        Snap(to);
        curve_ = curve;
        return;
    }

    start_ = from;
    target_ = to;
    value_ = from;
    elapsed_ = 0.0f;
    duration_ = durationSeconds;
    active_ = from != to;

    if (!active_) {
        value_ = target_;
    }
}

void Tween1D::Retarget(
    float target,
    float durationSeconds,
    TweenCurve curve) noexcept
{
    Start(value_, target, durationSeconds, curve);
}

void Tween1D::Step(float deltaSeconds) noexcept {
    if (!active_ || !IsFinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }

    elapsed_ = std::min(elapsed_ + deltaSeconds, duration_);

    const float t = elapsed_ / duration_;
    const float u = ApplyCurve(t, curve_);

    value_ = start_ + (target_ - start_) * u;

    if (elapsed_ >= duration_) {
        value_ = target_;
        active_ = false;
    }
}

Spring1D::Spring1D(float value) noexcept {
    Snap(value);
}

void Spring1D::Snap(float value) noexcept {
    if (!IsFinite(value)) {
        value = 0.0f;
    }

    value_ = value;
    target_ = value;
    velocity_ = 0.0f;
}

void Spring1D::SetResponseSeconds(float seconds) noexcept {
    if (!IsFinite(seconds) || seconds <= 0.0f) {
        return;
    }

    responseSeconds_ = std::max(seconds, kMinimumResponseSeconds);
}

void Spring1D::SetTarget(float target) noexcept {
    if (!IsFinite(target)) {
        return;
    }

    target_ = target;
}

void Spring1D::Step(float deltaSeconds) noexcept {
    if (!IsFinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }

    const float omega = 4.0f / responseSeconds_;
    const float displacement = value_ - target_;
    const float c2 = velocity_ + omega * displacement;
    const float decay = std::exp(-omega * deltaSeconds);

    const float nextDisplacement =
        (displacement + c2 * deltaSeconds) * decay;

    const float nextVelocity =
        (velocity_ - omega * c2 * deltaSeconds) * decay;

    value_ = target_ + nextDisplacement;
    velocity_ = nextVelocity;

    if (IsSettled()) {
        value_ = target_;
        velocity_ = 0.0f;
    }
}

bool Spring1D::IsSettled(
    float valueEpsilon,
    float velocityEpsilon) const noexcept
{
    if (!IsFinite(valueEpsilon) ||
        !IsFinite(velocityEpsilon) ||
        valueEpsilon < 0.0f ||
        velocityEpsilon < 0.0f) {
        return false;
    }

    return std::fabs(value_ - target_) <= valueEpsilon &&
           std::fabs(velocity_) <= velocityEpsilon;
}

} // namespace AuroraGlass::Motion