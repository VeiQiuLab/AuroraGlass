// ============================================================
// AuroraGlass P3 Slice D — interactive control semantics tests.
// Pure CPU. No D3D device, no HWND, no GlassSurface.
// ============================================================

#include "controls/control_button.h"
#include "controls/control_toggle.h"
#include "controls/control_slider.h"

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

static ControlBounds R(float x, float y, float w, float h) {
    ControlBounds b; b.x = x; b.y = y; b.width = w; b.height = h; return b;
}

static void TestButton() {
    std::printf("[A] GlassButton\n");
    GlassButton b;
    b.bounds = R(0, 0, 100, 40);
    int clicks = 0;
    b.onClick = [&]() { ++clicks; };

    // Click: down inside, up inside.
    b.PointerDown({50, 20});
    CHECK(b.State() == ControlInteractionState::Pressed);
    CHECK(b.PointerUp({50, 20}));
    CHECK(clicks == 1);

    // Release outside -> no click.
    b.PointerDown({50, 20});
    CHECK(!b.PointerUp({500, 500}));
    CHECK(clicks == 1);

    // Disabled -> no click, ignores input.
    b.SetEnabled(false);
    b.PointerDown({50, 20});
    CHECK(!b.PointerUp({50, 20}));
    CHECK(clicks == 1);
    CHECK(b.State() == ControlInteractionState::Disabled);
    b.SetEnabled(true);

    // KeyActivate -> click when enabled.
    CHECK(b.KeyActivate());
    CHECK(clicks == 2);

    // Repeated pointer sequences deterministic.
    for (int i = 0; i < 3; ++i) { b.PointerDown({10, 10}); b.PointerUp({10, 10}); }
    CHECK(clicks == 5);
}

static void TestToggle() {
    std::printf("[B] GlassToggle\n");
    GlassToggle t;
    t.bounds = R(0, 0, 60, 30);
    int changes = 0; bool last = false;
    t.onChanged = [&](bool v) { ++changes; last = v; };

    CHECK(!t.IsChecked());
    t.PointerDown({30, 15}); CHECK(t.PointerUp({30, 15}));
    CHECK(t.IsChecked()); CHECK(changes == 1 && last == true);
    t.PointerDown({30, 15}); CHECK(t.PointerUp({30, 15}));
    CHECK(!t.IsChecked()); CHECK(changes == 2 && last == false);

    // SetChecked only fires on actual change.
    t.SetChecked(false); CHECK(changes == 2);
    t.SetChecked(true);  CHECK(changes == 3 && t.IsChecked());

    // Disabled: no change.
    t.SetEnabled(false);
    t.PointerDown({30, 15}); CHECK(!t.PointerUp({30, 15}));
    CHECK(t.IsChecked()); CHECK(changes == 3);
    t.SetChecked(false);  // disabled -> ignored
    CHECK(t.IsChecked());
    t.SetEnabled(true);

    // KeyActivate toggles.
    bool before = t.IsChecked();
    CHECK(t.KeyActivate());
    CHECK(t.IsChecked() == !before);

    // Material selection: optional checked style.
    t.style.normal.SetBlurRadius(3.0f);
    t.checkedStyle.normal.SetBlurRadius(9.0f);
    t.useCheckedStyle = true;
    t.SetChecked(false);
    CHECK(t.Material().GetBlurRadius() == 3.0f);
    t.SetChecked(true);
    CHECK(t.Material().GetBlurRadius() == 9.0f);
}

static void TestSlider() {
    std::printf("[C] GlassSlider\n");
    GlassSlider s;
    s.bounds = R(0, 0, 200, 20);
    CHECK(s.SetRange(0.0f, 100.0f).ok());
    float last = -1.0f; int changes = 0;
    s.onValueChanged = [&](float v) { last = v; ++changes; };

    // Pointer at left -> min (start from a non-min value so the change fires).
    s.SetValue(50.0f);
    s.PointerDown({0, 10}); s.PointerUp({0, 10});
    CHECK(s.Value() == 0.0f); CHECK(last == 0.0f);

    // Pointer center -> ~50.
    s.PointerDown({100, 10}); s.PointerUp({100, 10});
    CHECK(s.Value() == 50.0f); CHECK(last == 50.0f);

    // Pointer right -> max.
    s.PointerDown({200, 10}); s.PointerUp({200, 10});
    CHECK(s.Value() == 100.0f); CHECK(last == 100.0f);

    // Press outside the track is ignored (no aggressive jump).
    s.SetValue(50.0f);
    s.PointerDown({9999, 10}); s.PointerUp({9999, 10});
    CHECK(s.Value() == 50.0f);
    s.PointerDown({-9999, 10}); s.PointerUp({-9999, 10});
    CHECK(s.Value() == 50.0f);

    // Restrained: no value change on press-down; deliberate click-to-set on up.
    s.SetValue(0.0f);
    s.PointerDown({150, 10});
    CHECK(s.Value() == 0.0f);          // no jump on down
    s.PointerUp({150, 10});
    CHECK(s.Value() == 75.0f);         // deliberate click-to-set on up

    // Off-thumb press followed by drag becomes a drag (after threshold).
    s.SetValue(0.0f);
    s.PointerDown({20, 10});
    s.PointerMove({100, 10});
    CHECK(s.Value() == 50.0f);
    s.PointerUp({100, 10});

    // Callback only on actual change.
    int c0 = changes;
    s.SetValue(s.Value());   // same value
    CHECK(changes == c0);

    // Disabled -> no value change.
    s.SetValue(50.0f);
    s.SetEnabled(false);
    s.PointerDown({200, 10}); s.PointerUp({200, 10});
    CHECK(s.Value() == 50.0f);
    s.SetEnabled(true);

    // Invalid range rejected.
    CHECK(s.SetRange(10.0f, 10.0f).code == ErrorCode::InvalidArgument);  // inverted/equal
    CHECK(s.SetRange(5.0f, 1.0f).code == ErrorCode::InvalidArgument);
    CHECK(s.SetRange(std::numeric_limits<float>::quiet_NaN(), 1.0f).code ==
          ErrorCode::InvalidArgument);
    // Range preserved after rejected SetRange.
    CHECK(s.MinValue() == 0.0f && s.MaxValue() == 100.0f);

    // Drag: down then move updates value.
    s.SetValue(0.0f);
    s.PointerDown({0, 10});
    s.PointerMove({100, 10});
    CHECK(s.Value() == 50.0f);
    s.PointerUp({100, 10});
}

int main() {
    std::printf("AuroraGlass P3 Slice D interactive control tests\n");
    TestButton();
    TestToggle();
    TestSlider();
    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
