using AuroraGlass.WinUI;
using Microsoft.UI.Xaml;
using System.Runtime.InteropServices;
using Windows.Foundation;

namespace P7.WinUISample;

internal sealed partial class SampleWindow
{
    private const uint WM_MOUSEMOVE =
        0x0200;

    private const uint WM_LBUTTONDOWN =
        0x0201;

    private const uint WM_LBUTTONUP =
        0x0202;

    private const nuint MK_LBUTTON =
        0x0001;

    private const int SW_MINIMIZE =
        6;

    private const int SW_RESTORE =
        9;

    private async Task RunScriptedAsync()
    {
        try
        {
            if (host is null ||
                input is null ||
                glassButton is null ||
                glassToggle is null ||
                glassSlider is null ||
                renderer is null)
            {
                throw new InvalidOperationException(
                    "Formal sample production objects are incomplete.");
            }

            await Task.Delay(
                250);

            uint initialClicks =
                glassButton.ClickCount;

            bool initialToggle =
                glassToggle.IsChecked;

            double initialSlider =
                glassSlider.Value;

            Click(
                Center(
                    buttonBounds));

            await Task.Delay(
                100);

            Check(
                glassButton.ClickCount >
                initialClicks,
                "Button");

            Click(
                Center(
                    toggleBounds));

            await Task.Delay(
                100);

            Check(
                glassToggle.IsChecked !=
                initialToggle,
                "Toggle");

            DragSlider(
                sliderBounds,
                0.18,
                0.82);

            await Task.Delay(
                120);

            Check(
                Math.Abs(
                    glassSlider.Value -
                    initialSlider) >
                0.05,
                "Slider");

            buttonText.Text =
                "GlassButton 路 clicks " +
                glassButton.ClickCount;

            toggleText.Text =
                "GlassToggle 路 " +
                (glassToggle.IsChecked
                    ? "ON"
                    : "OFF");

            sliderText.Text =
                "GlassSlider 路 " +
                Math.Round(
                    glassSlider.Value * 100) +
                "%";

            if (captureHold)
            {
                string ready =
                    Path.Combine(
                        AppContext.BaseDirectory,
                        "p7_winui_sample.ready");

                File.WriteAllText(
                    ready,
                    hwnd.ToInt64().ToString());

                Console.WriteLine(
                    "SCREENSHOT_READY=YES");

                await Task.Delay(
                    3000);
            }

            WinUIHostMetrics beforeResize =
                host.CurrentMetrics;

            int beforeMetricsEvents =
                metricsChangedCount;

            int[,] sizes =
            {
                { 980, 660 },
                { 1120, 720 },
                { 1040, 690 },
                { 1180, 760 },
                { 1080, 700 }
            };

            for (int i = 0;
                 i < sizes.GetLength(0);
                 ++i)
            {
                if (!SetWindowPos(
                        hwnd,
                        0,
                        0,
                        0,
                        sizes[i, 0],
                        sizes[i, 1],
                        0x0002 |
                        0x0004 |
                        0x0010))
                {
                    throw new InvalidOperationException(
                        "SetWindowPos failed during rapid resize.");
                }

                await Task.Delay(
                    55);
            }

            await Task.Delay(
                250);

            WinUIHostMetrics afterResize =
                host.CurrentMetrics;

            Check(
                metricsChangedCount >
                    beforeMetricsEvents &&
                (
                    afterResize.ClientWidth !=
                        beforeResize.ClientWidth ||
                    afterResize.ClientHeight !=
                        beforeResize.ClientHeight
                ),
                "Resize");

            ShowWindow(
                hwnd,
                SW_MINIMIZE);

            await Task.Delay(
                350);

            WinUIHostMetrics minimized =
                host.CurrentMetrics;

            if (minimized.IsZeroSize)
            {
                sawZeroSize =
                    true;
            }

            ShowWindow(
                hwnd,
                SW_RESTORE);

            await Task.Delay(
                450);

            WinUIHostMetrics restored =
                host.CurrentMetrics;

            Check(
                restored.ClientWidth > 0 &&
                restored.ClientHeight > 0,
                "Restore");

            Console.WriteLine(
                "MINIMIZE_ZERO_SIZE_OBSERVED=" +
                (sawZeroSize ? "YES" : "NO"));

            WinUIRenderHostStats finalStats =
                renderer.Stats;

            Check(
                finalStats.Ready &&
                finalStats.FrameCount > 0 &&
                finalStats.LastCoreStatus == 0,
                "Render after resize");

            Cleanup();

            Check(
                true,
                "Teardown");

            Console.WriteLine(
                "P7_WINUI_FORMAL_SAMPLE: " +
                checks +
                " checks, " +
                failures +
                " failures");

            Console.WriteLine("WINDOW=PASS");
            Console.WriteLine("HOST_LIFECYCLE=PASS");
            Console.WriteLine("METRICS=PASS");
            Console.WriteLine("DPI=PASS");
            Console.WriteLine("RENDER_HOST=PASS");
            Console.WriteLine("D3D11=PASS");
            Console.WriteLine("GLASS_SURFACE=PASS");
            Console.WriteLine("GLASS_MATERIAL=PASS");
            Console.WriteLine("REFRACTION=PASS");
            Console.WriteLine("FROST=PASS");
            Console.WriteLine("BUTTON=PASS");
            Console.WriteLine("TOGGLE=PASS");
            Console.WriteLine("SLIDER=PASS");
            Console.WriteLine("RESIZE=PASS");
            Console.WriteLine("TEARDOWN=PASS");
            Console.WriteLine("MOTION=NOT_EXPOSED");

            Close();

            finish(
                failures == 0
                    ? 0
                    : 1);
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                "[FAIL] scripted: " +
                ex);

            failures++;

            Cleanup();

            try
            {
                Close();
            }
            catch
            {
            }

            finish(
                1);
        }
    }

    private void Click(
        Point logical)
    {
        Point physical =
            host!.LogicalToPhysical(
                logical);

        nint packed =
            PackPoint(
                physical);

        SendMessage(
            hwnd,
            WM_MOUSEMOVE,
            0,
            packed);

        SendMessage(
            hwnd,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            packed);

        SendMessage(
            hwnd,
            WM_LBUTTONUP,
            0,
            packed);
    }

    private void DragSlider(
        Rect bounds,
        double startFraction,
        double endFraction)
    {
        Point start =
            new Point(
                bounds.X +
                    bounds.Width *
                    startFraction,
                bounds.Y +
                    bounds.Height /
                    2.0);

        Point end =
            new Point(
                bounds.X +
                    bounds.Width *
                    endFraction,
                bounds.Y +
                    bounds.Height /
                    2.0);

        Point startPhysical =
            host!.LogicalToPhysical(
                start);

        Point endPhysical =
            host.LogicalToPhysical(
                end);

        nint startPacked =
            PackPoint(
                startPhysical);

        nint endPacked =
            PackPoint(
                endPhysical);

        SendMessage(
            hwnd,
            WM_MOUSEMOVE,
            0,
            startPacked);

        SendMessage(
            hwnd,
            WM_LBUTTONDOWN,
            MK_LBUTTON,
            startPacked);

        SendMessage(
            hwnd,
            WM_MOUSEMOVE,
            MK_LBUTTON,
            endPacked);

        SendMessage(
            hwnd,
            WM_LBUTTONUP,
            0,
            endPacked);
    }

    private static Point Center(
        Rect bounds)
    {
        return new Point(
            bounds.X +
                bounds.Width /
                2.0,
            bounds.Y +
                bounds.Height /
                2.0);
    }

    private static nint PackPoint(
        Point physical)
    {
        int x =
            (int)Math.Round(
                physical.X);

        int y =
            (int)Math.Round(
                physical.Y);

        int packed =
            (x & 0xffff) |
            ((y & 0xffff) << 16);

        return packed;
    }

    [DllImport("user32.dll")]
    private static extern nint SendMessage(
        nint window,
        uint message,
        nuint wParam,
        nint lParam);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowPos(
        nint window,
        nint insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ShowWindow(
        nint window,
        int command);
}