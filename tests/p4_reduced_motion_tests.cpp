#include "controls/control_toggle.h"
#include "motion/control_motion.h"

#include <cmath>
#include <cstdio>

using namespace AuroraGlass;
using namespace AuroraGlass::Motion;

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* name) {
    ++g_checks;

    if (!condition) {
        ++g_failures;
        std::printf("[FAIL] %s\n", name);
    }
}

void CheckNear(
    float actual,
    float expected,
    const char* name,
    float epsilon = 0.0001f)
{
    ++g_checks;

    if (std::fabs(actual - expected) > epsilon) {
        ++g_failures;
        std::printf(
            "[FAIL] %s actual=%.6f expected=%.6f\n",
            name,
            actual,
            expected);
    }
}

bool Between(float value, float a, float b) {
    const float lo = a < b ? a : b;
    const float hi = a < b ? b : a;
    return value > lo && value < hi;
}

ControlBounds MakeBounds() {
    ControlBounds bounds{};
    bounds.x = 10.0f;
    bounds.y = 20.0f;
    bounds.width = 100.0f;
    bounds.height = 50.0f;
    return bounds;
}

ControlPoint MakePoint(float x, float y) {
    ControlPoint point{};
    point.x = x;
    point.y = y;
    return point;
}

} // namespace

int main() {
    // 1. Button: reduced-motion Sync reaches exact presentation target.
    {
        ButtonMotion motion;
        motion.SetReducedMotion(true);

        motion.Sync(ControlInteractionState::Hover);
        CheckNear(motion.Presentation().scale, 1.010f, "button hover scale immediate");
        CheckNear(motion.Presentation().response, 0.35f, "button hover response immediate");

        motion.Sync(ControlInteractionState::Pressed);
        CheckNear(motion.Presentation().scale, 0.970f, "button pressed scale immediate");
        CheckNear(motion.Presentation().response, 1.00f, "button pressed response immediate");

        motion.Sync(ControlInteractionState::Focused);
        CheckNear(motion.Presentation().scale, 1.005f, "button focused scale immediate");
        CheckNear(motion.Presentation().response, 0.18f, "button focused response immediate");

        motion.Sync(ControlInteractionState::Normal);
        CheckNear(motion.Presentation().scale, 1.000f, "button normal scale immediate");
        CheckNear(motion.Presentation().response, 0.00f, "button normal response immediate");
    }

    // 2. Toggle: checked presentation is immediate 0/1.
    {
        ToggleMotion motion(false);
        motion.SetReducedMotion(true);

        motion.Sync(true);
        CheckNear(motion.Presentation().progress, 1.0f, "toggle on immediate");

        motion.Sync(false);
        CheckNear(motion.Presentation().progress, 0.0f, "toggle off immediate");
    }

    // 3. Slider: thumb presentation is immediate for every semantic state.
    {
        SliderMotion motion;
        motion.SetReducedMotion(true);

        motion.Sync(ControlInteractionState::Hover);
        CheckNear(motion.Presentation().thumbSizePx, 19.5f, "slider hover immediate");

        motion.Sync(ControlInteractionState::Pressed);
        CheckNear(motion.Presentation().thumbSizePx, 23.0f, "slider pressed immediate");

        motion.Sync(ControlInteractionState::Normal);
        CheckNear(motion.Presentation().thumbSizePx, 18.0f, "slider idle immediate");
    }

    // 4-5. Light-follow: pointer target and leave/rest are immediate.
    {
        LightFollowMotion motion;
        const ControlBounds bounds = MakeBounds();

        motion.SetReducedMotion(true);

        motion.RetargetPointer(
            MakePoint(110.0f, 70.0f),
            bounds);

        CheckNear(motion.Presentation().x, 1.0f, "light pointer x immediate");
        CheckNear(motion.Presentation().y, 1.0f, "light pointer y immediate");

        motion.RetargetPointer(
            MakePoint(10.0f, 20.0f),
            bounds);

        CheckNear(motion.Presentation().x, -1.0f, "light consecutive x snap");
        CheckNear(motion.Presentation().y, -1.0f, "light consecutive y snap");

        motion.RetargetRest();

        CheckNear(motion.Presentation().x, 0.50f, "light leave rest x immediate");
        CheckNear(motion.Presentation().y, 0.35f, "light leave rest y immediate");
    }

    // 6. Enabling reduced-motion during active tween converges in that call.
    {
        ButtonMotion motion;

        motion.Sync(ControlInteractionState::Hover);
        motion.Step(0.020f);

        Check(
            Between(motion.Presentation().scale, 1.0f, 1.010f),
            "button tween is in flight before enable");

        motion.SetReducedMotion(true);

        CheckNear(motion.Presentation().scale, 1.010f, "button mid-flight enable snaps scale");
        CheckNear(motion.Presentation().response, 0.35f, "button mid-flight enable snaps response");
    }

    // 7. Enabling during active spring snaps to target and clears residual velocity.
    //    Velocity is proven indirectly: after disabling reduced-motion again,
    //    stepping with no new target must not move the snapped presentation.
    {
        SliderMotion motion;

        motion.Sync(ControlInteractionState::Pressed);
        motion.Step(0.010f);

        Check(
            Between(motion.Presentation().thumbSizePx, 18.0f, 23.0f),
            "slider spring is in flight before enable");

        motion.SetReducedMotion(true);
        CheckNear(motion.Presentation().thumbSizePx, 23.0f, "slider mid-flight enable snaps target");

        motion.SetReducedMotion(false);
        motion.Step(1.0f);

        CheckNear(
            motion.Presentation().thumbSizePx,
            23.0f,
            "slider stale spring velocity cleared");
    }

    // 8. While reduced-motion remains enabled, every retarget is immediate.
    {
        ButtonMotion button;
        button.SetReducedMotion(true);

        button.Sync(ControlInteractionState::Hover);
        CheckNear(button.Presentation().scale, 1.010f, "reduced retarget hover immediate");

        button.Sync(ControlInteractionState::Pressed);
        CheckNear(button.Presentation().scale, 0.970f, "reduced retarget press immediate");

        button.Sync(ControlInteractionState::Focused);
        CheckNear(button.Presentation().scale, 1.005f, "reduced retarget focus immediate");

        ToggleMotion toggle(false);
        toggle.SetReducedMotion(true);

        toggle.Sync(true);
        CheckNear(toggle.Presentation().progress, 1.0f, "reduced toggle retarget on immediate");

        toggle.Sync(false);
        CheckNear(toggle.Presentation().progress, 0.0f, "reduced toggle retarget off immediate");
    }

    // 9-10. Disabling reduced-motion does not animate by itself.
    //       A later NEW target restores the normal animation path.
    {
        ToggleMotion motion(false);

        motion.SetReducedMotion(true);
        motion.Sync(true);

        CheckNear(motion.Presentation().progress, 1.0f, "toggle reduced baseline at target");

        motion.SetReducedMotion(false);
        motion.Step(0.500f);

        CheckNear(
            motion.Presentation().progress,
            1.0f,
            "disable reduced motion does not self animate");

        motion.Sync(false);

        CheckNear(
            motion.Presentation().progress,
            1.0f,
            "new normal retarget does not snap immediately");

        motion.Step(0.030f);

        Check(
            Between(motion.Presentation().progress, 0.0f, 1.0f),
            "normal tween resumes after new target");

        motion.Step(1.0f);

        CheckNear(
            motion.Presentation().progress,
            0.0f,
            "normal tween reaches new target");
    }

    // 10 also applies to light-follow: after disabling, a NEW pointer target
    // resumes the existing tween behavior rather than snapping.
    {
        LightFollowMotion motion;
        const ControlBounds bounds = MakeBounds();

        motion.SetReducedMotion(true);

        motion.RetargetPointer(
            MakePoint(110.0f, 45.0f),
            bounds);

        CheckNear(motion.Presentation().x, 1.0f, "light reduced baseline right");

        motion.SetReducedMotion(false);
        motion.Step(0.500f);

        CheckNear(
            motion.Presentation().x,
            1.0f,
            "light disable does not resurrect animation");

        motion.RetargetPointer(
            MakePoint(10.0f, 45.0f),
            bounds);

        CheckNear(
            motion.Presentation().x,
            1.0f,
            "light normal new target starts from current value");

        motion.Step(0.020f);

        Check(
            Between(motion.Presentation().x, -1.0f, 1.0f),
            "light normal tween resumes");

        motion.Step(1.0f);

        CheckNear(motion.Presentation().x, -1.0f, "light normal tween settles");
    }

    // 11. Rapid false -> true -> false must not restore stale tween state.
    {
        ToggleMotion motion(false);

        motion.Sync(true);
        motion.Step(0.030f);

        Check(
            Between(motion.Presentation().progress, 0.0f, 1.0f),
            "rapid toggle starts with active tween");

        motion.SetReducedMotion(true);

        CheckNear(
            motion.Presentation().progress,
            1.0f,
            "rapid enable snaps active tween");

        motion.SetReducedMotion(false);
        motion.Step(0.100f);

        CheckNear(
            motion.Presentation().progress,
            1.0f,
            "rapid disable does not resume stale tween");

        motion.Sync(false);
        motion.Step(0.020f);

        Check(
            Between(motion.Presentation().progress, 0.0f, 1.0f),
            "rapid sequence accepts fresh normal retarget");
    }

    // 12. P3 semantic truth remains authoritative and is not mutated by P4.
    {
        GlassToggle semantic;
        semantic.SetEnabled(true);
        semantic.SetChecked(true);

        ToggleMotion presentation(false);
        presentation.SetReducedMotion(true);
        presentation.Sync(semantic.IsChecked());

        Check(semantic.IsChecked(), "P3 semantic remains checked");
        CheckNear(presentation.Presentation().progress, 1.0f, "presentation follows P3 semantic");

        presentation.Sync(false);

        Check(
            semantic.IsChecked(),
            "presentation retarget cannot mutate P3 semantic truth");
    }

    // Idempotent repeated enable must remain deterministic.
    {
        SliderMotion motion;
        motion.Sync(ControlInteractionState::Pressed);
        motion.Step(0.010f);

        motion.SetReducedMotion(true);
        motion.SetReducedMotion(true);

        CheckNear(
            motion.Presentation().thumbSizePx,
            23.0f,
            "repeated reduced enable remains snapped");

        motion.Step(1.0f);

        CheckNear(
            motion.Presentation().thumbSizePx,
            23.0f,
            "reduced step remains stationary");
    }

    if (g_failures != 0) {
        std::printf(
            "P4_REDUCED_MOTION_TESTS=FAIL checks=%d failures=%d\n",
            g_checks,
            g_failures);

        return 1;
    }

    std::printf(
        "P4_REDUCED_MOTION_TESTS=PASS checks=%d failures=0\n",
        g_checks);

    return 0;
}