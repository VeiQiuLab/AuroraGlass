#include "winui/winui_host_bridge.h"

#include <Windows.h>

#include <iostream>

namespace {

int failures = 0;
int checks = 0;

void Check(
    bool condition,
    const char* name)
{
    ++checks;

    if (condition) {
        std::cout
            << "[PASS] "
            << name
            << "\n";
        return;
    }

    ++failures;

    std::cout
        << "[FAIL] "
        << name
        << "\n";
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

HWND CreateTestWindow()
{
    constexpr wchar_t className[] =
        L"AuroraGlass.P7.WinUI.InteropTest";

    WNDCLASSW wc{};
    wc.lpfnWndProc =
        TestWndProc;
    wc.hInstance =
        GetModuleHandleW(nullptr);
    wc.lpszClassName =
        className;

    ATOM atom =
        RegisterClassW(&wc);

    if (atom == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        className,
        L"AuroraGlass P7 Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320,
        240,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);
}

}

int main()
{
    AuroraGlassWinUIHostDestroy(nullptr);
    Check(
        true,
        "destroy null is safe");

    Check(
        AuroraGlassWinUIHostAttach(
            nullptr,
            0) ==
            AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT,
        "null host and HWND rejected");

    auto* host =
        AuroraGlassWinUIHostCreate();

    Check(
        host != nullptr,
        "host create");

    if (host == nullptr) {
        return 1;
    }

    Check(
        AuroraGlassWinUIHostIsAttached(host) == 0,
        "new host detached");

    Check(
        AuroraGlassWinUIHostAttach(
            host,
            0) ==
            AURORAGLASS_WINUI_HOST_INVALID_ARGUMENT,
        "null HWND rejected");

    Check(
        AuroraGlassWinUIHostAttach(
            host,
            static_cast<intptr_t>(1)) ==
            AURORAGLASS_WINUI_HOST_INVALID_WINDOW,
        "non-window HWND rejected");

    HWND first =
        CreateTestWindow();

    Check(
        first != nullptr,
        "real HWND created");

    if (first == nullptr) {
        AuroraGlassWinUIHostDestroy(host);
        return 1;
    }

    Check(
        AuroraGlassWinUIHostAttach(
            host,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WINUI_HOST_OK,
        "real HWND attach");

    Check(
        AuroraGlassWinUIHostIsAttached(host) == 1,
        "attached state valid");

    Check(
        AuroraGlassWinUIHostAttach(
            host,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WINUI_HOST_OK,
        "same HWND attach idempotent");

    HWND second =
        CreateTestWindow();

    Check(
        second != nullptr,
        "second HWND created");

    if (second != nullptr) {
        Check(
            AuroraGlassWinUIHostAttach(
                host,
                reinterpret_cast<intptr_t>(second)) ==
                AURORAGLASS_WINUI_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED,
            "different HWND rejected while attached");
    }

    AuroraGlassWinUIHostDetach(host);

    Check(
        AuroraGlassWinUIHostIsAttached(host) == 0,
        "explicit detach");

    AuroraGlassWinUIHostDetach(host);

    Check(
        AuroraGlassWinUIHostIsAttached(host) == 0,
        "repeated detach safe");

    Check(
        AuroraGlassWinUIHostAttach(
            host,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WINUI_HOST_OK,
        "reattach for host-destroyed-first case");

    AuroraGlassWinUIHostDestroy(host);
    host = nullptr;

    Check(
        IsWindow(first) != FALSE,
        "adapter destruction does not destroy host HWND");

    host =
        AuroraGlassWinUIHostCreate();

    Check(
        host != nullptr,
        "second host create");

    if (host != nullptr) {
        Check(
            AuroraGlassWinUIHostAttach(
                host,
                reinterpret_cast<intptr_t>(first)) ==
                AURORAGLASS_WINUI_HOST_OK,
            "attach for window-destroyed-first case");

        DestroyWindow(first);
        first = nullptr;

        Check(
            AuroraGlassWinUIHostIsAttached(host) == 0,
            "WM_NCDESTROY clears attachment");

        AuroraGlassWinUIHostDestroy(host);
        host = nullptr;
    }

    if (second != nullptr &&
        IsWindow(second))
    {
        DestroyWindow(second);
    }

    std::cout
        << "P7_WINUI_NATIVE: "
        << checks
        << " checks, "
        << failures
        << " failures\n";

    return failures == 0
        ? 0
        : 1;
}