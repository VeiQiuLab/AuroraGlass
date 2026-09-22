// ============================================================
// AuroraGlass P1 — Visual Parity Sample (uses the P1 public API).
//
// Sits alongside the FROZEN samples/p0_proof (golden baseline) and
// samples/p1_smoke (API/runtime smoke).
//
// Purpose: demonstrate that the P1 public Core API (GlassMaterial +
// GlassSurface + DiagnosticStages + Status) reproduces the exact
// visuals and interactive behavior of the P0 baseline.
//
// Keyboard: 1..7 toggle DiagnosticStages, [ ] select param, Up/Down adjust,
//           R reset, Space pause, Esc quit. Mouse drives highlight.
// --frames N  automated run with resize + stage toggle + param change.
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

// ---------------------------------------------------------------------------
static D3D11Device       g_Device;
static BackgroundSource  g_Background;
static GlassSurface      g_Surface;
static GlassMaterial     g_Material;
static DiagnosticStages  g_Stages;

static bool     g_Running = true;
static bool     g_Paused  = false;
static bool     g_Minimized = false;
static uint32_t g_PendingW = 0, g_PendingH = 0;
static float    g_Time = 0.0f;
static double   g_FpsEMA = 0.0, g_FrameMsEMA = 0.0;

// Parameter table: maps keyboard-adjustable params to GlassMaterial setters.
struct ParamEntry {
    const char* name;
    Status (GlassMaterial::*setter)(float) noexcept;
    float step;
};
static std::vector<ParamEntry> g_ParamTable;
static int g_SelectedParam = 0;

static void BuildParamTable() {
    g_ParamTable = {
        { "blurRadius",         &GlassMaterial::SetBlurRadius,         1.0f   },
        { "refractionStrength", &GlassMaterial::SetRefractionStrength, 0.05f  },
        { "dispersionStrength", &GlassMaterial::SetDispersionStrength, 0.05f  },
        { "thickness",          &GlassMaterial::SetThickness,          0.05f  },
        { "edgeFresnel",        &GlassMaterial::SetEdgeFresnel,        0.05f  },
        { "specularStrength",   &GlassMaterial::SetSpecularStrength,   0.1f   },
        { "tintAmount",         &GlassMaterial::SetTintAmount,         0.05f  },
        { "saturation",         &GlassMaterial::SetSaturation,         0.05f  },
        { "brightness",         &GlassMaterial::SetBrightness,         0.05f  },
        { "noiseAmount",        &GlassMaterial::SetNoiseAmount,        0.005f },
        { "cornerRadius",       &GlassMaterial::SetCornerRadius,       2.0f   },
        { "opacity",            &GlassMaterial::SetOpacity,            0.05f  },
    };
}

// Host-side shader dir (for BackgroundSource, which lives at sample layer).
static std::wstring FindHostShaderDir() {
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
        if (fs::exists(c / "background.hlsl", ec)) return fs::absolute(c).wstring();
    }
    return L"";
}

static void UpdateTitle(HWND hwnd) {
    auto b = [](bool v) { return v ? '1' : '0'; };
    wchar_t title[256];
    swprintf_s(title,
        L"AuroraGlass P1 | %5.1f FPS | %4.2f ms | stages B%c R%c D%c F%c S%c M%c C%c | param:%hs",
        g_FpsEMA, g_FrameMsEMA,
        b(g_Stages.blur), b(g_Stages.refraction), b(g_Stages.dispersion),
        b(g_Stages.fresnel), b(g_Stages.specular), b(g_Stages.mask), b(g_Stages.colorAdjust),
        g_ParamTable.empty() ? "-" : g_ParamTable[g_SelectedParam].name);
    SetWindowTextW(hwnd, title);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) g_Minimized = true;
        else { g_Minimized = false; g_PendingW = LOWORD(lParam); g_PendingH = HIWORD(lParam); }
        return 0;
    case WM_MOUSEMOVE: {
        int mx = (short)LOWORD(lParam);
        int my = (short)HIWORD(lParam);
        float cx = g_Device.width * 0.5f;
        float cy = g_Device.height * 0.5f;
        float hx = g_Device.width * 0.30f;
        float hy = g_Device.height * 0.30f;
        if (hx > 0 && hy > 0) {
            float nx = (mx - cx) / hx;
            float ny = (my - cy) / hy;
            // SetHighlightPosition returns Status; clamp inside.
            g_Material.SetHighlightPosition(nx, ny);
        }
        return 0;
    }
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_ESCAPE: g_Running = false; break;
        case VK_SPACE:  g_Paused = !g_Paused; break;
        case 'R':       g_Material = GlassMaterial{}; g_Surface.SetMaterial(g_Material); break;
        case '1': g_Stages.blur = !g_Stages.blur; break;
        case '2': g_Stages.refraction = !g_Stages.refraction; break;
        case '3': g_Stages.dispersion = !g_Stages.dispersion; break;
        case '4': g_Stages.fresnel = !g_Stages.fresnel; break;
        case '5': g_Stages.specular = !g_Stages.specular; break;
        case '6': g_Stages.mask = !g_Stages.mask; break;
        case '7': g_Stages.colorAdjust = !g_Stages.colorAdjust; break;
        case 0xDB: // '['
            if (!g_ParamTable.empty())
                g_SelectedParam = (g_SelectedParam - 1 + (int)g_ParamTable.size()) % (int)g_ParamTable.size();
            break;
        case 0xDD: { // ']'
            if (!g_ParamTable.empty())
                g_SelectedParam = (g_SelectedParam + 1) % (int)g_ParamTable.size();
            break;
        }
        case VK_UP:
            if (!g_ParamTable.empty()) {
                auto& p = g_ParamTable[g_SelectedParam];
                float cur = 0.0f;
                // Read current value by invoking getter-like access via a small helper.
                // Use a lambda table of getter mirrors instead (simplest):
                static const std::vector<float (GlassMaterial::*)() const> getters = {
                    &GlassMaterial::GetBlurRadius, &GlassMaterial::GetRefractionStrength,
                    &GlassMaterial::GetDispersionStrength, &GlassMaterial::GetThickness,
                    &GlassMaterial::GetEdgeFresnel, &GlassMaterial::GetSpecularStrength,
                    &GlassMaterial::GetTintAmount, &GlassMaterial::GetSaturation,
                    &GlassMaterial::GetBrightness, &GlassMaterial::GetNoiseAmount,
                    &GlassMaterial::GetCornerRadius, &GlassMaterial::GetOpacity,
                };
                cur = (g_Material.*getters[g_SelectedParam])();
                (g_Material.*p.setter)(cur + p.step);
                g_Surface.SetMaterial(g_Material);
            }
            break;
        case VK_DOWN:
            if (!g_ParamTable.empty()) {
                auto& p = g_ParamTable[g_SelectedParam];
                static const std::vector<float (GlassMaterial::*)() const> getters = {
                    &GlassMaterial::GetBlurRadius, &GlassMaterial::GetRefractionStrength,
                    &GlassMaterial::GetDispersionStrength, &GlassMaterial::GetThickness,
                    &GlassMaterial::GetEdgeFresnel, &GlassMaterial::GetSpecularStrength,
                    &GlassMaterial::GetTintAmount, &GlassMaterial::GetSaturation,
                    &GlassMaterial::GetBrightness, &GlassMaterial::GetNoiseAmount,
                    &GlassMaterial::GetCornerRadius, &GlassMaterial::GetOpacity,
                };
                float cur = (g_Material.*getters[g_SelectedParam])();
                (g_Material.*p.setter)(cur - p.step);
                g_Surface.SetMaterial(g_Material);
            }
            break;
        default: break;
        }
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
    std::printf("AuroraGlass P1 Proof (visual parity sample) starting...\n");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"AuroraGlassP1ProofClass";
    if (!RegisterClassExW(&wc)) { std::printf("RegisterClass failed\n"); return 1; }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"AuroraGlass P1 Proof",
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

    // Background generator lives at SAMPLE layer, NOT in Core.
    ShaderLibrary shaders; shaders.Init(shaderDir);
    if (!g_Background.Init(g_Device.device.Get(), shaders)) {
        std::printf("BackgroundSource init FAILED\n"); return 1;
    }
    if (!g_Background.Resize(g_Device.device.Get(), g_Device.width, g_Device.height)) {
        std::printf("BackgroundSource resize FAILED\n"); return 1;
    }

    // Create the glass surface via the P1 PUBLIC API.
    // Note: SurfaceDesc no longer carries shaderDir; Core discovers it internally.
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

    BuildParamTable();
    // Default material = GlassMaterial{} (matches P0 baseline defaults).
    g_Surface.SetMaterial(g_Material);
    g_Stages = DiagnosticStages::AllEnabled();

    std::printf("Running. Keys: 1-7 stages, [ ] param, Up/Down adjust, R reset, Space pause, Esc quit.\n");

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    uint64_t frameCount = 0;
    int titleCounter = 0;

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
        if (!g_Paused) g_Time += (float)dt;

        if (!g_Minimized && g_PendingW > 0 && g_PendingH > 0 &&
            (g_PendingW != g_Device.width || g_PendingH != g_Device.height)) {
            g_Device.Resize(g_PendingW, g_PendingH);
            g_Background.Resize(g_Device.device.Get(), g_PendingW, g_PendingH);
            g_Surface.Resize(g_PendingW, g_PendingH);
            g_PendingW = g_PendingH = 0;
        }

        // Automated test sequence — mirror of p0_proof parity checks.
        if (autoFrames > 0) {
            if (frameCount == 30) {
                g_Device.Resize(1024, 768);
                g_Background.Resize(g_Device.device.Get(), 1024, 768);
                g_Surface.Resize(1024, 768);
                std::printf("[p1-proof] frame 30: Resize -> 1024x768\n");
            }
            if (frameCount == 60) {
                g_Stages.blur = !g_Stages.blur;
                g_Stages.refraction = !g_Stages.refraction;
                g_Stages.dispersion = !g_Stages.dispersion;
                g_Stages.fresnel = !g_Stages.fresnel;
                g_Stages.specular = !g_Stages.specular;
                g_Stages.mask = !g_Stages.mask;
                g_Stages.colorAdjust = !g_Stages.colorAdjust;
                std::printf("[p1-proof] frame 60: all stages toggled\n");
            }
            if (frameCount == 90) {
                g_Stages = DiagnosticStages::AllEnabled();
                g_Material.SetBlurRadius(17.0f);         // default 12 + 5
                g_Material.SetRefractionStrength(0.70f); // default 0.6 + 0.1
                g_Material.SetDispersionStrength(0.60f); // default 0.5 + 0.1
                g_Surface.SetMaterial(g_Material);
                std::printf("[p1-proof] frame 90: stages restored + params adjusted\n");
            }
            if (frameCount >= (uint64_t)autoFrames) g_Running = false;
        }

        if (!g_Minimized && g_Device.rtv) {
            g_Device.Clear(0.0f, 0.0f, 0.0f, 1.0f);

            // Background at sample layer (P0-equivalent procedural source).
            g_Background.Render(g_Device.context.Get(), g_Time);

            // Render via the P1 PUBLIC API.
            FrameInfo frame;
            frame.timeSeconds = g_Time;
            frame.stages = g_Stages;
            Status rr = g_Surface.Render(g_Device.context.Get(), g_Device.rtv.Get(),
                                         g_Background.TextureSRV(), frame);
            if (!rr.ok()) {
                std::printf("[p1-proof] Render FAILED: code=%s\n", ErrorCodeToString(rr.code));
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

        if (++titleCounter >= 15) { titleCounter = 0; UpdateTitle(hwnd); }
    }

    std::printf("Shutting down... (frames=%llu, fps=%.1f, ms=%.2f)\n",
        (unsigned long long)frameCount, g_FpsEMA, g_FrameMsEMA);

    g_Surface.Reset();          // explicit deterministic cleanup (dtor also handles)
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
