#include "controls/control_button.h"
#include "controls/control_slider.h"
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

static bool Near(float a, float b, float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
}

static void Advance(
    LightFollowMotion& motion,
    int steps,
    float dt = 0.016f)
{
    for (int i = 0; i < steps; ++i)
        motion.Step(dt);
}

static void TestCoordinateNormalization()
{
    std::printf("[A] pointer normalization\n");

    LightFollowMotion motion;
    ControlBounds bounds{100.0f, 50.0f, 200.0f, 100.0f};

    motion.RetargetPointer(
        ControlPoint{100.0f, 50.0f},
        bounds);

    Advance(motion, 10);

    CHECK(Near(motion.Presentation().x, -1.0f));
    CHECK(Near(motion.Presentation().y, -1.0f));

    motion.RetargetPointer(
        ControlPoint{200.0f, 100.0f},
        bounds);

    Advance(motion, 10);

    CHECK(Near(motion.Presentation().x, 0.0f));
    CHECK(Near(motion.Presentation().y, 0.0f));

    motion.RetargetPointer(
        ControlPoint{350.0f, 180.0f},
        bounds);

    Advance(motion, 10);

    CHECK(Near(motion.Presentation().x, 1.0f));
    CHECK(Near(motion.Presentation().y, 1.0f));
}

static void TestEnterAndMove()
{
    std::printf("[B] enter and move across control\n");

    LightFollowMotion motion;
    ControlBounds bounds{0.0f, 0.0f, 100.0f, 100.0f};

    CHECK(Near(motion.Presentation().x, 0.50f));
    CHECK(Near(motion.Presentation().y, 0.35f));

    motion.RetargetPointer(
        ControlPoint{10.0f, 20.0f},
        bounds);

    const float before =
        motion.Presentation().x;

    motion.Step(0.030f);

    const float entered =
        motion.Presentation().x;

    CHECK(entered < before);
    CHECK(entered > -0.80f);

    motion.RetargetPointer(
        ControlPoint{90.0f, 80.0f},
        bounds);

    const float beforeMove =
        motion.Presentation().x;

    CHECK(Near(beforeMove, entered));

    motion.Step(0.030f);

    CHECK(motion.Presentation().x > beforeMove);
}

static void TestRapidDirectionReversal()
{
    std::printf("[C] rapid direction reversal\n");

    LightFollowMotion motion;
    ControlBounds bounds{0.0f, 0.0f, 100.0f, 100.0f};

    motion.RetargetPointer(
        ControlPoint{95.0f, 50.0f},
        bounds);

    motion.Step(0.035f);

    const float movingRight =
        motion.Presentation().x;

    CHECK(movingRight > 0.50f);
    CHECK(movingRight < 0.90f);

    motion.RetargetPointer(
        ControlPoint{5.0f, 50.0f},
        bounds);

    const float beforeReverse =
        motion.Presentation().x;

    CHECK(Near(beforeReverse, movingRight));

    motion.Step(0.030f);

    CHECK(motion.Presentation().x < beforeReverse);
}

static void TestLeaveAndConvergence()
{
    std::printf("[D] leave and final convergence\n");

    LightFollowMotion motion;
    ControlBounds bounds{10.0f, 20.0f, 200.0f, 80.0f};

    motion.RetargetPointer(
        ControlPoint{20.0f, 25.0f},
        bounds);

    motion.Step(0.030f);

    const float beforeLeaveX =
        motion.Presentation().x;

    const float beforeLeaveY =
        motion.Presentation().y;

    motion.RetargetRest();

    CHECK(Near(
        motion.Presentation().x,
        beforeLeaveX));

    CHECK(Near(
        motion.Presentation().y,
        beforeLeaveY));

    Advance(motion, 10);

    CHECK(Near(motion.Presentation().x, 0.50f));
    CHECK(Near(motion.Presentation().y, 0.35f));
}

static void TestSemanticIndependence()
{
    std::printf("[E] semantic state remains independent\n");

    GlassButton button;
    button.bounds = {0.0f, 0.0f, 120.0f, 48.0f};

    LightFollowMotion motion;

    const ControlPoint pointer{30.0f, 24.0f};

    button.PointerMove(pointer);

    CHECK(
        button.State() ==
        ControlInteractionState::Hover);

    motion.RetargetPointer(
        pointer,
        button.bounds);

    const auto stateBefore =
        button.State();

    Advance(motion, 8);

    CHECK(button.State() == stateBefore);

    GlassSlider slider;
    slider.bounds = {0.0f, 0.0f, 200.0f, 40.0f};
    slider.SetValue(0.40f);

    const float valueBefore =
        slider.Value();

    motion.RetargetPointer(
        ControlPoint{150.0f, 20.0f},
        slider.bounds);

    Advance(motion, 8);

    CHECK(Near(slider.Value(), valueBefore));
}

static void TestResizeNormalization()
{
    std::printf("[F] resize normalization\n");

    LightFollowMotion motion;

    ControlBounds before{
        100.0f,
        100.0f,
        200.0f,
        100.0f
    };

    motion.RetargetPointer(
        ControlPoint{250.0f, 125.0f},
        before);

    Advance(motion, 10);

    CHECK(Near(motion.Presentation().x, 0.50f));
    CHECK(Near(motion.Presentation().y, -0.50f));

    ControlBounds after{
        100.0f,
        100.0f,
        400.0f,
        200.0f
    };

    motion.RetargetPointer(
        ControlPoint{250.0f, 125.0f},
        after);

    const float oldX =
        motion.Presentation().x;

    motion.Step(0.030f);

    CHECK(motion.Presentation().x < oldX);

    Advance(motion, 10);

    CHECK(Near(motion.Presentation().x, -0.25f));
    CHECK(Near(motion.Presentation().y, -0.75f));
}

static void TestDeterministicStepping()
{
    std::printf("[G] deterministic stepping\n");

    LightFollowMotion a;
    LightFollowMotion b;

    ControlBounds bounds{0.0f, 0.0f, 300.0f, 120.0f};
    ControlPoint pointer{240.0f, 30.0f};

    a.RetargetPointer(pointer, bounds);
    b.RetargetPointer(pointer, bounds);

    const float steps[] = {
        0.010f,
        0.016f,
        0.008f,
        0.020f,
        0.012f
    };

    for (float dt : steps)
    {
        a.Step(dt);
        b.Step(dt);
    }

    CHECK(Near(
        a.Presentation().x,
        b.Presentation().x));

    CHECK(Near(
        a.Presentation().y,
        b.Presentation().y));
}

int main()
{
    std::printf(
        "AuroraGlass P4 highlight/light-follow tests\n");

    TestCoordinateNormalization();
    TestEnterAndMove();
    TestRapidDirectionReversal();
    TestLeaveAndConvergence();
    TestSemanticIndependence();
    TestResizeNormalization();
    TestDeterministicStepping();

    std::printf(
        "\nChecks: %d, Failures: %d\n",
        g_checks,
        g_failures);

    std::printf(
        "%s\n",
        g_failures == 0
            ? "RESULT: PASS"
            : "RESULT: FAIL");

    return g_failures == 0 ? 0 : 1;
}