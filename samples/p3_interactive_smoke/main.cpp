// ============================================================
// AuroraGlass P3 - interactive controls sample (Slice D).
//
// Liquid Glass Test Bench:
//  - bright/neutral sample-only background (testbench.h, CPU-generated)
//  - draggable sample-only pure-optics lens
//  - GlassButton / GlassToggle / GlassSlider (semantic controls)
//  - sample-only non-glass appearance overlay (discoverability)
//  - deterministic visual capture harness (--visual)
//
// No Core changes, no DirectWrite, no text/layout framework. Sample only.
// ============================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincodec.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "core/d3d11_device.h"
#include "core/shader_library.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"
#include "controls/control_button.h"
#include "controls/control_toggle.h"
#include "controls/control_slider.h"
#include "testbench.h"
#include "png_writer.h"

using namespace AuroraGlass;
namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

static D3D11Device  g_Device;
static GlassSurface g_Surface;

static GlassButton g_Button;
static GlassToggle g_Toggle;
static GlassSlider g_Slider;

static bool     g_Running = true;
static bool     g_Minimized = false;
static uint32_t g_PendingW = 0, g_PendingH = 0;
static float    g_Time = 0.0f;
static float    g_MouseX = 0.0f, g_MouseY = 0.0f;
static bool     g_VisualMode = false;
static int      g_VisualIndex = 0;
static std::string g_VisualOutDir = "visual_tests";
static bool     g_RecordingMode = false;
static bool     g_RecordingStills = false;
static int      g_RecordingStillIndex = 0;

// Draggable sample-only pure-optics object (NOT a control, NOT in Core).
static ControlBounds g_DragObj{ 0, 0, 160, 90 };
static bool   g_DragActive = false;
static bool   g_MouseCaptured = false;
static float  g_DragOffX = 0.0f, g_DragOffY = 0.0f;

static bool DragObjectHit(ControlPoint p) { return HitTestRect(g_DragObj, p); }

struct CBData { float a[4]; float b[4]; };

// One blur radius for the whole frame (batch contract). Pure-lens baseline: 0.
static constexpr float kBlur = 0.0f;

static ComPtr<ID3D11Texture2D>          g_BgTex;
static ComPtr<ID3D11RenderTargetView>   g_BgRtv;
static ComPtr<ID3D11ShaderResourceView> g_BgSrv;

// Dedicated calibration-only background. Original g_Bg* remains untouched for
// deterministic visual cases 01-14.
static ComPtr<ID3D11Texture2D>          g_CalibrationBgTex;
static ComPtr<ID3D11ShaderResourceView> g_CalibrationBgSrv;
static ComPtr<ID3D11VertexShader>       g_FsVS;
static ComPtr<ID3D11PixelShader>        g_BlitPS;
static ComPtr<ID3D11PixelShader>        g_OverlayPS;
static ComPtr<ID3D11Buffer>             g_CB;
static ComPtr<ID3D11SamplerState>       g_Sampler;
static ComPtr<ID3D11BlendState>         g_AlphaBlend;
static ComPtr<ID3D11Texture2D>          g_DemoBeforeTex, g_DemoBgTex, g_DemoLabelTex;
static ComPtr<ID3D11ShaderResourceView> g_DemoBeforeSrv, g_DemoBgSrv, g_DemoLabelSrv;
static int g_DemoBgKey = -1;
static uint32_t g_DemoBgW = 0, g_DemoBgH = 0;
static std::string g_DemoLabelKey;

// Sample fail-fast for Core Status results (no logging framework).
static void FailFast(const char* op, const AuroraGlass::Status& s) {
    if (s.ok()) return;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[p3] %s FAILED: code=%s hr=0x%08X\n",
                  op, ErrorCodeToString(s.code), (unsigned)s.hr);
    std::printf("%s", buf);
    OutputDebugStringA(buf);
}

static std::wstring FindHostShaderDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();
    std::vector<fs::path> candidates = {
        exeDir / "shaders", exeDir / ".." / "shaders", exeDir / ".." / ".." / "shaders",
        fs::current_path() / "shaders", fs::current_path() / ".." / "shaders",
    };
    for (auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c / "blit.hlsl", ec)) return fs::absolute(c).wstring();
    }
    return L"";
}

static bool MakeBackgroundTexture(ID3D11Device* dev, uint32_t w, uint32_t h) {
    std::vector<uint32_t> pixels;
    testbench::Generate((int)w, (int)h, pixels);
    g_BgSrv.Reset(); g_BgRtv.Reset(); g_BgTex.Reset();
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = pixels.data(); sd.SysMemPitch = w * 4;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &g_BgTex))) return false;
    if (FAILED(dev->CreateShaderResourceView(g_BgTex.Get(), nullptr, &g_BgSrv))) return false;
    return true;
}

static bool MakeMaterialCalibrationTexture(
    ID3D11Device* dev,
    uint32_t w,
    uint32_t h,
    int polishKind = -1)
{
    std::vector<uint32_t> pixels;
    if (polishKind >= 0)
        testbench::GeneratePolishBackground((int)w, (int)h, polishKind, pixels);
    else
        testbench::GenerateMaterialCalibration((int)w, (int)h, pixels);

    g_CalibrationBgSrv.Reset();
    g_CalibrationBgTex.Reset();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = pixels.data();
    sd.SysMemPitch = w * 4;

    if (FAILED(dev->CreateTexture2D(
            &td,
            &sd,
            &g_CalibrationBgTex)))
        return false;

    if (FAILED(dev->CreateShaderResourceView(
            g_CalibrationBgTex.Get(),
            nullptr,
            &g_CalibrationBgSrv)))
        return false;

    return true;
}

static bool MakeDemoTexture(ID3D11Device* dev, uint32_t w, uint32_t h,
                            const std::vector<uint32_t>& pixels,
                            ComPtr<ID3D11Texture2D>& tex,
                            ComPtr<ID3D11ShaderResourceView>& srv) {
    if (pixels.size() != (size_t)w * h) return false;
    srv.Reset(); tex.Reset();
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = pixels.data(); sd.SysMemPitch = w * 4;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    return SUCCEEDED(dev->CreateShaderResourceView(tex.Get(), nullptr, &srv));
}

static bool LoadDemoBefore(ID3D11Device* dev) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path asset = fs::path(exePath).parent_path() / "recording_before_dark.png";
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninit = SUCCEEDED(init);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
              SUCCEEDED(factory->CreateDecoderFromFilename(asset.c_str(), nullptr,
                          GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)) &&
              SUCCEEDED(decoder->GetFrame(0, &frame)) &&
              SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
              SUCCEEDED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                          WICBitmapDitherTypeNone, nullptr, 0.0,
                          WICBitmapPaletteTypeCustom));
    if (ok) {
        UINT w = 0, h = 0;
        ok = SUCCEEDED(converter->GetSize(&w, &h)) && w == 1280 && h == 720;
        if (ok) {
            std::vector<uint32_t> pixels((size_t)w * h);
            ok = SUCCEEDED(converter->CopyPixels(nullptr, w * 4, w * h * 4,
                                                  (BYTE*)pixels.data())) &&
                 MakeDemoTexture(dev, w, h, pixels, g_DemoBeforeTex, g_DemoBeforeSrv);
        }
    }
    converter.Reset(); frame.Reset(); decoder.Reset(); factory.Reset();
    if (uninit) CoUninitialize();
    if (!ok) std::printf("[recording] before image unavailable: %ls\n", asset.c_str());
    return ok;
}

static bool SetDemoBackground(int key, uint32_t w, uint32_t h) {
    if (g_DemoBgSrv && key == g_DemoBgKey && w == g_DemoBgW && h == g_DemoBgH)
        return true;
    std::vector<uint32_t> pixels;
    if (key == 3) testbench::GenerateRecordingOpticsBackground((int)w, (int)h, pixels);
    else if (key == 4) testbench::GenerateRecordingControlsBackground((int)w, (int)h, pixels);
    else if (key == 5) testbench::GenerateRecordingGeometryBackground((int)w, (int)h, pixels);
    else testbench::GeneratePolishBackground((int)w, (int)h, key, pixels);
    if (!MakeDemoTexture(g_Device.device.Get(), w, h, pixels,
                         g_DemoBgTex, g_DemoBgSrv)) return false;
    g_DemoBgKey = key; g_DemoBgW = w; g_DemoBgH = h;
    return true;
}

static bool SetDemoLabel(const std::string& title, const std::string& detail,
                         uint32_t w, uint32_t h) {
    const std::string key = title + "|" + detail + "|" + std::to_string(w) + "x" + std::to_string(h);
    if (g_DemoLabelSrv && key == g_DemoLabelKey) return true;
    std::vector<uint32_t> pixels;
    testbench::GenerateRecordingLabel((int)w, (int)h, title, detail, pixels);
    if (!MakeDemoTexture(g_Device.device.Get(), w, h, pixels,
                         g_DemoLabelTex, g_DemoLabelSrv)) return false;
    g_DemoLabelKey = key;
    return true;
}

static void BlitToTarget(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv,
                         ID3D11ShaderResourceView* src, uint32_t w, uint32_t h) {
    D3D11_VIEWPORT vp{}; vp.Width = (float)w; vp.Height = (float)h; vp.MaxDepth = 1.0f;
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->RSSetViewports(1, &vp);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_FsVS.Get(), nullptr, 0);
    ctx->PSSetShader(g_BlitPS.Get(), nullptr, 0);
    CBData cb{}; cb.a[0] = 1.0f / (float)w; cb.a[1] = 1.0f / (float)h;
    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(g_CB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, &cb, sizeof(cb)); ctx->Unmap(g_CB.Get(), 0);
    }
    ID3D11Buffer* cbs[] = { g_CB.Get() };
    ctx->PSSetConstantBuffers(0, 1, cbs);
    ID3D11ShaderResourceView* srvs[] = { src };
    ctx->PSSetShaderResources(0, 1, srvs);
    ctx->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);
}

static bool DrawDemoLabel(const std::string& title, const std::string& detail,
                          uint32_t w, uint32_t h) {
    if (!SetDemoLabel(title, detail, w, h)) return false;
    float factors[4] = { 1, 1, 1, 1 };
    g_Device.context->OMSetBlendState(g_AlphaBlend.Get(), factors, 0xFFFFFFFF);
    BlitToTarget(g_Device.context.Get(), g_Device.rtv.Get(),
                 g_DemoLabelSrv.Get(), w, h);
    g_Device.context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    return true;
}

// Sample-only: copy backbuffer to staging and write a PNG.
static bool CaptureBackbuffer(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                              IDXGISwapChain* sc, uint32_t w, uint32_t h,
                              const std::wstring& path) {
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), &back))) return false;
    D3D11_TEXTURE2D_DESC td{}; back->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ; td.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &staging))) return false;
    ctx->CopyResource(staging.Get(), back.Get());
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(ctx->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m))) return false;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    const uint8_t* src = (const uint8_t*)m.pData;
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* row = src + (size_t)y * m.RowPitch;
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t b = row[x*4+0], g = row[x*4+1], r = row[x*4+2], a = row[x*4+3];
            uint8_t* d = &rgba[((size_t)y * w + x) * 4];
            d[0]=r; d[1]=g; d[2]=b; d[3]=a;
        }
    }
    ctx->Unmap(staging.Get(), 0);
    return sample::WritePng(path, rgba.data(), w, h);
}

static void LayoutControls(uint32_t W, uint32_t H) {
    const float w = (float)W, h = (float)H;
    g_Button.bounds = ControlBounds{ w * 0.10f, h * 0.72f, 200.0f, 56.0f };
    g_Toggle.bounds = ControlBounds{ w * 0.10f, h * 0.82f, 110.0f, 44.0f };
    g_Slider.bounds = ControlBounds{ w * 0.34f, h * 0.80f, w * 0.42f, 28.0f };
}

static void Soften(GlassMaterial& m) {
    m.SetSpecularStrength(0.0f);
    m.SetEdgeFresnel(0.0f);
    m.SetDispersionStrength(0.0f);
    m.SetTintAmount(0.0f);
    m.SetBrightness(1.0f);
    m.SetSaturation(1.0f);
    m.SetRefractionStrength(0.45f);
    m.SetBlurRadius(kBlur);
}

static constexpr float kRecordingSeconds = 59.0f;
struct RecordingStill { float time; const char* file; };
static const RecordingStill kRecordingStills[] = {
    { 0.5f, "01_before.png" }, { 4.5f, "02_after.png" },
    { 9.5f, "03_text.png" }, { 13.5f, "04_shapes.png" },
    { 16.0f, "05_fine_lines.png" }, { 19.0f, "06_grid.png" },
    { 21.5f, "07_contrast.png" },
    { 23.5f, "08_button_normal.png" }, { 25.5f, "09_button_hover.png" },
    { 27.5f, "10_button_pressed.png" }, { 29.5f, "11_button_disabled.png" },
    { 31.5f, "12_button_focused.png" },
    { 33.5f, "13_toggle_off.png" }, { 35.0f, "14_toggle_on.png" },
    { 36.8f, "15_toggle_hover.png" }, { 38.2f, "16_toggle_pressed.png" },
    { 39.3f, "17_slider_start.png" }, { 40.6f, "18_slider_drag.png" },
    { 43.0f, "19_geometry_large.png" }, { 49.0f, "20_geometry_small.png" },
    { 51.0f, "21_extreme_radius.png" },
    { 53.5f, "22_background_dark.png" },
    { 55.5f, "23_background_light.png" },
    { 57.5f, "24_background_contrast.png" },
};
static constexpr int kRecordingStillCount =
    (int)(sizeof(kRecordingStills) / sizeof(kRecordingStills[0]));

static GlassMaterial MakeRecordingGlass(float radius, float thickness) {
    GlassMaterial m;
    m.SetBlurRadius(0.0f);
    m.SetDispersionStrength(0.0f);
    m.SetTintAmount(0.0f);
    m.SetSaturation(1.0f);
    m.SetBrightness(1.0f);
    m.SetNoiseAmount(0.0f);
    m.SetOpacity(0.96f);
    m.SetRefractionStrength(0.50f);
    m.SetThickness(thickness);
    m.SetEdgeFresnel(0.15f);
    m.SetSpecularStrength(0.12f);
    m.SetCornerRadius(radius);
    return m;
}

static void DrawRecordingOutline(const ControlBounds& b, float radius,
                                 float strength, uint32_t W, uint32_t H) {
    struct OCB { float res[4]; float rc[4]; float pr[4]; float tint[4]; } cb{};
    cb.res[0] = (float)W; cb.res[1] = (float)H;
    cb.rc[0] = b.x + b.width * 0.5f; cb.rc[1] = b.y + b.height * 0.5f;
    cb.rc[2] = b.width * 0.5f; cb.rc[3] = b.height * 0.5f;
    cb.pr[0] = radius; cb.pr[1] = strength;
    cb.tint[0] = cb.tint[1] = cb.tint[2] = 0.55f;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g_Device.context->Map(g_CB.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                                     0, &mapped))) return;
    memcpy(mapped.pData, &cb, sizeof(cb));
    g_Device.context->Unmap(g_CB.Get(), 0);
    D3D11_VIEWPORT vp{}; vp.Width = (float)W; vp.Height = (float)H; vp.MaxDepth = 1.0f;
    g_Device.context->OMSetRenderTargets(1, g_Device.rtv.GetAddressOf(), nullptr);
    g_Device.context->RSSetViewports(1, &vp);
    g_Device.context->IASetInputLayout(nullptr);
    g_Device.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_Device.context->VSSetShader(g_FsVS.Get(), nullptr, 0);
    g_Device.context->PSSetShader(g_OverlayPS.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { g_CB.Get() };
    g_Device.context->PSSetConstantBuffers(0, 1, cbs);
    float factors[4] = { 1, 1, 1, 1 };
    g_Device.context->OMSetBlendState(g_AlphaBlend.Get(), factors, 0xFFFFFFFF);
    g_Device.context->Draw(3, 0);
    g_Device.context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
}

static float DemoEase(float x) {
    x = (std::max)(0.0f, (std::min)(1.0f, x));
    return x * x * (3.0f - 2.0f * x);
}

static GlassRect RecordingMotionRect(float t, uint32_t W, uint32_t H) {
    struct Waypoint { float time, x, y; };
    static const Waypoint points[] = {
        { 8.0f, 75.0f, 100.0f }, { 10.5f, 75.0f, 100.0f },
        { 13.0f, 145.0f, 250.0f }, { 14.0f, 145.0f, 250.0f },
        { 18.0f, 470.0f, 150.0f }, { 20.0f, 760.0f, 100.0f },
        { 21.0f, 760.0f, 100.0f }, { 23.0f, 760.0f, 300.0f },
    };
    float x = points[7].x, y = points[7].y;
    for (int i = 0; i < 7; ++i) {
        if (t <= points[i + 1].time) {
            float a = DemoEase((t - points[i].time) /
                               (points[i + 1].time - points[i].time));
            x = points[i].x + (points[i + 1].x - points[i].x) * a;
            y = points[i].y + (points[i + 1].y - points[i].y) * a;
            break;
        }
    }
    return { x * W / 1280.0f, y * H / 720.0f,
             280.0f * W / 1280.0f, 155.0f * H / 720.0f };
}

static bool RenderRecordingFrame(float t, uint32_t W, uint32_t H) {
    std::string title, detail;
    if (t < 4.0f) {
        BlitToTarget(g_Device.context.Get(), g_Device.rtv.Get(),
                     g_DemoBeforeSrv.Get(), W, H);
        return DrawDemoLabel("BEFORE", "BASELINE WHITE SPOT", W, H);
    }

    const int bgKey = (t < 8.0f || t >= 53.0f)
        ? (t >= 57.0f ? 2 : (t >= 55.0f ? 1 : 0))
        : (t < 23.0f ? 3 : (t < 41.0f ? 4 : 5));
    if (!SetDemoBackground(bgKey, W, H)) return false;
    BlitToTarget(g_Device.context.Get(), g_Device.rtv.Get(),
                 g_DemoBgSrv.Get(), W, H);

    FrameInfo fi{};
    fi.timeSeconds = 0.0f;
    const float blur = (t < 8.0f || t >= 53.0f) ? 12.0f : 0.0f;
    if (!g_Surface.PrepareFrame(g_Device.context.Get(), g_DemoBgSrv.Get(),
                                fi, blur).ok()) return false;
    auto rect = [&](const GlassRect& r, const GlassMaterial& m) {
        g_Surface.SetMaterial(m);
        return g_Surface.RenderRect(g_Device.context.Get(),
                                    g_Device.rtv.Get(), r).ok();
    };

    if (t < 8.0f || t >= 53.0f) {
        GlassMaterial m;
        if (!rect({ W * 0.25f, H * 0.28f, W * 0.50f, H * 0.44f }, m)) return false;
        if (t < 8.0f) { title = "AFTER"; detail = "CURRENT LIVE SHADER"; }
        else if (t < 55.0f) { title = "BACKGROUND DARK"; detail = "SAME GLASS SAME POSITION"; }
        else if (t < 57.0f) { title = "BACKGROUND LIGHT"; detail = "SAME GLASS SAME POSITION"; }
        else { title = "HIGH CONTRAST"; detail = "SAME GLASS SAME POSITION"; }
    } else if (t < 23.0f) {
        GlassMaterial m = MakeRecordingGlass(22.0f, 0.55f);
        GlassRect lens = RecordingMotionRect(t, W, H);
        if (!rect(lens, m)) return false;
        DrawRecordingOutline({ lens.x, lens.y, lens.width, lens.height },
                             22.0f, 0.16f, W, H);
        title = "DYNAMIC REFRACTION";
        detail = t < 11.0f ? "TEXT" : t < 14.0f ? "SHAPES" :
                 t < 18.0f ? "FINE LINES" : t < 21.0f ? "GRID" : "HIGH CONTRAST";
    } else if (t < 41.0f) {
        g_Button.bounds = { W * 0.11f, H * 0.43f, 260.0f, 68.0f };
        g_Toggle.bounds = { W * 0.44f, H * 0.44f, 130.0f, 56.0f };
        g_Slider.bounds = { W * 0.67f, H * 0.45f, 300.0f, 32.0f };
        const ControlPoint bp{ g_Button.bounds.x + 130.0f, g_Button.bounds.y + 34.0f };
        const ControlPoint tp{ g_Toggle.bounds.x + 65.0f, g_Toggle.bounds.y + 28.0f };
        g_Button.SetEnabled(true); g_Button.SetFocused(false); g_Button.PointerLeave();
        g_Toggle.SetEnabled(true); g_Toggle.SetFocused(false); g_Toggle.PointerLeave();
        g_Slider.PointerLeave();
        g_Toggle.checked = t >= 34.5f;

        if (t < 33.0f) {
            static const char* states[] = { "NORMAL", "HOVER", "PRESSED", "DISABLED", "FOCUSED" };
            int state = (std::min)(4, (int)((t - 23.0f) / 2.0f));
            if (state == 1) g_Button.PointerMove(bp);
            else if (state == 2) { g_Button.PointerMove(bp); g_Button.PointerDown(bp); }
            else if (state == 3) g_Button.SetEnabled(false);
            else if (state == 4) g_Button.SetFocused(true);
            title = std::string("BUTTON ") + states[state]; detail = "CONTROL STATE";
        } else if (t < 39.0f) {
            int state = (std::min)(3, (int)((t - 33.0f) / 1.5f));
            if (state == 2) g_Toggle.PointerMove(tp);
            else if (state == 3) { g_Toggle.PointerMove(tp); g_Toggle.PointerDown(tp); }
            static const char* states[] = { "OFF", "ON", "HOVER", "PRESSED" };
            title = std::string("TOGGLE ") + states[state]; detail = "CONTROL STATE";
        } else {
            float progress = DemoEase((t - 39.0f) / 2.0f);
            g_Slider.SetValue(0.20f);
            ControlPoint start{ g_Slider.ThumbCenterX(), g_Slider.bounds.y + 16.0f };
            g_Slider.PointerDown(start);
            ControlPoint moved{ g_Slider.bounds.x + (0.20f + progress * 0.60f) *
                                g_Slider.bounds.width, start.y };
            g_Slider.PointerMove(moved);
            title = "SLIDER DRAG"; detail = "TRACK FILL THUMB";
        }

        if (!rect({ g_Button.bounds.x, g_Button.bounds.y,
                    g_Button.bounds.width, g_Button.bounds.height },
                  g_Button.Material())) return false;
        const auto bs = g_Button.State();
        const float buttonOutline = bs == ControlInteractionState::Pressed ? 0.75f :
            bs == ControlInteractionState::Focused ? 0.78f :
            bs == ControlInteractionState::Hover ? 0.65f :
            bs == ControlInteractionState::Disabled ? 0.18f : 0.48f;
        DrawRecordingOutline(g_Button.bounds, 14.0f, buttonOutline, W, H);
        if (!rect({ g_Toggle.bounds.x, g_Toggle.bounds.y,
                    g_Toggle.bounds.width, g_Toggle.bounds.height },
                  g_Toggle.Material())) return false;
        GlassMaterial toggleKnob = MakeRecordingGlass(11.0f, 0.12f);
        float knobX = g_Toggle.bounds.x + (g_Toggle.checked ? 96.0f : 12.0f);
        if (!rect({ knobX, g_Toggle.bounds.y + 17.0f, 22.0f, 22.0f },
                  toggleKnob)) return false;
        DrawRecordingOutline(g_Toggle.bounds, 28.0f,
                             g_Toggle.State() == ControlInteractionState::Pressed ? 0.75f :
                             g_Toggle.State() == ControlInteractionState::Hover ? 0.65f : 0.50f,
                             W, H);
        DrawRecordingOutline({ knobX, g_Toggle.bounds.y + 17.0f, 22.0f, 22.0f },
                             11.0f, 0.70f, W, H);

        const float trackY = g_Slider.bounds.y + 13.0f;
        GlassMaterial track = MakeRecordingGlass(3.0f, 0.10f);
        if (!rect({ g_Slider.bounds.x, trackY, g_Slider.bounds.width, 6.0f }, track)) return false;
        GlassMaterial fill = track; fill.SetBrightness(0.88f);
        if (!rect({ g_Slider.bounds.x, trackY,
                    g_Slider.bounds.width * g_Slider.NormalizedValue(), 6.0f }, fill)) return false;
        GlassMaterial thumb = MakeRecordingGlass(11.0f, 0.10f);
        if (!rect({ g_Slider.ThumbCenterX() - 11.0f,
                    g_Slider.bounds.y + 5.0f, 22.0f, 22.0f }, thumb)) return false;
        DrawRecordingOutline({ g_Slider.ThumbCenterX() - 11.0f,
                               g_Slider.bounds.y + 5.0f, 22.0f, 22.0f },
                             11.0f, 0.70f, W, H);
    } else {
        auto shape = [&](const GlassRect& r, const GlassMaterial& material,
                         float radius) {
            if (!rect(r, material)) return false;
            DrawRecordingOutline({ r.x, r.y, r.width, r.height },
                                 radius, 0.55f, W, H);
            return true;
        };
        GlassMaterial m = MakeRecordingGlass(24.0f, 0.48f);
        if (t < 47.0f) {
            if (!shape({ 90.0f, 205.0f, 440.0f, 250.0f }, m, 24.0f)) return false;
            if (!shape({ 620.0f, 180.0f, 280.0f, 180.0f }, m, 24.0f)) return false;
            GlassMaterial input = m; input.SetCornerRadius(18.0f);
            if (!shape({ 620.0f, 475.0f, 500.0f, 64.0f }, input, 18.0f)) return false;
            GlassMaterial vertical = m; vertical.SetCornerRadius(28.0f);
            if (!shape({ 1000.0f, 170.0f, 140.0f, 260.0f }, vertical, 28.0f)) return false;
            title = "GEOMETRY LARGE"; detail = "PANEL CARD INPUT VERTICAL";
        } else {
            GlassMaterial compactMaterial = MakeRecordingGlass(12.0f, 0.18f);
            if (!shape({ 90.0f, 250.0f, 220.0f, 54.0f }, compactMaterial, 12.0f)) return false;
            if (!shape({ 360.0f, 260.0f, 120.0f, 44.0f }, compactMaterial, 12.0f)) return false;
            GlassMaterial slider = MakeRecordingGlass(6.0f, 0.08f);
            if (!shape({ 540.0f, 270.0f, 540.0f, 12.0f }, slider, 6.0f)) return false;
            GlassMaterial pill = MakeRecordingGlass(22.0f, 0.18f);
            if (!shape({ 90.0f, 400.0f, 180.0f, 44.0f }, pill, 22.0f)) return false;
            GlassMaterial knob = MakeRecordingGlass(11.0f, 0.08f);
            if (!shape({ 360.0f, 405.0f, 22.0f, 22.0f }, knob, 11.0f)) return false;
            GlassMaterial extreme = MakeRecordingGlass(200.0f, 0.18f);
            if (!shape({ 550.0f, 390.0f, 300.0f, 70.0f }, extreme, 200.0f)) return false;
            title = t < 50.0f ? "GEOMETRY SMALL" : "EXTREME RADIUS";
            detail = t < 50.0f ? "BUTTON TOGGLE SLIDER PILL 22PX" :
                                 "200PX CORNER INPUT VALUE";
        }
    }
    return DrawDemoLabel(title, detail, W, H);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) g_Minimized = true;
        else { g_Minimized = false; g_PendingW = LOWORD(lParam); g_PendingH = HIWORD(lParam); }
        return 0;
    case WM_MOUSEMOVE: {
        if (g_RecordingMode) return 0;
        g_MouseX = (float)(short)LOWORD(lParam); g_MouseY = (float)(short)HIWORD(lParam);
        if (g_DragActive) {
            g_DragObj.x = g_MouseX - g_DragOffX;
            g_DragObj.y = g_MouseY - g_DragOffY;
        } else if (!g_MouseCaptured) {
            ControlPoint p{ g_MouseX, g_MouseY };
            g_Button.PointerMove(p); g_Toggle.PointerMove(p); g_Slider.PointerMove(p);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (g_RecordingMode) return 0;
        g_MouseX = (float)(short)LOWORD(lParam); g_MouseY = (float)(short)HIWORD(lParam);
        ControlPoint p{ g_MouseX, g_MouseY };
        if (DragObjectHit(p)) {
            SetCapture(hwnd); g_MouseCaptured = true; g_DragActive = true;
            g_DragOffX = g_MouseX - g_DragObj.x; g_DragOffY = g_MouseY - g_DragObj.y;
        } else {
            g_Button.PointerDown(p); g_Toggle.PointerDown(p); g_Slider.PointerDown(p);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_RecordingMode) return 0;
        g_MouseX = (float)(short)LOWORD(lParam); g_MouseY = (float)(short)HIWORD(lParam);
        ControlPoint p{ g_MouseX, g_MouseY };
        if (g_DragActive) { g_DragActive = false; }
        else { g_Button.PointerUp(p); g_Toggle.PointerUp(p); g_Slider.PointerUp(p); }
        if (g_MouseCaptured) { ReleaseCapture(); g_MouseCaptured = false; }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) g_Running = false;
        return 0;
    case WM_DESTROY:
        g_Running = false; PostQuitMessage(0); return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---- Deterministic visual capture presets (sample-only) ----
struct VisualPreset { const char* file; int kind; };
static const VisualPreset kPresets[] = {
    { "01_button_normal.png",      0 },
    { "02_button_hover.png",       1 },
    { "03_button_pressed.png",     2 },
    { "04_toggle_off.png",         3 },
    { "05_toggle_on.png",          4 },
    { "06_slider_idle_25.png",     5 },
    { "07_slider_idle_50.png",     6 },
    { "08_slider_pressed_50.png",  7 },
    { "09_slider_idle_90.png",     8 },
    { "10_lens_text.png",          9 },
    { "11_lens_grid_cross.png",   10 },
    { "12_lens_shape_edge.png",   11 },
    { "13_lens_contrast_edge.png",12 },
    { "14_full_scene.png",        13 },
    { "15_material_calibration.png", 14 },
    { "16_default_dark.png",         15 },
    { "17_default_light.png",        16 },
    { "18_default_contrast.png",     17 },
    { "19_geometry_dark.png",        18 },
    { "20_geometry_light.png",       19 },
    { "21_geometry_contrast.png",    20 },
    { "22_states_dark.png",          21 },
    { "23_states_light.png",         22 },
    { "24_states_contrast.png",      23 },
};
static const int kPresetCount = (int)(sizeof(kPresets)/sizeof(kPresets[0]));

struct MaterialCalibrationPreset {
    const char* name;
    float refraction;
    float thickness;
    float frost;
    float dispersion;
    float edgeFresnel;
    float specular;
    float tint;
    float opacity;
};

static const MaterialCalibrationPreset kMaterialCalibration[] = {
    // Clear: small controls / rich background.
    { "Clear",   0.34f, 0.22f, 1.5f, 0.000f, 0.03f, 0.00f, 0.02f, 0.96f },

    // Regular: balanced general-purpose glass.
    { "Regular", 0.42f, 0.50f, 4.0f, 0.010f, 0.05f, 0.02f, 0.03f, 0.95f },

    // Thick: larger lens/panel; deeper profile, not merely stronger refraction.
    { "Thick",   0.48f, 0.85f, 7.0f, 0.020f, 0.07f, 0.03f, 0.04f, 0.94f },
};

static GlassMaterial MakeCalibrationMaterial(const MaterialCalibrationPreset& p) {
    GlassMaterial m;
    m.SetCornerRadius(24.0f);
    m.SetRefractionStrength(p.refraction);
    m.SetThickness(p.thickness);
    m.SetBlurRadius(p.frost);
    m.SetDispersionStrength(p.dispersion);
    m.SetEdgeFresnel(p.edgeFresnel);
    m.SetSpecularStrength(p.specular);
    m.SetTintAmount(p.tint);
    m.SetOpacity(p.opacity);
    m.SetBrightness(1.0f);
    m.SetSaturation(1.0f);
    m.SetNoiseAmount(0.0f);
    m.SetHighlightPosition(-0.35f, -0.30f);
    return m;
}

static void ApplyPreset(int kind, uint32_t W, uint32_t H) {
    const ControlPoint away{ -10000.0f, -10000.0f };
    g_Button.PointerLeave(); g_Toggle.PointerLeave(); g_Slider.PointerLeave();
    g_Button.SetFocused(false); g_Toggle.SetFocused(false);
    g_Toggle.checked = false;
    g_Slider.SetValue(0.5f);
    g_Button.PointerMove(away); g_Toggle.PointerMove(away); g_Slider.PointerMove(away);
    g_DragObj = ControlBounds{ (float)W * 0.72f, (float)H * 0.06f, 170.0f, 100.0f };
    g_DragActive = false; g_MouseCaptured = false;

    switch (kind) {
    case 0: break;
    case 1: g_Button.PointerMove({ g_Button.bounds.x + g_Button.bounds.width*0.5f,
                                   g_Button.bounds.y + g_Button.bounds.height*0.5f }); break;
    case 2: {
        ControlPoint c{ g_Button.bounds.x + g_Button.bounds.width*0.5f,
                        g_Button.bounds.y + g_Button.bounds.height*0.5f };
        g_Button.PointerMove(c); g_Button.PointerDown(c); break;
    }
    case 3: g_Toggle.checked = false; break;
    case 4: g_Toggle.checked = true; break;
    case 5: g_Slider.SetValue(0.25f); break;
    case 6: g_Slider.SetValue(0.50f); break;
    case 7: {
        g_Slider.SetValue(0.50f);
        ControlPoint c{ g_Slider.ThumbCenterX(), g_Slider.bounds.y + g_Slider.bounds.height*0.5f };
        g_Slider.PointerDown(c); break;
    }
    case 8: g_Slider.SetValue(0.90f); break;
    case 9:  g_DragObj = ControlBounds{ (float)W*0.10f, (float)H*0.10f, 200.0f, 110.0f }; break;
    case 10: g_DragObj = ControlBounds{ (float)W*0.62f, (float)H*0.10f, 200.0f, 110.0f }; break;
    case 11: g_DragObj = ControlBounds{ (float)W*0.10f, (float)H*0.34f, 200.0f, 110.0f }; break;
    case 12: g_DragObj = ControlBounds{ (float)W*0.62f, (float)H*0.34f, 200.0f, 110.0f }; break;
    case 13: g_Toggle.checked = true; g_Slider.SetValue(0.60f); break;
    case 14: case 15: case 16: case 17:
    case 18: case 19: case 20: case 21: case 22: case 23:
        break; // dedicated material scenes
    default: break;
    }
}

int main(int argc, char** argv) {
    int autoFrames = 0;
    bool outSpecified = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) autoFrames = std::atoi(argv[++i]);
        else if (a == "--visual") g_VisualMode = true;
        else if (a == "--recording-demo") g_RecordingMode = true;
        else if (a == "--recording-stills") {
            g_RecordingMode = true; g_RecordingStills = true;
        }
        else if (a == "--out" && i + 1 < argc) {
            g_VisualOutDir = argv[++i]; outSpecified = true;
        }
    }
    if (g_RecordingMode && g_VisualMode) {
        std::printf("Choose --visual or --recording-demo, not both.\n");
        return 1;
    }
    if (g_RecordingStills && !outSpecified) g_VisualOutDir = "recording_stills";
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("AuroraGlass P3 Test Bench starting...\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"AuroraGlassP3TestBenchClass";
    if (!RegisterClassExW(&wc)) { std::printf("RegisterClass failed\n"); return 1; }

    const DWORD windowStyle = g_RecordingMode
        ? (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
        : WS_OVERLAPPEDWINDOW;
    RECT wr{ 0, 0, 1280, 720 };
    AdjustWindowRect(&wr, windowStyle, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
        g_RecordingMode ? L"AuroraGlass Liquid Glass Recording Demo" :
                          L"AuroraGlass P3 Test Bench",
        windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { std::printf("CreateWindow failed\n"); return 1; }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (!g_Device.Init(hwnd)) { std::printf("D3D11 device init FAILED\n"); return 1; }
    if (g_RecordingMode && !LoadDemoBefore(g_Device.device.Get())) return 1;
    std::printf("D3D11 initialized (%ux%u), debugLayer=%s\n",
        g_Device.width, g_Device.height, g_Device.debugLayerActive ? "yes" : "no");

    std::wstring shaderDir = FindHostShaderDir();
    if (shaderDir.empty()) { std::printf("ERROR: sample shaders not found\n"); return 1; }

    ShaderLibrary shaders; shaders.Init(shaderDir);
    {
        auto vsBlob = shaders.Compile(L"fullscreen_triangle.hlsl", "FullscreenVS", "vs_5_0");
        auto blitBlob = shaders.Compile(L"blit.hlsl", "BlitPS", "ps_5_0");
        auto ovBlob = shaders.Compile(L"appearance_overlay.hlsl", "OverlayPS", "ps_5_0");
        if (!vsBlob || !blitBlob || !ovBlob) { std::printf("sample shader compile FAILED\n"); return 1; }
        g_Device.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, g_FsVS.GetAddressOf());
        g_Device.device->CreatePixelShader(blitBlob->GetBufferPointer(), blitBlob->GetBufferSize(), nullptr, g_BlitPS.GetAddressOf());
        g_Device.device->CreatePixelShader(ovBlob->GetBufferPointer(), ovBlob->GetBufferSize(), nullptr, g_OverlayPS.GetAddressOf());
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = sizeof(CBData);
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        g_Device.device->CreateBuffer(&cbd, nullptr, g_CB.GetAddressOf());
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        g_Device.device->CreateSamplerState(&sd, g_Sampler.GetAddressOf());
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        g_Device.device->CreateBlendState(&bd, g_AlphaBlend.GetAddressOf());
    }
    if (!MakeBackgroundTexture(g_Device.device.Get(), g_Device.width, g_Device.height)) {
        std::printf("background texture FAILED\n"); return 1;
    }

    SurfaceDesc desc; desc.width = g_Device.width; desc.height = g_Device.height;
    if (!GlassSurface::Create(g_Device.device.Get(), desc, g_Surface).ok()) {
        std::printf("GlassSurface::Create FAILED\n"); return 1;
    }

    g_Button.onClick = []() { std::printf("[p3] Button clicked\n"); };
    g_Toggle.onChanged = [](bool v) { std::printf("[p3] Toggle changed: %d\n", (int)v); };
    g_Slider.SetRange(0.0f, 1.0f);
    g_Slider.SetValue(0.5f);
    g_Slider.onValueChanged = [](float v) { std::printf("[p3] Slider value: %.3f\n", v); };
    if (g_RecordingMode) {
        g_Button.onClick = nullptr;
        g_Toggle.onChanged = nullptr;
        g_Slider.onValueChanged = nullptr;
    }
    for (auto* s : { &g_Button.style, &g_Toggle.style, &g_Toggle.checkedStyle }) {
        Soften(s->normal); Soften(s->hover); Soften(s->pressed);
        Soften(s->disabled); Soften(s->focused);
    }
    for (auto* s : { &g_Button.style, &g_Toggle.style }) {
        s->normal.SetTintAmount(0.06f);   s->normal.SetBrightness(0.985f);   s->normal.SetOpacity(0.96f);
        s->hover.SetTintAmount(0.09f);    s->hover.SetBrightness(0.99f);     s->hover.SetOpacity(0.97f);
        s->pressed.SetTintAmount(0.12f);  s->pressed.SetBrightness(0.95f);   s->pressed.SetOpacity(0.98f);
        s->focused.SetTintAmount(0.08f);  s->focused.SetBrightness(0.985f);  s->focused.SetOpacity(0.96f);
        s->disabled.SetTintAmount(0.03f); s->disabled.SetBrightness(0.99f);  s->disabled.SetOpacity(0.90f);
    }
    g_Toggle.checkedStyle = g_Toggle.style;
    g_Toggle.checkedStyle.normal.SetTintAmount(0.22f);
    g_Toggle.checkedStyle.normal.SetBrightness(0.96f);
    g_Toggle.checkedStyle.hover.SetTintAmount(0.26f);
    g_Toggle.useCheckedStyle = true;

    LayoutControls(g_Device.width, g_Device.height);
    g_DragObj = ControlBounds{ g_Device.width * 0.30f, g_Device.height * 0.42f, 190.0f, 110.0f };
    if (g_VisualMode) { std::error_code ec; fs::create_directories(g_VisualOutDir, ec); }
    if (g_RecordingStills) { std::error_code ec; fs::create_directories(g_VisualOutDir, ec); }
    std::printf(g_RecordingMode
        ? "Recording Demo: 59-second loop. Esc quits.\n"
        : "Running. Drag the glass object over content. Esc quit.\n");

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    const LARGE_INTEGER recordingStart = prev;
    uint64_t tick = 0;
    float previousRecordingTime = -1.0f;

    while (g_Running) {
        ++tick;
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_Running = false;
        }
        if (!g_Running) break;

        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        prev = now;
        if (dt > 0.25) dt = 0.25;
        g_Time += (float)dt;

        if (!g_Minimized && g_PendingW > 0 && g_PendingH > 0 &&
            (g_PendingW != g_Device.width || g_PendingH != g_Device.height)) {
            g_Device.Resize(g_PendingW, g_PendingH);
            MakeBackgroundTexture(g_Device.device.Get(), g_PendingW, g_PendingH);
            g_Surface.Resize(g_PendingW, g_PendingH);
            LayoutControls(g_PendingW, g_PendingH);
            g_PendingW = g_PendingH = 0;
        }

        if (autoFrames > 0 && tick >= (uint64_t)autoFrames) g_Running = false;

        if (!g_Minimized && g_Device.rtv) {
            const uint32_t W = g_Device.width, H = g_Device.height;

            if (g_RecordingMode) {
                const float demoTime = g_RecordingStills
                    ? kRecordingStills[g_RecordingStillIndex].time
                    : std::fmod((float)((now.QuadPart - recordingStart.QuadPart) /
                                         (double)freq.QuadPart), kRecordingSeconds);
                if (!g_RecordingStills && previousRecordingTime >= 0.0f &&
                    demoTime < previousRecordingTime)
                    std::printf("[recording] 59-second loop completed\n");
                previousRecordingTime = demoTime;
                if (!RenderRecordingFrame(demoTime, W, H)) {
                    std::printf("[recording] frame render FAILED at %.2fs\n", demoTime);
                    return 1;
                }
                if (g_RecordingStills) {
                    fs::path path = fs::path(g_VisualOutDir) /
                        kRecordingStills[g_RecordingStillIndex].file;
                    bool ok = CaptureBackbuffer(g_Device.device.Get(),
                        g_Device.context.Get(), g_Device.swapChain.Get(),
                        W, H, path.wstring());
                    std::printf("[recording] %s : %s\n",
                        ok ? "OK" : "FAIL",
                        kRecordingStills[g_RecordingStillIndex].file);
                    if (!ok) return 1;
                    ++g_RecordingStillIndex;
                    if (g_RecordingStillIndex >= kRecordingStillCount)
                        g_Running = false;
                } else {
                    g_Device.Present();
                }
                continue;
            }

            if (g_VisualMode) {
                if (g_VisualIndex >= kPresetCount) { g_Running = false; break; }
                ApplyPreset(kPresets[g_VisualIndex].kind, W, H);
                g_Time = 0.0f;
            }

            const int diagnosticKind = g_VisualMode ? kPresets[g_VisualIndex].kind : -1;
            const bool materialCalibration = diagnosticKind == 14;
            const bool polishDiagnostic = diagnosticKind >= 15;

            ID3D11ShaderResourceView* frameBackground =
                g_BgSrv.Get();

            if (materialCalibration || polishDiagnostic) {
                if (!MakeMaterialCalibrationTexture(
                        g_Device.device.Get(),
                        W,
                        H,
                        polishDiagnostic ? (diagnosticKind - 15) % 3 : -1)) {
                    std::printf(
                        "[visual] calibration background creation FAILED\n");
                    return 1;
                }

                frameBackground =
                    g_CalibrationBgSrv.Get();
            }

            BlitToTarget(
                g_Device.context.Get(),
                g_Device.rtv.Get(),
                frameBackground,
                W,
                H);

            if (materialCalibration || polishDiagnostic) {
                if (diagnosticKind >= 18) {
                    FrameInfo cfi;
                    cfi.timeSeconds = 0.0f;
                    auto draw = [&](GlassRect r, GlassMaterial m) {
                        FailFast("PrepareFrame(geometry/state)",
                                 g_Surface.PrepareFrame(g_Device.context.Get(), frameBackground,
                                                        cfi, m.GetBlurRadius()));
                        g_Surface.SetMaterial(m);
                        FailFast("RenderRect(geometry/state)",
                                 g_Surface.RenderRect(g_Device.context.Get(),
                                                      g_Device.rtv.Get(), r));
                    };
                    if (diagnosticKind >= 21) {
                        const GlassMaterial* buttonStates[] = {
                            &g_Button.style.normal, &g_Button.style.hover,
                            &g_Button.style.pressed, &g_Button.style.disabled,
                            &g_Button.style.focused
                        };
                        const GlassMaterial* toggleStates[] = {
                            &g_Toggle.style.normal, &g_Toggle.style.hover,
                            &g_Toggle.style.pressed, &g_Toggle.style.disabled,
                            &g_Toggle.style.focused
                        };
                        for (int i = 0; i < 5; ++i) {
                            float x = 25.0f + i * 250.0f;
                            draw({ x, 230.0f, 200.0f, 56.0f }, *buttonStates[i]);
                            draw({ x + 45.0f, 420.0f, 110.0f, 44.0f }, *toggleStates[i]);
                        }
                        for (int i = 0; i < 2; ++i) {
                            const float x = 325.0f + i * 450.0f;
                            GlassMaterial track;
                            track.SetCornerRadius(3.0f);
                            track.SetSpecularStrength(0.0f);
                            track.SetEdgeFresnel(0.0f);
                            track.SetRefractionStrength(0.30f);
                            track.SetBlurRadius(kBlur);
                            draw({ x, 600.0f, 260.0f, 6.0f }, track);
                            GlassMaterial thumb = track;
                            thumb.SetCornerRadius(i == 0 ? 9.0f : 11.5f);
                            thumb.SetRefractionStrength(i == 0 ? 0.42f : 0.55f);
                            const float size = i == 0 ? 18.0f : 23.0f;
                            draw({ x + 130.0f - size * 0.5f,
                                   603.0f - size * 0.5f, size, size }, thumb);
                        }
                    } else {
                        struct Case { float cx, cy, w, h, radius; };
                        const Case cases[] = {
                            { 215, 115, 350, 170, 32 }, // panel
                            { 640, 115, 250, 120, 28 }, // card
                            { 1065, 115, 200, 56, 28 }, // button
                            { 215, 345, 110, 44, 28 }, // toggle, over-radius input
                            { 640, 345, 260, 48, 12 }, // input region geometry
                            { 1065, 345, 260, 6, 3 }, // slider track
                            { 215, 565, 200, 44, 22 }, // pill
                            { 640, 565, 82, 160, 41 }, // tall card
                            { 1065, 565, 22, 22, 11 } // small thumb
                        };
                        for (const Case& c : cases) {
                            GlassMaterial m;
                            m.SetCornerRadius(c.radius);
                            draw({ c.cx - c.w * 0.5f, c.cy - c.h * 0.5f,
                                   c.w, c.h }, m);
                        }
                    }
                } else if (polishDiagnostic) {
                    GlassMaterial m;
                    FrameInfo cfi;
                    cfi.timeSeconds = 0.0f;
                    FailFast("PrepareFrame(default material)",
                             g_Surface.PrepareFrame(g_Device.context.Get(), frameBackground,
                                                    cfi, m.GetBlurRadius()));
                    g_Surface.SetMaterial(m);
                    GlassRect r{ W * 0.25f, H * 0.28f, W * 0.50f, H * 0.44f };
                    FailFast("RenderRect(default material)",
                             g_Surface.RenderRect(g_Device.context.Get(),
                                                  g_Device.rtv.Get(), r));
                } else {
                    // One deterministic frame, same-size references, left -> right:
                    // Clear | Regular | Thick.
                    // Each reference prepares its own backdrop blur.
                    const float glassW = 260.0f;
                    const float glassH = 150.0f;
                    const float y = (float)H * 0.30f;
                    const float xs[3] = {
                        (float)W * 0.07f,
                        (float)W * 0.395f,
                        (float)W * 0.72f
                    };

                    FrameInfo cfi;
                    cfi.timeSeconds = 0.0f;

                    for (int i = 0; i < 3; ++i) {
                        GlassMaterial m = MakeCalibrationMaterial(kMaterialCalibration[i]);

                        FailFast(
                            "PrepareFrame(calibration)",
                            g_Surface.PrepareFrame(
                                g_Device.context.Get(),
                                frameBackground,
                                cfi,
                                m.GetBlurRadius()));

                        g_Surface.SetMaterial(m);

                        GlassRect r{
                            xs[i],
                            y,
                            glassW,
                            glassH
                        };

                        FailFast(
                            "RenderRect(calibration)",
                            g_Surface.RenderRect(
                                g_Device.context.Get(),
                                g_Device.rtv.Get(),
                                r));
                    }

                    std::printf(
                        "[visual] material calibration: Clear | Regular | Thick\n");
                }

                const char* fn = kPresets[g_VisualIndex].file;
                std::wstring path =
                    std::wstring(g_VisualOutDir.begin(), g_VisualOutDir.end());
                path += L"\\";
                for (const char* p = fn; *p; ++p)
                    path += (wchar_t)(unsigned char)*p;

                bool ok = CaptureBackbuffer(
                    g_Device.device.Get(),
                    g_Device.context.Get(),
                    g_Device.swapChain.Get(),
                    W,
                    H,
                    path);

                std::printf(
                    "[visual] %s : %s\n",
                    ok ? "OK" : "FAIL",
                    fn);

                OutputDebugStringA(
                    ok
                        ? "[visual] calibration captured\n"
                        : "[visual] calibration capture FAILED\n");

                ++g_VisualIndex;
                if (g_VisualIndex >= kPresetCount)
                    g_Running = false;

                continue;
            }

            FrameInfo fi; fi.timeSeconds = g_Time;
            FailFast("PrepareFrame",
                     g_Surface.PrepareFrame(g_Device.context.Get(), frameBackground, fi, kBlur));

            auto rect = [&](float x, float y, float w, float h, const GlassMaterial& m) {
                g_Surface.SetMaterial(m);
                GlassRect r{ x, y, w, h };
                FailFast("RenderRect", g_Surface.RenderRect(g_Device.context.Get(), g_Device.rtv.Get(), r));
            };

            auto overlay = [&](const ControlBounds& b, float radius,
                               float outlineA, float lightA, float darkA) {
                struct OCB { float res[4]; float rc[4]; float pr[4]; float tint[4]; } cb{};
                cb.res[0] = (float)W; cb.res[1] = (float)H;
                cb.rc[0] = b.x + b.width * 0.5f; cb.rc[1] = b.y + b.height * 0.5f;
                cb.rc[2] = b.width * 0.5f;       cb.rc[3] = b.height * 0.5f;
                cb.pr[0] = radius; cb.pr[1] = outlineA; cb.pr[2] = lightA; cb.pr[3] = darkA;
                cb.tint[0] = 0.10f; cb.tint[1] = 0.12f; cb.tint[2] = 0.16f;
                D3D11_MAPPED_SUBRESOURCE m{};
                if (SUCCEEDED(g_Device.context->Map(g_CB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
                    memcpy(m.pData, &cb, sizeof(cb)); g_Device.context->Unmap(g_CB.Get(), 0);
                }
                D3D11_VIEWPORT vp{}; vp.Width = (float)W; vp.Height = (float)H; vp.MaxDepth = 1.0f;
                g_Device.context->OMSetRenderTargets(1, g_Device.rtv.GetAddressOf(), nullptr);
                g_Device.context->RSSetViewports(1, &vp);
                g_Device.context->IASetInputLayout(nullptr);
                g_Device.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                g_Device.context->VSSetShader(g_FsVS.Get(), nullptr, 0);
                g_Device.context->PSSetShader(g_OverlayPS.Get(), nullptr, 0);
                ID3D11Buffer* cbs[] = { g_CB.Get() };
                g_Device.context->PSSetConstantBuffers(0, 1, cbs);
                float bf[4] = { 1,1,1,1 };
                g_Device.context->OMSetBlendState(g_AlphaBlend.Get(), bf, 0xFFFFFFFF);
                g_Device.context->Draw(3, 0);
                g_Device.context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
            };

            // Draggable pure-optics lens (no UI overlay except ultra-weak marker).
            {
                GlassMaterial om;
                om.SetCornerRadius(22.0f);
                om.SetSpecularStrength(0.0f);
                om.SetEdgeFresnel(0.0f);
                om.SetRefractionStrength(0.50f);
                om.SetDispersionStrength(0.0f);
                om.SetTintAmount(0.0f);
                om.SetBlurRadius(kBlur);
                om.SetBrightness(1.0f);
                rect(g_DragObj.x, g_DragObj.y, g_DragObj.width, g_DragObj.height, om);
                overlay(g_DragObj, 22.0f, 0.12f, 0.0f, 0.0f);
            }

            // Button / Toggle.
            rect(g_Button.bounds.x, g_Button.bounds.y, g_Button.bounds.width, g_Button.bounds.height, g_Button.Material());
            rect(g_Toggle.bounds.x, g_Toggle.bounds.y, g_Toggle.bounds.width, g_Toggle.bounds.height, g_Toggle.Material());
            {
                float oA = (g_Button.State() == ControlInteractionState::Normal) ? 0.28f : 0.42f;
                overlay(g_Button.bounds, 14.0f, oA, 0.0f, 0.06f);
                float tA = (g_Toggle.State() == ControlInteractionState::Normal) ? 0.28f : 0.42f;
                overlay(g_Toggle.bounds, 12.0f, tA, 0.0f, 0.06f);
            }

            // Slider: thin track + small thumb; pressed = slight scale only.
            {
                const ControlBounds& b = g_Slider.bounds;
                const float cy = b.y + b.height * 0.5f;
                const float trackH = 6.0f;
                const float trackY = cy - trackH * 0.5f;
                const bool active = (g_Slider.State() == ControlInteractionState::Pressed);

                GlassMaterial tm;
                tm.SetCornerRadius(trackH * 0.5f);
                tm.SetSpecularStrength(0.0f);
                tm.SetEdgeFresnel(0.0f);
                tm.SetRefractionStrength(0.30f);
                tm.SetDispersionStrength(0.0f);
                tm.SetTintAmount(0.0f);
                tm.SetBrightness(0.92f);
                tm.SetNoiseAmount(0.0f);
                tm.SetOpacity(1.0f);
                tm.SetBlurRadius(kBlur);
                rect(b.x, trackY, b.width, trackH, tm);

                const float fw = std::max(trackH, b.width * g_Slider.NormalizedValue());
                GlassMaterial fm;
                fm.SetCornerRadius(trackH * 0.5f);
                fm.SetSpecularStrength(0.0f);
                fm.SetEdgeFresnel(0.0f);
                fm.SetRefractionStrength(0.35f);
                fm.SetDispersionStrength(0.0f);
                fm.SetTintAmount(0.06f);
                fm.SetBlurRadius(kBlur);
                rect(b.x, trackY, fw, trackH, fm);

                const float thumbH = active ? 23.0f : 18.0f;
                const float thumbW = active ? 23.0f : 18.0f;
                const float thumbX = std::min(std::max(g_Slider.ThumbCenterX() - thumbW * 0.5f, b.x),
                                              b.x + b.width - thumbW);
                const float thumbY = cy - thumbH * 0.5f;

                GlassMaterial hm;
                hm.SetCornerRadius(thumbW * 0.5f);
                hm.SetSpecularStrength(0.0f);
                hm.SetEdgeFresnel(0.0f);
                hm.SetRefractionStrength(active ? 0.55f : 0.42f);
                hm.SetDispersionStrength(0.0f);
                hm.SetTintAmount(0.03f);
                hm.SetBlurRadius(kBlur);
                rect(thumbX, thumbY, thumbW, thumbH, hm);

                ControlBounds tb{ b.x, trackY, b.width, trackH };
                overlay(tb, trackH * 0.5f, 0.16f, 0.0f, 0.0f);
                ControlBounds hb{ thumbX, thumbY, thumbW, thumbH };
                overlay(hb, thumbW * 0.5f, active ? 0.36f : 0.26f, 0.0f, 0.05f);
            }

            if (g_VisualMode) {
                const char* fn = kPresets[g_VisualIndex].file;
                std::wstring path = std::wstring(g_VisualOutDir.begin(), g_VisualOutDir.end());
                path += L"\\";
                for (const char* p = fn; *p; ++p) path += (wchar_t)(unsigned char)*p;
                bool ok = CaptureBackbuffer(g_Device.device.Get(), g_Device.context.Get(),
                                            g_Device.swapChain.Get(), W, H, path);
                std::printf("[visual] %s : %s\n", ok ? "OK" : "FAIL", fn);
                OutputDebugStringA(ok ? "[visual] captured\n" : "[visual] capture FAILED\n");
                ++g_VisualIndex;
                if (g_VisualIndex >= kPresetCount) g_Running = false;
                continue;
            }

            g_Device.Present();
        } else {
            Sleep(16);
        }
    }

    std::printf("Shutting down...\n");
    g_Surface.Reset();
    g_DemoLabelSrv.Reset(); g_DemoLabelTex.Reset();
    g_DemoBgSrv.Reset(); g_DemoBgTex.Reset();
    g_DemoBeforeSrv.Reset(); g_DemoBeforeTex.Reset();
    g_CalibrationBgSrv.Reset();
    g_CalibrationBgTex.Reset();
    g_Device.context->ClearState();
    g_Device.context->Flush();
    g_Device.rtv.Reset();
    g_Device.swapChain.Reset();
    g_Device.context.Reset();
    g_Device.device.Reset();
    g_Device.ReportLiveObjects();
    g_Device.debugDevice.Reset();
    std::printf("Done.\n");
    return 0;
}
