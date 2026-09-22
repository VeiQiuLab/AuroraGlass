
// AuroraGlass P0 — Visual Core Proof sample host.
// Win32 window + D3D11 + procedural liquid-glass pipeline.
// Keyboard: 1..7 toggle stages, [ ] select param, Up/Down adjust,
//           R reset, Space pause, Esc quit. Mouse drives highlight.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/d3d11_device.h"
#include "core/glass_renderer.h"
#include "core/glass_params.h"

using namespace AuroraGlass;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Globals (single proof surface; acceptable for P0 sample)
// ---------------------------------------------------------------------------
static D3D11Device       g_Device;
static GlassRenderer     g_Renderer;
static GlassMaterialParams g_Params;
static StageFlags        g_Stages;

static bool   g_Running = true;
static bool   g_Paused = false;
static bool   g_Minimized = false;
static uint32_t g_PendingW = 0, g_PendingH = 0;
static float  g_AnimTime = 0.0f;

// Telemetry
static double g_FpsEMA = 0.0;
static double g_FrameMsEMA = 0.0;

// Selected parameter index for keyboard adjustment.
struct ParamEntry {
    const char* name;
    float* field;
    float step;
};
static std::vector<ParamEntry> g_ParamTable;
static int g_SelectedParam = 0;

static void BuildParamTable() {
    g_ParamTable = {
        { "blurRadius",         &g_Params.blurRadius,         1.0f   },
        { "refractionStrength", &g_Params.refractionStrength, 0.05f  },
        { "dispersionStrength", &g_Params.dispersionStrength, 0.05f  },
        { "thickness",          &g_Params.thickness,          0.05f  },
        { "edgeFresnel",        &g_Params.edgeFresnel,        0.05f  },
        { "specularStrength",   &g_Params.specularStrength,   0.1f   },
        { "tintAmount",         &g_Params.tintAmount,         0.05f  },
        { "saturation",         &g_Params.saturation,         0.05f  },
        { "brightness",         &g_Params.brightness,         0.05f  },
        { "noiseAmount",        &g_Params.noiseAmount,        0.005f },
        { "cornerRadius",       &g_Params.cornerRadius,       2.0f   },
        { "opacity",            &g_Params.opacity,            0.05f  },
    };
}

// ---------------------------------------------------------------------------
// Locate the shaders/ directory relative to the executable or CWD.
// ---------------------------------------------------------------------------
static std::wstring FindShaderDir() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();

    std::vector<fs::path> candidates = {
        exeDir / "shaders",
        exeDir / ".." / "shaders",
        exeDir / ".." / ".." / "shaders",
        fs::current_path() / "shaders",
        fs::current_path() / ".." / "shaders",
    };
    for (auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c / "background.hlsl", ec)) {
            return fs::absolute(c).wstring();
        }
    }
    return L"";
}

// ---------------------------------------------------------------------------
// Telemetry title
// ---------------------------------------------------------------------------
static void UpdateTitle(HWND hwnd) {
    auto b = [](bool v) { return v ? L'1' : L'0'; };
    wchar_t title[256];
    swprintf_s(title,
        L"AuroraGlass P0 | %5.1f FPS | %4.2f ms | stages B%c R%c D%c F%c S%c M%c C%c | param:%S",
        g_FpsEMA, g_FrameMsEMA,
        b(g_Stages.blur), b(g_Stages.refraction), b(g_Stages.dispersion),
        b(g_Stages.fresnel), b(g_Stages.specular), b(g_Stages.mask), b(g_Stages.colorAdjust),
        g_ParamTable.empty() ? "-" : g_ParamTable[g_SelectedParam].name);
    SetWindowTextW(hwnd, title);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            g_Minimized = true;
        } else {
            g_Minimized = false;
            g_PendingW = LOWORD(lParam);
            g_PendingH = HIWORD(lParam);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        // Map cursor to surface-local UV in [-1,1]. Surface spans +/-30% of window.
        int mx = (short)LOWORD(lParam);
        int my = (short)HIWORD(lParam);
        float cx = g_Device.width * 0.5f;
        float cy = g_Device.height * 0.5f;
        float hx = g_Device.width * 0.30f;
        float hy = g_Device.height * 0.30f;
        if (hx > 0 && hy > 0) {
            g_Params.highlightPos[0] = (mx - cx) / hx;
            g_Params.highlightPos[1] = (my - cy) / hy;
            g_Params.Clamp();
        }
        return 0;
    }

    case WM_KEYDOWN: {
        switch (wParam) {
        case VK_ESCAPE: g_Running = false; break;
        case VK_SPACE:  g_Paused = !g_Paused; break;
        case 'R':       g_Params = GlassMaterialParams{}; g_Params.Clamp(); break;
        case '1': g_Stages.blur = !g_Stages.blur; break;
        case '2': g_Stages.refraction = !g_Stages.refraction; break;
        case '3': g_Stages.dispersion = !g_Stages.dispersion; break;
        case '4': g_Stages.fresnel = !g_Stages.fresnel; break;
        case '5': g_Stages.specular = !g_Stages.specular; break;
        case '6': g_Stages.mask = !g_Stages.mask; break;
        case '7': g_Stages.colorAdjust = !g_Stages.colorAdjust; break;
        case VK_LEFT:
        case 0xBB: // ']' use bracket keys instead; handle below
            break;
        case 0xDB: // '['
            if (!g_ParamTable.empty()) {
                g_SelectedParam = (g_SelectedParam - 1 + (int)g_ParamTable.size()) % (int)g_ParamTable.size();
            }
            break;
        case 0xDD: // ']'
            if (!g_ParamTable.empty()) {
                g_SelectedParam = (g_SelectedParam + 1) % (int)g_ParamTable.size();
            }
            break;
        case VK_UP:
            if (!g_ParamTable.empty()) {
                auto& p = g_ParamTable[g_SelectedParam];
                *p.field += p.step;
                g_Params.Clamp();
            }
            break;
        case VK_DOWN:
            if (!g_ParamTable.empty()) {
                auto& p = g_ParamTable[g_SelectedParam];
                *p.field -= p.step;
                g_Params.Clamp();
            }
            break;
        default: break;
        }
        return 0;
    }

    case WM_DESTROY:
        g_Running = false;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Parse --frames N for automated test mode
    int autoFrames = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--frames" && i + 1 < argc) {
            autoFrames = std::atoi(argv[++i]);
        }
    }

    // Basic per-monitor DPI awareness (full DPI validation is P2, not P0).
    typedef BOOL(WINAPI* SetCtxFn)(HANDLE);
    if (HMODULE u32 = GetModuleHandleW(L"user32.dll")) {
        if (auto fn = (SetCtxFn)GetProcAddress(u32, "SetProcessDpiAwarenessContext")) {
            fn((HANDLE)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        }
    }

    std::wprintf(L"AuroraGlass P0 starting...\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"AuroraGlassP0Class";
    if (!RegisterClassExW(&wc)) { std::wprintf(L"RegisterClass failed\n"); return 1; }

    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"AuroraGlass P0",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { std::wprintf(L"CreateWindow failed\n"); return 1; }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    if (!g_Device.Init(hwnd)) {
        std::wprintf(L"D3D11 device init FAILED\n");
        return 1;
    }
    std::wprintf(L"D3D11 initialized (%ux%u), debugLayer=%hs\n",
                 g_Device.width, g_Device.height,
                 g_Device.debugLayerActive ? "yes" : "no");

    std::wstring shaderDir = FindShaderDir();
    if (shaderDir.empty()) {
        std::wprintf(L"ERROR: shaders/ directory not found\n");
        return 1;
    }
    std::wprintf(L"Shader dir: %ls\n", shaderDir.c_str());

    if (!g_Renderer.Init(g_Device.device.Get(), shaderDir)) {
        std::wprintf(L"GlassRenderer init FAILED (shader compile?)\n");
        return 1;
    }
    if (!g_Renderer.Resize(g_Device.device.Get(), g_Device.width, g_Device.height)) {
        std::wprintf(L"GlassRenderer resize FAILED\n");
        return 1;
    }

    BuildParamTable();
    g_Params.Clamp();

    std::wprintf(L"Running. Keys: 1-7 stages, [ ] param, Up/Down adjust, R reset, Space pause, Esc quit.\n");

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    int titleCounter = 0;
    uint64_t frameCount = 0;

    while (g_Running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_Running = false;
        }
        if (!g_Running) break;

        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        prev = now;
        if (dt > 0.25) dt = 0.25; // clamp after stalls

        if (!g_Paused) g_AnimTime += (float)dt;

        // Handle deferred resize at frame boundary (avoids mid-frame RT recreation).
        if (!g_Minimized && (g_PendingW > 0 && g_PendingH > 0) &&
            (g_PendingW != g_Device.width || g_PendingH != g_Device.height)) {
            g_Device.Resize(g_PendingW, g_PendingH);
            g_Renderer.Resize(g_Device.device.Get(), g_PendingW, g_PendingH);
            g_PendingW = g_PendingH = 0;
        }

        if (!g_Minimized && g_Device.rtv) {
            g_Device.Clear(0.0f, 0.0f, 0.0f, 1.0f);

            // Automated test: trigger resize, stage toggles, param changes
            if (autoFrames > 0) {
                if (frameCount == 30) {
                    g_PendingW = 1024;
                    g_PendingH = 768;
                }
                if (frameCount == 60) {
                    g_Stages.blur = !g_Stages.blur;
                    g_Stages.refraction = !g_Stages.refraction;
                    g_Stages.dispersion = !g_Stages.dispersion;
                    g_Stages.fresnel = !g_Stages.fresnel;
                    g_Stages.specular = !g_Stages.specular;
                    g_Stages.mask = !g_Stages.mask;
                    g_Stages.colorAdjust = !g_Stages.colorAdjust;
                }
                if (frameCount == 90) {
                    g_Stages.blur = true;
                    g_Stages.refraction = true;
                    g_Stages.dispersion = true;
                    g_Stages.fresnel = true;
                    g_Stages.specular = true;
                    g_Stages.mask = true;
                    g_Stages.colorAdjust = true;
                    g_Params.blurRadius += 5.0f;
                    g_Params.refractionStrength += 0.1f;
                    g_Params.dispersionStrength += 0.1f;
                    g_Params.Clamp();
                }
                if (frameCount >= (uint64_t)autoFrames) {
                    g_Running = false;
                }
            }

            g_Renderer.Render(g_Device.context.Get(), g_Device.rtv.Get(),
                              g_Device.width, g_Device.height,
                              g_AnimTime, g_Params, g_Stages);
            g_Device.Present();

            double ms = dt * 1000.0;
            double a = 0.1;
            // 统一统计窗口：只计算帧时间的 EMA，然后推导出对应的 FPS
            g_FrameMsEMA = g_FrameMsEMA + a * (ms - g_FrameMsEMA);
            g_FpsEMA = g_FrameMsEMA > 0.001 ? (1000.0 / g_FrameMsEMA) : 0.0;

            frameCount++;
        } else {
            Sleep(16);
        }

        if (++titleCounter >= 15) { titleCounter = 0; UpdateTitle(hwnd); }
    }

    std::wprintf(L"Shutting down... (frames=%llu, fps=%.1f, ms=%.2f)\n",
                 (unsigned long long)frameCount, g_FpsEMA, g_FrameMsEMA);
    g_Renderer.Release();
    g_Device.context->ClearState();
    g_Device.context->Flush();
    
    // 必须在 ReportLiveObjects 之前显式释放设备级对象，
    // 否则全局变量尚未析构，Debug Layer 会误报泄漏。
    g_Device.rtv.Reset();
    g_Device.swapChain.Reset();
    g_Device.context.Reset();
    g_Device.device.Reset();
    
    g_Device.ReportLiveObjects();
    g_Device.debugDevice.Reset();
    g_Device.debugDevice.Reset();
    std::wprintf(L"Done.\n");
    return 0;
}
