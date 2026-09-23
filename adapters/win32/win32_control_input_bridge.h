#pragma once

// ============================================================
// AuroraGlass P5 Win32 Adapter - minimal glass-control input bridge.
//
// Translates only the Win32 mouse input required by the frozen P3
// GlassButton / GlassToggle / GlassSlider public semantic APIs.
//
// Coordinate contract:
//   Win32 client coordinates in physical pixels
//   -> AuroraGlass ControlPoint physical pixels.
//
// This adapter intentionally does NOT provide:
//   - generic Win32 message routing
//   - event bus / routed events
//   - visual tree / z-order framework
//   - keyboard framework
//   - touch / pen / gesture abstraction
//   - layout
//   - window management
//
// HWND ownership remains with the host application.
// ============================================================

#include <Windows.h>

#include "controls/control_button.h"
#include "controls/control_slider.h"
#include "controls/control_toggle.h"

#include <vector>

namespace AuroraGlass::Adapters::Win32 {

class Win32ControlInputBridge final {
public:
    Win32ControlInputBridge() noexcept = default;
    ~Win32ControlInputBridge() noexcept;

    Win32ControlInputBridge(
        const Win32ControlInputBridge&) = delete;

    Win32ControlInputBridge& operator=(
        const Win32ControlInputBridge&) = delete;

    Win32ControlInputBridge(
        Win32ControlInputBridge&&) = delete;

    Win32ControlInputBridge& operator=(
        Win32ControlInputBridge&&) = delete;

    bool Attach(HWND hwnd) noexcept;
    void Detach() noexcept;

    bool IsAttached() const noexcept {
        return hwnd_ != nullptr;
    }

    HWND Window() const noexcept {
        return hwnd_;
    }

    bool AddButton(GlassButton& button) noexcept;
    bool AddToggle(GlassToggle& toggle) noexcept;
    bool AddSlider(GlassSlider& slider) noexcept;

    void ClearControls() noexcept;

    bool OwnsCapture() const noexcept {
        return ownsCapture_;
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

    static ControlPoint PointFromLParam(
        LPARAM lParam) noexcept;

    void ArmMouseLeave() noexcept;

    void DispatchMove(ControlPoint point) noexcept;
    void DispatchDown(ControlPoint point) noexcept;
    void DispatchUp(ControlPoint point) noexcept;
    void DispatchLeave() noexcept;

    bool AnyControlPressed() const noexcept;

    bool AcquireCapture() noexcept;
    void ReleaseOwnedCapture() noexcept;

    void ClearWindowState() noexcept;

    HWND hwnd_ = nullptr;

    bool trackingMouseLeave_ = false;
    bool ownsCapture_ = false;
    bool intentionalCaptureRelease_ = false;

    std::vector<GlassButton*> buttons_;
    std::vector<GlassToggle*> toggles_;
    std::vector<GlassSlider*> sliders_;
};

} // namespace AuroraGlass::Adapters::Win32