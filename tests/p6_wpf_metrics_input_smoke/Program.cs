using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;
using AuroraGlass.Wpf;

namespace P6.WpfMetricsInput.Smoke;

internal static class Program
{
    private const uint WmMouseMove = 0x0200;
    private const uint WmLButtonDown = 0x0201;
    private const uint WmLButtonUp = 0x0202;
    private const nuint MkLButton = 0x0001;

    private static int sChecks;
    private static int sFailures;

    [STAThread]
    private static int Main()
    {
        Console.WriteLine(
            "P6_WPF_METRICS_INPUT_SMOKE_BEGIN");

        Application application =
            new()
            {
                ShutdownMode =
                    ShutdownMode.OnExplicitShutdown
            };

        application.Startup +=
            (_, _) =>
            {
                application.Dispatcher.BeginInvoke(
                    DispatcherPriority.ApplicationIdle,
                    new Action(
                        () => Run(application)));
            };

        application.Run();

        Console.WriteLine(
            "P6_WPF_METRICS_INPUT_SMOKE: " +
            sChecks +
            " checks, " +
            sFailures +
            " failures");

        return sFailures == 0
            ? 0
            : 1;
    }

    private static void Run(
        Application application)
    {
        try
        {
            RunScenario(
                application.Dispatcher);
        }
        catch (Exception ex)
        {
            ++sFailures;

            Console.WriteLine(
                "[FAIL] unhandled exception");

            Console.WriteLine(
                ex);
        }
        finally
        {
            application.Shutdown(
                sFailures == 0
                    ? 0
                    : 1);
        }
    }

    private static void RunScenario(
        Dispatcher dispatcher)
    {
        Window window =
            new()
            {
                Title =
                    "AuroraGlass P6 Slice C Runtime Smoke",
                Width = 520,
                Height = 380,
                ShowInTaskbar = false,
                ShowActivated = false
            };

        using WpfHostAttachment host =
            new();

        int metricsEvents = 0;

        host.MetricsChanged +=
            metrics =>
            {
                ++metricsEvents;

                Console.WriteLine(
                    "METRICS " +
                    metrics.ClientWidth +
                    "x" +
                    metrics.ClientHeight +
                    " dpi=" +
                    metrics.Dpi);
            };

        Check(
            !host.Attach(window),
            "Attach before HWND does not pretend success");

        Check(
            !host.IsAttached,
            "host remains detached before SourceInitialized");

        window.Show();

        Pump(
            dispatcher);

        Check(
            host.IsAttached,
            "SourceInitialized attaches native host");

        IntPtr hwnd =
            new WindowInteropHelper(
                window).Handle;

        Check(
            hwnd != IntPtr.Zero,
            "real WPF HWND obtained");

        Check(
            NativeMethods.IsWindow(hwnd),
            "real WPF HWND is live");

        WpfHostMetrics initial =
            host.CurrentMetrics;

        Check(
            initial.ClientWidth > 0 &&
            initial.ClientHeight > 0,
            "initial native client metrics valid");

        Check(
            initial.Dpi > 0,
            "initial native HWND DPI valid");

        DpiScale wpfDpi =
            VisualTreeHelper.GetDpi(
                window);

        Check(
            wpfDpi.PixelsPerDip > 0.0,
            "WPF DPI information available");

        Point dipPoint =
            new(
                10.0,
                10.0);

        Point physicalPoint =
            host.DipToPhysical(
                dipPoint);

        double nativeScale =
            initial.Dpi / 96.0;

        Check(
            Math.Abs(
                physicalPoint.X -
                dipPoint.X *
                nativeScale) < 0.01,
            "DIP conversion follows native HWND DPI truth");

        uint widthBefore =
            initial.ClientWidth;

        int eventsBeforeResize =
            metricsEvents;

        window.Width +=
            120;

        window.Height +=
            70;

        window.UpdateLayout();

        Pump(
            dispatcher);

        WpfHostMetrics resized =
            host.CurrentMetrics;

        Check(
            resized.ClientWidth !=
                widthBefore,
            "WPF resize updates native client metrics");

        Check(
            metricsEvents >
                eventsBeforeResize,
            "MetricsChanged observes native resize");

        for (int i = 0; i < 6; ++i)
        {
            window.Width +=
                9;

            window.Height +=
                6;

            window.UpdateLayout();
        }

        Pump(
            dispatcher);

        WpfHostMetrics rapid =
            host.CurrentMetrics;

        Check(
            rapid.ClientWidth > 0 &&
            rapid.ClientHeight > 0,
            "rapid resize leaves valid metrics");

        window.WindowState =
            WindowState.Minimized;

        Pump(
            dispatcher);

        WpfHostMetrics minimized =
            host.CurrentMetrics;

        Check(
            minimized.ClientWidth == 0 &&
            minimized.ClientHeight == 0,
            "minimize preserves legal zero-size state");

        window.WindowState =
            WindowState.Normal;

        Pump(
            dispatcher);

        WpfHostMetrics restored =
            host.CurrentMetrics;

        Check(
            restored.ClientWidth > 0 &&
            restored.ClientHeight > 0,
            "restore recovers client metrics");

        using WpfControlInputBridge input =
            new();

        Check(
            input.Attach(host) ==
                WpfInputStatus.Ok,
            "input bridge attaches to same WPF HWND");

        Check(
            input.IsAttached,
            "managed input bridge reports attached");

        WpfControlInputBridge.WpfButton button =
            input.AddButton(
                new Rect(
                    20,
                    20,
                    100,
                    40));

        WpfControlInputBridge.WpfToggle toggle =
            input.AddToggle(
                new Rect(
                    20,
                    80,
                    100,
                    40));

        WpfControlInputBridge.WpfSlider slider =
            input.AddSlider(
                new Rect(
                    20,
                    145,
                    220,
                    30));

        Point buttonPhysical =
            host.DipToPhysical(
                new Point(
                    40,
                    40));

        SendMouse(
            hwnd,
            WmMouseMove,
            0,
            buttonPhysical);

        SendMouse(
            hwnd,
            WmLButtonDown,
            MkLButton,
            buttonPhysical);

        Check(
            button.IsPressed,
            "button press reaches frozen P3 semantics");

        Check(
            input.OwnsCapture,
            "P5 owns capture during button press");

        SendMouse(
            hwnd,
            WmLButtonUp,
            0,
            buttonPhysical);

        Check(
            !button.IsPressed,
            "button release clears pressed state");

        Check(
            button.ClickCount == 1,
            "button semantic click occurs");

        Point togglePhysical =
            host.DipToPhysical(
                new Point(
                    40,
                    100));

        SendMouse(
            hwnd,
            WmLButtonDown,
            MkLButton,
            togglePhysical);

        SendMouse(
            hwnd,
            WmLButtonUp,
            0,
            togglePhysical);

        Check(
            toggle.IsChecked,
            "toggle changes off to on");

        Point sliderDown =
            host.DipToPhysical(
                new Point(
                    20,
                    160));

        Point sliderDrag =
            host.DipToPhysical(
                new Point(
                    190,
                    160));

        SendMouse(
            hwnd,
            WmLButtonDown,
            MkLButton,
            sliderDown);

        Check(
            slider.IsPressed,
            "slider press reaches frozen P3 semantics");

        Check(
            input.OwnsCapture,
            "P5 owns capture during slider interaction");

        SendMouse(
            hwnd,
            WmMouseMove,
            MkLButton,
            sliderDrag);

        Check(
            slider.Value > 0.5f,
            "slider drag updates frozen P3 value");

        SendMouse(
            hwnd,
            WmLButtonUp,
            0,
            sliderDrag);

        Check(
            !slider.IsPressed,
            "slider release clears pressed state");

        Point teardownPress =
            host.DipToPhysical(
                new Point(
                    40,
                    40));

        SendMouse(
            hwnd,
            WmLButtonDown,
            MkLButton,
            teardownPress);

        Check(
            button.IsPressed,
            "active press established before adapter dispose");

        Check(
            input.OwnsCapture,
            "capture active before adapter dispose");

        input.Dispose();

        Check(
            NativeMethods.IsWindow(hwnd),
            "input adapter dispose leaves WPF HWND alive");

        Check(
            window.IsVisible,
            "input adapter dispose leaves WPF Window alive");

        window.Close();

        Pump(
            dispatcher);

        Check(
            !NativeMethods.IsWindow(hwnd),
            "WPF owns final HWND destruction");

        Check(
            !host.IsAttached,
            "Window close detaches native host");
    }

    private static void Pump(
        Dispatcher dispatcher)
    {
        dispatcher.Invoke(
            () => { },
            DispatcherPriority.ApplicationIdle);
    }

    private static void SendMouse(
        IntPtr hwnd,
        uint message,
        nuint wParam,
        Point physical)
    {
        int x =
            (int)Math.Round(
                physical.X);

        int y =
            (int)Math.Round(
                physical.Y);

        nint lParam =
            (nint)(
                ((y & 0xffff) << 16) |
                (x & 0xffff));

        NativeMethods.SendMessage(
            hwnd,
            message,
            wParam,
            lParam);
    }

    private static void Check(
        bool condition,
        string name)
    {
        ++sChecks;

        if (condition)
        {
            Console.WriteLine(
                "[PASS] " +
                name);

            return;
        }

        ++sFailures;

        Console.WriteLine(
            "[FAIL] " +
            name);
    }

    private static class NativeMethods
    {
        [DllImport(
            "user32.dll",
            EntryPoint = "SendMessageW")]
        private static extern nint
            SendMessageNative(
                IntPtr hwnd,
                uint message,
                nuint wParam,
                nint lParam);

        [DllImport(
            "user32.dll",
            EntryPoint = "IsWindow")]
        private static extern bool
            IsWindowNative(
                IntPtr hwnd);

        internal static void SendMessage(
            IntPtr hwnd,
            uint message,
            nuint wParam,
            nint lParam)
        {
            SendMessageNative(
                hwnd,
                message,
                wParam,
                lParam);
        }

        internal static bool IsWindow(
            IntPtr hwnd)
        {
            return hwnd != IntPtr.Zero &&
                IsWindowNative(hwnd);
        }
    }
}
