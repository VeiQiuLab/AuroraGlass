#pragma once
// ============================================================
// AuroraGlass P5 Win32 Adapter - host window attachment lifecycle.
//
// This adapter does not own the HWND.
// It installs only a Win32 subclass hook so later P5 slices can bridge
// size/DPI/input messages without replacing the application's WndProc.
//
// No layout.
// No window management.
// No input semantics.
// No ownership of the host window.
// ============================================================

#include <Windows.h>

namespace AuroraGlass::Adapters::Win32 {

class Win32HostAttachment final {
public:
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
    // Re-attaching the same HWND is idempotent.
    // Attaching a different HWND while already attached fails.
    bool Attach(HWND hwnd) noexcept;

    // Removes the adapter's subclass hook when the host window is still alive.
    // Safe to call repeatedly.
    void Detach() noexcept;

    bool IsAttached() const noexcept {
        return hwnd_ != nullptr;
    }

    HWND Window() const noexcept {
        return hwnd_;
    }

private:
    static LRESULT CALLBACK SubclassProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData) noexcept;

    UINT_PTR SubclassId() const noexcept {
        return reinterpret_cast<UINT_PTR>(this);
    }

    HWND hwnd_ = nullptr;
};

} // namespace AuroraGlass::Adapters::Win32