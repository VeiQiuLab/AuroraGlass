#include "win32/win32_host_attachment.h"

#include <CommCtrl.h>

#include <utility>

namespace AuroraGlass::Adapters::Win32 {

namespace {

using GetDpiForWindowFn =
    UINT (WINAPI*)(HWND);

} // namespace

Win32HostAttachment::~Win32HostAttachment() noexcept {
    Detach();
}

UINT Win32HostAttachment::QueryWindowDpi(
    HWND hwnd) noexcept
{
    HMODULE user32 =
        GetModuleHandleW(L"user32.dll");

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

    if (rawDpi <= 0) {
        return 96;
    }

    return static_cast<UINT>(rawDpi);
}

bool Win32HostAttachment::Attach(
    HWND hwnd) noexcept
{
    if (hwnd == nullptr ||
        !IsWindow(hwnd))
    {
        return false;
    }

    if (hwnd_ == hwnd) {
        return true;
    }

    if (hwnd_ != nullptr) {
        return false;
    }

    RECT clientRect{};

    if (!GetClientRect(
            hwnd,
            &clientRect))
    {
        return false;
    }

    Win32HostMetrics initialMetrics{};
    initialMetrics.clientWidth =
        static_cast<std::uint32_t>(
            clientRect.right - clientRect.left);
    initialMetrics.clientHeight =
        static_cast<std::uint32_t>(
            clientRect.bottom - clientRect.top);
    initialMetrics.dpi =
        QueryWindowDpi(hwnd);

    const UINT_PTR subclassId =
        SubclassId();

    if (!SetWindowSubclass(
            hwnd,
            &Win32HostAttachment::SubclassProc,
            subclassId,
            reinterpret_cast<DWORD_PTR>(this)))
    {
        return false;
    }

    hwnd_ = hwnd;
    metrics_ = initialMetrics;

    return true;
}

void Win32HostAttachment::ClearAttachmentState() noexcept {
    hwnd_ = nullptr;
    metrics_ = {};
    resizeCallback_ = {};
    dpiChangedCallback_ = {};
}

void Win32HostAttachment::Detach() noexcept {
    HWND hwnd = hwnd_;

    if (hwnd == nullptr) {
        ClearAttachmentState();
        return;
    }

    // Clear first so no callback can observe an attached state after Detach().
    ClearAttachmentState();

    if (IsWindow(hwnd)) {
        RemoveWindowSubclass(
            hwnd,
            &Win32HostAttachment::SubclassProc,
            SubclassId());
    }
}

void Win32HostAttachment::SetResizeCallback(
    ResizeCallback callback)
{
    resizeCallback_ =
        std::move(callback);
}

void Win32HostAttachment::SetDpiChangedCallback(
    DpiChangedCallback callback)
{
    dpiChangedCallback_ =
        std::move(callback);
}

LRESULT CALLBACK Win32HostAttachment::SubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR referenceData) noexcept
{
    auto* attachment =
        reinterpret_cast<Win32HostAttachment*>(
            referenceData);

    if (attachment == nullptr) {
        return DefSubclassProc(
            hwnd,
            message,
            wParam,
            lParam);
    }

    if (message == WM_SIZE &&
        attachment->hwnd_ == hwnd)
    {
        // Win32's WM_SIZE contract exposes client pixels through lParam.
        // Zero-by-zero is a valid minimized host state at this adapter layer.
        attachment->metrics_.clientWidth =
            static_cast<std::uint32_t>(
                LOWORD(lParam));
        attachment->metrics_.clientHeight =
            static_cast<std::uint32_t>(
                HIWORD(lParam));

        const Win32HostMetrics snapshot =
            attachment->metrics_;

        auto callback =
            attachment->resizeCallback_;

        if (callback) {
            callback(snapshot);
        }
    }
    else if (message == WM_DPICHANGED &&
             attachment->hwnd_ == hwnd)
    {
        // Repository P2 host contract uses the X DPI from LOWORD(wParam)
        // as the single window DPI value. Do not apply the suggested RECT:
        // the host application remains responsible for window positioning.
        const UINT newDpi =
            LOWORD(wParam);

        if (newDpi != 0) {
            attachment->metrics_.dpi =
                newDpi;
        }

        const Win32HostMetrics snapshot =
            attachment->metrics_;

        auto callback =
            attachment->dpiChangedCallback_;

        if (callback) {
            callback(snapshot);
        }
    }
    else if (message == WM_NCDESTROY &&
             attachment->hwnd_ == hwnd)
    {
        // Remove our hook before the HWND finishes destruction.
        // No host-owned HWND or callback survives in adapter state.
        RemoveWindowSubclass(
            hwnd,
            &Win32HostAttachment::SubclassProc,
            subclassId);

        attachment->ClearAttachmentState();
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace AuroraGlass::Adapters::Win32