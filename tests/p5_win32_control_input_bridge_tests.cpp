#include "win32/win32_control_input_bridge.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>

using AuroraGlass::Adapters::Win32::Win32ControlInputBridge;
using AuroraGlass::ControlInteractionState;
using AuroraGlass::GlassButton;
using AuroraGlass::GlassSlider;
using AuroraGlass::GlassToggle;

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

bool Near(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) <= epsilon;
}

LPARAM PointParam(int x, int y) {
    return MAKELPARAM(
        static_cast<WORD>(static_cast<short>(x)),
        static_cast<WORD>(static_cast<short>(y)));
}

LRESULT CALLBACK TestWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return DefWindowProcW(
        hwnd, message, wParam, lParam);
}

} // namespace

int main() {
    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    const wchar_t* className =
        L"AuroraGlass.P5.ControlInputBridge.Test";

    WNDCLASSW wc{};
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;

    Check(
        RegisterClassW(&wc) != 0,
        "register real HWND class");

    HWND hwnd =
        CreateWindowExW(
            0,
            className,
            L"AuroraGlass P5 Input",
            WS_OVERLAPPEDWINDOW,
            100, 100, 640, 480,
            nullptr, nullptr,
            instance, nullptr);

    HWND interrupter =
        CreateWindowExW(
            0,
            className,
            L"AuroraGlass P5 Capture Interrupter",
            WS_OVERLAPPEDWINDOW,
            800, 100, 320, 240,
            nullptr, nullptr,
            instance, nullptr);

    Check(
        hwnd != nullptr &&
        interrupter != nullptr,
        "create real HWNDs");

    if (hwnd == nullptr || interrupter == nullptr)
        return 1;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    GlassButton button;
    button.bounds = { 20.0f, 20.0f, 120.0f, 40.0f };

    GlassToggle toggle;
    toggle.bounds = { 20.0f, 80.0f, 120.0f, 40.0f };

    GlassSlider slider;
    slider.bounds = { 20.0f, 150.0f, 200.0f, 24.0f };

    int buttonClicks = 0;
    int toggleChanges = 0;
    int sliderChanges = 0;

    button.onClick = [&]() {
        ++buttonClicks;
        std::printf("Button clicked\n");
    };

    toggle.onChanged = [&](bool checked) {
        ++toggleChanges;
        std::printf(
            "Toggle changed=%s\n",
            checked ? "true" : "false");
    };

    slider.onValueChanged = [&](float value) {
        ++sliderChanges;
        std::printf(
            "Slider value changed=%.3f\n",
            value);
    };

    Win32ControlInputBridge bridge;

    Check(bridge.AddButton(button), "register button");
    Check(bridge.AddToggle(toggle), "register toggle");
    Check(bridge.AddSlider(slider), "register slider");
    Check(bridge.AddButton(button), "duplicate registration idempotent");
    Check(bridge.Attach(hwnd), "attach real HWND");
    Check(bridge.IsAttached(), "attached state");

    SendMessageW(hwnd, WM_MOUSEMOVE, 0, PointParam(30, 30));

    Check(
        button.State() == ControlInteractionState::Hover,
        "hover enter");

    SendMessageW(hwnd, WM_MOUSEMOVE, 0, PointParam(60, 30));

    Check(
        button.State() == ControlInteractionState::Hover,
        "hover movement");

    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);

    Check(
        button.State() == ControlInteractionState::Normal,
        "mouse leave clears hover");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 40));

    Check(
        button.State() == ControlInteractionState::Pressed,
        "button pressed");

    Check(
        bridge.OwnsCapture() &&
        GetCapture() == hwnd,
        "button capture started");

    std::printf("capture started\n");

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(40, 40));

    Check(buttonClicks == 1, "button clicked");

    Check(
        !bridge.OwnsCapture() &&
        GetCapture() != hwnd,
        "button capture ended");

    std::printf("capture ended\n");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 40));

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(300, 300));

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(300, 300));

    Check(
        buttonClicks == 1,
        "button outside release no click");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 100));

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(40, 100));

    Check(toggle.IsChecked(), "toggle on");
    Check(toggleChanges == 1, "toggle callback");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 100));

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(40, 100));

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 100));

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(40, 100));

    Check(toggle.IsChecked(), "toggle rapid final state");
    Check(toggleChanges == 3, "toggle rapid callback count");

    Check(Near(slider.Value(), 0.0f), "slider initial value");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(20, 162));

    Check(
        slider.State() == ControlInteractionState::Pressed,
        "slider pressed");

    Check(
        bridge.OwnsCapture() &&
        GetCapture() == hwnd,
        "slider capture");

    int beforeDrag = sliderChanges;

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(120, 162));

    Check(Near(slider.Value(), 0.5f), "slider direct value");
    Check(sliderChanges > beforeDrag, "slider callback");

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(220, 162));

    Check(Near(slider.Value(), 1.0f), "slider maximum");

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(220, 162));

    Check(
        !bridge.OwnsCapture(),
        "slider release capture");

    slider.SetValue(0.0f);

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(20, 162));

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(100, 162));

    float beforeLoss = slider.Value();

    SetCapture(interrupter);

    Check(
        GetCapture() == interrupter,
        "external capture acquired");

    Check(
        !bridge.OwnsCapture(),
        "bridge observes capture loss");

    Check(
        slider.State() != ControlInteractionState::Pressed,
        "capture loss clears slider pressed");

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(200, 162));

    Check(
        Near(slider.Value(), beforeLoss),
        "capture loss prevents stale drag");

    ReleaseCapture();

    slider.SetValue(0.0f);

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(20, 162));

    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);

    Check(
        slider.State() == ControlInteractionState::Pressed,
        "leave during capture keeps drag");

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(160, 162));

    Check(
        slider.Value() > 0.6f,
        "drag continues after leave");

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(160, 162));

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 40));

    Check(
        bridge.OwnsCapture(),
        "detach setup capture");

    int beforeDetach = buttonClicks;

    bridge.Detach();

    Check(!bridge.IsAttached(), "detach clears HWND");
    Check(!bridge.OwnsCapture(), "detach releases capture");

    Check(
        button.State() != ControlInteractionState::Pressed,
        "detach clears pressed");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(40, 40));

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(40, 40));

    Check(
        buttonClicks == beforeDetach,
        "no input after detach");

    Check(bridge.AddButton(button), "rebind button");
    Check(bridge.AddToggle(toggle), "rebind toggle");
    Check(bridge.AddSlider(slider), "rebind slider");
    Check(bridge.Attach(hwnd), "reattach");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(20, 162));

    Check(
        bridge.OwnsCapture() &&
        GetCapture() == hwnd,
        "destroy setup capture");

    int b0 = buttonClicks;
    int t0 = toggleChanges;
    int s0 = sliderChanges;

    DestroyWindow(hwnd);

    Check(!bridge.IsAttached(), "destroy clears HWND");
    Check(!bridge.OwnsCapture(), "destroy clears capture");

    Check(
        button.State() != ControlInteractionState::Pressed &&
        slider.State() != ControlInteractionState::Pressed,
        "destroy clears transient state");

    Check(
        buttonClicks == b0 &&
        toggleChanges == t0 &&
        sliderChanges == s0,
        "destroy no semantic callback");

    DestroyWindow(interrupter);

    Check(
        UnregisterClassW(className, instance) != FALSE,
        "unregister class");

    std::printf(
        "P5_WIN32_CONTROL_INPUT_BRIDGE_TESTS=%s checks=%d failures=%d\n",
        g_failures == 0 ? "PASS" : "FAIL",
        g_checks,
        g_failures);

    return g_failures == 0 ? 0 : 1;
}