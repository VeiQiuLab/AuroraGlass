using System.Windows;

namespace P6.WpfSample;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        bool scripted =
            args.Contains(
                "--scripted",
                StringComparer.OrdinalIgnoreCase);

        string? screenshot =
            ReadOption(
                args,
                "--screenshot");

        Application app =
            new()
            {
                ShutdownMode =
                    ShutdownMode.OnMainWindowClose
            };

        SampleWindow window =
            new(
                scripted,
                screenshot);

        app.MainWindow =
            window;

        window.Show();

        int result =
            app.Run();

        return scripted
            ? window.ScriptExitCode
            : result;
    }

    private static string? ReadOption(
        string[] args,
        string name)
    {
        for (int i = 0;
             i + 1 < args.Length;
             ++i)
        {
            if (string.Equals(
                    args[i],
                    name,
                    StringComparison.OrdinalIgnoreCase))
            {
                return args[i + 1];
            }
        }

        return null;
    }
}
