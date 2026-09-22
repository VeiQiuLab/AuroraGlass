// ============================================================
// AuroraGlass P1 — Minimal automated tests (no external framework).
//
// Covers, per P1 scope:
//   A. GlassMaterial validation
//   B. Status / error semantics
//   C. GlassSurface lifecycle (uses a minimal real D3D11 device)
//   D. Minimal device-lost / recreate path
//
// Returns non-zero exit code if any check fails.
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

#include <cmath>
#include <cstdio>
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

#define CHECK_STATUS_OK(s) CHECK((s).ok())

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// A. GlassMaterial validation
// ---------------------------------------------------------------------------
static void TestMaterialValidation() {
    std::printf("[A] GlassMaterial validation\n");

    // Defaults valid and within range.
    GlassMaterial m;
    CHECK(m.GetBlurRadius()      >= 0.0f && m.GetBlurRadius()      <= 24.0f);
    CHECK(m.GetRefractionStrength() >= 0.0f && m.GetRefractionStrength() <= 1.0f);
    CHECK(m.GetCornerRadius()    >= 0.0f && m.GetCornerRadius()    <= 200.0f);
    CHECK(m.GetOpacity()         >= 0.0f && m.GetOpacity()         <= 1.0f);

    // Legal in-range set.
    CHECK_STATUS_OK(m.SetBlurRadius(12.0f));
    CHECK(m.GetBlurRadius() == 12.0f);

    // Boundaries.
    CHECK_STATUS_OK(m.SetBlurRadius(0.0f));
    CHECK(m.GetBlurRadius() == 0.0f);
    CHECK_STATUS_OK(m.SetBlurRadius(24.0f));
    CHECK(m.GetBlurRadius() == 24.0f);
    CHECK_STATUS_OK(m.SetCornerRadius(0.0f));
    CHECK_STATUS_OK(m.SetCornerRadius(200.0f));
    CHECK(m.GetCornerRadius() == 200.0f);

    // Out-of-range values are clamped (Ok), not rejected.
    CHECK_STATUS_OK(m.SetBlurRadius(-5.0f));
    CHECK(m.GetBlurRadius() == 0.0f);
    CHECK_STATUS_OK(m.SetBlurRadius(100.0f));
    CHECK(m.GetBlurRadius() == 24.0f);

    // NaN rejected, old value preserved.
    CHECK_STATUS_OK(m.SetBlurRadius(10.0f));
    Status s = m.SetBlurRadius(std::numeric_limits<float>::quiet_NaN());
    CHECK(s.code == ErrorCode::InvalidArgument);
    CHECK(m.GetBlurRadius() == 10.0f);

    // +Inf / -Inf rejected, old value preserved.
    s = m.SetBlurRadius(std::numeric_limits<float>::infinity());
    CHECK(s.code == ErrorCode::InvalidArgument);
    CHECK(m.GetBlurRadius() == 10.0f);
    s = m.SetBlurRadius(-std::numeric_limits<float>::infinity());
    CHECK(s.code == ErrorCode::InvalidArgument);
    CHECK(m.GetBlurRadius() == 10.0f);

    // Highlight position validation + clamp.
    CHECK_STATUS_OK(m.SetHighlightPosition(0.0f, 0.0f));
    CHECK_STATUS_OK(m.SetHighlightPosition(5.0f, -5.0f)); // clamped
    float hx = 0.0f, hy = 0.0f;
    m.GetHighlightPosition(hx, hy);
    CHECK(hx == 1.0f && hy == -1.0f);
    s = m.SetHighlightPosition(std::numeric_limits<float>::quiet_NaN(), 0.0f);
    CHECK(s.code == ErrorCode::InvalidArgument);

    // A representative sample of other setters reject NaN.
    CHECK(m.SetOpacity(std::numeric_limits<float>::quiet_NaN()).code == ErrorCode::InvalidArgument);
    CHECK(m.SetSaturation(std::numeric_limits<float>::quiet_NaN()).code == ErrorCode::InvalidArgument);
}

// ---------------------------------------------------------------------------
// B. Status / error semantics
// ---------------------------------------------------------------------------
static void TestStatusSemantics() {
    std::printf("[B] Status / error semantics\n");

    Status ok = Status::Ok();
    CHECK(ok.ok());
    CHECK(static_cast<bool>(ok));
    CHECK(ok.code == ErrorCode::Ok);

    Status ia = Status::InvalidArgument();
    CHECK(!ia.ok());
    CHECK(ia.code == ErrorCode::InvalidArgument);
    CHECK(ia.hr == E_INVALIDARG);

    const HRESULT h = 0x887A0005; // DXGI_ERROR_DEVICE_REMOVED
    Status de = Status::DeviceError(h);
    CHECK(de.code == ErrorCode::DeviceError);
    CHECK(de.hr == h); // HRESULT preserved

    Status re = Status::ResourceError(h);
    CHECK(re.code == ErrorCode::ResourceError);
    CHECK(re.hr == h);

    Status se = Status::ShaderError(h);
    CHECK(se.code == ErrorCode::ShaderError);
    CHECK(se.hr == h);

    Status dl = Status::DeviceLost(h);
    CHECK(dl.code == ErrorCode::DeviceLost);
    CHECK(dl.hr == h);

    Status ni = Status::NotInitialized();
    CHECK(ni.code == ErrorCode::NotInitialized);

    CHECK(std::string(ErrorCodeToString(ErrorCode::Ok)) == "Ok");
    CHECK(std::string(ErrorCodeToString(ErrorCode::DeviceLost)) == "DeviceLost");

    // HRESULT classification helper.
    CHECK(IsDeviceLostHResult(DXGI_ERROR_DEVICE_REMOVED));
    CHECK(IsDeviceLostHResult(DXGI_ERROR_DEVICE_RESET));
    CHECK(!IsDeviceLostHResult(S_OK));
    CHECK(!IsDeviceLostHResult(E_INVALIDARG));
}

// ---------------------------------------------------------------------------
// C. GlassSurface lifecycle
// ---------------------------------------------------------------------------
static void TestSurfaceLifecycle() {
    std::printf("[C] GlassSurface lifecycle\n");

    // Default / uninitialized state.
    GlassSurface surf;
    CHECK(!surf.IsInitialized());
    CHECK(surf.Width() == 0 && surf.Height() == 0);

    // Create with null device -> InvalidArgument.
    SurfaceDesc d; d.width = 64; d.height = 64;
    Status s = GlassSurface::Create(nullptr, d, surf);
    CHECK(s.code == ErrorCode::InvalidArgument);

    ComPtr<ID3D11Device> dev = CreateTestDevice();
    if (!dev) {
        std::printf("  SKIP: no D3D11 device available\n");
        return;
    }
    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);

    // Create with zero size -> InvalidArgument.
    SurfaceDesc dz; dz.width = 0; dz.height = 64;
    s = GlassSurface::Create(dev.Get(), dz, surf);
    CHECK(s.code == ErrorCode::InvalidArgument);

    // Create valid.
    s = GlassSurface::Create(dev.Get(), d, surf);
    CHECK_STATUS_OK(s);
    CHECK(surf.IsInitialized());
    CHECK(surf.Width() == 64 && surf.Height() == 64);

    // SetMaterial never fails for a well-formed material.
    GlassMaterial m;
    m.SetBlurRadius(8.0f);
    surf.SetMaterial(m);

    Targets tg;
    CHECK(MakeTargets(dev.Get(), 64, 64, tg));

    // Render with null args -> InvalidArgument.
    FrameInfo fi;
    Status r = surf.Render(nullptr, tg.rtv.Get(), tg.srv.Get(), fi);
    CHECK(r.code == ErrorCode::InvalidArgument);
    r = surf.Render(ctx.Get(), nullptr, tg.srv.Get(), fi);
    CHECK(r.code == ErrorCode::InvalidArgument);
    r = surf.Render(ctx.Get(), tg.rtv.Get(), nullptr, fi);
    CHECK(r.code == ErrorCode::InvalidArgument);

    // Valid render -> Ok.
    r = surf.Render(ctx.Get(), tg.rtv.Get(), tg.srv.Get(), fi);
    CHECK_STATUS_OK(r);

    // Reset -> uninitialized.
    surf.Reset();
    CHECK(!surf.IsInitialized());
    CHECK(surf.Width() == 0 && surf.Height() == 0);

    // Reset x2 idempotent.
    surf.Reset();
    CHECK(!surf.IsInitialized());

    // Create after Reset -> Ok.
    s = GlassSurface::Create(dev.Get(), d, surf);
    CHECK_STATUS_OK(s);
    CHECK(surf.IsInitialized());

    // Move construction.
    GlassSurface moved(std::move(surf));
    CHECK(moved.IsInitialized());
    CHECK(!surf.IsInitialized());           // moved-from is empty
    CHECK(surf.Width() == 0 && surf.Height() == 0);
    surf.Reset();                            // moved-from safe to use
    CHECK(!surf.IsInitialized());

    // Move assignment.
    GlassSurface a, b;
    CHECK_STATUS_OK(GlassSurface::Create(dev.Get(), d, a));
    b = std::move(a);
    CHECK(b.IsInitialized());
    CHECK(!a.IsInitialized());

    // Create after Reset on the moved-to object.
    b.Reset();
    CHECK_STATUS_OK(GlassSurface::Create(dev.Get(), d, b));
    CHECK(b.IsInitialized());
}

// ---------------------------------------------------------------------------
// D. Minimal device-lost / recreate path
// ---------------------------------------------------------------------------
static void TestDeviceLostRecreate() {
    std::printf("[D] Device-lost / recreate path\n");

    ComPtr<ID3D11Device> dev = CreateTestDevice();
    if (!dev) {
        std::printf("  SKIP: no D3D11 device available\n");
        return;
    }
    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);

    SurfaceDesc d; d.width = 64; d.height = 64;
    GlassSurface surf;
    Status s = GlassSurface::Create(dev.Get(), d, surf);
    CHECK_STATUS_OK(s);
    CHECK_STATUS_OK(surf.CheckDeviceLost());

    Targets tg;
    CHECK(MakeTargets(dev.Get(), 64, 64, tg));

    FrameInfo fi;
    CHECK_STATUS_OK(surf.Render(ctx.Get(), tg.rtv.Get(), tg.srv.Get(), fi));

    // NOTE: D3D11 exposes no programmatic device-removal API (there is no
    // D3D11 equivalent of ID3D12Device::RemoveDevice); a real hardware removal
    // can only be *observed* (ID3D11Device4::RegisterDeviceRemovedEvent) and is
    // deferred to P2 device-loss stress testing. Here we verify the observable
    // contract and the recovery path.
    //
    // The HRESULTs Core treats as device-lost are asserted in TestStatusSemantics
    // via IsDeviceLostHResult() and Status::DeviceLost() (HRESULT preserved).
    CHECK(IsDeviceLostHResult(DXGI_ERROR_DEVICE_REMOVED));
    CHECK(IsDeviceLostHResult(DXGI_ERROR_DEVICE_RESET));

    // Healthy device -> no loss reported.
    CHECK_STATUS_OK(surf.CheckDeviceLost());

    // Force teardown + recreate path even without a real removal:
    // Reset frees GPU resources, then Create on the same device must succeed.
    surf.Reset();
    CHECK(!surf.IsInitialized());
    Status rs = GlassSurface::Create(dev.Get(), d, surf);
    CHECK_STATUS_OK(rs);
    CHECK(surf.IsInitialized());
    CHECK_STATUS_OK(surf.CheckDeviceLost());

    // Teardown: drop all invalid GPU resources.
    surf.Reset();
    CHECK(!surf.IsInitialized());

    // Recreate with a fresh, valid device and confirm render recovers.
    ComPtr<ID3D11Device> dev2 = CreateTestDevice();
    CHECK(dev2 != nullptr);
    if (dev2) {
        ComPtr<ID3D11DeviceContext> ctx2;
        dev2->GetImmediateContext(&ctx2);
        Status rc = GlassSurface::Create(dev2.Get(), d, surf);
        CHECK_STATUS_OK(rc);
        CHECK(surf.IsInitialized());
        CHECK_STATUS_OK(surf.CheckDeviceLost());

        Targets tg2;
        CHECK(MakeTargets(dev2.Get(), 64, 64, tg2));
        Status rr2 = surf.Render(ctx2.Get(), tg2.rtv.Get(), tg2.srv.Get(), fi);
        CHECK_STATUS_OK(rr2);
    }
}

int main() {
    std::printf("AuroraGlass P1 tests\n");
    TestMaterialValidation();
    TestStatusSemantics();
    TestSurfaceLifecycle();
    TestDeviceLostRecreate();
    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}

