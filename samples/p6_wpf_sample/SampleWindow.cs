using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;
using AuroraGlass.Wpf;

namespace P6.WpfSample;

internal sealed partial class SampleWindow : Window
{
    private readonly bool _scripted;
    private readonly string? _screenshotPath;

    private readonly WpfHostAttachment _host = new();
    private readonly WpfControlInputBridge _input = new();
    private readonly WpfGlassMaterial _material = new();
    private readonly WpfRenderHost _renderHost = new();

    private readonly TextBlock _status = new();
    private readonly DispatcherTimer _timer = new();

    private WpfControlInputBridge.WpfButton? _button;
    private WpfControlInputBridge.WpfToggle? _toggle;
    private WpfControlInputBridge.WpfSlider? _slider;

    private readonly Rect _buttonLocal =
        new(72, 78, 230, 74);

    private readonly Rect _toggleLocal =
        new(72, 198, 230, 74);

    private readonly Rect _sliderLocal =
        new(72, 322, 410, 58);

    private bool _initialized;
    private int _checks;
    private int _failures;

    internal int ScriptExitCode =>
        _failures == 0 ? 0 : 1;

    internal SampleWindow(
        bool scripted,
        string? screenshotPath)
    {
        _scripted = scripted;
        _screenshotPath = screenshotPath;

        Title =
            "AuroraGlass P6 — WPF Integration Sample";

        Width = 1080;
        Height = 700;
        MinWidth = 900;
        MinHeight = 560;

        WindowStartupLocation =
            WindowStartupLocation.CenterScreen;

        Background =
            new SolidColorBrush(
                Color.FromRgb(
                    18,
                    20,
                    25));

        Content = BuildContent();

        bool immediate =
            _host.Attach(this);

        if (_scripted)
        {
            Check(
                !immediate,
                "Attach waits for SourceInitialized");
        }

        _host.MetricsChanged +=
            _ =>
            {
                Dispatcher.BeginInvoke(
                    DispatcherPriority.Render,
                    new Action(UpdateBoundsAndRender));
            };

        Loaded += OnLoaded;
        Closed += OnClosed;

        _timer.Interval =
            TimeSpan.FromMilliseconds(100);

        _timer.Tick +=
            (_, _) =>
            {
                if (_initialized &&
                    _host.IsAttached)
                {
                    UpdateRenderRects();
                    UpdateStatus();
                }
            };
    }

    private UIElement BuildContent()
    {
        Grid root = new();

        root.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        root.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(300)
            });

        Border frame =
            new()
            {
                Margin =
                    new Thickness(18),
                BorderThickness =
                    new Thickness(1),
                BorderBrush =
                    new SolidColorBrush(
                        Color.FromRgb(
                            82,
                            88,
                            102)),
                Child = _renderHost
            };

        Grid.SetColumn(frame, 0);
        root.Children.Add(frame);

        Border panel =
            new()
            {
                Padding =
                    new Thickness(22),
                Background =
                    new SolidColorBrush(
                        Color.FromRgb(
                            28,
                            31,
                            38))
            };

        Grid.SetColumn(panel, 1);

        StackPanel stack = new();

        stack.Children.Add(
            new TextBlock
            {
                Text =
                    "AuroraGlass / WPF",
                FontSize = 25,
                FontWeight =
                    FontWeights.SemiBold,
                Foreground =
                    Brushes.White,
                Margin =
                    new Thickness(
                        0,
                        0,
                        0,
                        8)
            });

        stack.Children.Add(
            new TextBlock
            {
                Text =
                    "HwndHost → native child HWND → frozen Core",
                TextWrapping =
                    TextWrapping.Wrap,
                Foreground =
                    new SolidColorBrush(
                        Color.FromRgb(
                            170,
                            177,
                            190)),
                Margin =
                    new Thickness(
                        0,
                        0,
                        0,
                        22)
            });

        stack.Children.Add(
            Label(
                "GLASS BUTTON",
                "upper glass control"));

        stack.Children.Add(
            Label(
                "GLASS TOGGLE",
                "middle glass control"));

        stack.Children.Add(
            Label(
                "GLASS SLIDER",
                "lower glass control"));

        stack.Children.Add(
            new Border
            {
                Height = 1,
                Margin =
                    new Thickness(
                        0,
                        18,
                        0,
                        18),
                Background =
                    new SolidColorBrush(
                        Color.FromRgb(
                            64,
                            69,
                            79))
            });

        _status.FontFamily =
            new FontFamily("Consolas");

        _status.FontSize = 12;
        _status.Foreground =
            new SolidColorBrush(
                Color.FromRgb(
                    205,
                    210,
                    220));

        _status.TextWrapping =
            TextWrapping.Wrap;

        stack.Children.Add(_status);

        panel.Child = stack;
        root.Children.Add(panel);

        return root;
    }

    private static Border Label(
        string title,
        string subtitle)
    {
        StackPanel stack = new();

        stack.Children.Add(
            new TextBlock
            {
                Text = title,
                FontSize = 13,
                FontWeight =
                    FontWeights.SemiBold,
                Foreground =
                    Brushes.White
            });

        stack.Children.Add(
            new TextBlock
            {
                Text = subtitle,
                FontSize = 11,
                Foreground =
                    new SolidColorBrush(
                        Color.FromRgb(
                            148,
                            155,
                            168))
            });

        return new Border
        {
            Padding =
                new Thickness(
                    0,
                    8,
                    0,
                    8),
            Child = stack
        };
    }

    private async void OnLoaded(
        object sender,
        RoutedEventArgs e)
    {
        try
        {
            await Dispatcher.Yield(
                DispatcherPriority.ApplicationIdle);

            InitializeAuroraGlass();

            _timer.Start();

            if (_scripted)
            {
                await RunScriptedAsync();
            }
        }
        catch (Exception ex)
        {
            ++_failures;

            Console.WriteLine(
                "[FAIL] unhandled sample exception");

            Console.WriteLine(ex);

            if (_scripted)
            {
                Close();
            }
        }
    }

    private void InitializeAuroraGlass()
    {
        if (_initialized)
        {
            return;
        }

        Check(
            _host.IsAttached,
            "WpfHostAttachment attached");

        WpfHostMetrics metrics =
            _host.CurrentMetrics;

        Check(
            metrics.ClientWidth > 0 &&
            metrics.ClientHeight > 0,
            "initial host metrics valid");

        Check(
            metrics.Dpi > 0,
            "initial native DPI valid");

        Check(
            _input.Attach(_host) ==
                WpfInputStatus.Ok,
            "WpfControlInputBridge attached");

        _material.SetBlurRadius(14.0f);
        _material.SetRefractionStrength(0.22f);
        _material.SetDispersionStrength(0.06f);
        _material.SetThickness(0.58f);
        _material.SetEdgeFresnel(0.76f);
        _material.SetSpecularStrength(1.05f);
        _material.SetTintAmount(0.08f);
        _material.SetSaturation(1.02f);
        _material.SetBrightness(1.02f);
        _material.SetNoiseAmount(0.01f);
        _material.SetCornerRadius(24.0f);
        _material.SetOpacity(0.80f);
        _material.SetHighlightPosition(
            0.30f,
            0.22f);

        _renderHost.SetMaterial(
            _material);

        _initialized = true;

        UpdateRenderRects();

        _button =
            _input.AddButton(
                ToWindowDip(
                    _buttonLocal));

        _toggle =
            _input.AddToggle(
                ToWindowDip(
                    _toggleLocal));

        _slider =
            _input.AddSlider(
                ToWindowDip(
                    _sliderLocal));

        Check(
            _button is not null,
            "frozen GlassButton registered");

        Check(
            _toggle is not null,
            "frozen GlassToggle registered");

        Check(
            _slider is not null,
            "frozen GlassSlider registered");

        UpdateStatus();
    }

    private void UpdateBoundsAndRender()
    {
        if (!_initialized ||
            !_host.IsAttached)
        {
            return;
        }

        UpdateRenderRects();

        _button?.SetBounds(
            ToWindowDip(
                _buttonLocal));

        _toggle?.SetBounds(
            ToWindowDip(
                _toggleLocal));

        _slider?.SetBounds(
            ToWindowDip(
                _sliderLocal));

        UpdateStatus();
    }

    private void UpdateRenderRects()
    {
        if (!_initialized ||
            !_host.IsAttached)
        {
            return;
        }

        float toggle =
            _toggle?.IsChecked == true
                ? 1.0f
                : 0.0f;

        float slider =
            _slider?.Value ?? 0.0f;

        Rect toggleThumb =
            new(
                _toggleLocal.X +
                    14 +
                    toggle *
                    (_toggleLocal.Width - 74),
                _toggleLocal.Y + 13,
                48,
                48);

        Rect sliderTrack =
            new(
                _sliderLocal.X,
                _sliderLocal.Y + 18,
                _sliderLocal.Width,
                22);

        Rect sliderThumb =
            new(
                _sliderLocal.X +
                    slider *
                    (_sliderLocal.Width - 42),
                _sliderLocal.Y + 7,
                42,
                44);

        _renderHost.SetPhysicalRects(
            _host.DipToPhysical(
                _buttonLocal),
            _host.DipToPhysical(
                _toggleLocal),
            _host.DipToPhysical(
                toggleThumb),
            _host.DipToPhysical(
                sliderTrack),
            _host.DipToPhysical(
                sliderThumb));
    }

    private Rect ToWindowDip(
        Rect local)
    {
        Point point =
            _renderHost.TranslatePoint(
                new Point(
                    local.X,
                    local.Y),
                this);

        return new Rect(
            point.X,
            point.Y,
            local.Width,
            local.Height);
    }

    private void UpdateStatus()
    {
        if (!_host.IsAttached)
        {
            _status.Text =
                "Host: detached";

            return;
        }

        WpfHostMetrics host =
            _host.CurrentMetrics;

        WpfRenderHostStats render =
            _renderHost.Stats;

        WpfGlassMaterialSnapshot material =
            _material.Snapshot;

        _status.Text =
            "Host: attached" +
            Environment.NewLine +
            "DPI: " + host.Dpi +
            Environment.NewLine +
            "Client: " +
            host.ClientWidth +
            " x " +
            host.ClientHeight +
            Environment.NewLine +
            "Render: " +
            render.Width +
            " x " +
            render.Height +
            Environment.NewLine +
            "Frames: " +
            render.FrameCount +
            Environment.NewLine +
            "Core status: " +
            render.LastCoreStatus +
            Environment.NewLine +
            Environment.NewLine +
            "Blur: " +
            material.BlurRadius.ToString("0.0") +
            Environment.NewLine +
            "Refraction: " +
            material.RefractionStrength.ToString("0.00") +
            Environment.NewLine +
            "Opacity: " +
            material.Opacity.ToString("0.00") +
            Environment.NewLine +
            Environment.NewLine +
            "Clicks: " +
            (_button?.ClickCount ?? 0) +
            Environment.NewLine +
            "Toggle: " +
            (_toggle?.IsChecked ?? false) +
            Environment.NewLine +
            "Slider: " +
            (_slider?.Value.ToString("0.00") ??
                "n/a") +
            Environment.NewLine +
            Environment.NewLine +
            "P4 motion: not exposed to WPF" +
            Environment.NewLine +
            "No managed animation substitute";
    }

    private void OnClosed(
        object? sender,
        EventArgs e)
    {
        _timer.Stop();

        bool inputWasAttached =
            _input.IsAttached;

        _input.Dispose();
        _host.Dispose();
        _material.Dispose();

        if (_scripted)
        {
            Check(
                inputWasAttached,
                "input attached before teardown");

            Check(
                !_host.IsAttached,
                "WpfHostAttachment clean detach");

            Console.WriteLine(
                "RUNTIME_TEARDOWN=" +
                (_failures == 0
                    ? "PASS"
                    : "FAIL"));

            Console.WriteLine(
                "P6_WPF_SAMPLE_SCRIPTED: " +
                _checks +
                " checks, " +
                _failures +
                " failures");
        }
    }
}
