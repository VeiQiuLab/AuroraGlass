// ============================================================
// AuroraGlass P3 Slice E - Reusability Proof.
//
// Independent Win32 consumer using AuroraGlass Core + Controls.
// No dependency on p3_interactive_smoke private implementation.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "core/d3d11_device.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"
#include "controls/control_button.h"
#include "controls/control_toggle.h"
#include "controls/control_slider.h"

using namespace AuroraGlass;
using Microsoft::WRL::ComPtr;

static D3D11Device g_Device;
static GlassSurface g_Surface;

static ComPtr<ID3D11Texture2D> g_BackgroundTexture;
static ComPtr<ID3D11ShaderResourceView> g_BackgroundSRV;

static GlassButton g_Button;
static GlassToggle g_Toggle;
static GlassSlider g_Slider;

static GlassMaterial g_LensMaterial;
static GlassMaterial g_TrackMaterial;
static GlassMaterial g_FillMaterial;
static GlassMaterial g_ThumbIdleMaterial;
static GlassMaterial g_ThumbPressedMaterial;

static ControlBounds g_LensBounds{};

static bool g_Running = true;
static bool g_Minimized = false;
static bool g_TrackingMouse = false;
static uint32_t g_PendingW = 0;
static uint32_t g_PendingH = 0;
static float g_Time = 0.0f;

static bool Check(Status s, const char* where)
{
    if (s.ok())
        return true;

    std::printf(
        "[reuse] %s FAILED: code=%s hr=0x%08X\n",
        where,
        ErrorCodeToString(s.code),
        static_cast<unsigned>(s.hr));

    return false;
}

static void Layout(uint32_t width, uint32_t height)
{
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);

    g_Button.bounds = { w * 0.08f, h * 0.72f, 190.0f, 54.0f };
    g_Toggle.bounds = { w * 0.08f, h * 0.82f, 112.0f, 44.0f };
    g_Slider.bounds = { w * 0.35f, h * 0.80f, w * 0.46f, 28.0f };

    const float lensW = std::min(300.0f, w * 0.32f);
    const float lensH = std::min(180.0f, h * 0.30f);

    g_LensBounds = {
        w * 0.55f,
        h * 0.17f,
        lensW,
        lensH
    };
}

static void Soften(GlassMaterial& m)
{
    m.SetSpecularStrength(0.0f);
    m.SetEdgeFresnel(0.0f);
    m.SetDispersionStrength(0.0f);
    m.SetTintAmount(0.0f);
    m.SetBrightness(1.0f);
    m.SetSaturation(1.0f);
    m.SetRefractionStrength(0.45f);
    m.SetBlurRadius(0.0f);
}

static void ConfigureMaterials()
{
    for (auto* style : {
        &g_Button.style,
        &g_Toggle.style,
        &g_Toggle.checkedStyle })
    {
        Soften(style->normal);
        Soften(style->hover);
        Soften(style->pressed);
        Soften(style->disabled);
        Soften(style->focused);
    }

    for (auto* style : {
        &g_Button.style,
        &g_Toggle.style })
    {
        style->normal.SetTintAmount(0.06f);
        style->normal.SetBrightness(0.985f);
        style->normal.SetOpacity(0.96f);

        style->hover.SetTintAmount(0.09f);
        style->hover.SetBrightness(0.99f);
        style->hover.SetOpacity(0.97f);

        style->pressed.SetTintAmount(0.12f);
        style->pressed.SetBrightness(0.95f);
        style->pressed.SetOpacity(0.98f);

        style->focused.SetTintAmount(0.08f);
        style->focused.SetBrightness(0.985f);
        style->focused.SetOpacity(0.96f);

        style->disabled.SetTintAmount(0.03f);
        style->disabled.SetBrightness(0.99f);
        style->disabled.SetOpacity(0.90f);
    }

    g_Toggle.checkedStyle = g_Toggle.style;
    g_Toggle.checkedStyle.normal.SetTintAmount(0.22f);
    g_Toggle.checkedStyle.normal.SetBrightness(0.96f);
    g_Toggle.checkedStyle.hover.SetTintAmount(0.26f);
    g_Toggle.useCheckedStyle = true;

    g_LensMaterial.SetCornerRadius(22.0f);
    g_LensMaterial.SetSpecularStrength(0.0f);
    g_LensMaterial.SetEdgeFresnel(0.0f);
    g_LensMaterial.SetRefractionStrength(0.50f);
    g_LensMaterial.SetDispersionStrength(0.0f);
    g_LensMaterial.SetTintAmount(0.0f);
    g_LensMaterial.SetBrightness(1.0f);
    g_LensMaterial.SetSaturation(1.0f);
    g_LensMaterial.SetNoiseAmount(0.0f);
    g_LensMaterial.SetBlurRadius(0.0f);

    g_TrackMaterial.SetCornerRadius(3.0f);
    g_TrackMaterial.SetSpecularStrength(0.0f);
    g_TrackMaterial.SetEdgeFresnel(0.0f);
    g_TrackMaterial.SetRefractionStrength(0.30f);
    g_TrackMaterial.SetDispersionStrength(0.0f);
    g_TrackMaterial.SetTintAmount(0.0f);
    g_TrackMaterial.SetBrightness(0.92f);
    g_TrackMaterial.SetNoiseAmount(0.0f);
    g_TrackMaterial.SetOpacity(1.0f);
    g_TrackMaterial.SetBlurRadius(0.0f);

    g_FillMaterial = g_TrackMaterial;
    g_FillMaterial.SetRefractionStrength(0.35f);
    g_FillMaterial.SetTintAmount(0.06f);

    g_ThumbIdleMaterial.SetCornerRadius(9.0f);
    g_ThumbIdleMaterial.SetSpecularStrength(0.0f);
    g_ThumbIdleMaterial.SetEdgeFresnel(0.0f);
    g_ThumbIdleMaterial.SetRefractionStrength(0.42f);
    g_ThumbIdleMaterial.SetDispersionStrength(0.0f);
    g_ThumbIdleMaterial.SetTintAmount(0.03f);
    g_ThumbIdleMaterial.SetBlurRadius(0.0f);

    g_ThumbPressedMaterial = g_ThumbIdleMaterial;
    g_ThumbPressedMaterial.SetCornerRadius(11.5f);
    g_ThumbPressedMaterial.SetRefractionStrength(0.55f);
}

static void DrawTextCentered(
    HDC dc,
    const wchar_t* text,
    RECT rect,
    int height,
    int weight)
{
    HFONT font = CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    HGDIOBJ oldFont = SelectObject(dc, font);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(35, 40, 48));

    DrawTextW(
        dc,
        text,
        -1,
        &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, oldFont);
    DeleteObject(font);
}

static bool CreateBackground(uint32_t width, uint32_t height)
{
    if (!g_Device.device || width == 0 || height == 0)
        return false;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(width);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(height);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;

    HBITMAP bitmap = CreateDIBSection(
        nullptr,
        &bmi,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0);

    if (!bitmap || !bits)
        return false;

    HDC dc = CreateCompatibleDC(nullptr);

    if (!dc)
    {
        DeleteObject(bitmap);
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    RECT full{
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)
    };

    HBRUSH bg = CreateSolidBrush(RGB(238, 241, 245));
    FillRect(dc, &full, bg);
    DeleteObject(bg);

    HPEN grid = CreatePen(PS_SOLID, 1, RGB(200, 207, 216));
    HGDIOBJ oldPen = SelectObject(dc, grid);

    for (int x = 0; x < static_cast<int>(width); x += 40)
    {
        MoveToEx(dc, x, 0, nullptr);
        LineTo(dc, x, static_cast<int>(height));
    }

    for (int y = 0; y < static_cast<int>(height); y += 40)
    {
        MoveToEx(dc, 0, y, nullptr);
        LineTo(dc, static_cast<int>(width), y);
    }

    SelectObject(dc, oldPen);
    DeleteObject(grid);

    const int crossX = static_cast<int>(width * 0.70f);
    const int crossY = static_cast<int>(height * 0.30f);

    HPEN vertical = CreatePen(PS_SOLID, 4, RGB(210, 55, 65));
    oldPen = SelectObject(dc, vertical);
    MoveToEx(dc, crossX, 30, nullptr);
    LineTo(dc, crossX, static_cast<int>(height * 0.58f));
    SelectObject(dc, oldPen);
    DeleteObject(vertical);

    HPEN horizontal = CreatePen(PS_SOLID, 4, RGB(45, 95, 205));
    oldPen = SelectObject(dc, horizontal);
    MoveToEx(dc, static_cast<int>(width * 0.43f), crossY, nullptr);
    LineTo(dc, static_cast<int>(width * 0.94f), crossY);
    SelectObject(dc, oldPen);
    DeleteObject(horizontal);

    HBRUSH dark = CreateSolidBrush(RGB(30, 34, 42));

    RECT darkRect{
        static_cast<LONG>(width * 0.06f),
        static_cast<LONG>(height * 0.26f),
        static_cast<LONG>(width * 0.25f),
        static_cast<LONG>(height * 0.52f)
    };

    FillRect(dc, &darkRect, dark);
    DeleteObject(dark);

    HBRUSH light = CreateSolidBrush(RGB(250, 250, 250));

    RECT lightRect{
        static_cast<LONG>(width * 0.10f),
        static_cast<LONG>(height * 0.33f),
        static_cast<LONG>(width * 0.21f),
        static_cast<LONG>(height * 0.45f)
    };

    FillRect(dc, &lightRect, light);
    DeleteObject(light);

    RECT title{
        static_cast<LONG>(width * 0.24f),
        28,
        static_cast<LONG>(width * 0.76f),
        92
    };

    DrawTextCentered(
        dc,
        L"AURORAGLASS SDK CONSUMER",
        title,
        34,
        FW_SEMIBOLD);

    RECT lensLabel{
        static_cast<LONG>(g_LensBounds.x),
        static_cast<LONG>(g_LensBounds.y),
        static_cast<LONG>(g_LensBounds.Right()),
        static_cast<LONG>(g_LensBounds.Bottom())
    };

    DrawTextCentered(
        dc,
        L"REFRACTION  MAGNIFICATION",
        lensLabel,
        22,
        FW_NORMAL);

    RECT buttonLabel{
        static_cast<LONG>(g_Button.bounds.x),
        static_cast<LONG>(g_Button.bounds.y),
        static_cast<LONG>(g_Button.bounds.Right()),
        static_cast<LONG>(g_Button.bounds.Bottom())
    };

    DrawTextCentered(
        dc,
        L"BUTTON",
        buttonLabel,
        20,
        FW_SEMIBOLD);

    RECT toggleLabel{
        static_cast<LONG>(g_Toggle.bounds.x),
        static_cast<LONG>(g_Toggle.bounds.y),
        static_cast<LONG>(g_Toggle.bounds.Right()),
        static_cast<LONG>(g_Toggle.bounds.Bottom())
    };

    DrawTextCentered(
        dc,
        L"TOGGLE",
        toggleLabel,
        17,
        FW_SEMIBOLD);

    RECT sliderLabel{
        static_cast<LONG>(g_Slider.bounds.x),
        static_cast<LONG>(g_Slider.bounds.y - 30.0f),
        static_cast<LONG>(g_Slider.bounds.Right()),
        static_cast<LONG>(g_Slider.bounds.y - 4.0f)
    };

    DrawTextCentered(
        dc,
        L"SLIDER",
        sliderLabel,
        17,
        FW_NORMAL);

    const auto* bgra =
        static_cast<const uint8_t*>(bits);

    std::vector<uint8_t> rgba(
        static_cast<size_t>(width) *
        static_cast<size_t>(height) *
        4);

    const size_t pixelCount =
        static_cast<size_t>(width) *
        static_cast<size_t>(height);

    for (size_t i = 0; i < pixelCount; ++i)
    {
        rgba[i * 4 + 0] = bgra[i * 4 + 2];
        rgba[i * 4 + 1] = bgra[i * 4 + 1];
        rgba[i * 4 + 2] = bgra[i * 4 + 0];
        rgba[i * 4 + 3] = 255;
    }

    SelectObject(dc, oldBitmap);
    DeleteDC(dc);
    DeleteObject(bitmap);

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = rgba.data();
    init.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> texture;

    HRESULT hr = g_Device.device->CreateTexture2D(
        &td,
        &init,
        &texture);

    if (FAILED(hr))
        return false;

    ComPtr<ID3D11ShaderResourceView> srv;

    hr = g_Device.device->CreateShaderResourceView(
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

    HRESULT hr = g_Device.swapChain->GetBuffer(
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

static bool RenderFrame()
{
    if (!CopyBackgroundToBackbuffer())
    {
        std::printf(
            "[reuse] background copy FAILED\n");

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

    if (!DrawGlass(g_LensMaterial, g_LensBounds))
        return false;

    if (!DrawGlass(g_Button.Material(), g_Button.bounds))
        return false;

    if (!DrawGlass(g_Toggle.Material(), g_Toggle.bounds))
        return false;

    const ControlBounds& slider = g_Slider.bounds;

    const float centerY =
        slider.y + slider.height * 0.5f;

    constexpr float trackHeight = 6.0f;

    const float trackY =
        centerY - trackHeight * 0.5f;

    ControlBounds track{
        slider.x,
        trackY,
        slider.width,
        trackHeight
    };

    if (!DrawGlass(g_TrackMaterial, track))
        return false;

    const float fillWidth =
        std::max(
            trackHeight,
            slider.width *
                g_Slider.NormalizedValue());

    ControlBounds fill{
        slider.x,
        trackY,
        fillWidth,
        trackHeight
    };

    if (!DrawGlass(g_FillMaterial, fill))
        return false;

    const bool pressed =
        g_Slider.State() ==
        ControlInteractionState::Pressed;

    const float thumbSize =
        pressed ? 23.0f : 18.0f;

    const float thumbX =
        std::clamp(
            g_Slider.ThumbCenterX() -
                thumbSize * 0.5f,
            slider.x,
            slider.x +
                slider.width -
                thumbSize);

    ControlBounds thumb{
        thumbX,
        centerY - thumbSize * 0.5f,
        thumbSize,
        thumbSize
    };

    if (!DrawGlass(
        pressed
            ? g_ThumbPressedMaterial
            : g_ThumbIdleMaterial,
        thumb))
    {
        return false;
    }

    HRESULT presentHr = g_Device.Present();

    if (FAILED(presentHr))
    {
        std::printf(
            "[reuse] Present FAILED hr=0x%08X\n",
            static_cast<unsigned>(presentHr));

        return false;
    }

    return true;
}

static ControlPoint MousePoint(LPARAM lParam)
{
    return ControlPoint{
        static_cast<float>(
            static_cast<short>(
                LOWORD(lParam))),
        static_cast<float>(
            static_cast<short>(
                HIWORD(lParam)))
    };
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
        if (!g_TrackingMouse)
        {
            TRACKMOUSEEVENT tme{
                sizeof(tme),
                TME_LEAVE,
                hwnd,
                0
            };

            TrackMouseEvent(&tme);
            g_TrackingMouse = true;
        }

        const ControlPoint p =
            MousePoint(lParam);

        g_Button.PointerMove(p);
        g_Toggle.PointerMove(p);
        g_Slider.PointerMove(p);

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
        SetCapture(hwnd);

        const ControlPoint p =
            MousePoint(lParam);

        g_Button.PointerDown(p);
        g_Toggle.PointerDown(p);
        g_Slider.PointerDown(p);

        return 0;
    }

    case WM_LBUTTONUP:
    {
        const ControlPoint p =
            MousePoint(lParam);

        g_Button.PointerUp(p);
        g_Toggle.PointerUp(p);
        g_Slider.PointerUp(p);

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

int main()
{
    std::printf(
        "AuroraGlass P3 Reuse Smoke starting...\n");

    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor =
        LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName =
        L"AuroraGlassP3ReuseSmokeClass";

    if (!RegisterClassW(&wc))
    {
        std::printf(
            "[reuse] RegisterClass FAILED\n");

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

    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"AuroraGlass P3 Reuse Smoke - Independent Win32 Consumer",
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

    if (!hwnd)
    {
        std::printf(
            "[reuse] CreateWindow FAILED\n");

        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (!g_Device.Init(hwnd))
    {
        std::printf(
            "[reuse] D3D11Device::Init FAILED\n");

        return 1;
    }

    Layout(
        g_Device.width,
        g_Device.height);

    if (!CreateBackground(
        g_Device.width,
        g_Device.height))
    {
        std::printf(
            "[reuse] CreateBackground FAILED\n");

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

    ConfigureMaterials();

    if (!g_Slider.SetRange(
        0.0f,
        1.0f).ok())
    {
        std::printf(
            "[reuse] slider range FAILED\n");

        return 1;
    }

    g_Slider.SetValue(0.5f);

    g_Button.onClick = []()
    {
        std::printf(
            "[reuse] Button clicked\n");
    };

    g_Toggle.onChanged =
        [](bool checked)
    {
        std::printf(
            "[reuse] Toggle changed: %d\n",
            checked ? 1 : 0);
    };

    g_Slider.onValueChanged =
        [](float value)
    {
        std::printf(
            "[reuse] Slider value: %.3f\n",
            value);
    };

    std::printf(
        "[reuse] Ready %ux%u debugLayer=%s\n",
        g_Device.width,
        g_Device.height,
        g_Device.debugLayerActive
            ? "yes"
            : "no");

    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous{};
    LARGE_INTEGER current{};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);

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
            TranslateMessage(&msg);
            DispatchMessageW(&msg);

            if (msg.message == WM_QUIT)
                g_Running = false;
        }

        if (!g_Running)
            break;

        QueryPerformanceCounter(&current);

        double dt =
            static_cast<double>(
                current.QuadPart -
                previous.QuadPart) /
            static_cast<double>(
                frequency.QuadPart);

        previous = current;

        if (dt > 0.25)
            dt = 0.25;

        g_Time +=
            static_cast<float>(dt);

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
                    "[reuse] device resize FAILED hr=0x%08X\n",
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
                    "[reuse] background resize FAILED\n");

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

            std::printf(
                "[reuse] Resized: %ux%u\n",
                newWidth,
                newHeight);
        }

        if (!g_Minimized)
        {
            if (!RenderFrame())
                return 3;
        }
        else
        {
            Sleep(16);
        }
    }

    g_Surface.Reset();
    g_BackgroundSRV.Reset();
    g_BackgroundTexture.Reset();

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

    std::printf(
        "[reuse] Clean exit\n");

    return 0;
}