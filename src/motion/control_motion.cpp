#include "motion/control_motion.h"

namespace AuroraGlass::Motion {
namespace {

// Initial restrained reference values for Slice B.
// These are presentation values, not public material calibration.
constexpr float kButtonHoverScale = 1.010f;
constexpr float kButtonPressedScale = 0.970f;
constexpr float kButtonFocusedScale = 1.005f;

constexpr float kButtonHoverResponse = 0.35f;
constexpr float kButtonPressedResponse = 1.00f;
constexpr float kButtonFocusedResponse = 0.18f;

constexpr float kButtonScaleTweenSeconds = 0.070f;
constexpr float kButtonResponseTweenSeconds = 0.090f;

constexpr float kToggleTweenSeconds = 0.140f;

constexpr float kLightRestX = 0.50f;
constexpr float kLightRestY = 0.35f;
constexpr float kLightFollowSeconds = 0.080f;

constexpr float kSliderIdlePx = 18.0f;
constexpr float kSliderHoverPx = 19.5f;
constexpr float kSliderPressedPx = 23.0f;
constexpr float kSliderSpringResponseSeconds = 0.075f;

float ButtonScaleTarget(ControlInteractionState state) noexcept {
    switch (state) {
        case ControlInteractionState::Hover:
            return kButtonHoverScale;
        case ControlInteractionState::Pressed:
            return kButtonPressedScale;
        case ControlInteractionState::Focused:
            return kButtonFocusedScale;
        case ControlInteractionState::Normal:
        case ControlInteractionState::Disabled:
        default:
            return 1.0f;
    }
}

float ButtonResponseTarget(ControlInteractionState state) noexcept {
    switch (state) {
        case ControlInteractionState::Hover:
            return kButtonHoverResponse;
        case ControlInteractionState::Pressed:
            return kButtonPressedResponse;
        case ControlInteractionState::Focused:
            return kButtonFocusedResponse;
        case ControlInteractionState::Normal:
        case ControlInteractionState::Disabled:
        default:
            return 0.0f;
    }
}

float SliderThumbTarget(ControlInteractionState state) noexcept {
    switch (state) {
        case ControlInteractionState::Pressed:
            return kSliderPressedPx;
        case ControlInteractionState::Hover:
        case ControlInteractionState::Focused:
            return kSliderHoverPx;
        case ControlInteractionState::Normal:
        case ControlInteractionState::Disabled:
        default:
            return kSliderIdlePx;
    }
}

} // namespace

ButtonMotion::ButtonMotion() noexcept {
    presentation_.scale = scale_.Value();
    presentation_.response = response_.Value();
}

void ButtonMotion::RetargetScale(float target) noexcept {
    if (target == scaleTarget_) {
        return;
    }

    scaleTarget_ = target;

    // Button press/release must reverse immediately when semantic input changes.
    // Tween retarget starts from the CURRENT presentation value, so there is no
    // snap to an old origin and no residual spring velocity after release.
    scale_.Retarget(
        target,
        kButtonScaleTweenSeconds,
        TweenCurve::SmoothStep);
}

void ButtonMotion::RetargetResponse(float target) noexcept {
    if (target == responseTarget_) {
        return;
    }

    responseTarget_ = target;
    response_.Retarget(
        target,
        kButtonResponseTweenSeconds,
        TweenCurve::SmoothStep);
}

void ButtonMotion::Sync(ControlInteractionState state) noexcept {
    RetargetScale(ButtonScaleTarget(state));
    RetargetResponse(ButtonResponseTarget(state));
}

void ButtonMotion::Step(float deltaSeconds) noexcept {
    scale_.Step(deltaSeconds);
    response_.Step(deltaSeconds);

    presentation_.scale = scale_.Value();
    presentation_.response = response_.Value();
}

ToggleMotion::ToggleMotion(bool checked) noexcept {
    const float value = checked ? 1.0f : 0.0f;
    progress_.Snap(value);
    progressTarget_ = value;
    presentation_.progress = value;
}

void ToggleMotion::Sync(bool checked) noexcept {
    const float target = checked ? 1.0f : 0.0f;

    if (target == progressTarget_) {
        return;
    }

    progressTarget_ = target;

    progress_.Retarget(
        target,
        kToggleTweenSeconds,
        TweenCurve::SmoothStep);
}

void ToggleMotion::Step(float deltaSeconds) noexcept {
    progress_.Step(deltaSeconds);
    presentation_.progress = progress_.Value();
}

LightFollowMotion::LightFollowMotion() noexcept {
    presentation_.x = x_.Value();
    presentation_.y = y_.Value();
}

void LightFollowMotion::Retarget(float x, float y) noexcept {
    if (x != targetX_) {
        targetX_ = x;
        x_.Retarget(
            targetX_,
            kLightFollowSeconds,
            TweenCurve::SmoothStep);
    }

    if (y != targetY_) {
        targetY_ = y;
        y_.Retarget(
            targetY_,
            kLightFollowSeconds,
            TweenCurve::SmoothStep);
    }
}

void LightFollowMotion::RetargetPointer(
    ControlPoint pointer,
    const ControlBounds& bounds) noexcept
{
    if (!bounds.IsValid() ||
        bounds.IsEmpty())
    {
        RetargetRest();
        return;
    }

    float nx =
        ((pointer.x - bounds.x) / bounds.width) *
        2.0f - 1.0f;

    float ny =
        ((pointer.y - bounds.y) / bounds.height) *
        2.0f - 1.0f;

    if (nx < -1.0f) nx = -1.0f;
    if (nx >  1.0f) nx =  1.0f;
    if (ny < -1.0f) ny = -1.0f;
    if (ny >  1.0f) ny =  1.0f;

    Retarget(nx, ny);
}

void LightFollowMotion::RetargetRest() noexcept {
    Retarget(
        kLightRestX,
        kLightRestY);
}

void LightFollowMotion::Step(float deltaSeconds) noexcept {
    x_.Step(deltaSeconds);
    y_.Step(deltaSeconds);

    presentation_.x = x_.Value();
    presentation_.y = y_.Value();
}

SliderMotion::SliderMotion() noexcept {
    thumbSize_.SetResponseSeconds(kSliderSpringResponseSeconds);
    presentation_.thumbSizePx = thumbSize_.Value();
}

void SliderMotion::Sync(ControlInteractionState state) noexcept {
    const float target = SliderThumbTarget(state);

    if (target == thumbTarget_) {
        return;
    }

    thumbTarget_ = target;
    thumbSize_.SetTarget(target);
}

void SliderMotion::Step(float deltaSeconds) noexcept {
    thumbSize_.Step(deltaSeconds);
    presentation_.thumbSizePx = thumbSize_.Value();
}

} // namespace AuroraGlass::Motion