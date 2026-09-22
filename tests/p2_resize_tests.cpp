// ============================================================
// AuroraGlass P2 — Resize / zero-size robustness (Slice 2).
//
// Core-level (no window) verification of the resize lifecycle:
//   - rapid resize sequence
//   - repeated same-size resize (no-op)
//   - very small valid sizes
//   - zero width / zero height handling contract
//   - size preservation after a rejected resize
//   - render after resize
//   - resize on uninitialized surface
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

#include <cstdio>
#include <cstdint>

#include "core/result.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"

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

static ComPtr<ID3D11Device> CreateTestDevice() {
    ComPtr<ID3D11Device> dev;
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_DEBUG, nullptr, 0,
                                   D3D11_SDK_VERSION, &dev, &fl, nullptr);
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

int main() {
    std::printf("AuroraGlass P2 resize robustness tests\n");

    ComPtr<ID3D11Device> dev = CreateTestDevice();
    if (!dev) { std::printf("FATAL: no D3D11 device\n"); return 1; }
    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);

    SurfaceDesc d; d.width = 64; d.height = 64;
    GlassSurface surf;
    Status cs = GlassSurface::Create(dev.Get(), d, surf);
    CHECK(cs.ok());
    CHECK(surf.Width() == 64 && surf.Height() == 64);

    // --- Rapid resize sequence ---
    const uint32_t seq[][2] = {
        { 320, 240 }, { 1280, 720 }, { 64, 64 }, { 800, 600 },
        { 1024, 768 }, { 16, 16 }, { 640, 480 }, { 320, 240 },
        { 1280, 720 }, { 1, 1 }, { 64, 64 },
    };
    for (auto& s : seq) {
        Status rs = surf.Resize(s[0], s[1]);
        CHECK(rs.ok());
        CHECK(surf.Width() == s[0] && surf.Height() == s[1]);
    }

    // --- Repeated same-size resize is a no-op Ok ---
    CHECK(surf.Resize(64, 64).ok());
    CHECK(surf.Resize(64, 64).ok());
    CHECK(surf.Width() == 64 && surf.Height() == 64);

    // --- Very small valid sizes ---
    CHECK(surf.Resize(1, 1).ok());
    CHECK(surf.Width() == 1 && surf.Height() == 1);
    CHECK(surf.Resize(2, 1).ok());
    CHECK(surf.Resize(1, 2).ok());
    CHECK(surf.Resize(8, 8).ok());

    // --- Zero-size handling contract ---
    // Restore a known-good size first.
    CHECK(surf.Resize(64, 64).ok());
    Status z0 = surf.Resize(0, 64);
    CHECK(z0.code == ErrorCode::InvalidArgument);
    CHECK(surf.Width() == 64 && surf.Height() == 64);   // size preserved
    Status z1 = surf.Resize(64, 0);
    CHECK(z1.code == ErrorCode::InvalidArgument);
    CHECK(surf.Width() == 64 && surf.Height() == 64);
    Status z2 = surf.Resize(0, 0);
    CHECK(z2.code == ErrorCode::InvalidArgument);
    CHECK(surf.Width() == 64 && surf.Height() == 64);

    // --- Render after resize ---
    Targets tg;
    CHECK(MakeTargets(dev.Get(), 64, 64, tg));
    FrameInfo fi;
    CHECK(surf.Render(ctx.Get(), tg.rtv.Get(), tg.srv.Get(), fi).ok());
    CHECK(surf.Resize(128, 128).ok());
    Targets tg2;
    CHECK(MakeTargets(dev.Get(), 128, 128, tg2));
    CHECK(surf.Render(ctx.Get(), tg2.rtv.Get(), tg2.srv.Get(), fi).ok());

    // --- Resize on uninitialized surface ---
    GlassSurface uninit;
    Status us = uninit.Resize(32, 32);
    CHECK(us.code == ErrorCode::NotInitialized);

    // --- Create rejects zero-size ---
    GlassSurface bad;
    SurfaceDesc dz; dz.width = 0; dz.height = 32;
    Status bz = GlassSurface::Create(dev.Get(), dz, bad);
    CHECK(bz.code == ErrorCode::InvalidArgument);
    CHECK(!bad.IsInitialized());

    // --- Reset then resize -> NotInitialized ---
    surf.Reset();
    CHECK(surf.Resize(32, 32).code == ErrorCode::NotInitialized);

    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
