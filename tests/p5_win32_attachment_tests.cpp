#include "win32/win32_host_attachment.h"

#include <Windows.h>

#include <cstdio>

using AuroraGlass::Adapters::Win32::Win32HostAttachment;

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
    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam);
}

HWND CreateTestWindow(
    HINSTANCE instance,
    const wchar_t* className)
{
    return CreateWindowExW(
        0,
        className,
        L"AuroraGlass P5 Attachment Test",
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

} // namespace

int main() {
    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    const wchar_t* className =
        L"AuroraGlass.P5.Win32Attachment.Test";

    WNDCLASSW wc{};
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;

    const ATOM atom =
        RegisterClassW(&wc);

    Check(atom != 0, "register test window class");

    if (atom == 0) {
        std::printf(
            "P5_WIN32_ATTACHMENT_TESTS=FAIL checks=%d failures=%d\n",
            g_checks,
            g_failures);

        return 1;
    }

    HWND first =
        CreateTestWindow(
            instance,
            className);

    HWND second =
        CreateTestWindow(
            instance,
            className);

    Check(first != nullptr, "create first real HWND");
    Check(second != nullptr, "create second real HWND");

    if (first == nullptr ||
        second == nullptr)
    {
        if (first != nullptr) {
            DestroyWindow(first);
        }

        if (second != nullptr) {
            DestroyWindow(second);
        }

        UnregisterClassW(
            className,
            instance);

        std::printf(
            "P5_WIN32_ATTACHMENT_TESTS=FAIL checks=%d failures=%d\n",
            g_checks,
            g_failures);

        return 1;
    }

    {
        Win32HostAttachment attachment;

        Check(
            !attachment.IsAttached(),
            "new attachment starts detached");

        Check(
            attachment.Window() == nullptr,
            "new attachment has no HWND");

        Check(
            !attachment.Attach(nullptr),
            "null HWND rejected");

        Check(
            attachment.Attach(first),
            "attach first real HWND");

        Check(
            attachment.IsAttached(),
            "attachment reports attached");

        Check(
            attachment.Window() == first,
            "attachment exposes host HWND");

        Check(
            attachment.Attach(first),
            "same HWND attach is idempotent");

        Check(
            !attachment.Attach(second),
            "different HWND rejected while attached");

        attachment.Detach();

        Check(
            !attachment.IsAttached(),
            "explicit detach clears state");

        Check(
            attachment.Window() == nullptr,
            "explicit detach clears HWND");

        attachment.Detach();

        Check(
            !attachment.IsAttached(),
            "repeated detach is safe");

        Check(
            attachment.Attach(second),
            "reattach after detach succeeds");

        DestroyWindow(second);
        second = nullptr;

        Check(
            !attachment.IsAttached(),
            "WM_NCDESTROY auto-clears attachment");

        Check(
            attachment.Window() == nullptr,
            "WM_NCDESTROY clears host HWND");

        attachment.Detach();

        Check(
            !attachment.IsAttached(),
            "detach after host destruction is safe");
    }

    {
        Win32HostAttachment attachment;

        Check(
            attachment.Attach(first),
            "second attachment can attach first HWND");

        Check(
            attachment.IsAttached(),
            "second attachment reports attached");
    }

    // The adapter destructor must have removed its subclass hook.
    // Sending a normal message after adapter destruction must continue
    // through the host WndProc without referencing the dead adapter.
    const LRESULT pingResult =
        SendMessageW(
            first,
            WM_NULL,
            0,
            0);

    Check(
        pingResult == 0,
        "host window remains valid after adapter destructor teardown");

    DestroyWindow(first);

    Check(
        !IsWindow(first),
        "host window destroys normally after adapter teardown");

    const BOOL unregistered =
        UnregisterClassW(
            className,
            instance);

    Check(
        unregistered != FALSE,
        "unregister test window class");

    if (g_failures != 0) {
        std::printf(
            "P5_WIN32_ATTACHMENT_TESTS=FAIL checks=%d failures=%d\n",
            g_checks,
            g_failures);

        return 1;
    }

    std::printf(
        "P5_WIN32_ATTACHMENT_TESTS=PASS checks=%d failures=0\n",
        g_checks);

    return 0;
}