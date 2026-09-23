#include "win32/win32_control_input_bridge.h"

#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>

namespace AuroraGlass::Adapters::Win32 {

namespace {

template <typename T>
bool AddUnique(
    std::vector<T*>& controls,
    T& control) noexcept
{
    if (std::find(
            controls.begin(),
            controls.end(),
            &control) != controls.end())
    {
        return true;
    }

    try {
        controls.push_back(&control);
        return true;
    }
    catch (...) {
        return false;
    }
}

} // namespace

Win32ControlInputBridge::~Win32ControlInputBridge() noexcept {
    Detach();
}

bool Win32ControlInputBridge::Attach(
    HWND hwnd) noexcept
{
    if (hwnd == nullptr || !IsWindow(hwnd))
        return false;

    if (hwnd_ == hwnd)
        return true;

    if (hwnd_ != nullptr)
        return false;

    if (!SetWindowSubclass(
            hwnd,
            &Win32ControlInputBridge::SubclassProc,
            SubclassId(),
            reinterpret_cast<DWORD_PTR>(this)))
    {
        return false;
    }

    hwnd_ = hwnd;
    return true;
}

void Win32ControlInputBridge::Detach() noexcept {
    HWND hwnd = hwnd_;

    DispatchLeave();
    ReleaseOwnedCapture();

    hwnd_ = nullptr;
    trackingMouseLeave_ = false;

    if (hwnd != nullptr && IsWindow(hwnd)) {
        RemoveWindowSubclass(
            hwnd,
            &Win32ControlInputBridge::SubclassProc,
            SubclassId());
    }

    buttons_.clear();
    toggles_.clear();
    sliders_.clear();

    ClearWindowState();
}

bool Win32ControlInputBridge::AddButton(
    GlassButton& button) noexcept
{
    return AddUnique(buttons_, button);
}

bool Win32ControlInputBridge::AddToggle(
    GlassToggle& toggle) noexcept
{
    return AddUnique(toggles_, toggle);
}

bool Win32ControlInputBridge::AddSlider(
    GlassSlider& slider) noexcept
{
    return AddUnique(sliders_, slider);
}

void Win32ControlInputBridge::ClearControls() noexcept {
    DispatchLeave();
    ReleaseOwnedCapture();

    buttons_.clear();
    toggles_.clear();
    sliders_.clear();
}

ControlPoint Win32ControlInputBridge::PointFromLParam(
    LPARAM lParam) noexcept
{
    return ControlPoint{
        static_cast<float>(GET_X_LPARAM(lParam)),
        static_cast<float>(GET_Y_LPARAM(lParam))
    };
}

void Win32ControlInputBridge::ArmMouseLeave() noexcept {
    if (hwnd_ == nullptr ||
        trackingMouseLeave_ ||
        ownsCapture_)
    {
        return;
    }

    TRACKMOUSEEVENT tracking{};
    tracking.cbSize = sizeof(tracking);
    tracking.dwFlags = TME_LEAVE;
    tracking.hwndTrack = hwnd_;

    if (TrackMouseEvent(&tracking))
        trackingMouseLeave_ = true;
}

void Win32ControlInputBridge::DispatchMove(
    ControlPoint point) noexcept
{
    for (GlassButton* c : buttons_)
        if (c) c->PointerMove(point);

    for (GlassToggle* c : toggles_)
        if (c) c->PointerMove(point);

    for (GlassSlider* c : sliders_)
        if (c) c->PointerMove(point);
}

void Win32ControlInputBridge::DispatchDown(
    ControlPoint point) noexcept
{
    for (GlassButton* c : buttons_)
        if (c) c->PointerDown(point);

    for (GlassToggle* c : toggles_)
        if (c) c->PointerDown(point);

    for (GlassSlider* c : sliders_)
        if (c) c->PointerDown(point);
}

void Win32ControlInputBridge::DispatchUp(
    ControlPoint point) noexcept
{
    for (GlassButton* c : buttons_)
        if (c) c->PointerUp(point);

    for (GlassToggle* c : toggles_)
        if (c) c->PointerUp(point);

    for (GlassSlider* c : sliders_)
        if (c) c->PointerUp(point);
}

void Win32ControlInputBridge::DispatchLeave() noexcept {
    for (GlassButton* c : buttons_)
        if (c) c->PointerLeave();

    for (GlassToggle* c : toggles_)
        if (c) c->PointerLeave();

    for (GlassSlider* c : sliders_)
        if (c) c->PointerLeave();
}

bool Win32ControlInputBridge::AnyControlPressed() const noexcept {
    for (const GlassButton* c : buttons_) {
        if (c && c->State() == ControlInteractionState::Pressed)
            return true;
    }

    for (const GlassToggle* c : toggles_) {
        if (c && c->State() == ControlInteractionState::Pressed)
            return true;
    }

    for (const GlassSlider* c : sliders_) {
        if (c && c->State() == ControlInteractionState::Pressed)
            return true;
    }

    return false;
}

bool Win32ControlInputBridge::AcquireCapture() noexcept {
    if (hwnd_ == nullptr)
        return false;

    if (ownsCapture_ && GetCapture() == hwnd_)
        return true;

    SetCapture(hwnd_);

    ownsCapture_ =
        GetCapture() == hwnd_;

    if (!ownsCapture_)
        DispatchLeave();

    return ownsCapture_;
}

void Win32ControlInputBridge::ReleaseOwnedCapture() noexcept {
    HWND hwnd = hwnd_;

    if (!ownsCapture_)
        return;

    ownsCapture_ = false;

    if (hwnd == nullptr || GetCapture() != hwnd)
        return;

    intentionalCaptureRelease_ = true;
    ReleaseCapture();
    intentionalCaptureRelease_ = false;
}

void Win32ControlInputBridge::ClearWindowState() noexcept {
    hwnd_ = nullptr;
    trackingMouseLeave_ = false;
    ownsCapture_ = false;
    intentionalCaptureRelease_ = false;
}

LRESULT CALLBACK Win32ControlInputBridge::SubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR subclassId,
    DWORD_PTR referenceData) noexcept
{
    auto* bridge =
        reinterpret_cast<Win32ControlInputBridge*>(referenceData);

    if (bridge == nullptr) {
        return DefSubclassProc(
            hwnd, message, wParam, lParam);
    }

    if (bridge->hwnd_ == hwnd) {
        switch (message) {
        case WM_MOUSEMOVE:
            bridge->ArmMouseLeave();
            bridge->DispatchMove(
                PointFromLParam(lParam));
            break;

        case WM_LBUTTONDOWN:
            bridge->ArmMouseLeave();

            bridge->DispatchDown(
                PointFromLParam(lParam));

            if (bridge->AnyControlPressed())
                bridge->AcquireCapture();

            break;

        case WM_LBUTTONUP:
            bridge->DispatchUp(
                PointFromLParam(lParam));

            bridge->ReleaseOwnedCapture();
            break;

        case WM_MOUSELEAVE:
            bridge->trackingMouseLeave_ = false;

            if (!bridge->ownsCapture_)
                bridge->DispatchLeave();

            break;

        case WM_CAPTURECHANGED:
            if (!bridge->intentionalCaptureRelease_ &&
                bridge->ownsCapture_)
            {
                bridge->ownsCapture_ = false;
                bridge->trackingMouseLeave_ = false;
                bridge->DispatchLeave();
            }
            break;

        case WM_NCDESTROY:
            bridge->DispatchLeave();
            bridge->ReleaseOwnedCapture();

            RemoveWindowSubclass(
                hwnd,
                &Win32ControlInputBridge::SubclassProc,
                subclassId);

            bridge->buttons_.clear();
            bridge->toggles_.clear();
            bridge->sliders_.clear();
            bridge->ClearWindowState();

            return DefSubclassProc(
                hwnd, message, wParam, lParam);

        default:
            break;
        }
    }

    return DefSubclassProc(
        hwnd, message, wParam, lParam);
}

} // namespace AuroraGlass::Adapters::Win32