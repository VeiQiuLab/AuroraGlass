#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "controls/control_button.h"
#include "controls/control_slider.h"
#include "controls/control_toggle.h"
#include "core/d3d11_device.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"
#include "motion/control_motion.h"

using namespace AuroraGlass;
using namespace AuroraGlass::Motion;
using Microsoft::WRL::ComPtr;

static D3D11Device g_Device;
static GlassSurface g_Surface;

static ComPtr<ID3D11Texture2D> g_BackgroundTexture;
static ComPtr<ID3D11ShaderResourceView> g_BackgroundSRV;

static GlassButton g_Button;
static GlassToggle g_Toggle;
static GlassSlider g_Slider;

static ButtonMotion g_ButtonMotion;
static ToggleMotion g_ToggleMotion(false);
static SliderMotion g_SliderMotion;

static GlassMaterial g_ControlMaterial;
static GlassMaterial g_TrackMaterial;
static GlassMaterial g_FillMaterial;
static GlassMaterial g_ThumbMaterial;

static bool g_Running = true;
static bool g_Minimized = false;
static bool g_TrackingMouse = false;
static bool g_Scripted = false;

static uint32_t g_PendingW = 0;
static uint32_t g_PendingH = 0;

static float g_Time = 0.0f;
static int g_ScriptFrame = 0;
static int g_RuntimeFailures = 0;

static bool g_SawButtonRapid = false;
static bool g_SawToggleRapid = false;
static bool g_SawSliderDrag = false;
static bool g_SawResize = false;

static HWND g_Window = nullptr;

static bool Check(Status status, const char* where)
{
    if (status.ok())
        return true;

    std::printf(
        "[p4-motion] %s FAILED: code=%s hr=0x%08X\n",
        where,
        ErrorCodeToString(status.code),
        static_cast<unsigned>(status.hr));

    return false;
}

static bool Near(float a, float b, float epsilon = 0.001f)
{
    return std::fabs(a - b) <= epsilon;
}

static void RuntimeCheck(bool condition, const char* message)
{
    if (condition)
        return;

    ++g_RuntimeFailures;
    std::printf(
        "[p4-motion] RUNTIME CHECK FAILED: %s\n",
        message);
}

static uint32_t Pixel(int r, int g, int b)
{
    return 0xFF000000u |
           (static_cast<uint32_t>(b & 255) << 16) |
           (static_cast<uint32_t>(g & 255) << 8) |
           static_cast<uint32_t>(r & 255);
}

static void FillRect(
    std::vector<uint32_t>& pixels,
    uint32_t width,
    uint32_t height,
    int x,
    int y,
    int w,
    int h,
    uint32_t color)
{
    for (int yy = 0; yy < h; ++yy)
    {
        const int py = y + yy;

        if (py < 0 || py >= static_cast<int>(height))
            continue;

        for (int xx = 0; xx < w; ++xx)
        {
            const int px = x + xx;

            if (px < 0 || px >= static_cast<int>(width))
                continue;

            pixels[
                static_cast<size_t>(py) *
                    static_cast<size_t>(width) +
                static_cast<size_t>(px)] = color;
        }
    }
}

static void Layout(uint32_t width, uint32_t height)
{
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);

    g_Button.bounds = {
        w * 0.08f,
        h * 0.22f,
        210.0f,
        62.0f
    };

    g_Toggle.bounds = {
        w * 0.08f,
        h * 0.43f,
        126.0f,
        48.0f
    };

    g_Slider.bounds = {
        w * 0.30f,
        h * 0.70f,
        w * 0.54f,
        34.0f
    };
}

static void ConfigureMaterials()
{
    g_ControlMaterial.SetCornerRadius(18.0f);
    g_ControlMaterial.SetSpecularStrength(0.0f);
    g_ControlMaterial.SetEdgeFresnel(0.0f);
    g_ControlMaterial.SetDispersionStrength(0.0f);
    g_ControlMaterial.SetRefractionStrength(0.42f);
    g_ControlMaterial.SetTintAmount(0.05f);
    g_ControlMaterial.SetBrightness(1.0f);
    g_ControlMaterial.SetSaturation(1.0f);
    g_ControlMaterial.SetNoiseAmount(0.0f);
    g_ControlMaterial.SetBlurRadius(0.0f);
    g_ControlMaterial.SetOpacity(0.96f);

    g_Button.style.SetAll(g_ControlMaterial);
    g_Toggle.style.SetAll(g_ControlMaterial);
    g_Toggle.useCheckedStyle = false;

    g_TrackMaterial = g_ControlMaterial;
    g_TrackMaterial.SetCornerRadius(3.0f);
    g_TrackMaterial.SetRefractionStrength(0.30f);
    g_TrackMaterial.SetTintAmount(0.02f);

    g_FillMaterial = g_TrackMaterial;
    g_FillMaterial.SetRefractionStrength(0.36f);
    g_FillMaterial.SetTintAmount(0.07f);

    g_ThumbMaterial = g_ControlMaterial;
    g_ThumbMaterial.SetRefractionStrength(0.45f);
    g_ThumbMaterial.SetTintAmount(0.04f);
}

static bool CreateBackground(uint32_t width, uint32_t height)
{
    if (!g_Device.device || width == 0 || height == 0)
        return false;

    std::vector<uint32_t> pixels(
        static_cast<size_t>(width) *
        static_cast<size_t>(height),
        Pixel(238, 241, 245));

    const uint32_t grid = Pixel(198, 205, 214);
    const uint32_t dark = Pixel(30, 34, 42);
    const uint32_t light = Pixel(252, 252, 252);
    const uint32_t red = Pixel(205, 58, 65);
    const uint32_t blue = Pixel(55, 92, 205);

    for (int x = 0; x < static_cast<int>(width); x += 36)
        FillRect(
            pixels,
            width,
            height,
            x,
            0,
            1,
            static_cast<int>(height),
            grid);

    for (int y = 0; y < static_cast<int>(height); y += 36)
        FillRect(
            pixels,
            width,
            height,
            0,
            y,
            static_cast<int>(width),
            1,
            grid);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.38f),
        static_cast<int>(height * 0.12f),
        4,
        static_cast<int>(height * 0.46f),
        red);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.26f),
        static_cast<int>(height * 0.34f),
        static_cast<int>(width * 0.50f),
        4,
        blue);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.68f),
        static_cast<int>(height * 0.15f),
        static_cast<int>(width * 0.14f),
        static_cast<int>(height * 0.18f),
        dark);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.71f),
        static_cast<int>(height * 0.18f),
        static_cast<int>(width * 0.08f),
        static_cast<int>(height * 0.12f),
        light);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> texture;

    HRESULT hr =
        g_Device.device->CreateTexture2D(
            &desc,
            &init,
            &texture);

    if (FAILED(hr))
        return false;

    ComPtr<ID3D11ShaderResourceView> srv;

    hr =
        g_Device.device->CreateShaderResourceView(
            texture.Get(),
            nullptr,
            &srv);

    if (FAILED(hr))
        return false;

    g_BackgroundTexture = std::move(texture);
    g_BackgroundSRV = std::move(srv);

    return true;
}

static bool CopyBackgroundToBackbuffer()
{
    if (!g_BackgroundTexture ||
        !g_Device.swapChain ||
        !g_Device.context ||
        !g_Device.rtv)
    {
        return false;
    }

    ComPtr<ID3D11Texture2D> backBuffer;

    HRESULT hr =
        g_Device.swapChain->GetBuffer(
            0,
            IID_PPV_ARGS(&backBuffer));

    if (FAILED(hr))
        return false;

    g_Device.context->OMSetRenderTargets(
        0,
        nullptr,
        nullptr);

    g_Device.context->CopyResource(
        backBuffer.Get(),
        g_BackgroundTexture.Get());

    ID3D11RenderTargetView* rtv =
        g_Device.rtv.Get();

    g_Device.context->OMSetRenderTargets(
        1,
        &rtv,
        nullptr);

    return true;
}

static bool DrawGlass(
    const GlassMaterial& material,
    const ControlBounds& bounds)
{
    g_Surface.SetMaterial(material);

    GlassRect rect{
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height
    };

    return Check(
        g_Surface.RenderRect(
            g_Device.context.Get(),
            g_Device.rtv.Get(),
            rect),
        "RenderRect");
}

static ControlBounds ScaleAboutCenter(
    const ControlBounds& bounds,
    float scale)
{
    const float width = bounds.width * scale;
    const float height = bounds.height * scale;

    return {
        bounds.x + (bounds.width - width) * 0.5f,
        bounds.y + (bounds.height - height) * 0.5f,
        width,
        height
    };
}

static void AdvanceMotion(float dt)
{
    g_ButtonMotion.Sync(g_Button.State());
    g_ToggleMotion.Sync(g_Toggle.IsChecked());
    g_SliderMotion.Sync(g_Slider.State());

    const float sliderValueBefore =
        g_Slider.Value();

    g_ButtonMotion.Step(dt);
    g_ToggleMotion.Step(dt);
    g_SliderMotion.Step(dt);

    RuntimeCheck(
        Near(g_Slider.Value(), sliderValueBefore, 0.000001f),
        "motion step changed semantic slider value");
}

static bool RenderFrame()
{
    if (!CopyBackgroundToBackbuffer())
    {
        std::printf(
            "[p4-motion] background copy FAILED\n");

        return false;
    }

    FrameInfo frame{};
    frame.timeSeconds = g_Time;

    if (!Check(
        g_Surface.PrepareFrame(
            g_Device.context.Get(),
            g_BackgroundSRV.Get(),
            frame,
            0.0f),
        "PrepareFrame"))
    {
        return false;
    }

    const ButtonPresentation& buttonVisual =
        g_ButtonMotion.Presentation();

    const ControlBounds buttonBounds =
        ScaleAboutCenter(
            g_Button.bounds,
            buttonVisual.scale);

    if (!DrawGlass(
        g_Button.Material(),
        buttonBounds))
    {
        return false;
    }

    if (!DrawGlass(
        g_Toggle.Material(),
        g_Toggle.bounds))
    {
        return false;
    }

    const float toggleProgress =
        g_ToggleMotion.Presentation().progress;

    constexpr float knobSize = 32.0f;
    constexpr float knobInset = 7.0f;

    const float knobTravel =
        g_Toggle.bounds.width -
        knobSize -
        knobInset * 2.0f;

    ControlBounds toggleKnob{
        g_Toggle.bounds.x +
            knobInset +
            knobTravel * toggleProgress,
        g_Toggle.bounds.y +
            (g_Toggle.bounds.height - knobSize) * 0.5f,
        knobSize,
        knobSize
    };

    GlassMaterial knobMaterial =
        g_ThumbMaterial;

    knobMaterial.SetCornerRadius(
        knobSize * 0.5f);

    if (!DrawGlass(
        knobMaterial,
        toggleKnob))
    {
        return false;
    }

    const float centerY =
        g_Slider.bounds.y +
        g_Slider.bounds.height * 0.5f;

    constexpr float trackHeight = 6.0f;

    ControlBounds track{
        g_Slider.bounds.x,
        centerY - trackHeight * 0.5f,
        g_Slider.bounds.width,
        trackHeight
    };

    if (!DrawGlass(
        g_TrackMaterial,
        track))
    {
        return false;
    }

    const float fillWidth =
        std::max(
            trackHeight,
            g_Slider.bounds.width *
                g_Slider.NormalizedValue());

    ControlBounds fill{
        g_Slider.bounds.x,
        track.y,
        fillWidth,
        trackHeight
    };

    if (!DrawGlass(
        g_FillMaterial,
        fill))
    {
        return false;
    }

    const float thumbSize =
        g_SliderMotion.Presentation().thumbSizePx;

    const float thumbX =
        std::clamp(
            g_Slider.ThumbCenterX() -
                thumbSize * 0.5f,
            g_Slider.bounds.x,
            g_Slider.bounds.Right() -
                thumbSize);

    ControlBounds thumb{
        thumbX,
        centerY - thumbSize * 0.5f,
        thumbSize,
        thumbSize
    };

    GlassMaterial thumbMaterial =
        g_ThumbMaterial;

    thumbMaterial.SetCornerRadius(
        thumbSize * 0.5f);

    if (!DrawGlass(
        thumbMaterial,
        thumb))
    {
        return false;
    }

    HRESULT presentHr =
        g_Device.Present();

    if (FAILED(presentHr))
    {
        std::printf(
            "[p4-motion] Present FAILED hr=0x%08X\n",
            static_cast<unsigned>(presentHr));

        return false;
    }

    return true;
}

static ControlPoint MousePoint(LPARAM lParam)
{
    return {
        static_cast<float>(
            static_cast<short>(
                LOWORD(lParam))),
        static_cast<float>(
            static_cast<short>(
                HIWORD(lParam)))
    };
}

static LPARAM PointParam(ControlPoint point)
{
    const short x =
        static_cast<short>(
            std::lround(point.x));

    const short y =
        static_cast<short>(
            std::lround(point.y));

    return MAKELPARAM(x, y);
}

static ControlPoint Center(
    const ControlBounds& bounds)
{
    return {
        bounds.x + bounds.width * 0.5f,
        bounds.y + bounds.height * 0.5f
    };
}

static void SendMove(ControlPoint point)
{
    // Scripted mode drives the same frozen semantic APIs directly.
    // This keeps the runtime deterministic and prevents physical cursor
    // messages from interleaving with the synthetic interaction sequence.
    g_Button.PointerMove(point);
    g_Toggle.PointerMove(point);
    g_Slider.PointerMove(point);
}

static void SendDown(ControlPoint point)
{
    g_Button.PointerDown(point);
    g_Toggle.PointerDown(point);
    g_Slider.PointerDown(point);
}

static void SendUp(ControlPoint point)
{
    g_Button.PointerUp(point);
    g_Toggle.PointerUp(point);
    g_Slider.PointerUp(point);
}

static void Click(ControlPoint point)
{
    SendMove(point);
    SendDown(point);
    SendUp(point);
}

static void Evidence(const char* name)
{
    std::printf(
        "[motion-evidence] %s frame=%d "
        "buttonState=%d buttonScale=%.5f "
        "toggle=%d progress=%.5f "
        "sliderState=%d thumb=%.3f value=%.4f\n",
        name,
        g_ScriptFrame,
        static_cast<int>(g_Button.State()),
        g_ButtonMotion.Presentation().scale,
        g_Toggle.IsChecked() ? 1 : 0,
        g_ToggleMotion.Presentation().progress,
        static_cast<int>(g_Slider.State()),
        g_SliderMotion.Presentation().thumbSizePx,
        g_Slider.Value());
}

static void RunScriptedInput()
{
    const ControlPoint button =
        Center(g_Button.bounds);

    const ControlPoint toggle =
        Center(g_Toggle.bounds);

    const float sliderY =
        g_Slider.bounds.y +
        g_Slider.bounds.height * 0.5f;

    switch (g_ScriptFrame)
    {
    case 15:
        SendMove(button);
        Evidence("button-hover-input");
        break;

    case 28:
        SendDown(button);
        Evidence("button-press-input");
        break;

    case 34:
    {
        const float before =
            g_ButtonMotion.Presentation().scale;

        SendUp(button);
        g_ButtonMotion.Sync(g_Button.State());

        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                before,
                0.000001f),
            "button release retarget snapped");

        Evidence("button-release-retarget");
        break;
    }

    case 37:
    {
        const float before =
            g_ButtonMotion.Presentation().scale;

        SendDown(button);
        g_ButtonMotion.Sync(g_Button.State());

        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                before,
                0.000001f),
            "button rapid re-press snapped");

        g_SawButtonRapid = true;
        Evidence("button-rapid-repress");
        break;
    }

    case 43:
        SendUp(button);
        Evidence("button-final-release");
        break;

    case 52:
        SendMessageW(
            g_Window,
            WM_MOUSELEAVE,
            0,
            0);
        break;

    case 68:
        Click(toggle);
        Evidence("toggle-on");
        break;

    case 72:
    {
        const float before =
            g_ToggleMotion.Presentation().progress;

        Click(toggle);
        g_ToggleMotion.Sync(g_Toggle.IsChecked());

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                before,
                0.000001f),
            "toggle On-Off retarget snapped");

        Evidence("toggle-off-mid-motion");
        break;
    }

    case 76:
    {
        const float before =
            g_ToggleMotion.Presentation().progress;

        Click(toggle);
        g_ToggleMotion.Sync(g_Toggle.IsChecked());

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                before,
                0.000001f),
            "toggle Off-On retarget snapped");

        g_SawToggleRapid = true;
        Evidence("toggle-on-retarget");
        break;
    }

    case 96:
    {
        ControlPoint thumb{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        SendMove(thumb);
        Evidence("slider-hover");
        break;
    }

    case 103:
    {
        ControlPoint thumb{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        SendDown(thumb);
        Evidence("slider-press");
        break;
    }

    case 104:
    case 105:
    case 106:
    case 107:
    case 108:
    case 109:
    case 110:
    {
        const float t =
            static_cast<float>(
                g_ScriptFrame - 104) /
            6.0f;

        const float normalized =
            0.50f +
            (0.82f - 0.50f) * t;

        ControlPoint drag{
            g_Slider.bounds.x +
                g_Slider.bounds.width *
                    normalized,
            sliderY
        };

        SendMove(drag);

        RuntimeCheck(
            Near(
                g_Slider.NormalizedValue(),
                normalized,
                0.002f),
            "slider semantic value lagged pointer");

        g_SawSliderDrag = true;

        if (g_ScriptFrame == 107 ||
            g_ScriptFrame == 110)
        {
            Evidence("slider-drag");
        }

        break;
    }

    case 111:
    {
        ControlPoint current{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        SendUp(current);
        Evidence("slider-release");
        break;
    }

    case 114:
    {
        ControlPoint current{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        const float before =
            g_SliderMotion.Presentation().thumbSizePx;

        SendDown(current);
        g_SliderMotion.Sync(g_Slider.State());

        RuntimeCheck(
            Near(
                g_SliderMotion.Presentation().thumbSizePx,
                before,
                0.000001f),
            "slider rapid press retarget snapped");

        Evidence("slider-rapid-repress");
        break;
    }

    case 120:
    {
        ControlPoint current{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        SendUp(current);
        Evidence("slider-final-release");
        break;
    }

    case 145:
        SetWindowPos(
            g_Window,
            nullptr,
            0,
            0,
            1120,
            700,
            SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);

        Evidence("resize-request");
        break;

    case 180:
        Evidence("settled");
        break;

    case 195:
        DestroyWindow(g_Window);
        break;

    default:
        break;
    }
}

static LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            g_Minimized = true;
        }
        else
        {
            g_Minimized = false;
            g_PendingW = LOWORD(lParam);
            g_PendingH = HIWORD(lParam);
        }

        return 0;

    case WM_MOUSEMOVE:
    {
        if (g_Scripted)
            return 0;

        // Real interactive mode uses OS mouse-leave tracking.
        // Scripted mode injects synthetic coordinates; registering
        // TrackMouseEvent there would observe the PHYSICAL cursor instead
        // and can generate a spurious WM_MOUSELEAVE mid-drag.
        if (!g_Scripted &&
            !g_TrackingMouse)
        {
            TRACKMOUSEEVENT event{
                sizeof(event),
                TME_LEAVE,
                hwnd,
                0
            };

            TrackMouseEvent(&event);
            g_TrackingMouse = true;
        }

        const ControlPoint point =
            MousePoint(lParam);

        g_Button.PointerMove(point);
        g_Toggle.PointerMove(point);
        g_Slider.PointerMove(point);

        return 0;
    }

    case WM_MOUSELEAVE:
        g_TrackingMouse = false;
        g_Button.PointerLeave();
        g_Toggle.PointerLeave();
        g_Slider.PointerLeave();
        return 0;

    case WM_LBUTTONDOWN:
    {
        if (g_Scripted)
            return 0;

        SetCapture(hwnd);

        const ControlPoint point =
            MousePoint(lParam);

        g_Button.PointerDown(point);
        g_Toggle.PointerDown(point);
        g_Slider.PointerDown(point);

        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g_Scripted)
            return 0;

        const ControlPoint point =
            MousePoint(lParam);

        g_Button.PointerUp(point);
        g_Toggle.PointerUp(point);
        g_Slider.PointerUp(point);

        if (GetCapture() == hwnd)
            ReleaseCapture();

        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hwnd);

        return 0;

    case WM_DESTROY:
        g_Running = false;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(
            hwnd,
            msg,
            wParam,
            lParam);
    }
}

int main(int argc, char** argv)
{
    g_Scripted =
        argc > 1 &&
        std::strcmp(
            argv[1],
            "--scripted") == 0;

    std::printf(
        "AuroraGlass P4 Motion Smoke starting... scripted=%s\n",
        g_Scripted ? "yes" : "no");

    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor =
        LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName =
        L"AuroraGlassP4MotionSmokeClass";

    if (!RegisterClassW(&wc))
    {
        std::printf(
            "[p4-motion] RegisterClass FAILED\n");

        return 1;
    }

    RECT windowRect{
        0,
        0,
        1280,
        720
    };

    AdjustWindowRect(
        &windowRect,
        WS_OVERLAPPEDWINDOW,
        FALSE);

    g_Window =
        CreateWindowExW(
            0,
            wc.lpszClassName,
            L"AuroraGlass P4 Motion Integration",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            windowRect.right -
                windowRect.left,
            windowRect.bottom -
                windowRect.top,
            nullptr,
            nullptr,
            instance,
            nullptr);

    if (!g_Window)
    {
        std::printf(
            "[p4-motion] CreateWindow FAILED\n");

        return 1;
    }

    ShowWindow(
        g_Window,
        SW_SHOW);

    UpdateWindow(
        g_Window);

    if (!g_Device.Init(g_Window))
    {
        std::printf(
            "[p4-motion] D3D11Device::Init FAILED\n");

        return 1;
    }

    Layout(
        g_Device.width,
        g_Device.height);

    ConfigureMaterials();

    if (!g_Slider.SetRange(
            0.0f,
            1.0f).ok())
    {
        std::printf(
            "[p4-motion] slider range FAILED\n");

        return 1;
    }

    g_Slider.SetValue(0.5f);

    if (!CreateBackground(
            g_Device.width,
            g_Device.height))
    {
        std::printf(
            "[p4-motion] CreateBackground FAILED\n");

        return 1;
    }

    SurfaceDesc desc{};
    desc.width = g_Device.width;
    desc.height = g_Device.height;

    if (!Check(
            GlassSurface::Create(
                g_Device.device.Get(),
                desc,
                g_Surface),
            "GlassSurface::Create"))
    {
        return 1;
    }

    g_Button.onClick = []()
    {
        std::printf(
            "[p4-motion] Button clicked\n");
    };

    g_Toggle.onChanged = [](bool checked)
    {
        std::printf(
            "[p4-motion] Toggle semantic=%d\n",
            checked ? 1 : 0);
    };

    g_Slider.onValueChanged = [](float value)
    {
        if (g_Scripted)
        {
            std::printf(
                "[p4-motion] Slider semantic=%.4f\n",
                value);
        }
    };

    std::printf(
        "[p4-motion] Ready %ux%u debugLayer=%s\n",
        g_Device.width,
        g_Device.height,
        g_Device.debugLayerActive
            ? "yes"
            : "no");

    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous{};
    LARGE_INTEGER current{};

    QueryPerformanceFrequency(
        &frequency);

    QueryPerformanceCounter(
        &previous);

    while (g_Running)
    {
        MSG msg{};

        while (PeekMessageW(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE))
        {
            TranslateMessage(
                &msg);

            DispatchMessageW(
                &msg);

            if (msg.message == WM_QUIT)
                g_Running = false;
        }

        if (!g_Running)
            break;

        float dt = 0.0f;

        if (g_Scripted)
        {
            dt = 1.0f / 60.0f;
            RunScriptedInput();
        }
        else
        {
            QueryPerformanceCounter(
                &current);

            double seconds =
                static_cast<double>(
                    current.QuadPart -
                    previous.QuadPart) /
                static_cast<double>(
                    frequency.QuadPart);

            previous = current;

            if (seconds > 0.05)
                seconds = 0.05;

            dt =
                static_cast<float>(
                    seconds);
        }

        g_Time += dt;

        if (!g_Minimized &&
            g_PendingW > 0 &&
            g_PendingH > 0 &&
            (g_PendingW != g_Device.width ||
             g_PendingH != g_Device.height))
        {
            const uint32_t newWidth =
                g_PendingW;

            const uint32_t newHeight =
                g_PendingH;

            HRESULT resizeHr =
                g_Device.Resize(
                    newWidth,
                    newHeight);

            if (FAILED(resizeHr))
            {
                std::printf(
                    "[p4-motion] device resize FAILED hr=0x%08X\n",
                    static_cast<unsigned>(
                        resizeHr));

                return 2;
            }

            Layout(
                newWidth,
                newHeight);

            if (!CreateBackground(
                    newWidth,
                    newHeight))
            {
                std::printf(
                    "[p4-motion] background resize FAILED\n");

                return 2;
            }

            if (!Check(
                    g_Surface.Resize(
                        newWidth,
                        newHeight),
                    "GlassSurface::Resize"))
            {
                return 2;
            }

            g_SawResize = true;

            std::printf(
                "[p4-motion] Resized: %ux%u\n",
                newWidth,
                newHeight);
        }

        AdvanceMotion(dt);

        if (!g_Minimized)
        {
            if (!RenderFrame())
                return 3;
        }

        if (g_Scripted)
        {
            ++g_ScriptFrame;
            Sleep(16);
        }
    }

    g_Surface.Reset();
    g_BackgroundSRV.Reset();
    g_BackgroundTexture.Reset();

    if (g_Scripted)
    {
        RuntimeCheck(
            g_SawButtonRapid,
            "button rapid re-press path not executed");

        RuntimeCheck(
            g_SawToggleRapid,
            "toggle rapid retarget path not executed");

        RuntimeCheck(
            g_SawSliderDrag,
            "slider drag path not executed");

        RuntimeCheck(
            g_SawResize,
            "resize path not executed");

        std::printf(
            "[p4-motion] scripted summary: "
            "buttonRapid=%d toggleRapid=%d "
            "sliderDrag=%d resize=%d failures=%d\n",
            g_SawButtonRapid ? 1 : 0,
            g_SawToggleRapid ? 1 : 0,
            g_SawSliderDrag ? 1 : 0,
            g_SawResize ? 1 : 0,
            g_RuntimeFailures);

        if (g_RuntimeFailures != 0)
        {
            std::printf(
                "SCRIPTED_RUNTIME=FAIL\n");

            return 4;
        }

        std::printf(
            "SCRIPTED_RUNTIME=PASS\n");
    }

    std::printf(
        "[p4-motion] Clean exit\n");

    return 0;
}