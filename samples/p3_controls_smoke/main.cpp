// ============================================================
// AuroraGlass P3 — multi-control glass rendering sample (Slice C).
//
// Demonstrates the batch path: ONE GlassSurface, ONE PrepareFrame blur,
// several RenderRect glass shapes:
//   - a large GlassPanel
//   - a smaller GlassButton nested inside the panel
//   - additional non-overlapping rects
//
// Uses the P1/P3 public Core API only. Does not modify frozen baselines.
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
#include <filesystem>
#include <string>
#include <vector>

#include "core/d3d11_device.h"
#include "core/shader_library.h"
#include "core/background_source.h"
#include "core/glass_material.h"
#include "core/glass_surface.h"

using namespace AuroraGlass;
namespace fs = std::filesystem;

static D3D11Device      g_Device;
static BackgroundSource g_Background;
static GlassSurface     g_Surface;
static bool             g_Running = true;
static bool             g_Minimized = false;
static uint32_t         g_PendingW = 0, g_PendingH = 0;
static float            g_Time = 0.0f;
// Live mouse position (client-area physical pixels), P0/P1 highlight semantics.
static float            g_MouseX = 0.0f, g_MouseY = 0.0f;
static bool             g_HasMouse = false;

// Full-screen background blit (sample-layer only). Lays down the sharp
// background baseline the multi-rect batch composes over, using the SAME
// background SRV that PrepareFrame consumes. Not part of Core.
struct BlitCBData { float texel[4]; };
static Microsoft::WRL::ComPtr<ID3D11VertexShader> g_BlitVS;
static Microsoft::WRL::ComPtr<ID3D11PixelShader>  g_BlitPS;
static Microsoft::WRL::ComPtr<ID3D11Buffer>       g_BlitCB;
static Microsoft::WRL::ComPtr<ID3D11SamplerState> g_BlitSampler;

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
        if (fs::exists(c / "background.hlsl", ec)) return fs::absolute(c).wstring();
    }
    return L"";
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) g_Minimized = true;
        else { g_Minimized = false; g_PendingW = LOWORD(lParam); g_PendingH = HIWORD(lParam); }
        return 0;
    case WM_MOUSEMOVE: {
        // Same highlight semantics as P0/P1: store the client-area mouse position;
        // the per-control normalized highlight is computed against each glass
        // rect's own center/half-size in the render loop.
        g_MouseX = (float)(short)LOWORD(lParam);
        g_MouseY = (float)(short)HIWORD(lParam);
        g_HasMouse = true;
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

int main(int argc, char** argv) {
    int autoFrames = 0;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--frames" && i + 1 < argc)
            autoFrames = std::atoi(argv[++i]);
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("AuroraGlass P3 Controls starting...\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"AuroraGlassP3ControlsClass";
    if (!RegisterClassExW(&wc)) { std::printf("RegisterClass failed\n"); return 1; }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AuroraGlass P3 Controls",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { std::printf("CreateWindow failed\n"); return 1; }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (!g_Device.Init(hwnd)) { std::printf("D3D11 device init FAILED\n"); return 1; }
    std::printf("D3D11 initialized (%ux%u), debugLayer=%s\n",
        g_Device.width, g_Device.height, g_Device.debugLayerActive ? "yes" : "no");

    std::wstring shaderDir = FindHostShaderDir();
    if (shaderDir.empty()) { std::printf("ERROR: shaders/ not found\n"); return 1; }

    ShaderLibrary shaders; shaders.Init(shaderDir);
    if (!g_Background.Init(g_Device.device.Get(), shaders)) { std::printf("bg init FAILED\n"); return 1; }
    if (!g_Background.Resize(g_Device.device.Get(), g_Device.width, g_Device.height)) { std::printf("bg resize FAILED\n"); return 1; }

    // Minimal full-screen blit so the sample can lay down the sharp procedural
    // background baseline into the target BEFORE the glass batch.
    {
        auto vsBlob = shaders.Compile(L"fullscreen_triangle.hlsl", "FullscreenVS", "vs_5_0");
        auto psBlob = shaders.Compile(L"blit.hlsl", "BlitPS", "ps_5_0");
        if (!vsBlob || !psBlob) { std::printf("blit shader compile FAILED\n"); return 1; }
        if (FAILED(g_Device.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, g_BlitVS.GetAddressOf()))) { std::printf("blit VS FAILED\n"); return 1; }
        if (FAILED(g_Device.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, g_BlitPS.GetAddressOf()))) { std::printf("blit PS FAILED\n"); return 1; }
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = sizeof(BlitCBData);
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(g_Device.device->CreateBuffer(&cbd, nullptr, g_BlitCB.GetAddressOf()))) { std::printf("blit CB FAILED\n"); return 1; }
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(g_Device.device->CreateSamplerState(&sd, g_BlitSampler.GetAddressOf()))) { std::printf("blit sampler FAILED\n"); return 1; }
    }

    SurfaceDesc desc; desc.width = g_Device.width; desc.height = g_Device.height;
    if (!GlassSurface::Create(g_Device.device.Get(), desc, g_Surface).ok()) {
        std::printf("GlassSurface::Create FAILED\n"); return 1;
    }
    std::printf("Running. (multi-control: panel + nested button + extra rects)\n");

    // Phase 1 strict P0/P1 parity: every control uses the exact P0/P1 default
    // GlassMaterial (GlassMaterial{} == P1-verified defaults). No per-control
    // visual tuning in this phase. highlightPosition is driven per-control from
    // the live mouse position (see the render loop).
    GlassMaterial panelMat;
    GlassMaterial buttonMat;
    GlassMaterial cardMat;

    const float blurRadius = 12.0f;   // P0/P1 default

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    uint64_t frameCount = 0;
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
            g_Background.Resize(g_Device.device.Get(), g_PendingW, g_PendingH);
            g_Surface.Resize(g_PendingW, g_PendingH);
            g_PendingW = g_PendingH = 0;
        }

        if (autoFrames > 0 && tick >= (uint64_t)autoFrames) g_Running = false;

        if (!g_Minimized && g_Device.rtv) {
            const float W = (float)g_Device.width;
            const float H = (float)g_Device.height;

            // 1) Generate/update the procedural background into its offscreen texture.
            g_Background.Render(g_Device.context.Get(), g_Time);

            // 2) Lay down the SAME sharp background across the whole target: the
            //    baseline the multi-rect batch composes over. This is a full-screen
            //    blit of the exact SRV PrepareFrame will also consume, so the
            //    parity condition matches P0/P1 (background -> glass).
            {
                D3D11_VIEWPORT vp{}; vp.Width = W; vp.Height = H; vp.MaxDepth = 1.0f;
                g_Device.context->OMSetRenderTargets(1, g_Device.rtv.GetAddressOf(), nullptr);
                g_Device.context->RSSetViewports(1, &vp);
                g_Device.context->IASetInputLayout(nullptr);
                g_Device.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                g_Device.context->VSSetShader(g_BlitVS.Get(), nullptr, 0);
                g_Device.context->PSSetShader(g_BlitPS.Get(), nullptr, 0);
                BlitCBData cb{}; cb.texel[0] = 1.0f / W; cb.texel[1] = 1.0f / H;
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(g_Device.context->Map(g_BlitCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                    memcpy(mapped.pData, &cb, sizeof(cb));
                    g_Device.context->Unmap(g_BlitCB.Get(), 0);
                }
                ID3D11Buffer* cbs[] = { g_BlitCB.Get() };
                g_Device.context->PSSetConstantBuffers(0, 1, cbs);
                ID3D11ShaderResourceView* srvs[] = { g_Background.TextureSRV() };
                g_Device.context->PSSetShaderResources(0, 1, srvs);
                g_Device.context->PSSetSamplers(0, 1, g_BlitSampler.GetAddressOf());
                g_Device.context->Draw(3, 0);
                ID3D11ShaderResourceView* nullSRV[] = { nullptr };
                g_Device.context->PSSetShaderResources(0, 1, nullSRV);
            }

            // ONE PrepareFrame (one blur) for this frame.
            FrameInfo fi; fi.timeSeconds = g_Time;
            Status ps = g_Surface.PrepareFrame(g_Device.context.Get(),
                                               g_Background.TextureSRV(), fi, blurRadius);
            if (!ps.ok()) std::printf("[p3] PrepareFrame: %s\n", ErrorCodeToString(ps.code));

            // Highlight contract (same as P0/P1): highlight is normalized in the
            // glass rect's own space, +-1 == +-halfSize, and the shader computes
            // lightPx = rectCenter + highlight * halfSize. So per control:
            //   highlight = (mouse - rectCenter) / rectHalfSize, clamped to [-1,1].
            auto setHighlightFor = [&](GlassMaterial& m, const GlassRect& r) {
                if (!g_HasMouse) return;
                const float cx = r.x + r.width  * 0.5f;
                const float cy = r.y + r.height * 0.5f;
                const float hw = r.width  * 0.5f;
                const float hh = r.height * 0.5f;
                if (hw > 0.0f && hh > 0.0f)
                    m.SetHighlightPosition((g_MouseX - cx) / hw, (g_MouseY - cy) / hh);
            };

            // Panel (large), centered.
            GlassRect panel{ W * 0.20f, H * 0.20f, W * 0.60f, H * 0.55f };
            setHighlightFor(panelMat, panel);
            g_Surface.SetMaterial(panelMat);
            g_Surface.RenderRect(g_Device.context.Get(), g_Device.rtv.Get(), panel);

            // Button nested inside the panel (its rounded corners must NOT erase
            // the panel outside the button shape).
            GlassRect button{ panel.x + 40.0f, panel.y + 40.0f, 180.0f, 64.0f };
            setHighlightFor(buttonMat, button);
            g_Surface.SetMaterial(buttonMat);
            g_Surface.RenderRect(g_Device.context.Get(), g_Device.rtv.Get(), button);

            // Two non-overlapping cards.
            GlassRect card1{ W * 0.20f, H * 0.82f, W * 0.22f, H * 0.10f };
            GlassRect card2{ W * 0.58f, H * 0.82f, W * 0.22f, H * 0.10f };
            setHighlightFor(cardMat, card1);
            g_Surface.SetMaterial(cardMat);
            g_Surface.RenderRect(g_Device.context.Get(), g_Device.rtv.Get(), card1);
            setHighlightFor(cardMat, card2);
            g_Surface.SetMaterial(cardMat);
            g_Surface.RenderRect(g_Device.context.Get(), g_Device.rtv.Get(), card2);

            g_Device.Present();
            frameCount++;
        } else {
            Sleep(16);
        }
    }

    std::printf("Shutting down... (frames=%llu)\n", (unsigned long long)frameCount);
    g_Surface.Reset();
    g_Background.Release();
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
