// ============================================================
// AuroraGlass P3 Slice C — multi-control batch path tests.
// Uses a minimal real D3D11 device (no window, offscreen targets).
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
#include <cmath>
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
    std::printf("AuroraGlass P3 Slice C batch tests\n");

    ComPtr<ID3D11Device> dev = CreateTestDevice();
    if (!dev) { std::printf("FATAL: no device\n"); return 1; }
    ComPtr<ID3D11DeviceContext> ctx;
    dev->GetImmediateContext(&ctx);

    SurfaceDesc d; d.width = 256; d.height = 256;
    GlassSurface surf;
    CHECK(GlassSurface::Create(dev.Get(), d, surf).ok());

    Targets target, bg;
    CHECK(MakeTargets(dev.Get(), 256, 256, target));
    CHECK(MakeTargets(dev.Get(), 256, 256, bg));

    FrameInfo fi;   // all stages enabled by default

    // [1] PrepareFrame null ctx / null background -> InvalidArgument
    CHECK(surf.PrepareFrame(nullptr, bg.srv.Get(), fi, 8.0f).code == ErrorCode::InvalidArgument);
    CHECK(surf.PrepareFrame(ctx.Get(), nullptr, fi, 8.0f).code == ErrorCode::InvalidArgument);

    // [2] invalid / NaN blurRadius -> InvalidArgument
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi,
                            std::numeric_limits<float>::quiet_NaN()).code == ErrorCode::InvalidArgument);
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi,
                            std::numeric_limits<float>::infinity()).code == ErrorCode::InvalidArgument);

    // [4] RenderRect before PrepareFrame -> NotInitialized (behavioral check;
    // no public IsPrepared query exists)
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {0, 0, 64, 64}).code ==
          ErrorCode::NotInitialized);

    // [3] PrepareFrame -> RenderRect success
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi, 8.0f).ok());
    GlassMaterial m; m.SetBlurRadius(8.0f);
    surf.SetMaterial(m);
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 100, 100}).ok());
    CHECK(surf.RenderRect(nullptr, target.rtv.Get(), {10, 10, 100, 100}).code ==
          ErrorCode::InvalidArgument);
    CHECK(surf.RenderRect(ctx.Get(), nullptr, {10, 10, 100, 100}).code ==
          ErrorCode::InvalidArgument);

    // [5] zero / negative / non-finite rect -> InvalidArgument
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 0, 50}).code ==
          ErrorCode::InvalidArgument);
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 50, -1}).code ==
          ErrorCode::InvalidArgument);
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(),
                          {std::numeric_limits<float>::quiet_NaN(), 0, 50, 50}).code ==
          ErrorCode::InvalidArgument);

    // [6] partially offscreen rect accepted (clipped)
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {-20, -20, 100, 100}).ok());
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {200, 200, 200, 200}).ok());

    // [7] fully offscreen rect -> successful no-op
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {1000, 1000, 50, 50}).ok());
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {-500, -500, 50, 50}).ok());

    // [8] multiple RenderRect after one PrepareFrame succeed
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {0, 0, 60, 60}).ok());
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {70, 70, 60, 60}).ok());
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {140, 140, 60, 60}).ok());

    // [10] blur enabled: material blurRadius mismatch rejected
    GlassMaterial wrong; wrong.SetBlurRadius(12.0f);   // prepared was 8
    surf.SetMaterial(wrong);
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 50, 50}).code ==
          ErrorCode::InvalidArgument);

    // [14] repeated PrepareFrame replaces prior batch safely
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi, 12.0f).ok());
    surf.SetMaterial(wrong);   // now matches prepared 12
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 50, 50}).ok());

    // [11] blur disabled: differing material blurRadius does NOT cause rejection
    FrameInfo noBlur = fi; noBlur.stages.blur = false;
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), noBlur, 4.0f).ok());
    GlassMaterial mb; mb.SetBlurRadius(20.0f);   // differs from prepared 4
    surf.SetMaterial(mb);
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 50, 50}).ok());

    // [12] Resize invalidates prepared state (behavioral)
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi, 6.0f).ok());
    CHECK(surf.Resize(128, 128).ok());
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 50, 50}).code ==
          ErrorCode::NotInitialized);

    // [13] Reset invalidates prepared state (behavioral)
    SurfaceDesc d2; d2.width = 128; d2.height = 128;
    CHECK(GlassSurface::Create(dev.Get(), d2, surf).ok());
    CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi, 6.0f).ok());
    surf.Reset();
    CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 50, 50}).code ==
          ErrorCode::NotInitialized);

    // [17] RenderRect restores caller's rasterizer state + scissor rect.
    {
        SurfaceDesc d3; d3.width = 128; d3.height = 128;
        CHECK(GlassSurface::Create(dev.Get(), d3, surf).ok());

        // Create a distinct known rasterizer state (scissor OFF) and set it.
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE; rd.ScissorEnable = FALSE;
        ComPtr<ID3D11RasterizerState> knownRS;
        CHECK(SUCCEEDED(dev->CreateRasterizerState(&rd, knownRS.GetAddressOf())));
        ctx->RSSetState(knownRS.Get());

        // (A) Single known scissor rect.
        D3D11_RECT knownScissor{ 2, 3, 40, 41 };
        ctx->RSSetScissorRects(1, &knownScissor);

        GlassMaterial mok; mok.SetBlurRadius(8.0f);
        surf.SetMaterial(mok);
        CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi, 8.0f).ok());
        CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 60, 60}).ok());

        ComPtr<ID3D11RasterizerState> afterRS;
        ctx->RSGetState(afterRS.GetAddressOf());
        CHECK(afterRS.Get() == knownRS.Get());   // same object restored

        UINT n = 1; D3D11_RECT afterScissor{};
        ctx->RSGetScissorRects(&n, &afterScissor);
        CHECK(n == 1);
        CHECK(afterScissor.left == knownScissor.left &&
              afterScissor.top == knownScissor.top &&
              afterScissor.right == knownScissor.right &&
              afterScissor.bottom == knownScissor.bottom);

        // (B) THREE known scissor rects — full set must be restored.
        D3D11_RECT known3[3] = {
            {  1,  2, 11, 12 },
            { 20, 21, 31, 32 },
            { 40, 41, 51, 52 },
        };
        ctx->RSSetScissorRects(3, known3);

        CHECK(surf.PrepareFrame(ctx.Get(), bg.srv.Get(), fi, 8.0f).ok());
        CHECK(surf.RenderRect(ctx.Get(), target.rtv.Get(), {10, 10, 60, 60}).ok());

        UINT n3 = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        D3D11_RECT after3[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        ctx->RSGetScissorRects(&n3, after3);
        CHECK(n3 == 3);
        bool allMatch = (n3 == 3);
        for (UINT i = 0; i < n3 && allMatch; ++i) {
            allMatch = after3[i].left == known3[i].left &&
                       after3[i].top == known3[i].top &&
                       after3[i].right == known3[i].right &&
                       after3[i].bottom == known3[i].bottom;
        }
        CHECK(allMatch);
    }

    std::printf("\nChecks: %d, Failures: %d\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
    return g_failures == 0 ? 0 : 1;
}
