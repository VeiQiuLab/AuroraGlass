#include "winui/winui_material_bridge.h"
#include "winui/winui_render_host_bridge.h"
#include "wpf/wpf_material_bridge.h"

#include <Windows.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>

namespace {

int checks = 0;
int failures = 0;

void Check(bool value, const char* name)
{
    ++checks;

    if (!value) {
        ++failures;
        std::cout << "[FAIL] " << name << "\n";
        return;
    }

    std::cout << "[PASS] " << name << "\n";
}

bool Near(float a, float b)
{
    return std::fabs(a - b) <= 0.0001f;
}

LRESULT CALLBACK Proc(
    HWND hwnd,
    UINT msg,
    WPARAM wp,
    LPARAM lp)
{
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND MakeWindow()
{
    constexpr wchar_t name[] =
        L"AuroraGlass.P7.SliceC";

    WNDCLASSW wc{};
    wc.lpfnWndProc = Proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = name;

    if (RegisterClassW(&wc) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return nullptr;
    }

    HWND hwnd =
        CreateWindowExW(
            0,
            name,
            L"P7 Slice C",
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            640,
            480,
            nullptr,
            nullptr,
            wc.hInstance,
            nullptr);

    if (hwnd != nullptr) {
        ShowWindow(hwnd, SW_SHOWNA);
    }

    return hwnd;
}

void Pump()
{
    const auto end =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(700);

    while (std::chrono::steady_clock::now() < end) {
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
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1));
    }
}

}

int main()
{
    auto* p7 =
        AuroraGlassWinUIMaterialCreate();

    auto* frozen =
        AuroraGlassWpfMaterialCreate();

    Check(p7 != nullptr, "material lifecycle");
    Check(frozen != nullptr, "frozen material reference");

    if (p7 == nullptr ||
        frozen == nullptr)
    {
        return 1;
    }

    AuroraGlassWinUIMaterialSnapshot a{};
    AuroraGlassWpfMaterialSnapshot b{};

    Check(
        AuroraGlassWinUIMaterialGetSnapshot(p7, &a) == 0 &&
        AuroraGlassWpfMaterialGetSnapshot(frozen, &b) == 0,
        "default material snapshots");

    Check(
        Near(a.blurRadius, b.blurRadius) &&
        Near(a.opacity, b.opacity) &&
        Near(a.cornerRadius, b.cornerRadius),
        "same frozen defaults");

    int validStatus =
        AuroraGlassWinUIMaterialSetOpacity(
            p7,
            0.75f);

    Check(
        validStatus == 0,
        "validated setter forwarding");

    AuroraGlassWinUIMaterialSnapshot before{};

    AuroraGlassWinUIMaterialGetSnapshot(
        p7,
        &before);

    const float nan =
        std::numeric_limits<float>::quiet_NaN();

    const int invalidStatus =
        AuroraGlassWinUIMaterialSetOpacity(
            p7,
            nan);

    AuroraGlassWinUIMaterialSnapshot after{};

    AuroraGlassWinUIMaterialGetSnapshot(
        p7,
        &after);

    Check(
        invalidStatus != 0,
        "invalid setter rejected");

    Check(
        Near(before.opacity, after.opacity),
        "invalid setter preserves prior state");

    HWND parent =
        MakeWindow();

    Check(
        parent != nullptr,
        "real HWND");

    auto* render =
        AuroraGlassWinUIRenderHostCreate(
            reinterpret_cast<intptr_t>(parent));

    Check(
        render != nullptr,
        "render host create");

    if (render == nullptr) {
        return 1;
    }

    HWND child =
        reinterpret_cast<HWND>(
            AuroraGlassWinUIRenderHostGetHwnd(render));

    Check(
        child != nullptr &&
        IsWindow(child),
        "render child");

    Check(
        GetParent(child) == parent,
        "render child ownership");

    Check(
        SendMessageW(
            child,
            WM_NCHITTEST,
            0,
            0) == HTTRANSPARENT,
        "single input path");

    Check(
        AuroraGlassWinUIRenderHostResize(
            render,
            320,
            240) == 0,
        "resize");

    Check(
        AuroraGlassWinUIRenderHostResize(
            render,
            0,
            0) == 0 &&
        IsWindowVisible(child) == FALSE,
        "zero-size");

    Check(
        AuroraGlassWinUIRenderHostResize(
            render,
            420,
            300) == 0 &&
        IsWindowVisible(child) != FALSE,
        "restore");

    bool rapid = true;

    for (uint32_t i = 0;
         i < 8;
         ++i)
    {
        if (AuroraGlassWinUIRenderHostResize(
                render,
                420 + i * 7,
                300 + i * 5) != 0)
        {
            rapid = false;
        }
    }

    Check(
        rapid,
        "rapid resize");

    AuroraGlassWinUIMaterialSnapshot active{};

    AuroraGlassWinUIMaterialGetSnapshot(
        p7,
        &active);

    Check(
        AuroraGlassWinUIRenderHostSetMaterial(
            render,
            &active) == 0,
        "material application");

    AuroraGlassWinUIRenderRect rect{
        30.0f,
        30.0f,
        180.0f,
        110.0f
    };

    Check(
        AuroraGlassWinUIRenderHostSetRects(
            render,
            &rect,
            1) == 0,
        "RenderRect submission");

    Pump();

    AuroraGlassWinUIRenderStats stats{};

    Check(
        AuroraGlassWinUIRenderHostGetStats(
            render,
            &stats) == 0,
        "render stats");

    Check(
        stats.ready != 0,
        "D3D11 ready");

    Check(
        stats.lastCoreStatus == 0,
        "RenderRect execution");

    Check(
        stats.frameCount > 0,
        "present output");

    Check(
        stats.width > 0 &&
        stats.height > 0,
        "GlassSurface");

    HWND oldChild = child;

    AuroraGlassWinUIRenderHostDestroy(
        render);

    Check(
        !IsWindow(oldChild),
        "clean child teardown");

    Check(
        IsWindow(parent),
        "top-level ownership preserved");

    DestroyWindow(parent);

    AuroraGlassWinUIMaterialDestroy(p7);
    AuroraGlassWpfMaterialDestroy(frozen);

    std::cout
        << "P7_WINUI_MATERIAL_RENDER_TESTS: "
        << checks
        << " checks, "
        << failures
        << " failures\n";

    return failures == 0 ? 0 : 1;
}