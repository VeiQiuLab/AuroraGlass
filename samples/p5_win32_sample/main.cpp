#define NOMINMAX

#include <Windows.h>
#include <CommCtrl.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "core/d3d11_device.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"

#include "controls/control_button.h"
#include "controls/control_geometry.h"
#include "controls/control_slider.h"
#include "controls/control_toggle.h"
#include "controls/control_visual_style.h"

#include "motion/control_motion.h"

#include "win32/win32_control_input_bridge.h"
#include "win32/win32_host_attachment.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace AuroraGlass;
using namespace AuroraGlass::Adapters::Win32;
using namespace AuroraGlass::Motion;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kClassName[] =
    L"AuroraGlass.P5.FreshWin32Sample";

constexpr wchar_t kWindowTitle[] =
    L"AuroraGlass P5 Fresh Win32 Integration";

D3D11Device g_Device;
GlassSurface g_Surface;

ComPtr<ID3D11Texture2D> g_BackgroundTexture;
ComPtr<ID3D11ShaderResourceView> g_BackgroundSRV;

Win32HostAttachment g_HostAttachment;
Win32ControlInputBridge g_InputBridge;

GlassButton g_Button;
GlassToggle g_Toggle;
GlassSlider g_Slider;

ControlVisualStyle g_ControlStyle;
GlassMaterial g_BaseMaterial;

ButtonMotion g_ButtonMotion;
ToggleMotion g_ToggleMotion;
SliderMotion g_SliderMotion;
LightFollowMotion g_LightMotion;

ControlBounds g_PanelBounds{};

Win32HostMetrics g_PendingMetrics{};
bool g_HasPendingResize = false;
bool g_Suspended = false;

UINT g_CurrentDpi = 0;

bool g_Scripted = false;
bool g_ReducedMotion = false;

bool g_WindowLaunched = false;
bool g_AttachSucceeded = false;
bool g_InitialMetricsValid = false;
bool g_InitialDpiValid = false;
bool g_RenderedGlass = false;
bool g_ButtonVerified = false;
bool g_ToggleVerified = false;
bool g_SliderVerified = false;
bool g_MotionObserved = false;
bool g_ResizeVerified = false;
bool g_ScreenshotSaved = false;
bool g_TeardownVerified = false;

int g_ButtonClicks = 0;
int g_ToggleChanges = 0;
int g_SliderChanges = 0;
int g_ScriptFrame = 0;

std::chrono::steady_clock::time_point g_StartTime;
std::chrono::steady_clock::time_point g_LastFrame;

bool StatusOk(const Status& status)
{
    return status.ok();
}

LPARAM PointParam(float x, float y)
{
    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));

    return MAKELPARAM(
        static_cast<WORD>(static_cast<short>(ix)),
        static_cast<WORD>(static_cast<short>(iy)));
}

ControlBounds ScaledBounds(
    const ControlBounds& bounds,
    float scale)
{
    const float cx = bounds.x + bounds.width * 0.5f;
    const float cy = bounds.y + bounds.height * 0.5f;
    const float w = bounds.width * scale;
    const float h = bounds.height * scale;

    return {
        cx - w * 0.5f,
        cy - h * 0.5f,
        w,
        h
    };
}

GlassRect ToGlassRect(const ControlBounds& bounds)
{
    return {
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height
    };
}

void Layout(
    std::uint32_t width,
    std::uint32_t height)
{
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);

    g_PanelBounds = {
        56.0f,
        52.0f,
        std::max(420.0f, w - 112.0f),
        std::max(380.0f, h - 104.0f)
    };

    const float x = g_PanelBounds.x + 46.0f;

    g_Button.bounds = {
        x,
        g_PanelBounds.y + 104.0f,
        180.0f,
        54.0f
    };

    g_Toggle.bounds = {
        x,
        g_PanelBounds.y + 194.0f,
        112.0f,
        44.0f
    };

    g_Slider.bounds = {
        x,
        g_PanelBounds.y + 286.0f,
        std::min(
            360.0f,
            std::max(
                220.0f,
                g_PanelBounds.width - 110.0f)),
        28.0f
    };
}

bool CreateBackgroundTexture()
{
    g_BackgroundSRV.Reset();
    g_BackgroundTexture.Reset();

    if (!g_Device.device || !g_Device.rtv) {
        return false;
    }

    ComPtr<ID3D11Resource> targetResource;
    g_Device.rtv->GetResource(targetResource.GetAddressOf());

    ComPtr<ID3D11Texture2D> targetTexture;

    if (FAILED(targetResource.As(&targetTexture))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC targetDesc{};
    targetTexture->GetDesc(&targetDesc);

    if (targetDesc.Width == 0 || targetDesc.Height == 0) {
        return false;
    }

    const bool rgba =
        targetDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
        targetDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    const bool bgra =
        targetDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        targetDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    if (!rgba && !bgra) {
        return false;
    }

    const std::size_t count =
        static_cast<std::size_t>(targetDesc.Width) *
        static_cast<std::size_t>(targetDesc.Height);

    std::vector<std::uint32_t> pixels(count);

    for (UINT y = 0; y < targetDesc.Height; ++y)
    {
        for (UINT x = 0; x < targetDesc.Width; ++x)
        {
            const bool major =
                (x % 64u) < 3u ||
                (y % 64u) < 3u;

            const bool minor =
                (x % 16u) == 0u ||
                (y % 16u) == 0u;

            const bool checker =
                ((x / 96u) + (y / 96u)) % 2u == 0u;

            const int dx =
                static_cast<int>(x) -
                static_cast<int>(targetDesc.Width * 3u / 4u);

            const int dy =
                static_cast<int>(y) -
                static_cast<int>(targetDesc.Height / 3u);

            const bool ring =
                std::abs(dx * dx + dy * dy - 115 * 115) < 1300;

            std::uint8_t r = checker ? 34u : 17u;
            std::uint8_t g = checker ? 74u : 35u;
            std::uint8_t b = checker ? 112u : 68u;

            if (minor) {
                r = 80u;
                g = 108u;
                b = 142u;
            }

            if (major) {
                r = 224u;
                g = 232u;
                b = 240u;
            }

            if (ring) {
                r = 230u;
                g = 92u;
                b = 78u;
            }

            std::uint32_t packed = 0;

            if (bgra) {
                packed =
                    static_cast<std::uint32_t>(b) |
                    (static_cast<std::uint32_t>(g) << 8u) |
                    (static_cast<std::uint32_t>(r) << 16u) |
                    0xFF000000u;
            } else {
                packed =
                    static_cast<std::uint32_t>(r) |
                    (static_cast<std::uint32_t>(g) << 8u) |
                    (static_cast<std::uint32_t>(b) << 16u) |
                    0xFF000000u;
            }

            pixels[
                static_cast<std::size_t>(y) *
                targetDesc.Width +
                x] = packed;
        }
    }

    D3D11_TEXTURE2D_DESC desc = targetDesc;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initial{};
    initial.pSysMem = pixels.data();
    initial.SysMemPitch =
        targetDesc.Width * sizeof(std::uint32_t);

    if (FAILED(
            g_Device.device->CreateTexture2D(
                &desc,
                &initial,
                g_BackgroundTexture.GetAddressOf())))
    {
        return false;
    }

    if (FAILED(
            g_Device.device->CreateShaderResourceView(
                g_BackgroundTexture.Get(),
                nullptr,
                g_BackgroundSRV.GetAddressOf())))
    {
        g_BackgroundTexture.Reset();
        return false;
    }

    return true;
}

bool CopyBackgroundToBackbuffer()
{
    if (!g_BackgroundTexture ||
        !g_Device.context ||
        !g_Device.rtv)
    {
        return false;
    }

    ComPtr<ID3D11Resource> target;
    g_Device.rtv->GetResource(target.GetAddressOf());

    if (!target) {
        return false;
    }

    g_Device.context->CopyResource(
        target.Get(),
        g_BackgroundTexture.Get());

    return true;
}

bool RenderGlass(
    const GlassMaterial& material,
    const ControlBounds& bounds)
{
    if (bounds.width <= 0.0f ||
        bounds.height <= 0.0f)
    {
        return true;
    }

    g_Surface.SetMaterial(material);

    return StatusOk(
        g_Surface.RenderRect(
            g_Device.context.Get(),
            g_Device.rtv.Get(),
            ToGlassRect(bounds)));
}

bool RenderFrame()
{
    if (g_Suspended ||
        !g_Surface.IsInitialized() ||
        !g_BackgroundSRV ||
        !g_Device.context ||
        !g_Device.rtv)
    {
        return true;
    }

    if (!CopyBackgroundToBackbuffer()) {
        return false;
    }

    FrameInfo frame{};

    frame.timeSeconds =
        std::chrono::duration<float>(
            std::chrono::steady_clock::now() -
            g_StartTime).count();

    if (!StatusOk(
            g_Surface.PrepareFrame(
                g_Device.context.Get(),
                g_BackgroundSRV.Get(),
                frame,
                g_BaseMaterial.GetBlurRadius())))
    {
        return false;
    }

    const auto& light =
        g_LightMotion.Presentation();

    GlassMaterial panelMaterial = g_BaseMaterial;

    if (!panelMaterial
            .SetHighlightPosition(light.x, light.y)
            .ok())
    {
        return false;
    }

    if (!RenderGlass(panelMaterial, g_PanelBounds)) {
        return false;
    }

    GlassMaterial buttonMaterial =
        g_ControlStyle.MaterialFor(g_Button.State());

    if (!buttonMaterial
            .SetHighlightPosition(light.x, light.y)
            .ok())
    {
        return false;
    }

    const ControlBounds buttonVisual =
        ScaledBounds(
            g_Button.bounds,
            g_ButtonMotion.Presentation().scale);

    if (!RenderGlass(buttonMaterial, buttonVisual)) {
        return false;
    }

    GlassMaterial toggleMaterial =
        g_ControlStyle.MaterialFor(g_Toggle.State());

    if (!RenderGlass(toggleMaterial, g_Toggle.bounds)) {
        return false;
    }

    const float inset = 5.0f;
    const float knob =
        g_Toggle.bounds.height - inset * 2.0f;

    const float travel =
        std::max(
            0.0f,
            g_Toggle.bounds.width -
            knob -
            inset * 2.0f);

    const ControlBounds toggleKnob{
        g_Toggle.bounds.x +
            inset +
            travel *
                g_ToggleMotion.Presentation().progress,
        g_Toggle.bounds.y + inset,
        knob,
        knob
    };

    if (!RenderGlass(toggleMaterial, toggleKnob)) {
        return false;
    }

    GlassMaterial sliderMaterial =
        g_ControlStyle.MaterialFor(g_Slider.State());

    const ControlBounds sliderTrack{
        g_Slider.bounds.x,
        g_Slider.bounds.y +
            g_Slider.bounds.height * 0.5f -
            5.0f,
        g_Slider.bounds.width,
        10.0f
    };

    if (!RenderGlass(sliderMaterial, sliderTrack)) {
        return false;
    }

    const float thumb =
        g_SliderMotion.Presentation().thumbSizePx;

    const float thumbX =
        g_Slider.bounds.x +
        g_Slider.Value() *
            g_Slider.bounds.width;

    const ControlBounds sliderThumb{
        thumbX - thumb * 0.5f,
        g_Slider.bounds.y +
            g_Slider.bounds.height * 0.5f -
            thumb * 0.5f,
        thumb,
        thumb
    };

    if (!RenderGlass(sliderMaterial, sliderThumb)) {
        return false;
    }

    if (FAILED(g_Device.Present())) {
        return false;
    }

    g_RenderedGlass = true;
    return true;
}

bool SaveBackbufferBmp(const char* path)
{
    if (!g_Device.device ||
        !g_Device.context ||
        !g_Device.rtv)
    {
        return false;
    }

    ComPtr<ID3D11Resource> resource;
    g_Device.rtv->GetResource(resource.GetAddressOf());

    ComPtr<ID3D11Texture2D> texture;

    if (FAILED(resource.As(&texture))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);

    const bool rgba =
        desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
        desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    const bool bgra =
        desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
        desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    if (!rgba && !bgra) {
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;

    if (FAILED(
            g_Device.device->CreateTexture2D(
                &stagingDesc,
                nullptr,
                staging.GetAddressOf())))
    {
        return false;
    }

    g_Device.context->CopyResource(
        staging.Get(),
        texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (FAILED(
            g_Device.context->Map(
                staging.Get(),
                0,
                D3D11_MAP_READ,
                0,
                &mapped)))
    {
        return false;
    }

    BITMAPFILEHEADER fileHeader{};
    BITMAPINFOHEADER infoHeader{};

    const std::uint32_t rowBytes =
        desc.Width * 4u;

    const std::uint32_t imageBytes =
        rowBytes * desc.Height;

    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits =
        sizeof(BITMAPFILEHEADER) +
        sizeof(BITMAPINFOHEADER);

    fileHeader.bfSize =
        fileHeader.bfOffBits +
        imageBytes;

    infoHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    infoHeader.biWidth =
        static_cast<LONG>(desc.Width);

    infoHeader.biHeight =
        static_cast<LONG>(desc.Height);

    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = imageBytes;

    std::ofstream out(path, std::ios::binary);

    if (!out) {
        g_Device.context->Unmap(staging.Get(), 0);
        return false;
    }

    out.write(
        reinterpret_cast<const char*>(&fileHeader),
        sizeof(fileHeader));

    out.write(
        reinterpret_cast<const char*>(&infoHeader),
        sizeof(infoHeader));

    std::vector<std::uint8_t> row(rowBytes);

    for (std::uint32_t outY = 0;
         outY < desc.Height;
         ++outY)
    {
        const std::uint32_t sourceY =
            desc.Height - 1u - outY;

        const auto* source =
            static_cast<const std::uint8_t*>(mapped.pData) +
            static_cast<std::size_t>(sourceY) *
                mapped.RowPitch;

        if (bgra) {
            std::copy(
                source,
                source + rowBytes,
                row.begin());
        } else {
            for (std::uint32_t x = 0;
                 x < desc.Width;
                 ++x)
            {
                const std::size_t p =
                    static_cast<std::size_t>(x) * 4u;

                row[p + 0] = source[p + 2];
                row[p + 1] = source[p + 1];
                row[p + 2] = source[p + 0];
                row[p + 3] = source[p + 3];
            }
        }

        out.write(
            reinterpret_cast<const char*>(row.data()),
            static_cast<std::streamsize>(row.size()));
    }

    g_Device.context->Unmap(staging.Get(), 0);

    return static_cast<bool>(out);
}

bool ApplyPendingResize()
{
    if (!g_HasPendingResize) {
        return true;
    }

    g_HasPendingResize = false;

    const Win32HostMetrics metrics =
        g_PendingMetrics;

    if (metrics.clientWidth == 0 ||
        metrics.clientHeight == 0)
    {
        g_Suspended = true;
        return true;
    }

    g_Suspended = false;

    if (FAILED(
            g_Device.Resize(
                metrics.clientWidth,
                metrics.clientHeight)))
    {
        return false;
    }

    if (!StatusOk(
            g_Surface.Resize(
                metrics.clientWidth,
                metrics.clientHeight)))
    {
        return false;
    }

    if (!CreateBackgroundTexture()) {
        return false;
    }

    Layout(
        metrics.clientWidth,
        metrics.clientHeight);

    g_ResizeVerified =
        g_Device.width == metrics.clientWidth &&
        g_Device.height == metrics.clientHeight &&
        g_Surface.Width() == metrics.clientWidth &&
        g_Surface.Height() == metrics.clientHeight;

    return true;
}

void SyncAndStepMotion(float dt)
{
    g_ButtonMotion.Sync(g_Button.State());
    g_ToggleMotion.Sync(g_Toggle.IsChecked());
    g_SliderMotion.Sync(g_Slider.State());

    g_ButtonMotion.Step(dt);
    g_ToggleMotion.Step(dt);
    g_SliderMotion.Step(dt);
    g_LightMotion.Step(dt);

    const auto& button =
        g_ButtonMotion.Presentation();

    const auto& toggle =
        g_ToggleMotion.Presentation();

    const auto& slider =
        g_SliderMotion.Presentation();

    const auto& light =
        g_LightMotion.Presentation();

    if (std::fabs(button.scale - 1.0f) > 0.0005f ||
        (toggle.progress > 0.001f &&
         toggle.progress < 0.999f) ||
        std::fabs(slider.thumbSizePx - 18.0f) > 0.01f ||
        std::fabs(light.x - 0.50f) > 0.001f ||
        std::fabs(light.y - 0.35f) > 0.001f)
    {
        g_MotionObserved = true;
    }
}

void DriveScriptedInput(HWND hwnd)
{
    ++g_ScriptFrame;

    const auto center =
        [](const ControlBounds& bounds)
        {
            return ControlPoint{
                bounds.x + bounds.width * 0.5f,
                bounds.y + bounds.height * 0.5f
            };
        };

    if (g_ScriptFrame == 10)
    {
        const ControlPoint p = center(g_Button.bounds);

        SendMessageW(
            hwnd,
            WM_MOUSEMOVE,
            0,
            PointParam(p.x, p.y));
    }
    else if (g_ScriptFrame == 18)
    {
        const ControlPoint p = center(g_Button.bounds);

        SendMessageW(
            hwnd,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            PointParam(p.x, p.y));
    }
    else if (g_ScriptFrame == 24)
    {
        const ControlPoint p = center(g_Button.bounds);

        SendMessageW(
            hwnd,
            WM_LBUTTONUP,
            0,
            PointParam(p.x, p.y));
    }
    else if (g_ScriptFrame == 36)
    {
        const ControlPoint p = center(g_Toggle.bounds);

        SendMessageW(
            hwnd,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            PointParam(p.x, p.y));

        SendMessageW(
            hwnd,
            WM_LBUTTONUP,
            0,
            PointParam(p.x, p.y));
    }
    else if (g_ScriptFrame == 52)
    {
        const ControlPoint p = center(g_Toggle.bounds);

        SendMessageW(
            hwnd,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            PointParam(p.x, p.y));

        SendMessageW(
            hwnd,
            WM_LBUTTONUP,
            0,
            PointParam(p.x, p.y));
    }
    else if (g_ScriptFrame == 66)
    {
        const float y =
            g_Slider.bounds.y +
            g_Slider.bounds.height * 0.5f;

        const float x =
            g_Slider.bounds.x +
            g_Slider.Value() *
                g_Slider.bounds.width;

        SendMessageW(
            hwnd,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            PointParam(x, y));
    }
    else if (g_ScriptFrame >= 70 &&
             g_ScriptFrame <= 82 &&
             (g_ScriptFrame % 2) == 0)
    {
        const float t =
            static_cast<float>(
                g_ScriptFrame - 70) /
            12.0f;

        const float x =
            g_Slider.bounds.x +
            g_Slider.bounds.width *
                (0.30f + t * 0.55f);

        const float y =
            g_Slider.bounds.y +
            g_Slider.bounds.height * 0.5f;

        SendMessageW(
            hwnd,
            WM_MOUSEMOVE,
            MK_LBUTTON,
            PointParam(x, y));
    }
    else if (g_ScriptFrame == 86)
    {
        const float x =
            g_Slider.bounds.x +
            g_Slider.bounds.width * 0.85f;

        const float y =
            g_Slider.bounds.y +
            g_Slider.bounds.height * 0.5f;

        SendMessageW(
            hwnd,
            WM_LBUTTONUP,
            0,
            PointParam(x, y));
    }
    else if (g_ScriptFrame == 100)
    {
        RECT rect{};

        if (GetWindowRect(hwnd, &rect))
        {
            SetWindowPos(
                hwnd,
                nullptr,
                0,
                0,
                rect.right - rect.left + 120,
                rect.bottom - rect.top + 70,
                SWP_NOMOVE |
                SWP_NOZORDER |
                SWP_NOACTIVATE);
        }
    }
    else if (g_ScriptFrame == 132)
    {
        g_ScreenshotSaved =
            SaveBackbufferBmp(
                "p5_win32_sample.bmp");
    }
    else if (g_ScriptFrame == 150)
    {
        g_ButtonVerified =
            g_ButtonClicks >= 1;

        g_ToggleVerified =
            g_ToggleChanges >= 2 &&
            !g_Toggle.IsChecked();

        g_SliderVerified =
            g_SliderChanges >= 2 &&
            g_Slider.Value() > 0.70f;

        PostMessageW(
            hwnd,
            WM_CLOSE,
            0,
            0);
    }
}

void WriteRuntimeReport()
{
    std::ofstream out(
        "p5_win32_sample_runtime.txt",
        std::ios::trunc);

    if (!out) {
        return;
    }

    const bool pass =
        g_WindowLaunched &&
        g_AttachSucceeded &&
        g_InitialMetricsValid &&
        g_InitialDpiValid &&
        g_RenderedGlass &&
        g_ButtonVerified &&
        g_ToggleVerified &&
        g_SliderVerified &&
        g_MotionObserved &&
        g_ResizeVerified &&
        g_ScreenshotSaved &&
        g_TeardownVerified;

    out << "P5_FRESH_WIN32_SAMPLE="
        << (pass ? "PASS" : "FAIL")
        << "\n";

    out << "WINDOW="
        << (g_WindowLaunched ? "PASS" : "FAIL")
        << "\n";

    out << "ATTACHMENT="
        << (g_AttachSucceeded ? "PASS" : "FAIL")
        << "\n";

    out << "INITIAL_METRICS="
        << (g_InitialMetricsValid ? "PASS" : "FAIL")
        << "\n";

    out << "INITIAL_DPI="
        << (g_InitialDpiValid ? "PASS" : "FAIL")
        << " value="
        << g_CurrentDpi
        << "\n";

    out << "GLASS="
        << (g_RenderedGlass ? "PASS" : "FAIL")
        << "\n";

    out << "BUTTON="
        << (g_ButtonVerified ? "PASS" : "FAIL")
        << " clicks="
        << g_ButtonClicks
        << "\n";

    out << "TOGGLE="
        << (g_ToggleVerified ? "PASS" : "FAIL")
        << " changes="
        << g_ToggleChanges
        << "\n";

    out << "SLIDER="
        << (g_SliderVerified ? "PASS" : "FAIL")
        << " changes="
        << g_SliderChanges
        << " value="
        << g_Slider.Value()
        << "\n";

    out << "MOTION="
        << (g_MotionObserved ? "PASS" : "FAIL")
        << "\n";

    out << "RESIZE="
        << (g_ResizeVerified ? "PASS" : "FAIL")
        << "\n";

    out << "SCREENSHOT="
        << (g_ScreenshotSaved ? "PASS" : "FAIL")
        << " path=p5_win32_sample.bmp\n";

    out << "TEARDOWN="
        << (g_TeardownVerified ? "PASS" : "FAIL")
        << "\n";

    out << "REDUCED_MOTION="
        << (g_ReducedMotion ? "ON" : "OFF")
        << "\n";
}

LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_MOUSEMOVE:
    {
        const ControlPoint pointer{
            static_cast<float>(
                GET_X_LPARAM(lParam)),
            static_cast<float>(
                GET_Y_LPARAM(lParam))
        };

        g_LightMotion.RetargetPointer(
            pointer,
            g_PanelBounds);

        break;
    }

    case WM_MOUSELEAVE:
        g_LightMotion.RetargetRest();
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

bool ConfigureAuroraGlass(HWND hwnd)
{
    if (!g_HostAttachment.Attach(hwnd)) {
        return false;
    }

    const Win32HostMetrics initial =
        g_HostAttachment.Metrics();

    g_AttachSucceeded = true;

    g_InitialMetricsValid =
        initial.clientWidth > 0 &&
        initial.clientHeight > 0;

    g_InitialDpiValid =
        initial.dpi > 0;

    g_CurrentDpi = initial.dpi;

    g_HostAttachment.SetResizeCallback(
        [](Win32HostMetrics metrics)
        {
            g_PendingMetrics = metrics;
            g_HasPendingResize = true;
        });

    g_HostAttachment.SetDpiChangedCallback(
        [](Win32HostMetrics metrics)
        {
            g_CurrentDpi = metrics.dpi;
        });

    if (!g_InitialMetricsValid ||
        !g_InitialDpiValid)
    {
        return false;
    }

    if (!g_Device.Init(hwnd)) {
        return false;
    }

    SurfaceDesc desc{};
    desc.width = initial.clientWidth;
    desc.height = initial.clientHeight;

    if (!StatusOk(
            GlassSurface::Create(
                g_Device.device.Get(),
                desc,
                g_Surface)))
    {
        return false;
    }

    g_BaseMaterial = GlassMaterial{};
    g_ControlStyle.SetAll(g_BaseMaterial);

    Layout(
        initial.clientWidth,
        initial.clientHeight);

    if (!g_Slider.SetRange(0.0f, 1.0f)) {
        return false;
    }

    g_Slider.SetValue(0.30f);

    g_Button.onClick =
        []()
        {
            ++g_ButtonClicks;
        };

    g_Toggle.onChanged =
        [](bool)
        {
            ++g_ToggleChanges;
        };

    g_Slider.onValueChanged =
        [](float)
        {
            ++g_SliderChanges;
        };

    if (!g_InputBridge.AddButton(g_Button) ||
        !g_InputBridge.AddToggle(g_Toggle) ||
        !g_InputBridge.AddSlider(g_Slider))
    {
        return false;
    }

    if (!g_InputBridge.Attach(hwnd)) {
        return false;
    }

    g_ButtonMotion.SetReducedMotion(g_ReducedMotion);
    g_ToggleMotion.SetReducedMotion(g_ReducedMotion);
    g_SliderMotion.SetReducedMotion(g_ReducedMotion);
    g_LightMotion.SetReducedMotion(g_ReducedMotion);

    return CreateBackgroundTexture();
}

void ShutdownAuroraGlass()
{
    g_InputBridge.Detach();
    g_HostAttachment.Detach();

    g_BackgroundSRV.Reset();
    g_BackgroundTexture.Reset();

    g_Surface.Reset();

    if (g_Device.context)
    {
        g_Device.context->ClearState();
        g_Device.context->Flush();
    }

    g_Device.rtv.Reset();
    g_Device.swapChain.Reset();
    g_Device.context.Reset();
    g_Device.device.Reset();
    g_Device.debugDevice.Reset();

    g_TeardownVerified =
        !g_InputBridge.IsAttached() &&
        !g_HostAttachment.IsAttached() &&
        !g_Surface.IsInitialized() &&
        !g_BackgroundSRV &&
        !g_BackgroundTexture;
}

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand)
{
    const std::wstring commandLine =
        GetCommandLineW();

    g_Scripted =
        commandLine.find(
            L"--scripted-smoke") !=
        std::wstring::npos;

    g_ReducedMotion =
        commandLine.find(
            L"--reduced-motion") !=
        std::wstring::npos;

    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_STANDARD_CLASSES;

    InitCommonControlsEx(&common);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc)) {
        return 10;
    }

    HWND hwnd =
        CreateWindowExW(
            0,
            kClassName,
            kWindowTitle,
            WS_OVERLAPPEDWINDOW |
            WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            920,
            680,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (hwnd == nullptr)
    {
        UnregisterClassW(
            kClassName,
            instance);

        return 11;
    }

    g_WindowLaunched = true;

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    if (!ConfigureAuroraGlass(hwnd))
    {
        DestroyWindow(hwnd);
        ShutdownAuroraGlass();
        WriteRuntimeReport();

        UnregisterClassW(
            kClassName,
            instance);

        return 12;
    }

    g_StartTime =
        std::chrono::steady_clock::now();

    g_LastFrame = g_StartTime;

    MSG message{};
    bool running = true;
    int exitCode = 0;

    while (running)
    {
        while (PeekMessageW(
            &message,
            nullptr,
            0,
            0,
            PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = false;

                exitCode =
                    static_cast<int>(
                        message.wParam);

                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (!running) {
            break;
        }

        if (!ApplyPendingResize())
        {
            exitCode = 20;
            break;
        }

        const auto now =
            std::chrono::steady_clock::now();

        float dt =
            std::chrono::duration<float>(
                now - g_LastFrame).count();

        g_LastFrame = now;

        dt = std::clamp(
            dt,
            0.0f,
            0.050f);

        if (g_Scripted) {
            DriveScriptedInput(hwnd);
        }

        SyncAndStepMotion(dt);

        if (!RenderFrame())
        {
            exitCode = 21;
            break;
        }

        Sleep(8);
    }

    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }

    ShutdownAuroraGlass();

    if (g_Scripted)
    {
        if (!g_ButtonVerified) {
            g_ButtonVerified =
                g_ButtonClicks >= 1;
        }

        if (!g_ToggleVerified) {
            g_ToggleVerified =
                g_ToggleChanges >= 2 &&
                !g_Toggle.IsChecked();
        }

        if (!g_SliderVerified) {
            g_SliderVerified =
                g_SliderChanges >= 2 &&
                g_Slider.Value() > 0.70f;
        }

        WriteRuntimeReport();

        const bool pass =
            g_WindowLaunched &&
            g_AttachSucceeded &&
            g_InitialMetricsValid &&
            g_InitialDpiValid &&
            g_RenderedGlass &&
            g_ButtonVerified &&
            g_ToggleVerified &&
            g_SliderVerified &&
            g_MotionObserved &&
            g_ResizeVerified &&
            g_ScreenshotSaved &&
            g_TeardownVerified;

        if (!pass && exitCode == 0) {
            exitCode = 30;
        }
    }

    UnregisterClassW(
        kClassName,
        instance);

    return exitCode;
}