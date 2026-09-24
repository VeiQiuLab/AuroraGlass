using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using WinRT;

namespace P8FreshWinUI;

internal static class Program
{
    private static FreshApplication? app;

    [STAThread]
    public static int Main()
    {
        int exitCode = 1;

        ComWrappersSupport.InitializeComWrappers();

        Application.Start(_ =>
        {
            var queue = DispatcherQueue.GetForCurrentThread();

            SynchronizationContext.SetSynchronizationContext(
                new DispatcherQueueSynchronizationContext(queue));

            app = new FreshApplication(code => exitCode = code);
        });

        app = null;
        return exitCode;
    }
}