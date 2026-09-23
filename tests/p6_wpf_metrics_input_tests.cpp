#include "wpf/wpf_host_bridge.h"
#include "wpf/wpf_input_bridge.h"

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
    const AuroraGlassWpfHostMetrics* metrics,
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
        L"AuroraGlassP6WpfMetricsInputFocused";

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
            L"AuroraGlass P6 Slice C",
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
            "P6_WPF_METRICS_INPUT_TESTS: %d checks, %d failures\n",
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

    AuroraGlassWpfHostAttachment* host =
        AuroraGlassWpfHostCreate();

    Check(
        host != nullptr,
        "create WPF host ABI handle");

    Check(
        AuroraGlassWpfHostSetMetricsCallback(
            host,
            MetricsChanged,
            nullptr) ==
            AURORAGLASS_WPF_HOST_OK,
        "install P5 metrics callback");

    Check(
        AuroraGlassWpfHostAttach(
            host,
            reinterpret_cast<intptr_t>(
                hwnd)) ==
            AURORAGLASS_WPF_HOST_OK,
        "attach host metrics to real HWND");

    Check(
        AuroraGlassWpfHostIsAttached(
            host) == 1,
        "host reports attached");

    AuroraGlassWpfHostMetrics metrics{};

    Check(
        AuroraGlassWpfHostGetMetrics(
            host,
            &metrics) ==
            AURORAGLASS_WPF_HOST_OK,
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
        AuroraGlassWpfHostGetMetrics(
            host,
            &metrics) ==
            AURORAGLASS_WPF_HOST_OK,
        "query metrics after real resize");

    Check(
        metrics.client_width !=
            initialWidth,
        "real WM_SIZE updates P5 width");

    Check(
        gMetricCallbacks >
            callbacksBeforeResize,
        "resize callback crosses WPF ABI");

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
        AuroraGlassWpfHostGetMetrics(
            host,
            &metrics) ==
            AURORAGLASS_WPF_HOST_OK,
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

    AuroraGlassWpfHostGetMetrics(
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

    AuroraGlassWpfHostGetMetrics(
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

    AuroraGlassWpfHostGetMetrics(
        host,
        &metrics);

    Check(
        metrics.dpi == 144,
        "deterministic DPI update reaches P5 metrics");

    Check(
        gMetricCallbacks >
            callbacksBeforeDpi,
        "DPI callback crosses WPF ABI");

    AuroraGlassWpfInputBridge* input =
        AuroraGlassWpfInputCreate();

    Check(
        input != nullptr,
        "create WPF input ABI handle");

    Check(
        AuroraGlassWpfInputAttach(
            input,
            reinterpret_cast<intptr_t>(
                hwnd)) ==
            AURORAGLASS_WPF_INPUT_OK,
        "attach existing P5 input bridge");

    Check(
        AuroraGlassWpfInputIsAttached(
            input) == 1,
        "input bridge reports attached");

    AuroraGlassWpfButton* button =
        nullptr;

    AuroraGlassWpfToggle* toggle =
        nullptr;

    AuroraGlassWpfSlider* slider =
        nullptr;

    Check(
        AuroraGlassWpfInputAddButton(
            input,
            20.0f,
            20.0f,
            100.0f,
            40.0f,
            &button) ==
            AURORAGLASS_WPF_INPUT_OK,
        "register frozen P3 GlassButton");

    Check(
        button != nullptr,
        "button handle returned");

    Check(
        AuroraGlassWpfInputAddToggle(
            input,
            20.0f,
            80.0f,
            100.0f,
            40.0f,
            &toggle) ==
            AURORAGLASS_WPF_INPUT_OK,
        "register frozen P3 GlassToggle");

    Check(
        toggle != nullptr,
        "toggle handle returned");

    Check(
        AuroraGlassWpfInputAddSlider(
            input,
            20.0f,
            145.0f,
            220.0f,
            30.0f,
            &slider) ==
            AURORAGLASS_WPF_INPUT_OK,
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
        AuroraGlassWpfButtonIsPressed(
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
        AuroraGlassWpfButtonIsPressed(
            button) == 0,
        "button release clears pressed");

    Check(
        AuroraGlassWpfButtonClickCount(
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
        AuroraGlassWpfToggleIsChecked(
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
        AuroraGlassWpfSliderIsPressed(
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
        AuroraGlassWpfSliderValue(
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
        AuroraGlassWpfSliderIsPressed(
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
        AuroraGlassWpfButtonIsPressed(
            button) == 1,
        "button active before capture loss");

    Check(
        AuroraGlassWpfInputOwnsCapture(
            input) == 1,
        "P5 input bridge owns capture");

    ReleaseCapture();

    Check(
        AuroraGlassWpfInputOwnsCapture(
            input) == 0,
        "capture loss clears P5 ownership");

    Check(
        AuroraGlassWpfButtonIsPressed(
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
        AuroraGlassWpfSliderIsPressed(
            slider) == 1,
        "slider active before detach");

    AuroraGlassWpfInputDetach(
        input);

    Check(
        AuroraGlassWpfInputIsAttached(
            input) == 0,
        "explicit input detach");

    Check(
        AuroraGlassWpfInputOwnsCapture(
            input) == 0,
        "detach releases capture");

    Check(
        AuroraGlassWpfSliderIsPressed(
            slider) == 0,
        "detach clears slider pressed state");

    Check(
        IsWindow(hwnd) != FALSE,
        "input detach leaves host HWND alive");

    Check(
        AuroraGlassWpfInputAttach(
            input,
            reinterpret_cast<intptr_t>(
                hwnd)) ==
            AURORAGLASS_WPF_INPUT_OK,
        "input bridge reattaches");

    AuroraGlassWpfButton* destroyButton =
        nullptr;

    Check(
        AuroraGlassWpfInputAddButton(
            input,
            300.0f,
            20.0f,
            100.0f,
            40.0f,
            &destroyButton) ==
            AURORAGLASS_WPF_INPUT_OK,
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
        AuroraGlassWpfButtonIsPressed(
            destroyButton) == 1,
        "fresh button active before host destruction");

    Check(
        AuroraGlassWpfInputOwnsCapture(
            input) == 1,
        "capture active before host destruction");

    Check(
        DestroyWindow(
            hwnd) != FALSE,
        "host destroys its own HWND");

    hwnd =
        nullptr;

    Check(
        AuroraGlassWpfInputIsAttached(
            input) == 0,
        "WM_NCDESTROY detaches input bridge");

    Check(
        AuroraGlassWpfInputOwnsCapture(
            input) == 0,
        "WM_NCDESTROY clears input capture");

    Check(
        AuroraGlassWpfButtonIsPressed(
            destroyButton) == 0,
        "WM_NCDESTROY clears active button state");

    Check(
        AuroraGlassWpfHostIsAttached(
            host) == 0,
        "WM_NCDESTROY detaches host metrics");

    AuroraGlassWpfInputDestroy(
        input);

    AuroraGlassWpfHostDestroy(
        host);

    UnregisterClassW(
        className,
        instance);

    std::printf(
        "P6_WPF_METRICS_INPUT_TESTS: %d checks, %d failures\n",
        gChecks,
        gFailures);

    return gFailures == 0
        ? 0
        : 1;
}
