// AuroraGlass P1 — Independent Runtime Smoke Test (clean rewrite).
// Exercises the P1 public Core API without touching the frozen P0 sample.

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
static GlassMaterial    g_Material;
static DiagnosticStages g_Stages;

static bool     g_Running = true;
static bool     g_Minimized = false;
static uint32_t g_PendingW = 0, g_PendingH = 0;
static float    g_Time = 0.0f;
static double   g_FpsEMA = 0.0, g_FrameMsEMA = 0.0;

static std::wstring FindHostShaderDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();
    std::vector<fs::path> candidates = {
        exeDir / "shaders", exeDir / ".." / "shaders",
        exeDir / ".." / ".." / "shaders",
        fs::current_path() / "shaders",
        fs::current_path() / ".." / "shaders",
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
    std::printf("AuroraGlass P1 Smoke starting...\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"AuroraGlassP1SmokeClass";
    if (!RegisterClassExW(&wc)) { std::printf("RegisterClass failed\n"); return 1; }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AuroraGlass P1 Smoke",
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
    std::printf("Shader dir (host layer): %ls\n", shaderDir.c_str());

    // Background generator lives at SAMPLE layer.
    ShaderLibrary shaders; shaders.Init(shaderDir);
    if (!g_Background.Init(g_Device.device.Get(), shaders)) {
        std::printf("BackgroundSource init FAILED\n"); return 1;
    }
    if (!g_Background.Resize(g_Device.device.Get(), g_Device.width, g_Device.height)) {
        std::printf("BackgroundSource resize FAILED\n"); return 1;
    }

    // Create glass surface via P1 PUBLIC API.
    // Note: SurfaceDesc carries ONLY width/height. Shader discovery is internal.
    SurfaceDesc desc;
    desc.width = g_Device.width;
    desc.height = g_Device.height;
    Status cs = GlassSurface::Create(g_Device.device.Get(), desc, g_Surface);
    if (!cs.ok()) {
        std::printf("GlassSurface::Create FAILED: code=%s hr=0x%08X\n",
            ErrorCodeToString(cs.code), (unsigned)cs.hr);
        return 1;
    }
    std::printf("GlassSurface::Create OK (%ux%u)\n", g_Surface.Width(), g_Surface.Height());

    g_Material.SetBlurRadius(12.0f);
    g_Material.SetRefractionStrength(0.6f);
    g_Surface.SetMaterial(g_Material);
    g_Stages = DiagnosticStages::AllEnabled();

    std::printf("Running.\n");

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    uint64_t frameCount = 0;

    while (g_Running) {
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

        if (autoFrames > 0) {
            if (frameCount == 30) {
                g_Device.Resize(1024, 768);
                g_Background.Resize(g_Device.device.Get(), 1024, 768);
                Status rs = g_Surface.Resize(1024, 768);
                std::printf("[smoke] frame 30: Resize -> 1024x768: %s\n", rs.ok() ? "OK" : "FAILED");
            }
            if (frameCount == 50) {
                g_Material.SetBlurRadius(4.0f);
                g_Material.SetRefractionStrength(0.2f);
                g_Material.SetDispersionStrength(0.9f);
                g_Surface.SetMaterial(g_Material);
                std::printf("[smoke] frame 50: runtime material change\n");
            }
            if (frameCount == 70) {
                g_Stages.blur = false;
                g_Stages.refraction = false;
                std::printf("[smoke] frame 70: DiagnosticStages blur+refraction disabled\n");
            }
            if (frameCount == 85) {
                g_Stages = DiagnosticStages::AllEnabled();
                std::printf("[smoke] frame 85: DiagnosticStages re-enabled\n");
            }
            if (frameCount == 100) {
                g_Surface.Reset();
                g_Surface.Reset();
                bool uninit = !g_Surface.IsInitialized();
                std::printf("[smoke] frame 100: Reset()x2 -> uninitialized=%s\n", uninit ? "YES" : "NO");
                SurfaceDesc d2; d2.width = 1024; d2.height = 768;
                Status rcs = GlassSurface::Create(g_Device.device.Get(), d2, g_Surface);
                std::printf("[smoke] frame 100: re-Create after Reset: %s\n", rcs.ok() ? "OK" : "FAILED");
            }
            if (frameCount >= (uint64_t)autoFrames) g_Running = false;
        }

        if (!g_Minimized && g_Device.rtv) {
            g_Device.Clear(0.0f, 0.0f, 0.0f, 1.0f);
            g_Background.Render(g_Device.context.Get(), g_Time);

            FrameInfo frame;
            frame.timeSeconds = g_Time;
            frame.stages = g_Stages;
            Status rr = g_Surface.Render(g_Device.context.Get(), g_Device.rtv.Get(),
                                         g_Background.TextureSRV(), frame);
            if (!rr.ok()) {
                std::printf("[smoke] Render FAILED: code=%s\n", ErrorCodeToString(rr.code));
            }

            g_Device.Present();

            double ms = dt * 1000.0;
            double a = 0.1;
            g_FrameMsEMA += a * (ms - g_FrameMsEMA);
            g_FpsEMA = g_FrameMsEMA > 0.001 ? (1000.0 / g_FrameMsEMA) : 0.0;
            frameCount++;
        } else {
            Sleep(16);
        }
    }

    std::printf("Shutting down... (frames=%llu, fps=%.1f, ms=%.2f)\n",
        (unsigned long long)frameCount, g_FpsEMA, g_FrameMsEMA);

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
