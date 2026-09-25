// ============================================================
// AuroraGlass P8 - CPU-only micro-benchmarks.
//
// Purpose: honest, reproducible measurement of small CPU operations
// that an SDK consumer exercises every frame. This is NOT a frame-rate
// benchmark and NOT a rendering-throughput benchmark.
//
// What is measured (all CPU-only, no GPU, no HWND):
//   - GlassMaterial validated setter cost
//   - GlassMaterial snapshot (read all getters) cost
//   - Motion Tween1D::Step cost
//   - Motion Spring1D::Step cost
//   - GlassButton pointer press+release cycle cost
//   - GlassSlider drag step cost
//
// Method: QueryPerformanceCounter around a batch of N iterations;
// per-iteration ns = batch_ns / N. Repeated for T trials; the report
// uses the MEDIAN and P95 of the trial values (robust to outliers).
// Warm-up: one discarded batch before timing.
//
// This harness performs no rendering, so it is unaffected by the
// render-thread contract. It uses only the frozen public headers.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "core/glass_material.h"
#include "motion/motion.h"
#include "controls/control_button.h"
#include "controls/control_slider.h"
#include "controls/control_toggle.h"

using namespace AuroraGlass;

static double NowNs() {
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e9 / (double)freq.QuadPart;
}

struct Stat {
    double median = 0.0;
    double p95 = 0.0;
};

template <typename F>
static double OneBatch(int iters, F&& body) {
    double t0 = NowNs();
    for (int i = 0; i < iters; ++i) body(i);
    double t1 = NowNs();
    return (t1 - t0) / (double)iters;
}

template <typename F>
static Stat Measure(const char* name, int iters, int trials, F&& body) {
    OneBatch(iters, body);

    std::vector<double> samples;
    samples.reserve(trials);
    for (int t = 0; t < trials; ++t) samples.push_back(OneBatch(iters, body));

    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    Stat s;
    s.median = sorted[sorted.size() / 2];
    size_t idx = (size_t)(0.95 * (double)(sorted.size() - 1));
    s.p95 = sorted[idx];

    std::printf("%-38s median=%8.2f ns/op  p95=%8.2f ns/op  (iters=%d trials=%d)\n",
                name, s.median, s.p95, iters, trials);
    return s;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);

#if defined(_DEBUG) || !defined(NDEBUG)
    const char* buildType = "Debug";
#else
    const char* buildType = "Release";
#endif

    std::printf("AuroraGlass P8 CPU micro-benchmarks\n");
    std::printf("build=%s\n", buildType);
    std::printf("Method: QueryPerformanceCounter; batch ns / iters; median & p95 of trials.\n");
    std::printf("This is CPU-only. It is NOT a frame-rate or GPU benchmark.\n\n");

    const int iters = 200000;
    const int trials = 15;

    volatile float fSink = 0.0f;
    volatile bool  bSink = false;

    {
        GlassMaterial m;
        Measure("material.SetBlurRadius", iters, trials, [&](int i) {
            Status st = m.SetBlurRadius(0.0f + (float)(i & 63) * 0.25f);
            fSink = (float)st.code;
        });
    }

    {
        GlassMaterial m;
        Measure("material.snapshot(all getters)", iters, trials, [&](int) {
            float a = m.GetBlurRadius() + m.GetRefractionStrength() + m.GetDispersionStrength()
                    + m.GetThickness() + m.GetEdgeFresnel() + m.GetSpecularStrength()
                    + m.GetTintAmount() + m.GetSaturation() + m.GetBrightness()
                    + m.GetNoiseAmount() + m.GetCornerRadius() + m.GetOpacity();
            float hx, hy; m.GetHighlightPosition(hx, hy);
            fSink = a + hx + hy;
        });
    }

    {
        Motion::Tween1D tw(0.0f);
        tw.Start(0.0f, 1.0f, 0.5f);
        Measure("motion.Tween1D.Step", iters, trials, [&](int) {
            tw.Step(1.0f / 60.0f);
            fSink = tw.Value();
        });
    }

    {
        Motion::Spring1D sp(0.0f);
        sp.SetResponseSeconds(0.2f);
        sp.SetTarget(1.0f);
        Measure("motion.Spring1D.Step", iters, trials, [&](int) {
            sp.Step(1.0f / 60.0f);
            fSink = sp.Value();
        });
    }

    {
        GlassButton b;
        b.bounds = { 0.0f, 0.0f, 200.0f, 48.0f };
        b.onClick = [] {};
        Measure("controls.Button.PressRelease", iters, trials, [&](int) {
            b.PointerDown({ 100.0f, 24.0f });
            bSink = b.PointerUp({ 100.0f, 24.0f });
        });
    }

    {
        GlassSlider s;
        s.bounds = { 0.0f, 0.0f, 300.0f, 32.0f };
        s.PointerDown({ 150.0f, 16.0f });
        Measure("controls.Slider.DragMove", iters, trials, [&](int i) {
            float x = 150.0f + (float)(i % 100);
            s.PointerMove({ x, 16.0f });
            fSink = s.Value();
        });
    }

    std::printf("\n(sinks: f=%g b=%d)\n", (double)fSink, (int)bSink);
    return 0;
}
