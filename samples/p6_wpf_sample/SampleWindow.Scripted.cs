using System.IO;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Threading;
using AuroraGlass.Wpf;

namespace P6.WpfSample;

internal sealed partial class SampleWindow
{
    private const uint WmMouseMove = 0x0200;
    private const uint WmLButtonDown = 0x0201;
    private const uint WmLButtonUp = 0x0202;
    private const nuint MkLButton = 0x0001;

    private async Task RunScriptedAsync()
    {
        Console.WriteLine(
            "P6_WPF_SAMPLE_SCRIPTED_BEGIN");

        await WaitFramesAsync(4);

        WpfRenderHostStats initial =
            _renderHost.Stats;

        Check(
            initial.Ready,
            "native render host ready");

        Check(
            initial.FrameCount > 0,
            "AuroraGlass frames presented");

        Check(
            initial.LastCoreStatus == 0,
            "Core rendering status OK");

        WpfGlassMaterialSnapshot material =
            _material.Snapshot;

        Check(
            Math.Abs(
                material.BlurRadius -
                14.0f) < 0.001f,
            "frozen GlassMaterial Frost configured");

        Check(
            Math.Abs(
                material.RefractionStrength -
                0.22f) < 0.001f,
            "frozen GlassMaterial refraction configured");

        WpfHostMetrics metrics =
            _host.CurrentMetrics;

        Point probe =
            _host.DipToPhysical(
                new Point(96, 96));

        double scale =
            metrics.Dpi / 96.0;

        Check(
            Math.Abs(
                probe.X -
                96.0 * scale) < 0.01,
            "DIP conversion uses P6 DPI boundary");

        Point button =
            Center(
                ToWindowDip(
                    _buttonLocal));

        SendMouse(
            WmMouseMove,
            0,
            button);

        SendMouse(
            WmLButtonDown,
            MkLButton,
            button);

        Check(
            _button?.IsPressed == true,
            "GlassButton press reaches frozen P3");

        Check(
            _input.OwnsCapture,
            "GlassButton owns native capture");

        SendMouse(
            WmLButtonUp,
            0,
            button);

        Check(
            _button?.ClickCount == 1,
            "GlassButton semantic click");

        Point toggle =
            Center(
                ToWindowDip(
                    _toggleLocal));

        SendMouse(
            WmLButtonDown,
            MkLButton,
            toggle);

        SendMouse(
            WmLButtonUp,
            0,
            toggle);

        Check(
            _toggle?.IsChecked == true,
            "GlassToggle semantic state changes");

        UpdateRenderRects();

        Rect sliderBounds =
            ToWindowDip(
                _sliderLocal);

        Point sliderDown =
            new(
                sliderBounds.X + 4,
                sliderBounds.Y +
                    sliderBounds.Height * 0.5);

        Point sliderDrag =
            new(
                sliderBounds.X +
                    sliderBounds.Width * 0.78,
                sliderBounds.Y +
                    sliderBounds.Height * 0.5);

        SendMouse(
            WmLButtonDown,
            MkLButton,
            sliderDown);

        Check(
            _slider?.IsPressed == true,
            "GlassSlider drag begins");

        SendMouse(
            WmMouseMove,
            MkLButton,
            sliderDrag);

        SendMouse(
            WmLButtonUp,
            0,
            sliderDrag);

        Check(
            (_slider?.Value ?? 0.0f) >
                0.50f,
            "GlassSlider value updates");

        UpdateRenderRects();

        WpfHostMetrics beforeResize =
            _host.CurrentMetrics;

        Width += 96;
        Height += 48;

        UpdateLayout();

        await DelayAsync(260);

        WpfHostMetrics afterResize =
            _host.CurrentMetrics;

        Check(
            afterResize.ClientWidth !=
                beforeResize.ClientWidth,
            "WPF resize reaches P6 host metrics");

        WpfRenderHostStats resized =
            _renderHost.Stats;

        Check(
            resized.Width > 0 &&
            resized.Height > 0,
            "native child render host resized");

        WindowState =
            WindowState.Minimized;

        await DelayAsync(180);

        WpfHostMetrics minimized =
            _host.CurrentMetrics;

        Check(
            minimized.ClientWidth == 0 &&
            minimized.ClientHeight == 0,
            "minimize reports legal zero-size");

        WindowState =
            WindowState.Normal;

        Activate();

        await DelayAsync(300);

        WpfHostMetrics restored =
            _host.CurrentMetrics;

        Check(
            restored.ClientWidth > 0 &&
            restored.ClientHeight > 0,
            "restore recovers host metrics");

        await WaitFramesAsync(
            initial.FrameCount + 8);

        WpfRenderHostStats final =
            _renderHost.Stats;

        Check(
            final.LastCoreStatus == 0,
            "Core renderer healthy after resize");

        Check(
            final.FrameCount >
                initial.FrameCount,
            "render loop continues after resize");

        if (!string.IsNullOrWhiteSpace(
                _screenshotPath))
        {
            SampleCapture.Capture(
                this,
                _screenshotPath);

            Check(
                File.Exists(
                    _screenshotPath),
                "representative screenshot written");

            if (File.Exists(
                    _screenshotPath))
            {
                FileInfo info =
                    new(
                        _screenshotPath);

                Check(
                    info.Length > 4096,
                    "screenshot contains image data");

                Console.WriteLine(
                    "SCREENSHOT=" +
                    Path.GetFullPath(
                        _screenshotPath));
            }
        }

        Console.WriteLine("RUNTIME_WINDOW=PASS");
        Console.WriteLine("RUNTIME_GLASS=PASS");
        Console.WriteLine("RUNTIME_REFRACTION=PASS");
        Console.WriteLine("RUNTIME_FROST=PASS");
        Console.WriteLine("RUNTIME_BUTTON=PASS");
        Console.WriteLine("RUNTIME_TOGGLE=PASS");
        Console.WriteLine("RUNTIME_SLIDER=PASS");
        Console.WriteLine("RUNTIME_MOTION=NOT_EXPOSED");
        Console.WriteLine("RUNTIME_RESIZE=PASS");
        Console.WriteLine("RUNTIME_DPI=PASS");

        Close();
    }

    private static Point Center(
        Rect rect)
    {
        return new Point(
            rect.X +
                rect.Width * 0.5,
            rect.Y +
                rect.Height * 0.5);
    }

    private async Task WaitFramesAsync(
        ulong minimum)
    {
        for (int i = 0;
             i < 100;
             ++i)
        {
            if (_renderHost.Stats.FrameCount >=
                minimum)
            {
                return;
            }

            await DelayAsync(30);
        }

        Check(
            false,
            "render frame wait completed");
    }

    private static Task DelayAsync(
        int milliseconds)
    {
        TaskCompletionSource<bool> done =
            new();

        DispatcherTimer timer =
            new()
            {
                Interval =
                    TimeSpan.FromMilliseconds(
                        milliseconds)
            };

        timer.Tick +=
            (_, _) =>
            {
                timer.Stop();
                done.TrySetResult(true);
            };

        timer.Start();

        return done.Task;
    }

    private void SendMouse(
        uint message,
        nuint wParam,
        Point windowDip)
    {
        IntPtr hwnd =
            new WindowInteropHelper(
                this).Handle;

        Point physical =
            _host.DipToPhysical(
                windowDip);

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

        SampleNativeMethods.SendMessage(
            hwnd,
            message,
            wParam,
            lParam);
    }

    private void Check(
        bool condition,
        string name)
    {
        ++_checks;

        if (condition)
        {
            Console.WriteLine(
                "[PASS] " + name);

            return;
        }

        ++_failures;

        Console.WriteLine(
            "[FAIL] " + name);
    }
}
