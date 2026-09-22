// ============================================================
// AuroraGlass P2 — Error-path coverage (Slice 5).
//
// Verifies the frozen P1 public API error semantics for reliably-triggerable
// paths, and the post-failure object state (Reset / recreate still work).
//
// Paths that CANNOT be triggered reliably/stably from a test process
// (real device removal, forced shader compile failure on a healthy device)
// are NOT faked here; see docs/P2_API_REVIEW.md "Not automatically covered".
//
// Does not modify any frozen P1 public API.
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
#include <limits>

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
    std::printf("AuroraGlass P2 error-path tests\n");

    // ---- Create(null device) ----
    {
        GlassSurface s;
        SurfaceDesc d; d.width = 32; d.height = 32;
        Status st = GlassSurface::Create(nullptr, d, s);
        CHECK(st.code == ErrorCode::InvalidArgument);
        CHECK(!s.IsInitialized());
    }

    // ---- Create with zero width / height ----
    {
        ComPtr<ID3D11Device> dev = CreateTestDevice();
        if (!dev) { std::printf("FATAL: no device\n"); return 1; }
        GlassSurface s;
        SurfaceDesc dw; dw.width = 0; dw.height = 32;
        CHECK(GlassSurface::Create(dev.Get(), dw, s).code == ErrorCode::InvalidArgument);
        CHECK(!s.IsInitialized());
        SurfaceDesc dh; dh.width = 32; dh.height = 0;
        CHECK(GlassSurface::Create(dev.Get(), dh, s).code == ErrorCode::InvalidArgument);
        CHECK(!s.IsInitialized());
    }

    ComPtr<ID3D11Device> dev = CreateTestDevice();
    if (!dev) { std::printf("FATAL: no device\n"); return 1; }
    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);
    Targets tg;
    CHECK(MakeTargets(dev.Get(), 64, 64, tg));

    // ---- Render before Create -> NotInitialized ----
    {
        GlassSurface s;
        FrameInfo fi;
        Status st = s.Render(ctx.Get(), tg.rtv.Get(), tg.srv.Get(), fi);
        CHECK(st.code == ErrorCode::NotInitialized);
    }

    // ---- Resize before Create -> NotInitialized ----
    {
        GlassSurface s;
        CHECK(s.Resize(32, 32).code == ErrorCode::NotInitialized);
    }

    // ---- Create valid, then null-input render variants ----
    {
        GlassSurface s;
        SurfaceDesc d; d.width = 64; d.height = 64;
        CHECK(GlassSurface::Create(dev.Get(), d, s).ok());
        FrameInfo fi;
        CHECK(s.Render(nullptr, tg.rtv.Get(), tg.srv.Get(), fi).code == ErrorCode::InvalidArgument);
        CHECK(s.Render(ctx.Get(), nullptr, tg.srv.Get(), fi).code == ErrorCode::InvalidArgument);
        CHECK(s.Render(ctx.Get(), tg.rtv.Get(), nullptr, fi).code == ErrorCode::InvalidArgument);

        // Valid render still works after the rejected calls.
        CHECK(s.Render(ctx.Get(), tg.rtv.Get(), tg.srv.Get(), fi).ok());
    }

    // ---- Invalid GlassMaterial parameter -> rejected, prior value kept ----
    {
        GlassMaterial m;
        CHECK(m.SetBlurRadius(10.0f).ok());
        Status st = m.SetBlurRadius(std::numeric_limits<float>::quiet_NaN());
        CHECK(st.code == ErrorCode::InvalidArgument);
        CHECK(m.GetBlurRadius() == 10.0f);
        st = m.SetRefractionStrength(std::numeric_limits<float>::infinity());
        CHECK(st.code == ErrorCode::InvalidArgument);
    }

    // ---- Repeated failure -> Reset -> valid Create ----
    {
        GlassSurface s;
        SurfaceDesc bad; bad.width = 0; bad.height = 0;
        for (int i = 0; i < 3; ++i)
            CHECK(GlassSurface::Create(dev.Get(), bad, s).code == ErrorCode::InvalidArgument);
        CHECK(!s.IsInitialized());
        s.Reset();   // safe after repeated failure
        SurfaceDesc good; good.width = 64; good.height = 64;
        CHECK(GlassSurface::Create(dev.Get(), good, s).ok());
        CHECK(s.IsInitialized());
        FrameInfo fi;
        CHECK(s.Render(ctx.Get(), tg.rtv.Get(), tg.srv.Get(), fi).ok());
    }

    // ---- device-lost classification helper (HRESULT preserved) ----
    {
        CHECK(IsDeviceLostHResult(DXGI_ERROR_DEVICE_REMOVED));
        CHECK(IsDeviceLostHResult(DXGI_ERROR_DEVICE_RESET));
        CHECK(!IsDeviceLostHResult(S_OK));
        Status st = Status::DeviceLost(DXGI_ERROR_DEVICE_REMOVED);
        CHECK(st.code == ErrorCode::DeviceLost);
        CHECK(st.hr == DXGI_ERROR_DEVICE_REMOVED);
    }

    // ---- Create on a healthy device after a failed Resize keeps validity ----
    {
        GlassSurface s;
        SurfaceDesc d; d.width = 64; d.height = 64;
        CHECK(GlassSurface::Create(dev.Get(), d, s).ok());
        CHECK(s.Resize(0, 0).code == ErrorCode::InvalidArgument);
        CHECK(s.IsInitialized());                 // still valid
        CHECK(s.Width() == 64 && s.Height() == 64); // size preserved
    }

    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
