using AuroraGlass.WinUI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
using System.Runtime.InteropServices;
using Windows.Foundation;
using Windows.UI;
using WinRT.Interop;

namespace P7.WinUISample;

internal sealed partial class SampleWindow : Window
{
    private readonly bool scripted;
    private readonly bool captureHold;
    private readonly Action<int> finish;

    private readonly Grid root;
    private readonly Canvas background;
    private readonly TextBlock statusText;
    private readonly TextBlock buttonText;
    private readonly TextBlock toggleText;
    private readonly TextBlock sliderText;

    private WinUIHostAttachment? host;
    private WinUIControlInputBridge? input;
    private WinUIControlInputBridge.WinUIButton? glassButton;
    private WinUIControlInputBridge.WinUIToggle? glassToggle;
    private WinUIControlInputBridge.WinUISlider? glassSlider;
    private WinUIGlassMaterial? material;
    private WinUIRenderHost? renderer;

    private Rect buttonBounds;
    private Rect toggleBounds;
    private Rect sliderBounds;
    private Rect heroBounds;

    private nint hwnd;
    private int metricsChangedCount;
    private bool sawZeroSize;
    private bool cleaned;
    private int checks;
    private int failures;

    internal SampleWindow(
        bool scripted,
        bool captureHold,
        Action<int> finish)
    {
        this.scripted =
            scripted;

        this.captureHold =
            captureHold;

        this.finish =
            finish;

        Title =
            "AuroraGlass — WinUI 3 Integration Sample";

        root =
            new Grid
            {
                Background =
                    Brush(
                        255,
                        12,
                        17,
                        28)
            };

        background =
            new Canvas();

        root.Children.Add(
            background);

        BuildObservationBackground();

        TextBlock title =
            Text(
                "AuroraGlass / WinUI 3",
                30,
                32,
                30);

        title.FontWeight =
            Microsoft.UI.Text.FontWeights.SemiBold;

        root.Children.Add(
            title);

        TextBlock subtitle =
            Text(
                "P7 production host · input · material · rendering",
                32,
                76,
                15);

        subtitle.Opacity =
            0.66;

        root.Children.Add(
            subtitle);

        buttonText =
            Text(
                "GlassButton",
                112,
                173,
                18);

        toggleText =
            Text(
                "GlassToggle · OFF",
                112,
                273,
                18);

        sliderText =
            Text(
                "GlassSlider · 0%",
                112,
                373,
                18);

        statusText =
            Text(
                "Initializing AuroraGlass...",
                32,
                535,
                14);

        root.Children.Add(
            buttonText);

        root.Children.Add(
            toggleText);

        root.Children.Add(
            sliderText);

        root.Children.Add(
            statusText);

        Content =
            root;

        root.Loaded +=
            OnLoaded;

        Closed +=
            OnClosed;
    }

    private async void OnLoaded(
        object sender,
        RoutedEventArgs args)
    {
        root.Loaded -=
            OnLoaded;

        try
        {
            hwnd =
                WindowNative.GetWindowHandle(
                    this);

            Check(
                hwnd != 0 &&
                IsWindow(hwnd),
                "Window");

            host =
                new WinUIHostAttachment();

            host.MetricsChanged +=
                OnMetricsChanged;

            host.Attach(
                this);

            Check(
                host.IsAttached &&
                host.WindowHandle == hwnd,
                "Host lifecycle");

            WinUIHostMetrics initial =
                host.CurrentMetrics;

            Check(
                initial.ClientWidth > 0 &&
                initial.ClientHeight > 0,
                "Metrics");

            Point dpiProbe =
                host.LogicalToPhysical(
                    new Point(
                        100,
                        100));

            Check(
                initial.Dpi > 0 &&
                initial.LogicalToPhysicalScale > 0 &&
                dpiProbe.X > 0 &&
                dpiProbe.Y > 0,
                "DPI");

            input =
                new WinUIControlInputBridge();

            Check(
                input.Attach(
                    host) ==
                WinUIInputStatus.Ok,
                "Input attachment");

            LayoutProductionControls(
                initial,
                createControls: true);

            material =
                new WinUIGlassMaterial();

            material.SetOpacity(
                0.90f);

            material.SetCornerRadius(
                26.0f);

            material.SetHighlightPosition(
                0.34f,
                0.22f);

            WinUIGlassMaterialSnapshot snapshot =
                material.Snapshot;

            Check(
                snapshot.Opacity > 0 &&
                snapshot.CornerRadius > 0,
                "GlassMaterial");

            Check(
                snapshot.RefractionStrength > 0,
                "Refraction");

            Check(
                snapshot.BlurRadius > 0,
                "Frost");

            renderer =
                new WinUIRenderHost(
                    host);

            renderer.SetMaterial(
                material);

            ApplyRenderRects();

            await Task.Delay(
                650);

            WinUIRenderHostStats stats =
                renderer.Stats;

            Check(
                stats.Ready,
                "RenderHost");

            Check(
                stats.Ready &&
                stats.LastCoreStatus == 0,
                "D3D11");

            Check(
                stats.Width > 0 &&
                stats.Height > 0,
                "GlassSurface");

            Check(
                stats.FrameCount > 0 &&
                stats.LastCoreStatus == 0,
                "AuroraGlass renderer");

            statusText.Text =
                "Production renderer active · " +
                stats.Width +
                "×" +
                stats.Height +
                " · DPI " +
                initial.Dpi;

            if (scripted)
            {
                await RunScriptedAsync();
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                "[FAIL] " +
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

    private void OnMetricsChanged(
        WinUIHostMetrics metrics)
    {
        metricsChangedCount++;

        if (metrics.IsZeroSize)
        {
            sawZeroSize =
                true;
        }

        if (cleaned)
        {
            return;
        }

        try
        {
            LayoutProductionControls(
                metrics,
                createControls: false);

            renderer?.ResizeNow();

            ApplyRenderRects();
        }
        catch (Exception ex)
        {
            Console.WriteLine(
                "[FAIL] MetricsChanged: " +
                ex.Message);

            failures++;
        }
    }

    private void LayoutProductionControls(
        WinUIHostMetrics metrics,
        bool createControls)
    {
        double scale =
            metrics.LogicalToPhysicalScale <= 0
                ? 1.0
                : metrics.LogicalToPhysicalScale;

        double logicalWidth =
            metrics.ClientWidth /
            scale;

        buttonBounds =
            new Rect(
                82,
                142,
                260,
                72);

        toggleBounds =
            new Rect(
                82,
                242,
                260,
                72);

        sliderBounds =
            new Rect(
                82,
                342,
                430,
                72);

        double heroX =
            Math.Max(
                560,
                logicalWidth - 470);

        heroBounds =
            new Rect(
                heroX,
                122,
                390,
                360);

        if (createControls)
        {
            glassButton =
                input!.AddButton(
                    buttonBounds);

            glassToggle =
                input.AddToggle(
                    toggleBounds);

            glassSlider =
                input.AddSlider(
                    sliderBounds);
        }
        else
        {
            glassButton?.SetBounds(
                buttonBounds);

            glassToggle?.SetBounds(
                toggleBounds);

            glassSlider?.SetBounds(
                sliderBounds);
        }
    }

    private void ApplyRenderRects()
    {
        if (renderer is null)
        {
            return;
        }

        renderer.SetLogicalRects(
            buttonBounds,
            toggleBounds,
            sliderBounds,
            heroBounds);
    }

    private void BuildObservationBackground()
    {
        for (int i = 0;
             i < 11;
             ++i)
        {
            Rectangle line =
                new Rectangle
                {
                    Width = 3,
                    Height = 620,
                    Fill =
                        Brush(
                            110,
                            (byte)(50 + i * 12),
                            (byte)(105 + i * 7),
                            (byte)(185 + i * 4))
                };

            Canvas.SetLeft(
                line,
                390 + i * 58);

            Canvas.SetTop(
                line,
                0);

            background.Children.Add(
                line);
        }

        for (int i = 0;
             i < 8;
             ++i)
        {
            Rectangle band =
                new Rectangle
                {
                    Width = 1100,
                    Height = 2,
                    Fill =
                        Brush(
                            80,
                            210,
                            225,
                            255)
                };

            Canvas.SetLeft(
                band,
                0);

            Canvas.SetTop(
                band,
                112 + i * 62);

            background.Children.Add(
                band);
        }

        Rectangle warm =
            new Rectangle
            {
                Width = 260,
                Height = 260,
                RadiusX = 130,
                RadiusY = 130,
                Fill =
                    Brush(
                        80,
                        210,
                        83,
                        120)
            };

        Canvas.SetLeft(
            warm,
            760);

        Canvas.SetTop(
            warm,
            80);

        background.Children.Add(
            warm);

        Rectangle cool =
            new Rectangle
            {
                Width = 330,
                Height = 180,
                RadiusX = 48,
                RadiusY = 48,
                Fill =
                    Brush(
                        68,
                        65,
                        145,
                        255)
            };

        Canvas.SetLeft(
            cool,
            620);

        Canvas.SetTop(
            cool,
            330);

        background.Children.Add(
            cool);
    }

    private static TextBlock Text(
        string value,
        double x,
        double y,
        double size)
    {
        TextBlock text =
            new TextBlock
            {
                Text = value,
                FontSize = size,
                Foreground =
                    Brush(
                        235,
                        245,
                        248,
                        255)
            };

        text.HorizontalAlignment =
            HorizontalAlignment.Left;

        text.VerticalAlignment =
            VerticalAlignment.Top;

        text.Margin =
            new Thickness(
                x,
                y,
                0,
                0);

        return text;
    }

    private static SolidColorBrush Brush(
        byte a,
        byte r,
        byte g,
        byte b)
    {
        return new SolidColorBrush(
            Color.FromArgb(
                a,
                r,
                g,
                b));
    }

    private void Check(
        bool condition,
        string name)
    {
        checks++;

        if (!condition)
        {
            failures++;
        }

        Console.WriteLine(
            (condition ? "[PASS] " : "[FAIL] ") +
            name);
    }

    private void OnClosed(
        object sender,
        WindowEventArgs args)
    {
        Cleanup();

        if (!scripted)
        {
            finish(
                0);
        }
    }

    private void Cleanup()
    {
        if (cleaned)
        {
            return;
        }

        cleaned =
            true;

        if (host is not null)
        {
            host.MetricsChanged -=
                OnMetricsChanged;
        }

        renderer?.Dispose();
        renderer = null;

        material?.Dispose();
        material = null;

        input?.Detach();
        input?.Dispose();
        input = null;

        host?.Detach();
        host?.Dispose();
        host = null;
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWindow(
        nint window);
}