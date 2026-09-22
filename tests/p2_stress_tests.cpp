// ============================================================
// AuroraGlass P2 — Repeated lifecycle stress + resource leak verification.
//
// Slice 1 scope: Core resource lifecycle under heavy repetition.
//   - repeated Create -> Reset
//   - repeated Create -> Resize sequence -> Reset
//   - repeated Reset x2/x3 (idempotent)
//   - Create after Reset
//   - move construction / move assignment loops
//   - per-iteration object-state checks
//   - render exercised within the lifecycle
//
// Honesty note on live-object verification:
//   ID3D11Debug::ReportLiveDeviceObjects() writes its report to the debugger
//   output (Visual Studio Output / DebugView). It does NOT return a machine-
//   readable live-object count. Therefore this test does NOT assert a zero
//   GPU-object count; it EXECUTES ReportLiveDeviceObjects() at shutdown and
//   marks "no residual live objects" as a MANUAL / debugger verification item.
//   What is automated here: lifecycle Status correctness, state invariants,
//   idempotency, and (best-effort) process working-set trend.
//
// Does not modify any P1 frozen public API.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <psapi.h>

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "core/result.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"

#pragma comment(lib, "psapi.lib")

using namespace AuroraGlass;
using Microsoft::WRL::ComPtr;

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

static bool g_debugLayerActive = false;
static bool g_forceNoDebug = false;   // --no-debug: classify memory growth

// ---------------------------------------------------------------------------
// Device / targets
// ---------------------------------------------------------------------------

static ComPtr<ID3D11Device> CreateStressDevice() {
    ComPtr<ID3D11Device> dev;
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = E_FAIL;

    if (!g_forceNoDebug) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                               D3D11_CREATE_DEVICE_DEBUG, nullptr, 0,
                               D3D11_SDK_VERSION, &dev, &fl, nullptr);
        if (SUCCEEDED(hr)) g_debugLayerActive = true;
    }
    if (FAILED(hr)) {
        g_debugLayerActive = false;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                               nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, nullptr);
        if (FAILED(hr)) {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                   nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, nullptr);
        }
    }
    return dev;
}

struct Targets {
    ComPtr<ID3D11Texture2D>          tex;
    ComPtr<ID3D11RenderTargetView>   rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
};

static bool MakeTargets(ID3D11Device* dev, UINT w, UINT h, Targets& out) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &out.tex))) return false;
    if (FAILED(dev->CreateRenderTargetView(out.tex.Get(), nullptr, &out.rtv))) return false;
    if (FAILED(dev->CreateShaderResourceView(out.tex.Get(), nullptr, &out.srv))) return false;
    return true;
}

static size_t WorkingSetBytes() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize;
    return 0;
}

// ---------------------------------------------------------------------------
// Stress cases
// ---------------------------------------------------------------------------

// Case A: N x (Create -> validate -> Render a few frames -> Reset -> validate)
static void StressCreateReset(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                              ID3D11RenderTargetView* rtv, ID3D11ShaderResourceView* srv,
                              int iterations) {
    std::printf("[A] Create->Reset x%d\n", iterations);
    SurfaceDesc d; d.width = 64; d.height = 64;
    FrameInfo fi;
    GlassMaterial m;

    for (int i = 0; i < iterations; ++i) {
        GlassSurface surf;
        CHECK(!surf.IsInitialized());

        Status cs = GlassSurface::Create(dev, d, surf);
        if (!cs.ok()) { CHECK(false); break; }
        CHECK(surf.IsInitialized());
        CHECK(surf.Width() == 64 && surf.Height() == 64);

        surf.SetMaterial(m);
        Status rs = surf.Render(ctx, rtv, srv, fi);
        CHECK(rs.ok());

        surf.Reset();
        CHECK(!surf.IsInitialized());
        CHECK(surf.Width() == 0 && surf.Height() == 0);
    }
}

// Case B: N x (Create -> Resize sequence -> Reset)
static void StressCreateResizeReset(ID3D11Device* dev, int iterations) {
    std::printf("[B] Create->Resize(seq)->Reset x%d\n", iterations);
    const uint32_t seq[][2] = {
        { 64, 64 }, { 320, 240 }, { 1280, 720 }, { 16, 16 }, { 800, 600 },
    };
    SurfaceDesc d; d.width = 64; d.height = 64;

    for (int i = 0; i < iterations; ++i) {
        GlassSurface surf;
        Status cs = GlassSurface::Create(dev, d, surf);
        if (!cs.ok()) { CHECK(false); break; }
        for (auto& s : seq) {
            Status rs = surf.Resize(s[0], s[1]);
            CHECK(rs.ok());
            CHECK(surf.Width() == s[0] && surf.Height() == s[1]);
        }
        surf.Reset();
        CHECK(!surf.IsInitialized());
    }
}

// Case C: repeated Reset x2/x3 idempotent + Create after Reset
static void StressResetIdempotent(ID3D11Device* dev, int iterations) {
    std::printf("[C] Reset x2/x3 idempotent + Create-after-Reset x%d\n", iterations);
    SurfaceDesc d; d.width = 64; d.height = 64;

    for (int i = 0; i < iterations; ++i) {
        GlassSurface surf;
        Status cs = GlassSurface::Create(dev, d, surf);
        if (!cs.ok()) { CHECK(false); break; }

        surf.Reset();
        CHECK(!surf.IsInitialized());
        surf.Reset();               // idempotent
        CHECK(!surf.IsInitialized());
        surf.Reset();               // idempotent
        CHECK(!surf.IsInitialized());

        Status rcs = GlassSurface::Create(dev, d, surf);
        CHECK(rcs.ok());
        CHECK(surf.IsInitialized());
    }
}

// Case D: move construction / move assignment loops
static void StressMoveLifecycle(ID3D11Device* dev, int iterations) {
    std::printf("[D] move construct / move assign x%d\n", iterations);
    SurfaceDesc d; d.width = 64; d.height = 64;

    for (int i = 0; i < iterations; ++i) {
        GlassSurface a;
        Status cs = GlassSurface::Create(dev, d, a);
        if (!cs.ok()) { CHECK(false); break; }
        CHECK(a.IsInitialized());

        GlassSurface b(std::move(a));       // move construct
        CHECK(b.IsInitialized());
        CHECK(!a.IsInitialized());          // moved-from empty
        CHECK(a.Width() == 0 && a.Height() == 0);
        a.Reset();                          // moved-from safe
        CHECK(!a.IsInitialized());

        GlassSurface c;
        c = std::move(b);                   // move assign
        CHECK(c.IsInitialized());
        CHECK(!b.IsInitialized());
        b.Reset();                          // moved-from safe

        c.Reset();
        CHECK(!c.IsInitialized());
    }
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--no-debug") g_forceNoDebug = true;

    std::printf("AuroraGlass P2 stress tests%s\n", g_forceNoDebug ? " (no-debug)" : "");

    ComPtr<ID3D11Device> dev = CreateStressDevice();
    if (!dev) {
        std::printf("FATAL: no D3D11 device available\n");
        return 1;
    }
    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);

    std::printf("Debug Layer: %s\n", g_debugLayerActive ? "ACTIVE" : "UNAVAILABLE (SKIPPED)");

    Targets tg;
    if (!MakeTargets(dev.Get(), 64, 64, tg)) {
        std::printf("FATAL: could not create render targets\n");
        return 1;
    }

    // Two identical rounds: if round-2 delta is much smaller than round-1 delta,
    // the working-set growth is cache/fragmentation (converging), not a per-
    // iteration leak. If both rounds grow similarly, that indicates sustained
    // growth worth a manual DebugView investigation. Working set is a coarse
    // signal only; it does not prove/refute GPU-object leaks on its own.
    const size_t wsStart = WorkingSetBytes();
    size_t lastRoundEnd = wsStart;
    std::printf("Working set base: %zu KB\n", wsStart / 1024);

    for (int round = 1; round <= 2; ++round) {
        std::printf("--- Round %d ---\n", round);
        StressCreateReset(dev.Get(), ctx.Get(), tg.rtv.Get(), tg.srv.Get(), 200);
        StressCreateResizeReset(dev.Get(), 100);
        StressResetIdempotent(dev.Get(), 200);
        StressMoveLifecycle(dev.Get(), 200);
        size_t cur = WorkingSetBytes();
        std::printf("Round %d end WS: %zu KB (delta from base %+lld KB)\n",
                    round, cur / 1024,
                    (long long)((long long)cur - (long long)wsStart) / 1024);
        if (round == 1) lastRoundEnd = cur;
    }
    {
        size_t finalWs = WorkingSetBytes();
        long long round2Marginal = (long long)finalWs - (long long)lastRoundEnd;
        long long round1Total    = (long long)lastRoundEnd - (long long)wsStart;
        // Converging (round-2 marginal << round-1 total) => cache/fragmentation,
        // not sustained per-iteration growth. Coarse signal only; not proof of
        // zero GPU-object leaks (that is the manual ReportLiveDeviceObjects review).
        std::printf("Convergence: round1 total=%+lld KB, round2 marginal=%+lld KB -> %s\n",
                    round1Total / 1024, round2Marginal / 1024,
                    (round2Marginal * 4 < round1Total) ? "CONVERGING (likely cache/fragmentation)"
                                                       : "SUSTAINED (investigate manually)");
    }

    // Release everything the test holds, then report live device objects.
    tg.srv.Reset(); tg.rtv.Reset(); tg.tex.Reset();
    ctx.Reset();

    // Debug-layer live-object report (goes to Debug Output / DebugView, NOT to
    // this process's stdout). "No residual live objects" is a MANUAL/debugger
    // verification item; it is not machine-asserted here.
    {
        ComPtr<ID3D11Debug> dbg;
        if (g_debugLayerActive && SUCCEEDED(dev.As(&dbg)) && dbg) {
            std::printf("ReportLiveDeviceObjects(): executing (report goes to Debug Output / DebugView)\n");
            dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        } else {
            std::printf("ReportLiveDeviceObjects(): SKIPPED (debug layer unavailable)\n");
        }
    }
    dev.Reset();

    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
