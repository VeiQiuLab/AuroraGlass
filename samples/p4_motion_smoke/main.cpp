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

static LightFollowMotion g_ButtonLight;
static LightFollowMotion g_ToggleLight;
static LightFollowMotion g_SliderLight;

// Sample-only direct-manipulation prototype for GlassToggle.
//
// P3 remains authoritative for the checked semantic value. The host owns
// pointer capture and drag interpretation. This Tween1D exists only so a
// released drag can settle from the exact pointer-driven presentation value.
static Tween1D g_ToggleDragVisual{0.0f};
static bool g_ToggleDragArmed = false;
static bool g_ToggleDragMoved = false;
static bool g_ToggleDragSettling = false;
static float g_ToggleDragStartX = 0.0f;
static float g_ToggleDragPointerOffset = 0.0f;

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

// Scripted evidence for the authoritative P4 reduced-motion capability.
static bool g_SawReducedMidFlightSnap = false;
static bool g_SawReducedImmediateRetarget = false;
static bool g_SawReducedDisableStable = false;
static bool g_SawReducedNormalResume = false;

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

static GlassMaterial WithHighlight(
    GlassMaterial material,
    const LightFollowPresentation& light)
{
    const Status status =
        material.SetHighlightPosition(
            light.x,
            light.y);

    RuntimeCheck(
        status.ok(),
        "SetHighlightPosition rejected P4 presentation value");

    return material;
}

static bool LightTracksState(
    ControlInteractionState state)
{
    return
        state == ControlInteractionState::Hover ||
        state == ControlInteractionState::Pressed;
}

static void RetargetLight(
    LightFollowMotion& motion,
    ControlInteractionState state,
    ControlPoint pointer,
    const ControlBounds& bounds)
{
    if (LightTracksState(state))
    {
        motion.RetargetPointer(
            pointer,
            bounds);
    }
    else
    {
        motion.RetargetRest();
    }
}

static void UpdateLightTargets(
    ControlPoint pointer)
{
    RetargetLight(
        g_ButtonLight,
        g_Button.State(),
        pointer,
        g_Button.bounds);

    RetargetLight(
        g_ToggleLight,
        g_Toggle.State(),
        pointer,
        g_Toggle.bounds);

    RetargetLight(
        g_SliderLight,
        g_Slider.State(),
        pointer,
        g_Slider.bounds);
}

static void RetargetAllLightsRest()
{
    g_ButtonLight.RetargetRest();
    g_ToggleLight.RetargetRest();
    g_SliderLight.RetargetRest();
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

static uint8_t Glyph5x7(char c, int row)
{
    if (row < 0 || row >= 7)
        return 0;

    switch (c)
    {
        case 'C':
        {
            static constexpr uint8_t p[7] =
                {14, 17, 16, 16, 16, 17, 14};
            return p[row];
        }

        case 'O':
        {
            static constexpr uint8_t p[7] =
                {14, 17, 17, 17, 17, 17, 14};
            return p[row];
        }

        case 'N':
        {
            static constexpr uint8_t p[7] =
                {17, 25, 21, 19, 17, 17, 17};
            return p[row];
        }

        case 'T':
        {
            static constexpr uint8_t p[7] =
                {31, 4, 4, 4, 4, 4, 4};
            return p[row];
        }

        case 'I':
        {
            static constexpr uint8_t p[7] =
                {14, 4, 4, 4, 4, 4, 14};
            return p[row];
        }

        case 'U':
        {
            static constexpr uint8_t p[7] =
                {17, 17, 17, 17, 17, 17, 14};
            return p[row];
        }

        case 'E':
        {
            static constexpr uint8_t p[7] =
                {31, 16, 16, 30, 16, 16, 31};
            return p[row];
        }

        case 'G':
        {
            static constexpr uint8_t p[7] =
                {14, 17, 16, 23, 17, 17, 14};
            return p[row];
        }

        case 'L':
        {
            static constexpr uint8_t p[7] =
                {16, 16, 16, 16, 16, 16, 31};
            return p[row];
        }

        case 'S':
        {
            static constexpr uint8_t p[7] =
                {15, 16, 16, 14, 1, 1, 30};
            return p[row];
        }

        case 'D':
        {
            static constexpr uint8_t p[7] =
                {30, 17, 17, 17, 17, 17, 30};
            return p[row];
        }

        case 'R':
        {
            static constexpr uint8_t p[7] =
                {30, 17, 17, 30, 20, 18, 17};
            return p[row];
        }

        default:
            return 0;
    }
}

static int BitmapTextWidth(
    const char* text,
    int scale)
{
    if (!text || scale <= 0)
        return 0;

    const int count =
        static_cast<int>(
            std::strlen(text));

    if (count <= 0)
        return 0;

    return
        (count * 6 - 1) *
        scale;
}

static void DrawBitmapText(
    std::vector<uint32_t>& pixels,
    uint32_t width,
    uint32_t height,
    int x,
    int y,
    const char* text,
    int scale,
    uint32_t color)
{
    if (!text || scale <= 0)
        return;

    int cursorX = x;

    for (const char* p = text; *p; ++p)
    {
        for (int row = 0; row < 7; ++row)
        {
            const uint8_t bits =
                Glyph5x7(*p, row);

            for (int col = 0; col < 5; ++col)
            {
                const uint8_t mask =
                    static_cast<uint8_t>(
                        1u << (4 - col));

                if ((bits & mask) == 0)
                    continue;

                FillRect(
                    pixels,
                    width,
                    height,
                    cursorX + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color);
            }
        }

        cursorX +=
            6 * scale;
    }
}

static void DrawCenteredBitmapText(
    std::vector<uint32_t>& pixels,
    uint32_t width,
    uint32_t height,
    const ControlBounds& bounds,
    const char* text,
    int scale,
    uint32_t color)
{
    const int textWidth =
        BitmapTextWidth(
            text,
            scale);

    const int textHeight =
        7 * scale;

    const int x =
        static_cast<int>(
            std::round(
                bounds.x +
                (bounds.width -
                 static_cast<float>(textWidth)) *
                    0.5f));

    const int y =
        static_cast<int>(
            std::round(
                bounds.y +
                (bounds.height -
                 static_cast<float>(textHeight)) *
                    0.5f));

    DrawBitmapText(
        pixels,
        width,
        height,
        x,
        y,
        text,
        scale,
        color);
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
    // Keep the existing GlassMaterial specular/highlight stage enabled.
    // P4 light-follow changes only highlight position; it does not introduce
    // a new shader or material model.
    // Readability diagnostic calibration pass 2:
    // the hotspot still read too strongly and made the interior feel
    // asymmetrically dented. Keep a restrained edge response, but reduce
    // interior specular dominance further.
    g_ControlMaterial.SetSpecularStrength(0.18f);
    g_ControlMaterial.SetEdgeFresnel(0.30f);
    g_ControlMaterial.SetDispersionStrength(0.0f);
    // Interior-shape diagnostic:
    // reduce lensing strength while preserving the accepted edge response.
    g_ControlMaterial.SetRefractionStrength(0.26f);
    // Screenshot diagnosis: the left/right optical lobes read too convex.
    // Reduce surface height/normal slope without changing accepted edge,
    // specular, or refraction-strength calibration.
    g_ControlMaterial.SetThickness(0.28f);
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
    g_TrackMaterial.SetSpecularStrength(0.0f);
    g_TrackMaterial.SetCornerRadius(3.0f);
    g_TrackMaterial.SetRefractionStrength(0.30f);
    g_TrackMaterial.SetTintAmount(0.02f);

    g_FillMaterial = g_TrackMaterial;
    g_FillMaterial.SetRefractionStrength(0.36f);
    g_FillMaterial.SetTintAmount(0.07f);

    g_ThumbMaterial = g_ControlMaterial;
    g_ThumbMaterial.SetRefractionStrength(0.28f);
    g_ThumbMaterial.SetThickness(0.30f);
    g_ThumbMaterial.SetTintAmount(0.04f);
    g_ThumbMaterial.SetSpecularStrength(0.16f);
    g_ThumbMaterial.SetEdgeFresnel(0.27f);
}

static bool CreateBackground(uint32_t width, uint32_t height)
{
    if (!g_Device.device || width == 0 || height == 0)
        return false;

    std::vector<uint32_t> pixels(
        static_cast<size_t>(width) *
        static_cast<size_t>(height));

    // Readability diagnostic scene only.
    //
    // Three deliberately different regions:
    //   left   = dark neutral content
    //   middle = saturated/colorful content
    //   right  = bright neutral content
    //
    // Fine structure, hard edges and thin lines make real refraction easy
    // to distinguish from a flat translucent rectangle.
    for (uint32_t y = 0; y < height; ++y)
    {
        const float fy =
            static_cast<float>(y) /
            static_cast<float>(
                std::max<uint32_t>(1, height - 1));

        for (uint32_t x = 0; x < width; ++x)
        {
            const float fx =
                static_cast<float>(x) /
                static_cast<float>(
                    std::max<uint32_t>(1, width - 1));

            int r = 0;
            int g = 0;
            int b = 0;

            if (fx < 0.34f)
            {
                const float t = fx / 0.34f;

                r = static_cast<int>(
                    22.0f + 30.0f * t +
                    8.0f * fy);

                g = static_cast<int>(
                    27.0f + 34.0f * t +
                    10.0f * fy);

                b = static_cast<int>(
                    36.0f + 42.0f * t +
                    14.0f * fy);
            }
            else if (fx < 0.68f)
            {
                const float t =
                    (fx - 0.34f) / 0.34f;

                r = static_cast<int>(
                    54.0f +
                    84.0f * t +
                    22.0f * fy);

                g = static_cast<int>(
                    72.0f +
                    36.0f * fy);

                b = static_cast<int>(
                    132.0f -
                    54.0f * t +
                    36.0f * fy);
            }
            else
            {
                const float t =
                    (fx - 0.68f) / 0.32f;

                r = static_cast<int>(
                    210.0f + 38.0f * t);

                g = static_cast<int>(
                    218.0f + 31.0f * t);

                b = static_cast<int>(
                    229.0f + 24.0f * t);
            }

            // Subtle horizontal banding creates small-scale sampling detail.
            if (((y / 28u) & 1u) != 0u)
            {
                r -= 7;
                g -= 7;
                b -= 7;
            }

            pixels[
                static_cast<size_t>(y) *
                    static_cast<size_t>(width) +
                static_cast<size_t>(x)] =
                    Pixel(
                        std::clamp(r, 0, 255),
                        std::clamp(g, 0, 255),
                        std::clamp(b, 0, 255));
        }
    }

    const uint32_t darkLine =
        Pixel(18, 22, 29);

    const uint32_t lightLine =
        Pixel(244, 247, 250);

    const uint32_t blue =
        Pixel(44, 126, 226);

    const uint32_t cyan =
        Pixel(36, 183, 205);

    const uint32_t magenta =
        Pixel(188, 72, 162);

    const uint32_t amber =
        Pixel(226, 156, 51);

    // Vertical high-frequency references.
    for (int x = 0;
         x < static_cast<int>(width);
         x += 42)
    {
        const bool bright =
            ((x / 42) & 1) != 0;

        FillRect(
            pixels,
            width,
            height,
            x,
            0,
            2,
            static_cast<int>(height),
            bright
                ? lightLine
                : darkLine);
    }

    // Horizontal references.
    for (int y = 0;
         y < static_cast<int>(height);
         y += 48)
    {
        FillRect(
            pixels,
            width,
            height,
            0,
            y,
            static_cast<int>(width),
            1,
            lightLine);
    }

    // Strong dark/light boundary passing directly underneath the button.
    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.16f),
        static_cast<int>(height * 0.14f),
        static_cast<int>(width * 0.075f),
        static_cast<int>(height * 0.22f),
        Pixel(246, 247, 249));

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.235f),
        static_cast<int>(height * 0.14f),
        static_cast<int>(width * 0.070f),
        static_cast<int>(height * 0.22f),
        Pixel(15, 18, 24));

    // Saturated content blocks for chromatic/refraction visibility.
    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.35f),
        static_cast<int>(height * 0.12f),
        static_cast<int>(width * 0.075f),
        static_cast<int>(height * 0.32f),
        blue);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.44f),
        static_cast<int>(height * 0.18f),
        static_cast<int>(width * 0.065f),
        static_cast<int>(height * 0.28f),
        cyan);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.53f),
        static_cast<int>(height * 0.10f),
        static_cast<int>(width * 0.060f),
        static_cast<int>(height * 0.36f),
        magenta);

    FillRect(
        pixels,
        width,
        height,
        static_cast<int>(width * 0.61f),
        static_cast<int>(height * 0.17f),
        static_cast<int>(width * 0.050f),
        static_cast<int>(height * 0.30f),
        amber);

    // Text-like content rows behind the slider region.
    for (int row = 0; row < 5; ++row)
    {
        const int y =
            static_cast<int>(
                height * 0.62f) +
            row * 22;

        const int x =
            static_cast<int>(
                width * 0.26f) +
            (row % 2) * 18;

        const int w =
            static_cast<int>(
                width *
                (0.38f -
                 static_cast<float>(row) * 0.035f));

        FillRect(
            pixels,
            width,
            height,
            x,
            y,
            w,
            4,
            row < 3
                ? Pixel(238, 241, 246)
                : Pixel(27, 31, 39));
    }

    // A compact checker field on the bright side.
    const int checkerX =
        static_cast<int>(
            width * 0.72f);

    const int checkerY =
        static_cast<int>(
            height * 0.18f);

    const int cell = 20;

    for (int cy = 0; cy < 8; ++cy)
    {
        for (int cx = 0; cx < 10; ++cx)
        {
            FillRect(
                pixels,
                width,
                height,
                checkerX + cx * cell,
                checkerY + cy * cell,
                cell,
                cell,
                ((cx + cy) & 1) != 0
                    ? Pixel(25, 29, 37)
                    : Pixel(239, 242, 247));
        }
    }

    // P4 stable diagnostic labels.
    //
    // These are baked ONCE into the background texture. They deliberately
    // avoid live HWND/GDI overlays fighting the D3D11 Present loop.
    const uint32_t diagnosticText =
        Pixel(246, 248, 252);

    const ControlBounds diagnosticButton{
        static_cast<float>(width) * 0.08f,
        static_cast<float>(height) * 0.22f,
        210.0f,
        62.0f
    };

    const ControlBounds diagnosticToggle{
        static_cast<float>(width) * 0.08f,
        static_cast<float>(height) * 0.43f,
        126.0f,
        48.0f
    };

    DrawCenteredBitmapText(
        pixels,
        width,
        height,
        diagnosticButton,
        "CONTINUE",
        3,
        diagnosticText);

    DrawCenteredBitmapText(
        pixels,
        width,
        height,
        diagnosticToggle,
        "TOGGLE",
        3,
        diagnosticText);

    DrawBitmapText(
        pixels,
        width,
        height,
        static_cast<int>(
            static_cast<float>(width) * 0.30f),
        static_cast<int>(
            static_cast<float>(height) * 0.70f) - 30,
        "SLIDER",
        3,
        diagnosticText);
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage =
        D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem =
        pixels.data();
    init.SysMemPitch =
        width * 4;

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

    g_BackgroundTexture =
        texture;

    g_BackgroundSRV =
        srv;

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

static void SetReducedMotionForPresentation(
    bool enabled) noexcept
{
    // Sample host policy wiring only. Production motion bindings own the
    // presentation response; P3 semantics remain completely unaware.
    g_ButtonMotion.SetReducedMotion(enabled);
    g_ToggleMotion.SetReducedMotion(enabled);
    g_SliderMotion.SetReducedMotion(enabled);

    g_ButtonLight.SetReducedMotion(enabled);
    g_ToggleLight.SetReducedMotion(enabled);
    g_SliderLight.SetReducedMotion(enabled);
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

    g_ButtonLight.Step(dt);
    g_ToggleLight.Step(dt);
    g_SliderLight.Step(dt);

    if (g_ToggleDragSettling)
    {
        g_ToggleDragVisual.Step(dt);

        const float target =
            g_Toggle.IsChecked()
                ? 1.0f
                : 0.0f;

        if (std::fabs(
                g_ToggleDragVisual.Value() -
                target) <= 0.001f)
        {
            g_ToggleDragVisual.Snap(
                target);

            g_ToggleDragSettling = false;
        }
    }

    RuntimeCheck(
        Near(g_Slider.Value(), sliderValueBefore, 0.000001f),
        "motion step changed semantic slider value");
}

static constexpr float kTogglePrototypeKnobSize = 32.0f;
static constexpr float kTogglePrototypeKnobInset = 7.0f;
static constexpr float kTogglePrototypeDragThresholdPx = 5.0f;
static constexpr float kTogglePrototypeSettleSeconds = 0.110f;

static float TogglePrototypeTravel() noexcept
{
    return std::max(
        1.0f,
        g_Toggle.bounds.width -
            kTogglePrototypeKnobSize -
            kTogglePrototypeKnobInset * 2.0f);
}

static float TogglePrototypeBaseCenterX() noexcept
{
    return
        g_Toggle.bounds.x +
        kTogglePrototypeKnobInset +
        kTogglePrototypeKnobSize * 0.5f;
}

static float ToggleVisualProgress() noexcept
{
    if (g_ToggleDragArmed ||
        g_ToggleDragSettling)
    {
        return std::clamp(
            g_ToggleDragVisual.Value(),
            0.0f,
            1.0f);
    }

    return std::clamp(
        g_ToggleMotion.Presentation().progress,
        0.0f,
        1.0f);
}

static void BeginToggleDrag(
    ControlPoint point) noexcept
{
    if (!g_Toggle.IsEnabled())
        return;

    if (!HitTestRect(
            g_Toggle.bounds,
            point))
    {
        return;
    }

    const float progress =
        std::clamp(
            g_ToggleMotion.Presentation().progress,
            0.0f,
            1.0f);

    g_ToggleDragVisual.Snap(
        progress);

    const float knobCenterX =
        TogglePrototypeBaseCenterX() +
        TogglePrototypeTravel() *
            progress;

    g_ToggleDragPointerOffset =
        point.x -
        knobCenterX;

    g_ToggleDragStartX =
        point.x;

    g_ToggleDragArmed = true;
    g_ToggleDragMoved = false;
    g_ToggleDragSettling = false;
}

static void UpdateToggleDrag(
    ControlPoint point) noexcept
{
    if (!g_ToggleDragArmed)
        return;

    if (!g_ToggleDragMoved)
    {
        const float distance =
            std::fabs(
                point.x -
                g_ToggleDragStartX);

        if (distance <
            kTogglePrototypeDragThresholdPx)
        {
            return;
        }

        g_ToggleDragMoved = true;
    }

    const float desiredCenterX =
        point.x -
        g_ToggleDragPointerOffset;

    const float progress =
        std::clamp(
            (desiredCenterX -
             TogglePrototypeBaseCenterX()) /
                TogglePrototypeTravel(),
            0.0f,
            1.0f);

    // During direct manipulation there is deliberately no animation latency.
    g_ToggleDragVisual.Snap(
        progress);

    // Crossing the midpoint previews the semantic result immediately.
    g_Toggle.SetChecked(
        progress >= 0.5f);
}

static bool EndToggleDrag(
    ControlPoint point) noexcept
{
    if (!g_ToggleDragArmed)
        return false;

    if (!g_ToggleDragMoved)
    {
        // It was a normal click. Let frozen GlassToggle::PointerUp preserve
        // the existing click-to-toggle behavior.
        g_ToggleDragArmed = false;
        return false;
    }

    UpdateToggleDrag(
        point);

    const bool finalChecked =
        g_ToggleDragVisual.Value() >=
        0.5f;

    g_Toggle.SetChecked(
        finalChecked);

    // Do NOT call GlassToggle::PointerUp for a completed drag because that
    // method intentionally toggles on activation. Clear only transient
    // interaction state, then restore hover from the final pointer position.
    g_Toggle.PointerLeave();
    g_Toggle.PointerMove(
        point);

    g_ToggleDragArmed = false;

    const float target =
        finalChecked
            ? 1.0f
            : 0.0f;

    g_ToggleDragVisual.Retarget(
        target,
        kTogglePrototypeSettleSeconds,
        TweenCurve::SmoothStep);

    g_ToggleDragSettling = true;

    std::printf(
        "[p4-motion] Toggle drag release progress=%.4f checked=%d\n",
        g_ToggleDragVisual.Value(),
        finalChecked ? 1 : 0);

    return true;
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

    const GlassMaterial buttonMaterial =
        WithHighlight(
            g_Button.Material(),
            g_ButtonLight.Presentation());

    if (!DrawGlass(
        buttonMaterial,
        buttonBounds))
    {
        return false;
    }

    const GlassMaterial toggleMaterial =
        WithHighlight(
            g_Toggle.Material(),
            g_ToggleLight.Presentation());

    if (!DrawGlass(
        toggleMaterial,
        g_Toggle.bounds))
    {
        return false;
    }

    const float toggleProgress =
        ToggleVisualProgress();

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
        WithHighlight(
            g_ThumbMaterial,
            g_SliderLight.Presentation());

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
    // Pointer position itself remains host-owned and is separately supplied
    // to the P4 presentation-only light-follow binding.
    g_Button.PointerMove(point);
    g_Toggle.PointerMove(point);
    g_Slider.PointerMove(point);
    UpdateLightTargets(point);
}

static void SendDown(ControlPoint point)
{
    g_Button.PointerDown(point);
    g_Toggle.PointerDown(point);
    g_Slider.PointerDown(point);
    UpdateLightTargets(point);
}

static void SendUp(ControlPoint point)
{
    g_Button.PointerUp(point);
    g_Toggle.PointerUp(point);
    g_Slider.PointerUp(point);
    UpdateLightTargets(point);
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
        "sliderState=%d thumb=%.3f value=%.4f "
        "buttonLight=(%.3f,%.3f) "
        "toggleLight=(%.3f,%.3f) "
        "sliderLight=(%.3f,%.3f)\n",
        name,
        g_ScriptFrame,
        static_cast<int>(g_Button.State()),
        g_ButtonMotion.Presentation().scale,
        g_Toggle.IsChecked() ? 1 : 0,
        g_ToggleMotion.Presentation().progress,
        static_cast<int>(g_Slider.State()),
        g_SliderMotion.Presentation().thumbSizePx,
        g_Slider.Value(),
        g_ButtonLight.Presentation().x,
        g_ButtonLight.Presentation().y,
        g_ToggleLight.Presentation().x,
        g_ToggleLight.Presentation().y,
        g_SliderLight.Presentation().x,
        g_SliderLight.Presentation().y);
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

    case 18:
    {
        ControlPoint left{
            g_Button.bounds.x +
                g_Button.bounds.width * 0.15f,
            button.y
        };

        SendMove(left);
        Evidence("light-button-left");
        break;
    }

    case 21:
    {
        ControlPoint right{
            g_Button.bounds.x +
                g_Button.bounds.width * 0.85f,
            button.y
        };

        SendMove(right);
        Evidence("light-button-right");
        break;
    }

    case 24:
    {
        ControlPoint reverse{
            g_Button.bounds.x +
                g_Button.bounds.width * 0.20f,
            button.y
        };

        SendMove(reverse);
        Evidence("light-button-rapid-reverse");
        break;
    }

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

    case 60:
        Evidence("light-rest-after-leave");
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

    // ------------------------------------------------------------
    // Reduced-motion runtime capability.
    //
    // Frames 124-125 first establish real in-flight presentation motion.
    // Frame 126 enables reduced motion while those animations are active.
    // ------------------------------------------------------------
    case 124:
    {
        ControlPoint sliderPoint{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        ControlPoint left{
            g_Button.bounds.x +
                g_Button.bounds.width * 0.15f,
            button.y
        };

        // Establish four independent normal-mode targets.
        g_Button.PointerMove(button);
        g_Toggle.SetChecked(false);
        g_Slider.PointerDown(sliderPoint);
        g_ButtonLight.RetargetPointer(
            left,
            g_Button.bounds);

        g_ButtonMotion.Sync(
            g_Button.State());

        g_ToggleMotion.Sync(
            g_Toggle.IsChecked());

        g_SliderMotion.Sync(
            g_Slider.State());

        Evidence("reduced-preflight-normal-start");
        break;
    }

    case 126:
    {
        // Prove these paths are actually in-flight before policy changes.
        RuntimeCheck(
            g_ButtonMotion.Presentation().scale > 1.0f &&
            g_ButtonMotion.Presentation().scale < 1.010f,
            "reduced preflight button was not mid-flight");

        RuntimeCheck(
            g_ToggleMotion.Presentation().progress > 0.0f &&
            g_ToggleMotion.Presentation().progress < 1.0f,
            "reduced preflight toggle was not mid-flight");

        RuntimeCheck(
            g_SliderMotion.Presentation().thumbSizePx > 19.5f &&
            g_SliderMotion.Presentation().thumbSizePx < 23.0f,
            "reduced preflight slider was not mid-flight");

        RuntimeCheck(
            g_ButtonLight.Presentation().x > -0.701f &&
            g_ButtonLight.Presentation().x < 0.499f,
            "reduced preflight light was not mid-flight");

        SetReducedMotionForPresentation(true);

        // SetReducedMotion(true) must settle in THIS CALL.
        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                1.010f,
                0.000001f),
            "reduced mid-flight button did not snap");

        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().response,
                0.35f,
                0.000001f),
            "reduced mid-flight button response did not snap");

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                0.0f,
                0.000001f),
            "reduced mid-flight toggle did not snap");

        RuntimeCheck(
            Near(
                g_SliderMotion.Presentation().thumbSizePx,
                23.0f,
                0.000001f),
            "reduced mid-flight slider did not snap");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().x,
                -0.70f,
                0.001f),
            "reduced mid-flight light x did not snap");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().y,
                0.0f,
                0.001f),
            "reduced mid-flight light y did not snap");

        // P3 semantic truth is unchanged by presentation policy.
        RuntimeCheck(
            !g_Toggle.IsChecked(),
            "reduced motion changed P3 toggle semantic truth");

        g_SawReducedMidFlightSnap = true;
        Evidence("reduced-mid-flight-enable");
        break;
    }

    case 128:
    {
        ControlPoint sliderPoint{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        ControlPoint right{
            g_Button.bounds.x +
                g_Button.bounds.width * 0.85f,
            button.y
        };

        // While reduced motion remains enabled, every new target must
        // settle synchronously with no temporal interpolation.
        g_Button.PointerDown(button);
        g_ButtonMotion.Sync(
            g_Button.State());

        g_Toggle.SetChecked(true);
        g_ToggleMotion.Sync(
            g_Toggle.IsChecked());

        g_Slider.PointerUp(sliderPoint);
        g_SliderMotion.Sync(
            g_Slider.State());

        g_ButtonLight.RetargetPointer(
            right,
            g_Button.bounds);

        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                0.970f,
                0.000001f),
            "reduced button new target was not immediate");

        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().response,
                1.0f,
                0.000001f),
            "reduced button response new target was not immediate");

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                1.0f,
                0.000001f),
            "reduced toggle new target was not immediate");

        RuntimeCheck(
            Near(
                g_SliderMotion.Presentation().thumbSizePx,
                19.5f,
                0.000001f),
            "reduced slider new target was not immediate");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().x,
                0.70f,
                0.001f),
            "reduced light new pointer target was not immediate");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().y,
                0.0f,
                0.001f),
            "reduced light new pointer y was not immediate");

        g_ButtonLight.RetargetRest();

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().x,
                0.50f,
                0.000001f),
            "reduced light leave/rest x was not immediate");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().y,
                0.35f,
                0.000001f),
            "reduced light leave/rest y was not immediate");

        // Leave it at another immediate target for the disable-stability check.
        g_ButtonLight.RetargetPointer(
            right,
            g_Button.bounds);

        g_SawReducedImmediateRetarget = true;
        Evidence("reduced-immediate-retarget");
        break;
    }

    case 130:
    {
        const float buttonBefore =
            g_ButtonMotion.Presentation().scale;

        const float toggleBefore =
            g_ToggleMotion.Presentation().progress;

        const float sliderBefore =
            g_SliderMotion.Presentation().thumbSizePx;

        const float lightBefore =
            g_ButtonLight.Presentation().x;

        SetReducedMotionForPresentation(false);

        // Disabling reduced motion itself must be presentation-neutral.
        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                buttonBefore,
                0.000001f),
            "disabling reduced motion moved button");

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                toggleBefore,
                0.000001f),
            "disabling reduced motion moved toggle");

        RuntimeCheck(
            Near(
                g_SliderMotion.Presentation().thumbSizePx,
                sliderBefore,
                0.000001f),
            "disabling reduced motion moved slider");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().x,
                lightBefore,
                0.000001f),
            "disabling reduced motion moved light");

        Evidence("reduced-disabled");
        break;
    }

    case 131:
        // One complete normal Step(dt) has elapsed since disabling.
        // No stale tween trajectory or spring velocity may resurrect.
        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                0.970f,
                0.000001f),
            "button resurrected stale motion after disable");

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                1.0f,
                0.000001f),
            "toggle resurrected stale motion after disable");

        RuntimeCheck(
            Near(
                g_SliderMotion.Presentation().thumbSizePx,
                19.5f,
                0.000001f),
            "slider resurrected stale velocity after disable");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().x,
                0.70f,
                0.001f),
            "light resurrected stale tween after disable");

        g_SawReducedDisableStable = true;
        Evidence("reduced-disable-stable");
        break;

    case 132:
    {
        ControlPoint sliderPoint{
            g_Slider.ThumbCenterX(),
            sliderY
        };

        const float buttonBefore =
            g_ButtonMotion.Presentation().scale;

        const float toggleBefore =
            g_ToggleMotion.Presentation().progress;

        const float sliderBefore =
            g_SliderMotion.Presentation().thumbSizePx;

        const float lightBefore =
            g_ButtonLight.Presentation().x;

        // New input after reduced motion is disabled must resume the existing
        // normal P4 animation paths, not snap.
        g_Button.PointerUp(button);
        g_ButtonMotion.Sync(
            g_Button.State());

        g_Toggle.SetChecked(false);
        g_ToggleMotion.Sync(
            g_Toggle.IsChecked());

        g_Slider.PointerDown(sliderPoint);
        g_SliderMotion.Sync(
            g_Slider.State());

        g_ButtonLight.RetargetRest();

        RuntimeCheck(
            Near(
                g_ButtonMotion.Presentation().scale,
                buttonBefore,
                0.000001f),
            "normal button retarget snapped after reduced disable");

        RuntimeCheck(
            Near(
                g_ToggleMotion.Presentation().progress,
                toggleBefore,
                0.000001f),
            "normal toggle retarget snapped after reduced disable");

        RuntimeCheck(
            Near(
                g_SliderMotion.Presentation().thumbSizePx,
                sliderBefore,
                0.000001f),
            "normal slider retarget snapped after reduced disable");

        RuntimeCheck(
            Near(
                g_ButtonLight.Presentation().x,
                lightBefore,
                0.000001f),
            "normal light retarget snapped after reduced disable");

        Evidence("reduced-normal-resume-input");
        break;
    }

    case 133:
        // Frame 132 AdvanceMotion has now stepped once in normal mode.
        RuntimeCheck(
            g_ButtonMotion.Presentation().scale > 0.970f &&
            g_ButtonMotion.Presentation().scale < 1.010f,
            "button normal animation did not resume");

        RuntimeCheck(
            g_ToggleMotion.Presentation().progress > 0.0f &&
            g_ToggleMotion.Presentation().progress < 1.0f,
            "toggle normal animation did not resume");

        RuntimeCheck(
            g_SliderMotion.Presentation().thumbSizePx > 19.5f &&
            g_SliderMotion.Presentation().thumbSizePx < 23.0f,
            "slider normal animation did not resume");

        RuntimeCheck(
            g_ButtonLight.Presentation().x > 0.50f &&
            g_ButtonLight.Presentation().x < 0.70f,
            "light normal animation did not resume");

        g_SawReducedNormalResume = true;
        Evidence("reduced-normal-resumed");
        break;
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

        UpdateToggleDrag(point);
        UpdateLightTargets(point);

        return 0;
    }

    case WM_MOUSELEAVE:
        g_TrackingMouse = false;
        g_Button.PointerLeave();

        if (!g_ToggleDragArmed)
        {
            g_Toggle.PointerLeave();
        }

        g_Slider.PointerLeave();
        RetargetAllLightsRest();
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

        BeginToggleDrag(point);
        UpdateLightTargets(point);

        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (g_Scripted)
            return 0;

        const ControlPoint point =
            MousePoint(lParam);

        g_Button.PointerUp(point);

        const bool toggleDragConsumed =
            EndToggleDrag(point);

        if (!toggleDragConsumed)
        {
            g_Toggle.PointerUp(point);
        }

        g_Slider.PointerUp(point);

        UpdateLightTargets(point);

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

        RuntimeCheck(
            g_SawReducedMidFlightSnap,
            "reduced-motion mid-flight snap path not executed");

        RuntimeCheck(
            g_SawReducedImmediateRetarget,
            "reduced-motion immediate retarget path not executed");

        RuntimeCheck(
            g_SawReducedDisableStable,
            "reduced-motion disable stability path not executed");

        RuntimeCheck(
            g_SawReducedNormalResume,
            "normal motion did not resume after reduced-motion disable");

        std::printf(
            "[p4-motion] reduced summary: "
            "midFlight=%d immediate=%d disableStable=%d resume=%d\n",
            g_SawReducedMidFlightSnap ? 1 : 0,
            g_SawReducedImmediateRetarget ? 1 : 0,
            g_SawReducedDisableStable ? 1 : 0,
            g_SawReducedNormalResume ? 1 : 0);

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