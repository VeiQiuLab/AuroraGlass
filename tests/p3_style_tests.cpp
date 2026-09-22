// ============================================================
// AuroraGlass P3 Slice B — state -> GlassMaterial mapping tests.
// Pure CPU. No D3D device, no HWND, no GlassSurface.
// ============================================================

#include "controls/control_visual_style.h"

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

static bool SameMaterial(const GlassMaterial& a, const GlassMaterial& b) {
    float ax, ay, bx, by;
    a.GetHighlightPosition(ax, ay);
    b.GetHighlightPosition(bx, by);
    return a.GetBlurRadius() == b.GetBlurRadius() &&
           a.GetRefractionStrength() == b.GetRefractionStrength() &&
           a.GetDispersionStrength() == b.GetDispersionStrength() &&
           a.GetThickness() == b.GetThickness() &&
           a.GetEdgeFresnel() == b.GetEdgeFresnel() &&
           a.GetSpecularStrength() == b.GetSpecularStrength() &&
           a.GetTintAmount() == b.GetTintAmount() &&
           a.GetSaturation() == b.GetSaturation() &&
           a.GetBrightness() == b.GetBrightness() &&
           a.GetNoiseAmount() == b.GetNoiseAmount() &&
           a.GetCornerRadius() == b.GetCornerRadius() &&
           a.GetOpacity() == b.GetOpacity() &&
           ax == bx && ay == by;
}

static void TestStateSelection() {
    std::printf("[A] state -> material selection\n");
    ControlVisualStyle style;

    GlassMaterial mn;  mn.SetBlurRadius(1.0f);
    GlassMaterial mh;  mh.SetBlurRadius(2.0f);
    GlassMaterial mp;  mp.SetBlurRadius(3.0f);
    GlassMaterial md;  md.SetBlurRadius(4.0f);
    GlassMaterial mf;  mf.SetBlurRadius(5.0f);
    style.normal = mn; style.hover = mh; style.pressed = mp;
    style.disabled = md; style.focused = mf;

    CHECK(SameMaterial(style.MaterialFor(ControlInteractionState::Normal), mn));
    CHECK(SameMaterial(style.MaterialFor(ControlInteractionState::Hover), mh));
    CHECK(SameMaterial(style.MaterialFor(ControlInteractionState::Pressed), mp));
    CHECK(SameMaterial(style.MaterialFor(ControlInteractionState::Disabled), md));
    CHECK(SameMaterial(style.MaterialFor(ControlInteractionState::Focused), mf));

    // Returns a reference into the struct (same address).
    CHECK(&style.MaterialFor(ControlInteractionState::Normal) == &style.normal);
    CHECK(&style.MaterialFor(ControlInteractionState::Hover) == &style.hover);
}

static void TestDeterminism() {
    std::printf("[B] determinism\n");
    ControlVisualStyle s;
    GlassMaterial m; m.SetBlurRadius(9.0f); m.SetOpacity(0.5f);
    s.hover = m;

    // Same state -> same values, repeatedly.
    for (int i = 0; i < 5; ++i) {
        const GlassMaterial& a = s.MaterialFor(ControlInteractionState::Hover);
        const GlassMaterial& b = s.MaterialFor(ControlInteractionState::Hover);
        CHECK(SameMaterial(a, b));
        CHECK(a.GetBlurRadius() == 9.0f && a.GetOpacity() == 0.5f);
    }
}

static void TestSetAll() {
    std::printf("[C] SetAll / defaults\n");
    ControlVisualStyle s;

    // Defaults are the P1-verified default material in every slot.
    GlassMaterial def;
    CHECK(SameMaterial(s.MaterialFor(ControlInteractionState::Normal), def));
    CHECK(SameMaterial(s.MaterialFor(ControlInteractionState::Disabled), def));

    GlassMaterial m; m.SetBlurRadius(7.5f); m.SetRefractionStrength(0.25f);
    s.SetAll(m);
    const ControlInteractionState states[] = {
        ControlInteractionState::Normal, ControlInteractionState::Hover,
        ControlInteractionState::Pressed, ControlInteractionState::Disabled,
        ControlInteractionState::Focused,
    };
    for (ControlInteractionState st : states) {
        CHECK(SameMaterial(s.MaterialFor(st), m));
    }
}

static void TestValidationStillApplies() {
    std::printf("[D] GlassMaterial validation still applies\n");
    // GlassMaterial setters still validate; style stores validated values.
    GlassMaterial m;
    CHECK(m.SetBlurRadius(10.0f).ok());
    CHECK(m.SetBlurRadius(-1.0f).ok());     // clamped to 0
    CHECK(m.GetBlurRadius() == 0.0f);
    // NaN rejected, prior value kept.
    CHECK(m.SetBlurRadius(std::numeric_limits<float>::quiet_NaN()).code ==
          ErrorCode::InvalidArgument);
    CHECK(m.GetBlurRadius() == 0.0f);

    ControlVisualStyle s;
    s.normal = m;
    CHECK(s.MaterialFor(ControlInteractionState::Normal).GetBlurRadius() == 0.0f);
}

int main() {
    std::printf("AuroraGlass P3 Slice B style tests\n");
    TestStateSelection();
    TestDeterminism();
    TestSetAll();
    TestValidationStillApplies();
    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
