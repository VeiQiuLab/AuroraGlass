#include "win32/win32_host_attachment.h"

#include <Windows.h>

#include <cstdio>
#include <cstdint>

using AuroraGlass::Adapters::Win32::Win32HostAttachment;
using AuroraGlass::Adapters::Win32::Win32HostMetrics;

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(
    bool condition,
    const char* name)
{
    ++g_checks;

    if (!condition) {
        ++g_failures;
        std::printf(
            "[FAIL] %s\n",
            name);
    }
}

LRESULT CALLBACK TestWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

UINT QueryExpectedDpi(
    HWND hwnd)
{
    using GetDpiForWindowFn =
        UINT (WINAPI*)(HWND);

    HMODULE user32 =
        GetModuleHandleW(
            L"user32.dll");

    if (user32 != nullptr) {
        auto getDpiForWindow =
            reinterpret_cast<GetDpiForWindowFn>(
                GetProcAddress(
                    user32,
                    "GetDpiForWindow"));

        if (getDpiForWindow != nullptr) {
            const UINT dpi =
                getDpiForWindow(hwnd);

            if (dpi != 0) {
                return dpi;
            }
        }
    }

    HDC dc =
        GetDC(hwnd);

    if (dc == nullptr) {
        return 96;
    }

    const int rawDpi =
        GetDeviceCaps(
            dc,
            LOGPIXELSX);

    ReleaseDC(
        hwnd,
        dc);

    return rawDpi > 0
        ? static_cast<UINT>(rawDpi)
        : 96;
}

bool SameRect(
    const RECT& a,
    const RECT& b)
{
    return
        a.left == b.left &&
        a.top == b.top &&
        a.right == b.right &&
        a.bottom == b.bottom;
}

} // namespace

int main() {
    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    const wchar_t* className =
        L"AuroraGlass.P5.HostMetrics.Test";

    WNDCLASSW wc{};
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;

    const ATOM atom =
        RegisterClassW(&wc);

    Check(
        atom != 0,
        "register test class");

    if (atom == 0) {
        return 1;
    }

    HWND hwnd =
        CreateWindowExW(
            0,
            className,
            L"AuroraGlass P5 Host Metrics",
            WS_POPUP,
            100,
            100,
            320,
            200,
            nullptr,
            nullptr,
            instance,
            nullptr);

    Check(
        hwnd != nullptr,
        "create real HWND");

    if (hwnd == nullptr) {
        UnregisterClassW(
            className,
            instance);

        return 1;
    }

    RECT initialClient{};
    Check(
        GetClientRect(
            hwnd,
            &initialClient) != FALSE,
        "query initial client rect");

    const std::uint32_t expectedInitialWidth =
        static_cast<std::uint32_t>(
            initialClient.right -
            initialClient.left);

    const std::uint32_t expectedInitialHeight =
        static_cast<std::uint32_t>(
            initialClient.bottom -
            initialClient.top);

    const UINT expectedInitialDpi =
        QueryExpectedDpi(hwnd);

    Win32HostAttachment attachment;

    int resizeNotifications = 0;
    int dpiNotifications = 0;

    Win32HostMetrics lastResize{};
    Win32HostMetrics lastDpi{};

    attachment.SetResizeCallback(
        [&](Win32HostMetrics metrics) {
            ++resizeNotifications;
            lastResize = metrics;
        });

    attachment.SetDpiChangedCallback(
        [&](Win32HostMetrics metrics) {
            ++dpiNotifications;
            lastDpi = metrics;
        });

    Check(
        attachment.Attach(hwnd),
        "attach real HWND");

    Check(
        attachment.IsAttached(),
        "attached state true");

    Win32HostMetrics metrics =
        attachment.Metrics();

    Check(
        metrics.clientWidth ==
            expectedInitialWidth,
        "attach captures initial client width");

    Check(
        metrics.clientHeight ==
            expectedInitialHeight,
        "attach captures initial client height");

    Check(
        metrics.dpi ==
            expectedInitialDpi,
        "attach captures initial DPI");

    Check(
        resizeNotifications == 0,
        "attach does not synthesize resize notification");

    Check(
        dpiNotifications == 0,
        "attach does not synthesize DPI notification");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_RESTORED,
        MAKELPARAM(640, 360));

    metrics =
        attachment.Metrics();

    Check(
        metrics.clientWidth == 640 &&
        metrics.clientHeight == 360,
        "WM_SIZE updates metrics");

    Check(
        resizeNotifications == 1,
        "WM_SIZE emits one resize notification");

    Check(
        lastResize.clientWidth == 640 &&
        lastResize.clientHeight == 360,
        "resize callback receives updated size");

    Check(
        lastResize.dpi == expectedInitialDpi,
        "resize callback carries current DPI");

    const std::uint32_t rapidSizes[][2] = {
        {800, 600},
        {1, 1},
        {1280, 720},
        {320, 240}
    };

    for (const auto& size : rapidSizes) {
        SendMessageW(
            hwnd,
            WM_SIZE,
            SIZE_RESTORED,
            MAKELPARAM(
                size[0],
                size[1]));

        metrics =
            attachment.Metrics();

        Check(
            metrics.clientWidth == size[0] &&
            metrics.clientHeight == size[1],
            "rapid WM_SIZE updates current metrics");
    }

    Check(
        resizeNotifications == 5,
        "rapid resize emits one notification per message");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_MINIMIZED,
        MAKELPARAM(0, 0));

    metrics =
        attachment.Metrics();

    Check(
        metrics.clientWidth == 0 &&
        metrics.clientHeight == 0,
        "zero-size minimized state is preserved");

    Check(
        resizeNotifications == 6,
        "zero-size state emits resize notification");

    Check(
        lastResize.clientWidth == 0 &&
        lastResize.clientHeight == 0,
        "zero-size callback receives zero snapshot");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_RESTORED,
        MAKELPARAM(500, 300));

    metrics =
        attachment.Metrics();

    Check(
        metrics.clientWidth == 500 &&
        metrics.clientHeight == 300,
        "restore updates client size");

    RECT beforeDpiRect{};
    RECT afterDpiRect{};

    Check(
        GetWindowRect(
            hwnd,
            &beforeDpiRect) != FALSE,
        "capture window rect before DPI change");

    RECT suggested{
        20,
        30,
        920,
        730
    };

    const UINT injectedDpi = 144;

    SendMessageW(
        hwnd,
        WM_DPICHANGED,
        MAKEWPARAM(
            injectedDpi,
            injectedDpi),
        reinterpret_cast<LPARAM>(
            &suggested));

    metrics =
        attachment.Metrics();

    Check(
        metrics.dpi == injectedDpi,
        "WM_DPICHANGED updates DPI");

    Check(
        dpiNotifications == 1,
        "WM_DPICHANGED emits one DPI notification");

    Check(
        lastDpi.dpi == injectedDpi,
        "DPI callback receives updated DPI");

    Check(
        lastDpi.clientWidth == 500 &&
        lastDpi.clientHeight == 300,
        "DPI callback carries current client size");

    Check(
        GetWindowRect(
            hwnd,
            &afterDpiRect) != FALSE,
        "capture window rect after DPI change");

    Check(
        SameRect(
            beforeDpiRect,
            afterDpiRect),
        "adapter does not apply WM_DPICHANGED suggested RECT");

    const int resizeBeforeDetach =
        resizeNotifications;

    const int dpiBeforeDetach =
        dpiNotifications;

    attachment.Detach();

    Check(
        !attachment.IsAttached(),
        "explicit detach clears attached state");

    metrics =
        attachment.Metrics();

    Check(
        metrics.clientWidth == 0 &&
        metrics.clientHeight == 0 &&
        metrics.dpi == 0,
        "explicit detach clears metrics snapshot");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_RESTORED,
        MAKELPARAM(777, 555));

    SendMessageW(
        hwnd,
        WM_DPICHANGED,
        MAKEWPARAM(192, 192),
        reinterpret_cast<LPARAM>(
            &suggested));

    Check(
        resizeNotifications ==
            resizeBeforeDetach,
        "no resize callback after detach");

    Check(
        dpiNotifications ==
            dpiBeforeDetach,
        "no DPI callback after detach");

    Check(
        attachment.Attach(hwnd),
        "reattach succeeds");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_RESTORED,
        MAKELPARAM(444, 222));

    SendMessageW(
        hwnd,
        WM_DPICHANGED,
        MAKEWPARAM(168, 168),
        reinterpret_cast<LPARAM>(
            &suggested));

    Check(
        resizeNotifications ==
            resizeBeforeDetach,
        "detach releases resize callback");

    Check(
        dpiNotifications ==
            dpiBeforeDetach,
        "detach releases DPI callback");

    int destroyResizeNotifications = 0;
    int destroyDpiNotifications = 0;

    attachment.SetResizeCallback(
        [&](Win32HostMetrics) {
            ++destroyResizeNotifications;
        });

    attachment.SetDpiChangedCallback(
        [&](Win32HostMetrics) {
            ++destroyDpiNotifications;
        });

    DestroyWindow(hwnd);

    Check(
        !attachment.IsAttached(),
        "WM_NCDESTROY auto-detaches");

    metrics =
        attachment.Metrics();

    Check(
        metrics.clientWidth == 0 &&
        metrics.clientHeight == 0 &&
        metrics.dpi == 0,
        "WM_NCDESTROY clears metrics snapshot");

    Check(
        destroyResizeNotifications == 0,
        "host destruction emits no stale resize callback");

    Check(
        destroyDpiNotifications == 0,
        "host destruction emits no stale DPI callback");

    Check(
        !IsWindow(hwnd),
        "real host HWND destroyed");

    Check(
        UnregisterClassW(
            className,
            instance) != FALSE,
        "unregister test class");

    if (g_failures != 0) {
        std::printf(
            "P5_WIN32_HOST_METRICS_TESTS=FAIL checks=%d failures=%d\n",
            g_checks,
            g_failures);

        return 1;
    }

    std::printf(
        "P5_WIN32_HOST_METRICS_TESTS=PASS checks=%d failures=0\n",
        g_checks);

    return 0;
}