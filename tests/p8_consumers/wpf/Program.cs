using System;
using System.IO;
using System.Windows;
using System.Windows.Threading;
using AuroraGlass.Wpf;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        var app = new Application();
        var host = new WpfHostAttachment();
        var material = new WpfGlassMaterial();
        var render = new WpfRenderHost();

        var window = new Window
        {
            Title = "P8 Fresh WPF",
            Width = 640,
            Height = 360,
            Content = render
        };

        host.Attach(window);

        var timer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(100)
        };

        int ticks = 0;
        int result = 10;

        window.Loaded += (_, _) =>
        {
            material.SetBlurRadius(14.0f);
            material.SetOpacity(0.85f);
            render.SetMaterial(material);

            Rect r = host.DipToPhysical(
                new Rect(80, 70, 360, 180));

            render.SetPhysicalRects(r);

            timer.Tick += (_, _) =>
            {
                ticks++;

                var stats = render.Stats;

                if (host.IsAttached &&
                    stats.Ready &&
                    stats.FrameCount > 2)
                {
                    result = 0;
                    timer.Stop();
                    window.Close();
                    return;
                }

                if (ticks > 100)
                {
                    result = 20;
                    timer.Stop();
                    window.Close();
                }
            };

            timer.Start();
        };

        window.Closed += (_, _) =>
        {
            host.Detach();
            material.Dispose();
            render.Dispose();

            File.WriteAllText(
                Path.Combine(AppContext.BaseDirectory, "p8-result.txt"),
                "ATTACHED=" + host.IsAttached +
                Environment.NewLine +
                "EXIT=" + result);
        };

        app.Run(window);
        Environment.ExitCode = result;
    }
}