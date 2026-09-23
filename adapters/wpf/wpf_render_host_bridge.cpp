#include "wpf/wpf_render_host_bridge.h"

#include "core/d3d11_device.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"
#include "core/result.h"

#include <Windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <new>
#include <vector>

using AuroraGlass::D3D11Device;
using AuroraGlass::ErrorCode;
using AuroraGlass::FrameInfo;
using AuroraGlass::GlassMaterial;
using AuroraGlass::GlassRect;
using AuroraGlass::GlassSurface;
using AuroraGlass::Status;
using AuroraGlass::SurfaceDesc;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kRenderWindowClass[] =
    L"AuroraGlass.P6.WpfRenderHost";

int32_t Code(ErrorCode code) noexcept {
    return static_cast<int32_t>(code);
}

int32_t Code(Status status) noexcept {
    return static_cast<int32_t>(status.code);
}

bool ApplyMaterialSnapshot(
    const AuroraGlassWpfMaterialSnapshot& snapshot,
    GlassMaterial& out) noexcept
{
    GlassMaterial material{};

    if (!material.SetBlurRadius(snapshot.blurRadius).ok()) return false;
    if (!material.SetRefractionStrength(snapshot.refractionStrength).ok()) return false;
    if (!material.SetDispersionStrength(snapshot.dispersionStrength).ok()) return false;
    if (!material.SetThickness(snapshot.thickness).ok()) return false;
    if (!material.SetEdgeFresnel(snapshot.edgeFresnel).ok()) return false;
    if (!material.SetSpecularStrength(snapshot.specularStrength).ok()) return false;
    if (!material.SetTintAmount(snapshot.tintAmount).ok()) return false;
    if (!material.SetSaturation(snapshot.saturation).ok()) return false;
    if (!material.SetBrightness(snapshot.brightness).ok()) return false;
    if (!material.SetNoiseAmount(snapshot.noiseAmount).ok()) return false;
    if (!material.SetCornerRadius(snapshot.cornerRadius).ok()) return false;
    if (!material.SetOpacity(snapshot.opacity).ok()) return false;
    if (!material.SetHighlightPosition(snapshot.highlightX, snapshot.highlightY).ok()) return false;

    out = material;
    return true;
}

bool EnsureWindowClass() noexcept;

} // namespace

struct AuroraGlassWpfRenderHost {
    HWND hwnd = nullptr;
    D3D11Device device{};
    GlassSurface surface{};
    GlassMaterial material{};

    ComPtr<ID3D11Texture2D> backBuffer;
    ComPtr<ID3D11Texture2D> background;
    ComPtr<ID3D11ShaderResourceView> backgroundSrv;

    std::vector<GlassRect> rects;

    uint64_t frameCount = 0;
    int32_t lastCoreStatus = Code(ErrorCode::Ok);
    bool initialized = false;

    std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();

    bool BuildBackground(uint32_t width, uint32_t height) noexcept {
        backgroundSrv.Reset();
        background.Reset();
        backBuffer.Reset();

        if (width == 0 || height == 0) {
            return true;
        }

        HRESULT hr =
            device.swapChain->GetBuffer(
                0,
                IID_PPV_ARGS(&backBuffer));

        if (FAILED(hr) || !backBuffer) {
            return false;
        }

        D3D11_TEXTURE2D_DESC backDesc{};
        backBuffer->GetDesc(&backDesc);

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = backDesc.Format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        std::vector<uint32_t> pixels;
        pixels.resize(
            static_cast<size_t>(width) *
            static_cast<size_t>(height));

        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const bool checker =
                    (((x / 56u) + (y / 56u)) & 1u) != 0u;

                uint8_t value =
                    checker ? 52u : 205u;

                if ((x % 72u) < 3u ||
                    (y % 72u) < 3u)
                {
                    value = 245u;
                }

                const uint32_t safeHeight =
                    height == 0u
                        ? 1u
                        : height;

                const float diagonal =
                    static_cast<float>(y) *
                    static_cast<float>(width) /
                    static_cast<float>(safeHeight);

                if (std::fabs(
                        static_cast<float>(x) -
                        diagonal) < 4.0f)
                {
                    value = 18u;
                }

                const uint32_t pixel =
                    0xff000000u |
                    (static_cast<uint32_t>(value) << 16u) |
                    (static_cast<uint32_t>(value) << 8u) |
                    static_cast<uint32_t>(value);

                pixels[
                    static_cast<size_t>(y) *
                    static_cast<size_t>(width) +
                    static_cast<size_t>(x)] = pixel;
            }
        }

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = pixels.data();
        data.SysMemPitch =
            width * static_cast<uint32_t>(sizeof(uint32_t));

        hr =
            device.device->CreateTexture2D(
                &desc,
                &data,
                &background);

        if (FAILED(hr) || !background) {
            return false;
        }

        hr =
            device.device->CreateShaderResourceView(
                background.Get(),
                nullptr,
                &backgroundSrv);

        return SUCCEEDED(hr) &&
            backgroundSrv != nullptr;
    }

    bool Initialize() noexcept {
        RECT rect{};

        if (!GetClientRect(hwnd, &rect)) {
            return false;
        }

        const uint32_t width =
            static_cast<uint32_t>(
                std::max<LONG>(
                    1,
                    rect.right - rect.left));

        const uint32_t height =
            static_cast<uint32_t>(
                std::max<LONG>(
                    1,
                    rect.bottom - rect.top));

        if (!device.Init(hwnd)) {
            return false;
        }

        SurfaceDesc desc{};
        desc.width = width;
        desc.height = height;

        Status status =
            GlassSurface::Create(
                device.device.Get(),
                desc,
                surface);

        if (!status.ok()) {
            lastCoreStatus = Code(status);
            return false;
        }

        surface.SetMaterial(material);

        if (!BuildBackground(width, height)) {
            return false;
        }

        initialized = true;

        SetTimer(
            hwnd,
            1,
            16,
            nullptr);

        return true;
    }

    void Resize(uint32_t width, uint32_t height) noexcept {
        if (!initialized) {
            return;
        }

        if (width == 0 || height == 0) {
            backBuffer.Reset();
            backgroundSrv.Reset();
            background.Reset();
            return;
        }

        backBuffer.Reset();
        backgroundSrv.Reset();
        background.Reset();

        HRESULT hr =
            device.Resize(
                width,
                height);

        if (FAILED(hr)) {
            lastCoreStatus = -1;
            return;
        }

        Status status =
            surface.Resize(
                width,
                height);

        if (!status.ok()) {
            lastCoreStatus = Code(status);
            return;
        }

        if (!BuildBackground(width, height)) {
            lastCoreStatus = -1;
            return;
        }

        lastCoreStatus =
            Code(ErrorCode::Ok);
    }

    void Render() noexcept {
        if (!initialized ||
            device.width == 0 ||
            device.height == 0 ||
            !backBuffer ||
            !backgroundSrv)
        {
            return;
        }

        ID3D11RenderTargetView* nullTarget =
            nullptr;

        device.context->OMSetRenderTargets(
            0,
            &nullTarget,
            nullptr);

        device.context->CopyResource(
            backBuffer.Get(),
            background.Get());

        FrameInfo frame{};

        const auto now =
            std::chrono::steady_clock::now();

        frame.timeSeconds =
            std::chrono::duration<float>(
                now - startTime).count();

        surface.SetMaterial(material);

        Status status =
            surface.PrepareFrame(
                device.context.Get(),
                backgroundSrv.Get(),
                frame,
                material.GetBlurRadius());

        if (!status.ok()) {
            lastCoreStatus = Code(status);
            return;
        }

        for (const GlassRect& rect : rects) {
            status =
                surface.RenderRect(
                    device.context.Get(),
                    device.rtv.Get(),
                    rect);

            if (!status.ok()) {
                lastCoreStatus = Code(status);
                return;
            }
        }

        HRESULT presentHr =
            device.Present();

        if (FAILED(presentHr)) {
            lastCoreStatus = -1;
            return;
        }

        lastCoreStatus =
            Code(ErrorCode::Ok);

        ++frameCount;
    }

    void Shutdown() noexcept {
        if (hwnd != nullptr) {
            KillTimer(
                hwnd,
                1);
        }

        initialized = false;

        device.context.Reset();
        surface.Reset();

        backgroundSrv.Reset();
        background.Reset();
        backBuffer.Reset();

        device.rtv.Reset();
        device.swapChain.Reset();
        device.device.Reset();
        device.debugDevice.Reset();

        hwnd = nullptr;
    }
};

namespace {

LRESULT CALLBACK RenderWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    AuroraGlassWpfRenderHost* host =
        reinterpret_cast<AuroraGlassWpfRenderHost*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create =
            reinterpret_cast<const CREATESTRUCTW*>(
                lParam);

        host =
            static_cast<AuroraGlassWpfRenderHost*>(
                create->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                host));

        if (host != nullptr) {
            host->hwnd = hwnd;
        }
    }

    if (host != nullptr) {
        switch (message) {
        case WM_SIZE:
            host->Resize(
                static_cast<uint32_t>(
                    LOWORD(lParam)),
                static_cast<uint32_t>(
                    HIWORD(lParam)));
            return 0;

        case WM_TIMER:
            if (wParam == 1) {
                host->Render();
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_NCHITTEST:
            return HTTRANSPARENT;

        case WM_NCDESTROY:
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                0);

            if (host->hwnd == hwnd) {
                host->hwnd = nullptr;
            }

            break;
        }
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

bool EnsureWindowClass() noexcept {
    static bool registered = false;

    if (registered) {
        return true;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = RenderWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kRenderWindowClass;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));

    if (RegisterClassW(&wc) == 0) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    registered = true;
    return true;
}

} // namespace

extern "C" {

AuroraGlassWpfRenderHost*
AuroraGlassWpfRenderHostCreate(
    intptr_t parentHwnd) noexcept
{
    HWND parent =
        reinterpret_cast<HWND>(
            parentHwnd);

    if (parent == nullptr ||
        !IsWindow(parent) ||
        !EnsureWindowClass())
    {
        return nullptr;
    }

    AuroraGlassWpfRenderHost* host =
        new (std::nothrow) AuroraGlassWpfRenderHost{};

    if (host == nullptr) {
        return nullptr;
    }

    RECT parentRect{};

    GetClientRect(
        parent,
        &parentRect);

    const int width =
        std::max<LONG>(
            1,
            parentRect.right - parentRect.left);

    const int height =
        std::max<LONG>(
            1,
            parentRect.bottom - parentRect.top);

    HWND child =
        CreateWindowExW(
            0,
            kRenderWindowClass,
            L"",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            width,
            height,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            host);

    if (child == nullptr) {
        delete host;
        return nullptr;
    }

    if (!host->Initialize()) {
        DestroyWindow(child);
        delete host;
        return nullptr;
    }

    return host;
}

void AuroraGlassWpfRenderHostDestroy(
    AuroraGlassWpfRenderHost* host) noexcept
{
    if (host == nullptr) {
        return;
    }

    HWND hwnd =
        host->hwnd;

    host->Shutdown();

    if (hwnd != nullptr &&
        IsWindow(hwnd))
    {
        DestroyWindow(hwnd);
    }

    delete host;
}

intptr_t AuroraGlassWpfRenderHostGetHwnd(
    const AuroraGlassWpfRenderHost* host) noexcept
{
    if (host == nullptr) {
        return 0;
    }

    return reinterpret_cast<intptr_t>(
        host->hwnd);
}

int32_t AuroraGlassWpfRenderHostSetMaterial(
    AuroraGlassWpfRenderHost* host,
    const AuroraGlassWpfMaterialSnapshot* snapshot) noexcept
{
    if (host == nullptr ||
        snapshot == nullptr)
    {
        return Code(
            ErrorCode::InvalidArgument);
    }

    GlassMaterial material{};

    if (!ApplyMaterialSnapshot(
            *snapshot,
            material))
    {
        return Code(
            ErrorCode::InvalidArgument);
    }

    host->material = material;

    if (host->surface.IsInitialized()) {
        host->surface.SetMaterial(
            host->material);
    }

    return Code(
        ErrorCode::Ok);
}

int32_t AuroraGlassWpfRenderHostSetRects(
    AuroraGlassWpfRenderHost* host,
    const AuroraGlassWpfRenderRect* rects,
    uint32_t count) noexcept
{
    if (host == nullptr ||
        (count > 0 && rects == nullptr) ||
        count > 32)
    {
        return Code(
            ErrorCode::InvalidArgument);
    }

    try {
        host->rects.clear();
        host->rects.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            const AuroraGlassWpfRenderRect& source =
                rects[i];

            if (!std::isfinite(source.x) ||
                !std::isfinite(source.y) ||
                !std::isfinite(source.width) ||
                !std::isfinite(source.height) ||
                source.width < 0.0f ||
                source.height < 0.0f)
            {
                return Code(
                    ErrorCode::InvalidArgument);
            }

            GlassRect rect{};
            rect.x = source.x;
            rect.y = source.y;
            rect.width = source.width;
            rect.height = source.height;

            host->rects.push_back(
                rect);
        }
    }
    catch (...) {
        return Code(
            ErrorCode::ResourceError);
    }

    return Code(
        ErrorCode::Ok);
}

int32_t AuroraGlassWpfRenderHostGetStats(
    const AuroraGlassWpfRenderHost* host,
    AuroraGlassWpfRenderStats* outStats) noexcept
{
    if (host == nullptr ||
        outStats == nullptr)
    {
        return Code(
            ErrorCode::InvalidArgument);
    }

    outStats->frameCount =
        host->frameCount;

    outStats->width =
        host->device.width;

    outStats->height =
        host->device.height;

    outStats->lastCoreStatus =
        host->lastCoreStatus;

    outStats->ready =
        host->initialized ? 1u : 0u;

    return Code(
        ErrorCode::Ok);
}

} // extern "C"
