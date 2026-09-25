#include "wpf/wpf_composition_bridge.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "core/glass_material.h"
#include "core/glass_surface.h"
#include "core/result.h"

#include <Windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <new>
#include <vector>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using AuroraGlass::ErrorCode;
using AuroraGlass::FrameInfo;
using AuroraGlass::GlassMaterial;
using AuroraGlass::GlassRect;
using AuroraGlass::GlassSurface;
using AuroraGlass::Status;
using AuroraGlass::SurfaceDesc;
using Microsoft::WRL::ComPtr;

namespace {

int32_t Code(ErrorCode c) noexcept { return static_cast<int32_t>(c); }
int32_t Code(Status s) noexcept { return static_cast<int32_t>(s.code); }

bool ApplyMaterialSnapshot(
    const AuroraGlassWpfMaterialSnapshot& snapshot,
    GlassMaterial& out) noexcept
{
    GlassMaterial m{};
    if (!m.SetBlurRadius(snapshot.blurRadius).ok()) return false;
    if (!m.SetRefractionStrength(snapshot.refractionStrength).ok()) return false;
    if (!m.SetDispersionStrength(snapshot.dispersionStrength).ok()) return false;
    if (!m.SetThickness(snapshot.thickness).ok()) return false;
    if (!m.SetEdgeFresnel(snapshot.edgeFresnel).ok()) return false;
    if (!m.SetSpecularStrength(snapshot.specularStrength).ok()) return false;
    if (!m.SetTintAmount(snapshot.tintAmount).ok()) return false;
    if (!m.SetSaturation(snapshot.saturation).ok()) return false;
    if (!m.SetBrightness(snapshot.brightness).ok()) return false;
    if (!m.SetNoiseAmount(snapshot.noiseAmount).ok()) return false;
    if (!m.SetCornerRadius(snapshot.cornerRadius).ok()) return false;
    if (!m.SetOpacity(snapshot.opacity).ok()) return false;
    if (!m.SetHighlightPosition(snapshot.highlightX, snapshot.highlightY).ok()) return false;
    out = m;
    return true;
}

// Internal hidden helper window. Used ONLY so D3D9Ex can create a device.
// It is a top-level window that is never shown and is never a render surface.
constexpr wchar_t kHelperClass[] = L"AuroraGlass.P6.WpfCompositionHelper";

LRESULT CALLBACK HelperProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefWindowProcW(h, m, w, l);
}

HWND CreateHelperWindow() noexcept {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = HelperProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kHelperClass;
        if (RegisterClassW(&wc) == 0) {
            if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
        }
        registered = true;
    }
    // A hidden top-level popup; never shown, never a WS_CHILD.
    return CreateWindowExW(
        0, kHelperClass, L"", WS_POPUP,
        0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

} // namespace

struct AuroraGlassWpfComposition {
    HWND helper = nullptr;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDirect3D9Ex> d3d9;
    ComPtr<IDirect3DDevice9Ex> device9;

    ComPtr<IDirect3DTexture9> sharedTex9;
    ComPtr<IDirect3DSurface9> sharedSurface9;
    ComPtr<ID3D11Texture2D> sharedTex11;
    ComPtr<ID3D11RenderTargetView> sharedRtv11;

    ComPtr<ID3D11Texture2D> background;
    ComPtr<ID3D11ShaderResourceView> backgroundSrv;

    GlassSurface surface{};
    GlassMaterial material{};
    std::vector<GlassRect> rects;

    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t frameCount = 0;
    int32_t lastCoreStatus = Code(ErrorCode::Ok);
    bool ready = false;

    // Smooth vertical gradient background (NOT a checkerboard / test pattern).
    bool BuildBackground() noexcept {
        background.Reset();
        backgroundSrv.Reset();
        if (width == 0 || height == 0) return true;

        D3D11_TEXTURE2D_DESC td{};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
        for (uint32_t y = 0; y < height; ++y) {
            float t = (height > 1) ? (float)y / (float)(height - 1) : 0.0f;
            // Top: deep indigo-blue; bottom: near-black slate.
            uint8_t r = (uint8_t)(28.0f + (14.0f - 28.0f) * t);
            uint8_t g = (uint8_t)(38.0f + (18.0f - 38.0f) * t);
            uint8_t b = (uint8_t)(66.0f + (26.0f - 66.0f) * t);
            for (uint32_t x = 0; x < width; ++x) {
                // A soft diagonal sheen (a gradient, not a hard test line).
                float sx = (float)x / (float)(width ? width : 1);
                float sheen = std::max(0.0f, 1.0f - std::abs(sx - (1.0f - t)) * 2.0f);
                uint8_t rr = (uint8_t)std::min(255.0f, r + sheen * 40.0f);
                uint8_t gg = (uint8_t)std::min(255.0f, g + sheen * 50.0f);
                uint8_t bb = (uint8_t)std::min(255.0f, b + sheen * 70.0f);
                pixels[static_cast<size_t>(y) * width + x] =
                    0xFF000000u | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | (uint32_t)bb;
            }
        }

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = pixels.data();
        sd.SysMemPitch = width * (uint32_t)sizeof(uint32_t);

        HRESULT hr = device->CreateTexture2D(&td, &sd, background.GetAddressOf());
        if (FAILED(hr) || !background) return false;
        hr = device->CreateShaderResourceView(background.Get(), nullptr, backgroundSrv.GetAddressOf());
        return SUCCEEDED(hr) && backgroundSrv;
    }

    // Create the shared D3D9Ex render texture + open it on D3D11 + RTV.
    bool CreateSharedTarget() noexcept {
        sharedRtv11.Reset();
        sharedTex11.Reset();
        sharedSurface9.Reset();
        sharedTex9.Reset();
        if (width == 0 || height == 0) return true;

        HANDLE hShared = nullptr;
        HRESULT hr = device9->CreateTexture(
            width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, sharedTex9.GetAddressOf(), &hShared);
        if (FAILED(hr) || !hShared || !sharedTex9) return false;

        hr = sharedTex9->GetSurfaceLevel(0, sharedSurface9.GetAddressOf());
        if (FAILED(hr) || !sharedSurface9) return false;

        hr = device->OpenSharedResource(
            hShared, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(sharedTex11.GetAddressOf()));
        if (FAILED(hr) || !sharedTex11) return false;

        hr = device->CreateRenderTargetView(sharedTex11.Get(), nullptr, sharedRtv11.GetAddressOf());
        return SUCCEEDED(hr) && sharedRtv11;
    }

    bool CreateDevices() noexcept {
        helper = CreateHelperWindow();
        if (!helper) return false;

        HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, d3d9.GetAddressOf());
        if (FAILED(hr)) return false;

        D3DPRESENT_PARAMETERS pp{};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = helper;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        hr = d3d9->CreateDeviceEx(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, helper,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
            &pp, nullptr, device9.GetAddressOf());
        if (FAILED(hr)) return false;

        ComPtr<IDXGIFactory1> factory;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory.GetAddressOf())))
            return false;
        ComPtr<IDXGIAdapter1> adapter;
        if (FAILED(factory->EnumAdapters1(0, adapter.GetAddressOf()))) return false;

        D3D_FEATURE_LEVEL fl{};
        hr = D3D11CreateDevice(
            adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            device.GetAddressOf(), &fl, context.GetAddressOf());
        if (FAILED(hr) || !device || !context) return false;

        return true;
    }

    bool Initialize(uint32_t w, uint32_t h) noexcept {
        width = std::max<uint32_t>(1, w);
        height = std::max<uint32_t>(1, h);

        if (!CreateDevices()) return false;
        if (!CreateSharedTarget()) return false;

        SurfaceDesc desc{};
        desc.width = width;
        desc.height = height;
        Status st = GlassSurface::Create(device.Get(), desc, surface);
        if (!st.ok()) { lastCoreStatus = Code(st); return false; }

        surface.SetMaterial(material);

        if (!BuildBackground()) return false;

        ready = true;
        return true;
    }

    int32_t Resize(uint32_t w, uint32_t h) noexcept {
        if (!ready) return Code(ErrorCode::NotInitialized);
        if (w == 0 || h == 0) return Code(ErrorCode::InvalidArgument);

        width = w;
        height = h;

        if (!CreateSharedTarget()) { lastCoreStatus = -1; return lastCoreStatus; }

        Status st = surface.Resize(width, height);
        if (!st.ok()) { lastCoreStatus = Code(st); return lastCoreStatus; }

        if (!BuildBackground()) { lastCoreStatus = -1; return lastCoreStatus; }

        lastCoreStatus = Code(ErrorCode::Ok);
        return lastCoreStatus;
    }

    int32_t Render(float timeSeconds) noexcept {
        if (!ready || width == 0 || height == 0 ||
            !sharedRtv11 || !backgroundSrv) {
            return Code(ErrorCode::NotInitialized);
        }

        ID3D11RenderTargetView* nullTarget = nullptr;
        context->OMSetRenderTargets(0, &nullTarget, nullptr);

        // Baseline: the sharp background becomes the initial target content.
        context->CopyResource(sharedTex11.Get(), background.Get());

        FrameInfo frame{};
        frame.timeSeconds = timeSeconds;

        surface.SetMaterial(material);

        Status st = surface.PrepareFrame(
            context.Get(), backgroundSrv.Get(), frame, material.GetBlurRadius());
        if (!st.ok()) { lastCoreStatus = Code(st); return lastCoreStatus; }

        for (const GlassRect& r : rects) {
            st = surface.RenderRect(context.Get(), sharedRtv11.Get(), r);
            if (!st.ok()) { lastCoreStatus = Code(st); return lastCoreStatus; }
        }

        context->Flush();
        lastCoreStatus = Code(ErrorCode::Ok);
        ++frameCount;
        return lastCoreStatus;
    }

    void Shutdown() noexcept {
        ready = false;
        rects.clear();
        surface.Reset();
        backgroundSrv.Reset();
        background.Reset();
        sharedRtv11.Reset();
        sharedTex11.Reset();
        sharedSurface9.Reset();
        sharedTex9.Reset();
        context.Reset();
        device.Reset();
        device9.Reset();
        d3d9.Reset();
        if (helper) { DestroyWindow(helper); helper = nullptr; }
    }
};

extern "C" {

AuroraGlassWpfComposition*
AuroraGlassWpfCompositionCreate(uint32_t width, uint32_t height) noexcept {
    AuroraGlassWpfComposition* c = new (std::nothrow) AuroraGlassWpfComposition{};
    if (!c) return nullptr;
    if (!c->Initialize(width, height)) {
        c->Shutdown();
        delete c;
        return nullptr;
    }
    return c;
}

void AuroraGlassWpfCompositionDestroy(AuroraGlassWpfComposition* c) noexcept {
    if (!c) return;
    c->Shutdown();
    delete c;
}

intptr_t AuroraGlassWpfCompositionGetSurface(const AuroraGlassWpfComposition* c) noexcept {
    if (!c || !c->sharedSurface9) return 0;
    return reinterpret_cast<intptr_t>(c->sharedSurface9.Get());
}

int32_t AuroraGlassWpfCompositionResize(
    AuroraGlassWpfComposition* c, uint32_t width, uint32_t height) noexcept
{
    if (!c) return Code(ErrorCode::InvalidArgument);
    return c->Resize(width, height);
}

int32_t AuroraGlassWpfCompositionSetMaterial(
    AuroraGlassWpfComposition* c,
    const AuroraGlassWpfMaterialSnapshot* snapshot) noexcept
{
    if (!c || !snapshot) return Code(ErrorCode::InvalidArgument);
    GlassMaterial m{};
    if (!ApplyMaterialSnapshot(*snapshot, m)) return Code(ErrorCode::InvalidArgument);
    c->material = m;
    return Code(ErrorCode::Ok);
}

int32_t AuroraGlassWpfCompositionSetRects(
    AuroraGlassWpfComposition* c,
    const AuroraGlassWpfRenderRect* rects,
    uint32_t count) noexcept
{
    if (!c) return Code(ErrorCode::InvalidArgument);
    c->rects.clear();
    if (rects && count) {
        c->rects.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            GlassRect r{};
            r.x = rects[i].x;
            r.y = rects[i].y;
            r.width = rects[i].width;
            r.height = rects[i].height;
            c->rects.push_back(r);
        }
    }
    return Code(ErrorCode::Ok);
}

int32_t AuroraGlassWpfCompositionRender(
    AuroraGlassWpfComposition* c, float timeSeconds) noexcept
{
    if (!c) return Code(ErrorCode::InvalidArgument);
    return c->Render(timeSeconds);
}

int32_t AuroraGlassWpfCompositionGetStats(
    const AuroraGlassWpfComposition* c,
    AuroraGlassWpfCompositionStats* out) noexcept
{
    if (!c || !out) return Code(ErrorCode::InvalidArgument);
    out->frameCount = c->frameCount;
    out->width = c->width;
    out->height = c->height;
    out->lastCoreStatus = c->lastCoreStatus;
    out->ready = c->ready ? 1u : 0u;
    return Code(ErrorCode::Ok);
}

} // extern "C"
