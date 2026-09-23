using AuroraGlass.WinUI;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Runtime.InteropServices;
using Windows.Foundation;
using WinRT.Interop;

namespace P7.WinUIHostLifecycleSmoke;

internal sealed class SmokeApplication : Application
{
    private const uint WM_MOUSEMOVE = 0x0200;
    private const uint WM_LBUTTONDOWN = 0x0201;
    private const uint WM_LBUTTONUP = 0x0202;
    private const nuint MK_LBUTTON = 0x0001;

    private const uint SWP_NOMOVE = 0x0002;
    private const uint SWP_NOZORDER = 0x0004;
    private const uint SWP_NOACTIVATE = 0x0010;

    private readonly Action<int> setExitCode;

    private Window? window;
    private Grid? root;
    private WinUIHostAttachment? host;
    private WinUIControlInputBridge? input;
    private WinUIControlInputBridge.WinUIButton? closeButton;
    private nint hwnd;

    private int checks;
    private int failures;
    private int metricsEvents;

    internal SmokeApplication(
        Action<int> setExitCode)
    {
        this.setExitCode =
            setExitCode;
    }

    protected override void OnLaunched(
        LaunchActivatedEventArgs args)
    {
        try
        {
            window =
                new Window();

            window.Title =
                "AuroraGlass P7 WinUI 3 Slice B Smoke";

            root =
                new Grid();

            root.Loaded +=
                OnRootLoaded;

            window.Content =
                root;

            window.Activate();

            hwnd =
                WindowNative.GetWindowHandle(
                    window);

            Check(
                hwnd != 0,
                "real WinUI 3 HWND obtained");

            Check(
                IsWindow(hwnd),
                "WinUI 3 HWND is live");

            host =
                new WinUIHostAttachment();

            host.MetricsChanged +=
                _ => ++metricsEvents;

            host.Attach(
                window);

            Check(
                host.IsAttached,
                "P5 host attachment valid");

            WinUIHostMetrics initial =
                host.CurrentMetrics;

            Check(
                initial.ClientWidth > 0 &&
                initial.ClientHeight > 0,
                "initial native metrics valid");

            Check(
                initial.Dpi >= 96,
                "initial authoritative HWND DPI valid");


        }
        catch (Exception ex)
        {
            FailFatal(
                ex);
        }
    }

    private void OnRootLoaded(
        object sender,
        RoutedEventArgs args)
    {
        if (root is null)
        {
            FailFatal(
                new InvalidOperationException(
                    "WinUI root was not available at Loaded."));
            return;
        }

        root.Loaded -=
            OnRootLoaded;

        DispatcherQueue queue =
            DispatcherQueue.GetForCurrentThread();

        if (!queue.TryEnqueue(
                RunSliceB))
        {
            FailFatal(
                new InvalidOperationException(
                    "Failed to enqueue Slice B runtime after Loaded."));
        }
    }
    private void RunSliceB()
    {
        try
        {
            if (window is null ||
                root is null ||
                host is null)
            {
                throw new InvalidOperationException(
                    "Smoke state is incomplete.");
            }

            WinUIHostMetrics beforeResize =
                host.CurrentMetrics;

            bool resized =
                SetWindowPos(
                    hwnd,
                    0,
                    0,
                    0,
                    760,
                    520,
                    SWP_NOMOVE |
                    SWP_NOZORDER |
                    SWP_NOACTIVATE);

            Check(
                resized,
                "real WinUI HWND resize requested");

            WinUIHostMetrics afterResize =
                host.CurrentMetrics;

            Check(
                afterResize.ClientWidth > 0 &&
                afterResize.ClientHeight > 0 &&
                (
                    afterResize.ClientWidth != beforeResize.ClientWidth ||
                    afterResize.ClientHeight != beforeResize.ClientHeight
                ),
                "resize reaches P5 host metrics");

            for (int index = 0;
                 index < 4;
                 ++index)
            {
                SetWindowPos(
                    hwnd,
                    0,
                    0,
                    0,
                    700 + index * 17,
                    480 + index * 13,
                    SWP_NOMOVE |
                    SWP_NOZORDER |
                    SWP_NOACTIVATE);
            }

            Check(
                metricsEvents > 0,
                "resize callback reaches managed adapter");

            WinUIHostMetrics metrics =
                host.CurrentMetrics;

            double nativeScale =
                metrics.LogicalToPhysicalScale;

            double xamlScale =
                root.XamlRoot?.RasterizationScale ??
                0.0;

            Check(
                xamlScale > 0.0,
                "WinUI XamlRoot rasterization scale available");

            Check(
                Math.Abs(
                    nativeScale -
                    xamlScale) < 0.05,
                "WinUI scale agrees with authoritative HWND DPI");

            Rect logicalBounds =
                new Rect(
                    20,
                    20,
                    120,
                    40);

            Rect physicalBounds =
                host.LogicalToPhysical(
                    logicalBounds);

            Check(
                Math.Abs(
                    physicalBounds.Width -
                    logicalBounds.Width *
                    nativeScale) < 0.01,
                "single logical to physical conversion boundary");

            input =
                new WinUIControlInputBridge();

            WinUIInputStatus attachStatus =
                input.Attach(
                    host);

            Check(
                attachStatus ==
                WinUIInputStatus.Ok,
                "P5 input bridge attached to real WinUI HWND");

            WinUIControlInputBridge.WinUIButton button =
                input.AddButton(
                    new Rect(
                        20,
                        20,
                        120,
                        40));

            WinUIControlInputBridge.WinUIToggle toggle =
                input.AddToggle(
                    new Rect(
                        20,
                        80,
                        120,
                        40));

            WinUIControlInputBridge.WinUISlider slider =
                input.AddSlider(
                    new Rect(
                        20,
                        140,
                        220,
                        40));

            Point buttonPoint =
                host.LogicalToPhysical(
                    new Point(
                        40,
                        40));

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                buttonPoint);

            Check(
                button.IsPressed,
                "button press reaches frozen P3");

            Check(
                input.OwnsCapture,
                "button press owns native capture");

            SendMouse(
                WM_LBUTTONUP,
                0,
                buttonPoint);

            Check(
                !button.IsPressed &&
                button.ClickCount == 1,
                "button release produces semantic click");

            Point togglePoint =
                host.LogicalToPhysical(
                    new Point(
                        40,
                        100));

            Check(
                !toggle.IsChecked,
                "toggle starts off");

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                togglePoint);

            SendMouse(
                WM_LBUTTONUP,
                0,
                togglePoint);

            Check(
                toggle.IsChecked,
                "toggle press release reaches frozen state change");

            Point sliderStart =
                host.LogicalToPhysical(
                    new Point(
                        30,
                        160));

            Point sliderEnd =
                host.LogicalToPhysical(
                    new Point(
                        210,
                        160));

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                sliderStart);

            Check(
                slider.IsPressed,
                "slider drag begins");

            SendMouse(
                WM_MOUSEMOVE,
                MK_LBUTTON,
                sliderEnd);

            Check(
                slider.Value > 0.5f,
                "slider drag updates frozen P3 value");

            SendMouse(
                WM_LBUTTONUP,
                0,
                sliderEnd);

            Check(
                !slider.IsPressed &&
                !input.OwnsCapture,
                "slider release clears pressed state and capture");

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                buttonPoint);

            Check(
                button.IsPressed &&
                input.OwnsCapture,
                "active input established before capture loss");

            ReleaseCapture();

            Check(
                !button.IsPressed &&
                !input.OwnsCapture,
                "capture loss clears active control state");

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                sliderStart);

            Check(
                slider.IsPressed &&
                input.OwnsCapture,
                "active slider established before explicit detach");

            input.Detach();

            Check(
                !input.IsAttached &&
                !input.OwnsCapture &&
                GetCapture() != hwnd,
                "explicit input detach releases capture");

            input.Dispose();
            input = null;

            input =
                new WinUIControlInputBridge();

            Check(
                input.Attach(host) ==
                WinUIInputStatus.Ok,
                "input bridge can reattach after cleanup");

            WinUIControlInputBridge.WinUIButton teardownButton =
                input.AddButton(
                    new Rect(
                        20,
                        20,
                        120,
                        40));

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                buttonPoint);

            Check(
                teardownButton.IsPressed &&
                input.OwnsCapture,
                "active input established before Dispose");

            input.Dispose();
            input = null;

            Check(
                GetCapture() != hwnd,
                "Dispose while pressed releases native capture");

            input =
                new WinUIControlInputBridge();

            Check(
                input.Attach(host) ==
                WinUIInputStatus.Ok,
                "input bridge attaches for Window-close teardown");

            closeButton =
                input.AddButton(
                    new Rect(
                        20,
                        20,
                        120,
                        40));

            SendMouse(
                WM_LBUTTONDOWN,
                MK_LBUTTON,
                buttonPoint);

            Check(
                closeButton.IsPressed &&
                input.OwnsCapture,
                "active input established before Window close");

            window.Close();

            Check(
                input is not null &&
                !input.IsAttached &&
                !input.OwnsCapture,
                "WM_NCDESTROY detaches input and releases capture");

            Check(
                closeButton is not null &&
                !closeButton.IsPressed,
                "WM_NCDESTROY clears frozen P3 pressed state");

            input?.Dispose();
            input = null;
            closeButton = null;

            Check(
                host is not null &&
                !host.IsAttached,
                "Window destruction detaches P5 host attachment");

            host?.Dispose();
            host = null;

            Check(
                GetCapture() != hwnd,
                "Window destruction leaves no native capture");

            Finish();
        }
        catch (Exception ex)
        {
            FailFatal(
                ex);
        }
    }

    private void Finish()
    {
        Console.WriteLine(
            "P7_WINUI3_SLICE_B_SMOKE: " +
            checks +
            " checks, " +
            failures +
            " failures");

        setExitCode(
            failures == 0
                ? 0
                : 1);
    }
    private void SendMouse(
        uint message,
        nuint wParam,
        Point point)
    {
        int x =
            (int)Math.Round(
                point.X);

        int y =
            (int)Math.Round(
                point.Y);

        nint lParam =
            MakeLParam(
                x,
                y);

        SendMessage(
            hwnd,
            message,
            wParam,
            lParam);
    }

    private static nint MakeLParam(
        int x,
        int y)
    {
        int packed =
            (y << 16) |
            (x & 0xffff);

        return packed;
    }

    private void Check(
        bool condition,
        string name)
    {
        ++checks;

        if (condition)
        {
            Console.WriteLine(
                "[PASS] " +
                name);

            return;
        }

        ++failures;

        Console.WriteLine(
            "[FAIL] " +
            name);
    }

    private void FailFatal(
        Exception ex)
    {
        ++failures;

        Console.WriteLine(
            "[FAIL] " +
            ex);

        Console.WriteLine(
            "P7_WINUI3_SLICE_B_SMOKE: " +
            checks +
            " checks, " +
            failures +
            " failures");

        Environment.Exit(1);
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWindow(
        nint hwnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowPos(
        nint hwnd,
        nint hwndInsertAfter,
        int x,
        int y,
        int cx,
        int cy,
        uint flags);

    [DllImport("user32.dll")]
    private static extern nint SendMessage(
        nint hwnd,
        uint message,
        nuint wParam,
        nint lParam);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ReleaseCapture();

    [DllImport("user32.dll")]
    private static extern nint GetCapture();
}