using System.IO;
using AuroraGlass.WinUI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Foundation;

namespace P8FreshWinUI;

internal sealed class FreshWindow : Window
{
    private readonly Action<int> finish;
    private readonly Grid root = new();

    private WinUIHostAttachment? host;
    private WinUIControlInputBridge? input;
    private WinUIGlassMaterial? material;
    private WinUIRenderHost? renderer;

    private int result = 20;
    private bool cleaned;

    internal FreshWindow(Action<int> finish)
    {
        this.finish = finish;
        Title = "P8 Fresh WinUI";
        Content = root;

        root.Loaded += OnLoaded;
        Closed += OnClosed;
    }

    private async void OnLoaded(object sender, RoutedEventArgs args)
    {
        try
        {
            host = new WinUIHostAttachment();
            host.Attach(this);

            if (!host.IsAttached)
            {
                result = 21;
                Close();
                return;
            }

            input = new WinUIControlInputBridge();

            if (input.Attach(host) != WinUIInputStatus.Ok)
            {
                result = 22;
                Close();
                return;
            }

            var button = input.AddButton(
                new Rect(40, 40, 180, 64));

            if (button is null)
            {
                result = 23;
                Close();
                return;
            }

            material = new WinUIGlassMaterial();
            material.SetBlurRadius(14.0f);
            material.SetOpacity(0.85f);

            renderer = new WinUIRenderHost(host);
            renderer.SetMaterial(material);
            renderer.SetLogicalRects(
                new[] { new Rect(40, 40, 320, 180) });

            renderer.ResizeNow();

            for (int i = 0; i < 30; ++i)
            {
                await Task.Delay(100);

                var stats = renderer.Stats;

                if (stats.Ready &&
                    stats.FrameCount > 0 &&
                    stats.LastCoreStatus == 0)
                {
                    result = 0;
                    Close();
                    return;
                }
            }

            result = 24;
            Close();
        }
        catch
        {
            result = 25;
            Close();
        }
    }

    private void OnClosed(object sender, WindowEventArgs args)
    {
        Cleanup();

        File.WriteAllText(
            Path.Combine(AppContext.BaseDirectory, "p8-result.txt"),
            "ATTACHED=" + (host?.IsAttached ?? false) +
            Environment.NewLine +
            "EXIT=" + result);

        finish(result);
    }

    private void Cleanup()
    {
        if (cleaned)
            return;

        cleaned = true;

        renderer?.Dispose();
        renderer = null;

        material?.Dispose();
        material = null;

        input?.Detach();
        input?.Dispose();
        input = null;

        host?.Detach();
        host?.Dispose();
    }
}