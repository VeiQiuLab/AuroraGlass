#include "controls/control_button.h"
#include "controls/control_slider.h"
#include "controls/control_toggle.h"
#include "motion/control_motion.h"

#include <cmath>
#include <cstdio>

using namespace AuroraGlass;
using namespace AuroraGlass::Motion;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                    \
    } while (0)

static bool Near(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

static ControlPoint Center(const ControlBounds& b) {
    return {
        b.x + b.width * 0.5f,
        b.y + b.height * 0.5f
    };
}

static void TestButtonMotion() {
    std::printf("[A] button semantic state -> motion presentation\n");

    GlassButton button;
    button.bounds = {10.0f, 10.0f, 120.0f, 44.0f};

    ButtonMotion motion;

    motion.Sync(button.State());
    motion.Step(0.016f);
    CHECK(Near(motion.Presentation().scale, 1.0f));

    const ControlPoint inside = Center(button.bounds);

    button.PointerMove(inside);
    CHECK(button.State() == ControlInteractionState::Hover);

    motion.Sync(button.State());
    const float hoverStart = motion.Presentation().scale;
    motion.Step(0.030f);
    const float hoverMoving = motion.Presentation().scale;

    CHECK(hoverMoving > hoverStart);
    CHECK(hoverMoving < 1.011f);

    button.PointerDown(inside);
    CHECK(button.State() == ControlInteractionState::Pressed);

    motion.Sync(button.State());

    const float beforePressStep = motion.Presentation().scale;
    CHECK(Near(beforePressStep, hoverMoving));

    motion.Step(0.025f);
    const float pressedMoving = motion.Presentation().scale;

    CHECK(pressedMoving < beforePressStep);

    button.PointerUp(inside);
    CHECK(button.State() == ControlInteractionState::Hover);

    motion.Sync(button.State());

    const float beforeReleaseStep = motion.Presentation().scale;
    CHECK(Near(beforeReleaseStep, pressedMoving));

    motion.Step(0.020f);
    CHECK(motion.Presentation().scale > beforeReleaseStep);

    // Rapid re-press before release motion finishes.
    button.PointerDown(inside);
    motion.Sync(button.State());

    const float beforeRapidRepress = motion.Presentation().scale;
    motion.Step(0.020f);

    CHECK(motion.Presentation().scale < beforeRapidRepress);
}

static void TestToggleRetarget() {
    std::printf("[B] toggle semantic bool -> interruptible progress\n");

    GlassToggle toggle;
    ToggleMotion motion(toggle.IsChecked());

    CHECK(!toggle.IsChecked());
    CHECK(Near(motion.Presentation().progress, 0.0f));

    toggle.SetChecked(true);
    CHECK(toggle.IsChecked());

    motion.Sync(toggle.IsChecked());
    motion.Step(0.050f);

    const float towardOn = motion.Presentation().progress;

    CHECK(towardOn > 0.0f);
    CHECK(towardOn < 1.0f);

    toggle.SetChecked(false);
    CHECK(!toggle.IsChecked());

    motion.Sync(toggle.IsChecked());

    const float beforeOffStep = motion.Presentation().progress;
    CHECK(Near(beforeOffStep, towardOn));

    motion.Step(0.030f);
    const float towardOff = motion.Presentation().progress;

    CHECK(towardOff < beforeOffStep);

    // Interrupt again: Off -> On before the previous visual transition settles.
    toggle.SetChecked(true);
    CHECK(toggle.IsChecked());

    motion.Sync(toggle.IsChecked());

    const float beforeSecondOn = motion.Presentation().progress;
    CHECK(Near(beforeSecondOn, towardOff));

    motion.Step(0.030f);
    CHECK(motion.Presentation().progress > beforeSecondOn);

    for (int i = 0; i < 20; ++i) {
        motion.Step(0.016f);
    }

    CHECK(Near(motion.Presentation().progress, 1.0f));
}

static void TestSliderPresentationAndSemanticLatency() {
    std::printf("[C] slider direct semantic value + animated thumb\n");

    GlassSlider slider;
    slider.bounds = {20.0f, 20.0f, 200.0f, 40.0f};
    slider.SetValue(0.25f);

    SliderMotion motion;

    const ControlPoint thumb{
        slider.ThumbCenterX(),
        slider.bounds.y + slider.bounds.height * 0.5f
    };

    slider.PointerMove(thumb);
    motion.Sync(slider.State());
    motion.Step(0.020f);

    CHECK(motion.Presentation().thumbSizePx > 18.0f);

    slider.PointerDown(thumb);
    CHECK(slider.State() == ControlInteractionState::Pressed);

    motion.Sync(slider.State());

    const float beforePressStep =
        motion.Presentation().thumbSizePx;

    motion.Step(0.025f);

    const float pressedSize =
        motion.Presentation().thumbSizePx;

    CHECK(pressedSize > beforePressStep);
    CHECK(pressedSize < 23.01f);

    // Pointer-driven semantic value must change immediately.
    const ControlPoint drag{
        slider.bounds.x + slider.bounds.width * 0.80f,
        thumb.y
    };

    slider.PointerMove(drag);

    CHECK(Near(slider.Value(), 0.80f, 0.001f));
    CHECK(Near(
        slider.ThumbCenterX(),
        slider.bounds.x + slider.bounds.width * 0.80f,
        0.01f));

    // Motion state only affects presentation size, never semantic value.
    const float valueBeforeMotionStep = slider.Value();
    motion.Step(0.016f);
    CHECK(Near(slider.Value(), valueBeforeMotionStep));

    slider.PointerUp(drag);
    motion.Sync(slider.State());

    const float beforeReleaseStep =
        motion.Presentation().thumbSizePx;

    motion.Step(0.025f);

    const float releasingSize =
        motion.Presentation().thumbSizePx;

    CHECK(releasingSize < beforeReleaseStep);
    CHECK(Near(slider.Value(), valueBeforeMotionStep));

    // Rapid press again while release motion is still in flight.
    ControlPoint currentThumb{
        slider.ThumbCenterX(),
        thumb.y
    };

    slider.PointerDown(currentThumb);
    motion.Sync(slider.State());

    const float beforeRapidPress =
        motion.Presentation().thumbSizePx;

    motion.Step(0.020f);

    CHECK(motion.Presentation().thumbSizePx > beforeRapidPress);
    CHECK(Near(slider.Value(), valueBeforeMotionStep));
}

static void TestDeterministicStepping() {
    std::printf("[D] deterministic stepping\n");

    ButtonMotion a;
    ButtonMotion b;

    a.Sync(ControlInteractionState::Pressed);
    b.Sync(ControlInteractionState::Pressed);

    const float steps[] = {
        0.010f,
        0.016f,
        0.008f,
        0.020f,
        0.012f
    };

    for (float dt : steps) {
        a.Step(dt);
        b.Step(dt);
    }

    CHECK(Near(
        a.Presentation().scale,
        b.Presentation().scale));

    CHECK(Near(
        a.Presentation().response,
        b.Presentation().response));

    ToggleMotion ta(false);
    ToggleMotion tb(false);

    ta.Sync(true);
    tb.Sync(true);

    for (float dt : steps) {
        ta.Step(dt);
        tb.Step(dt);
    }

    CHECK(Near(
        ta.Presentation().progress,
        tb.Presentation().progress));
}

int main() {
    std::printf("AuroraGlass P4 Slice B motion integration tests\n");

    TestButtonMotion();
    TestToggleRetarget();
    TestSliderPresentationAndSemanticLatency();
    TestDeterministicStepping();

    std::printf(
        "\nChecks: %d, Failures: %d\n",
        g_checks,
        g_failures);

    std::printf(
        "%s\n",
        g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");

    return g_failures == 0 ? 0 : 1;
}