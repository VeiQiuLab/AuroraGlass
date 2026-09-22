// ============================================================
// AuroraGlass P3 Slice A — Controls geometry / state / input tests.
//
// PURE CPU. No D3D device, no HWND, no Core dependency.
// ============================================================

#include "controls/control_geometry.h"
#include "controls/control_interaction.h"

#include <cstdio>
#include <limits>

using namespace AuroraGlass;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

static ControlBounds Rect(float x, float y, float w, float h) {
    ControlBounds b; b.x = x; b.y = y; b.width = w; b.height = h; return b;
}

static void TestGeometryBounds() {
    std::printf("[A] geometry / bounds\n");
    ControlBounds b = Rect(10, 20, 100, 50);
    CHECK(b.Left() == 10.0f && b.Top() == 20.0f);
    CHECK(b.Right() == 110.0f && b.Bottom() == 70.0f);
    CHECK(b.IsValid());
    CHECK(!b.IsEmpty());

    // Zero-size is valid but empty.
    ControlBounds z = Rect(0, 0, 0, 0);
    CHECK(z.IsValid());
    CHECK(z.IsEmpty());

    // Negative dimension is invalid.
    ControlBounds n = Rect(0, 0, -1, 5);
    CHECK(!n.IsValid());
    CHECK(n.IsEmpty());

    // NaN dimension is invalid (NaN >= 0 is false).
    ControlBounds nan = Rect(0, 0, std::numeric_limits<float>::quiet_NaN(), 5);
    CHECK(!nan.IsValid());
}

static void TestHitTestRect() {
    std::printf("[B] rectangle hit-test\n");
    ControlBounds b = Rect(10, 20, 100, 50); // [10,110) x [20,70)

    // Interior.
    CHECK(HitTestRect(b, {50, 45}));
    // Left/top edges inclusive.
    CHECK(HitTestRect(b, {10, 20}));
    CHECK(HitTestRect(b, {10, 45}));
    CHECK(HitTestRect(b, {50, 20}));
    // Right/bottom edges exclusive.
    CHECK(!HitTestRect(b, {110, 45}));
    CHECK(!HitTestRect(b, {50, 70}));
    // Just inside near the exclusive edges.
    CHECK(HitTestRect(b, {109.999f, 69.999f}));
    // Exterior.
    CHECK(!HitTestRect(b, {9.999f, 45}));
    CHECK(!HitTestRect(b, {50, 19.999f}));
    CHECK(!HitTestRect(b, {-100, -100}));

    // Zero-size contains nothing.
    CHECK(!HitTestRect(Rect(0, 0, 0, 10), {0, 5}));
    CHECK(!HitTestRect(Rect(0, 0, 10, 0), {5, 0}));
    // Invalid bounds contain nothing.
    CHECK(!HitTestRect(Rect(0, 0, -1, 10), {0, 5}));
    // NaN point rejected.
    CHECK(!HitTestRect(b, {std::numeric_limits<float>::quiet_NaN(), 45}));
}

static void TestHitTestRounded() {
    std::printf("[C] rounded-rect hit-test\n");
    ControlBounds b = Rect(0, 0, 100, 100);
    const float r = 20.0f;

    // Center inside.
    CHECK(HitTestRoundedRect(b, {50, 50}, r));
    // Straight-edge midpoints inside.
    CHECK(HitTestRoundedRect(b, {50, 1}, r));
    CHECK(HitTestRoundedRect(b, {1, 50}, r));
    // Corner: point in the square corner but outside the rounded outline.
    // At (2,2) the distance to the corner circle center (20,20) is ~25.4 > 20.
    CHECK(!HitTestRoundedRect(b, {2, 2}, r));
    // A point clearly on the rounded boundary region.
    CHECK(HitTestRoundedRect(b, {20, 20}, r));   // circle center, inside
    // Outside entirely.
    CHECK(!HitTestRoundedRect(b, {-1, 50}, r));
    CHECK(!HitTestRoundedRect(b, {50, 101}, r));

    // cornerRadius 0 must match HitTestRect exactly (half-open outer bounds).
    CHECK(HitTestRoundedRect(b, {50, 50}, 0.0f));
    CHECK(HitTestRoundedRect(b, {0, 0}, 0.0f));        // left/top inclusive
    CHECK(!HitTestRoundedRect(b, {100, 50}, 0.0f));    // right exclusive
    CHECK(!HitTestRoundedRect(b, {50, 100}, 0.0f));    // bottom exclusive
    CHECK(HitTestRoundedRect(b, {99.999f, 99.999f}, 0.0f)); // just inside

    // Equivalence: HitTestRoundedRect(r=0) == HitTestRect over a sample grid.
    {
        bool consistent = true;
        for (int ix = -5; ix <= 105; ++ix) {
            for (int iy = -5; iy <= 105; ++iy) {
                ControlPoint q{ (float)ix, (float)iy };
                if (HitTestRoundedRect(b, q, 0.0f) != HitTestRect(b, q))
                    consistent = false;
            }
        }
        CHECK(consistent);
    }

    // cornerRadius clamped to half-min-dimension; a huge radius still valid center.
    CHECK(HitTestRoundedRect(b, {50, 50}, 1000.0f));

    // Negative radius treated as 0.
    CHECK(HitTestRoundedRect(b, {50, 50}, -5.0f));
}

static void TestStateTransitions() {
    std::printf("[D] interaction state transitions\n");
    ControlBounds b = Rect(0, 0, 100, 100);
    ControlInteraction c;

    CHECK(c.State() == ControlInteractionState::Normal);

    // Normal -> Hover
    c.PointerMove({50, 50}, b);
    CHECK(c.State() == ControlInteractionState::Hover);

    // Hover -> Pressed
    c.PointerDown({50, 50}, b);
    CHECK(c.State() == ControlInteractionState::Pressed);

    // Pressed -> Hover on release inside
    bool act = c.PointerUp({50, 50}, b);
    CHECK(act);
    CHECK(c.State() == ControlInteractionState::Hover);

    // Move outside -> Normal
    c.PointerMove({200, 200}, b);
    CHECK(c.State() == ControlInteractionState::Normal);

    // Press inside, release outside -> not activated, state Normal
    c.PointerMove({50, 50}, b);
    c.PointerDown({50, 50}, b);
    bool act2 = c.PointerUp({200, 200}, b);
    CHECK(!act2);
    CHECK(c.State() == ControlInteractionState::Normal);

    // PointerLeave clears hover/press
    c.PointerMove({50, 50}, b);
    c.PointerDown({50, 50}, b);
    CHECK(c.State() == ControlInteractionState::Pressed);
    c.PointerLeave();
    CHECK(c.State() == ControlInteractionState::Normal);
    CHECK(!c.IsHovered());
    CHECK(!c.IsPressed());
}

static void TestDisabledAndFocus() {
    std::printf("[E] disabled / focus\n");
    ControlBounds b = Rect(0, 0, 100, 100);
    ControlInteraction c;

    c.SetEnabled(false);
    CHECK(c.State() == ControlInteractionState::Disabled);
    // Disabled ignores all input.
    c.PointerMove({50, 50}, b);
    CHECK(c.State() == ControlInteractionState::Disabled);
    CHECK(!c.IsHovered());
    c.PointerDown({50, 50}, b);
    CHECK(!c.IsPressed());
    CHECK(!c.PointerUp({50, 50}, b));
    CHECK(!c.KeyActivate());
    CHECK(c.State() == ControlInteractionState::Disabled);

    // Re-enable.
    c.SetEnabled(true);
    CHECK(c.State() == ControlInteractionState::Normal);

    // Focus is state only.
    c.SetFocused(true);
    CHECK(c.IsFocused());
    CHECK(c.State() == ControlInteractionState::Focused);
    // Hover overrides focus for display ordering.
    c.PointerMove({50, 50}, b);
    CHECK(c.State() == ControlInteractionState::Hover);
    c.SetFocused(false);
    CHECK(c.State() == ControlInteractionState::Hover);

    // KeyActivate returns enabled state, does not change state.
    ControlInteraction k;
    CHECK(k.KeyActivate());
    CHECK(k.State() == ControlInteractionState::Normal);
}

static void TestDeterminismAndContract() {
    std::printf("[F] determinism / pixel contract\n");
    ControlBounds b = Rect(10.5f, 20.25f, 64.0f, 32.0f); // fractional physical pixels

    // Repeated identical input is deterministic.
    ControlInteraction a, c;
    for (int i = 0; i < 5; ++i) {
        a.PointerMove({12.5f, 24.0f}, b);
        c.PointerMove({12.5f, 24.0f}, b);
    }
    CHECK(a.State() == c.State());
    CHECK(a.State() == ControlInteractionState::Hover);

    // Fractional coordinates obey the half-open contract in pixel space.
    CHECK(HitTestRect(b, {10.5f, 20.25f}));
    CHECK(!HitTestRect(b, {10.5f + 64.0f, 20.25f + 32.0f})); // right/bottom exclusive

    // Reset clears transient state but keeps enabled.
    a.SetEnabled(false);
    a.Reset();
    CHECK(!a.IsFocused() && !a.IsHovered() && !a.IsPressed());
    CHECK(!a.IsEnabled());   // enabled preserved (false)
    a.SetEnabled(true);
    CHECK(a.State() == ControlInteractionState::Normal);
}

int main() {
    std::printf("AuroraGlass P3 Slice A control tests\n");
    TestGeometryBounds();
    TestHitTestRect();
    TestHitTestRounded();
    TestStateTransitions();
    TestDisabledAndFocus();
    TestDeterminismAndContract();
    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
