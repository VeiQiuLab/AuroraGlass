// ============================================================
// Step 27D - native tests for the airspace-safe WPF composition bridge.
//
// Covers: create, offscreen shared surface, resize, render, cleanup, and the
// no-HWND contract (the bridge exposes a D3D9 surface, never a child HWND).
// Does not (and cannot) validate final visual quality.
// ============================================================

#include "wpf/wpf_composition_bridge.h"

#include <cstdint>
#include <cstdio>

static int g_checks = 0;
static int g_failures = 0;

static void Check(bool ok, const char* name) {
    ++g_checks;
    if (ok) {
        std::printf("[PASS] %s\n", name);
    } else {
        ++g_failures;
        std::printf("[FAIL] %s\n", name);
    }
}

int main() {
    // Create at a modest size.
    AuroraGlassWpfComposition* c =
        AuroraGlassWpfCompositionCreate(320, 240);
    Check(c != nullptr, "composition create");
    if (!c) {
        std::printf("P6_WPF_COMPOSITION_TESTS: FAIL (create)\n");
        return 1;
    }

    // The bridge must expose a D3D9 surface (D3DImage source), not an HWND.
    intptr_t surface = AuroraGlassWpfCompositionGetSurface(c);
    Check(surface != 0, "shared D3D9 surface exposed");

    // Initial stats: ready, correct size.
    AuroraGlassWpfCompositionStats stats{};
    int st = AuroraGlassWpfCompositionGetStats(c, &stats);
    Check(st == 0, "get stats ok");
    Check(stats.ready == 1, "ready after create");
    Check(stats.width == 320 && stats.height == 240, "initial size correct");

    // Render a few frames (offscreen; no HWND, no Present).
    for (int i = 0; i < 5; ++i) {
        st = AuroraGlassWpfCompositionRender(c, (float)i * (1.0f / 60.0f));
        Check(st == 0, "render frame status ok");
    }

    st = AuroraGlassWpfCompositionGetStats(c, &stats);
    Check(stats.frameCount == 5, "frame count advanced");
    Check(stats.lastCoreStatus == 0, "core status OK after render");

    // Resize the shared surface.
    st = AuroraGlassWpfCompositionResize(c, 512, 384);
    Check(st == 0, "resize status ok");
    st = AuroraGlassWpfCompositionGetStats(c, &stats);
    Check(stats.width == 512 && stats.height == 384, "size after resize");

    st = AuroraGlassWpfCompositionRender(c, 1.0f);
    Check(st == 0, "render after resize ok");
    Check(AuroraGlassWpfCompositionGetSurface(c) != 0, "surface valid after resize");

    // Set rects (glass shapes).
    AuroraGlassWpfRenderRect rects[2] = {
        { 20.0f, 20.0f, 200.0f, 120.0f },
        { 60.0f, 160.0f, 260.0f, 140.0f },
    };
    st = AuroraGlassWpfCompositionSetRects(c, rects, 2);
    Check(st == 0, "set rects ok");
    st = AuroraGlassWpfCompositionRender(c, 2.0f);
    Check(st == 0, "render with rects ok");

    // Material snapshot path.
    AuroraGlassWpfMaterialSnapshot snap{};
    snap.blurRadius = 14.0f;
    snap.refractionStrength = 0.25f;
    snap.dispersionStrength = 0.08f;
    snap.thickness = 0.55f;
    snap.edgeFresnel = 0.75f;
    snap.specularStrength = 1.0f;
    snap.tintAmount = 0.1f;
    snap.saturation = 1.0f;
    snap.brightness = 1.0f;
    snap.noiseAmount = 0.01f;
    snap.cornerRadius = 30.0f;
    snap.opacity = 0.85f;
    snap.highlightX = 0.3f;
    snap.highlightY = 0.2f;
    st = AuroraGlassWpfCompositionSetMaterial(c, &snap);
    Check(st == 0, "set material ok");

    // Cleanup: destroy must not crash and must fully release.
    AuroraGlassWpfCompositionDestroy(c);
    Check(true, "destroy without crash");

    // Null-safety.
    AuroraGlassWpfCompositionDestroy(nullptr);
    Check(AuroraGlassWpfCompositionGetSurface(nullptr) == 0, "null get surface safe");

    std::printf("P6_WPF_COMPOSITION_TESTS: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
