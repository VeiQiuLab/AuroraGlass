// ============================================================
// AuroraGlass P2 — Core rendering performance baseline (Slice 4).
//
// Measures AuroraGlass Core rendering via the P1 public GlassSurface API on a
// FIXED-SIZE OFFSCREEN D3D11 render target, so results do not depend on the
// current desktop/window size or on a swap chain.
//
// CPU time : QPC around the GlassSurface::Render() submission call only.
// GPU time : D3D11 timestamp queries (TIMESTAMP_DISJOINT + start/end TIMESTAMP),
//            resolved per frame; GPU ms = (end-start) / Frequency * 1000.
//
// Deliberately EXCLUDED from the measured region:
//   - Present / vsync          (offscreen; no swap chain)
//   - background generation    (a static background SRV is used)
//
// Usage:
//   P2_Bench [--w N --h N] [--warmup N] [--frames N] [--debug|--no-debug]
//   With no --w/--h, runs 1920x1080 and 2560x1440.
//
// Does not modify any frozen P1 public API, nor frozen samples.
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

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/result.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"

using namespace AuroraGlass;
using Microsoft::WRL::ComPtr;

struct BenchResult {
    uint32_t w = 0, h = 0;
    int warmup = 0, measured = 0;
    double cpuAvg = 0, cpuMin = 0, cpuMax = 0;
    double gpuAvg = 0, gpuMin = 0, gpuMax = 0;
    double gpuFps = 0;
    int disjointFrames = 0;
    bool ok = false;
};

static ComPtr<ID3D11Device> CreateDevice(bool enableDebug, bool& debugActive) {
    ComPtr<ID3D11Device> dev;
    D3D_FEATURE_LEVEL fl{};
    debugActive = false;
    HRESULT hr = E_FAIL;
    if (enableDebug) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                               D3D11_CREATE_DEVICE_DEBUG, nullptr, 0,
                               D3D11_SDK_VERSION, &dev, &fl, nullptr);
        if (SUCCEEDED(hr)) debugActive = true;
    }
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                               nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, nullptr);
    }
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                               nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, nullptr);
    }
    return dev;
}

// A fixed, static background texture (no per-frame generation).
static bool CreateStaticBackground(ID3D11Device* dev, UINT w, UINT h,
                                   ComPtr<ID3D11ShaderResourceView>& outSrv) {
    std::vector<uint32_t> pixels((size_t)w * h);
    for (UINT y = 0; y < h; ++y)
        for (UINT x = 0; x < w; ++x) {
            uint8_t r = (uint8_t)(x * 255 / (w ? w : 1));
            uint8_t g = (uint8_t)(y * 255 / (h ? h : 1));
            uint8_t b = (uint8_t)((x ^ y) & 0xFF);
            pixels[(size_t)y * w + x] = (0xFFu << 24) | (b << 16) | (g << 8) | r;
        }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = pixels.data();
    sd.SysMemPitch = w * 4;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    return SUCCEEDED(dev->CreateShaderResourceView(tex.Get(), nullptr, &outSrv));
}

static BenchResult RunBenchmark(ComPtr<ID3D11Device> dev, uint32_t W, uint32_t H,
                                int warmup, int frames) {
    BenchResult r; r.w = W; r.h = H; r.warmup = warmup; r.measured = frames;

    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);

    // Offscreen target (where glass composite is written).
    D3D11_TEXTURE2D_DESC td{};
    td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> targetTex;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &targetTex))) return r;
    ComPtr<ID3D11RenderTargetView> targetRtv;
    if (FAILED(dev->CreateRenderTargetView(targetTex.Get(), nullptr, &targetRtv))) return r;

    ComPtr<ID3D11ShaderResourceView> bgSrv;
    if (!CreateStaticBackground(dev.Get(), W, H, bgSrv)) return r;

    GlassSurface surf;
    SurfaceDesc desc; desc.width = W; desc.height = H;
    Status cs = GlassSurface::Create(dev.Get(), desc, surf);
    if (!cs.ok()) { std::printf("  GlassSurface::Create failed: %s\n", ErrorCodeToString(cs.code)); return r; }

    GlassMaterial mat;   // P1-verified defaults
    surf.SetMaterial(mat);

    // Timestamp queries.
    D3D11_QUERY_DESC qdDisjoint{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
    D3D11_QUERY_DESC qdTs{ D3D11_QUERY_TIMESTAMP, 0 };
    ComPtr<ID3D11Query> qDisjoint, qStart, qEnd;
    if (FAILED(dev->CreateQuery(&qdDisjoint, qDisjoint.GetAddressOf())) ||
        FAILED(dev->CreateQuery(&qdTs, qStart.GetAddressOf())) ||
        FAILED(dev->CreateQuery(&qdTs, qEnd.GetAddressOf()))) return r;

    LARGE_INTEGER freq, a, b;
    QueryPerformanceFrequency(&freq);

    std::vector<double> cpuMs, gpuMs;
    cpuMs.reserve(frames); gpuMs.reserve(frames);

    FrameInfo fi;
    const int total = warmup + frames;
    for (int i = 0; i < total; ++i) {
        fi.timeSeconds = (float)i * (1.0f / 60.0f);
        fi.stages = DiagnosticStages::AllEnabled();

        const bool measure = (i >= warmup);

        // ---- CPU timing region: Render submission only ----
        if (measure) ctx->Begin(qDisjoint.Get());
        if (measure) ctx->End(qStart.Get());

        QueryPerformanceCounter(&a);
        Status rr = surf.Render(ctx.Get(), targetRtv.Get(), bgSrv.Get(), fi);
        QueryPerformanceCounter(&b);

        if (measure) {
            ctx->End(qEnd.Get());
            ctx->End(qDisjoint.Get());
        }
        if (!rr.ok()) { std::printf("  Render failed: %s\n", ErrorCodeToString(rr.code)); return r; }

        if (measure) {
            cpuMs.push_back((double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)freq.QuadPart);

            // Resolve queries with polling (data not ready -> wait).
            ctx->Flush();
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
            HRESULT hr;
            do { hr = ctx->GetData(qDisjoint.Get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH); }
            while (hr == S_FALSE);
            if (hr == S_OK) {
                if (dj.Disjoint) {
                    r.disjointFrames++;
                } else {
                    UINT64 t0 = 0, t1 = 0;
                    do { hr = ctx->GetData(qStart.Get(), &t0, sizeof(t0), D3D11_ASYNC_GETDATA_DONOTFLUSH); }
                    while (hr == S_FALSE);
                    do { hr = ctx->GetData(qEnd.Get(), &t1, sizeof(t1), D3D11_ASYNC_GETDATA_DONOTFLUSH); }
                    while (hr == S_FALSE);
                    if (dj.Frequency > 0 && t1 >= t0)
                        gpuMs.push_back((double)(t1 - t0) * 1000.0 / (double)dj.Frequency);
                }
            }
        }
    }

    auto stats = [](std::vector<double>& v, double& avg, double& mn, double& mx) {
        if (v.empty()) { avg = mn = mx = 0; return; }
        double s = 0; mn = v[0]; mx = v[0];
        for (double x : v) { s += x; mn = std::min(mn, x); mx = std::max(mx, x); }
        avg = s / (double)v.size();
    };
    stats(cpuMs, r.cpuAvg, r.cpuMin, r.cpuMax);
    stats(gpuMs, r.gpuAvg, r.gpuMin, r.gpuMax);
    r.gpuFps = r.gpuAvg > 0.0 ? 1000.0 / r.gpuAvg : 0.0;
    r.ok = true;

    surf.Reset();
    return r;
}

static void PrintResult(const BenchResult& r, const char* buildType, bool debugActive) {
    std::printf("--- %ux%u ---\n", r.w, r.h);
    if (!r.ok) { std::printf("  RESULT: FAILED (setup/render error)\n"); return; }
    std::printf("  resolution       : %ux%u\n", r.w, r.h);
    std::printf("  warmup_frames    : %d\n", r.warmup);
    std::printf("  measured_frames  : %d\n", r.measured);
    std::printf("  cpu_avg_ms       : %.4f\n", r.cpuAvg);
    std::printf("  cpu_min_ms       : %.4f\n", r.cpuMin);
    std::printf("  cpu_max_ms       : %.4f\n", r.cpuMax);
    std::printf("  gpu_avg_ms       : %.4f\n", r.gpuAvg);
    std::printf("  gpu_min_ms       : %.4f\n", r.gpuMin);
    std::printf("  gpu_max_ms       : %.4f\n", r.gpuMax);
    std::printf("  gpu_fps          : %.2f\n", r.gpuFps);
    std::printf("  disjoint_frames  : %d (invalid, excluded)\n", r.disjointFrames);
    std::printf("  build            : %s\n", buildType);
    std::printf("  debug_layer      : %s\n", debugActive ? "on" : "off");
    std::printf("  includes_present : no (offscreen)\n");
    std::printf("  background_gen   : no (static SRV)\n");
    const double threshold = 16.67;
    std::printf("  60fps(16.67ms)   : %s (gpu_avg=%.4f ms)\n",
                r.gpuAvg > 0 && r.gpuAvg <= threshold ? "PASS" : "FAIL", r.gpuAvg);
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    uint32_t w = 0, h = 0;
    int warmup = 300, frames = 1000;
    bool enableDebug = true;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "--w" && i + 1 < argc) w = (uint32_t)std::atoi(argv[++i]);
        else if (s == "--h" && i + 1 < argc) h = (uint32_t)std::atoi(argv[++i]);
        else if (s == "--warmup" && i + 1 < argc) warmup = std::atoi(argv[++i]);
        else if (s == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (s == "--debug") enableDebug = true;
        else if (s == "--no-debug") enableDebug = false;
    }

#if defined(_DEBUG) || !defined(NDEBUG)
    const char* buildType = "Debug";
#else
    const char* buildType = "Release";
#endif

    std::printf("AuroraGlass P2 Benchmark (Core rendering, offscreen)\n");
    bool debugActive = false;
    ComPtr<ID3D11Device> dev = CreateDevice(enableDebug, debugActive);
    if (!dev) { std::printf("FATAL: no D3D11 device\n"); return 1; }
    std::printf("Device: debug_layer=%s build=%s\n", debugActive ? "on" : "off", buildType);

    std::vector<std::pair<uint32_t,uint32_t>> sizes;
    if (w && h) sizes.push_back({ w, h });
    else { sizes.push_back({ 1920, 1080 }); sizes.push_back({ 2560, 1440 }); }

    for (auto& s : sizes) {
        BenchResult r = RunBenchmark(dev, s.first, s.second, warmup, frames);
        PrintResult(r, buildType, debugActive);
    }
    return 0;
}
