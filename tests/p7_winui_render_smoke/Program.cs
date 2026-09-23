using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using WinRT;

namespace P7.WinUIRenderSmoke;

internal static class Program
{
    private static SmokeApplication? app;

    [STAThread]
    public static int Main()
    {
        int result = 1;

        ComWrappersSupport.InitializeComWrappers();

        Application.Start(
            _ =>
            {
                DispatcherQueue queue =
                    DispatcherQueue.GetForCurrentThread();

                SynchronizationContext.SetSynchronizationContext(
                    new DispatcherQueueSynchronizationContext(
                        queue));

                app =
                    new SmokeApplication(
                        code => result = code);
            });

        app = null;

        return result;
    }
}