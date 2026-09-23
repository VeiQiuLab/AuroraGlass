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

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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
    uint32_t h)
{
    std::vector<uint32_t> pixels;
    testbench::GenerateMaterialCalibration(
        (int)w,
        (int)h,
        pixels);

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

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) g_Minimized = true;
        else { g_Minimized = false; g_PendingW = LOWORD(lParam); g_PendingH = HIWORD(lParam); }
        return 0;
    case WM_MOUSEMOVE: {
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
    case 14: break; // dedicated material calibration scene in the render loop
    default: break;
    }
}

int main(int argc, char** argv) {
    int autoFrames = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) autoFrames = std::atoi(argv[++i]);
        else if (a == "--visual") g_VisualMode = true;
        else if (a == "--out" && i + 1 < argc) g_VisualOutDir = argv[++i];
    }
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

    RECT wr{ 0, 0, 1280, 720 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AuroraGlass P3 Test Bench",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { std::printf("CreateWindow failed\n"); return 1; }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (!g_Device.Init(hwnd)) { std::printf("D3D11 device init FAILED\n"); return 1; }
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
    std::printf("Running. Drag the glass object over content. Esc quit.\n");

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    uint64_t tick = 0;

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

            if (g_VisualMode) {
                if (g_VisualIndex >= kPresetCount) { g_Running = false; break; }
                ApplyPreset(kPresets[g_VisualIndex].kind, W, H);
                g_Time = 0.0f;
            }

            const bool materialCalibration =
                g_VisualMode && kPresets[g_VisualIndex].kind == 14;

            ID3D11ShaderResourceView* frameBackground =
                g_BgSrv.Get();

            if (materialCalibration) {
                if (!MakeMaterialCalibrationTexture(
                        g_Device.device.Get(),
                        W,
                        H)) {
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

            if (materialCalibration) {
                // One deterministic frame, same-size references, left -> right:
                // Clear | Regular | Thick.
                //
                // Each reference prepares its own shared backdrop blur because
                // Frost is a real background-sampling dimension and the batch
                // contract intentionally fixes one blur radius per PrepareFrame.
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
