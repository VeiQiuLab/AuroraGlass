#include "motion/motion.h"

#include <cmath>
#include <cstdio>
#include <limits>

using AuroraGlass::Motion::Spring1D;
using AuroraGlass::Motion::Tween1D;
using AuroraGlass::Motion::TweenCurve;

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

static void TestLinearTween() {
    std::printf("[A] linear tween\n");

    Tween1D t;
    t.Start(0.0f, 10.0f, 1.0f, TweenCurve::Linear);

    CHECK(t.Active());
    CHECK(Near(t.Value(), 0.0f));
    CHECK(Near(t.Target(), 10.0f));

    t.Step(0.25f);
    CHECK(Near(t.Value(), 2.5f));

    t.Step(0.25f);
    CHECK(Near(t.Value(), 5.0f));

    t.Step(0.50f);
    CHECK(Near(t.Value(), 10.0f));
    CHECK(!t.Active());
}

static void TestSmoothStepTween() {
    std::printf("[B] smooth-step tween\n");

    Tween1D t;
    t.Start(0.0f, 1.0f, 1.0f, TweenCurve::SmoothStep);

    t.Step(0.25f);
    CHECK(Near(t.Value(), 0.15625f));

    t.Step(0.25f);
    CHECK(Near(t.Value(), 0.5f));

    t.Step(0.50f);
    CHECK(Near(t.Value(), 1.0f));
    CHECK(!t.Active());
}

static void TestTweenRetargetContinuity() {
    std::printf("[C] tween retarget continuity\n");

    Tween1D t;
    t.Start(0.0f, 1.0f, 1.0f, TweenCurve::Linear);
    t.Step(0.50f);

    CHECK(Near(t.Value(), 0.5f));

    t.Retarget(0.0f, 0.50f, TweenCurve::Linear);

    CHECK(Near(t.Value(), 0.5f));

    t.Step(0.25f);
    CHECK(Near(t.Value(), 0.25f));

    t.Step(0.25f);
    CHECK(Near(t.Value(), 0.0f));
    CHECK(!t.Active());
}

static void TestTweenBoundaryBehavior() {
    std::printf("[D] tween boundary behavior\n");

    Tween1D t(2.0f);

    t.Start(2.0f, 7.0f, 0.0f, TweenCurve::Linear);
    CHECK(Near(t.Value(), 7.0f));
    CHECK(!t.Active());

    t.Start(0.0f, 1.0f, 1.0f, TweenCurve::Linear);
    t.Step(-1.0f);
    CHECK(Near(t.Value(), 0.0f));

    t.Step(std::numeric_limits<float>::quiet_NaN());
    CHECK(Near(t.Value(), 0.0f));
    CHECK(t.Active());
}

static void TestSpringConvergence() {
    std::printf("[E] spring convergence\n");

    Spring1D s(0.0f);
    s.SetResponseSeconds(0.25f);
    s.SetTarget(1.0f);

    for (int i = 0; i < 240; ++i) {
        s.Step(1.0f / 120.0f);
    }

    CHECK(Near(s.Value(), 1.0f, 0.001f));
    CHECK(Near(s.Velocity(), 0.0f, 0.001f));
    CHECK(s.IsSettled(0.001f, 0.001f));
}

static void TestSpringRetargetContinuity() {
    std::printf("[F] spring retarget continuity\n");

    Spring1D s(0.0f);
    s.SetResponseSeconds(0.30f);
    s.SetTarget(1.0f);

    for (int i = 0; i < 15; ++i) {
        s.Step(1.0f / 120.0f);
    }

    const float valueBefore = s.Value();
    const float velocityBefore = s.Velocity();

    s.SetTarget(-0.5f);

    CHECK(Near(s.Value(), valueBefore));
    CHECK(Near(s.Velocity(), velocityBefore));

    for (int i = 0; i < 300; ++i) {
        s.Step(1.0f / 120.0f);
    }

    CHECK(Near(s.Value(), -0.5f, 0.001f));
    CHECK(Near(s.Velocity(), 0.0f, 0.001f));
}

static void TestSpringFrameSegmentation() {
    std::printf("[G] spring frame segmentation\n");

    Spring1D oneStep(0.0f);
    oneStep.SetResponseSeconds(0.30f);
    oneStep.SetTarget(1.0f);
    oneStep.Step(0.10f);

    Spring1D tenSteps(0.0f);
    tenSteps.SetResponseSeconds(0.30f);
    tenSteps.SetTarget(1.0f);

    for (int i = 0; i < 10; ++i) {
        tenSteps.Step(0.01f);
    }

    CHECK(Near(oneStep.Value(), tenSteps.Value(), 0.0001f));
    CHECK(Near(oneStep.Velocity(), tenSteps.Velocity(), 0.0001f));
}

static void TestSpringBoundaryBehavior() {
    std::printf("[H] spring boundary behavior\n");

    Spring1D s(3.0f);

    const float originalResponse = s.ResponseSeconds();
    s.SetResponseSeconds(-1.0f);
    CHECK(Near(s.ResponseSeconds(), originalResponse));

    s.SetResponseSeconds(std::numeric_limits<float>::quiet_NaN());
    CHECK(Near(s.ResponseSeconds(), originalResponse));

    s.SetTarget(5.0f);
    s.Step(std::numeric_limits<float>::quiet_NaN());
    CHECK(Near(s.Value(), 3.0f));

    s.Snap(5.0f);
    CHECK(Near(s.Value(), 5.0f));
    CHECK(Near(s.Target(), 5.0f));
    CHECK(Near(s.Velocity(), 0.0f));
}

int main() {
    std::printf("AuroraGlass P4 Slice A motion tests\n");

    TestLinearTween();
    TestSmoothStepTween();
    TestTweenRetargetContinuity();
    TestTweenBoundaryBehavior();
    TestSpringConvergence();
    TestSpringRetargetContinuity();
    TestSpringFrameSegmentation();
    TestSpringBoundaryBehavior();

    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");

    return g_failures == 0 ? 0 : 1;
}