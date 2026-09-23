#include "winui/winui_host_bridge.h"
#include "winui/winui_control_input_bridge.h"

#include <Windows.h>

#include <cstdio>

namespace {

int gChecks = 0;
int gFailures = 0;
int gMetricCallbacks = 0;

void Check(
    bool condition,
    const char* name)
{
    ++gChecks;

    if (!condition) {
        ++gFailures;
        std::printf(
            "[FAIL] %s\n",
            name);
    }
}

void MetricsChanged(
    const AuroraGlassWinUIHostMetrics* metrics,
    void*)
{
    if (metrics != nullptr) {
        ++gMetricCallbacks;
    }
}

LRESULT CALLBACK TestWindowProc(
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

LPARAM PointParam(
    int x,
    int y)
{
    return MAKELPARAM(
        static_cast<WORD>(x),
        static_cast<WORD>(y));
}

}

int main()
{
    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    const wchar_t* className =
        L"AuroraGlassP7WinUIMetricsInputFocused";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc =
        TestWindowProc;
    windowClass.hInstance =
        instance;
    windowClass.lpszClassName =
        className;

    Check(
        RegisterClassW(
            &windowClass) != 0,
        "register real Win32 test class");

    HWND hwnd =
        CreateWindowExW(
            0,
            className,
            L"AuroraGlass P7 Slice C",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            480,
            360,
            nullptr,
            nullptr,
            instance,
            nullptr);

    Check(
        hwnd != nullptr,
        "create real HWND");

    if (hwnd == nullptr) {
        std::printf(
            "P7_WINUI_METRICS_INPUT_TESTS: %d checks, %d failures\n",
            gChecks,
            gFailures);

        return 1;
    }

    ShowWindow(
        hwnd,
        SW_SHOW);

    UpdateWindow(
        hwnd);

    SetForegroundWindow(
        hwnd);

    AuroraGlassWinUIHostAttachment* host =
        AuroraGlassWinUIHostCreate();

    Check(
        host != nullptr,
        "create WINUI host ABI handle");

    Check(
        AuroraGlassWinUIHostSetMetricsCallback(
            host,
            MetricsChanged,
            nullptr) ==
            AURORAGLASS_WINUI_HOST_OK,
        "install P5 metrics callback");

    Check(
        AuroraGlassWinUIHostAttach(
            host,
            reinterpret_cast<intptr_t>(
                hwnd)) ==
            AURORAGLASS_WINUI_HOST_OK,
        "attach host metrics to real HWND");

    Check(
        AuroraGlassWinUIHostIsAttached(
            host) == 1,
        "host reports attached");

    AuroraGlassWinUIHostMetrics metrics{};

    Check(
        AuroraGlassWinUIHostGetMetrics(
            host,
            &metrics) ==
            AURORAGLASS_WINUI_HOST_OK,
        "query initial host metrics");

    Check(
        metrics.client_width > 0 &&
        metrics.client_height > 0,
        "initial physical client size valid");

    Check(
        metrics.dpi > 0,
        "initial HWND DPI valid");

    const uint32_t initialWidth =
        metrics.client_width;

    const int callbacksBeforeResize =
        gMetricCallbacks;

    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        640,
        440,
        SWP_NOMOVE |
        SWP_NOZORDER |
        SWP_NOACTIVATE);

    Check(
        AuroraGlassWinUIHostGetMetrics(
            host,
            &metrics) ==
            AURORAGLASS_WINUI_HOST_OK,
        "query metrics after real resize");

    Check(
        metrics.client_width !=
            initialWidth,
        "real WM_SIZE updates P5 width");

    Check(
        gMetricCallbacks >
            callbacksBeforeResize,
        "resize callback crosses WINUI ABI");

    for (int i = 0; i < 8; ++i) {
        SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            650 + i * 7,
            445 + i * 5,
            SWP_NOMOVE |
            SWP_NOZORDER |
            SWP_NOACTIVATE);
    }

    Check(
        AuroraGlassWinUIHostGetMetrics(
            host,
            &metrics) ==
            AURORAGLASS_WINUI_HOST_OK,
        "query after rapid resize sequence");

    Check(
        metrics.client_width > 0 &&
        metrics.client_height > 0,
        "rapid resize leaves usable metrics");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_MINIMIZED,
        MAKELPARAM(
            0,
            0));

    AuroraGlassWinUIHostGetMetrics(
        host,
        &metrics);

    Check(
        metrics.client_width == 0 &&
        metrics.client_height == 0,
        "zero by zero is legal minimized state");

    SendMessageW(
        hwnd,
        WM_SIZE,
        SIZE_RESTORED,
        MAKELPARAM(
            500,
            300));

    AuroraGlassWinUIHostGetMetrics(
        host,
        &metrics);

    Check(
        metrics.client_width == 500 &&
        metrics.client_height == 300,
        "restore metrics propagate");

    const int callbacksBeforeDpi =
        gMetricCallbacks;

    RECT suggestedRect{
        100,
        100,
        740,
        540
    };

    SendMessageW(
        hwnd,
        WM_DPICHANGED,
        MAKELONG(
            144,
            144),
        reinterpret_cast<LPARAM>(
            &suggestedRect));

    AuroraGlassWinUIHostGetMetrics(
        host,
        &metrics);

    Check(
        metrics.dpi == 144,
        "deterministic DPI update reaches P5 metrics");

    Check(
        gMetricCallbacks >
            callbacksBeforeDpi,
        "DPI callback crosses WINUI ABI");

    AuroraGlassWinUIInputBridge* input =
        AuroraGlassWinUIInputCreate();

    Check(
        input != nullptr,
        "create WINUI input ABI handle");

    Check(
        AuroraGlassWinUIInputAttach(
            input,
            reinterpret_cast<intptr_t>(
                hwnd)) ==
            AURORAGLASS_WINUI_INPUT_OK,
        "attach existing P5 input bridge");

    Check(
        AuroraGlassWinUIInputIsAttached(
            input) == 1,
        "input bridge reports attached");

    AuroraGlassWinUIButton* button =
        nullptr;

    AuroraGlassWinUIToggle* toggle =
        nullptr;

    AuroraGlassWinUISlider* slider =
        nullptr;

    Check(
        AuroraGlassWinUIInputAddButton(
            input,
            20.0f,
            20.0f,
            100.0f,
            40.0f,
            &button) ==
            AURORAGLASS_WINUI_INPUT_OK,
        "register frozen P3 GlassButton");

    Check(
        button != nullptr,
        "button handle returned");

    Check(
        AuroraGlassWinUIInputAddToggle(
            input,
            20.0f,
            80.0f,
            100.0f,
            40.0f,
            &toggle) ==
            AURORAGLASS_WINUI_INPUT_OK,
        "register frozen P3 GlassToggle");

    Check(
        toggle != nullptr,
        "toggle handle returned");

    Check(
        AuroraGlassWinUIInputAddSlider(
            input,
            20.0f,
            145.0f,
            220.0f,
            30.0f,
            &slider) ==
            AURORAGLASS_WINUI_INPUT_OK,
        "register frozen P3 GlassSlider");

    Check(
        slider != nullptr,
        "slider handle returned");

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        0,
        PointParam(
            40,
            40));

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(
            40,
            40));

    Check(
        AuroraGlassWinUIButtonIsPressed(
            button) == 1,
        "button press reaches P3 semantic state");

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(
            40,
            40));

    Check(
        AuroraGlassWinUIButtonIsPressed(
            button) == 0,
        "button release clears pressed");

    Check(
        AuroraGlassWinUIButtonClickCount(
            button) == 1,
        "button semantic click fires");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(
            40,
            100));

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(
            40,
            100));

    Check(
        AuroraGlassWinUIToggleIsChecked(
            toggle) == 1,
        "toggle changes off to on");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(
            20,
            160));

    Check(
        AuroraGlassWinUISliderIsPressed(
            slider) == 1,
        "slider press reaches frozen P3");

    SendMessageW(
        hwnd,
        WM_MOUSEMOVE,
        MK_LBUTTON,
        PointParam(
            190,
            160));

    Check(
        AuroraGlassWinUISliderValue(
            slider) > 0.5f,
        "slider drag updates P3 value");

    SendMessageW(
        hwnd,
        WM_LBUTTONUP,
        0,
        PointParam(
            190,
            160));

    Check(
        AuroraGlassWinUISliderIsPressed(
            slider) == 0,
        "slider release clears pressed");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(
            40,
            40));

    Check(
        AuroraGlassWinUIButtonIsPressed(
            button) == 1,
        "button active before capture loss");

    Check(
        AuroraGlassWinUIInputOwnsCapture(
            input) == 1,
        "P5 input bridge owns capture");

    ReleaseCapture();

    Check(
        AuroraGlassWinUIInputOwnsCapture(
            input) == 0,
        "capture loss clears P5 ownership");

    Check(
        AuroraGlassWinUIButtonIsPressed(
            button) == 0,
        "capture loss clears pressed state");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(
            20,
            160));

    Check(
        AuroraGlassWinUISliderIsPressed(
            slider) == 1,
        "slider active before detach");

    AuroraGlassWinUIInputDetach(
        input);

    Check(
        AuroraGlassWinUIInputIsAttached(
            input) == 0,
        "explicit input detach");

    Check(
        AuroraGlassWinUIInputOwnsCapture(
            input) == 0,
        "detach releases capture");

    Check(
        AuroraGlassWinUISliderIsPressed(
            slider) == 0,
        "detach clears slider pressed state");

    Check(
        IsWindow(hwnd) != FALSE,
        "input detach leaves host HWND alive");

    Check(
        AuroraGlassWinUIInputAttach(
            input,
            reinterpret_cast<intptr_t>(
                hwnd)) ==
            AURORAGLASS_WINUI_INPUT_OK,
        "input bridge reattaches");

    AuroraGlassWinUIButton* destroyButton =
        nullptr;

    Check(
        AuroraGlassWinUIInputAddButton(
            input,
            300.0f,
            20.0f,
            100.0f,
            40.0f,
            &destroyButton) ==
            AURORAGLASS_WINUI_INPUT_OK,
        "register fresh button after reattach");

    Check(
        destroyButton != nullptr,
        "fresh reattach button handle returned");

    SendMessageW(
        hwnd,
        WM_LBUTTONDOWN,
        MK_LBUTTON,
        PointParam(
            320,
            40));

    Check(
        AuroraGlassWinUIButtonIsPressed(
            destroyButton) == 1,
        "fresh button active before host destruction");

    Check(
        AuroraGlassWinUIInputOwnsCapture(
            input) == 1,
        "capture active before host destruction");

    Check(
        DestroyWindow(
            hwnd) != FALSE,
        "host destroys its own HWND");

    hwnd =
        nullptr;

    Check(
        AuroraGlassWinUIInputIsAttached(
            input) == 0,
        "WM_NCDESTROY detaches input bridge");

    Check(
        AuroraGlassWinUIInputOwnsCapture(
            input) == 0,
        "WM_NCDESTROY clears input capture");

    Check(
        AuroraGlassWinUIButtonIsPressed(
            destroyButton) == 0,
        "WM_NCDESTROY clears active button state");

    Check(
        AuroraGlassWinUIHostIsAttached(
            host) == 0,
        "WM_NCDESTROY detaches host metrics");

    AuroraGlassWinUIInputDestroy(
        input);

    AuroraGlassWinUIHostDestroy(
        host);

    UnregisterClassW(
        className,
        instance);

    std::printf(
        "P7_WINUI_METRICS_INPUT_TESTS: %d checks, %d failures\n",
        gChecks,
        gFailures);

    return gFailures == 0
        ? 0
        : 1;
}
