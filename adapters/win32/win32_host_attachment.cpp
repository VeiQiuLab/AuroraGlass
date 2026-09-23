#include "win32/win32_host_attachment.h"

#include <CommCtrl.h>

namespace AuroraGlass::Adapters::Win32 {

Win32HostAttachment::~Win32HostAttachment() noexcept {
    Detach();
}

bool Win32HostAttachment::Attach(HWND hwnd) noexcept {
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
    return true;
}

void Win32HostAttachment::Detach() noexcept {
    HWND hwnd = hwnd_;

    if (hwnd == nullptr) {
        return;
    }

    hwnd_ = nullptr;

    if (IsWindow(hwnd)) {
        RemoveWindowSubclass(
            hwnd,
            &Win32HostAttachment::SubclassProc,
            SubclassId());
    }
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

    if (message == WM_NCDESTROY &&
        attachment != nullptr &&
        attachment->hwnd_ == hwnd)
    {
        // Remove our hook before the HWND finishes destruction.
        // The host still owns the window and normal destruction continues
        // through DefSubclassProc.
        RemoveWindowSubclass(
            hwnd,
            &Win32HostAttachment::SubclassProc,
            subclassId);

        attachment->hwnd_ = nullptr;
    }

    return DefSubclassProc(
        hwnd,
        message,
        wParam,
        lParam);
}

} // namespace AuroraGlass::Adapters::Win32