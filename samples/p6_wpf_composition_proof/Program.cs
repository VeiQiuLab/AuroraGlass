using System.Windows;

namespace P6.WpfCompositionProof;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        bool scripted = Array.Exists(args,
            a => string.Equals(a, "--scripted", StringComparison.OrdinalIgnoreCase));

        Application app = new()
        {
            ShutdownMode = ShutdownMode.OnMainWindowClose,
        };

        ProofWindow window = new(scripted);
        app.MainWindow = window;
        window.Show();

        int result = app.Run();
        return scripted ? window.ScriptExitCode : result;
    }
}
