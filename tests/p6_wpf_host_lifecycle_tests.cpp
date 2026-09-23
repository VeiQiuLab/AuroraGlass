#include "wpf/wpf_host_bridge.h"

#include <Windows.h>

#include <cstdio>

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* name) {
    ++g_checks;

    if (!condition) {
        ++g_failures;
        std::printf("[FAIL] %s\n", name);
    }
}

LRESULT CALLBACK TestWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HWND CreateTestWindow(
    HINSTANCE instance,
    const wchar_t* className,
    const wchar_t* title)
{
    return CreateWindowExW(
        0,
        className,
        title,
        WS_OVERLAPPED,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320,
        200,
        nullptr,
        nullptr,
        instance,
        nullptr);
}

void CheckExport(HMODULE module, const char* name) {
    Check(GetProcAddress(module, name) != nullptr, name);
}

}

int main() {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"AuroraGlassP6WpfHostLifecycleTest";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TestWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;

    Check(RegisterClassW(&windowClass) != 0, "register class");

    HWND first = CreateTestWindow(
        instance,
        className,
        L"P6 WPF Host First");

    HWND second = CreateTestWindow(
        instance,
        className,
        L"P6 WPF Host Second");

    Check(first != nullptr, "create first HWND");
    Check(second != nullptr, "create second HWND");

    AuroraGlassWpfHostAttachment* host =
        AuroraGlassWpfHostCreate();

    Check(host != nullptr, "create host handle");
    Check(AuroraGlassWpfHostIsAttached(nullptr) == 0, "null reports detached");

    Check(
        AuroraGlassWpfHostAttach(
            nullptr,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WPF_HOST_INVALID_ARGUMENT,
        "null host rejected");

    Check(
        AuroraGlassWpfHostAttach(host, 0) ==
            AURORAGLASS_WPF_HOST_INVALID_ARGUMENT,
        "null HWND rejected");

    Check(
        AuroraGlassWpfHostAttach(host, static_cast<intptr_t>(1)) ==
            AURORAGLASS_WPF_HOST_INVALID_WINDOW,
        "invalid HWND rejected");

    HMODULE module =
        GetModuleHandleW(L"AuroraGlassWpfInterop.dll");

    Check(module != nullptr, "interop DLL loaded");

    if (module != nullptr) {
        CheckExport(module, "AuroraGlassWpfHostCreate");
        CheckExport(module, "AuroraGlassWpfHostDestroy");
        CheckExport(module, "AuroraGlassWpfHostAttach");
        CheckExport(module, "AuroraGlassWpfHostDetach");
        CheckExport(module, "AuroraGlassWpfHostIsAttached");
    }

    Check(
        AuroraGlassWpfHostAttach(
            host,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WPF_HOST_OK,
        "attach first HWND");

    Check(AuroraGlassWpfHostIsAttached(host) == 1, "attached true");

    Check(
        AuroraGlassWpfHostAttach(
            host,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WPF_HOST_OK,
        "same HWND idempotent");

    Check(
        AuroraGlassWpfHostAttach(
            host,
            reinterpret_cast<intptr_t>(second)) ==
            AURORAGLASS_WPF_HOST_DIFFERENT_WINDOW_ALREADY_ATTACHED,
        "different HWND rejected");

    AuroraGlassWpfHostDetach(host);

    Check(AuroraGlassWpfHostIsAttached(host) == 0, "detach clears state");
    Check(IsWindow(first) != FALSE, "detach leaves HWND alive");

    AuroraGlassWpfHostDetach(host);
    Check(AuroraGlassWpfHostIsAttached(host) == 0, "repeated detach safe");

    Check(
        AuroraGlassWpfHostAttach(
            host,
            reinterpret_cast<intptr_t>(first)) ==
            AURORAGLASS_WPF_HOST_OK,
        "reattach first HWND");

    Check(DestroyWindow(first) != FALSE, "host destroys first HWND");
    first = nullptr;

    Check(
        AuroraGlassWpfHostIsAttached(host) == 0,
        "WM_NCDESTROY auto detaches");

    Check(
        AuroraGlassWpfHostAttach(
            host,
            reinterpret_cast<intptr_t>(second)) ==
            AURORAGLASS_WPF_HOST_OK,
        "attach second HWND");

    AuroraGlassWpfHostDestroy(host);
    host = nullptr;

    Check(
        IsWindow(second) != FALSE,
        "destroy adapter leaves HWND alive");

    AuroraGlassWpfHostAttachment* host2 =
        AuroraGlassWpfHostCreate();

    Check(host2 != nullptr, "create second host handle");

    Check(
        AuroraGlassWpfHostAttach(
            host2,
            reinterpret_cast<intptr_t>(second)) ==
            AURORAGLASS_WPF_HOST_OK,
        "second handle attaches");

    AuroraGlassWpfHostDetach(host2);

    Check(
        AuroraGlassWpfHostIsAttached(host2) == 0,
        "explicit detach");

    Check(
        IsWindow(second) != FALSE,
        "explicit detach leaves HWND alive");

    AuroraGlassWpfHostDestroy(host2);
    AuroraGlassWpfHostDestroy(nullptr);
    AuroraGlassWpfHostDetach(nullptr);

    Check(DestroyWindow(second) != FALSE, "host destroys second HWND");

    UnregisterClassW(className, instance);

    std::printf(
        "P6_WPF_HOST_LIFECYCLE_TESTS: %d checks, %d failures\n",
        g_checks,
        g_failures);

    return g_failures == 0 ? 0 : 1;
}
