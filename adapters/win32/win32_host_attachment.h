#pragma once
// ============================================================
// AuroraGlass P5 Win32 Adapter - host window integration state.
//
// The adapter observes a host-owned HWND through SetWindowSubclass.
// It does not own the HWND, replace the host WndProc, apply DPI-suggested
// window rectangles, route arbitrary Win32 messages, or manage windows.
//
// P5 host state carried here is intentionally narrow:
//   - physical client width / height
//   - current window DPI
//   - resize notification
//   - DPI-change notification
// ============================================================

#include <Windows.h>

#include <cstdint>
#include <functional>

namespace AuroraGlass::Adapters::Win32 {

struct Win32HostMetrics final {
    std::uint32_t clientWidth = 0;
    std::uint32_t clientHeight = 0;
    UINT dpi = 0;
};

class Win32HostAttachment final {
public:
    using ResizeCallback =
        std::function<void(Win32HostMetrics)>;

    using DpiChangedCallback =
        std::function<void(Win32HostMetrics)>;

    Win32HostAttachment() noexcept = default;
    ~Win32HostAttachment() noexcept;

    Win32HostAttachment(
        const Win32HostAttachment&) = delete;

    Win32HostAttachment& operator=(
        const Win32HostAttachment&) = delete;

    Win32HostAttachment(
        Win32HostAttachment&&) = delete;

    Win32HostAttachment& operator=(
        Win32HostAttachment&&) = delete;

    // Attaches to a live host window.
    //
    // The HWND remains owned by the host application.
    // Client size and DPI are captured before this function returns true.
    // Re-attaching the same HWND is idempotent.
    // Attaching a different HWND while already attached fails.
    bool Attach(HWND hwnd) noexcept;

    // Removes the adapter's subclass hook when the host window is still alive.
    // Detach clears the HWND and metrics snapshot and releases callbacks.
    // Safe to call repeatedly.
    void Detach() noexcept;

    bool IsAttached() const noexcept {
        return hwnd_ != nullptr;
    }

    HWND Window() const noexcept {
        return hwnd_;
    }

    // Metrics are valid whenever IsAttached() is true.
    // A zero client size is valid while the HWND is minimized.
    // Detached state is the explicit zero snapshot {0, 0, 0}.
    Win32HostMetrics Metrics() const noexcept {
        return metrics_;
    }

    void SetResizeCallback(
        ResizeCallback callback);

    void SetDpiChangedCallback(
        DpiChangedCallback callback);

private:
    static LRESULT CALLBACK SubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData) noexcept;

    static UINT QueryWindowDpi(
        HWND hwnd) noexcept;

    UINT_PTR SubclassId() const noexcept {
        return reinterpret_cast<UINT_PTR>(this);
    }

    void ClearAttachmentState() noexcept;

    HWND hwnd_ = nullptr;
    Win32HostMetrics metrics_{};
    ResizeCallback resizeCallback_{};
    DpiChangedCallback dpiChangedCallback_{};
};

} // namespace AuroraGlass::Adapters::Win32