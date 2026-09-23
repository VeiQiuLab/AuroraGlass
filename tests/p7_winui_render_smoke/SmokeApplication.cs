using AuroraGlass.WinUI;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Runtime.InteropServices;
using Windows.Foundation;
using WinRT.Interop;

namespace P7.WinUIRenderSmoke;

internal sealed class SmokeApplication : Application
{
    private readonly Action<int> finish;

    private Window? window;
    private Grid? root;
    private WinUIHostAttachment? host;
    private WinUIGlassMaterial? material;
    private WinUIRenderHost? renderer;
    private DispatcherQueueTimer? timer;

    private nint hwnd;
    private int checks;
    private int failures;

    internal SmokeApplication(
        Action<int> finish)
    {
        this.finish = finish;
    }

    protected override void OnLaunched(
        LaunchActivatedEventArgs args)
    {
        window = new Window();
        root = new Grid();

        root.Loaded += OnLoaded;
        window.Content = root;
        window.Activate();
    }

    private void OnLoaded(
        object sender,
        RoutedEventArgs args)
    {
        try
        {
            root!.Loaded -= OnLoaded;

            hwnd =
                WindowNative.GetWindowHandle(
                    window!);

            Check(
                hwnd != 0 &&
                IsWindow(hwnd),
                "Window");

            host =
                new WinUIHostAttachment();

            host.Attach(
                window!);

            Check(
                host.IsAttached,
                "HostAttachment");

            material =
                new WinUIGlassMaterial();

            material.SetOpacity(
                0.90f);

            Check(
                Math.Abs(
                    material.Snapshot.Opacity -
                    0.90f) < 0.001f,
                "GlassMaterial");

            renderer =
                new WinUIRenderHost(
                    host);

            Check(
                renderer.ChildWindowHandle != 0 &&
                GetParent(
                    renderer.ChildWindowHandle) ==
                    hwnd,
                "RenderHost");

            Check(
                SendMessage(
                    renderer.ChildWindowHandle,
                    0x0084,
                    0,
                    0) == (nint)(-1),
                "single input path");

            renderer.SetMaterial(
                material);

            renderer.SetLogicalRects(
                new Rect(
                    30,
                    30,
                    280,
                    160));

            Start(
                850,
                ValidateFrame);
        }
        catch (Exception ex)
        {
            Fail(ex);
        }
    }

    private void ValidateFrame()
    {
        try
        {
            WinUIRenderHostStats stats =
                renderer!.Stats;

            Check(
                stats.Ready,
                "D3D11");

            Check(
                stats.Width > 0 &&
                stats.Height > 0,
                "GlassSurface");

            Check(
                stats.LastCoreStatus == 0,
                "RenderRect");

            Check(
                stats.FrameCount > 0,
                "Present/output");

            WinUIHostMetrics before =
                host!.CurrentMetrics;

            Check(
                SetWindowPos(
                    hwnd,
                    0,
                    0,
                    0,
                    860,
                    620,
                    0x0002 |
                    0x0004 |
                    0x0010),
                "resize requested");

            Start(
                600,
                () => ValidateResize(
                    before,
                    stats.FrameCount));
        }
        catch (Exception ex)
        {
            Fail(ex);
        }
    }

    private void ValidateResize(
        WinUIHostMetrics before,
        ulong priorFrames)
    {
        try
        {
            WinUIHostMetrics after =
                host!.CurrentMetrics;

            WinUIRenderHostStats stats =
                renderer!.Stats;

            Check(
                after.ClientWidth != before.ClientWidth ||
                after.ClientHeight != before.ClientHeight,
                "Slice B resize");

            Check(
                renderer.LastResizeStatus == 0,
                "MetricsChanged");

            Check(
                stats.Width == after.ClientWidth &&
                stats.Height == after.ClientHeight,
                "physical render size");

            Check(
                stats.FrameCount > priorFrames,
                "render continues");

            nint child =
                renderer.ChildWindowHandle;

            renderer.Dispose();
            renderer = null;

            Check(
                !IsWindow(child),
                "render teardown");

            Check(
                IsWindow(hwnd),
                "top HWND survives");

            material!.Dispose();
            material = null;

            host.Detach();

            Check(
                !host.IsAttached,
                "host detach");

            host.Dispose();
            host = null;

            window!.Close();
            window = null;

            Console.WriteLine(
                "P7_WINUI3_RENDER_SMOKE: " +
                checks +
                " checks, " +
                failures +
                " failures");

            Console.WriteLine("WINDOW=PASS");
            Console.WriteLine("RENDER_HOST=PASS");
            Console.WriteLine("D3D11=PASS");
            Console.WriteLine("GLASS_SURFACE=PASS");
            Console.WriteLine("GLASS_MATERIAL=PASS");
            Console.WriteLine("RENDER_RECT=PASS");
            Console.WriteLine("OUTPUT=PASS");
            Console.WriteLine("RESIZE=PASS");
            Console.WriteLine("TEARDOWN=PASS");
            Console.WriteLine("SCREENSHOT=NOT_CAPTURED");

            finish(
                failures == 0
                    ? 0
                    : 1);
        }
        catch (Exception ex)
        {
            Fail(ex);
        }
    }

    private void Start(
        double milliseconds,
        Action action)
    {
        timer?.Stop();

        timer =
            DispatcherQueue
                .GetForCurrentThread()
                .CreateTimer();

        timer.Interval =
            TimeSpan.FromMilliseconds(
                milliseconds);

        timer.Tick += Tick;

        void Tick(
            DispatcherQueueTimer sender,
            object args)
        {
            sender.Stop();
            sender.Tick -= Tick;
            action();
        }

        timer.Start();
    }

    private void Check(
        bool value,
        string name)
    {
        ++checks;

        if (!value)
        {
            ++failures;
        }

        Console.WriteLine(
            (value ? "[PASS] " : "[FAIL] ") +
            name);
    }

    private void Fail(
        Exception ex)
    {
        Console.WriteLine(
            "[FAIL] " +
            ex);

        try
        {
            renderer?.Dispose();
            material?.Dispose();
            host?.Dispose();
            window?.Close();
        }
        catch
        {
        }

        Environment.Exit(1);
    }

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsWindow(
        nint hwnd);

    [DllImport("user32.dll")]
    private static extern nint GetParent(
        nint hwnd);

    [DllImport("user32.dll")]
    private static extern nint SendMessage(
        nint hwnd,
        uint message,
        nuint wParam,
        nint lParam);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWindowPos(
        nint hwnd,
        nint insertAfter,
        int x,
        int y,
        int width,
        int height,
        uint flags);
}