// ============================================================
// AuroraGlass P2 — DPI / multi-monitor validation host (Slice 3).
//
// Windows-host responsibility only. AuroraGlass Core still receives ONLY
// physical pixel width/height; DPI / logical coords / window positioning are
// handled entirely at this host layer. No DPI value is ever written into
// GlassMaterial / GlassSurface / SurfaceDesc.
//
// Behavior:
//   - Enables DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 before HWND creation.
//     If the call is unavailable or fails, the actual error is logged; the
//     sample does NOT silently pretend awareness was set.
//   - Handles WM_DPICHANGED: reads the new DPI, applies the lParam-suggested
//     RECT via SetWindowPos, and lets the subsequent WM_SIZE drive the render
//     resize from the real client pixel size.
//   - Logs DPI + client pixel size on every DPI/size change so an operator can
//     manually validate per-monitor behavior by moving the window across
//     monitors with different scale factors.
//
// Does not modify frozen p0_proof / p1_smoke / p1_proof / p1_tests.
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

// ---- DPI helpers (host-side; never passed to Core) ----
typedef UINT (WINAPI* GetDpiForWindowFn)(HWND);
typedef BOOL (WINAPI* SetProcessDpiAwarenessContextFn)(HANDLE);

static GetDpiForWindowFn g_GetDpiForWindow = nullptr;
static UINT g_currentDpi = 96;

static UINT QueryWindowDpi(HWND hwnd) {
    if (g_GetDpiForWindow) return g_GetDpiForWindow(hwnd);
    HDC dc = GetDC(hwnd);
    UINT dpi = dc ? (UINT)GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return dpi;
}

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

static void LogDpiAndClient(HWND hwnd, const char* when) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    UINT dpi = QueryWindowDpi(hwnd);
    std::printf("[p2-dpi] %s: dpi=%u (scale=%.2f) client=%ldx%ld\n",
                when, dpi, (double)dpi / 96.0,
                (long)(rc.right - rc.left), (long)(rc.bottom - rc.top));
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) g_Minimized = true;
        else { g_Minimized = false; g_PendingW = LOWORD(lParam); g_PendingH = HIWORD(lParam); }
        return 0;

    case WM_DPICHANGED: {
        // wParam: LOWORD = new X DPI, HIWORD = new Y DPI.
        UINT newDpi = LOWORD(wParam);
        g_currentDpi = newDpi;
        // lParam: suggested window rect for the new DPI.
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        std::printf("[p2-dpi] WM_DPICHANGED: newDpi=%u (scale=%.2f) suggested=%ld,%ld %ldx%ld\n",
                    newDpi, (double)newDpi / 96.0,
                    (long)suggested->left, (long)suggested->top,
                    (long)(suggested->right - suggested->left),
                    (long)(suggested->bottom - suggested->top));
        // Apply the suggested rect. This triggers WM_SIZE, which sets the
        // pending client pixel size; the render loop applies it exactly once.
        SetWindowPos(hwnd, nullptr,
                     suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        LogDpiAndClient(hwnd, "after WM_DPICHANGED");
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

    // ---- DPI awareness (host layer), BEFORE creating any window ----
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
    {
        HMODULE u32 = GetModuleHandleW(L"user32.dll");
        if (u32) {
            g_GetDpiForWindow =
                (GetDpiForWindowFn)GetProcAddress(u32, "GetDpiForWindow");
            auto setCtx =
                (SetProcessDpiAwarenessContextFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
            if (setCtx) {
                if (setCtx((HANDLE)-4)) {
                    std::printf("[p2-dpi] DPI awareness = PER_MONITOR_AWARE_V2\n");
                } else {
                    DWORD err = GetLastError();
                    std::printf("[p2-dpi] SetProcessDpiAwarenessContext FAILED (err=%lu); "
                                "awareness NOT confirmed\n", (unsigned long)err);
                }
            } else {
                std::printf("[p2-dpi] SetProcessDpiAwarenessContext unavailable; "
                            "awareness NOT confirmed\n");
            }
        } else {
            std::printf("[p2-dpi] user32.dll not found; awareness NOT confirmed\n");
        }
    }

    std::printf("AuroraGlass P2 DPI starting...\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"AuroraGlassP2DpiClass";
    if (!RegisterClassExW(&wc)) { std::printf("RegisterClass failed\n"); return 1; }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AuroraGlass P2 DPI",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { std::printf("CreateWindow failed\n"); return 1; }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    g_currentDpi = QueryWindowDpi(hwnd);
    LogDpiAndClient(hwnd, "initial");

    if (!g_Device.Init(hwnd)) { std::printf("D3D11 device init FAILED\n"); return 1; }
    std::printf("D3D11 initialized (%ux%u), debugLayer=%s\n",
        g_Device.width, g_Device.height, g_Device.debugLayerActive ? "yes" : "no");

    std::wstring shaderDir = FindHostShaderDir();
    if (shaderDir.empty()) { std::printf("ERROR: shaders/ not found\n"); return 1; }

    ShaderLibrary shaders; shaders.Init(shaderDir);
    if (!g_Background.Init(g_Device.device.Get(), shaders)) {
        std::printf("BackgroundSource init FAILED\n"); return 1;
    }
    if (!g_Background.Resize(g_Device.device.Get(), g_Device.width, g_Device.height)) {
        std::printf("BackgroundSource resize FAILED\n"); return 1;
    }

    // Core receives ONLY physical pixels. DPI scale is NOT passed here.
    SurfaceDesc desc;
    desc.width = g_Device.width;
    desc.height = g_Device.height;
    Status cs = GlassSurface::Create(g_Device.device.Get(), desc, g_Surface);
    if (!cs.ok()) {
        std::printf("GlassSurface::Create FAILED: code=%s hr=0x%08X\n",
            ErrorCodeToString(cs.code), (unsigned)cs.hr);
        return 1;
    }
    std::printf("GlassSurface::Create OK (%ux%u) [physical pixels]\n",
        g_Surface.Width(), g_Surface.Height());

    g_Material.SetBlurRadius(12.0f);
    g_Material.SetRefractionStrength(0.6f);
    g_Surface.SetMaterial(g_Material);
    g_Stages = DiagnosticStages::AllEnabled();

    std::printf("Running. Drag across monitors with different scaling to test. Esc to quit.\n");

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

        // Apply pending client pixel size exactly once (no recursive resize).
        if (!g_Minimized && g_PendingW > 0 && g_PendingH > 0 &&
            (g_PendingW != g_Device.width || g_PendingH != g_Device.height)) {
            std::printf("[p2-dpi] apply client pixels -> %ux%u\n", g_PendingW, g_PendingH);
            HRESULT dr = g_Device.Resize(g_PendingW, g_PendingH);
            if (FAILED(dr)) {
                std::printf("[p2-dpi] device Resize failed hr=0x%08X deviceLost=%s\n",
                            (unsigned)dr, IsDeviceLostHResult(dr) ? "YES" : "NO");
            }
            g_Background.Resize(g_Device.device.Get(), g_PendingW, g_PendingH);
            Status sr = g_Surface.Resize(g_PendingW, g_PendingH);
            if (!sr.ok()) {
                std::printf("[p2-dpi] surface Resize: code=%s\n", ErrorCodeToString(sr.code));
            }
            g_PendingW = g_PendingH = 0;
            LogDpiAndClient(hwnd, "after apply");
        }

        if (autoFrames > 0 && tick >= (uint64_t)autoFrames) g_Running = false;

        if (!g_Minimized && g_Device.rtv) {
            g_Device.Clear(0.0f, 0.0f, 0.0f, 1.0f);
            g_Background.Render(g_Device.context.Get(), g_Time);

            FrameInfo frame;
            frame.timeSeconds = g_Time;
            frame.stages = g_Stages;
            Status rr = g_Surface.Render(g_Device.context.Get(), g_Device.rtv.Get(),
                                         g_Background.TextureSRV(), frame);
            if (!rr.ok()) {
                std::printf("[p2-dpi] Render FAILED: code=%s\n", ErrorCodeToString(rr.code));
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
